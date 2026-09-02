// ThirdPersonCppCharacter.h

#pragma once

#include "CoreMinimal.h"
#include "SoccerCharacterBase.h"
#include "ThirdPersonCppCharacter.generated.h"

class ASoccerBall;
class USpringArmComponent;
class UCameraComponent;
class UAnimMontage;
class AActor;
class UStaticMeshComponent;

class ASoccerAICharacter;
class ASoccerMatchManager;

enum class ESoccerPlayerControlState : uint8
{
	Manual,
	ChasingBall,
	PossessingBall,
	DribbleTurning,
	Kicking
};

enum class ESoccerPendingKickMode : uint8
{
	None,
	KickAndFollow,
	KickAndRelease
};

UCLASS(config = Game)
class THIRDPERSONCPP_API AThirdPersonCppCharacter : public ASoccerCharacterBase
{
	GENERATED_BODY()

public:
	AThirdPersonCppCharacter();

	virtual void Tick(float DeltaTime) override;

	void AddScore(int32 Amount);

	int32 GetScore() const;

	bool GetAimCursorScreenPosition(float& OutScreenX, float& OutScreenY) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Control")
		bool IsPossessingBall() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Control")
		bool IsChasingBall() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Control")
		bool IsKicking() const;

	// True only while a left/right click has explicitly reserved the next
	// playable ball action for the human. The MatchManager uses this intent to
	// keep a slower teammate from duplicating the same recovery run.
	UFUNCTION(BlueprintPure, Category = "Soccer|Control")
		bool HasActiveHumanBallClaim() const;

	/** True while the human has an assisted jump-header request armed, queued, or playing. */
	UFUNCTION(BlueprintPure, Category = "Soccer|Aerial|Human")
		bool IsHumanJumpHeaderRequestActive() const;

	/** Small HUD status for the human jump-header request. */
	bool GetHumanJumpHeaderHUDStatus(
		FString& OutText,
		FLinearColor& OutColor
	) const;

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
		float BaseTurnRate;

	/** Returns CameraBoom subobject **/
	/** Returns FollowCamera subobject **/
	bool GetKickChargePercent(float& OutPercent) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Energy")
		float GetPlayerEnergyPercent() const;

	virtual void ResetRuntimeStateForIncomingSubstitute() override;

	UFUNCTION(BlueprintPure, Category = "Soccer|Animation")
		bool ShouldForceDribbleTurnLocomotion() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Animation")
		float GetForcedDribbleTurnLocomotionSpeed() const;

	void ReleaseBallForAISteal();

	void ReleaseBallForMatchRestart(bool bShowFeedback = true);

	// Clears direct ball-control intent without changing ownership of the ball.
	// Match restarts and goalkeeper hand protection use this so a click, chase,
	// steal attempt or charged kick cannot survive the restricted window and
	// fire later when play becomes legal again.
	void ClearBallActionsForMatchRestriction();

	// Human throw-in support. This is deliberately owned by the human class
	// instead of sharing the AI throw-in implementation, so player input and
	// AI execution can evolve independently.
	bool HoldThrowInBall(ASoccerBall* SoccerBall);
	bool IsHoldingThrowInBall() const;
	void CancelHeldThrowInBall();
	float PlayThrowInMontage();
	bool GetThrowInMontagePlaybackState(
		float& OutMontagePosition,
		float& OutMontageLength
	) const;
	bool MoveThrowInByWorldDelta(const FVector& WorldDelta);
	bool ReleaseHeldThrowInBallToAirTarget(
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime
	);
	void SetThrowInScriptedMovementVelocity(const FVector& WorldVelocity);
	void ClearThrowInScriptedMovementVelocity();

protected:
	virtual void BeginPlay() override;

	virtual bool ResolveAerialActiveHeaderTarget(
		FVector& OutTargetLocation
	) const override;

	virtual bool ResolveAerialStandingHeaderRedirectTarget(
		FVector& OutTargetLocation
	) const override;

	virtual float ResolveAerialActiveHeaderSpeedOverride() const override;

	virtual void OnAerialBallContactResolved(
		const FSoccerAerialContactResult& ContactResult
	) override;

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

