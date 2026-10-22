#include "SFloor101Overlay.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "F101/Assets/Library.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/Units.h"
#include "F101/Drawing/PlanDrawing.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101Display.h"
#include "Floor101EditingPawn.h"
#include "Floor101FileActions.h"
#include "Floor101FileDialog.h"
#include "Floor101FinishLibrary.h"
#include "Floor101GameViewportClient.h"
#include "Floor101Hdr.h"
#include "Floor101Names.h"
#include "Floor101PathTracing.h"
#include "Floor101Preferences.h"
#include "Floor101Startup.h"
#include "Floor101.h"
#include "CoreGlobals.h"
#include "HAL/PlatformOutputDevices.h"
#include "HAL/PlatformProcess.h"
#include "Misc/OutputDeviceConsole.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Floor101Catalogue.h"
#include "SFloor101FurnitureThumbnail.h"
#include "Floor101Style.h"
#include "Floor101Ui.h"
#include "SFloor101Icon.h"
#include "InputCoreTypes.h"
#include "Floor101SceneSpace.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SProgressBar.h"
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

	const FLinearColor PanelColor = Floor101::Studio::Palette().Island;
	const FLinearColor ActiveButtonColor = Floor101::Studio::Palette().Brass;
	const FLinearColor ErrorColor = Floor101::Studio::Palette().Danger;
	const FLinearColor MutedColor = Floor101::Studio::Palette().Text3;
	const FVector2D CursorLabelOffset(18.0, 18.0);

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return Floor101::Studio::Font(Size, bBold);
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

	using Floor101::FinishName;
	using Floor101::FurnitureName;

	// The tabs of the furniture palette (ADR-0019).
	FText FurnitureCategoryName(f101::cad::FurnitureCategory Category)
	{
		switch (Category)
		{
		case f101::cad::FurnitureCategory::Beds: return LOCTEXT("CategoryBeds", "Camas");
		case f101::cad::FurnitureCategory::Seating: return LOCTEXT("CategorySeating", "Sillas");
		case f101::cad::FurnitureCategory::Sofas: return LOCTEXT("CategorySofas", "Sofás");
		case f101::cad::FurnitureCategory::Tables: return LOCTEXT("CategoryTables", "Mesas");
		case f101::cad::FurnitureCategory::Storage: return LOCTEXT("CategoryStorage", "Almacenaje");
		case f101::cad::FurnitureCategory::Kitchen: return LOCTEXT("CategoryKitchen", "Cocina");
		case f101::cad::FurnitureCategory::Bathroom: return LOCTEXT("CategoryBathroom", "Baño");
		case f101::cad::FurnitureCategory::Lighting: return LOCTEXT("CategoryLighting", "Luces");
		default: return LOCTEXT("CategoryDecor", "Decoración");
		}
	}

	// "200 × 90 cm" (width by depth).
	FText FootprintText(std::int64_t WidthMm, std::int64_t DepthMm)
	{
		FNumberFormattingOptions Centimetres;
		Centimetres.SetMaximumFractionalDigits(1);
		return FText::Format(LOCTEXT("Footprint", "{0} × {1} cm"), FText::AsNumber(static_cast<double>(WidthMm) / 10.0, &Centimetres),
			FText::AsNumber(static_cast<double>(DepthMm) / 10.0, &Centimetres));
	}

	FLinearColor FinishSwatch(std::string_view Id)
	{
		const f101::cad::FinishInfo* Finish = f101::cad::findFinish(Id);
		return Finish ? FLinearColor::FromSRGBColor(FColor(Finish->color.r, Finish->color.g, Finish->color.b)) : Floor101::Studio::Palette().Invalid;
	}

	// A plan direction as a compass point (plan: +x east, +y north).
	FText CompassPoint(double Dx, double Dy)
	{
		const double Degrees = FMath::RadiansToDegrees(FMath::Atan2(Dy, Dx)); // from east
		static const FText Points[8] = {LOCTEXT("East", "este"), LOCTEXT("NorthEast", "noreste"), LOCTEXT("North", "norte"),
			LOCTEXT("NorthWest", "noroeste"), LOCTEXT("West", "oeste"), LOCTEXT("SouthWest", "suroeste"), LOCTEXT("South", "sur"),
			LOCTEXT("SouthEast", "sureste")};
		const int32 Sector = static_cast<int32>(FMath::RoundToDouble(FMath::Fmod(Degrees + 360.0, 360.0) / 45.0)) % 8;
		return Points[Sector];
	}

	// Where a wall face looks ("Cara norte").
	FText FaceDirection(const f101::cad::Wall& Wall, f101::cad::WallSide Side)
	{
		const double Ax = static_cast<double>(Wall.end.x_mm - Wall.start.x_mm);
		const double Ay = static_cast<double>(Wall.end.y_mm - Wall.start.y_mm);
		const double Sign = Side == f101::cad::WallSide::Left ? 1.0 : -1.0;
		return FText::Format(LOCTEXT("Face", "Cara {0}"), CompassPoint(-Ay * Sign, Ax * Sign)); // the face normal
	}

	// Which way an end of an opening lies along its wall ("Lado oeste").
	FText EndDirection(const f101::cad::Wall& Wall, f101::cad::OpeningEnd End)
	{
		const double Sign = End == f101::cad::OpeningEnd::End ? 1.0 : -1.0;
		return FText::Format(LOCTEXT("Side", "Lado {0}"), CompassPoint(static_cast<double>(Wall.end.x_mm - Wall.start.x_mm) * Sign,
			static_cast<double>(Wall.end.y_mm - Wall.start.y_mm) * Sign));
	}

	// How a door or window opens (ADR-0014), in the words used for each kind.
	FText OperationName(f101::cad::OpeningKind Kind, f101::cad::OpeningOperation Operation)
	{
		const bool bDoor = Kind == f101::cad::OpeningKind::Door;
		switch (Operation)
		{
		case f101::cad::OpeningOperation::DoubleHinged:
			return bDoor ? LOCTEXT("DoorDouble", "Abatible de dos hojas") : LOCTEXT("WindowDouble", "Practicable de dos hojas");
		case f101::cad::OpeningOperation::Sliding:
			return bDoor ? LOCTEXT("DoorSliding", "Corredera empotrada") : LOCTEXT("WindowSliding", "Corredera");
		case f101::cad::OpeningOperation::Fixed:
			return LOCTEXT("WindowFixed", "Fija");
		default:
			return bDoor ? LOCTEXT("DoorHinged", "Abatible") : LOCTEXT("WindowHinged", "Practicable");
		}
	}

	using Floor101::LevelName;

	// "+2,80 m" (elevation in metres, Spanish notation as on the 2D plan).
	FText ElevationText(std::int64_t Millimeters)
	{
		return FText::FromString(Floor101::Studio::Elevation(Millimeters) + TEXT(" m"));
	}

	TSharedRef<SWidget> Panel(const TSharedRef<SWidget>& Content)
	{
		const auto& P = Floor101::Studio::Palette();
		return SNew(SBorder)
			.BorderImage(Floor101::Studio::Box(P.Island, Floor101::Studio::Size::RadiusIsland, P.Line, 1.0f))
			.ForegroundColor(P.Text1)
			.Padding(FMargin(14.0f, 12.0f))
			[
				Content
			];
	}

	// A group of the inspector: its heading and its rows.
	TSharedRef<SWidget> Group(const FText& Title, const TSharedRef<SWidget>& Rows, bool bDivider = true)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Floor101::Ui::Section(Title, bDivider)]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 12.0f)[Rows];
	}

	// A setting of Options: its name and what it does on the left, its control on the right.
	TSharedRef<SWidget> SettingRow(const FText& Title, TAttribute<FText> Description, const TSharedRef<SWidget>& Control)
	{
		const auto& P = Floor101::Studio::Palette();
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 20.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[SNew(STextBlock).Text(Title).Font(Floor101::Studio::Text(Floor101::Studio::EText::BodyStrong)).ColorAndOpacity(P.Text1)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(Description).Font(Floor101::Studio::Text(Floor101::Studio::EText::Help)).ColorAndOpacity(P.Text3).AutoWrapText(true)
						.Visibility_Lambda([Description]() { return Description.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Control]
			]
			+ SVerticalBox::Slot().AutoHeight()[Floor101::Ui::Divider()];
	}

	// Rows of the inspector, 6 units apart.
	TSharedRef<SVerticalBox> Stack(std::initializer_list<TSharedRef<SWidget>> Rows)
	{
		TSharedRef<SVerticalBox> Result = SNew(SVerticalBox);
		for (const TSharedRef<SWidget>& Row : Rows)
		{
			Result->AddSlot().AutoHeight().Padding(0.0f, Result->NumSlots() > 0 ? 6.0f : 0.0f, 0.0f, 0.0f)[Row];
		}
		return Result;
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

		// A chip per room: its name, and its area in the figures of the plan. The selected room is outlined in brass.
		const auto& P = Floor101::Studio::Palette();
		const FSlateFontInfo NameFont = Floor101::Studio::Text(Floor101::Studio::EText::Button);
		const FSlateFontInfo AreaFont = Floor101::Studio::Text(Floor101::Studio::EText::NumericSmall);
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FSlateBrush* Chip = Floor101::Studio::Box(P.Glass, 7.0f, P.GlassEdge, 1.0f);
		const FSlateBrush* Chosen = Floor101::Studio::Box(P.Glass, 7.0f, P.Brass, 1.0f);
		const FString Selected = UTF8_TO_TCHAR(Subsystem->GetEditController().selectedRoom().c_str());
		const FVector2D Padding(10.0, 6.0);
		const double Gap = 8.0;
		for (const FFloor101RoomView& Room : Subsystem->GetRooms())
		{
			FVector2D Screen;
			if (!Controller->ProjectWorldLocationToScreen(Room.LabelLocation, Screen, true))
			{
				continue; // behind the camera
			}
			const FText Name = FText::FromString(Room.Name);
			const FText Area = FText::FromString(Floor101::Studio::Decimal(Room.AreaM2, 2) + TEXT(" m²"));
			const FVector2D NameSize = Measure->Measure(Name, NameFont);
			const FVector2D AreaSize = Measure->Measure(Area, AreaFont);
			const FVector2D BoxSize(NameSize.X + Gap + AreaSize.X + Padding.X * 2.0, FMath::Max(NameSize.Y, AreaSize.Y) + Padding.Y * 2.0);
			const FVector2D TopLeft = Screen * PixelsToLocal - BoxSize / 2.0;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(TopLeft)),
				!Selected.IsEmpty() && Room.Key == Selected ? Chosen : Chip, ESlateDrawEffect::None, P.Glass * InWidgetStyle.GetColorAndOpacityTint());
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
				AllottedGeometry.ToPaintGeometry(NameSize, FSlateLayoutTransform(TopLeft + FVector2D(Padding.X, (BoxSize.Y - NameSize.Y) / 2.0))), Name, NameFont,
				ESlateDrawEffect::None, P.Text1);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
				AllottedGeometry.ToPaintGeometry(AreaSize, FSlateLayoutTransform(TopLeft + FVector2D(Padding.X + NameSize.X + Gap, (BoxSize.Y - AreaSize.Y) / 2.0))), Area, AreaFont,
				ESlateDrawEffect::None, P.Text2);
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
	if (auto* Client = ViewportClient.Get()) { Client->UnsavedPrompt.BindSP(this, &SFloor101Overlay::ShowUnsavedPrompt); }
	for (const FString& Key : Floor101::Catalogue::Favorites()) { FavoriteKeys.Add(Key); }
	auto PlanShown = [this]() { const auto* S = Document.Get(); return S && S->IsPlanView(); };
	TSharedRef<SWidget> StartupReview = SNullWidget::NullWidget;
	FString ReviewScreen;
	if (FParse::Value(FCommandLine::Get(), TEXT("F101UIScreen="), ReviewScreen) && ReviewScreen == TEXT("startup"))
	{
		StartupReview = Floor101::Startup::MakeLoadingWidget();
	}
	const float Gutter = Floor101::Studio::Size::Gutter;
	ChildSlot
	[
		SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		[
			SNew(SFloor101PlanView)
			.Visibility_Lambda([PlanShown]() { return PlanShown() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.Drawing([this]() -> const f101::drawing::PlanDrawing* { const auto* S = Document.Get(); return S ? &S->GetPlanDrawing() : nullptr; })
			.Mapping([this](const FGeometry& Geometry) { return GetPlanMapping(Geometry); })
		]
		+ SOverlay::Slot()
		[
			SNew(SFloor101RoomLabels).Document(Document)
			.Visibility_Lambda([this, PlanShown]() { return PlanShown() || !IsDesignMode() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		]
		+ SOverlay::Slot()
		[
			// Beside the pointer while drawing; centred above the selected wall, like a dimension.
			SNew(SConstraintCanvas).Visibility_Lambda([this]() { return IsDesignMode() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SConstraintCanvas::Slot().Anchors(FAnchors(0.0f, 0.0f)).AutoSize(true)
			.Alignment_Lambda([this]() { return bCursorAnchored ? FVector2D(0.5, 1.0) : FVector2D(0.0, 0.0); })
			.Offset_Lambda([this]()
			{
				if (bCursorAnchored)
				{
					return FMargin(FMath::Clamp(CursorPosition.X, 200.0, FMath::Max(200.0, ViewSize.X - 200.0)),
						FMath::Clamp(CursorPosition.Y - 6.0, 140.0, FMath::Max(140.0, ViewSize.Y - 80.0)), 0.0f, 0.0f);
				}
				return FMargin(FMath::Clamp(CursorPosition.X + CursorLabelOffset.X, 70.0, FMath::Max(70.0, ViewSize.X - 370.0)),
					FMath::Clamp(CursorPosition.Y + CursorLabelOffset.Y, 120.0, FMath::Max(120.0, ViewSize.Y - 120.0)), 0.0f, 0.0f);
			})
			[MakeCursorLabel()]
		]
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox).Visibility(EVisibility::SelfHitTestInvisible)
			+ SVerticalBox::Slot().AutoHeight()[MakeToolbar()]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				// Islands float over the view, 12 units from its edges (Cota Cero).
				SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(Gutter)[MakeToolRail()]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(Gutter)[MakeLevelIsland()]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(Gutter)[MakeViewBar()]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(Gutter)
				[
					SNew(SBox).Visibility_Lambda([this]() { return IsDesignMode() && IsInspectorDocked() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
					.MaxDesiredHeight_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Max(160.0, ViewSize.Y - Floor101::Studio::Size::Bar - 4.0 * Floor101::Studio::Size::Gutter - 40.0))); })
					[Floor101::Ui::Island(MakePropertiesPanel(), FMargin(0.0f))]
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(Gutter)
				[
					SNew(SBox).Visibility_Lambda([this]() { return IsPhotoMode() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
					.MaxDesiredHeight_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Max(160.0, ViewSize.Y - Floor101::Studio::Size::Bar - 4.0 * Floor101::Studio::Size::Gutter - 40.0))); })
					[Floor101::Ui::Island(SNew(SScrollBox).ScrollBarStyle(&Floor101::Studio::ScrollBar()) + SScrollBox::Slot()[MakePhotoPanel()], FMargin(0.0f))]
				]
				+ SOverlay::Slot().VAlign(VAlign_Bottom).Padding(Gutter)
				[
					// Hint centred, status on the right; the empty sides let clicks through to the view and the levels.
					SNew(SHorizontalBox).Visibility(EVisibility::SelfHitTestInvisible)
					+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SBox).Visibility(EVisibility::HitTestInvisible)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[MakeHintBar()]
					+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(Gutter, 0.0f, 0.0f, 0.0f)[MakeStatusIsland()]
				]
			]
		]
		+ SOverlay::Slot()[MakeHomePanel()]
		+ SOverlay::Slot()[MakeOptionsPanel()]
		+ SOverlay::Slot()[MakeUnsavedPanel()]
		+ SOverlay::Slot()[StartupReview]
	];
}

TOptional<FFloor101PlanMapping> SFloor101Overlay::GetPlanMapping(const FGeometry& Geometry) const
{
	// The orthographic plan camera maps the plan without rotation: two projected points give the scale and the origin.
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	const UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
	if (!Player || !Player->ViewportClient)
	{
		return {};
	}
	FVector2D ViewportPixels;
	Player->ViewportClient->GetViewportSize(ViewportPixels);
	if (ViewportPixels.X <= 0.0 || ViewportPixels.Y <= 0.0)
	{
		return {};
	}
	const double Z = static_cast<double>(f101::cad::levelElevationMm(Subsystem->GetDocument().project(), Subsystem->GetEditController().activeLevel()));
	FVector2D Origin;
	FVector2D East;
	if (!Controller->ProjectWorldLocationToScreen(FVector(Floor101::Conversion::ToUnrealPosition({0.0, 0.0, Z})), Origin, true) ||
		!Controller->ProjectWorldLocationToScreen(FVector(Floor101::Conversion::ToUnrealPosition({1000.0, 0.0, Z})), East, true))
	{
		return {};
	}
	const FVector2D PixelsToLocal = Geometry.GetLocalSize() / ViewportPixels;
	FFloor101PlanMapping Result;
	Result.Origin = Origin * PixelsToLocal;
	Result.Scale = (East.X - Origin.X) * PixelsToLocal.X / 1000.0;
	return Result.Scale > 0.0 ? TOptional<FFloor101PlanMapping>(Result) : TOptional<FFloor101PlanMapping>();
}

void SFloor101Overlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	ViewSize = AllottedGeometry.GetLocalSize();
	if (auto* S = Document.Get(); S && S->IsPlanView())
	{
		if (const auto Map = GetPlanMapping(AllottedGeometry); Map.IsSet())
			S->SetDimensionPickTolerance(8.0 / Map->Scale);
	}
	if (FSlateApplication::IsInitialized())
	{
		CursorPosition = AllottedGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
	}
	bCursorAnchored = false;
	if (const auto* Selected = GetSelectedWall(); Selected && Document.IsValid() && Document->GetEditController().view().previewWalls.empty())
	{
		if (auto* Controller = Document->GetWorld()->GetFirstPlayerController())
		{
			const double Elevation = static_cast<double>(f101::cad::levelElevationMm(Document->GetDocument().project(), Selected->level));
			FVector2D Pixel;
			const auto Position = Floor101::Conversion::ToUnrealPosition({(static_cast<double>(Selected->start.x_mm) + Selected->end.x_mm) * 0.5,
				(static_cast<double>(Selected->start.y_mm) + Selected->end.y_mm) * 0.5, Elevation + Selected->heightMm});
			if (Controller->ProjectWorldLocationToScreen(FVector(Position), Pixel, true))
			{
				FVector2D ViewportPixels;
				if (auto* Player = Controller->GetLocalPlayer(); Player && Player->ViewportClient)
				{
					Player->ViewportClient->GetViewportSize(ViewportPixels);
					if (ViewportPixels.X > 0 && ViewportPixels.Y > 0) { CursorPosition = Pixel * ViewSize / ViewportPixels; bCursorAnchored = true; }
				}
			}
		}
	}
	// Imported assets (ADR-0017): the lists follow the library (import, rename, a model turned, undo, another project).
	FString Signature;
	if (const f101::cad::Project* Project = CurrentProject())
	{
		for (const f101::cad::Asset& Asset : Project->assets)
		{
			// Everything a row shows.
			Signature += UTF8_TO_TCHAR((f101::cad::toString(Asset.id) + Asset.name + "|" + std::to_string(Asset.model.widthMm) + "x" +
				std::to_string(Asset.model.depthMm) + "|" + std::to_string(Asset.texture.pixelWidth) + "x" +
				std::to_string(Asset.texture.pixelHeight) + ";").c_str());
		}
	}
	if (Signature != ImportedSignature)
	{
		ImportedSignature = Signature;
		RebuildImportedLists();
	}
	// The levels island follows the levels: added, removed, renamed, taller or lower (undo included).
	FString Levels;
	if (const f101::cad::Project* Project = CurrentProject())
	{
		for (const f101::cad::Level& Level : Project->levels)
		{
			Levels += UTF8_TO_TCHAR((f101::cad::toString(Level.id) + "|" + Level.name + "|" + std::to_string(f101::cad::levelElevationMm(*Project, Level.id)) + ";").c_str());
		}
	}
	if (Levels != LevelSignature)
	{
		LevelSignature = Levels;
		RefreshLevelIsland();
	}
	// The hint pill: the keys of Select while nothing is selected, otherwise the hint of the tool or the selection.
	bool bDefaultHint = false;
	if (const UFloor101DocumentSubsystem* S = Document.Get(); S && IsDesignMode())
	{
		const EditController& Editor = S->GetEditController();
		bDefaultHint = Editor.mode() == EditMode::Select && Editor.selection().isNil() && !Editor.selectedOpening() && !Editor.selectedFurniture() &&
			!Editor.selectedColumn() && !Editor.selectedServiceRun() && Editor.selectedRoom().empty() && !Editor.selectedDimension() && !Editor.isDragging();
	}
	const FText Hint = bDefaultHint ? FText::GetEmpty() : GetHintText();
	const FString HintKey = bDefaultHint ? FString(TEXT("\x1F")) : Hint.ToString();
	if (HintHost && HintKey != ShownHint)
	{
		ShownHint = HintKey;
		HintHost->SetContent(MakeHintContent(Hint, bDefaultHint));
	}
}

FReply SFloor101Overlay::Run(TFunctionRef<f101::Status(EditController&)> Action)
{
	if (UFloor101GameViewportClient* Client = ViewportClient.Get())
	{
		Client->SetNavigationMode(false);
	}
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

bool SFloor101Overlay::IsDesignMode() const
{
	const UFloor101GameViewportClient* Client = ViewportClient.Get();
	return !IsPhotoMode() && !(Client && Client->IsNavigationMode());
}

void SFloor101Overlay::OpenOptions()
{
	bOptionsOpen = true;
	RefreshModalInput();
}

void SFloor101Overlay::CloseOptions()
{
	bOptionsOpen = false;
	RefreshModalInput();
}

FReply SFloor101Overlay::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (bUnsavedPromptOpen)
	{
		if (Event.GetKey() == EKeys::Escape) { ResolveUnsavedPrompt(EAppReturnType::Cancel); }
		return FReply::Handled();
	}
	if (bOptionsOpen && Event.GetKey() == EKeys::Escape) { CloseOptions(); return FReply::Handled(); }
	if (bHomeOpen && Event.GetKey() == EKeys::Escape) { CloseHome(); return FReply::Handled(); }
	return SCompoundWidget::OnKeyDown(Geometry, Event);
}

