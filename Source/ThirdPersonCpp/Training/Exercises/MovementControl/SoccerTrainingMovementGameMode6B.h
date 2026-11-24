#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SoccerTrainingMovementGameMode6B.generated.h"

class ASoccerBall;
class ASoccerField;
class AThirdPersonCppCharacter;
class USoccerPlayerProfile;
class AStaticMeshActor;
class UPrimitiveComponent;
struct FHitResult;
class ASoccerAICharacter;
enum class ESoccerGoalkeeperAction : uint8;

// 6P: keep the cause of each shot result separate from the goal counter.
enum class ETrainingShotOutcome6P : uint8
{
    Goal, Wide, OverBar, Barrier, Unresolved, Saved, Post, Crossbar, Intercepted, InvalidZone
};

/** Standalone first dribbling lesson: guide the ball through four checkpoints. */
UCLASS()
class THIRDPERSONCPP_API ASoccerTrainingMovementGameMode6B : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASoccerTrainingMovementGameMode6B();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // 6H: pre-start instructions, active time trial and finish summary.
    UFUNCTION(BlueprintCallable, Category="Training|Exercise")
    void BeginExercise6H();
    bool IsAutoPassLesson6J() const { return bAutoPassLesson6J; }
    bool IsChargedShotLesson6M() const { return bChargedShotLesson6M; }
    bool IsGoalkeeperShotLesson6T() const { return bGoalkeeperShotLesson6T; }
    bool IsZoneFinishLesson6AH() const { return bZoneFinishLesson6AH; }
    bool IsAvoidTackleLesson6AK() const { return bAvoidTackleLesson6AK; }
    bool IsWinTackleLesson6AK() const { return bWinTackleLesson6AK; }
    bool IsTackleLesson6AK() const { return bAvoidTackleLesson6AK || bWinTackleLesson6AK; }
    bool IsBallInsideShotZone6AH() const;
    bool CanStartZoneShot6AH() const;
    void ReportInvalidZoneShot6AH();
    FVector GetZoneCenterWorld6AH() const;
    float GetZoneRadius6AH() const { return ZoneRadius6AH; }
    bool HasShotBeenReleased6M() const { return bShotReleased6M; }
    bool WasGoalScored6M() const { return bGoalScored6M; }
    float GetShotCharge6M() const { return ShotCharge6M; }
    int32 GetShotAttempt6N() const { return FMath::Clamp(ShotAttemptIndex6N + 1, 1, 10); }
    int32 GetShotsCompleted6N() const { return ShotsCompleted6N; }
    int32 GetGoalsScored6N() const { return GoalsScored6N; }
    int32 GetShotAttemptsTotal6N() const { return 10; }
    FString GetLastShotFeedback6P() const;
    FString GetShotResultsReport6P() const;
    FString GetAvoidTackleReport6AL2() const;
    bool IsBetweenShotAttempts6N() const { return bBetweenShotAttempts6N; }
    void RegisterChargedShotRelease6M(float ChargeFraction);
    // Called only after an autopass click while the player possesses the ball.
    void RegisterAutoPassClick6J();
    bool IsExerciseRunning6H() const { return bExerciseStarted6H && !bComplete6B; }
    bool HasExerciseStarted6H() const { return bExerciseStarted6H; }
    bool IsTimeEvaluationEnabled6H() const { return bEnableTimeEvaluation6H; }
    float GetApprovalTime6H() const { return ApprovalTimeSeconds6H; }
    int32 GetCheckpointIndex6B() const { return CheckpointIndex6B; }
    int32 GetCheckpointCount6B() const { return 4; }
    float GetElapsedTime6B() const { return ElapsedTime6B; }
    int32 GetLostControlCount6B() const { return LostControlCount6B; }
    bool IsExerciseComplete6B() const { return bComplete6B; }
    FString GetControlDiagnostics6D() const;
    FString GetSpeedDiagnostics6E() const;
    FString GetBallDiagnostics6E() const;
    FVector GetCurrentCheckpoint6B() const;
    FString GetExerciseMessage6B() const;

