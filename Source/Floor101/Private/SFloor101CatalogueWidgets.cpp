#include "SFloor101Overlay.h"
#include "Floor101Catalogue.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101Names.h"
#include "F101/Assets/Library.h"
#include "Floor101Style.h"
#include "Floor101Ui.h"
#include "SFloor101Icon.h"
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
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	const Studio::FPalette& P = Studio::Palette();
	auto FavoritesOnly = [this, bModel]() { return bModel ? bFurnitureFavoritesOnly : bMaterialFavoritesOnly; };
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(SBox).HeightOverride(Studio::Size::Field)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SEditableTextBox).Style(&Studio::TextBox()).Font(Studio::Text(Studio::EText::Body)).Padding(FMargin(32.0f, 5.0f, 10.0f, 5.0f))
					.HintText(bModel ? LOCTEXT("FindFurniture", "Buscar muebles…") : LOCTEXT("FindMaterials", "Buscar materiales…"))
					.Text_Lambda([this, bModel]() { return FText::FromString(bModel ? FurnitureQuery : MaterialQuery); })
					.OnTextChanged_Lambda([this, bModel](const FText& Text) { (bModel ? FurnitureQuery : MaterialQuery) = Text.ToString(); })
					.OnTextCommitted_Lambda([](const FText&, ETextCommit::Type Type) { if (Type == ETextCommit::OnEnter) { FSlateApplication::Get().SetAllUserFocusToGameViewport(); } })
				]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(10.0f, 0.0f)
				[SNew(SBox).Visibility(EVisibility::HitTestInvisible)[Ui::Icon(EFloor101Icon::Search, 16.0f, FSlateColor(P.Text3))]]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f).HAlign(HAlign_Left)
		[
			Ui::Choice(LOCTEXT("FavoritesFilter", "Solo favoritos"), TAttribute<bool>::CreateLambda(FavoritesOnly),
				[this, bModel]() { bool& Only = bModel ? bFurnitureFavoritesOnly : bMaterialFavoritesOnly; Only = !Only; },
				LOCTEXT("FavoritesFilterTip", "Muestra solo lo marcado con la estrella"),
				SNew(SFloor101Icon).Size(16.0f)
				.Icon_Lambda([FavoritesOnly]() { return FavoritesOnly() ? EFloor101Icon::StarFilled : EFloor101Icon::Star; })
				.Color_Lambda([FavoritesOnly]() { return FavoritesOnly() ? FSlateColor(Floor101::Studio::Palette().Brass) : FSlateColor(Floor101::Studio::Palette().Text2); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			// Nothing matches: say so and how to get results back.
			SNew(SBorder).BorderImage(Studio::Box(FLinearColor::Transparent, 10.0f, P.EdgeSoft, 1.0f)).Padding(FMargin(16.0f, 18.0f))
			.Visibility_Lambda([this, bModel]() { return CatalogueHasResults(bModel) ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Ui::Icon(EFloor101Icon::Search, 24.0f, FSlateColor(P.Text3))]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[SNew(STextBlock).Text(LOCTEXT("NoResultsTitle", "Sin resultados")).Font(Studio::Text(Studio::EText::BodyStrong)).ColorAndOpacity(P.Text1)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("NoResults", "Cambia la búsqueda o desactiva Solo favoritos.")).Font(Studio::Text(Studio::EText::Help))
					.ColorAndOpacity(P.Text3).AutoWrapText(true).Justification(ETextJustify::Center)
				]
			]
		];
}

TSharedRef<SWidget> SFloor101Overlay::MakeCatalogueTile(const TSharedRef<SWidget>& Picture, const FText& Name,
	const FText& ToolTip, TFunction<bool()> IsActive, TFunction<FReply()> OnClicked, const FString& Key, bool bModel)
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	const Studio::FPalette& P = Studio::Palette();
	auto Favorite = [this, Key]() { return FavoriteKeys.Contains(Key); };
	return SNew(SBox).WidthOverride(146.0f)
		.Visibility_Lambda([this, Name, Key, bModel]()
		{
			return Floor101::Catalogue::Matches(Name.ToString() + TEXT(" ") + Key, bModel ? FurnitureQuery : MaterialQuery,
				bModel ? bFurnitureFavoritesOnly : bMaterialFavoritesOnly, FavoriteKeys.Contains(Key)) ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(Studio::Box(P.Raised, 10.0f)).Padding(0.0f)
				[
					SNew(SButton).ButtonStyle(&Studio::ButtonStyle(Studio::EButton::Row)).ContentPadding(6.0f).IsFocusable(false).ToolTip(Ui::Tooltip(ToolTip))
					.OnClicked_Lambda([OnClicked]() { return OnClicked(); })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).HeightOverride(100.0f)
							[SNew(SBorder).BorderImage(Studio::Box(P.Paper, 7.0f)).Padding(4.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)[Picture]]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 8.0f, 2.0f, 2.0f)
						[
							SNew(SBox).HeightOverride(36.0f).VAlign(VAlign_Top)
							[
								SNew(STextBlock).Text(Name).Font(Studio::Text(Studio::EText::Label)).AutoWrapText(true).Justification(ETextJustify::Center)
								.ColorAndOpacity_Lambda([IsActive]() { return FSlateColor(IsActive() ? Floor101::Studio::Palette().Text1 : Floor101::Studio::Palette().Text2); })
							]
						]
					]
				]
			]
			// The chosen item wears a brass ring and a check.
			+ SOverlay::Slot()
			[
				SNew(SBorder).Visibility(EVisibility::HitTestInvisible).Padding(0.0f)
				.BorderImage_Lambda([IsActive]()
				{
					const auto& Colors = Floor101::Studio::Palette();
					return IsActive() ? Floor101::Studio::Box(FLinearColor::Transparent, 10.0f, Colors.Brass, 2.0f) : Floor101::Studio::Box(FLinearColor::Transparent, 10.0f, Colors.Line, 1.0f);
				})
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(10.0f)
			[
				SNew(SBox).WidthOverride(20.0f).HeightOverride(20.0f).Visibility_Lambda([IsActive]() { return IsActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[SNew(SBorder).BorderImage(Studio::Box(P.Brass, 10.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.0f)[Ui::Icon(EFloor101Icon::Check, 14.0f, FSlateColor(P.OnBrass))]]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(10.0f)
			[
				SNew(SBox).WidthOverride(26.0f).HeightOverride(26.0f)
				[
					SNew(SBorder).BorderImage(Studio::Box(P.Glass, 7.0f)).Padding(0.0f)
					[
						SNew(SButton).ButtonStyle(&Studio::ButtonStyle(Studio::EButton::Row)).ContentPadding(0.0f).IsFocusable(false).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.ToolTip(Ui::Tooltip(TAttribute<FText>::CreateLambda([Favorite]() { return Favorite() ? LOCTEXT("RemoveFavorite", "Quitar de favoritos") : LOCTEXT("AddFavorite", "Añadir a favoritos"); })))
						.OnClicked_Lambda([this, Key]()
						{
							if (FavoriteKeys.Contains(Key)) { FavoriteKeys.Remove(Key); } else { FavoriteKeys.Add(Key); }
							Floor101::Catalogue::SaveFavorites(FavoriteKeys.Array());
							return FReply::Handled();
						})
						[
							SNew(SFloor101Icon).Size(16.0f)
							.Icon_Lambda([Favorite]() { return Favorite() ? EFloor101Icon::StarFilled : EFloor101Icon::Star; })
							.Color_Lambda([Favorite]() { return Favorite() ? FSlateColor(Floor101::Studio::Palette().Brass) : FSlateColor(Floor101::Studio::Palette().Text2); })
						]
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
