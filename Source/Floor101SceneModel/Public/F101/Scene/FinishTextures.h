#pragma once

#include "F101/Cad/Finishes.h"

#include <cstdint>
#include <string_view>
#include <vector>

#ifndef FLOOR101SCENEMODEL_API
	#define FLOOR101SCENEMODEL_API
#endif

// Colour textures of the finishes (ADR-0012), synthesised from the catalogue's pattern description so every consumer
// (Unreal today, exporters later) shows the same bricks, tiles and planks without image assets. Deterministic.
namespace f101::scene
{
	struct TextureImage
	{
		int width = 0;
		int height = 0;
		std::vector<std::uint8_t> rgba; // sRGB, 4 bytes per pixel, row by row; alpha 255
	};

	// One repeat of a finish: the texture tiles seamlessly, and covers repeatWidthMm along the surface's u (along a
	// wall, or plan x) and repeatHeightMm along v (up a wall, or plan y). Mesh UVs are in metres, so a material samples
	// it at uv * 1000 / repeat.
	struct FinishTexture
	{
		double repeatWidthMm = 1000.0;
		double repeatHeightMm = 1000.0;
		std::vector<TextureImage> mips; // [0] is size x size; each next level halves it, down to 1 x 1
	};

	// `size` must be a power of two (at least 4).
	FLOOR101SCENEMODEL_API FinishTexture buildFinishTexture(const cad::FinishInfo& finish, int size = 512);

	// A decoded image (ADR-0017) as a tiling texture: resampled to the nearest power of two on each side (at most
	// `maxSize`, a power of two) with wrap-around filtering, and its whole mip chain. `flipVertically` puts the image's top
	// row at the far end of v (up a wall, north on a floor), as a surface texture needs; model textures keep their rows.
	FLOOR101SCENEMODEL_API FinishTexture textureFromImage(const TextureImage& image, double repeatWidthMm,
		double repeatHeightMm, bool flipVertically, int maxSize = 2048);

	// Everything a renderer needs to show a finish (SceneNode::finishes): roughness, opacity and colour texture. An id
	// that is not in the catalogue gives flat magenta, so a missing finish shows instead of passing unnoticed.
	struct FinishLook
	{
		double roughness = 0.85;
		double opacity = 1.0; // less than 1: see-through (glass), for a translucent material
		FinishTexture texture;
		double metallic = 0.0;
	};

	FLOOR101SCENEMODEL_API FinishLook buildFinishLook(std::string_view finishId, int size = 512);
}
