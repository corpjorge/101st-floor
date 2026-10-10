#include "Floor101Ui.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	using Floor101::Studio::EText;

	// A soft shadow under a rounded panel, as concentric translucent boxes that grow outwards (Slate has no blur).
	class SFloor101Shadow final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SFloor101Shadow) {} SLATE_ARGUMENT(float, Radius) SLATE_END_ARGS()
		void Construct(const FArguments& Args)
		{
			for (int32 Layer = 0; Layer < UE_ARRAY_COUNT(Brushes); ++Layer)
			{
				Brushes[Layer] = MakeUnique<FSlateRoundedBoxBrush>(FLinearColor::White, Args._Radius + 2.0f * (Layer + 1));
			}
		}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 LayerId,
			const FWidgetStyle& Style, bool) const override
		{
			const FVector2f Size = FVector2f(Geometry.GetLocalSize());
			for (int32 Layer = 0; Layer < UE_ARRAY_COUNT(Brushes); ++Layer)
			{
				const float Grow = 2.0f * (Layer + 1);
				const FVector2f Offset(-Grow, -Grow + 3.0f + 0.5f * Layer);
				FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(Size + FVector2f(2.0f * Grow), FSlateLayoutTransform(Offset)),
					Brushes[Layer].Get(), ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.045f) * Style.GetColorAndOpacityTint());
			}
			return LayerId;
		}
	private:
		TUniquePtr<FSlateRoundedBoxBrush> Brushes[6];
	};

	// Fades in and rises 4 units over 160 ms whenever it comes back into view (a mode or a selection changed).
	class SFloor101Island final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SFloor101Island) {} SLATE_DEFAULT_SLOT(FArguments, Content) SLATE_END_ARGS()
		void Construct(const FArguments& Args) { ChildSlot[Args._Content.Widget]; }
		virtual void Tick(const FGeometry& Geometry, const double Now, const float Delta) override
		{
			SCompoundWidget::Tick(Geometry, Now, Delta);
			if (Now - LastTick > 0.25) { Appeared = Now; } // it was collapsed: not ticked
			LastTick = Now;
			const float T = FMath::Clamp(static_cast<float>((Now - Appeared) / 0.16), 0.0f, 1.0f);
			const float Ease = 1.0f - FMath::Pow(1.0f - T, 3.0f);
			SetRenderOpacity(Ease);
			SetRenderTransform(FSlateRenderTransform(FVector2f(0.0f, 4.0f * (1.0f - Ease))));
		}
	private:
		double LastTick = -1.0;
		double Appeared = 0.0;
	};

	// The knob slides in 160 ms; the state always comes from IsOn, so undo and other inputs move it too.
	class SFloor101Switch final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SFloor101Switch) {} SLATE_ATTRIBUTE(bool, IsOn) SLATE_ARGUMENT(TFunction<void(bool)>, OnToggled) SLATE_END_ARGS()
		void Construct(const FArguments& Args)
		{
			IsOn = Args._IsOn;
			OnToggled = Args._OnToggled;
			Shown = IsOn.Get(false) ? 1.0f : 0.0f;
			SetCursor(EMouseCursor::Hand);
		}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(34.0, 20.0); }
		virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
		{
			if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !IsEnabled()) { return FReply::Unhandled(); }
			if (OnToggled) { OnToggled(!IsOn.Get(false)); }
			return FReply::Handled();
		}
		virtual void Tick(const FGeometry&, const double, const float Delta) override
		{
			const float Target = IsOn.Get(false) ? 1.0f : 0.0f;
			Shown = FMath::FInterpConstantTo(Shown, Target, Delta, 1.0f / 0.16f);
		}
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer,
			const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const auto& P = Floor101::Studio::Palette();
			const bool bEnabled = ShouldBeEnabled(bParentEnabled);
			const float Ease = Shown * Shown * (3.0f - 2.0f * Shown);
			const FLinearColor Opacity = Style.GetColorAndOpacityTint() * FLinearColor(1.0f, 1.0f, 1.0f, bEnabled ? 1.0f : 0.45f);
			const FLinearColor Track = FMath::Lerp(P.Inset, P.Brass, Ease);
			const FLinearColor Edge = FMath::Lerp(P.Edge, P.Brass, Ease);
			FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), Floor101::Studio::Box(FLinearColor::White, 10.0f, FLinearColor::White, 1.0f),
				ESlateDrawEffect::None, Edge * Opacity);
			FSlateDrawElement::MakeBox(Out, Layer + 1, Geometry.ToPaintGeometry(FVector2f(32.0f, 18.0f), FSlateLayoutTransform(FVector2f(1.0f, 1.0f))),
				Floor101::Studio::Box(FLinearColor::White, 9.0f), ESlateDrawEffect::None, Track * Opacity);
			const FLinearColor Knob = FMath::Lerp(P.Text2, P.OnBrass, Ease);
			FSlateDrawElement::MakeBox(Out, Layer + 2, Geometry.ToPaintGeometry(FVector2f(14.0f, 14.0f), FSlateLayoutTransform(FVector2f(3.0f + 14.0f * Ease, 3.0f))),
				Floor101::Studio::Box(FLinearColor::White, 7.0f), ESlateDrawEffect::None, Knob * Opacity);
			return Layer + 2;
		}
	private:
		TAttribute<bool> IsOn;
		TFunction<void(bool)> OnToggled;
		float Shown = 0.0f;
	};

	TSharedRef<SWidget> TextOf(TAttribute<FText> Text, EText Style)
	{
		return SNew(STextBlock).Text(Text).Font(Floor101::Studio::Text(Style)).ColorAndOpacity(FSlateColor::UseForeground());
	}
}

