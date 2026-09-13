#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerInstantReplayManager.generated.h"

class AActor;
class ACameraActor;
class APlayerController;
class ASoccerBall;
class ASoccerCharacterBase;
class ASoccerMatchManager;
class UAnimMontage;
class USceneComponent;

/*
 * One visual sample of a soccer character.
 *
 * The replay recorder intentionally stores appearance/motion state rather than
 * AI decisions. Playback can therefore reproduce what happened without asking
 * the AI, navigation or match rules to simulate the past again.
 */
struct FSoccerReplayCharacterSample
{
    TWeakObjectPtr<ASoccerCharacterBase> Character;

    FTransform ActorTransform = FTransform::Identity;
    FVector Velocity = FVector::ZeroVector;

    uint8 MovementMode = 0;
    uint8 CustomMovementMode = 0;

    bool bActorCollisionEnabled = true;
    bool bPossessingBall = false;
    bool bChasingBall = false;
    bool bKicking = false;
    bool bBracingPhysicalContact = false;
    bool bForceDribbleTurnLocomotion = false;
    float ForcedDribbleTurnLocomotionSpeed = 0.0f;

    TWeakObjectPtr<UAnimMontage> ActiveMontage;
    float MontagePositionSeconds = 0.0f;
    float MontagePlayRate = 1.0f;
    bool bMontagePlaying = false;
};

/* Visual/physics state required to reproduce the soccer ball later. */
struct FSoccerReplayBallSample
{
    TWeakObjectPtr<ASoccerBall> Ball;

    FTransform ActorTransform = FTransform::Identity;
    FVector LinearVelocity = FVector::ZeroVector;
    FVector AngularVelocityDegrees = FVector::ZeroVector;

    bool bActorCollisionEnabled = true;
    bool bSimulatingPhysics = false;

    TWeakObjectPtr<AActor> AttachedParentActor;
    TWeakObjectPtr<USceneComponent> AttachedParentComponent;
    FName AttachedSocketName = NAME_None;
};

/* A complete instant-replay sample at one point on the recorder timeline. */
struct FSoccerReplayFrame
{
    double RecordingTimeSeconds = 0.0;
    TArray<FSoccerReplayCharacterSample> Characters;
    FSoccerReplayBallSample Ball;
};

enum class ESoccerInstantReplayPlaybackReason : uint8
{
    None,
    Manual,
    Goal,
    Foul,
    Offside
};

struct FSoccerGoalReplayCameraConfig
{
    float Distance = 2600.0f;
    float InfieldOffset = 0.0f;
    float Height = 1100.0f;
    float FOV = 78.0f;
};

UCLASS()
class THIRDPERSONCPP_API ASoccerInstantReplayManager : public AActor
{
    GENERATED_BODY()

public:
    ASoccerInstantReplayManager();

    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void InitializeRecorder(
        ASoccerMatchManager* InMatchManager,
        ASoccerBall* InSoccerBall,
        float InHistorySeconds,
        float InSamplesPerSecond,
        float InManualReplaySeconds
    );

    void SetRecordingEnabled(bool bEnabled);
    bool IsRecordingEnabled() const;

    void ClearRecording();

    int32 GetRecordedFrameCount() const;
    float GetRecordedDurationSeconds() const;
    double GetLatestRecordingTimeSeconds() const;

    void CopyRecentFrames(
        float RequestedSeconds,
        TArray<FSoccerReplayFrame>& OutFrames
    ) const;

    ASoccerBall* GetTrackedBall() const;

    /* Stage 2 manual playback. NumPad 8 toggles this during PIE. */
    bool StartManualReplay(float RequestedSeconds = -1.0f);
    void StopManualReplay();

    /*
     * Generic event-triggered playback entry point. Goal is the first automatic
     * consumer; Foul and Offside are reserved for the later replay stages.
     */
    bool StartEventReplay(
        ESoccerInstantReplayPlaybackReason PlaybackReason,
        float RequestedSeconds,
        float GoalLineSign = 0.0f
    );

    void ConfigureGoalReplayPresentation(
        const FSoccerGoalReplayCameraConfig& InLeftCamera,
        const FSoccerGoalReplayCameraConfig& InRightCamera,
        const FSoccerGoalReplayCameraConfig& InFrontCamera,
        const FSoccerGoalReplayCameraConfig& InBehindCamera,
        float InCameraFadeDuration,
        float InCameraCollisionPadding
    );

    bool IsReplayPlaying() const;
    FString GetSkipReplayInputHintText() const;

protected:
    virtual void BeginPlay() override;

private:
    void ApplyRecorderConfiguration(
        float InHistorySeconds,
        float InSamplesPerSecond,
        float InManualReplaySeconds
    );

    void RebuildRingBuffer();
    void RefreshTrackedCharacters();
    void ResolveTrackedBallIfNeeded();
    void CaptureFrame();
    void CaptureCurrentWorldFrame(FSoccerReplayFrame& OutFrame) const;

    int32 GetOldestFrameIndex() const;
    int32 GetNewestFrameIndex() const;

    void TryBindManualReplayInput();
    void HandleManualReplayInput();
    void HandleSkipReplayInput();

    bool StartReplayInternal(
        float RequestedSeconds,
        ESoccerInstantReplayPlaybackReason PlaybackReason,
        bool bAppendImmediateEndpoint,
        float GoalLineSign
    );
    void AppendImmediatePlaybackEndpoint();
    void TickReplayPlayback();
    void FinishReplay(bool bRestoreLiveState);
    void PrepareActorsForReplay();
    void RestoreLiveStateAfterReplay();

