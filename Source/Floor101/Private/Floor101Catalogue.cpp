#include "Floor101Catalogue.h"
#include "Misc/ConfigCacheIni.h"

namespace Floor101::Catalogue
{
	namespace
	{
		FString SearchText(FString Text)
		{
			Text = Text.ToLower();
			const TCHAR* Accented = TEXT("áéíóúü");
			const TCHAR* Plain = TEXT("aeiouu");
			for (int32 I = 0; Accented[I]; ++I) { Text.ReplaceCharInline(Accented[I], Plain[I]); }
			return Text;
		}
	}
	bool Matches(const FString& Name, const FString& Query, bool bFavoritesOnly, bool bFavorite)
	{
		if (bFavoritesOnly && !bFavorite) { return false; }
		const FString Haystack = SearchText(Name);
		TArray<FString> Words;
		SearchText(Query).ParseIntoArrayWS(Words);
		for (const FString& Word : Words) { if (!Haystack.Contains(Word)) { return false; } }
		return true;
	}
	TArray<FString> Favorites()
	{
		TArray<FString> Result;
		if (GConfig) { GConfig->GetArray(TEXT("Floor101.Interface"), TEXT("CatalogueFavorites"), Result, GGameUserSettingsIni); }
		return Result;
	}
	void SaveFavorites(const TArray<FString>& Keys)
	{
		if (!GConfig) { return; }
		TArray<FString> Unique;
		for (const FString& Key : Keys) { if (!Key.IsEmpty()) { Unique.AddUnique(Key); } }
		Unique.Sort();
		GConfig->SetArray(TEXT("Floor101.Interface"), TEXT("CatalogueFavorites"), Unique, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
