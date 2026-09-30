#include "Floor101GameViewportClient.h"

#include "Engine/Console.h"
#include "Engine/World.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "Floor101.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101EditingPawn.h"
#include "Floor101SceneSpace.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UnrealClient.h"

namespace
{
	constexpr double CancelClickMaxPixels = 4.0;

	using f101::cad::EditController;

	FVector ScenePoint(const f101::cad::Point2D& P, double ZMm)
	{
		return FVector(Floor101::Conversion::ToUnrealPosition({static_cast<double>(P.x_mm), static_cast<double>(P.y_mm), ZMm}));
	}
}

UFloor101DocumentSubsystem* UFloor101GameViewportClient::GetEditingTarget()
{
	if (IgnoreInput() || (ViewportConsole && ViewportConsole->ConsoleActive()))
	{
		return nullptr;
	}
	const UWorld* CurrentWorld = GetWorld();
	return CurrentWorld ? CurrentWorld->GetSubsystem<UFloor101DocumentSubsystem>() : nullptr;
}

AFloor101EditingPawn* UFloor101GameViewportClient::GetEditingPawn() const
{
	const UWorld* CurrentWorld = GetWorld();
	const APlayerController* Controller = CurrentWorld ? CurrentWorld->GetFirstPlayerController() : nullptr;
	return Controller ? Cast<AFloor101EditingPawn>(Controller->GetPawn()) : nullptr;
}

bool UFloor101GameViewportClient::GetCursorRay(f101::cad::Ray3& OutRay) const
{
	const UWorld* CurrentWorld = GetWorld();
	const APlayerController* Controller = CurrentWorld ? CurrentWorld->GetFirstPlayerController() : nullptr;
	FVector2D Mouse;
	FVector Origin;
	FVector Direction;
	if (!Controller || !GetMousePosition(Mouse) || !Controller->DeprojectScreenPositionToWorld(Mouse.X, Mouse.Y, Origin, Direction))
	{
		return false;
	}
	const f101::geometry::Vec3 SceneOrigin = Floor101::Conversion::ToScenePosition(Origin);
	const f101::geometry::Vec3 SceneDirection = Floor101::Conversion::ToSceneDirection(Direction);
	OutRay = f101::cad::Ray3{SceneOrigin.x, SceneOrigin.y, SceneOrigin.z, SceneDirection.x, SceneDirection.y, SceneDirection.z};
	return true;
}

void UFloor101GameViewportClient::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// A button released outside the window (e.g. after Alt+Tab) never reaches InputKey.
	if (Viewport)
	{
		bOrbiting = bOrbiting && Viewport->KeyState(EKeys::RightMouseButton);
		bPanning = bPanning && Viewport->KeyState(EKeys::MiddleMouseButton);
	}

	FVector2D Mouse;
	if (!GetMousePosition(Mouse))
	{
		return;
	}
	const FVector2D Delta = LastMousePosition.X < 0.0 ? FVector2D::ZeroVector : Mouse - LastMousePosition;
	LastMousePosition = Mouse;
	if (AFloor101EditingPawn* Pawn = GetEditingPawn(); Pawn && !Delta.IsZero())
	{
		if (bOrbiting)
		{
			RightDragPixels += Delta.Size();
			Pawn->Orbit(Delta);
		}
		else if (bPanning)
		{
			FVector2D Size;
			GetViewportSize(Size);
			Pawn->Pan(Delta, Size);
		}
	}

	// The camera follows the edited level up and down (a new level, Page Up/Down, a selection on another level...).
	UFloor101DocumentSubsystem* Document = GetEditingTarget();
	if (Document)
	{
		const f101::cad::EditController& Editor = Document->GetEditController();
		const double Elevation = static_cast<double>(f101::cad::levelElevationMm(Document->GetDocument().project(), Editor.activeLevel()));
		if (AFloor101EditingPawn* Pawn = GetEditingPawn(); Pawn && FollowedElevationMm.IsSet() && Elevation != FollowedElevationMm.GetValue())
		{
			Pawn->RaiseFocus((Elevation - FollowedElevationMm.GetValue()) / 10.0);
		}
		FollowedElevationMm = Elevation;
	}

	// The ray changes when the cursor or the camera moves; only then is the view recomputed.
	f101::cad::Ray3 Ray;
	if (Document && GetCursorRay(Ray))
	{
		const FVector Origin(Ray.originX, Ray.originY, Ray.originZ);
		const FVector Direction(Ray.directionX, Ray.directionY, Ray.directionZ);
		if (!Origin.Equals(LastRayOrigin, 0.01) || !Direction.Equals(LastRayDirection, 1e-6))
		{
			LastRayOrigin = Origin;
			LastRayDirection = Direction;
			Document->UpdatePointer(Ray);
		}
	}
}

