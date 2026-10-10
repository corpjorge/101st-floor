#include "SFloor101Overlay.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101FileActions.h"
#include "Floor101Style.h"
#include "Floor101Ui.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SFloor101Overlay::ShowUnsavedPrompt(bool bExit, FSimpleDelegate Proceed)
{
	if (bUnsavedPromptOpen) { return; } // repeated clicks cannot replace the pending action
	bUnsavedExit = bExit;
	bUnsavedSaveFailed = false;
	PendingUnsavedAction = Proceed;
	bUnsavedPromptOpen = true;
	RefreshModalInput();
}

void SFloor101Overlay::ResolveUnsavedPrompt(EAppReturnType::Type Choice)
{
	if (!bUnsavedPromptOpen || bResolvingUnsaved) { return; }
	bResolvingUnsaved = true;
	auto* D = Document.Get();
	const bool bProceed = Choice != EAppReturnType::Cancel && D && FFloor101FileActions::PrepareExit(*D,
		[Choice]() { return Choice; }, [D]() { FFloor101FileActions::Save(*D); });
	bResolvingUnsaved = false;
	if (Choice == EAppReturnType::Yes && !bProceed)
	{
		bUnsavedSaveFailed = true;
		RefreshModalInput(); // save cancelled/failed: keep every choice available
		return;
	}
	const FSimpleDelegate Proceed = PendingUnsavedAction;
	PendingUnsavedAction.Unbind();
	bUnsavedPromptOpen = false;
	RefreshModalInput();
	if (bProceed) { Proceed.ExecuteIfBound(); }
}

TSharedRef<SWidget> SFloor101Overlay::MakeUnsavedPanel()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	const Studio::FPalette& P = Studio::Palette();
	auto Choice = [this](Studio::EButton Kind, const TCHAR* Label, EAppReturnType::Type Result, const FText& Keys)
	{
		return Ui::Button(Kind, FText::FromString(Label), [this, Result]() { ResolveUnsavedPrompt(Result); }, {}, FText::GetEmpty(), Keys,
			TAttribute<bool>::CreateLambda([this]() { return !bResolvingUnsaved; }), 36.0f);
	};
	const TSharedRef<SWidget> Dialog = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(24.0f, 24.0f, 24.0f, 8.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
			[
				SNew(SBox).WidthOverride(40.0f).HeightOverride(40.0f)
				[SNew(SBorder).BorderImage(Studio::Box(P.BrassSel, 10.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center)[Ui::Icon(EFloor101Icon::Warn, 20.0f, FSlateColor(P.Brass))]]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Font(Studio::Text(Studio::EText::Title)).ColorAndOpacity(P.Text1).AutoWrapText(true).Text(FText::FromString(TEXT("Hay cambios sin guardar")))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(Studio::Text(Studio::EText::Body)).ColorAndOpacity(P.Text2).AutoWrapText(true)
					.Text_Lambda([this]() { return FText::FromString(bUnsavedExit
						? TEXT("Guarda tu trabajo antes de salir, descarta los cambios o cancela para seguir trabajando.")
						: TEXT("Guarda tu trabajo antes de cambiar de proyecto, descarta los cambios o cancela para seguir trabajando.")); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
				[
					// A save that failed or was cancelled keeps every choice and says why.
					SNew(SHorizontalBox).Visibility_Lambda([this]() { return bUnsavedSaveFailed ? EVisibility::Visible : EVisibility::Collapsed; })
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 1.0f, 8.0f, 0.0f)[Ui::Icon(EFloor101Icon::Warn, 16.0f, FSlateColor(P.Danger))]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(STextBlock).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Danger).AutoWrapText(true)
						.Text_Lambda([this]() { const auto* D = Document.Get(); return D ? D->GetStatusMessage() : FText::GetEmpty(); })
					]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(24.0f, 16.0f, 24.0f, 22.0f))
		[
			// The destructive choice stands apart from the safe pair.
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Choice(Studio::EButton::Danger, TEXT("Descartar"), EAppReturnType::No, FText::GetEmpty())]
			+ SHorizontalBox::Slot().FillWidth(1.0f).MinWidth(8.0f)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)[Choice(Studio::EButton::Ghost, TEXT("Cancelar"), EAppReturnType::Cancel, INVTEXT("Esc"))]
			+ SHorizontalBox::Slot().AutoWidth()[Choice(Studio::EButton::Primary, TEXT("Guardar"), EAppReturnType::Yes, FText::GetEmpty())]
		];
	return SNew(SBorder).BorderImage(Studio::White()).BorderBackgroundColor(P.Scrim).Padding(20.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		.Visibility_Lambda([this]() { return bUnsavedPromptOpen ? EVisibility::Visible : EVisibility::Collapsed; })
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[
			SNew(SBox).WidthOverride_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Clamp(ViewSize.X - 40.0, 120.0, 500.0))); })
			.MaxDesiredHeight_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Max(120.0, ViewSize.Y - 40.0))); })
			[Ui::Island(SNew(SScrollBox).ScrollBarStyle(&Studio::ScrollBar()) + SScrollBox::Slot()[Dialog], FMargin(0.0f), EVisibility::SelfHitTestInvisible, Studio::Size::RadiusDialog)]
		];
}