	/** Debug tester: starts an OpponentTeam penalty (NumPad 6). */
	void DebugStartOpponentPenalty();

	/** Debug tester: starts a PlayerTeam penalty (NumPad 7). */
	void DebugStartPlayerTeamPenalty();

	/** Temporary Stage 9K substitution selector (NumPad 1 through 5). */
	void DebugCyclePlayerTeamOutgoingSubstitute();
	void DebugCyclePlayerTeamIncomingSubstitute();
	void DebugConfirmPlayerTeamSubstitution();
	void DebugCancelPlayerTeamSubstitution();
	void DebugRequestOpponentTeamSubstitution();
	void DebugForceOpponentCoachSubstitutionDecision();

	/** Opens the coach menu from gameplay. Bound to M and Gamepad Menu/Options. */
	void ToggleFormationMenu();

	/** Opens the four-slot quick tactics selector. Bound to Tab and Gamepad View/Back. */
	void ToggleQuickTacticsMenu();

	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate.
	 * @param Rate This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

	// SelecciÃ³n de velocidad con teclas 1, 2, 3, 4.
	void SelectWalkSpeed();
	void SelectJogSpeed();
	void SelectRunSpeed();
	void SelectFastRunSpeed();

	void SetSelectedMovementSpeed(float NewSpeed, const FString& SpeedLabel);

	// Control de pelota.
	void StartBallControl();
	void HandleLeftClickTarget();

	void HandleTackleInput();

	void HandleHumanRunningJump();
	bool FindHumanRunningJumpTackleThreat(
		ASoccerCharacterBase*& OutThreat,
		float& OutTimeToCrossing,
		bool& bOutAmbiguous
	) const;

	void RequestNormalPassFromTeammate();
	void RequestAerialPassFromTeammate();
	void RequestPassFromTeammate(
		ESoccerHumanPassRequestType RequestType
	);

	void MoveTowardBall();
	void MoveTowardAerialPreparation();
	bool TryStartHumanAerialApproach(
		ESoccerAerialActionIntent Intent
	);

	bool TryStartOrUpdateHumanJumpHeaderRequest(
		const FVector& TargetLocation,
		float ChargedHorizontalSpeed
	);

	bool ShouldArmHumanJumpHeaderFromCurrentBall() const;
	bool ArmHumanJumpHeaderRequestFromCurrentInput();
	void UpdateHumanJumpHeaderRequestLifecycle(float DeltaTime);
	void ClearHumanJumpHeaderRequest(
		bool bClearPendingKick,
		bool bHideTargetMarker
	);

	void PossessBall();
	void UpdatePossessedBallLocation();

	void StartAutoPassFollow(const FVector& TargetLocation);
	void ClearAutoPassFollowState();
	void UpdateAutoPassFollowTargetState();
	void CollectAutoPassBallWithoutCollision();
	void PrepareAutoPassBallForCleanKick(const FVector& KickTarget);
	void RedirectAutoPassBallTowardTargetIfNeeded(float DistanceToBall);
void StartAutoPassCollectCarry();
	void UpdateAutoPassCollectCarry();

	bool HasAutoPassBallReachedTarget(
		const FVector& PreviousBallLocation,
		const FVector& CurrentBallLocation
	) const;


	bool GetMouseFieldLocation(FVector& OutLocation) const;
	void StoreKickTarget(ESoccerPendingKickMode KickMode);
	void ExecutePendingKick();


	void MoveAimCursorVertical(float Value);

	void SetCameraRightSide();
	void SetCameraLeftSide();
	void SetCameraFront();

	void ResetCameraBehind();
	void UpdateCameraQuickView(float DeltaTime);
	void StartCameraRotationToRelativeYaw(float RelativeYawDegrees);
	void ApplyCameraYaw(float NewYaw);

	UAnimMontage* SelectKickMontageForTarget(
		const FVector& TargetLocation,
		float& OutHorizontalSpeed
	) const;

	void TurnCameraWithAcceleration(float Value);

	void PerformPendingKickImpact();
	void FinishPendingKickAnimation();

	float GetKickImpactDelayForMontage(UAnimMontage* KickMontage, float MontageDuration) const;

