#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SoccerAerialActionTypes.h"
#include "SoccerAerialChestDebugTester.generated.h"

class ASoccerBall;
class ASoccerCharacterBase;
class ASoccerAIController;
class ASoccerMatchManager;
class AController;
class USceneComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ESoccerAerialChestDebugMode : uint8
{
	FullPrediction UMETA(DisplayName = "Full Prediction And Positioning"),
	ExactMontageContact UMETA(DisplayName = "Exact Montage Contact")
};


UENUM(BlueprintType)
enum class ESoccerStandingControlTestTarget : uint8
{
	Chest UMETA(DisplayName = "Chest Height"),
	Head UMETA(DisplayName = "Head Height"),
	Alternate UMETA(DisplayName = "Alternate Chest / Head Each F7")
};

UENUM()
enum class ESoccerAerialChestDebugState : uint8
{
	Idle,
	WaitingForMontage,
	Running,
	Finished,
	Failed
};

struct FSoccerAerialFrozenCharacterState
{
	TWeakObjectPtr<ASoccerCharacterBase> Character;
	TWeakObjectPtr<AController> Controller;
	FTransform Transform = FTransform::Identity;
	bool bActorTickEnabled = true;
	bool bActorCollisionEnabled = true;
	bool bControllerTickEnabled = true;
	bool bHadMovementComponent = false;
	TEnumAsByte<EMovementMode> MovementMode = MOVE_Walking;
	uint8 CustomMovementMode = 0;
};

UCLASS(Blueprintable)
class THIRDPERSONCPP_API ASoccerAerialChestDebugTester : public AActor
{
	GENERATED_BODY()

public:
	ASoccerAerialChestDebugTester();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Aerial Chest Debug Test")
	void RunConfiguredTest();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Aerial Chest Debug Test")
	void ResetConfiguredTest();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool ResolveReferences();
	void CaptureInitialState();
	void RestoreInitialState();
	void FreezeEnvironment();
	void RestoreFrozenEnvironment();
	void PlaceCharacterAtTestStation();
	void ReleaseBallFromAnyOwner();

	bool StartExactMontageContactTest();
	bool StartFullPredictionTest();
	bool QueuePlanForTest(
		const FSoccerAerialInterceptionPlan& Plan,
		bool bAllowApproach,
		bool bLockPlan
	);

	FVector GetCharacterGroundLocation() const;
	FVector BuildConfiguredContactLocation(
		const FSoccerAerialActionProfile& Profile
	) const;
	FVector SelectStandingControlTrackPoint(
		const FVector& ChestLower,
		const FVector& ChestUpper,
		const FVector& Head
	) const;
	ESoccerAerialContactSurface ResolveTargetSurfaceForRun();
	FString GetExpectedSurfaceName() const;
	bool DidResolveExpectedSurface() const;
	FVector BuildFullPredictionContactLocation(
		const FSoccerAerialActionProfile& Profile
	) const;
	FVector BuildConfiguredBallStartLocation(
		const FVector& ContactLocation
	) const;
	bool LaunchBallToContact(
		const FVector& ContactLocation,
		float TravelTime
	);
	bool LaunchExactBallLikeGoalkeeper(
		const FVector& ContactLocation,
		float TravelTime
	);
	void ResetMeasurementsAtExactLaunch();
	bool SamplePredictedBallLocationAtTime(
		float TimeFromNow,
		FVector& OutLocation
	) const;
	void CapturePlannedTrajectoryDebug(float PredictionHorizon);
	bool IsPrimaryRuntimeHotkeyTester() const;
	int32 CountRuntimeHotkeyTesters() const;
	void StopOtherRunningTesters();

	void UpdateActiveTest(float DeltaTime);
	void UpdateDiagnosticMeasurements(
		const FSoccerAerialDebugSnapshot& Snapshot
	);
	void CaptureIdealMoment(
		const FSoccerAerialDebugSnapshot& Snapshot
	);
	void FinalizeTest();
	void MarkTestFailed(const FString& Reason);
	void ScheduleAutomaticRepeat();

	void DrawTestConfiguration() const;
	void DrawRuntimeDiagnostics(
		const FSoccerAerialDebugSnapshot& Snapshot
	) const;
	void DrawResultSummary() const;
void UpdateWorldStatusText();
	FString BuildWorldStatusText() const;
	FColor GetWorldStatusColor() const;

FString BuildFailureClassification() const;

	UPROPERTY(VisibleAnywhere, Category = "Aerial Chest Debug Test|Visible Status")
	USceneComponent* DebugRoot = nullptr;

