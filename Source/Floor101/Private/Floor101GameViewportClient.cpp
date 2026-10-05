#include "Floor101GameViewportClient.h"

#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Drawing/PlanDrawing.h"
#include "Floor101.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101Display.h"
#include "Floor101EditingPawn.h"
#include "Floor101FileActions.h"
#include "Floor101PathTracing.h"
#include "Floor101SceneSpace.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UnrealClient.h"
#include "Widgets/SWidget.h"

namespace
{
	constexpr double CancelClickMaxPixels = 4.0;

	using f101::cad::EditController;

	FVector ScenePoint(const f101::cad::Point2D& P, double ZMm)
	{
		return FVector(Floor101::Conversion::ToUnrealPosition({static_cast<double>(P.x_mm), static_cast<double>(P.y_mm), ZMm}));
	}

	bool IsShortcutModifier(const FKey& Key)
	{
		return Key == EKeys::LeftControl || Key == EKeys::RightControl || Key == EKeys::LeftAlt || Key == EKeys::RightAlt
			|| Key == EKeys::LeftCommand || Key == EKeys::RightCommand;
	}

	bool StudioTextEntryFocused()
	{
		const TSharedPtr<SWidget> Focused = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetKeyboardFocusedWidget() : nullptr;
		return Focused && (Focused->GetType() == FName(TEXT("SEditableText")) || Focused->GetType() == FName(TEXT("SEditableTextBox"))
			|| Focused->GetType() == FName(TEXT("SMultiLineEditableText")) || Focused->GetType() == FName(TEXT("SMultiLineEditableTextBox")));
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

void UFloor101GameViewportClient::SetNavigationSpeedLevel(int32 Level, bool bPreserveMovement)
{
	if (!bPreserveMovement) { NavigationKeys.Reset(); }
	if (AFloor101EditingPawn* Pawn = GetEditingPawn()) { Pawn->SetNavigationSpeedLevel(Level, true); }
}

int32 UFloor101GameViewportClient::GetNavigationSpeedLevel() const
{
	const AFloor101EditingPawn* Pawn = GetEditingPawn();
	return Pawn ? Pawn->GetNavigationSpeedLevel() : 5;
}

bool UFloor101GameViewportClient::RequestExit()
{
	if (!Viewport) { return false; }
	NavigationKeys.Reset();
	bOrbiting = false;
	bPanning = false;
	UWorld* CurrentWorld = GetWorld();
	UFloor101DocumentSubsystem* Document = CurrentWorld ? CurrentWorld->GetSubsystem<UFloor101DocumentSubsystem>() : nullptr;
	if (Document && Document->HasUnsavedChanges())
	{
		return RequestUnsavedAction(true, FSimpleDelegate::CreateWeakLambda(this, [this]() { CompleteExit(); }));
	}
	return CompleteExit();
}

bool UFloor101GameViewportClient::RequestUnsavedAction(bool bExit, FSimpleDelegate Proceed)
{
	if (!UnsavedPrompt.IsBound()) { return false; } // missing UI never discards unsaved work
	UnsavedPrompt.Execute(bExit, Proceed);
	return true;
}

bool UFloor101GameViewportClient::CompleteExit()
{
	if (!Viewport) { return false; }
	UWorld* CurrentWorld = GetWorld();
	if (CurrentWorld && CurrentWorld->WorldType == EWorldType::PIE)
	{
		OnCloseRequested().Broadcast(Viewport); // queue this play session's end; the editor owns viewport cleanup
		return true;
	}
	// QUIT defers teardown until the engine exits its frame. Keep the window/viewport alive for Slate shutdown.
	UEngine* Engine = GetOuterUEngine();
	return Engine && Engine->Exec(CurrentWorld, TEXT("QUIT"), *GLog);
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
	Floor101::Display::Tick(this);
	if (bPhotoMode)
	{
		Floor101::PathTracing::Tick(GetWorld());
	}

	// A button released outside the window (e.g. after Alt+Tab) never reaches InputKey.
	if (Viewport)
	{
		bOrbiting = bOrbiting && Viewport->KeyState(EKeys::RightMouseButton);
		bPanning = bPanning && Viewport->KeyState(EKeys::MiddleMouseButton);
	}
	TickNavigation(DeltaTime);

	FVector2D Mouse;
	if (!GetMousePosition(Mouse))
	{
		LastMousePosition = FVector2D(-1.0, -1.0);
		return;
	}
	const FVector2D Delta = LastMousePosition.X < 0.0 ? FVector2D::ZeroVector : Mouse - LastMousePosition;
	LastMousePosition = Mouse;
	if (AFloor101EditingPawn* Pawn = GetEditingPawn(); Pawn && CanNavigate() && !Delta.IsZero())
	{
		if (bOrbiting)
		{
			RightDragPixels += Delta.Size();
			if (bNavigationMode || bRightNavigationUsed)
			{
				Pawn->Look(Delta);
			}
			else
			{
				Pawn->Orbit(Delta);
			}
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
	if (Document && !bInterfaceOpen && !bPhotoMode && !bNavigationMode && !bOrbiting && GetCursorRay(Ray))
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

void UFloor101GameViewportClient::SetInterfaceOpen(bool bOpen, FSimpleDelegate Close)
{
	bInterfaceOpen = bOpen;
	CloseInterface = Close;
	NavigationKeys.Reset();
	bOrbiting = false;
	bPanning = false;
}

bool UFloor101GameViewportClient::CanNavigate()
{
	return !bInterfaceOpen && Viewport && Viewport->HasFocus() && Viewport->IsForegroundWindow() && !IgnoreInput()
		&& !(ViewportConsole && ViewportConsole->ConsoleActive());
}

void UFloor101GameViewportClient::TickNavigation(float DeltaTime)
{
	const FModifierKeysState Modifiers = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetModifierKeys() : FModifierKeysState();
	if (!CanNavigate() || !GetEditingTarget() || StudioTextEntryFocused()
		|| Modifiers.IsControlDown() || Modifiers.IsAltDown() || Modifiers.IsCommandDown())
	{
		NavigationKeys.Reset();
		return;
	}
	const FVector Direction = NavigationKeys.Direction([this](const FKey& Key) { return Viewport->KeyState(Key); });
	if (!Direction.IsZero())
	{
		if (AFloor101EditingPawn* Pawn = GetEditingPawn())
		{
			Pawn->Move(Direction, DeltaTime, Modifiers.IsShiftDown());
			bRightNavigationUsed |= bOrbiting;
		}
	}
}

void UFloor101GameViewportClient::LostFocus(FViewport* InViewport)
{
	NavigationKeys.Reset();
	bOrbiting = false;
	bPanning = false;
	bRightNavigationUsed = false;
	LastMousePosition = FVector2D(-1.0, -1.0);
	Super::LostFocus(InViewport);
}

void UFloor101GameViewportClient::SetNavigationMode(bool bNavigate)
{
	if (bNavigationMode == bNavigate)
	{
		return;
	}
	bNavigationMode = bNavigate;
	NavigationKeys.Reset();
	if (UFloor101DocumentSubsystem* Document = GetEditingTarget()) { Document->SetConstructionAidsVisible(!bNavigate); }
	if (bNavigate)
	{
		if (UFloor101DocumentSubsystem* Document = GetEditingTarget())
		{
			(void)Document->Edit([](EditController& Editor) { Editor.cancel(); return f101::Status::success(); });
		}
	}
}

bool UFloor101GameViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	// Slate can bubble unhandled letter-key events; typing a search must never switch CAD tools.
	if (StudioTextEntryFocused() && !EventArgs.Key.IsMouseButton()) { NavigationKeys.Reset(); return true; }
	if (bInterfaceOpen)
	{
		if (EventArgs.Key == EKeys::Escape && EventArgs.Event == IE_Pressed)
		{
			const FSimpleDelegate RequestedClose = CloseInterface;
			RequestedClose.ExecuteIfBound();
		}
		return true;
	}
	F101_LOG(LogFloor101, VeryVerbose, TEXT("F101: key %s %s"), *EventArgs.Key.ToString(),
		*StaticEnum<EInputEvent>()->GetNameStringByValue(EventArgs.Event));
	const FKey& Key = EventArgs.Key;
	const EInputEvent Event = EventArgs.Event;
	const bool bDown = Event == IE_Pressed || Event == IE_DoubleClick;
	const FModifierKeysState Modifiers = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetModifierKeys() : FModifierKeysState();
	const bool bShortcut = IsShortcutModifier(Key) || Modifiers.IsControlDown() || Modifiers.IsAltDown() || Modifiers.IsCommandDown();
	UFloor101DocumentSubsystem* Document = GetEditingTarget();
	if (NavigationKeys.HandleKey(Key, Event, Document && CanNavigate(), bShortcut))
	{
		bRightNavigationUsed |= bOrbiting && bDown;
		return true;
	}
	if (!Document)
	{
		return Super::InputKey(EventArgs);
	}

	if (Key == EKeys::LeftMouseButton)
	{
		if (bPhotoMode || bNavigationMode)
		{
			return true; // navigating and photos do not edit
		}
		f101::cad::Ray3 Ray;
		if (GetCursorRay(Ray))
		{
			if (Event == IE_DoubleClick)
			{
				(void)Document->Edit([&Ray](EditController& Editor) { return Editor.pointerDoubleClicked(Ray); });
			}
			else if (bDown)
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
			bRightNavigationUsed = false;
			RightDragPixels = 0.0;
		}
		else if (Event == IE_Released && bOrbiting)
		{
			bOrbiting = false;
			if (!bNavigationMode && !bRightNavigationUsed && RightDragPixels <= CancelClickMaxPixels)
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
	if (bInterfaceOpen || StudioTextEntryFocused()) { return true; }
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

EFloor101KeyCommand UFloor101GameViewportClient::MapKey(const FKey& Key, bool bControl, bool bShift, bool bRepeat, bool bTypingLength, bool bNavigating)
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
			return bShift ? EFloor101KeyCommand::SaveAs : EFloor101KeyCommand::Save;
		}
		if (Key == EKeys::O)
		{
			return EFloor101KeyCommand::Open;
		}
		return EFloor101KeyCommand::None; // Ctrl+W is not W
	}
	if (FFloor101NavigationKeys::IsMovementKey(Key))
	{
		return EFloor101KeyCommand::NavigationMove;
	}
	if (Key == EKeys::BackSpace)
	{
		return EFloor101KeyCommand::Backspace;
	}
	if (bRepeat || (bTypingLength && (Key == EKeys::M || Key == EKeys::C)))
	{
		return EFloor101KeyCommand::None;
	}
	if (Key == EKeys::B) { return EFloor101KeyCommand::WallTool; }
	if (Key == EKeys::V) { return EFloor101KeyCommand::SelectTool; }
	if (Key == EKeys::P) { return EFloor101KeyCommand::DoorTool; }
	if (Key == EKeys::O) { return EFloor101KeyCommand::WindowTool; }
	if (Key == EKeys::M) { return EFloor101KeyCommand::PaintTool; }
	if (Key == EKeys::G) { return EFloor101KeyCommand::FurnitureTool; }
	if (Key == EKeys::C) { return EFloor101KeyCommand::ColumnTool; }
	if (Key == EKeys::H) { return EFloor101KeyCommand::DimensionTool; }
	if (Key == EKeys::I) { return EFloor101KeyCommand::ServicesTool; }
	if (Key == EKeys::X) { return EFloor101KeyCommand::ServicesLayer; }
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
	if (Key == EKeys::R) { return EFloor101KeyCommand::PhotoMode; }
	if (Key == EKeys::F11) { return EFloor101KeyCommand::Fullscreen; }
	if (Key == EKeys::N) { return EFloor101KeyCommand::NavigationToggle; }
	return EFloor101KeyCommand::None;
}

bool UFloor101GameViewportClient::HandleKey(UFloor101DocumentSubsystem& Document, const FKey& Key, bool bRepeat)
{
	const FModifierKeysState Modifiers = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetModifierKeys()
		: FModifierKeysState();
	if (Modifiers.IsAltDown() || Modifiers.IsCommandDown())
	{
		return false;
	}
	auto Edit = [&Document](auto Action) { (void)Document.Edit(Action); };
	auto Always = [](auto Action)
	{
		return [Action](EditController& Editor) { Action(Editor); return f101::Status::success(); };
	};

	const EditController& Current = Document.GetEditController();
	const bool bTypingLength = Current.mode() == f101::cad::EditMode::Wall && Current.wallTool().state() == f101::cad::WallTool::State::Drafting;
	const int32 SpeedLevel = FFloor101NavigationKeys::SpeedLevelForKey(Key, bTypingLength && !bNavigationMode && !bPhotoMode && !bOrbiting,
		Modifiers.IsControlDown() || Modifiers.IsShiftDown());
	if (SpeedLevel > 0)
	{
		if (!bRepeat) { SetNavigationSpeedLevel(SpeedLevel, true); }
		return true;
	}
	const EFloor101KeyCommand Command = MapKey(Key, Modifiers.IsControlDown(), Modifiers.IsShiftDown(), bRepeat, bTypingLength, bNavigationMode || bOrbiting);
	if (Command == EFloor101KeyCommand::NavigationToggle || (Command == EFloor101KeyCommand::Cancel && bNavigationMode))
	{
		SetNavigationMode(!bNavigationMode);
		return true;
	}
	if (Command == EFloor101KeyCommand::NavigationMove)
	{
		return true;
	}
	if (bNavigationMode && !bPhotoMode && (Command == EFloor101KeyCommand::SelectTool || Command == EFloor101KeyCommand::WallTool
		|| Command == EFloor101KeyCommand::FurnitureTool || Command == EFloor101KeyCommand::DoorTool
		|| Command == EFloor101KeyCommand::WindowTool || Command == EFloor101KeyCommand::PaintTool
		|| Command == EFloor101KeyCommand::ColumnTool || Command == EFloor101KeyCommand::ServicesTool))
	{
		SetNavigationMode(false);
	}
	if (bPhotoMode)
	{
		// Only the camera and leaving the photo: editing keys do nothing while the view is path traced.
		switch (Command)
		{
		case EFloor101KeyCommand::None: return false;
		case EFloor101KeyCommand::PhotoMode:
		case EFloor101KeyCommand::Cancel: SetPhotoMode(false); break;
		case EFloor101KeyCommand::Frame: FrameProject(); break;
		case EFloor101KeyCommand::TopView: ShowTopView(); break;
		case EFloor101KeyCommand::Fullscreen: Floor101::Display::ToggleFullscreen(this); break;
		default: break;
		}
		return true;
	}
	switch (Command)
	{
	case EFloor101KeyCommand::None: return false;
	case EFloor101KeyCommand::Undo: Edit([](EditController& Editor) { return Editor.undo(); }); break;
	case EFloor101KeyCommand::Redo: Edit([](EditController& Editor) { return Editor.redo(); }); break;
	case EFloor101KeyCommand::Save: FFloor101FileActions::Save(Document); break;
	case EFloor101KeyCommand::SaveAs: FFloor101FileActions::SaveAs(Document); break;
	case EFloor101KeyCommand::Open: FFloor101FileActions::Open(Document); break;
	case EFloor101KeyCommand::Backspace: Edit(Always([](EditController& Editor) { Editor.backspace(); })); break;
	case EFloor101KeyCommand::WallTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Wall); })); break;
	case EFloor101KeyCommand::SelectTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Select); })); break;
	case EFloor101KeyCommand::DoorTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Door); })); break;
	case EFloor101KeyCommand::WindowTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Window); })); break;
	case EFloor101KeyCommand::PaintTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Paint); })); break;
	case EFloor101KeyCommand::FurnitureTool:
		Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Furniture); }));
		break;
	case EFloor101KeyCommand::ColumnTool: Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Column); })); break;
	case EFloor101KeyCommand::ServicesTool:
		Edit(Always([](EditController& Editor) { Editor.setMode(f101::cad::EditMode::Services); }));
		break;
	case EFloor101KeyCommand::ServicesLayer:
		Edit(Always([](EditController& Editor) { Editor.setShowServices(!Editor.showsServices()); }));
		break;
	case EFloor101KeyCommand::Cancel: Edit(Always([](EditController& Editor) { Editor.cancel(); })); break;
	case EFloor101KeyCommand::Confirm: Edit([](EditController& Editor) { return Editor.confirm(); }); break;
	case EFloor101KeyCommand::DimensionTool:
		SetPlanView(true);
		Edit(Always([](EditController& E) { E.setMode(f101::cad::EditMode::Dimension); }));
		break;
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
		// Only with a door, a window, a piece of furniture or a column at hand: elsewhere (e.g. typing "3.55 m") the space
		// stays a character.
		const EditController& Editor = Document.GetEditController();
		const bool bTool = Editor.mode() == f101::cad::EditMode::Door || Editor.mode() == f101::cad::EditMode::Window ||
			Editor.mode() == f101::cad::EditMode::Furniture || Editor.mode() == f101::cad::EditMode::Column;
		if (!bTool && !Editor.selectedOpening() && !Editor.selectedFurniture() && !Editor.selectedColumn())
		{
			return false;
		}
		Edit([](EditController& Target) { return Target.flip(); });
		break;
	}
	case EFloor101KeyCommand::PlanView: SetPlanView(!IsPlanView()); break;
	case EFloor101KeyCommand::PhotoMode: SetPhotoMode(true); break;
	case EFloor101KeyCommand::Fullscreen: Floor101::Display::ToggleFullscreen(this); break;
	case EFloor101KeyCommand::NavigationToggle:
	case EFloor101KeyCommand::NavigationMove: break; // handled before photo/edit routing
	}
	return true;
}

