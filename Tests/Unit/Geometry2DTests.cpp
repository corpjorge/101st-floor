#include "doctest.h"

#include "F101/Cad/Geometry2D.h"

using namespace f101::cad;

TEST_CASE("points and vectors are integer millimetres")
{
	const Point2D origin{0, 0};
	const Point2D end{3550, 0};
	CHECK((end - origin) == Vector2D{3550, 0});
	CHECK((origin + Vector2D{10, -5}) == Point2D{10, -5});
	CHECK(Vector2D{3000, 4000}.lengthMm() == 5000.0);
}

TEST_CASE("segment length")
{
	CHECK(Segment2D{{0, 0}, {3550, 0}}.lengthMm() == 3550.0);
	CHECK(Segment2D{{0, 0}, {0, -2400}}.lengthMm() == 2400.0);
	CHECK(Segment2D{{-1500, 200}, {1500, 4200}}.lengthMm() == 5000.0);
	CHECK(Segment2D{{7, 7}, {7, 7}}.lengthMm() == 0.0);
}
