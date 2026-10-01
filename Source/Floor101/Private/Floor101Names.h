#pragma once

#include "CoreMinimal.h"

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
}
