// The Diseñar workspace of the overlay (Cota Cero): top bar, tool dock, view bar, levels, menus, hint pill and status.
// Presentation only: every control runs the same call it ran before the redesign.
#include "SFloor101Overlay.h"

#include "F101/Cad/Document.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101EditingPawn.h"
#include "Floor101FileActions.h"
#include "Floor101GameViewportClient.h"
#include "Floor101Names.h"
#include "Floor101Preferences.h"
#include "Floor101Style.h"
#include "Floor101Ui.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Floor101Workspace"

namespace
{
	using f101::cad::EditController;
	using f101::cad::EditMode;

	// "0,5", "5", "12,5": a speed in m/s without trailing zeros.
	FString WorkspaceSpeed(double MetresPerSecond)
	{
		FString Text = Floor101::Studio::Decimal(MetresPerSecond, 2);
		while (Text.Contains(TEXT(",")) && (Text.EndsWith(TEXT("0")) || Text.EndsWith(TEXT(","))))
		{
			Text.LeftChopInline(1);
		}
		return Text;
	}

	// The words of a hint that name keys (drawn as keycaps) or mouse gestures (written in a quieter colour).
	bool WorkspaceIsKey(const FString& Word)
	{
		static const TSet<FString> Keys = {TEXT("V"), TEXT("B"), TEXT("P"), TEXT("O"), TEXT("M"), TEXT("G"), TEXT("C"), TEXT("I"), TEXT("H"),
			TEXT("X"), TEXT("N"), TEXT("R"), TEXT("F"), TEXT("T"), TEXT("L"), TEXT("Z"), TEXT("Y"), TEXT("Esc"), TEXT("Enter"), TEXT("Espacio"),
			TEXT("Supr"), TEXT("Retroceso"), TEXT("Tab"), TEXT("Shift"), TEXT("Ctrl"), TEXT("WASD"), TEXT("Q/E"), TEXT("RePág/AvPág"),
			TEXT("RePág"), TEXT("AvPág")};
		return Keys.Contains(Word);
	}

	bool WorkspaceIsGesture(const FString& Word)
	{
		static const TSet<FString> Gestures = {TEXT("clic"), TEXT("clic derecho"), TEXT("derecho"), TEXT("central"), TEXT("rueda"),
			TEXT("doble clic"), TEXT("derecho arrastrando")};
		return Gestures.Contains(Word);
	}

	TSharedRef<SWidget> WorkspaceText(const FString& Text, Floor101::Studio::EText Style, const FLinearColor& Color)
	{
		return SNew(STextBlock).Text(FText::FromString(Text)).Font(Floor101::Studio::Text(Style)).ColorAndOpacity(Color);
	}

	// "Esc o clic derecho", "Shift + derecho", "Ctrl+Z": keycaps and gestures, or false when the words are not keys.
	bool WorkspaceKeys(const FString& Keys, TSharedRef<SHorizontalBox>& Out)
	{
		const auto& P = Floor101::Studio::Palette();
		TArray<FString> Alternatives;
		Keys.ParseIntoArray(Alternatives, TEXT(" o "));
		if (Alternatives.IsEmpty()) { return false; }
		TArray<TSharedRef<SWidget>> Parts;
		for (int32 Index = 0; Index < Alternatives.Num(); ++Index)
		{
			if (Index > 0) { Parts.Add(WorkspaceText(TEXT("o"), Floor101::Studio::EText::Help, P.Text3)); }
			TArray<FString> Tokens;
			Alternatives[Index].ParseIntoArray(Tokens, TEXT("+"));
			for (int32 Token = 0; Token < Tokens.Num(); ++Token)
			{
				const FString Word = Tokens[Token].TrimStartAndEnd();
				if (Token > 0) { Parts.Add(WorkspaceText(TEXT("+"), Floor101::Studio::EText::Help, P.Text3)); }
				if (WorkspaceIsKey(Word)) { Parts.Add(Floor101::Ui::Keycap(FText::FromString(Word))); }
				else if (WorkspaceIsGesture(Word)) { Parts.Add(WorkspaceText(Word, Floor101::Studio::EText::Help, P.Text3)); }
				else { return false; }
			}
		}
		for (const TSharedRef<SWidget>& Part : Parts)
		{
			Out->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(Out->NumSlots() > 0 ? 5.0f : 0.0f, 0.0f, 0.0f, 0.0f)[Part];
		}
		return true;
	}
}

bool SFloor101Overlay::IsInspectorDocked() const
{
	return ViewSize.X >= Floor101::Studio::Breakpoint::InspectorDocked;
}