    void ApplyPlaybackTime(double TargetRecordingTimeSeconds, float VisualDeltaSeconds);
    void ApplyCharacterPlaybackSample(
        const FSoccerReplayCharacterSample& SampleA,
        const FSoccerReplayCharacterSample* SampleB,
        float Alpha,
        float VisualDeltaSeconds,
        bool bRestoreCollision
    );
    void ApplyBallPlaybackSample(
        const FSoccerReplayBallSample& SampleA,
        const FSoccerReplayBallSample* SampleB,
        float Alpha,
        bool bRestoreLiveState
    );

    const FSoccerReplayCharacterSample* FindCharacterSample(
        const FSoccerReplayFrame& Frame,
        const ASoccerCharacterBase* Character
    ) const;

    void ApplyCharacterMontageVisual(
        ASoccerCharacterBase* Character,
        const FSoccerReplayCharacterSample& Sample,
        float VisualDeltaSeconds,
        bool bResumeLivePlayback
    );

    bool BuildReplayCameraForReason(
        ESoccerInstantReplayPlaybackReason PlaybackReason,
        float GoalLineSign
    );
    bool BuildFixedReplayCamera();
    bool BuildGoalReplayCamera(float GoalLineSign);
    bool ConfigureCurrentGoalReplayCamera();
    bool AdvanceGoalReplayCameraTake();
    bool HasNextGoalReplayCameraTake() const;
    void BeginGoalReplayCameraTransition();
    void TickGoalReplayCameraTransition(
        double CurrentRealTimeSeconds,
        float RealDeltaSeconds
    );
    void ClearReplayCameraFade();
    FVector ResolveGoalReplayCameraCollision(
        const FVector& AimLocation,
        const FVector& DesiredCameraLocation
    ) const;
    const FSoccerGoalReplayCameraConfig& GetCurrentGoalReplayCameraConfig() const;
    void UpdateReplayCameraAim();
    void RefreshReplayCameraView(float RealDeltaSeconds);
    const TCHAR* GetCurrentGoalReplayCameraName() const;

    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Recording", meta = (ClampMin = "2.0", UIMin = "2.0"))
    float HistorySeconds = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Recording", meta = (ClampMin = "5.0", ClampMax = "60.0", UIMin = "5.0", UIMax = "60.0"))
    float SamplesPerSecond = 30.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Recording")
    bool bRecordingEnabled = true;

    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Manual Playback", meta = (ClampMin = "1.0", ClampMax = "10.0", UIMin = "1.0", UIMax = "10.0"))
    float ManualReplaySeconds = 5.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Manual Playback", meta = (ClampMin = "500.0", UIMin = "500.0"))
    float FixedCameraSideDistance = 3000.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Manual Playback", meta = (ClampMin = "200.0", UIMin = "200.0"))
    float FixedCameraHeight = 1500.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Manual Playback", meta = (ClampMin = "30.0", ClampMax = "120.0", UIMin = "30.0", UIMax = "120.0"))
    float FixedCameraFOV = 75.0f;

    // Runtime copies of MatchManager camera tuning. The editable settings live
    // in SoccerMatchManager so an automatically spawned replay manager needs no
    // level setup.
    FSoccerGoalReplayCameraConfig GoalReplayLeftCamera;
    FSoccerGoalReplayCameraConfig GoalReplayRightCamera;
    FSoccerGoalReplayCameraConfig GoalReplayFrontCamera;
    FSoccerGoalReplayCameraConfig GoalReplayBehindCamera;

    float GoalReplayCameraFadeDuration = 0.12f;
    float GoalReplayCameraCollisionPadding = 80.0f;

    /* Characters are refreshed occasionally so later spawned/replaced actors join recording. */
    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Recording", meta = (ClampMin = "0.25", UIMin = "0.25"))
    float TrackedActorRefreshInterval = 1.0f;

    TWeakObjectPtr<ASoccerMatchManager> MatchManager;
    TWeakObjectPtr<ASoccerBall> SoccerBall;
    TArray<TWeakObjectPtr<ASoccerCharacterBase>> TrackedCharacters;

    TArray<FSoccerReplayFrame> RingFrames;
    int32 NextWriteIndex = 0;
    int32 ValidFrameCount = 0;

    float SampleAccumulator = 0.0f;
    float TrackedActorRefreshAccumulator = 0.0f;
    double RecordingClockSeconds = 0.0;
    bool bLoggedFullHistoryReady = false;

    bool bManualReplayInputBound = false;
    bool bReplayPlaying = false;
    ESoccerInstantReplayPlaybackReason ActivePlaybackReason =
        ESoccerInstantReplayPlaybackReason::None;
    bool bRecordingWasEnabledBeforeReplay = false;
    bool bAppliedMoveInputIgnore = false;
    bool bAppliedLookInputIgnore = false;

    TArray<FSoccerReplayFrame> PlaybackFrames;
    FSoccerReplayFrame LiveResumeFrame;
    int32 PlaybackFrameCursor = 0;
    double PlaybackClipStartTimeSeconds = 0.0;
    double PlaybackClipEndTimeSeconds = 0.0;
    double PlaybackElapsedSeconds = 0.0;
    double LastPlaybackRealTimeSeconds = 0.0;

    float ActiveGoalLineSign = 0.0f;
    int32 ActiveGoalReplayCameraTakeIndex = 0;
    int32 GoalReplayCameraTakeCount = 4;

    bool bGoalReplayCameraTransitionActive = false;
    bool bGoalReplayCameraTransitionSwitched = false;
    double GoalReplayCameraTransitionPhaseStartRealTimeSeconds = 0.0;

    TWeakObjectPtr<APlayerController> ReplayPlayerController;
    TWeakObjectPtr<AActor> PreviousViewTarget;
    TWeakObjectPtr<ACameraActor> ReplayCamera;
};