namespace Floor101::Ui
{
	TSharedRef<SWidget> Icon(EFloor101Icon Kind, float Size, TAttribute<FSlateColor> Color)
	{
		return SNew(SFloor101Icon).Icon(Kind).Size(Size).Color(Color);
	}

	TSharedRef<SWidget> Label(TAttribute<FText> Text, EText Style, TAttribute<FSlateColor> Color, bool bWrap)
	{
		return SNew(STextBlock).Text(Text).Font(Studio::Text(Style)).ColorAndOpacity(Color).AutoWrapText(bWrap);
	}

	TSharedRef<SWidget> Keycap(const FText& Key, TAttribute<FSlateColor> Color)
	{
		const auto& P = Studio::Palette();
		return SNew(SBox).MinDesiredWidth(18.0f).HeightOverride(18.0f)
			[
				SNew(SBorder).BorderImage(Studio::Box(P.Raised, Studio::Size::RadiusKey, P.EdgeSoft, 1.0f)).Padding(FMargin(5.0f, 0.0f))
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				.ForegroundColor_Lambda([Color]() { const FSlateColor Value = Color.Get(); return Value.IsColorSpecified() ? Value : FSlateColor(Studio::Palette().Text2); })
				[
					SNew(STextBlock).Text(Key).Font(Studio::Text(EText::Keycap)).ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
	}

	TSharedRef<SWidget> Shortcut(const FText& Keys)
	{
		TSharedRef<SHorizontalBox> Caps = SNew(SHorizontalBox);
		TArray<FString> Parts;
		Keys.ToString().ParseIntoArray(Parts, TEXT("+"));
		for (const FString& Part : Parts)
		{
			Caps->AddSlot().AutoWidth().Padding(Caps->NumSlots() > 0 ? 3.0f : 0.0f, 0.0f, 0.0f, 0.0f)
				[Keycap(FText::FromString(Part.TrimStartAndEnd()))];
		}
		return Caps;
	}

	TSharedPtr<SToolTip> Tooltip(TAttribute<FText> Text, const FText& Keys)
	{
		if (!Text.IsBound() && Text.Get().IsEmpty() && Keys.IsEmpty())
		{
			return nullptr;
		}
		const auto& P = Studio::Palette();
		return SNew(SToolTip).BorderImage(Studio::TooltipBrush()).TextMargin(FMargin(10.0f, 6.0f, 8.0f, 6.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).MaxDesiredWidth(360.0f)
					[SNew(STextBlock).Text(Text).Font(Studio::Text(EText::Label)).ColorAndOpacity(P.Text1).AutoWrapText(true)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(Keys.IsEmpty() ? 0.0f : 8.0f, 0.0f, 0.0f, 0.0f)
				[
					Keys.IsEmpty() ? SNullWidget::NullWidget : Shortcut(Keys)
				]
			];
	}

	TSharedRef<SWidget> Island(const TSharedRef<SWidget>& Content, const FMargin& Padding, TAttribute<EVisibility> Visibility, float Radius)
	{
		const auto& P = Studio::Palette();
		return SNew(SFloor101Island).Visibility(Visibility)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SFloor101Shadow).Radius(Radius).Visibility(EVisibility::HitTestInvisible)]
				+ SOverlay::Slot()
				[
					SNew(SBorder).BorderImage(Studio::Box(P.Island, Radius, P.Line, 1.0f)).Padding(Padding).ForegroundColor(P.Text1)
					[Content]
				]
			];
	}

