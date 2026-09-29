#include "Floor101DocumentSubsystem.h"

#include "Engine/World.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Cad/Units.h"
#include "F101/Scene/SceneCompiler.h"
#include "Floor101.h"
#include "Floor101SceneNodeActor.h"
#include "Floor101SceneReconciler.h"
#include "Floor101SceneSpace.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include <filesystem>
#include <string_view>

#define LOCTEXT_NAMESPACE "Floor101Document"

namespace
{
	constexpr int64 ReferenceLengthMm = 3550;
	constexpr int64 ReferenceThicknessMm = 120;
	constexpr int64 ReferenceHeightMm = 2400;

	UFloor101DocumentSubsystem* FindSubsystem(UWorld* World)
	{
		UFloor101DocumentSubsystem* Subsystem = World ? World->GetSubsystem<UFloor101DocumentSubsystem>() : nullptr;
		if (!Subsystem)
		{
			UE_LOG(LogFloor101, Warning, TEXT("No 101st Floor document in this world"));
		}
		return Subsystem;
	}

	// UTF-8 in both directions, so non-ASCII paths (e.g. a user name with accents) survive on Windows.
	std::filesystem::path ToFilesystemPath(const FString& Path)
	{
		const FTCHARToUTF8 Utf8(*Path);
		return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(Utf8.Get()), Utf8.Length()));
	}

	FText ToText(const std::string& Utf8)
	{
		return FText::FromString(UTF8_TO_TCHAR(Utf8.c_str()));
	}

	// The file named on the command line, else the current one, else the default one.
	FString ResolveProjectPath(const TArray<FString>& Args, const UFloor101DocumentSubsystem& Subsystem)
	{
		if (!Args.IsEmpty())
		{
			return FPaths::ConvertRelativePathToFull(FString::Join(Args, TEXT(" ")));
		}
		return Subsystem.GetProjectPath().IsEmpty() ? UFloor101DocumentSubsystem::GetDefaultProjectPath() : Subsystem.GetProjectPath();
	}

	// Development commands; they act on the world of the console that runs them (editor, PIE or game).
	FAutoConsoleCommandWithWorld LoadCommand(
		TEXT("f101.Demo.Load"),
		TEXT("Creates the MVP 0 reference wall: (0,0)->(3550,0) mm, 120 mm thick, 2400 mm high."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->LoadReferenceWall();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SetLengthCommand(
		TEXT("f101.Demo.SetLength"),
		TEXT("Sets the reference wall length. Accepts 101st Floor input: 420, 420 cm, 4.2 m, 4,2 m, 4200 mm (no unit = cm)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World);
			if (!Subsystem)
			{
				return;
			}
			const FString Input = FString::Join(Args, TEXT(" "));
			const f101::Result<std::int64_t> Length = f101::cad::parseLength(TCHAR_TO_UTF8(*Input));
			if (!Length)
			{
				UE_LOG(LogFloor101, Warning, TEXT("f101.Demo.SetLength '%s': %s"), *Input, UTF8_TO_TCHAR(Length.error().message.c_str()));
				return;
			}
			Subsystem->SetReferenceWallLength(Length.value());
		}));

	FAutoConsoleCommandWithWorld ClearCommand(
		TEXT("f101.Demo.Clear"),
		TEXT("Removes the reference wall."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->ClearReferenceWall();
			}
		}));

	FAutoConsoleCommandWithWorld StatusCommand(
		TEXT("f101.Demo.Status"),
		TEXT("Logs the walls of the document and the Unreal actors that represent them."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->LogStatus();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SaveCommand(
		TEXT("f101.Project.Save"),
		TEXT("Saves the document as project.json: to the given path, else the current file, else Documents/101st Floor/project.json."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				(void)Subsystem->SaveProject(ResolveProjectPath(Args, *Subsystem));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs OpenCommand(
		TEXT("f101.Project.Open"),
		TEXT("Opens a project.json: the given path, else the current file, else Documents/101st Floor/project.json."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				(void)Subsystem->OpenProject(ResolveProjectPath(Args, *Subsystem));
			}
		}));

	FAutoConsoleCommandWithWorld UndoCommand(
		TEXT("f101.Undo"),
		TEXT("Undoes the last change of the document."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				(void)Subsystem->Edit([](f101::cad::EditController& Editor) { return Editor.undo(); });
			}
		}));

	FAutoConsoleCommandWithWorld RedoCommand(
		TEXT("f101.Redo"),
		TEXT("Redoes the last undone change of the document."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				(void)Subsystem->Edit([](f101::cad::EditController& Editor) { return Editor.redo(); });
			}
		}));
}

UFloor101DocumentSubsystem::UFloor101DocumentSubsystem() = default;
UFloor101DocumentSubsystem::~UFloor101DocumentSubsystem() = default;

bool UFloor101DocumentSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Editor);
}

