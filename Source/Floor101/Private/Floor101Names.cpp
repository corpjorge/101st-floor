#include "Floor101Names.h"

#include "F101/Cad/Asset.h"
#include "F101/Cad/Project.h"

#include <optional>
#include <string>

// The keys these texts had while they lived in the overlay.
#define LOCTEXT_NAMESPACE "Floor101Overlay"

namespace Floor101
{
	namespace
	{
		// The name of an imported asset of the project, if `Ref` is one ("asset:<uuid>", ADR-0017).
		TOptional<FText> AssetName(std::string_view Ref, const f101::cad::Project* Project)
		{
			const std::optional<f101::cad::AssetId> Id = f101::cad::parseAssetRef(Ref);
			const f101::cad::Asset* Asset = Id && Project ? Project->findAsset(*Id) : nullptr;
			if (!Id)
			{
				return {};
			}
			return Asset ? FText::FromString(UTF8_TO_TCHAR(Asset->name.c_str())) : LOCTEXT("MissingAsset", "(asset que falta)");
		}
	}

	// Display name of a finish (ADR-0012); the catalogue ids stay in English. Imported textures show their name; empty is
	// the original look (a piece's type finish, or a model's own materials).
	FText FinishName(std::string_view Id, const f101::cad::Project* Project)
	{
		if (Id.empty())
		{
			return LOCTEXT("FinishOriginal", "Original");
		}
		if (const TOptional<FText> Imported = AssetName(Id, Project))
		{
			return *Imported;
		}
		static const TMap<FString, FText> Names{
			{TEXT("plaster-white"), LOCTEXT("FinishPlaster", "Enlucido blanco")},
			{TEXT("paint-sand"), LOCTEXT("FinishSand", "Pintura arena")},
			{TEXT("paint-sage"), LOCTEXT("FinishSage", "Pintura verde salvia")},
			{TEXT("paint-slate"), LOCTEXT("FinishSlate", "Pintura gris azulado")},
			{TEXT("brick-red"), LOCTEXT("FinishBrick", "Ladrillo visto")},
			{TEXT("concrete"), LOCTEXT("FinishConcrete", "Hormigón visto")},
			{TEXT("tile-white"), LOCTEXT("FinishTileWhite", "Azulejo blanco")},
			{TEXT("screed-light"), LOCTEXT("FinishScreed", "Solado gris claro")},
			{TEXT("tile-grey"), LOCTEXT("FinishTileGrey", "Baldosa gris")},
			{TEXT("wood-oak"), LOCTEXT("FinishOak", "Madera de roble")}, // floorboards and doors alike
			{TEXT("wood-walnut"), LOCTEXT("FinishWalnut", "Madera de nogal")},
			{TEXT("lacquer-white"), LOCTEXT("FinishLacquer", "Lacado blanco")},
			{TEXT("aluminium-anthracite"), LOCTEXT("FinishAluminium", "Aluminio antracita")},
			{TEXT("glass-clear"), LOCTEXT("FinishGlass", "Vidrio")},
			{TEXT("fabric-grey"), LOCTEXT("FinishFabricGrey", "Tela gris")},
			{TEXT("fabric-sand"), LOCTEXT("FinishFabricSand", "Tela arena")},
			{TEXT("linen-white"), LOCTEXT("FinishLinen", "Lino blanco")},
			{TEXT("ceramic-white"), LOCTEXT("FinishCeramic", "Cerámica blanca")},
			{TEXT("stone-dark"), LOCTEXT("FinishStone", "Piedra oscura")},
			{TEXT("steel-brushed"), LOCTEXT("FinishSteel", "Acero cepillado")},
			{TEXT("glass-black"), LOCTEXT("FinishGlassBlack", "Vidrio negro")},
			// MVP 11: paints.
			{TEXT("paint-white-warm"), LOCTEXT("FinishWhiteWarm", "Blanco roto")},
			{TEXT("paint-ivory"), LOCTEXT("FinishIvory", "Marfil")},
			{TEXT("paint-cream"), LOCTEXT("FinishCream", "Crema")},
			{TEXT("paint-beige"), LOCTEXT("FinishBeige", "Beige")},
			{TEXT("paint-greige"), LOCTEXT("FinishGreige", "Greige")},
			{TEXT("paint-pearl"), LOCTEXT("FinishPearl", "Gris perla")},
			{TEXT("paint-taupe"), LOCTEXT("FinishTaupe", "Gris topo")},
			{TEXT("paint-charcoal"), LOCTEXT("FinishCharcoal", "Gris antracita")},
			{TEXT("paint-black"), LOCTEXT("FinishPaintBlack", "Negro")},
			{TEXT("paint-mint"), LOCTEXT("FinishMint", "Verde menta")},
			{TEXT("paint-olive"), LOCTEXT("FinishOlive", "Verde oliva")},
			{TEXT("paint-forest"), LOCTEXT("FinishForest", "Verde bosque")},
			{TEXT("paint-sky"), LOCTEXT("FinishSky", "Azul cielo")},
			{TEXT("paint-denim"), LOCTEXT("FinishDenim", "Azul vaquero")},
			{TEXT("paint-navy"), LOCTEXT("FinishNavy", "Azul marino")},
			{TEXT("paint-teal"), LOCTEXT("FinishTeal", "Azul petróleo")},
			{TEXT("paint-lavender"), LOCTEXT("FinishLavender", "Lavanda")},
			{TEXT("paint-blush"), LOCTEXT("FinishBlush", "Rosa palo")},
			{TEXT("paint-terracotta"), LOCTEXT("FinishPaintTerracotta", "Terracota")},
			{TEXT("paint-mustard"), LOCTEXT("FinishMustard", "Amarillo mostaza")},
			{TEXT("paint-ochre"), LOCTEXT("FinishOchre", "Ocre")},
			{TEXT("paint-chocolate"), LOCTEXT("FinishChocolate", "Chocolate")},
			// Tiles.
			{TEXT("tile-metro-white"), LOCTEXT("FinishMetroWhite", "Azulejo metro blanco")},
			{TEXT("tile-metro-green"), LOCTEXT("FinishMetroGreen", "Azulejo metro verde")},
			{TEXT("tile-metro-black"), LOCTEXT("FinishMetroBlack", "Azulejo metro negro")},
			{TEXT("tile-porcelain-beige"), LOCTEXT("FinishPorcelainBeige", "Porcelánico beige 60×60")},
			{TEXT("tile-porcelain-anthracite"), LOCTEXT("FinishPorcelainAnthracite", "Porcelánico antracita 60×60")},
			{TEXT("tile-porcelain-white"), LOCTEXT("FinishPorcelainWhite", "Porcelánico blanco 120×60")},
			{TEXT("tile-terracotta"), LOCTEXT("FinishTileTerracotta", "Barro cocido 30×30")},
			{TEXT("tile-checker-bw"), LOCTEXT("FinishCheckerBw", "Ajedrezado blanco y negro")},
			{TEXT("tile-checker-terracotta"), LOCTEXT("FinishCheckerTerracotta", "Ajedrezado crema y barro")},
			// Wood.
			{TEXT("laminate-oak-light"), LOCTEXT("FinishLaminateOakLight", "Laminado roble claro")},
			{TEXT("laminate-oak-natural"), LOCTEXT("FinishLaminateOakNatural", "Laminado roble natural")},
			{TEXT("laminate-oak-grey"), LOCTEXT("FinishLaminateOakGrey", "Laminado roble gris")},
			{TEXT("laminate-oak-white"), LOCTEXT("FinishLaminateOakWhite", "Laminado roble blanqueado")},
			{TEXT("laminate-walnut"), LOCTEXT("FinishLaminateWalnut", "Laminado nogal")},
			{TEXT("laminate-wenge"), LOCTEXT("FinishLaminateWenge", "Laminado wengué")},
			{TEXT("parquet-herringbone-oak"), LOCTEXT("FinishHerringboneOak", "Parquet espiga roble")},
			{TEXT("parquet-herringbone-walnut"), LOCTEXT("FinishHerringboneWalnut", "Parquet espiga nogal")},
			{TEXT("parquet-herringbone-grey"), LOCTEXT("FinishHerringboneGrey", "Parquet espiga gris")},
			{TEXT("boards-pine"), LOCTEXT("FinishBoardsPine", "Tarima de pino")},
			{TEXT("panel-oak"), LOCTEXT("FinishPanelOak", "Friso de roble")},
			// Stone, concrete, brick.
			{TEXT("brick-white"), LOCTEXT("FinishBrickWhite", "Ladrillo blanco")},
			{TEXT("concrete-polished"), LOCTEXT("FinishConcretePolished", "Hormigón pulido")},
			{TEXT("microcement-grey"), LOCTEXT("FinishMicrocementGrey", "Microcemento gris")},
			{TEXT("microcement-white"), LOCTEXT("FinishMicrocementWhite", "Microcemento blanco")},
			{TEXT("microcement-sand"), LOCTEXT("FinishMicrocementSand", "Microcemento arena")},
			// Carpet.
			{TEXT("carpet-grey"), LOCTEXT("FinishCarpetGrey", "Moqueta gris")},
			{TEXT("carpet-beige"), LOCTEXT("FinishCarpetBeige", "Moqueta beige")},
			{TEXT("carpet-blue"), LOCTEXT("FinishCarpetBlue", "Moqueta azul")},
			{TEXT("carpet-green"), LOCTEXT("FinishCarpetGreen", "Moqueta verde")},
			// Joinery and furniture.
			{TEXT("lacquer-black"), LOCTEXT("FinishLacquerBlack", "Lacado negro")},
			{TEXT("lacquer-grey"), LOCTEXT("FinishLacquerGrey", "Lacado gris")},
			{TEXT("brass"), LOCTEXT("FinishBrass", "Latón")},
			{TEXT("fabric-blue"), LOCTEXT("FinishFabricBlue", "Tela azul")},
			{TEXT("fabric-green"), LOCTEXT("FinishFabricGreen", "Tela verde")},
			{TEXT("fabric-terracotta"), LOCTEXT("FinishFabricTerracotta", "Tela teja")},
			{TEXT("leather-brown"), LOCTEXT("FinishLeatherBrown", "Cuero marrón")},
			{TEXT("leather-black"), LOCTEXT("FinishLeatherBlack", "Cuero negro")},
			// Building services (ADR-0021): the colour of each kind of run.
			{TEXT("service-electrical"), LOCTEXT("FinishServiceElectrical", "Tubo eléctrico verde")},
			{TEXT("service-water"), LOCTEXT("FinishServiceWater", "Tubo de agua blanco")},
			{TEXT("service-sewage"), LOCTEXT("FinishServiceSewage", "Tubo de saneamiento amarillo")},
			{TEXT("service-gas"), LOCTEXT("FinishServiceGas", "Tubo de gas dorado")},
		};
		const FString Key = UTF8_TO_TCHAR(std::string(Id).c_str());
		const FText* Name = Names.Find(Key);
		return Name ? *Name : FText::FromString(Key);
	}