bool UFloor101GameViewportClient::InputChar(FViewport* InViewport, int32 ControllerId, TCHAR Character)
{
	if (bInterfaceOpen || StudioTextEntryFocused()) { return true; }
	if (bNavigationMode || bOrbiting || FFloor101NavigationKeys::IsMovementCharacter(Character))
	{
		return true;
	}
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
	if (IsPlanView())
	{
		const auto& Plan = Document->GetPlanDrawing();
		if (!Plan.empty())
		{
			const double Base = static_cast<double>(f101::cad::levelElevationMm(Project, Editor.activeLevel()));
			Bounds += FVector(Floor101::Conversion::ToUnrealPosition({Plan.min.x_mm, Plan.min.y_mm, Base}));
			Bounds += FVector(Floor101::Conversion::ToUnrealPosition({Plan.max.x_mm, Plan.max.y_mm, Base}));
		}
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
	if (bPlan)
	{
		SetPhotoMode(false);
	}
	bDisableWorldRendering = bPlan; // the overlay paints the plan over the whole view
	Pawn->SetPlanView(bPlan);
	Document->SetPlanView(bPlan);
	FrameProject();
}

void UFloor101GameViewportClient::SetPhotoMode(bool bPhoto)
{
	UWorld* CurrentWorld = GetWorld();
	UFloor101DocumentSubsystem* Document = CurrentWorld ? CurrentWorld->GetSubsystem<UFloor101DocumentSubsystem>() : nullptr;
	if (!Document || bPhotoMode == bPhoto)
	{
		return;
	}
	if (bPhoto && IsPlanView())
	{
		SetPlanView(false);
	}
	bPhotoMode = bPhoto;
	if (bPhoto)
	{
		Floor101::PathTracing::SetTargetSamples(PhotoSamples);
	}
	Floor101::PathTracing::SetEnabled(*this, bPhoto);
	Document->SetPhotoMode(bPhoto);
}

void UFloor101GameViewportClient::SetPhotoSamples(int32 Samples)
{
	PhotoSamples = FMath::Clamp(Samples, 1, 65536);
	if (bPhotoMode)
	{
		Floor101::PathTracing::SetTargetSamples(PhotoSamples); // the path tracer starts over with the new target
	}
}

bool UFloor101GameViewportClient::IsPlanView() const
{
	const AFloor101EditingPawn* Pawn = GetEditingPawn();
	return Pawn && Pawn->IsPlanView();
}
