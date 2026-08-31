#include "SoccerInstantReplayManager.h"

#include "SoccerBall.h"
#include "SoccerCharacterBase.h"
#include "SoccerField.h"
#include "SoccerMatchManager.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

ASoccerInstantReplayManager::ASoccerInstantReplayManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ASoccerInstantReplayManager::BeginPlay()
{
    Super::BeginPlay();

    RebuildRingBuffer();
    ResolveTrackedBallIfNeeded();
    RefreshTrackedCharacters();
    TryBindManualReplayInput();
}

void ASoccerInstantReplayManager::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (bReplayPlaying)
    {
        FinishReplay(false);
    }

    Super::EndPlay(EndPlayReason);
}

void ASoccerInstantReplayManager::InitializeRecorder(
    ASoccerMatchManager* InMatchManager,
    ASoccerBall* InSoccerBall,
    float InHistorySeconds,
    float InSamplesPerSecond,
    float InManualReplaySeconds
)
{
    MatchManager = InMatchManager;
    SoccerBall = InSoccerBall;

    ApplyRecorderConfiguration(
        InHistorySeconds,
        InSamplesPerSecond,
        InManualReplaySeconds
    );

    ResolveTrackedBallIfNeeded();
    RefreshTrackedCharacters();
    TryBindManualReplayInput();

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[InstantReplay] Recorder ready: %.1f s @ %.1f Hz, capacity=%d frames, characters=%d."),
        HistorySeconds,
        SamplesPerSecond,
        RingFrames.Num(),
        TrackedCharacters.Num()
    );

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[InstantReplay] Manual replay: NumPad 8 plays the most recent %.1f seconds (press again to stop)."),
        ManualReplaySeconds
    );

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[InstantReplay] NumPad 9 skips any replay and immediately returns to the live match.")
    );
}

void ASoccerInstantReplayManager::ApplyRecorderConfiguration(
    float InHistorySeconds,
    float InSamplesPerSecond,
    float InManualReplaySeconds
)
{
    HistorySeconds = FMath::Max(2.0f, InHistorySeconds);
    SamplesPerSecond = FMath::Clamp(InSamplesPerSecond, 5.0f, 60.0f);
    ManualReplaySeconds = FMath::Clamp(InManualReplaySeconds, 1.0f, 10.0f);

    RebuildRingBuffer();
}

void ASoccerInstantReplayManager::ConfigureGoalReplayCameras(
    float InSideDistance,
    float InSideInfieldOffset,
    float InFrontDistance,
    float InBehindDistance,
    float InCameraHeight,
    float InCameraFOV
)
{
    GoalReplaySideDistance = FMath::Max(500.0f, InSideDistance);
    GoalReplaySideInfieldOffset = FMath::Max(0.0f, InSideInfieldOffset);
    GoalReplayFrontDistance = FMath::Max(500.0f, InFrontDistance);
    GoalReplayBehindDistance = FMath::Max(500.0f, InBehindDistance);
    GoalReplayCameraHeight = FMath::Max(200.0f, InCameraHeight);
    GoalReplayCameraFOV = FMath::Clamp(InCameraFOV, 30.0f, 120.0f);
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
    const double EarliestRequestedTime =
        NewestTime - FMath::Max(0.0f, RequestedSeconds);
    const int32 Capacity = RingFrames.Num();
    const int32 OldestIndex = GetOldestFrameIndex();

    if (OldestIndex == INDEX_NONE)
    {
        return;
    }

    OutFrames.Reserve(ValidFrameCount);

    for (
        int32 ChronologicalOffset = 0;
        ChronologicalOffset < ValidFrameCount;
        ++ChronologicalOffset
    )
    {
        const int32 FrameIndex =
            (OldestIndex + ChronologicalOffset) % Capacity;

        const FSoccerReplayFrame& Frame = RingFrames[FrameIndex];

        if (
            Frame.RecordingTimeSeconds + KINDA_SMALL_NUMBER <
            EarliestRequestedTime
        )
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

bool ASoccerInstantReplayManager::IsReplayPlaying() const
{
    return bReplayPlaying;
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

void ASoccerInstantReplayManager::TryBindManualReplayInput()
{
    if (bManualReplayInputBound)
    {
        return;
    }

    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);

    if (!IsValid(PlayerController))
    {
        return;
    }

    EnableInput(PlayerController);

    if (InputComponent == nullptr)
    {
        return;
    }

    FInputKeyBinding& ReplayBinding = InputComponent->BindKey(
        EKeys::NumPadEight,
        IE_Pressed,
        this,
        &ASoccerInstantReplayManager::HandleManualReplayInput
    );

    ReplayBinding.bExecuteWhenPaused = true;
    ReplayBinding.bConsumeInput = true;

    FInputKeyBinding& SkipReplayBinding = InputComponent->BindKey(
        EKeys::NumPadNine,
        IE_Pressed,
        this,
        &ASoccerInstantReplayManager::HandleSkipReplayInput
    );

    SkipReplayBinding.bExecuteWhenPaused = true;
    SkipReplayBinding.bConsumeInput = true;

    bManualReplayInputBound = true;
}

void ASoccerInstantReplayManager::HandleManualReplayInput()
{
    if (bReplayPlaying)
    {
        if (ActivePlaybackReason == ESoccerInstantReplayPlaybackReason::Manual)
        {
            StopManualReplay();
        }

        return;
    }

    StartManualReplay(ManualReplaySeconds);
}

void ASoccerInstantReplayManager::HandleSkipReplayInput()
{
    if (!bReplayPlaying)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[InstantReplay] Replay skipped by player input (NumPad 9).")
    );

    FinishReplay(true);
}

void ASoccerInstantReplayManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bManualReplayInputBound)
    {
        TryBindManualReplayInput();
    }

    if (bReplayPlaying)
    {
        TickReplayPlayback();
        return;
    }

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