	void CancelBallChaseByManualInput();
	bool IsHumanBallActionAllowedNow();

	void EnterManualControl();
	void EnterChasingBall();
	void ActivateHumanBallClaim();
	void ClearHumanBallClaim();

	void RegisterDribbleInput(const FVector& Direction, float Value);
	void UpdatePhysicalDribbleControl();

	void StartChargedKickRelease();
	void FinishChargedKickRelease();

	float GetCurrentKickChargePercent() const;

	FVector2D GetCurrentMovementInputVector() const;
	bool ShouldMovementInputCancelChase();

	void ArmChaseCancelIgnoreForCurrentMovementInput();
	void UpdateChaseCancelIgnoreAfterInputChanged();

	bool TryStartStrongRunDribbleTurnForDirection(
		const FVector& DesiredDirection
	);

	void StartStrongRunDribbleTurn(
		const FVector& DesiredDirection,
		UAnimMontage* TurnMontage,
		float TouchSpeed,
		float UpwardSpeed,
		bool bShouldKickToTargetAfterTurn = false,
		const FVector& KickTarget = FVector::ZeroVector,
		ESoccerPendingKickMode KickMode = ESoccerPendingKickMode::None,
		float KickHorizontalSpeed = 0.0f,
		bool bUsesChargedTrajectory = false
	);

	void PerformStrongRunDribbleTurnImpact();

	void FinishStrongRunDribbleTurnAnimation();

	UAnimMontage* SelectStrongRunDribbleTurnMontageForDirection(
		const FVector& DesiredDirection,
		float& OutAngleDegrees
	) const;

	bool TryStartStrongRunDribbleTurnForPendingKick();

	bool TryStartNormalRunDribbleTurnForDirection(
		const FVector& DesiredDirection
	);

	void StartNormalRunDribbleTurn(
		const FVector& DesiredDirection,
		UAnimMontage* TurnMontage,
		float TouchSpeed,
		float UpwardSpeed
	);

	void PerformNormalRunDribbleTurnImpact();

	void FinishNormalRunDribbleTurnAnimation();

	UAnimMontage* SelectNormalRunDribbleTurnMontageForDirection(
		const FVector& DesiredDirection,
		float& OutAngleDegrees
	) const;

	void ResumeDribbleMovementAfterTurn(const FVector& Direction, float ResumeSpeed);

	void StartStrongRunDribbleTurnActorRotation(
		const FVector& DesiredDirection,
		float Duration
	);

	void UpdateStrongRunDribbleTurnActorRotation(float DeltaTime);

	void UpdateStrongRunDribbleTurnMovement(float DeltaTime);

	void StartNormalRunDribbleTurnActorRotation(
		const FVector& DesiredDirection,
		float Duration
	);

	void UpdateNormalRunDribbleTurnActorRotation(float DeltaTime);

	void UpdateNormalRunDribbleTurnMovement(float DeltaTime);

	void ShowAutoPassTargetMarker(const FVector& TargetLocation);
	void HideAutoPassTargetMarker();

	void ShowReleaseTargetMarker(const FVector& TargetLocation);
	void HideReleaseTargetMarker();

	AActor* SpawnTargetMarker(TSubclassOf<AActor> MarkerClass, const FVector& TargetLocation);

	void DisableMarkerCollision(AActor* MarkerActor) const;

	void UpdatePossessionIndicator();

	void UpdatePlayerEnergy(float DeltaTime);
	void UpdateEnergyAdjustedMovementSpeed();
	void ApplyPlayerProfilePhysicalTuning();
	virtual void OnPlayerProfileChangedForMatch() override;

	float GetEnergyAdjustedFastRunSpeed() const;
	bool IsSelectedMovementSpeed(float Speed) const;

private:

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
		USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
		UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player", meta = (AllowPrivateAccess = "true"))
		int32 Score = 0;

	// Pelota controlada.
	UPROPERTY()
		ASoccerBall* ControlledBall = nullptr;

	ESoccerPlayerControlState SoccerControlState = ESoccerPlayerControlState::Manual;