	/*
	 * Texto real dentro del mundo. No depende de AddOnScreenDebugMessage,
	 * de GAreScreenMessagesEnabled ni del HUD del partido.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Aerial Chest Debug Test|Visible Status")
	UTextRenderComponent* WorldStatusText = nullptr;

	/*
	 * Copia negra colocada apenas detras del texto principal. Funciona como
	 * sombra/contorno y mantiene la leyenda legible sobre el cesped, lineas,
	 * jugadores y cielo sin depender del HUD.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Aerial Chest Debug Test|Visible Status")
	UTextRenderComponent* WorldStatusTextShadow = nullptr;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Visible Status")
	bool bShowWorldStatusText = true;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Visible Status")
	bool bShowWorldStatusTextShadow = true;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Visible Status", meta = (ClampMin = "100.0"))
	float WorldStatusTextHeight = 340.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Visible Status", meta = (ClampMin = "8.0"))
	float WorldStatusTextSize = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Visible Status", meta = (ClampMin = "0.0", ClampMax = "12.0"))
	float WorldStatusTextShadowOffset = 3.0f;

	UPROPERTY(EditInstanceOnly, Category = "Aerial Chest Debug Test|References")
	ASoccerCharacterBase* TestCharacter = nullptr;

	UPROPERTY(EditInstanceOnly, Category = "Aerial Chest Debug Test|References")
	ASoccerBall* SoccerBall = nullptr;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|References")
	bool bAutoFindReferences = true;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Mode")
	ESoccerAerialChestDebugMode TestMode =
		ESoccerAerialChestDebugMode::ExactMontageContact;

	/*
	 * Deliberately changes the height used to build the test trajectory.
	 * Contact resolution remains natural: neither HEAD nor CHEST is forced.
	 */
	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Mode")
	ESoccerStandingControlTestTarget StandingControlTestTarget =
		ESoccerStandingControlTestTarget::Alternate;

	/*
	 * FullPrediction + true:
	 * isolates planning/timing/contact while removing navigation error.
	 * FullPrediction + false:
	 * lets the selected AI navigate to the preparation point.
	 */
	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Mode")
	bool bSnapCharacterToExactPreparationLocation = true;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Station")
	bool bUseTesterTransformAsCharacterTransform = true;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Station")
	float CharacterYawOffsetDegrees = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball", meta = (ClampMin = "100.0"))
	float BallStartDistance = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball")
	float BallLateralOffset = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball", meta = (ClampMin = "20.0"))
	float BallStartHeightAboveGround = 165.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball", meta = (ClampMin = "40.0"))
	float DesiredContactHeightAboveGround = 135.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball", meta = (ClampMin = "0.15"))
	float FullPredictionBallTravelTime = 0.95f;

	/*
	 * FullPrediction must not aim at the chest's current world position, or the
	 * calculated PreparationLocation is already where the character is standing
	 * and Snap appears to do nothing. These offsets create an independent future
	 * crossing point so the calculated teleport is visible and meaningful.
	 */
	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball|Full Prediction")
	float FullPredictionContactForwardOffset = 320.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball|Full Prediction")
	float FullPredictionContactLateralOffset = 140.0f;

	/*
	 * Legacy property name kept so existing tester actors preserve their value.
	 * It is now the minimum flight time used by the full predictor/NAV test.
	 * A short value such as 0.95 s cannot leave enough time to run, settle and
	 * then reach the 0.78 s contact frame of header_chest.
	 */
	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball|Full Prediction", meta = (ClampMin = "0.50"))
	float FullPredictionSnapMinimumTravelTime = 1.80f;

	/* Extra reserve after the estimated arrival so navigation can brake. */
	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball|Full Prediction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FullPredictionNavSafetyMargin = 0.25f;

	/*
	 * Exact mode now follows GoalkeeperDebugShotTester:
	 * start the montage first, then launch a short shot toward the live
	 * animated target so gravity cannot hide contact-detection errors.
	 */
	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball|Exact Goalkeeper Style", meta = (ClampMin = "0.05", ClampMax = "0.30"))
	float ExactModeBallTravelTime = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball|Exact Goalkeeper Style", meta = (ClampMin = "40.0"))
	float ExactModeBallStartDistance = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball|Exact Goalkeeper Style")
	float ExactModeBallStartHeightOffset = 0.0f;