void ASoccerInstantReplayManager::CaptureCurrentWorldFrame(
    FSoccerReplayFrame& OutFrame
) const
{
    OutFrame.RecordingTimeSeconds = RecordingClockSeconds;
    OutFrame.Characters.Reset(TrackedCharacters.Num());
    OutFrame.Ball = FSoccerReplayBallSample();

    for (
        const TWeakObjectPtr<ASoccerCharacterBase>& CharacterPtr :
        TrackedCharacters
    )
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
        Sample.bActorCollisionEnabled = Character->GetActorEnableCollision();

        if (UCharacterMovementComponent* Movement =
            Character->GetCharacterMovement())
        {
            Sample.MovementMode =
                static_cast<uint8>(Movement->MovementMode);
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

        OutFrame.Characters.Add(MoveTemp(Sample));
    }

    ASoccerBall* Ball = SoccerBall.Get();

    if (!IsValid(Ball))
    {
        return;
    }

    OutFrame.Ball.Ball = Ball;
    OutFrame.Ball.ActorTransform = Ball->GetActorTransform();
    OutFrame.Ball.bActorCollisionEnabled = Ball->GetActorEnableCollision();
    OutFrame.Ball.AttachedParentActor = Ball->GetAttachParentActor();

    USceneComponent* BallRootComponent = Ball->GetRootComponent();

    if (BallRootComponent != nullptr)
    {
        OutFrame.Ball.AttachedParentComponent =
            BallRootComponent->GetAttachParent();
        OutFrame.Ball.AttachedSocketName =
            BallRootComponent->GetAttachSocketName();
    }

    UPrimitiveComponent* PrimitiveRoot =
        Cast<UPrimitiveComponent>(BallRootComponent);

    if (PrimitiveRoot != nullptr)
    {
        OutFrame.Ball.bSimulatingPhysics =
            PrimitiveRoot->IsSimulatingPhysics();
        OutFrame.Ball.LinearVelocity =
            PrimitiveRoot->GetPhysicsLinearVelocity();
        OutFrame.Ball.AngularVelocityDegrees =
            PrimitiveRoot->GetPhysicsAngularVelocityInDegrees();
    }
    else
    {
        OutFrame.Ball.LinearVelocity = Ball->GetVelocity();
    }
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

    FSoccerReplayFrame& Frame = RingFrames[NextWriteIndex];
    CaptureCurrentWorldFrame(Frame);

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

bool ASoccerInstantReplayManager::StartManualReplay(float RequestedSeconds)
{
    const float RequestedClipSeconds =
        RequestedSeconds > 0.0f
            ? RequestedSeconds
            : ManualReplaySeconds;

    return StartReplayInternal(
        RequestedClipSeconds,
        ESoccerInstantReplayPlaybackReason::Manual,
        true,
        0.0f
    );
}

bool ASoccerInstantReplayManager::StartEventReplay(
    ESoccerInstantReplayPlaybackReason PlaybackReason,
    float RequestedSeconds,
    float GoalLineSign
)
{
    if (
        PlaybackReason == ESoccerInstantReplayPlaybackReason::None ||
        PlaybackReason == ESoccerInstantReplayPlaybackReason::Manual
    )
    {
        return false;
    }

    return StartReplayInternal(
        RequestedSeconds,
        PlaybackReason,
        true,
        GoalLineSign
    );
}

bool ASoccerInstantReplayManager::StartReplayInternal(
    float RequestedSeconds,
    ESoccerInstantReplayPlaybackReason PlaybackReason,
    bool bAppendImmediateEndpoint,
    float GoalLineSign
)
{
    if (bReplayPlaying)
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    if (UGameplayStatics::IsGamePaused(World))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[InstantReplay] Replay ignored because the world is already paused by another system.")
        );
        return false;
    }

    const float RequestedClipSeconds = FMath::Clamp(
        RequestedSeconds > 0.0f ? RequestedSeconds : ManualReplaySeconds,
        1.0f,
        HistorySeconds
    );

    CopyRecentFrames(RequestedClipSeconds, PlaybackFrames);

    if (bAppendImmediateEndpoint)
    {
        AppendImmediatePlaybackEndpoint();
    }

    if (PlaybackFrames.Num() < 2)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[InstantReplay] Not enough recorded frames for playback yet.")
        );
        PlaybackFrames.Reset();
        return false;
    }

    RefreshTrackedCharacters();
    ResolveTrackedBallIfNeeded();
    CaptureCurrentWorldFrame(LiveResumeFrame);

    PlaybackClipStartTimeSeconds =
        PlaybackFrames[0].RecordingTimeSeconds;
    PlaybackClipEndTimeSeconds =
        PlaybackFrames.Last().RecordingTimeSeconds;

    if (
        PlaybackClipEndTimeSeconds - PlaybackClipStartTimeSeconds <=
        KINDA_SMALL_NUMBER
    )
    {
        PlaybackFrames.Reset();
        return false;
    }

    APlayerController* PlayerController =
        UGameplayStatics::GetPlayerController(this, 0);

    if (!IsValid(PlayerController))
    {
        PlaybackFrames.Reset();
        return false;
    }

    ReplayPlayerController = PlayerController;
    PreviousViewTarget = PlayerController->GetViewTarget();

    ActiveGoalLineSign =
        PlaybackReason == ESoccerInstantReplayPlaybackReason::Goal
            ? GoalLineSign
            : 0.0f;
    ActiveGoalReplayCameraTakeIndex = 0;

    if (!BuildReplayCameraForReason(PlaybackReason, ActiveGoalLineSign))
    {
        PlaybackFrames.Reset();
        ReplayPlayerController.Reset();
        PreviousViewTarget.Reset();
        ActiveGoalLineSign = 0.0f;
        ActiveGoalReplayCameraTakeIndex = 0;
        return false;
    }

    bRecordingWasEnabledBeforeReplay = bRecordingEnabled;
    SetRecordingEnabled(false);

    PlayerController->SetIgnoreMoveInput(true);
    bAppliedMoveInputIgnore = true;
    PlayerController->SetIgnoreLookInput(true);
    bAppliedLookInputIgnore = true;

    ActivePlaybackReason = PlaybackReason;
    bReplayPlaying = true;
    PlaybackFrameCursor = 0;
    PlaybackElapsedSeconds = 0.0;
    LastPlaybackRealTimeSeconds = FPlatformTime::Seconds();

    if (!UGameplayStatics::SetGamePaused(World, true))
    {
        FinishReplay(false);
        return false;
    }

    PrepareActorsForReplay();

    PlayerController->SetViewTarget(ReplayCamera.Get());
    RefreshReplayCameraView(0.0f);

    ApplyPlaybackTime(
        PlaybackClipStartTimeSeconds,
        0.0f
    );

    const TCHAR* ReplayReasonText = TEXT("Event");

    switch (ActivePlaybackReason)
    {
    case ESoccerInstantReplayPlaybackReason::Manual:
        ReplayReasonText = TEXT("Manual");
        break;
    case ESoccerInstantReplayPlaybackReason::Goal:
        ReplayReasonText = TEXT("Goal");
        break;
    case ESoccerInstantReplayPlaybackReason::Foul:
        ReplayReasonText = TEXT("Foul");
        break;
    case ESoccerInstantReplayPlaybackReason::Offside:
        ReplayReasonText = TEXT("Offside");
        break;
    default:
        break;
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[InstantReplay] %s replay started: %.2f s, %d frames."),
        ReplayReasonText,
        PlaybackClipEndTimeSeconds - PlaybackClipStartTimeSeconds,
        PlaybackFrames.Num()
    );

    if (ActivePlaybackReason == ESoccerInstantReplayPlaybackReason::Goal)
    {
        UE_LOG(
            LogTemp,
            Log,
            TEXT("[InstantReplay] Goal camera %d/%d: %s. NumPad 9 skips the replay."),
            ActiveGoalReplayCameraTakeIndex + 1,
            GoalReplayCameraTakeCount,
            GetCurrentGoalReplayCameraName()
        );
    }

    return true;
}