	TSharedRef<SWidget> Button(EButton Kind, TAttribute<FText> Text, TFunction<void()> OnClicked, TOptional<EFloor101Icon> Leading,
		TAttribute<FText> Tip, const FText& Keys, TAttribute<bool> Enabled, float Height)
	{
		TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);
		if (Leading.IsSet())
		{
			Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)[Icon(Leading.GetValue(), 16.0f)];
		}
		Content->AddSlot().AutoWidth().VAlign(VAlign_Center)[TextOf(Text, EText::Button)];
		return SNew(SBox).HeightOverride(Height)
			[
				SNew(SButton).ButtonStyle(&Studio::ButtonStyle(Kind)).IsFocusable(false).IsEnabled(Enabled)
				.ContentPadding(FMargin(Height >= 40.0f ? 18.0f : 12.0f, 0.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
				.ToolTip(Tooltip(Tip, Keys))
				.OnClicked_Lambda([OnClicked]() { if (OnClicked) { OnClicked(); } return FReply::Handled(); })
				[Content]
			];
	}

	TSharedRef<SWidget> IconButton(EFloor101Icon Kind, TFunction<void()> OnClicked, TAttribute<FText> Tip, const FText& Keys,
		TAttribute<bool> Active, TAttribute<bool> Enabled, float Size)
	{
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SBorder).BorderImage(Studio::Box(FLinearColor::White, Studio::Size::RadiusControl)).Padding(0.0f)
				.BorderBackgroundColor_Lambda([Active]() { return Active.Get() ? Studio::Palette().BrassSel : FLinearColor::Transparent; })
				[
					SNew(SButton).ButtonStyle(&Studio::ButtonStyle(EButton::Ghost)).IsFocusable(false).IsEnabled(Enabled)
					.ContentPadding(0.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
					.ToolTip(Tooltip(Tip, Keys))
					.ForegroundColor_Lambda([Active]() { return Active.Get() ? FSlateColor(Studio::Palette().Brass) : FSlateColor::UseStyle(); })
					.OnClicked_Lambda([OnClicked]() { if (OnClicked) { OnClicked(); } return FReply::Handled(); })
					[Icon(Kind, Size >= 32.0f ? 20.0f : 16.0f)]
				]
			];
	}

	TSharedRef<SWidget> Selectable(const TSharedRef<SWidget>& Content, TAttribute<bool> Active, TFunction<void()> OnClicked, TAttribute<FText> Tip,
		const FText& Keys, TAttribute<bool> Enabled, float Radius)
	{
		return SNew(SBorder).BorderImage(Studio::Box(FLinearColor::White, Radius)).Padding(0.0f)
			.BorderBackgroundColor_Lambda([Active]() { return Active.Get() ? Studio::Palette().BrassSel : FLinearColor::Transparent; })
			[
				SNew(SButton).ButtonStyle(&Studio::ButtonStyle(EButton::Row)).IsFocusable(false).IsEnabled(Enabled).ContentPadding(0.0f)
				.ToolTip(Tooltip(Tip, Keys))
				.ForegroundColor_Lambda([Active]() { return FSlateColor(Active.Get() ? Studio::Palette().Text1 : Studio::Palette().Text2); })
				.OnClicked_Lambda([OnClicked]() { if (OnClicked) { OnClicked(); } return FReply::Handled(); })
				[Content]
			];
	}

	TSharedRef<SWidget> Choice(TAttribute<FText> Text, TAttribute<bool> Active, TFunction<void()> OnClicked, TAttribute<FText> Tip,
		const TSharedPtr<SWidget>& Leading, bool bCentered)
	{
		TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);
		if (Leading.IsValid())
		{
			Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)[Leading.ToSharedRef()];
		}
		Content->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Text).Font(Studio::Text(EText::Button))
				.ColorAndOpacity_Lambda([Active]() { return FSlateColor(Active.Get() ? Studio::Palette().Brass : Studio::Palette().Text2); })
			];
		return SNew(SBox).MinDesiredHeight(Studio::Size::Compact)
			[
				SNew(SBorder).BorderImage(Studio::Box(FLinearColor::White, Studio::Size::RadiusControl)).Padding(0.0f)
				.BorderBackgroundColor_Lambda([Active]() { return Active.Get() ? Studio::Palette().BrassSel : FLinearColor::Transparent; })
				[
					SNew(SBorder).BorderImage_Lambda([Active]()
					{
						return Active.Get() ? Studio::Box(FLinearColor::Transparent, Studio::Size::RadiusControl, Studio::Palette().BrassLine, 1.0f)
							: Studio::Box(FLinearColor::Transparent, Studio::Size::RadiusControl, Studio::Palette().EdgeSoft, 1.0f);
					}).Padding(0.0f)
					[
						SNew(SButton).ButtonStyle(&Studio::ButtonStyle(EButton::Row)).IsFocusable(false).ContentPadding(FMargin(10.0f, 5.0f))
						.HAlign(bCentered ? HAlign_Center : HAlign_Left).VAlign(VAlign_Center).ToolTip(Tooltip(Tip))
						.OnClicked_Lambda([OnClicked]() { if (OnClicked) { OnClicked(); } return FReply::Handled(); })
						[Content]
					]
				]
			];
	}

	TSharedRef<SWidget> Segmented(const TArray<FSegment>& Segments, TAttribute<bool> ShowText, bool bSmall, bool bFill)
	{
		const auto& P = Studio::Palette();
		const float Height = bSmall ? 26.0f : 30.0f;
		TSharedRef<SHorizontalBox> Track = SNew(SHorizontalBox);
		for (const FSegment& Segment : Segments)
		{
			TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);
			const TFunction<bool()> IsActive = Segment.IsActive;
			if (Segment.Icon.IsSet())
			{
				Content->AddSlot().AutoWidth().VAlign(VAlign_Center)
					[Icon(Segment.Icon.GetValue(), 16.0f, TAttribute<FSlateColor>::CreateLambda([IsActive]()
					{
						return IsActive && IsActive() ? FSlateColor(Studio::Palette().Brass) : FSlateColor::UseForeground();
					}))];
			}
			const bool bHasIcon = Segment.Icon.IsSet();
			Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(bHasIcon ? 8.0f : 0.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(Segment.Text).Font(Studio::Text(EText::Button)).ColorAndOpacity(FSlateColor::UseForeground())
					.Visibility_Lambda([ShowText, bHasIcon]() { return ShowText.Get() || !bHasIcon ? EVisibility::Visible : EVisibility::Collapsed; })
				];
			if (!Segment.Key.IsEmpty())
			{
				Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SBox).Visibility_Lambda([ShowText]() { return ShowText.Get() ? EVisibility::Visible : EVisibility::Collapsed; })
						[Keycap(Segment.Key)]
					];
			}
			const TFunction<void()> OnClicked = Segment.OnClicked;
			TSharedRef<SWidget> Item = SNew(SBox).HeightOverride(Height)
				[
					SNew(SBorder).BorderImage_Lambda([IsActive]()
					{
						return IsActive && IsActive() ? Studio::Box(Studio::Palette().Hover, 6.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.05f), 1.0f) : Studio::Box(FLinearColor::Transparent, 6.0f);
					}).Padding(0.0f)
					[
						SNew(SButton).ButtonStyle(&Studio::ButtonStyle(EButton::Row)).IsFocusable(false)
						.ContentPadding(FMargin(bSmall ? 10.0f : 12.0f, 0.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.ToolTip(Tooltip(Segment.Tip, Segment.Key))
						.ForegroundColor_Lambda([IsActive]() { return FSlateColor(IsActive && IsActive() ? Studio::Palette().Text1 : Studio::Palette().Text2); })
						.OnClicked_Lambda([OnClicked]() { if (OnClicked) { OnClicked(); } return FReply::Handled(); })
						[Content]
					]
				];
			if (bFill) { Track->AddSlot().FillWidth(1.0f).Padding(Track->NumSlots() > 0 ? 2.0f : 0.0f, 0.0f, 0.0f, 0.0f)[Item]; }
			else { Track->AddSlot().AutoWidth().Padding(Track->NumSlots() > 0 ? 2.0f : 0.0f, 0.0f, 0.0f, 0.0f)[Item]; }
		}
		return SNew(SBorder).BorderImage(Studio::Box(P.Inset, 9.0f, P.Line, 1.0f)).Padding(3.0f)[Track];
	}

	TSharedRef<SWidget> Switch(TAttribute<bool> IsOn, TFunction<void(bool)> OnToggled, TAttribute<bool> Enabled)
	{
		return SNew(SFloor101Switch).IsOn(IsOn).OnToggled(OnToggled).IsEnabled(Enabled);
	}

	TSharedRef<SWidget> Section(TAttribute<FText> Title, bool bDivider)
	{
		const auto& P = Studio::Palette();
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(1.0f).Visibility(bDivider ? EVisibility::Visible : EVisibility::Collapsed)
				[SNew(SBorder).BorderImage(Studio::White()).BorderBackgroundColor(P.Line).Padding(0.0f)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 14.0f, 16.0f, 6.0f)
			[
				SNew(STextBlock).Text_Lambda([Title]() { return Title.Get().ToUpper(); }).Font(Studio::Text(EText::Overline)).ColorAndOpacity(P.Text3)
			];
	}

	TSharedRef<SWidget> Row(TAttribute<FText> Label, const TSharedRef<SWidget>& Control, TAttribute<FText> Tip)
	{
		const auto& P = Studio::Palette();
		return SNew(SBox).MinDesiredHeight(Studio::Size::Control).ToolTip(Tooltip(Tip))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(Studio::Size::LabelColumn).Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[SNew(STextBlock).Text(Label).Font(Studio::Text(EText::Label)).ColorAndOpacity(P.Text2).AutoWrapText(true)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[Control]
			];
	}

	TSharedRef<SWidget> Note(TAttribute<FText> Text, EFloor101Icon Kind, TAttribute<FSlateColor> Accent)
	{
		const auto& P = Studio::Palette();
		const FSlateColor Tint = Accent.IsSet() && Accent.Get().IsColorSpecified() ? Accent.Get() : FSlateColor(P.Info);
		return SNew(SBorder).BorderImage(Studio::Box(FLinearColor(P.Info.R, P.Info.G, P.Info.B, 0.08f), 8.0f)).Padding(FMargin(10.0f, 8.0f, 12.0f, 8.0f))
			.Visibility_Lambda([Text]() { return Text.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 1.0f, 10.0f, 0.0f)[Icon(Kind, 16.0f, Tint)]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[SNew(STextBlock).Text(Text).Font(Studio::Text(EText::Help)).ColorAndOpacity(P.Text2).AutoWrapText(true)]
			];
	}

	TSharedRef<SWidget> Divider(bool bVertical, float Length)
	{
		return SNew(SBox).WidthOverride(bVertical ? 1.0f : FOptionalSize()).HeightOverride(bVertical ? Length : 1.0f)
			[SNew(SBorder).BorderImage(Studio::White()).BorderBackgroundColor(Studio::Palette().Line).Padding(0.0f)];
	}

	TSharedRef<SWidget> Dot(TAttribute<FSlateColor> Color, float Size)
	{
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[SNew(SBorder).BorderImage(Studio::Box(FLinearColor::White, Size * 0.5f)).BorderBackgroundColor(Color).Padding(0.0f)];
	}

	TSharedRef<SWidget> MenuItem(TOptional<EFloor101Icon> Leading, TAttribute<FText> Text, TFunction<void()> OnClicked, const FText& Keys,
		TAttribute<FText> Tip, TFunction<bool()> IsChecked, TAttribute<bool> Enabled)
	{
		const auto& P = Studio::Palette();
		TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);
		Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)
				[
					IsChecked ? Icon(EFloor101Icon::Check, 16.0f, TAttribute<FSlateColor>::CreateLambda([IsChecked]()
						{ return FSlateColor(IsChecked() ? Studio::Palette().Brass : FLinearColor::Transparent); }))
					: Leading.IsSet() ? Icon(Leading.GetValue(), 16.0f, FSlateColor(P.Text2)) : SNullWidget::NullWidget
				]
			];
		Content->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)[TextOf(Text, EText::Body)];
		if (!Keys.IsEmpty())
		{
			Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f, 0.0f, 0.0f)[Shortcut(Keys)];
		}
		return SNew(SBox).MinDesiredHeight(32.0f)
			[
				SNew(SButton).ButtonStyle(&Studio::ButtonStyle(EButton::Row)).IsFocusable(false).IsEnabled(Enabled)
				.ContentPadding(FMargin(8.0f, 4.0f)).VAlign(VAlign_Center).ToolTip(Tooltip(Tip))
				.OnClicked_Lambda([OnClicked]()
				{
					FSlateApplication::Get().DismissAllMenus();
					if (OnClicked) { OnClicked(); }
					return FReply::Handled();
				})
				[Content]
			];
	}

	TSharedRef<SWidget> MenuHeading(const FText& Text)
	{
		return SNew(SBox).Padding(FMargin(8.0f, 10.0f, 8.0f, 4.0f))
			[SNew(STextBlock).Text(Text.ToUpper()).Font(Studio::Text(EText::Overline)).ColorAndOpacity(Studio::Palette().Text3)];
	}

	TSharedRef<SWidget> MenuSeparator()
	{
		return SNew(SBox).Padding(FMargin(4.0f, 6.0f))[Divider()];
	}

	TSharedRef<SComboButton> MenuButton(TSharedRef<SWidget> ButtonContent, TFunction<TSharedRef<SWidget>()> MenuContent, TAttribute<FText> Tip,
		EButton Kind, FMargin Padding, float Height)
	{
		static TArray<TUniquePtr<FComboButtonStyle>> Styles;
		while (Styles.Num() <= static_cast<int32>(Kind)) { Styles.Add(nullptr); }
		TUniquePtr<FComboButtonStyle>& Style = Styles[static_cast<int32>(Kind)];
		if (!Style)
		{
			Style = MakeUnique<FComboButtonStyle>(FComboButtonStyle(Studio::ComboButton()).SetButtonStyle(Studio::ButtonStyle(Kind)));
		}
		TSharedRef<SComboButton> Combo = SNew(SComboButton).ComboButtonStyle(Style.Get()).HasDownArrow(false).IsFocusable(false)
			.ContentPadding(Padding).ToolTip(Tooltip(Tip)).ForegroundColor(FSlateColor::UseStyle())
			.OnGetMenuContent_Lambda([MenuContent]() { return MenuContent(); })
			.ButtonContent()[SNew(SBox).HeightOverride(Height).VAlign(VAlign_Center)[ButtonContent]];
		return Combo;
	}

	TSharedRef<SWidget> MenuButtonContent(TOptional<EFloor101Icon> Leading, TAttribute<FText> Text, TAttribute<EVisibility> TextVisibility)
	{
		TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);
		if (Leading.IsSet())
		{
			Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)[Icon(Leading.GetValue(), 16.0f)];
		}
		Content->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[SNew(STextBlock).Text(Text).Font(Studio::Text(EText::Button)).ColorAndOpacity(FSlateColor::UseForeground()).Visibility(TextVisibility)];
		Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)[Icon(EFloor101Icon::ChevronDown, 16.0f)];
		return Content;
	}

	TSharedRef<SWidget> Field(TAttribute<FText> Current, TFunction<void(const FString&)> Apply, bool bNumeric, TAttribute<FText> Hint, TAttribute<FText> Tip)
	{
		return SNew(SBox).HeightOverride(Studio::Size::Field)
			[
				SNew(SEditableTextBox).Style(&Studio::TextBox()).Font(Studio::Text(bNumeric ? EText::Numeric : EText::Body))
				.SelectAllTextWhenFocused(true).Text(Current).HintText(Hint).ToolTip(Tooltip(Tip))
				.OnTextCommitted_Lambda([Current, Apply](const FText& Text, ETextCommit::Type Commit)
				{
					// Unchanged text is not re-applied: the shown value is rounded to 1 mm and could move a diagonal wall.
					if (Commit != ETextCommit::OnCleared && !Text.EqualTo(Current.Get()) && Apply)
					{
						Apply(Text.ToString());
					}
					if (Commit == ETextCommit::OnEnter)
					{
						FSlateApplication::Get().SetAllUserFocusToGameViewport();
					}
				})
			];
	}
}
