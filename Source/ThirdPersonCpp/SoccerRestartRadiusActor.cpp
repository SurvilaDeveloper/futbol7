#include "SoccerRestartRadiusActor.h"

#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Rendering/DrawElements.h"

void USoccerRestartRadiusWidget::ConfigureCircle(
	const FLinearColor& InOutlineColor,
	float InThicknessPixels
)
{
	IndicatorShape = ESoccerRestartIndicatorShape::Circle;
	OutlineColor = InOutlineColor;
	OutlineThicknessPixels = FMath::Max(1.0f, InThicknessPixels);

	InvalidateLayoutAndVolatility();
}

void USoccerRestartRadiusWidget::ConfigureRectangle(
	const FLinearColor& InOutlineColor,
	float InThicknessPixels
)
{
	IndicatorShape = ESoccerRestartIndicatorShape::Rectangle;
	OutlineColor = InOutlineColor;
	OutlineThicknessPixels = FMath::Max(1.0f, InThicknessPixels);

	InvalidateLayoutAndVolatility();
}

int32 USoccerRestartRadiusWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled
) const
{
	const int32 BaseLayerId = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled
	);

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float HalfThickness = OutlineThicknessPixels * 0.5f;
	const float Inset = HalfThickness + 2.0f;

	TArray<FVector2D> OutlinePoints;

	if (IndicatorShape == ESoccerRestartIndicatorShape::Rectangle)
	{
		const FVector2D Minimum(Inset, Inset);
		const FVector2D Maximum(
			FMath::Max(Inset + 1.0f, LocalSize.X - Inset),
			FMath::Max(Inset + 1.0f, LocalSize.Y - Inset)
		);

		OutlinePoints.Reserve(5);
		OutlinePoints.Add(FVector2D(Minimum.X, Minimum.Y));
		OutlinePoints.Add(FVector2D(Maximum.X, Minimum.Y));
		OutlinePoints.Add(FVector2D(Maximum.X, Maximum.Y));
		OutlinePoints.Add(FVector2D(Minimum.X, Maximum.Y));
		OutlinePoints.Add(FVector2D(Minimum.X, Minimum.Y));
	}
	else
	{
		const FVector2D Center = LocalSize * 0.5f;
		const float HalfMinimumSize =
			0.5f * FMath::Min(LocalSize.X, LocalSize.Y);

		const float RadiusPixels = FMath::Max(
			1.0f,
			HalfMinimumSize - HalfThickness - 2.0f
		);

		const int32 SegmentCount = FMath::Max(24, CircleSegmentCount);
		OutlinePoints.Reserve(SegmentCount + 1);

		for (int32 SegmentIndex = 0; SegmentIndex <= SegmentCount; ++SegmentIndex)
		{
			const float Alpha =
				static_cast<float>(SegmentIndex) /
				static_cast<float>(SegmentCount);

			const float AngleRadians = Alpha * 2.0f * PI;

			OutlinePoints.Add(
				Center +
				FVector2D(
					FMath::Cos(AngleRadians),
					FMath::Sin(AngleRadians)
				) * RadiusPixels
			);
		}
	}

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		BaseLayerId,
		AllottedGeometry.ToPaintGeometry(),
		OutlinePoints,
		ESlateDrawEffect::None,
		OutlineColor,
		true,
		OutlineThicknessPixels
	);

	return BaseLayerId + 1;
}

ASoccerRestartRadiusActor::ASoccerRestartRadiusActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RadiusRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RadiusRoot"));
	SetRootComponent(RadiusRoot);
	RadiusRoot->SetMobility(EComponentMobility::Movable);

	RadiusWidgetComponent =
		CreateDefaultSubobject<UWidgetComponent>(TEXT("RadiusWidget"));

	RadiusWidgetComponent->SetupAttachment(RadiusRoot);
	RadiusWidgetComponent->SetMobility(EComponentMobility::Movable);
	RadiusWidgetComponent->SetWidgetClass(
		USoccerRestartRadiusWidget::StaticClass()
	);
	RadiusWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	RadiusWidgetComponent->SetDrawSize(
		FVector2D(WidgetDrawSize, WidgetDrawSize)
	);
	RadiusWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	RadiusWidgetComponent->SetTwoSided(true);
	RadiusWidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);
	RadiusWidgetComponent->SetBackgroundColor(FLinearColor::Transparent);
	RadiusWidgetComponent->SetGeometryMode(EWidgetGeometryMode::Plane);
	RadiusWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RadiusWidgetComponent->SetGenerateOverlapEvents(false);
	RadiusWidgetComponent->SetCanEverAffectNavigation(false);
	RadiusWidgetComponent->SetCastShadow(false);
	RadiusWidgetComponent->SetTickWhenOffscreen(true);

	// A world-space widget normally stands vertically. Pitching it 90 degrees
	// lays the widget plane flat on the XY field plane.
	RadiusWidgetComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
}

