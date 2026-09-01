//SoccerAICharacter.h

#pragma once

#include "CoreMinimal.h"
#include "SoccerCharacterBase.h"
#include "SoccerTeamTypes.h"
#include "SoccerAICharacter.generated.h"

class UAnimMontage;
class UAnimSequence;
class UCurveTable;

UENUM(BlueprintType)
enum class ESoccerAIMovementMode : uint8
{
	Walk UMETA(DisplayName = "Walk"),
	Jog UMETA(DisplayName = "Jog"),
	Run UMETA(DisplayName = "Run"),
	FastRun UMETA(DisplayName = "Fast Run")
};

UENUM(BlueprintType)
enum class ESoccerAIMovementReason : uint8
{
	None UMETA(DisplayName = "None"),

	NearbyReposition UMETA(DisplayName = "Nearby Reposition"),
	ShapeSupport UMETA(DisplayName = "Shape Support"),
	NormalPlay UMETA(DisplayName = "Normal Play"),

	FreeBallRace UMETA(DisplayName = "Free Ball Race"),
	PressBallCarrier UMETA(DisplayName = "Press Ball Carrier"),
	DefensiveEmergencyRecovery UMETA(DisplayName = "Defensive Emergency Recovery"),
	AttackRunIntoSpace UMETA(DisplayName = "Attack Run Into Space"),
	ChaseOwnAutoPass UMETA(DisplayName = "Chase Own Auto Pass"),
	ReceivePass UMETA(DisplayName = "Receive Pass"),

	LowEnergyRecovery UMETA(DisplayName = "Low Energy Recovery"),
	GoalkeeperEmergency UMETA(DisplayName = "Goalkeeper Emergency")
};

UENUM(BlueprintType)
enum class ESoccerAIAerialHeaderTactic : uint8
{
	None UMETA(DisplayName = "None"),
	ShotAtGoal UMETA(DisplayName = "Shot At Goal"),
	PassToTeammate UMETA(DisplayName = "Pass To Teammate"),
	ProlongAttack UMETA(DisplayName = "Prolong Attack"),
	DefensiveClearance UMETA(DisplayName = "Defensive Clearance"),
	DefensivePass UMETA(DisplayName = "Defensive Pass")
};

class AActor;
class ASoccerBall;
class UAnimMontage;

UCLASS(Blueprintable)
class THIRDPERSONCPP_API ASoccerAICharacter : public ASoccerCharacterBase
{
	GENERATED_BODY()

public:
	ASoccerAICharacter();

	virtual void Tick(float DeltaTime) override;

	// During scripted movements (for example, leaving the NavMesh for a
	// throw-in or corner), CharacterMovement is not the component that
	// changes the actor location. Expose the real scripted velocity so the
	// Animation Blueprint does not fall back to idle.
	virtual FVector GetVelocity() const override;

	void SetScriptedLocomotionVelocity(
		const FVector& WorldVelocity,
		ESoccerAIMovementMode MovementMode,
		ESoccerAIMovementReason MovementReason
	);

	void ClearScriptedLocomotionVelocity();

