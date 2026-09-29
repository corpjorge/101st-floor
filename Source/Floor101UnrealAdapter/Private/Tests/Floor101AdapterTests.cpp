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
#include "Engine/Texture2D.h"
#include "Floor101FinishLibrary.h"
#include "Floor101SceneNodeActor.h"
#include "Floor101SceneReconciler.h"
#include "Floor101UnrealConversion.h"
#include "Materials/MaterialInstanceDynamic.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101WallOpeningTest, "Floor101.Adapter.WallOpening", TestFlags)
bool FFloor101WallOpeningTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	f101::cad::Project Project;
	const f101::cad::WallId WallId{f101::cad::Uuid{0, 1}};
	f101::cad::Wall Wall{WallId, {0, 0}, {4000, 0}, 120, 2400};
	Wall.openings.push_back(f101::cad::Opening{f101::cad::OpeningId{f101::cad::Uuid{0, 2}}, f101::cad::OpeningKind::Door, 500, 900, 2100, 0});
	Project.walls.push_back(Wall);

	FFloor101SceneReconciler Reconciler(TestWorld.World);
	Reconciler.Reconcile(f101::scene::compileScene(Project));
	AFloor101SceneNodeActor* Actor = Reconciler.FindActor(f101::scene::wallBodyNodeId(WallId));
	if (!TestNotNull(TEXT("wall actor"), Actor))
	{
		return false;
	}
	int32 Triangles = 0;
	Actor->GetMeshComponent()->ProcessMesh([&Triangles](const UE::Geometry::FDynamicMesh3& Mesh) { Triangles = Mesh.TriangleCount(); });
	TestEqual(TEXT("door hole: 36 triangles (12 for a plain wall)"), Triangles, 36);
	TestEqual(TEXT("seven material slots (reveal added)"), Actor->GetMeshComponent()->GetNumMaterials(), 7);
	TestEqual(TEXT("bounds unchanged: 400 cm"), MeshBounds(*Actor).GetSize().X, 400.0, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101ViewStylesTest, "Floor101.Adapter.ViewStyles", TestFlags)
bool FFloor101ViewStylesTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	f101::cad::Project Project;
	const f101::cad::WallId WallId{f101::cad::Uuid{0, 1}};
	Project.walls.push_back(f101::cad::Wall{WallId, {0, 0}, {3550, 0}, 120, 2400});
	FFloor101SceneReconciler Reconciler(TestWorld.World);
	Reconciler.Reconcile(f101::scene::compileScene(Project));
	AFloor101SceneNodeActor* Wall = Reconciler.FindActor(f101::scene::wallBodyNodeId(WallId));
	if (!TestNotNull(TEXT("wall actor"), Wall))
	{
		return false;
	}
	UMaterialInterface* DefaultMaterial = Wall->GetDisplayMaterial();
	FLinearColor Color;
	TestTrue(TEXT("the default material has the colour parameter the styles tint"),
		DefaultMaterial && DefaultMaterial->GetVectorParameterValue(FHashedMaterialParameterInfo(AFloor101SceneNodeActor::ColorParameterName), Color));

	// Selection only changes the style: same geometry hash, but the actor is updated in place.
	f101::scene::ViewOverlay Overlay;
	Overlay.selectedWall = WallId;
	const FFloor101ReconcileStats Selected = Reconciler.Reconcile(f101::scene::compileScene(Project, Overlay));
	TestEqual(TEXT("style change updates the actor"), Selected.Updated, 1);
	TestTrue(TEXT("same actor"), Reconciler.FindActor(f101::scene::wallBodyNodeId(WallId)) == Wall);
	const UMaterialInstanceDynamic* Tinted = Cast<UMaterialInstanceDynamic>(Wall->GetDisplayMaterial());
	TestNotNull(TEXT("selected wall uses a tinted instance"), Tinted);
	if (Tinted)
	{
		TestTrue(TEXT("tint overrides the colour"),
			Tinted->GetVectorParameterValue(FHashedMaterialParameterInfo(AFloor101SceneNodeActor::ColorParameterName), Color, true));
		TestTrue(TEXT("tint is orange"), Color.R > 0.9f && Color.B < 0.2f);
	}

	// A gesture previews walls: the dragged wall is replaced in place (same actor, preview tint, joined to the new
	// wall at the corner) and the wall being drawn is added.
	Overlay.previewWalls = {f101::cad::Wall{WallId, {0, 0}, {4200, 0}, 120, 2400},
		f101::cad::Wall{f101::cad::WallId{}, {4200, 0}, {4200, 3000}, 120, 2400}};
	Overlay.markers = {{0, 0}, {4200, 0}};
	const FFloor101ReconcileStats Dragging = Reconciler.Reconcile(f101::scene::compileScene(Project, Overlay));
	TestEqual(TEXT("drawn wall and two handles created"), Dragging.Created, 3);
	TestTrue(TEXT("the dragged wall keeps its actor"), Reconciler.FindActor(f101::scene::wallBodyNodeId(WallId)) == Wall);
	TestEqual(TEXT("dragged wall reaches the mitred outer corner (426 cm)"), MeshBounds(*Wall).GetSize().X, 426.0, Tolerance);
	TestNotNull(TEXT("dragged wall is tinted as a preview"), Cast<UMaterialInstanceDynamic>(Wall->GetDisplayMaterial()));
	AFloor101SceneNodeActor* Drawn = Reconciler.FindActor(f101::scene::SceneNodeId{"preview/wall/0"});
	if (TestNotNull(TEXT("drawn wall actor"), Drawn))
	{
		TestEqual(TEXT("drawn wall from the outer corner to 3000 mm (306 cm)"), MeshBounds(*Drawn).GetSize().Y, 306.0, Tolerance);
		TestNotNull(TEXT("drawn wall is tinted"), Cast<UMaterialInstanceDynamic>(Drawn->GetDisplayMaterial()));
	}
	TestNotNull(TEXT("handle actor"), Reconciler.FindActor(f101::scene::SceneNodeId{"marker/1"}));

	const FFloor101ReconcileStats Plain = Reconciler.Reconcile(f101::scene::compileScene(Project, f101::scene::ViewOverlay{}));
	TestEqual(TEXT("drawn wall and handles removed"), Plain.Removed, 3);
	TestEqual(TEXT("wall back to 355 cm"), MeshBounds(*Wall).GetSize().X, 355.0, Tolerance);
	TestTrue(TEXT("wall back to the default material"), Wall->GetDisplayMaterial() == DefaultMaterial);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101FinishesTest, "Floor101.Adapter.Finishes", TestFlags)
