#include "F101/Drawing/PlanDrawing.h"
#include "F101/Cad/Units.h"

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
#include <string>
#include <tuple>
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
		constexpr double FigureClearanceMm = 75.0; // room names keep this far from dimension figures...
		constexpr double LabelStepMm = 25.0;       // ...moving in steps of this when they have to...
		constexpr double FloorMarginMm = 50.0;     // ...and this far inside the floor
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

		void drawAnnotation(Builder& builder, const cad::Dimension& d, const DrawingTexts& texts, Ink ink,
			double faceS = 0.0, bool line = true)
		{
			cad::Wall axisWall;
			axisWall.start = d.start;
			axisWall.end = d.end;
			const Frame frame = frameOf(axisWall);
			const double side = d.offsetMm < 0 ? -1.0 : 1.0;
			const double s = static_cast<double>(d.offsetMm);
			if (line) builder.stroke({frame.at(0.0, s), frame.at(frame.length, s)}, ink, ThinMm);
			const double tickLength = std::min(TickLengthMm, frame.length * 0.4);
			const PlanPoint tick = scale(add(frame.axis, scale(frame.left, side)), tickLength / 2.0 / std::sqrt(2.0));
			if (cad::dimensionLabelOutside(d))
			{
				// A short value belongs beyond its end, over a leader, rather than over the two crowded ticks.
				const double labelS = frame.across(cad::dimensionLabelPoint(d)) - side * cad::DimensionFigureGapMm;
				const double run = frame.length + cad::DimensionLeaderRunMm;
				builder.stroke({frame.at(frame.length, s), frame.at(run, s), frame.at(run, labelS),
					frame.at(frame.length + cad::dimensionLabelWidthMm(d) + cad::DimensionLeaderRunMm, labelS)}, ink, ThinMm);
			}
			for (const double u : {0.0, frame.length})
			{
				builder.stroke({frame.at(u, faceS + side * ExtensionGapMm), frame.at(u, s + side * ExtensionGapMm)}, ink, ThinMm);
				const PlanPoint centre = frame.at(u, s);
				builder.stroke({sub(centre, tick), add(centre, tick)}, ink, TickMm);
			}
			double angle = std::atan2(frame.axis.y_mm, frame.axis.x_mm) * 180.0 / Pi;
			if (angle > 90.0) angle -= 180.0;
			else if (angle <= -90.0) angle += 180.0;
			builder.label(cad::dimensionLabelPoint(d), angle, FigureHeightMm, cad::dimensionLabel(d, texts.decimalSeparator), ink);
		}

		// An automatic dimension as the project and the current gesture leave it: its stored offset, text and visibility,
		// or the drag's preview. True when it stands apart from a chain (customized, previewed or selected).
		bool resolveAutomatic(cad::Dimension& d, const cad::Project& project, const PlanOverlay& overlay)
		{
			bool apart = d.id == overlay.selectedDimension;
			if (const cad::Dimension* custom = project.findDimension(d.id))
			{
				d.offsetMm = custom->offsetMm;
				d.text = custom->text;
				d.hidden = custom->hidden;
				apart = true;
			}
			if (overlay.previewDimension && overlay.previewDimension->id == d.id)
			{
				d = *overlay.previewDimension;
				apart = true;
			}
			return apart;
		}

		// Every segment has a stable key made of its wall and opening-edge identities, never its measured value.
		void drawDimension(Builder& builder, const Frame& frame, std::vector<double> us, double faceS, double s,
			const DrawingTexts& texts, const cad::Wall& wall, const cad::Project& project, const PlanOverlay& overlay,
			std::string_view role, PlanDrawing& drawing)
		{
			std::sort(us.begin(), us.end());
			us.erase(std::unique(us.begin(), us.end(), [](double a, double b) { return std::fabs(a - b) < 1.0; }), us.end());
			if (us.size() < 2) return;
			auto boundary = [&wall, &frame](double u) -> std::string
			{
				if (std::fabs(u) < 0.5) return "start";
				if (std::fabs(u - frame.length) < 0.5) return "end";
				for (const cad::Opening& opening : wall.openings)
				{
					if (std::fabs(u - static_cast<double>(opening.offsetMm)) < 0.5) return cad::toString(opening.id) + "/start";
					if (std::fabs(u - static_cast<double>(opening.offsetMm + opening.widthMm)) < 0.5) return cad::toString(opening.id) + "/end";
				}
				return "edge";
			};
			std::vector<cad::Dimension> entries;
			bool continuous = true;
			for (std::size_t i = 0; i + 1 < us.size(); ++i)
			{
				const PlanPoint a = frame.at(us[i], 0.0), b = frame.at(us[i + 1], 0.0);
				cad::Dimension d;
				d.id = "auto/" + cad::toString(wall.id) + "/" + std::string(role);
				if (role == "chain") d.id += "/" + boundary(us[i]) + "/" + boundary(us[i + 1]);
				d.level = project.levelOf(wall);
				d.start = {std::llround(a.x_mm), std::llround(a.y_mm)};
				d.end = {std::llround(b.x_mm), std::llround(b.y_mm)};
				d.offsetMm = std::llround(s);
				d.automatic = true;
				if (d.start == d.end) continue;
				if (resolveAutomatic(d, project, overlay)) continuous = false;
				entries.push_back(d);
			}
			if (continuous) builder.stroke({frame.at(us.front(), s), frame.at(us.back(), s)}, Ink::Dimension, ThinMm);
			for (const cad::Dimension& d : entries)
			{
				if (d.hidden) continue;
				drawing.dimensions.push_back(d);
				drawAnnotation(builder, d, texts, d.id == overlay.selectedDimension ? Ink::DimensionSelected : Ink::Dimension, faceS, !continuous);
			}
		}

		// A straight stretch of a room's floor edge: its consecutive sides on one line, named after the lowest of them
		// (wall, left face first, segment), like room keys.
		struct Face
		{
			PlanPoint start;
			PlanPoint end;
			std::tuple<cad::WallId, bool, std::size_t> name; // (wall, right face, segment)
			bool partition = false; // runs along a face that already carries a partition's dimension
		};

		std::tuple<cad::WallId, bool, std::size_t> nameOf(const geometry::RoomSide& side)
		{
			return {side.wall, !side.left, side.segment};
		}

		// The stretch from `start` to `end` goes straight on from where `face` ends, without a step.
		bool continues(const Face& face, PlanPoint start, PlanPoint end)
		{
			const PlanPoint u = sub(face.end, face.start);
			const PlanPoint v = sub(end, start);
			const PlanPoint gap = sub(start, face.end);
			return std::fabs(u.x_mm * v.y_mm - u.y_mm * v.x_mm) <= 1e-9 * std::hypot(u.x_mm, u.y_mm) * std::hypot(v.x_mm, v.y_mm) &&
				dot(u, v) > 0.0 && std::hypot(gap.x_mm, gap.y_mm) < 0.01;
		}

		std::vector<Face> roomFaces(const geometry::Room& room, const std::vector<cad::WallId>& partitions)
		{
			std::vector<Face> faces;
			for (const geometry::RoomSide& side : room.sides)
			{
				const bool partition = side.left && std::find(partitions.begin(), partitions.end(), side.wall) != partitions.end();
				if (!faces.empty() && continues(faces.back(), side.start, side.end))
				{
					faces.back().end = side.end;
					faces.back().name = std::min(faces.back().name, nameOf(side));
					faces.back().partition = faces.back().partition || partition;
				}
				else
				{
					faces.push_back(Face{side.start, side.end, nameOf(side), partition});
				}
			}
			// The outline starts anywhere: its last face may go on into the first.
			if (faces.size() > 2 && continues(faces.back(), faces.front().start, faces.front().end))
			{
				faces.front().start = faces.back().start;
				faces.front().name = std::min(faces.front().name, faces.back().name);
				faces.front().partition = faces.front().partition || faces.back().partition;
				faces.pop_back();
			}
			return faces;
		}

		// `other` runs back along the same stretch as `face`: the opposite side of a rectangle, whose figure would repeat.
		bool mirrors(const Face& face, const Face& other)
		{
			const PlanPoint u = sub(face.end, face.start);
			const double length = std::hypot(u.x_mm, u.y_mm);
			const PlanPoint axis = scale(u, 1.0 / length);
			const PlanPoint v = sub(other.end, other.start);
			return dot(axis, v) < 0.0 && std::fabs(axis.x_mm * v.y_mm - axis.y_mm * v.x_mm) <= 1e-9 * std::hypot(v.x_mm, v.y_mm) &&
				std::fabs(dot(sub(other.end, face.start), axis)) <= 1.0 && std::fabs(dot(sub(other.start, face.start), axis) - length) <= 1.0;
		}

		// Interior dimensions (ADR-0034): each straight stretch of the floor edge, face to face, InteriorOffsetMm into the
		// room. Not along a face that carries a partition's dimension, and once per pair of opposite faces of equal span.
		void drawInteriorDimensions(Builder& builder, const geometry::Room& room, const std::vector<cad::WallId>& partitions,
			const cad::LevelId& level, const DrawingTexts& texts, const cad::Project& project, const PlanOverlay& overlay,
			PlanDrawing& drawing)
		{
			std::vector<Face> faces = roomFaces(room, partitions);
			std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.name < b.name; });
			std::vector<Face> kept;
			for (const Face& face : faces)
			{
				const PlanPoint span = sub(face.end, face.start);
				if (face.partition || std::hypot(span.x_mm, span.y_mm) < 1.0 ||
					std::any_of(kept.begin(), kept.end(), [&face](const Face& other) { return mirrors(face, other); }))
				{
					continue;
				}
				kept.push_back(face);
				const auto& [wall, right, segment] = face.name;
				cad::Dimension d;
				d.id = "auto/" + cad::toString(wall) + (right ? "/interior-right-" : "/interior-left-") + std::to_string(segment);
				d.level = level;
				d.start = {std::llround(face.start.x_mm), std::llround(face.start.y_mm)};
				d.end = {std::llround(face.end.x_mm), std::llround(face.end.y_mm)};
				d.offsetMm = std::llround(InteriorOffsetMm); // the floor is on the left of its edge
				d.automatic = true;
				if (d.start == d.end) continue;
				resolveAutomatic(d, project, overlay);
				if (d.hidden) continue;
				drawing.dimensions.push_back(d);
				drawAnnotation(builder, d, texts, d.id == overlay.selectedDimension ? Ink::DimensionSelected : Ink::Dimension);
			}
		}

		// What a text covers on the plan, as an upright box: centred on `at`, turned `angleDegrees`, `marginMm` around it.
		struct Box
		{
			PlanPoint min;
			PlanPoint max;
		};

		Box textBox(PlanPoint at, double angleDegrees, double widthMm, double heightMm, double marginMm)
		{
			const double halfWidth = widthMm / 2.0 + marginMm;
			const double halfHeight = heightMm / 2.0 + marginMm;
			const double angle = angleDegrees * Pi / 180.0;
			const double padX = std::fabs(std::cos(angle)) * halfWidth + std::fabs(std::sin(angle)) * halfHeight;
			const double padY = std::fabs(std::sin(angle)) * halfWidth + std::fabs(std::cos(angle)) * halfHeight;
			return {{at.x_mm - padX, at.y_mm - padY}, {at.x_mm + padX, at.y_mm + padY}};
		}

		bool overlaps(const Box& a, const Box& b)
		{
			return a.min.x_mm < b.max.x_mm && b.min.x_mm < a.max.x_mm && a.min.y_mm < b.max.y_mm && b.min.y_mm < a.max.y_mm;
		}

		Box figureBox(const cad::Dimension& d)
		{
			const cad::Vector2D axis = d.end - d.start;
			const double angle = std::atan2(static_cast<double>(axis.dy_mm), static_cast<double>(axis.dx_mm)) * 180.0 / Pi;
			return textBox(cad::dimensionLabelPoint(d), angle, cad::dimensionLabelWidthMm(d), FigureHeightMm, FigureClearanceMm);
		}

		// A room text placed automatically: its centre relative to the spot of the room's texts, and its size.
		struct SpotText
		{
			PlanPoint offset;
			double widthMm = 0.0;
			double heightMm = 0.0;
		};

		// Where a room's automatic texts stand: at its label point, or else the nearest point sideways from it, then up or
		// down, where they cover no automatic dimension figure and stay on the floor (ADR-0034); the label point if there
		// is none. Sideways first: right over or under a figure, they would read as one block with it.
		PlanPoint roomLabelSpot(const geometry::Room& room, const std::vector<SpotText>& texts, const std::vector<Box>& figures)
		{
			Box bounds{room.outline.front(), room.outline.front()};
			for (const PlanPoint& p : room.outline)
			{
				bounds.min = {std::min(bounds.min.x_mm, p.x_mm), std::min(bounds.min.y_mm, p.y_mm)};
				bounds.max = {std::max(bounds.max.x_mm, p.x_mm), std::max(bounds.max.y_mm, p.y_mm)};
			}
			std::vector<Box> nearby; // the figures over this floor
			std::copy_if(figures.begin(), figures.end(), std::back_inserter(nearby), [&bounds](const Box& figure) { return overlaps(figure, bounds); });
			const auto boxes = [&](PlanPoint at)
			{
				std::vector<Box> result;
				for (const SpotText& text : texts) result.push_back(textBox(add(at, text.offset), 0.0, text.widthMm, text.heightMm, 0.0));
				return result;
			};
			const auto clear = [&](PlanPoint at)
			{
				for (const Box& text : boxes(at))
				{
					if (std::any_of(nearby.begin(), nearby.end(), [&text](const Box& figure) { return overlaps(text, figure); }))
					{
						return false;
					}
				}
				return true;
			};
			const auto onFloor = [&](PlanPoint at)
			{
				for (const Box& text : boxes(at))
				{
					const PlanPoint low = sub(text.min, {FloorMarginMm, FloorMarginMm});
					const PlanPoint high = add(text.max, {FloorMarginMm, FloorMarginMm});
					for (const PlanPoint corner : {low, high, PlanPoint{low.x_mm, high.y_mm}, PlanPoint{high.x_mm, low.y_mm}})
					{
						if (!geometry::containsPoint(room.outline, corner))
						{
							return false;
						}
					}
				}
				return true;
			};
			if (clear(room.labelPoint))
			{
				return room.labelPoint;
			}
			const double reach = std::max({room.labelPoint.x_mm - bounds.min.x_mm, bounds.max.x_mm - room.labelPoint.x_mm,
				room.labelPoint.y_mm - bounds.min.y_mm, bounds.max.y_mm - room.labelPoint.y_mm});
			for (const PlanPoint axis : {PlanPoint{1.0, 0.0}, PlanPoint{0.0, 1.0}})
			{
				for (double step = LabelStepMm; step <= reach; step += LabelStepMm)
				{
					for (const double sign : {-1.0, 1.0})
					{
						const PlanPoint at = add(room.labelPoint, scale(axis, sign * step));
						if (clear(at) && onFloor(at))
						{
							return at;
						}
					}
				}
			}
			return room.labelPoint;
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
				const Box box = textBox(label.at, label.angleDegrees, cad::planTextWidthMm(cad::textCharacters(label.text), label.heightMm),
					label.heightMm, 10.0);
				include(box.min, 0.0);
				include(box.max, 0.0);
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
		case Ink::DimensionSelected: return {151, 78, 12};
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
		return cad::formatHundredths(std::llround(millimeters / 10.0), decimalSeparator);
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
		std::vector<cad::WallId> partitions; // their dimension stands along their left face
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
				partitions.push_back(wall.id);
				drawDimension(builder, frame, {0.0, frame.length}, faceS, faceS + side * PartitionOffsetMm, texts, wall, project, overlay, "partition", drawing);
				continue;
			}
			std::vector<double> chain{0.0, frame.length};
			for (const cad::Opening& opening : wall.openings)
			{
				chain.push_back(static_cast<double>(opening.offsetMm));
				chain.push_back(static_cast<double>(opening.offsetMm + opening.widthMm));
			}
			const bool withOpenings = !wall.openings.empty();
			drawDimension(builder, frame, chain, faceS, faceS + side * ChainOffsetMm, texts, wall, project, overlay, withOpenings ? "chain" : "overall", drawing);
			if (withOpenings)
			{
				drawDimension(builder, frame, {0.0, frame.length}, faceS, faceS + side * OverallOffsetMm, texts, wall, project, overlay, "overall", drawing);
			}
		}
		// Inside each room, face to face, unless the view hides them.
		if (overlay.interiorDimensions)
		{
			for (const geometry::Room& room : rooms)
			{
				drawInteriorDimensions(builder, room, partitions, project.levels.empty() ? cad::LevelId() : level, texts, project, overlay, drawing);
			}
		}

		for (const cad::Dimension& d : project.dimensions)
		{
			if (d.automatic || d.hidden || d.level != level) continue;
			drawing.dimensions.push_back(d);
			if (!overlay.previewDimension || overlay.previewDimension->id != d.id)
				drawAnnotation(builder, d, texts, d.id == overlay.selectedDimension ? Ink::DimensionSelected : Ink::Dimension);
		}
		if (overlay.previewDimension && !overlay.previewDimension->automatic && overlay.previewDimension->start != overlay.previewDimension->end)
			drawAnnotation(builder, *overlay.previewDimension, texts, Ink::Preview);

		// Only the editing overlay adds grips; exports contain no handles.
		if (!overlay.selectedDimension.empty())
		{
			const auto selected = std::find_if(drawing.dimensions.begin(), drawing.dimensions.end(), [&](const cad::Dimension& d) { return d.id == overlay.selectedDimension; });
			if (selected != drawing.dimensions.end())
			{
				const cad::Dimension& shown = overlay.previewDimension && overlay.previewDimension->id == selected->id ? *overlay.previewDimension : *selected;
				std::array<PlanPoint, 4> handles;
				cad::dimensionHandles(shown, handles);
				for (const PlanPoint grip : handles) builder.square(grip, overlay.dimensionHandleSizeMm, Ink::DimensionSelected);
			}
		}

		// Room names and areas, clear of the automatic dimension figures.
		std::vector<Box> figures;
		for (const cad::Dimension& d : drawing.dimensions)
		{
			if (d.automatic)
			{
				figures.push_back(figureBox(d));
			}
		}
		for (std::size_t index = 0; index < rooms.size(); ++index)
		{
			const geometry::Room& room = rooms[index];
			const cad::RoomName* custom = project.findRoomName(room.key);
			std::string name = custom ? custom->name : texts.roomName;
			if (const std::size_t token = name.find("{n}"); !custom && token != std::string::npos)
			{
				name.replace(token, 3, std::to_string(index + 1));
			}
			std::string area = cad::formatHundredths(std::llround(room.areaMm2 / 1.0e4), texts.decimalSeparator) + texts.areaUnit;
			// Its sizes and moved texts (ADR-0035), as a drag would leave them.
			const cad::RoomLabel* stored = project.findRoomLabel(room.key);
			const bool previewed = overlay.previewRoomLabel && overlay.previewRoomLabel->room == room.key;
			const cad::RoomLabel layout = previewed ? *overlay.previewRoomLabel : stored ? *stored : cad::RoomLabel{room.key};
			const double nameHeight = static_cast<double>(layout.nameHeightMm);
			const double areaHeight = static_cast<double>(layout.areaHeightMm);
			const SpotText nameText{{0.0, nameHeight * 0.8}, cad::planTextWidthMm(cad::textCharacters(name), nameHeight), nameHeight};
			const SpotText areaText{{0.0, -areaHeight * 0.9}, cad::planTextWidthMm(cad::textCharacters(area), areaHeight), areaHeight};
			std::vector<SpotText> automatic;
			if (!layout.nameOffset) automatic.push_back(nameText);
			if (!layout.areaOffset) automatic.push_back(areaText);
			const PlanPoint spot = automatic.empty() ? room.labelPoint : roomLabelSpot(room, automatic, figures);
			const Ink ink = !overlay.selectedRoom.empty() && overlay.selectedRoom == room.key ? Ink::DimensionSelected : Ink::Label;
			const auto place = [&](const std::optional<cad::Vector2D>& moved, const SpotText& text, bool isArea, std::string written)
			{
				const PlanPoint centre = moved ? add(room.labelPoint, {static_cast<double>(moved->dx_mm), static_cast<double>(moved->dy_mm)})
					: add(spot, text.offset);
				builder.label(centre, 0.0, text.heightMm, std::move(written), ink);
				drawing.roomTexts.push_back(cad::RoomText{room.key, isArea, centre, text.widthMm, text.heightMm, room.labelPoint});
			};
			place(layout.nameOffset, nameText, false, std::move(name));
			place(layout.areaOffset, areaText, true, std::move(area));
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