	// This is deliberately separate from ChasingBall: that control state is
	// also entered by automatic aerial follow-ups and other non-click systems.
	bool bHumanBallClaimActive = false;

	ESoccerPendingKickMode PendingKickMode = ESoccerPendingKickMode::None;

	FVector PendingKickTarget = FVector::ZeroVector;

	bool bCameraQuickViewActive = false;

	float CameraQuickViewRelativeYaw = 0.0f;

	float CameraQuickViewBaseYaw = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Camera")
		float CameraQuickViewTransitionTime = 0.2f;

	bool bCameraRotationTransitionActive = false;

	float CameraRotationStartYaw = 0.0f;

	float CameraRotationTargetYaw = 0.0f;

	float CameraRotationElapsedTime = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Camera|Manual Turn")
		float CameraTurnAccelerationTime = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Camera|Manual Turn")
		float CameraTurnInputDeadZone = 0.02f;

	float CameraTurnAccelerationAlpha = 0.0f;

	float LastCameraTurnInputSign = 0.0f;

	// Movimiento manual.
	UPROPERTY(EditAnywhere, Category = "Soccer|Movement")
		float WalkSpeed = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Movement")
		float JogSpeed = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Movement")
		float RunSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Movement")
		float FastRunSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Energy")
		float MaxPlayerEnergy = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Energy", meta = (AllowPrivateAccess = "true"))
		float PlayerEnergy = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Energy")
		float FastRunEnergyDrainPerSecond = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Energy")
		float RunEnergyDrainPerSecond = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Energy")
		float JogEnergyRecoveryPerSecond = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Energy")
		float WalkEnergyRecoveryPerSecond = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Energy")
		float IdleEnergyRecoveryPerSecond = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Energy")
		float EnergyMovingSpeedThreshold = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Energy|Fast Run")
		float FastRunMinimumSpeed = 640.0f;

	// Unmodified movement values used to reapply a different runtime PlayerProfile
	// without multiplying already-tuned speeds.
	bool bPlayerProfilePhysicalBaselineCaptured = false;
	float PlayerProfileBaselineWalkSpeed = 0.0f;
	float PlayerProfileBaselineJogSpeed = 0.0f;
	float PlayerProfileBaselineRunSpeed = 0.0f;
	float PlayerProfileBaselineFastRunSpeed = 0.0f;
	float PlayerProfileBaselineFastRunMinimumSpeed = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Energy|Fast Run")
		float FastRunFullSpeedEnergy = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Energy|Fast Run")
		float FastRunMinimumSpeedEnergy = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Control")
		float BallPossessionDistance = 70.0f; /////////

	UPROPERTY(EditAnywhere, Category = "Soccer|Control")
		float RepossessDelayAfterKick = 0.45f;

	float LastKickTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassTargetArrivalRadius = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassCollectDistance = 130.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassLooseBallCollectSpeed = 120.0f;

	bool bIsAutoPassFollowActive = false;

	bool bAutoPassCanCollect = false;

	FVector AutoPassTargetLocation = FVector::ZeroVector;

	FVector LastAutoPassBallLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassPreKickBallDistanceFromPlayer = 105.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassRedirectDistance = 115.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassRedirectCooldown = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassRedirectMinSpeed = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassRedirectMaxSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassRedirectPlayerSpeedMultiplier = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassCollectSoftTouchSpeed = 120.0f;

	float LastAutoPassRedirectTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassCollectCarryDuration = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassCollectCarryStartSpeedMultiplier = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Auto Pass")
		float AutoPassCollectCarryMinSpeed = 180.0f;

	bool bIsAutoPassCollectCarrying = false;

	float AutoPassCollectCarryEndTime = 0.0f;

	float AutoPassCollectCarryStartTime = 0.0f;

	float AutoPassCollectCarryStartSpeed = 0.0f;

	FVector AutoPassCollectCarryDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Soccer|Control")
		float PossessedBallForwardOffset = 60.0f; ////////////

	UPROPERTY(EditAnywhere, Category = "Soccer|Control")
		float PossessedBallHeight = 11.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Control")
		float BallPossessionMaxHeight = 180.0f;

	bool IsBallAtPlayablePossessionHeight(const ASoccerBall* SoccerBall) const;

