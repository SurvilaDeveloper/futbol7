//SoccerOffsideLineActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerOffsideLineActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class THIRDPERSONCPP_API ASoccerOffsideLineActor : public AActor
{
	GENERATED_BODY()

public:
	ASoccerOffsideLineActor();

	void ConfigureLine(
		const FVector& CenterLocation,
		const FVector& SizeCm,
		const FLinearColor& LineColor
	);

private:
	UPROPERTY(VisibleAnywhere, Category = "Soccer|Offside Line")
		USceneComponent* LineRoot = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Soccer|Offside Line")
		UStaticMeshComponent* LineMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside Line")
		UMaterialInterface* BaseLineMaterial = nullptr;

	UPROPERTY(Transient)
		UMaterialInstanceDynamic* DynamicLineMaterial = nullptr;
};
