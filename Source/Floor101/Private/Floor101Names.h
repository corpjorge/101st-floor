#pragma once

#include "CoreMinimal.h"

#include "F101/Cad/Column.h"
#include "F101/Cad/Id.h"
#include "F101/Cad/Services.h"

#include <string_view>

namespace f101::cad
{
	struct Project;
}

// Names of the catalogues in the UI's language, shared by the overlay and the exports (the ids stay in English).
namespace Floor101
{
	// A finish (ADR-0012): the catalogue's name, an imported texture's own name with the project, "Original" for empty
	// (a piece's type finish, or a model's own materials); an unknown id as it is.
	FText FinishName(std::string_view Id, const f101::cad::Project* Project = nullptr);
	// A furniture type (ADR-0016): the catalogue's name, or an imported model's own name with the project.
	FText FurnitureName(std::string_view Type, const f101::cad::Project* Project = nullptr);
	// A kind of building-services run and a column's shape (ADR-0021).
	FText ServiceKindName(f101::cad::ServiceKind Kind);
	FText ColumnShapeName(f101::cad::ColumnShape Shape);
	// "Planta baja", "Planta 1"... unless the level has a name of its own (ADR-0011), and its short tag: "PB", "P1"...
	FText LevelName(const f101::cad::Project& Project, const f101::cad::LevelId& Level);
	FText LevelTag(const f101::cad::Project& Project, const f101::cad::LevelId& Level);
}
