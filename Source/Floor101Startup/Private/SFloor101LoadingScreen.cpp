#include "Floor101Startup.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

namespace
{
	// Cota Cero canvas, track and brass (Floor101Style): this module loads before the application module and cannot use it.
	FLinearColor StartupColor(const TCHAR* Hex) { return FLinearColor::FromSRGBColor(FColor::FromHex(Hex)); }

	// Indeterminate activity, never an invented loading percentage or a fixed delay.
	class SFloor101LoadingActivity final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SFloor101LoadingActivity) {} SLATE_END_ARGS()
		void Construct(const FArguments&) { ForceVolatile(true); }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(440.0, 4.0); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
			int32 Layer, const FWidgetStyle&, bool) const override
		{
			const double Width = Geometry.GetLocalSize().X;
			const double Height = Geometry.GetLocalSize().Y;
			const double Position = FMath::Fmod(FPlatformTime::Seconds(), 1.8) / 1.8 * (Width + 88.0) - 88.0;
			const double Left = FMath::Max(0.0, Position);
			const double Right = FMath::Min(Width, Position + 88.0);
			const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
			FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(), Brush, ESlateDrawEffect::None, StartupColor(TEXT("2D3439")));
			if (Right > Left)
			{
				FSlateDrawElement::MakeBox(Elements, Layer + 1, Geometry.ToPaintGeometry(FVector2D(Right - Left, Height), FSlateLayoutTransform(FVector2D(Left, 0.0))),
					Brush, ESlateDrawEffect::None, StartupColor(TEXT("D6A65F")));
			}
			return Layer + 1;
		}
	};

	class SFloor101LoadingScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SFloor101LoadingScreen) {} SLATE_END_ARGS()
		void Construct(const FArguments&)
		{
			Image = MakeShared<FSlateDynamicImageBrush>(FName(*FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Splash/Splash.png"))), FVector2D(720.0, 420.0));
			ChildSlot
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(StartupColor(TEXT("121518")))
				.Padding(0.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
					[SNew(SBox).WidthOverride(720.0f).HeightOverride(420.0f)
						[SNew(SOverlay)
							+ SOverlay::Slot()[SNew(SImage).Image(Image.Get())]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, 42.0f)
							[SNew(SBox).WidthOverride(440.0f).HeightOverride(4.0f)[SNew(SFloor101LoadingActivity)]]]]
				]
			];
		}
	private:
		TSharedPtr<FSlateDynamicImageBrush> Image;
	};
}

TSharedRef<SWidget> Floor101::Startup::MakeLoadingWidget() { return SNew(SFloor101LoadingScreen); }
