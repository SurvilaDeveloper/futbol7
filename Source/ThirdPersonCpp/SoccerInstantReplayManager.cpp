#include "SoccerInstantReplayManager.h"

#include "SoccerBall.h"
#include "SoccerCharacterBase.h"
#include "SoccerMatchManager.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ASoccerInstantReplayManager::ASoccerInstantReplayManager()
{
    PrimaryActorTick.bCanEverTick = true;

    /* Stage 2 will need the replay manager to keep ticking while gameplay is paused. */
    PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ASoccerInstantReplayManager::BeginPlay()
{
    Super::BeginPlay();

    RebuildRingBuffer();
    ResolveTrackedBallIfNeeded();
    RefreshTrackedCharacters();
}

void ASoccerInstantReplayManager::InitializeRecorder(
    ASoccerMatchManager* InMatchManager,
    ASoccerBall* InSoccerBall,
    float InHistorySeconds,
    float InSamplesPerSecond
)
{
    MatchManager = InMatchManager;
    SoccerBall = InSoccerBall;

    ApplyRecorderConfiguration(
        InHistorySeconds,
        InSamplesPerSecond
    );

    ResolveTrackedBallIfNeeded();
    RefreshTrackedCharacters();

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[InstantReplay] Recorder ready: %.1f s @ %.1f Hz, capacity=%d frames, characters=%d."),
        HistorySeconds,
        SamplesPerSecond,
        RingFrames.Num(),
        TrackedCharacters.Num()
    );
}

void ASoccerInstantReplayManager::ApplyRecorderConfiguration(
    float InHistorySeconds,
    float InSamplesPerSecond
)
{
    HistorySeconds = FMath::Max(2.0f, InHistorySeconds);
    SamplesPerSecond = FMath::Clamp(InSamplesPerSecond, 5.0f, 60.0f);

    RebuildRingBuffer();
}

void ASoccerInstantReplayManager::RebuildRingBuffer()
{
    const int32 RequiredFrameCapacity = FMath::Max(
        2,
        FMath::CeilToInt(
            FMath::Max(2.0f, HistorySeconds) *
            FMath::Clamp(SamplesPerSecond, 5.0f, 60.0f)
        ) + 2
    );

    RingFrames.Reset();
    RingFrames.SetNum(RequiredFrameCapacity);

    NextWriteIndex = 0;
    ValidFrameCount = 0;
    SampleAccumulator = 0.0f;
    RecordingClockSeconds = 0.0;
    bLoggedFullHistoryReady = false;
}

void ASoccerInstantReplayManager::SetRecordingEnabled(bool bEnabled)
{
    bRecordingEnabled = bEnabled;
    SampleAccumulator = 0.0f;
}

bool ASoccerInstantReplayManager::IsRecordingEnabled() const
{
    return bRecordingEnabled;
}

void ASoccerInstantReplayManager::ClearRecording()
{
    NextWriteIndex = 0;
    ValidFrameCount = 0;
    SampleAccumulator = 0.0f;
    RecordingClockSeconds = 0.0;
    bLoggedFullHistoryReady = false;
}

int32 ASoccerInstantReplayManager::GetRecordedFrameCount() const
{
    return ValidFrameCount;
}

float ASoccerInstantReplayManager::GetRecordedDurationSeconds() const
{
    if (ValidFrameCount <= 1 || RingFrames.Num() <= 0)
    {
        return 0.0f;
    }

    const int32 OldestIndex = GetOldestFrameIndex();
    const int32 NewestIndex = GetNewestFrameIndex();

    if (OldestIndex == INDEX_NONE || NewestIndex == INDEX_NONE)
    {
        return 0.0f;
    }

    return static_cast<float>(FMath::Max(
        0.0,
        RingFrames[NewestIndex].RecordingTimeSeconds -
        RingFrames[OldestIndex].RecordingTimeSeconds
    ));
}

double ASoccerInstantReplayManager::GetLatestRecordingTimeSeconds() const
{
    const int32 NewestIndex = GetNewestFrameIndex();

    return NewestIndex != INDEX_NONE
        ? RingFrames[NewestIndex].RecordingTimeSeconds
        : 0.0;
}