TSharedRef<SWidget> SFloor101Overlay::MakeToolbar()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	using Studio::EText;
	using Studio::EButton;
	const Studio::FPalette& P = Studio::Palette();
	auto Wide = [this]() { return ViewSize.X >= Studio::Breakpoint::BarLabels ? EVisibility::Visible : EVisibility::Collapsed; };
	auto Narrow = [this]() { return ViewSize.X < Studio::Breakpoint::BarLabels ? EVisibility::Visible : EVisibility::Collapsed; };

	const TSharedRef<SWidget> Wordmark = SNew(SHorizontalBox).Visibility_Lambda(Wide)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(STextBlock).Text(INVTEXT("101")).Font(Studio::Text(EText::Wordmark)).ColorAndOpacity(P.Text1)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(1.0f, 0.0f, 5.0f, 0.0f)
		[SNew(STextBlock).Text(INVTEXT("st")).Font(Studio::Text(EText::WordmarkSuffix)).ColorAndOpacity(P.Text1)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(STextBlock).Text(INVTEXT("FLOOR")).Font(Studio::Text(EText::Wordmark)).ColorAndOpacity(P.Text1)];

	const TSharedRef<SWidget> Brand = SNew(SButton).ButtonStyle(&Studio::ButtonStyle(EButton::Ghost)).IsFocusable(false)
		.ContentPadding(FMargin(6.0f, 0.0f, 8.0f, 0.0f)).VAlign(VAlign_Center)
		.ToolTip(Ui::Tooltip(LOCTEXT("StudioHomeTip", "Volver al inicio")))
		.OnClicked_Lambda([this]() { OpenHome(); return FReply::Handled(); })
		[
			SNew(SBox).HeightOverride(36.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Ui::Icon(EFloor101Icon::Mark, 24.0f, FSlateColor(P.Text1))]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)[Wordmark]
			]
		];

	const TSharedRef<SWidget> Left = SNew(SBox).Clipping(EWidgetClipping::ClipToBounds).HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Brand]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 12.0f, 0.0f)[Ui::Divider(true, 20.0f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).MaxDesiredWidth_Lambda([this]() { return FOptionalSize(ViewSize.X >= Studio::Breakpoint::BarLabels ? 280.0f : 140.0f); })
				[
					SNew(STextBlock).Font(Studio::Text(EText::BodyStrong)).ColorAndOpacity(P.Text1).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text_Lambda([this]()
					{
						const UFloor101DocumentSubsystem* S = Document.Get();
						return FText::FromString(S && !S->GetProjectPath().IsEmpty() ? FPaths::GetBaseFilename(S->GetProjectPath()) : TEXT("Nuevo proyecto"));
					})
					.ToolTipText_Lambda([this]() { const auto* S = Document.Get(); return S ? FText::FromString(S->GetProjectPath()) : FText::GetEmpty(); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox).ToolTip(Ui::Tooltip(LOCTEXT("StudioDirty", "Cambios sin guardar")))
				.Visibility_Lambda([this]() { const auto* S = Document.Get(); return S && S->HasUnsavedChanges() ? EVisibility::Visible : EVisibility::Collapsed; })
				[Ui::Dot(FSlateColor(P.Brass), 7.0f)]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)[MakeFileMenu()]
		];

	auto ModeActive = [this](int32 Index)
	{
		return [this, Index]()
		{
			const auto* Client = ViewportClient.Get();
			return Index == 0 ? IsDesignMode() : Index == 1 ? Client && Client->IsNavigationMode() && !IsPhotoMode() : IsPhotoMode();
		};
	};
	auto ModeClick = [this](int32 Index)
	{
		return [this, Index]()
		{
			if (auto* Client = ViewportClient.Get()) { Client->SetPhotoMode(Index == 2); Client->SetNavigationMode(Index == 1); }
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		};
	};
	TArray<Ui::FSegment> Modes;
	Modes.Add({EFloor101Icon::Design, LOCTEXT("StudioDesign", "Diseñar"), FText::GetEmpty(), LOCTEXT("StudioDesignTip", "Diseñar: dibujar y editar el proyecto"), ModeActive(0), ModeClick(0)});
	Modes.Add({EFloor101Icon::Walk, LOCTEXT("StudioWalk", "Recorrer"), INVTEXT("N"), LOCTEXT("StudioWalkTip", "Recorrer el proyecto"), ModeActive(1), ModeClick(1)});
	Modes.Add({EFloor101Icon::Photo, LOCTEXT("StudioPhoto", "Foto"), INVTEXT("R"), LOCTEXT("StudioPhotoTip", "Foto con path tracing"), ModeActive(2), ModeClick(2)});
	const TSharedRef<SWidget> ModeSwitch = Ui::Segmented(Modes, TAttribute<bool>::CreateLambda([this]() { return ViewSize.X >= Studio::Breakpoint::ModeLabels; }));

	auto Save = [this]() { if (auto* S = Document.Get()) { FFloor101FileActions::Save(*S); } };
	const TSharedRef<SWidget> Right = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			Ui::IconButton(EFloor101Icon::Undo, [this]() { Run([](EditController& E) { return E.undo(); }); }, LOCTEXT("StudioUndo", "Deshacer"),
				INVTEXT("Ctrl+Z"), false, TAttribute<bool>::CreateLambda([this]() { const auto* S = Document.Get(); return S && S->GetDocument().canUndo(); }))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f, 0.0f, 0.0f)
		[
			Ui::IconButton(EFloor101Icon::Redo, [this]() { Run([](EditController& E) { return E.redo(); }); }, LOCTEXT("StudioRedo", "Rehacer"),
				INVTEXT("Ctrl+Y"), false, TAttribute<bool>::CreateLambda([this]() { const auto* S = Document.Get(); return S && S->GetDocument().canRedo(); }))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox).Visibility_Lambda(Wide)
			[Ui::Button(EButton::Secondary, LOCTEXT("StudioSave", "Guardar"), Save, EFloor101Icon::Save, LOCTEXT("StudioSaveTip", "Guardar"), INVTEXT("Ctrl+S"), true, 30.0f)]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).Visibility_Lambda(Narrow)
			[Ui::IconButton(EFloor101Icon::Save, Save, LOCTEXT("StudioSaveTip", "Guardar"), INVTEXT("Ctrl+S"))]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)[MakeUtilityMenu()]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f)[Ui::Divider(true, 20.0f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeExitButton(true)];

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder).BorderImage(Studio::White()).BorderBackgroundColor(P.Bar).Padding(FMargin(8.0f, 0.0f, 10.0f, 0.0f))
			[
				SNew(SBox).HeightOverride(Studio::Size::Bar - 1.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[Left]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f)[ModeSwitch]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).HAlign(HAlign_Right)[Right]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(1.0f)[SNew(SBorder).BorderImage(Studio::White()).BorderBackgroundColor(Studio::Color(TEXT("262B30"))).Padding(0.0f)]
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakeUtilityMenu()
{
	return Floor101::Ui::IconButton(EFloor101Icon::Options, [this]() { OpenOptions(); }, LOCTEXT("StudioOptions", "Opciones"));
}