void UFloor101DocumentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Document = MakePimpl<f101::cad::Document>(f101::cad::makeRandomUuidSource());
	Editor = MakePimpl<f101::cad::EditController>(*Document, f101::cad::EditSettings{});
	Reconciler = MakePimpl<FFloor101SceneReconciler>(GetWorld());
	ListenerHandle = Document->subscribe([this](const f101::cad::ChangeSet&)
	{
		++ChangeCount;
		if (!bInEdit)
		{
			Resync(true); // changed outside an edit (console, MCP toolset, file): show it now
		}
	});
}

void UFloor101DocumentSubsystem::Deinitialize()
{
	if (Document)
	{
		Document->unsubscribe(ListenerHandle);
	}
	// Actors die with the world; the reconciler only drops its weak references.
	Reconciler.Reset();
	Editor.Reset();
	Document.Reset();
	Super::Deinitialize();
}

void UFloor101DocumentSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (FParse::Param(FCommandLine::Get(), TEXT("F101Demo")))
	{
		LoadReferenceWall();
	}
}

f101::Status UFloor101DocumentSubsystem::Edit(TFunctionRef<f101::Status(f101::cad::EditController&)> Action)
{
	// One resync once the action is over: while it runs, the controller's view state is not final (e.g. the wall
	// just created is still the tool's preview), so a resync from the document listener would show both.
	const uint64 ChangesBefore = ChangeCount;
	bInEdit = true;
	const f101::Status Result = Action(*Editor);
	bInEdit = false;
	if (Result)
	{
		SetStatus(FText::GetEmpty(), false);
	}
	else
	{
		SetStatus(ToText(Result.error().message), true);
	}
	Resync(ChangeCount != ChangesBefore);
	return Result;
}

void UFloor101DocumentSubsystem::UpdatePointer(const f101::cad::Ray3& Ray)
{
	Editor->pointerMoved(Ray);
	Resync(false);
}

void UFloor101DocumentSubsystem::SetStatus(const FText& Message, bool bIsError)
{
	StatusMessage = Message;
	bStatusIsError = bIsError;
	if (bIsError)
	{
		UE_LOG(LogFloor101, Warning, TEXT("F101: %s"), *Message.ToString());
	}
	else if (!Message.IsEmpty())
	{
		UE_LOG(LogFloor101, Display, TEXT("F101: %s"), *Message.ToString());
	}
}

FString UFloor101DocumentSubsystem::GetDefaultProjectPath()
{
	return FPaths::Combine(FPlatformProcess::UserDir(), TEXT("101st Floor"), TEXT("project.json"));
}

f101::Status UFloor101DocumentSubsystem::SaveProject(const FString& Path)
{
	const FString Directory = FPaths::GetPath(Path);
	if (!Directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*Directory, true))
	{
		const f101::Error Failed{f101::ErrorCode::NotFound, "Cannot create the folder " + std::string(TCHAR_TO_UTF8(*Directory))};
		SetStatus(ToText(Failed.message), true);
		return Failed;
	}
	const f101::Status Saved = f101::cad::saveProjectFile(ToFilesystemPath(Path), Document->project());
	if (!Saved)
	{
		SetStatus(ToText(Saved.error().message), true);
		return Saved;
	}
	ProjectPath = Path;
	SavedChangeCount = ChangeCount;
	SetStatus(FText::Format(LOCTEXT("Saved", "Guardado: {0}"), FText::FromString(Path)), false);
	return Saved;
}

f101::Status UFloor101DocumentSubsystem::OpenProject(const FString& Path)
{
	f101::Result<f101::cad::Project> Loaded = f101::cad::loadProjectFile(ToFilesystemPath(Path));
	if (!Loaded)
	{
		SetStatus(ToText(Loaded.error().message), true);
		return Loaded.error();
	}
	Editor->reset();
	const f101::Status Replaced = Document->replaceProject(std::move(Loaded).value());
	if (!Replaced)
	{
		SetStatus(ToText(Replaced.error().message), true);
		return Replaced;
	}
	ProjectPath = Path;
	SavedChangeCount = ChangeCount;
	SetStatus(FText::Format(LOCTEXT("Opened", "Abierto: {0}"), FText::FromString(Path)), false);
	return Replaced;
}

AActor* UFloor101DocumentSubsystem::FindWallActor(const f101::cad::WallId& Id) const
{
	return Reconciler->FindActor(f101::scene::wallBodyNodeId(Id));
}

AActor* UFloor101DocumentSubsystem::FindRoomFloorActor(const FString& RoomKey) const
{
	return Reconciler->FindActor(f101::scene::SceneNodeId{std::string(TCHAR_TO_UTF8(*RoomKey)) + "/floor"});
}

AActor* UFloor101DocumentSubsystem::FindRoomSlabActor(const FString& RoomKey) const
{
	return Reconciler->FindActor(f101::scene::SceneNodeId{std::string(TCHAR_TO_UTF8(*RoomKey)) + "/slab"});
}

void UFloor101DocumentSubsystem::LoadReferenceWall()
{
	if (!ReferenceWallId.isNil() && Document->project().findWall(ReferenceWallId))
	{
		UE_LOG(LogFloor101, Display, TEXT("F101: reference wall already loaded"));
		return;
	}
	const f101::Result<f101::cad::WallId> Added = Document->addWall(
		{0, 0}, {ReferenceLengthMm, 0}, ReferenceThicknessMm, ReferenceHeightMm);
	check(Added.ok());
	ReferenceWallId = Added.value();
}