TSharedRef<SWidget> SFloor101Overlay::MakeOptionsPanel()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	const Studio::FPalette& P = Studio::Palette();
	const FText Pages[] = {LOCTEXT("StudioDisplayPage", "Pantalla"), LOCTEXT("StudioGraphicsPage", "Gráficos"), LOCTEXT("StudioControlsPage", "Controles"), LOCTEXT("StudioDiagnosticsPage", "Diagnóstico")};
	const EFloor101Icon PageIcons[] = {EFloor101Icon::Monitor, EFloor101Icon::Sliders, EFloor101Icon::Keys, EFloor101Icon::Pulse};
	auto Wide = [this]() { return ViewSize.X >= 640.0; };

	// Pages: a column on the left, or a segmented control above the content in narrow windows.
	TSharedRef<SVerticalBox> Nav = SNew(SVerticalBox);
	TArray<Ui::FSegment> PageSegments;
	for (int32 I = 0; I < UE_ARRAY_COUNT(Pages); ++I)
	{
		auto Active = [this, I]() { return OptionsPage == I; };
		Nav->AddSlot().AutoHeight().Padding(0.0f, I > 0 ? 2.0f : 0.0f, 0.0f, 0.0f)
		[
			Ui::Selectable(SNew(SBox).HeightOverride(36.0f).Padding(FMargin(10.0f, 0.0f)).VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[Ui::Icon(PageIcons[I], 16.0f, TAttribute<FSlateColor>::CreateLambda([Active]() { return Active() ? FSlateColor(Floor101::Studio::Palette().Brass) : FSlateColor::UseForeground(); }))]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[SNew(STextBlock).Text(Pages[I]).Font(Studio::Text(Studio::EText::Body)).ColorAndOpacity(FSlateColor::UseForeground())]
				],
				TAttribute<bool>::CreateLambda(Active), [this, I]() { OptionsPage = I; })
		];
		PageSegments.Add({PageIcons[I], Pages[I], FText::GetEmpty(), Pages[I], Active, [this, I]() { OptionsPage = I; }});
	}
	auto Page = [this](int32 Index, const TSharedRef<SWidget>& Content)
	{
		return SNew(SBox).Visibility_Lambda([this, Index]() { return OptionsPage == Index ? EVisibility::Visible : EVisibility::Collapsed; })[Content];
	};

	// 1 · Display: the same calls the buttons made, now as a selector and switches that show the state.
	const TSharedRef<SWidget> Resolution = Ui::MenuButton(
		SNew(SBox).WidthOverride(200.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Studio::Text(Studio::EText::Numeric)).ColorAndOpacity(P.Text1)
				.Text_Lambda([this]()
				{
					const Floor101::Display::FStatus State = Floor101::Display::Query(ViewportClient.Get());
					return FText::FromString(FString::Printf(TEXT("%d × %d%s"), State.Image.X, State.Image.Y, State.Image == State.Native ? TEXT(" (nativa)") : TEXT("")));
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Ui::Icon(EFloor101Icon::ChevronDown, 16.0f, FSlateColor(P.Text3))]
		],
		[this]() -> TSharedRef<SWidget>
		{
			TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
			for (const FIntPoint Size : Floor101::Display::Resolutions(ViewportClient.Get()))
			{
				const FText Label = FText::FromString(FString::Printf(TEXT("%d × %d%s"), Size.X, Size.Y,
					Size == Floor101::Display::Query(ViewportClient.Get()).Native ? TEXT(" (nativa)") : TEXT("")));
				Menu->AddSlot().AutoHeight()[Ui::MenuItem({}, Label, [this, Size]()
				{
					Floor101::Display::SetResolution(ViewportClient.Get(), Size);
					FSlateApplication::Get().SetAllUserFocusToGameViewport();
				}, FText::GetEmpty(), FText::GetEmpty(), [this, Size]() { return Floor101::Display::Query(ViewportClient.Get()).Image == Size; })];
			}
			return SNew(SBox).WidthOverride(240.0f).MaxDesiredHeight(360.0f)[SNew(SScrollBox).ScrollBarStyle(&Floor101::Studio::ScrollBar()) + SScrollBox::Slot()[Menu]];
		},
		LOCTEXT("ResolutionTip", "Elige una resolución admitida por este monitor. La opción nativa coincide con los píxeles de la pantalla; el render se mantiene al 100 %."),
		Studio::EButton::Field, FMargin(10.0f, 0.0f), 30.0f);

	const TSharedRef<SWidget> Advanced = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f).HAlign(HAlign_Left)
		[
			Ui::Selectable(SNew(SBox).HeightOverride(30.0f).Padding(FMargin(6.0f, 0.0f, 10.0f, 0.0f)).VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[SNew(SFloor101Icon).Size(16.0f).Icon_Lambda([this]() { return bDisplayAdvanced ? EFloor101Icon::ChevronDown : EFloor101Icon::ChevronRight; })]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[SNew(STextBlock).Text(LOCTEXT("StudioAdvancedDisplay", "Información avanzada")).Font(Studio::Text(Studio::EText::Button)).ColorAndOpacity(FSlateColor::UseForeground())]
				],
				false, [this]() { bDisplayAdvanced = !bDisplayAdvanced; })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SVerticalBox).Visibility_Lambda([this]() { return bDisplayAdvanced ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Studio::Text(Studio::EText::Help)).AutoWrapText(true)
				.Text_Lambda([this]() { return Floor101::Display::Describe(Floor101::Display::Query(ViewportClient.Get())); })
				.ColorAndOpacity_Lambda([this]()
				{
					// Anything but native image and render is worth noticing.
					const Floor101::Display::FStatus State = Floor101::Display::Query(ViewportClient.Get());
					return State.Native == State.Image && (!State.bWorldRendering || State.Image == State.Render)
						? FSlateColor(Floor101::Studio::Palette().Text3) : FSlateColor(Floor101::Studio::Palette().Warn);
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Text3).AutoWrapText(true)
				.Text_Lambda([this]() { return FText::FromString(Floor101::Display::Query(ViewportClient.Get()).MonitorName); })
			]
		];

	const TSharedRef<SWidget> DisplayPage = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[SettingRow(LOCTEXT("ResolutionLabel", "Resolución"), LOCTEXT("ResolutionHelp", "Admitida por este monitor. La nativa coincide con sus píxeles; el render se mantiene al 100 %."), Resolution)]
		+ SVerticalBox::Slot().AutoHeight()[MakeDisplayControls()]
		+ SVerticalBox::Slot().AutoHeight()[Advanced];

	// 2 · Graphics: the three profiles of the quality menu.
	TArray<Ui::FSegment> Profiles;
	const FText Names[] = {LOCTEXT("StudioLightQuality", "Fluida"), LOCTEXT("StudioBalancedQuality", "Equilibrada"), LOCTEXT("StudioHighQuality", "Alta")};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
	{
		Profiles.Add({{}, Names[I], FText::GetEmpty(), FText::GetEmpty(), [I]() { return Floor101::Preferences::GraphicsQuality() == I; },
			[I]() { Floor101::Preferences::SetGraphicsQuality(I); }});
	}
	const TSharedRef<SWidget> GraphicsPage = SettingRow(LOCTEXT("QualityLabel", "Calidad gráfica"),
		LOCTEXT("StudioQualityPolicy", "Los perfiles ajustan detalle y sombras. Conservan Lumen y el render al 100 %; no cambian HDR ni la sincronización vertical. La calidad de la fotografía se elige en Foto."),
		Ui::Segmented(Profiles, true, true));

	// 3 · Controls: speed, and every shortcut drawn as keycaps.
	struct FShortcut
	{
		const TCHAR* Action;
		const TCHAR* Keys; // alternatives separated by " / "
	};
	struct FShortcutGroup
	{
		FText Title;
		TArray<FShortcut> Rows;
	};
	const FShortcutGroup Groups[] = {
		{LOCTEXT("ShortcutsMove", "Moverse"), {{TEXT("Moverse"), TEXT("W+A+S+D")}, {TEXT("Recorrer"), TEXT("N")}, {TEXT("Subir / bajar"), TEXT("E / Q")},
			{TEXT("Más rápido"), TEXT("Shift")}, {TEXT("Velocidad (no al escribir medidas)"), TEXT("1 … 9 / 0")}, {TEXT("Mirar sin moverse"), TEXT("Shift+Clic der.")},
			{TEXT("Orbitar"), TEXT("Clic der.")}, {TEXT("Desplazar la vista"), TEXT("Clic central")}, {TEXT("Acercarse"), TEXT("Rueda")}}},
		{LOCTEXT("ShortcutsTools", "Herramientas"), {{TEXT("Seleccionar"), TEXT("V")}, {TEXT("Pared"), TEXT("B")}, {TEXT("Puerta"), TEXT("P")}, {TEXT("Ventana"), TEXT("O")},
			{TEXT("Columna"), TEXT("C")}, {TEXT("Instalaciones"), TEXT("I")}, {TEXT("Muebles"), TEXT("G")}, {TEXT("Materiales"), TEXT("M")}, {TEXT("Cotas"), TEXT("H")}}},
		{LOCTEXT("ShortcutsFile", "Documento"), {{TEXT("Guardar"), TEXT("Ctrl+S")}, {TEXT("Abrir"), TEXT("Ctrl+O")}, {TEXT("Deshacer"), TEXT("Ctrl+Z")},
			{TEXT("Rehacer"), TEXT("Ctrl+Y / Ctrl+Shift+Z")}}},
		{LOCTEXT("ShortcutsView", "Vista"), {{TEXT("Encuadrar"), TEXT("F")}, {TEXT("Vista superior"), TEXT("T")}, {TEXT("Plano 2D"), TEXT("Tab")}, {TEXT("Foto"), TEXT("R")},
			{TEXT("Cerrar Opciones; al editar, cancelar"), TEXT("Esc")}}},
	};
	TSharedRef<SVerticalBox> Shortcuts = SNew(SVerticalBox);
	for (const FShortcutGroup& Group : Groups)
	{
		Shortcuts->AddSlot().AutoHeight().Padding(0.0f, Shortcuts->NumSlots() > 0 ? 14.0f : 0.0f, 0.0f, 6.0f)
			[SNew(STextBlock).Text(Group.Title.ToUpper()).Font(Studio::Text(Studio::EText::Overline)).ColorAndOpacity(P.Text3)];
		for (const FShortcut& Entry : Group.Rows)
		{
			TSharedRef<SHorizontalBox> Keys = SNew(SHorizontalBox);
			TArray<FString> Alternatives;
			FString(Entry.Keys).ParseIntoArray(Alternatives, TEXT(" / "));
			for (int32 Index = 0; Index < Alternatives.Num(); ++Index)
			{
				if (Index > 0)
				{
					Keys->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)
						[SNew(STextBlock).Text(LOCTEXT("ShortcutOr", "o")).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Text3)];
				}
				Keys->AddSlot().AutoWidth().VAlign(VAlign_Center)[Ui::Shortcut(FText::FromString(Alternatives[Index]))];
			}
			Shortcuts->AddSlot().AutoHeight()
			[
				SNew(SBox).MinDesiredHeight(30.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[SNew(STextBlock).Text(FText::FromString(Entry.Action)).Font(Studio::Text(Studio::EText::Label)).ColorAndOpacity(P.Text2)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Keys]
				]
			];
		}
	}
	const TSharedRef<SWidget> ControlsPage = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[SettingRow(LOCTEXT("SpeedLabelLong", "Velocidad de navegación"), LOCTEXT("SpeedHelp", "Se guarda y no cambia al acercarte. Shift la multiplica por cuatro."), MakeNavigationSpeed())]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)[Shortcuts];

	// 4 · Diagnostics.
	const TSharedRef<SWidget> DiagnosticsPage = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SettingRow(LOCTEXT("DiagnosticMode", "Modo depuración"), LOCTEXT("DiagnosticModeHelp", "Registro detallado de lo que hace el programa."),
				Ui::Switch(TAttribute<bool>::CreateLambda([]() { return Floor101::Diagnostics::IsVerbose(); }),
					[](bool bOn) { Floor101::Preferences::SetDiagnosticMode(bOn); }))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SettingRow(LOCTEXT("DiagnosticWindow", "Ventana de registros"), LOCTEXT("DiagnosticWindowHelp", "Muestra el registro en una ventana aparte mientras trabajas."),
				Ui::Switch(TAttribute<bool>::CreateLambda([]() { return Floor101::Diagnostics::IsWindowVisible(); }),
					[](bool bOn) { Floor101::Preferences::SetShowLogWindow(bOn); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
		[
			Ui::Note(LOCTEXT("DiagnosticHelp", "Los errores se guardan en archivo aunque esta ventana esté oculta. Activa el modo depuración antes de reproducir un problema y revisa el registro. Ambos ajustes se aplican ahora y se recuerdan al reiniciar."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f).HAlign(HAlign_Left)
		[
			Ui::Button(Studio::EButton::Secondary, LOCTEXT("DiagnosticFolder", "Abrir carpeta de registros"),
				[]() { FPlatformProcess::ExploreFolder(*FPaths::GetPath(Floor101::Diagnostics::LogPath())); }, EFloor101Icon::Folder)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Text3).AutoWrapText(true)
			.Text_Lambda([]() { FString Shown = Floor101::Diagnostics::LogPath(); FPaths::MakePlatformFilename(Shown); return FText::FromString(Shown); })
		];

	const TSharedRef<SWidget> Body = SNew(SScrollBox).ScrollBarStyle(&Studio::ScrollBar())
		+ SScrollBox::Slot().Padding(FMargin(24.0f, 8.0f, 24.0f, 20.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Page(0, DisplayPage)]
			+ SVerticalBox::Slot().AutoHeight()[Page(1, GraphicsPage)]
			+ SVerticalBox::Slot().AutoHeight()[Page(2, ControlsPage)]
			+ SVerticalBox::Slot().AutoHeight()[Page(3, DiagnosticsPage)]
		];

	const TSharedRef<SWidget> Dialog = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(24.0f, 16.0f, 16.0f, 14.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("StudioOptionsHeading", "Opciones")).Font(Studio::Text(Studio::EText::Title)).ColorAndOpacity(P.Text1)]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[Ui::IconButton(EFloor101Icon::Close, [this]() { CloseOptions(); }, LOCTEXT("StudioCloseOptions", "Cerrar"), INVTEXT("Esc"))]
		]
		+ SVerticalBox::Slot().AutoHeight()[Ui::Divider()]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(16.0f, 12.0f, 16.0f, 0.0f))
		[SNew(SBox).HAlign(HAlign_Left).Visibility_Lambda([Wide]() { return Wide() ? EVisibility::Collapsed : EVisibility::Visible; })[Ui::Segmented(PageSegments, false)]]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(184.0f).Visibility_Lambda([Wide]() { return Wide() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(12.0f)[Nav]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(1.0f)[SNew(SBorder).BorderImage(Studio::White()).BorderBackgroundColor(P.Line).Padding(0.0f)]]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[Body]
		]
		+ SVerticalBox::Slot().AutoHeight()[Ui::Divider()]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(16.0f, 12.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			+ SHorizontalBox::Slot().AutoWidth()[Ui::Button(Studio::EButton::Primary, LOCTEXT("StudioOptionsDone", "Listo"), [this]() { CloseOptions(); })]
		];

	return SNew(SBorder).BorderImage(Studio::White()).BorderBackgroundColor(P.Scrim).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(16.0f)
		.Visibility_Lambda([this]() { return bOptionsOpen ? EVisibility::Visible : EVisibility::Collapsed; })
		.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
		[
			SNew(SBox).WidthOverride_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Min(760.0, ViewSize.X - 32.0))); })
			.HeightOverride_Lambda([this]() { return FOptionalSize(static_cast<float>(FMath::Min(600.0, ViewSize.Y - 32.0))); })
			[Ui::Island(Dialog, FMargin(0.0f), EVisibility::SelfHitTestInvisible, Studio::Size::RadiusDialog)]
		];
}

void SFloor101Overlay::PrepareReview(const FString& Screen)
{
	if (Screen == TEXT("unsaved"))
	{
		OpenHome();
		if (auto* D = Document.Get())
		{
			(void)D->GetMutableDocument().addWall({0, 0}, {3000, 0}, 120, 2800); // transient capture-only edit, never saved
			FFloor101FileActions::RequestReplace(*D, FSimpleDelegate::CreateSP(this, &SFloor101Overlay::EnterNewProject));
		}
	}
	if (Screen == TEXT("home")) { OpenHome(); }
	if (Screen == TEXT("room-overlap") && Document.IsValid())
	{
		if (auto* C = ViewportClient.Get()) C->SetPlanView(true);
		Run([](EditController& E)
		{
			E.setMode(EditMode::Select);
			const f101::cad::Ray3 Ray{3300, 1300, 10000, 0, 0, -1};
			if (const auto Status = E.pointerPressed(Ray); !Status) return Status;
			(void)E.pointerReleased(Ray);
			return E.setSelectedRoomName("Baño");
		});
		if (auto* C = ViewportClient.Get()) C->FrameProject();
		if (!IsInspectorDocked() && PropertiesMenu) PropertiesMenu->SetIsOpen(true);
	}
	if (Screen == TEXT("diagnostics")) { OpenOptions(); OptionsPage = 3; }
	if (Screen == TEXT("selection") && Document.IsValid() && !Document->GetDocument().project().walls.empty())
	{
		const auto Id = Document->GetDocument().project().walls.front().id;
		Run([Id](EditController& E) { return E.select(Id); });
	}
	if (Screen == TEXT("wall-drag") && Document.IsValid() && !Document->GetDocument().project().walls.empty())
	{
		if (auto* C = ViewportClient.Get()) C->SetPlanView(true);
		const auto Wall = Document->GetDocument().project().walls.front();
		Run([Wall](EditController& E)
		{
			E.setMode(f101::cad::EditMode::Select);
			E.setDimensionsVisible(false);
			const double X = (static_cast<double>(Wall.start.x_mm) + static_cast<double>(Wall.end.x_mm)) / 2.0;
			const double Y = (static_cast<double>(Wall.start.y_mm) + static_cast<double>(Wall.end.y_mm)) / 2.0;
			const f101::cad::Ray3 Ray{X, Y, 10000, 0, 0, -1};
			if (const auto Status = E.pointerPressed(Ray); !Status) return Status;
			const f101::cad::Ray3 Moved{X + 400, Y, 10000, 0, 0, -1};
			E.pointerMoved(Moved);
			const auto Status = E.pointerReleased(Moved);
			E.setDimensionsVisible(true);
			return Status;
		});
		if (auto* C = ViewportClient.Get()) C->FrameProject();
		if (!IsInspectorDocked() && PropertiesMenu) PropertiesMenu->SetIsOpen(true);
	}
	if ((Screen == TEXT("room") || Screen == TEXT("baseboards")) && Document.IsValid() && !Document->GetRooms().IsEmpty())
	{
		const std::string Key = TCHAR_TO_UTF8(*Document->GetRooms()[0].Key);
		Run([Key](EditController& E) { return E.selectRoom(Key); });
		Run([](EditController& E) { return E.setSelectedRoomName("Cocina"); });
		if (Screen == TEXT("baseboards"))
		{
			Run([](EditController& E) { return E.setSelectedRoomBaseboard(true); });
			Run([](EditController& E) { return E.setSelectedBaseboardFinish("wood-walnut"); });
		}

		if (!IsInspectorDocked() && PropertiesMenu) PropertiesMenu->SetIsOpen(true);
	}
	if (Screen == TEXT("dimensions") && Document.IsValid())
	{
		if (auto* C = ViewportClient.Get()) C->SetPlanView(true);
		Run([](EditController& E)
		{
			E.setMode(EditMode::Dimension);
			for (const auto Point : {f101::cad::Point2D{61, 61}, f101::cad::Point2D{4238, 61}, f101::cad::Point2D{1500, 700}})
			{
				const f101::cad::Ray3 Ray{static_cast<double>(Point.x_mm), static_cast<double>(Point.y_mm), 10000, 0, 0, -1};
				if (const auto Status = E.pointerPressed(Ray); !Status) return Status;
				(void)E.pointerReleased(Ray);
			}
			return f101::Status::success();
		});
		if (auto* C = ViewportClient.Get()) C->FrameProject();
		if (!IsInspectorDocked() && PropertiesMenu) PropertiesMenu->SetIsOpen(true);
	}
	if (Screen == TEXT("short-dimensions") && Document.IsValid())
	{
		if (auto* C = ViewportClient.Get()) C->SetPlanView(true);
		Run([](EditController& E)
		{
			E.setMode(f101::cad::EditMode::Dimension);
			for (const auto Point : {f101::cad::Point2D{0, 0}, f101::cad::Point2D{0, 80}, f101::cad::Point2D{360, 40}})
			{
				const f101::cad::Ray3 Ray{static_cast<double>(Point.x_mm), static_cast<double>(Point.y_mm), 10000, 0, 0, -1};
				if (const auto Status = E.pointerPressed(Ray); !Status) return Status;
				(void)E.pointerReleased(Ray);
			}
			return f101::Status::success();
		});
		if (auto* C = ViewportClient.Get()) C->FrameProject();
		if (const auto* Controller = Document->GetWorld()->GetFirstPlayerController())
		{
			if (auto* Pawn = Cast<AFloor101EditingPawn>(Controller->GetPawn())) Pawn->Zoom(2);
		}
		if (!IsInspectorDocked() && PropertiesMenu) PropertiesMenu->SetIsOpen(true);
	}
	if (Screen == TEXT("options")) { OpenOptions(); }
	if (Screen == TEXT("controls")) { OpenOptions(); OptionsPage = 2; }
	if ((Screen.StartsWith(TEXT("doors")) || Screen.StartsWith(TEXT("windows"))) && Document.IsValid())
	{
		const bool bDoors = Screen.StartsWith(TEXT("doors"));
		Run([bDoors](EditController& E)
		{
			E.setMode(bDoors ? EditMode::Door : EditMode::Window);
			return E.setNewOpeningFamily(bDoors ? f101::cad::OpeningKind::Door : f101::cad::OpeningKind::Window,
				bDoors ? "door-glass-grid" : "window-fixed-grid");
		});
		if (Screen.EndsWith(TEXT("search"))) { OpeningQuery = TEXT("retícula"); }
		if (Screen.EndsWith(TEXT("empty"))) { OpeningQuery = TEXT("zzzz"); }
		if (Screen.EndsWith(TEXT("filter"))) { OpeningOperationFilter = static_cast<int32>(f101::cad::OpeningOperation::Sliding); }
		if (!IsInspectorDocked() && PropertiesMenu) { PropertiesMenu->SetIsOpen(true); }
	}
	if (Screen == TEXT("levels") && LevelMenu && IsLevelIslandCompact()) { LevelMenu->SetIsOpen(true); }
	if (Screen == TEXT("document-menu") && DocumentMenu) { DocumentMenu->SetIsOpen(true); }
	if (Screen == TEXT("view-menu") && ViewMenu) { ViewMenu->SetIsOpen(true); }
	if (Screen == TEXT("speed-menu") && SpeedMenu) { SpeedMenu->SetIsOpen(true); }
	if (UFloor101GameViewportClient* Client = ViewportClient.Get())
	{
		if (Screen == TEXT("walk")) { Client->SetNavigationMode(true); }
		if (Screen == TEXT("photo")) { Client->SetPhotoMode(true); Client->SetPhotoSamples(64); }
	}
	const bool bFurniture = Screen == TEXT("furniture") || Screen == TEXT("search") || Screen == TEXT("favorites");
	const bool bMaterials = Screen == TEXT("materials") || Screen == TEXT("materials-search");
	if (bFurniture) { Run([](EditController& E) { E.setMode(EditMode::Furniture); return f101::Status::success(); }); }
	if (bMaterials) { Run([](EditController& E) { E.setMode(EditMode::Paint); return f101::Status::success(); }); }
	if (Screen == TEXT("search")) { FurnitureQuery = TEXT("silla"); }
	if (Screen == TEXT("materials-search")) { MaterialQuery = TEXT("roble"); }
	if (Screen == TEXT("favorites")) { FavoriteKeys.Empty(); FavoriteKeys.Add(TEXT("furniture:chair")); bFurnitureFavoritesOnly = true; }
	if ((bFurniture || bMaterials) && !IsInspectorDocked() && PropertiesMenu) { PropertiesMenu->SetIsOpen(true); }
}