private:
    // Optional: select the SAME player-profile Data Asset used by the human
    // in a match. No synthetic speed or ball-control coefficients are applied.
    UPROPERTY(EditAnywhere, Category="Training|Movement Control|Parity")
    TSoftObjectPtr<USoccerPlayerProfile> TrainingPlayerProfile6D;

    // The 6F match log identified this profile for the controlled human. If
    // no asset is chosen explicitly, use it for this parity test.
    UPROPERTY(EditAnywhere, Category="Training|Movement Control|Parity")
    bool bUseRecordedMatchProfile6G = true;

    // Effective values from the MATCH runtime snapshot (2026-11-21).
    // Applied only to the spawned training ball; the Blueprint remains intact.
    UPROPERTY(EditAnywhere, Category="Training|Movement Control|Parity", meta=(ClampMin="0.1", ClampMax="2.0"))
    float TrainingHorizontalLaunchScale6G = 1.0f;

    UPROPERTY(EditAnywhere, Category="Training|Movement Control|Parity", meta=(ClampMin="0.0"))
    float TrainingAirLinearDamping6G = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category="Training|Movement Control|6B")
    bool bGenerateTestGround6B = true;
    UPROPERTY(EditDefaultsOnly, Category="Training|Movement Control|6B")
    bool bSpawnPracticeBall6B = true;
    UPROPERTY(EditDefaultsOnly, Category="Training|Movement Control|6B", meta=(ClampMin="75.0"))
    float CheckpointRadius6B = 150.f;
    UPROPERTY(EditDefaultsOnly, Category="Training|Movement Control|6B", meta=(ClampMin="100.0"))
    float MaximumControlDistance6B = 260.f;

    // Only evaluate pass/fail once the target has been agreed upon.
    UPROPERTY(EditAnywhere, Category="Training|Movement Control|Evaluation")
    bool bEnableTimeEvaluation6H = false;
    UPROPERTY(EditAnywhere, Category="Training|Movement Control|Evaluation", meta=(ClampMin="1.0", EditCondition="bEnableTimeEvaluation6H"))
    float ApprovalTimeSeconds6H = 60.f;

    bool bAutoPassLesson6J = false;
    bool bChargedShotLesson6M = false;
    bool bGoalkeeperShotLesson6T = false;
    bool bZoneFinishLesson6AH = false;
    bool bAvoidTackleLesson6AK = false;
    bool bWinTackleLesson6AK = false;
    // 6AK only: spawn a training opponent from an existing footballer BP.
    // Do not enable the regular match AI in the standalone training world.
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AK")
    TSubclassOf<ASoccerAICharacter> TackleOpponentClass6AK;
    UPROPERTY(Transient) ASoccerAICharacter* TackleOpponent6AK = nullptr;
    // 6AL.6: three real field-player BPs, one in each of zones 1, 2 and 3.
    // TackleOpponent6AK remains an alias ONLY to the currently active rival;
    // the existing 6AL.5.2 tackle/steal/open-play-clearance logic is reused.
    UPROPERTY(Transient) TArray<ASoccerAICharacter*> AvoidZoneDefenders6AL6;
    int32 ActiveAvoidZone6AL6 = -1;
    int32 HighestAvoidZone6AL6 = 0;
    float LastInactiveDefenderHomeAt6AL6 = -100.f;
    float LastZoneLinesAt6AL6 = -100.f;
    // 6AL.6.2: coverage movements are independent of the one active tackler.
    // X stays at each zone's defensive station; Y follows the ball-goal line.
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL6.2", meta=(ClampMin="0.0", ClampMax="350.0"))
    float AvoidCoverLateralMargin6AL62 = 95.f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL6.2", meta=(ClampMin="40.0", ClampMax="300.0"))
    float AvoidCoverRepathThreshold6AL62 = 120.f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL6.2", meta=(ClampMin="0.25", ClampMax="2.0"))
    float AvoidCoverUpdateSeconds6AL62 = 0.65f;
    TArray<FVector> AvoidCoverLastTarget6AL62;
    TArray<float> AvoidCoverLastOrderAt6AL62;
    FVector GetAvoidDefenderHomeLocal6AL6(int32 Zone) const;
    FVector GetAvoidDefenderCoverLocal6AL62(int32 Zone, const FVector& BallWorld) const;
    int32 GetBallZone6AL6(const FVector& BallWorld) const;
    void SetActiveAvoidZone6AL6(int32 Zone);
    void UpdateInactiveAvoidDefenders6AL6(float Now);
    void DrawAvoidZones6AL6(float Now);
    void EnsureTackleOpponent6AK();
    void PositionTackleScenario6AK();
    // 6AL: training-only use of the exact shared tackle and running-jump actions.
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL", meta=(ClampMin="200.0", ClampMax="900.0"))
    float AvoidTackleTriggerDistance6AL = 440.f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL", meta=(ClampMin="0.0", ClampMax="600.0"))
    float AvoidTackleMinimumRunnerSpeed6AL = 100.f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL", meta=(ClampMin="1.0", ClampMax="20.0"))
    float AvoidTackleResolveSeconds6AL = 4.5f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL", meta=(ClampMin="0.5", ClampMax="10.0"))
    float AvoidTackleResetDelay6AL = 2.f;
    int32 AvoidTackleAttempt6AL = 0;
    int32 AvoidTackleSuccesses6AL = 0;
    bool bAvoidTackleLaunched6AL = false;
    bool bAvoidTackleJumpSeen6AL = false;
    bool bAvoidTackleFallSeen6AL = false;
    bool bAvoidTackleWaitingReset6AL = false;
    float AvoidTackleLaunchedAt6AL = -1.f;
    float AvoidTackleResetAt6AL = -1.f;
    FString AvoidTackleFeedback6AL;
    // 6AL.2: keep an open defensive duel until a real on-field outcome.
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL2", meta=(ClampMin="0.1", ClampMax="2.0"))
    float AvoidDefenderRepathSeconds6AL2 = 0.35f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL2", meta=(ClampMin="100.0", ClampMax="700.0"))
    float AvoidDefenderTackleBallDistance6AL2 = 330.f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL2", meta=(ClampMin="0.5", ClampMax="6.0"))
    float AvoidDefenderTackleCooldown6AL2 = 1.7f;
    // 6AL.6.4: same carried-ball prediction used by match AI, with a lead
    // time matched to this exercise's actual slide travel time. These only
    // affect "Esquivar tackles", never player speeds or match parameters.
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL64 Prediction", meta=(ClampMin="350.0", ClampMax="1500.0"))
    float AvoidTackleEstimatedSlideSpeed6AL64 = 650.f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL64 Prediction", meta=(ClampMin="0.0", ClampMax="0.25"))
    float AvoidTackleLaunchDelay6AL64 = 0.03f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL64 Prediction", meta=(ClampMin="0.02", ClampMax="0.35"))
    float AvoidTackleMinLeadSeconds6AL64 = 0.12f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL64 Prediction", meta=(ClampMin="0.15", ClampMax="1.0"))
    float AvoidTackleMaxLeadSeconds6AL64 = 0.65f;
    UPROPERTY(EditAnywhere, Category="Training|Tackle|6AL2", meta=(ClampMin="150.0", ClampMax="1500.0"))
    float AvoidDefenderClearanceSpeed6AL2 = 1850.f;
    float AvoidDefenderLastMoveAt6AL2 = -100.f;
    float AvoidDefenderLastTackleAt6AL2 = -100.f;
    float AvoidDefenderLastClearanceAt6AL2 = -100.f;
    bool bAvoidDefenderWasTackling6AL2 = false;
    // 6AL.3: avoid repeated release attempts during the same tackle.
    bool bAvoidDefenderBallDislodgedThisTackle6AL3 = false;

    // 6AL.4: only an authored montage can initiate training clearance;
    // a short clearance returns the defender to a genuine second possession.
    bool bAvoidDefenderClearing6AL2 = false;
    bool bAvoidDefenderKickImpactLogged6AL4 = false;
    // 6AL.5: an attempted kick is distinguished from a real foot contact.
    int32 AvoidDefenderKickMisses6AL5 = 0;
    // 6AL.5.1: throttle navigation/alignment diagnostics near a loose ball.
    float AvoidDefenderLastKickApproachLog6AL51 = -100.f;

    bool bAvoidDefenderFoulSeen6AL2 = false;
    int32 AvoidDefenderTackles6AL2 = 0;
    int32 AvoidDefenderJumps6AL2 = 0;
    int32 AvoidDefenderFouls6AL2 = 0;
    int32 AvoidDefenderRecoveries6AL2 = 0;
    int32 AvoidDefenderOuts6AL2 = 0;
    int32 AvoidDefenderAttemptsFinished6AL2 = 0;
    TArray<FString> AvoidDefenderAttemptResults6AL2;
    void FinishAvoidTackleAttempt6AL2(const FString& Outcome, bool bHumanSuccess);
    void TickAvoidTackle6AL(float DeltaSeconds);
    void ResetAvoidTackle6AL();
    // 6AI only: a goalkeeper can rush out BEFORE a zone shot is released.
    // Do not enable in the legacy 6AG goalkeeper-shot exercise.
    bool bKeeperInterceptionThisAttempt6AI = false;
    UPROPERTY(EditAnywhere, Category="Training|Zone Finish|Target", meta=(ClampMin="100.0", ClampMax="800.0"))
    float ZoneRadius6AH = 260.f;
    float LastZoneWarningAt6AH = -100.f;
    // Fixed, field-relative targets so each of the ten attempts can be reproduced.
    FVector GetZoneCenterLocal6AH(int32 AttemptIndex) const;
    void DrawZoneFinishTarget6AH() const;
    // 6Z.3: Optional Data Asset for the goalkeeper in the training level.
    // Leave unset to preserve a profile already assigned by another system.
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Profile")
    TSoftObjectPtr<USoccerPlayerProfile> TrainingGoalkeeperProfile6Z3;

    UPROPERTY(Transient) ASoccerAICharacter* TrainingGoalkeeper6T = nullptr;
    float LastPositionDebug6AA = -100.f;
    // 6AA.2: avoid restarting path following every frame.
    FVector LastIssuedKeeperTarget6AA2 = FVector::ZeroVector;
    float LastKeeperMoveRequestAt6AA2 = -100.f;
    bool bHasIssuedKeeperTarget6AA2 = false;
    // 6V: training-only goalkeeper positioning. These mirror the geometric
    // principles of SoccerMatchManager, not its runtime Blueprint overrides.
    // Tune them here if the match uses different effective values.
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperHomeDepth6V = 150.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperMaxDepth6V = 820.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperFullDepthDistance6V = 3000.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperLateralRange6V = 230.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperPostMargin6V = 70.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperFullCoverageAngle6V = 55.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperAngleResponseExponent6V = 0.85f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperNearFollowDepth6V = 700.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperFarFollowDepth6V = 2200.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning", meta=(ClampMin="0.0", ClampMax="1.0"))
    float KeeperFarFollowAlpha6V = 0.8f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperForwardSpeed6V = 320.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Positioning")
    float KeeperLateralSpeed6V = 540.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Prediction")
    float KeeperPredictionMinForwardSpeed6V = 180.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Prediction")
    float KeeperPredictionHorizon6V = 1.2f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Prediction", meta=(ClampMin="0.0", ClampMax="1.0"))
    float KeeperTrajectoryFollowAlpha6V = 0.85f;
    // 6W: training-only contact and recovery tuning. Default reach is modest;
    // it does not modify the goalkeeper used in regular matches.
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Saves", meta=(ClampMin="0.0"))
    float KeeperContactReach6W = 85.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Saves", meta=(ClampMin="50.0"))
    float KeeperContactMaxHeight6W = 190.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Saves", meta=(ClampMin="0.0"))
    float KeeperMinimumShotSpeed6W = 250.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Saves", meta=(ClampMin="0.0"))
    float KeeperMinimumForwardSpeed6W = 180.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Saves")
    float KeeperLowShotHeight6W = 65.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Saves")
    float KeeperHighShotHeight6W = 155.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Saves", meta=(ClampMin="0.0"))
    float KeeperCentralSaveDistance6W = 40.f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Saves", meta=(ClampMin="1.5"))
    float KeeperSaveRecoverySeconds6W = 2.25f;
    // 6Y: continuously refreshed plan using the MATCH selector and its
    // authored montage contact timing. The regular match AI remains disabled.
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Timing", meta=(ClampMin="0.0", ClampMax="0.2"))
    float KeeperStartTolerance6Y = 0.025f;
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Timing", meta=(ClampMin="0.0", ClampMax="2.0"))
    float KeeperPlanLogInterval6Y = 0.30f;
    // Don't alter the keeper's location in the final moments before a planned
    // montage, because the montage selector scored the keeper's current pose.
    UPROPERTY(EditAnywhere, Category="Training|Goalkeeper|Timing", meta=(ClampMin="0.0", ClampMax="0.5"))
    float KeeperPreMontageFreezeSeconds6Y = 0.10f;
    bool bKeeperHasPendingPlan6Y = false;
    ESoccerGoalkeeperAction KeeperPlannedAction6Y = static_cast<ESoccerGoalkeeperAction>(0);
    float KeeperPlannedDelay6Y = 0.f;
    float KeeperNextPlanLogAt6Y = 0.f;
    // 6X: real match montage selector, independent of the training collision approximation.
    bool bKeeperMontageStarted6X = false;
    bool bKeeperSelectorLogged6X = false;
    bool bGoalkeeperPrepared6T = false;
    bool bKeeperSaveTriggered6T = false;
    // 6N: ten controlled shot attempts using the existing match kick mechanic.
    int32 ShotAttemptIndex6N = 0;  // zero-based current attempt
    int32 ShotsCompleted6N = 0;
    int32 GoalsScored6N = 0;
    // 6O: practice barriers are temporary actors owned by the shooting exercise.
    UPROPERTY(Transient) TArray<AStaticMeshActor*> PracticeBarriers6O;
    UPROPERTY(EditAnywhere, Category="Training|Charged Shot|Barriers", meta=(ClampMin="80.0", ClampMax="300.0"))
    float BarrierHeightCm6O = 175.f;
    UPROPERTY(EditAnywhere, Category="Training|Charged Shot|Barriers", meta=(ClampMin="120.0", ClampMax="600.0"))
    float BarrierWidthCm6O = 300.f;
    // Results are reset when the exercise starts. A barrier contact is
    // registered independently, even when the shot later reaches the goal.
    TArray<ETrainingShotOutcome6P> ShotOutcomes6P;
    // 6AI.4: preserve the on-field result of a shot from outside the circle.
    TArray<ETrainingShotOutcome6P> ShotPhysicalOutcomes6AI4;
    bool bZoneShotOriginChecked6AI4 = false;
    bool bZoneShotOriginValid6AI4 = false;
    // 6AJ: record the full ten-attempt journey independently of the shot result.
    // None of these fields affect the goalkeeper, kick, ball or match code.
    bool bReachedShotZoneThisAttempt6AJ = false;
    float ZoneAttemptStartSeconds6AJ = 0.f;
    TArray<bool> ShotZoneReached6AJ;
    TArray<float> ShotZoneAttemptSeconds6AJ;
    TArray<bool> ShotOriginInside6AJ;
    TArray<bool> ShotHadKick6AJ;
    TArray<float> ShotCharges6P;
    TArray<bool> ShotBarrierContacts6P;
    // 6AE: explanatory evidence alongside each outcome (training only).
    TArray<FString> ShotEndDetails6AE;
    // 6AF: a goalkeeper touch / frame impact is evidence, not yet a final result.
    TArray<bool> ShotGoalkeeperTouches6AF;
    TArray<bool> ShotGoalsAfterKeeperTouch6AF;
    // 6AG: preserve actual frame contacts for each completed attempt.
    // A goal after a frame impact remains a goal, not a separate attempt.
    TArray<bool> ShotPostTouches6AG;
    TArray<bool> ShotCrossbarTouches6AG;
    bool bKeeperTouchedThisShot6AF = false;
    float KeeperTouchTime6AF = -1.f;
    bool bPostTouchedThisShot6AF = false;
    bool bCrossbarTouchedThisShot6AF = false;
    bool bTrackFrameImpact6AF = false;
    UFUNCTION()
    void OnPracticeBallHit6AF(UPrimitiveComponent* HitComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
    FString ShotEndDetail6AE;
    bool bBarrierContactThisShot6P = false;
    bool bBetweenShotAttempts6N = false;
    float NextAttemptAt6N = 0.f;
    bool bShotReleased6M = false;
    bool bGoalScored6M = false;
    bool bShotInFlight6M = false;
    float ShotCharge6M = 0.f;
    float ShotReleaseTime6M = 0.f;
    FVector PreviousBallLocal6M = FVector::ZeroVector;
    bool bHavePreviousBall6M = false;
    bool bAutoPassArmed6J = false;
    int32 AutoPassCount6J = 0;
    bool bExerciseStarted6H = false;
    UPROPERTY(Transient) ASoccerBall* PracticeBall6B = nullptr;
    UPROPERTY(Transient) AThirdPersonCppCharacter* Trainee6B = nullptr;
    int32 CheckpointIndex6B = 0;
    float ElapsedTime6B = 0.f;
    int32 LostControlCount6B = 0;
    float OutOfControlSeconds6B = 0.f;
    bool bHadControl6B = false;
    bool bComplete6B = false;
    bool bTrainingBallParityApplied6G = false;
    bool bTrainingProfileApplied6D = false;
    int32 LastPossessionState6D = -1;

    ASoccerField* FindSharedField6L() const;
    void EnsureGround6B();
    void EnsurePlayerStart6B();
    void EnsureBall6B();
    void FindExerciseActors6B();
    void EnsureTrainingProfile6D();
    void EnsureTrainingBallParity6G();
    void DrawCheckpoint6B() const;
    void TickChargedShot6M(float DeltaSeconds);
    void FindTrainingGoalkeeper6T();
    void EnsureTrainingGoalkeeperProfile6Z3();
    void UpdateTrainingGoalkeeper6T(float DeltaSeconds);
    void ResolveShotAttempt6N(ETrainingShotOutcome6P Outcome);
    UFUNCTION()
    void OnPracticeBarrierHit6P(AActor* SelfActor, AActor* OtherActor,
        FVector NormalImpulse, const FHitResult& Hit);
    static FString ShotOutcomeLabel6P(ETrainingShotOutcome6P Outcome);
    FVector FindSafeShotBallSpawn6AB2(const FVector& Requested) const;
    void VerifyShotBallReset6AB2();
    FVector ShotBallResetWorld6AB2 = FVector::ZeroVector;
    float ShotBallResetUntil6AB2 = -1.f;
    float ShotBallNextCheck6AB2 = 0.f;
    int32 ShotBallResetCorrections6AB2 = 0;
    void PrepareShotAttempt6N();
    void ClearPracticeBarriers6O();
    void SetupPracticeBarriers6O(const FVector& BallLocal);
    FVector GetShotBallLocal6N(int32 AttemptIndex) const;
};
