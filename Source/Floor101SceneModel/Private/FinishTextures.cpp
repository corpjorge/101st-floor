#include "F101/Scene/FinishTextures.h"

#include "F101/Cad/Assert.h"

#include <algorithm>
#include <cmath>

namespace f101::scene
{
	namespace
	{
		// Integer hash (lowbias32) for per-unit variation and noise lattices.
		std::uint32_t hash(std::uint32_t x)
		{
			x ^= x >> 16;
			x *= 0x7feb352dU;
			x ^= x >> 15;
			x *= 0x846ca68bU;
			x ^= x >> 16;
			return x;
		}

		std::uint32_t hash2(std::int64_t a, std::int64_t b)
		{
			return hash(static_cast<std::uint32_t>(a) * 0x9e3779b9U ^ hash(static_cast<std::uint32_t>(b)));
		}

		// Uniform in [-1, 1].
		double signedUnit(std::uint32_t h)
		{
			return static_cast<double>(h & 0xffffffU) / static_cast<double>(0xffffffU) * 2.0 - 1.0;
		}

		// Tileable value noise in [-1, 1] over a lattice of `cells` x `cells` covering the unit square.
		double valueNoise(double u, double v, int cells, std::uint32_t seed)
		{
			const double x = u * cells;
			const double y = v * cells;
			const int x0 = static_cast<int>(std::floor(x));
			const int y0 = static_cast<int>(std::floor(y));
			const double fx = x - x0;
			const double fy = y - y0;
			auto at = [&](int i, int j)
			{
				const int wi = ((i % cells) + cells) % cells;
				const int wj = ((j % cells) + cells) % cells;
				return signedUnit(hash2(wi, wj * 7919 + static_cast<std::int64_t>(seed)));
			};
			const double sx = fx * fx * (3.0 - 2.0 * fx);
			const double sy = fy * fy * (3.0 - 2.0 * fy);
			const double top = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * sx;
			const double bottom = at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * sx;
			return top + (bottom - top) * sy;
		}

		struct Shade
		{
			double r = 0.0;
			double g = 0.0;
			double b = 0.0;
		};

		Shade toShade(cad::Rgb8 c)
		{
			return Shade{static_cast<double>(c.r), static_cast<double>(c.g), static_cast<double>(c.b)};
		}

		// Brightness change by a factor (1 = unchanged).
		Shade scaled(Shade s, double factor)
		{
			return Shade{s.r * factor, s.g * factor, s.b * factor};
		}

