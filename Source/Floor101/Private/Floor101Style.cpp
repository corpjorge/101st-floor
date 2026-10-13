#include "Floor101Style.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

namespace Floor101::Studio
{
	FLinearColor Color(const TCHAR* Hex) { return FLinearColor::FromSRGBColor(FColor::FromHex(Hex)); }

	namespace
	{
		FLinearColor Alpha(const TCHAR* Hex, float Opacity)
		{
			FLinearColor Value = Color(Hex);
			Value.A = Opacity;
			return Value;
		}

		// Installed Windows fonts, read in place: Microsoft's font files are never copied or redistributed (ADR-0026).
		enum class EFace : uint8 { Semilight, Regular, Semibold, Bold, Din };

		FSlateFontInfo Face(EFace Kind, float Size, int32 LetterSpacing = 0)
		{
			struct FEntry
			{
				const TCHAR* File;
				const char* Fallback; // Roboto from the engine when the Windows font is missing
			};
			static const FEntry Entries[] = {
				{TEXT("segoeuisl.ttf"), "Light"},
				{TEXT("segoeui.ttf"), "Regular"},
				{TEXT("seguisb.ttf"), "Bold"},
				{TEXT("segoeuib.ttf"), "Bold"},
				{TEXT("bahnschrift.ttf"), "Regular"},
			};
			static TSharedPtr<const FCompositeFont> Fonts[UE_ARRAY_COUNT(Entries)];
			static bool bLoaded[UE_ARRAY_COUNT(Entries)] = {};
			const int32 Index = static_cast<int32>(Kind);
			if (!bLoaded[Index])
			{
				bLoaded[Index] = true;
				const FString Path = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR")), TEXT("Fonts"), Entries[Index].File);
				if (IFileManager::Get().FileExists(*Path))
				{
					Fonts[Index] = MakeShared<FCompositeFont>(TEXT("Regular"), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
				}
			}
			FSlateFontInfo Info = Fonts[Index].IsValid() ? FSlateFontInfo(Fonts[Index], Size, TEXT("Regular"))
				// Without Bahnschrift the figures use Segoe UI Semibold, as wide and as legible.
				: Kind == EFace::Din ? Face(EFace::Semibold, Size)
				: FCoreStyle::GetDefaultFontStyle(Entries[Index].Fallback, FMath::RoundToInt(Size));
			Info.LetterSpacing = LetterSpacing;
			return Info;
		}

		FButtonStyle MakeButton(const FSlateBrush* Normal, const FSlateBrush* Hovered, const FSlateBrush* Pressed, const FSlateBrush* Disabled,
			const FLinearColor& Foreground, const FLinearColor& HoveredForeground, const FLinearColor& PressedForeground, const FLinearColor& DisabledForeground)
		{
			return FButtonStyle(FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.SetNormal(*Normal).SetHovered(*Hovered).SetPressed(*Pressed).SetDisabled(*Disabled)
				.SetNormalForeground(Foreground).SetHoveredForeground(HoveredForeground)
				.SetPressedForeground(PressedForeground).SetDisabledForeground(DisabledForeground)
				.SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f));
		}
	}