void ASoccerRestartRadiusActor::InitializeWidget(
	const FLinearColor& InOutlineColor,
	float ThicknessPixels,
	ESoccerRestartIndicatorShape Shape
)
{
	RadiusWidgetComponent->InitWidget();

	USoccerRestartRadiusWidget* RadiusWidget =
		Cast<USoccerRestartRadiusWidget>(
			RadiusWidgetComponent->GetUserWidgetObject()
		);

	if (RadiusWidget != nullptr)
	{
		if (Shape == ESoccerRestartIndicatorShape::Rectangle)
		{
			RadiusWidget->ConfigureRectangle(
				InOutlineColor,
				ThicknessPixels
			);
		}
		else
		{
			RadiusWidget->ConfigureCircle(
				InOutlineColor,
				ThicknessPixels
			);
		}
	}

	RadiusWidgetComponent->SetVisibility(true, true);
	SetActorHiddenInGame(false);
}

void ASoccerRestartRadiusActor::ConfigureRadiusCircle(
	const FVector& CenterLocation,
	float RadiusCm,
	float ThicknessCm,
	const FLinearColor& CircleColor
)
{
	const float SafeRadiusCm = FMath::Max(1.0f, RadiusCm);
	const float SafeThicknessCm = FMath::Max(1.0f, ThicknessCm);
	const int32 SafeDrawSize = FMath::Clamp(WidgetDrawSize, 128, 2048);

	RadiusWidgetComponent->SetDrawSize(
		FVector2D(SafeDrawSize, SafeDrawSize)
	);

	SetActorLocationAndRotation(
		CenterLocation,
		FRotator::ZeroRotator,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	const float AvailableRadiusPixels =
		FMath::Max(1.0f, static_cast<float>(SafeDrawSize) * 0.5f - 2.0f);

	const float WorldUnitsPerPixel =
		(SafeRadiusCm + SafeThicknessCm * 0.5f) /
		AvailableRadiusPixels;

	RadiusWidgetComponent->SetRelativeScale3D(
		FVector(WorldUnitsPerPixel)
	);

	const float ThicknessPixels =
		FMath::Max(1.0f, SafeThicknessCm / WorldUnitsPerPixel);

	InitializeWidget(
		CircleColor,
		ThicknessPixels,
		ESoccerRestartIndicatorShape::Circle
	);
}

void ASoccerRestartRadiusActor::ConfigurePenaltyAreaRectangle(
	const FVector& RectangleCenterLocation,
	const FVector& DepthDirection,
	float DepthCm,
	float HalfWidthCm,
	float ThicknessCm,
	const FLinearColor& RectangleColor
)
{
	const float SafeDepthCm = FMath::Max(1.0f, DepthCm);
	const float SafeWidthCm = FMath::Max(2.0f, HalfWidthCm * 2.0f);
	const float SafeThicknessCm = FMath::Max(1.0f, ThicknessCm);
	const int32 SafeDrawSize = FMath::Clamp(WidgetDrawSize, 128, 2048);

	RadiusWidgetComponent->SetDrawSize(
		FVector2D(SafeDrawSize, SafeDrawSize)
	);

	FVector SafeDepthDirection = DepthDirection;
	SafeDepthDirection.Z = 0.0f;

	if (!SafeDepthDirection.Normalize())
	{
		SafeDepthDirection = FVector::ForwardVector;
	}

	SetActorLocationAndRotation(
		RectangleCenterLocation,
		SafeDepthDirection.Rotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	const float AvailablePixels =
		FMath::Max(1.0f, static_cast<float>(SafeDrawSize) - 4.0f);

	const float WorldUnitsPerPixelX =
		(SafeDepthCm + SafeThicknessCm) / AvailablePixels;
	const float WorldUnitsPerPixelY =
		(SafeWidthCm + SafeThicknessCm) / AvailablePixels;

	// WidgetComponent draws its plane on local Y/Z. After the 90-degree
	// pitch, local Z follows the rectangle depth and local Y its width.
	RadiusWidgetComponent->SetRelativeScale3D(
		FVector(1.0f, WorldUnitsPerPixelY, WorldUnitsPerPixelX)
	);

	const float AverageWorldUnitsPerPixel =
		FMath::Max(
			KINDA_SMALL_NUMBER,
			0.5f * (WorldUnitsPerPixelX + WorldUnitsPerPixelY)
		);

	const float ThicknessPixels =
		FMath::Max(1.0f, SafeThicknessCm / AverageWorldUnitsPerPixel);

	InitializeWidget(
		RectangleColor,
		ThicknessPixels,
		ESoccerRestartIndicatorShape::Rectangle
	);
}
