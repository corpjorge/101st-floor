#include "SFloor101Overlay.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/Units.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101GameViewportClient.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <algorithm>

#define LOCTEXT_NAMESPACE "Floor101Overlay"

namespace
{
	using f101::cad::EditController;
	using f101::cad::EditMode;

	const FLinearColor PanelColor(0.015f, 0.015f, 0.02f, 0.8f);
	const FLinearColor ActiveButtonColor(1.0f, 0.5f, 0.12f);
	const FLinearColor ErrorColor(1.0f, 0.4f, 0.35f);
	const FLinearColor MutedColor(0.7f, 0.7f, 0.72f);
	const FLinearColor RoomLabelColor(0.05f, 0.18f, 0.3f, 0.85f);
	const FVector2D CursorLabelOffset(18.0, 18.0);

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FText LengthText(double Millimeters)
	{
		return FText::FromString(UTF8_TO_TCHAR(f101::cad::formatLength(Millimeters, f101::cad::LengthUnit::Centimeter).c_str()));
	}

	FText SnapText(f101::cad::SnapKind Kind)
	{
		switch (Kind)
		{
		case f101::cad::SnapKind::Endpoint: return LOCTEXT("SnapEndpoint", "extremo");
		case f101::cad::SnapKind::Wall: return LOCTEXT("SnapWall", "muro");
		case f101::cad::SnapKind::Angle: return LOCTEXT("SnapAngle", "ángulo");
		case f101::cad::SnapKind::Grid: return LOCTEXT("SnapGrid", "rejilla");
		default: return FText::GetEmpty();
		}
	}

	// "Planta baja", "Planta 1"... unless the level has a name of its own.
	FText LevelName(const f101::cad::Project& Project, const f101::cad::LevelId& Level)
	{
		const std::size_t Index = Project.levelIndex(Level).value_or(0);
		const f101::cad::Level* Found = Project.findLevel(Level);
		if (Found && !Found->name.empty())
		{
			return FText::FromString(UTF8_TO_TCHAR(Found->name.c_str()));
		}
		return Index == 0 ? LOCTEXT("GroundFloor", "Planta baja") : FText::Format(LOCTEXT("FloorN", "Planta {0}"), FText::AsNumber(static_cast<int32>(Index)));
	}

	// "+2,80 m" (elevation in metres, two decimals, current culture).
	FText ElevationText(std::int64_t Millimeters)
	{
		FNumberFormattingOptions TwoDecimals;
		TwoDecimals.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("Elevation", "{0}{1} m"), Millimeters >= 0 ? FText::FromString(TEXT("+")) : FText::GetEmpty(),
			FText::AsNumber(static_cast<double>(Millimeters) / 1000.0, &TwoDecimals));
	}

	TSharedRef<SWidget> Panel(const TSharedRef<SWidget>& Content)
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(PanelColor)
			.Padding(FMargin(8.0f, 6.0f))
			[
				Content
			];
	}
}

// Net area of each room, drawn where its floor label point projects on screen. Repainted every frame.
class SFloor101RoomLabels : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFloor101RoomLabels) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UFloor101DocumentSubsystem>, Document)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Document = InArgs._Document;
		ForceVolatile(true);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D::ZeroVector;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const UFloor101DocumentSubsystem* Subsystem = Document.Get();
		const UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		const ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
		if (!Player || !Player->ViewportClient || Subsystem->GetRooms().IsEmpty())
		{
			return LayerId;
		}
		FVector2D ViewportPixels;
		Player->ViewportClient->GetViewportSize(ViewportPixels);
		const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
		if (ViewportPixels.X <= 0.0 || ViewportPixels.Y <= 0.0)
		{
			return LayerId;
		}
		const FVector2D PixelsToLocal = LocalSize / ViewportPixels;

		FNumberFormattingOptions TwoDecimals;
		TwoDecimals.SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);
		const FSlateFontInfo LabelFont = Font(11, true);
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FSlateBrush* Background = FCoreStyle::Get().GetBrush("WhiteBrush");
		const FVector2D Padding(6.0, 3.0);
		for (const FFloor101RoomView& Room : Subsystem->GetRooms())
		{
			FVector2D Screen;
			if (!Controller->ProjectWorldLocationToScreen(Room.LabelLocation, Screen, true))
			{
				continue; // behind the camera
			}
			const FText Text = FText::Format(LOCTEXT("RoomArea", "{0} m²"), FText::AsNumber(Room.AreaM2, &TwoDecimals));
			const FVector2D TextSize = Measure->Measure(Text, LabelFont);
			const FVector2D BoxSize = TextSize + Padding * 2.0;
			const FVector2D TopLeft = Screen * PixelsToLocal - BoxSize / 2.0;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(TopLeft)),
				Background, ESlateDrawEffect::None, RoomLabelColor);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
				AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TopLeft + Padding)), Text, LabelFont,
				ESlateDrawEffect::None, FLinearColor::White);
		}
		return LayerId + 1;
	}

