#include "F101/Drawing/PlanDrawing.h"

#include "F101/Geometry/CarpentryBuilder.h"
#include "F101/Geometry/FurnitureBuilder.h"
#include "F101/Geometry/PolygonGeometry.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Geometry/ServicesBuilder.h"
#include "F101/Geometry/WallJoins.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>

namespace f101::drawing
{
	namespace
	{
		using cad::PlanPoint;

		constexpr double Pi = 3.14159265358979323846;
		constexpr double LineMm = 10.0;      // carpentry
		constexpr double ThinMm = 7.0;       // swings, dimension lines, extension lines
		constexpr double TickMm = 15.0;      // dimension ticks
		constexpr double TickLengthMm = 150.0;
		constexpr double ExtensionGapMm = 100.0; // between a face and its extension lines, and past the dimension line
		constexpr double PartitionOffsetMm = 300.0;
		constexpr double FurnitureLineMm = 8.0;
		constexpr double FurnitureDetailMm = 5.0;
		constexpr int ArcSegments = 16;

		PlanPoint add(PlanPoint a, PlanPoint b) { return {a.x_mm + b.x_mm, a.y_mm + b.y_mm}; }
		PlanPoint sub(PlanPoint a, PlanPoint b) { return {a.x_mm - b.x_mm, a.y_mm - b.y_mm}; }
		PlanPoint scale(PlanPoint a, double s) { return {a.x_mm * s, a.y_mm * s}; }
		double dot(PlanPoint a, PlanPoint b) { return a.x_mm * b.x_mm + a.y_mm * b.y_mm; }

		// A wall's frame: u along its axis from its start, s across it towards its left face.
		struct Frame
		{
			PlanPoint start;
			PlanPoint axis; // unit
			PlanPoint left; // unit
			double length = 0.0;

			PlanPoint at(double u, double s) const { return add(start, add(scale(axis, u), scale(left, s))); }
			double along(PlanPoint p) const { return dot(sub(p, start), axis); }
			double across(PlanPoint p) const { return dot(sub(p, start), left); }
		};

		Frame frameOf(const cad::Wall& wall)
		{
			Frame frame;
			frame.start = PlanPoint{static_cast<double>(wall.start.x_mm), static_cast<double>(wall.start.y_mm)};
			const PlanPoint vector{static_cast<double>(wall.end.x_mm - wall.start.x_mm), static_cast<double>(wall.end.y_mm - wall.start.y_mm)};
			frame.length = std::hypot(vector.x_mm, vector.y_mm);
			frame.axis = scale(vector, 1.0 / frame.length);
			frame.left = PlanPoint{-frame.axis.y_mm, frame.axis.x_mm};
			return frame;
		}

		// Two decimals of a whole number of hundredths: "3.55", "-0.07".
		std::string formatHundredths(long long hundredths, char separator)
		{
			const long long magnitude = std::llabs(hundredths);
			const std::string cents = std::to_string(magnitude % 100);
			return std::string(hundredths < 0 ? "-" : "") + std::to_string(magnitude / 100) + separator + (cents.size() < 2 ? "0" : "") + cents;
		}

		class Builder
		{
		public:
			explicit Builder(PlanDrawing& target) : drawing(target) {}

			void fill(std::vector<PlanPoint> outline, Ink ink)
			{
				// A piece ending at a mitre can repeat a corner: drop repeated points before triangulating.
				outline.erase(std::unique(outline.begin(), outline.end(), [](PlanPoint a, PlanPoint b)
				{
					return std::fabs(a.x_mm - b.x_mm) < 0.01 && std::fabs(a.y_mm - b.y_mm) < 0.01;
				}), outline.end());
				while (outline.size() > 1 && std::fabs(outline.front().x_mm - outline.back().x_mm) < 0.01 &&
					std::fabs(outline.front().y_mm - outline.back().y_mm) < 0.01)
				{
					outline.pop_back();
				}
				if (outline.size() < 3)
				{
					return;
				}
				if (geometry::signedArea(outline) < 0.0)
				{
					std::reverse(outline.begin(), outline.end());
				}
				std::vector<std::uint32_t> triangles = geometry::triangulatePolygon(outline);
				if (triangles.empty())
				{
					return; // degenerate
				}
				drawing.fills.push_back(Fill{std::move(outline), std::move(triangles), ink});
			}

			void stroke(std::vector<PlanPoint> points, Ink ink, double widthMm, bool closed = false, bool dashed = false)
			{
				drawing.strokes.push_back(Stroke{std::move(points), closed, dashed, ink, widthMm});
			}