void ASoccerInstantReplayManager::CopyRecentFrames(
    float RequestedSeconds,
    TArray<FSoccerReplayFrame>& OutFrames
) const
{
    OutFrames.Reset();

    if (ValidFrameCount <= 0 || RingFrames.Num() <= 0)
    {
        return;
    }

    const int32 NewestIndex = GetNewestFrameIndex();

    if (NewestIndex == INDEX_NONE)
    {
        return;
    }

    const double NewestTime = RingFrames[NewestIndex].RecordingTimeSeconds;
    const double EarliestRequestedTime = NewestTime - FMath::Max(0.0f, RequestedSeconds);
    const int32 Capacity = RingFrames.Num();
    const int32 OldestIndex = GetOldestFrameIndex();

    if (OldestIndex == INDEX_NONE)
    {
        return;
    }

    OutFrames.Reserve(ValidFrameCount);

    for (int32 ChronologicalOffset = 0; ChronologicalOffset < ValidFrameCount; ++ChronologicalOffset)
    {
        const int32 FrameIndex =
            (OldestIndex + ChronologicalOffset) % Capacity;

        const FSoccerReplayFrame& Frame = RingFrames[FrameIndex];

        if (Frame.RecordingTimeSeconds + KINDA_SMALL_NUMBER < EarliestRequestedTime)
        {
            continue;
        }

        OutFrames.Add(Frame);
    }
}

ASoccerBall* ASoccerInstantReplayManager::GetTrackedBall() const
{
    return SoccerBall.Get();
}

int32 ASoccerInstantReplayManager::GetOldestFrameIndex() const
{
    if (ValidFrameCount <= 0 || RingFrames.Num() <= 0)
    {
        return INDEX_NONE;
    }

    const int32 Capacity = RingFrames.Num();

    return
        (NextWriteIndex - ValidFrameCount + Capacity) % Capacity;
}

int32 ASoccerInstantReplayManager::GetNewestFrameIndex() const
{
    if (ValidFrameCount <= 0 || RingFrames.Num() <= 0)
    {
        return INDEX_NONE;
    }

    const int32 Capacity = RingFrames.Num();

    return
        (NextWriteIndex - 1 + Capacity) % Capacity;
}

void ASoccerInstantReplayManager::ResolveTrackedBallIfNeeded()
{
    if (SoccerBall.IsValid())
    {
        return;
    }

    ASoccerMatchManager* Manager = MatchManager.Get();

    if (IsValid(Manager))
    {
        ASoccerBall* ManagerBall = Manager->GetSoccerBall();

        if (IsValid(ManagerBall))
        {
            SoccerBall = ManagerBall;
            return;
        }
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    for (TActorIterator<ASoccerBall> It(World); It; ++It)
    {
        ASoccerBall* Candidate = *It;

        if (IsValid(Candidate))
        {
            SoccerBall = Candidate;
            return;
        }
    }
}

void ASoccerInstantReplayManager::RefreshTrackedCharacters()
{
    TrackedCharacters.Reset();

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
    {
        ASoccerCharacterBase* Character = *It;

        if (IsValid(Character))
        {
            TrackedCharacters.Add(Character);
        }
    }
}

void ASoccerInstantReplayManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bRecordingEnabled || DeltaTime <= 0.0f)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (World == nullptr || UGameplayStatics::IsGamePaused(World))
    {
        return;
    }

    RecordingClockSeconds += DeltaTime;

    TrackedActorRefreshAccumulator += DeltaTime;

    if (
        TrackedActorRefreshAccumulator >=
        FMath::Max(0.25f, TrackedActorRefreshInterval)
    )
    {
        TrackedActorRefreshAccumulator = 0.0f;
        ResolveTrackedBallIfNeeded();
        RefreshTrackedCharacters();
    }

    const float SafeSamplesPerSecond =
        FMath::Clamp(SamplesPerSecond, 5.0f, 60.0f);
    const float SampleInterval = 1.0f / SafeSamplesPerSecond;

    SampleAccumulator += DeltaTime;

    if (SampleAccumulator < SampleInterval)
    {
        return;
    }

    /*
     * Capture one real current state, even after a hitch. Repeating the same
     * transform several times to artificially catch up would make playback
     * look frozen and would not represent historical states that we never saw.
     */
    SampleAccumulator = FMath::Fmod(SampleAccumulator, SampleInterval);

    CaptureFrame();
}