void ASoccerInstantReplayManager::AppendImmediatePlaybackEndpoint()
{
    ResolveTrackedBallIfNeeded();
    RefreshTrackedCharacters();

    FSoccerReplayFrame ImmediateFrame;
    CaptureCurrentWorldFrame(ImmediateFrame);

    if (PlaybackFrames.Num() > 0)
    {
        const double PreviousTime =
            PlaybackFrames.Last().RecordingTimeSeconds;

        if (ImmediateFrame.RecordingTimeSeconds <= PreviousTime + KINDA_SMALL_NUMBER)
        {
            const double SafeSampleInterval =
                1.0 / static_cast<double>(
                    FMath::Clamp(SamplesPerSecond, 5.0f, 60.0f)
                );

            ImmediateFrame.RecordingTimeSeconds =
                PreviousTime + SafeSampleInterval;
        }
    }

    PlaybackFrames.Add(MoveTemp(ImmediateFrame));
}

void ASoccerInstantReplayManager::StopManualReplay()
{
    if (
        !bReplayPlaying ||
        ActivePlaybackReason != ESoccerInstantReplayPlaybackReason::Manual
    )
    {
        return;
    }

    FinishReplay(true);
}

void ASoccerInstantReplayManager::TickReplayPlayback()
{
    if (!bReplayPlaying || PlaybackFrames.Num() < 2)
    {
        return;
    }

    const double CurrentRealTimeSeconds = FPlatformTime::Seconds();
    const double RawRealDelta =
        CurrentRealTimeSeconds - LastPlaybackRealTimeSeconds;
    LastPlaybackRealTimeSeconds = CurrentRealTimeSeconds;

    const double SafeRealDelta =
        FMath::Clamp(RawRealDelta, 0.0, 0.1);

    PlaybackElapsedSeconds += SafeRealDelta;

    const double TargetRecordingTime =
        PlaybackClipStartTimeSeconds + PlaybackElapsedSeconds;

    if (TargetRecordingTime >= PlaybackClipEndTimeSeconds)
    {
        ApplyPlaybackTime(
            PlaybackClipEndTimeSeconds,
            static_cast<float>(SafeRealDelta)
        );

        if (AdvanceGoalReplayCameraTake())
        {
            return;
        }

        FinishReplay(true);
        return;
    }

    ApplyPlaybackTime(
        TargetRecordingTime,
        static_cast<float>(SafeRealDelta)
    );
}

