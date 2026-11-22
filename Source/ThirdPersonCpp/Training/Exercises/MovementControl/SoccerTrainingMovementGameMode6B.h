#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SoccerTrainingMovementGameMode6B.generated.h"

class ASoccerBall;
class AThirdPersonCppCharacter;
class USoccerPlayerProfile;

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

    void EnsureGround6B();
    void EnsurePlayerStart6B();
    void EnsureBall6B();
    void FindExerciseActors6B();
    void EnsureTrainingProfile6D();
    void EnsureTrainingBallParity6G();
    void DrawCheckpoint6B() const;
};