TSharedRef<SWidget> SFloor101Overlay::MakeExitButton(bool bCompact)
{
	return Floor101::Ui::Button(Floor101::Studio::EButton::Exit, LOCTEXT("Exit", "Salir"),
		[this]() { if (UFloor101GameViewportClient* Client = ViewportClient.Get()) { Client->RequestExit(); } }, EFloor101Icon::Exit,
		LOCTEXT("ExitTip", "Cierra 101st Floor. Si hay cambios sin guardar, permite guardarlos, descartarlos o cancelar."), FText::GetEmpty(), true,
		bCompact ? 30.0f : 40.0f);
}

TSharedRef<SWidget> SFloor101Overlay::MakeFileMenu()
{
	namespace Ui = Floor101::Ui;
	return (DocumentMenu = Ui::MenuButton(
		Ui::MenuButtonContent(EFloor101Icon::Doc, LOCTEXT("StudioDocument", "Documento"),
			TAttribute<EVisibility>::CreateLambda([this]() { return ViewSize.X >= Floor101::Studio::Breakpoint::BarLabels ? EVisibility::Visible : EVisibility::Collapsed; })),
		[this]() -> TSharedRef<SWidget>
		{
			return SNew(SBox).WidthOverride(300.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuItem(EFloor101Icon::Folder, LOCTEXT("StudioOpen", "Abrir…"),
					[this]() { if (auto* S = Document.Get()) { FFloor101FileActions::Open(*S); } }, INVTEXT("Ctrl+O"))]
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuItem(EFloor101Icon::Save, LOCTEXT("StudioSaveAs", "Guardar como…"),
					[this]() { if (auto* S = Document.Get()) { FFloor101FileActions::SaveAs(*S); } }, INVTEXT("Ctrl+Shift+S"))]
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuSeparator()]
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuHeading(LOCTEXT("StudioExport", "Exportar"))]
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuItem(EFloor101Icon::Export, LOCTEXT("StudioGltf", "Para Twinmotion (.glb)…"),
					[this]() { if (auto* S = Document.Get()) { FFloor101FileActions::ExportTwinmotion(*S); } })]
				+ SVerticalBox::Slot().AutoHeight()[MakePlanExportButton(TEXT("svg"))]
				+ SVerticalBox::Slot().AutoHeight()[MakePlanExportButton(TEXT("png"))]
			];
		},
		LOCTEXT("StudioDocumentTip", "Abrir, guardar como y exportar"), Floor101::Studio::EButton::Ghost, FMargin(8.0f, 0.0f), 30.0f)).ToSharedRef();
}

