#include "doctest.h"

#include "F101/Assets/ImageInfo.h"
#include "F101/Exporters/Png.h"
#include "F101/Scene/FinishTextures.h"

#include <cstdlib>
#include <cstring>
#include <optional>
#include <random>
#include <string>
#include <vector>

using namespace f101;
using namespace f101::exporters;

// The PNG encoder of the exporters (ADR-0020), read back by an independent minimal decoder written here.
namespace
{
	// Inflate (RFC 1951) of fixed-Huffman blocks: what zlibCompress writes.
	class BitReader
	{
	public:
		BitReader(const std::vector<std::uint8_t>& bytes, std::size_t start)
			: data(bytes), position(start)
		{
		}

		std::uint32_t bits(int count)
		{
			std::uint32_t value = 0;
			for (int i = 0; i < count; ++i)
			{
				if (position >= data.size())
				{
					failed = true;
					return 0;
				}
				value |= static_cast<std::uint32_t>((data[position] >> bit) & 1u) << i;
				if (++bit == 8)
				{
					bit = 0;
					++position;
				}
			}
			return value;
		}

		void alignToByte()
		{
			if (bit != 0)
			{
				bit = 0;
				++position;
			}
		}

		std::size_t offset() const { return position; }
		bool failed = false;

	private:
		const std::vector<std::uint8_t>& data;
		std::size_t position = 0;
		int bit = 0;
	};

	int fixedLiteral(BitReader& in)
	{
		int code = 0;
		for (int length = 1; length <= 9 && !in.failed; ++length)
		{
			code = (code << 1) | static_cast<int>(in.bits(1));
			if (length == 7 && code <= 0x17)
			{
				return 256 + code;
			}
			if (length == 8 && code >= 0x30 && code <= 0xBF)
			{
				return code - 0x30;
			}
			if (length == 8 && code >= 0xC0 && code <= 0xC7)
			{
				return 280 + code - 0xC0;
			}
			if (length == 9 && code >= 0x190)
			{
				return 144 + code - 0x190;
			}
		}
		return -1;
	}