	const FPalette& Palette()
	{
		static const FPalette Value = []
		{
			FPalette P;
			P.Canvas = Color(TEXT("121518"));
			P.Bar = Color(TEXT("1A1E22"));
			P.Island = Color(TEXT("1F2428"));
			P.Raised = Color(TEXT("262C31"));
			P.Hover = Color(TEXT("2D3439"));
			P.Pressed = Color(TEXT("363E44"));
			P.Inset = Color(TEXT("15181B"));
			P.Line = Color(TEXT("2C3238"));
			P.EdgeSoft = Color(TEXT("3A4248"));
			P.Edge = Color(TEXT("697177")); // 3.1:1 on the islands: the edge of a control
			P.Tip = Color(TEXT("2A3036"));
			P.Text1 = Color(TEXT("ECEEEA"));
			P.Text2 = Color(TEXT("A9B1AE"));
			P.Text3 = Color(TEXT("88918E"));
			P.Text4 = Color(TEXT("5E6764"));
			P.Brass = Color(TEXT("D6A65F"));
			P.BrassHi = Color(TEXT("E2B572"));
			P.BrassLo = Color(TEXT("BF8F4B"));
			P.BrassSel = Color(TEXT("393630"));
			P.BrassLine = Alpha(TEXT("D6A65F"), 0.55f);
			P.OnBrass = Color(TEXT("17140F"));
			P.Danger = Color(TEXT("EF7F6E"));
			P.DangerFill = Color(TEXT("B8443A"));
			P.DangerPress = Color(TEXT("9C3830"));
			P.Ok = Color(TEXT("74C095"));
			P.Warn = Color(TEXT("F2994A"));
			P.Info = Color(TEXT("7FA6D6"));
			P.Paper = Color(TEXT("E4E2DC"));
			P.Scrim = Alpha(TEXT("0C0E10"), 0.62f);
			P.Glass = Alpha(TEXT("1A1E22"), 0.92f);
			P.Shadow = FLinearColor(0.0f, 0.0f, 0.0f, 0.045f); // one layer of an island's shadow
			P.Sheen = FLinearColor(1.0f, 1.0f, 1.0f, 0.05f);
			P.GlassEdge = FLinearColor(1.0f, 1.0f, 1.0f, 0.08f);
			P.BarEdge = Color(TEXT("262B30"));
			P.SwatchEdge = FLinearColor(0.0f, 0.0f, 0.0f, 0.35f);
			P.OnPaper = Color(TEXT("5C5A55"));
			P.Missing = FLinearColor(0.5f, 0.5f, 0.52f); // a picture or finish that is not there
			P.Invalid = FLinearColor(1.0f, 0.0f, 1.0f); // a finish id the catalogue does not know
			return P;
		}();
		return Value;
	}