TSharedRef<SWidget> SFloor101Overlay::MakeNetworksRow()
{
	namespace Ui = Floor101::Ui;
	auto Shown = [this](f101::cad::ServiceKind Kind)
	{
		const UFloor101DocumentSubsystem* S = Document.Get();
		return S && S->GetEditController().shownServiceKinds().contains(Kind);
	};
	auto SetAll = [this](bool bShown)
	{
		Run([bShown](EditController& Editor)
		{
			for (const f101::cad::ServiceKindInfo& Info : f101::cad::serviceKinds())
			{
				Editor.setServiceKindShown(Info.kind, bShown);
			}
			return f101::Status::success();
		});
	};
	TSharedRef<SWrapBox> Chips = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.0, 6.0));
	for (const f101::cad::ServiceKindInfo& Info : f101::cad::serviceKinds())
	{
		const f101::cad::ServiceKind Kind = Info.kind;
		Chips->AddSlot()
		[
			Ui::Choice(Floor101::ServiceKindName(Kind), TAttribute<bool>::CreateLambda([Shown, Kind]() { return Shown(Kind); }),
				[this, Shown, Kind]()
				{
					const bool bShow = !Shown(Kind);
					Run([Kind, bShow](EditController& Editor) { Editor.setServiceKindShown(Kind, bShow); return f101::Status::success(); });
				},
				FText::Format(LOCTEXT("NetworkTip", "Ver u ocultar la red de {0}: en 3D, en el plano 2D y en el plano exportado"),
					Floor101::ServiceKindName(Kind).ToLower()),
				MakeSwatch(std::string(Info.finish), 12.0f))
		];
	}
	return SNew(SVerticalBox)
		.Visibility_Lambda([this]()
		{
			const UFloor101DocumentSubsystem* S = Document.Get();
			return S && S->GetEditController().showsServices() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		+ SVerticalBox::Slot().AutoHeight()
		[SNew(STextBlock).Text(LOCTEXT("Networks", "REDES VISIBLES")).Font(Floor101::Studio::Text(Floor101::Studio::EText::Overline)).ColorAndOpacity(Floor101::Studio::Palette().Text3)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[Chips]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[Ui::Button(Floor101::Studio::EButton::Ghost, LOCTEXT("AllNetworks", "Todas"), [SetAll]() { SetAll(true); }, {}, LOCTEXT("AllNetworksTip", "Ver las cuatro redes"),
				FText::GetEmpty(), true, 28.0f)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[Ui::Button(Floor101::Studio::EButton::Ghost, LOCTEXT("NoNetworks", "Ninguna"), [SetAll]() { SetAll(false); }, {},
				LOCTEXT("NoNetworksTip", "Ocultar todas las redes (los muros siguen transparentes; X vuelve a la vista normal)"), FText::GetEmpty(), true, 28.0f)]
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakePlanExportButton(const FString& Extension)
{
	return Floor101::Ui::MenuItem(EFloor101Icon::Plan, FText::Format(LOCTEXT("StudioPlanExport", "Plano {0}…"), FText::FromString(Extension.ToUpper())),
		[this, Extension]()
		{
			if (UFloor101DocumentSubsystem* Subsystem = Document.Get())
			{
				FFloor101FileActions::ExportPlan(*Subsystem, Extension);
			}
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		},
		FText::GetEmpty(),
		TAttribute<FText>::CreateLambda([this, Extension]()
		{
			const UFloor101DocumentSubsystem* S = Document.Get();
			return FText::Format(LOCTEXT("ExportPlanTip", "Elige dónde guardar el plano de esta planta. Nombre propuesto: {0}"),
				S ? FText::FromString(S->GetDefaultPlanPath(Extension)) : FText::GetEmpty());
		}));
}

TSharedRef<SWidget> SFloor101Overlay::MakePropertyRow(const FText& Label, TFunction<TOptional<double>()> Value, FSelectionSetter Setter)
{
	// A length: what is typed follows the wall tool's rules (no unit = cm).
	auto Current = [Value]()
	{
		const TOptional<double> Millimeters = Value();
		return Millimeters.IsSet() ? LengthText(Millimeters.GetValue()) : FText::GetEmpty();
	};
	return Floor101::Ui::Row(Label, Floor101::Ui::Field(TAttribute<FText>::CreateLambda(Current),
		[this, Setter](const FString& Typed)
		{
			if (UFloor101DocumentSubsystem* Subsystem = Document.Get())
			{
				(void)Subsystem->Edit([&Typed, Setter](EditController& Editor) { return (Editor.*Setter)(TCHAR_TO_UTF8(*Typed)); });
			}
		}, true, FText::GetEmpty(), LOCTEXT("UnitsHint", "Sin unidad = cm. También: 3.55 m, 3550 mm")));
}

TSharedRef<SWidget> SFloor101Overlay::MakeTextRow(const FText& Label, TFunction<FText()> CurrentText, FSelectionSetter Setter, bool bNumeric)
{
	return Floor101::Ui::Row(Label, Floor101::Ui::Field(TAttribute<FText>::CreateLambda([CurrentText]() { return CurrentText(); }),
		[this, Setter](const FString& Typed)
		{
			if (UFloor101DocumentSubsystem* Subsystem = Document.Get())
			{
				(void)Subsystem->Edit([&Typed, Setter](EditController& Editor) { return (Editor.*Setter)(TCHAR_TO_UTF8(*Typed)); });
			}
		}, bNumeric));
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

SFloor101Overlay::FInspectorHeading SFloor101Overlay::GetInspectorHeading() const
{
	FInspectorHeading Heading;
	const UFloor101DocumentSubsystem* S = Document.Get();
	if (!S)
	{
		return Heading;
	}
	const f101::cad::Project& Project = S->GetDocument().project();
	const EditController& Editor = S->GetEditController();
	if (const f101::cad::Wall* Wall = GetSelectedWall())
	{
		return {EFloor101Icon::Wall, LOCTEXT("HeadWall", "Muro"), LevelName(Project, Wall->level)};
	}
	if (const std::optional<f101::cad::OpeningRef> Opening = GetSelectedOpening())
	{
		const bool bWindow = Opening->opening.kind == f101::cad::OpeningKind::Window;
		return {bWindow ? EFloor101Icon::Window : EFloor101Icon::Door, bWindow ? LOCTEXT("SelectedWindow", "Ventana") : LOCTEXT("SelectedDoor", "Puerta"),
			OperationName(Opening->opening.kind, Opening->opening.operation)};
	}
	if (const std::optional<f101::cad::Furniture> Piece = GetSelectedFurniture())
	{
		return {EFloor101Icon::Furniture, FurnitureName(Piece->type, &Project), FootprintText(Piece->widthMm, Piece->depthMm)};
	}
	if (const std::optional<f101::cad::Column> Column = GetSelectedColumn())
	{
		return {EFloor101Icon::Column, LOCTEXT("SelectedColumn", "Pilar"), Floor101::ColumnShapeName(Column->shape)};
	}
	if (const std::optional<f101::cad::ServiceRun> SelectedRun = GetSelectedServiceRun())
	{
		return {EFloor101Icon::Services, LOCTEXT("SelectedRun", "Instalación"), Floor101::ServiceKindName(SelectedRun->kind)};
	}
	if (const FFloor101RoomView* Room = GetSelectedRoom())
	{
		return {EFloor101Icon::Plan, FText::FromString(Room->Name), FText::FromString(Floor101::Studio::Decimal(Room->AreaM2, 2) + TEXT(" m²"))};
	}
	if (const std::optional<f101::cad::Dimension> Dimension = GetSelectedDimension())
	{
		return {EFloor101Icon::Dimension, LOCTEXT("SelectedDimension", "Cota"), FText::Format(LOCTEXT("DimensionReal", "{0} · distancia real: {1}"),
			Dimension->automatic ? LOCTEXT("DimensionAuto", "Automática") : LOCTEXT("DimensionManual", "Manual"),
			FText::FromString(UTF8_TO_TCHAR(f101::cad::formatLength((Dimension->end - Dimension->start).lengthMm(), f101::cad::LengthUnit::Meter).c_str())))};
	}
	switch (Editor.mode())
	{
	case EditMode::Dimension:
		return {EFloor101Icon::Dimension, LOCTEXT("DimensionTitle", "Cotas del plano"), LOCTEXT("DimensionSub", "Elige dos puntos y coloca la línea")};
	case EditMode::Door:
	case EditMode::Window:
	{
		const bool bDoor = Editor.mode() == EditMode::Door;
		const f101::cad::Opening& Next = bDoor ? Editor.settings().door : Editor.settings().window;
		const FText Size = FootprintText(Next.widthMm, Next.heightMm);
		return {bDoor ? EFloor101Icon::Door : EFloor101Icon::Window, bDoor ? LOCTEXT("DoorTitle", "Puerta") : LOCTEXT("WindowTitle", "Ventana"),
			FText::Format(LOCTEXT("NextOpening", "{0} · {1}"), OperationName(Next.kind, Next.operation), bDoor ? Size
				: FText::Format(LOCTEXT("NewWindowSize", "{0}, antepecho {1} cm"), Size, FText::AsNumber(static_cast<double>(Next.sillMm) / 10.0)))};
	}
	case EditMode::Column:
		return {EFloor101Icon::Column, LOCTEXT("ColumnTitle", "Pilar"), LOCTEXT("ColumnSub", "Forma y medidas del siguiente")};
	case EditMode::Services:
		return {EFloor101Icon::Services, LOCTEXT("ServicesTitle", "Instalación"), FText::Format(LOCTEXT("ServicesSub", "{0} a {1}"),
			Floor101::ServiceKindName(Editor.settings().serviceKind), LengthText(static_cast<double>(Editor.settings().serviceHeightMm)))};
	case EditMode::Furniture:
		return {EFloor101Icon::Furniture, LOCTEXT("FurnitureTitle", "Muebles"), FText::Format(LOCTEXT("FurnitureChosen", "Elegido: {0}"),
			FurnitureName(Editor.settings().furnitureType, &Project))};
	case EditMode::Paint:
		return {EFloor101Icon::Paint, LOCTEXT("PaintTitle", "Materiales"), FText::Format(LOCTEXT("PaintChosen", "Elegido: {0}"),
			FinishName(Editor.paintFinish(), &Project))};
	default:
		break;
	}
	if (const f101::cad::Level* Level = GetActiveLevel())
	{
		return {EFloor101Icon::Levels, LevelName(Project, Level->id), FText::Format(LOCTEXT("LevelInfoShort", "Cota {0} · altura libre {1}"),
			ElevationText(f101::cad::levelElevationMm(Project, Level->id)), LengthText(static_cast<double>(Level->clearHeightMm())))};
	}
	return Heading;
}

TSharedRef<SWidget> SFloor101Overlay::MakeInspectorHeader()
{
	namespace Studio = Floor101::Studio;
	const Studio::FPalette& P = Studio::Palette();
	return SNew(SBox).Padding(FMargin(16.0f, 14.0f, 16.0f, 12.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
			[
				SNew(SBox).WidthOverride(36.0f).HeightOverride(36.0f)
				[
					SNew(SBorder).BorderImage(Studio::Box(P.BrassSel, 8.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.0f)
					[SNew(SFloor101Icon).Icon_Lambda([this]() { return GetInspectorHeading().Icon; }).Size(20.0f).Color(FSlateColor(P.Brass))]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Font(Studio::Text(Studio::EText::Heading)).ColorAndOpacity(P.Text1).AutoWrapText(true).Text_Lambda([this]() { return GetInspectorHeading().Title; })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Text3).AutoWrapText(true)
					.Text_Lambda([this]() { return GetInspectorHeading().Subtitle; })
					.Visibility_Lambda([this]() { return GetInspectorHeading().Subtitle.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				]
			]
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakePropertiesPanel()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
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

	const TSharedRef<SWidget> WallSection = SNew(SVerticalBox).Visibility_Lambda(WallVisible)
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupGeometry", "Geometría"), Stack({
				MakePropertyRow(LOCTEXT("Length", "Longitud"), WallValue([](const f101::cad::Wall& Wall) { return Wall.lengthMm(); }), &EditController::setSelectionLength),
				MakePropertyRow(LOCTEXT("Thickness", "Espesor"), WallValue([](const f101::cad::Wall& Wall) { return static_cast<double>(Wall.thicknessMm); }), &EditController::setSelectionThickness),
				MakePropertyRow(LOCTEXT("Height", "Altura"), WallValue([](const f101::cad::Wall& Wall) { return static_cast<double>(Wall.heightMm); }), &EditController::setSelectionHeight)}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[Group(LOCTEXT("GroupFinishes", "Acabados"), Stack({MakeWallFaceRow(f101::cad::WallSide::Left), MakeWallFaceRow(f101::cad::WallSide::Right)}))]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
		[Ui::Note(LOCTEXT("WallBodyDragHint", "Arrastra el cuerpo para mover la pared. Las puntas ajustan sus extremos."))];

	const TSharedRef<SWidget> OpeningSection = SNew(SVerticalBox).Visibility_Lambda(OpeningVisible)
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupSize", "Medidas"), Stack({
				MakePropertyRow(LOCTEXT("OpeningWidth", "Ancho"), OpeningValue(&f101::cad::Opening::widthMm), &EditController::setOpeningWidth),
				MakePropertyRow(LOCTEXT("OpeningHeight", "Alto"), OpeningValue(&f101::cad::Opening::heightMm), &EditController::setOpeningHeight),
				SNew(SBox)
				.Visibility_Lambda([this]()
				{
					const std::optional<f101::cad::OpeningRef> Selected = GetSelectedOpening();
					return Selected && Selected->opening.kind == f101::cad::OpeningKind::Window ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[MakePropertyRow(LOCTEXT("OpeningSill", "Antepecho"), OpeningValue(&f101::cad::Opening::sillMm), &EditController::setOpeningSill)],
				MakePropertyRow(LOCTEXT("OpeningOffset", "Posición"), OpeningValue(&f101::cad::Opening::offsetMm), &EditController::setOpeningOffset)}))
		]
		+ SVerticalBox::Slot().AutoHeight()[Group(LOCTEXT("GroupOperation", "Apertura"), MakeOpeningTypeSection())]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupCarpentry", "Carpintería"), Ui::Row(LOCTEXT("OpeningFinish", "Acabado"),
				MakeFinishCombo(f101::cad::Surface::Carpentry,
					[this]() -> FString
					{
						const std::optional<f101::cad::OpeningRef> Selected = GetSelectedOpening();
						const std::string_view Own = Selected ? std::string_view(Selected->opening.finish) : std::string_view();
						return UTF8_TO_TCHAR(std::string(f101::cad::resolveFinish(Own, f101::cad::Surface::Carpentry)).c_str());
					},
					[](EditController& Editor, const std::string& Finish) { return Editor.setOpeningFinish(Finish); })))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupGlazing", "Vidrio"), Stack({
				Ui::Row(LOCTEXT("OpeningGlazing", "Acabado"), MakeFinishCombo(f101::cad::Surface::Glazing,
					[this]() -> FString
					{
						const auto Selected = GetSelectedOpening();
						return UTF8_TO_TCHAR(std::string(f101::cad::resolveFinish(Selected ? std::string_view(Selected->opening.glazing) : std::string_view(), f101::cad::Surface::Glazing)).c_str());
					}, [](EditController& E, const std::string& Finish) { return E.setOpeningGlazing(Finish); })),
				Ui::Note(LOCTEXT("ReflectivePrivacyHint", "El vidrio reflectivo ofrece privacidad cuando hay más luz fuera. De noche puede verse el interior iluminado."))}))
		]
		+ SVerticalBox::Slot().AutoHeight()[Group(LOCTEXT("SelectedOpeningCatalogue", "Diseño"), MakeOpeningCatalogue(true))]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
		[Ui::Note(LOCTEXT("OffsetHint", "Posición: desde el inicio del muro hasta el borde del hueco"))];

	const TSharedRef<SVerticalBox> Sections = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[WallSection]
		+ SVerticalBox::Slot().AutoHeight()[OpeningSection]
		+ SVerticalBox::Slot().AutoHeight()[MakeLevelSection()]
		+ SVerticalBox::Slot().AutoHeight()[MakeDimensionSection()]
		+ SVerticalBox::Slot().AutoHeight()[MakeRoomSection()]
		+ SVerticalBox::Slot().AutoHeight()[MakeOpeningPalette()]
		+ SVerticalBox::Slot().AutoHeight()[MakePaintPalette()]
		+ SVerticalBox::Slot().AutoHeight()[MakeFurnitureSection()]
		+ SVerticalBox::Slot().AutoHeight()[MakeFurniturePalette()]
		+ SVerticalBox::Slot().AutoHeight()[MakeColumnSection()]
		+ SVerticalBox::Slot().AutoHeight()[MakeColumnPalette()]
		+ SVerticalBox::Slot().AutoHeight()[MakeServiceRunSection()]
		+ SVerticalBox::Slot().AutoHeight()[MakeServicesPalette()];

	const TSharedRef<SWidget> Footer = SNew(SVerticalBox)
		.Visibility_Lambda([this]()
		{
			return GetSelectedWall() || GetSelectedOpening() || GetSelectedFurniture() || GetSelectedColumn() || GetSelectedServiceRun() || GetSelectedDimension()
				? EVisibility::Visible : EVisibility::Collapsed;
		})
		+ SVerticalBox::Slot().AutoHeight()[Ui::Divider()]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(8.0f, 8.0f, 14.0f, 8.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[Ui::Button(Studio::EButton::Danger, LOCTEXT("Delete", "Eliminar"), [this]() { Run([](EditController& Editor) { return Editor.deleteSelection(); }); },
				EFloor101Icon::Trash, LOCTEXT("DeleteTip", "Eliminar la selección"), INVTEXT("Supr"), true, 30.0f)]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Ui::Keycap(INVTEXT("Supr"))]
		];

	return SNew(SBox)
		.WidthOverride_Lambda([this]()
		{
			// Wider for two readable catalogue cards per row and the services' height presets.
			const UFloor101DocumentSubsystem* S = Document.Get();
			const EditMode Mode = S ? S->GetEditController().mode() : EditMode::Select;
			const double Width = Mode == EditMode::Door || Mode == EditMode::Window || GetSelectedOpening() ? static_cast<double>(Floor101::Studio::Size::OpeningCatalogue) : Mode == EditMode::Furniture || Mode == EditMode::Paint || Mode == EditMode::Services ? 340.0
				: ViewSize.X >= Floor101::Studio::Breakpoint::DockLabels ? Floor101::Studio::Size::Inspector : Floor101::Studio::Size::InspectorNarrow;
			return FOptionalSize(static_cast<float>(FMath::Min(ViewSize.X - 40.0, Width)));
		})
		.Visibility_Lambda([this]()
		{
			const UFloor101DocumentSubsystem* S = Document.Get();
			const EditMode Mode = S ? S->GetEditController().mode() : EditMode::Select;
			const bool bTool = Mode == EditMode::Paint || Mode == EditMode::Furniture || Mode == EditMode::Door || Mode == EditMode::Window ||
				Mode == EditMode::Column || Mode == EditMode::Services || Mode == EditMode::Dimension;
			if (IsPhotoMode())
			{
				return EVisibility::Collapsed;
			}
			return GetSelectedWall() || GetSelectedOpening() || GetSelectedFurniture() || GetSelectedColumn() || GetSelectedServiceRun() ||
				GetSelectedRoom() || GetSelectedDimension() || IsLevelPanelShown() || bTool
				? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[MakeInspectorHeader()]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[SNew(SScrollBox).ScrollBarStyle(&Studio::ScrollBar()) + SScrollBox::Slot()[Sections]]
			+ SVerticalBox::Slot().AutoHeight()[Footer]
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakeFlipRow(TFunction<FText()> Label, TFunction<FText()> Value,
	f101::Status (f101::cad::EditController::*Flip)())
{
	namespace Studio = Floor101::Studio;
	return Floor101::Ui::Row(TAttribute<FText>::CreateLambda([Label]() { return Label(); }),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[SNew(STextBlock).Font(Studio::Text(Studio::EText::Body)).ColorAndOpacity(Studio::Palette().Text1).Text_Lambda([Value]() { return Value(); })]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[Floor101::Ui::Button(Studio::EButton::Ghost, LOCTEXT("Flip", "Invertir"), [this, Flip]() { Run([Flip](EditController& Editor) { return (Editor.*Flip)(); }); },
			EFloor101Icon::Flip, FText::GetEmpty(), FText::GetEmpty(), true, 28.0f)]);
}

TSharedRef<SWidget> SFloor101Overlay::MakeOpeningTypeSection()
{
	namespace Studio = Floor101::Studio;
	using f101::cad::OpeningOperation;
	if (DoorOperations.IsEmpty())
	{
		for (const OpeningOperation Operation : {OpeningOperation::Hinged, OpeningOperation::DoubleHinged, OpeningOperation::Sliding})
		{
			DoorOperations.Add(MakeShared<OpeningOperation>(Operation));
			WindowOperations.Add(MakeShared<OpeningOperation>(Operation));
		}
		WindowOperations.Add(MakeShared<OpeningOperation>(OpeningOperation::Fixed));
	}
	// The selected opening with its wall, re-read every frame.
	auto Selected = [this]() -> std::optional<std::pair<f101::cad::Wall, f101::cad::Opening>>
	{
		const std::optional<f101::cad::OpeningRef> Opening = GetSelectedOpening();
		const UFloor101DocumentSubsystem* Subsystem = Document.Get();
		const f101::cad::Wall* Wall = Opening && Subsystem ? Subsystem->GetDocument().project().findWall(Opening->wall) : nullptr;
		return Wall ? std::optional<std::pair<f101::cad::Wall, f101::cad::Opening>>(std::pair{*Wall, Opening->opening}) : std::nullopt;
	};
	auto VisibleFor = [Selected](TFunction<bool(const f101::cad::Opening&)> Shown)
	{
		return [Selected, Shown]()
		{
			const auto Current = Selected();
			return Current && Shown(Current->second) ? EVisibility::Visible : EVisibility::Collapsed;
		};
	};
	auto TypeCombo = [this, Selected, VisibleFor](f101::cad::OpeningKind Kind, TArray<TSharedPtr<OpeningOperation>>* Options)
	{
		return SNew(SBox)
			.HeightOverride(Studio::Size::Field)
			.Visibility_Lambda(VisibleFor([Kind](const f101::cad::Opening& Opening) { return Opening.kind == Kind; }))
			[
				SNew(SComboBox<TSharedPtr<OpeningOperation>>).ComboBoxStyle(&Studio::ComboBox()).ItemStyle(&Studio::ListRow()).HasDownArrow(false)
				.IsFocusable(false)
				.OptionsSource(Options)
				.OnGenerateWidget_Lambda([Kind](TSharedPtr<OpeningOperation> Operation)
				{
					return SNew(STextBlock).Text(OperationName(Kind, *Operation)).Font(Studio::Text(Studio::EText::Body)).ColorAndOpacity(Studio::Palette().Text1);
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<OpeningOperation> Operation, ESelectInfo::Type Info)
				{
					if (Operation && Info != ESelectInfo::Direct)
					{
						const OpeningOperation Chosen = *Operation;
						(void)Run([Chosen](EditController& Editor) { return Editor.setOpeningOperation(Chosen); });
					}
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Font(Studio::Text(Studio::EText::Label)).ColorAndOpacity(Studio::Palette().Text1).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						.Text_Lambda([Selected, Kind]()
						{
							const auto Current = Selected();
							return Current ? OperationName(Kind, Current->second.operation) : FText::GetEmpty();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 2.0f, 0.0f)[Floor101::Ui::Icon(EFloor101Icon::ChevronDown, 16.0f, FSlateColor(Studio::Palette().Text3))]
				]
			];
	};

	return Stack({
		Floor101::Ui::Row(LOCTEXT("OpeningType", "Tipo"),
			SNew(SOverlay)
			+ SOverlay::Slot()[TypeCombo(f101::cad::OpeningKind::Door, &DoorOperations)]
			+ SOverlay::Slot()[TypeCombo(f101::cad::OpeningKind::Window, &WindowOperations)]),
		SNew(SBox)
		.Visibility_Lambda(VisibleFor([](const f101::cad::Opening& Opening)
		{
			return Opening.operation == OpeningOperation::Hinged || Opening.operation == OpeningOperation::Sliding;
		}))
		[
			MakeFlipRow(
				[Selected]()
				{
					const auto Current = Selected();
					return Current && Current->second.operation == OpeningOperation::Sliding
						? LOCTEXT("SlidesTowards", "Se abre hacia") : LOCTEXT("Hinges", "Bisagras");
				},
				[Selected]()
				{
					const auto Current = Selected();
					return Current ? EndDirection(Current->first, Current->second.hinge) : FText::GetEmpty();
				},
				&EditController::flipOpeningHinge)
		],
		SNew(SBox)
		.Visibility_Lambda(VisibleFor([](const f101::cad::Opening& Opening)
		{
			return Opening.operation == OpeningOperation::Hinged || Opening.operation == OpeningOperation::DoubleHinged;
		}))
		[
			MakeFlipRow([]() { return LOCTEXT("OpensTo", "Abre hacia"); },
				[Selected]()
				{
					const auto Current = Selected();
					return Current ? FaceDirection(Current->first, Current->second.opensTo) : FText::GetEmpty();
				},
				&EditController::flipOpeningSide)
		],
		SNew(SBox)
		.Visibility_Lambda(VisibleFor([](const f101::cad::Opening& Opening) { return Opening.operation != OpeningOperation::Fixed; }))
		[
			MakeTextRow(LOCTEXT("OpenPercent", "Apertura"),
				[Selected]()
				{
					const auto Current = Selected();
					return Current ? FText::Format(LOCTEXT("Percent", "{0} %"), FText::AsNumber(Current->second.openPercent)) : FText::GetEmpty();
				},
				&EditController::setOpeningOpenPercent)
		]});
}

const f101::cad::Level* SFloor101Overlay::GetActiveLevel() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	return Subsystem ? Subsystem->GetDocument().project().findLevel(Subsystem->GetEditController().activeLevel()) : nullptr;
}

TSharedRef<SWidget> SFloor101Overlay::MakeOpeningPalette()
{
	using f101::cad::OpeningKind;
	auto Template = [this]() -> std::optional<f101::cad::Opening>
	{
		const auto* S = Document.Get();
		if (!S) { return std::nullopt; }
		const auto& Editor = S->GetEditController();
		if (Editor.mode() == EditMode::Door) { return Editor.settings().door; }
		if (Editor.mode() == EditMode::Window) { return Editor.settings().window; }
		return std::nullopt;
	};
	return SNew(SVerticalBox)
		.Visibility_Lambda([Template]() { return Template() ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("OpeningTemplateSize", "Medidas"), Stack({
				MakePropertyRow(LOCTEXT("OpeningTemplateWidth", "Ancho"), [Template]() -> TOptional<double>
					{ const auto O = Template(); return O ? TOptional<double>(static_cast<double>(O->widthMm)) : TOptional<double>(); }, &EditController::setNewOpeningWidth),
				MakePropertyRow(LOCTEXT("OpeningTemplateHeight", "Alto"), [Template]() -> TOptional<double>
					{ const auto O = Template(); return O ? TOptional<double>(static_cast<double>(O->heightMm)) : TOptional<double>(); }, &EditController::setNewOpeningHeight),
				SNew(SBox).Visibility_Lambda([Template]() { const auto O = Template(); return O && O->kind == OpeningKind::Window ? EVisibility::Visible : EVisibility::Collapsed; })
				[MakePropertyRow(LOCTEXT("OpeningTemplateSill", "Antepecho"), [Template]() -> TOptional<double>
					{ const auto O = Template(); return O ? TOptional<double>(static_cast<double>(O->sillMm)) : TOptional<double>(); }, &EditController::setNewOpeningSill)]
			}))
		]
		+ SVerticalBox::Slot().AutoHeight()[Group(LOCTEXT("OpeningCatalogue", "Diseños"), MakeOpeningCatalogue(false))]
		+ SVerticalBox::Slot().AutoHeight().Padding(Floor101::Studio::Size::Gutter)
		[Floor101::Ui::Note(LOCTEXT("OpeningPaletteHint", "Elige un diseño y ajusta sus medidas. Clic en un muro para colocarlo; Espacio invierte las bisagras. Las correderas empotradas necesitan muro libre al lado."))];
}

bool SFloor101Overlay::IsPhotoMode() const
{
	const UFloor101GameViewportClient* Client = ViewportClient.Get();
	return Client && Client->IsPhotoMode();
}

TSharedRef<SWidget> SFloor101Overlay::MakeHdrButton()
{
	// HDR on or off, only where Windows offers it; the explanation says why it is unavailable.
	return SettingRow(LOCTEXT("HDRLabel", "HDR"),
		TAttribute<FText>::CreateLambda([this]() { return Floor101::HDR::Tooltip(Floor101::HDR::Query(ViewportClient.Get())); }),
		Floor101::Ui::Switch(TAttribute<bool>::CreateLambda([this]() { return Floor101::HDR::Query(ViewportClient.Get()).bEnabled; }),
			[this](bool bOn)
			{
				Floor101::HDR::SetEnabled(ViewportClient.Get(), bOn);
				FSlateApplication::Get().SetAllUserFocusToGameViewport();
			},
			TAttribute<bool>::CreateLambda([this]() { return Floor101::HDR::Query(ViewportClient.Get()).bAvailable; })));
}

TSharedRef<SWidget> SFloor101Overlay::MakeVSyncButton()
{
	return SettingRow(LOCTEXT("VSyncLabel", "Sincronización vertical"),
		LOCTEXT("VSyncTip", "Activar sincroniza los fotogramas con el monitor para reducir cortes de imagen. Desactivar permite una frecuencia de fotogramas independiente. Se guarda para la próxima sesión."),
		Floor101::Ui::Switch(TAttribute<bool>::CreateLambda([]() { return Floor101::Display::IsVSyncEnabled(); }),
			[](bool bOn)
			{
				Floor101::Display::SetVSyncEnabled(bOn);
				FSlateApplication::Get().SetAllUserFocusToGameViewport();
			}));
}

TSharedRef<SWidget> SFloor101Overlay::MakeNavigationButton()
{
	namespace Studio = Floor101::Studio;
	// Navigate on or off: a switch shows the state; the change is the same call as N.
	return Floor101::Ui::Row(LOCTEXT("NavigationLabel", "Navegar"),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Floor101::Ui::Keycap(INVTEXT("N"))]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			Floor101::Ui::Switch(TAttribute<bool>::CreateLambda([this]() { const UFloor101GameViewportClient* Client = ViewportClient.Get(); return Client && Client->IsNavigationMode(); }),
				[this](bool bOn)
				{
					if (UFloor101GameViewportClient* Client = ViewportClient.Get())
					{
						Client->SetNavigationMode(bOn);
					}
					FSlateApplication::Get().SetAllUserFocusToGameViewport();
				})
		],
		LOCTEXT("NavigationTip", "N: navegar o volver a editar. WASD: moverse; Q/E: bajar/subir; Shift: más rápido; derecho: mirar en Recorrer; Shift + derecho: mirar también en Diseñar y Foto. WASD funciona también al diseñar, sin activar N."));
}

TSharedRef<SWidget> SFloor101Overlay::MakeDisplayControls(bool bCompact)
{
	(void)bCompact;
	const TSharedRef<SWidget> Fullscreen = SettingRow(LOCTEXT("FullscreenLabel", "Pantalla completa"),
		LOCTEXT("FullscreenTip", "Pantalla completa dedicada con la resolución nativa de este monitor y render al 100 %. F11 vuelve a la ventana anterior."),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)[Floor101::Ui::Keycap(INVTEXT("F11"))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			Floor101::Ui::Switch(TAttribute<bool>::CreateLambda([this]() { return Floor101::Display::Query(ViewportClient.Get()).bFullscreen; }),
				[this](bool bOn)
				{
					if (bOn != Floor101::Display::Query(ViewportClient.Get()).bFullscreen)
					{
						Floor101::Display::ToggleFullscreen(ViewportClient.Get());
					}
					RefreshModalInput();
				},
				TAttribute<bool>::CreateLambda([this]() { return Floor101::Display::Query(ViewportClient.Get()).Native.X > 0; }))
		]);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Fullscreen]
		+ SVerticalBox::Slot().AutoHeight()[MakeVSyncButton()]
		+ SVerticalBox::Slot().AutoHeight()[MakeHdrButton()];
}

