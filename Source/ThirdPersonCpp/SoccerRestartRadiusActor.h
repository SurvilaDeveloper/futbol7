#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "SoccerRestartRadiusActor.generated.h"

class USceneComponent;
class UWidgetComponent;

enum class ESoccerRestartIndicatorShape : uint8
{
	Circle,
	Rectangle
};

UCLASS()
class THIRDPERSONCPP_API USoccerRestartRadiusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ConfigureCircle(
		const FLinearColor& InOutlineColor,
		float InThicknessPixels
	);

	void ConfigureRectangle(
		const FLinearColor& InOutlineColor,
		float InThicknessPixels
	);

protected:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override;

private:
	ESoccerRestartIndicatorShape IndicatorShape =
		ESoccerRestartIndicatorShape::Circle;

	FLinearColor OutlineColor =
		FLinearColor(0.62f, 0.56f, 0.18f, 0.34f);

	float OutlineThicknessPixels = 5.0f;

	int32 CircleSegmentCount = 160;
};

UCLASS()
class THIRDPERSONCPP_API ASoccerRestartRadiusActor : public AActor
{
	GENERATED_BODY()

public:
	ASoccerRestartRadiusActor();

	void ConfigureRadiusCircle(
		const FVector& CenterLocation,
		float RadiusCm,
		float ThicknessCm,
		const FLinearColor& CircleColor
	);

	void ConfigurePenaltyAreaRectangle(
		const FVector& RectangleCenterLocation,
		const FVector& DepthDirection,
		float DepthCm,
		float HalfWidthCm,
		float ThicknessCm,
		const FLinearColor& RectangleColor
	);

private:
	void InitializeWidget(
		const FLinearColor& OutlineColor,
		float ThicknessPixels,
		ESoccerRestartIndicatorShape Shape
	);

	UPROPERTY(VisibleAnywhere, Category = "Soccer|Restart Restriction Indicator")
		USceneComponent* RadiusRoot = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Soccer|Restart Restriction Indicator")
		UWidgetComponent* RadiusWidgetComponent = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction Indicator", meta = (ClampMin = "128", ClampMax = "2048"))
		int32 WidgetDrawSize = 1024;
};
