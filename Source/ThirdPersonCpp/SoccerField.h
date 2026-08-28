//SoccerField.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerField.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class ANavMeshBoundsVolume;

UCLASS()
class THIRDPERSONCPP_API ASoccerField : public AActor
{
	GENERATED_BODY()

public:
	ASoccerField();

	// Stage 10B: authoritative bridge between the local dimensions declared
	// in SoccerFieldDimensions and world-space football geometry. Gameplay
	// systems should query these helpers instead of rebuilding world
	// coordinates from hardcoded X/Y values.
	FVector PitchLocalToWorld(const FVector& LocalLocation) const;
	FVector WorldToPitchLocal(const FVector& WorldLocation) const;

	FVector GetPitchCenterWorldLocation(float LocalZ = 0.0f) const;
	FVector GetGoalCenterWorldLocation(float GoalLineSign, float LocalZ = 0.0f) const;
	FVector GetPenaltySpotWorldLocation(float GoalLineSign, float LocalZ = 0.0f) const;
	FVector GetCornerWorldLocation(
		float GoalLineSign,
		float TouchlineSign,
		float InsetCm = 0.0f,
		float LocalZ = 0.0f
	) const;

	bool IsWorldLocationInsidePitch(const FVector& WorldLocation, float InsetCm = 0.0f) const;
	bool IsWorldLocationInsideGoalArea(
		const FVector& WorldLocation,
		float GoalLineSign,
		float MarginCm = 0.0f
	) const;
	bool IsWorldLocationInsidePenaltyArea(
		const FVector& WorldLocation,
		float GoalLineSign,
		float ExtraDepthCm = 0.0f,
		float ExtraHalfWidthCm = 0.0f
	) const;
	FVector ClampWorldLocationInsidePitch(
		const FVector& WorldLocation,
		float InsetCm = 0.0f
	) const;

	FVector GetPitchLengthWorldDirection() const;
	FVector GetPitchWidthWorldDirection() const;

	// Returns -1 for the local -X goal line and +1 for the local +X goal line.
	float GetNearestGoalLineSign(const FVector& WorldLocation) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditAnywhere, Category = "Soccer Field|Navigation")
		bool bAutoSizeNavigationBounds = true;

	UPROPERTY(EditAnywhere, Category = "Soccer Field|Navigation", meta = (ClampMin = "0.0"))
		float NavigationBoundsOutsideMarginCm = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer Field|Navigation", meta = (ClampMin = "1.0"))
		float NavigationBoundsHeightCm = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer Field|Navigation")
		float NavigationBoundsBottomLocalZ = -1.0f;

	UPROPERTY(VisibleAnywhere, Category = "Soccer Field")
		USceneComponent* FieldRoot = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Soccer Field|Ground")
		UStaticMeshComponent* OuterGround = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Soccer Field|Ground")
		UStaticMeshComponent* PitchSurface = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Soccer Field|Walls")
		UStaticMeshComponent* NorthWall = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Soccer Field|Walls")
		UStaticMeshComponent* SouthWall = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Soccer Field|Walls")
		UStaticMeshComponent* EastWall = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Soccer Field|Walls")
		UStaticMeshComponent* WestWall = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer Field|Materials")
		UMaterialInterface* OuterGroundMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer Field|Materials")
		UMaterialInterface* PitchSurfaceMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer Field|Materials")
		UMaterialInterface* LineMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer Field|Materials")
		UMaterialInterface* GoalFrameMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer Field|Materials")
		UMaterialInterface* GoalNetMaterial = nullptr;

	void UpdateNavigationBounds();
	ANavMeshBoundsVolume* FindNavigationBoundsVolume() const;

	UStaticMeshComponent* CreateBoxComponent(
		UStaticMesh* Mesh,
		const FString& ComponentName,
		const FVector& Location,
		const FVector& SizeCm,
		bool bEnableCollision,
		FName ComponentTag,
		bool bCanCharacterStepUp
	);

	UStaticMeshComponent* CreateLineSegment(
		UStaticMesh* Mesh,
		const FString& ComponentName,
		const FVector& Start,
		const FVector& End,
		float ThicknessCm,
		float HeightCm
	);

	void CreateGoalLineAreaMarking(
		UStaticMesh* Mesh,
		const FString& Prefix,
		float GoalLineX,
		float FrontLineX,
		float HalfWidthCm,
		float ThicknessCm,
		float HeightCm,
		float Z
	);

	void CreateCircleMarking(
		UStaticMesh* Mesh,
		const FString& Prefix,
		const FVector& Center,
		float RadiusCm,
		int32 Segments,
		float ThicknessCm,
		float HeightCm
	);

	void CreateArcMarking(
		UStaticMesh* Mesh,
		const FString& Prefix,
		const FVector& Center,
		float RadiusCm,
		float StartDegrees,
		float EndDegrees,
		int32 Segments,
		float ThicknessCm,
		float HeightCm
	);
};
