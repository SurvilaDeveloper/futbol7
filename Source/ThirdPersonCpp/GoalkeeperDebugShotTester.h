#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerTeamTypes.h"
#include "GoalkeeperDebugShotTester.generated.h"

class ASoccerAICharacter;
class ASoccerAIController;
class ASoccerBall;
class UAnimMontage;

UENUM(BlueprintType)
enum class EGoalkeeperDebugTargetMode : uint8
{
	Bone UMETA(DisplayName = "Bone"),
	HandsMidpoint UMETA(DisplayName = "Hands Midpoint")
};

UENUM()
enum class EGoalkeeperDebugShotTestState : uint8
{
	Idle,
	WaitingForLaunch,
	ShotLaunched,
	Finished,
	Failed
};

UCLASS(Blueprintable)
class THIRDPERSONCPP_API AGoalkeeperDebugShotTester : public AActor
{
	GENERATED_BODY()

public:
	AGoalkeeperDebugShotTester();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Goalkeeper Debug Test")
	void RunConfiguredTest();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Goalkeeper Debug Test")
	void ResetConfiguredTest();

protected:
	virtual void BeginPlay() override;

private:
	bool ResolveReferences();

	ASoccerAIController* GetGoalkeeperController() const;

	bool FindContactWindowTimes(
		UAnimMontage* Montage,
		float& OutWindowStartTime,
		float& OutWindowEndTime
	) const;

	bool GetCurrentTargetLocation(
		FVector& OutTargetLocation
	) const;

	void LaunchBallTowardCurrentTarget();

	void UpdateWaitingForLaunch();

	void UpdateShotLaunched();

	void DrawConfiguredTarget() const;

	void SetTestPanelLine(
		int32 Slot,
		const FString& Text,
		const FColor& Color,
		float Scale = 1.0f
	) const;

	void MarkTestFailed(const FString& Reason);

	FString GetActionName() const;

	FString GetTargetName() const;

	UPROPERTY(EditInstanceOnly, Category = "Goalkeeper Debug Test|References")
	ASoccerAICharacter* GoalkeeperCharacter = nullptr;

	UPROPERTY(EditInstanceOnly, Category = "Goalkeeper Debug Test|References")
	ASoccerBall* SoccerBall = nullptr;

	UPROPERTY(EditAnywhere, Category = "Goalkeeper Debug Test|References")
	bool bAutoFindReferences = true;

	UPROPERTY(EditAnywhere, Category = "Goalkeeper Debug Test|Configuration")
	ESoccerGoalkeeperAction TestAction =
		ESoccerGoalkeeperAction::CatchAbdomen;

	UPROPERTY(EditAnywhere, Category = "Goalkeeper Debug Test|Configuration")
	EGoalkeeperDebugTargetMode TargetMode =
		EGoalkeeperDebugTargetMode::Bone;

	UPROPERTY(
		EditAnywhere,
		Category = "Goalkeeper Debug Test|Configuration",
		meta = (
			EditCondition = "TargetMode == EGoalkeeperDebugTargetMode::Bone"
		)
	)
	FName TargetBoneName = TEXT("RightHand");

	UPROPERTY(
		EditAnywhere,
		Category = "Goalkeeper Debug Test|Timing",
		meta = (ClampMin = "0.0", ClampMax = "1.0")
	)
	float ContactWindowAlpha = 0.5f;

	UPROPERTY(
		EditAnywhere,
		Category = "Goalkeeper Debug Test|Ball",
		meta = (ClampMin = "0.02", UIMin = "0.02")
	)
	float BallTravelTime = 0.08f;

	UPROPERTY(
		EditAnywhere,
		Category = "Goalkeeper Debug Test|Ball",
		meta = (ClampMin = "40.0", UIMin = "40.0")
	)
	float BallStartDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Goalkeeper Debug Test|Ball")
	float BallStartHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Goalkeeper Debug Test|Behavior")
	bool bAllowEmergencyBodyContactOutsideWindow = false;

	UPROPERTY(EditAnywhere, Category = "Goalkeeper Debug Test|Execution")
	bool bAutoRunOnBeginPlay = false;

	UPROPERTY(
		EditAnywhere,
		Category = "Goalkeeper Debug Test|Execution",
		meta = (ClampMin = "0.0")
	)
	float AutoRunDelay = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Goalkeeper Debug Test|Execution")
	bool bEnableRuntimeHotkeys = true;

	UPROPERTY(EditAnywhere, Category = "Goalkeeper Debug Test|Draw")
	bool bDrawTarget = true;

	UPROPERTY(EditAnywhere, Category = "Goalkeeper Debug Test|Draw")
	float TargetSphereRadius = 14.0f;

	UPROPERTY(EditAnywhere, Category = "Goalkeeper Debug Test|Draw")
	float DebugDrawingLifetime = 0.12f;

	EGoalkeeperDebugShotTestState TestState =
		EGoalkeeperDebugShotTestState::Idle;

	float ContactWindowStartTime = 0.0f;
	float ContactWindowEndTime = 0.0f;
	float DesiredImpactMontageTime = 0.0f;
	float ScheduledLaunchMontageTime = 0.0f;

	float ShotLaunchWorldTime = -1000.0f;

	FTimerHandle AutoRunTimerHandle;

	bool bInitialGoalkeeperTransformCaptured =
		false;

	FTransform InitialGoalkeeperTransform =
		FTransform::Identity;
};