	FSlateFontInfo Text(EText Style)
	{
		switch (Style)
		{
		case EText::Display: return Face(EFace::Semilight, 39.0f);
		case EText::Title: return Face(EFace::Semibold, 13.5f);
		case EText::Heading: return Face(EFace::Semibold, 12.0f);
		case EText::BodyStrong: return Face(EFace::Semibold, 10.5f);
		case EText::Button: return Face(EFace::Semibold, 9.75f);
		case EText::Label: return Face(EFace::Regular, 9.75f);
		case EText::Help: return Face(EFace::Regular, 9.4f);
		case EText::Overline: return Face(EFace::Semibold, 8.25f, 90);
		case EText::Keycap: return Face(EFace::Semibold, 8.25f);
		case EText::Numeric: return Face(EFace::Din, 10.5f);
		case EText::NumericSmall: return Face(EFace::Din, 9.75f);
		case EText::NumericStrong: return Face(EFace::Din, 11.25f);
		case EText::NumericLarge: return Face(EFace::Din, 15.0f);
		case EText::Wordmark: return Face(EFace::Din, 11.25f, 140);
		case EText::WordmarkSuffix: return Face(EFace::Din, 7.0f, 60);
		default: return Face(EFace::Regular, 10.5f);
		}
	}

	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		const float Mapped = Size <= 9 ? 9.4f : Size == 10 ? 10.5f : Size == 11 ? 11.25f : Size == 12 ? 11.5f : static_cast<float>(Size);
		return Face(bBold ? EFace::Semibold : EFace::Regular, Mapped);
	}

	FString Decimal(double Value, int32 Digits)
	{
		const double Scale = FMath::Pow(10.0, static_cast<double>(Digits));
		double Rounded = FMath::RoundToDouble(Value * Scale) / Scale;
		if (Rounded == 0.0) { Rounded = 0.0; } // never "-0,00"
		return FString::Printf(TEXT("%.*f"), Digits, Rounded).Replace(TEXT("."), TEXT(","));
	}

	FString Elevation(int64 Millimeters)
	{
		const FString Metres = Decimal(FMath::Abs(static_cast<double>(Millimeters)) / 1000.0, 2);
		return Millimeters == 0 ? TEXT("±") + Metres : (Millimeters > 0 ? TEXT("+") : TEXT("−")) + Metres;
	}

	const FSlateBrush* Box(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		static TMap<FString, TUniquePtr<FSlateBrush>> Brushes;
		const FString Key = FString::Printf(TEXT("%s|%.2f|%s|%.2f"), *Fill.ToString(), Radius, *Outline.ToString(), OutlineWidth);
		TUniquePtr<FSlateBrush>& Brush = Brushes.FindOrAdd(Key);
		if (!Brush)
		{
			Brush = OutlineWidth > 0.0f ? MakeUnique<FSlateRoundedBoxBrush>(Fill, Radius, Outline, OutlineWidth)
				: MakeUnique<FSlateRoundedBoxBrush>(Fill, Radius);
		}
		return Brush.Get();
	}

	const FSlateBrush* White() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }

	const FButtonStyle& ButtonStyle(EButton Kind)
	{
		static const TArray<FButtonStyle> Styles = []
		{
			const FPalette& P = Palette();
			const float R = Size::RadiusControl;
			const FLinearColor Clear = FLinearColor::Transparent;
			const FLinearColor Wash(1.0f, 1.0f, 1.0f, 0.06f);
			const FLinearColor WashStrong(1.0f, 1.0f, 1.0f, 0.10f);
			TArray<FButtonStyle> Result;
			Result.SetNum(static_cast<int32>(EButton::Card) + 1);
			Result[static_cast<int32>(EButton::Primary)] = MakeButton(Box(P.Brass, R), Box(P.BrassHi, R), Box(P.BrassLo, R), Box(P.Raised, R),
				P.OnBrass, P.OnBrass, P.OnBrass, P.Text4);
			Result[static_cast<int32>(EButton::Secondary)] = MakeButton(Box(P.Raised, R, P.EdgeSoft, 1.0f), Box(P.Hover, R, P.EdgeSoft, 1.0f),
				Box(P.Pressed, R, P.EdgeSoft, 1.0f), Box(P.Raised, R), P.Text1, P.Text1, P.Text1, P.Text4);
			Result[static_cast<int32>(EButton::Ghost)] = MakeButton(Box(Clear, R), Box(Wash, R), Box(WashStrong, R), Box(Clear, R),
				P.Text2, P.Text1, P.Text1, P.Text4);
			Result[static_cast<int32>(EButton::Danger)] = MakeButton(Box(Clear, R), Box(FLinearColor(P.Danger.R, P.Danger.G, P.Danger.B, 0.14f), R),
				Box(FLinearColor(P.Danger.R, P.Danger.G, P.Danger.B, 0.22f), R), Box(Clear, R), P.Danger, P.Danger, P.Danger, P.Text4);
			// Leaving the program stays quiet at rest and turns red only under the pointer.
			Result[static_cast<int32>(EButton::Exit)] = MakeButton(Box(Clear, R), Box(P.DangerFill, R), Box(P.DangerPress, R), Box(Clear, R),
				P.Text2, FLinearColor::White, FLinearColor::White, P.Text4);
			Result[static_cast<int32>(EButton::Row)] = MakeButton(Box(Clear, 6.0f), Box(Wash, 6.0f), Box(WashStrong, 6.0f), Box(Clear, 6.0f),
				P.Text1, P.Text1, P.Text1, P.Text4);
			Result[static_cast<int32>(EButton::Field)] = MakeButton(Box(P.Inset, Size::RadiusField, P.Edge, 1.0f), Box(P.Inset, Size::RadiusField, P.Text3, 1.0f),
				Box(P.Inset, Size::RadiusField, P.Brass, 1.0f), Box(P.Inset, Size::RadiusField, P.Line, 1.0f), P.Text1, P.Text1, P.Text1, P.Text4);
			Result[static_cast<int32>(EButton::Card)] = MakeButton(Box(P.Raised, 9.0f, P.Line, 1.0f), Box(P.Hover, 9.0f, P.EdgeSoft, 1.0f),
				Box(P.Pressed, 9.0f, P.EdgeSoft, 1.0f), Box(P.Raised, 9.0f), P.Text1, P.Text1, P.Text1, P.Text4);
			return Result;
		}();
		return Styles[static_cast<int32>(Kind)];
	}

	const FButtonStyle& Button() { return ButtonStyle(EButton::Secondary); }
	const FButtonStyle& ExitButton() { return ButtonStyle(EButton::Exit); }

	const FComboButtonStyle& ComboButton()
	{
		static const FComboButtonStyle Style = FComboButtonStyle(FCoreStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton"))
			.SetButtonStyle(ButtonStyle(EButton::Ghost))
			.SetMenuBorderBrush(*Box(Palette().Island, 10.0f, Palette().EdgeSoft, 1.0f)).SetMenuBorderPadding(FMargin(6.0f))
			.SetShadowOffset(FVector2D::ZeroVector);
		return Style;
	}

	const FComboBoxStyle& ComboBox()
	{
		static const FComboButtonStyle Field = FComboButtonStyle(ComboButton()).SetButtonStyle(ButtonStyle(EButton::Field));
		static const FComboBoxStyle Style = FComboBoxStyle(FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox"))
			.SetComboButtonStyle(Field).SetContentPadding(FMargin(8.0f, 0.0f, 8.0f, 0.0f)).SetMenuRowPadding(FMargin(8.0f, 6.0f));
		return Style;
	}

	const FTableRowStyle& ListRow()
	{
		static const FTableRowStyle Style = []
		{
			const FPalette& P = Palette();
			const FSlateBrush* Clear = Box(FLinearColor::Transparent, 6.0f);
			const FSlateBrush* Hovered = Box(FLinearColor(1.0f, 1.0f, 1.0f, 0.06f), 6.0f);
			const FSlateBrush* Chosen = Box(P.BrassSel, 6.0f);
			return FTableRowStyle(FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
				.SetEvenRowBackgroundBrush(*Clear).SetOddRowBackgroundBrush(*Clear)
				.SetEvenRowBackgroundHoveredBrush(*Hovered).SetOddRowBackgroundHoveredBrush(*Hovered)
				.SetActiveBrush(*Chosen).SetActiveHoveredBrush(*Chosen).SetInactiveBrush(*Chosen).SetInactiveHoveredBrush(*Chosen)
				.SetActiveHighlightedBrush(*Chosen).SetInactiveHighlightedBrush(*Chosen).SetSelectorFocusedBrush(*Clear)
				.SetTextColor(P.Text1).SetSelectedTextColor(P.Text1);
		}();
		return Style;
	}

	const FEditableTextBoxStyle& TextBox()
	{
		static const FEditableTextBoxStyle Style = []
		{
			const FPalette& P = Palette();
			const float R = Size::RadiusField;
			return FEditableTextBoxStyle(FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
				.SetBackgroundImageNormal(*Box(P.Inset, R, P.Edge, 1.0f))
				.SetBackgroundImageHovered(*Box(P.Inset, R, P.Text3, 1.0f))
				.SetBackgroundImageFocused(*Box(P.Inset, R, P.Brass, 1.0f))
				.SetBackgroundImageReadOnly(*Box(P.Raised, R))
				.SetForegroundColor(P.Text1).SetFocusedForegroundColor(P.Text1).SetReadOnlyForegroundColor(P.Text2)
				.SetPadding(FMargin(10.0f, 5.0f)).SetFont(Text(EText::Numeric));
		}();
		return Style;
	}

	const FProgressBarStyle& Progress()
	{
		static const FProgressBarStyle Style = FProgressBarStyle(FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar"))
			.SetBackgroundImage(*Box(Palette().Inset, 2.0f))
			.SetFillImage(*Box(Palette().Brass, 2.0f))
			.SetMarqueeImage(*Box(Palette().Brass, 2.0f));
		return Style;
	}

	const FScrollBarStyle& ScrollBar()
	{
		static const FScrollBarStyle Style = []
		{
			const FPalette& P = Palette();
			const FSlateBrush* Clear = Box(FLinearColor::Transparent, 3.0f);
			return FScrollBarStyle(FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("Scrollbar"))
				.SetVerticalBackgroundImage(*Clear).SetHorizontalBackgroundImage(*Clear)
				.SetVerticalTopSlotImage(*Clear).SetVerticalBottomSlotImage(*Clear)
				.SetHorizontalTopSlotImage(*Clear).SetHorizontalBottomSlotImage(*Clear)
				.SetNormalThumbImage(*Box(P.EdgeSoft, 3.0f)).SetHoveredThumbImage(*Box(P.Text4, 3.0f)).SetDraggedThumbImage(*Box(P.Text3, 3.0f))
				.SetThickness(6.0f);
		}();
		return Style;
	}

	const FCheckBoxStyle& CheckBox()
	{
		static const FCheckBoxStyle Style = []
		{
			const FPalette& P = Palette();
			FCheckBoxStyle Result(FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox"));
			Result.SetForegroundColor(P.Text1).SetHoveredForegroundColor(P.Text1).SetPressedForegroundColor(P.Text1)
				.SetCheckedForegroundColor(P.Text1).SetCheckedHoveredForegroundColor(P.Text1).SetCheckedPressedForegroundColor(P.Text1);
			return Result;
		}();
		return Style;
	}

	const FSlateBrush* TooltipBrush() { return Box(Palette().Tip, Size::RadiusControl, Palette().EdgeSoft, 1.0f); }
}
