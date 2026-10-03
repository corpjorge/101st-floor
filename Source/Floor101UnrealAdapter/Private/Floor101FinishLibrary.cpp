#include "Floor101FinishLibrary.h"

#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "F101/Scene/FinishTextures.h"
#include "ImageCore.h"
#include "ImageUtils.h"
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
const FLinearColor UFloor101FinishLibrary::GhostTint(0.62f, 0.68f, 0.76f);
const float UFloor101FinishLibrary::GhostOpacity = 0.12f;

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

UMaterialInterface* UFloor101FinishLibrary::GetGhostMaterial()
{
	if (!GhostMaterial)
	{
		UMaterialInterface* Glass = GetGlassMaterial();
		if (!Glass)
		{
			return nullptr;
		}
		GhostMaterial = UMaterialInstanceDynamic::Create(Glass, this, TEXT("MI_Floor101Ghost"));
		GhostMaterial->SetVectorParameterValue(TintParameter, GhostTint);
		GhostMaterial->SetScalarParameterValue(OpacityParameter, GhostOpacity);
		GhostMaterial->SetScalarParameterValue(RoughnessParameter, 0.6f);
	}
	return GhostMaterial;
}

UTexture2D* UFloor101FinishLibrary::GetImageTexture(const FString& Path, bool bFlip, float RepeatUMeters, float RepeatVMeters,
	bool& bOutMissing)
{
	bOutMissing = false;
	const FString Key = Path.IsEmpty() ? FString(TEXT("#white")) : Path + (bFlip ? TEXT("#flip") : TEXT(""));
	if (const TObjectPtr<UTexture2D>* Cached = ImageTextures.Find(Key))
	{
		bOutMissing = !Path.IsEmpty() && MissingAssetImages.Contains(Path);
		return *Cached;
	}
	f101::scene::TextureImage Image;
	FImage Decoded;
	if (!Path.IsEmpty() && FImageUtils::LoadImage(*Path, Decoded) && Decoded.SizeX > 0 && Decoded.SizeY > 0)
	{
		Decoded.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		Image.width = static_cast<int>(Decoded.SizeX);
		Image.height = static_cast<int>(Decoded.SizeY);
		Image.rgba.resize(static_cast<size_t>(Image.width) * Image.height * 4);
		const uint8* Source = Decoded.RawData.GetData();
		for (size_t Pixel = 0; Pixel < static_cast<size_t>(Image.width) * Image.height; ++Pixel)
		{
			Image.rgba[Pixel * 4 + 0] = Source[Pixel * 4 + 2];
			Image.rgba[Pixel * 4 + 1] = Source[Pixel * 4 + 1];
			Image.rgba[Pixel * 4 + 2] = Source[Pixel * 4 + 0];
			Image.rgba[Pixel * 4 + 3] = 255;
		}
	}
	else
	{
		if (!Path.IsEmpty())
		{
			UE_LOG(LogFloor101Finishes, Warning, TEXT("F101: cannot read the image %s; its finish shows its colour alone"), *Path);
			MissingAssetImages.Add(Path);
			bOutMissing = true;
		}
		Image.width = Image.height = 4;
		Image.rgba.assign(4 * 4 * 4, 255);
	}
	const f101::scene::FinishTexture Texture = f101::scene::textureFromImage(Image, RepeatUMeters * 1000.0, RepeatVMeters * 1000.0, bFlip);
	UTexture2D* Made = MakeTexture(Texture, FString::Printf(TEXT("Asset_%d"), ImageTextures.Num())); // unique: never reused
	ImageTextures.Add(Key, Made);
	return Made;
}

void UFloor101FinishLibrary::ApplyAssetFinish(UMaterialInstanceDynamic* Material, const FString& FinishId,
	const FFloor101AssetFinish& Finish, bool bContext)
{
	bool bMissing = false;
	UTexture2D* Texture = GetImageTexture(Finish.ImagePath, Finish.bFlipImage, Finish.RepeatU, Finish.RepeatV, bMissing);
	if (Texture)
	{
		Material->SetTextureParameterValue(TextureParameter, Texture);
		if (!bContext)
		{
			FinishTextures.Add(FinishId, Texture);
		}
	}
	Material->SetVectorParameterValue(TintParameter, bContext ? Finish.Tint * ContextTint : Finish.Tint);
	Material->SetScalarParameterValue(RoughnessParameter, Finish.Roughness);
	Material->SetScalarParameterValue(RepeatUParameter, Finish.RepeatU);
	Material->SetScalarParameterValue(RepeatVParameter, Finish.RepeatV);
}

void UFloor101FinishLibrary::SetAssetFinish(const FString& FinishId, const FFloor101AssetFinish& Finish)
{
	if (const FFloor101AssetFinish* Current = AssetFinishes.Find(FinishId); Current && *Current == Finish)
	{
		return;
	}
	AssetFinishes.Add(FinishId, Finish);
	for (const bool bContext : {false, true})
	{
		const FString Key = bContext ? FinishId + TEXT("#context") : FinishId;
		if (const TObjectPtr<UMaterialInstanceDynamic>* Existing = FinishMaterials.Find(Key))
		{
			ApplyAssetFinish(*Existing, FinishId, Finish, bContext);
		}
	}
}

bool UFloor101FinishLibrary::IsAssetImageMissing(const FString& FinishId) const
{
	const FFloor101AssetFinish* Finish = AssetFinishes.Find(FinishId);
	return Finish && !Finish->ImagePath.IsEmpty() && MissingAssetImages.Contains(Finish->ImagePath);
}

UTexture2D* UFloor101FinishLibrary::GetAssetFinishImage(const FString& FinishId)
{
	const FFloor101AssetFinish* Finish = AssetFinishes.Find(FinishId);
	if (!Finish || Finish->ImagePath.IsEmpty())
	{
		return nullptr;
	}
	bool bMissing = false;
	UTexture2D* Texture = GetImageTexture(Finish->ImagePath, Finish->bFlipImage, Finish->RepeatU, Finish->RepeatV, bMissing);
	return bMissing ? nullptr : Texture;
}

UTexture2D* UFloor101FinishLibrary::GetPicture(const FString& Path)
{
	if (const TObjectPtr<UTexture2D>* Cached = Pictures.Find(Path))
	{
		return *Cached;
	}
	UTexture2D* Picture = FImageUtils::ImportFileAsTexture2D(Path);
	if (!Picture)
	{
		UE_LOG(LogFloor101Finishes, Warning, TEXT("F101: cannot read the picture %s"), *Path);
	}
	Pictures.Add(Path, Picture);
	return Picture;
}

UMaterialInterface* UFloor101FinishLibrary::GetFinishMaterial(const FString& FinishId, bool bContext)
{
	const FString Key = bContext ? FinishId + TEXT("#context") : FinishId;
	if (const TObjectPtr<UMaterialInstanceDynamic>* Cached = FinishMaterials.Find(Key))
	{
		return *Cached;
	}
	if (const FFloor101AssetFinish* Asset = AssetFinishes.Find(FinishId))
	{
		UMaterialInterface* Base = GetBaseMaterial();
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this); // ids have characters names cannot
		ApplyAssetFinish(Material, FinishId, *Asset, bContext);
		FinishMaterials.Add(Key, Material);
		return Material;
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
