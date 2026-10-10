#include "F101/Drawing/SvgWriter.h"

#include <charconv>
#include <cmath>

namespace f101::drawing
{
	namespace
	{
		constexpr double CapHeightPerFontSize = 0.72; // Arial-like sans serif
		constexpr const char* DashPattern = "120 80";

		// At most one decimal, always ".", no trailing ".0": "12.5", "-3", "0".
		std::string number(double value)
		{
			double rounded = std::round(value * 10.0) / 10.0;
			if (rounded == 0.0)
			{
				rounded = 0.0; // no "-0"
			}
			char buffer[64];
			const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), rounded, std::chars_format::fixed, 1);
			std::string text(buffer, result.ptr);
			if (text.size() > 2 && text.compare(text.size() - 2, 2, ".0") == 0)
			{
				text.resize(text.size() - 2);
			}
			return text;
		}

		std::string colour(Ink ink)
		{
			static constexpr char Hex[] = "0123456789abcdef";
			const Rgb c = inkColor(ink);
			std::string text = "#";
			for (const std::uint8_t channel : {c.r, c.g, c.b})
			{
				text += Hex[channel >> 4];
				text += Hex[channel & 15];
			}
			return text;
		}

		std::string escape(const std::string& text)
		{
			std::string escaped;
			for (const char c : text)
			{
				switch (c)
				{
				case '&': escaped += "&amp;"; break;
				case '<': escaped += "&lt;"; break;
				case '>': escaped += "&gt;"; break;
				case '"': escaped += "&quot;"; break;
				default: escaped += c; break;
				}
			}
			return escaped;
		}
	}

	std::string writeSvg(const PlanDrawing& drawing, const SvgOptions& options)
	{
		const bool empty = drawing.empty();
		const double minX = (empty ? 0.0 : drawing.min.x_mm) - options.marginMm;
		const double minY = (empty ? 0.0 : drawing.min.y_mm) - options.marginMm;
		const double maxX = (empty ? 0.0 : drawing.max.x_mm) + options.marginMm;
		const double maxY = (empty ? 0.0 : drawing.max.y_mm) + options.marginMm;
		const double width = maxX - minX;
		const double height = maxY - minY;
		// SVG runs y down: north goes up by measuring y from the top edge.
		auto point = [&](const cad::PlanPoint& p) { return number(p.x_mm - minX) + "," + number(maxY - p.y_mm); };
		auto points = [&](const std::vector<cad::PlanPoint>& all)
		{
			std::string text;
			for (const cad::PlanPoint& p : all)
			{
				text += (text.empty() ? "" : " ") + point(p);
			}
			return text;
		};

		std::string svg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
		svg += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + number(width / options.scale) + "mm\" height=\"" +
			number(height / options.scale) + "mm\" viewBox=\"0 0 " + number(width) + " " + number(height) + "\">\n";
		svg += "  <rect width=\"" + number(width) + "\" height=\"" + number(height) + "\" fill=\"#ffffff\"/>\n";
		for (const Fill& fill : drawing.fills)
		{
			svg += "  <polygon points=\"" + points(fill.outline) + "\" fill=\"" + colour(fill.ink) + "\"/>\n";
		}
		for (const Stroke& stroke : drawing.strokes)
		{
			svg += std::string("  <") + (stroke.closed ? "polygon" : "polyline") + " points=\"" + points(stroke.points) +
				"\" fill=\"none\" stroke=\"" + colour(stroke.ink) + "\" stroke-width=\"" + number(stroke.widthMm) +
				"\" stroke-linecap=\"round\" stroke-linejoin=\"round\"" + (stroke.dashed ? std::string(" stroke-dasharray=\"") + DashPattern + "\"" : "") +
				"/>\n";
		}
		for (const Label& label : drawing.labels)
		{
			const std::string x = number(label.at.x_mm - minX);
			const std::string y = number(maxY - label.at.y_mm);
			if (label.ink == Ink::Dimension || label.ink == Ink::DimensionSelected)
			{
				const double paperWidth = cad::planTextWidthMm(cad::textCharacters(label.text), label.heightMm) + 20.0;
				svg += "  <rect x=\"" + number(label.at.x_mm - minX - paperWidth / 2.0) + "\" y=\"" +
					number(maxY - label.at.y_mm - label.heightMm / 2.0 - 10.0) + "\" width=\"" + number(paperWidth) +
					"\" height=\"" + number(label.heightMm + 20.0) + "\" fill=\"#ffffff\"" +
					(label.angleDegrees != 0.0 ? " transform=\"rotate(" + number(-label.angleDegrees) + " " + x + " " + y + ")\"" : "") + "/>\n";
			}
		}
		for (const Label& label : drawing.labels)
		{
			const std::string x = number(label.at.x_mm - minX);
			const std::string y = number(maxY - label.at.y_mm);
			svg += "  <text x=\"" + x + "\" y=\"" + y + "\" font-size=\"" + number(label.heightMm / CapHeightPerFontSize) + "\" fill=\"" +
				colour(label.ink) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" font-family=\"Arial, Helvetica, sans-serif\"" +
				(label.angleDegrees != 0.0 ? " transform=\"rotate(" + number(-label.angleDegrees) + " " + x + " " + y + ")\"" : "") + ">" +
				escape(label.text) + "</text>\n";
		}
		svg += "</svg>\n";
		return svg;
	}
}