			void label(PlanPoint at, double angleDegrees, double heightMm, std::string text, Ink ink)
			{
				drawing.labels.push_back(Label{at, angleDegrees, heightMm, std::move(text), ink});
			}

			void square(PlanPoint centre, double size, Ink ink)
			{
				const double half = size / 2.0;
				fill({add(centre, {-half, -half}), add(centre, {half, -half}), add(centre, {half, half}), add(centre, {-half, half})}, ink);
			}

			// Quarter circle round `pivot` from `from` (unit) to `to` (unit, perpendicular), radius `radius`.
			void arc(PlanPoint pivot, PlanPoint from, PlanPoint to, double radius, Ink ink, double widthMm)
			{
				std::vector<PlanPoint> points;
				for (int i = 0; i <= ArcSegments; ++i)
				{
					const double angle = Pi / 2.0 * i / ArcSegments;
					points.push_back(add(pivot, scale(add(scale(from, std::cos(angle)), scale(to, std::sin(angle))), radius)));
				}
				stroke(std::move(points), ink, widthMm);
			}

		private:
			PlanDrawing& drawing;
		};

		bool isCut(const cad::Opening& opening)
		{
			if (opening.kind == cad::OpeningKind::Door)
			{
				return true;
			}
			const double sill = static_cast<double>(opening.sillMm);
			return sill < CutHeightMm && sill + static_cast<double>(opening.heightMm) > CutHeightMm;
		}

		// A wall of the drawn level with how its cut is inked.
		struct DrawnWall
		{
			cad::Wall wall;
			Ink ink = Ink::Cut;
		};