	// Mantiene la pelota en el punto de conduccion mientras una decision
	// tactica ordena al poseedor desplazarse sin soltarla. No bloquea robos.
	void SetAIPossessionCarryActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category = "Soccer|AI")
		AActor* GetHomePositionActor() const;

	void SetAIChasingBall(bool bNewAIChasingBall);

	void PossessAIBall(ASoccerBall* NewControlledBall);

	void ReleaseAIBall();

	bool IsAIPossessingBall() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Goalkeeper")
		bool IsGoalkeeperHoldingBall() const;

	bool HoldGoalkeeperBallInHands(
		ASoccerBall* SoccerBall
	);

	ASoccerBall* GetControlledAIBall() const;

	float PlayGoalkeeperDistributionMontage(
		ESoccerGoalkeeperDistributionType DistributionType
	);

	bool GetGoalkeeperDistributionMontagePlaybackState(
		ESoccerGoalkeeperDistributionType DistributionType,
		float& OutMontagePosition,
		float& OutMontageLength
	) const;

	bool MoveGoalkeeperDistributionByWorldDelta(
		const FVector& WorldDelta
	);

	// Saque lateral: la misma arquitectura in-place usada por las
	// distribuciones del arquero, pero disponible para jugadores de campo.
	bool HoldThrowInBall(ASoccerBall* SoccerBall);

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

	bool ReleaseHeldGoalkeeperBallToAirTarget(
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime
	);

	bool ReleaseHeldGoalkeeperBallForDrop(
		float ForwardSpeed,
		float LateralSpeed,
		float UpwardSpeed
	);

	bool PlaceHeldGoalkeeperBallForDistribution();

	bool KickReleasedGoalkeeperDistributionBall(
		ASoccerBall* SoccerBall,
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime,
		bool bUseGroundPass
	);

	void KickAIBallToTarget(
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime
	);

	void KickAIBallToAirTarget(
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime
	);

	// Legacy generic restart kick animation. Kept as fallback when no authored
	// montage is assigned or a montage cannot be started.
	void PlayAIKickAnimationForRestart();

	// Starts one of the AI-owned pass/strike montages for a stationary restart
	// ball without giving the character normal dribble possession. The ball is
	// frozen until the authored impact delay and then launched by the montage.
	bool StartAIKickMontageForRestart(
		ASoccerBall* BallToKick,
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime,
		bool bUseAirTarget
	);

	// True while an AI pass/shot/restart montage is still active.
	bool IsAIKickMontageActive() const;

	// Restart state machines use this to complete the restart only after the
	// actual timed foot impact has launched the ball.
	bool HasAIKickMontageImpactedBall() const;

	// For normal kicks the controller remains locked through montage blend-out.
	// For an autopass it may resume the chase immediately after ball impact.
	bool ShouldAIKickMontageLockController() const;

	FVector GetPendingAIKickTarget() const;

	// Cancels only a montage that has not reached foot contact yet. Used by
	// restart cancellation paths so an obsolete timer cannot kick the ball later.
	void CancelAIKickMontageBeforeImpact();

	void StartAIAutoPassToLocation(
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime
	);

	bool IsAIAutoPassActive() const;

	ASoccerBall* GetAIAutoPassBall() const;

	FVector GetAIAutoPassTargetLocation() const;

	FVector GetAIAutoPassStartBallLocation() const;

	float GetTimeSinceAIAutoPassStarted() const;

	void ClearAIAutoPassState();

	bool TryCollectAIAutoPassIfClose(float CollectDistance);

	float GetTimeSinceAIPossessionStarted() const;

	float GetTimeSinceAIBallReleased() const;

	bool IsAIPossessionProtected(float ProtectionTime) const;

	bool IsAIStealRecoveryActive(float RecoveryTime) const;

	void ConfigureAIAerialHeaderDecision(
		ESoccerAIAerialHeaderTactic NewTactic,
		const FVector& TargetLocation,
		float HorizontalSpeedOverride = -1.0f
	);

	void ClearAIAerialHeaderDecision();

	UFUNCTION(BlueprintPure, Category = "Soccer|AI Aerial")
	bool HasAIAerialHeaderTarget() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|AI Aerial")
	FVector GetAIAerialHeaderTargetLocation() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|AI Movement")
		float GetAIPlayerEnergyPercent() const;

	void UpdateAIMovementModeForOrder(
		ESoccerAIOrder CurrentOrder,
		const FVector& DesiredMoveLocation,
		bool bHasDesiredMoveLocation
	);

	void RequestAIMovementReevaluation();

	void RequestAIMovementMode(
		ESoccerAIMovementMode NewMovementMode,
		ESoccerAIMovementReason NewMovementReason,
		bool bForceUpdate = false
	);

	//arquero
	UFUNCTION(BlueprintPure, Category = "Soccer|Goalkeeper")
		bool IsGoalkeeperActionActive() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Goalkeeper")
		ESoccerGoalkeeperAction GetCurrentGoalkeeperAction() const;

	bool StartGoalkeeperAction(
		ESoccerGoalkeeperAction NewGoalkeeperAction,
		float MontagePlayRate = 1.0f
	);

	void ClearGoalkeeperAction();

	UAnimMontage* GetGoalkeeperMontageForAction(
		ESoccerGoalkeeperAction GoalkeeperAction
	) const;

	bool GetGoalkeeperActionMontagePlaybackState(
		ESoccerGoalkeeperAction GoalkeeperAction,
		float& OutMontagePosition,
		float& OutMontageLength
	) const;

	bool TryGetGoalkeeperSaveCurveMotionLocalOffset(
		ESoccerGoalkeeperAction GoalkeeperAction,
		float MontageTime,
		FVector2D& OutLocalOffset
	) const;

	// Se configura inmediatamente antes de comenzar una atajada seleccionada.
	// Solo escala la fila Lateral de la CurveTable de movimiento; un valor menor
	// que 1.0 acorta el traslado y nunca se permiten valores negativos.
	void SetGoalkeeperSaveAdaptiveLateralScale(
		float NewScale,
		float MaximumExtraDistance
	);

	float GetGoalkeeperSaveAdaptiveLateralScale() const;

	// Correccion procedural congelada por el selector al comenzar la atajada.
	// El vector es local al arquero: X Forward, Y Lateral, Z Up.
	void SetGoalkeeperSaveNearPerfectContactCorrection(
		const FVector& NewLocalCorrection,
		float ContactMontageTime,
		float BlendInTime,
		float ReleaseTime
	);

	void ClearGoalkeeperSaveNearPerfectContactCorrection();

	void StopGoalkeeperActionMontage(
		float BlendOutTime = 0.08f
	);

	// Carrera hacia atras del arquero. La reproduccion ya no depende
	// del estado tactico Retreat: se activa cuando la velocidad real
	// de la capsula tiene componente negativa respecto del Forward
	// del personaje y se corta automaticamente al dejar de retroceder.
	bool StartGoalkeeperRetreatBackpedalAnimation();

	void StopGoalkeeperRetreatBackpedalAnimation(
		float BlendOutTime = -1.0f
	);

