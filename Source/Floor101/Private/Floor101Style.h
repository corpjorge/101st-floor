#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

namespace Floor101::Studio
{
	FLinearColor Color(const TCHAR* Hex);
	FSlateFontInfo Font(int32 Size, bool bBold = false);
	const FButtonStyle& Button();
	const FComboButtonStyle& ComboButton();
	const FComboBoxStyle& ComboBox();
	const FEditableTextBoxStyle& TextBox();
}