		// The cut of a wall: its footprint minus the openings the plan cuts, and each cut opening's symbol.
		void drawWall(Builder& builder, const DrawnWall& drawn, const geometry::WallFootprint& footprint)
		{
			const cad::Wall& wall = drawn.wall;
			const Frame frame = frameOf(wall);
			const double rightS = frame.across(footprint.startRight);
			const double leftS = frame.across(footprint.startLeft);
			auto rightAt = [&](double u) { return frame.at(u, rightS); };
			auto leftAt = [&](double u) { return frame.at(u, leftS); };
			const double faceStart = std::max(frame.along(footprint.startRight), frame.along(footprint.startLeft));
			const double faceEnd = std::min(frame.along(footprint.endRight), frame.along(footprint.endLeft));

			std::optional<double> from; // nullopt: the wall start
			for (const cad::Opening& opening : wall.openings)
			{
				if (!isCut(opening))
				{
					continue;
				}
				const double u0 = std::max(static_cast<double>(opening.offsetMm), faceStart);
				const double u1 = std::min(static_cast<double>(opening.offsetMm + opening.widthMm), faceEnd);
				if (u1 <= u0)
				{
					continue;
				}
				if (!from)
				{
					builder.fill({footprint.startRight, rightAt(u0), leftAt(u0), footprint.startLeft}, drawn.ink);
				}
				else if (u0 > *from)
				{
					builder.fill({rightAt(*from), rightAt(u0), leftAt(u0), leftAt(*from)}, drawn.ink);
				}
				from = u1;

				// The symbol.
				const double sigma = opening.opensTo == cad::WallSide::Left ? 1.0 : -1.0;
				const double frameWidth = std::min(geometry::FrameWidthMm, (u1 - u0) / 4.0);
				const double depth = std::min(geometry::FrameDepthMm, static_cast<double>(wall.thicknessMm)) / 2.0;
				if (opening.kind == cad::OpeningKind::Window)
				{
					builder.stroke({rightAt(u0), rightAt(u1)}, Ink::Carpentry, LineMm); // sill lines on both faces
					builder.stroke({leftAt(u0), leftAt(u1)}, Ink::Carpentry, LineMm);
					builder.stroke({frame.at(u0, -depth), frame.at(u1, -depth), frame.at(u1, depth), frame.at(u0, depth)}, Ink::Carpentry,
						LineMm, true); // the frame
					if (opening.operation == cad::OpeningOperation::Sliding)
					{
						const double middle = (u0 + u1) / 2.0;
						const double overlap = std::min(geometry::SlidingOverlapMm, (u1 - u0) / 4.0) / 2.0;
						builder.stroke({frame.at(u0, depth / 2.0), frame.at(middle + overlap, depth / 2.0)}, Ink::Carpentry, LineMm);
						builder.stroke({frame.at(middle - overlap, -depth / 2.0), frame.at(u1, -depth / 2.0)}, Ink::Carpentry, LineMm);
					}
					else
					{
						builder.stroke({frame.at(u0, 0.0), frame.at(u1, 0.0)}, Ink::Carpentry, LineMm); // the glass
					}
					continue;
				}
				const double leafU0 = u0 + frameWidth;
				const double leafU1 = u1 - frameWidth;
				if (leafU1 <= leafU0)
				{
					continue;
				}
				const double thickness = std::min(geometry::LeafThicknessMm, 2.0 * depth);
				if (opening.operation == cad::OpeningOperation::Sliding)
				{
					// The leaf closed in the gap, and its pocket inside the wall, dashed along the axis.
					builder.stroke({frame.at(leafU0, -thickness / 2.0), frame.at(leafU1, -thickness / 2.0), frame.at(leafU1, thickness / 2.0),
						frame.at(leafU0, thickness / 2.0)}, Ink::Carpentry, LineMm, true);
					const cad::AxisSpan taken = cad::occupiedSpan(opening);
					const double pocket0 = opening.hinge == cad::OpeningEnd::Start ? static_cast<double>(taken.fromMm) : static_cast<double>(opening.offsetMm + opening.widthMm);
					const double pocket1 = opening.hinge == cad::OpeningEnd::Start ? static_cast<double>(opening.offsetMm) : static_cast<double>(taken.toMm);
					builder.stroke({frame.at(pocket0, 0.0), frame.at(pocket1, 0.0)}, Ink::Hidden, LineMm, false, true);
					continue;
				}
				// Hinged leaves drawn open at 90 degrees from their hinge on the face they open to, with the arc of their free edge.
				const double face = sigma > 0.0 ? leftS : rightS;
				const PlanPoint out = scale(frame.left, sigma);
				auto leaf = [&](double hingeU, bool fromStart, double width)
				{
					const PlanPoint along = fromStart ? frame.axis : scale(frame.axis, -1.0);
					const PlanPoint pivot = frame.at(hingeU, face);
					builder.stroke({pivot, add(pivot, scale(out, width)), add(add(pivot, scale(out, width)), scale(along, thickness)),
						add(pivot, scale(along, thickness))}, Ink::Carpentry, LineMm, true);
					builder.arc(pivot, along, out, width, Ink::Swing, ThinMm);
				};
				if (opening.operation == cad::OpeningOperation::DoubleHinged)
				{
					const double middle = (leafU0 + leafU1) / 2.0;
					leaf(leafU0, true, middle - leafU0);
					leaf(leafU1, false, leafU1 - middle);
				}
				else
				{
					const bool fromStart = opening.hinge == cad::OpeningEnd::Start;
					leaf(fromStart ? leafU0 : leafU1, fromStart, leafU1 - leafU0);
				}
			}
			if (!from)
			{
				builder.fill({footprint.startRight, footprint.endRight, footprint.endLeft, footprint.startLeft}, drawn.ink);
			}
			else
			{
				builder.fill({rightAt(*from), footprint.endRight, footprint.endLeft, leftAt(*from)}, drawn.ink); // nothing if flat
			}
		}

		// A dimension line along a wall at `s` (across its axis), through the points `us` (along it), on the side `side`
		// (+1 left, -1 right) of the face `faceS`.
		void drawDimension(Builder& builder, const Frame& frame, std::vector<double> us, double faceS, double s, double side,
			const DrawingTexts& texts)
		{
			std::sort(us.begin(), us.end());
			us.erase(std::unique(us.begin(), us.end(), [](double a, double b) { return std::fabs(a - b) < 1.0; }), us.end());
			if (us.size() < 2)
			{
				return;
			}
			builder.stroke({frame.at(us.front(), s), frame.at(us.back(), s)}, Ink::Dimension, ThinMm);
			const PlanPoint tick = scale(add(frame.axis, scale(frame.left, side)), TickLengthMm / 2.0 / std::sqrt(2.0));
			for (const double u : us)
			{
				builder.stroke({frame.at(u, faceS + side * ExtensionGapMm), frame.at(u, s + side * ExtensionGapMm)}, Ink::Dimension, ThinMm);
				const PlanPoint centre = frame.at(u, s);
				builder.stroke({sub(centre, tick), add(centre, tick)}, Ink::Dimension, TickMm);
			}
			// Figures read upright: along the axis, turned by at most 90 degrees.
			double angle = std::atan2(frame.axis.y_mm, frame.axis.x_mm) * 180.0 / Pi;
			if (angle > 90.0)
			{
				angle -= 180.0;
			}
			else if (angle <= -90.0)
			{
				angle += 180.0;
			}
			for (std::size_t i = 0; i + 1 < us.size(); ++i)
			{
				builder.label(frame.at((us[i] + us[i + 1]) / 2.0, s + side * FigureHeightMm * 0.9), angle, FigureHeightMm,
					formatMeters(us[i + 1] - us[i], texts.decimalSeparator), Ink::Dimension);
			}
		}