		void put(TextureImage& image, int x, int y, Shade s)
		{
			auto channel = [](double value) { return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L)); };
			std::uint8_t* p = &image.rgba[(static_cast<std::size_t>(y) * image.width + x) * 4];
			p[0] = channel(s.r);
			p[1] = channel(s.g);
			p[2] = channel(s.b);
			p[3] = 255;
		}

		// Position within a joint-separated unit along one axis: which unit and whether the point is on the unit
		// (not in the joint).
		struct Cell
		{
			std::int64_t index = 0;
			bool onUnit = true;
		};

		Cell cellAt(double mm, double unitMm, double jointMm)
		{
			const double module = unitMm + jointMm;
			const double index = std::floor(mm / module);
			return Cell{static_cast<std::int64_t>(index), mm - index * module < unitMm};
		}

		// Units per repeat (per axis) for the patterns with units: enough to vary colours without an obvious repeat.
		constexpr int UnitsAcross = 4;
		constexpr int BrickCourses = 8;
		constexpr int PlankRows = 6;
		constexpr int PlanksAcross = 3;

		Shade sample(const cad::FinishInfo& finish, const FinishTexture& texture, double u, double v)
		{
			const Shade base = toShade(finish.color);
			const Shade joint = toShade(finish.jointColor);
			const double unitW = static_cast<double>(finish.unitWidthMm);
			const double unitH = static_cast<double>(finish.unitHeightMm);
			const double gap = static_cast<double>(finish.jointMm);
			const double xMm = u * texture.repeatWidthMm;
			const double yMm = v * texture.repeatHeightMm;
			switch (finish.pattern)
			{
			case cad::FinishPattern::Plain:
				return base;
			case cad::FinishPattern::Speckle:
				return scaled(base, 1.0 + 0.05 * valueNoise(u, v, 16, 1) + 0.04 * valueNoise(u, v, 128, 2));
			case cad::FinishPattern::Bricks:
			{
				const Cell course = cellAt(yMm, unitH, gap);
				const double offset = (course.index % 2 != 0) ? (unitW + gap) / 2.0 : 0.0;
				const Cell brick = cellAt(xMm + offset, unitW, gap);
				if (!course.onUnit || !brick.onUnit)
				{
					return scaled(joint, 1.0 + 0.04 * valueNoise(u, v, 64, 3));
				}
				const std::int64_t column = ((brick.index % UnitsAcross) + UnitsAcross) % UnitsAcross;
				const double variation = 0.12 * signedUnit(hash2(course.index, column));
				return scaled(base, 1.0 + variation + 0.05 * valueNoise(u, v, 96, 4));
			}
			case cad::FinishPattern::Tiles:
			{
				const Cell row = cellAt(yMm, unitH, gap);
				const Cell column = cellAt(xMm, unitW, gap);
				if (!row.onUnit || !column.onUnit)
				{
					return joint;
				}
				return scaled(base, 1.0 + 0.03 * signedUnit(hash2(row.index, column.index)) + 0.01 * valueNoise(u, v, 64, 5));
			}
			case cad::FinishPattern::Planks:
			{
				// Rows of planks along u; each row's butt joints are staggered.
				static constexpr double Stagger[PlankRows] = {0.0, 0.5, 0.25, 0.75, 0.125, 0.625};
				const Cell row = cellAt(yMm, unitH, gap);
				const double shift = Stagger[((row.index % PlankRows) + PlankRows) % PlankRows] * (unitW + gap);
				const Cell plank = cellAt(xMm + shift, unitW, gap);
				if (!row.onUnit || !plank.onUnit)
				{
					return joint;
				}
				// Modulo the planks per repeat, so the plank across the seam has one colour.
				const std::uint32_t id = hash2(row.index, ((plank.index % PlanksAcross) + PlanksAcross) % PlanksAcross);
				const double across = (yMm - static_cast<double>(row.index) * (unitH + gap)) / unitH; // 0..1 across the plank
				const double grain = std::sin((across * 9.0 + 2.5 * valueNoise(u, v, 8, id & 1023U)) * 3.14159265358979) * 0.06;
				return scaled(base, 1.0 + 0.1 * signedUnit(id) + grain + 0.03 * valueNoise(u, v, 128, 6));
			}
			}
			return base;
		}

		TextureImage halve(const TextureImage& image)
		{
			TextureImage half;
			half.width = std::max(1, image.width / 2);
			half.height = std::max(1, image.height / 2);
			half.rgba.resize(static_cast<std::size_t>(half.width) * half.height * 4);
			for (int y = 0; y < half.height; ++y)
			{
				for (int x = 0; x < half.width; ++x)
				{
					for (int c = 0; c < 4; ++c)
					{
						int sum = 0;
						for (const auto& [dx, dy] : {std::pair{0, 0}, {1, 0}, {0, 1}, {1, 1}})
						{
							const int sx = std::min(image.width - 1, x * 2 + dx);
							const int sy = std::min(image.height - 1, y * 2 + dy);
							sum += image.rgba[(static_cast<std::size_t>(sy) * image.width + sx) * 4 + c];
						}
						half.rgba[(static_cast<std::size_t>(y) * half.width + x) * 4 + c] = static_cast<std::uint8_t>((sum + 2) / 4);
					}
				}
			}
			return half;
		}
	}

	FinishTexture buildFinishTexture(const cad::FinishInfo& finish, int size)
	{
		F101_ASSERT(size >= 4 && (size & (size - 1)) == 0);
		FinishTexture texture;
		const double unitW = static_cast<double>(finish.unitWidthMm + finish.jointMm);
		const double unitH = static_cast<double>(finish.unitHeightMm + finish.jointMm);
		switch (finish.pattern)
		{
		case cad::FinishPattern::Plain:
			size = 4; // one colour: no detail to keep
			break;
		case cad::FinishPattern::Speckle:
			texture.repeatWidthMm = texture.repeatHeightMm = 1000.0;
			break;
		case cad::FinishPattern::Bricks:
			texture.repeatWidthMm = UnitsAcross * unitW;
			texture.repeatHeightMm = BrickCourses * unitH;
			break;
		case cad::FinishPattern::Tiles:
			texture.repeatWidthMm = UnitsAcross * unitW;
			texture.repeatHeightMm = UnitsAcross * unitH;
			break;
		case cad::FinishPattern::Planks:
			texture.repeatWidthMm = PlanksAcross * unitW;
			texture.repeatHeightMm = PlankRows * unitH;
			break;
		}

		TextureImage image;
		image.width = size;
		image.height = size;
		image.rgba.resize(static_cast<std::size_t>(size) * size * 4);
		for (int y = 0; y < size; ++y)
		{
			for (int x = 0; x < size; ++x)
			{
				// Pixel centres, as fractions of the repeat.
				const double u = (x + 0.5) / size;
				const double v = (y + 0.5) / size;
				put(image, x, y, sample(finish, texture, u, v));
			}
		}
		texture.mips.push_back(std::move(image));
		while (texture.mips.back().width > 1 || texture.mips.back().height > 1)
		{
			texture.mips.push_back(halve(texture.mips.back()));
		}
		return texture;
	}

	FinishLook buildFinishLook(std::string_view finishId, int size)
	{
		if (const cad::FinishInfo* finish = cad::findFinish(finishId))
		{
			return FinishLook{finish->roughness, buildFinishTexture(*finish, size)};
		}
		cad::FinishInfo missing;
		missing.color = cad::Rgb8{255, 0, 255};
		return FinishLook{0.85, buildFinishTexture(missing, size)};
	}
}
