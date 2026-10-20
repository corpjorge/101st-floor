#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

struct FSlateBrush;

// Cota Cero design system (docs/ui-cota-cero.md): the colours, type, sizes and widget styles every part of the interface
// takes its values from. Presentation only: nothing here reads or changes the document.
namespace Floor101::Studio
{
	FLinearColor Color(const TCHAR* Hex);

	struct FPalette
	{
		FLinearColor Canvas, Bar, Island, Raised, Hover, Pressed, Inset, Line, EdgeSoft, Edge, Tip;
		FLinearColor Text1, Text2, Text3, Text4;
		FLinearColor Brass, BrassHi, BrassLo, BrassSel, BrassLine, OnBrass;
		FLinearColor Danger, DangerFill, DangerPress, Ok, Warn, Info, Paper, Scrim, Glass;
		FLinearColor Shadow, Sheen, GlassEdge, BarEdge, SwatchEdge, OnPaper, Missing, Invalid;
	};
	const FPalette& Palette();

	enum class EText : uint8
	{
		Display, Title, Heading, Body, BodyStrong, Button, Label, Help, Overline, Keycap, Numeric, NumericSmall, NumericStrong, NumericLarge,
		Wordmark, WordmarkSuffix
	};
	FSlateFontInfo Text(EText Style);
	// Sizes written before the type scale, mapped onto it (no hidden +2 any more); bold becomes Semibold.
	FSlateFontInfo Font(int32 Size, bool bBold = false);

	// Read-only figures in Spanish notation, as the 2D plan writes them: Decimal(22.806, 2) = "22,81". Editable fields keep
	// the text of the CAD core.
	FString Decimal(double Value, int32 Digits);
	// A level's elevation in metres: "±0,00", "+2,80", "−3,05".
	FString Elevation(int64 Millimeters);

	// Slate units (pixels at 100 %).
	namespace Size
	{
		inline constexpr float Gutter = 12.0f;
		inline constexpr float Bar = 52.0f;
		inline constexpr float Control = 32.0f;
		inline constexpr float Compact = 28.0f;
		inline constexpr float Field = 30.0f;
		inline constexpr float DockWide = 212.0f;
		inline constexpr float DockNarrow = 52.0f;
		inline constexpr float Inspector = 320.0f;
		inline constexpr float InspectorNarrow = 296.0f;
		inline constexpr float LabelColumn = 104.0f;
		inline constexpr float RadiusKey = 4.0f;
		inline constexpr float RadiusField = 6.0f;
		inline constexpr float RadiusControl = 7.0f;
		inline constexpr float RadiusIsland = 12.0f;
		inline constexpr float RadiusDialog = 16.0f;
		inline constexpr float OpeningCard = 132.0f;
		inline constexpr float OpeningPicture = 92.0f;
		inline constexpr float OpeningCardLabel = 56.0f;
		inline constexpr float OpeningCatalogue = 340.0f;
	}
	// Window sizes where the layout rearranges itself instead of shrinking (ADR-0026).
	namespace Breakpoint
	{
		inline constexpr double DockLabels = 1440.0;      // tool names beside their icons
		inline constexpr double InspectorDocked = 1000.0; // narrower: the inspector opens from the view bar
		inline constexpr double BarLabels = 1100.0;       // wordmark and labels of the secondary bar buttons
		inline constexpr double ModeLabels = 760.0;       // names inside the mode switch
		inline constexpr double LevelIsland = 720.0;      // shorter windows: the levels collapse into a selector
	}

	// A rounded box, cached: the same arguments give the same brush for the life of the program.
	const FSlateBrush* Box(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.0f);
	const FSlateBrush* White();

	enum class EButton : uint8 { Primary, Secondary, Ghost, Danger, Exit, Row, Field, Card };
	const FButtonStyle& ButtonStyle(EButton Kind);
	// Names kept from the first Studio styles.
	const FButtonStyle& Button();
	const FButtonStyle& ExitButton();
	const FComboButtonStyle& ComboButton();
	const FComboBoxStyle& ComboBox();
	const FTableRowStyle& ListRow();
	const FEditableTextBoxStyle& TextBox();
	const FProgressBarStyle& Progress();
	const FScrollBarStyle& ScrollBar();
	const FCheckBoxStyle& CheckBox();
	const FSlateBrush* TooltipBrush();
}