protected:
	void UpdateGoalkeeperRetreatBackpedalAnimationFromVelocity();

	virtual void BeginPlay() override;

	virtual void UpdateSoccerAnimationState() override;

	virtual bool ResolveAerialActiveHeaderTarget(
		FVector& OutTargetLocation
	) const override;

	virtual bool ResolveAerialDefensiveBlockTarget(
		FVector& OutTargetLocation
	) const override;

	virtual float ResolveAerialActiveHeaderSpeedOverride() const override;

	virtual float ResolveAerialDefensiveBlockSpeedOverride() const override;

	virtual void OnAerialBallContactResolved(
		const FSoccerAerialContactResult& ContactResult
	) override;

private:

	UAnimMontage* GetGoalkeeperDistributionMontage(
		ESoccerGoalkeeperDistributionType DistributionType
	) const;

	UPROPERTY(EditAnywhere, Category = "Soccer|Movement")
		float WalkSpeed = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Movement")
		float JogSpeed = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Movement")
		float RunSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Movement")
		float FastRunSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI")
		AActor* HomePositionActor = nullptr;

	bool bAIIsChasingBall = false;

	bool bScriptedLocomotionVelocityActive = false;

	FVector ScriptedLocomotionVelocity = FVector::ZeroVector;

	void UpdateAIPossessedBallLocation();

	UPROPERTY()
		ASoccerBall* ControlledBall = nullptr;

	bool bAIIsPossessingBall = false;

	bool bAIPossessionCarryActive = false;

	ESoccerAIAerialHeaderTactic AIAerialHeaderTactic =
		ESoccerAIAerialHeaderTactic::None;

	bool bHasAIAerialHeaderTarget = false;

	FVector AIAerialHeaderTargetLocation = FVector::ZeroVector;

	float AIAerialHeaderHorizontalSpeedOverride = -1.0f;

	// Indica que la posesión actual no es con los pies:
	// la pelota está físicamente adjunta a las manos del arquero.
	bool bGoalkeeperHoldingBall = false;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Ball"
	)
		FName GoalkeeperBallHoldSocketName =
		TEXT("GK_BallHold_R");

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Possession")
		float AIPossessedBallForwardOffset = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Possession")
		float AIPossessedBallHeight = 11.0f;

	float LastAIPossessionStartTime = -1000.0f;

	float LastAIBallReleasedTime = -1000.0f;

	UPROPERTY()
		ASoccerBall* AIAutoPassBall = nullptr;

	bool bAIAutoPassActive = false;

	FVector AIAutoPassTargetLocation = FVector::ZeroVector;

	float AIAutoPassStartTime = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassMinCollectDelay = 0.18f;

	FVector AIAutoPassStartBallLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassMinBallTravelDistanceBeforeCollect = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassMaxCollectBallHeight = 180.0f;

	bool bAIIsKickingForAnimation = false;

	float AIKickingAnimationEndTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Animation")
		float AIKickAnimationDuration = 0.45f;

	// AI-only kick montage set. These intentionally remain separate from the
	// human character configuration so both systems can evolve independently.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Animations")
		UAnimMontage* RunningLeftLegPassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Animations")
		UAnimMontage* RunningLeftLegSidePassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Animations")
		UAnimMontage* RunningRightLegPassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Animations")
		UAnimMontage* RunningRightLegSidePassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Animations")
		UAnimMontage* StandLeftLegPassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Animations")
		UAnimMontage* StandLeftLegSidePassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Animations")
		UAnimMontage* StandRightLegPassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Animations")
		UAnimMontage* StandRightLegSidePassMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Animations")
		UAnimMontage* StrikeLeftLegForwardJogMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Animations")
		UAnimMontage* StrikeRightLegForwardJogMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Selection")
		float AIShortKickMaxDistance = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Selection")
		float AILongKickMinDistance = 6000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Selection")
		float AISideKickMinAngleDegrees = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Selection")
		float AIRunningKickMinSpeed = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Selection")
		float AIForcedLongAnglePassSpeed = 1300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Timing")
		float AIPassKickImpactDelay = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Timing")
		float AIStrikeKickImpactDelay = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Timing")
		float AIKickAnimationFinishExtraDelay = 0.05f;

	// If another possession or animation moves the frozen ball this far before
	// impact, the pending kick is cancelled instead of kicking a stolen ball.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Kick Timing")
		float AIPendingKickMaxBallDriftDistance = 80.0f;

	UPROPERTY()
		ASoccerBall* PendingAIKickBall = nullptr;

	UAnimMontage* ActiveAIKickMontage = nullptr;

	FVector PendingAIKickTarget = FVector::ZeroVector;
	FVector PendingAIKickStartBallLocation = FVector::ZeroVector;

	float PendingAIKickHorizontalSpeed = 0.0f;
	float PendingAIKickMinTravelTime = 0.0f;
	float PendingAIKickMaxTravelTime = 0.0f;

	bool bAIKickMontageActive = false;
	bool bPendingAIKickUsesAirTarget = false;
	bool bPendingAIKickHasImpactedBall = false;
	bool bLastAIKickMontageEndedAfterImpact = false;
	bool bPendingAIKickIsAutoPass = false;
	bool bPendingAIKickIsRestart = false;

	FTimerHandle AIKickImpactTimerHandle;
	FTimerHandle AIKickFinishTimerHandle;

	void StartAIKickAnimation();

	UAnimMontage* SelectAIKickMontageForTarget(
		const FVector& TargetLocation,
		float& OutHorizontalSpeed
	) const;

	UAnimMontage* SelectAIKickMontageForBallLocation(
		const FVector& BallLocation,
		const FVector& TargetLocation,
		float& OutHorizontalSpeed
	) const;

	bool StartAIKickMontage(
		UAnimMontage* KickMontage,
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime,
		bool bUseAirTarget,
		bool bTreatAsAutoPass = false
	);

	bool StartAIKickMontageWithBall(
		ASoccerBall* BallToKick,
		UAnimMontage* KickMontage,
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime,
		bool bUseAirTarget,
		bool bReleaseCurrentAIPossession,
		bool bTreatAsAutoPass,
		bool bTreatAsRestart
	);

	float GetAIKickImpactDelayForMontage(
		UAnimMontage* KickMontage,
		float MontageDuration
	) const;

	void PerformPendingAIKickImpact();
	void FinishPendingAIKickAnimation();
	void CancelPendingAIKickAnimation(bool bReleaseFrozenBall);
	bool ShouldCancelPendingAIKickAnimation() const;

	bool TryStartAIDribbleTurnAutoPassToLocation(
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime
	);

	void StartAIDribbleTurnAutoPass(
		const FVector& DesiredDirection,
		UAnimMontage* TurnMontage,
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime,
		float ForcedLocomotionSpeed
	);

	void PerformAIDribbleTurnAutoPassImpact();

	void FinishAIDribbleTurnAutoPassAnimation();

	void ExecuteAIAutoPassImmediate(
		const FVector& TargetLocation,
		float HorizontalSpeed,
		float MinTravelTime,
		float MaxTravelTime,
		bool bPlayGenericKickAnimation
	);

	UAnimMontage* SelectAIDribbleTurnMontageForDirection(
		const FVector& DesiredDirection,
		float CurrentSpeed,
		float& OutAngleDegrees,
		float& OutForcedLocomotionSpeed
	) const;

	void StartAIDribbleTurnActorRotation(
		const FVector& DesiredDirection,
		float Duration
	);

	void UpdateAIDribbleTurnActorRotation(float DeltaTime);

