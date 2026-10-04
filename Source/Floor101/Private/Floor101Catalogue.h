#pragma once
#include "CoreMinimal.h"

namespace Floor101::Catalogue
{
	bool Matches(const FString& Name, const FString& Query, bool bFavoritesOnly, bool bFavorite);
	TArray<FString> Favorites();
	void SaveFavorites(const TArray<FString>& Keys);
}