bool UFloor101DocumentSubsystem::SetReferenceWallLength(int64 LengthMm)
{
	if (ReferenceWallId.isNil())
	{
		UE_LOG(LogFloor101, Warning, TEXT("F101: no reference wall (run f101.Demo.Load first)"));
		return false;
	}
	const f101::Status Changed = Document->setWallEnd(ReferenceWallId, {LengthMm, 0});
	if (!Changed)
	{
		UE_LOG(LogFloor101, Warning, TEXT("F101: length %lld mm rejected: %s"), LengthMm, UTF8_TO_TCHAR(Changed.error().message.c_str()));
		return false;
	}
	return true;
}

void UFloor101DocumentSubsystem::ClearReferenceWall()
{
	if (!ReferenceWallId.isNil())
	{
		const f101::Status Removed = Document->removeWall(ReferenceWallId);
		check(Removed.ok());
		ReferenceWallId = f101::cad::WallId();
	}
}

void UFloor101DocumentSubsystem::Resync(bool bDocumentChanged)
{
	const double Start = FPlatformTime::Seconds();

	const f101::cad::EditView View = Editor->view();
	f101::scene::ViewOverlay Overlay;
	Overlay.selectedWall = View.selectedWall;
	Overlay.previewWalls = View.previewWalls;
	Overlay.markers = View.handles;
	Overlay.markerSizeMm = Editor->settings().handleSizeMm;
	Overlay.markerHeightMm = Editor->settings().handleHeightMm;
	Overlay.activeLevel = View.activeLevel;
	Overlay.showAllLevels = View.showAllLevels;

	const f101::scene::Scene Scene = f101::scene::compileScene(Document->project(), Overlay);
	const FFloor101ReconcileStats Stats = Reconciler->Reconcile(Scene);

	// Labels sit on the floor of each room of the edited level, or of every level when all are shown (preview included,
	// so they follow a gesture live).
	Rooms.Reset(static_cast<int32>(Scene.rooms.size()));
	for (const f101::scene::SceneRoom& Entry : Scene.rooms)
	{
		if (!View.showAllLevels && Entry.level != View.activeLevel)
		{
			continue;
		}
		const f101::geometry::Room& Room = Entry.room;
		const FVector Label = FVector(Floor101::Conversion::ToUnrealPosition({Room.labelPoint.x_mm, Room.labelPoint.y_mm,
			static_cast<double>(Entry.elevationMm) + f101::geometry::FloorThicknessMm}));
		Rooms.Add(FFloor101RoomView{UTF8_TO_TCHAR(Room.key.c_str()), Label, Room.areaMm2 / 1.0e6});
	}
	if (bDocumentChanged)
	{
		UE_LOG(LogFloor101, Display, TEXT("F101: scene resync in %.3f ms (created %d, updated %d, unchanged %d, removed %d)"),
			(FPlatformTime::Seconds() - Start) * 1000.0, Stats.Created, Stats.Updated, Stats.Unchanged, Stats.Removed);
		LogStatus();
	}
	else
	{
		UE_LOG(LogFloor101, Verbose, TEXT("F101: view refresh in %.3f ms (created %d, updated %d, unchanged %d, removed %d)"),
			(FPlatformTime::Seconds() - Start) * 1000.0, Stats.Created, Stats.Updated, Stats.Unchanged, Stats.Removed);
	}
}

void UFloor101DocumentSubsystem::LogStatus() const
{
	for (const f101::cad::Wall& Wall : Document->project().walls)
	{
		const AFloor101SceneNodeActor* Actor = Reconciler->FindActor(f101::scene::wallBodyNodeId(Wall.id));
		const FVector Size = Actor ? Actor->GetMeshBounds().GetSize() : FVector::ZeroVector;
		UE_LOG(LogFloor101, Display,
			TEXT("F101: wall %s length=%.0f mm thickness=%lld mm height=%lld mm -> actor %s size=(%.1f, %.1f, %.1f) cm"),
			UTF8_TO_TCHAR(f101::cad::toString(Wall.id).c_str()), Wall.lengthMm(), Wall.thicknessMm, Wall.heightMm,
			Actor ? *Actor->GetName() : TEXT("<none>"), Size.X, Size.Y, Size.Z);
	}
	if (Document->project().walls.empty())
	{
		UE_LOG(LogFloor101, Display, TEXT("F101: document has no walls"));
	}
	for (const FFloor101RoomView& Room : Rooms)
	{
		const AActor* Floor = FindRoomFloorActor(Room.Key);
		UE_LOG(LogFloor101, Display, TEXT("F101: room %s area=%.4f m2 -> floor %s"), *Room.Key, Room.AreaM2,
			Floor ? *Floor->GetName() : TEXT("<none>"));
	}
}

#undef LOCTEXT_NAMESPACE