TSharedRef<SWidget> SFloor101Overlay::MakeViewMenu()
{
	namespace Ui = Floor101::Ui;
	auto Focus = []() { FSlateApplication::Get().SetAllUserFocusToGameViewport(); };
	return (ViewMenu = Ui::MenuButton(Ui::MenuButtonContent(EFloor101Icon::Eye, LOCTEXT("StudioView", "Vista")),
		[this, Focus]() -> TSharedRef<SWidget>
		{
			return SNew(SBox).WidthOverride(320.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuItem(EFloor101Icon::Frame, LOCTEXT("StudioFrame", "Encuadrar"),
					[this, Focus]() { if (auto* C = ViewportClient.Get()) { C->FrameProject(); } Focus(); }, INVTEXT("F"))]
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuItem(EFloor101Icon::Top, LOCTEXT("StudioTop", "Vista superior"),
					[this, Focus]() { if (auto* C = ViewportClient.Get()) { C->ShowTopView(); } Focus(); }, INVTEXT("T"))]
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuItem({}, LOCTEXT("StudioPlan", "Planta 2D"),
					[this, Focus]() { if (auto* C = ViewportClient.Get()) { C->SetPlanView(!C->IsPlanView()); } Focus(); }, INVTEXT("Tab"), FText::GetEmpty(),
					[this]() { const auto* C = ViewportClient.Get(); return C && C->IsPlanView(); })]
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuItem({}, LOCTEXT("StudioXray", "Rayos X"),
					[this]() { Run([](EditController& E) { E.setShowServices(!E.showsServices()); return f101::Status::success(); }); }, INVTEXT("X"),
					FText::GetEmpty(), [this]() { const auto* S = Document.Get(); return S && S->GetEditController().showsServices(); })]
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuItem({}, LOCTEXT("StudioGrid", "Cuadrícula del suelo"),
					[]() { Floor101::Preferences::SetShowGrid(!Floor101::Preferences::ShowGrid()); }, FText::GetEmpty(), FText::GetEmpty(),
					[]() { return Floor101::Preferences::ShowGrid(); })]
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuSeparator()]
				+ SVerticalBox::Slot().AutoHeight()[Ui::MenuItem(EFloor101Icon::Dimension, LOCTEXT("RestoreDimensions", "Restaurar cotas automáticas de esta planta"),
					[this]() { Run([](EditController& E) { return E.restoreAutomaticDimensions(); }); }, FText::GetEmpty(),
					LOCTEXT("RestoreDimensionsTip", "Recupera cotas ocultas y sus rótulos y posiciones originales. Se puede deshacer."))]
				+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 8.0f, 8.0f, 4.0f)[MakeNetworksRow()]
			];
		},
		LOCTEXT("StudioViewTip", "Encuadre, vistas, cuadrícula, cotas y redes"), Floor101::Studio::EButton::Ghost, FMargin(8.0f, 0.0f), 32.0f)).ToSharedRef();
}

