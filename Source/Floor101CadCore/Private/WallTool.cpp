#include "F101/Cad/WallTool.h"

#include <utility>

namespace f101::cad
{
	namespace
	{
		bool isLengthCharacter(char c)
		{
			return (c >= '0' && c <= '9') || c == '.' || c == ',' || c == ' ' || c == 'm' || c == 'M' || c == 'c' || c == 'C';
		}

		bool isBlank(const std::string& text)
		{
			return text.find_first_not_of(' ') == std::string::npos;
		}
	}

	WallTool::WallTool(Document& targetDocument, WallToolSettings settings)
		: document(targetDocument)
		, toolSettings(std::move(settings))
	{
	}

	void WallTool::setLevel(const LevelId& newLevel)
	{
		if (newLevel != level)
		{
			level = newLevel;
			cancel();
		}
	}

	LevelId WallTool::targetLevel() const
	{
		const Project& project = document.project();
		if (project.findLevel(level))
		{
			return level;
		}
		return project.levels.empty() ? LevelId() : project.levels.front().id;
	}

	std::int64_t WallTool::wallHeightMm() const
	{
		if (toolSettings.heightMm > 0)
		{
			return toolSettings.heightMm;
		}
		const Level* target = document.project().findLevel(targetLevel());
		return target ? target->wallHeightMm() : DefaultLevelHeightMm;
	}

	void WallTool::pointerMoved(PlanPoint newCursor)
	{
		cursor = newCursor;
		update();
	}

	void WallTool::update()
	{
		const Project project = snapReference(document.project(), targetLevel());
		textError.clear();
		if (currentState == State::Idle)
		{
			const SnapResult snapped = snapPoint(project, cursor, toolSettings.snap);
			hover = snapped.point;
			kind = snapped.kind;
			return;
		}

		const SnapResult snapped = snapFromAnchor(project, start, cursor, toolSettings.snap);
		hover = snapped.point;
		kind = snapped.kind;
		if (isBlank(text))
		{
			return;
		}
		const Result<std::int64_t> length = parseLength(text, toolSettings.inputUnit);
		if (!length)
		{
			textError = length.error().message;
			return;
		}
		if (length.value() <= 0)
		{
			textError = "The length must be positive";
			return;
		}
		// The typed length follows the snapped direction exactly (angle snapping still applies, before any rounding).
		const Result<Point2D> end = pointAtDistance(start, snapped.constrained, length.value());
		if (!end)
		{
			textError = end.error().message;
			return;
		}
		hover = end.value();
	}

	std::optional<Wall> WallTool::preview() const
	{
		if (currentState != State::Drafting || hover == start)
		{
			return std::nullopt;
		}
		Wall wall{WallId{}, start, hover, toolSettings.thicknessMm, wallHeightMm()};
		wall.level = targetLevel();
		return wall;
	}

	Result<WallId> WallTool::commit()
	{
		if (currentState != State::Drafting)
		{
			return Error{ErrorCode::InvalidArgument, "Click to start a wall first"};
		}
		if (!textError.empty())
		{
			return Error{ErrorCode::ParseError, textError};
		}
		const std::optional<Wall> draft = preview();
		if (!draft)
		{
			return Error{ErrorCode::InvalidArgument, "Move the cursor or type a length"};
		}
		const Result<Transaction> applied = document.execute(
			CreateWallCommand{draft->start, draft->end, draft->thicknessMm, draft->heightMm, draft->level});
		if (!applied)
		{
			return applied.error();
		}
		const WallId created = applied.value().walls.front().id;
		start = draft->end;
		text.clear();
		update();
		return created;
	}

	Result<std::optional<WallId>> WallTool::click(PlanPoint newCursor)
	{
		pointerMoved(newCursor);
		if (currentState == State::Idle)
		{
			start = hover;
			currentState = State::Drafting;
			text.clear();
			update();
			return std::optional<WallId>();
		}
		const Result<WallId> created = commit();
		if (!created)
		{
			return created.error();
		}
		return std::optional<WallId>(created.value());
	}

	Result<WallId> WallTool::confirm()
	{
		return commit();
	}

	void WallTool::cancel()
	{
		currentState = State::Idle;
		text.clear();
		update();
	}

	void WallTool::appendText(std::string_view typed)
	{
		if (currentState != State::Drafting)
		{
			return;
		}
		for (const char c : typed)
		{
			if (isLengthCharacter(c))
			{
				text.push_back(c);
			}
		}
		update();
	}

	void WallTool::backspace()
	{
		if (!text.empty())
		{
			text.pop_back();
			update();
		}
	}
}