void ASoccerInstantReplayManager::CaptureFrame()
{
    if (RingFrames.Num() <= 0)
    {
        RebuildRingBuffer();
    }

    if (RingFrames.Num() <= 0)
    {
        return;
    }

    ResolveTrackedBallIfNeeded();

    /*
     * Reuse the TArray storage owned by this ring slot. After the first
     * history window fills, recording no longer needs to allocate a fresh
     * character array on every sample.
     */
    FSoccerReplayFrame& Frame = RingFrames[NextWriteIndex];
    Frame.RecordingTimeSeconds = RecordingClockSeconds;
    Frame.Characters.Reset(TrackedCharacters.Num());
    Frame.Ball = FSoccerReplayBallSample();

    for (const TWeakObjectPtr<ASoccerCharacterBase>& CharacterPtr : TrackedCharacters)
    {
        ASoccerCharacterBase* Character = CharacterPtr.Get();

        if (!IsValid(Character))
        {
            continue;
        }

        FSoccerReplayCharacterSample Sample;
        Sample.Character = Character;
        Sample.ActorTransform = Character->GetActorTransform();
        Sample.Velocity = Character->GetVelocity();

        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Sample.MovementMode = static_cast<uint8>(Movement->MovementMode);
            Sample.CustomMovementMode = Movement->CustomMovementMode;
        }

        Sample.bPossessingBall = Character->GetSoccerIsPossessingBall();
        Sample.bChasingBall = Character->GetSoccerIsChasingBall();
        Sample.bKicking = Character->GetSoccerIsKicking();
        Sample.bForceDribbleTurnLocomotion =
            Character->GetSoccerShouldForceDribbleTurnLocomotion();
        Sample.ForcedDribbleTurnLocomotionSpeed =
            Character->GetSoccerForcedDribbleTurnLocomotionSpeed();

        USkeletalMeshComponent* Mesh = Character->GetMesh();
        UAnimInstance* AnimInstance =
            Mesh != nullptr ? Mesh->GetAnimInstance() : nullptr;

        if (AnimInstance != nullptr)
        {
            UAnimMontage* ActiveMontage =
                AnimInstance->GetCurrentActiveMontage();

            if (
                ActiveMontage != nullptr &&
                AnimInstance->Montage_IsActive(ActiveMontage)
            )
            {
                Sample.ActiveMontage = ActiveMontage;
                Sample.MontagePositionSeconds =
                    AnimInstance->Montage_GetPosition(ActiveMontage);
                Sample.MontagePlayRate =
                    AnimInstance->Montage_GetPlayRate(ActiveMontage);
                Sample.bMontagePlaying =
                    AnimInstance->Montage_IsPlaying(ActiveMontage);
            }
        }

        Frame.Characters.Add(MoveTemp(Sample));
    }

    ASoccerBall* Ball = SoccerBall.Get();

    if (IsValid(Ball))
    {
        Frame.Ball.Ball = Ball;
        Frame.Ball.ActorTransform = Ball->GetActorTransform();
        Frame.Ball.AttachedParentActor = Ball->GetAttachParentActor();

        USceneComponent* BallRootComponent = Ball->GetRootComponent();

        if (BallRootComponent != nullptr)
        {
            Frame.Ball.AttachedSocketName =
                BallRootComponent->GetAttachSocketName();
        }

        UPrimitiveComponent* PrimitiveRoot =
            Cast<UPrimitiveComponent>(BallRootComponent);

        if (PrimitiveRoot != nullptr)
        {
            Frame.Ball.bSimulatingPhysics =
                PrimitiveRoot->IsSimulatingPhysics();
            Frame.Ball.LinearVelocity =
                PrimitiveRoot->GetPhysicsLinearVelocity();
            Frame.Ball.AngularVelocityDegrees =
                PrimitiveRoot->GetPhysicsAngularVelocityInDegrees();
        }
        else
        {
            Frame.Ball.LinearVelocity = Ball->GetVelocity();
        }
    }

    NextWriteIndex =
        (NextWriteIndex + 1) % RingFrames.Num();

    ValidFrameCount = FMath::Min(
        ValidFrameCount + 1,
        RingFrames.Num()
    );

    if (
        !bLoggedFullHistoryReady &&
        ValidFrameCount >= RingFrames.Num()
    )
    {
        bLoggedFullHistoryReady = true;

        UE_LOG(
            LogTemp,
            Log,
            TEXT("[InstantReplay] Full rolling history is ready: %d frames, %.2f s recorded."),
            ValidFrameCount,
            GetRecordedDurationSeconds()
        );
    }
}