	// Display name of a furniture type (ADR-0016); the catalogue ids stay in English. Imported models show their name.
	FText FurnitureName(std::string_view Type, const f101::cad::Project* Project)
	{
		if (const TOptional<FText> Imported = AssetName(Type, Project))
		{
			return *Imported;
		}
		static const TMap<FString, FText> Names{
			{TEXT("bed-double"), LOCTEXT("BedDouble", "Cama doble")},
			{TEXT("bed-single"), LOCTEXT("BedSingle", "Cama individual")},
			{TEXT("sofa"), LOCTEXT("Sofa", "Sofá")},
			{TEXT("table"), LOCTEXT("Table", "Mesa")},
			{TEXT("chair"), LOCTEXT("Chair", "Silla")},
			{TEXT("wardrobe"), LOCTEXT("Wardrobe", "Armario")},
			{TEXT("kitchen-counter"), LOCTEXT("Kitchen", "Cocina")},
			{TEXT("fridge"), LOCTEXT("Fridge", "Frigorífico")},
			{TEXT("toilet"), LOCTEXT("Toilet", "Inodoro")},
			{TEXT("washbasin"), LOCTEXT("Washbasin", "Lavabo")},
			{TEXT("bathtub"), LOCTEXT("Bathtub", "Bañera")},
			{TEXT("shower-tray"), LOCTEXT("Shower", "Plato de ducha")},
		};
		const FString Key = UTF8_TO_TCHAR(std::string(Type).c_str());
		const FText* Name = Names.Find(Key);
		return Name ? *Name : FText::FromString(Key);
	}

	FText ServiceKindName(f101::cad::ServiceKind Kind)
	{
		switch (Kind)
		{
		case f101::cad::ServiceKind::Water: return LOCTEXT("ServiceWater", "Agua");
		case f101::cad::ServiceKind::Sewage: return LOCTEXT("ServiceSewage", "Aguas negras");
		case f101::cad::ServiceKind::Gas: return LOCTEXT("ServiceGas", "Gas");
		default: return LOCTEXT("ServiceElectrical", "Electricidad");
		}
	}

	FText ColumnShapeName(f101::cad::ColumnShape Shape)
	{
		return Shape == f101::cad::ColumnShape::Round ? LOCTEXT("ColumnRound", "Redondo") : LOCTEXT("ColumnRectangle", "Rectangular");
	}
}

#undef LOCTEXT_NAMESPACE
