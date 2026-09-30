#pragma once

#include "F101/Assets/Files.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

// Imported textures (ADR-0017): what kind of image a file is and its size, read from its header. Decoding the pixels is
// the renderer's job; the model only needs to know the file is a usable image.
namespace f101::assets
{
	enum class ImageFormat
	{
		Png,
		Jpeg,
	};

	struct ImageInfo
	{
		ImageFormat format = ImageFormat::Png;
		int width = 0;
		int height = 0;
	};

	// Largest side accepted, in pixels.
	inline constexpr int MaxImageSide = 16384;

	// A PNG or JPEG image and its size; an error for anything else, a damaged header or a side over MaxImageSide.
	FLOOR101ASSETS_API Result<ImageInfo> readImageInfo(const std::uint8_t* data, std::size_t size);

	// "png" or "jpg".
	FLOOR101ASSETS_API std::string_view imageExtension(ImageFormat format);
}
