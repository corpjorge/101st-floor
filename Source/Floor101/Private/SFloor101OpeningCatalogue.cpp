#include "SFloor101Overlay.h"

#include "Floor101Catalogue.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101Style.h"
#include "Floor101Ui.h"
#include "SFloor101OpeningThumbnail.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Floor101OpeningCatalogue"

TSharedRef<SWidget> SFloor101Overlay::MakeOpeningCatalogue(bool bSelection)
{
	namespace Studio = Floor101::Studio;
	namespace Ui = Floor101::Ui;
	using namespace f101::cad;
	auto Current = [this, bSelection]() -> std::optional<Opening>
	{
		if (bSelection)
		{
			const auto Selected = GetSelectedOpening();
			return Selected ? std::optional<Opening>(Selected->opening) : std::nullopt;
		}
		const auto* S = Document.Get();
		if (!S) { return std::nullopt; }
		const auto& Editor = S->GetEditController();
		if (Editor.mode() == EditMode::Door) { return Editor.settings().door; }
		if (Editor.mode() == EditMode::Window) { return Editor.settings().window; }
		return std::nullopt;
	};
	auto Matches = [this, Current](const OpeningFamily& Family)
	{
		const auto Opening = Current();
		return Opening && Opening->kind == Family.kind &&
			(OpeningOperationFilter < 0 || OpeningOperationFilter == static_cast<int32>(Family.operation)) &&
			Floor101::Catalogue::Matches(UTF8_TO_TCHAR(std::string(Family.name).c_str()), OpeningQuery, false, false);
	};
	const auto& P = Studio::Palette();
	const auto FilterName = [](int32 Filter) -> FText
	{
		switch (Filter)
		{
		case 0: return LOCTEXT("Hinged", "Una hoja");
		case 1: return LOCTEXT("Double", "Dos hojas");
		case 2: return LOCTEXT("Sliding", "Correderas");
		case 3: return LOCTEXT("Fixed", "Fijas");
		default: return LOCTEXT("All", "Todas las aperturas");
		}
	};
	TSharedRef<SWrapBox> Cards = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(Studio::Size::Gutter));
	for (const auto& Family : openingCatalogue())
	{
		const std::string Id(Family.id);
		auto Active = [Current, Id]() { const auto Opening = Current(); return Opening && Opening->family == Id; };
		Cards->AddSlot()
		[
			SNew(SBox).WidthOverride(Studio::Size::OpeningCard)
			.Visibility_Lambda([Matches, Family]() { return Matches(Family) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SBorder).Padding(Studio::Size::RadiusKey)
				.BorderImage_Lambda([Active]()
				{
					const auto& Colors = Studio::Palette();
					return Studio::Box(Colors.Raised, Studio::Size::RadiusControl, Active() ? Colors.Brass : Colors.Line, Active() ? 2.0f : 1.0f);
				})
				[
					SNew(SButton).ButtonStyle(&Studio::ButtonStyle(Studio::EButton::Card)).IsFocusable(false)
					.ToolTip(Ui::Tooltip(FText::FromString(UTF8_TO_TCHAR(std::string(Family.name).c_str()))))
					.OnClicked_Lambda([this, bSelection, Id, Family]()
					{
						return Run([bSelection, Id, Family](EditController& Editor)
						{
							return bSelection ? Editor.setOpeningFamily(Id) : Editor.setNewOpeningFamily(Family.kind, Id);
						});
					})
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[SNew(SBox).HeightOverride(Studio::Size::OpeningPicture)
							[SNew(SBorder).BorderImage(Studio::Box(P.Paper, Studio::Size::RadiusKey)).Padding(0.0f)
								[SNew(SFloor101OpeningThumbnail).Family(Id)]]]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).HeightOverride(Studio::Size::OpeningCardLabel)
							[SNew(STextBlock).Text(FText::FromString(UTF8_TO_TCHAR(std::string(Family.name).c_str())))
							.Font(Studio::Text(Studio::EText::Label)).ColorAndOpacity(P.Text1).AutoWrapText(true).Justification(ETextJustify::Center)]
						]
					]
				]
			]
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Studio::Size::Gutter)
		[
			SNew(SEditableTextBox).Style(&Studio::TextBox()).Font(Studio::Text(Studio::EText::Body))
			.HintText(LOCTEXT("Search", "Buscar diseño…"))
			.Text_Lambda([this]() { return FText::FromString(OpeningQuery); })
			.OnTextChanged_Lambda([this](const FText& Text) { OpeningQuery = Text.ToString(); })
			.OnTextCommitted_Lambda([](const FText&, ETextCommit::Type Type)
			{
				if (Type == ETextCommit::OnEnter) { FSlateApplication::Get().SetAllUserFocusToGameViewport(); }
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Studio::Size::Gutter)
		[
			SNew(SComboButton).ComboButtonStyle(&Studio::ComboButton()).IsFocusable(false)
			.ButtonContent()[SNew(STextBlock).Text_Lambda([this, FilterName]() { return FilterName(OpeningOperationFilter); })
				.Font(Studio::Text(Studio::EText::Body)).ColorAndOpacity(P.Text1)]
			.OnGetMenuContent_Lambda([this, FilterName, Current]() -> TSharedRef<SWidget>
			{
				TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
				for (int32 Filter = -1; Filter <= 3; ++Filter)
				{
					const auto Opening = Current();
					if (Filter == 3 && Opening && Opening->kind == OpeningKind::Door) { continue; }
					Menu->AddSlot().AutoHeight()[Ui::Button(Studio::EButton::Row, FilterName(Filter), [this, Filter]()
					{
						OpeningOperationFilter = Filter;
						FSlateApplication::Get().DismissAllMenus();
					})];
				}
				return Menu;
			})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(LOCTEXT("Empty", "Sin resultados. Cambia la búsqueda o el filtro de apertura."))
			.Font(Studio::Text(Studio::EText::Help)).ColorAndOpacity(P.Text3).AutoWrapText(true)
			.Visibility_Lambda([Matches]()
			{
				for (const auto& Family : openingCatalogue()) { if (Matches(Family)) { return EVisibility::Collapsed; } }
				return EVisibility::Visible;
			})
		]
		+ SVerticalBox::Slot().AutoHeight()[Cards];
}

#undef LOCTEXT_NAMESPACE