private:
	TWeakObjectPtr<UFloor101DocumentSubsystem> Document;
};

void SFloor101Overlay::Construct(const FArguments& InArgs)
{
	Document = InArgs._Document;
	ViewportClient = InArgs._ViewportClient;
	PathText = GetPathOrDefault();

	// Only the panels take the mouse; everything else lets clicks through to the viewport.
	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		[
			SNew(SFloor101RoomLabels)
			.Visibility(EVisibility::HitTestInvisible)
			.Document(Document)
		]
		+ SOverlay::Slot()
		[
			SNew(SConstraintCanvas)
			.Visibility(EVisibility::HitTestInvisible)
			+ SConstraintCanvas::Slot()
			.Anchors(FAnchors(0.0f, 0.0f))
			.AutoSize(true)
			.Offset_Lambda([this]()
			{
				return FMargin(CursorPosition.X + CursorLabelOffset.X, CursorPosition.Y + CursorLabelOffset.Y, 0.0f, 0.0f);
			})
			[
				MakeCursorLabel()
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(10.0f)
		[
			MakeToolbar()
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(10.0f)
		[
			MakePropertiesPanel()
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		[
			MakeStatusBar()
		]
	];
}

void SFloor101Overlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (FSlateApplication::IsInitialized())
	{
		CursorPosition = AllottedGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
	}
}

FReply SFloor101Overlay::Run(TFunctionRef<f101::Status(EditController&)> Action)
{
	if (UFloor101DocumentSubsystem* Subsystem = Document.Get())
	{
		(void)Subsystem->Edit(Action);
	}
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
	return FReply::Handled();
}

const f101::cad::Wall* SFloor101Overlay::GetSelectedWall() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (!Subsystem || Subsystem->GetEditController().mode() != EditMode::Select)
	{
		return nullptr;
	}
	const f101::cad::WallId Selected = Subsystem->GetEditController().selection();
	return Selected.isNil() ? nullptr : Subsystem->GetDocument().project().findWall(Selected);
}

FString SFloor101Overlay::GetPathOrDefault() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	return Subsystem && !Subsystem->GetProjectPath().IsEmpty() ? Subsystem->GetProjectPath() : UFloor101DocumentSubsystem::GetDefaultProjectPath();
}