bool UFloor101GameViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	UE_LOG(LogFloor101, VeryVerbose, TEXT("F101: key %s %s"), *EventArgs.Key.ToString(),
		*StaticEnum<EInputEvent>()->GetNameStringByValue(EventArgs.Event));
	UFloor101DocumentSubsystem* Document = GetEditingTarget();
	if (!Document)
	{
		return Super::InputKey(EventArgs);
	}

	const FKey& Key = EventArgs.Key;
	const EInputEvent Event = EventArgs.Event;
	const bool bDown = Event == IE_Pressed || Event == IE_DoubleClick;

	if (Key == EKeys::LeftMouseButton)
	{
		f101::cad::Ray3 Ray;
		if (GetCursorRay(Ray))
		{
			if (bDown)
			{
				(void)Document->Edit([&Ray](EditController& Editor) { return Editor.pointerPressed(Ray); });
			}
			else if (Event == IE_Released)
			{
				(void)Document->Edit([&Ray](EditController& Editor) { return Editor.pointerReleased(Ray); });
			}
		}
		return true;
	}
	if (Key == EKeys::RightMouseButton)
	{
		if (bDown)
		{
			bOrbiting = true;
			RightDragPixels = 0.0;
		}
		else if (Event == IE_Released && bOrbiting)
		{
			bOrbiting = false;
			if (RightDragPixels <= CancelClickMaxPixels)
			{
				(void)Document->Edit([](EditController& Editor) { Editor.cancel(); return f101::Status::success(); });
			}
		}
		return true;
	}
	if (Key == EKeys::MiddleMouseButton)
	{
		bPanning = bDown || (bPanning && Event != IE_Released);
		return true;
	}
	if (Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown)
	{
		return true; // one key event per wheel event, whatever its size: zoom uses MouseWheelAxis (InputAxis)
	}
	if ((Event == IE_Pressed || Event == IE_Repeat) && HandleKey(*Document, Key, Event == IE_Repeat))
	{
		return true;
	}
	return Super::InputKey(EventArgs);
}

bool UFloor101GameViewportClient::InputAxis(const FInputKeyEventArgs& EventArgs)
{
	if (EventArgs.Key == EKeys::MouseWheelAxis && GetEditingTarget())
	{
		if (AFloor101EditingPawn* Pawn = GetEditingPawn())
		{
			Pawn->Zoom(EventArgs.AmountDepressed); // wheel notches, positive away from the user
			return true;
		}
	}
	return Super::InputAxis(EventArgs);
}

EFloor101KeyCommand UFloor101GameViewportClient::MapKey(const FKey& Key, bool bControl, bool bShift, bool bRepeat, bool bTypingLength)
{
	if (bControl)
	{
		if (Key == EKeys::Z)
		{
			return bShift ? EFloor101KeyCommand::Redo : EFloor101KeyCommand::Undo;
		}
		if (Key == EKeys::Y)
		{
			return EFloor101KeyCommand::Redo;
		}
		if (bRepeat)
		{
			return EFloor101KeyCommand::None;
		}
		if (Key == EKeys::S)
		{
			return EFloor101KeyCommand::Save;
		}
		if (Key == EKeys::O)
		{
			return EFloor101KeyCommand::Open;
		}
		return EFloor101KeyCommand::None; // Ctrl+W is not W
	}
	if (Key == EKeys::BackSpace)
	{
		return EFloor101KeyCommand::Backspace;
	}
	if (bRepeat || (bTypingLength && (Key == EKeys::M || Key == EKeys::C)))
	{
		return EFloor101KeyCommand::None;
	}
	if (Key == EKeys::W) { return EFloor101KeyCommand::WallTool; }
	if (Key == EKeys::V) { return EFloor101KeyCommand::SelectTool; }
	if (Key == EKeys::P) { return EFloor101KeyCommand::DoorTool; }
	if (Key == EKeys::O) { return EFloor101KeyCommand::WindowTool; }
	if (Key == EKeys::M) { return EFloor101KeyCommand::PaintTool; }
	if (Key == EKeys::A) { return EFloor101KeyCommand::FurnitureTool; }
	if (Key == EKeys::Escape) { return EFloor101KeyCommand::Cancel; }
	if (Key == EKeys::Enter) { return EFloor101KeyCommand::Confirm; }
	if (Key == EKeys::Delete) { return EFloor101KeyCommand::Delete; }
	if (Key == EKeys::F) { return EFloor101KeyCommand::Frame; }
	if (Key == EKeys::T) { return EFloor101KeyCommand::TopView; }
	if (Key == EKeys::PageUp) { return EFloor101KeyCommand::LevelUp; }
	if (Key == EKeys::PageDown) { return EFloor101KeyCommand::LevelDown; }
	if (Key == EKeys::L) { return EFloor101KeyCommand::AllLevels; }
	if (Key == EKeys::SpaceBar) { return EFloor101KeyCommand::Flip; }
	if (Key == EKeys::Tab) { return EFloor101KeyCommand::PlanView; }
	return EFloor101KeyCommand::None;
}