TSharedRef<SWidget> SFloor101Overlay::MakeNavigationSpeed()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	const Studio::FPalette& P = Studio::Palette();
	auto Level = [this]() { const UFloor101GameViewportClient* Client = ViewportClient.Get(); return Client ? Client->GetNavigationSpeedLevel() : 5; };
	const TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)[Ui::Icon(EFloor101Icon::Walk, 16.0f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[SNew(STextBlock).Text(LOCTEXT("SpeedLabel", "Velocidad")).Font(Studio::Text(Studio::EText::Button)).ColorAndOpacity(FSlateColor::UseForeground())]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[SNew(STextBlock).Font(Studio::Text(Studio::EText::Numeric)).ColorAndOpacity(P.Text1).Text_Lambda([Level]() { return FText::AsNumber(Level()); })]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(Studio::Text(Studio::EText::NumericSmall)).ColorAndOpacity(P.Text3)
			.Text_Lambda([Level]() { return FText::FromString(WorkspaceSpeed(AFloor101EditingPawn::NavigationSpeedForLevel(Level()) / 100.0) + TEXT(" m/s")); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)[Ui::Icon(EFloor101Icon::ChevronDown, 16.0f)];
	const TSharedRef<SComboButton> Combo = Ui::MenuButton(Content, [this]() -> TSharedRef<SWidget>
		{
			TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
			Menu->AddSlot().AutoHeight()[Ui::MenuHeading(LOCTEXT("SpeedHeading", "Velocidad de navegación"))];
			for (int32 Step = 1; Step <= 10; ++Step)
			{
				const FText Label = FText::FromString(FString::Printf(TEXT("Nivel %d · %s m/s%s"), Step,
					*WorkspaceSpeed(AFloor101EditingPawn::NavigationSpeedForLevel(Step) / 100.0), Step == 5 ? TEXT(" (inicial)") : TEXT("")));
				Menu->AddSlot().AutoHeight()[Ui::MenuItem({}, Label, [this, Step]()
				{
					if (UFloor101GameViewportClient* Client = ViewportClient.Get()) { Client->SetNavigationSpeedLevel(Step); }
					FSlateApplication::Get().SetAllUserFocusToGameViewport();
				}, Step == 10 ? INVTEXT("0") : FText::AsNumber(Step), FText::GetEmpty(), [this, Step]()
				{
					const UFloor101GameViewportClient* Client = ViewportClient.Get();
					return Client && Client->GetNavigationSpeedLevel() == Step;
				})];
			}
			return SNew(SBox).WidthOverride(260.0f)[Menu];
		},
		LOCTEXT("NavigationSpeedTip", "Teclas superiores 1–9 y 0 (nivel 10), excepto al escribir medidas. Niveles 1–10: desde precisión hasta recorrido rápido. La velocidad elegida se guarda y no cambia al acercarte. Shift la multiplica por cuatro."),
		Studio::EButton::Ghost, FMargin(10.0f, 0.0f), 32.0f);
	if (!SpeedMenu) { SpeedMenu = Combo; } // the view bar's, built first
	return Combo;
}

TSharedRef<SWidget> SFloor101Overlay::MakeViewBar()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	auto Focus = []() { FSlateApplication::Get().SetAllUserFocusToGameViewport(); };
	PropertiesMenu = Ui::MenuButton(Ui::MenuButtonContent(EFloor101Icon::Sliders, LOCTEXT("StudioProperties", "Propiedades")),
		[this]() -> TSharedRef<SWidget>
		{
			return SNew(SBox).MaxDesiredHeight(static_cast<float>(FMath::Max(180.0, ViewSize.Y - 140.0)))[MakePropertiesPanel()];
		},
		LOCTEXT("StudioPropertiesTip", "Propiedades de la selección o de la planta"), Studio::EButton::Ghost, FMargin(8.0f, 0.0f), 32.0f);
	const TSharedRef<SWidget> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[Ui::IconButton(EFloor101Icon::Frame, [this, Focus]() { if (auto* C = ViewportClient.Get()) { C->FrameProject(); } Focus(); },
			LOCTEXT("StudioFrame", "Encuadrar"), INVTEXT("F"))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)[Ui::IconButton(EFloor101Icon::Top, [this, Focus]() { if (auto* C = ViewportClient.Get()) { C->ShowTopView(); } Focus(); },
			LOCTEXT("StudioTop", "Vista superior"), INVTEXT("T"))]
		+ SHorizontalBox::Slot().AutoWidth()[Ui::IconButton(EFloor101Icon::Plan, [this, Focus]() { if (auto* C = ViewportClient.Get()) { C->SetPlanView(!C->IsPlanView()); } Focus(); },
			LOCTEXT("StudioPlan", "Planta 2D"), INVTEXT("Tab"), TAttribute<bool>::CreateLambda([this]() { const auto* C = ViewportClient.Get(); return C && C->IsPlanView(); }))]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)[Ui::IconButton(EFloor101Icon::Xray,
			[this]() { Run([](EditController& E) { E.setShowServices(!E.showsServices()); return f101::Status::success(); }); },
			LOCTEXT("StudioXray", "Rayos X"), INVTEXT("X"), TAttribute<bool>::CreateLambda([this]() { const auto* S = Document.Get(); return S && S->GetEditController().showsServices(); }))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)[Ui::Divider(true, 20.0f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeViewMenu()]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)[Ui::Divider(true, 20.0f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeNavigationSpeed()]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox).Visibility_Lambda([this]() { return !IsInspectorDocked() && IsDesignMode() ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)[Ui::Divider(true, 20.0f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[PropertiesMenu.ToSharedRef()]
		];
	return Ui::Island(Row, FMargin(4.0f), EVisibility::SelfHitTestInvisible, 11.0f);
}

TSharedRef<SWidget> SFloor101Overlay::MakeToolRail()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	struct FTool
	{
		EditMode Mode;
		FText Name;
		FText Key;
	};
	// Grouped as the work goes: choose, build, install, furnish and finish, document.
	const TArray<TArray<FTool>> Groups = {
		{{EditMode::Select, LOCTEXT("StudioSelect", "Seleccionar"), INVTEXT("V")}},
		{{EditMode::Wall, LOCTEXT("StudioWall", "Pared"), INVTEXT("B")}, {EditMode::Door, LOCTEXT("StudioDoor", "Puerta"), INVTEXT("P")},
			{EditMode::Window, LOCTEXT("StudioWindow", "Ventana"), INVTEXT("O")}, {EditMode::Column, LOCTEXT("StudioColumn", "Columna"), INVTEXT("C")}},
		{{EditMode::Services, LOCTEXT("StudioServices", "Instalaciones"), INVTEXT("I")}},
		{{EditMode::Furniture, LOCTEXT("StudioFurniture", "Muebles"), INVTEXT("G")}, {EditMode::Paint, LOCTEXT("StudioPaint", "Materiales"), INVTEXT("M")}},
		{{EditMode::Dimension, LOCTEXT("StudioDimensions", "Cotas"), INVTEXT("H")}},
	};
	auto Wide = [this]() { return ViewSize.X >= Studio::Breakpoint::DockLabels ? EVisibility::Visible : EVisibility::Collapsed; };
	TSharedRef<SVerticalBox> Tools = SNew(SVerticalBox);
	for (int32 Group = 0; Group < Groups.Num(); ++Group)
	{
		if (Group > 0)
		{
			Tools->AddSlot().AutoHeight().Padding(4.0f, 6.0f)[Ui::Divider()];
		}
		for (const FTool& Tool : Groups[Group])
		{
			const EditMode Mode = Tool.Mode;
			auto Active = [this, Mode]() { const auto* S = Document.Get(); return S && S->GetEditController().mode() == Mode; };
			const TSharedRef<SWidget> Content = SNew(SBox).HeightOverride(36.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(0.0f, 9.0f)
					[
						SNew(SBox).WidthOverride(2.0f).Visibility_Lambda([Active]() { return Active() ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
						[SNew(SBorder).BorderImage(Studio::Box(Studio::Palette().Brass, 1.0f)).Padding(0.0f)]
					]
					+ SOverlay::Slot().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[Ui::Icon(SFloor101Icon::ForMode(Mode), 20.0f, TAttribute<FSlateColor>::CreateLambda([Active]()
							{ return Active() ? FSlateColor(Floor101::Studio::Palette().Brass) : FSlateColor::UseForeground(); }))]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10.0f, 0.0f, 8.0f, 0.0f)
						[SNew(STextBlock).Text(Tool.Name).Font(Studio::Text(Studio::EText::Body)).ColorAndOpacity(FSlateColor::UseForeground()).Visibility_Lambda(Wide)]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).Visibility_Lambda(Wide)
							[Ui::Keycap(Tool.Key, TAttribute<FSlateColor>::CreateLambda([Active]()
								{ return Active() ? FSlateColor(Floor101::Studio::Palette().Brass) : FSlateColor::UseForeground(); }))]
						]
					]
				];
			Tools->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
			[
				Ui::Selectable(Content, TAttribute<bool>::CreateLambda(Active), [this, Mode]()
				{
					if (Mode == EditMode::Dimension) { if (auto* C = ViewportClient.Get()) { C->SetPlanView(true); } }
					Run([Mode](EditController& E) { E.setMode(Mode); return f101::Status::success(); });
				}, Tool.Name, Tool.Key)
			];
		}
	}
	return SNew(SBox)
		.WidthOverride_Lambda([this]() { return FOptionalSize(ViewSize.X >= Studio::Breakpoint::DockLabels ? Studio::Size::DockWide : Studio::Size::DockNarrow); })
		.Visibility_Lambda([this]() { return IsDesignMode() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
		[Ui::Island(Tools, FMargin(6.0f))];
}

bool SFloor101Overlay::IsLevelIslandCompact() const
{
	const UFloor101DocumentSubsystem* S = Document.Get();
	const int32 Rows = S ? FMath::Clamp(static_cast<int32>(S->GetDocument().project().levels.size()), 1, 5) : 1;
	const double Available = ViewSize.Y - Floor101::Studio::Size::Bar;
	const double Dock = IsDesignMode() ? Floor101::Studio::Size::Gutter + 412.0 : 0.0;
	const double Island = 100.0 + 38.0 * Rows;
	return Dock + Island + 2.0 * Floor101::Studio::Size::Gutter > Available;
}

TSharedRef<SWidget> SFloor101Overlay::MakeLevelRow()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	const Studio::FPalette& P = Studio::Palette();
	auto ActiveIndex = [this]() -> std::size_t
	{
		const auto* S = Document.Get();
		return S ? S->GetDocument().project().levelIndex(S->GetEditController().activeLevel()).value_or(0) : 0;
	};
	auto LevelCount = [this]() -> std::size_t { const auto* S = Document.Get(); return S ? S->GetDocument().project().levels.size() : 0; };

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	if (const UFloor101DocumentSubsystem* S = Document.Get())
	{
		const f101::cad::Project& Project = S->GetDocument().project();
		for (auto It = Project.levels.rbegin(); It != Project.levels.rend(); ++It)
		{
			const f101::cad::LevelId Id = It->id;
			const FText Short = Floor101::LevelTag(Project, Id);
			const FText Name = Floor101::LevelName(Project, Id);
			const FText Elevation = FText::FromString(Studio::Elevation(f101::cad::levelElevationMm(Project, Id)));
			auto Active = [this, Id]() { const auto* D = Document.Get(); return D && D->GetEditController().activeLevel() == Id; };
			const TSharedRef<SWidget> Content = SNew(SBox).HeightOverride(36.0f).Padding(FMargin(8.0f, 0.0f, 10.0f, 0.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).MinDesiredWidth(30.0f).HeightOverride(22.0f)
						[
							SNew(SBorder).Padding(FMargin(5.0f, 0.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
							.BorderImage_Lambda([Active]()
							{
								const auto& Colors = Floor101::Studio::Palette();
								return Active() ? Floor101::Studio::Box(Colors.Brass, 5.0f) : Floor101::Studio::Box(FLinearColor::Transparent, 5.0f, Colors.EdgeSoft, 1.0f);
							})
							[
								SNew(STextBlock).Text(Short).Font(Studio::Text(Studio::EText::NumericSmall))
								.ColorAndOpacity_Lambda([Active]() { return FSlateColor(Active() ? Floor101::Studio::Palette().OnBrass : Floor101::Studio::Palette().Text2); })
							]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10.0f, 0.0f, 8.0f, 0.0f)
					[SNew(STextBlock).Text(Name).Font(Studio::Text(Studio::EText::Body)).ColorAndOpacity(FSlateColor::UseForeground()).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(Elevation).Font(Studio::Text(Studio::EText::NumericSmall))
						.ColorAndOpacity_Lambda([Active]() { return FSlateColor(Active() ? Floor101::Studio::Palette().Brass : Floor101::Studio::Palette().Text3); })
					]
				];
			List->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
			[
				Ui::Selectable(Content, TAttribute<bool>::CreateLambda(Active),
					[this, Id]() { Run([Id](EditController& E) { return E.setActiveLevel(Id); }); },
					FText::Format(LOCTEXT("EditLevelTip", "Editar {0}"), Name))
			];
		}
	}

	const TSharedRef<SWidget> ShowAll = Ui::Selectable(
		SNew(SBox).HeightOverride(28.0f).Padding(FMargin(8.0f, 0.0f)).VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[Ui::Icon(EFloor101Icon::Levels, 16.0f, TAttribute<FSlateColor>::CreateLambda([this]()
				{ const auto* D = Document.Get(); return D && D->GetEditController().showsAllLevels() ? FSlateColor(Floor101::Studio::Palette().Brass) : FSlateColor::UseForeground(); }))]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("AllLevels", "Ver todas")).Font(Studio::Text(Studio::EText::Button)).ColorAndOpacity(FSlateColor::UseForeground())]
		],
		TAttribute<bool>::CreateLambda([this]() { const auto* D = Document.Get(); return D && D->GetEditController().showsAllLevels(); }),
		[this]() { Run([](EditController& E) { E.setShowAllLevels(!E.showsAllLevels()); return f101::Status::success(); }); },
		LOCTEXT("AllLevelsTip", "Muestra todas las plantas con sus forjados"), INVTEXT("L"));

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f, 2.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("LevelLabel", "PLANTAS")).Font(Studio::Text(Studio::EText::Overline)).ColorAndOpacity(P.Text3)]
			+ SHorizontalBox::Slot().AutoWidth()
			[Ui::IconButton(EFloor101Icon::Up, [this]() { Run([](EditController& E) { return E.levelUp(); }); }, LOCTEXT("LevelUpTip", "Editar la planta de arriba"),
				INVTEXT("RePág"), false, TAttribute<bool>::CreateLambda([ActiveIndex, LevelCount]() { return ActiveIndex() + 1 < LevelCount(); }), 28.0f)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[Ui::IconButton(EFloor101Icon::Down, [this]() { Run([](EditController& E) { return E.levelDown(); }); }, LOCTEXT("LevelDownTip", "Editar la planta de abajo"),
				INVTEXT("AvPág"), false, TAttribute<bool>::CreateLambda([ActiveIndex]() { return ActiveIndex() > 0; }), 28.0f)]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).MaxDesiredHeight(5.0f * 38.0f)
			[SNew(SScrollBox).ScrollBarStyle(&Studio::ScrollBar()) + SScrollBox::Slot()[List]]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 6.0f)[Ui::Divider()]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[Ui::Button(Studio::EButton::Ghost, LOCTEXT("AddLevel", "Nueva"), [this]() { Run([](EditController& E) { return E.addLevel(); }); }, EFloor101Icon::Plus,
				LOCTEXT("AddLevelTip", "Nueva planta: se añade encima y pasa a editarla"), FText::GetEmpty(), true, 28.0f)]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			+ SHorizontalBox::Slot().AutoWidth()[ShowAll]
		];
}

