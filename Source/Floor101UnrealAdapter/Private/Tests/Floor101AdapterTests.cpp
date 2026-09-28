// Unreal integration tests of the adapter (docs/architecture.md, "Tests"). Run with .\Scripts\Test.ps1 -Integration
// or through the MCP AutomationTestToolset.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "F101/Cad/Project.h"
#include "F101/Scene/SceneCompiler.h"
#include "Floor101SceneNodeActor.h"
#include "Floor101SceneReconciler.h"
#include "Floor101UnrealConversion.h"

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr double Tolerance = 1e-6;
	constexpr float VectorTolerance = 1e-4f; // TestEqual(FVector, ...) takes a float tolerance

	// Minimal game world owned by a test.
	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("Floor101AdapterTestWorld"));
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}

		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	FBox MeshBounds(const AFloor101SceneNodeActor& Actor)
	{
		return Actor.GetMeshBounds();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101ConversionTest, "Floor101.Adapter.Conversion", TestFlags)
bool FFloor101ConversionTest::RunTest(const FString& Parameters)
{
	using namespace Floor101::Conversion;
	TestEqual(TEXT("3550 mm = 355 cm"), ToUnrealLength(3550.0), 355.0, Tolerance);
	TestEqual(TEXT("120 mm = 12 cm"), ToUnrealLength(120.0), 12.0, Tolerance);
	TestEqual(TEXT("2400 mm = 240 cm"), ToUnrealLength(2400.0), 240.0, Tolerance);
	TestEqual(TEXT("Y is mirrored (right- to left-handed)"), ToUnrealPosition({1000.0, 2000.0, 3000.0}), FVector3d(100.0, -200.0, 300.0), VectorTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101ReferenceWallTest, "Floor101.Adapter.ReferenceWall", TestFlags)
bool FFloor101ReferenceWallTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	// The adapter knows scenes, not the CAD model: the project is built directly and compiled like the app does.
	f101::cad::Project Project;
	const f101::cad::WallId WallId{f101::cad::Uuid{0, 1}};
	Project.walls.push_back(f101::cad::Wall{WallId, {0, 0}, {3550, 0}, 120, 2400});

	FFloor101SceneReconciler Reconciler(TestWorld.World);
	const FFloor101ReconcileStats Created = Reconciler.Reconcile(f101::scene::compileScene(Project));
	TestEqual(TEXT("one actor created"), Created.Created, 1);

	AFloor101SceneNodeActor* Actor = Reconciler.FindActor(f101::scene::wallBodyNodeId(WallId));
	if (!TestNotNull(TEXT("wall actor"), Actor))
	{
		return false;
	}
	UDynamicMeshComponent* Mesh = Actor->GetMeshComponent();
	TestNotNull(TEXT("dynamic mesh component"), Mesh);
	TestTrue(TEXT("actor is transient (never saved)"), Actor->HasAnyFlags(RF_Transient));
	TestTrue(TEXT("actor tagged with its scene node id"), Actor->ActorHasTag(AFloor101SceneNodeActor::MakeNodeTag(TEXT("00000000-0000-0000-0000-000000000001/body"))));
	TestEqual(TEXT("one material slot per wall face"), Mesh->GetNumMaterials(), 6);
	TestEqual(TEXT("actor at origin"), Actor->GetActorLocation(), FVector::ZeroVector, VectorTolerance);

	const FBox Box = MeshBounds(*Actor);
	TestEqual(TEXT("length 355 cm"), Box.GetSize().X, 355.0, Tolerance);
	TestEqual(TEXT("thickness 12 cm"), Box.GetSize().Y, 12.0, Tolerance);
	TestEqual(TEXT("height 240 cm"), Box.GetSize().Z, 240.0, Tolerance);
	TestEqual(TEXT("starts at x = 0"), Box.Min.X, 0.0, Tolerance);
	TestEqual(TEXT("centred on the axis"), Box.GetCenter().Y, 0.0, Tolerance);
	TestEqual(TEXT("stands on z = 0"), Box.Min.Z, 0.0, Tolerance);

	// Unreal's own triangle normal must agree with the converted normals: faces point outward, none is inverted.
	int32 TrianglesChecked = 0;
	int32 InvertedTriangles = 0;
	Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& DynamicMesh)
	{
		const UE::Geometry::FDynamicMeshNormalOverlay* Normals = DynamicMesh.Attributes()->PrimaryNormals();
		for (const int32 TriangleId : DynamicMesh.TriangleIndicesItr())
		{
			const FVector3f Expected = Normals->GetElement(Normals->GetTriangle(TriangleId).A);
			InvertedTriangles += FVector3d::DotProduct(DynamicMesh.GetTriNormal(TriangleId), FVector3d(Expected)) > 0.99 ? 0 : 1;
			++TrianglesChecked;
		}
	});
	TestEqual(TEXT("12 triangles"), TrianglesChecked, 12);
	TestEqual(TEXT("no inverted triangles"), InvertedTriangles, 0);
	AddInfo(FString::Printf(TEXT("3550 mm wall -> actor %s, size (%.3f, %.3f, %.3f) cm, %d triangles, %d inverted"),
		*Actor->GetName(), Box.GetSize().X, Box.GetSize().Y, Box.GetSize().Z, TrianglesChecked, InvertedTriangles));

	// 3550 -> 4200 mm: the same actor is rebuilt, never scaled.
	Project.walls[0].end = {4200, 0};
	const FFloor101ReconcileStats Updated = Reconciler.Reconcile(f101::scene::compileScene(Project));
	TestEqual(TEXT("one actor updated"), Updated.Updated, 1);
	TestEqual(TEXT("no actor created"), Updated.Created, 0);
	TestTrue(TEXT("same actor"), Reconciler.FindActor(f101::scene::wallBodyNodeId(WallId)) == Actor);
	TestEqual(TEXT("length 420 cm"), MeshBounds(*Actor).GetSize().X, 420.0, Tolerance);
	TestEqual(TEXT("thickness still 12 cm"), MeshBounds(*Actor).GetSize().Y, 12.0, Tolerance);
	TestEqual(TEXT("actor scale stays 1"), Actor->GetActorScale3D(), FVector::OneVector, VectorTolerance);
	TestEqual(TEXT("component scale stays 1"), Mesh->GetRelativeScale3D(), FVector::OneVector, VectorTolerance);
	AddInfo(FString::Printf(TEXT("4200 mm wall -> same actor %s, size (%.3f, %.3f, %.3f) cm, actor scale %s"),
		*Actor->GetName(), MeshBounds(*Actor).GetSize().X, MeshBounds(*Actor).GetSize().Y, MeshBounds(*Actor).GetSize().Z,
		*Actor->GetActorScale3D().ToString()));

	const FFloor101ReconcileStats Unchanged = Reconciler.Reconcile(f101::scene::compileScene(Project));
	TestEqual(TEXT("unchanged scene rebuilds nothing"), Unchanged.Unchanged, 1);

	Project.walls.clear();
	const FFloor101ReconcileStats Removed = Reconciler.Reconcile(f101::scene::compileScene(Project));
	TestEqual(TEXT("one actor removed"), Removed.Removed, 1);
	TestTrue(TEXT("actor destroyed"), !IsValid(Actor) || Actor->IsActorBeingDestroyed());
	TestEqual(TEXT("no actors left"), Reconciler.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
