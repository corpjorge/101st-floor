#include "SFloor101Overlay.h"
#include "Floor101Catalogue.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101Names.h"
#include "F101/Assets/Library.h"
#include "Floor101Style.h"
#include "SFloor101FavoriteIcon.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Floor101Catalogue"

bool SFloor101Overlay::CatalogueHasResults(bool bModel) const
{
	const FString& Query = bModel ? FurnitureQuery : MaterialQuery;
	const bool Only = bModel ? bFurnitureFavoritesOnly : bMaterialFavoritesOnly;
	auto Match = [this, &Query, Only](const FString& Name, const FString& Key)
	{
		return Floor101::Catalogue::Matches(Name + TEXT(" ") + Key, Query, Only, FavoriteKeys.Contains(Key));
	};
	const auto& Library = UFloor101DocumentSubsystem::GetLibrary();
	const auto Target = bPaintFloors ? f101::cad::Surface::Floor : f101::cad::Surface::Wall;
	if (bModel)
	{
		for (const auto& Type : f101::cad::furnitureCatalogue())
		{
			if ((Only || !Query.IsEmpty() || Type.category == FurnitureTab) && Match(Floor101::FurnitureName(Type.id).ToString(), FString(TEXT("furniture:")) + UTF8_TO_TCHAR(std::string(Type.id).c_str()))) { return true; }
		}
		for (const auto& Item : Library.furniture)
		{
			if ((Only || !Query.IsEmpty() || Item.category == FurnitureTab) && Match(UTF8_TO_TCHAR(Item.name.c_str()), FString(TEXT("library-model:")) + UTF8_TO_TCHAR(Item.id.c_str()))) { return true; }
		}
	}
	else
	{
		for (const auto& Finish : f101::cad::finishCatalogue())
		{
			if (Finish.allows(Target) && Match(Floor101::FinishName(Finish.id).ToString(), FString(TEXT("finish:")) + UTF8_TO_TCHAR(std::string(Finish.id).c_str()))) { return true; }
		}
		for (const auto& Item : Library.textures)
		{
			if ((Item.texture.surfaces & static_cast<std::uint8_t>(Target)) != 0 && Match(UTF8_TO_TCHAR(Item.name.c_str()), FString(TEXT("library-texture:")) + UTF8_TO_TCHAR(Item.id.c_str()))) { return true; }
		}
	}
	if (const auto* Project = CurrentProject())
	{
		for (const auto& Asset : Project->assets)
		{
			if (Asset.library.empty() && (Asset.kind == f101::cad::AssetKind::Model) == bModel && Match(UTF8_TO_TCHAR(Asset.name.c_str()), FString(TEXT("asset:")) + UTF8_TO_TCHAR(f101::cad::assetRef(Asset.id).c_str()))) { return true; }
		}
	}
	return false;
}