		void includeBounds(PlanDrawing& drawing)
		{
			double minX = std::numeric_limits<double>::infinity();
			double minY = minX;
			double maxX = -minX;
			double maxY = -minX;
			auto include = [&](PlanPoint p, double pad)
			{
				minX = std::min(minX, p.x_mm - pad);
				minY = std::min(minY, p.y_mm - pad);
				maxX = std::max(maxX, p.x_mm + pad);
				maxY = std::max(maxY, p.y_mm + pad);
			};
			for (const Fill& fill : drawing.fills)
			{
				for (const PlanPoint& p : fill.outline)
				{
					include(p, 0.0);
				}
			}
			for (const Stroke& stroke : drawing.strokes)
			{
				for (const PlanPoint& p : stroke.points)
				{
					include(p, stroke.widthMm / 2.0);
				}
			}
			for (const Label& label : drawing.labels)
			{
				include(label.at, label.heightMm);
			}
			if (drawing.empty())
			{
				return;
			}
			drawing.min = PlanPoint{minX, minY};
			drawing.max = PlanPoint{maxX, maxY};
		}

		// Items as the overlay previews them, each with its ink: the selected one tinted, a valid preview with an existing
		// id in its place, new ones last.
		template <typename T, typename Id, typename Valid>
		std::vector<std::pair<T, Ink>> overlaid(const std::vector<T>& items, const Id& selected, const std::vector<T>& previews,
			Ink ink, Valid valid)
		{
			std::vector<std::pair<T, Ink>> shown;
			for (const T& item : items)
			{
				shown.emplace_back(item, !selected.isNil() && item.id == selected ? Ink::Selected : ink);
			}
			for (const T& preview : previews)
			{
				if (!valid(preview))
				{
					continue;
				}
				const auto existing = std::find_if(shown.begin(), shown.end(),
					[&preview](const auto& item) { return !preview.id.isNil() && item.first.id == preview.id; });
				if (existing != shown.end())
				{
					*existing = {preview, Ink::Preview};
				}
				else
				{
					shown.emplace_back(preview, Ink::Preview);
				}
			}
			return shown;
		}

		// A run: its path in plan with a casing under its core (`casing`: the casing's ink), and a ring where it rises or
		// drops on the spot.
		void drawServiceRun(Builder& builder, const cad::ServiceRun& run, Ink casing)
		{
			const double core = std::max(static_cast<double>(cad::serviceKindInfo(run.kind).diameterMm), MinServiceCoreMm);
			const double cased = core + 2.0 * ServiceCasingMm;
			const Ink coreInk = serviceInk(run.kind);
			std::vector<PlanPoint> path;
			std::vector<PlanPoint> risers;
			for (std::size_t index = 0; index < run.points.size(); ++index)
			{
				const cad::ServicePoint& point = run.points[index];
				const PlanPoint at{static_cast<double>(point.x_mm), static_cast<double>(point.y_mm)};
				if (!path.empty() && path.back().x_mm == at.x_mm && path.back().y_mm == at.y_mm)
				{
					if (risers.empty() || risers.back().x_mm != at.x_mm || risers.back().y_mm != at.y_mm)
					{
						risers.push_back(at);
					}
					continue;
				}
				path.push_back(at);
			}
			if (path.size() >= 2)
			{
				builder.stroke(path, casing, cased);
				builder.stroke(path, coreInk, core);
			}
			for (const PlanPoint& riser : risers)
			{
				std::vector<PlanPoint> ring;
				for (int i = 0; i < ArcSegments; ++i)
				{
					const double angle = 2.0 * Pi * static_cast<double>(i) / static_cast<double>(ArcSegments);
					ring.push_back(add(riser, PlanPoint{core * std::cos(angle), core * std::sin(angle)}));
				}
				builder.stroke(ring, casing, cased, true);
				builder.stroke(std::move(ring), coreInk, core, true);
			}
		}
	}