TSharedRef<SWidget> SFloor101Overlay::MakeToolbar()
{
	auto ModeButton = [this](const FText& Label, const FText& ToolTip, EditMode Mode)
	{
		return SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(ToolTip)
			.ButtonColorAndOpacity_Lambda([this, Mode]()
			{
				const UFloor101DocumentSubsystem* Subsystem = Document.Get();
				return Subsystem && Subsystem->GetEditController().mode() == Mode ? FSlateColor(ActiveButtonColor) : FSlateColor(FLinearColor::White);
			})
			.OnClicked_Lambda([this, Mode]()
			{
				return Run([Mode](EditController& Editor) { Editor.setMode(Mode); return f101::Status::success(); });
			})
			[
				SNew(STextBlock).Text(Label).Font(Font(10, true))
			];
	};

	auto ActionButton = [](const FText& Label, TAttribute<FText> ToolTip, TAttribute<bool> Enabled, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(ToolTip)
			.IsEnabled(Enabled)
			.OnClicked(OnClicked)
			[
				SNew(STextBlock).Text(Label).Font(Font(10))
			];
	};

	auto CanUndo = [this]() { const UFloor101DocumentSubsystem* S = Document.Get(); return S && S->GetDocument().canUndo(); };
	auto CanRedo = [this]() { const UFloor101DocumentSubsystem* S = Document.Get(); return S && S->GetDocument().canRedo(); };

	return Panel(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("Title", "101st Floor")).Font(Font(12, true))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				ModeButton(LOCTEXT("Select", "Seleccionar (V)"), LOCTEXT("SelectTip", "Selecciona muros y arrastra sus extremos"), EditMode::Select)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				ModeButton(LOCTEXT("Wall", "Muro (W)"), LOCTEXT("WallTip", "Dibuja muros: clic, mueve, escribe la longitud, Enter"), EditMode::Wall)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				ModeButton(LOCTEXT("Door", "Puerta (P)"), LOCTEXT("DoorTip", "Coloca una puerta de 90 × 210 cm: clic sobre un muro"), EditMode::Door)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				ModeButton(LOCTEXT("Window", "Ventana (O)"), LOCTEXT("WindowTip", "Coloca una ventana de 120 × 120 cm a 90 cm del suelo: clic sobre un muro"), EditMode::Window)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 2.0f, 2.0f, 2.0f)
			[
				ActionButton(LOCTEXT("Undo", "Deshacer"),
					TAttribute<FText>::CreateLambda([this]()
					{
						const UFloor101DocumentSubsystem* S = Document.Get();
						return FText::Format(LOCTEXT("UndoTip", "Deshacer: {0} (Ctrl+Z)"),
							S ? FText::FromString(UTF8_TO_TCHAR(std::string(S->GetDocument().undoLabel()).c_str())) : FText::GetEmpty());
					}),
					TAttribute<bool>::CreateLambda(CanUndo),
					FOnClicked::CreateLambda([this]() { return Run([](EditController& Editor) { return Editor.undo(); }); }))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				ActionButton(LOCTEXT("Redo", "Rehacer"),
					TAttribute<FText>::CreateLambda([this]()
					{
						const UFloor101DocumentSubsystem* S = Document.Get();
						return FText::Format(LOCTEXT("RedoTip", "Rehacer: {0} (Ctrl+Y)"),
							S ? FText::FromString(UTF8_TO_TCHAR(std::string(S->GetDocument().redoLabel()).c_str())) : FText::GetEmpty());
					}),
					TAttribute<bool>::CreateLambda(CanRedo),
					FOnClicked::CreateLambda([this]() { return Run([](EditController& Editor) { return Editor.redo(); }); }))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 2.0f, 2.0f, 2.0f)
			[
				ActionButton(LOCTEXT("Frame", "Encuadrar (F)"), LOCTEXT("FrameTip", "Encuadra todos los muros"), true,
					FOnClicked::CreateLambda([this]()
					{
						if (UFloor101GameViewportClient* Client = ViewportClient.Get())
						{
							Client->FrameProject();
						}
						FSlateApplication::Get().SetAllUserFocusToGameViewport();
						return FReply::Handled();
					}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				ActionButton(LOCTEXT("TopView", "Planta (T)"), LOCTEXT("TopViewTip", "Vista desde arriba, con el norte hacia arriba"), true,
					FOnClicked::CreateLambda([this]()
					{
						if (UFloor101GameViewportClient* Client = ViewportClient.Get())
						{
							Client->ShowTopView();
						}
						FSlateApplication::Get().SetAllUserFocusToGameViewport();
						return FReply::Handled();
					}))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("File", "Archivo")).Font(Font(10))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(420.0f)
				[
					SNew(SEditableTextBox)
					.Font(Font(9))
					.Text_Lambda([this]() { return FText::FromString(PathText); })
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type Commit)
					{
						if (Commit != ETextCommit::OnCleared)
						{
							PathText = Text.ToString().TrimStartAndEnd();
						}
						if (Commit == ETextCommit::OnEnter)
						{
							FSlateApplication::Get().SetAllUserFocusToGameViewport();
						}
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 2.0f, 0.0f)
			[
				ActionButton(LOCTEXT("Save", "Guardar"), LOCTEXT("SaveTip", "Guarda project.json en la ruta indicada (Ctrl+S)"), true,
					FOnClicked::CreateLambda([this]()
					{
						if (UFloor101DocumentSubsystem* Subsystem = Document.Get())
						{
							(void)Subsystem->SaveProject(PathText);
						}
						FSlateApplication::Get().SetAllUserFocusToGameViewport();
						return FReply::Handled();
					}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
			[
				ActionButton(LOCTEXT("Open", "Abrir"), LOCTEXT("OpenTip", "Abre el project.json de la ruta indicada (Ctrl+O)"), true,
					FOnClicked::CreateLambda([this]()
					{
						if (UFloor101DocumentSubsystem* Subsystem = Document.Get())
						{
							(void)Subsystem->OpenProject(PathText);
						}
						FSlateApplication::Get().SetAllUserFocusToGameViewport();
						return FReply::Handled();
					}))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			MakeLevelRow()
		]);
}

TSharedRef<SWidget> SFloor101Overlay::MakeLevelRow()
{
	auto LevelButton = [](const FText& Label, const FText& ToolTip, TAttribute<bool> Enabled, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(ToolTip)
			.IsEnabled(Enabled)
			.OnClicked(OnClicked)
			[
				SNew(STextBlock).Text(Label).Font(Font(10))
			];
	};
	auto ActiveIndex = [this]() -> std::size_t
	{
		const UFloor101DocumentSubsystem* S = Document.Get();
		return S ? S->GetDocument().project().levelIndex(S->GetEditController().activeLevel()).value_or(0) : 0;
	};
	auto LevelCount = [this]() -> std::size_t
	{
		const UFloor101DocumentSubsystem* S = Document.Get();
		return S ? S->GetDocument().project().levels.size() : 0;
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("LevelLabel", "Planta")).Font(Font(10))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(170.0f)
			[
				SNew(STextBlock)
				.Font(Font(10, true))
				.Text_Lambda([this]()
				{
					const UFloor101DocumentSubsystem* S = Document.Get();
					if (!S)
					{
						return FText::GetEmpty();
					}
					const f101::cad::Project& Project = S->GetDocument().project();
					const f101::cad::LevelId Level = S->GetEditController().activeLevel();
					return FText::Format(LOCTEXT("ActiveLevel", "{0}  ({1})"), LevelName(Project, Level),
						ElevationText(f101::cad::levelElevationMm(Project, Level)));
				})
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
		[
			LevelButton(LOCTEXT("LevelUp", "▲ Subir"), LOCTEXT("LevelUpTip", "Editar la planta de arriba (RePág)"),
				TAttribute<bool>::CreateLambda([ActiveIndex, LevelCount]() { return ActiveIndex() + 1 < LevelCount(); }),
				FOnClicked::CreateLambda([this]() { return Run([](EditController& Editor) { return Editor.levelUp(); }); }))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
		[
			LevelButton(LOCTEXT("LevelDown", "▼ Bajar"), LOCTEXT("LevelDownTip", "Editar la planta de abajo (AvPág)"),
				TAttribute<bool>::CreateLambda([ActiveIndex]() { return ActiveIndex() > 0; }),
				FOnClicked::CreateLambda([this]() { return Run([](EditController& Editor) { return Editor.levelDown(); }); }))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
		[
			LevelButton(LOCTEXT("AddLevel", "+ Nueva planta"),
				LOCTEXT("AddLevelTip", "Añade una planta encima de todas, igual que la de arriba, y pasa a editarla"), true,
				FOnClicked::CreateLambda([this]() { return Run([](EditController& Editor) { return Editor.addLevel(); }); }))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 2.0f, 0.0f)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("AllLevelsTip", "Muestra todas las plantas con sus forjados; si no, solo la que editas y, atenuadas, las de abajo (L)"))
			.ButtonColorAndOpacity_Lambda([this]()
			{
				const UFloor101DocumentSubsystem* S = Document.Get();
				return S && S->GetEditController().showsAllLevels() ? FSlateColor(ActiveButtonColor) : FSlateColor(FLinearColor::White);
			})
			.OnClicked_Lambda([this]()
			{
				return Run([](EditController& Editor) { Editor.setShowAllLevels(!Editor.showsAllLevels()); return f101::Status::success(); });
			})
			[
				SNew(STextBlock).Text(LOCTEXT("AllLevels", "Ver todas (L)")).Font(Font(10))
			]
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakePropertyRow(const FText& Label, TFunction<TOptional<double>()> Value, FSelectionSetter Setter)
{
	auto CurrentText = [Value]()
	{
		const TOptional<double> Millimeters = Value();
		return Millimeters.IsSet() ? LengthText(Millimeters.GetValue()) : FText::GetEmpty();
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(Label).Font(Font(10))
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(120.0f)
			[
				SNew(SEditableTextBox)
				.Font(Font(10))
				.SelectAllTextWhenFocused(true)
				.Text_Lambda(CurrentText)
				.OnTextCommitted_Lambda([this, CurrentText, Setter](const FText& Text, ETextCommit::Type Commit)
				{
					// Unchanged text is not re-applied: the shown value is rounded to 1 mm and could move a diagonal wall.
					if (Commit != ETextCommit::OnCleared && !Text.EqualTo(CurrentText()))
					{
						const FString Typed = Text.ToString();
						UFloor101DocumentSubsystem* Subsystem = Document.Get();
						if (Subsystem)
						{
							(void)Subsystem->Edit([&Typed, Setter](EditController& Editor)
							{
								return (Editor.*Setter)(TCHAR_TO_UTF8(*Typed));
							});
						}
					}
					if (Commit == ETextCommit::OnEnter)
					{
						FSlateApplication::Get().SetAllUserFocusToGameViewport();
					}
				})
			]
		];
}

std::optional<f101::cad::OpeningRef> SFloor101Overlay::GetSelectedOpening() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (!Subsystem || Subsystem->GetEditController().mode() != EditMode::Select)
	{
		return std::nullopt;
	}
	return Subsystem->GetEditController().selectedOpening();
}

TSharedRef<SWidget> SFloor101Overlay::MakePropertiesPanel()
{
	// Values shown in the rows: the selected wall's or the selected opening's, in mm.
	auto WallValue = [this](double (*Read)(const f101::cad::Wall&))
	{
		return [this, Read]() -> TOptional<double>
		{
			const f101::cad::Wall* Wall = GetSelectedWall();
			return Wall ? TOptional<double>(Read(*Wall)) : TOptional<double>();
		};
	};
	auto OpeningValue = [this](std::int64_t f101::cad::Opening::* Field)
	{
		return [this, Field]() -> TOptional<double>
		{
			const std::optional<f101::cad::OpeningRef> Selected = GetSelectedOpening();
			return Selected ? TOptional<double>(static_cast<double>(Selected->opening.*Field)) : TOptional<double>();
		};
	};
	auto WallVisible = [this]() { return GetSelectedWall() ? EVisibility::Visible : EVisibility::Collapsed; };
	auto OpeningVisible = [this]() { return GetSelectedOpening() ? EVisibility::Visible : EVisibility::Collapsed; };

	return SNew(SBox)
		.WidthOverride(260.0f)
		.Visibility_Lambda([this]()
		{
			return GetSelectedWall() || GetSelectedOpening() || IsLevelPanelShown() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			Panel(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SVerticalBox)
					.Visibility_Lambda(WallVisible)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("SelectedWall", "Muro seleccionado")).Font(Font(11, true))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
					[
						MakePropertyRow(LOCTEXT("Length", "Longitud"),
							WallValue([](const f101::cad::Wall& Wall) { return Wall.lengthMm(); }), &EditController::setSelectionLength)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
					[
						MakePropertyRow(LOCTEXT("Thickness", "Espesor"),
							WallValue([](const f101::cad::Wall& Wall) { return static_cast<double>(Wall.thicknessMm); }), &EditController::setSelectionThickness)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
					[
						MakePropertyRow(LOCTEXT("Height", "Altura"),
							WallValue([](const f101::cad::Wall& Wall) { return static_cast<double>(Wall.heightMm); }), &EditController::setSelectionHeight)
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SVerticalBox)
					.Visibility_Lambda(OpeningVisible)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(STextBlock)
						.Font(Font(11, true))
						.Text_Lambda([this]()
						{
							const std::optional<f101::cad::OpeningRef> Selected = GetSelectedOpening();
							return Selected && Selected->opening.kind == f101::cad::OpeningKind::Window
								? LOCTEXT("SelectedWindow", "Ventana seleccionada") : LOCTEXT("SelectedDoor", "Puerta seleccionada");
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
					[
						MakePropertyRow(LOCTEXT("OpeningWidth", "Ancho"), OpeningValue(&f101::cad::Opening::widthMm), &EditController::setOpeningWidth)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
					[
						MakePropertyRow(LOCTEXT("OpeningHeight", "Alto"), OpeningValue(&f101::cad::Opening::heightMm), &EditController::setOpeningHeight)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
					[
						SNew(SBox)
						.Visibility_Lambda([this]()
						{
							const std::optional<f101::cad::OpeningRef> Selected = GetSelectedOpening();
							return Selected && Selected->opening.kind == f101::cad::OpeningKind::Window ? EVisibility::Visible : EVisibility::Collapsed;
						})
						[
							MakePropertyRow(LOCTEXT("OpeningSill", "Antepecho"), OpeningValue(&f101::cad::Opening::sillMm), &EditController::setOpeningSill)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
					[
						MakePropertyRow(LOCTEXT("OpeningOffset", "Posición"), OpeningValue(&f101::cad::Opening::offsetMm), &EditController::setOpeningOffset)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Font(Font(9))
						.ColorAndOpacity(MutedColor)
						.Text(LOCTEXT("OffsetHint", "Posición: desde el inicio del muro hasta el borde del hueco"))
						.AutoWrapText(true)
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeLevelSection()
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(Font(9))
					.ColorAndOpacity(MutedColor)
					.Text(LOCTEXT("UnitsHint", "Sin unidad = cm. También: 3.55 m, 3550 mm"))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.IsFocusable(false)
					.Visibility_Lambda([this]() { return IsLevelPanelShown() ? EVisibility::Collapsed : EVisibility::Visible; })
					.OnClicked_Lambda([this]() { return Run([](EditController& Editor) { return Editor.deleteSelection(); }); })
					[
						SNew(STextBlock).Text(LOCTEXT("Delete", "Eliminar (Supr)")).Font(Font(10))
					]
				])
		];
}

const f101::cad::Level* SFloor101Overlay::GetActiveLevel() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	return Subsystem ? Subsystem->GetDocument().project().findLevel(Subsystem->GetEditController().activeLevel()) : nullptr;
}

bool SFloor101Overlay::IsLevelPanelShown() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	return Subsystem && Subsystem->GetEditController().mode() == EditMode::Select && !GetSelectedWall() && !GetSelectedOpening() &&
		GetActiveLevel();
}

TSharedRef<SWidget> SFloor101Overlay::MakeLevelSection()
{
	auto LevelValue = [this](std::int64_t f101::cad::Level::* Field)
	{
		return [this, Field]() -> TOptional<double>
		{
			const f101::cad::Level* Level = GetActiveLevel();
			return Level ? TOptional<double>(static_cast<double>(Level->*Field)) : TOptional<double>();
		};
	};

	return SNew(SVerticalBox)
		.Visibility_Lambda([this]() { return IsLevelPanelShown() ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
			.Font(Font(11, true))
			.Text_Lambda([this]()
			{
				const UFloor101DocumentSubsystem* S = Document.Get();
				return S ? FText::Format(LOCTEXT("ActiveLevelTitle", "Planta: {0}"),
					LevelName(S->GetDocument().project(), S->GetEditController().activeLevel())) : FText::GetEmpty();
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("LevelName", "Nombre")).Font(Font(10))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(120.0f)
				[
					SNew(SEditableTextBox)
					.Font(Font(10))
					.SelectAllTextWhenFocused(true)
					.HintText_Lambda([this]()
					{
						const UFloor101DocumentSubsystem* S = Document.Get();
						return S ? LevelName(S->GetDocument().project(), S->GetEditController().activeLevel()) : FText::GetEmpty();
					})
					.Text_Lambda([this]()
					{
						const f101::cad::Level* Level = GetActiveLevel();
						return Level ? FText::FromString(UTF8_TO_TCHAR(Level->name.c_str())) : FText::GetEmpty();
					})
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type Commit)
					{
						const f101::cad::Level* Level = GetActiveLevel();
						const FString Typed = Text.ToString().TrimStartAndEnd();
						if (Commit != ETextCommit::OnCleared && Level && Typed != UTF8_TO_TCHAR(Level->name.c_str()))
						{
							if (UFloor101DocumentSubsystem* Subsystem = Document.Get())
							{
								(void)Subsystem->Edit([&Typed](EditController& Editor) { return Editor.setLevelName(TCHAR_TO_UTF8(*Typed)); });
							}
						}
						if (Commit == ETextCommit::OnEnter)
						{
							FSlateApplication::Get().SetAllUserFocusToGameViewport();
						}
					})
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			MakePropertyRow(LOCTEXT("LevelHeight", "Altura de planta"), LevelValue(&f101::cad::Level::heightMm), &EditController::setLevelHeight)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			MakePropertyRow(LOCTEXT("LevelSlab", "Forjado"), LevelValue(&f101::cad::Level::slabThicknessMm),
				&EditController::setLevelSlabThickness)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Font(Font(9))
			.ColorAndOpacity(MutedColor)
			.AutoWrapText(true)
			.Text_Lambda([this]()
			{
				const UFloor101DocumentSubsystem* S = Document.Get();
				const f101::cad::Level* Level = GetActiveLevel();
				if (!S || !Level)
				{
					return FText::GetEmpty();
				}
				return FText::Format(LOCTEXT("LevelInfo", "Cota {0} · altura libre {1}. Los muros nuevos suben hasta la planta de arriba."),
					ElevationText(f101::cad::levelElevationMm(S->GetDocument().project(), Level->id)), LengthText(static_cast<double>(Level->clearHeightMm())));
			})
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("DeleteLevelTip", "Elimina esta planta y sus muros (se puede deshacer)"))
			.IsEnabled_Lambda([this]()
			{
				const UFloor101DocumentSubsystem* S = Document.Get();
				return S && S->GetDocument().project().levels.size() > 1;
			})
			.OnClicked_Lambda([this]() { return Run([](EditController& Editor) { return Editor.deleteActiveLevel(); }); })
			[
				SNew(STextBlock).Text(LOCTEXT("DeleteLevel", "Eliminar planta")).Font(Font(10))
			]
		];
}

FText SFloor101Overlay::GetHintText() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (!Subsystem)
	{
		return FText::GetEmpty();
	}
	const EditController& Editor = Subsystem->GetEditController();
	if (Editor.mode() == EditMode::Wall)
	{
		return Editor.wallTool().state() == f101::cad::WallTool::State::Drafting
			? LOCTEXT("HintDrafting", "Muro · mueve el cursor o escribe la longitud (sin unidad = cm) · Enter o clic: crear · cerca de un muro se une a él · Esc o clic derecho: terminar")
			: LOCTEXT("HintWallIdle", "Muro · clic: inicio del muro (en un extremo o sobre un muro para unirlo) · Esc o clic derecho: volver a Seleccionar");
	}
	if (Editor.mode() == EditMode::Door || Editor.mode() == EditMode::Window)
	{
		return Editor.mode() == EditMode::Door
			? LOCTEXT("HintDoor", "Puerta · apunta a un muro y haz clic para colocarla · Esc o clic derecho: volver a Seleccionar")
			: LOCTEXT("HintWindow", "Ventana · apunta a un muro y haz clic para colocarla · Esc o clic derecho: volver a Seleccionar");
	}
	if (Editor.isDragging())
	{
		return LOCTEXT("HintDragging", "Moviendo esquina · los muros unidos la siguen · suelta para aplicar · Esc o clic derecho: cancelar");
	}
	if (Editor.selectedOpening())
	{
		return LOCTEXT("HintOpening", "Seleccionar · edita sus medidas en el panel · Supr: eliminar");
	}
	return Editor.selection().isNil()
		? LOCTEXT("HintSelect", "Seleccionar · clic: muro, puerta o ventana · W: muros · P: puertas · O: ventanas · RePág/AvPág: planta")
		: LOCTEXT("HintSelected", "Seleccionar · arrastra un asa amarilla para mover la esquina · Supr: eliminar");
}

TSharedRef<SWidget> SFloor101Overlay::MakeStatusBar()
{
	return Panel(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(Font(10))
			.Text_Lambda([this]() { return GetHintText(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f)
		[
			SNew(STextBlock)
			.Font(Font(10))
			.ColorAndOpacity_Lambda([this]()
			{
				const UFloor101DocumentSubsystem* S = Document.Get();
				return S && S->IsStatusError() ? FSlateColor(ErrorColor) : FSlateColor(FLinearColor::White);
			})
			.Text_Lambda([this]() { const UFloor101DocumentSubsystem* S = Document.Get(); return S ? S->GetStatusMessage() : FText::GetEmpty(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(Font(9))
			.ColorAndOpacity(MutedColor)
			.Text_Lambda([this]()
			{
				const UFloor101DocumentSubsystem* S = Document.Get();
				const bool bUnsaved = S && S->HasUnsavedChanges();
				return FText::Format(LOCTEXT("CameraHint", "{0}Clic derecho arrastrando: orbitar · Rueda: zoom · Clic central: desplazar"),
					bUnsaved ? LOCTEXT("Unsaved", "Sin guardar  ·  ") : FText::GetEmpty());
			})
		]);
}

FText SFloor101Overlay::GetCursorText() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (!Subsystem)
	{
		return FText::GetEmpty();
	}
	const EditController& Editor = Subsystem->GetEditController();
	// The wall being drawn, or the selected wall whose corner is being dragged.
	const f101::cad::EditView View = Editor.view();
	const auto Shown = std::find_if(View.previewWalls.begin(), View.previewWalls.end(),
		[&View](const f101::cad::Wall& Wall) { return Wall.id == View.selectedWall; });
	if (Shown == View.previewWalls.end())
	{
		return FText::GetEmpty();
	}
	const FText Length = LengthText(Shown->lengthMm());
	if (Editor.mode() != EditMode::Wall)
	{
		return Length; // endpoint drag
	}
	const f101::cad::WallTool& Tool = Editor.wallTool();
	if (!Tool.typedText().empty())
	{
		return FText::Format(LOCTEXT("CursorTyped", "{0}\nEscrito: {1}"), Length, FText::FromString(UTF8_TO_TCHAR(Tool.typedText().c_str())));
	}
	const FText Snap = SnapText(Tool.snapKind());
	return Snap.IsEmpty() ? Length : FText::Format(LOCTEXT("CursorSnap", "{0}  ({1})"), Length, Snap);
}

TSharedRef<SWidget> SFloor101Overlay::MakeCursorLabel()
{
	auto TypedError = [this]() -> FText
	{
		const UFloor101DocumentSubsystem* S = Document.Get();
		if (!S || S->GetEditController().mode() != EditMode::Wall || S->GetEditController().wallTool().typedTextError().empty())
		{
			return FText::GetEmpty();
		}
		return FText::FromString(UTF8_TO_TCHAR(S->GetEditController().wallTool().typedTextError().c_str()));
	};

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(PanelColor)
		.Padding(FMargin(6.0f, 3.0f))
		.Visibility_Lambda([this, TypedError]()
		{
			return GetCursorText().IsEmpty() && TypedError().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(12, true)).Text_Lambda([this]() { return GetCursorText(); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Font(Font(9))
				.ColorAndOpacity(ErrorColor)
				.Visibility_Lambda([TypedError]() { return TypedError().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				.Text_Lambda(TypedError)
			]
		];
}

#undef LOCTEXT_NAMESPACE