	std::optional<std::vector<std::uint8_t>> inflateZlib(const std::vector<std::uint8_t>& stream)
	{
		static const int lengthBase[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115,
			131, 163, 195, 227, 258};
		static const int lengthExtra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
		static const int distanceBase[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
			2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
		static const int distanceExtra[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12,
			13, 13};
		if (stream.size() < 6 || (stream[0] & 0x0F) != 8 || ((stream[0] << 8) | stream[1]) % 31 != 0)
		{
			return std::nullopt;
		}
		std::vector<std::uint8_t> out;
		BitReader in(stream, 2);
		bool last = false;
		while (!last)
		{
			last = in.bits(1) == 1;
			if (in.bits(2) != 1)
			{
				return std::nullopt; // stored and dynamic blocks: never written by zlibCompress
			}
			for (;;)
			{
				const int symbol = fixedLiteral(in);
				if (symbol < 0 || in.failed || symbol > 285)
				{
					return std::nullopt;
				}
				if (symbol < 256)
				{
					out.push_back(static_cast<std::uint8_t>(symbol));
					continue;
				}
				if (symbol == 256)
				{
					break;
				}
				const int lengthCode = symbol - 257;
				const int length = lengthBase[lengthCode] + static_cast<int>(in.bits(lengthExtra[lengthCode]));
				int distanceCode = 0;
				for (int i = 0; i < 5; ++i)
				{
					distanceCode = (distanceCode << 1) | static_cast<int>(in.bits(1));
				}
				if (distanceCode > 29)
				{
					return std::nullopt;
				}
				const int distance = distanceBase[distanceCode] + static_cast<int>(in.bits(distanceExtra[distanceCode]));
				if (distance > static_cast<int>(out.size()))
				{
					return std::nullopt;
				}
				for (int i = 0; i < length; ++i)
				{
					out.push_back(out[out.size() - static_cast<std::size_t>(distance)]);
				}
			}
		}
		in.alignToByte();
		const std::size_t at = in.offset();
		if (at + 4 != stream.size())
		{
			return std::nullopt;
		}
		const std::uint32_t expected = (static_cast<std::uint32_t>(stream[at]) << 24) | (static_cast<std::uint32_t>(stream[at + 1]) << 16) |
			(static_cast<std::uint32_t>(stream[at + 2]) << 8) | stream[at + 3];
		if (expected != adler32(out.data(), out.size()))
		{
			return std::nullopt;
		}
		return out;
	}

	std::uint32_t bigEndian(const std::vector<std::uint8_t>& bytes, std::size_t at)
	{
		return (static_cast<std::uint32_t>(bytes[at]) << 24) | (static_cast<std::uint32_t>(bytes[at + 1]) << 16) |
			(static_cast<std::uint32_t>(bytes[at + 2]) << 8) | bytes[at + 3];
	}

	struct DecodedPng
	{
		scene::TextureImage image;
		int colorType = 0;
		std::vector<std::string> chunks;
	};

	// Chunks with their CRCs checked, IDAT inflated, rows unfiltered; RGB read as opaque RGBA.
	std::optional<DecodedPng> decodePng(const std::vector<std::uint8_t>& png)
	{
		static const std::uint8_t signature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
		if (png.size() < 8 || std::memcmp(png.data(), signature, 8) != 0)
		{
			return std::nullopt;
		}
		DecodedPng decoded;
		std::vector<std::uint8_t> idat;
		std::size_t at = 8;
		while (at + 12 <= png.size())
		{
			const std::uint32_t length = bigEndian(png, at);
			if (at + 12 + length > png.size() || crc32(png.data() + at + 4, 4 + length) != bigEndian(png, at + 8 + length))
			{
				return std::nullopt;
			}
			const std::string type(reinterpret_cast<const char*>(png.data() + at + 4), 4);
			decoded.chunks.push_back(type);
			if (type == "IHDR")
			{
				decoded.image.width = static_cast<int>(bigEndian(png, at + 8));
				decoded.image.height = static_cast<int>(bigEndian(png, at + 12));
				decoded.colorType = png[at + 17];
			}
			else if (type == "IDAT")
			{
				idat.insert(idat.end(), png.begin() + static_cast<std::ptrdiff_t>(at + 8), png.begin() + static_cast<std::ptrdiff_t>(at + 8 + length));
			}
			at += 12 + length;
		}
		const std::optional<std::vector<std::uint8_t>> raw = inflateZlib(idat);
		const int channels = decoded.colorType == 6 ? 4 : 3;
		const std::size_t rowBytes = static_cast<std::size_t>(decoded.image.width) * static_cast<std::size_t>(channels);
		if (!raw || raw->size() != (rowBytes + 1) * static_cast<std::size_t>(decoded.image.height))
		{
			return std::nullopt;
		}
		std::vector<std::uint8_t> previous(rowBytes, 0);
		std::vector<std::uint8_t> row(rowBytes);
		for (int y = 0; y < decoded.image.height; ++y)
		{
			const std::uint8_t* line = raw->data() + static_cast<std::size_t>(y) * (rowBytes + 1);
			const int filter = line[0];
			for (std::size_t i = 0; i < rowBytes; ++i)
			{
				const int a = i >= static_cast<std::size_t>(channels) ? row[i - static_cast<std::size_t>(channels)] : 0;
				const int b = previous[i];
				const int c = i >= static_cast<std::size_t>(channels) ? previous[i - static_cast<std::size_t>(channels)] : 0;
				int predictor = 0;
				if (filter == 1) predictor = a;
				if (filter == 2) predictor = b;
				if (filter == 3) predictor = (a + b) / 2;
				if (filter == 4)
				{
					const int p = a + b - c;
					const int pa = std::abs(p - a);
					const int pb = std::abs(p - b);
					const int pc = std::abs(p - c);
					predictor = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
				}
				row[i] = static_cast<std::uint8_t>((line[1 + i] + predictor) & 0xFF);
			}
			for (int x = 0; x < decoded.image.width; ++x)
			{
				for (int ch = 0; ch < 4; ++ch)
				{
					decoded.image.rgba.push_back(ch < channels ? row[static_cast<std::size_t>(x * channels + ch)] : std::uint8_t{255});
				}
			}
			previous = row;
		}
		return decoded;
	}
}

TEST_CASE("CRC-32 and Adler-32 give the reference values")
{
	const std::string digits = "123456789";
	CHECK(crc32(reinterpret_cast<const std::uint8_t*>(digits.data()), digits.size()) == 0xCBF43926u);
	const std::string word = "Wikipedia";
	CHECK(adler32(reinterpret_cast<const std::uint8_t*>(word.data()), word.size()) == 0x11E60398u);
	CHECK(adler32(nullptr, 0) == 1u);
}

TEST_CASE("the zlib stream inflates back to its data: empty, text, long runs, noise")
{
	std::mt19937 random(7);
	std::vector<std::uint8_t> noise(70000);
	for (std::uint8_t& byte : noise)
	{
		byte = static_cast<std::uint8_t>(random() & 0xFF);
	}
	std::vector<std::uint8_t> runs;
	for (int i = 0; i < 100000; ++i)
	{
		runs.push_back(static_cast<std::uint8_t>((i / 700) % 5)); // matches up to 258 bytes, far back
	}
	const std::string sentence = "Muros, suelos y forjados: 101st Floor exporta la casa a Twinmotion. Muros, suelos...";
	for (const std::vector<std::uint8_t>& data : {std::vector<std::uint8_t>{}, std::vector<std::uint8_t>(sentence.begin(), sentence.end()),
			 runs, noise})
	{
		CAPTURE(data.size());
		const std::vector<std::uint8_t> compressed = zlibCompress(data);
		const std::optional<std::vector<std::uint8_t>> back = inflateZlib(compressed);
		REQUIRE(back.has_value());
		CHECK(*back == data);
		if (data.size() == runs.size())
		{
			CHECK(compressed.size() * 20 < data.size()); // long runs cost almost nothing
		}
	}
}

TEST_CASE("a PNG holds the image exactly: RGB when opaque, RGBA otherwise, sRGB, deterministic")
{
	scene::TextureImage image;
	image.width = 37;
	image.height = 23;
	for (int y = 0; y < image.height; ++y)
	{
		for (int x = 0; x < image.width; ++x)
		{
			image.rgba.insert(image.rgba.end(), {static_cast<std::uint8_t>(x * 7), static_cast<std::uint8_t>(y * 11),
				static_cast<std::uint8_t>((x * y) & 0xFF), 255});
		}
	}
	const std::vector<std::uint8_t> png = encodePng(image);
	const Result<assets::ImageInfo> info = assets::readImageInfo(png.data(), png.size());
	REQUIRE(info.ok());
	CHECK(info.value().format == assets::ImageFormat::Png);
	CHECK(info.value().width == 37);
	CHECK(info.value().height == 23);
	const std::optional<DecodedPng> decoded = decodePng(png);
	REQUIRE(decoded.has_value());
	CHECK(decoded->colorType == 2);
	CHECK(decoded->chunks == std::vector<std::string>{"IHDR", "sRGB", "IDAT", "IEND"});
	CHECK(decoded->image.rgba == image.rgba);
	CHECK(encodePng(image) == png);

	image.rgba[3] = 128; // one translucent pixel: RGBA
	const std::optional<DecodedPng> translucent = decodePng(encodePng(image));
	REQUIRE(translucent.has_value());
	CHECK(translucent->colorType == 6);
	CHECK(translucent->image.rgba == image.rgba);
}

TEST_CASE("a synthesised finish texture compresses well and decodes to the same pixels")
{
	const scene::FinishLook bricks = scene::buildFinishLook("brick-red");
	const scene::TextureImage& top = bricks.texture.mips.front();
	const std::vector<std::uint8_t> png = encodePng(top);
	CHECK(png.size() * 2 < top.rgba.size() * 3 / 4); // well under half its RGB bytes
	const std::optional<DecodedPng> decoded = decodePng(png);
	REQUIRE(decoded.has_value());
	CHECK(decoded->image.width == top.width);
	CHECK(decoded->image.rgba == top.rgba);
}
