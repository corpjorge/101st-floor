#include "Floor101Style.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

namespace Floor101::Studio
{
	FLinearColor Color(const TCHAR* Hex) { return FLinearColor::FromSRGBColor(FColor::FromHex(Hex)); }

	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		const int32 ReadableSize = Size <= 12 ? Size + 2 : Size;
		// Read the installed Windows font; do not copy or redistribute Microsoft's font binaries.
		static const FString Regular = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR")), TEXT("Fonts/segoeui.ttf"));
		static const FString Bold = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR")), TEXT("Fonts/segoeuib.ttf"));
		static const auto RegularFont = MakeShared<FCompositeFont>(TEXT("Regular"), Regular, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		static const auto BoldFont = MakeShared<FCompositeFont>(TEXT("Bold"), Bold, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		const FString& Path = bBold ? Bold : Regular;
		return IFileManager::Get().FileExists(*Path) ? FSlateFontInfo(bBold ? BoldFont : RegularFont, ReadableSize, bBold ? TEXT("Bold") : TEXT("Regular"))
			: FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", ReadableSize);
	}

	const FButtonStyle& Button()
	{
		static const FButtonStyle Style = FButtonStyle(FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.SetNormal(FSlateRoundedBoxBrush(Color(TEXT("30373D")), 4.0f))
			.SetHovered(FSlateRoundedBoxBrush(Color(TEXT("3C454B")), 4.0f))
			.SetPressed(FSlateRoundedBoxBrush(Color(TEXT("475157")), 4.0f))
			.SetDisabled(FSlateRoundedBoxBrush(Color(TEXT("252B30")), 4.0f))
			.SetNormalForeground(Color(TEXT("EEF0EC"))).SetHoveredForeground(Color(TEXT("EEF0EC")))
			.SetPressedForeground(Color(TEXT("D6AD78"))).SetDisabledForeground(Color(TEXT("88918F")))
			.SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f));
		return Style;
	}

	const FComboButtonStyle& ComboButton()
	{
		static const FComboButtonStyle Style = FComboButtonStyle(FCoreStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton"))
			.SetButtonStyle(Button()).SetMenuBorderBrush(FSlateRoundedBoxBrush(Color(TEXT("252B30")), 4.0f)).SetMenuBorderPadding(FMargin(8.0f));
		return Style;
	}

	const FComboBoxStyle& ComboBox()
	{
		static const FComboBoxStyle Style = FComboBoxStyle(FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox")).SetComboButtonStyle(ComboButton());
		return Style;
	}

	const FProgressBarStyle& Progress()
	{
		static const FProgressBarStyle Style = FProgressBarStyle(FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar"))
			.SetBackgroundImage(FSlateRoundedBoxBrush(Color(TEXT("1D2327")), 2.0f))
			.SetFillImage(FSlateRoundedBoxBrush(Color(TEXT("D6AD78")), 2.0f))
			.SetMarqueeImage(FSlateRoundedBoxBrush(Color(TEXT("D6AD78")), 2.0f));
		return Style;
	}

	const FEditableTextBoxStyle& TextBox()
	{
		static const FEditableTextBoxStyle Style = FEditableTextBoxStyle(FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
			.SetBackgroundImageNormal(FSlateRoundedBoxBrush(Color(TEXT("1D2327")), 4.0f, Color(TEXT("495056")), 1.0f))
			.SetBackgroundImageHovered(FSlateRoundedBoxBrush(Color(TEXT("1D2327")), 4.0f, Color(TEXT("69716F")), 1.0f))
			.SetBackgroundImageFocused(FSlateRoundedBoxBrush(Color(TEXT("1D2327")), 4.0f, Color(TEXT("D6AD78")), 1.0f))
			.SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Color(TEXT("252B30")), 4.0f))
			.SetForegroundColor(Color(TEXT("EEF0EC"))).SetPadding(FMargin(8.0f, 5.0f));
		return Style;
	}
}
