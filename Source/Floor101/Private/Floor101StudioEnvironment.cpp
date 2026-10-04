#include "Floor101StudioEnvironment.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "UDynamicMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "EngineUtils.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101FinishLibrary.h"
#include "Floor101Preferences.h"
#include "Floor101SceneNodeActor.h"
#include "Floor101Style.h"
#include "F101/Cad/EditController.h"
#include "F101/Cad/Document.h"
#include "Materials/MaterialInstanceDynamic.h"

AFloor101StudioEnvironment::AFloor101StudioEnvironment()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AFloor101StudioEnvironment::ConfigureTemplate()
{
	UFloor101FinishLibrary* Finishes = UFloor101FinishLibrary::Get();
	UMaterialInterface* Base = Finishes ? Finishes->GetBaseMaterial() : nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UStaticMeshComponent*> Components;
		It->GetComponents(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			const UStaticMesh* Mesh = Component->GetStaticMesh();
			if (!Mesh) { continue; }
			// Touch only the engine template assets, never a user's model or another map's ground.
			if (Mesh->GetPathName() == TEXT("/Engine/MapTemplates/SM_Template_Map_Floor.SM_Template_Map_Floor") && Base)
			{
				auto* Neutral = UMaterialInstanceDynamic::Create(Base, this);
				Neutral->SetVectorParameterValue(TEXT("Tint"), Floor101::Studio::Color(TEXT("DAD8D0")));
				Neutral->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
				Component->SetMaterial(0, Neutral);
				Component->SetCastShadow(false);
			}

		}
	}
	for (TActorIterator<ASkyAtmosphere> It(GetWorld()); It; ++It)
	{
		if (auto* Atmosphere = It->FindComponentByClass<USkyAtmosphereComponent>())
		{
			// These colors are normalized; their physical coefficient is a separate scale.
			// Preserve the template's energy while removing its strong blue cast.
			const float Rayleigh = Atmosphere->RayleighScattering.GetLuminance();
			const float Mie = Atmosphere->MieScattering.GetLuminance();
			Atmosphere->SetRayleighScattering(FLinearColor(Rayleigh, Rayleigh, Rayleigh));
			Atmosphere->SetMieScattering(FLinearColor(Mie, Mie, Mie));
		}
	}
	for (TActorIterator<AVolumetricCloud> It(GetWorld()); It; ++It) { It->SetActorHiddenInGame(true); }
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		if (auto* Light = It->FindComponentByClass<UDirectionalLightComponent>())
		{
			Light->SetLightSourceAngle(3.0f);
			Light->SetLightColor(FLinearColor(1.0f, 0.98f, 0.94f));
		}
	}
}

void AFloor101StudioEnvironment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bConfigured) { ConfigureTemplate(); bConfigured = true; }
	auto* Document = GetWorld()->GetSubsystem<UFloor101DocumentSubsystem>();
	if (!Document || !Document->AreConstructionAidsVisible()) { return; }
	if (Floor101::Preferences::ShowGrid())
	{
		const auto& Editor = Document->GetEditController();
		const auto Elevation = f101::cad::levelElevationMm(Document->GetDocument().project(), Editor.activeLevel());
		const double Z = static_cast<double>(Elevation) * 0.1 + 0.2;
		const FColor GridColor(150, 154, 149);
		// 1 m presentation grid, independent of the controller's millimetre snapping grid.
		for (int32 Coordinate = -2000; Coordinate <= 2000; Coordinate += 100)
		{
			DrawDebugLine(GetWorld(), FVector(Coordinate, -2000, Z), FVector(Coordinate, 2000, Z), GridColor, false, 0.0f, 0, 1.0f);
			DrawDebugLine(GetWorld(), FVector(-2000, Coordinate, Z), FVector(2000, Coordinate, Z), GridColor, false, 0.0f, 0, 1.0f);
		}
	}
	if (Document->IsPlanView()) { return; } // the drawing owns the plan's selection strokes
	for (TActorIterator<AFloor101SceneNodeActor> It(GetWorld()); It; ++It)
	{
		if (!It->IsStudioSelected()) { continue; }
		const FTransform Transform = It->GetActorTransform();
		It->GetMeshComponent()->GetDynamicMesh()->ProcessMesh([this, Transform](const UE::Geometry::FDynamicMesh3& Mesh)
		{
			for (int32 Edge : Mesh.EdgeIndicesItr())
			{
				const auto Triangles = Mesh.GetEdgeT(Edge);
				// Suppress coplanar triangulation diagonals; outline geometric creases and open boundaries.
				if (Triangles.B >= 0 && Mesh.GetTriNormal(Triangles.A).Dot(Mesh.GetTriNormal(Triangles.B)) > 0.98) { continue; }
				const auto Vertices = Mesh.GetEdgeV(Edge);
				DrawDebugLine(GetWorld(), Transform.TransformPosition(FVector(Mesh.GetVertex(Vertices.A))),
					Transform.TransformPosition(FVector(Mesh.GetVertex(Vertices.B))), FColor(214, 173, 120), false, 0.0f, 0, 1.2f);
			}
		});
	}
}
