#include "F101/Geometry/FurnitureBuilder.h"

#include "F101/Geometry/Rooms.h"
#include "MeshBuilding.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace f101::geometry
{
	using namespace detail;

	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		// Builds a shape in the piece's frame; w, d, h are half the width, half the depth and the height.
		struct Maker
		{
			FurnitureShape shape;
			double w = 0.0;
			double d = 0.0;
			double h = 0.0;

			void box(const char* slot, double x0, double x1, double y0, double y1, double z0, double z1)
			{
				if (x1 > x0 && y1 > y0 && z1 > z0)
				{
					shape.boxes.push_back(FurnitureBox{slot, x0, x1, y0, y1, z0, z1});
				}
			}

			// A box open at the top: a bottom `bottom` thick and four sides `side` thick.
			void hollow(const char* slot, double x0, double x1, double y0, double y1, double z0, double z1, double side, double bottom)
			{
				box(slot, x0, x1, y0, y1, z0, z0 + bottom);
				box(slot, x0, x1, y0, y0 + side, z0 + bottom, z1);
				box(slot, x0, x1, y1 - side, y1, z0 + bottom, z1);
				box(slot, x0, x0 + side, y0 + side, y1 - side, z0 + bottom, z1);
				box(slot, x1 - side, x1, y0 + side, y1 - side, z0 + bottom, z1);
			}

			// A slab with a rectangular hole (hx0..hx1, hy0..hy1) for a sink or a basin.
			void slabWithHole(const char* slot, double x0, double x1, double y0, double y1, double z0, double z1, double hx0,
				double hx1, double hy0, double hy1)
			{
				box(slot, x0, x1, y0, hy0, z0, z1);
				box(slot, x0, x1, hy1, y1, z0, z1);
				box(slot, x0, hx0, hy0, hy1, z0, z1);
				box(slot, hx1, x1, hy0, hy1, z0, z1);
			}

			void rect(double x0, double x1, double y0, double y1)
			{
				if (x1 > x0 && y1 > y0)
				{
					shape.planDetails.push_back({{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}});
				}
			}

			void ellipse(double cx, double cy, double rx, double ry)
			{
				if (rx <= 0.0 || ry <= 0.0)
				{
					return;
				}
				constexpr int Segments = 24;
				FurnitureOutline outline;
				for (int i = 0; i < Segments; ++i)
				{
					const double angle = 2.0 * Pi * i / Segments;
					outline.push_back({cx + rx * std::cos(angle), cy + ry * std::sin(angle)});
				}
				shape.planDetails.push_back(std::move(outline));
			}

			// `count` equal spans across x0..x1 separated by `gap`: calls span(from, to). The last one ends exactly at x1.
			static void split(double x0, double x1, int count, double gap, const std::function<void(double, double)>& span)
			{
				const double each = (x1 - x0 - gap * (count - 1)) / count;
				for (int i = 0; i < count; ++i)
				{
					const double from = x0 + i * (each + gap);
					span(from, i + 1 == count ? x1 : from + each);
				}
			}
		};

		int modules(double width, double module)
		{
			return std::max(1, static_cast<int>(std::lround(width / module)));
		}

		void bed(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			m.box(furniture_slots::Body, -w, w, d - 60.0, d, 0.0, h); // headboard
			const double baseTop = std::min(350.0, h * 0.4);
			const double mattressTop = baseTop + 200.0;
			for (const double x : {-w + 40.0, w - 100.0})
			{
				for (const double y : {-d + 40.0, d - 160.0})
				{
					m.box(furniture_slots::Body, x, x + 60.0, y, y + 60.0, 0.0, 150.0);
				}
			}
			m.box(furniture_slots::Body, -w, w, -d, d - 60.0, 150.0, baseTop);
			m.box(furniture_slots::Linen, -w + 20.0, w - 20.0, -d + 20.0, d - 80.0, baseTop, mattressTop);
			const double pillowFront = d - 450.0;
			const double pillowBack = d - 100.0;
			Maker::split(-w + 60.0, w - 60.0, m.w * 2.0 >= 1200.0 ? 2 : 1, 60.0, [&](double x0, double x1)
			{
				m.box(furniture_slots::Linen, x0, x1, pillowFront, pillowBack, mattressTop, mattressTop + 120.0);
				m.rect(x0, x1, pillowFront, pillowBack);
			});
			const double duvetBack = d - 520.0;
			m.box(furniture_slots::Fabric, -w + 10.0, w - 10.0, -d + 10.0, duvetBack, baseTop + 60.0, mattressTop + 40.0);
			m.rect(-w + 10.0, w - 10.0, -d + 10.0, duvetBack);
		}

		void sofa(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			const double seat = std::min(420.0, h * 0.5);
			const double arm = std::min(180.0, w / 4.0);
			const double armTop = std::min(620.0, h - 100.0);
			const double back = std::min(200.0, d / 4.0);
			for (const double x : {-w + 60.0, w - 100.0})
			{
				for (const double y : {-d + 60.0, d - 100.0})
				{
					m.box(furniture_slots::Metal, x, x + 40.0, y, y + 40.0, 0.0, 60.0);
				}
			}
			m.box(furniture_slots::Body, -w, w, -d, d, 60.0, seat);
			m.box(furniture_slots::Body, -w, -w + arm, -d, d, seat, armTop);
			m.box(furniture_slots::Body, w - arm, w, -d, d, seat, armTop);
			m.box(furniture_slots::Body, -w + arm, w - arm, d - back, d, seat, h);
			m.rect(-w, -w + arm, -d, d);
			m.rect(w - arm, w, -d, d);
			m.rect(-w + arm, w - arm, d - back, d);
			const double cushionTop = seat + 120.0;
			Maker::split(-w + arm, w - arm, 2.0 * (w - arm) >= 1600.0 ? 3 : 2, 10.0, [&](double x0, double x1)
			{
				m.box(furniture_slots::Body, x0, x1, -d + 10.0, d - back, seat, cushionTop);
				m.box(furniture_slots::Body, x0, x1, d - back - 150.0, d - back, cushionTop, std::min(h - 60.0, cushionTop + 380.0));
				m.rect(x0, x1, -d + 10.0, d - back - 150.0);
			});
		}

		void table(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			m.box(furniture_slots::Body, -w, w, -d, d, h - 40.0, h);
			for (const double x : {-w + 60.0, w - 130.0})
			{
				for (const double y : {-d + 60.0, d - 130.0})
				{
					m.box(furniture_slots::Body, x, x + 70.0, y, y + 70.0, 0.0, h - 40.0);
				}
			}
		}

		void chair(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			const double seatTop = 460.0;
			m.box(furniture_slots::Body, -w, w, -d, d, seatTop - 40.0, seatTop);
			for (const double x : {-w + 10.0, w - 45.0})
			{
				m.box(furniture_slots::Body, x, x + 35.0, -d + 10.0, -d + 45.0, 0.0, seatTop - 40.0);
				m.box(furniture_slots::Body, x, x + 35.0, d - 45.0, d - 10.0, 0.0, h);
			}
			m.box(furniture_slots::Body, -w + 45.0, w - 45.0, d - 40.0, d - 15.0, std::max(seatTop + 150.0, h - 320.0), h - 20.0);
			m.rect(-w + 10.0, w - 10.0, d - 45.0, d - 10.0);
		}

		void wardrobe(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			m.box(furniture_slots::Body, -w, w, -d + 20.0, d, 0.0, h);
			const int doors = modules(2.0 * w, 550.0);
			int index = 0;
			Maker::split(-w, w, doors, 4.0, [&](double x0, double x1)
			{
				m.box(furniture_slots::Body, x0, x1, -d, -d + 20.0, 80.0, h - 10.0);
				// Handles at the meeting edges of each pair of doors.
				const double handle = index % 2 == 0 && index + 1 < doors ? x1 - 60.0 : x0 + 40.0;
				const double middle = std::min(1100.0, h / 2.0);
				m.box(furniture_slots::Metal, handle, handle + 20.0, -d - 25.0, -d, middle - 200.0, middle + 200.0);
				++index;
			});
			m.rect(-w, w, -d, -d + 20.0);
		}

		void kitchenCounter(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			const double top = h - 40.0;
			m.box(furniture_slots::Body, -w, w, -d + 60.0, d, 0.0, 100.0);
			m.hollow(furniture_slots::Body, -w, w, -d + 20.0, d, 100.0, top, 18.0, 18.0);
			Maker::split(-w, w, modules(2.0 * w, 600.0), 3.0, [&](double x0, double x1)
			{
				m.box(furniture_slots::Body, x0, x1, -d, -d + 20.0, 103.0, top - 3.0);
				const double bar = std::min(300.0, (x1 - x0) - 100.0) / 2.0;
				const double centre = (x0 + x1) / 2.0;
				m.box(furniture_slots::Metal, centre - bar, centre + bar, -d - 25.0, -d, top - 63.0, top - 43.0);
			});
			const bool withHob = 2.0 * w >= 1200.0;
			const double sinkHalf = (withHob ? std::clamp(w - 100.0, 300.0, 500.0) : std::min(500.0, 2.0 * w - 100.0)) / 2.0;
			const double sinkX = withHob ? -w / 2.0 : 0.0;
			const double sinkFront = -d + 80.0;
			const double sinkBack = d - 100.0;
			m.slabWithHole(furniture_slots::Stone, -w, w, -d, d, top, h, sinkX - sinkHalf, sinkX + sinkHalf, sinkFront, sinkBack);
			m.hollow(furniture_slots::Metal, sinkX - sinkHalf, sinkX + sinkHalf, sinkFront, sinkBack, h - 200.0, h, 10.0, 10.0);
			m.rect(sinkX - sinkHalf, sinkX + sinkHalf, sinkFront, sinkBack);
			m.box(furniture_slots::Metal, sinkX - 15.0, sinkX + 15.0, sinkBack + 20.0, sinkBack + 50.0, h, h + 250.0);
			m.box(furniture_slots::Metal, sinkX - 15.0, sinkX + 15.0, sinkBack - 130.0, sinkBack + 20.0, h + 220.0, h + 250.0);
			if (withHob)
			{
				const double hobHalf = std::min(290.0, w / 2.0 - 50.0);
				const double hobX = w / 2.0;
				const double hobFront = -d + 50.0;
				const double hobBack = std::min(d - 30.0, hobFront + 520.0);
				m.box(furniture_slots::Dark, hobX - hobHalf, hobX + hobHalf, hobFront, hobBack, h, h + 6.0);
				m.rect(hobX - hobHalf, hobX + hobHalf, hobFront, hobBack);
				const double rx = hobHalf / 2.0;
				const double ry = (hobBack - hobFront) / 4.0;
				for (const double x : {hobX - rx, hobX + rx})
				{
					for (const double y : {hobFront + ry, hobBack - ry})
					{
						m.ellipse(x, y, std::min(rx, ry) * 0.7, std::min(rx, ry) * 0.7);
					}
				}
			}
		}

		void fridge(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			m.box(furniture_slots::Dark, -w + 20.0, w - 20.0, -d + 40.0, d, 0.0, 60.0);
			m.box(furniture_slots::Body, -w, w, -d + 30.0, d, 60.0, h);
			const double split = 60.0 + (h - 60.0) * 0.35;
			m.box(furniture_slots::Body, -w + 2.0, w - 2.0, -d, -d + 30.0, 63.0, split - 3.0);
			m.box(furniture_slots::Body, -w + 2.0, w - 2.0, -d, -d + 30.0, split + 3.0, h - 3.0);
			m.box(furniture_slots::Dark, w - 60.0, w - 40.0, -d - 30.0, -d, split - 250.0, split - 50.0);
			m.box(furniture_slots::Dark, w - 60.0, w - 40.0, -d - 30.0, -d, split + 50.0, split + 350.0);
			m.rect(-w, w, -d, -d + 30.0);
		}

		void toilet(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			const double cistern = 180.0;
			const double seat = 400.0;
			m.box(furniture_slots::Body, -w, w, d - cistern, d, h - 380.0, h);
			m.box(furniture_slots::Body, -std::min(110.0, w - 40.0), std::min(110.0, w - 40.0), -d + 200.0, d - cistern, 0.0, seat - 70.0);
			m.hollow(furniture_slots::Body, -w + 10.0, w - 10.0, -d, d - cistern, seat - 70.0, seat, 40.0, 30.0);
			m.box(furniture_slots::Metal, -40.0, 40.0, d - cistern / 2.0 - 20.0, d - cistern / 2.0 + 20.0, h, h + 8.0);
			m.rect(-w, w, d - cistern, d);
			m.ellipse(0.0, (-d + d - cistern) / 2.0, w - 30.0, (2.0 * d - cistern) / 2.0 - 20.0);
		}

		void washbasin(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			const double bowlHalf = std::min(450.0, 2.0 * w - 120.0) / 2.0;
			const double bowlFront = -d + 60.0;
			const double bowlBack = d - 100.0;
			m.hollow(furniture_slots::Body, -w, w, -d, d, std::max(0.0, h - 600.0), h - 30.0, 18.0, 18.0);
			const double bar = std::min(150.0, w - 60.0);
			m.box(furniture_slots::Metal, -bar, bar, -d - 25.0, -d, h - 100.0, h - 80.0);
			m.slabWithHole(furniture_slots::Ceramic, -w, w, -d, d, h - 30.0, h, -bowlHalf, bowlHalf, bowlFront, bowlBack);
			m.hollow(furniture_slots::Ceramic, -bowlHalf, bowlHalf, bowlFront, bowlBack, h - 180.0, h, 12.0, 12.0);
			m.box(furniture_slots::Metal, -15.0, 15.0, bowlBack + 10.0, bowlBack + 40.0, h, h + 180.0);
			m.box(furniture_slots::Metal, -15.0, 15.0, bowlBack - 110.0, bowlBack + 10.0, h + 150.0, h + 180.0);
			m.ellipse(0.0, (bowlFront + bowlBack) / 2.0, bowlHalf - 12.0, (bowlBack - bowlFront) / 2.0 - 12.0);
		}

		void bathtub(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			const double rim = 70.0;
			m.hollow(furniture_slots::Body, -w, w, -d, d, 0.0, h, rim, std::max(60.0, h - 450.0));
			m.box(furniture_slots::Metal, w - 250.0, w - 220.0, d - 50.0, d - 20.0, h, h + 150.0);
			m.box(furniture_slots::Metal, w - 250.0, w - 220.0, d - 170.0, d - 50.0, h + 120.0, h + 150.0);
			m.rect(-w + rim, w - rim, -d + rim, d - rim);
		}

		void showerTray(Maker& m)
		{
			const double w = m.w, d = m.d, h = m.h;
			m.box(furniture_slots::Body, -w, w, -d, d, 0.0, h);
			m.box(furniture_slots::Metal, -60.0, 60.0, -60.0, 60.0, h, h + 2.0);
			m.rect(-60.0, 60.0, -60.0, 60.0);
		}
	}

	FurnitureShape furnitureShape(const cad::Furniture& furniture)
	{
		using Build = void (*)(Maker&);
		static const std::pair<std::string_view, Build> builders[] = {{"bed-double", bed}, {"bed-single", bed},
			{"sofa", sofa}, {"table", table}, {"chair", chair}, {"wardrobe", wardrobe}, {"kitchen-counter", kitchenCounter},
			{"fridge", fridge}, {"toilet", toilet}, {"washbasin", washbasin}, {"bathtub", bathtub},
			{"shower-tray", showerTray}};
		Maker maker;
		maker.w = static_cast<double>(furniture.widthMm) / 2.0;
		maker.d = static_cast<double>(furniture.depthMm) / 2.0;
		maker.h = static_cast<double>(furniture.heightMm);
		if (maker.w <= 0.0 || maker.d <= 0.0 || maker.h <= 0.0)
		{
			return {};
		}
		for (const auto& [type, build] : builders)
		{
			if (type == furniture.type)
			{
				build(maker);
			}
		}
		return std::move(maker.shape);
	}

	std::string_view furnitureSlotFinish(const cad::Furniture& furniture, std::string_view slot)
	{
		static const std::pair<std::string_view, std::string_view> fixed[] = {{furniture_slots::Linen, "linen-white"},
			{furniture_slots::Fabric, "fabric-sand"}, {furniture_slots::Ceramic, "ceramic-white"},
			{furniture_slots::Metal, "steel-brushed"}, {furniture_slots::Stone, "stone-dark"},
			{furniture_slots::Dark, "glass-black"}};
		for (const auto& [name, finish] : fixed)
		{
			if (name == slot)
			{
				return finish;
			}
		}
		return cad::resolveFurnitureFinish(furniture);
	}

	MeshData buildFurnitureMesh(const cad::Furniture& furniture)
	{
		MeshData mesh;
		const cad::FurnitureAxes axes = cad::furnitureAxes(furniture);
		const cad::PlanPoint centre = toPlan(furniture.position);
		for (const FurnitureBox& part : furnitureShape(furniture).boxes)
		{
			addWallBox(mesh, part.slot, centre, axes.x, axes.y, part.x0, part.x1, part.y0, part.y1,
				FloorThicknessMm + part.z0, FloorThicknessMm + part.z1);
		}
		return mesh;
	}
}
