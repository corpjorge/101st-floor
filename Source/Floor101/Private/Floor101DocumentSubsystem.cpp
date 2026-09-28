#include "Floor101DocumentSubsystem.h"

#include "Engine/World.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/Units.h"
#include "F101/Scene/SceneCompiler.h"
#include "Floor101.h"
#include "Floor101SceneNodeActor.h"
#include "Floor101SceneReconciler.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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

	// Development commands for MVP 0; they act on the world of the console that runs them (editor, PIE or game).
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
	Reconciler = MakePimpl<FFloor101SceneReconciler>(GetWorld());
	ListenerHandle = Document->subscribe([this](const f101::cad::ChangeSet&) { Resync(); });
}

void UFloor101DocumentSubsystem::Deinitialize()
{
	if (Document)
	{
		Document->unsubscribe(ListenerHandle);
	}
	// Actors die with the world; the reconciler only drops its weak references.
	Reconciler.Reset();
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

AActor* UFloor101DocumentSubsystem::FindWallActor(const f101::cad::WallId& Id) const
{
	return Reconciler->FindActor(f101::scene::wallBodyNodeId(Id));
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

void UFloor101DocumentSubsystem::Resync()
{
	const double Start = FPlatformTime::Seconds();
	const f101::scene::Scene Scene = f101::scene::compileScene(Document->project());
	const FFloor101ReconcileStats Stats = Reconciler->Reconcile(Scene);
	UE_LOG(LogFloor101, Display, TEXT("F101: scene resync in %.3f ms (created %d, updated %d, unchanged %d, removed %d)"),
		(FPlatformTime::Seconds() - Start) * 1000.0, Stats.Created, Stats.Updated, Stats.Unchanged, Stats.Removed);
	LogStatus();
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
}
