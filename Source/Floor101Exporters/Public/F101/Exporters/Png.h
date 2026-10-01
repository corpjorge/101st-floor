#pragma once

#include "F101/Cad/Export.h"
#include "F101/Scene/FinishTextures.h"

#include <cstdint>
#include <vector>

#ifndef FLOOR101EXPORTERS_API
	#define FLOOR101EXPORTERS_API
#endif

// PNG encoding for exporters (ADR-0020): formats such as glTF carry images as PNG or JPEG files. Written for this project
// (zlib stream with LZ77 and the fixed Huffman codes of RFC 1951): no third-party library.
namespace f101::exporters
{
	// An 8-bit sRGB PNG of the image: RGB when every pixel is opaque, else RGBA. Each row takes the filter that makes it
	// smallest. Deterministic: the same image gives the same bytes.
	FLOOR101EXPORTERS_API std::vector<std::uint8_t> encodePng(const scene::TextureImage& image);

	// The zlib stream (RFC 1950) of `data`: one fixed-Huffman deflate block with back-references up to 32 KB away.
	FLOOR101EXPORTERS_API std::vector<std::uint8_t> zlibCompress(const std::vector<std::uint8_t>& data);

	// CRC-32 (PNG chunks) and Adler-32 (zlib streams) of a run of bytes.
	FLOOR101EXPORTERS_API std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0);
	FLOOR101EXPORTERS_API std::uint32_t adler32(const std::uint8_t* data, std::size_t size);
}