	Rgb inkColor(Ink ink)
	{
		switch (ink)
		{
		case Ink::Cut: return {38, 38, 38};
		case Ink::Carpentry: return {26, 26, 26};
		case Ink::Swing: return {110, 110, 110};
		case Ink::Hidden: return {176, 176, 176};
		case Ink::Dimension: return {31, 63, 102};
		case Ink::Furniture: return {96, 96, 96};
		case Ink::Selected: return {255, 107, 20};
		case Ink::Preview: return {56, 148, 255};
		case Ink::Handle: return {230, 190, 0};
		case Ink::ServiceCasing: return {64, 64, 64};
		case Ink::ServiceElectrical: return {46, 156, 72};
		case Ink::ServiceWater: return {238, 238, 234};
		case Ink::ServiceSewage: return {232, 186, 34};
		case Ink::ServiceGas: return {212, 172, 64};
		default: return {17, 17, 17}; // Label
		}
	}

	Ink serviceInk(cad::ServiceKind kind)
	{
		switch (kind)
		{
		case cad::ServiceKind::Water: return Ink::ServiceWater;
		case cad::ServiceKind::Sewage: return Ink::ServiceSewage;
		case cad::ServiceKind::Gas: return Ink::ServiceGas;
		default: return Ink::ServiceElectrical;
		}
	}

	std::string formatMeters(double millimeters, char decimalSeparator)
	{
		return formatHundredths(std::llround(millimeters / 10.0), decimalSeparator);
	}

