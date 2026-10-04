#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Floor101StudioEnvironment.generated.h"

class UMaterialInterface;

// Transient presentation of the studio; never part of the CAD model or exports.
UCLASS(Transient, NotPlaceable)
class AFloor101StudioEnvironment : public AActor
{
	GENERATED_BODY()
public:
	AFloor101StudioEnvironment();
	virtual void Tick(float DeltaSeconds) override;
private:
	void ConfigureTemplate();
	bool bConfigured = false;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Backdrop;
};
