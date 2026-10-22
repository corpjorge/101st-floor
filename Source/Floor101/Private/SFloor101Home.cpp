#include "SFloor101Overlay.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101FileActions.h"
#include "Floor101GameViewportClient.h"
#include "Floor101Style.h"
#include "Floor101Ui.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "SFloor101Icon.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
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
		if (bFrame) { Client->FrameProject(true); }
	}
	CloseHome();
}

namespace
{
	// A draughtsman's sheet behind Inicio: a 24-unit grid with a heavier line every 120.
	class SFloor101DraftingGrid final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SFloor101DraftingGrid) {} SLATE_END_ARGS()
		void Construct(const FArguments&) {}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer,
			const FWidgetStyle& Style, bool) const override
		{
			const FVector2f Size = FVector2f(Geometry.GetLocalSize());
			const FLinearColor Minor(1.0f, 1.0f, 1.0f, 0.018f);
			const FLinearColor Major(1.0f, 1.0f, 1.0f, 0.042f);
			for (float X = 0.0f; X <= Size.X; X += 24.0f)
			{
				const bool bMajor = FMath::IsNearlyZero(FMath::Fmod(X, 120.0f));
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), {FVector2f(X, 0.0f), FVector2f(X, Size.Y)}, ESlateDrawEffect::None,
					(bMajor ? Major : Minor) * Style.GetColorAndOpacityTint(), false, 1.0f);
			}
			for (float Y = 0.0f; Y <= Size.Y; Y += 24.0f)
			{
				const bool bMajor = FMath::IsNearlyZero(FMath::Fmod(Y, 120.0f));
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), {FVector2f(0.0f, Y), FVector2f(Size.X, Y)}, ESlateDrawEffect::None,
					(bMajor ? Major : Minor) * Style.GetColorAndOpacityTint(), false, 1.0f);
			}
			return Layer;
		}
	};

	// 101 floors of 3 m drawn as hairline plates, the last one lit in brass at +300,00: the 101st floor.
	class SFloor101Tower final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SFloor101Tower) {} SLATE_END_ARGS()
		void Construct(const FArguments&) {}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(300.0, 400.0); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer,
			const FWidgetStyle& Style, bool) const override
		{
			const auto& P = Floor101::Studio::Palette();
			const FLinearColor Tint = Style.GetColorAndOpacityTint();
			const FVector2f Size = FVector2f(Geometry.GetLocalSize());
			const float Cx = Size.X * 0.36f;
			const float HalfWidth = FMath::Min(74.0f, Size.X * 0.25f);
			const float HalfDepth = HalfWidth * 0.4f;
			const float Base = Size.Y - HalfDepth - 24.0f;
			const float Top = HalfDepth + 24.0f;
			const int32 Floors = 101;
			const float Step = (Base - Top) / (Floors - 1);
			auto Plate = [&](float Y) -> TArray<FVector2f>
			{
				return {{Cx - HalfWidth, Y}, {Cx, Y - HalfDepth}, {Cx + HalfWidth, Y}, {Cx, Y + HalfDepth}, {Cx - HalfWidth, Y}};
			};
			for (int32 Floor = 0; Floor < Floors - 1; ++Floor)
			{
				const FLinearColor Ink(0.925f, 0.933f, 0.918f, Floor % 10 == 0 ? 0.13f : 0.055f);
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Plate(Base - Floor * Step), ESlateDrawEffect::None, Ink * Tint, true, 1.0f);
			}
			const FLinearColor Edge(0.925f, 0.933f, 0.918f, 0.16f);
			for (const FVector2f Corner : {FVector2f(Cx - HalfWidth, 0.0f), FVector2f(Cx, HalfDepth), FVector2f(Cx + HalfWidth, 0.0f)})
			{
				FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), {FVector2f(Corner.X, Base + Corner.Y), FVector2f(Corner.X, Top + Corner.Y)},
					ESlateDrawEffect::None, Edge * Tint, true, 1.0f);
			}
			// The top floor, filled and outlined in brass.
			if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
			{
				const FColor Fill = (FLinearColor(P.Brass.R, P.Brass.G, P.Brass.B, 0.16f) * Tint).ToFColorSRGB();
				const FSlateRenderTransform& Transform = Geometry.GetAccumulatedRenderTransform();
				TArray<FSlateVertex> Vertices;
				for (const FVector2f& Point : Plate(Top))
				{
					Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Point, FVector2f(0.5f), Fill));
				}
				TArray<SlateIndex> Indices = {0, 1, 2, 0, 2, 3};
				FSlateDrawElement::MakeCustomVerts(Out, Layer, FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Floor101::Studio::White()),
					Vertices, Indices, nullptr, 0, 0);
			}
			FSlateDrawElement::MakeLines(Out, Layer + 1, Geometry.ToPaintGeometry(), Plate(Top), ESlateDrawEffect::None, P.Brass * Tint, true, 1.5f);
			// Elevations, as on a section drawing.
			const FSlateFontInfo Font = Floor101::Studio::Text(Floor101::Studio::EText::NumericSmall);
			auto Label = [&](float Y, const FString& Text, const FLinearColor& Color)
			{
				const float X = Cx + HalfWidth + 10.0f;
				FSlateDrawElement::MakeLines(Out, Layer + 1, Geometry.ToPaintGeometry(), {FVector2f(X, Y), FVector2f(X + 12.0f, Y)}, ESlateDrawEffect::None,
					FLinearColor(P.Text3.R, P.Text3.G, P.Text3.B, 0.8f) * Tint, true, 1.0f);
				FSlateDrawElement::MakeText(Out, Layer + 1, Geometry.ToPaintGeometry(FVector2f(200.0f, 18.0f), FSlateLayoutTransform(FVector2f(X + 18.0f, Y - 9.0f))),
					Text, Font, ESlateDrawEffect::None, Color * Tint);
			};
			Label(Top, TEXT("+300,00 · planta 101"), P.Brass);
			Label(Base - 50.0f * Step, TEXT("+150,00"), P.Text3);
			Label(Base, TEXT("±0,00"), P.Text3);
			return Layer + 1;
		}
	};
}