TSharedRef<SWidget> SFloor101Overlay::MakePhotoPanel()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	const Studio::FPalette& P = Studio::Palette();
	auto ClientAction = [this](TFunction<void(UFloor101GameViewportClient&)> Action)
	{
		return [this, Action]()
		{
			if (UFloor101GameViewportClient* Client = ViewportClient.Get())
			{
				Action(*Client);
			}
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		};
	};
	// Samples per pixel of each quality: more samples, less noise, longer.
	TArray<Ui::FSegment> Qualities;
	for (const TPair<int32, FText>& Quality : {TPair<int32, FText>(64, LOCTEXT("PhotoDraft", "Borrador")), TPair<int32, FText>(256, LOCTEXT("PhotoMedium", "Media")),
		TPair<int32, FText>(1024, LOCTEXT("PhotoHigh", "Alta"))})
	{
		const int32 Samples = Quality.Key;
		Qualities.Add({{}, Quality.Value, FText::GetEmpty(), FText::Format(LOCTEXT("PhotoQualityTip", "{0} muestras por píxel"), Samples),
			[this, Samples]() { const UFloor101GameViewportClient* Client = ViewportClient.Get(); return Client && Client->GetPhotoSamples() == Samples; },
			ClientAction([Samples](UFloor101GameViewportClient& Client) { Client.SetPhotoSamples(Samples); })});
	}

	return SNew(SBox)
		.WidthOverride(296.0f)
		.Visibility_Lambda([this]() { return IsPhotoMode() ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(16.0f, 14.0f, 16.0f, 12.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
				[
					SNew(SBox).WidthOverride(36.0f).HeightOverride(36.0f)
					[SNew(SBorder).BorderImage(Studio::Box(P.BrassSel, 8.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center)[Ui::Icon(EFloor101Icon::Photo, 20.0f, FSlateColor(P.Brass))]]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("PhotoTitle", "Foto")).Font(Studio::Text(Studio::EText::Heading)).ColorAndOpacity(P.Text1)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[SNew(STextBlock).Text(LOCTEXT("PhotoSub", "Path tracing: se afina mientras la cámara está quieta")).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Text3).AutoWrapText(true)]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()[Ui::Divider()]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 14.0f, 16.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Studio::Text(Studio::EText::Label)).ColorAndOpacity(P.Text2).AutoWrapText(true)
					.Text_Lambda([]()
					{
						if (const int32 Pending = Floor101::PathTracing::GetPendingShaders(); Pending > 0)
						{
							return FText::Format(LOCTEXT("PhotoShaders", "Preparando materiales ({0} shaders)…"), Pending);
						}
						return Floor101::PathTracing::IsComplete() ? LOCTEXT("PhotoReady", "Lista") : LOCTEXT("PhotoSamples", "Muestras");
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Studio::Text(Studio::EText::Numeric)).ColorAndOpacity(P.Text1)
					.Text_Lambda([]()
					{
						return FText::Format(LOCTEXT("PhotoProgressCount", "{0} / {1}"), Floor101::PathTracing::GetAccumulatedSamples(), Floor101::PathTracing::GetTargetSamples());
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 12.0f)
			[
				SNew(SBox).HeightOverride(4.0f)
				[
					SNew(SProgressBar).Style(&Studio::Progress()).FillColorAndOpacity(FLinearColor::White)
					.Percent_Lambda([]() -> TOptional<float>
					{
						if (Floor101::PathTracing::GetPendingShaders() > 0)
						{
							return TOptional<float>(); // preparing: activity, not an invented percentage
						}
						const int32 Target = Floor101::PathTracing::GetTargetSamples();
						return Target > 0 ? static_cast<float>(Floor101::PathTracing::GetAccumulatedSamples()) / static_cast<float>(Target) : 0.0f;
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight()[Floor101::Ui::Section(LOCTEXT("PhotoQuality", "Calidad"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 12.0f)[Ui::Segmented(Qualities, true, true, true)]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 8.0f)
			[
				Ui::Button(Studio::EButton::Primary, LOCTEXT("PhotoSave", "Guardar imagen…"),
					[this]()
					{
						if (UFloor101DocumentSubsystem* Subsystem = Document.Get())
						{
							FFloor101FileActions::SavePhoto(*Subsystem);
						}
					},
					EFloor101Icon::Save, LOCTEXT("PhotoSaveTip", "Elige dónde guardar la imagen y con qué nombre: PNG con HDR desactivado; EXR para conservar HDR"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 12.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[Ui::Button(Studio::EButton::Ghost, LOCTEXT("PhotoFrame", "Encuadrar"), ClientAction([](UFloor101GameViewportClient& Client) { Client.FrameProject(); }),
					EFloor101Icon::Frame, LOCTEXT("PhotoFrameTip", "Encuadra todo el proyecto"), INVTEXT("F"), true, 30.0f)]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[Ui::Button(Studio::EButton::Ghost, LOCTEXT("PhotoTop", "Cenital"), ClientAction([](UFloor101GameViewportClient& Client) { Client.ShowTopView(); }),
					EFloor101Icon::Top, LOCTEXT("PhotoTopTip", "Vista desde arriba"), INVTEXT("T"), true, 30.0f)]
			]
			+ SVerticalBox::Slot().AutoHeight()[Floor101::Ui::Section(LOCTEXT("PhotoCamera", "Cámara"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 6.0f)[MakeNavigationButton()]
			+ SVerticalBox::Slot().AutoHeight().Padding(12.0f, 0.0f, 16.0f, 12.0f).HAlign(HAlign_Left)[MakeNavigationSpeed()]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 12.0f)
			[
				Ui::Note(LOCTEXT("PhotoHint",
					"La imagen se afina muestra a muestra y empieza de nuevo si mueves la cámara. Guárdala cuando esté lista. HDR se guarda como EXR; desactívalo para guardar PNG."))
			]
			+ SVerticalBox::Slot().AutoHeight()[Ui::Divider()]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(8.0f, 8.0f, 14.0f, 8.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[Ui::Button(Studio::EButton::Ghost, LOCTEXT("PhotoLeave", "Volver a editar"), ClientAction([](UFloor101GameViewportClient& Client) { Client.SetPhotoMode(false); }),
					EFloor101Icon::Design, LOCTEXT("PhotoLeaveTip", "Sale de la foto (también Esc)"), INVTEXT("R"), true, 30.0f)]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Ui::Keycap(INVTEXT("R"))]
			]
		];
}

bool SFloor101Overlay::IsLevelPanelShown() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	return Subsystem && Subsystem->GetEditController().mode() == EditMode::Select && !GetSelectedWall() && !GetSelectedOpening() &&
		!GetSelectedFurniture() && !GetSelectedColumn() && !GetSelectedServiceRun() && !GetSelectedRoom() && !GetSelectedDimension() && GetActiveLevel();
}

std::optional<f101::cad::Furniture> SFloor101Overlay::GetSelectedFurniture() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (!Subsystem || Subsystem->GetEditController().mode() != EditMode::Select)
	{
		return std::nullopt;
	}
	return Subsystem->GetEditController().selectedFurniture();
}

TSharedRef<SWidget> SFloor101Overlay::MakeFurnitureSection()
{
	namespace Ui = Floor101::Ui;
	auto Size = [this](std::int64_t f101::cad::Furniture::* Field)
	{
		return [this, Field]() -> TOptional<double>
		{
			const std::optional<f101::cad::Furniture> Selected = GetSelectedFurniture();
			return Selected ? TOptional<double>(static_cast<double>((*Selected).*Field)) : TOptional<double>();
		};
	};

	return SNew(SVerticalBox)
		.Visibility_Lambda([this]() { return GetSelectedFurniture() ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupSize", "Medidas"), Stack({
				MakePropertyRow(LOCTEXT("FurnitureWidth", "Ancho"), Size(&f101::cad::Furniture::widthMm), &EditController::setFurnitureWidth),
				MakePropertyRow(LOCTEXT("FurnitureDepth", "Fondo"), Size(&f101::cad::Furniture::depthMm), &EditController::setFurnitureDepth),
				MakePropertyRow(LOCTEXT("FurnitureHeight", "Alto"), Size(&f101::cad::Furniture::heightMm), &EditController::setFurnitureHeight),
				MakePropertyRow(LOCTEXT("FurnitureElevation", "Sobre el suelo"), Size(&f101::cad::Furniture::elevationMm), &EditController::setFurnitureElevation)}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).Visibility_Lambda([this]()
			{
				const auto Selected = GetSelectedFurniture();
				return Selected && Selected->light ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[Group(LOCTEXT("FixtureLighting", "Iluminación"), Stack({
				Ui::Row(LOCTEXT("FixtureOn", "Encendida"), Ui::Switch(TAttribute<bool>::CreateLambda([this]()
				{
					const auto Selected = GetSelectedFurniture();
					return Selected && Selected->light && Selected->light->enabled;
				}), [this](bool Enabled) { Run([Enabled](EditController& Editor) { return Editor.setFurnitureLightEnabled(Enabled); }); })),
				Ui::Row(LOCTEXT("FixtureLumens", "Flujo (lm)"), Ui::Field(TAttribute<FText>::CreateLambda([this]()
				{
					const auto Selected = GetSelectedFurniture();
					return Selected && Selected->light ? FText::AsNumber(Selected->light->lumens, &FNumberFormattingOptions::DefaultNoGrouping()) : FText::GetEmpty();
				}), [this](const FString& Text)
				{
					Run([Text](EditController& Editor)
					{
						int64 Lumens = 0;
						if (!LexTryParseString(Lumens, *Text)) return f101::Status(f101::Error{f101::ErrorCode::InvalidArgument, "Escribe un número entero de lúmenes"});
						return Editor.setFurnitureLightLumens(Lumens);
					});
				}))}))]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupRotation", "Giro"), Stack({
				MakeTextRow(LOCTEXT("FurnitureRotation", "Giro"), [this]()
				{
					const std::optional<f101::cad::Furniture> Selected = GetSelectedFurniture();
					if (!Selected)
					{
						return FText::GetEmpty();
					}
					FNumberFormattingOptions Hundredths;
					Hundredths.SetMaximumFractionalDigits(2);
					return FText::Format(LOCTEXT("Degrees", "{0}°"),
						FText::AsNumber(static_cast<double>(Selected->rotationCentidegrees) / 100.0, &Hundredths));
				}, &EditController::setFurnitureRotation),
				SNew(SBox).HAlign(HAlign_Right)
				[Ui::Button(Floor101::Studio::EButton::Secondary, LOCTEXT("Rotate", "Girar 90°"), [this]() { Run([](EditController& Editor) { return Editor.rotateFurniture(); }); },
					EFloor101Icon::Rotate, LOCTEXT("RotateTip", "Gira el mueble 90° en sentido antihorario sobre su centro"), INVTEXT("Espacio"), true, 30.0f)]}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupFinish", "Acabado"), Ui::Row(LOCTEXT("FurnitureFinish", "Acabado"),
				MakeFinishCombo(f101::cad::Surface::Furniture,
					[this]() -> FString
					{
						// An imported model with no finish of its own shows its materials: "Original".
						const std::optional<f101::cad::Furniture> Selected = GetSelectedFurniture();
						if (!Selected || (Selected->finish.empty() && f101::cad::isAssetRef(Selected->type)))
						{
							return FString();
						}
						return UTF8_TO_TCHAR(std::string(f101::cad::resolveFurnitureFinish(*Selected)).c_str());
					},
					[](EditController& Editor, const std::string& Finish) { return Editor.setFurnitureFinish(Finish); })))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
		[Ui::Note(LOCTEXT("FurnitureHint", "Arrástralo para moverlo: se ajusta a la rejilla y se apoya en los muros."))];
}

std::optional<f101::cad::Column> SFloor101Overlay::GetSelectedColumn() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (!Subsystem || Subsystem->GetEditController().mode() != EditMode::Select)
	{
		return std::nullopt;
	}
	return Subsystem->GetEditController().selectedColumn();
}

std::optional<f101::cad::ServiceRun> SFloor101Overlay::GetSelectedServiceRun() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (!Subsystem || Subsystem->GetEditController().mode() != EditMode::Select)
	{
		return std::nullopt;
	}
	return Subsystem->GetEditController().selectedServiceRun();
}

TSharedRef<SWidget> SFloor101Overlay::MakeColumnShapeButtons(TFunction<std::optional<f101::cad::ColumnShape>()> Current,
	TFunction<f101::Status(f101::cad::EditController&, f101::cad::ColumnShape)> Apply)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (const f101::cad::ColumnShape Shape : {f101::cad::ColumnShape::Rectangle, f101::cad::ColumnShape::Round})
	{
		Row->AddSlot().FillWidth(1.0f).Padding(Row->NumSlots() > 0 ? 6.0f : 0.0f, 0.0f, 0.0f, 0.0f)
		[
			Floor101::Ui::Choice(Floor101::ColumnShapeName(Shape), TAttribute<bool>::CreateLambda([Current, Shape]() { return Current() == Shape; }),
				[this, Apply, Shape]() { Run([&Apply, Shape](EditController& Editor) { return Apply(Editor, Shape); }); })
		];
	}
	return Row;
}

TSharedRef<SWidget> SFloor101Overlay::MakeServiceKindButtons(TFunction<std::optional<f101::cad::ServiceKind>()> Current,
	TFunction<f101::Status(f101::cad::EditController&, f101::cad::ServiceKind)> Apply)
{
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const f101::cad::ServiceKindInfo& Info : f101::cad::serviceKinds())
	{
		const f101::cad::ServiceKind Kind = Info.kind;
		List->AddSlot().AutoHeight().Padding(0.0f, List->NumSlots() > 0 ? 6.0f : 0.0f, 0.0f, 0.0f)
		[
			Floor101::Ui::Choice(FText::Format(LOCTEXT("ServiceKindLabel", "{0} · Ø {1} mm"), Floor101::ServiceKindName(Kind), FText::AsNumber(Info.diameterMm)),
				TAttribute<bool>::CreateLambda([Current, Kind]() { return Current() == Kind; }),
				[this, Apply, Kind]() { Run([&Apply, Kind](EditController& Editor) { return Apply(Editor, Kind); }); },
				FText::Format(LOCTEXT("ServiceKindTip", "{0}: tubo de {1} mm de diámetro"), Floor101::ServiceKindName(Kind), FText::AsNumber(Info.diameterMm)),
				MakeSwatch(std::string(Info.finish), 14.0f), false)
		];
	}
	return List;
}

TSharedRef<SWidget> SFloor101Overlay::MakeColumnSection()
{
	namespace Ui = Floor101::Ui;
	auto Size = [this](std::int64_t f101::cad::Column::* Field)
	{
		return [this, Field]() -> TOptional<double>
		{
			const std::optional<f101::cad::Column> Selected = GetSelectedColumn();
			return Selected ? TOptional<double>(static_cast<double>((*Selected).*Field)) : TOptional<double>();
		};
	};
	auto ShownFor = [this](bool bRound)
	{
		return [this, bRound]()
		{
			const std::optional<f101::cad::Column> Selected = GetSelectedColumn();
			return Selected && (Selected->shape == f101::cad::ColumnShape::Round) == bRound ? EVisibility::Visible : EVisibility::Collapsed;
		};
	};

	return SNew(SVerticalBox)
		.Visibility_Lambda([this]() { return GetSelectedColumn() ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupShape", "Forma"), MakeColumnShapeButtons([this]() -> std::optional<f101::cad::ColumnShape>
				{
					const std::optional<f101::cad::Column> Selected = GetSelectedColumn();
					return Selected ? std::optional<f101::cad::ColumnShape>(Selected->shape) : std::nullopt;
				},
				[](EditController& Editor, f101::cad::ColumnShape Shape) { return Editor.setColumnShape(Shape); }))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupSize", "Medidas"), Stack({
				SNew(SBox).Visibility_Lambda(ShownFor(true))
				[MakePropertyRow(LOCTEXT("ColumnDiameter", "Diámetro"), Size(&f101::cad::Column::widthMm), &EditController::setColumnWidth)],
				SNew(SBox).Visibility_Lambda(ShownFor(false))
				[MakePropertyRow(LOCTEXT("ColumnWidth", "Ancho"), Size(&f101::cad::Column::widthMm), &EditController::setColumnWidth)],
				SNew(SBox).Visibility_Lambda(ShownFor(false))
				[MakePropertyRow(LOCTEXT("ColumnDepth", "Fondo"), Size(&f101::cad::Column::depthMm), &EditController::setColumnDepth)],
				SNew(SBox).Visibility_Lambda(ShownFor(false))
				[
					MakeTextRow(LOCTEXT("ColumnRotation", "Giro"), [this]()
					{
						const std::optional<f101::cad::Column> Selected = GetSelectedColumn();
						if (!Selected)
						{
							return FText::GetEmpty();
						}
						FNumberFormattingOptions Hundredths;
						Hundredths.SetMaximumFractionalDigits(2);
						return FText::Format(LOCTEXT("ColumnDegrees", "{0}°"),
							FText::AsNumber(static_cast<double>(Selected->rotationCentidegrees) / 100.0, &Hundredths));
					}, &EditController::setColumnRotation)
				],
				SNew(SBox).HAlign(HAlign_Right).Visibility_Lambda(ShownFor(false))
				[Ui::Button(Floor101::Studio::EButton::Secondary, LOCTEXT("RotateColumn", "Girar 90°"), [this]() { Run([](EditController& Editor) { return Editor.flip(); }); },
					EFloor101Icon::Rotate, LOCTEXT("RotateColumnTip", "Gira el pilar 90° en sentido antihorario sobre su centro"), INVTEXT("Espacio"), true, 30.0f)]}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupFinish", "Acabado"), Ui::Row(LOCTEXT("ColumnFinish", "Acabado"),
				MakeFinishCombo(f101::cad::Surface::Wall,
					[this]() -> FString
					{
						const std::optional<f101::cad::Column> Selected = GetSelectedColumn();
						return Selected ? FString(UTF8_TO_TCHAR(std::string(f101::cad::resolveColumnFinish(*Selected)).c_str())) : FString();
					},
					[](EditController& Editor, const std::string& Finish) { return Editor.setColumnFinish(Finish); })))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
		[Ui::Note(LOCTEXT("ColumnHint", "Arrástralo para moverlo: se ajusta a las esquinas y a los ejes de los muros. Llega del suelo al forjado de su planta."))];
}

