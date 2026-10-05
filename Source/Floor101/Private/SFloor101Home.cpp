#include "SFloor101Overlay.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101FileActions.h"
#include "Floor101GameViewportClient.h"
#include "Floor101Style.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TArray<FString> SFloor101Overlay::RecentProjects() const
{
	FString Capture;
	if (FParse::Value(FCommandLine::Get(), TEXT("F101UICapture="), Capture))
	{
		if (const auto* D = Document.Get(); D && !D->GetProjectPath().IsEmpty()) { return {D->GetProjectPath()}; }
		return {}; // screenshots never expose or change the user's recent files
	}
	return FFloor101FileActions::GetRecentProjects();
}

void SFloor101Overlay::RefreshModalInput()
{
	if (auto* Client = ViewportClient.Get())
	{
		const FSimpleDelegate Close = bUnsavedPromptOpen ? FSimpleDelegate::CreateSP(this, &SFloor101Overlay::ResolveUnsavedPrompt, EAppReturnType::Cancel)
			: bOptionsOpen ? FSimpleDelegate::CreateSP(this, &SFloor101Overlay::CloseOptions)
			: FSimpleDelegate::CreateSP(this, &SFloor101Overlay::CloseHome);
		Client->SetInterfaceOpen(bUnsavedPromptOpen || bOptionsOpen || bHomeOpen, Close);
	}
	if (bUnsavedPromptOpen || bOptionsOpen || bHomeOpen) { FSlateApplication::Get().SetKeyboardFocus(AsShared()); }
	else { FSlateApplication::Get().SetAllUserFocusToGameViewport(); }
}

void SFloor101Overlay::ShowStartup()
{
	FString Capture;
	if (!FParse::Value(FCommandLine::Get(), TEXT("F101UICapture="), Capture) &&
		!FParse::Param(FCommandLine::Get(), TEXT("F101RenderCheck")) && !FParse::Param(FCommandLine::Get(), TEXT("F101HDRCheck")) &&
		!FParse::Param(FCommandLine::Get(), TEXT("F101DisplayCheck")) && !FParse::Param(FCommandLine::Get(), TEXT("F101PackageCheck"))) { OpenHome(); }
}

void SFloor101Overlay::OpenHome()
{
	bHomeOpen = true;
	RefreshModalInput();
}

void SFloor101Overlay::CloseHome()
{
	bHomeOpen = false;
	bHasEnteredWorkspace = true;
	RefreshModalInput();
}

void SFloor101Overlay::EnterWorkspace(bool bFrame)
{
	if (auto* Client = ViewportClient.Get())
	{
		Client->SetPhotoMode(false);
		Client->SetNavigationMode(false);
		if (bFrame) { Client->FrameProject(); }
	}
	CloseHome();
}

