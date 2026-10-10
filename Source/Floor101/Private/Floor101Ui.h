#pragma once

#include "CoreMinimal.h"
#include "Floor101Style.h"
#include "SFloor101Icon.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"

class SComboButton;
class SToolTip;

// Shared Cota Cero widgets (docs/ui-cota-cero.md). They only present: every one takes the callback it runs, so each
// control keeps calling exactly what it called before the redesign.
namespace Floor101::Ui
{
	using Floor101::Studio::EButton;
	using Floor101::Studio::EText;

	TSharedRef<SWidget> Icon(EFloor101Icon Icon, float Size = 20.0f, TAttribute<FSlateColor> Color = FSlateColor::UseForeground());
	TSharedRef<SWidget> Label(TAttribute<FText> Text, EText Style = EText::Body, TAttribute<FSlateColor> Color = FSlateColor::UseForeground(),
		bool bWrap = false);

	// A key drawn as a keycap; Shortcut splits "Ctrl+Shift+S" into one cap per key.
	TSharedRef<SWidget> Keycap(const FText& Key, TAttribute<FSlateColor> Color = FSlateColor::UseForeground());
	TSharedRef<SWidget> Shortcut(const FText& Keys);
	// A tooltip in the system style, with the shortcut drawn as keycaps.
	TSharedPtr<SToolTip> Tooltip(TAttribute<FText> Text, const FText& Keys = FText::GetEmpty());

	// A floating panel: rounded, outlined, with a soft shadow around it. It fades in when it appears.
	TSharedRef<SWidget> Island(const TSharedRef<SWidget>& Content, const FMargin& Padding = FMargin(6.0f),
		TAttribute<EVisibility> Visibility = EVisibility::SelfHitTestInvisible, float Radius = Studio::Size::RadiusIsland);

	// A text button with an optional leading icon.
	TSharedRef<SWidget> Button(EButton Kind, TAttribute<FText> Text, TFunction<void()> OnClicked, TOptional<EFloor101Icon> Leading = {},
		TAttribute<FText> Tip = FText::GetEmpty(), const FText& Keys = FText::GetEmpty(), TAttribute<bool> Enabled = true,
		float Height = Studio::Size::Control);
	// A square icon button; Active keeps it highlighted (a view that is on).
	TSharedRef<SWidget> IconButton(EFloor101Icon Icon, TFunction<void()> OnClicked, TAttribute<FText> Tip, const FText& Keys = FText::GetEmpty(),
		TAttribute<bool> Active = false, TAttribute<bool> Enabled = true, float Size = Studio::Size::Control);
	// Any content made clickable, with the selected background while Active (tools, choices, chips).
	TSharedRef<SWidget> Selectable(const TSharedRef<SWidget>& Content, TAttribute<bool> Active, TFunction<void()> OnClicked,
		TAttribute<FText> Tip = FText::GetEmpty(), const FText& Keys = FText::GetEmpty(), TAttribute<bool> Enabled = true,
		float Radius = Studio::Size::RadiusControl);
	// A text choice of a group (shape, type, network...): brass text on the selected background while Active.
	TSharedRef<SWidget> Choice(TAttribute<FText> Text, TAttribute<bool> Active, TFunction<void()> OnClicked, TAttribute<FText> Tip = FText::GetEmpty(),
		const TSharedPtr<SWidget>& Leading = nullptr, bool bCentered = true);

	struct FSegment
	{
		TOptional<EFloor101Icon> Icon;
		FText Text;
		FText Key;
		FText Tip;
		TFunction<bool()> IsActive;
		TFunction<void()> OnClicked;
	};
	// Mutually exclusive options in one track. ShowText false leaves the icons (narrow windows).
	TSharedRef<SWidget> Segmented(const TArray<FSegment>& Segments, TAttribute<bool> ShowText = true, bool bSmall = false, bool bFill = false);

	// An on/off switch: it shows the state, not the action.
	TSharedRef<SWidget> Switch(TAttribute<bool> IsOn, TFunction<void(bool)> OnToggled, TAttribute<bool> Enabled = true);

	// An uppercase group heading with a hairline above it.
	TSharedRef<SWidget> Section(TAttribute<FText> Title, bool bDivider = true);
	// Label on the left, control filling the rest.
	TSharedRef<SWidget> Row(TAttribute<FText> Label, const TSharedRef<SWidget>& Control, TAttribute<FText> Tip = FText::GetEmpty());
	// A short explanation with its icon.
	TSharedRef<SWidget> Note(TAttribute<FText> Text, EFloor101Icon Icon = EFloor101Icon::Info, TAttribute<FSlateColor> Accent = FSlateColor::UseForeground());
	TSharedRef<SWidget> Divider(bool bVertical = false, float Length = 20.0f);
	TSharedRef<SWidget> Dot(TAttribute<FSlateColor> Color, float Size = 7.0f);
	// "101st FLOOR" with the raised suffix; Scale 1 is the bar size.
	TSharedRef<SWidget> Wordmark(float Scale = 1.0f);

	// Menus: rows run their action and close every open menu.
	TSharedRef<SWidget> MenuItem(TOptional<EFloor101Icon> Icon, TAttribute<FText> Text, TFunction<void()> OnClicked, const FText& Keys = FText::GetEmpty(),
		TAttribute<FText> Tip = FText::GetEmpty(), TFunction<bool()> IsChecked = nullptr, TAttribute<bool> Enabled = true);
	TSharedRef<SWidget> MenuHeading(const FText& Text);
	TSharedRef<SWidget> MenuSeparator();
	// A button that opens a menu built when it opens.
	TSharedRef<SComboButton> MenuButton(TSharedRef<SWidget> ButtonContent, TFunction<TSharedRef<SWidget>()> MenuContent, TAttribute<FText> Tip = FText::GetEmpty(),
		EButton Kind = EButton::Ghost, FMargin Padding = FMargin(10.0f, 0.0f), float Height = Studio::Size::Control);
	// The usual content of such a button: icon, text and a chevron.
	TSharedRef<SWidget> MenuButtonContent(TOptional<EFloor101Icon> Icon, TAttribute<FText> Text, TAttribute<EVisibility> TextVisibility = EVisibility::Visible);

	// A field for a value typed like the CAD inputs: Apply receives the committed text unless it is unchanged.
	TSharedRef<SWidget> Field(TAttribute<FText> Current, TFunction<void(const FString&)> Apply, bool bNumeric = true,
		TAttribute<FText> Hint = FText::GetEmpty(), TAttribute<FText> Tip = FText::GetEmpty());
}