	PlanDrawing buildPlanDrawing(const cad::Project& project, const cad::LevelId& level, const PlanOverlay& overlay, const DrawingTexts& texts)
	{
		// The project as the overlay previews it: replaced walls keep their place, new ones go last with a stand-in id
		// (never a real UUID v4) so joins and rooms can tell them apart.
		std::uint64_t added = 0;
		std::vector<DrawnWall> drawn;
		for (const cad::Wall& wall : project.walls)
		{
			drawn.push_back(DrawnWall{wall, !overlay.selectedWall.isNil() && wall.id == overlay.selectedWall ? Ink::Selected : Ink::Cut});
		}
		for (const cad::Wall& preview : overlay.previewWalls)
		{
			if (!cad::validateWallGeometry(preview).ok())
			{
				continue;
			}
			const auto existing = std::find_if(drawn.begin(), drawn.end(),
				[&preview](const DrawnWall& d) { return !preview.id.isNil() && d.wall.id == preview.id; });
			if (existing != drawn.end())
			{
				*existing = DrawnWall{preview, Ink::Preview};
			}
			else
			{
				cad::Wall wall = preview;
				wall.id = cad::WallId{cad::Uuid{~0ull, ++added}};
				drawn.push_back(DrawnWall{wall, Ink::Preview});
			}
		}
		// The level's walls, in that order.
		std::vector<DrawnWall> onLevel;
		cad::Project levelWalls;
		for (const DrawnWall& d : drawn)
		{
			if (project.levels.empty() || project.levelOf(d.wall) == level)
			{
				onLevel.push_back(d);
				levelWalls.walls.push_back(d.wall);
			}
		}

		PlanDrawing drawing;
		Builder builder(drawing);
		const geometry::WallJoins joins = geometry::solveWallJoins(levelWalls);
		for (std::size_t index = 0; index < onLevel.size(); ++index)
		{
			drawWall(builder, onLevel[index], joins.footprints[index]);
		}
		for (const geometry::JointFill& joint : joins.fills)
		{
			builder.fill(joint.outline, Ink::Cut);
		}

		// Dimensions on the side of each wall without a room (walls between rooms: their length only).
		const std::vector<geometry::Room> rooms = geometry::detectRooms(levelWalls);
		auto hasRoom = [&rooms](PlanPoint point)
		{
			return std::any_of(rooms.begin(), rooms.end(), [point](const geometry::Room& room) { return geometry::containsPoint(room.outline, point); });
		};
		for (std::size_t index = 0; index < onLevel.size(); ++index)
		{
			const cad::Wall& wall = onLevel[index].wall;
			const Frame frame = frameOf(wall);
			const geometry::WallFootprint& footprint = joins.footprints[index];
			const double leftS = frame.across(footprint.startLeft);
			const double rightS = frame.across(footprint.startRight);
			const bool roomLeft = hasRoom(frame.at(frame.length / 2.0, leftS + ExtensionGapMm));
			const bool roomRight = hasRoom(frame.at(frame.length / 2.0, rightS - ExtensionGapMm));
			const double side = roomLeft && !roomRight ? -1.0 : 1.0;
			const double faceS = side > 0.0 ? leftS : rightS;
			if (roomLeft && roomRight)
			{
				drawDimension(builder, frame, {0.0, frame.length}, faceS, faceS + side * PartitionOffsetMm, side, texts);
				continue;
			}
			std::vector<double> chain{0.0, frame.length};
			for (const cad::Opening& opening : wall.openings)
			{
				chain.push_back(static_cast<double>(opening.offsetMm));
				chain.push_back(static_cast<double>(opening.offsetMm + opening.widthMm));
			}
			const bool withOpenings = !wall.openings.empty();
			drawDimension(builder, frame, chain, faceS, faceS + side * ChainOffsetMm, side, texts);
			if (withOpenings)
			{
				drawDimension(builder, frame, {0.0, frame.length}, faceS, faceS + side * OverallOffsetMm, side, texts);
			}
		}

		// Room names and areas.
		for (std::size_t index = 0; index < rooms.size(); ++index)
		{
			const geometry::Room& room = rooms[index];
			std::string name = texts.roomName;
			if (const std::size_t token = name.find("{n}"); token != std::string::npos)
			{
				name.replace(token, 3, std::to_string(index + 1));
			}
			builder.label(add(room.labelPoint, {0.0, RoomNameHeightMm * 0.8}), 0.0, RoomNameHeightMm, std::move(name), Ink::Label);
			builder.label(add(room.labelPoint, {0.0, -RoomAreaHeightMm * 0.9}), 0.0, RoomAreaHeightMm,
				formatHundredths(std::llround(room.areaMm2 / 1.0e4), texts.decimalSeparator) + texts.areaUnit, Ink::Label);
		}

		// Furniture, as the overlay previews it: its outline, then the details of its type.
		const auto pieces = overlaid(project.furniture, overlay.selectedFurniture, overlay.previewFurniture, Ink::Furniture,
			[](const cad::Furniture& piece) { return cad::validateFurniture(piece).ok(); });
		for (const auto& [piece, ink] : pieces)
		{
			if (!project.levels.empty() && piece.level != level)
			{
				continue;
			}
			const std::array<PlanPoint, 4> corners = cad::furnitureFootprint(piece);
			builder.stroke(std::vector<PlanPoint>(corners.begin(), corners.end()), ink, FurnitureLineMm, true);
			for (const geometry::FurnitureOutline& detail : geometry::furnitureShape(piece).planDetails)
			{
				std::vector<PlanPoint> points;
				for (const PlanPoint& local : detail)
				{
					points.push_back(cad::furnitureToPlan(piece, local.x_mm, local.y_mm));
				}
				builder.stroke(std::move(points), ink, FurnitureDetailMm, true);
			}
		}

		// Columns, cut like the walls.
		const auto columns = overlaid(project.columns, overlay.selectedColumn, overlay.previewColumns, Ink::Cut,
			[](const cad::Column& column) { return cad::validateColumn(column).ok(); });
		for (const auto& [column, ink] : columns)
		{
			if (project.levels.empty() || column.level == level)
			{
				builder.fill(geometry::columnOutline(column), ink);
			}
		}

		// Runs of the building services: those of the networks shown with the layer on, and any a gesture previews.
		std::vector<cad::ServiceRun> shown;
		if (overlay.services)
		{
			std::copy_if(project.services.begin(), project.services.end(), std::back_inserter(shown),
				[&overlay](const cad::ServiceRun& run) { return overlay.serviceKinds.contains(run.kind); });
		}
		const auto runs = overlaid(shown, overlay.selectedServiceRun, overlay.previewServiceRuns,
			Ink::ServiceCasing, [](const cad::ServiceRun& run) { return cad::validateServiceRun(run).ok(); });
		for (const auto& [run, ink] : runs)
		{
			if (project.levels.empty() || run.level == level)
			{
				drawServiceRun(builder, run, ink);
			}
		}

		for (const cad::Point2D& handle : overlay.handles)
		{
			builder.square(PlanPoint{static_cast<double>(handle.x_mm), static_cast<double>(handle.y_mm)}, overlay.handleSizeMm, Ink::Handle);
		}
		includeBounds(drawing);
		return drawing;
	}
}