void ASoccerInstantReplayManager::PrepareActorsForReplay()
{
    for (const FSoccerReplayCharacterSample& Sample : LiveResumeFrame.Characters)
    {
        ASoccerCharacterBase* Character = Sample.Character.Get();

        if (IsValid(Character))
        {
            Character->SetActorEnableCollision(false);
        }
    }

    ASoccerBall* Ball = LiveResumeFrame.Ball.Ball.Get();

    if (!IsValid(Ball))
    {
        return;
    }

    Ball->SetActorEnableCollision(false);

    USceneComponent* BallRootComponent = Ball->GetRootComponent();
    UPrimitiveComponent* PrimitiveRoot =
        Cast<UPrimitiveComponent>(BallRootComponent);

    if (PrimitiveRoot != nullptr)
    {
        PrimitiveRoot->SetSimulatePhysics(false);
        PrimitiveRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
        PrimitiveRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }

    Ball->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}

void ASoccerInstantReplayManager::ApplyPlaybackTime(
    double TargetRecordingTimeSeconds,
    float VisualDeltaSeconds
)
{
    if (PlaybackFrames.Num() <= 0)
    {
        return;
    }

    while (
        PlaybackFrameCursor + 1 < PlaybackFrames.Num() - 1 &&
        PlaybackFrames[PlaybackFrameCursor + 1].RecordingTimeSeconds <
            TargetRecordingTimeSeconds
    )
    {
        ++PlaybackFrameCursor;
    }

    const int32 IndexA = FMath::Clamp(
        PlaybackFrameCursor,
        0,
        PlaybackFrames.Num() - 1
    );
    const int32 IndexB = FMath::Min(
        IndexA + 1,
        PlaybackFrames.Num() - 1
    );

    const FSoccerReplayFrame& FrameA = PlaybackFrames[IndexA];
    const FSoccerReplayFrame& FrameB = PlaybackFrames[IndexB];

    const double Interval =
        FrameB.RecordingTimeSeconds - FrameA.RecordingTimeSeconds;

    const float Alpha =
        Interval > KINDA_SMALL_NUMBER
            ? FMath::Clamp(
                static_cast<float>(
                    (TargetRecordingTimeSeconds -
                        FrameA.RecordingTimeSeconds) /
                    Interval
                ),
                0.0f,
                1.0f
            )
            : 0.0f;

    for (const FSoccerReplayCharacterSample& SampleA : FrameA.Characters)
    {
        ASoccerCharacterBase* Character = SampleA.Character.Get();

        if (!IsValid(Character))
        {
            continue;
        }

        const FSoccerReplayCharacterSample* SampleB =
            FindCharacterSample(FrameB, Character);

        ApplyCharacterPlaybackSample(
            SampleA,
            SampleB,
            Alpha,
            VisualDeltaSeconds,
            false
        );
    }

    ApplyBallPlaybackSample(
        FrameA.Ball,
        &FrameB.Ball,
        Alpha,
        false
    );

    UpdateReplayCameraAim();
    RefreshReplayCameraView(VisualDeltaSeconds);
}

const FSoccerReplayCharacterSample*
ASoccerInstantReplayManager::FindCharacterSample(
    const FSoccerReplayFrame& Frame,
    const ASoccerCharacterBase* Character
) const
{
    if (Character == nullptr)
    {
        return nullptr;
    }

    for (const FSoccerReplayCharacterSample& Sample : Frame.Characters)
    {
        if (Sample.Character.Get() == Character)
        {
            return &Sample;
        }
    }

    return nullptr;
}