	// Patada hacia punto seleccionado.
	UPROPERTY(EditAnywhere, Category = "Soccer|Kick")
		float TargetKickHorizontalSpeed = 2200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick")
		float TargetKickMinTravelTime = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick")
		float TargetKickMaxTravelTime = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick")
		float KickExecutionDistance = 55.0f;


	UPROPERTY(EditAnywhere, Category = "Soccer|Aim")
		float AimCursorScreenYPercent = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aim")
		float AimCursorVerticalSensitivity = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aim")
		float AimCursorMinScreenYPercent = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aim")
		float AimCursorMaxScreenYPercent = 0.90f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aim")
		float AimCursorAccelerationTime = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aim")
		float AimCursorInputDeadZone = 0.02f;

	float AimCursorAccelerationAlpha = 0.0f;

	float LastAimCursorInputSign = 0.0f;

	// Animaciones de patada.
	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Animations")
		UAnimMontage* RunningLeftLegPassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Animations")
		UAnimMontage* RunningLeftLegSidePassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Animations")
		UAnimMontage* RunningRightLegPassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Animations")
		UAnimMontage* RunningRightLegSidePassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Animations")
		UAnimMontage* StandLeftLegPassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Animations")
		UAnimMontage* StandLeftLegSidePassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Animations")
		UAnimMontage* StandRightLegPassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Animations")
		UAnimMontage* StandRightLegSidePassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Animations")
		UAnimMontage* StrikeLeftLegForwardJogMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Animations")
		UAnimMontage* StrikeRightLegForwardJogMontage = nullptr;

	// Human throw-in uses the same authored throw_in_in_place montage as the AI,
	// but keeps a separate Blueprint assignment and runtime.
	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Animation")
		UAnimMontage* ThrowInMontage = nullptr;

	// May be either a socket or a bone. The default matches the socket already
	// used by the goalkeeper/AI throw-in setup in this project.
	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Ball")
		FName ThrowInBallHoldSocketName = TEXT("GK_BallHold_R");

	bool bHumanThrowInHoldingBall = false;
	bool bHumanThrowInScriptedMovementActive = false;
	float HumanThrowInSavedMaxWalkSpeed = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Selection")
		float ShortKickMaxDistance = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Selection")
		float LongKickMinDistance = 6000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Selection")
		float SideKickMinAngleDegrees = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Selection")
		float RunningKickMinSpeed = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Selection")
		float ForcedLongAnglePassSpeed = 1300.0f;

	FTimerHandle KickImpactTimerHandle;
	FTimerHandle KickFinishTimerHandle;

	FVector ActiveKickTarget = FVector::ZeroVector;

	float ActiveKickHorizontalSpeed = 0.0f;

	ESoccerPendingKickMode ActiveKickMode = ESoccerPendingKickMode::None;

	bool bActiveKickHasImpactedBall = false;

	bool bActiveKickUsesChargedTrajectory = false;

	// Captured when a kick animation starts so the restart-specific behavior
	// survives the first-touch transition that immediately ends the context.
	bool bActiveKickWasHumanRestartExecution = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Timing")
		float PassKickImpactDelay = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Timing")
		float StrikeKickImpactDelay = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Timing")
		float KickAnimationFinishExtraDelay = 0.05f;

	float SelectedMovementSpeed = 350.0f;
	
	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleMinSpeed = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleForwardExtraOffset = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleSideOffset = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleFrequency = 4.0f;
	
	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleTouchDistance = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleTouchCooldown = 0.20f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleTouchSpeed = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleTouchUpwardSpeed = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleMaxPossessionDistance = 550.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleNoInputStopSpeed = 20.0f;

	FVector PendingDribbleInputDirection = FVector::ZeroVector;
	FVector DesiredDribbleDirection = FVector::ZeroVector;
	FVector CurrentDribbleDirection = FVector::ZeroVector;

	bool bHasDesiredDribbleDirection = false;

	float LastDribbleTouchTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleTouchPlayerSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleTouchMaxSpeed = 1350.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleRearmDistance = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleMaxTouchWaitTime = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribbling")
		float DribbleStuckBallSpeed = 60.0f;