public:
	UFUNCTION(BlueprintPure, Category = "Soccer|AI Animation")
		bool IsAIDribbleTurnAutoPassActive() const;

private:
	bool bAIDribbleTurnAutoPassActive = false;

	bool bAIDribbleTurnAutoPassHasImpactedBall = false;

	FVector AIDribbleTurnCurrentDirection = FVector::ZeroVector;

	FVector ActiveAIDribbleTurnDirection = FVector::ZeroVector;

	FVector ActiveAIDribbleTurnAutoPassTargetLocation = FVector::ZeroVector;

	float ActiveAIDribbleTurnAutoPassHorizontalSpeed = 0.0f;

	float ActiveAIDribbleTurnAutoPassMinTravelTime = 0.0f;

	float ActiveAIDribbleTurnAutoPassMaxTravelTime = 0.0f;

	float ActiveAIDribbleTurnForcedLocomotionSpeed = 0.0f;

	FTimerHandle AIDribbleTurnImpactTimerHandle;

	FTimerHandle AIDribbleTurnFinishTimerHandle;

	bool bIsAIDribbleTurnActorRotating = false;

	FRotator AIDribbleTurnActorStartRotation = FRotator::ZeroRotator;

	FRotator AIDribbleTurnActorTargetRotation = FRotator::ZeroRotator;

	float AIDribbleTurnActorRotationElapsedTime = 0.0f;

	float AIDribbleTurnActorRotationCurrentDuration = 0.0f;

	float AIDribbleTurnActorTotalYawDelta = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run Animations")
		UAnimMontage* AIStrongRunDribbleTurnLeft45To90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run Animations")
		UAnimMontage* AIStrongRunDribbleTurnRight45To90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run Animations")
		UAnimMontage* AIStrongRunDribbleTurnLeftOver90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run Animations")
		UAnimMontage* AIStrongRunDribbleTurnRightOver90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run Animations")
		UAnimMontage* AINormalRunDribbleTurnLeft45To90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run Animations")
		UAnimMontage* AINormalRunDribbleTurnRight45To90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run Animations")
		UAnimMontage* AINormalRunDribbleTurnLeftOver90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run Animations")
		UAnimMontage* AINormalRunDribbleTurnRightOver90Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run")
		float AIStrongRunDribbleTurnMinPlayerSpeed = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run")
		float AIStrongRunDribbleTurnMinAngleDegrees = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run")
		float AIStrongRunDribbleTurnHardAngleDegrees = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run")
		float AIStrongRunDribbleTurnImpactDelay = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run")
		float AIStrongRunDribbleTurnFinishExtraDelay = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run")
		float AIStrongRunDribbleTurnQuickRotationDuration = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Strong Run")
		float AIStrongRunDribbleTurnForcedSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run")
		float AINormalRunDribbleTurnMinPlayerSpeed = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run")
		float AINormalRunDribbleTurnMaxPlayerSpeed = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run")
		float AINormalRunDribbleTurnMinAngleDegrees = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run")
		float AINormalRunDribbleTurnHardAngleDegrees = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run")
		float AINormalRunDribbleTurnImpactDelay = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run")
		float AINormalRunDribbleTurnFinishExtraDelay = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run")
		float AINormalRunDribbleTurnQuickRotationDuration = 0.07f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Dribble Turn|Normal Run")
		float AINormalRunDribbleTurnForcedSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Energy")
		float MaxAIPlayerEnergy = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|AI Energy", meta = (AllowPrivateAccess = "true"))
		float AIPlayerEnergy = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Energy")
		float AIFastRunEnergyDrainPerSecond = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Energy")
		float AIRunEnergyDrainPerSecond = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Energy")
		float AIJogEnergyRecoveryPerSecond = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Energy")
		float AIWalkEnergyRecoveryPerSecond = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Energy")
		float AIIdleEnergyRecoveryPerSecond = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Energy")
		float AIEnergyMovingSpeedThreshold = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Energy|Fast Run")
		float AIFastRunMinimumSpeed = 640.0f;

	// Unmodified movement values used to reapply a different runtime PlayerProfile
	// without multiplying already-tuned speeds.
	bool bPlayerProfilePhysicalBaselineCaptured = false;
	float PlayerProfileBaselineWalkSpeed = 0.0f;
	float PlayerProfileBaselineJogSpeed = 0.0f;
	float PlayerProfileBaselineRunSpeed = 0.0f;
	float PlayerProfileBaselineFastRunSpeed = 0.0f;
	float PlayerProfileBaselineFastRunMinimumSpeed = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Energy|Fast Run")
		float AIFastRunFullSpeedEnergy = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Energy|Fast Run")
		float AIFastRunMinimumSpeedEnergy = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement Decision")
		float AIMovementShortDistance = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement Decision")
		float AIMovementMediumDistance = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement Decision")
		float AIMovementLongDistance = 1300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement Decision")
		float AIMovementTargetShiftReevaluationDistance = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement Decision")
		float AIMovementTargetReachedDistance = 130.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement Decision")
		float AIMovementPassiveReviewInterval = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement Decision")
		float AINormalFastRunMinimumEnergyPercent = 0.28f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement Decision")
		float AIEmergencyFastRunMinimumEnergyPercent = 0.08f;

	ESoccerAIMovementMode CurrentAIMovementMode =
		ESoccerAIMovementMode::Run;

	ESoccerAIMovementReason CurrentAIMovementReason =
		ESoccerAIMovementReason::NormalPlay;

	ESoccerAIOrder LastAIMovementOrderAtDecision =
		ESoccerAIOrder::ReturnHome;

	FVector LastAIMovementTargetLocation =
		FVector::ZeroVector;

	bool bHadAIMovementTargetAtDecision = false;

	bool bAIMovementReevaluationRequested = true;

	float LastAIMovementDecisionTime = -1000.0f;

	void UpdateAIPlayerEnergy(float DeltaTime);
	void ApplyPlayerProfilePhysicalTuning();
	virtual void OnPlayerProfileChangedForMatch() override;

	void UpdateAIMovementSpeed();

	float GetAIBaseSpeedForMode(
		ESoccerAIMovementMode MovementMode
	) const;


	float GetEnergyAdjustedAIFastRunSpeed() const;

	bool IsAIMovementEmergencyReason(
		ESoccerAIMovementReason MovementReason
	) const;

	ESoccerAIMovementMode LimitAIMovementModeByEnergy(
		ESoccerAIMovementMode DesiredMode,
		ESoccerAIMovementReason DesiredReason
	) const;

	bool ShouldKeepCurrentAIMovementDecision(
		ESoccerAIOrder CurrentOrder,
		const FVector& DesiredMoveLocation,
		bool bHasDesiredMoveLocation
	) const;

	void ChooseAIMovementModeAndReasonForOrder(
		ESoccerAIOrder CurrentOrder,
		const FVector& DesiredMoveLocation,
		bool bHasDesiredMoveLocation,
		ESoccerAIMovementMode& OutMovementMode,
		ESoccerAIMovementReason& OutMovementReason
	) const;

	void ApplyAIMovementDecision(
		ESoccerAIMovementMode NewMovementMode,
		ESoccerAIMovementReason NewMovementReason,
		ESoccerAIOrder CurrentOrder,
		const FVector& DesiredMoveLocation,
		bool bHasDesiredMoveLocation
	);

	//arquero
		// Nuevas animaciones in place de atajada.

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperBodyBlockCatchToLeftMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperBodyBlockCatchToRightMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperBodyBlockDeflectToLeftMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperBodyBlockDeflectToRightMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperCatchAbdomenMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperCatchFaceToLeftMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperCatchFaceToRightMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperCatchOverHeadJumpToLeftMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperCatchOverHeadJumpToRightMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperCatchOverHeadRunJumpMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperDivingSaveFloorToLeftMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperDivingSaveFloorToRightMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperDivingSaveOneMeterToLeftMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperDivingSaveOneMeterToRightMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperScoopToLeftMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Animations"
	)
		UAnimMontage*
		GoalkeeperScoopToRightMontage = nullptr;
	//x///

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Animations")
		UAnimMontage* GoalkeeperCatchLowMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Animations")
		UAnimMontage* GoalkeeperCatchChestMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Animations")
		UAnimMontage* GoalkeeperCatchHighForwardMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Animations")
		UAnimMontage* GoalkeeperCatchHighRightMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Animations")
		UAnimMontage* GoalkeeperBodyBlockLeftMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Animations")
		UAnimMontage* GoalkeeperBodyBlockRightMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Animations")
		UAnimMontage* GoalkeeperBodyBlockLeftAltMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Animations")
		UAnimMontage* GoalkeeperDivingSaveLeftMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Animations")
		UAnimMontage* GoalkeeperDivingSaveRightMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Animations")
		UAnimMontage* GoalkeeperMissMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Distribution Animations"
	)
		UAnimMontage* GoalkeeperOverhandThrowMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Distribution Animations"
	)
		UAnimMontage* GoalkeeperDropKickMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Distribution Animations"
	)
		UAnimMontage* GoalkeeperPlacingBallShortMontage = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Distribution Animations"
	)
		UAnimMontage* GoalkeeperPlacingBallLongMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Animation")
		UAnimMontage* ThrowInMontage = nullptr;

	// Puede ser un socket creado para la pelota o directamente un hueso.
	// Reutiliza por defecto el socket que ya sostiene la pelota del arquero.
	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Ball")
		FName ThrowInBallHoldSocketName = TEXT("GK_BallHold_R");

	// Animacion in-place de carrera hacia atras. Se reproduce como
	// Dynamic Montage cuando la velocidad real del arquero apunta en
	// sentido contrario a su Forward, independientemente de Retreat.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Retreat Animation"
	)
		UAnimSequence* GoalkeeperRetreatBackpedalAnimation = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Retreat Animation"
	)
		FName GoalkeeperRetreatBackpedalSlotName = TEXT("DefaultSlot");

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Retreat Animation",
		meta = (ClampMin = "0.10", UIMin = "0.50", UIMax = "2.00")
	)
		float GoalkeeperRetreatBackpedalPlayRate = 1.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Retreat Animation",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "0.5")
	)
		float GoalkeeperRetreatBackpedalBlendInTime = 0.10f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Retreat Animation",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "0.5")
	)
		float GoalkeeperRetreatBackpedalBlendOutTime = 0.10f;

	// Un numero alto permite tratar la secuencia como loop mientras la
	// velocidad longitudinal del arquero siga siendo negativa.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Retreat Animation",
		meta = (ClampMin = "1", UIMin = "1", UIMax = "1000")
	)
		int32 GoalkeeperRetreatBackpedalLoopCount = 1000;

	// Histeresis de velocidad longitudinal (cm/s): entra en backpedal
	// cuando ForwardSpeed <= -StartSpeed y sale cuando
	// ForwardSpeed >= -StopSpeed. Start debe ser mayor que Stop.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Retreat Animation",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0")
	)
		float GoalkeeperBackpedalStartSpeed = 70.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Retreat Animation",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "300.0")
	)
		float GoalkeeperBackpedalStopSpeed = 20.0f;

	UPROPERTY(Transient)
		UAnimMontage* ActiveGoalkeeperRetreatBackpedalMontage = nullptr;

	bool bGoalkeeperActionActive = false;

	ESoccerGoalkeeperAction CurrentGoalkeeperAction =
		ESoccerGoalkeeperAction::None;

	float GoalkeeperActionEndTime = -1000.0f;

	void UpdateGoalkeeperActionState();

	// ============================================================
	// GOALKEEPER SAVE CURVE MOTION
	// ============================================================

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Curve Motion"
	)
		bool bUseGoalkeeperSaveCurveMotion = true;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Curve Motion"
	)
		bool bDebugGoalkeeperSaveCurveMotion = false;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Curve Motion",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "2.0"
			)
	)
		float GoalkeeperSaveCurveMotionScale = 1.0f;

	// Factor runtime congelado al iniciar el montage. No se expone como tuning
	// porque lo calcula el selector a partir de pelota, manos y curva lateral.
	float GoalkeeperSaveAdaptiveLateralScale = 1.0f;

	float GoalkeeperSaveAdaptiveLateralMaximumExtraDistance = 0.0f;

	// Runtime de Etapa 15B. Se configura justo antes del montage y se resetea
	// al terminar la accion; no son valores de tuning del Character.
	FVector GoalkeeperSaveNearPerfectContactCorrectionLocal = FVector::ZeroVector;
	float GoalkeeperSaveNearPerfectContactMontageTime = 0.0f;
	float GoalkeeperSaveNearPerfectCorrectionBlendInTime = 0.18f;
	float GoalkeeperSaveNearPerfectCorrectionReleaseTime = 0.22f;
	bool bGoalkeeperSaveNearPerfectCorrectionConfigured = false;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Curve Motion|Curves"
	)
		TMap<
		ESoccerGoalkeeperAction,
		UCurveTable*
		> GoalkeeperSaveMotionCurveTables;

	bool bGoalkeeperSaveCurveMotionInitialized =
		false;

	FVector GoalkeeperSaveMotionStartLocation =
		FVector::ZeroVector;

	// La correccion vertical 15B se aplica al Mesh, no a la capsula, para que
	// una correccion hacia abajo no quede bloqueada por el piso.
	FVector GoalkeeperSaveMotionInitialMeshRelativeLocation =
		FVector::ZeroVector;

	FVector GoalkeeperSaveMotionForwardDirection =
		FVector::ForwardVector;

	FVector GoalkeeperSaveMotionRightDirection =
		FVector::RightVector;

	FVector2D GoalkeeperSaveMotionInitialLocalDisplacement =
		FVector2D::ZeroVector;

	UCurveTable* GetGoalkeeperSaveMotionCurveTable(
		ESoccerGoalkeeperAction GoalkeeperAction
	) const;

	bool EvaluateGoalkeeperSaveLocalDisplacement(
		ESoccerGoalkeeperAction GoalkeeperAction,
		float MontageTime,
		FVector2D& OutLocalDisplacement
	) const;

	bool InitializeGoalkeeperSaveCurveMotion();

	bool ApplyGoalkeeperSaveCurveMotionAtTime(
		float MontageTime
	);

	float GetGoalkeeperSaveNearPerfectContactCorrectionAlpha(
		float MontageTime
	) const;

	void UpdateGoalkeeperSaveCurveMotion();

	void CommitGoalkeeperSaveCurveMotionToEnd();

	void ResetGoalkeeperSaveCurveMotion();

};