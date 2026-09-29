#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"

#include "Floor101GenerateAssetsCommandlet.generated.h"

// Builds the project's few assets from code, so Content/ is reproducible from Git + Scripts (no manual editor steps):
// the materials of the finishes, /Game/Floor101/Materials/M_Floor101Finish (ADR-0012) and M_Floor101Glass
// (ADR-0013). Run it with Scripts/GenerateAssets.ps1 (UnrealEditor-Cmd <project> -run=Floor101GenerateAssets).
UCLASS()
class UFloor101GenerateAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UFloor101GenerateAssetsCommandlet();

	virtual int32 Main(const FString& Params) override;

	// Create (or recreate) the materials in memory; the finish material returns null if the engine texture it needs is
	// missing.
	static UMaterial* BuildFinishMaterial(UPackage* Package);
	static UMaterial* BuildGlassMaterial(UPackage* Package);
};