TSharedRef<SWidget> SFloor101Overlay::MakeCatalogueFilters(bool bModel)
{
	using namespace Floor101::Studio;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
		[SNew(SEditableTextBox).Style(&TextBox()).Font(Floor101::Studio::Font(12))
			.HintText(bModel ? LOCTEXT("FindFurniture", "Buscar muebles…") : LOCTEXT("FindMaterials", "Buscar materiales…"))
			.Text_Lambda([this, bModel]() { return FText::FromString(bModel ? FurnitureQuery : MaterialQuery); })
			.OnTextChanged_Lambda([this, bModel](const FText& Text) { (bModel ? FurnitureQuery : MaterialQuery) = Text.ToString(); })
			.OnTextCommitted_Lambda([](const FText&, ETextCommit::Type Type) { if (Type == ETextCommit::OnEnter) { FSlateApplication::Get().SetAllUserFocusToGameViewport(); } })]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[SNew(SButton).ButtonStyle(&Floor101::Studio::Button()).ContentPadding(FMargin(8.0f, 6.0f)).IsFocusable(false)
			.OnClicked_Lambda([this, bModel]() { bool& Only = bModel ? bFurnitureFavoritesOnly : bMaterialFavoritesOnly; Only = !Only; return FReply::Handled(); })
			[SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[SNew(SFloor101FavoriteIcon).Active_Lambda([this, bModel]() { return bModel ? bFurnitureFavoritesOnly : bMaterialFavoritesOnly; })]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[SNew(STextBlock).Font(Floor101::Studio::Font(12)).ColorAndOpacity(Floor101::Studio::Color(TEXT("EEF0EC"))).Text(LOCTEXT("FavoritesFilter", "Solo favoritos"))]]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[SNew(STextBlock).Font(Floor101::Studio::Font(12)).ColorAndOpacity(Floor101::Studio::Color(TEXT("B3BAB9"))).AutoWrapText(true)
			.Visibility_Lambda([this, bModel]() { return CatalogueHasResults(bModel) ? EVisibility::Collapsed : EVisibility::Visible; })
			.Text(LOCTEXT("NoResults", "Sin resultados. Cambia la búsqueda o desactiva Solo favoritos."))];
}

TSharedRef<SWidget> SFloor101Overlay::MakeCatalogueTile(const TSharedRef<SWidget>& Picture, const FText& Name,
	const FText& ToolTip, TFunction<bool()> IsActive, TFunction<FReply()> OnClicked, const FString& Key, bool bModel)
{
	using namespace Floor101::Studio;
	return SNew(SBox).WidthOverride(144.0f)
		.Visibility_Lambda([this, Name, Key, bModel]()
		{
			return Floor101::Catalogue::Matches(Name.ToString() + TEXT(" ") + Key, bModel ? FurnitureQuery : MaterialQuery,
				bModel ? bFurnitureFavoritesOnly : bMaterialFavoritesOnly, FavoriteKeys.Contains(Key)) ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).Padding(1.0f)
			.BorderBackgroundColor_Lambda([IsActive]() { return IsActive() ? Floor101::Studio::Color(TEXT("D6AD78")) : Floor101::Studio::Color(TEXT("30373D")); })
			[SNew(SOverlay)
				+ SOverlay::Slot()
				[SNew(SButton).ButtonStyle(&Floor101::Studio::Button()).ContentPadding(6.0f).IsFocusable(false).ToolTipText(ToolTip)
					.OnClicked_Lambda([OnClicked]() { return OnClicked(); })
					[SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[SNew(SBox).WidthOverride(120.0f).HeightOverride(100.0f)
							[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Floor101::Studio::Color(TEXT("DAD8D0"))).Padding(4.0f)[Picture]]]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
						[SNew(SBox).HeightOverride(50.0f)
							[SNew(STextBlock).Font(Floor101::Studio::Font(11)).Text(Name).ColorAndOpacity(Floor101::Studio::Color(TEXT("EEF0EC"))).AutoWrapText(true).Justification(ETextJustify::Center)]]]]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top)
				[SNew(SButton).ButtonStyle(&Floor101::Studio::Button()).ContentPadding(FMargin(4.0f, 0.0f)).IsFocusable(false)
					.ToolTipText_Lambda([this, Key]() { return FavoriteKeys.Contains(Key) ? LOCTEXT("RemoveFavorite", "Quitar de favoritos") : LOCTEXT("AddFavorite", "Añadir a favoritos"); })
					.OnClicked_Lambda([this, Key]()
					{
						if (FavoriteKeys.Contains(Key)) { FavoriteKeys.Remove(Key); } else { FavoriteKeys.Add(Key); }
						Floor101::Catalogue::SaveFavorites(FavoriteKeys.Array());
						return FReply::Handled();
					})
					[SNew(SFloor101FavoriteIcon).Active_Lambda([this, Key]() { return FavoriteKeys.Contains(Key); })]]]];
}

#undef LOCTEXT_NAMESPACE
