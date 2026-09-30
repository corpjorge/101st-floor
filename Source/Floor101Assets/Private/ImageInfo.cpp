#include "F101/Assets/ImageInfo.h"

#include <algorithm>
#include <iterator>
#include <string>

namespace f101::assets
{
	namespace
	{
		std::uint32_t bigEndian32(const std::uint8_t* p)
		{
			return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) | (std::uint32_t{p[2]} << 8) | std::uint32_t{p[3]};
		}

		std::uint32_t bigEndian16(const std::uint8_t* p)
		{
			return (std::uint32_t{p[0]} << 8) | std::uint32_t{p[1]};
		}

		Result<ImageInfo> checked(ImageFormat format, std::uint32_t width, std::uint32_t height)
		{
			if (width == 0 || height == 0)
			{
				return Error{ErrorCode::ParseError, "The image has no pixels"};
			}
			if (width > static_cast<std::uint32_t>(MaxImageSide) || height > static_cast<std::uint32_t>(MaxImageSide))
			{
				return Error{ErrorCode::OutOfRange, "The image is larger than " + std::to_string(MaxImageSide) + " pixels a side"};
			}
			return ImageInfo{format, static_cast<int>(width), static_cast<int>(height)};
		}
	}

	Result<ImageInfo> readImageInfo(const std::uint8_t* data, std::size_t size)
	{
		// PNG: signature, then the IHDR chunk (length, type, width, height).
		constexpr std::uint8_t PngSignature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
		if (size >= 8 && std::equal(std::begin(PngSignature), std::end(PngSignature), data))
		{
			if (size < 24 || data[12] != 'I' || data[13] != 'H' || data[14] != 'D' || data[15] != 'R')
			{
				return Error{ErrorCode::ParseError, "Damaged PNG header"};
			}
			return checked(ImageFormat::Png, bigEndian32(data + 16), bigEndian32(data + 20));
		}
		// JPEG: SOI, then segments until a start of frame (SOF0-15 but DHT, JPG and DAC) with the height and width.
		if (size >= 4 && data[0] == 0xFF && data[1] == 0xD8)
		{
			std::size_t at = 2;
			while (at + 4 <= size)
			{
				if (data[at] != 0xFF)
				{
					return Error{ErrorCode::ParseError, "Damaged JPEG header"};
				}
				const std::uint8_t marker = data[at + 1];
				if (marker == 0xFF)
				{
					++at; // fill byte
					continue;
				}
				if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7))
				{
					at += 2; // markers without a length
					continue;
				}
				if (marker == 0xD9 || marker == 0xDA)
				{
					break; // end of image or start of scan before any frame
				}
				const std::uint32_t length = bigEndian16(data + at + 2);
				if (length < 2 || at + 2 + length > size)
				{
					return Error{ErrorCode::ParseError, "Damaged JPEG header"};
				}
				const bool frame = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
				if (frame)
				{
					if (length < 7)
					{
						return Error{ErrorCode::ParseError, "Damaged JPEG header"};
					}
					return checked(ImageFormat::Jpeg, bigEndian16(data + at + 7), bigEndian16(data + at + 5));
				}
				at += 2 + length;
			}
			return Error{ErrorCode::ParseError, "The JPEG image has no frame header"};
		}
		return Error{ErrorCode::InvalidArgument, "Not a PNG or JPEG image"};
	}

	std::string_view imageExtension(ImageFormat format)
	{
		return format == ImageFormat::Png ? "png" : "jpg";
	}
}