void ASoccerInstantReplayManager::ApplyCharacterPlaybackSample(
    const FSoccerReplayCharacterSample& SampleA,
    const FSoccerReplayCharacterSample* SampleB,
    float Alpha,
    float VisualDeltaSeconds,
    bool bRestoreCollision
)
{
    ASoccerCharacterBase* Character = SampleA.Character.Get();

    if (!IsValid(Character))
    {
        return;
    }

    const FSoccerReplayCharacterSample& VisualSample =
        SampleB != nullptr && Alpha >= 0.5f
            ? *SampleB
            : SampleA;

    FTransform TargetTransform = SampleA.ActorTransform;
    FVector TargetVelocity = SampleA.Velocity;

    if (SampleB != nullptr)
    {
        const FVector InterpolatedLocation = FMath::Lerp(
            SampleA.ActorTransform.GetLocation(),
            SampleB->ActorTransform.GetLocation(),
            Alpha
        );

        const FQuat InterpolatedRotation = FQuat::Slerp(
            SampleA.ActorTransform.GetRotation(),
            SampleB->ActorTransform.GetRotation(),
            Alpha
        ).GetNormalized();

        const FVector InterpolatedScale = FMath::Lerp(
            SampleA.ActorTransform.GetScale3D(),
            SampleB->ActorTransform.GetScale3D(),
            Alpha
        );

        TargetTransform = FTransform(
            InterpolatedRotation,
            InterpolatedLocation,
            InterpolatedScale
        );

        TargetVelocity = FMath::Lerp(
            SampleA.Velocity,
            SampleB->Velocity,
            Alpha
        );
    }

    Character->SetActorTransform(
        TargetTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    if (bRestoreCollision)
    {
        Character->SetActorEnableCollision(
            VisualSample.bActorCollisionEnabled
        );
    }
    else
    {
        Character->SetActorEnableCollision(false);
    }

    if (UCharacterMovementComponent* Movement =
        Character->GetCharacterMovement())
    {
        Movement->Velocity = TargetVelocity;
        Movement->MovementMode =
            static_cast<EMovementMode>(VisualSample.MovementMode);
        Movement->CustomMovementMode =
            VisualSample.CustomMovementMode;
    }

    Character->ApplyInstantReplayVisualState(
        VisualSample.bPossessingBall,
        VisualSample.bChasingBall,
        VisualSample.bKicking,
        VisualSample.bForceDribbleTurnLocomotion,
        VisualSample.ForcedDribbleTurnLocomotionSpeed
    );

    ApplyCharacterMontageVisual(
        Character,
        VisualSample,
        VisualDeltaSeconds,
        bRestoreCollision
    );
}

void ASoccerInstantReplayManager::ApplyCharacterMontageVisual(
    ASoccerCharacterBase* Character,
    const FSoccerReplayCharacterSample& Sample,
    float VisualDeltaSeconds,
    bool bResumeLivePlayback
)
{
    if (!IsValid(Character))
    {
        return;
    }

    USkeletalMeshComponent* Mesh = Character->GetMesh();
    UAnimInstance* AnimInstance =
        Mesh != nullptr ? Mesh->GetAnimInstance() : nullptr;

    if (Mesh == nullptr || AnimInstance == nullptr)
    {
        return;
    }

    UAnimMontage* TargetMontage = Sample.ActiveMontage.Get();
    UAnimMontage* CurrentMontage =
        AnimInstance->GetCurrentActiveMontage();

    if (TargetMontage != nullptr)
    {
        if (
            CurrentMontage != TargetMontage ||
            !AnimInstance->Montage_IsActive(TargetMontage)
        )
        {
            AnimInstance->Montage_Stop(0.0f);
            AnimInstance->Montage_Play(
                TargetMontage,
                FMath::Max(0.01f, Sample.MontagePlayRate)
            );
        }

        AnimInstance->Montage_SetPosition(
            TargetMontage,
            FMath::Max(0.0f, Sample.MontagePositionSeconds)
        );

        if (bResumeLivePlayback)
        {
            AnimInstance->Montage_SetPlayRate(
                TargetMontage,
                FMath::Max(0.01f, Sample.MontagePlayRate)
            );

            if (Sample.bMontagePlaying)
            {
                AnimInstance->Montage_Resume(TargetMontage);
            }
            else
            {
                AnimInstance->Montage_Pause(TargetMontage);
            }
        }
        else
        {
            /*
             * Playback time is controlled by recorded samples, not by the
             * animation clock. Pausing prevents AnimNotifies from executing
             * gameplay actions while the replay is being viewed.
             */
            AnimInstance->Montage_Pause(TargetMontage);
        }
    }
    else if (CurrentMontage != nullptr)
    {
        AnimInstance->Montage_Stop(0.0f);
    }

    /*
     * The world itself is paused. Advance only the locomotion graph manually;
     * a replay montage remains paused and is positioned from the recording.
     */
    Mesh->TickAnimation(
        bResumeLivePlayback ? 0.0f : FMath::Max(0.0f, VisualDeltaSeconds),
        false
    );

    if (TargetMontage != nullptr && !bResumeLivePlayback)
    {
        AnimInstance->Montage_SetPosition(
            TargetMontage,
            FMath::Max(0.0f, Sample.MontagePositionSeconds)
        );
    }

    Mesh->RefreshBoneTransforms(nullptr);
}

void ASoccerInstantReplayManager::ApplyBallPlaybackSample(
    const FSoccerReplayBallSample& SampleA,
    const FSoccerReplayBallSample* SampleB,
    float Alpha,
    bool bRestoreLiveState
)
{
    ASoccerBall* Ball = SampleA.Ball.Get();

    if (!IsValid(Ball))
    {
        return;
    }

    FTransform TargetTransform = SampleA.ActorTransform;

    if (SampleB != nullptr && SampleB->Ball.Get() == Ball)
    {
        const FVector InterpolatedLocation = FMath::Lerp(
            SampleA.ActorTransform.GetLocation(),
            SampleB->ActorTransform.GetLocation(),
            Alpha
        );

        const FQuat InterpolatedRotation = FQuat::Slerp(
            SampleA.ActorTransform.GetRotation(),
            SampleB->ActorTransform.GetRotation(),
            Alpha
        ).GetNormalized();

        const FVector InterpolatedScale = FMath::Lerp(
            SampleA.ActorTransform.GetScale3D(),
            SampleB->ActorTransform.GetScale3D(),
            Alpha
        );

        TargetTransform = FTransform(
            InterpolatedRotation,
            InterpolatedLocation,
            InterpolatedScale
        );
    }

    USceneComponent* BallRootComponent = Ball->GetRootComponent();
    UPrimitiveComponent* PrimitiveRoot =
        Cast<UPrimitiveComponent>(BallRootComponent);

    if (!bRestoreLiveState)
    {
        if (PrimitiveRoot != nullptr && PrimitiveRoot->IsSimulatingPhysics())
        {
            PrimitiveRoot->SetSimulatePhysics(false);
        }

        if (Ball->GetAttachParentActor() != nullptr)
        {
            Ball->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        }

        Ball->SetActorEnableCollision(false);
        Ball->SetActorTransform(
            TargetTransform,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
        return;
    }

    if (PrimitiveRoot != nullptr)
    {
        PrimitiveRoot->SetSimulatePhysics(false);
    }

    Ball->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Ball->SetActorTransform(
        SampleA.ActorTransform,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    USceneComponent* BallParentComponent =
        SampleA.AttachedParentComponent.Get();

    if (IsValid(BallParentComponent))
    {
        Ball->AttachToComponent(
            BallParentComponent,
            FAttachmentTransformRules::KeepWorldTransform,
            SampleA.AttachedSocketName
        );
    }

    Ball->SetActorEnableCollision(SampleA.bActorCollisionEnabled);

    if (PrimitiveRoot != nullptr)
    {
        const bool bCanResumePhysics =
            SampleA.bSimulatingPhysics && !IsValid(BallParentComponent);

        PrimitiveRoot->SetSimulatePhysics(bCanResumePhysics);

        if (bCanResumePhysics)
        {
            PrimitiveRoot->SetPhysicsLinearVelocity(
                SampleA.LinearVelocity
            );
            PrimitiveRoot->SetPhysicsAngularVelocityInDegrees(
                SampleA.AngularVelocityDegrees
            );
        }
    }
}

bool ASoccerInstantReplayManager::BuildReplayCameraForReason(
    ESoccerInstantReplayPlaybackReason PlaybackReason,
    float GoalLineSign
)
{
    if (PlaybackReason == ESoccerInstantReplayPlaybackReason::Goal)
    {
        if (BuildGoalReplayCamera(GoalLineSign))
        {
            return true;
        }

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[InstantReplay] Goal camera setup failed; falling back to the fixed replay camera.")
        );
    }

    return BuildFixedReplayCamera();
}

bool ASoccerInstantReplayManager::BuildFixedReplayCamera()
{
    UWorld* World = GetWorld();

    if (World == nullptr || PlaybackFrames.Num() <= 0)
    {
        return false;
    }

    if (ReplayCamera.IsValid())
    {
        ReplayCamera->Destroy();
        ReplayCamera.Reset();
    }

    FVector FocusLocation = FVector::ZeroVector;
    int32 FocusSampleCount = 0;

    for (const FSoccerReplayFrame& Frame : PlaybackFrames)
    {
        if (Frame.Ball.Ball.IsValid())
        {
            FocusLocation += Frame.Ball.ActorTransform.GetLocation();
            ++FocusSampleCount;
        }
    }

    if (FocusSampleCount <= 0 && PlaybackFrames[0].Characters.Num() > 0)
    {
        for (
            const FSoccerReplayCharacterSample& Sample :
            PlaybackFrames[0].Characters
        )
        {
            if (Sample.Character.IsValid())
            {
                FocusLocation += Sample.ActorTransform.GetLocation();
                ++FocusSampleCount;
            }
        }
    }

    if (FocusSampleCount > 0)
    {
        FocusLocation /= static_cast<float>(FocusSampleCount);
    }

    FVector SideDirection = FVector::RightVector;

    const ASoccerMatchManager* Manager = MatchManager.Get();
    const ASoccerField* Field =
        IsValid(Manager) ? Manager->GetSoccerField() : nullptr;

    if (IsValid(Field))
    {
        SideDirection = Field->GetPitchWidthWorldDirection();
        SideDirection.Z = 0.0f;

        if (!SideDirection.Normalize())
        {
            SideDirection = FVector::RightVector;
        }
    }

    const FVector CameraLocation =
        FocusLocation - SideDirection * FMath::Max(500.0f, FixedCameraSideDistance) +
        FVector::UpVector * FMath::Max(200.0f, FixedCameraHeight);

    const FVector CameraAimLocation =
        FocusLocation + FVector::UpVector * 120.0f;

    const FRotator CameraRotation =
        (CameraAimLocation - CameraLocation).Rotation();

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ACameraActor* CameraActor = World->SpawnActor<ACameraActor>(
        CameraLocation,
        CameraRotation,
        SpawnParameters
    );

    if (!IsValid(CameraActor))
    {
        return false;
    }

    if (UCameraComponent* CameraComponent = CameraActor->GetCameraComponent())
    {
        CameraComponent->SetFieldOfView(
            FMath::Clamp(FixedCameraFOV, 30.0f, 120.0f)
        );
    }

    ReplayCamera = CameraActor;
    return true;
}

bool ASoccerInstantReplayManager::BuildGoalReplayCamera(float GoalLineSign)
{
    UWorld* World = GetWorld();
    const ASoccerMatchManager* Manager = MatchManager.Get();
    const ASoccerField* Field =
        IsValid(Manager) ? Manager->GetSoccerField() : nullptr;

    if (
        World == nullptr ||
        !IsValid(Field) ||
        PlaybackFrames.Num() <= 0
    )
    {
        return false;
    }

    float SafeGoalLineSign = 0.0f;

    if (FMath::Abs(GoalLineSign) >= 0.5f)
    {
        SafeGoalLineSign = GoalLineSign >= 0.0f ? 1.0f : -1.0f;
    }
    else if (PlaybackFrames.Last().Ball.Ball.IsValid())
    {
        SafeGoalLineSign = Field->GetNearestGoalLineSign(
            PlaybackFrames.Last().Ball.ActorTransform.GetLocation()
        );
    }
    else
    {
        return false;
    }

    ActiveGoalLineSign = SafeGoalLineSign;
    ActiveGoalReplayCameraTakeIndex = 0;

    if (ReplayCamera.IsValid())
    {
        ReplayCamera->Destroy();
        ReplayCamera.Reset();
    }

    const FVector GoalCenter =
        Field->GetGoalCenterWorldLocation(ActiveGoalLineSign, 0.0f);

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ACameraActor* ReplayCameraActor = World->SpawnActor<ACameraActor>(
        GoalCenter + FVector::UpVector * GoalReplayCameraHeight,
        FRotator::ZeroRotator,
        SpawnParameters
    );

    if (!IsValid(ReplayCameraActor))
    {
        return false;
    }

    if (UCameraComponent* ReplayCameraComponent =
        ReplayCameraActor->GetCameraComponent())
    {
        ReplayCameraComponent->SetFieldOfView(
            FMath::Clamp(GoalReplayCameraFOV, 30.0f, 120.0f)
        );
    }

    ReplayCamera = ReplayCameraActor;

    if (!ConfigureCurrentGoalReplayCamera())
    {
        ReplayCameraActor->Destroy();
        ReplayCamera.Reset();
        return false;
    }

    return true;
}

bool ASoccerInstantReplayManager::ConfigureCurrentGoalReplayCamera()
{
    ACameraActor* ReplayCameraActor = ReplayCamera.Get();
    const ASoccerMatchManager* Manager = MatchManager.Get();
    const ASoccerField* Field =
        IsValid(Manager) ? Manager->GetSoccerField() : nullptr;

    if (
        !IsValid(ReplayCameraActor) ||
        !IsValid(Field) ||
        FMath::Abs(ActiveGoalLineSign) < 0.5f
    )
    {
        return false;
    }

    const FVector GoalCenter =
        Field->GetGoalCenterWorldLocation(ActiveGoalLineSign, 0.0f);

    FVector TowardGoalDirection = Field->GetPitchLengthWorldDirection();
    TowardGoalDirection.Z = 0.0f;

    if (!TowardGoalDirection.Normalize())
    {
        return false;
    }

    TowardGoalDirection *= ActiveGoalLineSign >= 0.0f ? 1.0f : -1.0f;

    const FVector InfieldDirection = -TowardGoalDirection;

    FVector AttackerRightDirection = FVector::CrossProduct(
        FVector::UpVector,
        TowardGoalDirection
    );
    AttackerRightDirection.Z = 0.0f;

    if (!AttackerRightDirection.Normalize())
    {
        AttackerRightDirection = Field->GetPitchWidthWorldDirection();
        AttackerRightDirection.Z = 0.0f;

        if (!AttackerRightDirection.Normalize())
        {
            return false;
        }
    }

    const FVector AttackerLeftDirection = -AttackerRightDirection;
    const float SafeHeight = FMath::Max(200.0f, GoalReplayCameraHeight);

    FVector ReplayCameraLocation = GoalCenter;

    switch (ActiveGoalReplayCameraTakeIndex)
    {
    case 0: // Left side from the attacker's perspective.
        ReplayCameraLocation +=
            AttackerLeftDirection * FMath::Max(500.0f, GoalReplaySideDistance);
        ReplayCameraLocation +=
            InfieldDirection * FMath::Max(0.0f, GoalReplaySideInfieldOffset);
        ReplayCameraLocation += FVector::UpVector * SafeHeight;
        break;

    case 1: // Right side from the attacker's perspective.
        ReplayCameraLocation +=
            AttackerRightDirection * FMath::Max(500.0f, GoalReplaySideDistance);
        ReplayCameraLocation +=
            InfieldDirection * FMath::Max(0.0f, GoalReplaySideInfieldOffset);
        ReplayCameraLocation += FVector::UpVector * SafeHeight;
        break;

    case 2: // In front of the goal, inside the pitch.
        ReplayCameraLocation +=
            InfieldDirection * FMath::Max(500.0f, GoalReplayFrontDistance);
        ReplayCameraLocation += FVector::UpVector * SafeHeight;
        break;

    case 3: // Behind the goal, outside the pitch.
    default:
        ReplayCameraLocation +=
            TowardGoalDirection * FMath::Max(500.0f, GoalReplayBehindDistance);
        ReplayCameraLocation += FVector::UpVector * (SafeHeight * 0.85f);
        break;
    }

    ReplayCameraActor->SetActorLocation(
        ReplayCameraLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    const FVector InitialAimLocation =
        GoalCenter + FVector::UpVector * 120.0f;

    ReplayCameraActor->SetActorRotation(
        (InitialAimLocation - ReplayCameraLocation).Rotation()
    );

    if (UCameraComponent* ReplayCameraComponent =
        ReplayCameraActor->GetCameraComponent())
    {
        ReplayCameraComponent->SetFieldOfView(
            FMath::Clamp(GoalReplayCameraFOV, 30.0f, 120.0f)
        );
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[InstantReplay] Goal camera transform %d/%d (%s): Location=(%.1f, %.1f, %.1f)."),
        ActiveGoalReplayCameraTakeIndex + 1,
        GoalReplayCameraTakeCount,
        GetCurrentGoalReplayCameraName(),
        ReplayCameraLocation.X,
        ReplayCameraLocation.Y,
        ReplayCameraLocation.Z
    );

    return true;
}

bool ASoccerInstantReplayManager::AdvanceGoalReplayCameraTake()
{
    if (
        ActivePlaybackReason != ESoccerInstantReplayPlaybackReason::Goal ||
        ActiveGoalReplayCameraTakeIndex + 1 >= GoalReplayCameraTakeCount
    )
    {
        return false;
    }

    ++ActiveGoalReplayCameraTakeIndex;

    if (!ConfigureCurrentGoalReplayCamera())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[InstantReplay] Could not configure goal replay camera take %d/%d."),
            ActiveGoalReplayCameraTakeIndex + 1,
            GoalReplayCameraTakeCount
        );
        return false;
    }

    PlaybackFrameCursor = 0;
    PlaybackElapsedSeconds = 0.0;
    LastPlaybackRealTimeSeconds = FPlatformTime::Seconds();

    ApplyPlaybackTime(
        PlaybackClipStartTimeSeconds,
        0.0f
    );

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[InstantReplay] Goal camera %d/%d: %s."),
        ActiveGoalReplayCameraTakeIndex + 1,
        GoalReplayCameraTakeCount,
        GetCurrentGoalReplayCameraName()
    );

    return true;
}