TSharedRef<SWidget> SFloor101Overlay::MakeHomePanel()
{
	using namespace Floor101::Studio;
	TSharedRef<SVerticalBox> Recent = SNew(SVerticalBox);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		auto Path = [this, Index]()
		{
			const auto Paths = RecentProjects();
			return Paths.IsValidIndex(Index) ? Paths[Index] : FString();
		};
		Recent->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(SButton).ButtonStyle(&Floor101::Studio::Button()).ContentPadding(FMargin(16.0f, 10.0f)).IsFocusable(false)
			.Visibility_Lambda([Path]() { return Path().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.IsEnabled_Lambda([Path]() { return IFileManager::Get().FileExists(*Path()); })
			.ToolTipText_Lambda([Path]() { return FText::FromString(Path()); })
			.OnClicked_Lambda([this, Path]()
			{
				if (auto* D = Document.Get()) { FFloor101FileActions::OpenPath(*D, Path(), FSimpleDelegate::CreateSP(this, &SFloor101Overlay::EnterWorkspace, true)); }
				return FReply::Handled();
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Font(Floor101::Studio::Font(15, true)).ColorAndOpacity(Floor101::Studio::Color(TEXT("EEF0EC"))).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text_Lambda([Path]() { return FText::FromString(FPaths::GetBaseFilename(Path())); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
				[SNew(STextBlock).Font(Floor101::Studio::Font(11)).ColorAndOpacity(Floor101::Studio::Color(TEXT("B3BAB9"))).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text_Lambda([Path]() { return FText::FromString(FPaths::GetPath(Path()) + (IFileManager::Get().FileExists(*Path()) ? TEXT("") : TEXT(" · Archivo no disponible"))); })]
			]
		];
	}
	auto Action = [](const FText& Label, FOnClicked Click, TAttribute<bool> Enabled = true)
	{
		return SNew(SButton).ButtonStyle(&Floor101::Studio::Button()).IsFocusable(false).ContentPadding(FMargin(24.0f, 16.0f)).IsEnabled(Enabled).OnClicked(Click)
			[SNew(STextBlock).Font(Floor101::Studio::Font(17, true)).ColorAndOpacity(Floor101::Studio::Color(TEXT("EEF0EC"))).Text(Label)];
	};
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Floor101::Studio::Color(TEXT("1E2328")))
		.Padding(24.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		.Visibility_Lambda([this]() { return bHomeOpen ? EVisibility::Visible : EVisibility::Collapsed; })
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[
			SNew(SBox).WidthOverride_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Min(1000.0, ViewSize.X - 48.0))); })
			.MaxDesiredHeight_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Max(120.0, ViewSize.Y - 48.0))); })
			[SNew(SScrollBox) + SScrollBox::Slot()
			[SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Floor101::Studio::Font(15, true)).ColorAndOpacity(Floor101::Studio::Color(TEXT("D6AD78"))).Text(FText::FromString(TEXT("101st FLOOR · ESTUDIO ARQUITECTÓNICO")))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 8.0f)
				[SNew(STextBlock).Font(Floor101::Studio::Font(34)).ColorAndOpacity(Floor101::Studio::Color(TEXT("EEF0EC"))).AutoWrapText(true).Text(FText::FromString(TEXT("Tu espacio de diseño.")))]
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Font(Floor101::Studio::Font(14)).ColorAndOpacity(Floor101::Studio::Color(TEXT("B3BAB9"))).AutoWrapText(true).Text(FText::FromString(TEXT("Modela, recorre y presenta tu proyecto.")))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 24.0f)
				[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12.0, 12.0))
					+ SWrapBox::Slot()[Action(FText::FromString(TEXT("Nuevo proyecto")), FOnClicked::CreateLambda([this]()
					{
						if (auto* D = Document.Get())
						{
							FFloor101FileActions::RequestReplace(*D, FSimpleDelegate::CreateSP(this, &SFloor101Overlay::EnterNewProject));
						}
						return FReply::Handled();
					}))]
					+ SWrapBox::Slot()[Action(FText::FromString(TEXT("Continuar")), FOnClicked::CreateLambda([this]()
					{
						if (auto* D = Document.Get())
						{
							if (bHasEnteredWorkspace || !D->GetProjectPath().IsEmpty()) { EnterWorkspace(false); }
							else
							{
								bool bFound = false;
								for (const FString& Path : RecentProjects())
								{
									if (IFileManager::Get().FileExists(*Path)) { bFound = true; FFloor101FileActions::OpenPath(*D, Path, FSimpleDelegate::CreateSP(this, &SFloor101Overlay::EnterWorkspace, true)); break; }
								}
								if (!bFound && D->HasUnsavedChanges()) { EnterWorkspace(false); }
							}
						}
						return FReply::Handled();
					}), TAttribute<bool>::CreateLambda([this]()
					{
						if (auto* D = Document.Get(); D && (bHasEnteredWorkspace || !D->GetProjectPath().IsEmpty() || D->HasUnsavedChanges())) { return true; }
						for (const FString& Path : RecentProjects()) { if (IFileManager::Get().FileExists(*Path)) { return true; } }
						return false;
					}))]
					+ SWrapBox::Slot()[Action(FText::FromString(TEXT("Abrir proyecto…")), FOnClicked::CreateLambda([this]()
					{
						if (auto* D = Document.Get()) { FFloor101FileActions::Open(*D, FSimpleDelegate::CreateSP(this, &SFloor101Overlay::EnterWorkspace, true)); }
						return FReply::Handled();
					}))]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[SNew(STextBlock).Font(Floor101::Studio::Font(18, true)).ColorAndOpacity(Floor101::Studio::Color(TEXT("EEF0EC"))).Text(FText::FromString(TEXT("Proyectos recientes")))]
				+ SVerticalBox::Slot().AutoHeight()[Recent]
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Floor101::Studio::Font(12)).ColorAndOpacity(Floor101::Studio::Color(TEXT("B3BAB9")))
					.Visibility_Lambda([this]() { return RecentProjects().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
					.AutoWrapText(true).Text(FText::FromString(TEXT("Tus proyectos aparecerán aquí después de abrirlos o guardarlos.")))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 24.0f)
				[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12.0, 12.0))
					+ SWrapBox::Slot()[SNew(SButton).ButtonStyle(&Floor101::Studio::Button()).ContentPadding(10.0f).IsFocusable(false).OnClicked_Lambda([this]() { OpenOptions(); return FReply::Handled(); })
						[SNew(STextBlock).Font(Floor101::Studio::Font(12)).ColorAndOpacity(Floor101::Studio::Color(TEXT("EEF0EC"))).Text(FText::FromString(TEXT("Opciones")))]]
					+ SWrapBox::Slot()[MakeExitButton()]
				]
			]]
		];
}

void SFloor101Overlay::EnterNewProject()
{
	if (auto* D = Document.Get(); D && D->NewProject()) { EnterWorkspace(true); }
}
