#include "Floor101GenerateAssetsCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogFloor101Assets, Log, All);

namespace
{
	const TCHAR* FinishMaterialPackage = TEXT("/Game/Floor101/Materials/M_Floor101Finish");
	const TCHAR* WhiteTexturePath = TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture");

	template <typename T>
	T* AddExpression(UMaterial* Material, int32 X, int32 Y)
	{
		T* Expression = NewObject<T>(Material);
		Expression->MaterialExpressionEditorX = X;
		Expression->MaterialExpressionEditorY = Y;
		Material->GetExpressionCollection().AddExpression(Expression);
		return Expression;
	}

	UMaterialExpressionScalarParameter* AddScalar(UMaterial* Material, const TCHAR* Name, float Default, int32 X, int32 Y)
	{
		UMaterialExpressionScalarParameter* Parameter = AddExpression<UMaterialExpressionScalarParameter>(Material, X, Y);
		Parameter->ParameterName = Name;
		Parameter->DefaultValue = Default;
		return Parameter;
	}
}

UFloor101GenerateAssetsCommandlet::UFloor101GenerateAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

UMaterial* UFloor101GenerateAssetsCommandlet::BuildFinishMaterial(UPackage* Package)
{
	UTexture2D* White = LoadObject<UTexture2D>(nullptr, WhiteTexturePath);
	if (!White)
	{
		UE_LOG(LogFloor101Assets, Error, TEXT("Missing engine texture %s"), WhiteTexturePath);
		return nullptr;
	}

	// BaseColor = BaseTexture(uv / (RepeatU, RepeatV)) * Tint; Roughness = Roughness. Mesh UVs are in metres, the
	// repeats are the metres one texture covers (f101::scene::FinishTexture), and Tint shows view styles.
	UMaterial* Material = NewObject<UMaterial>(Package, FName(FPackageName::GetShortName(FinishMaterialPackage)),
		RF_Public | RF_Standalone | RF_Transactional);

	UMaterialExpressionTextureCoordinate* Uv = AddExpression<UMaterialExpressionTextureCoordinate>(Material, -900, 0);
	UMaterialExpressionScalarParameter* RepeatU = AddScalar(Material, TEXT("RepeatU"), 1.0f, -900, 120);
	UMaterialExpressionScalarParameter* RepeatV = AddScalar(Material, TEXT("RepeatV"), 1.0f, -900, 220);
	UMaterialExpressionAppendVector* Repeat = AddExpression<UMaterialExpressionAppendVector>(Material, -700, 160);
	Repeat->A.Expression = RepeatU;
	Repeat->B.Expression = RepeatV;
	UMaterialExpressionDivide* Tiled = AddExpression<UMaterialExpressionDivide>(Material, -550, 60);
	Tiled->A.Expression = Uv;
	Tiled->B.Expression = Repeat;

	UMaterialExpressionTextureSampleParameter2D* Base = AddExpression<UMaterialExpressionTextureSampleParameter2D>(Material, -400, 0);
	Base->ParameterName = TEXT("BaseTexture");
	Base->Texture = White;
	Base->SamplerType = SAMPLERTYPE_Color;
	Base->Coordinates.Expression = Tiled;

	UMaterialExpressionVectorParameter* Tint = AddExpression<UMaterialExpressionVectorParameter>(Material, -400, 260);
	Tint->ParameterName = TEXT("Tint");
	Tint->DefaultValue = FLinearColor::White;

	UMaterialExpressionMultiply* Color = AddExpression<UMaterialExpressionMultiply>(Material, -150, 60);
	Color->A.Expression = Base;
	Color->B.Expression = Tint;

	UMaterialExpressionScalarParameter* Roughness = AddScalar(Material, TEXT("Roughness"), 0.85f, -150, 260);

	UMaterialEditorOnlyData* Inputs = Material->GetEditorOnlyData();
	Inputs->BaseColor.Expression = Color;
	Inputs->Roughness.Expression = Roughness;

	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	return Material;
}

int32 UFloor101GenerateAssetsCommandlet::Main(const FString& Params)
{
	UPackage* Package = CreatePackage(FinishMaterialPackage);
	Package->FullyLoad();
	if (UObject* Existing = StaticFindObject(UMaterial::StaticClass(), Package, *FPackageName::GetShortName(FinishMaterialPackage)))
	{
		// Rebuild from scratch: rename the old one out of the way.
		Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	}
	UMaterial* Material = BuildFinishMaterial(Package);
	if (!Material)
	{
		return 1;
	}
	FAssetRegistryModule::AssetCreated(Material);
	Package->MarkPackageDirty();

	const FString Filename = FPackageName::LongPackageNameToFilename(FinishMaterialPackage, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Material, *Filename, SaveArgs))
	{
		UE_LOG(LogFloor101Assets, Error, TEXT("Could not save %s"), *Filename);
		return 1;
	}
	UE_LOG(LogFloor101Assets, Display, TEXT("Saved %s"), *Filename);
	return 0;
}
