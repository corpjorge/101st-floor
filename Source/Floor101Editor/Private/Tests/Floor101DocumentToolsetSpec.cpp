#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Floor101DocumentToolset.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"

BEGIN_DEFINE_SPEC(FFloor101DocumentToolsetSpec, "Floor101.Editor.DocumentToolset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	static FVector ActorSizeCm(const FFloor101WallInfo& Wall)
	{
		FVector Origin;
		FVector Extent;
		Wall.Actor->GetActorBounds(false, Origin, Extent);
		return Extent * 2.0;
	}

	static bool HasWall(const FString& Id)
	{
		return UFloor101DocumentToolset::GetWalls().ContainsByPredicate([&Id](const FFloor101WallInfo& Wall) { return Wall.Id == Id; });
	}

	// Runs a tool call the way the ToolsetRegistry does (inside a script frame, where RaiseScriptError reports) and
	// checks that it raised an error containing Fragment.
	void ExpectRaises(const TCHAR* Fragment, TFunction<void()>&& Call)
	{
		AddExpectedErrorPlain(Fragment);
		UE::ToolsetRegistry::FToolCallExceptionHandler ExceptionHandler;
		ExceptionHandler.CaptureErrorsIn(MoveTemp(Call));
		TestTrue(FString::Printf(TEXT("raised '%s' (got '%s')"), Fragment, *ExceptionHandler.GetException()),
			ExceptionHandler.GetException().Contains(Fragment));
	}

END_DEFINE_SPEC(FFloor101DocumentToolsetSpec)

void FFloor101DocumentToolsetSpec::Define()
{
	Describe("wall lifecycle", [this]()
	{
		It("adds the reference wall, regenerates it from 3550 to 4200 mm in the same actor, and removes it", [this]()
		{
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(0, 0, 3550, 0, 120, 2400);
			TestEqual(TEXT("length"), Added.LengthMm, 3550.0);
			TestEqual(TEXT("thickness"), Added.ThicknessMm, int64(120));
			TestEqual(TEXT("height"), Added.HeightMm, int64(2400));
			TestTrue(TEXT("listed"), HasWall(Added.Id));
			if (!TestNotNull(TEXT("actor"), Added.Actor.Get()))
			{
				return;
			}
			TestEqual(TEXT("actor size 355 x 12 x 240 cm"), ActorSizeCm(Added), FVector(355.0, 12.0, 240.0), 1e-3f);

			const FFloor101WallInfo Moved = UFloor101DocumentToolset::SetWallEnd(Added.Id, 4200, 0);
			TestEqual(TEXT("same id"), Moved.Id, Added.Id);
			TestEqual(TEXT("new length"), Moved.LengthMm, 4200.0);
			TestTrue(TEXT("same actor"), Moved.Actor == Added.Actor);
			TestEqual(TEXT("actor size 420 x 12 x 240 cm"), ActorSizeCm(Moved), FVector(420.0, 12.0, 240.0), 1e-3f);

			UFloor101DocumentToolset::RemoveWall(Added.Id);
			TestFalse(TEXT("removed"), HasWall(Added.Id));
		});
	});

	Describe("dimensions", [this]()
	{
		It("changes length, thickness and height, each regenerating the same actor", [this]()
		{
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(0, 0, 3550, 0, 120, 2400);
			const FFloor101WallInfo Longer = UFloor101DocumentToolset::SetWallLength(Added.Id, 4200);
			TestEqual(TEXT("end moved along the axis"), Longer.EndXMm, int64(4200));
			const FFloor101WallInfo Thicker = UFloor101DocumentToolset::SetWallThickness(Added.Id, 150);
			TestEqual(TEXT("thickness"), Thicker.ThicknessMm, int64(150));
			const FFloor101WallInfo Taller = UFloor101DocumentToolset::SetWallHeight(Added.Id, 2700);
			TestEqual(TEXT("height"), Taller.HeightMm, int64(2700));
			if (TestNotNull(TEXT("actor"), Taller.Actor.Get()))
			{
				TestTrue(TEXT("same actor"), Taller.Actor == Added.Actor);
				TestEqual(TEXT("actor size 420 x 15 x 270 cm"), ActorSizeCm(Taller), FVector(420.0, 15.0, 270.0), 1e-3f);
			}
			UFloor101DocumentToolset::RemoveWall(Added.Id);
		});
	});

	Describe("history and files", [this]()
	{
		It("undoes and redoes a change, returning what it was", [this]()
		{
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(0, 0, 3550, 0, 120, 2400);
			TestEqual(TEXT("undo label"), UFloor101DocumentToolset::Undo(), FString(TEXT("Create wall")));
			TestFalse(TEXT("undone"), HasWall(Added.Id));
			TestEqual(TEXT("redo label"), UFloor101DocumentToolset::Redo(), FString(TEXT("Create wall")));
			TestTrue(TEXT("redone with the same id"), HasWall(Added.Id));
			UFloor101DocumentToolset::RemoveWall(Added.Id);
			ExpectRaises(TEXT("nothing to redo"), [] { UFloor101DocumentToolset::Redo(); });
		});

		It("saves and opens project.json; opening clears the history", [this]()
		{
			const FString Path = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Toolset"), TEXT("project.json")));
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(0, 0, 3550, 0, 120, 2400);
			UFloor101DocumentToolset::SaveProject(Path);
			TestTrue(TEXT("file written"), IFileManager::Get().FileExists(*Path));
			UFloor101DocumentToolset::RemoveWall(Added.Id);
			const TArray<FFloor101WallInfo> Opened = UFloor101DocumentToolset::OpenProject(Path);
			TestTrue(TEXT("the saved wall is back"), Opened.ContainsByPredicate([&Added](const FFloor101WallInfo& Wall) { return Wall.Id == Added.Id; }));
			ExpectRaises(TEXT("nothing to undo"), [] { UFloor101DocumentToolset::Undo(); });
			UFloor101DocumentToolset::RemoveWall(Added.Id);
			IFileManager::Get().Delete(*Path, false, true, true);
		});
	});

	Describe("errors", [this]()
	{
		It("AddWall raises on a degenerate wall", [this]()
		{
			ExpectRaises(TEXT("wall rejected"), [] { UFloor101DocumentToolset::AddWall(0, 0, 0, 0, 120, 2400); });
		});

		It("SetWallEnd raises on a malformed id", [this]()
		{
			ExpectRaises(TEXT("is not a wall id"), [] { UFloor101DocumentToolset::SetWallEnd(TEXT("not-a-uuid"), 10, 0); });
		});

		It("SetWallEnd raises on an unknown id", [this]()
		{
			ExpectRaises(TEXT("cannot move the end"),
				[] { UFloor101DocumentToolset::SetWallEnd(TEXT("00000000-0000-0000-0000-0000000000ff"), 10, 0); });
		});

		It("SetWallEnd raises when the end would coincide with the start", [this]()
		{
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(0, 0, 1000, 0, 120, 2400);
			ExpectRaises(TEXT("cannot move the end"), [&Added] { UFloor101DocumentToolset::SetWallEnd(Added.Id, 0, 0); });
			UFloor101DocumentToolset::RemoveWall(Added.Id);
		});

		It("SetWallLength, SetWallThickness and SetWallHeight raise on non-positive values", [this]()
		{
			const FFloor101WallInfo Added = UFloor101DocumentToolset::AddWall(0, 0, 1000, 0, 120, 2400);
			ExpectRaises(TEXT("cannot change the length"), [&Added] { UFloor101DocumentToolset::SetWallLength(Added.Id, 0); });
			ExpectRaises(TEXT("cannot change the thickness"), [&Added] { UFloor101DocumentToolset::SetWallThickness(Added.Id, 0); });
			ExpectRaises(TEXT("cannot change the height"), [&Added] { UFloor101DocumentToolset::SetWallHeight(Added.Id, -1); });
			UFloor101DocumentToolset::RemoveWall(Added.Id);
		});

		It("SaveProject raises when the folder cannot be created", [this]()
		{
			const FString Path = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("bad<>|folder"), TEXT("project.json")));
			AddExpectedMessagePlain(TEXT("F101: Cannot create the folder"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("cannot save"), [&Path] { UFloor101DocumentToolset::SaveProject(Path); });
		});

		It("OpenProject raises on a missing file", [this]()
		{
			const FString Path = FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("Floor101Toolset"), TEXT("missing.json")));
			AddExpectedMessagePlain(TEXT("F101: Cannot open"), ELogVerbosity::Warning); // the status message
			ExpectRaises(TEXT("cannot open"), [&Path] { UFloor101DocumentToolset::OpenProject(Path); });
		});

		It("RemoveWall raises on an unknown id", [this]()
		{
			ExpectRaises(TEXT("cannot remove wall"),
				[] { UFloor101DocumentToolset::RemoveWall(TEXT("00000000-0000-0000-0000-0000000000ff")); });
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
