#include "Floor101EditorToolsetSubsystem.h"

#include "Floor101DocumentToolset.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

void UFloor101EditorToolsetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UToolsetRegistrySubsystem>();
	UToolsetRegistry::RegisterToolsetClass(UFloor101DocumentToolset::StaticClass());
}

void UFloor101EditorToolsetSubsystem::Deinitialize()
{
	UToolsetRegistry::UnregisterToolsetClass(UFloor101DocumentToolset::StaticClass());
	Super::Deinitialize();
}
