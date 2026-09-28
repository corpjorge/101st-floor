#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Floor101DocumentToolset.h"
#include "GameFramework/Actor.h"
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

		It("RemoveWall raises on an unknown id", [this]()
		{
			ExpectRaises(TEXT("cannot remove wall"),
				[] { UFloor101DocumentToolset::RemoveWall(TEXT("00000000-0000-0000-0000-0000000000ff")); });
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
