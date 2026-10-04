#include "SFloor101Overlay.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101FileActions.h"
#include "Floor101Style.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
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
	auto ChoiceButton = [this](const TCHAR* Label, EAppReturnType::Type Choice)
	{
		return SNew(SButton).ButtonStyle(&Floor101::Studio::Button()).ContentPadding(FMargin(18.0f, 12.0f))
			.IsEnabled_Lambda([this]() { return !bResolvingUnsaved; })
			.OnClicked_Lambda([this, Choice]() { ResolveUnsavedPrompt(Choice); return FReply::Handled(); })
			[SNew(STextBlock).Font(Floor101::Studio::Font(14, true)).ColorAndOpacity(Floor101::Studio::Color(TEXT("EEF0EC")))
				.Text(FText::FromString(Label))];
	};
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f)).Padding(20.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		.Visibility_Lambda([this]() { return bUnsavedPromptOpen ? EVisibility::Visible : EVisibility::Collapsed; })
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[
			SNew(SBox).WidthOverride_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Clamp(ViewSize.X - 40.0, 120.0, 620.0))); })
			.MaxDesiredHeight_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Max(120.0, ViewSize.Y - 40.0))); })
			[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Floor101::Studio::Color(TEXT("252B30"))).Padding(24.0f)
			[SNew(SScrollBox) + SScrollBox::Slot()
			[SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Font(Floor101::Studio::Font(23, true)).ColorAndOpacity(Floor101::Studio::Color(TEXT("D6AD78")))
					.AutoWrapText(true).Text(FText::FromString(TEXT("Hay cambios sin guardar")))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f)
				[SNew(STextBlock).Font(Floor101::Studio::Font(15)).ColorAndOpacity(Floor101::Studio::Color(TEXT("EEF0EC"))).AutoWrapText(true)
					.Text_Lambda([this]() { return FText::FromString(bUnsavedExit
						? TEXT("Guarda tu trabajo antes de salir, descarta los cambios o cancela para seguir trabajando.")
						: TEXT("Guarda tu trabajo antes de cambiar de proyecto, descarta los cambios o cancela para seguir trabajando.")); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 16.0f)
				[SNew(STextBlock).Font(Floor101::Studio::Font(12)).ColorAndOpacity(Floor101::Studio::Color(TEXT("B3BAB9"))).AutoWrapText(true)
					.Visibility_Lambda([this]() { return bUnsavedSaveFailed ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text_Lambda([this]() { const auto* D = Document.Get(); return D ? D->GetStatusMessage() : FText::GetEmpty(); })]
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.0, 10.0))
					+ SWrapBox::Slot()[ChoiceButton(TEXT("Guardar"), EAppReturnType::Yes)]
					+ SWrapBox::Slot()[ChoiceButton(TEXT("Descartar"), EAppReturnType::No)]
					+ SWrapBox::Slot()[ChoiceButton(TEXT("Cancelar · Esc"), EAppReturnType::Cancel)]
				]
			]]]
		];
}