TSharedRef<SWidget> SFloor101Overlay::MakeColumnPalette()
{
	// The settings of the Column mode, while it is on.
	auto Settings = [this]() -> const f101::cad::EditSettings*
	{
		const UFloor101DocumentSubsystem* S = Document.Get();
		return S && S->GetEditController().mode() == EditMode::Column ? &S->GetEditController().settings() : nullptr;
	};
	auto ShownFor = [Settings](bool bRound)
	{
		return [Settings, bRound]()
		{
			const f101::cad::EditSettings* Current = Settings();
			return Current && (Current->columnShape == f101::cad::ColumnShape::Round) == bRound ? EVisibility::Visible : EVisibility::Collapsed;
		};
	};
	auto SizeText = [Settings](std::int64_t f101::cad::EditSettings::* Field)
	{
		return [Settings, Field]()
		{
			const f101::cad::EditSettings* Current = Settings();
			return Current ? LengthText(static_cast<double>(Current->*Field)) : FText::GetEmpty();
		};
	};

	return SNew(SVerticalBox)
		.Visibility_Lambda([Settings]() { return Settings() ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupShape", "Forma"), MakeColumnShapeButtons([Settings]() -> std::optional<f101::cad::ColumnShape>
				{
					const f101::cad::EditSettings* Current = Settings();
					return Current ? std::optional<f101::cad::ColumnShape>(Current->columnShape) : std::nullopt;
				},
				[](EditController& Editor, f101::cad::ColumnShape Shape) { return Editor.setNewColumnShape(Shape); }))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupSize", "Medidas"), Stack({
				SNew(SBox).Visibility_Lambda(ShownFor(true))
				[MakeTextRow(LOCTEXT("NewColumnDiameter", "Diámetro"), SizeText(&f101::cad::EditSettings::columnWidthMm), &EditController::setNewColumnWidth)],
				SNew(SBox).Visibility_Lambda(ShownFor(false))
				[MakeTextRow(LOCTEXT("NewColumnWidth", "Ancho"), SizeText(&f101::cad::EditSettings::columnWidthMm), &EditController::setNewColumnWidth)],
				SNew(SBox).Visibility_Lambda(ShownFor(false))
				[MakeTextRow(LOCTEXT("NewColumnDepth", "Fondo"), SizeText(&f101::cad::EditSettings::columnDepthMm), &EditController::setNewColumnDepth)]}))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
		[
			Floor101::Ui::Note(LOCTEXT("ColumnPaletteHint",
				"Clic para colocarlo, de hormigón y del suelo al forjado. Se ajusta a las esquinas y a los ejes de los muros; Espacio lo gira 90°. Medidas de 10 a 300 cm."))
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakeServiceRunSection()
{
	namespace Studio = Floor101::Studio;
	return SNew(SVerticalBox)
		.Visibility_Lambda([this]() { return GetSelectedServiceRun() ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupNetwork", "Red"), MakeServiceKindButtons([this]() -> std::optional<f101::cad::ServiceKind>
				{
					const std::optional<f101::cad::ServiceRun> Selected = GetSelectedServiceRun();
					return Selected ? std::optional<f101::cad::ServiceKind>(Selected->kind) : std::nullopt;
				},
				[](EditController& Editor, f101::cad::ServiceKind Kind) { return Editor.setServiceRunKind(Kind); }))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupRun", "Tramo"), SNew(STextBlock).Font(Studio::Text(Studio::EText::Label)).ColorAndOpacity(Studio::Palette().Text1).AutoWrapText(true)
				.Text_Lambda([this]()
				{
					const std::optional<f101::cad::ServiceRun> Selected = GetSelectedServiceRun();
					if (!Selected)
					{
						return FText::GetEmpty();
					}
					std::int64_t Low = Selected->points.front().z_mm;
					std::int64_t High = Low;
					for (const f101::cad::ServicePoint& Point : Selected->points)
					{
						Low = std::min(Low, Point.z_mm);
						High = std::max(High, Point.z_mm);
					}
					const FText Heights = Low == High ? LengthText(static_cast<double>(Low))
						: FText::Format(LOCTEXT("RunHeights", "{0} a {1}"), LengthText(static_cast<double>(Low)), LengthText(static_cast<double>(High)));
					return FText::Format(LOCTEXT("RunInfo", "Longitud {0} · {1} puntos · altura {2}"),
						LengthText(f101::cad::serviceRunLengthMm(*Selected)), FText::AsNumber(static_cast<int32>(Selected->points.size())), Heights);
				}))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
		[Floor101::Ui::Note(LOCTEXT("RunHint", "Arrastra la tubería para moverla entera o un asa para mover ese codo. X oculta las instalaciones."))];
}

TSharedRef<SWidget> SFloor101Overlay::MakeServicesPalette()
{
	namespace Studio = Floor101::Studio;
	// The settings of the Services mode, while it is on.
	auto Settings = [this]() -> const f101::cad::EditSettings*
	{
		const UFloor101DocumentSubsystem* S = Document.Get();
		return S && S->GetEditController().mode() == EditMode::Services ? &S->GetEditController().settings() : nullptr;
	};
	// A height preset: highlighted while it is the height of the next points.
	auto HeightButton = [this, Settings](const FText& Label, const FText& ToolTip, TFunction<std::int64_t()> Height)
	{
		return Floor101::Ui::Choice(Label, TAttribute<bool>::CreateLambda([Settings, Height]()
			{
				const f101::cad::EditSettings* Current = Settings();
				return Current && Current->serviceHeightMm == Height();
			}),
			[this, Height]()
			{
				const std::int64_t Millimeters = Height();
				Run([Millimeters](EditController& Editor) { return Editor.setServiceHeightMm(Millimeters); });
			}, ToolTip);
	};
	// Just under the slab of the active level.
	auto CeilingHeight = [this]() -> std::int64_t
	{
		const f101::cad::Level* Level = GetActiveLevel();
		return (Level ? Level->clearHeightMm() : f101::cad::DefaultLevelHeightMm - f101::cad::DefaultSlabThicknessMm) - 100;
	};

	return SNew(SVerticalBox)
		.Visibility_Lambda([Settings]() { return Settings() ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupNetwork", "Red"), MakeServiceKindButtons([Settings]() -> std::optional<f101::cad::ServiceKind>
				{
					const f101::cad::EditSettings* Current = Settings();
					return Current ? std::optional<f101::cad::ServiceKind>(Current->serviceKind) : std::nullopt;
				},
				[](EditController& Editor, f101::cad::ServiceKind Kind) { return Editor.setServiceKind(Kind); }))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupHeight", "Altura"), Stack({
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[HeightButton(LOCTEXT("UnderFloor", "Bajo suelo"), LOCTEXT("UnderFloorTip", "A 10 cm bajo el suelo de la planta"), []() { return std::int64_t(-100); })]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6.0f, 0.0f)
				[HeightButton(LOCTEXT("InWall", "Pared"), LOCTEXT("InWallTip", "A 30 cm del suelo, a la altura de los enchufes"), []() { return std::int64_t(300); })]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[HeightButton(LOCTEXT("Ceiling", "Techo"), LOCTEXT("CeilingTip", "A 10 cm bajo el forjado de la planta"), CeilingHeight)],
				MakePropertyRow(LOCTEXT("ServiceHeight", "Altura"), [Settings]() -> TOptional<double>
				{
					const f101::cad::EditSettings* Current = Settings();
					return Current ? TOptional<double>(static_cast<double>(Current->serviceHeightMm)) : TOptional<double>();
				}, &EditController::setServiceHeight),
				SNew(STextBlock).Font(Studio::Text(Studio::EText::Label)).ColorAndOpacity(Studio::Palette().Text1)
				.Text_Lambda([this]()
				{
					const UFloor101DocumentSubsystem* S = Document.Get();
					const std::size_t Points = S ? S->GetEditController().runPoints().size() : 0;
					return Points == 0 ? LOCTEXT("RunNotStarted", "Clic en la planta para empezar")
						: FText::Format(LOCTEXT("RunPoints", "{0} puntos colocados"), FText::AsNumber(static_cast<int32>(Points)));
				})}))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
		[
			Floor101::Ui::Note(LOCTEXT("ServicesPaletteHint",
				"Clic: añade un punto (se ajusta a los muros, la rejilla y los ángulos) · doble clic o Enter: termina · Retroceso: quita el último · Esc: descarta. Cambiar la altura a mitad sube o baja en vertical."))
		];
}

const FSlateBrush* SFloor101Overlay::GetPictureBrush(const FString& Path)
{
	const FString Key = TEXT("picture:") + Path;
	if (const TSharedPtr<FSlateBrush>* Found = AssetBrushes.Find(Key))
	{
		return Found->Get();
	}
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	UTexture2D* Picture = Library ? Library->GetPicture(Path) : nullptr;
	if (!Picture)
	{
		if (!NoImageBrush)
		{
			NoImageBrush = MakeShared<FSlateColorBrush>(Floor101::Studio::Palette().Missing);
		}
		return NoImageBrush.Get();
	}
	TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Picture);
	Brush->ImageSize = FVector2D(Picture->GetSizeX(), Picture->GetSizeY());
	AssetBrushes.Add(Key, Brush);
	return Brush.Get();
}

FReply SFloor101Overlay::UseLibraryItem(const std::string& ItemId, bool bModel)
{
	UFloor101DocumentSubsystem* Subsystem = Document.Get();
	const f101::Result<f101::cad::AssetId> Added = Subsystem ? Subsystem->AddFromLibrary(UTF8_TO_TCHAR(ItemId.c_str()))
		: f101::Result<f101::cad::AssetId>(f101::Error{f101::ErrorCode::NotFound, "No document"});
	if (!Added)
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		return FReply::Handled();
	}
	const std::string Ref = f101::cad::assetRef(Added.value());
	return Run([bModel, &Ref](EditController& Editor) { return bModel ? Editor.setFurnitureType(Ref) : Editor.setPaintFinish(Ref); });
}

bool SFloor101Overlay::IsLibraryItemInUse(const std::string& ItemId, bool bModel) const
{
	const f101::cad::Asset* Asset = bModel ? GetFurnitureAsset() : GetPaintAsset();
	return Asset && Asset->library == ItemId;
}