	bool bCanDribbleTouch = true;

	FVector LastDribbleTouchBallLocation = FVector::ZeroVector;

	bool bForceDribbleTurnLocomotion = false;

	float ForcedDribbleTurnLocomotionSpeed = 0.0f;

	float ForcedDribbleTurnLocomotionEndTime = 0.0f;
	
	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run Animations")
		UAnimMontage* StrongRunDribbleTurnLeft45To90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run Animations")
		UAnimMontage* StrongRunDribbleTurnRight45To90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run Animations")
		UAnimMontage* StrongRunDribbleTurnLeftOver90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run Animations")
		UAnimMontage* StrongRunDribbleTurnRightOver90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run")
		float StrongRunDribbleTurnMinPlayerSpeed = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run")
		float StrongRunDribbleTurnMinAngleDegrees = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run")
		float StrongRunDribbleTurnHardAngleDegrees = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run")
		float StrongRunDribbleTurnImpactDelay = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run")
		float StrongRunDribbleTurnFinishExtraDelay = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run")
		float StrongRunDribbleTurnQuickRotationDegrees = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run")
		float StrongRunDribbleTurnQuickRotationDuration = 0.05f;

	float StrongRunDribbleTurnActorTotalYawDelta = 0.0f;

	bool bIsStrongRunDribbleTurnActorRotating = false;

	FRotator StrongRunDribbleTurnActorStartRotation = FRotator::ZeroRotator;
	FRotator StrongRunDribbleTurnActorTargetRotation = FRotator::ZeroRotator;

	float StrongRunDribbleTurnActorRotationElapsedTime = 0.0f;
	float StrongRunDribbleTurnActorRotationCurrentDuration = 0.35f;

	FTimerHandle StrongRunDribbleTurnImpactTimerHandle;
	FTimerHandle StrongRunDribbleTurnFinishTimerHandle;

	FVector ActiveStrongRunDribbleTurnDirection = FVector::ZeroVector;

	float ActiveStrongRunDribbleTurnTouchSpeed = 0.0f;
	float ActiveStrongRunDribbleTurnUpwardSpeed = 0.0f;

	bool bActiveStrongRunDribbleTurnHasImpactedBall = false;


	bool bActiveStrongRunDribbleTurnShouldKickToTarget = false;

	FVector ActiveStrongRunDribbleTurnKickTarget = FVector::ZeroVector;

	ESoccerPendingKickMode ActiveStrongRunDribbleTurnKickMode =
		ESoccerPendingKickMode::None;

	float ActiveStrongRunDribbleTurnKickHorizontalSpeed = 0.0f;

	bool bActiveStrongRunDribbleTurnUsesChargedTrajectory = false;

	float ActiveStrongRunDribbleTurnResumeSpeed = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Strong Run")
		float StrongRunDribbleTurnZeroSpeedTime = 0.18f;

	float ActiveStrongRunDribbleTurnDuration = 0.0f;
	float ActiveStrongRunDribbleTurnElapsedTime = 0.0f;
	float ActiveStrongRunDribbleTurnInitialSpeed = 0.0f;
	FVector ActiveStrongRunDribbleTurnInitialDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run Animations")
		UAnimMontage* NormalRunDribbleTurnLeft45To90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run Animations")
		UAnimMontage* NormalRunDribbleTurnRight45To90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run Animations")
		UAnimMontage* NormalRunDribbleTurnLeftOver90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run Animations")
		UAnimMontage* NormalRunDribbleTurnRightOver90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run")
		float NormalRunDribbleTurnMinPlayerSpeed = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run")
		float NormalRunDribbleTurnMaxPlayerSpeed = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run")
		float NormalRunDribbleTurnMinAngleDegrees = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run")
		float NormalRunDribbleTurnHardAngleDegrees = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run")
		float NormalRunDribbleTurnImpactDelay = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run")
		float NormalRunDribbleTurnFinishExtraDelay = 0.05f;

	FTimerHandle NormalRunDribbleTurnImpactTimerHandle;
	FTimerHandle NormalRunDribbleTurnFinishTimerHandle;

	FVector ActiveNormalRunDribbleTurnDirection = FVector::ZeroVector;

