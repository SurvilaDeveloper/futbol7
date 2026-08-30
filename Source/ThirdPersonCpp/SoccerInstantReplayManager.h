#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerInstantReplayManager.generated.h"

class AActor;
class ASoccerBall;
class ASoccerCharacterBase;
class ASoccerMatchManager;
class UAnimMontage;

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

    bool bPossessingBall = false;
    bool bChasingBall = false;
    bool bKicking = false;
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

    bool bSimulatingPhysics = false;

    TWeakObjectPtr<AActor> AttachedParentActor;
    FName AttachedSocketName = NAME_None;
};

/* A complete instant-replay sample at one point on the recorder timeline. */
struct FSoccerReplayFrame
{
    double RecordingTimeSeconds = 0.0;
    TArray<FSoccerReplayCharacterSample> Characters;
    FSoccerReplayBallSample Ball;
};

UCLASS()
class THIRDPERSONCPP_API ASoccerInstantReplayManager : public AActor
{
    GENERATED_BODY()

public:
    ASoccerInstantReplayManager();

    virtual void Tick(float DeltaTime) override;

    /*
     * Called by ASoccerMatchManager after it has resolved the active ball.
     * This does not start playback; Stage 1 is recording only.
     */
    void InitializeRecorder(
        ASoccerMatchManager* InMatchManager,
        ASoccerBall* InSoccerBall,
        float InHistorySeconds,
        float InSamplesPerSecond
    );

    void SetRecordingEnabled(bool bEnabled);
    bool IsRecordingEnabled() const;

    void ClearRecording();

    int32 GetRecordedFrameCount() const;
    float GetRecordedDurationSeconds() const;
    double GetLatestRecordingTimeSeconds() const;

    /*
     * Copies the requested tail of the ring buffer in chronological order.
     * Stage 2 will use this to build a playback clip without knowing how the
     * recorder stores its circular history internally.
     */
    void CopyRecentFrames(
        float RequestedSeconds,
        TArray<FSoccerReplayFrame>& OutFrames
    ) const;

    ASoccerBall* GetTrackedBall() const;

protected:
    virtual void BeginPlay() override;

private:
    void ApplyRecorderConfiguration(
        float InHistorySeconds,
        float InSamplesPerSecond
    );

    void RebuildRingBuffer();
    void RefreshTrackedCharacters();
    void ResolveTrackedBallIfNeeded();
    void CaptureFrame();

    int32 GetOldestFrameIndex() const;
    int32 GetNewestFrameIndex() const;

    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Recording", meta = (ClampMin = "2.0", UIMin = "2.0"))
    float HistorySeconds = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Recording", meta = (ClampMin = "5.0", ClampMax = "60.0", UIMin = "5.0", UIMax = "60.0"))
    float SamplesPerSecond = 30.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Recording")
    bool bRecordingEnabled = true;

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
};
