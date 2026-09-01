//SoccerCharacterBase.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SoccerTeamTypes.h"
#include "SoccerInterceptionTypes.h"
#include "SoccerAerialActionTypes.h"
#include "SoccerTackleTypes.h"
#include "SoccerFoulTypes.h"
#include "SoccerCharacterBase.generated.h"

class UMaterialInterface;
class ASoccerBall;
class UAnimMontage;
class UCurveTable;
class UCurveFloat;
class USoccerPlayerProfile;

UCLASS(Blueprintable)
class THIRDPERSONCPP_API ASoccerCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ASoccerCharacterBase();

	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category = "Soccer|Team")
		ESoccerTeam GetTeam() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Team")
		ESoccerPlayerRole GetPlayerRole() const;

	/** Content-side profile assigned to this match character. */
	UFUNCTION(BlueprintPure, Category = "Soccer|Player Profile")
		USoccerPlayerProfile* GetPlayerProfile() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Player Profile")
		bool HasPlayerProfile() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Player Profile")
		FName GetPlayerProfileId() const;

	/**
	 * Runtime assignment used by the Director Technical -> match handoff.
	 * Reapplies profile-driven movement tuning immediately and safely.
	 */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Player Profile")
		void SetPlayerProfileForMatch(USoccerPlayerProfile* NewPlayerProfile);

	UFUNCTION(BlueprintCallable, Category = "Soccer|Uniform")
		void ApplyTeamUniform();

	UFUNCTION(BlueprintPure, Category = "Soccer|Animation", meta = (DisplayName = "Is Possessing Ball"))
		bool GetSoccerIsPossessingBall() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Animation", meta = (DisplayName = "Is Chasing Ball"))
		bool GetSoccerIsChasingBall() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Animation", meta = (DisplayName = "Is Kicking"))
		bool GetSoccerIsKicking() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Animation", meta = (DisplayName = "Get Player Energy Percent"))
		float GetSoccerEnergyPercent() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Animation", meta = (DisplayName = "Should Force Dribble Turn Locomotion"))
		bool GetSoccerShouldForceDribbleTurnLocomotion() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Animation", meta = (DisplayName = "Get Forced Dribble Turn Locomotion Speed"))
		float GetSoccerForcedDribbleTurnLocomotionSpeed() const;

	/*
	 * Instant replay writes only the visual animation flags that are already
	 * sampled by the recorder. The live values are restored from the resume
	 * frame before gameplay is unpaused.
	 */
	void ApplyInstantReplayVisualState(
		bool bInPossessingBall,
		bool bInChasingBall,
		bool bInKicking,
		bool bInForceDribbleTurnLocomotion,
		float InForcedDribbleTurnLocomotionSpeed
	);

	/*
	 * Calcula el tiempo aproximado que necesita este jugador para alcanzar
	 * una ubicacion horizontal. Considera distancia, alcance, velocidad actual,
	 * aceleracion, velocidad maxima y una penalizacion por giro.
	 */
	float EstimateArrivalTimeToLocation(
		const FVector& TargetLocation,
		float ReachRadius = -1.0f
	) const;

	/*
	 * Etapa 3: busca el primer punto controlable de una trayectoria terrestre
	 * o aerea. Si no llega a tiempo, conserva el mejor punto tardio.
	 */
	bool FindBestBallInterception(
		const ASoccerBall* SoccerBall,
		FSoccerBallInterceptionResult& OutResult
	) const;

	/*
	 * Devuelve un objetivo estable para persecucion automatica. Invalida el
	 * compromiso inmediatamente cuando la pelota es tocada o rebota.
	 */
	bool ResolveStableBallPursuitTarget(
		const ASoccerBall* SoccerBall,
		FVector& OutPursuitLocation,
		FSoccerBallInterceptionResult* OutResult = nullptr,
		float RefreshIntervalOverride = -1.0f,
		bool bForceRefresh = false
	);

	void ClearBallPursuitTarget();
	// ============================================================
	// AERIAL ACTIONS - STAGES 1-5
	// ============================================================

	/* Builds a synchronized plan without moving or playing an animation. */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Aerial")
	bool FindBestAerialInterceptionPlan(
		ASoccerBall* SoccerBall,
		ESoccerAerialActionIntent Intent,
		FSoccerAerialInterceptionPlan& OutPlan
	) const;

	/*
	 * Stage 1 compatibility: queues only when the character is already near
	 * the preparation point.
	 */
	/*
	 * Stage 2 entry point: plans the action from a distance and exposes a
	 * preparation target so the human automatic movement or AI navigation can
	 * run there before the montage begins.
	 */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Aerial")
	bool TryStartBestAerialActionApproach(
		ASoccerBall* SoccerBall,
		ESoccerAerialActionIntent Intent
	);

	/*
	 * Stage 5 coordination entry point. The caller can rank several players
	 * using plans built for the same trajectory and then queue only the winner
	 * without asking the planner to choose a different solution a frame later.
	 */
	bool TryStartAerialActionApproachForPlan(
		const FSoccerAerialInterceptionPlan& Plan,
		ESoccerAerialActionIntent Intent
	);

	/* Replaces an approach/waiting plan without interrupting a playing montage. */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Aerial")
	bool TryChangeQueuedAerialActionIntent(
		ESoccerAerialActionIntent NewIntent
	);

	UFUNCTION(BlueprintCallable, Category = "Soccer|Aerial")
	void CancelAerialAction();

	UFUNCTION(BlueprintPure, Category = "Soccer|Aerial")
	bool IsAerialActionLocked() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Aerial")
	bool IsAerialActionApproaching() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Aerial")
	bool IsAerialActionWaitingToStart() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Aerial")
	bool IsAerialActionQueuedOrPlaying() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Aerial")
	ESoccerAerialActionType GetActiveAerialActionType() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Aerial")
	ESoccerAerialActionPhase GetAerialActionPhase() const;

	bool GetAerialPreparationTarget(
		FVector& OutLocation,
		float& OutAcceptanceRadius
	) const;

	ASoccerBall* GetAerialActionBall() const;
	const FSoccerAerialInterceptionPlan& GetQueuedAerialPlan() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Aerial|Contact")
	bool HasResolvedAerialBallContact() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Aerial|Contact")
	FSoccerAerialContactResult GetLastAerialContactResult() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Aerial|Contact")
	float GetLastAerialContactWorldTime() const;

	/* Debug/test support. Does not alter normal gameplay behavior. */
	bool GetAerialActionProfileCopy(
		ESoccerAerialActionType ActionType,
		FSoccerAerialActionProfile& OutProfile
	) const;

	bool GetAerialDebugSnapshot(
		FSoccerAerialDebugSnapshot& OutSnapshot
	) const;

	/*
	 * Same coordinate pipeline used by goalkeeper hand tracks.
	 * Curve rows are already canonical: X=Forward, Y=Right/Lateral, Z=Up.
	 */
	bool EvaluateStandingAerialControlContactTrack(
		float MontageTime,
		FVector& OutChestLowerLocal,
		FVector& OutChestUpperLocal,
		FVector& OutHeadLocal
	) const;

	bool GetStandingAerialControlTrackWorldLocations(
		float MontageTime,
		const FVector& FacingDirection,
		const FVector& ActorLocation,
		FVector& OutChestLowerWorld,
		FVector& OutChestUpperWorld,
		FVector& OutHeadWorld
	) const;

	/*
	 * Resolves the actual UE skeleton names used by aerial contacts.
	 * Mixamo namespaces are commonly stripped during FBX import, so the
	 * function accepts both Head and mixamorig:Head (same for torso bones).
	 */
	bool ResolveAerialContactBoneNames(
		FName& OutHeadBoneName,
		FName& OutChestLowerBoneName,
		FName& OutChestUpperBoneName,
		FString* OutFailureReason = nullptr
	) const;

	bool DebugQueueAerialPlan(
		const FSoccerAerialInterceptionPlan& Plan,
		ESoccerAerialActionIntent Intent,
		bool bAllowApproach,
		bool bLockPlan
	);

	/*
	 * Debug tester support: the goalkeeper-style tester launches the ball
	 * after the montage has already started. Adopt that new trajectory and
	 * restart continuous contact tracking from the new ball position.
	 */
	void DebugRefreshAerialContactTrackingForCurrentBallTrajectory();

	/*
	 * Debug-only contact filter used by isolated animation testers.
	 * None preserves normal gameplay selection. Chest or Head prevents the
	 * other surface from resolving the contact during the current test.
	 */
	void DebugSetForcedAerialContactSurface(
		ESoccerAerialContactSurface ContactSurface
	);

	/*
	 * Debug-only planning preference for StandingControl. It does not force
	 * the resolved contact surface; it only tells the isolated tester whether
	 * to place the actor for the animated chest track or the animated head
	 * track. None preserves normal gameplay planning (chest-first).
	 */
	void DebugSetStandingControlPlanningSurface(
		ESoccerAerialContactSurface ContactSurface
	);

	// ============================================================
	// TACKLE - STAGE 1: SHARED PHYSICAL ACTION
	// ============================================================

	/*
	 * Starts an in-place sliding tackle and supplies the horizontal motion from
	 * code. Both human and AI characters use this same physical action.
	 */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Tackle")
	bool TryStartTackle(
		ESoccerTackleSide TackleSide,
		const FVector& DesiredWorldDirection
	);

	/* Chooses left/right from the target's lateral position in local space. */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Tackle")
	bool TryStartTackleTowardLocation(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "Soccer|Tackle")
	void CancelTackle();

	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle")
	bool IsTackleActive() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle")
	ESoccerTacklePhase GetTacklePhase() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle")
	float GetTackleNormalizedTime() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle")
	FVector GetActiveTackleDirection() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle")
	float GetCurrentTackleHorizontalSpeed() const;

	// Shared in-place running jump. This is the physical/animation action;
	// tackle awareness is only one possible reason to choose a side.
	UFUNCTION(BlueprintCallable, Category = "Soccer|Running Jump")
	bool TryStartRunningJump(ESoccerTackleEvasionSide JumpSide);

	// AI/contextual wrapper: chooses a running-jump side from an incoming tackle.
	UFUNCTION(BlueprintCallable, Category = "Soccer|Tackle|Evasion")
	bool TryStartTackleEvasion(ASoccerCharacterBase* TackleInstigator);

	UFUNCTION(BlueprintCallable, Category = "Soccer|Tackle|Evasion")
	void CancelTackleEvasion();

	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle|Evasion")
	bool IsTackleEvasionActive() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle|Evasion")
	bool IsTackleEvasionAvoidingLowContact() const;

	/* Stage 2: raw physical contact data. No foul decision is made here. */
	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle|Contact")
	FSoccerTackleContactSummary GetTackleContactSummary() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle|Contact")
	ESoccerTackleContactOrder GetTackleContactOrder() const;

	UFUNCTION(BlueprintCallable, Category = "Soccer|Tackle|Fall Reaction")
	bool TryStartTackleFallReaction(
		ASoccerCharacterBase* TackleInstigator,
		const FVector& ContactLocation
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle|Fall Reaction")
	bool IsTackleFallReactionActive() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Tackle|Fall Reaction")
	ESoccerTackleFallSide GetTackleFallSide() const;

protected:
	virtual void BeginPlay() override;

	// Stage 3 player-profile physical tuning. Derived human/AI classes keep
	// their own movement/energy logic and consume these shared multipliers.
	float GetPlayerProfilePaceSpeedMultiplier() const;
	float GetPlayerProfileAccelerationMultiplier() const;
	float GetPlayerProfileStaminaDrainMultiplier() const;
	float GetPlayerProfileStaminaRecoveryMultiplier() const;
	float GetProfileAdjustedPaceSpeed(float BaseSpeed) const;

	// Stage 8A technical execution. Decision logic still chooses the intended
	// target; these helpers model how accurately/powerfully this player executes it.
	FVector GetProfileAdjustedTechnicalKickTarget(
		const FVector& BallLocation,
		const FVector& IntendedTarget,
		bool bShot
	) const;
	float GetProfileAdjustedTechnicalKickSpeed(
		float BaseHorizontalSpeed,
		bool bShot
	) const;
	bool IsPlayerProfileShotTarget(const FVector& IntendedTarget) const;
	float GetPlayerProfileTechnicalPressureAlpha() const;

	/** Derived human/AI classes refresh their own pace tiers after a runtime profile swap. */
	virtual void OnPlayerProfileChangedForMatch();
	void ApplyPlayerProfileAccelerationTuning();

	virtual void UpdateSoccerAnimationState();

	void SetSoccerAnimationState(
		bool bInIsPossessingBall,
		bool bInIsChasingBall,
		bool bInIsKicking,
		float InEnergyPercent,
		bool bInShouldForceDribbleTurnLocomotion,
		float InForcedDribbleTurnLocomotionSpeed
	);

	bool FindBestBallInterceptionFromTrajectory(
		const TArray<FSoccerBallTrajectorySample>& Trajectory,
		int32 TrajectoryRevision,
		FSoccerBallInterceptionResult& OutResult
	) const;

	void UpdateGroundInterceptionPredictionDebug(float DeltaTime);
	ASoccerBall* ResolveDebugInterceptionBall();
	void DrawGroundInterceptionPredictionDebug(
		const TArray<FSoccerBallTrajectorySample>& Trajectory,
		const FSoccerBallInterceptionResult& Result
	) const;

	void UpdateTackle();
	void ResetTackleContactTracking();
	void InitializeTackleContactTracking();
	void UpdateTackleContactTracking(float PreviousNormalizedTime);
	void FinalizeTackleContactTracking();

	void TrySubmitCurrentTackleFoulEvaluation(
		float CurrentNormalizedTime,
		bool bForce
	);
	ASoccerBall* ResolveTackleBall() const;
	bool RegisterTackleBallTouchForRules() const;
	bool ResolveTackleBallContactVelocity(
		ASoccerBall* Ball,
		ESoccerTackleContactLimb ContactLimb,
		const FVector& ContactBallLocation,
		float ContactNormalizedTime,
		FVector& OutVelocity
	) const;
	FVector GetTackleContactPoint(ESoccerTackleContactLimb Limb) const;
	bool ResolveTackleContactBoneNames(
		FName& OutLeftFootBoneName,
		FName& OutRightFootBoneName,
		FName& OutLeftLowerLegBoneName,
		FName& OutRightLowerLegBoneName,
		FString* OutFailureReason = nullptr
	) const;
	UAnimMontage* GetTackleMontage(ESoccerTackleSide TackleSide) const;
	ESoccerTackleSide ChooseTackleSideForTarget(const FVector& TargetLocation) const;
	float EvaluateTackleSpeedAlpha(float NormalizedTime) const;
	void FinishTackle();

	void UpdateTackleFallReaction();
	UAnimMontage* GetTackleFallMontage(ESoccerTackleFallSide FallSide) const;

	void UpdateTackleEvasion(float DeltaTime);
	UAnimMontage* GetTackleEvasionMontage(ESoccerTackleEvasionSide EvasionSide) const;
	ESoccerTackleEvasionSide ChooseTackleEvasionSide(
		const ASoccerCharacterBase* TackleInstigator
	) const;
	void FinishTackleEvasion();
	ESoccerTackleFallSide ChooseTackleFallSide(
		const ASoccerCharacterBase* TackleInstigator,
		const FVector& ContactLocation
	) const;
	float EvaluateTackleFallInertiaAlpha(float NormalizedTime) const;
	void FinishTackleFallReaction();

	void UpdateAerialAction(float DeltaTime);
	bool FindBestAerialInterceptionPlanInternal(
		ASoccerBall* SoccerBall,
		ESoccerAerialActionIntent Intent,
		ESoccerAerialContactSurface PreferredStandingSurface,
		FSoccerAerialInterceptionPlan& OutPlan
	) const;
	bool BuildAerialPlanForProfile(
		const ASoccerBall* SoccerBall,
		const TArray<FSoccerBallTrajectorySample>& Trajectory,
		const FSoccerAerialActionProfile& Profile,
		ESoccerAerialContactSurface PreferredStandingSurface,
		FSoccerAerialInterceptionPlan& OutPlan
	) const;
	bool BuildJumpHeaderPlanForProfile(
		const ASoccerBall* SoccerBall,
		const TArray<FSoccerBallTrajectorySample>& Trajectory,
		const FSoccerAerialActionProfile& Profile,
		FSoccerAerialInterceptionPlan& OutPlan
	) const;
	bool BuildStandingAerialControlPlanForSurface(
		const ASoccerBall* SoccerBall,
		const TArray<FSoccerBallTrajectorySample>& Trajectory,
		const FSoccerAerialActionProfile& Profile,
		ESoccerAerialContactSurface PlannedSurface,
		FSoccerAerialInterceptionPlan& OutPlan
	) const;
	bool BuildBestStandingAerialControlPlan(
		const ASoccerBall* SoccerBall,
		const TArray<FSoccerBallTrajectorySample>& Trajectory,
		ESoccerAerialActionIntent Intent,
		ESoccerAerialContactSurface PreferredStandingSurface,
		FSoccerAerialInterceptionPlan& OutPlan
	) const;
	UCurveTable* GetAerialContactTrackCurveTable(
		ESoccerAerialActionType ActionType
	) const;
	bool EvaluateAerialContactTrack(
		ESoccerAerialActionType ActionType,
		float MontageTime,
		FVector& OutChestLowerLocal,
		FVector& OutChestUpperLocal,
		FVector& OutHeadLocal
	) const;
	bool GetAerialContactTrackWorldLocations(
		ESoccerAerialActionType ActionType,
		float MontageTime,
		const FVector& FacingDirection,
		const FVector& ActorLocation,
		FVector& OutChestLowerWorld,
		FVector& OutChestUpperWorld,
		FVector& OutHeadWorld
	) const;
	bool ConfigureAutomaticStandingHeaderRedirect(
		const ASoccerBall* SoccerBall,
		FSoccerAerialInterceptionPlan& InOutPlan
	) const;
	bool ConfigureRequestedStandingHeaderRedirect(
		const ASoccerBall* SoccerBall,
		const FVector& RequestedTarget,
		FSoccerAerialInterceptionPlan& InOutPlan
	) const;
	void EvaluateStandingControlRecovery(
		const ASoccerBall* SoccerBall,
		const FVector& PredictedOutgoingVelocity,
		FSoccerAerialInterceptionPlan& InOutPlan
	) const;
	FVector CalculatePredictedChestControlVelocity(
		const FVector& IncomingVelocity,
		const FVector& FacingDirection
	) const;
	FVector CalculatePassiveStandingHeaderVelocity(
		const FVector& IncomingVelocity,
		const FVector& ContactLocation,
		const FVector& FacingDirection,
		const FVector& RedirectTarget,
		float ContactQuality,
		float OpponentPressure
	) const;
	FVector PredictStandingControlRecoveryLocation(
		const ASoccerBall* SoccerBall,
		const FVector& ContactLocation,
		const FVector& OutgoingVelocity,
		float& OutBallTravelTime
	) const;
	float EstimateStandingControlSelfRecoveryTime(
		const FSoccerAerialInterceptionPlan& Plan,
		const FVector& RecoveryLocation,
		float BallTravelTime
	) const;
	float FindEarliestOpponentRecoveryTime(
		const FVector& RecoveryLocation,
		const FVector& ContactLocation,
		float BallAvailableTime,
		float& OutNearestOpponentDistanceAtContact
	) const;
	bool IsStandingControlRecoveryLocationInsideField(
		const FVector& RecoveryLocation
	) const;
	const FSoccerAerialActionProfile* GetAerialProfile(
		ESoccerAerialActionType ActionType
	) const;
	UAnimMontage* GetAerialMontage(
		ESoccerAerialActionType ActionType
	) const;
	bool QueueAerialPlan(
		const FSoccerAerialInterceptionPlan& Plan,
		ESoccerAerialActionIntent Intent,
		bool bAllowApproach
	);
	bool RefreshQueuedAerialPlan(bool bForceRefresh);
	bool TryCommitQueuedAerialMontageAtScheduledTime(
		float CurrentTime,
		float DistanceToPreparation,
		float PreparationTolerance,
		ASoccerBall* Ball
	);
	bool StartQueuedAerialMontage();
	void ResetAerialStartTimingDebug();
	void FinishAerialAction();
	void ScheduleStandingHeaderPostContactRelease();
	void ReleaseStandingHeaderAfterContact();
	void RotateTowardQueuedAerialPlan(float DeltaTime);
	void SnapFacingToQueuedAerialPlan();
	void UpdateAerialDebugAutoStart(float DeltaTime);
	void DrawAerialPlanDebug(
		const FSoccerAerialInterceptionPlan& Plan,
		const FColor& Color
	) const;

	void UpdateAerialContact(float DeltaTime);
	void InitializeAerialContactTracking();
	bool TryResolveAerialContact(float MontagePosition);
	bool BuildAerialContactCandidate(
		float MontagePosition,
		FSoccerAerialContactCandidate& OutCandidate
	) const;
	float CalculateAerialContactQuality(
		FSoccerAerialContactCandidate& Candidate
	) const;
	float CalculateAerialOpponentPressure(
		ASoccerBall* Ball,
		int32& OutNearbyOpponentCount
	) const;
	bool IsBestAerialContestCandidate(
		const FSoccerAerialContactCandidate& Candidate,
		int32& OutCompetingPlayerCount,
		ASoccerCharacterBase*& OutBestOtherCharacter
	) const;
	void ResolveAerialContestBodyConsequences(
		const FSoccerAerialContactCandidate& WinningCandidate
	);
	void UpdateAerialBodyContest();
	void ReceiveAerialContestBodyReaction(
		ASoccerCharacterBase* OtherCharacter,
		const FVector& SeparationDirection,
		float StrengthMultiplier
	);
	float GetAerialAirborneCommitment(float MontagePosition) const;
	bool IsJumpAerialAction(ESoccerAerialActionType ActionType) const;
	bool DetectAerialHeadContact(
		const FVector& CurrentBallLocation,
		const FVector& CurrentHeadLocation,
		float CombinedRadius,
		float& OutContactAlpha,
		float& OutNormalizedDistance,
		FVector& OutContactLocation
	) const;
	bool DetectAerialChestContact(
		const FVector& CurrentBallLocation,
		const FVector& CurrentChestLowerLocation,
		const FVector& CurrentChestUpperLocation,
		float CombinedRadius,
		float& OutContactAlpha,
		float& OutNormalizedDistance,
		FVector& OutContactLocation
	) const;
	bool ResolveAerialContactVelocity(
		ESoccerAerialContactSurface ContactSurface,
		const FVector& ContactLocation,
		const FVector& IncomingVelocity,
		float ContactQuality,
		float OpponentPressure,
		FVector& OutVelocity
	) const;
	bool RegisterAerialTouchForRules() const;
	void DrawAerialContactDebug(
		const FSoccerAerialContactResult& ContactResult
	) const;

	virtual bool ResolveAerialActiveHeaderTarget(
		FVector& OutTargetLocation
	) const;
	virtual bool ResolveAerialDefensiveBlockTarget(
		FVector& OutTargetLocation
	) const;
	virtual bool ResolveAerialStandingHeaderRedirectTarget(
		FVector& OutTargetLocation
	) const;
	virtual float ResolveAerialActiveHeaderSpeedOverride() const;
	virtual float ResolveAerialDefensiveBlockSpeedOverride() const;
	virtual void OnAerialBallContactResolved(
		const FSoccerAerialContactResult& ContactResult
	);

	// ============================================================
	// PREDICCION DE INTERCEPCION - ETAPA 1
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception")
		float InterceptionPredictionHorizon = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception")
		float InterceptionTrajectorySampleInterval = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception")
		float InterceptionReachRadius = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception")
		float InterceptionArrivalSafetyMargin = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception")
		float InterceptionEstimatedTurnRateDegreesPerSecond = 540.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception")
		float InterceptionTurnTimeScale = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception")
		float InterceptionOpposingVelocityTimeScale = 0.50f;

	// Altura maxima a la que un jugador de campo intenta controlar la pelota.
	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Air", meta = (ClampMin = "0.0"))
		float InterceptionMaximumPlayableBallHeight = 155.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Air", meta = (ClampMin = "0.0"))
		float InterceptionMinimumPlayableBallHeight = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Air", meta = (ClampMin = "0.0"))
		float InterceptionAerialArrivalSafetyMargin = 0.12f;

	// Evita elegir una pelota que aun asciende si existe una muestra descendente cercana.
	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Air", meta = (ClampMin = "0.0"))
		float InterceptionRisingBallExtraSafetyTime = 0.08f;

	// ============================================================
	// PERSECUCION PREDICTIVA ESTABLE - ETAPA 3
	// ============================================================

	/* El nombre se conserva para no perder valores guardados en Blueprints. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Pursuit")
		bool bUsePredictiveGroundBallPursuit = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Pursuit", meta = (ClampMin = "0.02"))
		float GroundBallPursuitRefreshInterval = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Pursuit", meta = (ClampMin = "0.0"))
		float GroundBallPursuitMinimumCommitTime = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Pursuit", meta = (ClampMin = "0.0"))
		float GroundBallPursuitDirectChaseDistance = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Pursuit", meta = (ClampMin = "0.0"))
		float GroundBallPursuitDirectChaseSpeed = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Pursuit", meta = (ClampMin = "0.0"))
		float GroundBallPursuitSoftUpdateDistance = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Pursuit", meta = (ClampMin = "0.0"))
		float GroundBallPursuitTargetSwitchDistance = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Pursuit", meta = (ClampMin = "0.0"))
		float GroundBallPursuitRequiredMarginImprovement = 0.10f;

	/* Debug enable moved to SoccerDebugManager > AI Interception / Prediction. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Debug")
		float InterceptionDebugUpdateInterval = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Debug")
		float InterceptionDebugDrawingDuration = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Debug")
		bool bInterceptionDebugShowText = true;

	// ============================================================
	// AERIAL ACTIONS - STAGES 1-5
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Montages")
	UAnimMontage* HeaderJumpKickMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Montages")
	UAnimMontage* HeaderJumpBlockMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Montages")
	UAnimMontage* HeaderChestMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Profiles")
	FSoccerAerialActionProfile StandingAerialControlProfile;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Profiles")
	FSoccerAerialActionProfile JumpHeaderKickProfile;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Profiles")
	FSoccerAerialActionProfile JumpHeaderBlockProfile;

	/*
	 * Imported from header_chest_contact_track_head_center_seconds.csv as
	 * Curve Table / Linear. ChestLower and ChestUpper use the exported bone-head
	 * positions; HeadForward/Lateral/Up use the geometric center of the Head bone.
	 * Axes are converted before import:
	 * Forward=Blender Z, Lateral(right+)=-Blender X, Up=Blender Y.
	 */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Tracks")
	UCurveTable* StandingAerialControlContactTrackCurveTable = nullptr;

	/*
	 * Imported from header_jump_kick_contact_track_head_center_seconds.csv as
	 * Curve Table / Linear. The Head rows represent the geometric center of
	 * the animated head. Planning and contact detection use that same point,
	 * avoiding the old generic fixed-height approximation.
	 */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Tracks")
	UCurveTable* JumpHeaderKickContactTrackCurveTable = nullptr;

	/*
	 * Imported from header_jump_block_contact_track_head_center_seconds.csv as
	 * Curve Table / Linear. Its Head rows also represent the geometric center
	 * of the animated head.
	 */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Tracks")
	UCurveTable* JumpHeaderBlockContactTrackCurveTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Tracks", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StandingAerialChestTrackAlpha = 0.50f;

	/*
	 * A control-oriented player prefers the chest because it usually leaves
	 * the ball closer. The standing head control is used when the trajectory
	 * cannot be reached cleanly with the chest. The physical detector still
	 * decides the surface that actually touches the ball.
	 */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning|Standing Control")
	bool bPreferChestForStandingAerialControl = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning", meta = (ClampMin = "0.5"))
	float AerialPredictionHorizon = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning", meta = (ClampMin = "0.01"))
	float AerialPredictionSampleInterval = 0.025f;

	/*
	 * Stage 2: montage times examined inside the complete jump-header contact
	 * window. Smaller values give a more exact solution at a higher CPU cost.
	 */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning|Jump Header", meta = (ClampMin = "0.005", ClampMax = "0.10"))
	float AerialJumpHeaderContactTimeSearchInterval = 0.02f;

	/* Small scheduling tolerance for navigation/arrival estimation error. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning|Jump Header", meta = (ClampMin = "0.0", ClampMax = "0.20"))
	float AerialJumpHeaderArrivalTolerance = 0.04f;

	/* Penalty in the plan score for moving away from IdealContactTime. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning|Jump Header", meta = (ClampMin = "0.0"))
	float AerialJumpHeaderTimingDeviationScoreWeight = 0.35f;

	/* Penalty in seconds for using the edge of the allowed vertical contact radius. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning|Jump Header", meta = (ClampMin = "0.0"))
	float AerialJumpHeaderVerticalErrorScoreWeight = 0.10f;

	/* Rewards a small amount of preparation time without delaying the play excessively. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning|Jump Header", meta = (ClampMin = "0.0"))
	float AerialJumpHeaderArrivalMarginScoreBonus = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning", meta = (ClampMin = "0.0"))
	float AerialPlanStartLocationTolerance = 65.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning", meta = (ClampMin = "0.0"))
	float AerialPlanCancelLocationTolerance = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning", meta = (ClampMin = "1.0"))
	float AerialFacingRotationSpeedDegreesPerSecond = 720.0f;

	/*
	 * Las animaciones aereas son in-place. Si al llegar el instante exacto
	 * el personaje quedo apenas fuera del punto previsto por la navegacion,
	 * se permite una correccion horizontal pequena antes de iniciar el montage.
	 * Evita comenzar 0.10-0.20 s tarde esperando que MoveTo alcance un radio
	 * final muy estricto.
	 */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning")
	bool bUseAerialScheduledStartPositionCorrection = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning", meta = (ClampMin = "0.0"))
	float AerialScheduledStartMaxPositionCorrection = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Planning", meta = (ClampMin = "0.0"))
	float AerialPlanLateCancellationTolerance = 0.20f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Approach", meta = (ClampMin = "0.02"))
	float AerialApproachPlanRefreshInterval = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Approach", meta = (ClampMin = "0.0"))
	float AerialApproachPlanLossGraceTime = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Approach", meta = (ClampMin = "0.0"))
	float AerialApproachPlanFreezeBeforeStartTime = 0.05f;

	/* Disabled by default. Useful for testing a ball launched at one player. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Debug")
	bool bDebugAutoStartAerialAction = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Debug")
	ESoccerAerialActionIntent DebugAerialActionIntent =
		ESoccerAerialActionIntent::Automatic;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Debug", meta = (ClampMin = "0.02"))
	float AerialDebugAutoStartRefreshInterval = 0.10f;

	/* Aerial drawing enable moved to SoccerDebugManager > Aerial / Headers. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Bones")
	FName AerialHeadBoneName = FName(TEXT("Head"));

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Bones")
	FName AerialChestLowerBoneName = FName(TEXT("Spine2"));

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Bones")
	FName AerialChestUpperBoneName = FName(TEXT("Neck"));

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Detection", meta = (ClampMin = "1.0"))
	float AerialHeadContactRadius = 18.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Detection", meta = (ClampMin = "1.0"))
	float AerialChestContactRadius = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Detection", meta = (ClampMin = "0.0"))
	float AerialContactExtraTolerance = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Detection", meta = (ClampMin = "2", ClampMax = "16"))
	int32 AerialChestSweepTemporalSamples = 7;

	/*
	 * Legacy tuning value kept for Blueprint compatibility. The natural
	 * StandingControl split now uses the normalized ChestTarget->Head axis
	 * below, which is stable even when the two detection volumes overlap.
	 */
	/*
	 * Natural HEAD/CHEST separator for StandingControl. 0 is the animated
	 * chest target (Spine2->Neck at StandingAerialChestTrackAlpha) and 1 is
	 * the animated Head bone. Ball centers below this normalized boundary
	 * belong exclusively to CHEST; centers above it belong exclusively to
	 * HEAD. This prevents the large head sphere from stealing a chest ball
	 * and the broad chest capsule from stealing a head ball.
	 */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Detection", meta = (ClampMin = "0.25", ClampMax = "0.85"))
	float AerialStandingHeadZoneStartFromChestToHeadAlpha = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Active Header", meta = (ClampMin = "0.0"))
	float AerialActiveHeaderHorizontalSpeed = 1600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Active Header")
	float AerialActiveHeaderUpwardSpeed = 170.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Active Header", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AerialActiveHeaderMaximumRedirectAngle = 75.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Block", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float AerialBlockIncomingSpeedRetention = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Block", meta = (ClampMin = "0.0"))
	float AerialBlockMinimumHorizontalSpeed = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Block", meta = (ClampMin = "0.0"))
	float AerialBlockMaximumHorizontalSpeed = 1300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Block")
	float AerialBlockUpwardSpeed = 130.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Block", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AerialBlockMaximumRedirectAngle = 85.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Standing Control", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialChestHorizontalRetention = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Standing Control", meta = (ClampMin = "0.0"))
	float AerialChestMinimumHorizontalSpeed = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Standing Control", meta = (ClampMin = "0.0"))
	float AerialChestMaximumHorizontalSpeed = 240.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Standing Control")
	float AerialChestVerticalSpeed = -170.0f;

	/* CHEST is selected only when the predicted second ball remains safe. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Decision")
	bool bUseTacticalStandingControlSelection = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Decision", meta = (ClampMin = "0.0"))
	float AerialChestOpponentDangerDistance = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Decision", meta = (ClampMin = "-1.0", ClampMax = "2.0"))
	float AerialChestMinimumRecoveryAdvantage = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Decision", meta = (ClampMin = "0.0"))
	float AerialStandingControlRecoveryReachRadius = 105.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Decision", meta = (ClampMin = "0.0"))
	float AerialStandingControlOpponentReactionTime = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Decision", meta = (ClampMin = "0.2"))
	float AerialStandingControlMaximumRecoveryPredictionTime = 1.60f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Decision", meta = (ClampMin = "0.0"))
	float AerialStandingControlRecoveryRollTime = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Decision", meta = (ClampMin = "0.0"))
	float AerialStandingControlFieldSafetyMargin = 100.0f;

	/* Passive standing header: it redirects existing energy and never creates speed. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Header Redirect", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float AerialStandingHeaderMaximumRedirectAngle = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Header Redirect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialStandingHeaderStraightRetention = 0.68f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Header Redirect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialStandingHeaderMaximumAngleRetention = 0.36f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Header Redirect")
	float AerialStandingHeaderVerticalSpeed = -80.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Header Redirect", meta = (ClampMin = "100.0"))
	float AerialStandingHeaderTargetDistance = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Header Redirect", meta = (ClampMin = "5.0", ClampMax = "45.0"))
	float AerialStandingHeaderCandidateAngleStep = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Header Redirect", meta = (ClampMin = "-1.0", ClampMax = "2.0"))
	float AerialStandingHeaderMinimumRecoveryAdvantage = 0.05f;

	/*
	 * A standing passive header must release locomotion shortly after the
	 * actual ball contact. Waiting for the full header_chest montage would
	 * let nearby opponents attack the redirected second ball first.
	 */
	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Header Redirect|Recovery", meta = (ClampMin = "0.0", ClampMax = "0.50"))
	float AerialStandingHeaderPostContactReleaseDelay = 0.06f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Standing Control|Header Redirect|Recovery", meta = (ClampMin = "0.0", ClampMax = "0.50"))
	float AerialStandingHeaderPostContactBlendOutTime = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contact|Response", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialContactAngularVelocityRetention = 0.30f;

	// ============================================================
	// TACKLE - STAGE 1: SHARED PHYSICAL ACTION
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle")
	bool bEnableTackle = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Montages")
	UAnimMontage* TackleLeftLegMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Montages")
	UAnimMontage* TackleRightLegMontage = nullptr;

	// Stage 3 evasive jumps. Assign montages built from
	// running_jump_left_leg_in_place / running_jump_right_leg_in_place.
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Evasion|Montages")
	UAnimMontage* TackleEvasionLeftLegMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Evasion|Montages")
	UAnimMontage* TackleEvasionRightLegMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Evasion")
	bool bEnableTackleEvasion = true;

	// During this normalized montage interval, low tackle limb sweeps pass
	// underneath the evading player.
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Evasion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TackleEvasionAirborneStartNormalizedTime = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Evasion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TackleEvasionAirborneEndNormalizedTime = 0.72f;

	// In-place montage keeps most of the runner's world-space inertia.
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Evasion|Movement", meta = (ClampMin = "0.0", ClampMax = "1.25"))
	float TackleEvasionEndSpeedRetention = 0.88f;

	/*
	 * Optional normalized speed profile. X=montage normalized time, Y=speed
	 * multiplier. When empty, a built-in progressive deceleration is used.
	 */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Movement")
	UCurveFloat* TackleSpeedProfile = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Movement", meta = (ClampMin = "0.0"))
	float TackleInitialSpeedScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Movement", meta = (ClampMin = "0.0"))
	float TackleMinimumInitialSpeed = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Movement", meta = (ClampMin = "0.0"))
	float TackleMaximumInitialSpeed = 900.0f;

	/* Horizontal speed reaches zero by this normalized montage time. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Movement", meta = (ClampMin = "0.20", ClampMax = "1.0"))
	float TackleMovementStopNormalizedTime = 0.90f;

	/* Recovery begins while the character can still be sliding slowly. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Movement", meta = (ClampMin = "0.20", ClampMax = "1.0"))
	float TackleRecoveryStartNormalizedTime = 0.74f;

	/* Fallback profile exponent. Values below 1 retain speed longer. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Movement", meta = (ClampMin = "0.15", ClampMax = "3.0"))
	float TackleFallbackDecelerationExponent = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Movement", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float TackleMaximumStartTurnAngle = 80.0f;

	// ============================================================
	// TACKLE - STAGE 2: TEMPORAL LIMB CONTACT
	// ============================================================

	/* Only contacts inside this montage interval are challenge contacts. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Contact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TackleContactWindowStartNormalizedTime = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Contact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TackleContactWindowEndNormalizedTime = 0.74f;

	/* Radius around each sampled foot/shin point used for temporal sweeps. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Contact", meta = (ClampMin = "1.0", ClampMax = "40.0"))
	float TackleFootContactRadius = 13.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Contact", meta = (ClampMin = "1.0", ClampMax = "40.0"))
	float TackleLowerLegContactRadius = 11.0f;

	/* Chronology inside this tolerance is stored as NearlySimultaneous. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Contact", meta = (ClampMin = "0.0", ClampMax = "0.20"))
	float TackleNearlySimultaneousNormalizedTolerance = 0.025f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Contact|Bones")
	FName TackleLeftFootBoneName = FName(TEXT("LeftFoot"));

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Contact|Bones")
	FName TackleRightFootBoneName = FName(TEXT("RightFoot"));

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Contact|Bones")
	FName TackleLeftLowerLegBoneName = FName(TEXT("LeftLeg"));

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Contact|Bones")
	FName TackleRightLowerLegBoneName = FName(TEXT("RightLeg"));

	// ============================================================
	// TACKLE - STAGE 3: BALL RESPONSE
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Ball Response")
	bool bEnableTackleBallResponse = true;

	/* Portion of the current slide speed transferred into the ball. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Ball Response", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float TackleBallSlideSpeedTransfer = 1.35f;

	/* Keeps some of the ball's pre-contact horizontal motion. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Ball Response", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TackleBallIncomingHorizontalRetention = 0.28f;

	/* Keeps some existing vertical motion instead of flattening every interception. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Ball Response", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TackleBallIncomingVerticalRetention = 0.35f;

	/* Small lift generated by a foot sliding underneath/through the ball. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Ball Response", meta = (ClampMin = "0.0", ClampMax = "600.0"))
	float TackleBallFootUpwardSpeed = 85.0f;

	/* Shin contacts are flatter than foot contacts. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Ball Response", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TackleBallLowerLegUpwardMultiplier = 0.45f;

	/* Blend from the slide direction toward the actual ball bearing at contact. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Ball Response", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TackleBallContactDirectionInfluence = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Ball Response", meta = (ClampMin = "0.0"))
	float TackleBallMinimumHorizontalSpeed = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Ball Response", meta = (ClampMin = "0.0"))
	float TackleBallMaximumHorizontalSpeed = 1900.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Ball Response", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TackleBallAngularVelocityRetention = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Contact", meta = (ClampMin = "0.0"))
	float TackleContactDebugDuration = 0.12f;

	// ============================================================
	// TACKLE - STAGE 4: VICTIM FALL REACTION
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Fall Reaction")
	bool bEnableTackleFallReaction = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Fall Reaction|Montages")
	UAnimMontage* TackleLeftFallMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Fall Reaction|Montages")
	UAnimMontage* TackleRightFallMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Fall Reaction|Movement", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float TackleFallInitialInertiaScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Fall Reaction|Movement", meta = (ClampMin = "0.0"))
	float TackleFallMaximumInitialInertiaSpeed = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Fall Reaction|Movement", meta = (ClampMin = "0.20", ClampMax = "1.0"))
	float TackleFallRecoveryStartNormalizedTime = 0.70f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Fall Reaction|Movement", meta = (ClampMin = "0.20", ClampMax = "1.0"))
	float TackleFallMovementStopNormalizedTime = 0.92f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tackle|Fall Reaction|Movement", meta = (ClampMin = "0.15", ClampMax = "3.0"))
	float TackleFallDecelerationExponent = 0.70f;

	// ============================================================
	// AERIAL CONTESTS - STAGE 4
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "0.0"))
	float AerialContestSpatialWeight = 0.48f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "0.0"))
	float AerialContestTimingWeight = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "0.0"))
	float AerialContestFacingWeight = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "0.0"))
	float AerialContestSpeedWeight = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "0.0"))
	float AerialContestAirborneWeight = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "0.0"))
	float AerialComfortableIncomingSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "1.0"))
	float AerialExtremeIncomingSpeed = 3200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialExtremeSpeedMinimumQuality = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "0.0"))
	float AerialContestActiveHeaderBonus = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "0.0"))
	float AerialContestBlockHeaderBonus = 0.03f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Scoring", meta = (ClampMin = "0.0"))
	float AerialContestTieTolerance = 0.0025f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Pressure", meta = (ClampMin = "1.0"))
	float AerialContestPressureRadius = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Pressure", meta = (ClampMin = "0.0"))
	float AerialContestPressureVerticalTolerance = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Pressure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialContestActiveOpponentPressureBonus = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Quality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialContestPressureQualityPenalty = 0.38f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Quality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialMinimumEffectiveContactQuality = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Quality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialPoorControlHorizontalRetention = 0.48f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Quality")
	float AerialPoorControlVerticalSpeed = -35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Quality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialPoorHeaderPowerScale = 0.62f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Body", meta = (ClampMin = "1.0"))
	float AerialBodyContestRadius = 92.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Body", meta = (ClampMin = "0.0"))
	float AerialBodyContestHorizontalImpulse = 190.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Body", meta = (ClampMin = "0.0"))
	float AerialBodyContestUpwardImpulse = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Body", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialProtectedPlayerReactionScale = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Aerial|Contest|Body", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AerialSimultaneousBodyReactionScale = 0.60f;

private:
	/**
	 * Optional immutable player definition. Formation slot and current fatigue
	 * intentionally do not live in this asset. If null, legacy gameplay values
	 * are preserved exactly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer|Player Profile", meta = (AllowPrivateAccess = "true"))
		USoccerPlayerProfile* PlayerProfile = nullptr;

	// A profile value of 50 maps to the legacy/default behavior with the
	// default endpoints below. These endpoints are deliberately class-default
	// tuning so balance can change without editing every player Data Asset.
	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Physical Tuning", meta = (ClampMin = "0.10", ClampMax = "3.00"))
		float PaceSpeedMultiplierAtZero = 0.80f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Physical Tuning", meta = (ClampMin = "0.10", ClampMax = "3.00"))
		float PaceSpeedMultiplierAtHundred = 1.20f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Physical Tuning", meta = (ClampMin = "0.10", ClampMax = "3.00"))
		float AccelerationMultiplierAtZero = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Physical Tuning", meta = (ClampMin = "0.10", ClampMax = "3.00"))
		float AccelerationMultiplierAtHundred = 1.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Physical Tuning", meta = (ClampMin = "0.10", ClampMax = "3.00"))
		float StaminaDrainMultiplierAtZero = 1.50f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Physical Tuning", meta = (ClampMin = "0.10", ClampMax = "3.00"))
		float StaminaDrainMultiplierAtHundred = 0.50f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Physical Tuning", meta = (ClampMin = "0.10", ClampMax = "3.00"))
		float StaminaRecoveryMultiplierAtZero = 0.50f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Physical Tuning", meta = (ClampMin = "0.10", ClampMax = "3.00"))
		float StaminaRecoveryMultiplierAtHundred = 1.50f;

	// Technical attributes are intentionally tuned here instead of in each player
	// Data Asset. This lets global balance change without rewriting every profile.
	// Characters with no PlayerProfile keep the exact legacy deterministic kick.
	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.0", ClampMax = "30.0"))
		float PassingMaxAngularErrorDegreesAtZero = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.0", ClampMax = "30.0"))
		float PassingMaxAngularErrorDegreesAtHundred = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.0", ClampMax = "30.0"))
		float ShootingMaxAngularErrorDegreesAtZero = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.0", ClampMax = "30.0"))
		float ShootingMaxAngularErrorDegreesAtHundred = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.0", ClampMax = "0.50"))
		float PassingSpeedVariationAtZero = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.0", ClampMax = "0.50"))
		float PassingSpeedVariationAtHundred = 0.01f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.10", ClampMax = "3.00"))
		float ShotPowerMultiplierAtZero = 0.80f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.10", ClampMax = "3.00"))
		float ShotPowerMultiplierAtHundred = 1.20f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "50.0", ClampMax = "1500.0"))
		float TechnicalPressureRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.10", ClampMax = "5.00"))
		float TechnicalPressureWeightForFullPressure = 1.20f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "1.0", ClampMax = "4.0"))
		float ComposurePressureErrorMultiplierAtZero = 1.60f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "1.0", ClampMax = "4.0"))
		float ComposurePressureErrorMultiplierAtHundred = 1.00f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.10", ClampMax = "1.00"))
		float ComposureShotPowerRetentionAtZero = 0.86f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "0.10", ClampMax = "1.00"))
		float ComposureShotPowerRetentionAtHundred = 1.00f;

	UPROPERTY(EditDefaultsOnly, Category = "Soccer|Player Profile|Technical Tuning", meta = (ClampMin = "100.0", ClampMax = "2500.0"))
		float ShotTargetRecognitionRadius = 600.0f;

	// Runtime baseline makes profile application idempotent. Without this, changing
	// a profile after BeginPlay would multiply an already adjusted acceleration.
	bool bPlayerProfileAccelerationBaselineCaptured = false;
	float PlayerProfileBaselineMaxAcceleration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer|Team", meta = (AllowPrivateAccess = "true"))
		ESoccerTeam Team = ESoccerTeam::PlayerTeam;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer|Team", meta = (AllowPrivateAccess = "true"))
		ESoccerPlayerRole PlayerRole = ESoccerPlayerRole::Midfielder;

	UPROPERTY(EditAnywhere, Category = "Soccer|Uniform")
		int32 ShirtMaterialIndex = 1;

	UPROPERTY(EditAnywhere, Category = "Soccer|Uniform")
		int32 ShortsMaterialIndex = 2;

	UPROPERTY(EditAnywhere, Category = "Soccer|Uniform")
		UMaterialInterface* PlayerTeamShirtMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Uniform")
		UMaterialInterface* PlayerTeamShortsMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Uniform")
		UMaterialInterface* OpponentTeamShirtMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Uniform")
		UMaterialInterface* OpponentTeamShortsMaterial = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Animation", meta = (AllowPrivateAccess = "true"))
		bool bSoccerIsPossessingBall = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Animation", meta = (AllowPrivateAccess = "true"))
		bool bSoccerIsChasingBall = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Animation", meta = (AllowPrivateAccess = "true"))
		bool bSoccerIsKicking = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Animation", meta = (AllowPrivateAccess = "true"))
		float SoccerEnergyPercent = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Animation", meta = (AllowPrivateAccess = "true"))
		bool bSoccerShouldForceDribbleTurnLocomotion = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Animation", meta = (AllowPrivateAccess = "true"))
		float SoccerForcedDribbleTurnLocomotionSpeed = 0.0f;

	TWeakObjectPtr<ASoccerBall> CachedDebugInterceptionBall;
	float InterceptionDebugUpdateAccumulator = 0.0f;

	TWeakObjectPtr<ASoccerBall> StableGroundBallPursuitBall;
	bool bHasStableGroundBallPursuitTarget = false;
	FVector StableGroundBallPursuitLocation = FVector::ZeroVector;
	FSoccerBallInterceptionResult StableGroundBallPursuitResult;
	float StableGroundBallPursuitSelectionTime = -1000.0f;
	float StableGroundBallPursuitLastRefreshTime = -1000.0f;
	int32 StableBallPursuitTrajectoryRevision = 0;

	ESoccerTacklePhase TacklePhase = ESoccerTacklePhase::Inactive;
	ESoccerTackleSide ActiveTackleSide = ESoccerTackleSide::LeftLeg;
	FVector ActiveTackleDirection = FVector::ForwardVector;
	float ActiveTackleInitialSpeed = 0.0f;
	float ActiveTackleNormalizedTime = 0.0f;
	float SavedTackleGroundFriction = 0.0f;
	float SavedTackleBrakingDecelerationWalking = 0.0f;
	float SavedTackleMaxWalkSpeed = 0.0f;
	bool bTackleMovementSettingsSaved = false;

	bool bTackleContactTrackingInitialized = false;
FVector PreviousTackleBallLocation = FVector::ZeroVector;
	FVector PreviousTackleLeftFootLocation = FVector::ZeroVector;
	FVector PreviousTackleRightFootLocation = FVector::ZeroVector;
	FVector PreviousTackleLeftLowerLegLocation = FVector::ZeroVector;
	FVector PreviousTackleRightLowerLegLocation = FVector::ZeroVector;
	FSoccerTackleContactSummary CurrentTackleContactSummary;
	FSoccerTackleContactSummary LastTackleContactSummary;
	TWeakObjectPtr<ASoccerCharacterBase> CurrentFirstTackleOpponentContactCharacter;
	bool bTackleFoulEvaluationSubmitted = false;

	ESoccerTackleFallPhase TackleFallPhase = ESoccerTackleFallPhase::Inactive;
	ESoccerTackleFallSide ActiveTackleFallSide = ESoccerTackleFallSide::Left;
	FVector ActiveTackleFallInertiaDirection = FVector::ZeroVector;
	float ActiveTackleFallInitialSpeed = 0.0f;
	float ActiveTackleFallNormalizedTime = 0.0f;
	float SavedTackleFallGroundFriction = 0.0f;
	float SavedTackleFallBrakingDecelerationWalking = 0.0f;
	float SavedTackleFallMaxWalkSpeed = 0.0f;
	bool bTackleFallMovementSettingsSaved = false;
	bool bSavedTackleFallIgnoreMoveInput = false;
	bool bSavedTackleFallIgnoreMoveInputValid = false;

	bool bTackleEvasionActive = false;
	ESoccerTackleEvasionSide ActiveTackleEvasionSide =
		ESoccerTackleEvasionSide::Left;
	float ActiveTackleEvasionNormalizedTime = 0.0f;
	FVector ActiveTackleEvasionDirection = FVector::ForwardVector;
	float ActiveTackleEvasionInitialSpeed = 0.0f;
	float SavedTackleEvasionGroundFriction = 0.0f;
	float SavedTackleEvasionBrakingDecelerationWalking = 0.0f;
	float SavedTackleEvasionMaxWalkSpeed = 0.0f;
	bool bTackleEvasionMovementSettingsSaved = false;
	bool bSavedTackleEvasionIgnoreMoveInput = false;
	bool bSavedTackleEvasionIgnoreMoveInputValid = false;

	ESoccerAerialActionPhase AerialActionPhase =
		ESoccerAerialActionPhase::None;
	ESoccerAerialActionType ActiveAerialActionType =
		ESoccerAerialActionType::None;
	FSoccerAerialInterceptionPlan QueuedAerialPlan;
	ESoccerAerialActionIntent QueuedAerialIntent =
		ESoccerAerialActionIntent::Automatic;
	float QueuedAerialStartWorldTime = 0.0f;
	float ActiveAerialExpectedEndWorldTime = 0.0f;
	float AerialApproachPlanRefreshAccumulator = 0.0f;
	float AerialApproachPlanLossAccumulator = 0.0f;
	TWeakObjectPtr<ASoccerBall> LastCompletedAerialActionBall;
	int32 LastCompletedAerialTrajectoryRevision = INDEX_NONE;
	float AerialDebugAutoStartAccumulator = 0.0f;
	TEnumAsByte<EMovementMode> SavedAerialMovementMode = MOVE_Walking;
	uint8 SavedAerialCustomMovementMode = 0;
	bool bAerialMovementWasLocked = false;
	bool bStandingHeaderPostContactReleasePending = false;
	float StandingHeaderPostContactReleaseWorldTime = 0.0f;
	bool bAerialContactTrackingInitialized = false;
	bool bAerialContactResolved = false;
	float PreviousAerialMontagePosition = 0.0f;
	FVector PreviousAerialBallLocation = FVector::ZeroVector;
	FVector PreviousAerialHeadLocation = FVector::ZeroVector;
	FVector PreviousAerialChestLowerLocation = FVector::ZeroVector;
	FVector PreviousAerialChestUpperLocation = FVector::ZeroVector;
	int32 ActiveAerialContactTrajectoryRevision = INDEX_NONE;
	FVector PendingAerialContestExitImpulse = FVector::ZeroVector;
	TSet<TWeakObjectPtr<ASoccerCharacterBase>> AerialBodyContactProcessedOpponents;
bool bAerialDebugPlanLocked = false;
	ESoccerAerialContactSurface DebugForcedAerialContactSurface =
		ESoccerAerialContactSurface::None;
	ESoccerAerialContactSurface DebugStandingControlPlanningSurface =
		ESoccerAerialContactSurface::None;
	FSoccerAerialContactResult LastAerialContactResult;
	float LastAerialContactWorldTime = -1000.0f;

	/* Exact timeline used by SoccerAerialChestDebugTester to isolate late montage starts. */
	bool bAerialDebugScheduledStartObserved = false;
	float AerialDebugDistanceAtScheduledStart = -1.0f;
	float AerialDebugEnteredMaximumCommitRadiusWorldTime = -1.0f;
	float AerialDebugEnteredPreparationToleranceWorldTime = -1.0f;
	bool bAerialDebugScheduledCorrectionAttempted = false;
	float AerialDebugScheduledCorrectionDistanceBefore = -1.0f;
	float AerialDebugScheduledCorrectionDistanceAfter = -1.0f;
	float AerialDebugMontagePlayRequestedWorldTime = -1.0f;
	float AerialDebugMontagePlayReturnedDuration = 0.0f;
	float AerialDebugMontageStartPosition = 0.0f;
	float AerialDebugMontageLogicalStartWorldTime = -1.0f;
	float AerialDebugMontageBecameActiveWorldTime = -1.0f;
};