	float ActiveNormalRunDribbleTurnTouchSpeed = 0.0f;
	float ActiveNormalRunDribbleTurnUpwardSpeed = 0.0f;

	bool bActiveNormalRunDribbleTurnHasImpactedBall = false;

	float ActiveNormalRunDribbleTurnResumeSpeed = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run")
		float NormalRunDribbleTurnQuickRotationDegrees = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dribble Turn|Normal Run")
		float NormalRunDribbleTurnQuickRotationDuration = 0.07f;

	// Tiempo de desaceleracion antes de adoptar la nueva direccion.
	// Se conserva el nombre historico de la variable para no romper valores guardados en Blueprint,
	// pero el giro ya no llega a velocidad cero.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Dribble Turn|Normal Run",
		meta = (DisplayName = "Normal Run Dribble Turn Slowdown Time", ClampMin = "0.01")
	)
		float NormalRunDribbleTurnZeroSpeedTime = 0.18f;

	// Velocidad minima fisica (cm/s) durante un cambio de direccion de dribble normal.
	// Mantenerla por encima de la zona de Idle evita la secuencia Run -> Idle -> Run.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Dribble Turn|Normal Run",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "700.0")
	)
		float NormalRunDribbleTurnMinimumSpeed = 300.0f;

	bool bIsNormalRunDribbleTurnActorRotating = false;

	FRotator NormalRunDribbleTurnActorStartRotation = FRotator::ZeroRotator;
	FRotator NormalRunDribbleTurnActorTargetRotation = FRotator::ZeroRotator;

	float NormalRunDribbleTurnActorRotationElapsedTime = 0.0f;
	float NormalRunDribbleTurnActorRotationCurrentDuration = 0.35f;
	float NormalRunDribbleTurnActorTotalYawDelta = 0.0f;

	float ActiveNormalRunDribbleTurnDuration = 0.0f;
	float ActiveNormalRunDribbleTurnElapsedTime = 0.0f;
	float ActiveNormalRunDribbleTurnInitialSpeed = 0.0f;
	FVector ActiveNormalRunDribbleTurnInitialDirection = FVector::ZeroVector;

	bool bIsChargingKickRelease = false;

	float KickChargeStartTime = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Charge")
		float KickChargeFullTime = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Charge")
		float KickChargeMinHorizontalSpeed = 350.0f; //700

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Charge")
		float KickChargeMaxHorizontalSpeed = 4000.0f; //3200

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Charge")
		float KickChargeDistanceReference = 10000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kick Charge")
		float KickChargeDistanceMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact", meta = (ClampMin = "0.0"))
		float ChargedHeaderSpeedMultiplier = 0.50f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact", meta = (ClampMin = "0.0"))
		float ChargedHeaderMinimumSpeed = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact", meta = (ClampMin = "0.0"))
		float ChargedHeaderMaximumSpeed = 2200.0f;

	/*
	 * Stage 3: this state is separate from the generic pending ground kick.
	 * It survives the assisted approach, but is cleared if the predictive plan
	 * is lost before contact or if manual movement cancels the approach.
	 */
	// ============================================================
	// HUMAN RUNNING JUMP / TACKLE EVASION
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Running Jump")
	ESoccerTackleEvasionSide DefaultHumanRunningJumpSide =
		ESoccerTackleEvasionSide::Left;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Running Jump|Threat", meta = (ClampMin = "1.0"))
	float HumanRunningJumpThreatHalfWidth = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Running Jump|Threat", meta = (ClampMin = "1.0"))
	float HumanRunningJumpMinimumIncomingSlideSpeed = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Running Jump|Threat", meta = (ClampMin = "0.05"))
	float HumanRunningJumpMaximumThreatTime = 0.80f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Running Jump|Threat", meta = (ClampMin = "0.0"))
	float HumanRunningJumpThreatTieTime = 0.04f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Running Jump|Threat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HumanRunningJumpMaximumTacklerNormalizedTime = 0.72f;

	bool bHumanJumpHeaderRequestActive = false;
	bool bHumanJumpHeaderMontageStarted = false;
	float HumanJumpHeaderRequestStartWorldTime = -1.0f;
	float HumanJumpHeaderPlanSearchAccumulator = 0.0f;

	/** How long an armed right-click keeps searching for a synchronized jump. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Human", meta = (ClampMin = "0.1"))
		float HumanJumpHeaderPlanSearchTimeout = 4.0f;

	/** Retry interval while the ball is airborne but no valid plan exists yet. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Human", meta = (ClampMin = "0.01"))
		float HumanJumpHeaderPlanSearchInterval = 0.03f;

	/** A non-requested airborne ball can arm the header above this height. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Human", meta = (ClampMin = "0.0"))
		float HumanJumpHeaderArmMinimumBallHeight = 90.0f;

	/** Or when its vertical speed already indicates a meaningful aerial ball. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Human", meta = (ClampMin = "0.0"))
		float HumanJumpHeaderArmMinimumVerticalSpeed = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Human")
		bool bReturnToManualControlWhenHumanJumpHeaderPlanIsLost = true;

	float GetDistanceAdjustedChargedKickSpeed(
		float ChargedSpeed,
		const FVector& TargetLocation
	) const;

	bool bHasPendingKickHorizontalSpeedOverride = false;

	float PendingKickHorizontalSpeedOverride = 0.0f;

	float LastMoveForwardInputValue = 0.0f;
	float LastMoveRightInputValue = 0.0f;

	bool bIgnoreChaseCancelUntilMovementInputChanges = false;

	FVector2D IgnoredMovementInputForChaseCancel = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Soccer|Input")
		float MovementInputCancelDeadZone = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Input")
		float MovementInputChangeCancelThreshold = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Markers")
		TSubclassOf<AActor> AutoPassTargetCrossMarkerClass;

	UPROPERTY(EditAnywhere, Category = "Soccer|Markers")
		TSubclassOf<AActor> ReleaseTargetCrossMarkerClass;

	UPROPERTY(EditAnywhere, Category = "Soccer|Markers")
		float KickTargetMarkerLifeTime = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Markers")
		float KickTargetMarkerGroundOffset = 1.0f;

	UPROPERTY()
		AActor* ActiveAutoPassTargetMarker = nullptr;

	UPROPERTY()
		AActor* ActiveReleaseTargetMarker = nullptr;

	FTimerHandle AutoPassTargetMarkerTimerHandle;

	FTimerHandle ReleaseTargetMarkerTimerHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Possession Indicator", meta = (AllowPrivateAccess = "true"))
		UStaticMeshComponent* PossessionIndicator = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Possession Indicator")
		float PossessionIndicatorHeight = 130.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Possession Indicator")
		FVector PossessionIndicatorBaseScale = FVector(0.25f, 0.25f, 0.25f);

	UPROPERTY(EditAnywhere, Category = "Soccer|Possession Indicator")
		float PossessionIndicatorPulseAmount = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Possession Indicator")
		float PossessionIndicatorPulseSpeed = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Possession Indicator")
		float PossessionIndicatorBobHeight = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Possession Indicator")
		float PossessionIndicatorBobSpeed = 1.5f;

	void FindMatchManager();

	bool TryRegisterHumanKickTouchForRules();

	ASoccerAICharacter* GetOpponentPossessingAICharacter();

	void StartHumanStealAttempt(ASoccerAICharacter* TargetAICharacter);

	void UpdateHumanStealAttempt();

	void MoveTowardHumanStealTarget();

	void CompleteHumanStealAttempt(ASoccerAICharacter* TargetAICharacter, ASoccerBall* SoccerBall);

	void FailHumanStealAttempt();

	void ClearHumanStealAttemptOnly();

	void ClearPendingKickAfterFailedSteal();

	UPROPERTY()
		ASoccerMatchManager* MatchManager = nullptr;

	UPROPERTY()
		ASoccerAICharacter* HumanStealTargetAICharacter = nullptr;

	bool bIsHumanStealAttemptActive = false;

	float HumanStealAttemptStartTime = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Steal")
		float HumanStealSuccessDistance = 95.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Steal")
		float HumanStealMaxAttemptTime = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Steal")
		float HumanStealSuccessChance = 0.75f;
};