TSharedRef<SWidget> SFloor101Overlay::MakeHomePanel()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	const Studio::FPalette& P = Studio::Palette();
	auto Wide = [this]() { return ViewSize.X >= 1100.0; };

	// What Continuar returns to: the document in memory, else the first recent file that still exists.
	auto Continue = [this]()
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
	};
	auto CanContinue = [this]()
	{
		if (auto* D = Document.Get(); D && (bHasEnteredWorkspace || !D->GetProjectPath().IsEmpty() || D->HasUnsavedChanges())) { return true; }
		for (const FString& Path : RecentProjects()) { if (IFileManager::Get().FileExists(*Path)) { return true; } }
		return false;
	};
	// The file Continuar would show, for its card: empty for an unsaved new project.
	auto ContinuePath = [this]() -> FString
	{
		const auto* D = Document.Get();
		if (!D) { return FString(); }
		if (bHasEnteredWorkspace || !D->GetProjectPath().IsEmpty()) { return D->GetProjectPath(); }
		for (const FString& Path : RecentProjects()) { if (IFileManager::Get().FileExists(*Path)) { return Path; } }
		return FString();
	};

	auto LeftColumn = [&, this]()
	{
		FSlateFontInfo Lede = Studio::Text(Studio::EText::Body);
		Lede.Size = 12.75f;
		return SNew(SBox).MaxDesiredWidth(620.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Ui::Icon(EFloor101Icon::Mark, 40.0f, FSlateColor(P.Text1))]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.0f, 0.0f, 0.0f, 0.0f)
					[Ui::Wordmark(4.0f / 3.0f)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
				[SNew(STextBlock).Text(INVTEXT("ESTUDIO ARQUITECTÓNICO")).Font(Studio::Text(Studio::EText::Overline)).ColorAndOpacity(P.Brass)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 40.0f, 0.0f, 0.0f)
				[SNew(STextBlock).Text(INVTEXT("Tu espacio de diseño.")).Font(Studio::Text(Studio::EText::Display)).ColorAndOpacity(P.Text1).AutoWrapText(true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
				[SNew(STextBlock).Text(INVTEXT("Modela, recorre y presenta tu proyecto.")).Font(Lede).ColorAndOpacity(P.Text2).AutoWrapText(true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 32.0f, 0.0f, 0.0f)
				[
					SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12.0, 12.0))
					+ SWrapBox::Slot()
					[Ui::Button(Studio::EButton::Primary, INVTEXT("Nuevo proyecto"), [this]()
						{
							if (auto* D = Document.Get()) { FFloor101FileActions::RequestReplace(*D, FSimpleDelegate::CreateSP(this, &SFloor101Overlay::EnterNewProject)); }
						}, EFloor101Icon::Plus, FText::GetEmpty(), FText::GetEmpty(), true, 40.0f)]
					+ SWrapBox::Slot()
					[Ui::Button(Studio::EButton::Secondary, INVTEXT("Abrir proyecto…"), [this]()
						{
							if (auto* D = Document.Get()) { FFloor101FileActions::Open(*D, FSimpleDelegate::CreateSP(this, &SFloor101Overlay::EnterWorkspace, true)); }
						}, EFloor101Icon::Folder, FText::GetEmpty(), INVTEXT("Ctrl+O"), true, 40.0f)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 24.0f, 0.0f, 0.0f)
				[
					SNew(SButton).ButtonStyle(&Studio::ButtonStyle(Studio::EButton::Card)).IsFocusable(false).ContentPadding(FMargin(14.0f, 12.0f))
					.IsEnabled_Lambda(CanContinue)
					.ToolTip(Ui::Tooltip(TAttribute<FText>::CreateLambda([ContinuePath]() { return FText::FromString(ContinuePath()); })))
					.OnClicked_Lambda([Continue]() { Continue(); return FReply::Handled(); })
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(64.0f).HeightOverride(48.0f)
							[SNew(SBorder).BorderImage(Studio::Box(P.Paper, 6.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center)[Ui::Icon(EFloor101Icon::Plan, 24.0f, FSlateColor(P.OnPaper))]]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(16.0f, 0.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Studio::Text(Studio::EText::BodyStrong)).ColorAndOpacity(FSlateColor::UseForeground()).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								.Text_Lambda([this, ContinuePath, CanContinue]()
								{
									if (!CanContinue()) { return FText::FromString(TEXT("Nada que continuar todavía")); }
									const FString Path = ContinuePath();
									return FText::FromString(TEXT("Continuar con ") + (Path.IsEmpty() ? FString(TEXT("el proyecto nuevo")) : FPaths::GetBaseFilename(Path)));
								})
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Text3).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								.Text_Lambda([ContinuePath, CanContinue]()
								{
									if (!CanContinue()) { return FText::FromString(TEXT("Crea un proyecto nuevo o abre uno existente.")); }
									const FString Path = ContinuePath();
									return FText::FromString(Path.IsEmpty() ? FString(TEXT("Sin guardar")) : FPaths::GetPath(Path));
								})
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[SNew(STextBlock).Text(INVTEXT("Continuar")).Font(Studio::Text(Studio::EText::Button)).ColorAndOpacity(FSlateColor::UseForeground())]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)[Ui::Icon(EFloor101Icon::ChevronRight, 16.0f)]
						]
					]
				]
				// A new version (ADR-0039), while there is one.
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)[MakeUpdateNotice()]
			];
	};

	auto RightColumn = [&, this]()
	{
		TSharedRef<SVerticalBox> Recent = SNew(SVerticalBox);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			auto Path = [this, Index]()
			{
				const auto Paths = RecentProjects();
				return Paths.IsValidIndex(Index) ? Paths[Index] : FString();
			};
			auto Missing = [Path]() { return !IFileManager::Get().FileExists(*Path()); };
			Recent->AddSlot().AutoHeight().Padding(0.0f, Index > 0 ? 2.0f : 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton).ButtonStyle(&Studio::ButtonStyle(Studio::EButton::Row)).ContentPadding(FMargin(10.0f, 10.0f, 14.0f, 10.0f)).IsFocusable(false)
				.Visibility_Lambda([Path]() { return Path().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				.IsEnabled_Lambda([Path]() { return IFileManager::Get().FileExists(*Path()); })
				.ToolTipText_Lambda([Path]() { return FText::FromString(Path()); })
				.OnClicked_Lambda([this, Path]()
				{
					if (auto* D = Document.Get()) { FFloor101FileActions::OpenPath(*D, Path(), FSimpleDelegate::CreateSP(this, &SFloor101Overlay::EnterWorkspace, true)); }
					return FReply::Handled();
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(44.0f).HeightOverride(44.0f)
						[SNew(SBorder).BorderImage(Studio::Box(P.Raised, 8.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center)[Ui::Icon(EFloor101Icon::Plan, 20.0f)]]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(14.0f, 0.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Studio::Text(Studio::EText::BodyStrong)).ColorAndOpacity(FSlateColor::UseForeground()).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							.Text_Lambda([Path]() { return FText::FromString(FPaths::GetBaseFilename(Path())); })
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Text3).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							.Text_Lambda([Path]() { return FText::FromString(FPaths::GetPath(Path())); })
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBorder).BorderImage(Studio::Box(FLinearColor(P.Warn.R, P.Warn.G, P.Warn.B, 0.12f), 11.0f)).Padding(FMargin(8.0f, 2.0f))
						.Visibility_Lambda([Missing]() { return Missing() ? EVisibility::Visible : EVisibility::Collapsed; })
						[SNew(STextBlock).Text(INVTEXT("No disponible")).Font(Studio::Text(Studio::EText::Keycap)).ColorAndOpacity(P.Warn)]
					]
				]
			];
		}
		Recent->AddSlot().AutoHeight().Padding(10.0f, 8.0f)
		[
			SNew(STextBlock).Font(Studio::Text(Studio::EText::Label)).ColorAndOpacity(P.Text3).AutoWrapText(true)
			.Visibility_Lambda([this]() { return RecentProjects().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
			.Text(INVTEXT("Tus proyectos aparecerán aquí después de abrirlos o guardarlos."))
		];
		return SNew(SBox).MaxDesiredWidth(680.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 0.0f, 0.0f, 12.0f)
				[SNew(STextBlock).Text(INVTEXT("PROYECTOS RECIENTES")).Font(Studio::Text(Studio::EText::Overline)).ColorAndOpacity(P.Text3)]
				+ SVerticalBox::Slot().AutoHeight()[Ui::Island(Recent, FMargin(6.0f))]
			];
	};

	return SNew(SBorder).BorderImage(Studio::White()).BorderBackgroundColor(P.Canvas).Padding(0.0f)
		.Visibility_Lambda([this]() { return bHomeOpen ? EVisibility::Visible : EVisibility::Collapsed; })
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SFloor101DraftingGrid).Visibility(EVisibility::HitTestInvisible)]
			+ SOverlay::Slot()
			.Padding(TAttribute<FMargin>::CreateLambda([this]() { return FMargin(static_cast<float>(FMath::Clamp(ViewSize.X * 0.07, 24.0, 144.0)), 32.0f); }))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SScrollBox).ScrollBarStyle(&Studio::ScrollBar())
					+ SScrollBox::Slot()
					[
						SNew(SBox).MinDesiredHeight_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Max(0.0, ViewSize.Y - 140.0))); }).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							// Wide: brand and actions, the tower, the recent projects.
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox).Visibility_Lambda([Wide]() { return Wide() ? EVisibility::Visible : EVisibility::Collapsed; })
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[LeftColumn()]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill).Padding(24.0f, 0.0f)
								[SNew(SBox).WidthOverride(300.0f).HeightOverride_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Clamp(ViewSize.Y - 200.0, 360.0, 820.0))); })[SNew(SFloor101Tower)]]
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[RightColumn()]
							]
							// Narrow: one column.
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SVerticalBox).Visibility_Lambda([Wide]() { return Wide() ? EVisibility::Collapsed : EVisibility::Visible; })
								+ SVerticalBox::Slot().AutoHeight()[LeftColumn()]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 32.0f, 0.0f, 0.0f)[RightColumn()]
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[Ui::Button(Studio::EButton::Ghost, INVTEXT("Opciones"), [this]() { OpenOptions(); }, EFloor101Icon::Options, FText::GetEmpty(), FText::GetEmpty(), true, 40.0f)]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					+ SHorizontalBox::Slot().AutoWidth()[MakeExitButton()]
				]
			]
		];
}

void SFloor101Overlay::EnterNewProject()
{
	if (auto* D = Document.Get(); D && D->NewProject()) { EnterWorkspace(true); }
}