bool UFloor101GameViewportClient::HandleKey(UFloor101DocumentSubsystem& Document, const FKey& Key, bool bRepeat)
{
	const FModifierKeysState Modifiers = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetModifierKeys()
		: FModifierKeysState();
	auto Edit = [&Document](auto Action) { (void)Document.Edit(Action); };
	auto Always = [](auto Action)
	{
		return [Action](EditController& Editor) { Action(Editor); return f101::Status::success(); };
	};
	const FString Path = Document.GetProjectPath().IsEmpty() ? UFloor101DocumentSubsystem::GetDefaultProjectPath() : Document.GetProjectPath();

	const EditController& Current = Document.GetEditController();
	const bool bTypingLength = Current.mode() == f101::cad::EditMode::Wall && Current.wallTool().state() == f101::cad::WallTool::State::Drafting;
	switch (MapKey(Key, Modifiers.IsControlDown(), Modifiers.IsShiftDown(), bRepeat, bTypingLength))
	{
	case EFloor101KeyCommand::None: return false;
	case EFloor101KeyCommand::Undo: Edit([](EditController& Editor) { return Editor.undo(); }); break;
	case EFloor101KeyCommand::Redo: Edit([](EditController& Editor) { return Editor.redo(); }); break;
	case EFloor101KeyCommand::Save: (void)Document.SaveProject(Path); break;
	case EFloor101KeyCommand::Open: (void)Document.OpenProject(Path); break;
	case EFloor101KeyCommand::Backspace: Edit(Always([](EditController& Editor) { Editor.backspace(); })); break;
	case EFloor101KeyCommand::WallTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Wall); })); break;
	case EFloor101KeyCommand::SelectTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Select); })); break;
	case EFloor101KeyCommand::DoorTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Door); })); break;
	case EFloor101KeyCommand::WindowTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Window); })); break;
	case EFloor101KeyCommand::PaintTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Paint); })); break;
	case EFloor101KeyCommand::FurnitureTool:
		Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Furniture); }));
		break;
	case EFloor101KeyCommand::Cancel: Edit(Always([](EditController& Editor) { Editor.cancel(); })); break;
	case EFloor101KeyCommand::Confirm: Edit([](EditController& Editor) { return Editor.confirm(); }); break;
	case EFloor101KeyCommand::Delete: Edit([](EditController& Editor) { return Editor.deleteSelection(); }); break;
	case EFloor101KeyCommand::Frame: FrameProject(); break;
	case EFloor101KeyCommand::TopView: ShowTopView(); break;
	case EFloor101KeyCommand::LevelUp: Edit([](EditController& Editor) { return Editor.levelUp(); }); break;
	case EFloor101KeyCommand::LevelDown: Edit([](EditController& Editor) { return Editor.levelDown(); }); break;
	case EFloor101KeyCommand::AllLevels:
		Edit(Always([](EditController& Editor) { Editor.setShowAllLevels(!Editor.showsAllLevels()); }));
		break;
	case EFloor101KeyCommand::Flip:
	{
		// Only with a door, a window or a piece of furniture at hand: elsewhere (e.g. typing "3.55 m") the space stays a
		// character.
		const EditController& Editor = Document.GetEditController();
		const bool bTool = Editor.mode() == f101::cad::EditMode::Door || Editor.mode() == f101::cad::EditMode::Window ||
			Editor.mode() == f101::cad::EditMode::Furniture;
		if (!bTool && !Editor.selectedOpening() && !Editor.selectedFurniture())
		{
			return false;
		}
		Edit([](EditController& Target) { return Target.flip(); });
		break;
	}
	case EFloor101KeyCommand::PlanView: SetPlanView(!IsPlanView()); break;
	}
	return true;
}