TSharedRef<SWidget> SFloor101Overlay::MakeFurniturePalette()
{
	using f101::cad::FurnitureCategory;
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (const f101::cad::FurnitureType* Current = Subsystem ? f101::cad::findFurnitureType(Subsystem->GetEditController().settings().furnitureType) : nullptr)
	{
		FurnitureTab = Current->category;
	}
	const FString LibraryFolder = UFloor101DocumentSubsystem::GetLibraryFolder();
	const f101::assets::Library& Library = UFloor101DocumentSubsystem::GetLibrary();

	// Category chips, and one grid per category (each shows while it is the chosen one): the catalogue's parametric types,
	// then the library's models (ADR-0019).
	TSharedRef<SWrapBox> Tabs = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.0, 6.0))
		.Visibility_Lambda([this]() { return FurnitureQuery.IsEmpty() && !bFurnitureFavoritesOnly ? EVisibility::Visible : EVisibility::Collapsed; });
	TSharedRef<SVerticalBox> Grids = SNew(SVerticalBox);
	for (const FurnitureCategory Category : {FurnitureCategory::Beds, FurnitureCategory::Seating, FurnitureCategory::Sofas,
			 FurnitureCategory::Tables, FurnitureCategory::Storage, FurnitureCategory::Kitchen, FurnitureCategory::Bathroom,
			 FurnitureCategory::Lighting, FurnitureCategory::Decor})
	{
		Tabs->AddSlot()
		[
			Floor101::Ui::Choice(FurnitureCategoryName(Category), TAttribute<bool>::CreateLambda([this, Category]() { return FurnitureTab == Category; }),
				[this, Category]()
				{
					FurnitureTab = Category;
					FSlateApplication::Get().SetAllUserFocusToGameViewport();
				})
		];

		TSharedRef<SWrapBox> Grid = SNew(SWrapBox).UseAllottedSize(true);
		for (const f101::cad::FurnitureType& Type : f101::cad::furnitureCatalogue())
		{
			if (Type.category != Category)
			{
				continue;
			}
			const std::string Id(Type.id);
			const TSharedRef<SWidget> Picture = SNew(SFloor101FurnitureThumbnail).Type(Id);
			Grid->AddSlot().Padding(0.0f, 0.0f, 4.0f, 4.0f)
			[
				MakeCatalogueTile(Picture, FurnitureName(Id),
					Type.againstWall ? LOCTEXT("AgainstWallTip", "Cerca de un muro se apoya en él de espaldas")
						: LOCTEXT("FreeTip", "Se coloca donde apuntes; Espacio lo gira 90°"),
					[this, Id]()
					{
						const UFloor101DocumentSubsystem* S = Document.Get();
						return S && S->GetEditController().settings().furnitureType == Id;
					},
					[this, Id]() { return Run([&Id](EditController& Editor) { return Editor.setFurnitureType(Id); }); }, FString(TEXT("furniture:")) + UTF8_TO_TCHAR(Id.c_str()), true)
			];
		}
		for (const f101::assets::LibraryFurniture& Item : Library.furniture)
		{
			if (Item.category != Category)
			{
				continue;
			}
			const std::string Id = Item.id;
			const FString PicturePath = FPaths::Combine(LibraryFolder, UTF8_TO_TCHAR(Item.thumbnail.c_str()));
			Grid->AddSlot().Padding(0.0f, 0.0f, 4.0f, 4.0f)
			[
				MakeCatalogueTile(SNew(SImage).Image_Lambda([this, PicturePath]() { return GetPictureBrush(PicturePath); }),
					FText::FromString(UTF8_TO_TCHAR(Item.name.c_str())),
					FText::Format(LOCTEXT("LibraryFurnitureTip", "{0}\n{1}\nSe copia en la carpeta del proyecto al elegirlo."),
						FText::FromString(UTF8_TO_TCHAR(Item.name.c_str())), FText::FromString(UTF8_TO_TCHAR(Item.credit.c_str()))),
					[this, Id]() { return IsLibraryItemInUse(Id, true); },
					[this, Id]() { return UseLibraryItem(Id, true); }, FString(TEXT("library-model:")) + UTF8_TO_TCHAR(Id.c_str()), true)
			];
		}
		Grids->AddSlot().AutoHeight()
		[
			SNew(SBox).Visibility_Lambda([this, Category]() { return FurnitureTab == Category || !FurnitureQuery.IsEmpty() || bFurnitureFavoritesOnly ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				Grid
			]
		];
	}

	return SNew(SVerticalBox)
		.Visibility_Lambda([this]()
		{
			const UFloor101DocumentSubsystem* S = Document.Get();
			return S && S->GetEditController().mode() == EditMode::Furniture ? EVisibility::Visible : EVisibility::Collapsed;
		})
		+ SVerticalBox::Slot().AutoHeight()[Floor101::Ui::Section(LOCTEXT("GroupCatalogue", "Catálogo"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 0.0f)[MakeCatalogueFilters(true)]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 10.0f)[Tabs]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 12.0f, 0.0f)
		[
			SNew(SBox)
			.MaxDesiredHeight(380.0f)
			[
				SNew(SScrollBox).ScrollBarStyle(&Floor101::Studio::ScrollBar())
				+ SScrollBox::Slot()
				[
					Grids
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[Floor101::Ui::Section(LOCTEXT("GroupImportedModels", "Modelos importados"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 12.0f)
		[
			MakeImportedModels()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
		[
			Floor101::Ui::Note(LOCTEXT("FurniturePaletteHint",
				"Clic en el suelo para colocarlo. Después, en Seleccionar: arrástralo, gíralo o cambia sus medidas y su acabado."))
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakeLevelSection()
{
	namespace Ui = Floor101::Ui;
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
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupLevel", "Planta"), Stack({
				Ui::Row(LOCTEXT("LevelName", "Nombre"), Ui::Field(
					TAttribute<FText>::CreateLambda([this]()
					{
						const f101::cad::Level* Level = GetActiveLevel();
						return Level ? FText::FromString(UTF8_TO_TCHAR(Level->name.c_str())) : FText::GetEmpty();
					}),
					[this](const FString& Text)
					{
						const f101::cad::Level* Level = GetActiveLevel();
						const FString Typed = Text.TrimStartAndEnd();
						if (Level && Typed != UTF8_TO_TCHAR(Level->name.c_str()))
						{
							if (UFloor101DocumentSubsystem* Subsystem = Document.Get())
							{
								(void)Subsystem->Edit([&Typed](EditController& Editor) { return Editor.setLevelName(TCHAR_TO_UTF8(*Typed)); });
							}
						}
					}, false,
					TAttribute<FText>::CreateLambda([this]()
					{
						const UFloor101DocumentSubsystem* S = Document.Get();
						return S ? LevelName(S->GetDocument().project(), S->GetEditController().activeLevel()) : FText::GetEmpty();
					}))),
				MakePropertyRow(LOCTEXT("LevelHeight", "Altura"), LevelValue(&f101::cad::Level::heightMm), &EditController::setLevelHeight),
				MakePropertyRow(LOCTEXT("LevelSlab", "Forjado"), LevelValue(&f101::cad::Level::slabThicknessMm), &EditController::setLevelSlabThickness)}))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 12.0f)
		[
			Ui::Note(LOCTEXT("LevelInfoHint", "Para nombrar una habitación, selecciona su interior. Los muros nuevos suben hasta la planta de arriba."))
		]
		+ SVerticalBox::Slot().AutoHeight()[Ui::Divider()]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(8.0f, 8.0f, 14.0f, 8.0f)).HAlign(HAlign_Left)
		[
			Ui::Button(Floor101::Studio::EButton::Danger, LOCTEXT("DeleteLevel", "Eliminar planta"), [this]() { Run([](EditController& Editor) { return Editor.deleteActiveLevel(); }); },
				EFloor101Icon::Trash, LOCTEXT("DeleteLevelTip", "Elimina esta planta con sus muros y muebles (se puede deshacer)"), FText::GetEmpty(),
				TAttribute<bool>::CreateLambda([this]()
				{
					const UFloor101DocumentSubsystem* S = Document.Get();
					return S && S->GetDocument().project().levels.size() > 1;
				}), 30.0f)
		];
}

const FFloor101RoomView* SFloor101Overlay::GetSelectedRoom() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (!Subsystem || Subsystem->GetEditController().selectedRoom().empty())
	{
		return nullptr;
	}
	const FString Key = UTF8_TO_TCHAR(Subsystem->GetEditController().selectedRoom().c_str());
	return Subsystem->GetRooms().FindByPredicate([&Key](const FFloor101RoomView& Room) { return Room.Key == Key; });
}

TSharedRef<SWidget> SFloor101Overlay::MakeFinishCombo(f101::cad::Surface Surface, TFunction<FString()> Current,
	TFunction<f101::Status(f101::cad::EditController&, const std::string&)> Apply)
{
	namespace Studio = Floor101::Studio;
	// One list per surface, on the heap: combos keep a pointer to it while the map grows.
	TSharedPtr<TArray<TSharedPtr<FString>>>& Slot = FinishOptions.FindOrAdd(static_cast<uint8>(Surface));
	if (!Slot)
	{
		Slot = MakeShared<TArray<TSharedPtr<FString>>>();
	}
	const TSharedPtr<TArray<TSharedPtr<FString>>> List = Slot;
	// The catalogue's finishes for the surface and the project's imported textures that allow it (ADR-0017); furniture
	// also offers its original look (empty: its type's finish, or a model's own materials).
	auto Refresh = [this, Surface, List]()
	{
		TArray<TSharedPtr<FString>>& Options = *List;
		Options.Reset();
		if (Surface == f101::cad::Surface::Furniture)
		{
			Options.Add(MakeShared<FString>());
		}
		for (const f101::cad::FinishInfo& Finish : f101::cad::finishCatalogue())
		{
			if (Finish.allows(Surface))
			{
				Options.Add(MakeShared<FString>(UTF8_TO_TCHAR(std::string(Finish.id).c_str())));
			}
		}
		if (const f101::cad::Project* Project = CurrentProject())
		{
			for (const f101::cad::Asset& Asset : Project->assets)
			{
				if (Asset.kind == f101::cad::AssetKind::Texture && (Asset.texture.surfaces & static_cast<uint8>(Surface)) != 0)
				{
					Options.Add(MakeShared<FString>(UTF8_TO_TCHAR(f101::cad::assetRef(Asset.id).c_str())));
				}
			}
		}
	};
	Refresh();
	auto Swatch = [this](const std::string& Id)
	{
		return SNew(SBorder).BorderImage(Studio::Box(FLinearColor::Transparent, 3.0f, Floor101::Studio::Palette().SwatchEdge, 1.0f)).Padding(0.0f)
			[MakeSwatch(Id, 14.0f)];
	};
	auto Row = [this, Swatch](const FString& Id)
	{
		const std::string Utf8 = TCHAR_TO_UTF8(*Id);
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				Swatch(Utf8)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FinishName(Utf8, CurrentProject())).Font(Studio::Text(Studio::EText::Body)).ColorAndOpacity(Studio::Palette().Text1)
			];
	};

	using FCombo = SComboBox<TSharedPtr<FString>>;
	const TSharedRef<TWeakPtr<FCombo>> Self = MakeShared<TWeakPtr<FCombo>>();
	TSharedPtr<FCombo> Combo;
	TSharedRef<SWidget> Widget = SNew(SBox)
		.HeightOverride(Studio::Size::Field)
		[
			SAssignNew(Combo, FCombo).ComboBoxStyle(&Studio::ComboBox()).ItemStyle(&Studio::ListRow()).HasDownArrow(false).MaxListHeight(360.0f)
			.IsFocusable(false)
			.OptionsSource(List.Get())
			.OnComboBoxOpening_Lambda([Refresh, Self]()
			{
				Refresh(); // imported textures may have come or gone
				if (const TSharedPtr<FCombo> Opened = Self->Pin())
				{
					Opened->RefreshOptions();
				}
			})
			.OnGenerateWidget_Lambda([Row](TSharedPtr<FString> Id) { return Row(*Id); })
			.OnSelectionChanged_Lambda([this, Apply](TSharedPtr<FString> Id, ESelectInfo::Type Info)
			{
				if (Id && Info != ESelectInfo::Direct)
				{
					const std::string Finish = TCHAR_TO_UTF8(**Id);
					(void)Run([&Apply, &Finish](EditController& Editor) { return Apply(Editor, Finish); });
				}
			})
			[
				// The current finish, re-read every frame (undo, another tool...).
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)
					[
						SNew(SBorder).BorderImage(Studio::Box(FLinearColor::White, 3.0f)).Padding(1.0f)
						.BorderBackgroundColor(Floor101::Studio::Palette().SwatchEdge)
						[
							// The same picture as the catalogue: an imported texture, a pattern, or the plain colour.
							SNew(SImage)
							.Image_Lambda([this, Current]() -> const FSlateBrush*
							{
								const FString Id = Current();
								const std::string Utf8 = TCHAR_TO_UTF8(*Id);
								if (f101::cad::isAssetRef(Utf8))
								{
									return GetAssetBrush(Id);
								}
								const f101::cad::FinishInfo* Finish = f101::cad::findFinish(Utf8);
								return Finish && Finish->pattern != f101::cad::FinishPattern::Plain && Finish->opacity >= 1.0 ? GetCatalogueBrush(Id) : Floor101::Studio::White();
							})
							.ColorAndOpacity_Lambda([Current]()
							{
								const std::string Id = TCHAR_TO_UTF8(*Current());
								const f101::cad::FinishInfo* Finish = f101::cad::findFinish(Id);
								if (f101::cad::isAssetRef(Id) || (Finish && Finish->pattern != f101::cad::FinishPattern::Plain && Finish->opacity >= 1.0))
								{
									return FSlateColor(FLinearColor::White);
								}
								return FSlateColor(Id.empty() || !Finish ? Floor101::Studio::Palette().Missing : FinishSwatch(Id));
							})
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Studio::Text(Studio::EText::Label)).ColorAndOpacity(Studio::Palette().Text1).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text_Lambda([this, Current]() { return FinishName(TCHAR_TO_UTF8(*Current()), CurrentProject()); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 2.0f, 0.0f)
				[Floor101::Ui::Icon(EFloor101Icon::ChevronDown, 16.0f, FSlateColor(Studio::Palette().Text3))]
			]
		];
	*Self = Combo;
	return Widget;
}

TSharedRef<SWidget> SFloor101Overlay::MakeWallFaceRow(f101::cad::WallSide Side)
{
	return Floor101::Ui::Row(TAttribute<FText>::CreateLambda([this, Side]()
		{
			const f101::cad::Wall* Wall = GetSelectedWall();
			return Wall ? FaceDirection(*Wall, Side) : FText::GetEmpty();
		}),
		MakeFinishCombo(f101::cad::Surface::Wall,
			[this, Side]() -> FString
			{
				const f101::cad::Wall* Wall = GetSelectedWall();
				const std::string_view Own = Wall ? std::string_view(Wall->finish(Side)) : std::string_view();
				return UTF8_TO_TCHAR(std::string(f101::cad::resolveFinish(Own, f101::cad::Surface::Wall)).c_str());
			},
			[Side](EditController& Editor, const std::string& Finish) { return Editor.setWallFinish(Side, Finish); }));
}

TSharedRef<SWidget> SFloor101Overlay::MakePaintPalette()
{
	namespace Studio = Floor101::Studio;
	using f101::cad::FinishCategory;
	using f101::cad::Surface;
	// Named previews share filtering, selection and favorites with the furniture catalogue.
	auto Swatch = [this](const std::string& Id)
	{
		return MakeCatalogueTile(MakeSwatch(Id, 84.0f), FinishName(Id), FinishName(Id),
			[this, Id]() { const auto* S = Document.Get(); return S && S->GetEditController().paintFinish() == Id; },
			[this, Id]() { return Run([&Id](EditController& Editor) { return Editor.setPaintFinish(Id); }); },
			FString(TEXT("finish:")) + UTF8_TO_TCHAR(Id.c_str()), false);
	};
	struct FSection
	{
		FinishCategory Category;
		FText Name;
	};
	const FSection Sections[] = {
		{FinishCategory::Paint, LOCTEXT("SectionPaint", "Pinturas")},
		{FinishCategory::Tiles, LOCTEXT("SectionTiles", "Baldosas y azulejos")},
		{FinishCategory::Wood, LOCTEXT("SectionWood", "Madera, laminado y parquet")},
		{FinishCategory::Stone, LOCTEXT("SectionStone", "Piedra, hormigón y ladrillo")},
		{FinishCategory::Textile, LOCTEXT("SectionTextile", "Moquetas")},
	};
	// A photographic texture of the library (ADR-0019): added to the project when chosen.
	const FString LibraryFolder = UFloor101DocumentSubsystem::GetLibraryFolder();
	auto LibrarySwatch = [this, &LibraryFolder](const f101::assets::LibraryTexture& Item)
	{
		const std::string Id = Item.id;
		const FString PicturePath = FPaths::Combine(LibraryFolder, UTF8_TO_TCHAR(Item.file.c_str()));
		return MakeCatalogueTile(SNew(SImage).Image_Lambda([this, PicturePath]() { return GetPictureBrush(PicturePath); }),
			FText::FromString(UTF8_TO_TCHAR(Item.name.c_str())),
			FText::Format(LOCTEXT("LibraryTextureTip", "{0}\n{1}"), FText::FromString(UTF8_TO_TCHAR(Item.name.c_str())), FText::FromString(UTF8_TO_TCHAR(Item.credit.c_str()))),
			[this, Id]() { return IsLibraryItemInUse(Id, false); }, [this, Id]() { return UseLibraryItem(Id, false); },
			FString(TEXT("library-texture:")) + UTF8_TO_TCHAR(Id.c_str()), false);
	};
	const f101::assets::Library& Library = UFloor101DocumentSubsystem::GetLibrary();
	// Both tabs are built once; each shows while it is the chosen one.
	TSharedRef<SVerticalBox> Groups = SNew(SVerticalBox);
	for (const bool bFloors : {false, true})
	{
		const Surface Target = bFloors ? Surface::Floor : Surface::Wall;
		for (const FSection& Section : Sections)
		{
			TSharedRef<SWrapBox> Grid = SNew(SWrapBox).UseAllottedSize(true);
			TArray<TPair<FString, FString>> Candidates;
			int32 Count = 0;
			for (const f101::cad::FinishInfo& Finish : f101::cad::finishCatalogue())
			{
				if (Finish.category == Section.Category && Finish.allows(Target))
				{
					Grid->AddSlot().Padding(0.0f, 0.0f, 4.0f, 4.0f)[Swatch(std::string(Finish.id))];
					Candidates.Emplace(FinishName(Finish.id).ToString(), FString(TEXT("finish:")) + UTF8_TO_TCHAR(std::string(Finish.id).c_str()));
					++Count;
				}
			}
			for (const f101::assets::LibraryTexture& Item : Library.textures)
			{
				if (Item.category == Section.Category && (Item.texture.surfaces & static_cast<std::uint8_t>(Target)) != 0)
				{
					Grid->AddSlot().Padding(0.0f, 0.0f, 4.0f, 4.0f)[LibrarySwatch(Item)];
					Candidates.Emplace(UTF8_TO_TCHAR(Item.name.c_str()), FString(TEXT("library-texture:")) + UTF8_TO_TCHAR(Item.id.c_str()));
					++Count;
				}
			}
			if (Count == 0)
			{
				continue;
			}
			auto Shown = [this, bFloors, Candidates]()
			{
				if (bPaintFloors != bFloors) { return EVisibility::Collapsed; }
				for (const auto& Candidate : Candidates)
				{
					if (Floor101::Catalogue::Matches(Candidate.Key + TEXT(" ") + Candidate.Value, MaterialQuery, bMaterialFavoritesOnly, FavoriteKeys.Contains(Candidate.Value))) { return EVisibility::Visible; }
				}
				return EVisibility::Collapsed;
			};
			Groups->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 6.0f)
			[
				SNew(STextBlock).Visibility_Lambda(Shown).Text(Section.Name.ToUpper()).Font(Studio::Text(Studio::EText::Overline)).ColorAndOpacity(Studio::Palette().Text3)
			];
			Groups->AddSlot().AutoHeight()
			[
				SNew(SBox).Visibility_Lambda(Shown)[Grid]
			];
		}
	}
	TArray<Floor101::Ui::FSegment> Targets;
	Targets.Add({{}, LOCTEXT("PaintWalls", "Paredes"), FText::GetEmpty(), FText::GetEmpty(), [this]() { return !bPaintFloors; },
		[this]() { bPaintFloors = false; FSlateApplication::Get().SetAllUserFocusToGameViewport(); }});
	Targets.Add({{}, LOCTEXT("PaintFloors", "Suelos"), FText::GetEmpty(), FText::GetEmpty(), [this]() { return bPaintFloors; },
		[this]() { bPaintFloors = true; FSlateApplication::Get().SetAllUserFocusToGameViewport(); }});

	return SNew(SVerticalBox)
		.Visibility_Lambda([this]()
		{
			const UFloor101DocumentSubsystem* S = Document.Get();
			return S && S->GetEditController().mode() == EditMode::Paint ? EVisibility::Visible : EVisibility::Collapsed;
		})
		+ SVerticalBox::Slot().AutoHeight()[Floor101::Ui::Section(LOCTEXT("GroupCatalogue", "Catálogo"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 0.0f)[MakeCatalogueFilters(false)]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 4.0f).HAlign(HAlign_Left)[Floor101::Ui::Segmented(Targets, true, true)]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 12.0f, 0.0f)
		[
			SNew(SBox)
			.MaxDesiredHeight(420.0f)
			[
				SNew(SScrollBox).ScrollBarStyle(&Studio::ScrollBar())
				+ SScrollBox::Slot()
				[
					Groups
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[Floor101::Ui::Section(LOCTEXT("GroupImportedTextures", "Texturas importadas"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 12.0f)
		[
			MakeImportedTextures()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
		[
			Floor101::Ui::Note(TAttribute<FText>::CreateLambda([this]()
			{
				return bPaintFloors
					? LOCTEXT("PaintFloorsHint", "Clic en el suelo de una habitación (rodeada de muros). El techo se elige al seleccionar la habitación.")
					: LOCTEXT("PaintWallsHint", "Clic en una cara de muro: cada cara lleva su propio acabado.");
			}))
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakeRoomSection()
{
	namespace Ui = Floor101::Ui;
	auto RoomFinish = [this](bool bFloor)
	{
		return [this, bFloor]() -> FString
		{
			const UFloor101DocumentSubsystem* S = Document.Get();
			if (!S)
			{
				return FString();
			}
			const std::string& Key = S->GetEditController().selectedRoom();
			const f101::cad::RoomFinish* Finish = S->GetDocument().project().findRoomFinish(Key);
			const std::string_view Own = Finish ? std::string_view(bFloor ? Finish->floor : Finish->ceiling) : std::string_view();
			return UTF8_TO_TCHAR(std::string(f101::cad::resolveFinish(Own, bFloor ? f101::cad::Surface::Floor : f101::cad::Surface::Ceiling)).c_str());
		};
	};

	auto Roof = [this]() -> const f101::cad::RoomRoof*
	{
		const auto* S = Document.Get();
		return S ? S->GetDocument().project().findRoomRoof(S->GetEditController().selectedRoom()) : nullptr;
	};
	auto Baseboard = [this]() -> const f101::cad::RoomBaseboard*
	{
		const auto* S = Document.Get();
		return S ? S->GetDocument().project().findRoomBaseboard(S->GetEditController().selectedRoom()) : nullptr;
	};

	// How its name and area are written on the plan (ADR-0035).
	auto Label = [this]() -> f101::cad::RoomLabel
	{
		const auto* S = Document.Get();
		return S ? S->GetEditController().selectedRoomLabel() : f101::cad::RoomLabel{};
	};

	return SNew(SVerticalBox)
		.Visibility_Lambda([this]() { return GetSelectedRoom() ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupRoom", "Habitación"), Stack({
				MakeTextRow(LOCTEXT("RoomName", "Nombre"), [this]()
				{
					const UFloor101DocumentSubsystem* S = Document.Get();
					const f101::cad::RoomName* Entry = S ? S->GetDocument().project().findRoomName(S->GetEditController().selectedRoom()) : nullptr;
					return Entry ? FText::FromString(UTF8_TO_TCHAR(Entry->name.c_str())) : FText::GetEmpty();
				}, &EditController::setSelectedRoomName, false),
				Ui::Note(LOCTEXT("RoomNameHint", "Nombre de habitación. Ej.: Cocina, Baño o Sala. Vacío: nombre automático."))}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupRoomTexts", "Rótulo en el plano"), Stack({
				MakePropertyRow(LOCTEXT("RoomNameSize", "Tamaño del nombre"), [Label]() -> TOptional<double>
				{ return static_cast<double>(Label().nameHeightMm); }, &EditController::setSelectedRoomNameHeight),
				MakePropertyRow(LOCTEXT("RoomAreaSize", "Tamaño del área"), [Label]() -> TOptional<double>
				{ return static_cast<double>(Label().areaHeightMm); }, &EditController::setSelectedRoomAreaHeight),
				Ui::Row(LOCTEXT("RoomTextsPlace", "Posición"),
					SNew(SBox).HAlign(HAlign_Left)
					[
						Ui::Button(Floor101::Studio::EButton::Ghost, LOCTEXT("RoomTextsAuto", "Colocación automática"),
							[this]() { Run([](EditController& E) { return E.resetSelectedRoomTextPositions(); }); }, {},
							LOCTEXT("RoomTextsAutoTip", "Devuelve el nombre y el área a su sitio automático, apartados de las cotas. Conserva los tamaños."),
							FText::GetEmpty(), TAttribute<bool>::CreateLambda([Label]() { const f101::cad::RoomLabel L = Label(); return L.nameOffset || L.areaOffset; }))
					]),
				Ui::Note(LOCTEXT("RoomTextsHint", "Altura de las letras en el plano. Para mover el nombre o el área, arrástralos en el plano 2D (Tab)."))}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Group(LOCTEXT("GroupFinishes", "Acabados"), Stack({
				Ui::Row(LOCTEXT("RoomFloor", "Suelo"), MakeFinishCombo(f101::cad::Surface::Floor, RoomFinish(true),
					[](EditController& Editor, const std::string& Finish) { return Editor.setSelectedRoomFloor(Finish); })),
				Ui::Row(LOCTEXT("RoomCeiling", "Techo"), MakeFinishCombo(f101::cad::Surface::Ceiling, RoomFinish(false),
					[](EditController& Editor, const std::string& Finish) { return Editor.setSelectedRoomCeiling(Finish); })),
				Ui::Row(LOCTEXT("RoomTranslucentRoof", "Teja translúcida"),
					SNew(SBox).HAlign(HAlign_Left)
					[Ui::Switch(TAttribute<bool>::CreateLambda([Roof]() { return Roof() != nullptr; }),
						[this](bool bOn) { Run([bOn](EditController& E) { return E.setSelectedRoomRoof(bOn); }); })],
					LOCTEXT("TranslucentRoofTip", "Sustituye la losa por teja translúcida en la última planta. Se muestra con Ver todas, Recorrer y Foto.")),
				SNew(SVerticalBox)
				.Visibility_Lambda([Roof]() { return Roof() ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[MakePropertyRow(LOCTEXT("RoofRise", "Desnivel"), [Roof]() -> TOptional<double>
				{ const auto* R = Roof(); return R ? TOptional<double>(static_cast<double>(R->riseMm)) : TOptional<double>(); }, &EditController::setSelectedRoofRise)]
				+ SVerticalBox::Slot().AutoHeight()
				[Ui::Button(Floor101::Studio::EButton::Secondary, LOCTEXT("RotateRoofFall", "Girar caída 90°"), [this]() { Run([](EditController& E) { return E.rotateSelectedRoof(); }); })],
				Ui::Row(LOCTEXT("RoomBaseboards", "Rodapiés"),
					SNew(SBox).HAlign(HAlign_Left)
					[
						Ui::Switch(TAttribute<bool>::CreateLambda([Baseboard]() { const auto* B = Baseboard(); return B && B->enabled; }),
							[this](bool bOn) { Run([bOn](EditController& E) { return E.setSelectedRoomBaseboard(bOn); }); })
					],
					LOCTEXT("BaseboardTip", "Añade rodapiés al perímetro interior, respetando los huecos de puertas y ventanas bajas.")),
				SNew(SVerticalBox)
				.Visibility_Lambda([Baseboard]() { const auto* B = Baseboard(); return B && B->enabled ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakePropertyRow(LOCTEXT("BaseboardHeight", "Altura"), [Baseboard]() -> TOptional<double>
					{ const auto* B = Baseboard(); return B ? TOptional<double>(static_cast<double>(B->heightMm)) : TOptional<double>(); }, &EditController::setSelectedBaseboardHeight)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					MakePropertyRow(LOCTEXT("BaseboardThickness", "Espesor"), [Baseboard]() -> TOptional<double>
					{ const auto* B = Baseboard(); return B ? TOptional<double>(static_cast<double>(B->thicknessMm)) : TOptional<double>(); }, &EditController::setSelectedBaseboardThickness)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					Ui::Row(LOCTEXT("BaseboardFinish", "Acabado"), MakeFinishCombo(f101::cad::Surface::Carpentry, [Baseboard]() -> FString
					{ const auto* B = Baseboard(); return UTF8_TO_TCHAR(std::string(f101::cad::resolveFinish(B ? std::string_view(B->finish) : std::string_view(), f101::cad::Surface::Carpentry)).c_str()); },
						[](EditController& E, const std::string& Finish) { return E.setSelectedBaseboardFinish(Finish); }))
				]}))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
		[
			Ui::Note(LOCTEXT("RoomHint", "El techo es la cara inferior del forjado: se ve desde dentro de la habitación con \"Ver todas\"."))
		];
}

std::optional<f101::cad::Dimension> SFloor101Overlay::GetSelectedDimension() const
{
	const auto* S = Document.Get();
	return S ? S->GetEditController().selectedDimension() : std::nullopt;
}

TSharedRef<SWidget> SFloor101Overlay::MakeDimensionSection()
{
	namespace Ui = Floor101::Ui;
	auto D = [this]() { return GetSelectedDimension(); };
	TSharedRef<SVerticalBox> Fields = SNew(SVerticalBox);
	const FText Names[] = {LOCTEXT("DimensionStartX", "Inicio X"), LOCTEXT("DimensionStartY", "Inicio Y"),
		LOCTEXT("DimensionEndX", "Fin X"), LOCTEXT("DimensionEndY", "Fin Y")};
	for (int Coordinate = 0; Coordinate < 4; ++Coordinate)
	{
		Fields->AddSlot().AutoHeight().Padding(0.0f, Coordinate > 0 ? 6.0f : 0.0f, 0.0f, 0.0f)
		[MakeEditRow(Names[Coordinate], [D, Coordinate]()
		{
			const auto Value = D();
			if (!Value) return FText::GetEmpty();
			const int64 Mm = Coordinate == 0 ? Value->start.x_mm : Coordinate == 1 ? Value->start.y_mm : Coordinate == 2 ? Value->end.x_mm : Value->end.y_mm;
			return FText::FromString(FString::Printf(TEXT("%lld mm"), Mm));
		}, [this, Coordinate](const FString& Text)
		{
			const std::string Value = TCHAR_TO_UTF8(*Text);
			Run([&Value, Coordinate](EditController& E) { return E.setDimensionCoordinate(Value, Coordinate); });
		})];
	}
	return SNew(SVerticalBox).Visibility_Lambda([this, D]()
	{
		const auto* S = Document.Get();
		return D() || (S && S->GetEditController().mode() == EditMode::Dimension) ? EVisibility::Visible : EVisibility::Collapsed;
	})
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 4.0f, 16.0f, 14.0f)
		[
			SNew(SBox).Visibility_Lambda([D]() { return D() ? EVisibility::Collapsed : EVisibility::Visible; })
			[Ui::Note(LOCTEXT("DimensionHow", "Elige dos puntos exactos y coloca la línea con un tercer clic. Sin ajuste a rejilla ni ejes; precisión de 1 mm."))]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[SNew(SVerticalBox).Visibility_Lambda([D]() { return D() ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight()
			[
				Group(LOCTEXT("GroupDimension", "Cota"), Stack({
					MakePropertyRow(LOCTEXT("DimensionOffset", "Separación"), [D]() -> TOptional<double>
					{ const auto Value = D(); return Value ? TOptional<double>(static_cast<double>(Value->offsetMm)) : TOptional<double>(); }, &EditController::setDimensionOffset),
					MakeTextRow(LOCTEXT("DimensionText", "Rótulo"), [D]()
					{ const auto Value = D(); return Value ? FText::FromString(UTF8_TO_TCHAR(Value->text.c_str())) : FText::GetEmpty(); }, &EditController::setDimensionText, false),
					SNew(SBox).HAlign(HAlign_Right)
					[Ui::Button(Floor101::Studio::EButton::Secondary, LOCTEXT("DimensionResetText", "Usar medida real"),
						[this]() { Run([](EditController& E) { return E.setDimensionText(""); }); }, {}, FText::GetEmpty(), FText::GetEmpty(), true, 30.0f)]}))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).Visibility_Lambda([D]() { const auto Value = D(); return Value && !Value->automatic ? EVisibility::Visible : EVisibility::Collapsed; })
				[Group(LOCTEXT("GroupEnds", "Extremos"), Fields)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 0.0f, 16.0f, 14.0f)
			[
				Ui::Note(LOCTEXT("DimensionTextHint", "Arrastra los cuadrados para cambiar los extremos. Una automática se convierte en manual. Rótulo vacío = medida real; separación negativa = otro lado."))
			]];
}

FText SFloor101Overlay::GetHintText() const
{
	if (const UFloor101GameViewportClient* Client = ViewportClient.Get(); Client && Client->IsNavigationMode())
	{
		return Client->IsPlanView()
			? LOCTEXT("NavigationPlanHint", "Navegar · WASD: desplazar el plano · Shift: rápido · rueda: zoom · N o Esc: volver a editar")
			: LOCTEXT("NavigationHint", "Navegar · WASD: moverse · Q/E: bajar/subir · Shift: rápido · derecho: mirar · N o Esc: volver a editar");
	}
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (!Subsystem)
	{
		return FText::GetEmpty();
	}
	if (IsPhotoMode())
	{
		return LOCTEXT("HintPhoto", "Foto · Shift + derecho: mirar sin moverse · derecho: orbitar · central: desplazar · rueda: zoom · al mover la cámara la imagen empieza de nuevo · R o Esc: volver a editar");
	}
	const EditController& Editor = Subsystem->GetEditController();
	if (Editor.mode() == EditMode::Dimension)
	{
		return Editor.dimensionPointCount() == 0 ? LOCTEXT("DimensionStart", "Cota · clic: primer punto · Esc: cancelar") :
			Editor.dimensionPointCount() == 1 ? LOCTEXT("DimensionEnd", "Cota · clic: segundo punto · Esc: cancelar") :
			LOCTEXT("DimensionPlace", "Cota · clic: colocar línea · Esc: cancelar");
	}
	if (GetSelectedDimension()) return LOCTEXT("DimensionSelectedHint", "Cota · extremos: cambiar medida · línea: mover · Supr: quitar · Ctrl+Z: deshacer");
	if (Editor.mode() == EditMode::Wall)
	{
		return Editor.wallTool().state() == f101::cad::WallTool::State::Drafting
			? LOCTEXT("HintDrafting", "Muro · mueve el cursor o escribe la longitud (sin unidad = cm) · Enter o clic: crear · cerca de un muro se une a él · Esc o clic derecho: terminar")
			: LOCTEXT("HintWallIdle", "Muro · clic: inicio del muro (en un extremo o sobre un muro para unirlo) · Esc o clic derecho: volver a Seleccionar");
	}
	if (Editor.mode() == EditMode::Paint)
	{
		// What a click would paint: a named wall face, a room floor, or why there is nothing.
		const f101::cad::Project& Project = Subsystem->GetDocument().project();
		FText Target;
		if (const std::optional<f101::cad::PaintTarget>& At = Editor.paintTarget())
		{
			if (const f101::cad::Wall* Wall = At->wall.isNil() ? nullptr : Project.findWall(At->wall))
			{
				Target = FText::Format(LOCTEXT("PaintWallFace", "{0} del muro"), FaceDirection(*Wall, At->side));
			}
			else
			{
				const FString Key = UTF8_TO_TCHAR(At->room.c_str());
				const FFloor101RoomView* Room = Subsystem->GetRooms().FindByPredicate([&Key](const FFloor101RoomView& View) { return View.Key == Key; });
				FNumberFormattingOptions Area;
				Area.SetMaximumFractionalDigits(2);
				Target = Room ? FText::Format(LOCTEXT("PaintRoomFloor", "suelo de la habitación ({0} m²)"), FText::AsNumber(Room->AreaM2, &Area))
					: LOCTEXT("PaintFloor", "suelo de la habitación");
			}
		}
		else
		{
			const std::string& Finish = Editor.paintFinish();
			const bool bWalls = f101::cad::validateFinishIn(Project, Finish, f101::cad::Surface::Wall).ok();
			const bool bFloors = f101::cad::validateFinishIn(Project, Finish, f101::cad::Surface::Floor).ok();
			Target = bFloors && !bWalls ? LOCTEXT("PaintNeedsRoom", "apunta al suelo de una habitación cerrada por muros")
				: bWalls && !bFloors ? LOCTEXT("PaintNeedsWall", "apunta a una cara de muro")
				: LOCTEXT("PaintNeedsEither", "apunta a una cara de muro o al suelo de una habitación");
		}
		return FText::Format(LOCTEXT("HintPaint", "Pintar con {0} · {1} · Esc o clic derecho: volver a Seleccionar"),
			FinishName(Editor.paintFinish(), &Project), Target);
	}
	if (Editor.mode() == EditMode::Door || Editor.mode() == EditMode::Window)
	{
		const bool bDoor = Editor.mode() == EditMode::Door;
		const FText Type = bDoor ? OperationName(f101::cad::OpeningKind::Door, Editor.settings().door.operation)
			: OperationName(f101::cad::OpeningKind::Window, Editor.settings().window.operation);
		return FText::Format(bDoor
			? LOCTEXT("HintDoor", "Puerta: {0} · apunta a un muro: abre hacia el lado al que apuntas · Espacio: bisagras al otro lado · clic: colocarla · Esc o clic derecho: volver a Seleccionar")
			: LOCTEXT("HintWindow", "Ventana: {0} · apunta a un muro: abre hacia el lado al que apuntas · Espacio: bisagras al otro lado · clic: colocarla · Esc o clic derecho: volver a Seleccionar"),
			Type);
	}
	if (Editor.mode() == EditMode::Furniture)
	{
		return FText::Format(LOCTEXT("HintFurniture", "Amueblar: {0} · clic en el suelo: colocarlo · cerca de un muro se apoya en él de espaldas · Espacio: girar 90° · Esc o clic derecho: volver a Seleccionar"),
			FurnitureName(Editor.settings().furnitureType, &Subsystem->GetDocument().project()));
	}
	if (Editor.mode() == EditMode::Column)
	{
		return FText::Format(LOCTEXT("HintColumn", "Pilar {0} · clic: colocarlo · se ajusta a las esquinas y a los ejes de los muros · Espacio: girar 90° · Esc o clic derecho: volver a Seleccionar"),
			Floor101::ColumnShapeName(Editor.settings().columnShape).ToLower());
	}
	if (Editor.mode() == EditMode::Services)
	{
		const FText Kind = Floor101::ServiceKindName(Editor.settings().serviceKind);
		const FText Height = LengthText(static_cast<double>(Editor.settings().serviceHeightMm));
		return Editor.runPoints().empty()
			? FText::Format(LOCTEXT("HintServicesIdle", "Instalación: {0} a {1} · clic: primer punto · X: ocultar las instalaciones · Esc o clic derecho: volver a Seleccionar"), Kind, Height)
			: FText::Format(LOCTEXT("HintServicesDrawing", "Instalación: {0} a {1} · clic: siguiente punto · doble clic o Enter: terminar · Retroceso: quitar el último · Esc o clic derecho: descartar"), Kind, Height);
	}
	if (Editor.isDragging() && Editor.selectedColumn())
	{
		return LOCTEXT("HintDraggingColumn", "Moviendo pilar · se ajusta a las esquinas, a los ejes de los muros y a la rejilla · suelta para aplicar · Esc o clic derecho: cancelar");
	}
	if (Editor.isDragging() && Editor.selectedServiceRun())
	{
		return LOCTEXT("HintDraggingRun", "Moviendo instalación · se ajusta a la rejilla (un codo, también a los muros) · suelta para aplicar · Esc o clic derecho: cancelar");
	}
	if (Editor.isDraggingOpening())
	{
		return LOCTEXT("HintDraggingOpening", "Moviendo hueco · se ajusta a la rejilla y se aparta de los muros que se unen · suelta para aplicar · Esc o clic derecho: cancelar");
	}
	if (Editor.isDraggingFurniture())
	{
		return LOCTEXT("HintDraggingFurniture", "Moviendo mueble · se ajusta a la rejilla y se apoya en los muros · suelta para aplicar · Esc o clic derecho: cancelar");
	}
	if (Editor.isDragging())
	{
		return LOCTEXT("HintDragging", "Moviendo esquina · los muros unidos la siguen · suelta para aplicar · Esc o clic derecho: cancelar");
	}
	if (!Editor.selectedRoom().empty())
	{
		return LOCTEXT("HintRoom", "Seleccionar · elige el suelo y el techo de la habitación en el panel");
	}
	if (Editor.selectedOpening())
	{
		return LOCTEXT("HintOpening", "Seleccionar · arrastra el hueco para moverlo o sus asas para cambiar el ancho · Espacio: invertir bisagras · tipo, apertura y carpintería en el panel · Supr: eliminar");
	}
	if (Editor.selectedFurniture())
	{
		return LOCTEXT("HintSelectedFurniture", "Seleccionar · arrastra el mueble para moverlo · Espacio: girar 90° · medidas, giro y acabado en el panel · Supr: eliminar");
	}
	if (Editor.selectedColumn())
	{
		return LOCTEXT("HintSelectedColumn", "Seleccionar · arrastra el pilar para moverlo · Espacio: girar 90° · forma, medidas y acabado en el panel · Supr: eliminar");
	}
	if (Editor.selectedServiceRun())
	{
		return LOCTEXT("HintSelectedRun", "Seleccionar · arrastra la tubería para moverla o un asa para mover ese codo · tipo en el panel · Supr: eliminar");
	}
	return Editor.selection().isNil()
		? LOCTEXT("HintSelect", "Seleccionar · clic: muro, hueco, mueble, pilar, instalación o habitación · B: muros · P: puertas · O: ventanas · M: pintar · G: amueblar · C: pilares · I: instalaciones · X: rayos X · Tab: plano 2D · RePág/AvPág: planta")
		: LOCTEXT("HintSelected", "Seleccionar · arrastra un asa amarilla para mover la esquina · Supr: eliminar");
}

FText SFloor101Overlay::GetCursorText() const
{
	if (const UFloor101GameViewportClient* Client = ViewportClient.Get(); Client && Client->IsNavigationMode())
	{
		return FText::GetEmpty();
	}
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	if (!Subsystem)
	{
		return FText::GetEmpty();
	}
	const EditController& Editor = Subsystem->GetEditController();
	if (Editor.mode() == EditMode::Services)
	{
		// The height of the next point and, while drawing, how long the stretch to it is in plan.
		const std::optional<f101::cad::ServicePoint>& At = Editor.runPointer();
		if (!At)
		{
			return FText::GetEmpty();
		}
		const FText Height = FText::Format(LOCTEXT("CursorRunHeight", "altura {0}"), LengthText(static_cast<double>(At->z_mm)));
		if (Editor.runPoints().empty())
		{
			return Height;
		}
		const f101::cad::ServicePoint& Last = Editor.runPoints().back();
		const double Stretch = std::hypot(static_cast<double>(At->x_mm - Last.x_mm), static_cast<double>(At->y_mm - Last.y_mm));
		return FText::Format(LOCTEXT("CursorRunStretch", "{0} · {1}"), LengthText(Stretch), Height);
	}
	// The wall being drawn, the selected wall whose corner is being dragged, or the wall of the dragged opening.
	const f101::cad::EditView View = Editor.view();
	const auto Shown = std::find_if(View.previewWalls.begin(), View.previewWalls.end(),
		[&View](const f101::cad::Wall& Wall) { return Wall.id == View.selectedWall; });
	if (Shown == View.previewWalls.end())
	{
		if (const auto* Selected = GetSelectedWall())
		{
			return FText::Format(LOCTEXT("StudioWallMeasures", "L {0} · E {1} · H {2}"), LengthText(Selected->lengthMm()),
				LengthText(static_cast<double>(Selected->thicknessMm)), LengthText(static_cast<double>(Selected->heightMm)));
		}
		return FText::GetEmpty();
	}
	if (Editor.isDraggingOpening())
	{
		const std::optional<f101::cad::OpeningRef> Selected = Editor.selectedOpening();
		const f101::cad::Opening* Dragged = Selected ? Shown->findOpening(Selected->opening.id) : nullptr;
		return Dragged ? FText::Format(LOCTEXT("CursorOpening", "Posición {0} · ancho {1}"),
			LengthText(static_cast<double>(Dragged->offsetMm)), LengthText(static_cast<double>(Dragged->widthMm))) : FText::GetEmpty();
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
	namespace Studio = Floor101::Studio;
	const Studio::FPalette& P = Studio::Palette();
	auto TypedError = [this]() -> FText
	{
		const UFloor101DocumentSubsystem* S = Document.Get();
		if (!S || S->GetEditController().mode() != EditMode::Wall || S->GetEditController().wallTool().typedTextError().empty())
		{
			return FText::GetEmpty();
		}
		return FText::FromString(UTF8_TO_TCHAR(S->GetEditController().wallTool().typedTextError().c_str()));
	};
	// Over the selected wall: length, thickness and height, set like the figures of a drawing.
	TSharedRef<SHorizontalBox> Measures = SNew(SHorizontalBox)
		.Visibility_Lambda([this]() { return bCursorAnchored ? EVisibility::Visible : EVisibility::Collapsed; });
	for (int32 Which = 0; Which < 3; ++Which)
	{
		const FText Letter = Which == 0 ? INVTEXT("L") : Which == 1 ? INVTEXT("E") : INVTEXT("H");
		Measures->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(Which > 0 ? 14.0f : 0.0f, 0.0f, 6.0f, 0.0f)
			[SNew(STextBlock).Text(Letter).Font(Studio::Text(Studio::EText::Overline)).ColorAndOpacity(P.Brass)];
		Measures->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Studio::Text(Studio::EText::NumericStrong)).ColorAndOpacity(P.Text1)
				.Text_Lambda([this, Which]()
				{
					const f101::cad::Wall* Wall = GetSelectedWall();
					if (!Wall)
					{
						return FText::GetEmpty();
					}
					return LengthText(Which == 0 ? Wall->lengthMm() : Which == 1 ? static_cast<double>(Wall->thicknessMm) : static_cast<double>(Wall->heightMm));
				})
			];
	}
	return SNew(SVerticalBox)
		.Visibility_Lambda([this, TypedError]()
		{
			return GetCursorText().IsEmpty() && TypedError().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
		})
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage_Lambda([this, TypedError]()
			{
				const auto& Colors = Floor101::Studio::Palette();
				return Floor101::Studio::Box(Colors.Bar, 7.0f, !TypedError().IsEmpty() ? FLinearColor(Colors.Danger.R, Colors.Danger.G, Colors.Danger.B, 0.6f) : Colors.BrassLine, 1.0f);
			})
			.Padding(FMargin(12.0f, 7.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Measures]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Studio::Text(Studio::EText::NumericLarge)).ColorAndOpacity(P.Text1).Text_Lambda([this]() { return GetCursorText(); })
					.Visibility_Lambda([this]() { return bCursorAnchored || GetCursorText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Danger)
					.Visibility_Lambda([TypedError]() { return TypedError().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					.Text_Lambda(TypedError)
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			// A leader down to the wall, like the extension line of a dimension.
			SNew(SBox).WidthOverride(1.0f).HeightOverride(8.0f).Visibility_Lambda([this]() { return bCursorAnchored ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[SNew(SBorder).BorderImage(Studio::White()).BorderBackgroundColor(P.BrassLine).Padding(0.0f)]
		];
}

const f101::cad::Project* SFloor101Overlay::CurrentProject() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	return Subsystem ? &Subsystem->GetDocument().project() : nullptr;
}

const f101::cad::Asset* SFloor101Overlay::GetPaintAsset() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	const std::optional<f101::cad::AssetId> Id =
		Subsystem ? f101::cad::parseAssetRef(Subsystem->GetEditController().paintFinish()) : std::nullopt;
	return Id ? Subsystem->GetDocument().project().findAsset(*Id) : nullptr;
}

const f101::cad::Asset* SFloor101Overlay::GetFurnitureAsset() const
{
	const UFloor101DocumentSubsystem* Subsystem = Document.Get();
	const std::optional<f101::cad::AssetId> Id =
		Subsystem ? f101::cad::parseAssetRef(Subsystem->GetEditController().settings().furnitureType) : std::nullopt;
	return Id ? Subsystem->GetDocument().project().findAsset(*Id) : nullptr;
}

void SFloor101Overlay::ChangeAsset(const f101::cad::Asset& Asset)
{
	if (UFloor101DocumentSubsystem* Subsystem = Document.Get())
	{
		(void)Subsystem->ChangeAsset(Asset);
	}
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

const FSlateBrush* SFloor101Overlay::GetAssetBrush(const FString& FinishId)
{
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	UTexture2D* Texture = Library ? Library->GetAssetFinishImage(FinishId) : nullptr;
	if (!Texture)
	{
		if (!NoImageBrush)
		{
			NoImageBrush = MakeShared<FSlateColorBrush>(Floor101::Studio::Palette().Missing);
		}
		return NoImageBrush.Get();
	}
	TSharedPtr<FSlateBrush>& Brush = AssetBrushes.FindOrAdd(FinishId);
	if (!Brush)
	{
		Brush = MakeShared<FSlateBrush>();
		Brush->ImageSize = FVector2D(16.0, 16.0);
	}
	if (Brush->GetResourceObject() != Texture)
	{
		Brush->SetResourceObject(Texture); // first shown, redefined, or read at last from the project's folder
	}
	return Brush.Get();
}

const FSlateBrush* SFloor101Overlay::GetCatalogueBrush(const FString& FinishId)
{
	if (const TSharedPtr<FSlateBrush>* Found = AssetBrushes.Find(FinishId))
	{
		return Found->Get();
	}
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	UTexture2D* Texture = Library && Library->GetFinishMaterial(FinishId) ? Library->FindFinishTexture(FinishId) : nullptr;
	if (!Texture)
	{
		if (!NoImageBrush)
		{
			NoImageBrush = MakeShared<FSlateColorBrush>(Floor101::Studio::Palette().Missing);
		}
		return NoImageBrush.Get();
	}
	TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Texture);
	Brush->ImageSize = FVector2D(32.0, 32.0); // one repeat of the pattern
	AssetBrushes.Add(FinishId, Brush);
	return Brush.Get();
}

TSharedRef<SWidget> SFloor101Overlay::MakeSwatch(const std::string& FinishId, float Size)
{
	if (f101::cad::isAssetRef(FinishId))
	{
		// Looked up on every paint: the texture's image can change while the swatch is shown.
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[SNew(SImage).Image_Lambda([this, Key = FString(UTF8_TO_TCHAR(FinishId.c_str()))]() { return GetAssetBrush(Key); })];
	}
	if (const f101::cad::FinishInfo* Finish = f101::cad::findFinish(FinishId);
		Finish && Finish->pattern != f101::cad::FinishPattern::Plain && Finish->opacity >= 1.0)
	{
		// A pattern shows best as its texture (made on first sight, like the material).
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[SNew(SImage).Image_Lambda([this, Key = FString(UTF8_TO_TCHAR(FinishId.c_str()))]() { return GetCatalogueBrush(Key); })];
	}
	const FLinearColor Colour = FinishId.empty() ? Floor101::Studio::Palette().Missing : FinishSwatch(FinishId);
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(Colour)
		.Padding(FMargin(Size / 2.0f, Size / 2.0f));
}

TSharedRef<SWidget> SFloor101Overlay::MakeEditRow(const FText& Label, TFunction<FText()> Current, TFunction<void(const FString&)> Apply, bool bNumeric)
{
	return Floor101::Ui::Row(Label, Floor101::Ui::Field(TAttribute<FText>::CreateLambda([Current]() { return Current(); }),
		[Apply](const FString& Typed) { Apply(Typed.TrimStartAndEnd()); }, bNumeric));
}

void SFloor101Overlay::ImportFromDialog(bool bModel)
{
	UFloor101DocumentSubsystem* Subsystem = Document.Get();
	FString Path;
	const bool bChosen = Subsystem && FFloor101FileDialog::OpenFile(
		bModel ? LOCTEXT("ImportModelTitle", "Importar modelo 3D") : LOCTEXT("ImportTextureTitle", "Importar textura"),
		bModel ? LOCTEXT("ModelFiles", "Modelos 3D (glTF, GLB, OBJ)").ToString() : LOCTEXT("ImageFiles", "Imágenes (PNG, JPEG)").ToString(),
		bModel ? TEXT("*.glb;*.gltf;*.obj") : TEXT("*.png;*.jpg;*.jpeg"), Path);
	if (bChosen)
	{
		const f101::Result<f101::cad::AssetId> Imported = Subsystem->ImportAsset(Path);
		if (Imported)
		{
			// Ready to use: Pintar with the texture, Amueblar with the model.
			const std::string Ref = f101::cad::assetRef(Imported.value());
			(void)Run([bModel, &Ref](EditController& Editor)
			{
				Editor.setMode(bModel ? EditMode::Furniture : EditMode::Paint);
				return bModel ? Editor.setFurnitureType(Ref) : Editor.setPaintFinish(Ref);
			});
		}
	}
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

void SFloor101Overlay::RebuildImportedLists()
{
	const f101::cad::Project* Project = CurrentProject();
	for (const bool bModels : {false, true})
	{
		const TSharedPtr<SVerticalBox>& List = bModels ? ImportedModelList : ImportedTextureList;
		if (!List) { continue; }
		List->ClearChildren();
		TSharedRef<SWrapBox> Grid = SNew(SWrapBox).UseAllottedSize(true);
		for (const f101::cad::Asset& Asset : Project ? Project->assets : std::vector<f101::cad::Asset>())
		{
			if ((Asset.kind == f101::cad::AssetKind::Model) != bModels || !Asset.library.empty()) { continue; }
			const std::string Ref = f101::cad::assetRef(Asset.id);
			const FText Name = FText::FromString(UTF8_TO_TCHAR(Asset.name.c_str()));
			const FText Detail = bModels ? FootprintText(Asset.model.widthMm, Asset.model.depthMm)
				: FText::Format(LOCTEXT("ImageSize", "{0} × {1} px"), Asset.texture.pixelWidth, Asset.texture.pixelHeight);
			// Imported models without a photograph have a clearly identified model icon.
			TSharedRef<SWidget> Picture = bModels
				? StaticCastSharedRef<SWidget>(SNew(SBorder).HAlign(HAlign_Center).VAlign(VAlign_Center)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelColor)
					[SNew(SFloor101Icon).Icon(EFloor101Icon::Model).Size(40.0f).Color(FSlateColor(Floor101::Studio::Palette().Text2))]) : MakeSwatch(Ref, 84.0f);
			Grid->AddSlot().Padding(2.0f)
			[MakeCatalogueTile(Picture, Name, FText::Format(LOCTEXT("ImportedPreview", "{0}\n{1}"), Name, Detail),
				[this, Ref, bModels]() { const auto* S = Document.Get(); return S && (bModels ? S->GetEditController().settings().furnitureType == Ref : S->GetEditController().paintFinish() == Ref); },
				[this, Ref, bModels]() { return Run([&Ref, bModels](EditController& Editor) { return bModels ? Editor.setFurnitureType(Ref) : Editor.setPaintFinish(Ref); }); },
				FString(TEXT("asset:")) + UTF8_TO_TCHAR(Ref.c_str()), bModels)];
		}
		List->AddSlot().AutoHeight()[Grid];
	}
}
TSharedRef<SWidget> SFloor101Overlay::MakeImportedTextures()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	auto Texture = [this]() { return GetPaintAsset(); };
	// The settings of an imported texture; a library one comes with its real size (ADR-0019).
	auto Visible = [Texture]() { return Texture() && Texture()->library.empty() ? EVisibility::Visible : EVisibility::Collapsed; };
	auto Changed = [this, Texture](TFunctionRef<bool(f101::cad::Asset&)> Edit)
	{
		if (const f101::cad::Asset* Current = Texture())
		{
			f101::cad::Asset Asset = *Current;
			if (Edit(Asset))
			{
				ChangeAsset(Asset);
			}
		}
	};
	auto LengthRow = [this, Texture, Changed](const FText& Label, std::int64_t f101::cad::TextureAsset::* Field)
	{
		return MakeEditRow(Label,
			[Texture, Field]() { const f101::cad::Asset* A = Texture(); return A ? LengthText(static_cast<double>(A->texture.*Field)) : FText::GetEmpty(); },
			[this, Changed, Field](const FString& Typed)
			{
				const f101::Result<std::int64_t> Millimeters = f101::cad::parseLength(TCHAR_TO_UTF8(*Typed), f101::cad::LengthUnit::Centimeter);
				if (!Millimeters)
				{
					if (UFloor101DocumentSubsystem* S = Document.Get())
					{
						(void)S->Edit([&Millimeters](EditController&) { return f101::Status(Millimeters.error()); });
					}
					return;
				}
				Changed([&Millimeters, Field](f101::cad::Asset& Asset) { Asset.texture.*Field = Millimeters.value(); return true; });
			});
	};

	SAssignNew(ImportedTextureList, SVerticalBox);
	RebuildImportedLists();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			ImportedTextureList.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 6.0f)
		[
			Ui::Button(Studio::EButton::Secondary, LOCTEXT("ImportTexture", "Importar textura…"), [this]() { ImportFromDialog(false); }, EFloor101Icon::Plus,
				LOCTEXT("ImportTextureTip", "Trae una imagen PNG o JPEG al proyecto (se copia junto a project.json) para usarla como acabado"), FText::GetEmpty(), true, 30.0f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda(Visible)
			+ SVerticalBox::Slot().AutoHeight()
			[
				Stack({
					MakeEditRow(LOCTEXT("AssetName", "Nombre"),
						[Texture]() { const f101::cad::Asset* A = Texture(); return A ? FText::FromString(UTF8_TO_TCHAR(A->name.c_str())) : FText::GetEmpty(); },
						[Changed](const FString& Typed) { Changed([&Typed](f101::cad::Asset& Asset) { Asset.name = TCHAR_TO_UTF8(*Typed); return true; }); }, false),
					LengthRow(LOCTEXT("TextureWidth", "Ancho de la imagen"), &f101::cad::TextureAsset::widthMm),
					LengthRow(LOCTEXT("TextureHeight", "Alto de la imagen"), &f101::cad::TextureAsset::heightMm),
					MakeEditRow(LOCTEXT("TextureRoughness", "Rugosidad (%)"),
						[Texture]() { const f101::cad::Asset* A = Texture(); return A ? FText::AsNumber(A->texture.roughnessPercent) : FText::GetEmpty(); },
						[Changed](const FString& Typed)
						{
							Changed([&Typed](f101::cad::Asset& Asset)
							{
								Asset.texture.roughnessPercent = FCString::Atoi(*Typed);
								return true;
							});
						})})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				Ui::Button(Studio::EButton::Danger, LOCTEXT("RemoveTexture", "Eliminar textura"), [this, Texture]()
					{
						const f101::cad::Asset* Current = Texture();
						UFloor101DocumentSubsystem* Subsystem = Document.Get();
						if (Current && Subsystem && Subsystem->RemoveAsset(Current->id).ok())
						{
							(void)Run([](EditController& Editor) { return Editor.setPaintFinish("paint-sand"); });
						}
						FSlateApplication::Get().SetAllUserFocusToGameViewport();
					}, EFloor101Icon::Trash,
					LOCTEXT("RemoveTextureTip", "Quita la textura del proyecto si ninguna superficie la usa (se puede deshacer)"), FText::GetEmpty(), true, 30.0f)
			]
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakeImportedModels()
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	using f101::cad::ModelUnit;
	if (ModelUnits.IsEmpty())
	{
		for (const ModelUnit Unit : {ModelUnit::Meter, ModelUnit::Centimeter, ModelUnit::Millimeter, ModelUnit::Inch})
		{
			ModelUnits.Add(MakeShared<ModelUnit>(Unit));
		}
	}
	auto UnitName = [](ModelUnit Unit)
	{
		switch (Unit)
		{
		case ModelUnit::Centimeter: return LOCTEXT("UnitCm", "centímetros");
		case ModelUnit::Millimeter: return LOCTEXT("UnitMm", "milímetros");
		case ModelUnit::Inch: return LOCTEXT("UnitIn", "pulgadas");
		default: return LOCTEXT("UnitM", "metros");
		}
	};
	auto Model = [this]() { return GetFurnitureAsset(); };
	// The settings of an imported model; a library one comes set up (ADR-0019).
	auto Visible = [Model]() { return Model() && Model()->library.empty() ? EVisibility::Visible : EVisibility::Collapsed; };
	auto Changed = [this, Model](TFunctionRef<void(f101::cad::Asset&)> Edit)
	{
		if (const f101::cad::Asset* Current = Model())
		{
			f101::cad::Asset Asset = *Current;
			Edit(Asset);
			ChangeAsset(Asset);
		}
	};
	auto Toggle = [](const FText& Label, const FText& ToolTip, TFunction<void()> OnClicked)
	{
		return Ui::Button(Studio::EButton::Secondary, Label, OnClicked, {}, ToolTip, FText::GetEmpty(), true, 28.0f);
	};

	SAssignNew(ImportedModelList, SVerticalBox);
	RebuildImportedLists();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			ImportedModelList.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 6.0f)
		[
			Ui::Button(Studio::EButton::Secondary, LOCTEXT("ImportModel", "Importar modelo…"), [this]() { ImportFromDialog(true); }, EFloor101Icon::Plus,
				LOCTEXT("ImportModelTip", "Trae un modelo glTF (.glb, .gltf) u OBJ con sus texturas al proyecto para colocarlo como mueble"), FText::GetEmpty(), true, 30.0f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda(Visible)
			+ SVerticalBox::Slot().AutoHeight()
			[
				Stack({
					MakeEditRow(LOCTEXT("AssetName", "Nombre"),
						[Model]() { const f101::cad::Asset* A = Model(); return A ? FText::FromString(UTF8_TO_TCHAR(A->name.c_str())) : FText::GetEmpty(); },
						[Changed](const FString& Typed) { Changed([&Typed](f101::cad::Asset& Asset) { Asset.name = TCHAR_TO_UTF8(*Typed); }); }, false),
					Ui::Row(LOCTEXT("ModelUnits", "Unidades del archivo"),
						SNew(SBox).HeightOverride(Studio::Size::Field)
						[
							SNew(SComboBox<TSharedPtr<ModelUnit>>).ComboBoxStyle(&Studio::ComboBox()).ItemStyle(&Studio::ListRow()).HasDownArrow(false)
							.IsFocusable(false)
							.OptionsSource(&ModelUnits)
							.OnGenerateWidget_Lambda([UnitName](TSharedPtr<ModelUnit> Unit)
							{
								return SNew(STextBlock).Text(UnitName(*Unit)).Font(Floor101::Studio::Text(Floor101::Studio::EText::Body)).ColorAndOpacity(Floor101::Studio::Palette().Text1);
							})
							.OnSelectionChanged_Lambda([Changed](TSharedPtr<ModelUnit> Unit, ESelectInfo::Type Info)
							{
								if (Unit && Info != ESelectInfo::Direct)
								{
									const ModelUnit Chosen = *Unit;
									Changed([Chosen](f101::cad::Asset& Asset) { Asset.model.unit = Chosen; });
								}
							})
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(Studio::Text(Studio::EText::Label)).ColorAndOpacity(Studio::Palette().Text1)
									.Text_Lambda([Model, UnitName]() { const f101::cad::Asset* A = Model(); return A ? UnitName(A->model.unit) : FText::GetEmpty(); })
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 2.0f, 0.0f)
								[Ui::Icon(EFloor101Icon::ChevronDown, 16.0f, FSlateColor(Studio::Palette().Text3))]
							]
						]),
					SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.0, 6.0))
					+ SWrapBox::Slot()
					[
						Toggle(LOCTEXT("ModelUp", "Cambiar eje arriba"), LOCTEXT("ModelUpTip", "Alterna entre Y arriba (glTF y la mayoría de OBJ) y Z arriba"),
							[Changed]() { Changed([](f101::cad::Asset& Asset) { Asset.model.up = Asset.model.up == f101::cad::ModelUpAxis::Y ? f101::cad::ModelUpAxis::Z : f101::cad::ModelUpAxis::Y; }); })
					]
					+ SWrapBox::Slot()
					[
						Toggle(LOCTEXT("ModelFront", "Girar frente 90°"), LOCTEXT("ModelFrontTip", "Gira el modelo para que su frente sea el que mira a la habitación"),
							[Changed]() { Changed([](f101::cad::Asset& Asset) { Asset.model.frontQuarterTurns = (Asset.model.frontQuarterTurns + 1) % 4; }); })
					],
					Ui::Row(LOCTEXT("ModelAgainstWall", "Se apoya en los muros"),
						SNew(SBox).HAlign(HAlign_Left)
						[
							Ui::Switch(TAttribute<bool>::CreateLambda([Model]() { const f101::cad::Asset* A = Model(); return A && A->model.againstWall; }),
								[Changed](bool bOn) { Changed([bOn](f101::cad::Asset& Asset) { Asset.model.againstWall = bOn; }); })
						]),
					SNew(STextBlock).Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(Studio::Palette().Text3).AutoWrapText(true)
					.Text_Lambda([Model]()
					{
						const f101::cad::Asset* A = Model();
						if (!A)
						{
							return FText::GetEmpty();
						}
						FNumberFormattingOptions Centimetres;
						Centimetres.SetMaximumFractionalDigits(1);
						return FText::Format(LOCTEXT("ModelInfo", "Tamaño {0} × {1} × {2} cm · {3} triángulos"),
							FText::AsNumber(A->model.widthMm / 10.0, &Centimetres), FText::AsNumber(A->model.depthMm / 10.0, &Centimetres),
							FText::AsNumber(A->model.heightMm / 10.0, &Centimetres), FText::AsNumber(A->model.triangles));
					}),
					Ui::Note(TAttribute<FText>::CreateLambda([this, Model]()
					{
						const f101::cad::Asset* A = Model();
						const UFloor101DocumentSubsystem* S = Document.Get();
						const FString Problem = A && S ? S->GetAssetProblem(A->id) : FString();
						return Problem.IsEmpty() ? FText::GetEmpty()
							: FText::Format(LOCTEXT("ModelProblem", "No se puede mostrar ({0}); se ve como una caja"), FText::FromString(Problem));
					}), EFloor101Icon::Warn, FSlateColor(Studio::Palette().Danger))})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				Ui::Button(Studio::EButton::Danger, LOCTEXT("RemoveModel", "Eliminar modelo"), [this, Model]()
					{
						const f101::cad::Asset* Current = Model();
						UFloor101DocumentSubsystem* Subsystem = Document.Get();
						if (Current && Subsystem && Subsystem->RemoveAsset(Current->id).ok())
						{
							(void)Run([](EditController& Editor) { return Editor.setFurnitureType("sofa"); });
						}
						FSlateApplication::Get().SetAllUserFocusToGameViewport();
					}, EFloor101Icon::Trash,
					LOCTEXT("RemoveModelTip", "Quita el modelo del proyecto si ningún mueble lo usa (se puede deshacer)"), FText::GetEmpty(), true, 30.0f)
			]
		];
}

#undef LOCTEXT_NAMESPACE