void ASoccerInstantReplayManager::UpdateReplayCameraAim()
{
    if (
        ActivePlaybackReason != ESoccerInstantReplayPlaybackReason::Goal ||
        !ReplayCamera.IsValid()
    )
    {
        return;
    }

    const ASoccerMatchManager* Manager = MatchManager.Get();
    const ASoccerField* Field =
        IsValid(Manager) ? Manager->GetSoccerField() : nullptr;

    if (!IsValid(Field) || FMath::Abs(ActiveGoalLineSign) < 0.5f)
    {
        return;
    }

    const FVector GoalAimLocation =
        Field->GetGoalCenterWorldLocation(ActiveGoalLineSign, 120.0f);

    FVector ReplayAimLocation = GoalAimLocation;

    const ASoccerBall* Ball = SoccerBall.Get();
    if (IsValid(Ball))
    {
        const FVector BallAimLocation =
            Ball->GetActorLocation() + FVector::UpVector * 60.0f;

        // Keep the goal in the composition while still following the ball.
        ReplayAimLocation = FMath::Lerp(
            GoalAimLocation,
            BallAimLocation,
            0.58f
        );
    }

    ACameraActor* ReplayCameraActor = ReplayCamera.Get();
    const FVector ReplayCameraLocation = ReplayCameraActor->GetActorLocation();
    const FVector AimDelta = ReplayAimLocation - ReplayCameraLocation;

    if (!AimDelta.IsNearlyZero())
    {
        ReplayCameraActor->SetActorRotation(AimDelta.Rotation());
    }
}

