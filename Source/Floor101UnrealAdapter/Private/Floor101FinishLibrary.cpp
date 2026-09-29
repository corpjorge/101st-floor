#include "Floor101FinishLibrary.h"

#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "F101/Scene/FinishTextures.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogFloor101Finishes, Log, All);

const TCHAR* UFloor101FinishLibrary::BaseMaterialPath = TEXT("/Game/Floor101/Materials/M_Floor101Finish.M_Floor101Finish");
const TCHAR* UFloor101FinishLibrary::GlassMaterialPath = TEXT("/Game/Floor101/Materials/M_Floor101Glass.M_Floor101Glass");
const FName UFloor101FinishLibrary::TextureParameter(TEXT("BaseTexture"));
const FName UFloor101FinishLibrary::TintParameter(TEXT("Tint"));
const FName UFloor101FinishLibrary::RoughnessParameter(TEXT("Roughness"));
const FName UFloor101FinishLibrary::RepeatUParameter(TEXT("RepeatU"));
const FName UFloor101FinishLibrary::RepeatVParameter(TEXT("RepeatV"));
const FName UFloor101FinishLibrary::OpacityParameter(TEXT("Opacity"));
const FLinearColor UFloor101FinishLibrary::ContextTint(0.42f, 0.44f, 0.48f);

namespace
{
	// Transient sRGB texture with the whole mip chain of the synthesised finish texture.
	UTexture2D* MakeTexture(const f101::scene::FinishTexture& Source, const FString& Name)
	{
		const f101::scene::TextureImage& Top = Source.mips.front();
		UTexture2D* Texture = UTexture2D::CreateTransient(Top.width, Top.height, PF_R8G8B8A8, FName(*(TEXT("T_Finish_") + Name)));
		if (!Texture)
		{
			return nullptr;
		}
		Texture->SRGB = true;
		Texture->AddressX = TA_Wrap;
		Texture->AddressY = TA_Wrap;
		Texture->LODGroup = TEXTUREGROUP_World;

		FTexturePlatformData* Data = Texture->GetPlatformData();
		for (int32 Level = 0; Level < static_cast<int32>(Source.mips.size()); ++Level)
		{
			const f101::scene::TextureImage& Image = Source.mips[Level];
			FTexture2DMipMap* Mip = nullptr;
			if (Level == 0)
			{
				Mip = &Data->Mips[0];
			}
			else
			{
				Mip = new FTexture2DMipMap();
				Mip->SizeX = Image.width;
				Mip->SizeY = Image.height;
				Mip->SizeZ = 1;
				Data->Mips.Add(Mip);
			}
			const int64 Bytes = static_cast<int64>(Image.rgba.size());
			void* Destination = Mip->BulkData.Lock(LOCK_READ_WRITE);
			if (Level > 0)
			{
				Destination = Mip->BulkData.Realloc(Bytes);
			}
			FMemory::Memcpy(Destination, Image.rgba.data(), Bytes);
			Mip->BulkData.Unlock();
		}
		Texture->UpdateResource();
		return Texture;
	}
}

UFloor101FinishLibrary* UFloor101FinishLibrary::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UFloor101FinishLibrary>() : nullptr;
}

UMaterialInterface* UFloor101FinishLibrary::GetBaseMaterial()
{
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, BaseMaterialPath);
		if (!BaseMaterial && !bReportedMissingBase)
		{
			bReportedMissingBase = true;
			UE_LOG(LogFloor101Finishes, Error, TEXT("F101: finish material %s is missing: run Scripts/GenerateAssets.ps1"), BaseMaterialPath);
		}
	}
	return BaseMaterial;
}

UMaterialInterface* UFloor101FinishLibrary::GetGlassMaterial()
{
	if (!GlassMaterial)
	{
		GlassMaterial = LoadObject<UMaterialInterface>(nullptr, GlassMaterialPath);
		if (!GlassMaterial && !bReportedMissingGlass)
		{
			bReportedMissingGlass = true;
			UE_LOG(LogFloor101Finishes, Error, TEXT("F101: finish material %s is missing: run Scripts/GenerateAssets.ps1"), GlassMaterialPath);
		}
	}
	return GlassMaterial;
}

UMaterialInterface* UFloor101FinishLibrary::GetFinishMaterial(const FString& FinishId, bool bContext)
{
	const FString Key = bContext ? FinishId + TEXT("#context") : FinishId;
	if (const TObjectPtr<UMaterialInstanceDynamic>* Cached = FinishMaterials.Find(Key))
	{
		return *Cached;
	}
	if (bContext)
	{
		// A dynamic instance cannot parent another one: same base material, same parameters, darker tint.
		UMaterialInstanceDynamic* Finish = Cast<UMaterialInstanceDynamic>(GetFinishMaterial(FinishId, false));
		if (!Finish)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Dimmed = UMaterialInstanceDynamic::Create(Finish->Parent, this);
		Dimmed->CopyParameterOverrides(Finish);
		Dimmed->SetVectorParameterValue(TintParameter, ContextTint);
		FinishMaterials.Add(Key, Dimmed);
		return Dimmed;
	}
	const f101::scene::FinishLook Look = f101::scene::buildFinishLook(TCHAR_TO_UTF8(*FinishId));
	if (Look.opacity < 1.0)
	{
		// See-through (glass): the translucent material, tinted with the finish colour.
		UMaterialInterface* Glass = GetGlassMaterial();
		if (!Glass)
		{
			return nullptr;
		}
		const std::vector<std::uint8_t>& Pixel = Look.texture.mips.back().rgba;
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Glass, this, FName(*(TEXT("MI_Finish_") + FinishId)));
		Material->SetVectorParameterValue(TintParameter, FLinearColor::FromSRGBColor(FColor(Pixel[0], Pixel[1], Pixel[2])));
		Material->SetScalarParameterValue(OpacityParameter, static_cast<float>(Look.opacity));
		Material->SetScalarParameterValue(RoughnessParameter, static_cast<float>(Look.roughness));
		FinishMaterials.Add(FinishId, Material);
		return Material;
	}
	UMaterialInterface* Base = GetBaseMaterial();
	if (!Base)
	{
		return nullptr;
	}
	UTexture2D* Texture = MakeTexture(Look.texture, FinishId);
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this, FName(*(TEXT("MI_Finish_") + FinishId)));
	if (Texture)
	{
		Material->SetTextureParameterValue(TextureParameter, Texture);
		FinishTextures.Add(FinishId, Texture);
	}
	Material->SetScalarParameterValue(RoughnessParameter, static_cast<float>(Look.roughness));
	Material->SetScalarParameterValue(RepeatUParameter, static_cast<float>(Look.texture.repeatWidthMm / 1000.0));
	Material->SetScalarParameterValue(RepeatVParameter, static_cast<float>(Look.texture.repeatHeightMm / 1000.0));
	FinishMaterials.Add(FinishId, Material);
	UE_LOG(LogFloor101Finishes, Verbose, TEXT("F101: finish %s ready (%dx%d, %d mips)"), *FinishId,
		Look.texture.mips.front().width, Look.texture.mips.front().height, static_cast<int32>(Look.texture.mips.size()));
	return Material;
}

UTexture2D* UFloor101FinishLibrary::FindFinishTexture(const FString& FinishId) const
{
	const TObjectPtr<UTexture2D>* Found = FinishTextures.Find(FinishId);
	return Found ? Found->Get() : nullptr;
}