bool UFloor101GameViewportClient::InputChar(FViewport* InViewport, int32 ControllerId, TCHAR Character)
{
	UFloor101DocumentSubsystem* Document = GetEditingTarget();
	if (!Document || Document->GetEditController().mode() != f101::cad::EditMode::Wall)
	{
		return Super::InputChar(InViewport, ControllerId, Character);
	}
	// Control characters (Enter, Backspace, Ctrl+letter) arrive as keys; the wall tool keeps only length characters.
	if (Character >= TEXT(' '))
	{
		const FString Typed(1, &Character);
		(void)Document->Edit([&Typed](EditController& Editor)
		{
			Editor.appendText(TCHAR_TO_UTF8(*Typed));
			return f101::Status::success();
		});
	}
	return true;
}

void UFloor101GameViewportClient::FrameProject()
{
	AFloor101EditingPawn* Pawn = GetEditingPawn();
	const UWorld* CurrentWorld = GetWorld();
	const UFloor101DocumentSubsystem* Document = CurrentWorld ? CurrentWorld->GetSubsystem<UFloor101DocumentSubsystem>() : nullptr;
	if (!Pawn || !Document)
	{
		return;
	}
	const f101::cad::Project& Project = Document->GetDocument().project();
	const f101::cad::EditController& Editor = Document->GetEditController();
	const std::size_t Active = Project.levelIndex(Editor.activeLevel()).value_or(0);
	FBox Bounds(ForceInit);
	for (const f101::cad::Wall& Wall : Project.walls)
	{
		const f101::cad::LevelId Level = Project.levelOf(Wall);
		if (!Editor.showsAllLevels() && Project.levelIndex(Level).value_or(0) > Active)
		{
			continue; // hidden above the edited level
		}
		const double Base = static_cast<double>(f101::cad::levelElevationMm(Project, Level));
		const double Top = Base + static_cast<double>(Wall.heightMm);
		Bounds += ScenePoint(Wall.start, Base);
		Bounds += ScenePoint(Wall.end, Top);
		Bounds += ScenePoint(Wall.start, Top);
		Bounds += ScenePoint(Wall.end, Base);
	}
	if (!Bounds.IsValid)
	{
		// Around the origin, on the edited level.
		const double Base = static_cast<double>(f101::cad::levelElevationMm(Project, Editor.activeLevel())) / 10.0;
		Bounds = FBox(FVector(-500.0, -500.0, Base), FVector(500.0, 500.0, Base + 250.0));
	}
	Pawn->Frame(Bounds);
}

void UFloor101GameViewportClient::ShowTopView()
{
	if (IsPlanView())
	{
		SetPlanView(false);
	}
	if (AFloor101EditingPawn* Pawn = GetEditingPawn())
	{
		Pawn->TopView();
	}
}

void UFloor101GameViewportClient::SetPlanView(bool bPlan)
{
	UWorld* CurrentWorld = GetWorld();
	UFloor101DocumentSubsystem* Document = CurrentWorld ? CurrentWorld->GetSubsystem<UFloor101DocumentSubsystem>() : nullptr;
	AFloor101EditingPawn* Pawn = GetEditingPawn();
	if (!Document || !Pawn || Pawn->IsPlanView() == bPlan)
	{
		return;
	}
	bDisableWorldRendering = bPlan; // the overlay paints the plan over the whole view
	Pawn->SetPlanView(bPlan);
	Document->SetPlanView(bPlan);
	FrameProject();
}

bool UFloor101GameViewportClient::IsPlanView() const
{
	const AFloor101EditingPawn* Pawn = GetEditingPawn();
	return Pawn && Pawn->IsPlanView();
}