	/* Corrects the launch against SoccerBall's own trajectory simulator. */
	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball|Calibration", meta = (ClampMin = "1", ClampMax = "12"))
	int32 LaunchCalibrationIterations = 6;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball|Calibration", meta = (ClampMin = "0.1"))
	float LaunchCalibrationTolerance = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Ball|Calibration", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float LaunchCalibrationGain = 0.90f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Isolation")
	bool bFreezeOtherCharacters = true;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Isolation")
	bool bFreezeMatchManager = true;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Execution")
	bool bAutoRunOnBeginPlay = false;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Execution", meta = (ClampMin = "0.0"))
	float AutoRunDelay = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Execution")
	bool bAutoRepeat = false;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Execution", meta = (ClampMin = "0.25"))
	float AutoRepeatDelay = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Execution")
	bool bEnableRuntimeHotkeys = true;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Execution", meta = (ClampMin = "1.0"))
	float TestTimeout = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Draw")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Draw", meta = (ClampMin = "0.02"))
	float DebugDrawingLifetime = 0.25f;

	/*
	 * Desactivado por defecto. La leyenda principal es WorldStatusText; dejar
	 * esto en false evita duplicados con AddOnScreenDebugMessage.
	 */
	/* Off by default: only a stable status and final result remain on screen. */
	UPROPERTY(EditAnywhere, Category = "Aerial Chest Debug Test|Draw")
	bool bShowLiveMeasurements = false;

	ESoccerAerialChestDebugState TestState =
		ESoccerAerialChestDebugState::Idle;
	ESoccerAerialContactSurface ActiveExpectedContactSurface =
		ESoccerAerialContactSurface::Chest;
	bool bAlternateNextRunUsesHead = false;

	FSoccerAerialActionProfile ActiveProfile;
	FSoccerAerialInterceptionPlan ActivePlan;
	FSoccerAerialContactResult CapturedContactResult;

	FVector IntendedContactLocation = FVector::ZeroVector;
	FVector ConfiguredBallStartLocation = FVector::ZeroVector;
	FVector ConfiguredLaunchVelocity = FVector::ZeroVector;
	FVector LaunchCalibrationResidual = FVector::ZeroVector;
	FVector PredictionLocationBeforeSnap = FVector::ZeroVector;
	FVector PredictionRequestedSnapLocation = FVector::ZeroVector;
	FVector PredictionAppliedSnapLocation = FVector::ZeroVector;
	float PredictionRequestedSnapDistance = 0.0f;
	float PredictionSnapApplicationError = 0.0f;
	bool bPredictionSnapApplied = false;
	float FullPredictionEstimatedArrivalTime = -1.0f;
	float FullPredictionRequestedTravelTime = -1.0f;
	float FullPredictionUsefulApproachTime = -1.0f;
	float ExactLaunchMontageTime = -1.0f;
	float ExactLaunchTravelTime = 0.0f;
	bool bExactBallLaunched = false;

	/*
	 * Una sola trayectoria congelada para visualizar el intento. Antes el
	 * tester reconstruia y dibujaba una curva nueva cada Tick durante 0.25 s;
	 * las curvas viejas quedaban superpuestas y parecian lanzamientos dobles.
	 */
	TArray<FVector> PlannedTrajectoryDebugPoints;
	TArray<float> PlannedTrajectoryDebugTimes;
	TArray<FVector> ActualTrajectoryDebugPoints;
	int32 PlannedTrajectoryDebugRevision = INDEX_NONE;

	float CurrentTrajectoryPredictionError = 0.0f;
	float MaximumTrajectoryPredictionError = 0.0f;
	FVector CurrentPredictedBallLocation = FVector::ZeroVector;
	bool bHasCurrentPredictedBallLocation = false;

	float PlannedMontageStartWorldTime = -1000.0f;
	float ActualMontageStartWorldTime = -1000.0f;
	float MontageStartTimingError = 0.0f;
	bool bActualMontageStartCaptured = false;

	float TestStartWorldTime = -1000.0f;
	float BallLaunchWorldTime = -1000.0f;
	float PreviousMontagePosition = -1.0f;

	float ClosestChestDistance = BIG_NUMBER;
	float ClosestHeadDistance = BIG_NUMBER;
	float ClosestChestMontageTime = 0.0f;
	float ClosestHeadMontageTime = 0.0f;
	float ClosestChestDistanceInsideWindow = BIG_NUMBER;
	float ClosestHeadDistanceInsideWindow = BIG_NUMBER;

	bool bPreviousSnapshotValid = false;
	FSoccerAerialDebugSnapshot PreviousSnapshot;

	bool bMontageStarted = false;
	bool bIdealMomentCaptured = false;
	bool bContactCaptured = false;
	bool bBallEnteredChestVolumeInsideWindow = false;
	bool bBallEnteredHeadVolumeInsideWindow = false;
	bool bBallEnteredChestVolumeOutsideWindow = false;
	bool bBallEnteredHeadVolumeOutsideWindow = false;

	FVector IdealBallLocation = FVector::ZeroVector;
	FVector IdealChestPoint = FVector::ZeroVector;
	FVector IdealHeadLocation = FVector::ZeroVector;
	FVector IdealTrackToActualChestErrorLocal = FVector::ZeroVector;
	FVector ClosestChestErrorLocal = FVector::ZeroVector;
	float ClosestChestTimeError = 0.0f;
	FVector ClosestHeadErrorLocal = FVector::ZeroVector;
	float ClosestHeadTimeError = 0.0f;

	FString FailureReason;
	FString FinalClassification;

	bool bInitialStateCaptured = false;
	FTransform InitialCharacterTransform = FTransform::Identity;
	FTransform InitialBallTransform = FTransform::Identity;
	FVector InitialBallVelocity = FVector::ZeroVector;
	bool bInitialCharacterTickEnabled = true;
	bool bInitialCharacterCollisionEnabled = true;
	bool bInitialControllerTickEnabled = true;
	TEnumAsByte<EMovementMode> InitialMovementMode = MOVE_Walking;
	uint8 InitialCustomMovementMode = 0;

	TArray<FSoccerAerialFrozenCharacterState> FrozenCharacters;
	TWeakObjectPtr<ASoccerMatchManager> FrozenMatchManager;
	bool bFrozenMatchManagerTickEnabled = true;

	FTimerHandle AutoRunTimerHandle;
	FTimerHandle AutoRepeatTimerHandle;
};