bool FFloor101FinishesTest::RunTest(const FString& Parameters)
{
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	if (!TestNotNull(TEXT("finish library"), Library) || !TestNotNull(TEXT("generated base material"), Library->GetBaseMaterial()))
	{
		return false;
	}
	for (const FName Parameter : {UFloor101FinishLibrary::TintParameter, UFloor101FinishLibrary::RoughnessParameter,
			 UFloor101FinishLibrary::RepeatUParameter, UFloor101FinishLibrary::RepeatVParameter})
	{
		float Scalar = 0.0f;
		FLinearColor Vector;
		const FHashedMaterialParameterInfo Info(Parameter);
		TestTrue(*FString::Printf(TEXT("base material has %s"), *Parameter.ToString()),
			Library->GetBaseMaterial()->GetScalarParameterValue(Info, Scalar) || Library->GetBaseMaterial()->GetVectorParameterValue(Info, Vector));
	}

	// A wall with bricks on its left face and the default plaster on the right.
	FTestWorld TestWorld;
	f101::cad::Project Project;
	const f101::cad::WallId WallId{f101::cad::Uuid{0, 1}};
	f101::cad::Wall Wall{WallId, {0, 0}, {3550, 0}, 120, 2400};
	Wall.finishLeft = "brick-red";
	Project.walls.push_back(Wall);
	FFloor101SceneReconciler Reconciler(TestWorld.World);
	const f101::scene::Scene Scene = f101::scene::compileScene(Project);
	Reconciler.Reconcile(Scene);
	AFloor101SceneNodeActor* Actor = Reconciler.FindActor(f101::scene::wallBodyNodeId(WallId));
	if (!TestNotNull(TEXT("wall actor"), Actor))
	{
		return false;
	}
	const f101::scene::SceneNode& Node = *Scene.findNode(f101::scene::wallBodyNodeId(WallId));
	for (int32 Section = 0; Section < static_cast<int32>(Node.mesh.sections.size()); ++Section)
	{
		const FString Slot = UTF8_TO_TCHAR(Node.mesh.sections[Section].slot.c_str());
		const FString Expected = Slot == TEXT("left") ? TEXT("brick-red") : TEXT("plaster-white");
		TestTrue(*FString::Printf(TEXT("%s face shows %s"), *Slot, *Expected),
			Actor->GetMeshComponent()->GetMaterial(Section) == Library->GetFinishMaterial(Expected));
	}

	const UMaterialInterface* Bricks = Library->GetFinishMaterial(TEXT("brick-red"));
	UTexture2D* Texture = Library->FindFinishTexture(TEXT("brick-red"));
	if (TestNotNull(TEXT("brick texture"), Texture))
	{
		TestEqual(TEXT("512 pixels"), Texture->GetSizeX(), 512);
		TestEqual(TEXT("the whole mip chain"), Texture->GetNumMips(), 10);
		TestTrue(TEXT("sRGB"), Texture->SRGB);
		UTexture* Bound = nullptr;
		TestTrue(TEXT("bound to the material"), Bricks->GetTextureParameterValue(FHashedMaterialParameterInfo(UFloor101FinishLibrary::TextureParameter), Bound));
		TestTrue(TEXT("the brick texture"), Bound == Texture);
	}
	float Repeat = 0.0f;
	TestTrue(TEXT("repeat along the wall"), Bricks->GetScalarParameterValue(FHashedMaterialParameterInfo(UFloor101FinishLibrary::RepeatUParameter), Repeat));
	TestEqual(TEXT("four 250 mm brick modules = 1 m"), Repeat, 1.0f, 1e-5f);
	float Roughness = 0.0f;
	TestTrue(TEXT("roughness"), Bricks->GetScalarParameterValue(FHashedMaterialParameterInfo(UFloor101FinishLibrary::RoughnessParameter), Roughness));
	TestEqual(TEXT("brick is matte"), Roughness, 0.9f, 1e-5f);
	TestTrue(TEXT("one material per finish, shared"), Library->GetFinishMaterial(TEXT("brick-red")) == Bricks);

	// Changing a finish updates the same actor.
	Project.walls[0].finishLeft = "paint-sage";
	const FFloor101ReconcileStats Repainted = Reconciler.Reconcile(f101::scene::compileScene(Project));
	TestEqual(TEXT("a finish change updates the actor"), Repainted.Updated, 1);
	TestTrue(TEXT("same actor"), Reconciler.FindActor(f101::scene::wallBodyNodeId(WallId)) == Actor);
	TestTrue(TEXT("left face repainted"), Actor->GetDisplayMaterial() == Library->GetFinishMaterial(TEXT("paint-sage")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloor101CarpentryTest, "Floor101.Adapter.Carpentry", TestFlags)
bool FFloor101CarpentryTest::RunTest(const FString& Parameters)
{
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	if (!TestNotNull(TEXT("finish library"), Library) || !TestNotNull(TEXT("generated glass material"), Library->GetGlassMaterial()))
	{
		return false;
	}
	TestEqual(TEXT("glass is translucent"), Library->GetGlassMaterial()->GetBlendMode(), BLEND_Translucent);

	// A wall with a window: its carpentry is a node of its own, the frame in lacquer and the pane in glass.
	FTestWorld TestWorld;
	f101::cad::Project Project;
	const f101::cad::WallId WallId{f101::cad::Uuid{0, 1}};
	f101::cad::Wall Wall{WallId, {0, 0}, {4000, 0}, 120, 2800};
	const f101::cad::OpeningId WindowId{f101::cad::Uuid{0, 2}};
	Wall.openings.push_back(f101::cad::Opening{WindowId, f101::cad::OpeningKind::Window, 1000, 1200, 1200, 900});
	Project.walls.push_back(Wall);
	const f101::scene::Scene Scene = f101::scene::compileScene(Project);
	FFloor101SceneReconciler Reconciler(TestWorld.World);
	Reconciler.Reconcile(Scene);
	const f101::scene::SceneNode* Found = nullptr;
	for (const f101::scene::SceneNode& Candidate : Scene.nodes)
	{
		if (Candidate.sourceKind == f101::scene::SourceKind::Opening)
		{
			Found = &Candidate;
		}
	}
	AFloor101SceneNodeActor* Window = Found ? Reconciler.FindActor(Found->id) : nullptr;
	if (!TestNotNull(TEXT("window actor"), Window))
	{
		return false;
	}
	const f101::scene::SceneNode& Node = *Found;
	for (int32 Section = 0; Section < static_cast<int32>(Node.mesh.sections.size()); ++Section)
	{
		const FString Slot = UTF8_TO_TCHAR(Node.mesh.sections[Section].slot.c_str());
		UMaterialInterface* Material = Window->GetMeshComponent()->GetMaterial(Section);
		if (Slot == TEXT("glass"))
		{
			TestTrue(TEXT("pane in glass"), Material == Library->GetFinishMaterial(TEXT("glass-clear")));
			TestEqual(TEXT("see-through"), Material->GetBlendMode(), BLEND_Translucent);
			float Opacity = 1.0f;
			TestTrue(TEXT("opacity set"), Material->GetScalarParameterValue(FHashedMaterialParameterInfo(UFloor101FinishLibrary::OpacityParameter), Opacity));
			TestTrue(TEXT("mostly clear"), Opacity < 0.5f);
		}
		else
		{
			TestTrue(*FString::Printf(TEXT("%s in white lacquer"), *Slot), Material == Library->GetFinishMaterial(TEXT("lacquer-white")));
			TestEqual(TEXT("opaque"), Material->GetBlendMode(), BLEND_Opaque);
		}
	}
	const FBox Bounds = Window->GetMeshBounds();
	TestEqual(TEXT("in the hole: 120 cm wide"), Bounds.GetSize().X, 120.0, Tolerance);
	TestEqual(TEXT("from the sill (90 cm)"), Bounds.Min.Z, 90.0, Tolerance);
	TestEqual(TEXT("7 cm deep frame"), Bounds.GetSize().Y, 7.0, Tolerance);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