void ASoccerInstantReplayManager::RefreshReplayCameraView(float RealDeltaSeconds)
{
    APlayerController* PlayerController = ReplayPlayerController.Get();
    ACameraActor* ReplayCameraActor = ReplayCamera.Get();

    if (!IsValid(PlayerController) || !IsValid(ReplayCameraActor))
    {
        return;
    }

    // The replay world is intentionally paused. The replay manager itself ticks
    // while paused, but PlayerCameraManager may otherwise keep the POV cached
    // from the first take. Force its camera cache to be rebuilt from the current
    // replay-camera transform on every visual replay step.
    if (PlayerController->GetViewTarget() != ReplayCameraActor)
    {
        PlayerController->SetViewTarget(ReplayCameraActor);
    }

    APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;

    if (IsValid(CameraManager))
    {
        CameraManager->UpdateCamera(FMath::Max(0.0f, RealDeltaSeconds));
    }
}

const TCHAR* ASoccerInstantReplayManager::GetCurrentGoalReplayCameraName() const
{
    switch (ActiveGoalReplayCameraTakeIndex)
    {
    case 0:
        return TEXT("Left");
    case 1:
        return TEXT("Right");
    case 2:
        return TEXT("Front");
    case 3:
        return TEXT("Behind");
    default:
        return TEXT("Unknown");
    }
}