void SFloor101Overlay::RefreshLevelIsland()
{
	if (LevelIslandHost)
	{
		LevelIslandHost->SetContent(SNew(SBox).WidthOverride(Floor101::Studio::Size::DockWide - 12.0f)[MakeLevelRow()]);
	}
}

TSharedRef<SWidget> SFloor101Overlay::MakeLevelIsland()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	SAssignNew(LevelIslandHost, SBox);
	RefreshLevelIsland();
	const TSharedRef<SWidget> Selector = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)[Ui::Icon(EFloor101Icon::Levels, 16.0f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(Studio::Text(Studio::EText::Button)).ColorAndOpacity(FSlateColor::UseForeground())
			.Text_Lambda([this]()
			{
				const auto* S = Document.Get();
				return S ? Floor101::LevelName(S->GetDocument().project(), S->GetEditController().activeLevel()) : LOCTEXT("StudioLevel", "Plantas");
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock).Font(Studio::Text(Studio::EText::NumericSmall)).ColorAndOpacity(Studio::Palette().Brass)
			.Text_Lambda([this]()
			{
				const auto* S = Document.Get();
				return S ? FText::FromString(Studio::Elevation(f101::cad::levelElevationMm(S->GetDocument().project(), S->GetEditController().activeLevel()))) : FText::GetEmpty();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)[Ui::Icon(EFloor101Icon::Up, 16.0f)];
	LevelMenu = Ui::MenuButton(Selector, [this]() -> TSharedRef<SWidget> { return SNew(SBox).WidthOverride(Studio::Size::DockWide - 12.0f)[MakeLevelRow()]; },
		LOCTEXT("LevelsTip", "Plantas: subir, bajar, nueva planta y ver todas"), Studio::EButton::Ghost, FMargin(8.0f, 0.0f), 32.0f);
	LevelMenu->SetMenuPlacement(MenuPlacement_AboveAnchor);
	return SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
		[Ui::Island(LevelIslandHost.ToSharedRef(), FMargin(6.0f), TAttribute<EVisibility>::CreateLambda([this]()
			{ return IsLevelIslandCompact() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; }))]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
		[Ui::Island(LevelMenu.ToSharedRef(), FMargin(4.0f), TAttribute<EVisibility>::CreateLambda([this]()
			{ return IsLevelIslandCompact() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; }), 11.0f)];
}

TSharedRef<SWidget> SFloor101Overlay::MakeHintBar()
{
	namespace Studio = Floor101::Studio;
	SAssignNew(HintHost, SBox);
	return SNew(SBox)
		.MaxDesiredWidth_Lambda([this]()
		{
			return FOptionalSize(static_cast<float>(FMath::Max(240.0, ViewSize.X - 2.0 * (Studio::Size::DockWide + 2.0 * Studio::Size::Gutter) - 16.0)));
		})
		[
			Floor101::Ui::Island(SNew(SBox).MinDesiredHeight(28.0f).VAlign(VAlign_Center).Padding(FMargin(10.0f, 2.0f))[HintHost.ToSharedRef()],
				FMargin(4.0f), EVisibility::SelfHitTestInvisible, 19.0f)
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakeHintContent(const FText& Hint, bool bDefault)
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	using Studio::EText;
	const Studio::FPalette& P = Studio::Palette();
	// Wraps only past the widest the pill may be (the space between the levels and the status).
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(false).InnerSlotPadding(FVector2D(16.0, 4.0))
		.PreferredSize_Lambda([this]()
		{
			return static_cast<float>(FMath::Max(220.0, ViewSize.X - 2.0 * (Studio::Size::DockWide + 2.0 * Studio::Size::Gutter) - 60.0));
		});
	auto Add = [&Wrap](const TSharedRef<SWidget>& Widget) { Wrap->AddSlot().VAlign(VAlign_Center)[Widget]; };
	auto WrapAt = [this]() { return static_cast<float>(FMath::Max(200.0, ViewSize.X - 2.0 * (Studio::Size::DockWide + 2.0 * Studio::Size::Gutter) - 80.0)); };
	auto Pair = [&P](TArray<TSharedRef<SWidget>> Keys, const FText& Action)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		for (const TSharedRef<SWidget>& Key : Keys) { Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(Row->NumSlots() > 0 ? 4.0f : 0.0f, 0.0f, 0.0f, 0.0f)[Key]; }
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[SNew(STextBlock).Text(Action).Font(Studio::Text(EText::Label)).ColorAndOpacity(P.Text2)];
		return Row;
	};
	if (bDefault)
	{
		Add(Pair({Ui::Keycap(INVTEXT("V"))}, LOCTEXT("HintKeySelect", "Seleccionar")));
		Add(Pair({Ui::Keycap(INVTEXT("Esc"))}, LOCTEXT("HintKeyCancel", "Cancelar")));
		Add(Pair({Ui::Keycap(INVTEXT("WASD"))}, LOCTEXT("HintKeyMove", "Moverse")));
		Add(Pair({Ui::Keycap(INVTEXT("Shift")), WorkspaceText(TEXT("+ derecho"), EText::Help, P.Text3)}, LOCTEXT("HintKeyLook", "Mirar")));
		Add(Pair({WorkspaceText(TEXT("Rueda"), EText::Help, P.Text3)}, LOCTEXT("HintKeyZoom", "Acercarse")));
		return Wrap;
	}
	TArray<FString> Segments;
	Hint.ToString().ParseIntoArray(Segments, TEXT(" · "));
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const FString& Segment = Segments[Index];
		if (Index == 0)
		{
			Add(SNew(STextBlock).Text(FText::FromString(Segment)).Font(Studio::Text(EText::Button)).ColorAndOpacity(P.Text1));
			continue;
		}
		FString Keys;
		FString Action;
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		if (Segment.Split(TEXT(": "), &Keys, &Action) && WorkspaceKeys(Keys, Row))
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[SNew(STextBlock).Text(FText::FromString(Action)).Font(Studio::Text(EText::Label)).ColorAndOpacity(P.Text2).WrapTextAt_Lambda([WrapAt]() { return WrapAt() * 0.7f; })];
			Add(Row);
		}
		else
		{
			Add(SNew(STextBlock).Text(FText::FromString(Segment)).Font(Studio::Text(EText::Label)).ColorAndOpacity(P.Text2).WrapTextAt_Lambda(WrapAt));
		}
	}
	return Wrap;
}

TSharedRef<SWidget> SFloor101Overlay::MakeStatusIsland()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	const Studio::FPalette& P = Studio::Palette();
	auto Message = [this]() { const auto* S = Document.Get(); return S ? S->GetStatusMessage() : FText::GetEmpty(); };
	auto Error = [this]() { const auto* S = Document.Get(); return S && S->IsStatusError(); };
	auto Dirty = [this]() { const auto* S = Document.Get(); return S && S->HasUnsavedChanges(); };
	const TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
		[
			SNew(SHorizontalBox).Visibility_Lambda([Message]() { return Message().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.ToolTip(Ui::Tooltip(TAttribute<FText>::CreateLambda(Message)))
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[SNew(SBox).Visibility_Lambda([Error]() { return Error() ? EVisibility::Visible : EVisibility::Collapsed; })[Ui::Icon(EFloor101Icon::Warn, 16.0f, FSlateColor(P.Danger))]]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda(Message).Font(Studio::Text(Studio::EText::Help)).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				.ColorAndOpacity_Lambda([Error]() { return FSlateColor(Error() ? Floor101::Studio::Palette().Danger : Floor101::Studio::Palette().Text2); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
		[
			SNew(SHorizontalBox).Visibility_Lambda([Dirty]() { return Dirty() ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)[Ui::Dot(FSlateColor(P.Brass), 7.0f)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("StudioDirty", "Cambios sin guardar")).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Text2)]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(Studio::Box(P.Raised, 12.0f)).Padding(FMargin(10.0f, 3.0f))
			.ToolTip(Ui::Tooltip(LOCTEXT("UnitsHint", "Sin unidad = cm. También: 3.55 m, 3550 mm")))
			[SNew(STextBlock).Text(INVTEXT("cm")).Font(Studio::Text(Studio::EText::NumericSmall)).ColorAndOpacity(P.Text1)]
		];
	return Ui::Island(SNew(SBox).MinDesiredHeight(28.0f).Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))[Content], FMargin(4.0f), EVisibility::SelfHitTestInvisible, 19.0f);
}

#undef LOCTEXT_NAMESPACE
