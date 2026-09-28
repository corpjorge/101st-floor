#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "Floor101EditorToolsetSubsystem.generated.h"

// Registers the 101st Floor toolsets with the ToolsetRegistry once it is ready (RegisterToolsetClass is a silent no-op
// before that), so they become visible to Unreal MCP.
UCLASS()
class UFloor101EditorToolsetSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
};