void ASoccerInstantReplayManager::RestoreLiveStateAfterReplay()
{
    for (const FSoccerReplayCharacterSample& LiveSample : LiveResumeFrame.Characters)
    {
        ApplyCharacterPlaybackSample(
            LiveSample,
            nullptr,
            0.0f,
            0.0f,
            true
        );
    }

    ApplyBallPlaybackSample(
        LiveResumeFrame.Ball,
        nullptr,
        0.0f,
        true
    );
}

void ASoccerInstantReplayManager::FinishReplay(bool bRestoreLiveState)
{
    if (!bReplayPlaying)
    {
        return;
    }

    UWorld* World = GetWorld();
    APlayerController* PlayerController = ReplayPlayerController.Get();

    if (bRestoreLiveState)
    {
        RestoreLiveStateAfterReplay();
    }

    if (IsValid(PlayerController))
    {
        AActor* OldViewTarget = PreviousViewTarget.Get();

        if (IsValid(OldViewTarget))
        {
            PlayerController->SetViewTarget(OldViewTarget);
        }

        if (bAppliedMoveInputIgnore)
        {
            PlayerController->SetIgnoreMoveInput(false);
        }

        if (bAppliedLookInputIgnore)
        {
            PlayerController->SetIgnoreLookInput(false);
        }
    }

    bAppliedMoveInputIgnore = false;
    bAppliedLookInputIgnore = false;

    if (ReplayCamera.IsValid())
    {
        ReplayCamera->Destroy();
        ReplayCamera.Reset();
    }

    const ESoccerInstantReplayPlaybackReason FinishedPlaybackReason =
        ActivePlaybackReason;

    bReplayPlaying = false;
    ActivePlaybackReason = ESoccerInstantReplayPlaybackReason::None;

    if (World != nullptr && UGameplayStatics::IsGamePaused(World))
    {
        UGameplayStatics::SetGamePaused(World, false);
    }

    SetRecordingEnabled(bRecordingWasEnabledBeforeReplay);

    PlaybackFrames.Reset();
    LiveResumeFrame = FSoccerReplayFrame();
    PlaybackFrameCursor = 0;
    PlaybackClipStartTimeSeconds = 0.0;
    PlaybackClipEndTimeSeconds = 0.0;
    PlaybackElapsedSeconds = 0.0;
    LastPlaybackRealTimeSeconds = 0.0;
    ActiveGoalLineSign = 0.0f;
    ActiveGoalReplayCameraTakeIndex = 0;
    ReplayPlayerController.Reset();
    PreviousViewTarget.Reset();

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[InstantReplay] %s replay finished. Live match state restored."),
        FinishedPlaybackReason == ESoccerInstantReplayPlaybackReason::Goal
            ? TEXT("Goal")
            : (FinishedPlaybackReason == ESoccerInstantReplayPlaybackReason::Manual
                ? TEXT("Manual")
                : TEXT("Event"))
    );
}
