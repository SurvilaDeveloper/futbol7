//SoccerCharacterBase.cpp
#include "SoccerCharacterBase.h"
#include "SoccerPlayerProfile.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "SoccerBall.h"
#include "SoccerMatchManager.h"
#include "SoccerDebugManager.h"
#include "SoccerAICharacter.h"
#include "SoccerFieldDimensions.h"
#include "SoccerField.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Controller.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/CurveTable.h"
#include "Curves/RealCurve.h"
#include "Curves/CurveFloat.h"

namespace
{
    float SoccerPointSegmentDistanceSquared(
        const FVector& Point,
        const FVector& SegmentStart,
        const FVector& SegmentEnd,
        FVector* OutClosestPoint = nullptr
    )
    {
        const FVector Segment = SegmentEnd - SegmentStart;
        const float SegmentLengthSquared = Segment.SizeSquared();

        float Alpha = 0.0f;

        if (SegmentLengthSquared > KINDA_SMALL_NUMBER)
        {
            Alpha = FMath::Clamp(
                FVector::DotProduct(Point - SegmentStart, Segment) /
                    SegmentLengthSquared,
                0.0f,
                1.0f
            );
        }

        const FVector ClosestPoint =
            SegmentStart + Segment * Alpha;

        if (OutClosestPoint != nullptr)
        {
            *OutClosestPoint = ClosestPoint;
        }

        return FVector::DistSquared(Point, ClosestPoint);
    }

    float SoccerMovingPointClosestDistanceSquared(
        const FVector& PointStart,
        const FVector& PointEnd,
        const FVector& OtherStart,
        const FVector& OtherEnd,
        float& OutAlpha,
        FVector& OutPointLocation,
        FVector& OutOtherLocation
    )
    {
        const FVector RelativeStart = PointStart - OtherStart;
        const FVector RelativeDelta =
            (PointEnd - PointStart) -
            (OtherEnd - OtherStart);

        const float RelativeDeltaSquared =
            RelativeDelta.SizeSquared();

        OutAlpha = 0.0f;

        if (RelativeDeltaSquared > KINDA_SMALL_NUMBER)
        {
            OutAlpha = FMath::Clamp(
                -FVector::DotProduct(
                    RelativeStart,
                    RelativeDelta
                ) / RelativeDeltaSquared,
                0.0f,
                1.0f
            );
        }

        OutPointLocation = FMath::Lerp(
            PointStart,
            PointEnd,
            OutAlpha
        );

        OutOtherLocation = FMath::Lerp(
            OtherStart,
            OtherEnd,
            OutAlpha
        );

        return FVector::DistSquared(
            OutPointLocation,
            OutOtherLocation
        );
    }

    float SoccerSegmentSegmentDistanceSquared(
        const FVector& SegmentAStart,
        const FVector& SegmentAEnd,
        const FVector& SegmentBStart,
        const FVector& SegmentBEnd,
        float& OutAlphaA,
        float& OutAlphaB,
        FVector& OutClosestA,
        FVector& OutClosestB
    )
    {
        const FVector DirectionA = SegmentAEnd - SegmentAStart;
        const FVector DirectionB = SegmentBEnd - SegmentBStart;
        const FVector StartOffset = SegmentAStart - SegmentBStart;

        const float A = FVector::DotProduct(DirectionA, DirectionA);
        const float E = FVector::DotProduct(DirectionB, DirectionB);
        const float F = FVector::DotProduct(DirectionB, StartOffset);

        float S = 0.0f;
        float T = 0.0f;

        if (A <= KINDA_SMALL_NUMBER && E <= KINDA_SMALL_NUMBER)
        {
            OutAlphaA = 0.0f;
            OutAlphaB = 0.0f;
            OutClosestA = SegmentAStart;
            OutClosestB = SegmentBStart;
            return FVector::DistSquared(OutClosestA, OutClosestB);
        }

        if (A <= KINDA_SMALL_NUMBER)
        {
            S = 0.0f;
            T = FMath::Clamp(F / FMath::Max(E, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
        }
        else
        {
            const float C = FVector::DotProduct(DirectionA, StartOffset);

            if (E <= KINDA_SMALL_NUMBER)
            {
                T = 0.0f;
                S = FMath::Clamp(-C / A, 0.0f, 1.0f);
            }
            else
            {
                const float B = FVector::DotProduct(DirectionA, DirectionB);
                const float Denominator = A * E - B * B;

                if (FMath::Abs(Denominator) > KINDA_SMALL_NUMBER)
                {
                    S = FMath::Clamp((B * F - C * E) / Denominator, 0.0f, 1.0f);
                }
                else
                {
                    S = 0.0f;
                }

                T = (B * S + F) / E;

                if (T < 0.0f)
                {
                    T = 0.0f;
                    S = FMath::Clamp(-C / A, 0.0f, 1.0f);
                }
                else if (T > 1.0f)
                {
                    T = 1.0f;
                    S = FMath::Clamp((B - C) / A, 0.0f, 1.0f);
                }
            }
        }

        OutAlphaA = S;
        OutAlphaB = T;
        OutClosestA = SegmentAStart + DirectionA * S;
        OutClosestB = SegmentBStart + DirectionB * T;

        return FVector::DistSquared(OutClosestA, OutClosestB);
    }

    FVector SoccerClampHorizontalDirectionToYaw(
        const FVector& ReferenceDirection,
        const FVector& DesiredDirection,
        float MaximumAngleDegrees
    )
    {
        FVector SafeReference = ReferenceDirection;
        SafeReference.Z = 0.0f;
        SafeReference = SafeReference.GetSafeNormal();

        FVector SafeDesired = DesiredDirection;
        SafeDesired.Z = 0.0f;
        SafeDesired = SafeDesired.GetSafeNormal();

        if (SafeReference.IsNearlyZero())
        {
            return SafeDesired;
        }

        if (SafeDesired.IsNearlyZero())
        {
            return SafeReference;
        }

        const float ReferenceYaw = SafeReference.Rotation().Yaw;
        const float DesiredYaw = SafeDesired.Rotation().Yaw;

        const float DeltaYaw = FMath::FindDeltaAngleDegrees(
            ReferenceYaw,
            DesiredYaw
        );

        const float ClampedDeltaYaw = FMath::Clamp(
            DeltaYaw,
            -FMath::Max(0.0f, MaximumAngleDegrees),
            FMath::Max(0.0f, MaximumAngleDegrees)
        );

        return FRotator(
            0.0f,
            ReferenceYaw + ClampedDeltaYaw,
            0.0f
        ).Vector().GetSafeNormal();
    }
}

ASoccerCharacterBase::ASoccerCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	GetCapsuleComponent()->SetCollisionResponseToChannel(
		ECC_Camera,
		ECR_Ignore
	);

	GetMesh()->SetCollisionResponseToChannel(
		ECC_Camera,
		ECR_Ignore
	);

	// La pelota no debe rebotar f�sicamente contra la c�psula del jugador.
	// La interacci�n pelota-jugador la vamos a manejar por radio/intenci�n.
	GetCapsuleComponent()->SetCollisionResponseToChannel(
		ECC_PhysicsBody,
		ECR_Ignore
	);

	// La pelota tampoco debe rebotar contra la malla visual del jugador.
	GetMesh()->SetCollisionResponseToChannel(
		ECC_PhysicsBody,
		ECR_Ignore
	);

	StandingAerialControlProfile.ActionType =
		ESoccerAerialActionType::StandingControl;
	StandingAerialControlProfile.ExpectedMontageDuration = 1.90f;
	StandingAerialControlProfile.ContactWindowStart = 0.63f;
	StandingAerialControlProfile.IdealContactTime = 0.78f;
	StandingAerialControlProfile.ContactWindowEnd = 0.90f;
	StandingAerialControlProfile.MinimumBallHeight = 105.0f;
	StandingAerialControlProfile.MaximumBallHeight = 185.0f;
	StandingAerialControlProfile.ContactForwardOffset = 24.0f;
	StandingAerialControlProfile.PreparationReachRadius = 18.0f;
	StandingAerialControlProfile.PreparationSettleTime = 0.10f;
	StandingAerialControlProfile.MaximumLateStartTime = 0.12f;
	StandingAerialControlProfile.TakeoffTime = 0.0f;
	StandingAerialControlProfile.ApexTime = 0.0f;
	StandingAerialControlProfile.LandingTime = 0.0f;

	JumpHeaderKickProfile.ActionType =
		ESoccerAerialActionType::JumpHeaderKick;
	JumpHeaderKickProfile.ExpectedMontageDuration = 2.366667f;
	JumpHeaderKickProfile.ContactWindowStart = 1.13f;
	JumpHeaderKickProfile.IdealContactTime = 1.23f;
	JumpHeaderKickProfile.ContactWindowEnd = 1.30f;
	JumpHeaderKickProfile.MinimumBallHeight = 155.0f;
	JumpHeaderKickProfile.MaximumBallHeight = 235.0f;
	JumpHeaderKickProfile.ContactForwardOffset = 24.0f;
	JumpHeaderKickProfile.PreparationReachRadius = 50.0f;
	JumpHeaderKickProfile.PreparationSettleTime = 0.08f;
	JumpHeaderKickProfile.MaximumLateStartTime = 0.10f;
	JumpHeaderKickProfile.TakeoffTime = 0.67f;
	JumpHeaderKickProfile.ApexTime = 1.15f;
	JumpHeaderKickProfile.LandingTime = 1.48f;

	JumpHeaderBlockProfile.ActionType =
		ESoccerAerialActionType::JumpHeaderBlock;
	JumpHeaderBlockProfile.ExpectedMontageDuration = 2.366667f;
	JumpHeaderBlockProfile.ContactWindowStart = 1.07f;
	JumpHeaderBlockProfile.IdealContactTime = 1.20f;
	JumpHeaderBlockProfile.ContactWindowEnd = 1.37f;
	JumpHeaderBlockProfile.MinimumBallHeight = 150.0f;
	JumpHeaderBlockProfile.MaximumBallHeight = 240.0f;
	JumpHeaderBlockProfile.ContactForwardOffset = 24.0f;
	JumpHeaderBlockProfile.PreparationReachRadius = 55.0f;
	JumpHeaderBlockProfile.PreparationSettleTime = 0.06f;
	JumpHeaderBlockProfile.MaximumLateStartTime = 0.14f;
	JumpHeaderBlockProfile.TakeoffTime = 0.67f;
	JumpHeaderBlockProfile.ApexTime = 1.15f;
	JumpHeaderBlockProfile.LandingTime = 1.48f;
}

void ASoccerCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Capture the mesh configured by the native/Blueprint character before a
	// PlayerProfile is allowed to replace it.
	CapturePlayerProfileAppearanceBaseline();
	ApplyPlayerProfileAppearance();

	// Acceleration is shared by human and AI CharacterMovement. Pace and
	// fatigue remain in their existing class-specific movement systems.
	ApplyPlayerProfileAccelerationTuning();

	ApplyTeamUniform();
}

void ASoccerCharacterBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyTeamUniform();
}

void ASoccerCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateSoccerAnimationState();
	UpdateGroundInterceptionPredictionDebug(DeltaTime);
	UpdateTackle();
	UpdateTackleFallReaction();
	UpdateTackleEvasion(DeltaTime);
	UpdateAerialAction(DeltaTime);
	UpdateAerialDebugAutoStart(DeltaTime);
}


bool ASoccerCharacterBase::TryStartTackle(
    ESoccerTackleSide TackleSide,
    const FVector& DesiredWorldDirection
)
{
    if (
        !bEnableTackle ||
        IsTackleActive() ||
        IsTackleFallReactionActive() ||
        IsTackleEvasionActive() ||
        IsAerialActionQueuedOrPlaying()
    )
    {
        return false;
    }

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    UAnimInstance* AnimInstance =
        GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
    UAnimMontage* Montage = GetTackleMontage(TackleSide);

    if (
        Movement == nullptr ||
        AnimInstance == nullptr ||
        Montage == nullptr ||
        Movement->IsFalling()
    )
    {
        return false;
    }

    FVector DesiredDirection = DesiredWorldDirection;
    DesiredDirection.Z = 0.0f;

    if (DesiredDirection.IsNearlyZero())
    {
        DesiredDirection = GetVelocity();
        DesiredDirection.Z = 0.0f;
    }

    if (DesiredDirection.IsNearlyZero())
    {
        DesiredDirection = GetActorForwardVector();
        DesiredDirection.Z = 0.0f;
    }

    DesiredDirection = DesiredDirection.GetSafeNormal();

    ActiveTackleDirection = SoccerClampHorizontalDirectionToYaw(
        GetActorForwardVector(),
        DesiredDirection,
        TackleMaximumStartTurnAngle
    );

    if (ActiveTackleDirection.IsNearlyZero())
    {
        ActiveTackleDirection = GetActorForwardVector().GetSafeNormal2D();
    }

    const float CurrentHorizontalSpeed = GetVelocity().Size2D();
    ActiveTackleInitialSpeed = FMath::Clamp(
        FMath::Max(CurrentHorizontalSpeed, TackleMinimumInitialSpeed) *
            FMath::Max(0.0f, TackleInitialSpeedScale),
        0.0f,
        FMath::Max(TackleMinimumInitialSpeed, TackleMaximumInitialSpeed)
    );

    if (Controller != nullptr)
    {
        Controller->StopMovement();
    }

    SavedTackleGroundFriction = Movement->GroundFriction;
    SavedTackleBrakingDecelerationWalking =
        Movement->BrakingDecelerationWalking;
    SavedTackleMaxWalkSpeed = Movement->MaxWalkSpeed;
    bTackleMovementSettingsSaved = true;

    Movement->GroundFriction = 0.0f;
    Movement->BrakingDecelerationWalking = 0.0f;
    Movement->MaxWalkSpeed = FMath::Max(
        Movement->MaxWalkSpeed,
        ActiveTackleInitialSpeed
    );

    SetActorRotation(ActiveTackleDirection.Rotation());

    const float PlayedDuration = AnimInstance->Montage_Play(Montage, 1.0f);

    if (PlayedDuration <= 0.0f)
    {
        FinishTackle();
        return false;
    }

    ActiveTackleSide = TackleSide;
    ActiveTackleNormalizedTime = 0.0f;
    TacklePhase = ESoccerTacklePhase::Sliding;

    ResetTackleContactTracking();
    InitializeTackleContactTracking();

    FVector NewVelocity = ActiveTackleDirection * ActiveTackleInitialSpeed;
    NewVelocity.Z = Movement->Velocity.Z;
    Movement->Velocity = NewVelocity;

    return true;
}

bool ASoccerCharacterBase::TryStartTackleTowardLocation(
    const FVector& TargetLocation
)
{
    FVector DesiredDirection = TargetLocation - GetActorLocation();
    DesiredDirection.Z = 0.0f;

    return TryStartTackle(
        ChooseTackleSideForTarget(TargetLocation),
        DesiredDirection
    );
}

void ASoccerCharacterBase::CancelTackle()
{
    if (!IsTackleActive())
    {
        return;
    }

    UAnimInstance* AnimInstance =
        GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
    UAnimMontage* Montage = GetTackleMontage(ActiveTackleSide);

    if (AnimInstance != nullptr && Montage != nullptr)
    {
        AnimInstance->Montage_Stop(0.12f, Montage);
    }

    FinishTackle();
}

bool ASoccerCharacterBase::IsTackleActive() const
{
    return TacklePhase != ESoccerTacklePhase::Inactive;
}

ESoccerTacklePhase ASoccerCharacterBase::GetTacklePhase() const
{
    return TacklePhase;
}

float ASoccerCharacterBase::GetTackleNormalizedTime() const
{
    return ActiveTackleNormalizedTime;
}


FVector ASoccerCharacterBase::GetActiveTackleDirection() const
{
    return IsTackleActive()
        ? ActiveTackleDirection.GetSafeNormal2D()
        : FVector::ZeroVector;
}

float ASoccerCharacterBase::GetCurrentTackleHorizontalSpeed() const
{
    if (!IsTackleActive())
    {
        return 0.0f;
    }

    return ActiveTackleInitialSpeed *
        EvaluateTackleSpeedAlpha(ActiveTackleNormalizedTime);
}

bool ASoccerCharacterBase::TryStartRunningJump(
    ESoccerTackleEvasionSide JumpSide
)
{
    if (
        !bEnableTackleEvasion ||
        IsTackleEvasionActive() ||
        IsTackleActive() ||
        IsTackleFallReactionActive() ||
        IsAerialActionQueuedOrPlaying()
    )
    {
        return false;
    }

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    UAnimInstance* AnimInstance =
        GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;

    if (
        Movement == nullptr ||
        AnimInstance == nullptr ||
        Movement->IsFalling()
    )
    {
        return false;
    }

    UAnimMontage* Montage = GetTackleEvasionMontage(JumpSide);
    if (Montage == nullptr)
    {
        ASoccerDebugManager::Message(
            this,
            ESoccerDebugCategory::Tackle,
            FString::Printf(
                TEXT("RUNNING JUMP %s: falta montage %s"),
                *GetName(),
                JumpSide == ESoccerTackleEvasionSide::Left
                    ? TEXT("running_jump_left_leg_in_place")
                    : TEXT("running_jump_right_leg_in_place")
            ),
            FColor::Yellow
        );
        return false;
    }

    FVector PreJumpVelocity = GetVelocity();
    PreJumpVelocity.Z = 0.0f;

    ActiveTackleEvasionInitialSpeed = PreJumpVelocity.Size2D();
    ActiveTackleEvasionDirection = PreJumpVelocity.GetSafeNormal2D();

    if (ActiveTackleEvasionDirection.IsNearlyZero())
    {
        ActiveTackleEvasionDirection =
            GetActorForwardVector().GetSafeNormal2D();
    }

    if (Controller != nullptr)
    {
        bSavedTackleEvasionIgnoreMoveInput = Controller->IsMoveInputIgnored();
        bSavedTackleEvasionIgnoreMoveInputValid = true;
        Controller->SetIgnoreMoveInput(true);
        Controller->StopMovement();
    }

    SavedTackleEvasionGroundFriction = Movement->GroundFriction;
    SavedTackleEvasionBrakingDecelerationWalking =
        Movement->BrakingDecelerationWalking;
    SavedTackleEvasionMaxWalkSpeed = Movement->MaxWalkSpeed;
    bTackleEvasionMovementSettingsSaved = true;

    Movement->GroundFriction = 0.0f;
    Movement->BrakingDecelerationWalking = 0.0f;
    Movement->MaxWalkSpeed = FMath::Max(
        Movement->MaxWalkSpeed,
        ActiveTackleEvasionInitialSpeed
    );

    const float PlayedDuration = AnimInstance->Montage_Play(Montage, 1.0f);
    if (PlayedDuration <= 0.0f)
    {
        FinishTackleEvasion();
        return false;
    }

    ActiveTackleEvasionSide = JumpSide;
    ActiveTackleEvasionNormalizedTime = 0.0f;
    bTackleEvasionActive = true;

    FVector NewVelocity =
        ActiveTackleEvasionDirection * ActiveTackleEvasionInitialSpeed;
    NewVelocity.Z = Movement->Velocity.Z;
    Movement->Velocity = NewVelocity;

    ASoccerDebugManager::Message(
        this,
        ESoccerDebugCategory::Tackle,
        FString::Printf(
            TEXT("RUNNING JUMP %s: %s"),
            *GetName(),
            JumpSide == ESoccerTackleEvasionSide::Left
                ? TEXT("LEFT")
                : TEXT("RIGHT")
        ),
        FColor::Green
    );

    return true;
}

bool ASoccerCharacterBase::TryStartTackleEvasion(
    ASoccerCharacterBase* TackleInstigator
)
{
    if (
        !IsValid(TackleInstigator) ||
        TackleInstigator == this ||
        TackleInstigator->GetTeam() == GetTeam()
    )
    {
        return false;
    }

    return TryStartRunningJump(
        ChooseTackleEvasionSide(TackleInstigator)
    );
}

void ASoccerCharacterBase::CancelTackleEvasion()
{
    if (!IsTackleEvasionActive())
    {
        return;
    }

    UAnimInstance* AnimInstance =
        GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
    UAnimMontage* Montage =
        GetTackleEvasionMontage(ActiveTackleEvasionSide);

    if (AnimInstance != nullptr && Montage != nullptr)
    {
        AnimInstance->Montage_Stop(0.08f, Montage);
    }

    FinishTackleEvasion();
}

bool ASoccerCharacterBase::IsTackleEvasionActive() const
{
    return bTackleEvasionActive;
}

bool ASoccerCharacterBase::IsTackleEvasionAvoidingLowContact() const
{
    if (!IsTackleEvasionActive())
    {
        return false;
    }

    const float Start = FMath::Clamp(
        TackleEvasionAirborneStartNormalizedTime,
        0.0f,
        1.0f
    );
    const float End = FMath::Clamp(
        FMath::Max(Start, TackleEvasionAirborneEndNormalizedTime),
        Start,
        1.0f
    );

    return
        ActiveTackleEvasionNormalizedTime >= Start &&
        ActiveTackleEvasionNormalizedTime <= End;
}

FSoccerTackleContactSummary ASoccerCharacterBase::GetTackleContactSummary() const
{
    return IsTackleActive()
        ? CurrentTackleContactSummary
        : LastTackleContactSummary;
}

ESoccerTackleContactOrder ASoccerCharacterBase::GetTackleContactOrder() const
{
    const FSoccerTackleContactSummary Summary = GetTackleContactSummary();

    if (!Summary.bBallContactOccurred && !Summary.bOpponentContactOccurred)
    {
        return ESoccerTackleContactOrder::None;
    }

    if (Summary.bBallContactOccurred && !Summary.bOpponentContactOccurred)
    {
        return ESoccerTackleContactOrder::BallOnly;
    }

    if (!Summary.bBallContactOccurred && Summary.bOpponentContactOccurred)
    {
        return ESoccerTackleContactOrder::OpponentOnly;
    }

    const float Difference =
        Summary.BallContactNormalizedTime -
        Summary.OpponentContactNormalizedTime;

    if (
        FMath::Abs(Difference) <=
        FMath::Max(0.0f, TackleNearlySimultaneousNormalizedTolerance)
    )
    {
        return ESoccerTackleContactOrder::NearlySimultaneous;
    }

    return Difference < 0.0f
        ? ESoccerTackleContactOrder::BallFirst
        : ESoccerTackleContactOrder::OpponentFirst;
}


bool ASoccerCharacterBase::TryStartTackleFallReaction(
    ASoccerCharacterBase* TackleInstigator,
    const FVector& ContactLocation
)
{
    if (
        !bEnableTackleFallReaction ||
        IsTackleFallReactionActive() ||
        TackleInstigator == nullptr ||
        TackleInstigator == this
    )
    {
        return false;
    }

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    UAnimInstance* AnimInstance =
        GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;

    if (
        Movement == nullptr ||
        AnimInstance == nullptr ||
        Movement->IsFalling()
    )
    {
        return false;
    }

    const ESoccerTackleFallSide FallSide =
        ChooseTackleFallSide(TackleInstigator, ContactLocation);

    UAnimMontage* Montage = GetTackleFallMontage(FallSide);
    if (Montage == nullptr)
    {
        ASoccerDebugManager::Message(
            this,
            ESoccerDebugCategory::Tackle,
            FString::Printf(
                TEXT("TACKLE FALL %s: falta montage %s"),
                *GetName(),
                FallSide == ESoccerTackleFallSide::Left
                    ? TEXT("LEFT")
                    : TEXT("RIGHT")
            ),
            FColor::Yellow);
        return false;
    }

    /*
     * Capture the victim's movement BEFORE stopping path following. The
     * montage keeps its local Hips fall; this velocity is only world-space
     * inertia for the capsule.
     */
    FVector PreImpactVelocity = GetVelocity();
    PreImpactVelocity.Z = 0.0f;

    ActiveTackleFallInitialSpeed = FMath::Min(
        PreImpactVelocity.Size2D() *
            FMath::Max(0.0f, TackleFallInitialInertiaScale) *
            GetPlayerProfileStrengthTackleFallInertiaMultiplier(),
        FMath::Max(0.0f, TackleFallMaximumInitialInertiaSpeed)
    );

    ActiveTackleFallInertiaDirection =
        PreImpactVelocity.GetSafeNormal2D();

    /*
     * A fall reaction replaces any action that was already controlling the
     * character. Cancel those actions BEFORE saving this reaction's input
     * lock. In particular, a running jump/tackle evasion temporarily sets
     * IgnoreMoveInput=true. Saving that temporary value here and restoring it
     * when the fall ends would leave a human player permanently unable to
     * move after a tackle foul.
     *
     * PreImpactVelocity was captured above, so cancelling the previous action
     * here does not lose the inertia we want to carry into the fall.
     */
    if (IsTackleActive())
    {
        CancelTackle();
    }

    if (IsTackleEvasionActive())
    {
        CancelTackleEvasion();
    }

    if (IsAerialActionQueuedOrPlaying())
    {
        CancelAerialAction();
    }

    if (Controller != nullptr)
    {
        bSavedTackleFallIgnoreMoveInput = Controller->IsMoveInputIgnored();
        bSavedTackleFallIgnoreMoveInputValid = true;
        Controller->SetIgnoreMoveInput(true);
        Controller->StopMovement();
    }

    SavedTackleFallGroundFriction = Movement->GroundFriction;
    SavedTackleFallBrakingDecelerationWalking =
        Movement->BrakingDecelerationWalking;
    SavedTackleFallMaxWalkSpeed = Movement->MaxWalkSpeed;
    bTackleFallMovementSettingsSaved = true;

    Movement->GroundFriction = 0.0f;
    Movement->BrakingDecelerationWalking = 0.0f;
    Movement->MaxWalkSpeed = FMath::Max(
        Movement->MaxWalkSpeed,
        ActiveTackleFallInitialSpeed
    );

    const float PlayedDuration = AnimInstance->Montage_Play(Montage, 1.0f);
    if (PlayedDuration <= 0.0f)
    {
        FinishTackleFallReaction();
        return false;
    }

    ActiveTackleFallSide = FallSide;
    ActiveTackleFallNormalizedTime = 0.0f;
    TackleFallPhase = ESoccerTackleFallPhase::Falling;

    FVector NewVelocity =
        ActiveTackleFallInertiaDirection * ActiveTackleFallInitialSpeed;
    NewVelocity.Z = Movement->Velocity.Z;
    Movement->Velocity = NewVelocity;

    ASoccerDebugManager::Message(
        this,
        ESoccerDebugCategory::Tackle,
        FString::Printf(
            TEXT("TACKLE FALL %s: %s inertia=%.0f"),
            *GetName(),
            FallSide == ESoccerTackleFallSide::Left
                ? TEXT("LEFT")
                : TEXT("RIGHT"),
            ActiveTackleFallInitialSpeed
        ),
        FColor::Orange);

    return true;
}

bool ASoccerCharacterBase::IsTackleFallReactionActive() const
{
    return TackleFallPhase != ESoccerTackleFallPhase::Inactive;
}

ESoccerTackleFallSide ASoccerCharacterBase::GetTackleFallSide() const
{
    return ActiveTackleFallSide;
}

UAnimMontage* ASoccerCharacterBase::GetTackleFallMontage(
    ESoccerTackleFallSide FallSide
) const
{
    return FallSide == ESoccerTackleFallSide::Right
        ? TackleRightFallMontage
        : TackleLeftFallMontage;
}

ESoccerTackleFallSide ASoccerCharacterBase::ChooseTackleFallSide(
    const ASoccerCharacterBase* TackleInstigator,
    const FVector& ContactLocation
) const
{
    FVector ToInstigator = FVector::ZeroVector;

    if (TackleInstigator != nullptr)
    {
        ToInstigator =
            TackleInstigator->GetActorLocation() - GetActorLocation();
        ToInstigator.Z = 0.0f;
    }

    if (ToInstigator.IsNearlyZero())
    {
        ToInstigator = ContactLocation - GetActorLocation();
        ToInstigator.Z = 0.0f;
    }

    const float SideDot = FVector::DotProduct(
        ToInstigator.GetSafeNormal2D(),
        GetActorRightVector().GetSafeNormal2D()
    );

    /*
     * Naming follows the authored fall: a tackler arriving from the victim's
     * left pushes the feet right and the body falls left.
     */
    return SideDot <= 0.0f
        ? ESoccerTackleFallSide::Left
        : ESoccerTackleFallSide::Right;
}

float ASoccerCharacterBase::EvaluateTackleFallInertiaAlpha(
    float NormalizedTime
) const
{
    const float StopTime = FMath::Clamp(
        TackleFallMovementStopNormalizedTime,
        0.20f,
        1.0f
    );

    if (NormalizedTime >= StopTime)
    {
        return 0.0f;
    }

    const float MovementAlpha = FMath::Clamp(
        NormalizedTime / StopTime,
        0.0f,
        1.0f
    );

    return FMath::Pow(
        1.0f - MovementAlpha,
        FMath::Max(0.15f, TackleFallDecelerationExponent)
    );
}

UAnimMontage* ASoccerCharacterBase::GetTackleEvasionMontage(
    ESoccerTackleEvasionSide EvasionSide
) const
{
    return EvasionSide == ESoccerTackleEvasionSide::Right
        ? TackleEvasionRightLegMontage
        : TackleEvasionLeftLegMontage;
}

ESoccerTackleEvasionSide ASoccerCharacterBase::ChooseTackleEvasionSide(
    const ASoccerCharacterBase* TackleInstigator
) const
{
    if (!IsValid(TackleInstigator))
    {
        return ESoccerTackleEvasionSide::Left;
    }

    const FVector LocalInstigator =
        GetActorTransform().InverseTransformPosition(
            TackleInstigator->GetActorLocation()
        );

    // Incoming challenge from the character's right uses the right-leg jump,
    // and symmetrically for the left.
    return LocalInstigator.Y >= 0.0f
        ? ESoccerTackleEvasionSide::Right
        : ESoccerTackleEvasionSide::Left;
}

void ASoccerCharacterBase::UpdateTackleEvasion(float DeltaTime)
{
    (void)DeltaTime;

    if (!IsTackleEvasionActive())
    {
        return;
    }

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    UAnimInstance* AnimInstance =
        GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
    UAnimMontage* Montage =
        GetTackleEvasionMontage(ActiveTackleEvasionSide);

    if (
        Movement == nullptr ||
        AnimInstance == nullptr ||
        Montage == nullptr ||
        !AnimInstance->Montage_IsActive(Montage)
    )
    {
        FinishTackleEvasion();
        return;
    }

    const float MontageLength = FMath::Max(
        KINDA_SMALL_NUMBER,
        Montage->GetPlayLength()
    );
    const float MontagePosition = FMath::Clamp(
        AnimInstance->Montage_GetPosition(Montage),
        0.0f,
        MontageLength
    );

    ActiveTackleEvasionNormalizedTime = FMath::Clamp(
        MontagePosition / MontageLength,
        0.0f,
        1.0f
    );

    const float SpeedRetention = FMath::Lerp(
        1.0f,
        FMath::Clamp(TackleEvasionEndSpeedRetention, 0.0f, 1.25f),
        ActiveTackleEvasionNormalizedTime
    );

    FVector NewVelocity =
        ActiveTackleEvasionDirection *
        ActiveTackleEvasionInitialSpeed *
        SpeedRetention;
    NewVelocity.Z = Movement->Velocity.Z;
    Movement->Velocity = NewVelocity;
}

void ASoccerCharacterBase::FinishTackleEvasion()
{
    UCharacterMovementComponent* Movement = GetCharacterMovement();

    if (Movement != nullptr && bTackleEvasionMovementSettingsSaved)
    {
        Movement->GroundFriction = SavedTackleEvasionGroundFriction;
        Movement->BrakingDecelerationWalking =
            SavedTackleEvasionBrakingDecelerationWalking;
        Movement->MaxWalkSpeed = SavedTackleEvasionMaxWalkSpeed;
    }

    if (
        Controller != nullptr &&
        bSavedTackleEvasionIgnoreMoveInputValid
    )
    {
        Controller->SetIgnoreMoveInput(
            bSavedTackleEvasionIgnoreMoveInput
        );
    }

    bTackleEvasionActive = false;
    ActiveTackleEvasionNormalizedTime = 0.0f;
    ActiveTackleEvasionDirection = FVector::ForwardVector;
    ActiveTackleEvasionInitialSpeed = 0.0f;
    bTackleEvasionMovementSettingsSaved = false;
    bSavedTackleEvasionIgnoreMoveInput = false;
    bSavedTackleEvasionIgnoreMoveInputValid = false;
}

void ASoccerCharacterBase::UpdateTackleFallReaction()
{
    if (!IsTackleFallReactionActive())
    {
        return;
    }

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    UAnimInstance* AnimInstance =
        GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
    UAnimMontage* Montage = GetTackleFallMontage(ActiveTackleFallSide);

    if (
        Movement == nullptr ||
        AnimInstance == nullptr ||
        Montage == nullptr
    )
    {
        FinishTackleFallReaction();
        return;
    }

    const float MontageLength = FMath::Max(
        KINDA_SMALL_NUMBER,
        Montage->GetPlayLength()
    );

    if (!AnimInstance->Montage_IsPlaying(Montage))
    {
        FinishTackleFallReaction();
        return;
    }

    const float MontagePosition =
        AnimInstance->Montage_GetPosition(Montage);

    ActiveTackleFallNormalizedTime = FMath::Clamp(
        MontagePosition / MontageLength,
        0.0f,
        1.0f
    );

    if (
        TackleFallPhase == ESoccerTackleFallPhase::Falling &&
        ActiveTackleFallNormalizedTime >=
            FMath::Clamp(
                TackleFallRecoveryStartNormalizedTime,
                0.20f,
                1.0f
            )
    )
    {
        TackleFallPhase = ESoccerTackleFallPhase::Recovery;
    }

    const float InertiaAlpha = EvaluateTackleFallInertiaAlpha(
        ActiveTackleFallNormalizedTime
    );

    FVector NewVelocity =
        ActiveTackleFallInertiaDirection *
        ActiveTackleFallInitialSpeed *
        InertiaAlpha;

    NewVelocity.Z = Movement->Velocity.Z;
    Movement->Velocity = NewVelocity;
}

void ASoccerCharacterBase::FinishTackleFallReaction()
{
    UCharacterMovementComponent* Movement = GetCharacterMovement();

    if (Movement != nullptr)
    {
        if (bTackleFallMovementSettingsSaved)
        {
            Movement->GroundFriction = SavedTackleFallGroundFriction;
            Movement->BrakingDecelerationWalking =
                SavedTackleFallBrakingDecelerationWalking;
            Movement->MaxWalkSpeed = SavedTackleFallMaxWalkSpeed;
        }

        FVector Velocity = Movement->Velocity;
        Velocity.X = 0.0f;
        Velocity.Y = 0.0f;
        Movement->Velocity = Velocity;
    }

    if (
        Controller != nullptr &&
        bSavedTackleFallIgnoreMoveInputValid
    )
    {
        Controller->SetIgnoreMoveInput(
            bSavedTackleFallIgnoreMoveInput
        );
    }

    bTackleFallMovementSettingsSaved = false;
    bSavedTackleFallIgnoreMoveInputValid = false;
    ActiveTackleFallInertiaDirection = FVector::ZeroVector;
    ActiveTackleFallInitialSpeed = 0.0f;
    ActiveTackleFallNormalizedTime = 0.0f;
    TackleFallPhase = ESoccerTackleFallPhase::Inactive;
}

UAnimMontage* ASoccerCharacterBase::GetTackleMontage(
    ESoccerTackleSide TackleSide
) const
{
    return TackleSide == ESoccerTackleSide::RightLeg
        ? TackleRightLegMontage
        : TackleLeftLegMontage;
}

ESoccerTackleSide ASoccerCharacterBase::ChooseTackleSideForTarget(
    const FVector& TargetLocation
) const
{
    FVector ToTarget = TargetLocation - GetActorLocation();
    ToTarget.Z = 0.0f;

    if (ToTarget.IsNearlyZero())
    {
        return ActiveTackleSide;
    }

    const float LateralDot = FVector::DotProduct(
        ToTarget.GetSafeNormal(),
        GetActorRightVector().GetSafeNormal2D()
    );

    /*
     * In these mirrored animations the named leg is the upper/reaching leg.
     * Prefer the leg on the same lateral side as the target. A near-center
     * target keeps the previously selected side, preventing frame-to-frame
     * oscillation when AI planning is added later.
     */
    if (LateralDot > 0.08f)
    {
        return ESoccerTackleSide::RightLeg;
    }

    if (LateralDot < -0.08f)
    {
        return ESoccerTackleSide::LeftLeg;
    }

    return ActiveTackleSide;
}

float ASoccerCharacterBase::EvaluateTackleSpeedAlpha(
    float NormalizedTime
) const
{
    const float StopTime = FMath::Clamp(
        TackleMovementStopNormalizedTime,
        0.20f,
        1.0f
    );

    if (NormalizedTime >= StopTime)
    {
        return 0.0f;
    }

    if (TackleSpeedProfile != nullptr)
    {
        return FMath::Clamp(
            TackleSpeedProfile->GetFloatValue(NormalizedTime),
            0.0f,
            1.5f
        );
    }

    const float MovementAlpha = FMath::Clamp(
        NormalizedTime / StopTime,
        0.0f,
        1.0f
    );

    return FMath::Pow(
        1.0f - MovementAlpha,
        FMath::Max(0.15f, TackleFallbackDecelerationExponent)
    );
}

void ASoccerCharacterBase::UpdateTackle()
{
    if (!IsTackleActive())
    {
        return;
    }

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    UAnimInstance* AnimInstance =
        GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
    UAnimMontage* Montage = GetTackleMontage(ActiveTackleSide);

    if (Movement == nullptr || AnimInstance == nullptr || Montage == nullptr)
    {
        FinishTackle();
        return;
    }

    const bool bMontageActive = AnimInstance->Montage_IsActive(Montage);

    if (!bMontageActive)
    {
        FinishTackle();
        return;
    }

    const float MontageLength = FMath::Max(
        KINDA_SMALL_NUMBER,
        Montage->GetPlayLength()
    );
    const float MontagePosition = FMath::Clamp(
        AnimInstance->Montage_GetPosition(Montage),
        0.0f,
        MontageLength
    );

    const float PreviousNormalizedTime = ActiveTackleNormalizedTime;

    ActiveTackleNormalizedTime = FMath::Clamp(
        MontagePosition / MontageLength,
        0.0f,
        1.0f
    );

    UpdateTackleContactTracking(PreviousNormalizedTime);

    if (
        TacklePhase == ESoccerTacklePhase::Sliding &&
        ActiveTackleNormalizedTime >=
            FMath::Clamp(TackleRecoveryStartNormalizedTime, 0.20f, 1.0f)
    )
    {
        TacklePhase = ESoccerTacklePhase::Recovery;
    }

    const float SpeedAlpha = EvaluateTackleSpeedAlpha(
        ActiveTackleNormalizedTime
    );
    const float CurrentSlideSpeed = ActiveTackleInitialSpeed * SpeedAlpha;

    FVector NewVelocity = ActiveTackleDirection * CurrentSlideSpeed;
    NewVelocity.Z = Movement->Velocity.Z;
    Movement->Velocity = NewVelocity;
}

void ASoccerCharacterBase::FinishTackle()
{
    UCharacterMovementComponent* Movement = GetCharacterMovement();

    if (Movement != nullptr)
    {
        if (bTackleMovementSettingsSaved)
        {
            Movement->GroundFriction = SavedTackleGroundFriction;
            Movement->BrakingDecelerationWalking =
                SavedTackleBrakingDecelerationWalking;
            Movement->MaxWalkSpeed = SavedTackleMaxWalkSpeed;
        }

        FVector FinalVelocity = Movement->Velocity;
        FinalVelocity.X = 0.0f;
        FinalVelocity.Y = 0.0f;
        Movement->Velocity = FinalVelocity;
    }

    if (IsTackleActive())
    {
        FinalizeTackleContactTracking();
    }

    bTackleMovementSettingsSaved = false;
    ActiveTackleInitialSpeed = 0.0f;
    ActiveTackleNormalizedTime = 0.0f;
    TacklePhase = ESoccerTacklePhase::Inactive;
}

void ASoccerCharacterBase::ResetTackleContactTracking()
{
    bTackleContactTrackingInitialized = false;
PreviousTackleBallLocation = FVector::ZeroVector;
    PreviousTackleLeftFootLocation = FVector::ZeroVector;
    PreviousTackleRightFootLocation = FVector::ZeroVector;
    PreviousTackleLeftLowerLegLocation = FVector::ZeroVector;
    PreviousTackleRightLowerLegLocation = FVector::ZeroVector;
    CurrentTackleContactSummary = FSoccerTackleContactSummary();
    CurrentFirstTackleOpponentContactCharacter.Reset();
    bTackleFoulEvaluationSubmitted = false;
}

ASoccerBall* ASoccerCharacterBase::ResolveTackleBall() const
{
    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return nullptr;
    }

    for (TActorIterator<ASoccerBall> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            return *It;
        }
    }

    return nullptr;
}

bool ASoccerCharacterBase::ResolveTackleContactBoneNames(
    FName& OutLeftFootBoneName,
    FName& OutRightFootBoneName,
    FName& OutLeftLowerLegBoneName,
    FName& OutRightLowerLegBoneName,
    FString* OutFailureReason
) const
{
    OutLeftFootBoneName = NAME_None;
    OutRightFootBoneName = NAME_None;
    OutLeftLowerLegBoneName = NAME_None;
    OutRightLowerLegBoneName = NAME_None;

    if (OutFailureReason != nullptr)
    {
        OutFailureReason->Empty();
    }

    const USkeletalMeshComponent* CharacterMesh = GetMesh();

    if (CharacterMesh == nullptr)
    {
        if (OutFailureReason != nullptr)
        {
            *OutFailureReason = TEXT("El personaje no tiene SkeletalMeshComponent");
        }

        return false;
    }

    const auto ResolveOneBone =
        [CharacterMesh](
            const FName& ConfiguredName,
            const FName& PlainName,
            const FName& MixamoName,
            FName& OutResolvedName
        ) -> bool
        {
            TArray<FName> Candidates;

            if (!ConfiguredName.IsNone())
            {
                Candidates.AddUnique(ConfiguredName);

                const FString ConfiguredString = ConfiguredName.ToString();
                int32 NamespaceSeparatorIndex = INDEX_NONE;

                if (
                    ConfiguredString.FindLastChar(
                        TEXT(':'),
                        NamespaceSeparatorIndex
                    ) &&
                    NamespaceSeparatorIndex + 1 < ConfiguredString.Len()
                )
                {
                    Candidates.AddUnique(
                        FName(*ConfiguredString.Mid(NamespaceSeparatorIndex + 1))
                    );
                }
            }

            Candidates.AddUnique(PlainName);
            Candidates.AddUnique(MixamoName);

            for (const FName& Candidate : Candidates)
            {
                if (
                    !Candidate.IsNone() &&
                    CharacterMesh->GetBoneIndex(Candidate) != INDEX_NONE
                )
                {
                    OutResolvedName = Candidate;
                    return true;
                }
            }

            return false;
        };

    const bool bLeftFootResolved = ResolveOneBone(
        TackleLeftFootBoneName,
        FName(TEXT("LeftFoot")),
        FName(TEXT("mixamorig:LeftFoot")),
        OutLeftFootBoneName
    );

    const bool bRightFootResolved = ResolveOneBone(
        TackleRightFootBoneName,
        FName(TEXT("RightFoot")),
        FName(TEXT("mixamorig:RightFoot")),
        OutRightFootBoneName
    );

    const bool bLeftLowerLegResolved = ResolveOneBone(
        TackleLeftLowerLegBoneName,
        FName(TEXT("LeftLeg")),
        FName(TEXT("mixamorig:LeftLeg")),
        OutLeftLowerLegBoneName
    );

    const bool bRightLowerLegResolved = ResolveOneBone(
        TackleRightLowerLegBoneName,
        FName(TEXT("RightLeg")),
        FName(TEXT("mixamorig:RightLeg")),
        OutRightLowerLegBoneName
    );

    if (
        bLeftFootResolved &&
        bRightFootResolved &&
        bLeftLowerLegResolved &&
        bRightLowerLegResolved
    )
    {
        return true;
    }

    if (OutFailureReason != nullptr)
    {
        *OutFailureReason = FString::Printf(
            TEXT("No se pudieron resolver todos los huesos del tackle. LeftFoot=%s RightFoot=%s LeftLeg=%s RightLeg=%s"),
            bLeftFootResolved ? TEXT("OK") : TEXT("MISSING"),
            bRightFootResolved ? TEXT("OK") : TEXT("MISSING"),
            bLeftLowerLegResolved ? TEXT("OK") : TEXT("MISSING"),
            bRightLowerLegResolved ? TEXT("OK") : TEXT("MISSING")
        );
    }

    return false;
}

bool ASoccerCharacterBase::RegisterTackleBallTouchForRules() const
{
    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return true;
    }

    for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
    {
        ASoccerMatchManager* MatchManager = *It;

        if (IsValid(MatchManager))
        {
            return MatchManager->TryRegisterIntentionalBallTouch(
                const_cast<ASoccerCharacterBase*>(this)
            );
        }
    }

    return true;
}

bool ASoccerCharacterBase::ResolveTackleBallContactVelocity(
    ASoccerBall* Ball,
    ESoccerTackleContactLimb ContactLimb,
    const FVector& ContactBallLocation,
    float ContactNormalizedTime,
    FVector& OutVelocity
) const
{
    OutVelocity = FVector::ZeroVector;

    if (!IsValid(Ball))
    {
        return false;
    }

    FVector SlideDirection = ActiveTackleDirection.GetSafeNormal2D();

    if (SlideDirection.IsNearlyZero())
    {
        SlideDirection = GetActorForwardVector().GetSafeNormal2D();
    }

    if (SlideDirection.IsNearlyZero())
    {
        return false;
    }

    FVector ToBallDirection = ContactBallLocation - GetActorLocation();
    ToBallDirection.Z = 0.0f;
    ToBallDirection = ToBallDirection.GetSafeNormal();

    const float DirectionInfluence = FMath::Clamp(
        TackleBallContactDirectionInfluence,
        0.0f,
        1.0f
    );

    FVector ContactDirection = SlideDirection;

    if (!ToBallDirection.IsNearlyZero())
    {
        ContactDirection = (
            SlideDirection * (1.0f - DirectionInfluence) +
            ToBallDirection * DirectionInfluence
        ).GetSafeNormal2D();
    }

    if (ContactDirection.IsNearlyZero())
    {
        ContactDirection = SlideDirection;
    }

    const FVector IncomingVelocity = Ball->GetBallPhysicsVelocity();
    FVector IncomingHorizontal = IncomingVelocity;
    IncomingHorizontal.Z = 0.0f;

    const float ContactSpeedAlpha = EvaluateTackleSpeedAlpha(
        FMath::Clamp(ContactNormalizedTime, 0.0f, 1.0f)
    );
    const float SlideSpeedAtContact =
        FMath::Max(0.0f, ActiveTackleInitialSpeed) * ContactSpeedAlpha;

    const float TransferSpeed =
        SlideSpeedAtContact * FMath::Max(0.0f, TackleBallSlideSpeedTransfer);

    FVector OutgoingHorizontal =
        IncomingHorizontal *
            FMath::Clamp(TackleBallIncomingHorizontalRetention, 0.0f, 1.0f) +
        ContactDirection * TransferSpeed;

    const float MinimumHorizontalSpeed =
        FMath::Max(0.0f, TackleBallMinimumHorizontalSpeed);
    const float MaximumHorizontalSpeed = FMath::Max(
        MinimumHorizontalSpeed,
        TackleBallMaximumHorizontalSpeed
    );

    float HorizontalSpeed = OutgoingHorizontal.Size2D();

    if (HorizontalSpeed < KINDA_SMALL_NUMBER)
    {
        OutgoingHorizontal = ContactDirection * MinimumHorizontalSpeed;
        HorizontalSpeed = MinimumHorizontalSpeed;
    }
    else
    {
        const float ClampedHorizontalSpeed = FMath::Clamp(
            HorizontalSpeed,
            MinimumHorizontalSpeed,
            MaximumHorizontalSpeed
        );
        OutgoingHorizontal =
            OutgoingHorizontal.GetSafeNormal2D() * ClampedHorizontalSpeed;
    }

    const bool bLowerLegContact =
        ContactLimb == ESoccerTackleContactLimb::LeftLowerLeg ||
        ContactLimb == ESoccerTackleContactLimb::RightLowerLeg;

    const float UpwardMultiplier = bLowerLegContact
        ? FMath::Clamp(TackleBallLowerLegUpwardMultiplier, 0.0f, 1.0f)
        : 1.0f;

    const float RetainedVertical =
        IncomingVelocity.Z *
        FMath::Clamp(TackleBallIncomingVerticalRetention, 0.0f, 1.0f);

    const float ContactLift =
        FMath::Max(0.0f, TackleBallFootUpwardSpeed) * UpwardMultiplier;

    OutVelocity = OutgoingHorizontal;
    OutVelocity.Z = RetainedVertical + ContactLift;

    return !OutVelocity.ContainsNaN() && !OutVelocity.IsNearlyZero();
}

FVector ASoccerCharacterBase::GetTackleContactPoint(
    ESoccerTackleContactLimb Limb
) const
{
    const USkeletalMeshComponent* CharacterMesh = GetMesh();

    if (CharacterMesh == nullptr)
    {
        return FVector::ZeroVector;
    }

    FName ResolvedLeftFootBoneName;
    FName ResolvedRightFootBoneName;
    FName ResolvedLeftLowerLegBoneName;
    FName ResolvedRightLowerLegBoneName;

    if (
        !ResolveTackleContactBoneNames(
            ResolvedLeftFootBoneName,
            ResolvedRightFootBoneName,
            ResolvedLeftLowerLegBoneName,
            ResolvedRightLowerLegBoneName
        )
    )
    {
        return FVector::ZeroVector;
    }

    const auto BoneWorldLocation =
        [CharacterMesh](const FName& BoneName) -> FVector
        {
            return CharacterMesh->GetBoneLocation(
                BoneName,
                EBoneSpaces::WorldSpace
            );
        };

    switch (Limb)
    {
        case ESoccerTackleContactLimb::LeftFoot:
            return BoneWorldLocation(ResolvedLeftFootBoneName);

        case ESoccerTackleContactLimb::RightFoot:
            return BoneWorldLocation(ResolvedRightFootBoneName);

        case ESoccerTackleContactLimb::LeftLowerLeg:
        {
            const FVector Knee = BoneWorldLocation(ResolvedLeftLowerLegBoneName);
            const FVector Foot = BoneWorldLocation(ResolvedLeftFootBoneName);
            return FMath::Lerp(Knee, Foot, 0.5f);
        }

        case ESoccerTackleContactLimb::RightLowerLeg:
        {
            const FVector Knee = BoneWorldLocation(ResolvedRightLowerLegBoneName);
            const FVector Foot = BoneWorldLocation(ResolvedRightFootBoneName);
            return FMath::Lerp(Knee, Foot, 0.5f);
        }

        default:
            return FVector::ZeroVector;
    }
}

void ASoccerCharacterBase::InitializeTackleContactTracking()
{
    PreviousTackleLeftFootLocation = GetTackleContactPoint(
        ESoccerTackleContactLimb::LeftFoot
    );
    PreviousTackleRightFootLocation = GetTackleContactPoint(
        ESoccerTackleContactLimb::RightFoot
    );
    PreviousTackleLeftLowerLegLocation = GetTackleContactPoint(
        ESoccerTackleContactLimb::LeftLowerLeg
    );
    PreviousTackleRightLowerLegLocation = GetTackleContactPoint(
        ESoccerTackleContactLimb::RightLowerLeg
    );

    ASoccerBall* Ball = ResolveTackleBall();
    PreviousTackleBallLocation =
        Ball != nullptr ? Ball->GetActorLocation() : FVector::ZeroVector;

    bTackleContactTrackingInitialized =
        !PreviousTackleLeftFootLocation.IsNearlyZero() &&
        !PreviousTackleRightFootLocation.IsNearlyZero() &&
        !PreviousTackleLeftLowerLegLocation.IsNearlyZero() &&
        !PreviousTackleRightLowerLegLocation.IsNearlyZero();

    if (!bTackleContactTrackingInitialized)
    {
        ASoccerDebugManager::Message(
            this,
            ESoccerDebugCategory::Tackle,
            FString::Printf(
                TEXT("TACKLE %s: falta un hueso Foot/Leg; revisar Soccer|Tackle|Contact|Bones"),
                *GetName()
            ),
            FColor::Yellow,
            91001
        );
    }
}

void ASoccerCharacterBase::UpdateTackleContactTracking(
    float PreviousNormalizedTime
)
{
    if (!IsTackleActive())
    {
        return;
    }

    if (!bTackleContactTrackingInitialized)
    {
        InitializeTackleContactTracking();

        if (!bTackleContactTrackingInitialized)
        {
            return;
        }
    }

    const float CurrentNormalizedTime = ActiveTackleNormalizedTime;

    const FVector CurrentLeftFoot = GetTackleContactPoint(
        ESoccerTackleContactLimb::LeftFoot
    );
    const FVector CurrentRightFoot = GetTackleContactPoint(
        ESoccerTackleContactLimb::RightFoot
    );
    const FVector CurrentLeftLowerLeg = GetTackleContactPoint(
        ESoccerTackleContactLimb::LeftLowerLeg
    );
    const FVector CurrentRightLowerLeg = GetTackleContactPoint(
        ESoccerTackleContactLimb::RightLowerLeg
    );

    ASoccerBall* Ball = ResolveTackleBall();
    const FVector CurrentBallLocation =
        Ball != nullptr ? Ball->GetActorLocation() : PreviousTackleBallLocation;

    const float WindowStart = FMath::Clamp(
        TackleContactWindowStartNormalizedTime,
        0.0f,
        1.0f
    );
    const float WindowEnd = FMath::Clamp(
        FMath::Max(WindowStart, TackleContactWindowEndNormalizedTime),
        WindowStart,
        1.0f
    );

    const float FrameStartTime = FMath::Min(
        PreviousNormalizedTime,
        CurrentNormalizedTime
    );
    const float FrameEndTime = FMath::Max(
        PreviousNormalizedTime,
        CurrentNormalizedTime
    );

    const bool bFrameTouchesWindow =
        FrameEndTime >= WindowStart &&
        FrameStartTime <= WindowEnd &&
        CurrentNormalizedTime > PreviousNormalizedTime + KINDA_SMALL_NUMBER;

    struct FTackleLimbFrame
    {
        ESoccerTackleContactLimb Limb;
        FVector Previous;
        FVector Current;
        float Radius;
    };

    const FTackleLimbFrame Limbs[] =
    {
        {
            ESoccerTackleContactLimb::LeftFoot,
            PreviousTackleLeftFootLocation,
            CurrentLeftFoot,
            TackleFootContactRadius
        },
        {
            ESoccerTackleContactLimb::RightFoot,
            PreviousTackleRightFootLocation,
            CurrentRightFoot,
            TackleFootContactRadius
        },
        {
            ESoccerTackleContactLimb::LeftLowerLeg,
            PreviousTackleLeftLowerLegLocation,
            CurrentLeftLowerLeg,
            TackleLowerLegContactRadius
        },
        {
            ESoccerTackleContactLimb::RightLowerLeg,
            PreviousTackleRightLowerLegLocation,
            CurrentRightLowerLeg,
            TackleLowerLegContactRadius
        }
    };

    if (bFrameTouchesWindow)
    {
        const float FrameDuration = FMath::Max(
            KINDA_SMALL_NUMBER,
            CurrentNormalizedTime - PreviousNormalizedTime
        );

        const float ClipStartAlpha = FMath::Clamp(
            (WindowStart - PreviousNormalizedTime) / FrameDuration,
            0.0f,
            1.0f
        );
        const float ClipEndAlpha = FMath::Clamp(
            (WindowEnd - PreviousNormalizedTime) / FrameDuration,
            0.0f,
            1.0f
        );

        const float SafeClipStartAlpha = FMath::Min(
            ClipStartAlpha,
            ClipEndAlpha
        );
        const float SafeClipEndAlpha = FMath::Max(
            ClipStartAlpha,
            ClipEndAlpha
        );

        float EarliestBallFrameAlpha = TNumericLimits<float>::Max();
        FVector EarliestBallContactLocation = FVector::ZeroVector;
        ESoccerTackleContactLimb EarliestBallLimb =
            ESoccerTackleContactLimb::None;

        float EarliestOpponentFrameAlpha = TNumericLimits<float>::Max();
        FVector EarliestOpponentContactLocation = FVector::ZeroVector;
        ESoccerTackleContactLimb EarliestOpponentLimb =
            ESoccerTackleContactLimb::None;
        TWeakObjectPtr<ASoccerCharacterBase> EarliestOpponentCharacter;

        FCollisionQueryParams QueryParams(
            SCENE_QUERY_STAT(TackleTemporalContact),
            false,
            this
        );
        QueryParams.AddIgnoredActor(this);

        FCollisionObjectQueryParams ObjectQueryParams;
        ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

        for (const FTackleLimbFrame& LimbFrame : Limbs)
        {
            if (
                LimbFrame.Previous.IsNearlyZero() ||
                LimbFrame.Current.IsNearlyZero()
            )
            {
                continue;
            }

            const FVector LimbStart = FMath::Lerp(
                LimbFrame.Previous,
                LimbFrame.Current,
                SafeClipStartAlpha
            );
            const FVector LimbEnd = FMath::Lerp(
                LimbFrame.Previous,
                LimbFrame.Current,
                SafeClipEndAlpha
            );

            ASoccerDebugManager::DrawLine(
                this,
                ESoccerDebugCategory::Tackle,
                LimbStart,
                LimbEnd,
                FColor::Cyan,
                FMath::Max(0.0f, TackleContactDebugDuration),
                1.2f
            );

            if (
                Ball != nullptr &&
                !CurrentTackleContactSummary.bBallContactOccurred
            )
            {
                const FVector BallStart = FMath::Lerp(
                    PreviousTackleBallLocation,
                    CurrentBallLocation,
                    SafeClipStartAlpha
                );
                const FVector BallEnd = FMath::Lerp(
                    PreviousTackleBallLocation,
                    CurrentBallLocation,
                    SafeClipEndAlpha
                );

                float ContactAlpha = 0.0f;
                FVector LimbClosest;
                FVector BallClosest;

                const float DistanceSquared =
                    SoccerMovingPointClosestDistanceSquared(
                        LimbStart,
                        LimbEnd,
                        BallStart,
                        BallEnd,
                        ContactAlpha,
                        LimbClosest,
                        BallClosest
                    );

                const float EffectiveBallLimbRadius =
                    FMath::Max(
                        1.0f,
                        LimbFrame.Radius *
                            GetPlayerProfileTackleBallContactRadiusMultiplier()
                    );

                const float CombinedRadius =
                    EffectiveBallLimbRadius +
                    FMath::Max(1.0f, Ball->GetBallRadiusCm());

                if (DistanceSquared <= FMath::Square(CombinedRadius))
                {
                    const float WholeFrameAlpha = FMath::Lerp(
                        SafeClipStartAlpha,
                        SafeClipEndAlpha,
                        ContactAlpha
                    );

                    if (WholeFrameAlpha < EarliestBallFrameAlpha)
                    {
                        EarliestBallFrameAlpha = WholeFrameAlpha;
                        EarliestBallContactLocation = BallClosest;
                        EarliestBallLimb = LimbFrame.Limb;
                    }
                }
            }

            if (!CurrentTackleContactSummary.bOpponentContactOccurred)
            {
                TArray<FHitResult> Hits;

                const bool bHitAny = GetWorld()->SweepMultiByObjectType(
                    Hits,
                    LimbStart,
                    LimbEnd,
                    FQuat::Identity,
                    ObjectQueryParams,
                    FCollisionShape::MakeSphere(
                        FMath::Max(1.0f, LimbFrame.Radius)
                    ),
                    QueryParams
                );

                if (bHitAny)
                {
                    for (const FHitResult& Hit : Hits)
                    {
                        ASoccerCharacterBase* OtherCharacter =
                            Cast<ASoccerCharacterBase>(Hit.GetActor());

                        if (
                            OtherCharacter == nullptr ||
                            OtherCharacter == this ||
                            OtherCharacter->GetTeam() == GetTeam()
                        )
                        {
                            continue;
                        }

                        // Stage 3: the evasive montage has a normalized
                        // airborne window. Low tackle limbs pass underneath
                        // during that window; early/late contact still counts.
                        if (
                            OtherCharacter->
                                IsTackleEvasionAvoidingLowContact()
                        )
                        {
                            continue;
                        }

                        const float SweepAlpha = FMath::Clamp(
                            Hit.Time,
                            0.0f,
                            1.0f
                        );
                        const float WholeFrameAlpha = FMath::Lerp(
                            SafeClipStartAlpha,
                            SafeClipEndAlpha,
                            SweepAlpha
                        );

                        if (WholeFrameAlpha < EarliestOpponentFrameAlpha)
                        {
                            EarliestOpponentFrameAlpha = WholeFrameAlpha;
                            EarliestOpponentContactLocation =
                                Hit.ImpactPoint.IsNearlyZero()
                                ? FMath::Lerp(LimbStart, LimbEnd, SweepAlpha)
                                : Hit.ImpactPoint;
                            EarliestOpponentLimb = LimbFrame.Limb;
                            EarliestOpponentCharacter = OtherCharacter;
                        }
                    }
                }
            }
        }

        if (
            !CurrentTackleContactSummary.bBallContactOccurred &&
            EarliestBallLimb != ESoccerTackleContactLimb::None
        )
        {
            CurrentTackleContactSummary.bBallContactOccurred = true;
            CurrentTackleContactSummary.BallContactNormalizedTime =
                FMath::Lerp(
                    PreviousNormalizedTime,
                    CurrentNormalizedTime,
                    EarliestBallFrameAlpha
                );
            CurrentTackleContactSummary.IncomingBallVelocity =
                Ball != nullptr
                ? Ball->GetBallPhysicsVelocity()
                : FVector::ZeroVector;

            FVector OutgoingBallVelocity = FVector::ZeroVector;
            const bool bResolvedBallResponse =
                bEnableTackleBallResponse &&
                ResolveTackleBallContactVelocity(
                    Ball,
                    EarliestBallLimb,
                    EarliestBallContactLocation,
                    CurrentTackleContactSummary.BallContactNormalizedTime,
                    OutgoingBallVelocity
                );

            bool bTouchAcceptedByRules = true;

            if (bResolvedBallResponse)
            {
                bTouchAcceptedByRules = RegisterTackleBallTouchForRules();
            }

            if (
                bResolvedBallResponse &&
                bTouchAcceptedByRules &&
                Ball != nullptr
            )
            {
                CurrentTackleContactSummary.bBallContactApplied = true;
                CurrentTackleContactSummary.OutgoingBallVelocity =
                    OutgoingBallVelocity;

                Ball->ApplyTackleContactAtLocation(
                    EarliestBallContactLocation,
                    OutgoingBallVelocity,
                    TackleBallAngularVelocityRetention
                );

                FVector DebugDirection = OutgoingBallVelocity;
                DebugDirection.Z = 0.0f;
                DebugDirection = DebugDirection.GetSafeNormal();

                if (!DebugDirection.IsNearlyZero())
                {
                    ASoccerDebugManager::DrawLine(
                        this,
                        ESoccerDebugCategory::Tackle,
                        EarliestBallContactLocation,
                        EarliestBallContactLocation + DebugDirection * 220.0f,
                        FColor::Yellow,
                        1.5f,
                        2.5f
                    );
                }
            }

            ASoccerDebugManager::DrawSphere(
                this,
                ESoccerDebugCategory::Tackle,
                EarliestBallContactLocation,
                18.0f,
                FColor::Green,
                1.5f,
                10,
                2.0f
            );

            ASoccerDebugManager::Message(
                this,
                ESoccerDebugCategory::Tackle,
                FString::Printf(
                    TEXT("TACKLE %s: PELOTA t=%.3f limb=%d in=%.0f out=%.0f applied=%s"),
                    *GetName(),
                    CurrentTackleContactSummary.BallContactNormalizedTime,
                    static_cast<int32>(EarliestBallLimb),
                    CurrentTackleContactSummary.IncomingBallVelocity.Size(),
                    CurrentTackleContactSummary.OutgoingBallVelocity.Size(),
                    CurrentTackleContactSummary.bBallContactApplied
                        ? TEXT("SI")
                        : TEXT("NO")
                ),
                CurrentTackleContactSummary.bBallContactApplied
                    ? FColor::Green
                    : FColor::Yellow);
        }

        if (
            !CurrentTackleContactSummary.bOpponentContactOccurred &&
            EarliestOpponentCharacter.IsValid()
        )
        {
            CurrentTackleContactSummary.bOpponentContactOccurred = true;
            CurrentTackleContactSummary.OpponentContactNormalizedTime =
                FMath::Lerp(
                    PreviousNormalizedTime,
                    CurrentNormalizedTime,
                    EarliestOpponentFrameAlpha
                );
            CurrentTackleContactSummary.OpponentContactLocation =
                EarliestOpponentContactLocation;
            const UCapsuleComponent* Capsule = GetCapsuleComponent();
            const float CapsuleFloorZ =
                Capsule != nullptr
                ? GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight()
                : GetActorLocation().Z;

            CurrentTackleContactSummary.OpponentContactHeightCm =
                FMath::Max(
                    0.0f,
                    EarliestOpponentContactLocation.Z - CapsuleFloorZ
                );

            CurrentFirstTackleOpponentContactCharacter =
                EarliestOpponentCharacter;

            FVector TacklerVelocityAtContact = GetVelocity();
            FVector VictimVelocityAtContact =
                EarliestOpponentCharacter->GetVelocity();
            TacklerVelocityAtContact.Z = 0.0f;
            VictimVelocityAtContact.Z = 0.0f;

            CurrentTackleContactSummary.OpponentContactRelativeSpeedCmPerSec =
                (TacklerVelocityAtContact - VictimVelocityAtContact).Size();

            FVector VictimToTackler =
                GetActorLocation() - EarliestOpponentCharacter->GetActorLocation();
            VictimToTackler.Z = 0.0f;

            const FVector VictimForward =
                EarliestOpponentCharacter->GetActorForwardVector().GetSafeNormal2D();

            CurrentTackleContactSummary.bOpponentContactFromBehind =
                !VictimToTackler.IsNearlyZero() &&
                FVector::DotProduct(
                    VictimToTackler.GetSafeNormal(),
                    VictimForward
                ) < -0.35f;

            const bool bFallStarted =
                EarliestOpponentCharacter->TryStartTackleFallReaction(
                    this,
                    EarliestOpponentContactLocation
                );

            CurrentTackleContactSummary.bOpponentFallReactionStarted =
                bFallStarted;

            CurrentTackleContactSummary.OpponentFallSide =
                EarliestOpponentCharacter->GetTackleFallSide();

            ASoccerDebugManager::DrawSphere(
                this,
                ESoccerDebugCategory::Tackle,
                EarliestOpponentContactLocation,
                20.0f,
                FColor::Red,
                1.5f,
                10,
                2.0f
            );

            ASoccerDebugManager::Message(
                this,
                ESoccerDebugCategory::Tackle,
                FString::Printf(
                    TEXT("TACKLE %s: RIVAL t=%.3f limb=%d h=%.1fcm rel=%.0f behind=%s victim=%s fall=%s side=%s"),
                    *GetName(),
                    CurrentTackleContactSummary.OpponentContactNormalizedTime,
                    static_cast<int32>(EarliestOpponentLimb),
                    CurrentTackleContactSummary.OpponentContactHeightCm,
                    CurrentTackleContactSummary.OpponentContactRelativeSpeedCmPerSec,
                    CurrentTackleContactSummary.bOpponentContactFromBehind
                        ? TEXT("SI")
                        : TEXT("NO"),
                    *EarliestOpponentCharacter->GetName(),
                    CurrentTackleContactSummary.bOpponentFallReactionStarted
                        ? TEXT("SI")
                        : TEXT("NO"),
                    CurrentTackleContactSummary.OpponentFallSide ==
                        ESoccerTackleFallSide::Left
                        ? TEXT("LEFT")
                        : TEXT("RIGHT")
                ),
                FColor::Red);
        }
    }

    TrySubmitCurrentTackleFoulEvaluation(
        CurrentNormalizedTime,
        false
    );

    PreviousTackleLeftFootLocation = CurrentLeftFoot;
    PreviousTackleRightFootLocation = CurrentRightFoot;
    PreviousTackleLeftLowerLegLocation = CurrentLeftLowerLeg;
    PreviousTackleRightLowerLegLocation = CurrentRightLowerLeg;
    PreviousTackleBallLocation = CurrentBallLocation;
}

void ASoccerCharacterBase::FinalizeTackleContactTracking()
{
    // Do not wait until the montage ends to award a foul. In normal play the
    // evaluation is submitted as soon as the contact chronology is known.
    // This forced call is only a safety net for an edge case where the tackle
    // finishes inside the near-simultaneous tolerance window.
    TrySubmitCurrentTackleFoulEvaluation(
        ActiveTackleNormalizedTime,
        true
    );

    LastTackleContactSummary = CurrentTackleContactSummary;
    bTackleContactTrackingInitialized = false;

    ASoccerDebugManager::Message(
        this,
        ESoccerDebugCategory::Tackle,
        FString::Printf(
            TEXT("TACKLE %s: FIN order=%d ball=%.3f rival=%.3f"),
            *GetName(),
            static_cast<int32>(GetTackleContactOrder()),
            LastTackleContactSummary.BallContactNormalizedTime,
            LastTackleContactSummary.OpponentContactNormalizedTime
        ),
        FColor::Cyan);
}

void ASoccerCharacterBase::TrySubmitCurrentTackleFoulEvaluation(
    float CurrentNormalizedTime,
    bool bForce
)
{
    if (bTackleFoulEvaluationSubmitted)
    {
        return;
    }

    ASoccerCharacterBase* Victim =
        CurrentFirstTackleOpponentContactCharacter.Get();

    if (
        Victim == nullptr ||
        !CurrentTackleContactSummary.bOpponentContactOccurred
    )
    {
        return;
    }

    // If the opponent was contacted before the ball, wait only long enough
    // to preserve the configured NearlySimultaneous classification. Waiting
    // until the whole tackle montage ends lets a later goal-kick/corner/throw-in
    // replace a foul that actually happened while the ball was still in play.
    if (
        !bForce &&
        !CurrentTackleContactSummary.bBallContactOccurred
    )
    {
        const float Tolerance =
            FMath::Max(
                0.0f,
                TackleNearlySimultaneousNormalizedTolerance
            );

        const float EarliestSafeEvaluationTime =
            CurrentTackleContactSummary.OpponentContactNormalizedTime +
            Tolerance;

        if (CurrentNormalizedTime < EarliestSafeEvaluationTime)
        {
            return;
        }
    }

    ASoccerMatchManager* MatchManager = nullptr;
    UWorld* World = GetWorld();

    if (World != nullptr)
    {
        for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
        {
            MatchManager = *It;
            break;
        }
    }

    if (MatchManager == nullptr)
    {
        return;
    }

    FSoccerFoulIncident Incident;
    Incident.InstigatorCharacter = this;
    Incident.VictimCharacter = Victim;
    Incident.InstigatorTeam = GetTeam();
    Incident.VictimTeam = Victim->GetTeam();
    Incident.PhysicalActionType = ESoccerPhysicalActionType::SlidingTackle;
    Incident.IncidentLocation =
        CurrentTackleContactSummary.OpponentContactLocation;
    Incident.RelativeSpeedCmPerSec =
        CurrentTackleContactSummary.OpponentContactRelativeSpeedCmPerSec;
    Incident.ContactHeightCm =
        CurrentTackleContactSummary.OpponentContactHeightCm;
    Incident.ContactOrder = GetTackleContactOrder();
    Incident.BallContactNormalizedTime =
        CurrentTackleContactSummary.BallContactNormalizedTime;
    Incident.OpponentContactNormalizedTime =
        CurrentTackleContactSummary.OpponentContactNormalizedTime;
    Incident.bBallContactOccurred =
        CurrentTackleContactSummary.bBallContactOccurred;
    Incident.bBallContactApplied =
        CurrentTackleContactSummary.bBallContactApplied;
    Incident.bContactFromBehind =
        CurrentTackleContactSummary.bOpponentContactFromBehind;

    // Set this before submitting because a valid foul may synchronously change
    // match state and stop/cancel actions.
    bTackleFoulEvaluationSubmitted = true;

    MatchManager->SubmitFoulIncident(Incident);
}

ESoccerTeam ASoccerCharacterBase::GetTeam() const
{
	return Team;
}

ESoccerPlayerRole ASoccerCharacterBase::GetPlayerRole() const
{
	return PlayerRole;
}

USoccerPlayerProfile* ASoccerCharacterBase::GetPlayerProfile() const
{
	return PlayerProfile;
}

bool ASoccerCharacterBase::HasPlayerProfile() const
{
	return IsValid(PlayerProfile);
}

FName ASoccerCharacterBase::GetPlayerProfileId() const
{
	return HasPlayerProfile()
		? PlayerProfile->Identity.PlayerId
		: NAME_None;
}

void ASoccerCharacterBase::SetPlayerProfileForMatch(
	USoccerPlayerProfile* NewPlayerProfile
)
{
	PlayerProfile = NewPlayerProfile;

	ApplyPlayerProfileAppearance();
	ApplyPlayerProfileAccelerationTuning();
	OnPlayerProfileChangedForMatch();
}

void ASoccerCharacterBase::CapturePlayerProfileAppearanceBaseline()
{
	if (bPlayerProfileAppearanceBaselineCaptured)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (CharacterMesh == nullptr)
	{
		return;
	}

	PlayerProfileBaselineSkeletalMesh = CharacterMesh->SkeletalMesh;
	bPlayerProfileAppearanceBaselineCaptured = true;
}

void ASoccerCharacterBase::ApplyPlayerProfileAppearance()
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (CharacterMesh == nullptr)
	{
		return;
	}

	CapturePlayerProfileAppearanceBaseline();
	if (!bPlayerProfileAppearanceBaselineCaptured)
	{
		return;
	}

	USkeletalMesh* DesiredMesh = PlayerProfileBaselineSkeletalMesh;
	const TCHAR* MeshSource = TEXT("Baseline");
	if (HasPlayerProfile() && !PlayerProfile->Appearance.MeshOverride.IsNull())
	{
		USkeletalMesh* ProfileMesh =
			PlayerProfile->Appearance.MeshOverride.LoadSynchronous();

		if (ProfileMesh != nullptr)
		{
			DesiredMesh = ProfileMesh;
			MeshSource = TEXT("MeshOverride");
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[PlayerAppearance] %s could not load MeshOverride for profile %s; restoring baseline mesh."),
				*GetName(),
				*GetPlayerProfileId().ToString()
			);
		}
	}
	else if (
		HasPlayerProfile() &&
		!PlayerProfile->Appearance.BodyVariantId.IsNone()
	)
	{
		UWorld* ProfileWorld = GetWorld();
		ASoccerMatchManager* ProfileMatchManager = nullptr;

		if (ProfileWorld != nullptr)
		{
			for (TActorIterator<ASoccerMatchManager> It(ProfileWorld); It; ++It)
			{
				if (IsValid(*It))
				{
					ProfileMatchManager = *It;
					break;
				}
			}
		}

		USkeletalMesh* BodyVariantMesh =
			ProfileMatchManager != nullptr
				? ProfileMatchManager->ResolvePlayerBodyVariantMesh(
					PlayerProfile->Appearance.BodyVariantId
				)
				: nullptr;

		if (BodyVariantMesh != nullptr)
		{
			DesiredMesh = BodyVariantMesh;
			MeshSource = TEXT("BodyVariantId");
		}
		else if (ProfileMatchManager == nullptr)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[PlayerAppearance] %s could not find SoccerMatchManager; BodyVariantId '%s' cannot be resolved."),
				*GetName(),
				*PlayerProfile->Appearance.BodyVariantId.ToString()
			);
		}
	}

	if (CharacterMesh->SkeletalMesh != DesiredMesh)
	{
		CharacterMesh->SetSkeletalMesh(DesiredMesh, true);
	}

	ApplyPlayerProfilePersonalMaterials();

	// A replacement mesh starts with its own material array, so team uniform
	// materials must be applied again after every effective profile change.
	ApplyTeamUniform();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[PlayerAppearance] %s applied profile=%s mesh=%s source=%s."),
		*GetName(),
		HasPlayerProfile() ? *GetPlayerProfileId().ToString() : TEXT("None"),
		DesiredMesh != nullptr ? *DesiredMesh->GetName() : TEXT("None"),
		MeshSource
	);
}

void ASoccerCharacterBase::ApplyPlayerProfilePersonalMaterials()
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (CharacterMesh == nullptr)
	{
		return;
	}

	const int32 MaterialCount = CharacterMesh->GetNumMaterials();

	auto ApplyPersonalSlot = [this, CharacterMesh, MaterialCount](
		FName SemanticSlotName,
		int32 FallbackIndex,
		const TSoftObjectPtr<UMaterialInterface>& MaterialReference,
		const TCHAR* SlotLabel
	)
	{
		const int32 MaterialIndex = ResolveCharacterMaterialSlotIndex(
			CharacterMesh,
			SemanticSlotName,
			FallbackIndex
		);
		if (MaterialIndex < 0 || MaterialIndex >= MaterialCount)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[PlayerAppearance] %s cannot apply %s: material index %d is outside mesh material count %d."),
				*GetName(),
				SlotLabel,
				MaterialIndex,
				MaterialCount
			);
			return;
		}

		UMaterialInterface* PersonalMaterial = nullptr;
		if (!MaterialReference.IsNull())
		{
			PersonalMaterial = MaterialReference.LoadSynchronous();
			if (PersonalMaterial == nullptr)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[PlayerAppearance] %s could not load %s for profile %s; mesh default material restored."),
					*GetName(),
					SlotLabel,
					*GetPlayerProfileId().ToString()
				);
			}
		}

		// A null override deliberately restores the material authored on the
		// SkeletalMesh. This prevents a previous profile's material from leaking
		// into a later profile that leaves this field empty.
		CharacterMesh->SetMaterial(MaterialIndex, PersonalMaterial);
	};

	if (HasPlayerProfile())
	{
		ApplyPersonalSlot(
			TEXT("M_Eyelashes"),
			EyelashesMaterialIndex,
			PlayerProfile->Appearance.EyelashesMaterial,
			TEXT("EyelashesMaterial")
		);
		ApplyPersonalSlot(
			TEXT("M_Body"),
			BodyMaterialIndex,
			PlayerProfile->Appearance.BodyMaterial,
			TEXT("BodyMaterial")
		);
		ApplyPersonalSlot(
			TEXT("M_Hair"),
			HairMaterialIndex,
			PlayerProfile->Appearance.HairMaterial,
			TEXT("HairMaterial")
		);
		ApplyPersonalSlot(
			TEXT("M_Shoes"),
			ShoesMaterialIndex,
			PlayerProfile->Appearance.ShoesMaterial,
			TEXT("ShoesMaterial")
		);
	}
	else
	{
		const TSoftObjectPtr<UMaterialInterface> EmptyMaterialReference;
		ApplyPersonalSlot(
			TEXT("M_Eyelashes"),
			EyelashesMaterialIndex,
			EmptyMaterialReference,
			TEXT("EyelashesMaterial")
		);
		ApplyPersonalSlot(
			TEXT("M_Body"),
			BodyMaterialIndex,
			EmptyMaterialReference,
			TEXT("BodyMaterial")
		);
		ApplyPersonalSlot(
			TEXT("M_Hair"),
			HairMaterialIndex,
			EmptyMaterialReference,
			TEXT("HairMaterial")
		);
		ApplyPersonalSlot(
			TEXT("M_Shoes"),
			ShoesMaterialIndex,
			EmptyMaterialReference,
			TEXT("ShoesMaterial")
		);
	}
}

int32 ASoccerCharacterBase::ResolveCharacterMaterialSlotIndex(
	USkeletalMeshComponent* CharacterMesh,
	FName SemanticSlotName,
	int32 FallbackIndex
) const
{
	if (CharacterMesh == nullptr || SemanticSlotName.IsNone())
	{
		return FallbackIndex;
	}

	const int32 ExactIndex = CharacterMesh->GetMaterialIndex(SemanticSlotName);
	if (ExactIndex != INDEX_NONE)
	{
		return ExactIndex;
	}

	const FName ImportedVariantName(*FString::Printf(
		TEXT("%s_001"),
		*SemanticSlotName.ToString()
	));
	const int32 ImportedVariantIndex =
		CharacterMesh->GetMaterialIndex(ImportedVariantName);
	return ImportedVariantIndex != INDEX_NONE
		? ImportedVariantIndex
		: FallbackIndex;
}

float ASoccerCharacterBase::GetPlayerProfileDefensiveReactionAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Tactical.DefensiveReaction, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileAnticipationAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Tactical.Anticipation, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileDefensivePositioningAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Tactical.DefensivePositioning, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileMarkingAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Tactical.Marking, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileTacklingAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Technical.Tackling, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileStrengthAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Physical.Strength, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileAerialAbilityAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Technical.AerialAbility, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileOffBallPositioningAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Tactical.OffBallPositioning, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileDecisionMakingAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Tactical.DecisionMaking, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileComposureAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Tactical.Composure, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileGoalkeeperReflexesAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Goalkeeper.Reflexes, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileGoalkeeperPositioningAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Goalkeeper.Positioning, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileGoalkeeperHandlingAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Goalkeeper.Handling, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileGoalkeeperDivingAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Goalkeeper.Diving, 0, 100) / 100.0f
		: 0.5f;
}

float ASoccerCharacterBase::GetPlayerProfileGoalkeeperDistributionAlpha() const
{
	return HasPlayerProfile()
		? FMath::Clamp(PlayerProfile->Attributes.Goalkeeper.Distribution, 0, 100) / 100.0f
		: 0.5f;
}

void ASoccerCharacterBase::ApplyPlayerProfileAccelerationTuning()
{
	UCharacterMovementComponent* ProfileCharacterMovement = GetCharacterMovement();
	if (ProfileCharacterMovement == nullptr)
	{
		return;
	}

	if (!bPlayerProfileAccelerationBaselineCaptured)
	{
		PlayerProfileBaselineMaxAcceleration =
			ProfileCharacterMovement->MaxAcceleration;
		bPlayerProfileAccelerationBaselineCaptured = true;
	}

	ProfileCharacterMovement->MaxAcceleration =
		PlayerProfileBaselineMaxAcceleration *
		GetPlayerProfileAccelerationMultiplier();
}

void ASoccerCharacterBase::OnPlayerProfileChangedForMatch()
{
	// Human and AI classes intentionally keep separate pace/energy systems.
}

float ASoccerCharacterBase::GetPlayerProfilePaceSpeedMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	const float AttributeAlpha =
		FMath::Clamp(PlayerProfile->Attributes.Physical.Pace, 0, 100) / 100.0f;

	return FMath::Lerp(
		PaceSpeedMultiplierAtZero,
		PaceSpeedMultiplierAtHundred,
		AttributeAlpha
	);
}

float ASoccerCharacterBase::GetPlayerProfileAccelerationMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	const float AttributeAlpha =
		FMath::Clamp(PlayerProfile->Attributes.Physical.Acceleration, 0, 100) / 100.0f;

	return FMath::Lerp(
		AccelerationMultiplierAtZero,
		AccelerationMultiplierAtHundred,
		AttributeAlpha
	);
}

float ASoccerCharacterBase::GetPlayerProfileStaminaDrainMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	const float AttributeAlpha =
		FMath::Clamp(PlayerProfile->Attributes.Physical.Stamina, 0, 100) / 100.0f;

	return FMath::Lerp(
		StaminaDrainMultiplierAtZero,
		StaminaDrainMultiplierAtHundred,
		AttributeAlpha
	);
}

float ASoccerCharacterBase::GetPlayerProfileStaminaRecoveryMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	const float AttributeAlpha =
		FMath::Clamp(PlayerProfile->Attributes.Physical.StaminaRecovery, 0, 100) / 100.0f;

	return FMath::Lerp(
		StaminaRecoveryMultiplierAtZero,
		StaminaRecoveryMultiplierAtHundred,
		AttributeAlpha
	);
}

float ASoccerCharacterBase::GetProfileAdjustedPaceSpeed(float BaseSpeed) const
{
	return FMath::Max(0.0f, BaseSpeed) *
		GetPlayerProfilePaceSpeedMultiplier();
}

namespace
{
	float SoccerClampProfileAttributeAlpha(int32 AttributeValue)
	{
		return FMath::Clamp(AttributeValue, 0, 100) / 100.0f;
	}
}

FVector ASoccerCharacterBase::GetProfileAdjustedTechnicalKickTarget(
	const FVector& BallLocation,
	const FVector& IntendedTarget,
	bool bShot
) const
{
	if (!HasPlayerProfile())
	{
		return IntendedTarget;
	}

	FVector BallToTarget = IntendedTarget - BallLocation;
	BallToTarget.Z = 0.0f;

	if (BallToTarget.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		return IntendedTarget;
	}

	const int32 AccuracyValue = bShot
		? PlayerProfile->Attributes.Technical.ShootingAccuracy
		: PlayerProfile->Attributes.Technical.PassingAccuracy;

	const float AccuracyAlpha =
		SoccerClampProfileAttributeAlpha(AccuracyValue);

	const float BaseMaxAngularError = bShot
		? FMath::Lerp(
			ShootingMaxAngularErrorDegreesAtZero,
			ShootingMaxAngularErrorDegreesAtHundred,
			AccuracyAlpha
		)
		: FMath::Lerp(
			PassingMaxAngularErrorDegreesAtZero,
			PassingMaxAngularErrorDegreesAtHundred,
			AccuracyAlpha
		);

	const float PressureAlpha = GetPlayerProfileTechnicalPressureAlpha();

	const float ComposureAlpha = SoccerClampProfileAttributeAlpha(
		PlayerProfile->Attributes.Tactical.Composure
	);

	const float FullPressureErrorMultiplier = FMath::Lerp(
		ComposurePressureErrorMultiplierAtZero,
		ComposurePressureErrorMultiplierAtHundred,
		ComposureAlpha
	);

	const float EffectiveMaxAngularError =
		FMath::Max(0.0f, BaseMaxAngularError) *
		FMath::Lerp(1.0f, FullPressureErrorMultiplier, PressureAlpha);

	if (EffectiveMaxAngularError <= KINDA_SMALL_NUMBER)
	{
		return IntendedTarget;
	}

	const float SignedErrorDegrees = FMath::FRandRange(
		-EffectiveMaxAngularError,
		EffectiveMaxAngularError
	);

	const FVector AdjustedHorizontal = BallToTarget.RotateAngleAxis(
		SignedErrorDegrees,
		FVector::UpVector
	);

	FVector AdjustedTarget = BallLocation + AdjustedHorizontal;
	AdjustedTarget.Z = IntendedTarget.Z;

	return AdjustedTarget;
}

float ASoccerCharacterBase::GetProfileAdjustedTechnicalKickSpeed(
	float BaseHorizontalSpeed,
	bool bShot
) const
{
	const float SafeBaseSpeed = FMath::Max(0.0f, BaseHorizontalSpeed);

	if (!HasPlayerProfile())
	{
		return SafeBaseSpeed;
	}

	const float PressureAlpha = GetPlayerProfileTechnicalPressureAlpha();

	const float ComposureAlpha = SoccerClampProfileAttributeAlpha(
		PlayerProfile->Attributes.Tactical.Composure
	);

	if (bShot)
	{
		const float ShotPowerAlpha = SoccerClampProfileAttributeAlpha(
			PlayerProfile->Attributes.Technical.ShotPower
		);

		const float ShotPowerMultiplier = FMath::Lerp(
			ShotPowerMultiplierAtZero,
			ShotPowerMultiplierAtHundred,
			ShotPowerAlpha
		);

		const float FullPressureRetention = FMath::Lerp(
			ComposureShotPowerRetentionAtZero,
			ComposureShotPowerRetentionAtHundred,
			ComposureAlpha
		);

		return SafeBaseSpeed *
			FMath::Max(0.10f, ShotPowerMultiplier) *
			FMath::Lerp(1.0f, FullPressureRetention, PressureAlpha);
	}

	const float PassingAccuracyAlpha = SoccerClampProfileAttributeAlpha(
		PlayerProfile->Attributes.Technical.PassingAccuracy
	);

	const float BaseSpeedVariation = FMath::Lerp(
		PassingSpeedVariationAtZero,
		PassingSpeedVariationAtHundred,
		PassingAccuracyAlpha
	);

	const float FullPressureErrorMultiplier = FMath::Lerp(
		ComposurePressureErrorMultiplierAtZero,
		ComposurePressureErrorMultiplierAtHundred,
		ComposureAlpha
	);

	const float EffectiveVariation =
		FMath::Max(0.0f, BaseSpeedVariation) *
		FMath::Lerp(1.0f, FullPressureErrorMultiplier, PressureAlpha);

	const float SpeedFactor = FMath::Max(
		0.10f,
		1.0f + FMath::FRandRange(-EffectiveVariation, EffectiveVariation)
	);

	return SafeBaseSpeed * SpeedFactor;
}

FVector ASoccerCharacterBase::GetProfileAdjustedDribbleDirection(
	const FVector& IntendedDirection,
	bool bAutoPass
) const
{
	FVector SafeDirection = IntendedDirection;
	SafeDirection.Z = 0.0f;
	SafeDirection = SafeDirection.GetSafeNormal();

	if (SafeDirection.IsNearlyZero() || !HasPlayerProfile())
	{
		return SafeDirection;
	}

	const float DribblingAlpha = SoccerClampProfileAttributeAlpha(
		PlayerProfile->Attributes.Technical.Dribbling
	);

	const float BaseMaxAngularError = bAutoPass
		? FMath::Lerp(
			AutoPassMaxAngularErrorDegreesAtZero,
			AutoPassMaxAngularErrorDegreesAtHundred,
			DribblingAlpha
		)
		: FMath::Lerp(
			DribbleMaxAngularErrorDegreesAtZero,
			DribbleMaxAngularErrorDegreesAtHundred,
			DribblingAlpha
		);

	const float BallControlAlpha = SoccerClampProfileAttributeAlpha(
		PlayerProfile->Attributes.Technical.BallControl
	);

	const float FullPressureErrorMultiplier = FMath::Lerp(
		BallControlPressureErrorMultiplierAtZero,
		BallControlPressureErrorMultiplierAtHundred,
		BallControlAlpha
	);

	const float EffectiveMaxAngularError =
		FMath::Max(0.0f, BaseMaxAngularError) *
		FMath::Lerp(
			1.0f,
			FullPressureErrorMultiplier,
			GetPlayerProfileTechnicalPressureAlpha()
		);

	if (EffectiveMaxAngularError <= KINDA_SMALL_NUMBER)
	{
		return SafeDirection;
	}

	const float SignedErrorDegrees = FMath::FRandRange(
		-EffectiveMaxAngularError,
		EffectiveMaxAngularError
	);

	FVector AdjustedDirection = SafeDirection.RotateAngleAxis(
		SignedErrorDegrees,
		FVector::UpVector
	);
	AdjustedDirection.Z = 0.0f;

	return AdjustedDirection.GetSafeNormal();
}

FVector ASoccerCharacterBase::GetProfileAdjustedAutoPassTarget(
	const FVector& BallLocation,
	const FVector& IntendedTarget
) const
{
	FVector BallToTarget = IntendedTarget - BallLocation;
	const float IntendedHeightDelta = BallToTarget.Z;
	BallToTarget.Z = 0.0f;

	const float HorizontalDistance = BallToTarget.Size2D();
	if (HorizontalDistance <= KINDA_SMALL_NUMBER)
	{
		return IntendedTarget;
	}

	const FVector AdjustedDirection = GetProfileAdjustedDribbleDirection(
		BallToTarget,
		true
	);

	if (AdjustedDirection.IsNearlyZero())
	{
		return IntendedTarget;
	}

	FVector AdjustedTarget =
		BallLocation + AdjustedDirection * HorizontalDistance;
	AdjustedTarget.Z = BallLocation.Z + IntendedHeightDelta;

	return AdjustedTarget;
}

float ASoccerCharacterBase::GetProfileAdjustedDribbleTouchSpeed(
	float BaseTouchSpeed,
	bool bAutoPass
) const
{
	const float SafeBaseSpeed = FMath::Max(0.0f, BaseTouchSpeed);

	if (!HasPlayerProfile())
	{
		return SafeBaseSpeed;
	}

	const float BallControlAlpha = SoccerClampProfileAttributeAlpha(
		PlayerProfile->Attributes.Technical.BallControl
	);

	const float BaseMultiplier = bAutoPass
		? 1.0f
		: FMath::Lerp(
			DribbleTouchSpeedMultiplierAtZero,
			DribbleTouchSpeedMultiplierAtHundred,
			BallControlAlpha
		);

	const float BaseVariation = bAutoPass
		? FMath::Lerp(
			AutoPassSpeedVariationAtZero,
			AutoPassSpeedVariationAtHundred,
			BallControlAlpha
		)
		: FMath::Lerp(
			DribbleTouchSpeedVariationAtZero,
			DribbleTouchSpeedVariationAtHundred,
			BallControlAlpha
		);

	const float FullPressureErrorMultiplier = FMath::Lerp(
		BallControlPressureErrorMultiplierAtZero,
		BallControlPressureErrorMultiplierAtHundred,
		BallControlAlpha
	);

	const float EffectiveVariation = FMath::Max(0.0f, BaseVariation) *
		FMath::Lerp(
			1.0f,
			FullPressureErrorMultiplier,
			GetPlayerProfileTechnicalPressureAlpha()
		);

	const float VariationFactor = FMath::Max(
		0.10f,
		1.0f + FMath::FRandRange(-EffectiveVariation, EffectiveVariation)
	);

	return SafeBaseSpeed * FMath::Max(0.10f, BaseMultiplier) * VariationFactor;
}

float ASoccerCharacterBase::GetPlayerProfileAgilityTurnDurationMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	const float AgilityAlpha = SoccerClampProfileAttributeAlpha(
		PlayerProfile->Attributes.Physical.Agility
	);

	return FMath::Lerp(
		AgilityTurnDurationMultiplierAtZero,
		AgilityTurnDurationMultiplierAtHundred,
		AgilityAlpha
	);
}

float ASoccerCharacterBase::GetPlayerProfileBalanceTurnSpeedRetention(
	float TurnAngleDegrees
) const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	const float BalanceAlpha = SoccerClampProfileAttributeAlpha(
		PlayerProfile->Attributes.Physical.Balance
	);

	const float FullEffectRetention = FMath::Lerp(
		BalanceSharpTurnSpeedRetentionAtZero,
		BalanceSharpTurnSpeedRetentionAtHundred,
		BalanceAlpha
	);

	const float SafeFullEffectAngle = FMath::Max(1.0f, BalanceFullEffectTurnAngleDegrees);
	const float TurnEffectAlpha = FMath::Clamp(
		FMath::Abs(TurnAngleDegrees) / SafeFullEffectAngle,
		0.0f,
		1.0f
	);

	return FMath::Lerp(1.0f, FullEffectRetention, TurnEffectAlpha);
}

float ASoccerCharacterBase::GetPlayerProfileTackleBallContactRadiusMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Lerp(
		TackleBallContactRadiusMultiplierAtZero,
		TackleBallContactRadiusMultiplierAtHundred,
		GetPlayerProfileTacklingAlpha()
	);
}

float ASoccerCharacterBase::GetPlayerProfileStrengthAerialContestScoreAdjustment() const
{
	if (!HasPlayerProfile())
	{
		return 0.0f;
	}

	return FMath::Lerp(
		StrengthAerialContestScoreAdjustmentAtZero,
		StrengthAerialContestScoreAdjustmentAtHundred,
		GetPlayerProfileStrengthAlpha()
	);
}

float ASoccerCharacterBase::GetPlayerProfileStrengthBodyForceMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Lerp(
		StrengthBodyForceMultiplierAtZero,
		StrengthBodyForceMultiplierAtHundred,
		GetPlayerProfileStrengthAlpha()
	);
}

float ASoccerCharacterBase::GetPlayerProfileStrengthBodyResistanceMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Lerp(
		StrengthBodyResistanceMultiplierAtZero,
		StrengthBodyResistanceMultiplierAtHundred,
		GetPlayerProfileStrengthAlpha()
	);
}

float ASoccerCharacterBase::GetPlayerProfileStrengthTackleFallInertiaMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Lerp(
		StrengthTackleFallInertiaMultiplierAtZero,
		StrengthTackleFallInertiaMultiplierAtHundred,
		GetPlayerProfileStrengthAlpha()
	);
}

float ASoccerCharacterBase::GetPlayerProfileAerialAbilityContestScoreAdjustment() const
{
	if (!HasPlayerProfile())
	{
		return 0.0f;
	}

	return FMath::Lerp(
		AerialAbilityContestScoreAdjustmentAtZero,
		AerialAbilityContestScoreAdjustmentAtHundred,
		GetPlayerProfileAerialAbilityAlpha()
	);
}

float ASoccerCharacterBase::GetPlayerProfileAerialHeadContactRadiusMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Lerp(
		AerialHeadContactRadiusMultiplierAtZero,
		AerialHeadContactRadiusMultiplierAtHundred,
		GetPlayerProfileAerialAbilityAlpha()
	);
}

float ASoccerCharacterBase::GetPlayerProfileAerialContactQualityMultiplier(
	ESoccerAerialContactSurface ContactSurface
) const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	const float AbilityAlpha = GetPlayerProfileAerialAbilityAlpha();

	if (ContactSurface == ESoccerAerialContactSurface::Chest)
	{
		return FMath::Lerp(
			AerialChestContactQualityMultiplierAtZero,
			AerialChestContactQualityMultiplierAtHundred,
			AbilityAlpha
		);
	}

	return FMath::Lerp(
		AerialHeadContactQualityMultiplierAtZero,
		AerialHeadContactQualityMultiplierAtHundred,
		AbilityAlpha
	);
}

float ASoccerCharacterBase::GetPlayerProfileAerialHeaderPowerMultiplier() const
{
	if (!HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Lerp(
		AerialHeaderPowerMultiplierAtZero,
		AerialHeaderPowerMultiplierAtHundred,
		GetPlayerProfileAerialAbilityAlpha()
	);
}

float ASoccerCharacterBase::GetPlayerProfileTechnicalPressureAlpha() const
{
	UWorld* ProfileWorld = GetWorld();
	if (ProfileWorld == nullptr)
	{
		return 0.0f;
	}

	const float SafePressureRadius = FMath::Max(1.0f, TechnicalPressureRadius);
	float AccumulatedPressureWeight = 0.0f;

	for (TActorIterator<ASoccerCharacterBase> It(ProfileWorld); It; ++It)
	{
		const ASoccerCharacterBase* OpponentCharacter = *It;

		if (
			!IsValid(OpponentCharacter) ||
			OpponentCharacter == this ||
			OpponentCharacter->GetTeam() == GetTeam()
		)
		{
			continue;
		}

		const float OpponentDistance2D = FVector::Dist2D(
			GetActorLocation(),
			OpponentCharacter->GetActorLocation()
		);

		if (OpponentDistance2D >= SafePressureRadius)
		{
			continue;
		}

		AccumulatedPressureWeight +=
			1.0f - (OpponentDistance2D / SafePressureRadius);
	}

	return FMath::Clamp(
		AccumulatedPressureWeight /
			FMath::Max(0.10f, TechnicalPressureWeightForFullPressure),
		0.0f,
		1.0f
	);
}

bool ASoccerCharacterBase::IsPlayerProfileShotTarget(
	const FVector& IntendedTarget
) const
{
	UWorld* ProfileWorld = GetWorld();
	if (ProfileWorld == nullptr)
	{
		return false;
	}

	const ESoccerTeam OpponentTeam =
		GetTeam() == ESoccerTeam::PlayerTeam
		? ESoccerTeam::OpponentTeam
		: ESoccerTeam::PlayerTeam;

	for (TActorIterator<ASoccerMatchManager> It(ProfileWorld); It; ++It)
	{
		const ASoccerMatchManager* ProfileMatchManager = *It;
		if (!IsValid(ProfileMatchManager))
		{
			continue;
		}

		const FVector OpponentGoalCenter =
			ProfileMatchManager->GetOwnGoalCenterLocation(OpponentTeam);

		return FVector::Dist2D(IntendedTarget, OpponentGoalCenter) <=
			FMath::Max(100.0f, ShotTargetRecognitionRadius);
	}

	return false;
}

void ASoccerCharacterBase::ApplyTeamUniform()
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();

	if (CharacterMesh == nullptr)
	{
		return;
	}

	UMaterialInterface* ShirtMaterial = nullptr;
	UMaterialInterface* ShortsMaterial = nullptr;

	if (Team == ESoccerTeam::PlayerTeam)
	{
		ShirtMaterial = PlayerTeamShirtMaterial;
		ShortsMaterial = PlayerTeamShortsMaterial;
	}
	else
	{
		ShirtMaterial = OpponentTeamShirtMaterial;
		ShortsMaterial = OpponentTeamShortsMaterial;
	}

	if (ShirtMaterial != nullptr && ShirtMaterialIndex >= 0)
	{
		CharacterMesh->SetMaterial(
			ResolveCharacterMaterialSlotIndex(
				CharacterMesh,
				TEXT("M_Shirt"),
				ShirtMaterialIndex
			),
			ShirtMaterial
		);
	}

	if (ShortsMaterial != nullptr && ShortsMaterialIndex >= 0)
	{
		CharacterMesh->SetMaterial(
			ResolveCharacterMaterialSlotIndex(
				CharacterMesh,
				TEXT("M_Shorts"),
				ShortsMaterialIndex
			),
			ShortsMaterial
		);
	}
}

void ASoccerCharacterBase::ApplyClubKitMaterials(
	UMaterialInterface* SocksMaterial,
	UMaterialInterface* ShirtMaterial,
	UMaterialInterface* ShortsMaterial
)
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (CharacterMesh == nullptr)
	{
		return;
	}

	const int32 MaterialCount = CharacterMesh->GetNumMaterials();
	auto ApplyClubMaterial = [this, CharacterMesh, MaterialCount](
		FName SemanticSlotName,
		int32 FallbackIndex,
		UMaterialInterface* Material,
		const TCHAR* SlotLabel
	)
	{
		if (Material == nullptr)
		{
			return;
		}

		const int32 MaterialIndex = ResolveCharacterMaterialSlotIndex(
			CharacterMesh,
			SemanticSlotName,
			FallbackIndex
		);

		if (MaterialIndex < 0 || MaterialIndex >= MaterialCount)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[ClubKit] %s cannot apply %s: index %d is outside material count %d."),
				*GetName(),
				SlotLabel,
				MaterialIndex,
				MaterialCount
			);
			return;
		}

		CharacterMesh->SetMaterial(MaterialIndex, Material);
	};

	ApplyClubMaterial(
		TEXT("M_Socks"),
		SocksMaterialIndex,
		SocksMaterial,
		TEXT("Socks")
	);
	ApplyClubMaterial(
		TEXT("M_Shirt"),
		ShirtMaterialIndex,
		ShirtMaterial,
		TEXT("Shirt")
	);
	ApplyClubMaterial(
		TEXT("M_Shorts"),
		ShortsMaterialIndex,
		ShortsMaterial,
		TEXT("Shorts")
	);
}

void ASoccerCharacterBase::UpdateSoccerAnimationState()
{
	bSoccerIsPossessingBall = false;
	bSoccerIsChasingBall = false;
	bSoccerIsKicking = false;

	SoccerEnergyPercent = 1.0f;

	bSoccerShouldForceDribbleTurnLocomotion = false;
	SoccerForcedDribbleTurnLocomotionSpeed = 0.0f;
}

void ASoccerCharacterBase::SetSoccerAnimationState(
	bool bInIsPossessingBall,
	bool bInIsChasingBall,
	bool bInIsKicking,
	float InEnergyPercent,
	bool bInShouldForceDribbleTurnLocomotion,
	float InForcedDribbleTurnLocomotionSpeed
)
{
	bSoccerIsPossessingBall = bInIsPossessingBall;
	bSoccerIsChasingBall = bInIsChasingBall;
	bSoccerIsKicking = bInIsKicking;

	SoccerEnergyPercent = FMath::Clamp(InEnergyPercent, 0.0f, 1.0f);

	bSoccerShouldForceDribbleTurnLocomotion =
		bInShouldForceDribbleTurnLocomotion;

	SoccerForcedDribbleTurnLocomotionSpeed =
		InForcedDribbleTurnLocomotionSpeed;
}

float ASoccerCharacterBase::EstimateArrivalTimeToLocation(
	const FVector& TargetLocation,
	float ReachRadius
) const
{
	const UCharacterMovementComponent* MovementComponent =
		GetCharacterMovement();

	if (MovementComponent == nullptr)
	{
		return TNumericLimits<float>::Max();
	}

	FVector ToTarget = TargetLocation - GetActorLocation();
	ToTarget.Z = 0.0f;

	const float RawDistance = ToTarget.Size();
	const float SafeReachRadius =
		ReachRadius >= 0.0f
		? ReachRadius
		: InterceptionReachRadius;

	const float TravelDistance =
		FMath::Max(0.0f, RawDistance - SafeReachRadius);

	if (TravelDistance <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FVector DirectionToTarget =
		ToTarget / FMath::Max(RawDistance, KINDA_SMALL_NUMBER);

	FVector CurrentHorizontalVelocity = GetVelocity();
	CurrentHorizontalVelocity.Z = 0.0f;

	const float CurrentHorizontalSpeed =
		CurrentHorizontalVelocity.Size();

	FVector FacingReference = GetActorForwardVector();
	FacingReference.Z = 0.0f;
	FacingReference.Normalize();

	if (CurrentHorizontalSpeed > 50.0f)
	{
		FacingReference =
			CurrentHorizontalVelocity / CurrentHorizontalSpeed;
	}

	const float FacingDot =
		FMath::Clamp(
			FVector::DotProduct(
				FacingReference,
				DirectionToTarget
			),
			-1.0f,
			1.0f
		);

	const float TurnAngleDegrees =
		FMath::RadiansToDegrees(FMath::Acos(FacingDot));

	float EstimatedTurnRate =
		FMath::Abs(MovementComponent->RotationRate.Yaw);

	if (EstimatedTurnRate <= KINDA_SMALL_NUMBER)
	{
		EstimatedTurnRate =
			InterceptionEstimatedTurnRateDegreesPerSecond;
	}

	EstimatedTurnRate = FMath::Max(1.0f, EstimatedTurnRate);

	const float TurnTime =
		(TurnAngleDegrees / EstimatedTurnRate) *
		FMath::Max(0.0f, InterceptionTurnTimeScale);

	const float VelocityTowardTarget =
		FVector::DotProduct(
			CurrentHorizontalVelocity,
			DirectionToTarget
		);

	const float OpposingSpeed =
		FMath::Max(0.0f, -VelocityTowardTarget);

	const float BrakingDeceleration =
		FMath::Max(
			1.0f,
			MovementComponent->BrakingDecelerationWalking
		);

	const float OpposingVelocityTime =
		(OpposingSpeed / BrakingDeceleration) *
		FMath::Max(
			0.0f,
			InterceptionOpposingVelocityTimeScale
		);

	const float MaximumSpeed =
		FMath::Max(1.0f, MovementComponent->GetMaxSpeed());

	const float InitialUsefulSpeed =
		FMath::Clamp(
			VelocityTowardTarget,
			0.0f,
			MaximumSpeed
		);

	const float Acceleration =
		FMath::Max(1.0f, MovementComponent->MaxAcceleration);

	const float TimeToMaximumSpeed =
		FMath::Max(
			0.0f,
			(MaximumSpeed - InitialUsefulSpeed) /
			Acceleration
		);

	const float DistanceDuringAcceleration =
		InitialUsefulSpeed * TimeToMaximumSpeed +
		0.5f * Acceleration *
		FMath::Square(TimeToMaximumSpeed);

	float TravelTime = 0.0f;

	if (TravelDistance <= DistanceDuringAcceleration)
	{
		const float Discriminant =
			FMath::Square(InitialUsefulSpeed) +
			2.0f * Acceleration * TravelDistance;

		TravelTime =
			(
				-FMath::Max(0.0f, InitialUsefulSpeed) +
				FMath::Sqrt(FMath::Max(0.0f, Discriminant))
			) /
			Acceleration;
	}
	else
	{
		const float RemainingDistance =
			TravelDistance - DistanceDuringAcceleration;

		TravelTime =
			TimeToMaximumSpeed +
			RemainingDistance / MaximumSpeed;
	}

	return
		FMath::Max(0.0f, TurnTime) +
		FMath::Max(0.0f, OpposingVelocityTime) +
		FMath::Max(0.0f, TravelTime);
}

bool ASoccerCharacterBase::FindBestBallInterception(
	const ASoccerBall* SoccerBall,
	FSoccerBallInterceptionResult& OutResult
) const
{
	OutResult = FSoccerBallInterceptionResult();

	if (!IsValid(SoccerBall))
	{
		return false;
	}

	TArray<FSoccerBallTrajectorySample> Trajectory;

	if (
		!SoccerBall->BuildPredictedTrajectory(
			InterceptionPredictionHorizon,
			InterceptionTrajectorySampleInterval,
			Trajectory
		)
	)
	{
		return false;
	}

	return FindBestBallInterceptionFromTrajectory(
		Trajectory,
		SoccerBall->GetTrajectoryRevision(),
		OutResult
	);
}

bool ASoccerCharacterBase::ResolveStableBallPursuitTarget(
	const ASoccerBall* SoccerBall,
	FVector& OutPursuitLocation,
	FSoccerBallInterceptionResult* OutResult,
	float RefreshIntervalOverride,
	bool bForceRefresh
)
{
	OutPursuitLocation = FVector::ZeroVector;

	if (OutResult != nullptr)
	{
		*OutResult = FSoccerBallInterceptionResult();
	}

	if (!IsValid(SoccerBall))
	{
		ClearBallPursuitTarget();
		return false;
	}

	OutPursuitLocation = SoccerBall->GetActorLocation();

	if (
		StableGroundBallPursuitBall.IsValid() &&
		StableGroundBallPursuitBall.Get() != SoccerBall
	)
	{
		ClearBallPursuitTarget();
	}

	const FVector BallVelocity = SoccerBall->GetBallPhysicsVelocity();
	const float BallSpeed = BallVelocity.Size();
	const float DistanceToBall = FVector::Dist2D(
		GetActorLocation(),
		SoccerBall->GetActorLocation()
	);

	const bool bShouldUseDirectTarget =
		!bUsePredictiveGroundBallPursuit ||
		DistanceToBall <= GroundBallPursuitDirectChaseDistance ||
		BallSpeed <= GroundBallPursuitDirectChaseSpeed ||
		!SoccerBall->IsAvailableForTrajectoryPrediction();

	if (bShouldUseDirectTarget)
	{
		ClearBallPursuitTarget();

		if (OutResult != nullptr)
		{
			OutResult->bHasSolution = true;
			OutResult->bCanArriveInTime = true;
			OutResult->InterceptionLocation = OutPursuitLocation;
			OutResult->PlayerArrivalTime =
				EstimateArrivalTimeToLocation(OutPursuitLocation);
			OutResult->BallArrivalTime = 0.0f;
			OutResult->ArrivalTimeMargin =
				-OutResult->PlayerArrivalTime;
			OutResult->TrajectoryRevision =
				SoccerBall->GetTrajectoryRevision();
		}

		return true;
	}

	UWorld* World = GetWorld();
	const float CurrentTime =
		World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	const int32 CurrentTrajectoryRevision =
		SoccerBall->GetTrajectoryRevision();

	const bool bTrajectoryChanged =
		bHasStableGroundBallPursuitTarget &&
		StableBallPursuitTrajectoryRevision !=
		CurrentTrajectoryRevision;

	if (bTrajectoryChanged)
	{
		bForceRefresh = true;
	}

	const float RefreshInterval =
		RefreshIntervalOverride >= 0.0f
		? FMath::Max(0.02f, RefreshIntervalOverride)
		: FMath::Max(0.02f, GroundBallPursuitRefreshInterval);

	bool bActiveTargetExpired = bTrajectoryChanged;
	bool bActiveTargetCanStillBeReached = false;
	float ActiveCurrentMargin = -TNumericLimits<float>::Max();

	if (bHasStableGroundBallPursuitTarget && !bTrajectoryChanged)
	{
		const float TargetAge = FMath::Max(
			0.0f,
			CurrentTime - StableGroundBallPursuitSelectionTime
		);

		const float RemainingBallTime =
			StableGroundBallPursuitResult.BallArrivalTime - TargetAge;

		const float CurrentPlayerArrivalTime =
			EstimateArrivalTimeToLocation(
				StableGroundBallPursuitLocation,
				InterceptionReachRadius
			);

		if (StableGroundBallPursuitResult.bBallStoppedAtSample)
		{
			bActiveTargetCanStillBeReached = true;
			ActiveCurrentMargin = 0.0f;
		}
		else
		{
			bActiveTargetExpired = RemainingBallTime < -0.05f;
			ActiveCurrentMargin =
				RemainingBallTime - CurrentPlayerArrivalTime;
			bActiveTargetCanStillBeReached =
				!bActiveTargetExpired &&
				ActiveCurrentMargin >=
				-InterceptionArrivalSafetyMargin;
		}
	}

	const bool bRefreshDue =
		bForceRefresh ||
		!bHasStableGroundBallPursuitTarget ||
		bActiveTargetExpired ||
		CurrentTime - StableGroundBallPursuitLastRefreshTime >=
		RefreshInterval;

	if (!bRefreshDue)
	{
		OutPursuitLocation = StableGroundBallPursuitLocation;

		if (OutResult != nullptr)
		{
			*OutResult = StableGroundBallPursuitResult;
		}

		return true;
	}

	StableGroundBallPursuitLastRefreshTime = CurrentTime;

	FSoccerBallInterceptionResult CandidateResult;

	if (!FindBestBallInterception(SoccerBall, CandidateResult))
	{
		ClearBallPursuitTarget();
		OutPursuitLocation = SoccerBall->GetActorLocation();
		return true;
	}

	const FVector CandidateLocation = CandidateResult.InterceptionLocation;
	bool bAcceptCandidate =
		!bHasStableGroundBallPursuitTarget ||
		bTrajectoryChanged;

	if (
		bHasStableGroundBallPursuitTarget &&
		!bTrajectoryChanged
	)
	{
		const float TargetAge = FMath::Max(
			0.0f,
			CurrentTime - StableGroundBallPursuitSelectionTime
		);

		const float TargetChangeDistance = FVector::Dist2D(
			StableGroundBallPursuitLocation,
			CandidateLocation
		);

		const bool bSoftTargetUpdate =
			TargetChangeDistance <= GroundBallPursuitSoftUpdateDistance;

		const bool bMinimumCommitCompleted =
			TargetAge >= GroundBallPursuitMinimumCommitTime;

		const bool bCandidateRestoresReachability =
			CandidateResult.bCanArriveInTime &&
			!bActiveTargetCanStillBeReached;

		const bool bCandidateImprovesMargin =
			CandidateResult.ArrivalTimeMargin >=
			ActiveCurrentMargin +
			GroundBallPursuitRequiredMarginImprovement;

		const bool bMaterialTargetChange =
			TargetChangeDistance >= GroundBallPursuitTargetSwitchDistance;

		const bool bBounceStateChanged =
			CandidateResult.BounceCount !=
			StableGroundBallPursuitResult.BounceCount;

		bAcceptCandidate =
			bActiveTargetExpired ||
			bSoftTargetUpdate ||
			bBounceStateChanged ||
			(
				bMinimumCommitCompleted &&
				(
					bCandidateRestoresReachability ||
					bCandidateImprovesMargin ||
					(
						bMaterialTargetChange &&
						CandidateResult.bCanArriveInTime
					)
				)
			);
	}

	if (bAcceptCandidate)
	{
		StableGroundBallPursuitBall =
			const_cast<ASoccerBall*>(SoccerBall);
		bHasStableGroundBallPursuitTarget = true;
		StableGroundBallPursuitLocation = CandidateLocation;
		StableGroundBallPursuitResult = CandidateResult;
		StableGroundBallPursuitSelectionTime = CurrentTime;
		StableBallPursuitTrajectoryRevision =
			CurrentTrajectoryRevision;
	}

	if (bHasStableGroundBallPursuitTarget)
	{
		OutPursuitLocation = StableGroundBallPursuitLocation;

		if (OutResult != nullptr)
		{
			*OutResult = StableGroundBallPursuitResult;
		}

		return true;
	}

	OutPursuitLocation = SoccerBall->GetActorLocation();
	return true;
}

void ASoccerCharacterBase::ClearBallPursuitTarget()
{
	StableGroundBallPursuitBall.Reset();
	bHasStableGroundBallPursuitTarget = false;
	StableGroundBallPursuitLocation = FVector::ZeroVector;
	StableGroundBallPursuitResult = FSoccerBallInterceptionResult();
	StableGroundBallPursuitSelectionTime = -1000.0f;
	StableGroundBallPursuitLastRefreshTime = -1000.0f;
	StableBallPursuitTrajectoryRevision = 0;
}

bool ASoccerCharacterBase::FindBestBallInterceptionFromTrajectory(
	const TArray<FSoccerBallTrajectorySample>& Trajectory,
	int32 TrajectoryRevision,
	FSoccerBallInterceptionResult& OutResult
) const
{
	OutResult = FSoccerBallInterceptionResult();

	if (Trajectory.Num() <= 0)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float CharacterGroundZ =
		GetActorLocation().Z -
		(Capsule != nullptr
			? Capsule->GetScaledCapsuleHalfHeight()
			: 0.0f);

	float BestLateArrival = TNumericLimits<float>::Max();
	FSoccerBallInterceptionResult BestFallback;

	for (int32 SampleIndex = 0; SampleIndex < Trajectory.Num(); ++SampleIndex)
	{
		const FSoccerBallTrajectorySample& Sample =
			Trajectory[SampleIndex];

		const float BallHeightAboveGround =
			Sample.Location.Z - CharacterGroundZ;

		if (
			BallHeightAboveGround <
			InterceptionMinimumPlayableBallHeight ||
			BallHeightAboveGround >
			InterceptionMaximumPlayableBallHeight
		)
		{
			continue;
		}

		const float PlayerArrivalTime =
			EstimateArrivalTimeToLocation(
				Sample.Location,
				InterceptionReachRadius
			);

		if (!FMath::IsFinite(PlayerArrivalTime))
		{
			continue;
		}

		const bool bAirborneAtSample = !Sample.bNearGround;
		const bool bRisingAtSample =
			bAirborneAtSample && Sample.Velocity.Z > 0.0f;

		float RequiredSafetyMargin =
			bAirborneAtSample
			? InterceptionAerialArrivalSafetyMargin
			: InterceptionArrivalSafetyMargin;

		if (bRisingAtSample)
		{
			RequiredSafetyMargin +=
				InterceptionRisingBallExtraSafetyTime;
		}

		float EffectiveBallArrivalTime = Sample.TimeFromNow;

		if (Sample.bBallStopped)
		{
			EffectiveBallArrivalTime =
				FMath::Max(
					Sample.TimeFromNow,
					PlayerArrivalTime + RequiredSafetyMargin
				);
		}

		const float ArrivalMargin =
			EffectiveBallArrivalTime - PlayerArrivalTime;

		FSoccerBallInterceptionResult Candidate;
		Candidate.bHasSolution = true;
		Candidate.bCanArriveInTime =
			Sample.bBallStopped ||
			PlayerArrivalTime <= KINDA_SMALL_NUMBER ||
			ArrivalMargin >= RequiredSafetyMargin;
		Candidate.InterceptionLocation = Sample.Location;
		Candidate.BallArrivalTime = EffectiveBallArrivalTime;
		Candidate.PlayerArrivalTime = PlayerArrivalTime;
		Candidate.ArrivalTimeMargin = ArrivalMargin;
		Candidate.bBallStoppedAtSample = Sample.bBallStopped;
		Candidate.BounceCount = Sample.BounceCount;
		Candidate.BallHeightAboveGround = BallHeightAboveGround;
		Candidate.TrajectoryRevision = TrajectoryRevision;

		if (Candidate.bCanArriveInTime)
		{
			OutResult = Candidate;
			return true;
		}

		const float LateArrival =
			FMath::Max(
				0.0f,
				RequiredSafetyMargin - ArrivalMargin
			);

		if (LateArrival < BestLateArrival)
		{
			BestLateArrival = LateArrival;
			BestFallback = Candidate;
		}
	}

	if (BestFallback.bHasSolution)
	{
		OutResult = BestFallback;
		return true;
	}

	return false;
}

void ASoccerCharacterBase::UpdateGroundInterceptionPredictionDebug(
	float DeltaTime
)
{
	if (!ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::AIInterception))
	{
		InterceptionDebugUpdateAccumulator = 0.0f;
		return;
	}

	InterceptionDebugUpdateAccumulator += DeltaTime;

	const float SafeUpdateInterval =
		FMath::Max(0.02f, InterceptionDebugUpdateInterval);

	if (InterceptionDebugUpdateAccumulator < SafeUpdateInterval)
	{
		return;
	}

	InterceptionDebugUpdateAccumulator = 0.0f;

	ASoccerBall* SoccerBall = ResolveDebugInterceptionBall();

	if (!IsValid(SoccerBall))
	{
		return;
	}

	TArray<FSoccerBallTrajectorySample> Trajectory;

	if (
		!SoccerBall->BuildPredictedTrajectory(
			InterceptionPredictionHorizon,
			InterceptionTrajectorySampleInterval,
			Trajectory
		)
		)
	{
		return;
	}

	FSoccerBallInterceptionResult Result;
	FindBestBallInterceptionFromTrajectory(
		Trajectory,
		SoccerBall->GetTrajectoryRevision(),
		Result
	);

	DrawGroundInterceptionPredictionDebug(
		Trajectory,
		Result
	);
}

ASoccerBall* ASoccerCharacterBase::ResolveDebugInterceptionBall()
{
	if (CachedDebugInterceptionBall.IsValid())
	{
		return CachedDebugInterceptionBall.Get();
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<ASoccerBall> It(World); It; ++It)
	{
		ASoccerBall* SoccerBall = *It;

		if (IsValid(SoccerBall))
		{
			CachedDebugInterceptionBall = SoccerBall;
			return SoccerBall;
		}
	}

	return nullptr;
}

void ASoccerCharacterBase::DrawGroundInterceptionPredictionDebug(
	const TArray<FSoccerBallTrajectorySample>& Trajectory,
	const FSoccerBallInterceptionResult& Result
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr || Trajectory.Num() <= 0)
	{
		return;
	}

	const float DrawingDuration =
		FMath::Max(0.02f, InterceptionDebugDrawingDuration);

	for (int32 Index = 0; Index < Trajectory.Num(); ++Index)
	{
		const FSoccerBallTrajectorySample& Sample = Trajectory[Index];

		FColor SampleColor = Sample.bNearGround
			? FColor::Cyan
			: FColor(170, 80, 255);

		if (Sample.bAfterBounce)
		{
			SampleColor = FColor::Yellow;
		}

		if (Sample.bHitObstacle)
		{
			SampleColor = FColor::Red;
		}

		if (Sample.bBallStopped)
		{
			SampleColor = FColor(120, 190, 255);
		}

		DrawDebugSphere(
			World,
			Sample.Location,
			8.0f,
			8,
			SampleColor,
			false,
			DrawingDuration,
			0,
			1.2f
		);

		if (Index > 0)
		{
			DrawDebugLine(
				World,
				Trajectory[Index - 1].Location,
				Sample.Location,
				SampleColor,
				false,
				DrawingDuration,
				0,
				1.5f
			);
		}
	}

	if (!Result.bHasSolution)
	{
		return;
	}

	const FColor ResultColor =
		Result.bCanArriveInTime
		? FColor::Green
		: FColor::Orange;

	DrawDebugSphere(
		World,
		Result.InterceptionLocation,
		24.0f,
		16,
		ResultColor,
		false,
		DrawingDuration,
		0,
		3.0f
	);

	FVector CharacterLineStart = GetActorLocation();
	CharacterLineStart.Z = Result.InterceptionLocation.Z;

	DrawDebugLine(
		World,
		CharacterLineStart,
		Result.InterceptionLocation,
		ResultColor,
		false,
		DrawingDuration,
		0,
		3.0f
	);

	if (bInterceptionDebugShowText)
	{
		const FString StatusText =
			Result.bCanArriveInTime
			? TEXT("ALCANZABLE")
			: TEXT("FALLBACK TARDIO");

		const FString DebugText = FString::Printf(
			TEXT("%s | pelota %.2fs | jugador %.2fs | margen %+.2fs | altura %.0f | rebotes %d"),
			*StatusText,
			Result.BallArrivalTime,
			Result.PlayerArrivalTime,
			Result.ArrivalTimeMargin,
			Result.BallHeightAboveGround,
			Result.BounceCount
		);

		DrawDebugString(
			World,
			Result.InterceptionLocation + FVector(0.0f, 0.0f, 55.0f),
			DebugText,
			nullptr,
			ResultColor,
			DrawingDuration,
			false,
			1.0f
		);
	}
}

bool ASoccerCharacterBase::GetSoccerIsPossessingBall() const
{
	return bSoccerIsPossessingBall;
}

bool ASoccerCharacterBase::GetSoccerIsChasingBall() const
{
	return bSoccerIsChasingBall;
}

bool ASoccerCharacterBase::GetSoccerIsKicking() const
{
	return bSoccerIsKicking;
}

float ASoccerCharacterBase::GetSoccerEnergyPercent() const
{
	return SoccerEnergyPercent;
}

bool ASoccerCharacterBase::GetSoccerShouldForceDribbleTurnLocomotion() const
{
	return bSoccerShouldForceDribbleTurnLocomotion;
}

float ASoccerCharacterBase::GetSoccerForcedDribbleTurnLocomotionSpeed() const
{
	return SoccerForcedDribbleTurnLocomotionSpeed;
}

void ASoccerCharacterBase::ApplyInstantReplayVisualState(
	bool bInPossessingBall,
	bool bInChasingBall,
	bool bInKicking,
	bool bInForceDribbleTurnLocomotion,
	float InForcedDribbleTurnLocomotionSpeed
)
{
	bSoccerIsPossessingBall = bInPossessingBall;
	bSoccerIsChasingBall = bInChasingBall;
	bSoccerIsKicking = bInKicking;
	bSoccerShouldForceDribbleTurnLocomotion =
		bInForceDribbleTurnLocomotion;
	SoccerForcedDribbleTurnLocomotionSpeed =
		FMath::Max(0.0f, InForcedDribbleTurnLocomotionSpeed);
}

bool ASoccerCharacterBase::FindBestAerialInterceptionPlan(
    ASoccerBall* SoccerBall,
    ESoccerAerialActionIntent Intent,
    FSoccerAerialInterceptionPlan& OutPlan
) const
{
    return FindBestAerialInterceptionPlanInternal(
        SoccerBall,
        Intent,
        ESoccerAerialContactSurface::None,
        OutPlan
    );
}

bool ASoccerCharacterBase::FindBestAerialInterceptionPlanInternal(
    ASoccerBall* SoccerBall,
    ESoccerAerialActionIntent Intent,
    ESoccerAerialContactSurface PreferredStandingSurface,
    FSoccerAerialInterceptionPlan& OutPlan
) const
{
    OutPlan = FSoccerAerialInterceptionPlan();

    if (
        !IsValid(SoccerBall) ||
        !SoccerBall->IsAvailableForTrajectoryPrediction()
    )
    {
        return false;
    }

    TArray<FSoccerBallTrajectorySample> Trajectory;

    if (
        !SoccerBall->BuildPredictedTrajectory(
            AerialPredictionHorizon,
            AerialPredictionSampleInterval,
            Trajectory
        ) ||
        Trajectory.Num() <= 0
    )
    {
        return false;
    }

    TArray<const FSoccerAerialActionProfile*> CandidateProfiles;

    switch (Intent)
    {
    case ESoccerAerialActionIntent::Control:
    case ESoccerAerialActionIntent::StandingHeaderRedirect:
        CandidateProfiles.Add(&StandingAerialControlProfile);
        break;

    case ESoccerAerialActionIntent::ActiveHeader:
        CandidateProfiles.Add(&JumpHeaderKickProfile);
        break;

    case ESoccerAerialActionIntent::DefensiveBlock:
        CandidateProfiles.Add(&JumpHeaderBlockProfile);
        break;

    case ESoccerAerialActionIntent::Automatic:
    default:
        CandidateProfiles.Add(&StandingAerialControlProfile);
        CandidateProfiles.Add(&JumpHeaderBlockProfile);
        break;
    }

    bool bFoundPlan = false;
    float BestScore = TNumericLimits<float>::Max();

    for (const FSoccerAerialActionProfile* Profile : CandidateProfiles)
    {
        if (Profile == nullptr)
        {
            continue;
        }

        FSoccerAerialInterceptionPlan CandidatePlan;

        const bool bBuiltCandidate =
            Profile->ActionType == ESoccerAerialActionType::StandingControl
            ? BuildBestStandingAerialControlPlan(
                SoccerBall,
                Trajectory,
                Intent,
                PreferredStandingSurface,
                CandidatePlan
            )
            : BuildAerialPlanForProfile(
                SoccerBall,
                Trajectory,
                *Profile,
                PreferredStandingSurface,
                CandidatePlan
            );

        if (!bBuiltCandidate)
        {
            continue;
        }

        /*
         * Automatic slightly prefers a standing control when both actions
         * solve practically the same ball arrival.
         */
        float ActionBias = 0.0f;

        if (
            Intent == ESoccerAerialActionIntent::Automatic &&
            CandidatePlan.ActionType ==
                ESoccerAerialActionType::StandingControl
        )
        {
            ActionBias = -0.06f;
        }

        const float CandidateScore =
            CandidatePlan.BallArrivalTime + ActionBias;

        if (!bFoundPlan || CandidateScore < BestScore)
        {
            bFoundPlan = true;
            BestScore = CandidateScore;
            OutPlan = CandidatePlan;
        }
    }

    return bFoundPlan;
}

bool ASoccerCharacterBase::BuildAerialPlanForProfile(
    const ASoccerBall* SoccerBall,
    const TArray<FSoccerBallTrajectorySample>& Trajectory,
    const FSoccerAerialActionProfile& Profile,
    ESoccerAerialContactSurface PreferredStandingSurface,
    FSoccerAerialInterceptionPlan& OutPlan
) const
{
    OutPlan = FSoccerAerialInterceptionPlan();

    if (
        !IsValid(SoccerBall) ||
        Profile.ActionType == ESoccerAerialActionType::None ||
        Profile.ContactWindowEnd <= Profile.ContactWindowStart ||
        Profile.IdealContactTime < Profile.ContactWindowStart ||
        Profile.IdealContactTime > Profile.ContactWindowEnd
    )
    {
        return false;
    }

    /*
     * StandingControl has two intentional solutions that share the same
     * montage: chest control and soft standing header. The tactical intent
     * asks for control; the trajectory decides which surface is reachable.
     * Chest is preferred because it normally leaves the ball closer, while
     * the head becomes a physical fallback when the chest crossing is not
     * reachable. The tester can still request one surface explicitly.
     */
    if (
        Profile.ActionType == ESoccerAerialActionType::StandingControl &&
        StandingAerialControlContactTrackCurveTable != nullptr &&
        Trajectory.Num() >= 2
    )
    {
        TArray<ESoccerAerialContactSurface> SurfacePriority;

        const auto AddSurfaceIfMissing =
            [&SurfacePriority](ESoccerAerialContactSurface Surface)
            {
                if (
                    Surface != ESoccerAerialContactSurface::Chest &&
                    Surface != ESoccerAerialContactSurface::Head
                )
                {
                    return;
                }

                if (!SurfacePriority.Contains(Surface))
                {
                    SurfacePriority.Add(Surface);
                }
            };

        if (
            DebugStandingControlPlanningSurface ==
                ESoccerAerialContactSurface::Chest ||
            DebugStandingControlPlanningSurface ==
                ESoccerAerialContactSurface::Head
        )
        {
            AddSurfaceIfMissing(DebugStandingControlPlanningSurface);
        }
        else
        {
            /* Keep an already queued standing-control plan stable on refresh. */
            AddSurfaceIfMissing(PreferredStandingSurface);

            if (bPreferChestForStandingAerialControl)
            {
                AddSurfaceIfMissing(ESoccerAerialContactSurface::Chest);
                AddSurfaceIfMissing(ESoccerAerialContactSurface::Head);
            }
            else
            {
                AddSurfaceIfMissing(ESoccerAerialContactSurface::Head);
                AddSurfaceIfMissing(ESoccerAerialContactSurface::Chest);
            }
        }

        for (ESoccerAerialContactSurface Surface : SurfacePriority)
        {
            if (BuildStandingAerialControlPlanForSurface(
                SoccerBall,
                Trajectory,
                Profile,
                Surface,
                OutPlan
            ))
            {
                return true;
            }
        }

        return false;
    }

    const bool bJumpHeaderAction = IsJumpAerialAction(Profile.ActionType);

    /*
     * A jump header is solved against the exported animation track. Without
     * its Curve Table we would return to the old fixed-height approximation,
     * which can start a visually correct jump at a physically wrong point.
     */
    if (
        bJumpHeaderAction &&
        GetAerialContactTrackCurveTable(Profile.ActionType) == nullptr
    )
    {
        return false;
    }

    if (bJumpHeaderAction)
    {
        return BuildJumpHeaderPlanForProfile(
            SoccerBall,
            Trajectory,
            Profile,
            OutPlan
        );
    }

    /*
     * Compatibility fallback for StandingControl when its exported track is
     * not assigned. Jump headers never use this fixed-offset approximation.
     */
    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    const float CharacterGroundZ =
        GetActorLocation().Z -
        (
            Capsule != nullptr
            ? Capsule->GetScaledCapsuleHalfHeight()
            : 0.0f
        );
    const FVector CurrentActorLocation = GetActorLocation();

    for (int32 SampleIndex = 0; SampleIndex < Trajectory.Num(); ++SampleIndex)
    {
        const FSoccerBallTrajectorySample& Sample = Trajectory[SampleIndex];

        if (
            Sample.TimeFromNow <= KINDA_SMALL_NUMBER ||
            Sample.bNearGround ||
            Sample.bBallStopped
        )
        {
            continue;
        }

        const float BallHeightAboveGround =
            Sample.Location.Z - CharacterGroundZ;

        if (
            BallHeightAboveGround < Profile.MinimumBallHeight ||
            BallHeightAboveGround > Profile.MaximumBallHeight
        )
        {
            continue;
        }

        FVector IncomingHorizontalVelocity = Sample.Velocity;
        IncomingHorizontalVelocity.Z = 0.0f;

        FVector FacingDirection =
            -IncomingHorizontalVelocity.GetSafeNormal();

        if (FacingDirection.IsNearlyZero())
        {
            FacingDirection =
                (SoccerBall->GetActorLocation() - CurrentActorLocation)
                .GetSafeNormal2D();
        }

        if (FacingDirection.IsNearlyZero())
        {
            FacingDirection = GetActorForwardVector().GetSafeNormal2D();
        }

        if (FacingDirection.IsNearlyZero())
        {
            FacingDirection = FVector::ForwardVector;
        }

        const float RawMontageStartDelay =
            Sample.TimeFromNow - Profile.IdealContactTime;

        if (RawMontageStartDelay < -Profile.MaximumLateStartTime)
        {
            continue;
        }

        const float MontageStartDelay =
            FMath::Max(0.0f, RawMontageStartDelay);
        const float ExpectedContactTimeFromStart =
            Sample.TimeFromNow - MontageStartDelay;

        if (
            ExpectedContactTimeFromStart <
                Profile.ContactWindowStart - KINDA_SMALL_NUMBER ||
            ExpectedContactTimeFromStart >
                Profile.ContactWindowEnd + KINDA_SMALL_NUMBER
        )
        {
            continue;
        }

        FVector PreparationLocation =
            Sample.Location -
            FacingDirection * Profile.ContactForwardOffset;
        PreparationLocation.Z = CurrentActorLocation.Z;

        const float PlayerArrivalTime =
            EstimateArrivalTimeToLocation(
                PreparationLocation,
                Profile.PreparationReachRadius
            );

        if (!FMath::IsFinite(PlayerArrivalTime))
        {
            continue;
        }

        const float LatestUsefulArrivalTime = FMath::Max(
            0.0f,
            MontageStartDelay - Profile.PreparationSettleTime
        );
        const float ArrivalMargin =
            LatestUsefulArrivalTime - PlayerArrivalTime;

        if (PlayerArrivalTime > LatestUsefulArrivalTime + 0.04f)
        {
            continue;
        }

        OutPlan.bValid = true;
        OutPlan.ActionType = Profile.ActionType;
        OutPlan.PlannedContactSurface =
            PreferredStandingSurface == ESoccerAerialContactSurface::Head
            ? ESoccerAerialContactSurface::Head
            : ESoccerAerialContactSurface::Chest;
        OutPlan.Ball = const_cast<ASoccerBall*>(SoccerBall);
        OutPlan.ContactLocation = Sample.Location;
        OutPlan.PreparationLocation = PreparationLocation;
        OutPlan.FacingDirection = FacingDirection;
        OutPlan.BallArrivalTime = Sample.TimeFromNow;
        OutPlan.PlayerArrivalTime = PlayerArrivalTime;
        OutPlan.MontageStartDelay = MontageStartDelay;
        OutPlan.ExpectedContactTimeFromMontageStart =
            ExpectedContactTimeFromStart;
        OutPlan.PreparationArrivalMargin = ArrivalMargin;
        OutPlan.ContactTimingOffsetFromIdeal =
            ExpectedContactTimeFromStart - Profile.IdealContactTime;
        OutPlan.BallHeightAboveGround = BallHeightAboveGround;
        OutPlan.PredictedIncomingBallVelocity = Sample.Velocity;
        OutPlan.TrajectoryRevision = SoccerBall->GetTrajectoryRevision();

        return true;
    }

    return false;
}


bool ASoccerCharacterBase::BuildJumpHeaderPlanForProfile(
    const ASoccerBall* SoccerBall,
    const TArray<FSoccerBallTrajectorySample>& Trajectory,
    const FSoccerAerialActionProfile& Profile,
    FSoccerAerialInterceptionPlan& OutPlan
) const
{
    OutPlan = FSoccerAerialInterceptionPlan();

    if (
        !IsValid(SoccerBall) ||
        !IsJumpAerialAction(Profile.ActionType) ||
        GetAerialContactTrackCurveTable(Profile.ActionType) == nullptr ||
        Trajectory.Num() < 2 ||
        Profile.ContactWindowEnd <= Profile.ContactWindowStart ||
        Profile.IdealContactTime < Profile.ContactWindowStart ||
        Profile.IdealContactTime > Profile.ContactWindowEnd
    )
    {
        return false;
    }

    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    const USkeletalMeshComponent* CharacterMesh = GetMesh();

    if (CharacterMesh == nullptr)
    {
        return false;
    }

    const FVector CurrentActorLocation = GetActorLocation();
    const float CharacterGroundZ =
        CurrentActorLocation.Z -
        (
            Capsule != nullptr
            ? Capsule->GetScaledCapsuleHalfHeight()
            : 0.0f
        );
    const float BallRadius = FMath::Max(
        1.0f,
        SoccerBall->GetBallRadiusCm()
    );
    const float MaximumVerticalError =
        BallRadius +
        FMath::Max(1.0f, AerialHeadContactRadius) +
        FMath::Max(0.0f, AerialContactExtraTolerance);
    const float SearchInterval = FMath::Clamp(
        AerialJumpHeaderContactTimeSearchInterval,
        0.005f,
        0.10f
    );

    TArray<float> ContactTimes;

    const auto AddContactTime =
        [&ContactTimes, &Profile](float Time)
        {
            const float ClampedTime = FMath::Clamp(
                Time,
                Profile.ContactWindowStart,
                Profile.ContactWindowEnd
            );

            for (const float ExistingTime : ContactTimes)
            {
                if (FMath::IsNearlyEqual(ExistingTime, ClampedTime, 0.0005f))
                {
                    return;
                }
            }

            ContactTimes.Add(ClampedTime);
        };

    AddContactTime(Profile.ContactWindowStart);
    AddContactTime(Profile.IdealContactTime);
    AddContactTime(Profile.ContactWindowEnd);

    for (
        float ContactTime = Profile.ContactWindowStart;
        ContactTime <= Profile.ContactWindowEnd + KINDA_SMALL_NUMBER;
        ContactTime += SearchInterval
    )
    {
        AddContactTime(ContactTime);
    }

    ContactTimes.Sort();

    bool bFoundPlan = false;
    float BestScore = TNumericLimits<float>::Max();

    for (const float RequestedContactTime : ContactTimes)
    {
        FVector TrackChestLowerLocal;
        FVector TrackChestUpperLocal;
        FVector TrackHeadLocal;

        if (!EvaluateAerialContactTrack(
            Profile.ActionType,
            RequestedContactTime,
            TrackChestLowerLocal,
            TrackChestUpperLocal,
            TrackHeadLocal
        ))
        {
            continue;
        }

        /* Yaw rotation does not change Z, so this is the exact target height. */
        const float RequestedHeadWorldZ =
            CurrentActorLocation.Z +
            CharacterMesh->GetRelativeLocation().Z +
            TrackHeadLocal.Z;

        for (int32 SampleIndex = 1; SampleIndex < Trajectory.Num(); ++SampleIndex)
        {
            const FSoccerBallTrajectorySample& PreviousSample =
                Trajectory[SampleIndex - 1];
            const FSoccerBallTrajectorySample& CurrentSample =
                Trajectory[SampleIndex];

            if (
                CurrentSample.TimeFromNow <= KINDA_SMALL_NUMBER ||
                CurrentSample.TimeFromNow <= PreviousSample.TimeFromNow ||
                PreviousSample.bBallStopped ||
                CurrentSample.bBallStopped ||
                PreviousSample.bNearGround ||
                CurrentSample.bNearGround ||
                PreviousSample.bTrajectoryTerminated ||
                CurrentSample.bAfterBounce ||
                CurrentSample.bHitObstacle
            )
            {
                continue;
            }

            const float PreviousError =
                PreviousSample.Location.Z - RequestedHeadWorldZ;
            const float CurrentError =
                CurrentSample.Location.Z - RequestedHeadWorldZ;
            const float ErrorDelta = CurrentError - PreviousError;

            TArray<float, TInlineAllocator<5>> SegmentAlphas;

            const auto AddSegmentAlpha =
                [&SegmentAlphas](float Alpha)
                {
                    if (Alpha < -KINDA_SMALL_NUMBER || Alpha > 1.0f + KINDA_SMALL_NUMBER)
                    {
                        return;
                    }

                    const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

                    for (const float ExistingAlpha : SegmentAlphas)
                    {
                        if (FMath::IsNearlyEqual(ExistingAlpha, ClampedAlpha, 0.0005f))
                        {
                            return;
                        }
                    }

                    SegmentAlphas.Add(ClampedAlpha);
                };

            AddSegmentAlpha(0.0f);
            AddSegmentAlpha(1.0f);

            if (FMath::Abs(ErrorDelta) > KINDA_SMALL_NUMBER)
            {
                AddSegmentAlpha(-PreviousError / ErrorDelta);
                AddSegmentAlpha(
                    (-MaximumVerticalError - PreviousError) /
                    ErrorDelta
                );
                AddSegmentAlpha(
                    (MaximumVerticalError - PreviousError) /
                    ErrorDelta
                );
            }

            SegmentAlphas.Sort();

            for (const float SegmentAlpha : SegmentAlphas)
            {
                const float RequestedVerticalError = FMath::Abs(
                    FMath::Lerp(
                        PreviousError,
                        CurrentError,
                        SegmentAlpha
                    )
                );

                if (RequestedVerticalError > MaximumVerticalError)
                {
                    continue;
                }

                const float CandidateBallTime = FMath::Lerp(
                    PreviousSample.TimeFromNow,
                    CurrentSample.TimeFromNow,
                    SegmentAlpha
                );

                if (CandidateBallTime <= KINDA_SMALL_NUMBER)
                {
                    continue;
                }

                float ActualContactTime = RequestedContactTime;
                float MontageStartDelay =
                    CandidateBallTime - ActualContactTime;

                if (MontageStartDelay < 0.0f)
                {
                    if (
                        -MontageStartDelay > Profile.MaximumLateStartTime ||
                        CandidateBallTime < Profile.ContactWindowStart ||
                        CandidateBallTime > Profile.ContactWindowEnd
                    )
                    {
                        continue;
                    }

                    /* Immediate late start: montage position equals elapsed world time. */
                    ActualContactTime = CandidateBallTime;
                    MontageStartDelay = 0.0f;
                }

                FVector CandidateLocation = FMath::Lerp(
                    PreviousSample.Location,
                    CurrentSample.Location,
                    SegmentAlpha
                );
                const FVector CandidateVelocity = FMath::Lerp(
                    PreviousSample.Velocity,
                    CurrentSample.Velocity,
                    SegmentAlpha
                );
                const float BallHeightAboveGround =
                    CandidateLocation.Z - CharacterGroundZ;

                if (
                    BallHeightAboveGround < Profile.MinimumBallHeight ||
                    BallHeightAboveGround > Profile.MaximumBallHeight
                )
                {
                    continue;
                }

                FVector IncomingHorizontalVelocity = CandidateVelocity;
                IncomingHorizontalVelocity.Z = 0.0f;

                FVector FacingDirection =
                    -IncomingHorizontalVelocity.GetSafeNormal();

                if (FacingDirection.IsNearlyZero())
                {
                    FacingDirection =
                        (SoccerBall->GetActorLocation() - CurrentActorLocation)
                        .GetSafeNormal2D();
                }

                if (FacingDirection.IsNearlyZero())
                {
                    FacingDirection = GetActorForwardVector().GetSafeNormal2D();
                }

                if (FacingDirection.IsNearlyZero())
                {
                    FacingDirection = FVector::ForwardVector;
                }

                FVector PredictedChestLower;
                FVector PredictedChestUpper;
                FVector PredictedHead;

                if (!GetAerialContactTrackWorldLocations(
                    Profile.ActionType,
                    ActualContactTime,
                    FacingDirection,
                    CurrentActorLocation,
                    PredictedChestLower,
                    PredictedChestUpper,
                    PredictedHead
                ))
                {
                    continue;
                }

                const FVector HeadOffsetFromActor =
                    PredictedHead - CurrentActorLocation;
                FVector PreparationLocation =
                    CandidateLocation - HeadOffsetFromActor;
                PreparationLocation.Z = CurrentActorLocation.Z;

                const FVector ReconstructedHeadLocation =
                    PreparationLocation + HeadOffsetFromActor;
                const float VerticalError = FMath::Abs(
                    CandidateLocation.Z - ReconstructedHeadLocation.Z
                );

                if (VerticalError > MaximumVerticalError)
                {
                    continue;
                }

                const float PlayerArrivalTime =
                    EstimateArrivalTimeToLocation(
                        PreparationLocation,
                        Profile.PreparationReachRadius
                    );

                if (!FMath::IsFinite(PlayerArrivalTime))
                {
                    continue;
                }

                const float LatestUsefulArrivalTime = FMath::Max(
                    0.0f,
                    MontageStartDelay - Profile.PreparationSettleTime
                );
                const float ArrivalMargin =
                    LatestUsefulArrivalTime - PlayerArrivalTime;

                if (
                    PlayerArrivalTime >
                    LatestUsefulArrivalTime +
                    FMath::Max(0.0f, AerialJumpHeaderArrivalTolerance)
                )
                {
                    continue;
                }

                const float TimingOffset =
                    ActualContactTime - Profile.IdealContactTime;
                const float NormalizedVerticalError =
                    MaximumVerticalError > KINDA_SMALL_NUMBER
                    ? FMath::Clamp(
                        VerticalError / MaximumVerticalError,
                        0.0f,
                        1.0f
                    )
                    : 0.0f;
                const float UsefulArrivalMargin = FMath::Clamp(
                    ArrivalMargin,
                    0.0f,
                    0.30f
                );

                const float CandidateScore =
                    CandidateBallTime +
                    FMath::Abs(TimingOffset) *
                        FMath::Max(
                            0.0f,
                            AerialJumpHeaderTimingDeviationScoreWeight
                        ) +
                    NormalizedVerticalError *
                        FMath::Max(
                            0.0f,
                            AerialJumpHeaderVerticalErrorScoreWeight
                        ) -
                    UsefulArrivalMargin *
                        FMath::Max(
                            0.0f,
                            AerialJumpHeaderArrivalMarginScoreBonus
                        );

                if (bFoundPlan && CandidateScore >= BestScore)
                {
                    continue;
                }

                bFoundPlan = true;
                BestScore = CandidateScore;

                OutPlan.bValid = true;
                OutPlan.ActionType = Profile.ActionType;
                OutPlan.PlannedContactSurface =
                    ESoccerAerialContactSurface::Head;
                OutPlan.Ball = const_cast<ASoccerBall*>(SoccerBall);
                OutPlan.ContactLocation = CandidateLocation;
                OutPlan.PreparationLocation = PreparationLocation;
                OutPlan.FacingDirection = FacingDirection;
                OutPlan.BallArrivalTime = CandidateBallTime;
                OutPlan.PlayerArrivalTime = PlayerArrivalTime;
                OutPlan.MontageStartDelay = MontageStartDelay;
                OutPlan.ExpectedContactTimeFromMontageStart =
                    ActualContactTime;
                OutPlan.PreparationArrivalMargin = ArrivalMargin;
                OutPlan.ContactTimingOffsetFromIdeal = TimingOffset;
                OutPlan.PredictedHeadVerticalError = VerticalError;
                OutPlan.BallHeightAboveGround = BallHeightAboveGround;
                OutPlan.PredictedIncomingBallVelocity = CandidateVelocity;
                OutPlan.TrajectoryRevision =
                    SoccerBall->GetTrajectoryRevision();
            }
        }
    }

    return bFoundPlan;
}


bool ASoccerCharacterBase::BuildStandingAerialControlPlanForSurface(
    const ASoccerBall* SoccerBall,
    const TArray<FSoccerBallTrajectorySample>& Trajectory,
    const FSoccerAerialActionProfile& Profile,
    ESoccerAerialContactSurface PlannedSurface,
    FSoccerAerialInterceptionPlan& OutPlan
) const
{
    OutPlan = FSoccerAerialInterceptionPlan();

    if (
        !IsValid(SoccerBall) ||
        StandingAerialControlContactTrackCurveTable == nullptr ||
        Trajectory.Num() < 2 ||
        (
            PlannedSurface != ESoccerAerialContactSurface::Chest &&
            PlannedSurface != ESoccerAerialContactSurface::Head
        )
    )
    {
        return false;
    }

    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    const float CharacterGroundZ =
        GetActorLocation().Z -
        (
            Capsule != nullptr
            ? Capsule->GetScaledCapsuleHalfHeight()
            : 0.0f
        );
    const FVector CurrentActorLocation = GetActorLocation();

    for (int32 SampleIndex = 1; SampleIndex < Trajectory.Num(); ++SampleIndex)
    {
        const FSoccerBallTrajectorySample& PreviousSample =
            Trajectory[SampleIndex - 1];
        const FSoccerBallTrajectorySample& CurrentSample =
            Trajectory[SampleIndex];

        if (
            CurrentSample.TimeFromNow <= KINDA_SMALL_NUMBER ||
            PreviousSample.bBallStopped ||
            CurrentSample.bBallStopped ||
            PreviousSample.bNearGround ||
            CurrentSample.bNearGround
        )
        {
            continue;
        }

        FVector ProvisionalVelocity = FMath::Lerp(
            PreviousSample.Velocity,
            CurrentSample.Velocity,
            0.50f
        );
        ProvisionalVelocity.Z = 0.0f;

        FVector FacingDirection =
            -ProvisionalVelocity.GetSafeNormal();

        if (FacingDirection.IsNearlyZero())
        {
            FacingDirection =
                (SoccerBall->GetActorLocation() - CurrentActorLocation)
                .GetSafeNormal2D();
        }

        if (FacingDirection.IsNearlyZero())
        {
            FacingDirection = GetActorForwardVector().GetSafeNormal2D();
        }

        FVector PredictedChestLower;
        FVector PredictedChestUpper;
        FVector PredictedHead;

        if (!GetStandingAerialControlTrackWorldLocations(
            Profile.IdealContactTime,
            FacingDirection,
            CurrentActorLocation,
            PredictedChestLower,
            PredictedChestUpper,
            PredictedHead
        ))
        {
            continue;
        }

        const float SafeChestAlpha = FMath::Clamp(
            StandingAerialChestTrackAlpha,
            0.0f,
            1.0f
        );

        FVector PredictedStandingContact =
            PlannedSurface == ESoccerAerialContactSurface::Head
            ? PredictedHead
            : FMath::Lerp(
                PredictedChestLower,
                PredictedChestUpper,
                SafeChestAlpha
            );

        const float PreviousHeightError =
            PreviousSample.Location.Z - PredictedStandingContact.Z;
        const float CurrentHeightError =
            CurrentSample.Location.Z - PredictedStandingContact.Z;

        const bool bCrossesTargetHeight =
            (PreviousHeightError <= 0.0f && CurrentHeightError >= 0.0f) ||
            (PreviousHeightError >= 0.0f && CurrentHeightError <= 0.0f);

        if (!bCrossesTargetHeight)
        {
            continue;
        }

        const float HeightSpan =
            CurrentSample.Location.Z - PreviousSample.Location.Z;

        float SegmentAlpha = 0.0f;

        if (FMath::Abs(HeightSpan) > KINDA_SMALL_NUMBER)
        {
            SegmentAlpha = FMath::Clamp(
                (PredictedStandingContact.Z - PreviousSample.Location.Z) /
                    HeightSpan,
                0.0f,
                1.0f
            );
        }

        const float CandidateTime = FMath::Lerp(
            PreviousSample.TimeFromNow,
            CurrentSample.TimeFromNow,
            SegmentAlpha
        );

        FVector CandidateLocation = FMath::Lerp(
            PreviousSample.Location,
            CurrentSample.Location,
            SegmentAlpha
        );
        CandidateLocation.Z = PredictedStandingContact.Z;

        const FVector CandidateVelocity = FMath::Lerp(
            PreviousSample.Velocity,
            CurrentSample.Velocity,
            SegmentAlpha
        );

        const float BallHeightAboveGround =
            CandidateLocation.Z - CharacterGroundZ;

        if (
            BallHeightAboveGround < Profile.MinimumBallHeight ||
            BallHeightAboveGround > Profile.MaximumBallHeight
        )
        {
            continue;
        }

        FVector IncomingHorizontalVelocity = CandidateVelocity;
        IncomingHorizontalVelocity.Z = 0.0f;
        FacingDirection = -IncomingHorizontalVelocity.GetSafeNormal();

        if (FacingDirection.IsNearlyZero())
        {
            FacingDirection =
                (SoccerBall->GetActorLocation() - CurrentActorLocation)
                .GetSafeNormal2D();
        }

        if (FacingDirection.IsNearlyZero())
        {
            FacingDirection = GetActorForwardVector().GetSafeNormal2D();
        }

        /* Re-evaluate Forward/Lateral offsets using the final facing. */
        if (!GetStandingAerialControlTrackWorldLocations(
            Profile.IdealContactTime,
            FacingDirection,
            CurrentActorLocation,
            PredictedChestLower,
            PredictedChestUpper,
            PredictedHead
        ))
        {
            continue;
        }

        PredictedStandingContact =
            PlannedSurface == ESoccerAerialContactSurface::Head
            ? PredictedHead
            : FMath::Lerp(
                PredictedChestLower,
                PredictedChestUpper,
                SafeChestAlpha
            );

        const FVector ContactOffsetFromActor =
            PredictedStandingContact - CurrentActorLocation;

        FVector PreparationLocation =
            CandidateLocation - ContactOffsetFromActor;
        PreparationLocation.Z = CurrentActorLocation.Z;

        const float PlayerArrivalTime =
            EstimateArrivalTimeToLocation(
                PreparationLocation,
                Profile.PreparationReachRadius
            );

        if (!FMath::IsFinite(PlayerArrivalTime))
        {
            continue;
        }

        const float RawMontageStartDelay =
            CandidateTime - Profile.IdealContactTime;

        if (RawMontageStartDelay < -Profile.MaximumLateStartTime)
        {
            continue;
        }

        const float MontageStartDelay =
            FMath::Max(0.0f, RawMontageStartDelay);

        const float ExpectedContactTimeFromStart =
            CandidateTime - MontageStartDelay;

        if (
            ExpectedContactTimeFromStart <
                Profile.ContactWindowStart - KINDA_SMALL_NUMBER ||
            ExpectedContactTimeFromStart >
                Profile.ContactWindowEnd + KINDA_SMALL_NUMBER
        )
        {
            continue;
        }

        const float LatestUsefulArrivalTime = FMath::Max(
            0.0f,
            MontageStartDelay - Profile.PreparationSettleTime
        );

        if (PlayerArrivalTime > LatestUsefulArrivalTime + 0.04f)
        {
            continue;
        }

        OutPlan.bValid = true;
        OutPlan.ActionType = Profile.ActionType;
        OutPlan.PlannedContactSurface = PlannedSurface;
        OutPlan.Ball = const_cast<ASoccerBall*>(SoccerBall);
        OutPlan.ContactLocation = CandidateLocation;
        OutPlan.PreparationLocation = PreparationLocation;
        OutPlan.FacingDirection = FacingDirection;
        OutPlan.BallArrivalTime = CandidateTime;
        OutPlan.PlayerArrivalTime = PlayerArrivalTime;
        OutPlan.MontageStartDelay = MontageStartDelay;
        OutPlan.ExpectedContactTimeFromMontageStart =
            ExpectedContactTimeFromStart;
        OutPlan.BallHeightAboveGround = BallHeightAboveGround;
        OutPlan.PredictedIncomingBallVelocity = CandidateVelocity;
        OutPlan.TrajectoryRevision = SoccerBall->GetTrajectoryRevision();

        return true;
    }

    return false;
}

bool ASoccerCharacterBase::BuildBestStandingAerialControlPlan(
    const ASoccerBall* SoccerBall,
    const TArray<FSoccerBallTrajectorySample>& Trajectory,
    ESoccerAerialActionIntent Intent,
    ESoccerAerialContactSurface PreferredStandingSurface,
    FSoccerAerialInterceptionPlan& OutPlan
) const
{
    OutPlan = FSoccerAerialInterceptionPlan();

    if (!IsValid(SoccerBall))
    {
        return false;
    }

    const auto BuildSurfacePlan =
        [&](ESoccerAerialContactSurface Surface,
            FSoccerAerialInterceptionPlan& Plan)
        {
            return BuildStandingAerialControlPlanForSurface(
                SoccerBall,
                Trajectory,
                StandingAerialControlProfile,
                Surface,
                Plan
            );
        };

    if (
        DebugStandingControlPlanningSurface ==
            ESoccerAerialContactSurface::Chest ||
        DebugStandingControlPlanningSurface ==
            ESoccerAerialContactSurface::Head
    )
    {
        return BuildSurfacePlan(
            DebugStandingControlPlanningSurface,
            OutPlan
        );
    }

    FSoccerAerialInterceptionPlan ChestPlan;
    const bool bHasChestPlan = BuildSurfacePlan(
        ESoccerAerialContactSurface::Chest,
        ChestPlan
    );

    FSoccerAerialInterceptionPlan HeadPlan;
    const bool bHasHeadPlan = BuildSurfacePlan(
        ESoccerAerialContactSurface::Head,
        HeadPlan
    );

    if (Intent == ESoccerAerialActionIntent::StandingHeaderRedirect)
    {
        FVector RequestedTarget;

        if (
            !bHasHeadPlan ||
            !ResolveAerialStandingHeaderRedirectTarget(RequestedTarget) ||
            !ConfigureRequestedStandingHeaderRedirect(
                SoccerBall,
                RequestedTarget,
                HeadPlan
            )
        )
        {
            return false;
        }

        OutPlan = HeadPlan;
        return true;
    }

    const auto PrepareChestPlan =
        [&](FSoccerAerialInterceptionPlan& Plan)
        {
            const FVector ChestVelocity =
                CalculatePredictedChestControlVelocity(
                    Plan.PredictedIncomingBallVelocity,
                    Plan.FacingDirection
                );

            EvaluateStandingControlRecovery(
                SoccerBall,
                ChestVelocity,
                Plan
            );

            const bool bOpponentAlreadyClose =
                Plan.NearestOpponentDistanceAtContact >= 0.0f &&
                Plan.NearestOpponentDistanceAtContact <
                    FMath::Max(
                        0.0f,
                        AerialChestOpponentDangerDistance
                    );

            Plan.bPredictedStandingControlSafe =
                Plan.bPredictedStandingControlSafe &&
                !bOpponentAlreadyClose &&
                Plan.PredictedRecoveryAdvantage >=
                    AerialChestMinimumRecoveryAdvantage;
        };

    if (!bUseTacticalStandingControlSelection)
    {
        if (
            PreferredStandingSurface ==
                ESoccerAerialContactSurface::Head &&
            bHasHeadPlan
        )
        {
            ConfigureAutomaticStandingHeaderRedirect(
                SoccerBall,
                HeadPlan
            );
            OutPlan = HeadPlan;
            return true;
        }

        if (
            PreferredStandingSurface ==
                ESoccerAerialContactSurface::Chest &&
            bHasChestPlan
        )
        {
            PrepareChestPlan(ChestPlan);
            OutPlan = ChestPlan;
            return true;
        }

        if (bPreferChestForStandingAerialControl && bHasChestPlan)
        {
            PrepareChestPlan(ChestPlan);
            OutPlan = ChestPlan;
            return true;
        }

        if (bHasHeadPlan)
        {
            ConfigureAutomaticStandingHeaderRedirect(
                SoccerBall,
                HeadPlan
            );
            OutPlan = HeadPlan;
            return true;
        }

        if (bHasChestPlan)
        {
            PrepareChestPlan(ChestPlan);
            OutPlan = ChestPlan;
            return true;
        }

        return false;
    }

    /* A queued HEAD plan remains HEAD so refreshes do not oscillate. */
    if (
        PreferredStandingSurface ==
            ESoccerAerialContactSurface::Head &&
        bHasHeadPlan
    )
    {
        ConfigureAutomaticStandingHeaderRedirect(
            SoccerBall,
            HeadPlan
        );
        OutPlan = HeadPlan;
        return true;
    }

    if (bHasChestPlan)
    {
        PrepareChestPlan(ChestPlan);

        if (ChestPlan.bPredictedStandingControlSafe)
        {
            OutPlan = ChestPlan;
            return true;
        }
    }

    if (bHasHeadPlan)
    {
        ConfigureAutomaticStandingHeaderRedirect(
            SoccerBall,
            HeadPlan
        );

        /*
         * Under pressure, even an imperfect passive deflection is preferable
         * to cushioning the ball at the feet of the nearest opponent.
         */
        OutPlan = HeadPlan;
        return true;
    }

    if (bHasChestPlan)
    {
        OutPlan = ChestPlan;
        return true;
    }

    return false;
}

bool ASoccerCharacterBase::ConfigureAutomaticStandingHeaderRedirect(
    const ASoccerBall* SoccerBall,
    FSoccerAerialInterceptionPlan& InOutPlan
) const
{
    if (
        !IsValid(SoccerBall) ||
        !InOutPlan.bValid ||
        InOutPlan.PlannedContactSurface !=
            ESoccerAerialContactSurface::Head
    )
    {
        return false;
    }

    FVector FacingDirection = InOutPlan.FacingDirection;
    FacingDirection.Z = 0.0f;
    FacingDirection = FacingDirection.GetSafeNormal();

    if (FacingDirection.IsNearlyZero())
    {
        FacingDirection = GetActorForwardVector().GetSafeNormal2D();
    }

    if (FacingDirection.IsNearlyZero())
    {
        FacingDirection = FVector::ForwardVector;
    }

    FVector AttackDirection = FVector::ZeroVector;
    FVector AwayFromNearestOpponent = FVector::ZeroVector;
    float NearestOpponentDistance = TNumericLimits<float>::Max();
    UWorld* World = GetWorld();

    if (World != nullptr)
    {
        for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
        {
            ASoccerMatchManager* MatchManager = *It;

            const ASoccerAICharacter* AICharacter =
                Cast<ASoccerAICharacter>(
                    const_cast<ASoccerCharacterBase*>(this)
                );

            if (IsValid(MatchManager) && AICharacter != nullptr)
            {
                AttackDirection =
                    MatchManager->GetShotTargetLocation(AICharacter) -
                    InOutPlan.ContactLocation;
                AttackDirection.Z = 0.0f;
                AttackDirection = AttackDirection.GetSafeNormal();
                break;
            }
        }

        for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
        {
            ASoccerCharacterBase* Other = *It;

            if (
                !IsValid(Other) ||
                Other == this ||
                Other->GetTeam() == GetTeam()
            )
            {
                continue;
            }

            const float Distance = FVector::Dist2D(
                Other->GetActorLocation(),
                InOutPlan.ContactLocation
            );

            if (Distance < NearestOpponentDistance)
            {
                NearestOpponentDistance = Distance;
                AwayFromNearestOpponent =
                    InOutPlan.ContactLocation -
                    Other->GetActorLocation();
                AwayFromNearestOpponent.Z = 0.0f;
                AwayFromNearestOpponent =
                    AwayFromNearestOpponent.GetSafeNormal();
            }
        }
    }

    const float MaximumAngle = FMath::Clamp(
        AerialStandingHeaderMaximumRedirectAngle,
        0.0f,
        90.0f
    );
    const float AngleStep = FMath::Clamp(
        AerialStandingHeaderCandidateAngleStep,
        5.0f,
        45.0f
    );
    const float TargetDistance = FMath::Max(
        100.0f,
        AerialStandingHeaderTargetDistance
    );

    bool bFoundCandidate = false;
    float BestScore = -TNumericLimits<float>::Max();
    FSoccerAerialInterceptionPlan BestPlan;

    for (
        float Angle = -MaximumAngle;
        Angle <= MaximumAngle + KINDA_SMALL_NUMBER;
        Angle += AngleStep
    )
    {
        const FVector CandidateDirection =
            FacingDirection.RotateAngleAxis(
                Angle,
                FVector::UpVector
            ).GetSafeNormal2D();

        if (CandidateDirection.IsNearlyZero())
        {
            continue;
        }

        FSoccerAerialInterceptionPlan CandidatePlan = InOutPlan;
        CandidatePlan.bHasStandingHeaderRedirectTarget = true;
        CandidatePlan.StandingHeaderRedirectTarget =
            CandidatePlan.ContactLocation +
            CandidateDirection * TargetDistance;

        const FVector CandidateVelocity =
            CalculatePassiveStandingHeaderVelocity(
                CandidatePlan.PredictedIncomingBallVelocity,
                CandidatePlan.ContactLocation,
                CandidatePlan.FacingDirection,
                CandidatePlan.StandingHeaderRedirectTarget,
                0.90f,
                0.0f
            );

        if (CandidateVelocity.IsNearlyZero())
        {
            continue;
        }

        EvaluateStandingControlRecovery(
            SoccerBall,
            CandidateVelocity,
            CandidatePlan
        );

        if (!CandidatePlan.bPredictedStandingControlSafe)
        {
            continue;
        }

        const float AttackProgress =
            AttackDirection.IsNearlyZero()
            ? 0.0f
            : FVector::DotProduct(
                CandidatePlan.PredictedRecoveryLocation -
                    CandidatePlan.ContactLocation,
                AttackDirection
            ) / 1000.0f;

        const float OpponentAvoidance =
            AwayFromNearestOpponent.IsNearlyZero()
            ? 0.0f
            : FVector::DotProduct(
                CandidateDirection,
                AwayFromNearestOpponent
            );

        const float SelfTravelPenalty = FMath::Max(
            0.0f,
            CandidatePlan.PredictedSelfRecoveryTime -
                CandidatePlan.BallArrivalTime
        );

        const float CandidateScore =
            CandidatePlan.PredictedRecoveryAdvantage * 5.0f +
            OpponentAvoidance * 0.35f +
            AttackProgress * 0.18f -
            SelfTravelPenalty * 0.08f -
            (FMath::Abs(Angle) / 90.0f) * 0.03f;

        if (!bFoundCandidate || CandidateScore > BestScore)
        {
            bFoundCandidate = true;
            BestScore = CandidateScore;
            BestPlan = CandidatePlan;
        }
    }

    if (!bFoundCandidate)
    {
        const FVector FallbackTarget =
            InOutPlan.ContactLocation +
            FacingDirection * TargetDistance;

        return ConfigureRequestedStandingHeaderRedirect(
            SoccerBall,
            FallbackTarget,
            InOutPlan
        );
    }

    BestPlan.bPredictedStandingControlSafe =
        BestPlan.bPredictedStandingControlSafe &&
        BestPlan.PredictedRecoveryAdvantage >=
            AerialStandingHeaderMinimumRecoveryAdvantage;

    InOutPlan = BestPlan;
    return true;
}

bool ASoccerCharacterBase::ConfigureRequestedStandingHeaderRedirect(
    const ASoccerBall* SoccerBall,
    const FVector& RequestedTarget,
    FSoccerAerialInterceptionPlan& InOutPlan
) const
{
    if (
        !IsValid(SoccerBall) ||
        !InOutPlan.bValid ||
        InOutPlan.PlannedContactSurface !=
            ESoccerAerialContactSurface::Head
    )
    {
        return false;
    }

    FVector FacingDirection = InOutPlan.FacingDirection;
    FacingDirection.Z = 0.0f;
    FacingDirection = FacingDirection.GetSafeNormal();

    FVector RequestedDirection =
        RequestedTarget - InOutPlan.ContactLocation;
    RequestedDirection.Z = 0.0f;
    RequestedDirection = RequestedDirection.GetSafeNormal();

    if (
        FacingDirection.IsNearlyZero() ||
        RequestedDirection.IsNearlyZero()
    )
    {
        return false;
    }

    const float DeltaYaw = FMath::Abs(
        FMath::FindDeltaAngleDegrees(
            FacingDirection.Rotation().Yaw,
            RequestedDirection.Rotation().Yaw
        )
    );

    if (
        DeltaYaw >
        FMath::Clamp(
            AerialStandingHeaderMaximumRedirectAngle,
            0.0f,
            90.0f
        ) + 1.0f
    )
    {
        return false;
    }

    InOutPlan.bHasStandingHeaderRedirectTarget = true;
    InOutPlan.StandingHeaderRedirectTarget = RequestedTarget;

    const FVector PredictedVelocity =
        CalculatePassiveStandingHeaderVelocity(
            InOutPlan.PredictedIncomingBallVelocity,
            InOutPlan.ContactLocation,
            InOutPlan.FacingDirection,
            RequestedTarget,
            0.90f,
            0.0f
        );

    if (PredictedVelocity.IsNearlyZero())
    {
        return false;
    }

    EvaluateStandingControlRecovery(
        SoccerBall,
        PredictedVelocity,
        InOutPlan
    );

    return InOutPlan.bPredictedStandingControlSafe;
}

void ASoccerCharacterBase::EvaluateStandingControlRecovery(
    const ASoccerBall* SoccerBall,
    const FVector& PredictedOutgoingVelocity,
    FSoccerAerialInterceptionPlan& InOutPlan
) const
{
    float BallTravelTime = 0.0f;
    InOutPlan.PredictedRecoveryLocation =
        PredictStandingControlRecoveryLocation(
            SoccerBall,
            InOutPlan.ContactLocation,
            PredictedOutgoingVelocity,
            BallTravelTime
        );

    const float BallAvailableTime =
        InOutPlan.BallArrivalTime + BallTravelTime;

    InOutPlan.PredictedSelfRecoveryTime =
        EstimateStandingControlSelfRecoveryTime(
            InOutPlan,
            InOutPlan.PredictedRecoveryLocation,
            BallTravelTime
        );

    InOutPlan.PredictedOpponentRecoveryTime =
        FindEarliestOpponentRecoveryTime(
            InOutPlan.PredictedRecoveryLocation,
            InOutPlan.ContactLocation,
            BallAvailableTime,
            InOutPlan.NearestOpponentDistanceAtContact
        );

    InOutPlan.PredictedRecoveryAdvantage =
        InOutPlan.PredictedOpponentRecoveryTime -
        InOutPlan.PredictedSelfRecoveryTime;

    InOutPlan.bPredictedStandingControlSafe =
        IsStandingControlRecoveryLocationInsideField(
            InOutPlan.PredictedRecoveryLocation
        );
}

FVector ASoccerCharacterBase::CalculatePredictedChestControlVelocity(
    const FVector& IncomingVelocity,
    const FVector& FacingDirection
) const
{
    FVector SafeForward = FacingDirection;
    SafeForward.Z = 0.0f;
    SafeForward = SafeForward.GetSafeNormal();

    if (SafeForward.IsNearlyZero())
    {
        SafeForward = GetActorForwardVector().GetSafeNormal2D();
    }

    if (SafeForward.IsNearlyZero())
    {
        SafeForward = FVector::ForwardVector;
    }

    FVector SafeRight = FVector::CrossProduct(
        FVector::UpVector,
        SafeForward
    ).GetSafeNormal();

    const float MinimumSpeed =
        FMath::Max(0.0f, AerialChestMinimumHorizontalSpeed);
    const float MaximumSpeed = FMath::Max(
        MinimumSpeed,
        AerialChestMaximumHorizontalSpeed
    );
    const float HorizontalSpeed = FMath::Clamp(
        IncomingVelocity.Size2D() *
            FMath::Clamp(
                AerialChestHorizontalRetention,
                0.0f,
                1.0f
            ),
        MinimumSpeed,
        MaximumSpeed
    );
    const float IncomingLateralSpeed = FVector::DotProduct(
        IncomingVelocity,
        SafeRight
    );

    FVector Result =
        SafeForward * HorizontalSpeed +
        SafeRight * IncomingLateralSpeed * 0.10f;

    if (Result.Size2D() > MaximumSpeed)
    {
        Result = Result.GetSafeNormal2D() * MaximumSpeed;
    }

    Result.Z = AerialChestVerticalSpeed;
    return Result;
}

FVector ASoccerCharacterBase::CalculatePassiveStandingHeaderVelocity(
    const FVector& IncomingVelocity,
    const FVector& ContactLocation,
    const FVector& FacingDirection,
    const FVector& RedirectTarget,
    float ContactQuality,
    float OpponentPressure
) const
{
    const float IncomingTotalSpeed = IncomingVelocity.Size();
    const float IncomingHorizontalSpeed = IncomingVelocity.Size2D();

    if (IncomingTotalSpeed <= KINDA_SMALL_NUMBER)
    {
        return FVector::ZeroVector;
    }

    FVector SafeForward = FacingDirection;
    SafeForward.Z = 0.0f;
    SafeForward = SafeForward.GetSafeNormal();

    if (SafeForward.IsNearlyZero())
    {
        SafeForward = GetActorForwardVector().GetSafeNormal2D();
    }

    if (SafeForward.IsNearlyZero())
    {
        SafeForward = FVector::ForwardVector;
    }

    FVector DesiredDirection = RedirectTarget - ContactLocation;
    DesiredDirection.Z = 0.0f;
    DesiredDirection = DesiredDirection.GetSafeNormal();

    if (DesiredDirection.IsNearlyZero())
    {
        DesiredDirection = SafeForward;
    }

    const float MaximumAngle = FMath::Clamp(
        AerialStandingHeaderMaximumRedirectAngle,
        0.0f,
        90.0f
    );

    DesiredDirection = SoccerClampHorizontalDirectionToYaw(
        SafeForward,
        DesiredDirection,
        MaximumAngle
    );

    const float RedirectAngle = FMath::Abs(
        FMath::FindDeltaAngleDegrees(
            SafeForward.Rotation().Yaw,
            DesiredDirection.Rotation().Yaw
        )
    );
    const float AngleAlpha =
        MaximumAngle > KINDA_SMALL_NUMBER
        ? FMath::Clamp(
            RedirectAngle / MaximumAngle,
            0.0f,
            1.0f
        )
        : 0.0f;

    const float Retention = FMath::Lerp(
        FMath::Clamp(
            AerialStandingHeaderStraightRetention,
            0.0f,
            1.0f
        ),
        FMath::Clamp(
            AerialStandingHeaderMaximumAngleRetention,
            0.0f,
            1.0f
        ),
        AngleAlpha
    );
    const float Quality = FMath::Clamp(ContactQuality, 0.0f, 1.0f);
    const float Pressure = FMath::Clamp(OpponentPressure, 0.0f, 1.0f);
    const float SpeedScale =
        FMath::Lerp(0.55f, 1.0f, Quality) *
        FMath::Lerp(1.0f, 0.82f, Pressure);

    /* A falling ball may convert some vertical energy into a glancing deflection. */
    const float AvailableHorizontalSpeed = FMath::Max(
        IncomingHorizontalSpeed,
        IncomingTotalSpeed * 0.35f
    );
    const float HorizontalSpeed = FMath::Min(
        IncomingTotalSpeed,
        AvailableHorizontalSpeed * Retention * SpeedScale
    );

    FVector EffectiveDirection = FMath::Lerp(
        SafeForward,
        DesiredDirection,
        FMath::Lerp(0.45f, 1.0f, Quality)
    ).GetSafeNormal();

    if (EffectiveDirection.IsNearlyZero())
    {
        EffectiveDirection = DesiredDirection;
    }

    FVector Result = EffectiveDirection * HorizontalSpeed;

    const float MaximumVerticalMagnitude =
        IncomingTotalSpeed * 0.35f;
    Result.Z = FMath::Clamp(
        AerialStandingHeaderVerticalSpeed *
            FMath::Lerp(0.72f, 1.0f, Quality),
        -MaximumVerticalMagnitude,
        MaximumVerticalMagnitude
    );

    const float ResultSpeed = Result.Size();

    if (
        ResultSpeed > IncomingTotalSpeed &&
        ResultSpeed > KINDA_SMALL_NUMBER
    )
    {
        Result *= IncomingTotalSpeed / ResultSpeed;
    }

    return Result;
}

FVector ASoccerCharacterBase::PredictStandingControlRecoveryLocation(
    const ASoccerBall* SoccerBall,
    const FVector& ContactLocation,
    const FVector& OutgoingVelocity,
    float& OutBallTravelTime
) const
{
    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    const float CharacterGroundZ =
        GetActorLocation().Z -
        (
            Capsule != nullptr
            ? Capsule->GetScaledCapsuleHalfHeight()
            : 0.0f
        );
    const float BallRadius =
        IsValid(SoccerBall)
        ? SoccerBall->GetBallRadiusCm()
        : 11.0f;
    const float GroundBallCenterZ = CharacterGroundZ + BallRadius;
    const float GravityZ =
        GetWorld() != nullptr
        ? GetWorld()->GetGravityZ()
        : -980.0f;

    float LandingTime = 0.55f;
    const float HeightAboveGround =
        ContactLocation.Z - GroundBallCenterZ;

    if (HeightAboveGround <= 0.0f)
    {
        LandingTime = 0.10f;
    }
    else if (FMath::Abs(GravityZ) > KINDA_SMALL_NUMBER)
    {
        const float Discriminant =
            OutgoingVelocity.Z * OutgoingVelocity.Z -
            2.0f * GravityZ * HeightAboveGround;

        if (Discriminant >= 0.0f)
        {
            const float Root = FMath::Sqrt(Discriminant);
            const float RootA =
                (-OutgoingVelocity.Z + Root) / GravityZ;
            const float RootB =
                (-OutgoingVelocity.Z - Root) / GravityZ;

            bool bFoundPositiveRoot = false;
            float EarliestPositiveRoot = 0.55f;

            if (RootA > KINDA_SMALL_NUMBER)
            {
                EarliestPositiveRoot = RootA;
                bFoundPositiveRoot = true;
            }

            if (
                RootB > KINDA_SMALL_NUMBER &&
                (!bFoundPositiveRoot || RootB < EarliestPositiveRoot)
            )
            {
                EarliestPositiveRoot = RootB;
                bFoundPositiveRoot = true;
            }

            LandingTime =
                bFoundPositiveRoot
                ? EarliestPositiveRoot
                : 0.55f;
        }
    }

    LandingTime = FMath::Clamp(
        LandingTime,
        0.10f,
        FMath::Max(
            0.20f,
            AerialStandingControlMaximumRecoveryPredictionTime
        )
    );

    FVector RecoveryLocation =
        ContactLocation + OutgoingVelocity * LandingTime;
    RecoveryLocation.Z = GroundBallCenterZ;

    FVector HorizontalVelocity = OutgoingVelocity;
    HorizontalVelocity.Z = 0.0f;
    RecoveryLocation +=
        HorizontalVelocity *
        FMath::Max(0.0f, AerialStandingControlRecoveryRollTime) *
        0.55f;

    OutBallTravelTime =
        LandingTime +
        FMath::Max(0.0f, AerialStandingControlRecoveryRollTime);

    return RecoveryLocation;
}

float ASoccerCharacterBase::EstimateStandingControlSelfRecoveryTime(
    const FSoccerAerialInterceptionPlan& Plan,
    const FVector& RecoveryLocation,
    float BallTravelTime
) const
{
    const FSoccerAerialActionProfile* Profile =
        GetAerialProfile(Plan.ActionType);
    const float RemainingMontageTime =
        Profile != nullptr
        ? FMath::Max(
            0.0f,
            Profile->ExpectedMontageDuration -
                Plan.ExpectedContactTimeFromMontageStart
        )
        : 0.65f;

    /*
     * A planned standing HEAD redirects the ball and exits the full-body
     * montage shortly after contact. Use the same delay here that execution
     * uses, otherwise the tactical second-ball simulation and the real
     * character would disagree about when running becomes available.
     */
    const float PostContactRecoveryDelay =
        Plan.ActionType == ESoccerAerialActionType::StandingControl &&
        Plan.PlannedContactSurface == ESoccerAerialContactSurface::Head
        ? FMath::Max(
            0.0f,
            AerialStandingHeaderPostContactReleaseDelay
        )
        : RemainingMontageTime;

    const UCharacterMovementComponent* Movement =
        GetCharacterMovement();
    const float MaximumSpeed =
        Movement != nullptr
        ? FMath::Max(1.0f, Movement->GetMaxSpeed())
        : 600.0f;
    const float TravelDistance = FMath::Max(
        0.0f,
        FVector::Dist2D(
            Plan.PreparationLocation,
            RecoveryLocation
        ) -
        FMath::Max(
            0.0f,
            AerialStandingControlRecoveryReachRadius
        )
    );
    const float MovementTime = TravelDistance / MaximumSpeed;

    FVector ToRecovery =
        RecoveryLocation - Plan.PreparationLocation;
    ToRecovery.Z = 0.0f;
    ToRecovery = ToRecovery.GetSafeNormal();

    float TurnTime = 0.0f;

    if (!ToRecovery.IsNearlyZero())
    {
        const float TurnAngle = FMath::Abs(
            FMath::FindDeltaAngleDegrees(
                Plan.FacingDirection.Rotation().Yaw,
                ToRecovery.Rotation().Yaw
            )
        );
        TurnTime = TurnAngle /
            FMath::Max(
                1.0f,
                AerialFacingRotationSpeedDegreesPerSecond
            );
    }

    const float BallAvailableTime =
        Plan.BallArrivalTime + FMath::Max(0.0f, BallTravelTime);
    const float PlayerAvailableTime =
        Plan.BallArrivalTime +
        PostContactRecoveryDelay +
        MovementTime +
        TurnTime * 0.45f;

    return FMath::Max(BallAvailableTime, PlayerAvailableTime);
}

float ASoccerCharacterBase::FindEarliestOpponentRecoveryTime(
    const FVector& RecoveryLocation,
    const FVector& ContactLocation,
    float BallAvailableTime,
    float& OutNearestOpponentDistanceAtContact
) const
{
    OutNearestOpponentDistanceAtContact = -1.0f;

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return BallAvailableTime + 10.0f;
    }

    bool bFoundOpponent = false;
    bool bFoundOpponentArrival = false;
    float EarliestOpponentTime = BallAvailableTime + 10.0f;
    float NearestContactDistance = 0.0f;

    for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
    {
        ASoccerCharacterBase* Other = *It;

        if (
            !IsValid(Other) ||
            Other == this ||
            Other->GetTeam() == GetTeam()
        )
        {
            continue;
        }

        const float ContactDistance = FVector::Dist2D(
            Other->GetActorLocation(),
            ContactLocation
        );

        if (!bFoundOpponent || ContactDistance < NearestContactDistance)
        {
            NearestContactDistance = ContactDistance;
        }

        bFoundOpponent = true;

        const float ArrivalTime =
            Other->EstimateArrivalTimeToLocation(
                RecoveryLocation,
                AerialStandingControlRecoveryReachRadius
            );

        if (!FMath::IsFinite(ArrivalTime))
        {
            continue;
        }

        const float EffectiveArrivalTime = FMath::Max(
            BallAvailableTime,
            ArrivalTime +
                FMath::Max(
                    0.0f,
                    AerialStandingControlOpponentReactionTime
                )
        );

        if (
            !bFoundOpponentArrival ||
            EffectiveArrivalTime < EarliestOpponentTime
        )
        {
            EarliestOpponentTime = EffectiveArrivalTime;
            bFoundOpponentArrival = true;
        }
    }

    if (bFoundOpponent)
    {
        OutNearestOpponentDistanceAtContact =
            NearestContactDistance;
    }

    return
        bFoundOpponentArrival
        ? EarliestOpponentTime
        : BallAvailableTime + 10.0f;
}

bool ASoccerCharacterBase::IsStandingControlRecoveryLocationInsideField(
    const FVector& RecoveryLocation
) const
{
    const float Margin = FMath::Max(
        0.0f,
        AerialStandingControlFieldSafetyMargin
    );

    UWorld* World = GetWorld();
    if (World != nullptr)
    {
        for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
        {
            const ASoccerMatchManager* MatchManager = *It;
            if (IsValid(MatchManager))
            {
                const ASoccerField* SoccerField = MatchManager->GetSoccerField();
                if (IsValid(SoccerField))
                {
                    return SoccerField->IsWorldLocationInsidePitch(
                        RecoveryLocation,
                        Margin
                    );
                }
            }
            break;
        }
    }

    return SoccerFieldDimensions::IsLocationInsidePitch2D(
        RecoveryLocation,
        Margin
    );
}


bool ASoccerCharacterBase::TryStartBestAerialActionApproach(
    ASoccerBall* SoccerBall,
    ESoccerAerialActionIntent Intent
)
{
    if (IsAerialActionQueuedOrPlaying())
    {
        return false;
    }

    FSoccerAerialInterceptionPlan Plan;

    if (!FindBestAerialInterceptionPlan(SoccerBall, Intent, Plan))
    {
        return false;
    }

    return QueueAerialPlan(Plan, Intent, true);
}

bool ASoccerCharacterBase::TryStartAerialActionApproachForPlan(
    const FSoccerAerialInterceptionPlan& Plan,
    ESoccerAerialActionIntent Intent
)
{
    if (IsAerialActionQueuedOrPlaying())
    {
        return false;
    }

    return QueueAerialPlan(Plan, Intent, true);
}

bool ASoccerCharacterBase::TryChangeQueuedAerialActionIntent(
    ESoccerAerialActionIntent NewIntent
)
{
    if (
        AerialActionPhase != ESoccerAerialActionPhase::Approaching &&
        AerialActionPhase != ESoccerAerialActionPhase::WaitingToStart
    )
    {
        return false;
    }

    ASoccerBall* Ball = QueuedAerialPlan.Ball;

    if (!IsValid(Ball))
    {
        return false;
    }

    FSoccerAerialInterceptionPlan ReplacementPlan;

    if (!FindBestAerialInterceptionPlan(Ball, NewIntent, ReplacementPlan))
    {
        return false;
    }

    return QueueAerialPlan(ReplacementPlan, NewIntent, true);
}

void ASoccerCharacterBase::ResetAerialStartTimingDebug()
{
    bAerialDebugScheduledStartObserved = false;
    AerialDebugDistanceAtScheduledStart = -1.0f;
    AerialDebugEnteredMaximumCommitRadiusWorldTime = -1.0f;
    AerialDebugEnteredPreparationToleranceWorldTime = -1.0f;
    bAerialDebugScheduledCorrectionAttempted = false;
    AerialDebugScheduledCorrectionDistanceBefore = -1.0f;
    AerialDebugScheduledCorrectionDistanceAfter = -1.0f;
    AerialDebugMontagePlayRequestedWorldTime = -1.0f;
    AerialDebugMontagePlayReturnedDuration = 0.0f;
    AerialDebugMontageStartPosition = 0.0f;
    AerialDebugMontageLogicalStartWorldTime = -1.0f;
    AerialDebugMontageBecameActiveWorldTime = -1.0f;
}

bool ASoccerCharacterBase::QueueAerialPlan(
    const FSoccerAerialInterceptionPlan& Plan,
    ESoccerAerialActionIntent Intent,
    bool bAllowApproach
)
{
    if (IsTackleActive() || IsTackleFallReactionActive())
    {
        return false;
    }

    if (!Plan.bValid || !IsValid(Plan.Ball))
    {
        return false;
    }

    if (AerialActionPhase == ESoccerAerialActionPhase::Playing)
    {
        return false;
    }

    if (
        AerialActionPhase == ESoccerAerialActionPhase::None &&
        LastCompletedAerialActionBall.Get() == Plan.Ball &&
        LastCompletedAerialTrajectoryRevision == Plan.TrajectoryRevision
    )
    {
        return false;
    }

    if (GetAerialMontage(Plan.ActionType) == nullptr)
    {
        if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("%s: aerial plan found but montage is not assigned for action %d."),
                *GetName(),
                static_cast<int32>(Plan.ActionType)
            );
        }

        return false;
    }

    UAnimInstance* CurrentAnimInstance =
        GetMesh() != nullptr
        ? GetMesh()->GetAnimInstance()
        : nullptr;

    if (
        AerialActionPhase == ESoccerAerialActionPhase::None &&
        CurrentAnimInstance != nullptr &&
        CurrentAnimInstance->IsAnyMontagePlaying()
    )
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    const float DistanceToPreparation = FVector::Dist2D(
        GetActorLocation(),
        Plan.PreparationLocation
    );

    const FSoccerAerialActionProfile* PlanProfile =
        GetAerialProfile(Plan.ActionType);
    const float PreparationTolerance =
        PlanProfile != nullptr
        ? FMath::Min(
            AerialPlanStartLocationTolerance,
            FMath::Max(10.0f, PlanProfile->PreparationReachRadius)
        )
        : AerialPlanStartLocationTolerance;

    if (!bAllowApproach && DistanceToPreparation > PreparationTolerance)
    {
        if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
        {
            DrawAerialPlanDebug(Plan, FColor::Orange);
        }

        return false;
    }

    QueuedAerialPlan = Plan;
    QueuedAerialIntent = Intent;
    QueuedAerialStartWorldTime =
        World->GetTimeSeconds() + Plan.MontageStartDelay;
    ResetAerialStartTimingDebug();
    ActiveAerialActionType = Plan.ActionType;
    AerialApproachPlanRefreshAccumulator = 0.0f;
    AerialApproachPlanLossAccumulator = 0.0f;

    AerialActionPhase =
        DistanceToPreparation <= PreparationTolerance
        ? ESoccerAerialActionPhase::WaitingToStart
        : ESoccerAerialActionPhase::Approaching;

    if (AerialActionPhase == ESoccerAerialActionPhase::WaitingToStart)
    {
        if (Controller != nullptr)
        {
            Controller->StopMovement();
        }

        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }
    }

    if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
    {
        DrawAerialPlanDebug(
            QueuedAerialPlan,
            AerialActionPhase == ESoccerAerialActionPhase::Approaching
            ? FColor::Orange
            : FColor::Yellow
        );
    }

    return true;
}

bool ASoccerCharacterBase::RefreshQueuedAerialPlan(bool bForceRefresh)
{
    if (bAerialDebugPlanLocked)
    {
        AerialApproachPlanRefreshAccumulator = 0.0f;
        AerialApproachPlanLossAccumulator = 0.0f;
        return true;
    }

    if (
        AerialActionPhase != ESoccerAerialActionPhase::Approaching &&
        AerialActionPhase != ESoccerAerialActionPhase::WaitingToStart
    )
    {
        return false;
    }

    ASoccerBall* Ball = QueuedAerialPlan.Ball;

    if (!IsValid(Ball))
    {
        return false;
    }

    if (!bForceRefresh)
    {
        const UWorld* World = GetWorld();
        const float CurrentTime =
            World != nullptr ? World->GetTimeSeconds() : 0.0f;

        if (
            QueuedAerialStartWorldTime - CurrentTime <=
            AerialApproachPlanFreezeBeforeStartTime
        )
        {
            AerialApproachPlanRefreshAccumulator = 0.0f;
            return true;
        }
    }

    FSoccerAerialInterceptionPlan RefreshedPlan;

    if (!FindBestAerialInterceptionPlanInternal(
        Ball,
        QueuedAerialIntent,
        QueuedAerialPlan.PlannedContactSurface,
        RefreshedPlan
    ))
    {
        return false;
    }

    if (GetAerialMontage(RefreshedPlan.ActionType) == nullptr)
    {
        return false;
    }

    /* Keep an already selected passive redirect stable during approach. */
    if (
        QueuedAerialPlan.PlannedContactSurface ==
            ESoccerAerialContactSurface::Head &&
        QueuedAerialPlan.bHasStandingHeaderRedirectTarget &&
        RefreshedPlan.PlannedContactSurface ==
            ESoccerAerialContactSurface::Head
    )
    {
        FSoccerAerialInterceptionPlan StableRedirectPlan =
            RefreshedPlan;

        if (ConfigureRequestedStandingHeaderRedirect(
            Ball,
            QueuedAerialPlan.StandingHeaderRedirectTarget,
            StableRedirectPlan
        ))
        {
            RefreshedPlan = StableRedirectPlan;
        }
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    QueuedAerialPlan = RefreshedPlan;
    QueuedAerialStartWorldTime =
        World->GetTimeSeconds() + RefreshedPlan.MontageStartDelay;
    ResetAerialStartTimingDebug();
    ActiveAerialActionType = RefreshedPlan.ActionType;
    AerialApproachPlanRefreshAccumulator = 0.0f;
    AerialApproachPlanLossAccumulator = 0.0f;

    return true;
}

void ASoccerCharacterBase::UpdateAerialAction(float DeltaTime)
{
    if (AerialActionPhase == ESoccerAerialActionPhase::None)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        CancelAerialAction();
        return;
    }

    if (AerialActionPhase == ESoccerAerialActionPhase::Playing)
    {
        if (
            bStandingHeaderPostContactReleasePending &&
            World->GetTimeSeconds() >=
                StandingHeaderPostContactReleaseWorldTime
        )
        {
            ReleaseStandingHeaderAfterContact();
            return;
        }

        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }

        UAnimMontage* Montage =
            GetAerialMontage(ActiveAerialActionType);
        UAnimInstance* AnimInstance =
            GetMesh() != nullptr
            ? GetMesh()->GetAnimInstance()
            : nullptr;

        const bool bMontageStillPlaying =
            AnimInstance != nullptr &&
            Montage != nullptr &&
            AnimInstance->Montage_IsPlaying(Montage);

        if (
            bMontageStillPlaying &&
            AerialDebugMontageBecameActiveWorldTime < 0.0f
        )
        {
            AerialDebugMontageBecameActiveWorldTime =
                World->GetTimeSeconds();
        }

        if (bMontageStillPlaying)
        {
            UpdateAerialContact(DeltaTime);
        }

        if (
            !bMontageStillPlaying ||
            World->GetTimeSeconds() >=
                ActiveAerialExpectedEndWorldTime + 0.25f
        )
        {
            FinishAerialAction();
        }

        return;
    }

    ASoccerBall* Ball = QueuedAerialPlan.Ball;

    if (!QueuedAerialPlan.bValid || !IsValid(Ball))
    {
        CancelAerialAction();
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();
    const bool bTrajectoryChanged =
        Ball->GetTrajectoryRevision() !=
        QueuedAerialPlan.TrajectoryRevision;

    AerialApproachPlanRefreshAccumulator += DeltaTime;

    const bool bRefreshIntervalReached =
        AerialApproachPlanRefreshAccumulator >=
        FMath::Max(0.02f, AerialApproachPlanRefreshInterval);

    const bool bCanRegularlyRefresh =
        QueuedAerialStartWorldTime - CurrentTime >
            AerialApproachPlanFreezeBeforeStartTime ||
        AerialApproachPlanLossAccumulator > 0.0f;

    if (
        bTrajectoryChanged ||
        (bRefreshIntervalReached && bCanRegularlyRefresh)
    )
    {
        const bool bRefreshed =
            RefreshQueuedAerialPlan(bTrajectoryChanged);

        if (!bRefreshed)
        {
            AerialApproachPlanRefreshAccumulator = 0.0f;
            AerialApproachPlanLossAccumulator +=
                bTrajectoryChanged
                ? DeltaTime
                : FMath::Max(
                    DeltaTime,
                    AerialApproachPlanRefreshInterval
                );

            if (
                AerialApproachPlanLossAccumulator >=
                AerialApproachPlanLossGraceTime
            )
            {
                CancelAerialAction();
                return;
            }
        }
        else
        {
            AerialApproachPlanLossAccumulator = 0.0f;
        }
    }

    const float DistanceToPreparation = FVector::Dist2D(
        GetActorLocation(),
        QueuedAerialPlan.PreparationLocation
    );

    const FSoccerAerialActionProfile* Profile =
        GetAerialProfile(QueuedAerialPlan.ActionType);
    const float SettleTime =
        Profile != nullptr ? Profile->PreparationSettleTime : 0.08f;
    const float PreparationTolerance =
        Profile != nullptr
        ? FMath::Min(
            AerialPlanStartLocationTolerance,
            FMath::Max(10.0f, Profile->PreparationReachRadius)
        )
        : AerialPlanStartLocationTolerance;
    const float MaximumCommitDistance = FMath::Max(
        PreparationTolerance,
        FMath::Max(0.0f, AerialScheduledStartMaxPositionCorrection)
    );

    if (
        AerialDebugEnteredMaximumCommitRadiusWorldTime < 0.0f &&
        DistanceToPreparation <= MaximumCommitDistance
    )
    {
        AerialDebugEnteredMaximumCommitRadiusWorldTime = CurrentTime;
    }

    if (
        AerialDebugEnteredPreparationToleranceWorldTime < 0.0f &&
        DistanceToPreparation <= PreparationTolerance
    )
    {
        AerialDebugEnteredPreparationToleranceWorldTime = CurrentTime;
    }

    if (
        !bAerialDebugScheduledStartObserved &&
        CurrentTime >= QueuedAerialStartWorldTime
    )
    {
        bAerialDebugScheduledStartObserved = true;
        AerialDebugDistanceAtScheduledStart = DistanceToPreparation;
    }

    if (AerialActionPhase == ESoccerAerialActionPhase::Approaching)
    {
        if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
        {
            DrawAerialPlanDebug(QueuedAerialPlan, FColor::Orange);
        }

        /*
         * At the scheduled start time do not keep waiting for MoveTo to enter
         * the very small final reach radius. The montage is in-place, so a
         * bounded final XY correction is preferable to starting 0.10-0.20 s
         * late. The helper only commits if the trajectory is still valid and
         * the correction can actually place the capsule inside tolerance.
         */
        if (
            CurrentTime >= QueuedAerialStartWorldTime &&
            TryCommitQueuedAerialMontageAtScheduledTime(
                CurrentTime,
                DistanceToPreparation,
                PreparationTolerance,
                Ball
            )
        )
        {
            return;
        }

        if (
            CurrentTime >
            QueuedAerialStartWorldTime +
                AerialPlanLateCancellationTolerance
        )
        {
            CancelAerialAction();
            return;
        }

        if (DistanceToPreparation <= PreparationTolerance)
        {
            if (Controller != nullptr)
            {
                Controller->StopMovement();
            }

            if (UCharacterMovementComponent* Movement = GetCharacterMovement())
            {
                Movement->StopMovementImmediately();
            }

            AerialActionPhase =
                ESoccerAerialActionPhase::WaitingToStart;

            /*
             * If the character reaches the preparation point on or after
             * the scheduled start time, do not lose another frame and do
             * not wait for gradual yaw convergence. The plan already chose
             * the incoming-ball facing direction, so commit that yaw and
             * start the montage in this same Tick.
             */
            if (
                CurrentTime >= QueuedAerialStartWorldTime &&
                AerialApproachPlanLossAccumulator <= KINDA_SMALL_NUMBER &&
                Ball->GetTrajectoryRevision() ==
                    QueuedAerialPlan.TrajectoryRevision
            )
            {
                SnapFacingToQueuedAerialPlan();

                if (!StartQueuedAerialMontage())
                {
                    CancelAerialAction();
                }
            }
        }

        return;
    }

    if (AerialActionPhase == ESoccerAerialActionPhase::WaitingToStart)
    {
        if (DistanceToPreparation > AerialPlanCancelLocationTolerance)
        {
            CancelAerialAction();
            return;
        }

        if (
            DistanceToPreparation > PreparationTolerance &&
            CurrentTime + SettleTime < QueuedAerialStartWorldTime
        )
        {
            AerialActionPhase = ESoccerAerialActionPhase::Approaching;
            return;
        }

        if (Controller != nullptr)
        {
            Controller->StopMovement();
        }

        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }

        RotateTowardQueuedAerialPlan(DeltaTime);

        if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
        {
            DrawAerialPlanDebug(QueuedAerialPlan, FColor::Yellow);
        }

        if (
            CurrentTime >
            QueuedAerialStartWorldTime +
                AerialPlanLateCancellationTolerance
        )
        {
            CancelAerialAction();
            return;
        }

        if (CurrentTime >= QueuedAerialStartWorldTime)
        {
            TryCommitQueuedAerialMontageAtScheduledTime(
                CurrentTime,
                DistanceToPreparation,
                PreparationTolerance,
                Ball
            );
        }
    }
}

bool ASoccerCharacterBase::TryCommitQueuedAerialMontageAtScheduledTime(
    float CurrentTime,
    float DistanceToPreparation,
    float PreparationTolerance,
    ASoccerBall* Ball
)
{
    if (
        CurrentTime < QueuedAerialStartWorldTime ||
        AerialApproachPlanLossAccumulator > KINDA_SMALL_NUMBER ||
        !IsValid(Ball) ||
        Ball->GetTrajectoryRevision() !=
            QueuedAerialPlan.TrajectoryRevision
    )
    {
        return false;
    }

    const float MaximumCommitDistance = FMath::Max(
        PreparationTolerance,
        FMath::Max(
            0.0f,
            AerialScheduledStartMaxPositionCorrection
        )
    );

    if (DistanceToPreparation > MaximumCommitDistance)
    {
        return false;
    }

    if (Controller != nullptr)
    {
        Controller->StopMovement();
    }

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }

    if (
        bUseAerialScheduledStartPositionCorrection &&
        DistanceToPreparation > PreparationTolerance
    )
    {
        if (!bAerialDebugScheduledCorrectionAttempted)
        {
            bAerialDebugScheduledCorrectionAttempted = true;
            AerialDebugScheduledCorrectionDistanceBefore =
                DistanceToPreparation;
        }

        FVector CorrectedLocation =
            QueuedAerialPlan.PreparationLocation;
        CorrectedLocation.Z = GetActorLocation().Z;

        FHitResult SweepHit;
        SetActorLocation(
            CorrectedLocation,
            true,
            &SweepHit,
            ETeleportType::TeleportPhysics
        );
    }

    const float RemainingDistance = FVector::Dist2D(
        GetActorLocation(),
        QueuedAerialPlan.PreparationLocation
    );

    if (bAerialDebugScheduledCorrectionAttempted)
    {
        AerialDebugScheduledCorrectionDistanceAfter = RemainingDistance;
    }

    if (RemainingDistance > PreparationTolerance)
    {
        return false;
    }

    AerialActionPhase = ESoccerAerialActionPhase::WaitingToStart;
    SnapFacingToQueuedAerialPlan();

    if (!StartQueuedAerialMontage())
    {
        CancelAerialAction();
        return false;
    }

    return true;
}

bool ASoccerCharacterBase::StartQueuedAerialMontage()
{
    if (
        AerialActionPhase !=
            ESoccerAerialActionPhase::WaitingToStart ||
        !QueuedAerialPlan.bValid
    )
    {
        return false;
    }

    UAnimMontage* Montage =
        GetAerialMontage(QueuedAerialPlan.ActionType);
    UAnimInstance* AnimInstance =
        GetMesh() != nullptr
        ? GetMesh()->GetAnimInstance()
        : nullptr;

    if (Montage == nullptr || AnimInstance == nullptr)
    {
        return false;
    }

    if (Controller != nullptr)
    {
        Controller->StopMovement();
    }

    UCharacterMovementComponent* Movement = GetCharacterMovement();

    if (Movement != nullptr)
    {
        SavedAerialMovementMode = Movement->MovementMode;
        SavedAerialCustomMovementMode = Movement->CustomMovementMode;
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
        bAerialMovementWasLocked = true;
    }

    const UWorld* StartWorld = GetWorld();
    AerialDebugMontagePlayRequestedWorldTime =
        StartWorld != nullptr ? StartWorld->GetTimeSeconds() : -1.0f;

    /*
     * The scheduled instant can fall between two game Ticks. Starting the
     * montage at position zero on the next Tick would make the animation
     * permanently late by one frame (or more at low frame rate). Compensate
     * that discrete-Tick lateness by advancing the montage by the elapsed
     * fraction, bounded by the profile's permitted late-start window.
     */
    const FSoccerAerialActionProfile* ActiveProfile =
        GetAerialProfile(QueuedAerialPlan.ActionType);

    const float RawScheduledLateness =
        AerialDebugMontagePlayRequestedWorldTime >= 0.0f
        ? FMath::Max(
            0.0f,
            AerialDebugMontagePlayRequestedWorldTime -
                QueuedAerialStartWorldTime
        )
        : 0.0f;

    const float MaximumCatchUp =
        ActiveProfile != nullptr
        ? FMath::Max(0.0f, ActiveProfile->MaximumLateStartTime)
        : 0.12f;

    const float MontageStartPosition = FMath::Clamp(
        RawScheduledLateness,
        0.0f,
        MaximumCatchUp
    );

    const float PlayedDuration =
        AnimInstance->Montage_Play(Montage, 1.0f);
    AerialDebugMontagePlayReturnedDuration = PlayedDuration;

    if (PlayedDuration <= 0.0f)
    {
        if (Movement != nullptr && bAerialMovementWasLocked)
        {
            Movement->SetMovementMode(
                SavedAerialMovementMode,
                SavedAerialCustomMovementMode
            );
        }

        bAerialMovementWasLocked = false;
        return false;
    }

    if (MontageStartPosition > KINDA_SMALL_NUMBER)
    {
        AnimInstance->Montage_SetPosition(
            Montage,
            MontageStartPosition
        );
    }

    AerialDebugMontageStartPosition = MontageStartPosition;
    AerialDebugMontageLogicalStartWorldTime =
        AerialDebugMontagePlayRequestedWorldTime >= 0.0f
        ? AerialDebugMontagePlayRequestedWorldTime - MontageStartPosition
        : -1.0f;

    ActiveAerialActionType = QueuedAerialPlan.ActionType;
    AerialActionPhase = ESoccerAerialActionPhase::Playing;

    bStandingHeaderPostContactReleasePending = false;
    StandingHeaderPostContactReleaseWorldTime = 0.0f;
    bAerialContactTrackingInitialized = false;
    bAerialContactResolved = false;
    LastAerialContactWorldTime = -1000.0f;
    PreviousAerialMontagePosition = 0.0f;
    ActiveAerialContactTrajectoryRevision =
        QueuedAerialPlan.TrajectoryRevision;
    PendingAerialContestExitImpulse = FVector::ZeroVector;
    AerialBodyContactProcessedOpponents.Empty();
    LastAerialContactResult = FSoccerAerialContactResult();
    InitializeAerialContactTracking();

    const UWorld* World = GetWorld();
    ActiveAerialExpectedEndWorldTime =
        (World != nullptr ? World->GetTimeSeconds() : 0.0f) +
        FMath::Max(0.0f, PlayedDuration - MontageStartPosition);

    if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
    {
        DrawAerialPlanDebug(QueuedAerialPlan, FColor::Green);
    }

    return true;
}

void ASoccerCharacterBase::SnapFacingToQueuedAerialPlan()
{
    FVector DesiredDirection = QueuedAerialPlan.FacingDirection;
    DesiredDirection.Z = 0.0f;
    DesiredDirection.Normalize();

    if (DesiredDirection.IsNearlyZero())
    {
        return;
    }

    const FRotator DesiredRotation = DesiredDirection.Rotation();
    SetActorRotation(FRotator(0.0f, DesiredRotation.Yaw, 0.0f));
}

void ASoccerCharacterBase::RotateTowardQueuedAerialPlan(float DeltaTime)
{
    FVector DesiredDirection = QueuedAerialPlan.FacingDirection;
    DesiredDirection.Z = 0.0f;
    DesiredDirection.Normalize();

    if (DesiredDirection.IsNearlyZero())
    {
        return;
    }

    const FRotator TargetRotation = DesiredDirection.Rotation();
    FRotator CurrentRotation = GetActorRotation();
    CurrentRotation.Pitch = 0.0f;
    CurrentRotation.Roll = 0.0f;

    const float MaximumYawStep =
        FMath::Max(1.0f, AerialFacingRotationSpeedDegreesPerSecond) *
        FMath::Max(0.0f, DeltaTime);

    const float NewYaw = FMath::FixedTurn(
        CurrentRotation.Yaw,
        TargetRotation.Yaw,
        MaximumYawStep
    );

    SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
}

void ASoccerCharacterBase::ScheduleStandingHeaderPostContactRelease()
{
    if (
        AerialActionPhase != ESoccerAerialActionPhase::Playing ||
        ActiveAerialActionType !=
            ESoccerAerialActionType::StandingControl
    )
    {
        return;
    }

    const UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    bStandingHeaderPostContactReleasePending = true;
    StandingHeaderPostContactReleaseWorldTime =
        World->GetTimeSeconds() +
        FMath::Max(
            0.0f,
            AerialStandingHeaderPostContactReleaseDelay
        );
}

void ASoccerCharacterBase::ReleaseStandingHeaderAfterContact()
{
    if (
        AerialActionPhase != ESoccerAerialActionPhase::Playing ||
        ActiveAerialActionType !=
            ESoccerAerialActionType::StandingControl
    )
    {
        bStandingHeaderPostContactReleasePending = false;
        StandingHeaderPostContactReleaseWorldTime = 0.0f;
        return;
    }

    UAnimMontage* Montage = GetAerialMontage(ActiveAerialActionType);
    UAnimInstance* AnimInstance =
        GetMesh() != nullptr
        ? GetMesh()->GetAnimInstance()
        : nullptr;

    if (
        AnimInstance != nullptr &&
        Montage != nullptr &&
        AnimInstance->Montage_IsPlaying(Montage)
    )
    {
        AnimInstance->Montage_Stop(
            FMath::Max(
                0.0f,
                AerialStandingHeaderPostContactBlendOutTime
            ),
            Montage
        );
    }

    /*
     * Finish the gameplay lock immediately. The montage may still visually
     * blend out, but movement/navigation can already produce the first run
     * frame toward the redirected second ball.
     */
    FinishAerialAction();
}

void ASoccerCharacterBase::FinishAerialAction()
{
    const bool bCompletedPlayingAction =
        AerialActionPhase == ESoccerAerialActionPhase::Playing;
    ASoccerBall* CompletedBall = QueuedAerialPlan.Ball;
    const int32 CompletedRevision =
        IsValid(CompletedBall)
        ? CompletedBall->GetTrajectoryRevision()
        : QueuedAerialPlan.TrajectoryRevision;
    const FVector ExitImpulse = PendingAerialContestExitImpulse;

    if (bAerialMovementWasLocked)
    {
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->SetMovementMode(
                SavedAerialMovementMode,
                SavedAerialCustomMovementMode
            );
            Movement->StopMovementImmediately();
        }
    }

    if (!ExitImpulse.IsNearlyZero())
    {
        LaunchCharacter(
            ExitImpulse,
            false,
            false
        );
    }

    bAerialMovementWasLocked = false;
    bStandingHeaderPostContactReleasePending = false;
    StandingHeaderPostContactReleaseWorldTime = 0.0f;
    bAerialContactTrackingInitialized = false;
    bAerialContactResolved = false;
    PreviousAerialMontagePosition = 0.0f;
    PreviousAerialBallLocation = FVector::ZeroVector;
    PreviousAerialHeadLocation = FVector::ZeroVector;
    PreviousAerialChestLowerLocation = FVector::ZeroVector;
    PreviousAerialChestUpperLocation = FVector::ZeroVector;
    ActiveAerialContactTrajectoryRevision = INDEX_NONE;
    PendingAerialContestExitImpulse = FVector::ZeroVector;
    AerialBodyContactProcessedOpponents.Empty();
bAerialDebugPlanLocked = false;
    AerialActionPhase = ESoccerAerialActionPhase::None;
    ActiveAerialActionType = ESoccerAerialActionType::None;
    QueuedAerialPlan = FSoccerAerialInterceptionPlan();
    QueuedAerialIntent = ESoccerAerialActionIntent::Automatic;
    QueuedAerialStartWorldTime = 0.0f;
    ActiveAerialExpectedEndWorldTime = 0.0f;
    AerialApproachPlanRefreshAccumulator = 0.0f;
    AerialApproachPlanLossAccumulator = 0.0f;

    if (bCompletedPlayingAction && IsValid(CompletedBall))
    {
        LastCompletedAerialActionBall = CompletedBall;
        LastCompletedAerialTrajectoryRevision = CompletedRevision;
    }
}

void ASoccerCharacterBase::CancelAerialAction()
{
    UAnimMontage* Montage = GetAerialMontage(ActiveAerialActionType);
    UAnimInstance* AnimInstance =
        GetMesh() != nullptr
        ? GetMesh()->GetAnimInstance()
        : nullptr;

    if (
        AerialActionPhase == ESoccerAerialActionPhase::Playing &&
        AnimInstance != nullptr &&
        Montage != nullptr &&
        AnimInstance->Montage_IsPlaying(Montage)
    )
    {
        AnimInstance->Montage_Stop(0.15f, Montage);
    }

    FinishAerialAction();
}

bool ASoccerCharacterBase::IsAerialActionLocked() const
{
    return AerialActionPhase == ESoccerAerialActionPhase::Playing;
}

bool ASoccerCharacterBase::IsAerialActionApproaching() const
{
    return AerialActionPhase == ESoccerAerialActionPhase::Approaching;
}

bool ASoccerCharacterBase::IsAerialActionWaitingToStart() const
{
    return AerialActionPhase == ESoccerAerialActionPhase::WaitingToStart;
}

bool ASoccerCharacterBase::IsAerialActionQueuedOrPlaying() const
{
    return AerialActionPhase != ESoccerAerialActionPhase::None;
}

ESoccerAerialActionType
ASoccerCharacterBase::GetActiveAerialActionType() const
{
    return ActiveAerialActionType;
}

ESoccerAerialActionPhase
ASoccerCharacterBase::GetAerialActionPhase() const
{
    return AerialActionPhase;
}

bool ASoccerCharacterBase::GetAerialPreparationTarget(
    FVector& OutLocation,
    float& OutAcceptanceRadius
) const
{
    OutLocation = FVector::ZeroVector;
    OutAcceptanceRadius = AerialPlanStartLocationTolerance;

    if (
        AerialActionPhase != ESoccerAerialActionPhase::Approaching &&
        AerialActionPhase != ESoccerAerialActionPhase::WaitingToStart
    )
    {
        return false;
    }

    if (!QueuedAerialPlan.bValid)
    {
        return false;
    }

    OutLocation = QueuedAerialPlan.PreparationLocation;

    const FSoccerAerialActionProfile* Profile =
        GetAerialProfile(QueuedAerialPlan.ActionType);

    if (Profile != nullptr)
    {
        OutAcceptanceRadius = FMath::Min(
            AerialPlanStartLocationTolerance,
            FMath::Max(10.0f, Profile->PreparationReachRadius)
        );
    }

    return true;
}

ASoccerBall* ASoccerCharacterBase::GetAerialActionBall() const
{
    return QueuedAerialPlan.Ball;
}

const FSoccerAerialInterceptionPlan&
ASoccerCharacterBase::GetQueuedAerialPlan() const
{
    return QueuedAerialPlan;
}

const FSoccerAerialActionProfile*
ASoccerCharacterBase::GetAerialProfile(
    ESoccerAerialActionType ActionType
) const
{
    switch (ActionType)
    {
    case ESoccerAerialActionType::StandingControl:
        return &StandingAerialControlProfile;

    case ESoccerAerialActionType::JumpHeaderKick:
        return &JumpHeaderKickProfile;

    case ESoccerAerialActionType::JumpHeaderBlock:
        return &JumpHeaderBlockProfile;

    case ESoccerAerialActionType::None:
    default:
        return nullptr;
    }
}

UAnimMontage* ASoccerCharacterBase::GetAerialMontage(
    ESoccerAerialActionType ActionType
) const
{
    switch (ActionType)
    {
    case ESoccerAerialActionType::StandingControl:
        return HeaderChestMontage;

    case ESoccerAerialActionType::JumpHeaderKick:
        return HeaderJumpKickMontage;

    case ESoccerAerialActionType::JumpHeaderBlock:
        return HeaderJumpBlockMontage;

    case ESoccerAerialActionType::None:
    default:
        return nullptr;
    }
}

bool ASoccerCharacterBase::HasResolvedAerialBallContact() const
{
    return bAerialContactResolved;
}

FSoccerAerialContactResult
ASoccerCharacterBase::GetLastAerialContactResult() const
{
    return LastAerialContactResult;
}

float ASoccerCharacterBase::GetLastAerialContactWorldTime() const
{
    return LastAerialContactWorldTime;
}

bool ASoccerCharacterBase::GetAerialActionProfileCopy(
    ESoccerAerialActionType ActionType,
    FSoccerAerialActionProfile& OutProfile
) const
{
    const FSoccerAerialActionProfile* Profile =
        GetAerialProfile(ActionType);

    if (Profile == nullptr)
    {
        OutProfile = FSoccerAerialActionProfile();
        return false;
    }

    OutProfile = *Profile;
    return true;
}

bool ASoccerCharacterBase::ResolveAerialContactBoneNames(
    FName& OutHeadBoneName,
    FName& OutChestLowerBoneName,
    FName& OutChestUpperBoneName,
    FString* OutFailureReason
) const
{
    OutHeadBoneName = NAME_None;
    OutChestLowerBoneName = NAME_None;
    OutChestUpperBoneName = NAME_None;

    if (OutFailureReason != nullptr)
    {
        OutFailureReason->Empty();
    }

    const USkeletalMeshComponent* CharacterMesh = GetMesh();

    if (CharacterMesh == nullptr)
    {
        if (OutFailureReason != nullptr)
        {
            *OutFailureReason = TEXT("El personaje no tiene SkeletalMeshComponent");
        }

        return false;
    }

    const auto ResolveOneBone =
        [CharacterMesh](
            const FName& ConfiguredName,
            const FName& PlainName,
            const FName& MixamoName,
            FName& OutResolvedName
        ) -> bool
        {
            TArray<FName> Candidates;

            if (!ConfiguredName.IsNone())
            {
                Candidates.AddUnique(ConfiguredName);

                const FString ConfiguredString = ConfiguredName.ToString();
                int32 NamespaceSeparatorIndex = INDEX_NONE;

                if (
                    ConfiguredString.FindLastChar(
                        TEXT(':'),
                        NamespaceSeparatorIndex
                    ) &&
                    NamespaceSeparatorIndex + 1 < ConfiguredString.Len()
                )
                {
                    Candidates.AddUnique(
                        FName(*ConfiguredString.Mid(NamespaceSeparatorIndex + 1))
                    );
                }
            }

            Candidates.AddUnique(PlainName);
            Candidates.AddUnique(MixamoName);

            for (const FName& Candidate : Candidates)
            {
                if (
                    !Candidate.IsNone() &&
                    CharacterMesh->GetBoneIndex(Candidate) != INDEX_NONE
                )
                {
                    OutResolvedName = Candidate;
                    return true;
                }
            }

            return false;
        };

    const bool bHeadResolved = ResolveOneBone(
        AerialHeadBoneName,
        FName(TEXT("Head")),
        FName(TEXT("mixamorig:Head")),
        OutHeadBoneName
    );

    const bool bChestLowerResolved = ResolveOneBone(
        AerialChestLowerBoneName,
        FName(TEXT("Spine2")),
        FName(TEXT("mixamorig:Spine2")),
        OutChestLowerBoneName
    );

    const bool bChestUpperResolved = ResolveOneBone(
        AerialChestUpperBoneName,
        FName(TEXT("Neck")),
        FName(TEXT("mixamorig:Neck")),
        OutChestUpperBoneName
    );

    if (bHeadResolved && bChestLowerResolved && bChestUpperResolved)
    {
        return true;
    }

    if (OutFailureReason != nullptr)
    {
        TArray<FString> MissingBones;

        if (!bHeadResolved)
        {
            MissingBones.Add(
                FString::Printf(
                    TEXT("Head (configurado: %s)"),
                    *AerialHeadBoneName.ToString()
                )
            );
        }

        if (!bChestLowerResolved)
        {
            MissingBones.Add(
                FString::Printf(
                    TEXT("Spine2 (configurado: %s)"),
                    *AerialChestLowerBoneName.ToString()
                )
            );
        }

        if (!bChestUpperResolved)
        {
            MissingBones.Add(
                FString::Printf(
                    TEXT("Neck (configurado: %s)"),
                    *AerialChestUpperBoneName.ToString()
                )
            );
        }

        *OutFailureReason = FString::Printf(
            TEXT("Huesos aereos no encontrados: %s"),
            *FString::Join(MissingBones, TEXT(", "))
        );
    }

    return false;
}

UCurveTable* ASoccerCharacterBase::GetAerialContactTrackCurveTable(
    ESoccerAerialActionType ActionType
) const
{
    switch (ActionType)
    {
    case ESoccerAerialActionType::StandingControl:
        return StandingAerialControlContactTrackCurveTable;

    case ESoccerAerialActionType::JumpHeaderKick:
        return JumpHeaderKickContactTrackCurveTable;

    case ESoccerAerialActionType::JumpHeaderBlock:
        return JumpHeaderBlockContactTrackCurveTable;

    case ESoccerAerialActionType::None:
    default:
        return nullptr;
    }
}

bool ASoccerCharacterBase::EvaluateAerialContactTrack(
    ESoccerAerialActionType ActionType,
    float MontageTime,
    FVector& OutChestLowerLocal,
    FVector& OutChestUpperLocal,
    FVector& OutHeadLocal
) const
{
    OutChestLowerLocal = FVector::ZeroVector;
    OutChestUpperLocal = FVector::ZeroVector;
    OutHeadLocal = FVector::ZeroVector;

    UCurveTable* ContactTrackCurveTable =
        GetAerialContactTrackCurveTable(ActionType);

    if (ContactTrackCurveTable == nullptr)
    {
        return false;
    }

    const FString ContextString = FString::Printf(
        TEXT("Aerial Contact Track %d"),
        static_cast<int32>(ActionType)
    );

    auto FindTrackCurve =
        [ContactTrackCurveTable, &ContextString](
            const TCHAR* RowName
        ) -> const FRealCurve*
        {
            return ContactTrackCurveTable->FindCurve(
                FName(RowName),
                ContextString,
                false
            );
        };

    const FRealCurve* ChestLowerForward =
        FindTrackCurve(TEXT("ChestLowerForward"));
    const FRealCurve* ChestLowerLateral =
        FindTrackCurve(TEXT("ChestLowerLateral"));
    const FRealCurve* ChestLowerUp =
        FindTrackCurve(TEXT("ChestLowerUp"));

    const FRealCurve* ChestUpperForward =
        FindTrackCurve(TEXT("ChestUpperForward"));
    const FRealCurve* ChestUpperLateral =
        FindTrackCurve(TEXT("ChestUpperLateral"));
    const FRealCurve* ChestUpperUp =
        FindTrackCurve(TEXT("ChestUpperUp"));

    const FRealCurve* HeadForward = FindTrackCurve(TEXT("HeadForward"));
    const FRealCurve* HeadLateral = FindTrackCurve(TEXT("HeadLateral"));
    const FRealCurve* HeadUp = FindTrackCurve(TEXT("HeadUp"));

    if (
        ChestLowerForward == nullptr ||
        ChestLowerLateral == nullptr ||
        ChestLowerUp == nullptr ||
        ChestUpperForward == nullptr ||
        ChestUpperLateral == nullptr ||
        ChestUpperUp == nullptr ||
        HeadForward == nullptr ||
        HeadLateral == nullptr ||
        HeadUp == nullptr
    )
    {
        return false;
    }

    const float SafeTime = FMath::Max(0.0f, MontageTime);

    /*
     * These rows are already converted before Unreal import:
     *   Local X = Forward  = Blender/Mixamo Z
     *   Local Y = Lateral  = -Blender/Mixamo X (right positive)
     *   Local Z = Up       = Blender/Mixamo Y
     * ChestLower/ChestUpper contain the exported bone-head positions.
     * HeadForward/HeadLateral/HeadUp contain the geometric center of Head.
     */
    OutChestLowerLocal = FVector(
        ChestLowerForward->Eval(SafeTime),
        ChestLowerLateral->Eval(SafeTime),
        ChestLowerUp->Eval(SafeTime)
    );

    OutChestUpperLocal = FVector(
        ChestUpperForward->Eval(SafeTime),
        ChestUpperLateral->Eval(SafeTime),
        ChestUpperUp->Eval(SafeTime)
    );

    OutHeadLocal = FVector(
        HeadForward->Eval(SafeTime),
        HeadLateral->Eval(SafeTime),
        HeadUp->Eval(SafeTime)
    );

    return true;
}

bool ASoccerCharacterBase::GetAerialContactTrackWorldLocations(
    ESoccerAerialActionType ActionType,
    float MontageTime,
    const FVector& FacingDirection,
    const FVector& ActorLocation,
    FVector& OutChestLowerWorld,
    FVector& OutChestUpperWorld,
    FVector& OutHeadWorld
) const
{
    OutChestLowerWorld = FVector::ZeroVector;
    OutChestUpperWorld = FVector::ZeroVector;
    OutHeadWorld = FVector::ZeroVector;

    const USkeletalMeshComponent* CharacterMesh = GetMesh();

    if (CharacterMesh == nullptr)
    {
        return false;
    }

    FVector ChestLowerLocal;
    FVector ChestUpperLocal;
    FVector HeadLocal;

    if (!EvaluateAerialContactTrack(
        ActionType,
        MontageTime,
        ChestLowerLocal,
        ChestUpperLocal,
        HeadLocal
    ))
    {
        return false;
    }

    FVector Forward = FacingDirection;
    Forward.Z = 0.0f;
    Forward = Forward.GetSafeNormal();

    if (Forward.IsNearlyZero())
    {
        Forward = GetActorForwardVector().GetSafeNormal2D();
    }

    if (Forward.IsNearlyZero())
    {
        Forward = FVector::ForwardVector;
    }

    FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
    Right.Z = 0.0f;
    Right = Right.GetSafeNormal();

    if (Right.IsNearlyZero())
    {
        Right = GetActorRightVector().GetSafeNormal2D();
    }

    const FRotator FacingRotation = Forward.Rotation();
    const FVector MeshRelativeLocation = CharacterMesh->GetRelativeLocation();
    const FVector MeshOriginWorld =
        ActorLocation + FacingRotation.RotateVector(MeshRelativeLocation);

    auto TrackLocalToWorld =
        [&MeshOriginWorld, &Forward, &Right](const FVector& TrackLocal)
        {
            return
                MeshOriginWorld +
                Forward * TrackLocal.X +
                Right * TrackLocal.Y +
                FVector::UpVector * TrackLocal.Z;
        };

    OutChestLowerWorld = TrackLocalToWorld(ChestLowerLocal);
    OutChestUpperWorld = TrackLocalToWorld(ChestUpperLocal);
    OutHeadWorld = TrackLocalToWorld(HeadLocal);

    return true;
}

bool ASoccerCharacterBase::EvaluateStandingAerialControlContactTrack(
    float MontageTime,
    FVector& OutChestLowerLocal,
    FVector& OutChestUpperLocal,
    FVector& OutHeadLocal
) const
{
    return EvaluateAerialContactTrack(
        ESoccerAerialActionType::StandingControl,
        MontageTime,
        OutChestLowerLocal,
        OutChestUpperLocal,
        OutHeadLocal
    );
}

bool ASoccerCharacterBase::GetStandingAerialControlTrackWorldLocations(
    float MontageTime,
    const FVector& FacingDirection,
    const FVector& ActorLocation,
    FVector& OutChestLowerWorld,
    FVector& OutChestUpperWorld,
    FVector& OutHeadWorld
) const
{
    return GetAerialContactTrackWorldLocations(
        ESoccerAerialActionType::StandingControl,
        MontageTime,
        FacingDirection,
        ActorLocation,
        OutChestLowerWorld,
        OutChestUpperWorld,
        OutHeadWorld
    );
}

bool ASoccerCharacterBase::GetAerialDebugSnapshot(
    FSoccerAerialDebugSnapshot& OutSnapshot
) const
{
    OutSnapshot = FSoccerAerialDebugSnapshot();

    ASoccerBall* Ball = QueuedAerialPlan.Ball;
    USkeletalMeshComponent* CharacterMesh = GetMesh();
    const FSoccerAerialActionProfile* Profile =
        GetAerialProfile(ActiveAerialActionType);

    if (
        !IsValid(Ball) ||
        CharacterMesh == nullptr ||
        Profile == nullptr
    )
    {
        return false;
    }

    FName ResolvedHeadBoneName;
    FName ResolvedChestLowerBoneName;
    FName ResolvedChestUpperBoneName;

    if (
        !ResolveAerialContactBoneNames(
            ResolvedHeadBoneName,
            ResolvedChestLowerBoneName,
            ResolvedChestUpperBoneName
        )
    )
    {
        return false;
    }

    float MontagePosition = 0.0f;
    UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
    UAnimMontage* Montage = GetAerialMontage(ActiveAerialActionType);

    if (AnimInstance != nullptr && Montage != nullptr)
    {
        MontagePosition = AnimInstance->Montage_GetPosition(Montage);
    }

    OutSnapshot.bValid = true;
    OutSnapshot.ActionType = ActiveAerialActionType;
    OutSnapshot.Phase = AerialActionPhase;
    OutSnapshot.MontagePosition = MontagePosition;

    const UWorld* SnapshotWorld = GetWorld();
    OutSnapshot.CurrentWorldTime =
        SnapshotWorld != nullptr ? SnapshotWorld->GetTimeSeconds() : 0.0f;
    OutSnapshot.ScheduledStartWorldTime = QueuedAerialStartWorldTime;
    OutSnapshot.DistanceToPreparation = FVector::Dist2D(
        GetActorLocation(),
        QueuedAerialPlan.PreparationLocation
    );
    OutSnapshot.PreparationTolerance = FMath::Min(
        AerialPlanStartLocationTolerance,
        FMath::Max(10.0f, Profile->PreparationReachRadius)
    );
    OutSnapshot.MaximumCommitDistance = FMath::Max(
        OutSnapshot.PreparationTolerance,
        FMath::Max(0.0f, AerialScheduledStartMaxPositionCorrection)
    );
    OutSnapshot.DistanceAtScheduledStart =
        AerialDebugDistanceAtScheduledStart;
    OutSnapshot.EnteredMaximumCommitRadiusWorldTime =
        AerialDebugEnteredMaximumCommitRadiusWorldTime;
    OutSnapshot.EnteredPreparationToleranceWorldTime =
        AerialDebugEnteredPreparationToleranceWorldTime;
    OutSnapshot.bScheduledCorrectionAttempted =
        bAerialDebugScheduledCorrectionAttempted;
    OutSnapshot.ScheduledCorrectionDistanceBefore =
        AerialDebugScheduledCorrectionDistanceBefore;
    OutSnapshot.ScheduledCorrectionDistanceAfter =
        AerialDebugScheduledCorrectionDistanceAfter;
    OutSnapshot.MontagePlayRequestedWorldTime =
        AerialDebugMontagePlayRequestedWorldTime;
    OutSnapshot.MontagePlayReturnedDuration =
        AerialDebugMontagePlayReturnedDuration;
    OutSnapshot.MontageStartPosition =
        AerialDebugMontageStartPosition;
    OutSnapshot.MontageLogicalStartWorldTime =
        AerialDebugMontageLogicalStartWorldTime;
    OutSnapshot.MontageBecameActiveWorldTime =
        AerialDebugMontageBecameActiveWorldTime;

    OutSnapshot.ContactWindowStart = Profile->ContactWindowStart;
    OutSnapshot.IdealContactTime = Profile->IdealContactTime;
    OutSnapshot.ContactWindowEnd = Profile->ContactWindowEnd;
    OutSnapshot.bContactWindowActive =
        AerialActionPhase == ESoccerAerialActionPhase::Playing &&
        MontagePosition >= Profile->ContactWindowStart &&
        MontagePosition <= Profile->ContactWindowEnd;

    OutSnapshot.BallLocation = Ball->GetActorLocation();
    OutSnapshot.HeadLocation =
        CharacterMesh->GetBoneLocation(
            ResolvedHeadBoneName,
            EBoneSpaces::WorldSpace
        );
    OutSnapshot.ChestLowerLocation =
        CharacterMesh->GetBoneLocation(
            ResolvedChestLowerBoneName,
            EBoneSpaces::WorldSpace
        );
    OutSnapshot.ChestUpperLocation =
        CharacterMesh->GetBoneLocation(
            ResolvedChestUpperBoneName,
            EBoneSpaces::WorldSpace
        );

    if (
        GetAerialContactTrackCurveTable(ActiveAerialActionType) != nullptr
    )
    {
        FVector TrackChestLower;
        FVector TrackChestUpper;
        FVector TrackHeadCenter;

        if (GetAerialContactTrackWorldLocations(
            ActiveAerialActionType,
            MontagePosition,
            GetActorForwardVector(),
            GetActorLocation(),
            TrackChestLower,
            TrackChestUpper,
            TrackHeadCenter
        ))
        {
            OutSnapshot.HeadLocation = TrackHeadCenter;
        }
    }

    SoccerPointSegmentDistanceSquared(
        OutSnapshot.BallLocation,
        OutSnapshot.ChestLowerLocation,
        OutSnapshot.ChestUpperLocation,
        &OutSnapshot.ClosestChestPoint
    );

    OutSnapshot.BallRadius = Ball->GetBallRadiusCm();
    OutSnapshot.HeadContactRadius = AerialHeadContactRadius;
    OutSnapshot.ChestContactRadius = AerialChestContactRadius;
    OutSnapshot.ExtraTolerance = AerialContactExtraTolerance;
    OutSnapshot.BallToHeadDistance = FVector::Dist(
        OutSnapshot.BallLocation,
        OutSnapshot.HeadLocation
    );
    OutSnapshot.BallToChestDistance = FVector::Dist(
        OutSnapshot.BallLocation,
        OutSnapshot.ClosestChestPoint
    );

    return true;
}

bool ASoccerCharacterBase::DebugQueueAerialPlan(
    const FSoccerAerialInterceptionPlan& Plan,
    ESoccerAerialActionIntent Intent,
    bool bAllowApproach,
    bool bLockPlan
)
{
    CancelAerialAction();
    bAerialDebugPlanLocked = bLockPlan;

    const bool bQueued = QueueAerialPlan(
        Plan,
        Intent,
        bAllowApproach
    );

    if (!bQueued)
    {
        bAerialDebugPlanLocked = false;
    }

    return bQueued;
}

void ASoccerCharacterBase::DebugSetForcedAerialContactSurface(
    ESoccerAerialContactSurface ContactSurface
)
{
    DebugForcedAerialContactSurface = ContactSurface;
}

void ASoccerCharacterBase::DebugSetStandingControlPlanningSurface(
    ESoccerAerialContactSurface ContactSurface
)
{
    if (
        ContactSurface != ESoccerAerialContactSurface::Head &&
        ContactSurface != ESoccerAerialContactSurface::Chest
    )
    {
        DebugStandingControlPlanningSurface =
            ESoccerAerialContactSurface::None;
        return;
    }

    DebugStandingControlPlanningSurface = ContactSurface;
}

void ASoccerCharacterBase::DebugRefreshAerialContactTrackingForCurrentBallTrajectory()
{
    ASoccerBall* Ball = QueuedAerialPlan.Ball;

    if (!IsValid(Ball))
    {
        return;
    }

    const int32 CurrentRevision = Ball->GetTrajectoryRevision();

    QueuedAerialPlan.TrajectoryRevision = CurrentRevision;
    ActiveAerialContactTrajectoryRevision = CurrentRevision;

    /*
     * The test ball was stationary while the montage started and is now
     * teleported to its launch point. Reinitialize the previous-frame
     * positions so the first sweep does not span that artificial teleport.
     */
    bAerialContactTrackingInitialized = false;
    bAerialContactResolved = false;
LastAerialContactResult = FSoccerAerialContactResult();
}

void ASoccerCharacterBase::InitializeAerialContactTracking()
{
    ASoccerBall* Ball = QueuedAerialPlan.Ball;
    USkeletalMeshComponent* CharacterMesh = GetMesh();

    if (!IsValid(Ball) || CharacterMesh == nullptr)
    {
        bAerialContactTrackingInitialized = false;
        return;
    }

    FName ResolvedHeadBoneName;
    FName ResolvedChestLowerBoneName;
    FName ResolvedChestUpperBoneName;
    FString BoneResolutionFailure;

    if (
        !ResolveAerialContactBoneNames(
            ResolvedHeadBoneName,
            ResolvedChestLowerBoneName,
            ResolvedChestUpperBoneName,
            &BoneResolutionFailure
        )
    )
    {
        bAerialContactTrackingInitialized = false;

        UE_LOG(
            LogTemp,
            Error,
            TEXT("Aerial contact: %s"),
            *BoneResolutionFailure
        );

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                4.0f,
                FColor::Red,
                BoneResolutionFailure
            );
        }

        return;
    }

    UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
    UAnimMontage* Montage = GetAerialMontage(ActiveAerialActionType);

    PreviousAerialMontagePosition =
        AnimInstance != nullptr && Montage != nullptr
        ? AnimInstance->Montage_GetPosition(Montage)
        : 0.0f;

    PreviousAerialBallLocation = Ball->GetActorLocation();
    PreviousAerialChestLowerLocation = CharacterMesh->GetBoneLocation(
        ResolvedChestLowerBoneName,
        EBoneSpaces::WorldSpace
    );
    PreviousAerialChestUpperLocation = CharacterMesh->GetBoneLocation(
        ResolvedChestUpperBoneName,
        EBoneSpaces::WorldSpace
    );

    PreviousAerialHeadLocation = CharacterMesh->GetBoneLocation(
        ResolvedHeadBoneName,
        EBoneSpaces::WorldSpace
    );

    /*
     * Use the same exported head trajectory for planning and for continuous
     * contact detection. This removes the one-frame skeletal evaluation
     * difference that is especially visible near the apex of a jump header.
     */
    if (
        GetAerialContactTrackCurveTable(ActiveAerialActionType) != nullptr
    )
    {
        FVector TrackChestLower;
        FVector TrackChestUpper;
        FVector TrackHeadCenter;

        if (GetAerialContactTrackWorldLocations(
            ActiveAerialActionType,
            PreviousAerialMontagePosition,
            GetActorForwardVector(),
            GetActorLocation(),
            TrackChestLower,
            TrackChestUpper,
            TrackHeadCenter
        ))
        {
            PreviousAerialHeadLocation = TrackHeadCenter;
        }
    }

    bAerialContactTrackingInitialized = true;
}

void ASoccerCharacterBase::UpdateAerialContact(float DeltaTime)
{
    (void)DeltaTime;

    if (AerialActionPhase != ESoccerAerialActionPhase::Playing)
    {
        return;
    }

    UpdateAerialBodyContest();

    if (bAerialContactResolved)
    {
        return;
    }

    ASoccerBall* Ball = QueuedAerialPlan.Ball;
    USkeletalMeshComponent* CharacterMesh = GetMesh();
    UAnimMontage* Montage = GetAerialMontage(ActiveAerialActionType);
    UAnimInstance* AnimInstance =
        CharacterMesh != nullptr
        ? CharacterMesh->GetAnimInstance()
        : nullptr;

    if (
        !IsValid(Ball) ||
        CharacterMesh == nullptr ||
        Montage == nullptr ||
        AnimInstance == nullptr
    )
    {
        return;
    }

    if (
        ActiveAerialContactTrajectoryRevision != INDEX_NONE &&
        Ball->GetTrajectoryRevision() !=
            ActiveAerialContactTrajectoryRevision
    )
    {
        bAerialContactResolved = true;
return;
    }

    if (!bAerialContactTrackingInitialized)
    {
        InitializeAerialContactTracking();

        if (!bAerialContactTrackingInitialized)
        {
            return;
        }
    }

    const float MontagePosition =
        AnimInstance->Montage_GetPosition(Montage);

    TryResolveAerialContact(MontagePosition);

    FName ResolvedHeadBoneName;
    FName ResolvedChestLowerBoneName;
    FName ResolvedChestUpperBoneName;

    if (
        !ResolveAerialContactBoneNames(
            ResolvedHeadBoneName,
            ResolvedChestLowerBoneName,
            ResolvedChestUpperBoneName
        )
    )
    {
        bAerialContactTrackingInitialized = false;
        return;
    }

    PreviousAerialMontagePosition = MontagePosition;
    PreviousAerialBallLocation = Ball->GetActorLocation();
    PreviousAerialChestLowerLocation = CharacterMesh->GetBoneLocation(
        ResolvedChestLowerBoneName,
        EBoneSpaces::WorldSpace
    );
    PreviousAerialChestUpperLocation = CharacterMesh->GetBoneLocation(
        ResolvedChestUpperBoneName,
        EBoneSpaces::WorldSpace
    );
    PreviousAerialHeadLocation = CharacterMesh->GetBoneLocation(
        ResolvedHeadBoneName,
        EBoneSpaces::WorldSpace
    );

    if (
        GetAerialContactTrackCurveTable(ActiveAerialActionType) != nullptr
    )
    {
        FVector TrackChestLower;
        FVector TrackChestUpper;
        FVector TrackHeadCenter;

        if (GetAerialContactTrackWorldLocations(
            ActiveAerialActionType,
            MontagePosition,
            GetActorForwardVector(),
            GetActorLocation(),
            TrackChestLower,
            TrackChestUpper,
            TrackHeadCenter
        ))
        {
            PreviousAerialHeadLocation = TrackHeadCenter;
        }
    }
}

bool ASoccerCharacterBase::TryResolveAerialContact(
    float MontagePosition
)
{
    if (bAerialContactResolved)
    {
        return false;
    }

    FSoccerAerialContactCandidate Candidate;

    if (!BuildAerialContactCandidate(MontagePosition, Candidate))
    {
        return false;
    }

    int32 CompetingPlayerCount = 0;
    ASoccerCharacterBase* BestOtherCharacter = nullptr;

    if (
        !IsBestAerialContestCandidate(
            Candidate,
            CompetingPlayerCount,
            BestOtherCharacter
        )
    )
    {
        if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
        {
            DrawDebugSphere(
                GetWorld(),
                Candidate.ContactLocation,
                16.0f,
                12,
                FColor::Red,
                false,
                0.08f,
                0,
                2.0f
            );
        }

        return false;
    }

    ASoccerBall* Ball = Candidate.Ball;

    if (!IsValid(Ball))
    {
        return false;
    }

    int32 NearbyOpponentCount = 0;
    const float OpponentPressure =
        CalculateAerialOpponentPressure(
            Ball,
            NearbyOpponentCount
        );

    const float EffectiveContactQuality = FMath::Clamp(
        Candidate.ContactQuality *
            (
                1.0f -
                OpponentPressure *
                    FMath::Clamp(
                        AerialContestPressureQualityPenalty,
                        0.0f,
                        1.0f
                    )
            ),
        FMath::Clamp(
            AerialMinimumEffectiveContactQuality,
            0.0f,
            1.0f
        ),
        1.0f
    );

    const FVector IncomingVelocity = Ball->GetBallPhysicsVelocity();
    FVector OutgoingVelocity = FVector::ZeroVector;

    if (
        !ResolveAerialContactVelocity(
            Candidate.ContactSurface,
            Candidate.ContactLocation,
            IncomingVelocity,
            EffectiveContactQuality,
            OpponentPressure,
            OutgoingVelocity
        )
    )
    {
        return false;
    }

    bAerialContactResolved = true;

    FSoccerAerialContactResult ContactResult;
    ContactResult.bContactResolved = true;
    ContactResult.ActionType = ActiveAerialActionType;
    ContactResult.ContactSurface = Candidate.ContactSurface;
    ContactResult.ContactLocation = Candidate.ContactLocation;
    ContactResult.OutgoingBallVelocity = OutgoingVelocity;
    ContactResult.MontagePosition = MontagePosition;
    ContactResult.ContactQuality = EffectiveContactQuality;
    ContactResult.OpponentPressure = OpponentPressure;
    ContactResult.CompetingPlayerCount = FMath::Max(
        CompetingPlayerCount,
        NearbyOpponentCount
    );

    const bool bTouchAcceptedByRules =
        RegisterAerialTouchForRules();

    ContactResult.bTouchAcceptedByRules =
        bTouchAcceptedByRules;

    if (bTouchAcceptedByRules)
    {
        Ball->ApplyAerialContactAtLocation(
            Candidate.ContactLocation,
            OutgoingVelocity,
            AerialContactAngularVelocityRetention
        );
    }

    ResolveAerialContestBodyConsequences(Candidate);

    LastAerialContactResult = ContactResult;
    LastAerialContactWorldTime =
        GetWorld() != nullptr
        ? GetWorld()->GetTimeSeconds()
        : -1000.0f;

    DrawAerialContactDebug(ContactResult);
    OnAerialBallContactResolved(ContactResult);

    if (
        ContactResult.bTouchAcceptedByRules &&
        ContactResult.ActionType ==
            ESoccerAerialActionType::StandingControl &&
        ContactResult.ContactSurface ==
            ESoccerAerialContactSurface::Head
    )
    {
        ScheduleStandingHeaderPostContactRelease();
    }

    return true;
}

bool ASoccerCharacterBase::BuildAerialContactCandidate(
    float MontagePosition,
    FSoccerAerialContactCandidate& OutCandidate
) const
{
    OutCandidate = FSoccerAerialContactCandidate();

    if (bAerialContactResolved)
    {
        return false;
    }

    ASoccerBall* Ball = QueuedAerialPlan.Ball;
    USkeletalMeshComponent* CharacterMesh = GetMesh();

    const FSoccerAerialActionProfile* Profile =
        GetAerialProfile(ActiveAerialActionType);

    if (
        !IsValid(Ball) ||
        CharacterMesh == nullptr ||
        Profile == nullptr
    )
    {
        return false;
    }

    const bool bWindowOverlapped =
        MontagePosition >= Profile->ContactWindowStart &&
        PreviousAerialMontagePosition <= Profile->ContactWindowEnd;

    if (!bWindowOverlapped)
    {
        return false;
    }

    FName ResolvedHeadBoneName;
    FName ResolvedChestLowerBoneName;
    FName ResolvedChestUpperBoneName;

    if (
        !ResolveAerialContactBoneNames(
            ResolvedHeadBoneName,
            ResolvedChestLowerBoneName,
            ResolvedChestUpperBoneName
        )
    )
    {
        return false;
    }

    const FVector CurrentBallLocation = Ball->GetActorLocation();
    const FVector CurrentChestLowerLocation = CharacterMesh->GetBoneLocation(
        ResolvedChestLowerBoneName,
        EBoneSpaces::WorldSpace
    );
    const FVector CurrentChestUpperLocation = CharacterMesh->GetBoneLocation(
        ResolvedChestUpperBoneName,
        EBoneSpaces::WorldSpace
    );

    FVector CurrentHeadLocation = CharacterMesh->GetBoneLocation(
        ResolvedHeadBoneName,
        EBoneSpaces::WorldSpace
    );

    const bool bStandingControlAction =
        ActiveAerialActionType ==
            ESoccerAerialActionType::StandingControl;

    if (
        GetAerialContactTrackCurveTable(ActiveAerialActionType) != nullptr
    )
    {
        FVector TrackChestLower;
        FVector TrackChestUpper;
        FVector TrackHeadCenter;

        if (GetAerialContactTrackWorldLocations(
            ActiveAerialActionType,
            MontagePosition,
            GetActorForwardVector(),
            GetActorLocation(),
            TrackChestLower,
            TrackChestUpper,
            TrackHeadCenter
        ))
        {
            CurrentHeadLocation = TrackHeadCenter;
        }
    }

    const float BallRadius = FMath::Max(1.0f, Ball->GetBallRadiusCm());
    const float HeadCombinedRadius =
        BallRadius +
        FMath::Max(1.0f, AerialHeadContactRadius) *
            GetPlayerProfileAerialHeadContactRadiusMultiplier() +
        FMath::Max(0.0f, AerialContactExtraTolerance);
    const float ChestCombinedRadius =
        BallRadius +
        FMath::Max(1.0f, AerialChestContactRadius) +
        FMath::Max(0.0f, AerialContactExtraTolerance);

    float HeadAlpha = 0.0f;
    float HeadNormalizedDistance = TNumericLimits<float>::Max();
    FVector HeadContactLocation = FVector::ZeroVector;

    const bool bHeadContact = DetectAerialHeadContact(
        CurrentBallLocation,
        CurrentHeadLocation,
        HeadCombinedRadius,
        HeadAlpha,
        HeadNormalizedDistance,
        HeadContactLocation
    );

    float ChestAlpha = 0.0f;
    float ChestNormalizedDistance = TNumericLimits<float>::Max();
    FVector ChestContactLocation = FVector::ZeroVector;

    const bool bCanUseChest = bStandingControlAction;

    bool bFilteredHeadContact = bHeadContact;
    bool bFilteredChestContact =
        bCanUseChest &&
        DetectAerialChestContact(
            CurrentBallLocation,
            CurrentChestLowerLocation,
            CurrentChestUpperLocation,
            ChestCombinedRadius,
            ChestAlpha,
            ChestNormalizedDistance,
            ChestContactLocation
        );

    if (
        DebugForcedAerialContactSurface ==
        ESoccerAerialContactSurface::Chest
    )
    {
        bFilteredHeadContact = false;
    }
    else if (
        DebugForcedAerialContactSurface ==
        ESoccerAerialContactSurface::Head
    )
    {
        bFilteredChestContact = false;
    }

    /*
     * HEAD and CHEST overlap, but they must not classify the same moving
     * ball at two different instants. The previous implementation tested
     * the HEAD candidate at HeadAlpha and the CHEST candidate at ChestAlpha.
     * A descending chest-bound ball could therefore be "HEAD" early in the
     * frame and "CHEST" later in the same frame, letting the large head
     * sphere win intermittently.
     *
     * Resolve one natural surface coordinate for the whole frame instead:
     * sample the real ball path against the animated chest-anchor -> head-
     * center axis, choose the common instant of closest approach to that
     * axis, and classify that single coordinate. The intended tester target
     * is not consulted.
     */
    if (
        bCanUseChest &&
        DebugForcedAerialContactSurface ==
            ESoccerAerialContactSurface::None
    )
    {
        const int32 SurfaceDecisionSamples = FMath::Clamp(
            AerialChestSweepTemporalSamples,
            4,
            24
        );

        float BestAxisDistanceSquared = TNumericLimits<float>::Max();
        float StandingSurfaceCoordinate = 0.0f;
        bool bHasStandingSurfaceCoordinate = false;

        for (
            int32 SampleIndex = 0;
            SampleIndex < SurfaceDecisionSamples;
            ++SampleIndex
        )
        {
            const float Alpha =
                SurfaceDecisionSamples > 1
                ? static_cast<float>(SampleIndex) /
                    static_cast<float>(SurfaceDecisionSamples - 1)
                : 0.0f;

            const FVector BallAtSample = FMath::Lerp(
                PreviousAerialBallLocation,
                CurrentBallLocation,
                Alpha
            );
            const FVector ChestLowerAtSample = FMath::Lerp(
                PreviousAerialChestLowerLocation,
                CurrentChestLowerLocation,
                Alpha
            );
            const FVector ChestUpperAtSample = FMath::Lerp(
                PreviousAerialChestUpperLocation,
                CurrentChestUpperLocation,
                Alpha
            );
            const FVector HeadAtSample = FMath::Lerp(
                PreviousAerialHeadLocation,
                CurrentHeadLocation,
                Alpha
            );

            const FVector ChestAnchorAtSample = FMath::Lerp(
                ChestLowerAtSample,
                ChestUpperAtSample,
                FMath::Clamp(
                    StandingAerialChestTrackAlpha,
                    0.0f,
                    1.0f
                )
            );

            const FVector ChestToHead =
                HeadAtSample - ChestAnchorAtSample;
            const float AxisSizeSquared = ChestToHead.SizeSquared();

            if (AxisSizeSquared <= 1.0f)
            {
                continue;
            }

            const float RawCoordinate = FVector::DotProduct(
                BallAtSample - ChestAnchorAtSample,
                ChestToHead
            ) / AxisSizeSquared;

            const FVector ClosestAxisPoint =
                ChestAnchorAtSample +
                ChestToHead * FMath::Clamp(
                    RawCoordinate,
                    0.0f,
                    1.0f
                );

            const float AxisDistanceSquared = FVector::DistSquared(
                BallAtSample,
                ClosestAxisPoint
            );

            if (AxisDistanceSquared < BestAxisDistanceSquared)
            {
                BestAxisDistanceSquared = AxisDistanceSquared;
                StandingSurfaceCoordinate = RawCoordinate;
                bHasStandingSurfaceCoordinate = true;
            }
        }

        if (!bHasStandingSurfaceCoordinate)
        {
            const FVector CurrentChestAnchor = FMath::Lerp(
                CurrentChestLowerLocation,
                CurrentChestUpperLocation,
                FMath::Clamp(
                    StandingAerialChestTrackAlpha,
                    0.0f,
                    1.0f
                )
            );
            const FVector CurrentChestToHead =
                CurrentHeadLocation - CurrentChestAnchor;
            const float CurrentAxisSizeSquared =
                CurrentChestToHead.SizeSquared();

            if (CurrentAxisSizeSquared > 1.0f)
            {
                StandingSurfaceCoordinate = FVector::DotProduct(
                    CurrentBallLocation - CurrentChestAnchor,
                    CurrentChestToHead
                ) / CurrentAxisSizeSquared;
                bHasStandingSurfaceCoordinate = true;
            }
        }

        const bool bNaturalHeadZone =
            bHasStandingSurfaceCoordinate &&
            StandingSurfaceCoordinate >= FMath::Clamp(
                AerialStandingHeadZoneStartFromChestToHeadAlpha,
                0.25f,
                0.85f
            );

        if (bNaturalHeadZone)
        {
            bFilteredChestContact = false;
        }
        else
        {
            bFilteredHeadContact = false;
        }
    }

    if (!bFilteredHeadContact && !bFilteredChestContact)
    {
        if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
        {
            DrawDebugSphere(
                GetWorld(),
                CurrentHeadLocation,
                HeadCombinedRadius,
                12,
                FColor::Purple,
                false,
                0.06f,
                0,
                1.0f
            );

            if (bCanUseChest)
            {
                DrawDebugLine(
                    GetWorld(),
                    CurrentChestLowerLocation,
                    CurrentChestUpperLocation,
                    FColor::Cyan,
                    false,
                    0.06f,
                    0,
                    ChestCombinedRadius * 0.12f
                );
            }
        }

        return false;
    }

    OutCandidate.bValid = true;
    OutCandidate.Character =
        const_cast<ASoccerCharacterBase*>(this);
    OutCandidate.Ball = Ball;
    OutCandidate.ActionType = ActiveAerialActionType;
    OutCandidate.MontagePosition = MontagePosition;

    if (bFilteredHeadContact && bFilteredChestContact)
    {
        /* Only the narrow transition band can still contain both candidates. */
        if (HeadNormalizedDistance <= ChestNormalizedDistance)
        {
            OutCandidate.ContactSurface =
                ESoccerAerialContactSurface::Head;
            OutCandidate.ContactLocation = HeadContactLocation;
            OutCandidate.NormalizedContactDistance =
                HeadNormalizedDistance;
        }
        else
        {
            OutCandidate.ContactSurface =
                ESoccerAerialContactSurface::Chest;
            OutCandidate.ContactLocation = ChestContactLocation;
            OutCandidate.NormalizedContactDistance =
                ChestNormalizedDistance;
        }
    }
    else if (bFilteredChestContact)
    {
        OutCandidate.ContactSurface =
            ESoccerAerialContactSurface::Chest;
        OutCandidate.ContactLocation = ChestContactLocation;
        OutCandidate.NormalizedContactDistance =
            ChestNormalizedDistance;
    }
    else
    {
        OutCandidate.ContactSurface =
            ESoccerAerialContactSurface::Head;
        OutCandidate.ContactLocation = HeadContactLocation;
        OutCandidate.NormalizedContactDistance =
            HeadNormalizedDistance;
    }

    OutCandidate.SpatialQuality = FMath::Clamp(
        1.0f - OutCandidate.NormalizedContactDistance,
        0.0f,
        1.0f
    );
    OutCandidate.AirborneCommitment =
        GetAerialAirborneCommitment(MontagePosition);
    OutCandidate.ContactQuality = FMath::Clamp(
        CalculateAerialContactQuality(OutCandidate) *
            GetPlayerProfileAerialContactQualityMultiplier(
                OutCandidate.ContactSurface
            ),
        0.0f,
        1.0f
    );

    float ActionBonus = 0.0f;

    if (
        ActiveAerialActionType ==
        ESoccerAerialActionType::JumpHeaderKick
    )
    {
        ActionBonus = FMath::Max(
            0.0f,
            AerialContestActiveHeaderBonus
        );
    }
    else if (
        ActiveAerialActionType ==
        ESoccerAerialActionType::JumpHeaderBlock
    )
    {
        ActionBonus = FMath::Max(
            0.0f,
            AerialContestBlockHeaderBonus
        );
    }

    OutCandidate.ContestScore =
        OutCandidate.ContactQuality +
        ActionBonus +
        GetPlayerProfileAerialAbilityContestScoreAdjustment() +
        GetPlayerProfileStrengthAerialContestScoreAdjustment();

    return true;
}

float ASoccerCharacterBase::CalculateAerialContactQuality(
    FSoccerAerialContactCandidate& Candidate
) const
{
    const FSoccerAerialActionProfile* Profile =
        GetAerialProfile(Candidate.ActionType);

    if (Profile == nullptr)
    {
        return 0.0f;
    }

    const float TimingSpan = FMath::Max(
        0.01f,
        FMath::Max(
            Profile->IdealContactTime - Profile->ContactWindowStart,
            Profile->ContactWindowEnd - Profile->IdealContactTime
        )
    );

    const float TimingQuality = FMath::Clamp(
        1.0f -
        FMath::Abs(
            Candidate.MontagePosition -
            Profile->IdealContactTime
        ) / TimingSpan,
        0.0f,
        1.0f
    );

    FVector IncomingDirection = FVector::ZeroVector;

    if (IsValid(Candidate.Ball))
    {
        IncomingDirection =
            -Candidate.Ball->GetBallPhysicsVelocity();
        IncomingDirection.Z = 0.0f;
        IncomingDirection.Normalize();
    }

    FVector Forward = GetActorForwardVector();
    Forward.Z = 0.0f;
    Forward.Normalize();

    const float FacingQuality =
        IncomingDirection.IsNearlyZero() || Forward.IsNearlyZero()
        ? 1.0f
        : FMath::Clamp(
            FVector::DotProduct(Forward, IncomingDirection),
            0.0f,
            1.0f
        );

    float IncomingSpeed = 0.0f;

    if (IsValid(Candidate.Ball))
    {
        IncomingSpeed =
            Candidate.Ball->GetBallPhysicsVelocity().Size();
    }

    const float ComfortableSpeed = FMath::Max(
        0.0f,
        AerialComfortableIncomingSpeed
    );
    const float ExtremeSpeed = FMath::Max(
        ComfortableSpeed + 1.0f,
        AerialExtremeIncomingSpeed
    );
    const float SpeedDifficultyAlpha = FMath::Clamp(
        (IncomingSpeed - ComfortableSpeed) /
            (ExtremeSpeed - ComfortableSpeed),
        0.0f,
        1.0f
    );
    const float SpeedQuality = FMath::Lerp(
        1.0f,
        FMath::Clamp(
            AerialExtremeSpeedMinimumQuality,
            0.0f,
            1.0f
        ),
        SpeedDifficultyAlpha
    );

    const bool bUseAirborneWeight =
        IsJumpAerialAction(Candidate.ActionType);

    const float SpatialWeight =
        FMath::Max(0.0f, AerialContestSpatialWeight);
    const float TimingWeight =
        FMath::Max(0.0f, AerialContestTimingWeight);
    const float FacingWeight =
        FMath::Max(0.0f, AerialContestFacingWeight);
    const float SpeedWeight =
        FMath::Max(0.0f, AerialContestSpeedWeight);
    const float AirborneWeight =
        bUseAirborneWeight
        ? FMath::Max(0.0f, AerialContestAirborneWeight)
        : 0.0f;

    const float TotalWeight = FMath::Max(
        KINDA_SMALL_NUMBER,
        SpatialWeight +
        TimingWeight +
        FacingWeight +
        SpeedWeight +
        AirborneWeight
    );

    const float Quality =
        (
            Candidate.SpatialQuality * SpatialWeight +
            TimingQuality * TimingWeight +
            FacingQuality * FacingWeight +
            SpeedQuality * SpeedWeight +
            Candidate.AirborneCommitment * AirborneWeight
        ) / TotalWeight;

    Candidate.TimingQuality = TimingQuality;
    Candidate.FacingQuality = FacingQuality;
    Candidate.SpeedQuality = SpeedQuality;

    return FMath::Clamp(Quality, 0.0f, 1.0f);
}

float ASoccerCharacterBase::CalculateAerialOpponentPressure(
    ASoccerBall* Ball,
    int32& OutNearbyOpponentCount
) const
{
    OutNearbyOpponentCount = 0;

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return 0.0f;
    }

    const float SafeRadius = FMath::Max(
        1.0f,
        AerialContestPressureRadius
    );
    const float SafeVerticalTolerance = FMath::Max(
        0.0f,
        AerialContestPressureVerticalTolerance
    );

    float Pressure = 0.0f;

    for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
    {
        ASoccerCharacterBase* Other = *It;

        if (
            !IsValid(Other) ||
            Other == this ||
            Other->GetTeam() == GetTeam()
        )
        {
            continue;
        }

        const FVector Offset =
            Other->GetActorLocation() - GetActorLocation();

        if (FMath::Abs(Offset.Z) > SafeVerticalTolerance)
        {
            continue;
        }

        const float Distance = Offset.Size2D();

        if (Distance > SafeRadius)
        {
            continue;
        }

        ++OutNearbyOpponentCount;

        float Contribution = FMath::Clamp(
            1.0f - Distance / SafeRadius,
            0.0f,
            1.0f
        ) * 0.65f;

        if (
            IsValid(Ball) &&
            Other->IsAerialActionQueuedOrPlaying() &&
            Other->GetAerialActionBall() == Ball
        )
        {
            Contribution += FMath::Clamp(
                AerialContestActiveOpponentPressureBonus,
                0.0f,
                1.0f
            );
        }

        Contribution = FMath::Clamp(Contribution, 0.0f, 1.0f);
        Pressure = 1.0f - (1.0f - Pressure) * (1.0f - Contribution);
    }

    return FMath::Clamp(Pressure, 0.0f, 1.0f);
}

bool ASoccerCharacterBase::IsBestAerialContestCandidate(
    const FSoccerAerialContactCandidate& Candidate,
    int32& OutCompetingPlayerCount,
    ASoccerCharacterBase*& OutBestOtherCharacter
) const
{
    OutCompetingPlayerCount = 0;
    OutBestOtherCharacter = nullptr;

    UWorld* World = GetWorld();

    if (
        World == nullptr ||
        !Candidate.bValid ||
        !IsValid(Candidate.Ball)
    )
    {
        return Candidate.bValid;
    }

    const ASoccerCharacterBase* BestCharacter = this;
    float BestScore = Candidate.ContestScore;

    for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
    {
        ASoccerCharacterBase* Other = *It;

        if (
            !IsValid(Other) ||
            Other == this ||
            Other->AerialActionPhase !=
                ESoccerAerialActionPhase::Playing ||
            Other->bAerialContactResolved ||
            Other->QueuedAerialPlan.Ball != Candidate.Ball
        )
        {
            continue;
        }

        USkeletalMeshComponent* OtherMesh = Other->GetMesh();
        UAnimMontage* OtherMontage =
            Other->GetAerialMontage(
                Other->ActiveAerialActionType
            );
        UAnimInstance* OtherAnimInstance =
            OtherMesh != nullptr
            ? OtherMesh->GetAnimInstance()
            : nullptr;

        if (OtherMontage == nullptr || OtherAnimInstance == nullptr)
        {
            continue;
        }

        const float OtherMontagePosition =
            OtherAnimInstance->Montage_GetPosition(OtherMontage);

        FSoccerAerialContactCandidate OtherCandidate;

        if (
            !Other->BuildAerialContactCandidate(
                OtherMontagePosition,
                OtherCandidate
            )
        )
        {
            continue;
        }

        ++OutCompetingPlayerCount;

        const bool bClearlyBetter =
            OtherCandidate.ContestScore >
            BestScore + FMath::Max(0.0f, AerialContestTieTolerance);

        const bool bTieWithDeterministicPriority =
            FMath::Abs(OtherCandidate.ContestScore - BestScore) <=
                FMath::Max(0.0f, AerialContestTieTolerance) &&
            Other->GetUniqueID() < BestCharacter->GetUniqueID();

        if (bClearlyBetter || bTieWithDeterministicPriority)
        {
            BestCharacter = Other;
            BestScore = OtherCandidate.ContestScore;
        }
    }

    if (BestCharacter != this)
    {
        OutBestOtherCharacter =
            const_cast<ASoccerCharacterBase*>(BestCharacter);
        return false;
    }

    return true;
}

void ASoccerCharacterBase::ResolveAerialContestBodyConsequences(
    const FSoccerAerialContactCandidate& WinningCandidate
)
{
    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    const float SafeRadius = FMath::Max(
        1.0f,
        AerialBodyContestRadius
    );

    for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
    {
        ASoccerCharacterBase* Other = *It;
        const TWeakObjectPtr<ASoccerCharacterBase> OtherWeak(Other);

        if (
            !IsValid(Other) ||
            Other == this ||
            Other->AerialActionPhase !=
                ESoccerAerialActionPhase::Playing ||
            Other->QueuedAerialPlan.Ball != WinningCandidate.Ball
        )
        {
            continue;
        }

        FVector SeparationDirection =
            Other->GetActorLocation() - GetActorLocation();
        SeparationDirection.Z = 0.0f;

        const float Distance = SeparationDirection.Size();

        if (Distance > SafeRadius)
        {
            continue;
        }

        if (SeparationDirection.IsNearlyZero())
        {
            SeparationDirection =
                GetUniqueID() < Other->GetUniqueID()
                ? GetActorRightVector()
                : -GetActorRightVector();
        }

        SeparationDirection.Normalize();

        Other->bAerialContactResolved = true;

        if (Other->GetTeam() == GetTeam())
        {
            continue;
        }

        const bool bBodyReactionAlreadyProcessed =
            AerialBodyContactProcessedOpponents.Contains(OtherWeak);

        AerialBodyContactProcessedOpponents.Add(OtherWeak);
        Other->AerialBodyContactProcessedOpponents.Add(
            TWeakObjectPtr<ASoccerCharacterBase>(this)
        );


        if (bBodyReactionAlreadyProcessed)
        {
            continue;
        }

        const float WinnerCommitment =
            WinningCandidate.AirborneCommitment;

        float OtherMontagePosition =
            Other->PreviousAerialMontagePosition;

        if (USkeletalMeshComponent* OtherMesh = Other->GetMesh())
        {
            if (UAnimInstance* OtherAnim = OtherMesh->GetAnimInstance())
            {
                if (
                    UAnimMontage* OtherMontage =
                        Other->GetAerialMontage(
                            Other->ActiveAerialActionType
                        )
                )
                {
                    OtherMontagePosition =
                        OtherAnim->Montage_GetPosition(OtherMontage);
                }
            }
        }

        const float OtherCommitment =
            Other->GetAerialAirborneCommitment(
                OtherMontagePosition
            );

        const float Difference =
            WinnerCommitment - OtherCommitment;

        const float LoserStrength = FMath::Clamp(
            0.80f + Difference * 0.40f,
            0.45f,
            1.20f
        );

        Other->ReceiveAerialContestBodyReaction(
            this,
            SeparationDirection,
            LoserStrength
        );

        ReceiveAerialContestBodyReaction(
            Other,
            -SeparationDirection,
            FMath::Clamp(
                AerialProtectedPlayerReactionScale,
                0.0f,
                1.0f
            )
        );
    }
}

void ASoccerCharacterBase::UpdateAerialBodyContest()
{
    if (
        AerialActionPhase != ESoccerAerialActionPhase::Playing ||
        !IsJumpAerialAction(ActiveAerialActionType)
    )
    {
        return;
    }

    UWorld* World = GetWorld();
    UAnimMontage* Montage = GetAerialMontage(ActiveAerialActionType);
    UAnimInstance* AnimInstance =
        GetMesh() != nullptr
        ? GetMesh()->GetAnimInstance()
        : nullptr;

    if (World == nullptr || Montage == nullptr || AnimInstance == nullptr)
    {
        return;
    }

    const float MontagePosition =
        AnimInstance->Montage_GetPosition(Montage);
    const float ThisCommitment =
        GetAerialAirborneCommitment(MontagePosition);

    if (ThisCommitment <= 0.05f)
    {
        return;
    }

    const float SafeRadius = FMath::Max(
        1.0f,
        AerialBodyContestRadius
    );

    for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
    {
        ASoccerCharacterBase* Other = *It;

        const TWeakObjectPtr<ASoccerCharacterBase> OtherWeak(Other);

        if (
            !IsValid(Other) ||
            Other == this ||
            Other->GetTeam() == GetTeam() ||
            Other->AerialActionPhase !=
                ESoccerAerialActionPhase::Playing ||
            !Other->IsJumpAerialAction(
                Other->ActiveAerialActionType
            ) ||
            Other->QueuedAerialPlan.Ball != QueuedAerialPlan.Ball ||
            GetUniqueID() > Other->GetUniqueID() ||
            AerialBodyContactProcessedOpponents.Contains(OtherWeak)
        )
        {
            continue;
        }

        FVector SeparationDirection =
            Other->GetActorLocation() - GetActorLocation();
        SeparationDirection.Z = 0.0f;

        if (SeparationDirection.Size() > SafeRadius)
        {
            continue;
        }

        UAnimMontage* OtherMontage =
            Other->GetAerialMontage(
                Other->ActiveAerialActionType
            );
        UAnimInstance* OtherAnimInstance =
            Other->GetMesh() != nullptr
            ? Other->GetMesh()->GetAnimInstance()
            : nullptr;

        if (OtherMontage == nullptr || OtherAnimInstance == nullptr)
        {
            continue;
        }

        const float OtherPosition =
            OtherAnimInstance->Montage_GetPosition(OtherMontage);
        const float OtherCommitment =
            Other->GetAerialAirborneCommitment(OtherPosition);

        if (OtherCommitment <= 0.05f)
        {
            continue;
        }

        AerialBodyContactProcessedOpponents.Add(OtherWeak);
        Other->AerialBodyContactProcessedOpponents.Add(
            TWeakObjectPtr<ASoccerCharacterBase>(this)
        );

        if (SeparationDirection.IsNearlyZero())
        {
            SeparationDirection = GetActorRightVector();
        }

        SeparationDirection.Normalize();

        const float CommitmentDifference =
            ThisCommitment - OtherCommitment;

        if (CommitmentDifference > 0.12f)
        {
            ReceiveAerialContestBodyReaction(
                Other,
                -SeparationDirection,
                AerialProtectedPlayerReactionScale
            );
            Other->ReceiveAerialContestBodyReaction(
                this,
                SeparationDirection,
                1.0f
            );
        }
        else if (CommitmentDifference < -0.12f)
        {
            ReceiveAerialContestBodyReaction(
                Other,
                -SeparationDirection,
                1.0f
            );
            Other->ReceiveAerialContestBodyReaction(
                this,
                SeparationDirection,
                AerialProtectedPlayerReactionScale
            );
        }
        else
        {
            const float SimultaneousScale = FMath::Clamp(
                AerialSimultaneousBodyReactionScale,
                0.0f,
                1.0f
            );

            ReceiveAerialContestBodyReaction(
                Other,
                -SeparationDirection,
                SimultaneousScale
            );
            Other->ReceiveAerialContestBodyReaction(
                this,
                SeparationDirection,
                SimultaneousScale
            );
        }
    }
}

void ASoccerCharacterBase::ReceiveAerialContestBodyReaction(
    ASoccerCharacterBase* OtherCharacter,
    const FVector& SeparationDirection,
    float StrengthMultiplier
)
{
    FVector SafeDirection = SeparationDirection;
    SafeDirection.Z = 0.0f;
    SafeDirection.Normalize();

    if (SafeDirection.IsNearlyZero())
    {
        return;
    }

    const float SourceForceMultiplier =
        IsValid(OtherCharacter)
        ? OtherCharacter->GetPlayerProfileStrengthBodyForceMultiplier()
        : 1.0f;
    const float ReceiverResistanceMultiplier =
        GetPlayerProfileStrengthBodyResistanceMultiplier();
    const float SafeStrength = FMath::Max(
        0.0f,
        StrengthMultiplier *
            SourceForceMultiplier *
            ReceiverResistanceMultiplier
    );

    FVector AddedImpulse =
        SafeDirection *
        FMath::Max(0.0f, AerialBodyContestHorizontalImpulse) *
        SafeStrength;
    AddedImpulse.Z =
        FMath::Max(0.0f, AerialBodyContestUpwardImpulse) *
        SafeStrength;

    PendingAerialContestExitImpulse += AddedImpulse;

    FVector HorizontalImpulse = PendingAerialContestExitImpulse;
    HorizontalImpulse.Z = 0.0f;
    HorizontalImpulse = HorizontalImpulse.GetClampedToMaxSize(
        FMath::Max(
            1.0f,
            AerialBodyContestHorizontalImpulse * 1.5f
        )
    );

    PendingAerialContestExitImpulse.X = HorizontalImpulse.X;
    PendingAerialContestExitImpulse.Y = HorizontalImpulse.Y;
    PendingAerialContestExitImpulse.Z = FMath::Min(
        PendingAerialContestExitImpulse.Z,
        FMath::Max(
            0.0f,
            AerialBodyContestUpwardImpulse * 1.5f
        )
    );

    if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial) && GetWorld() != nullptr)
    {
        const FVector DebugStart =
            GetActorLocation() + FVector(0.0f, 0.0f, 110.0f);

        DrawDebugDirectionalArrow(
            GetWorld(),
            DebugStart,
            DebugStart + SafeDirection * 95.0f,
            18.0f,
            FColor::Magenta,
            false,
            0.35f,
            0,
            2.0f
        );
    }
}

float ASoccerCharacterBase::GetAerialAirborneCommitment(
    float MontagePosition
) const
{
    const FSoccerAerialActionProfile* Profile =
        GetAerialProfile(ActiveAerialActionType);

    if (
        Profile == nullptr ||
        !IsJumpAerialAction(ActiveAerialActionType) ||
        Profile->LandingTime <= Profile->TakeoffTime ||
        MontagePosition < Profile->TakeoffTime ||
        MontagePosition > Profile->LandingTime
    )
    {
        return 0.0f;
    }

    const float SafeApex = FMath::Clamp(
        Profile->ApexTime,
        Profile->TakeoffTime,
        Profile->LandingTime
    );

    if (MontagePosition <= SafeApex)
    {
        const float Alpha =
            (MontagePosition - Profile->TakeoffTime) /
            FMath::Max(0.01f, SafeApex - Profile->TakeoffTime);

        return FMath::Lerp(
            0.35f,
            1.0f,
            FMath::Clamp(Alpha, 0.0f, 1.0f)
        );
    }

    const float Alpha =
        (MontagePosition - SafeApex) /
        FMath::Max(0.01f, Profile->LandingTime - SafeApex);

    return FMath::Lerp(
        1.0f,
        0.25f,
        FMath::Clamp(Alpha, 0.0f, 1.0f)
    );
}

bool ASoccerCharacterBase::IsJumpAerialAction(
    ESoccerAerialActionType ActionType
) const
{
    return
        ActionType == ESoccerAerialActionType::JumpHeaderKick ||
        ActionType == ESoccerAerialActionType::JumpHeaderBlock;
}


bool ASoccerCharacterBase::DetectAerialHeadContact(
    const FVector& CurrentBallLocation,
    const FVector& CurrentHeadLocation,
    float CombinedRadius,
    float& OutContactAlpha,
    float& OutNormalizedDistance,
    FVector& OutContactLocation
) const
{
    FVector BallClosestLocation;
    FVector HeadClosestLocation;

    const float DistanceSquared =
        SoccerMovingPointClosestDistanceSquared(
            PreviousAerialBallLocation,
            CurrentBallLocation,
            PreviousAerialHeadLocation,
            CurrentHeadLocation,
            OutContactAlpha,
            BallClosestLocation,
            HeadClosestLocation
        );

    const float SafeCombinedRadius =
        FMath::Max(1.0f, CombinedRadius);

    OutNormalizedDistance =
        FMath::Sqrt(FMath::Max(0.0f, DistanceSquared)) /
        SafeCombinedRadius;

    /* The contact location represents the ball center at impact. */
    OutContactLocation = BallClosestLocation;

    return DistanceSquared <= FMath::Square(SafeCombinedRadius);
}

bool ASoccerCharacterBase::DetectAerialChestContact(
    const FVector& CurrentBallLocation,
    const FVector& CurrentChestLowerLocation,
    const FVector& CurrentChestUpperLocation,
    float CombinedRadius,
    float& OutContactAlpha,
    float& OutNormalizedDistance,
    FVector& OutContactLocation
) const
{
    const float SafeCombinedRadius =
        FMath::Max(1.0f, CombinedRadius);

    float BestDistanceSquared = TNumericLimits<float>::Max();
    float BestBallAlpha = 0.0f;
    FVector BestBallLocation = PreviousAerialBallLocation;
    FVector BestChestLocation = PreviousAerialChestLowerLocation;

    auto ConsiderSegmentPair =
        [&BestDistanceSquared,
         &BestBallAlpha,
         &BestBallLocation,
         &BestChestLocation](
            const FVector& BallStart,
            const FVector& BallEnd,
            const FVector& ChestStart,
            const FVector& ChestEnd
        )
        {
            float BallAlpha = 0.0f;
            float ChestAlpha = 0.0f;
            FVector ClosestBall;
            FVector ClosestChest;

            const float DistanceSquared =
                SoccerSegmentSegmentDistanceSquared(
                    BallStart,
                    BallEnd,
                    ChestStart,
                    ChestEnd,
                    BallAlpha,
                    ChestAlpha,
                    ClosestBall,
                    ClosestChest
                );

            if (DistanceSquared < BestDistanceSquared)
            {
                BestDistanceSquared = DistanceSquared;
                BestBallAlpha = BallAlpha;
                BestBallLocation = ClosestBall;
                BestChestLocation = ClosestChest;
            }
        };

    const FVector PreviousChestMiddle =
        (PreviousAerialChestLowerLocation +
         PreviousAerialChestUpperLocation) * 0.5f;
    const FVector CurrentChestMiddle =
        (CurrentChestLowerLocation +
         CurrentChestUpperLocation) * 0.5f;

    /*
     * Continuous conservative sweep of the moving ball segment against the
     * chest capsule at both ends of the frame and against the motion of its
     * lower, upper and center lines. Unlike temporal point sampling, this
     * cannot skip the whole torso when a fast ball crosses it in one frame.
     */
    ConsiderSegmentPair(
        PreviousAerialBallLocation,
        CurrentBallLocation,
        PreviousAerialChestLowerLocation,
        PreviousAerialChestUpperLocation
    );
    ConsiderSegmentPair(
        PreviousAerialBallLocation,
        CurrentBallLocation,
        CurrentChestLowerLocation,
        CurrentChestUpperLocation
    );
    ConsiderSegmentPair(
        PreviousAerialBallLocation,
        CurrentBallLocation,
        PreviousAerialChestLowerLocation,
        CurrentChestLowerLocation
    );
    ConsiderSegmentPair(
        PreviousAerialBallLocation,
        CurrentBallLocation,
        PreviousAerialChestUpperLocation,
        CurrentChestUpperLocation
    );
    ConsiderSegmentPair(
        PreviousAerialBallLocation,
        CurrentBallLocation,
        PreviousChestMiddle,
        CurrentChestMiddle
    );

    /* Keep synchronized samples as a second check for the deforming axis. */
    const int32 TemporalSamples = FMath::Clamp(
        AerialChestSweepTemporalSamples,
        2,
        24
    );

    for (int32 SampleIndex = 0; SampleIndex < TemporalSamples; ++SampleIndex)
    {
        const float Alpha =
            TemporalSamples > 1
            ? static_cast<float>(SampleIndex) /
                static_cast<float>(TemporalSamples - 1)
            : 0.0f;

        const FVector BallLocation = FMath::Lerp(
            PreviousAerialBallLocation,
            CurrentBallLocation,
            Alpha
        );
        const FVector ChestLowerLocation = FMath::Lerp(
            PreviousAerialChestLowerLocation,
            CurrentChestLowerLocation,
            Alpha
        );
        const FVector ChestUpperLocation = FMath::Lerp(
            PreviousAerialChestUpperLocation,
            CurrentChestUpperLocation,
            Alpha
        );

        FVector ClosestChestLocation;
        const float DistanceSquared =
            SoccerPointSegmentDistanceSquared(
                BallLocation,
                ChestLowerLocation,
                ChestUpperLocation,
                &ClosestChestLocation
            );

        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestBallAlpha = Alpha;
            BestBallLocation = BallLocation;
            BestChestLocation = ClosestChestLocation;
        }
    }

    OutContactAlpha = BestBallAlpha;
    OutNormalizedDistance =
        FMath::Sqrt(FMath::Max(0.0f, BestDistanceSquared)) /
        SafeCombinedRadius;

    /*
     * BestBallLocation is the closest point reached during the frame, not
     * necessarily the first point at which the ball touched the chest
     * volume. With Pawn collisions intentionally ignored, rewinding to that
     * closest point can place the ball inside or even behind the torso.
     *
     * Rebuild a safe ball-center location on the FRONT hemisphere of the
     * animated chest capsule. The character is already oriented toward the
     * incoming ball by the aerial plan, so CharacterForward is the side on
     * which a chest control must leave the ball.
     */
    const float ContactSurfaceRadius = FMath::Max(
        1.0f,
        SafeCombinedRadius - FMath::Max(0.0f, AerialContactExtraTolerance)
    );

    FVector CharacterForward = GetActorForwardVector();
    CharacterForward.Z = 0.0f;
    CharacterForward = CharacterForward.GetSafeNormal();

    if (CharacterForward.IsNearlyZero())
    {
        CharacterForward = FVector::ForwardVector;
    }

    FVector CharacterRight = GetActorRightVector();
    CharacterRight.Z = 0.0f;
    CharacterRight = CharacterRight.GetSafeNormal();

    if (CharacterRight.IsNearlyZero())
    {
        CharacterRight = FVector::RightVector;
    }

    const FVector ChestToClosestBall =
        BestBallLocation - BestChestLocation;

    float LateralOffset = FVector::DotProduct(
        ChestToClosestBall,
        CharacterRight
    );
    float VerticalOffset = FVector::DotProduct(
        ChestToClosestBall,
        FVector::UpVector
    );

    const float MaximumTangentialOffset =
        ContactSurfaceRadius * 0.92f;
    const float TangentialSize = FMath::Sqrt(
        LateralOffset * LateralOffset +
        VerticalOffset * VerticalOffset
    );

    if (
        TangentialSize > MaximumTangentialOffset &&
        TangentialSize > KINDA_SMALL_NUMBER
    )
    {
        const float TangentialScale =
            MaximumTangentialOffset / TangentialSize;
        LateralOffset *= TangentialScale;
        VerticalOffset *= TangentialScale;
    }

    const float TangentialSquared =
        LateralOffset * LateralOffset +
        VerticalOffset * VerticalOffset;
    const float ForwardOffset = FMath::Sqrt(
        FMath::Max(
            0.0f,
            ContactSurfaceRadius * ContactSurfaceRadius -
            TangentialSquared
        )
    );

    OutContactLocation =
        BestChestLocation +
        CharacterForward * ForwardOffset +
        CharacterRight * LateralOffset +
        FVector::UpVector * VerticalOffset;

    return BestDistanceSquared <= FMath::Square(SafeCombinedRadius);
}

bool ASoccerCharacterBase::ResolveAerialContactVelocity(
    ESoccerAerialContactSurface ContactSurface,
    const FVector& ContactLocation,
    const FVector& IncomingVelocity,
    float ContactQuality,
    float OpponentPressure,
    FVector& OutVelocity
) const
{
    OutVelocity = FVector::ZeroVector;

    const float Quality = FMath::Clamp(
        ContactQuality,
        FMath::Clamp(
            AerialMinimumEffectiveContactQuality,
            0.0f,
            1.0f
        ),
        1.0f
    );
    const float Pressure = FMath::Clamp(
        OpponentPressure,
        0.0f,
        1.0f
    );

    FVector CharacterForward = GetActorForwardVector();
    CharacterForward.Z = 0.0f;
    CharacterForward = CharacterForward.GetSafeNormal();

    if (CharacterForward.IsNearlyZero())
    {
        CharacterForward = FVector::ForwardVector;
    }

    FVector IncomingHorizontalDirection = IncomingVelocity;
    IncomingHorizontalDirection.Z = 0.0f;
    IncomingHorizontalDirection =
        IncomingHorizontalDirection.GetSafeNormal();

    if (IncomingHorizontalDirection.IsNearlyZero())
    {
        IncomingHorizontalDirection = CharacterForward;
    }

    const float IncomingHorizontalSpeed = IncomingVelocity.Size2D();

    switch (ActiveAerialActionType)
    {
    case ESoccerAerialActionType::JumpHeaderKick:
    {
        FVector TargetLocation;
        FVector DesiredDirection = CharacterForward;

        if (ResolveAerialActiveHeaderTarget(TargetLocation))
        {
            DesiredDirection = TargetLocation - ContactLocation;
            DesiredDirection.Z = 0.0f;
            DesiredDirection = DesiredDirection.GetSafeNormal();
        }

        DesiredDirection = SoccerClampHorizontalDirectionToYaw(
            CharacterForward,
            DesiredDirection,
            AerialActiveHeaderMaximumRedirectAngle
        );

        /*
         * A poor or heavily pressured contact cannot redirect the ball as
         * accurately as an ideal forehead contact. It stays closer to the
         * direction in which the body is facing.
         */
        FVector EffectiveDirection = FMath::Lerp(
            CharacterForward,
            DesiredDirection,
            Quality
        ).GetSafeNormal();

        const float OverrideSpeed =
            ResolveAerialActiveHeaderSpeedOverride();

        const float BaseHorizontalSpeed =
            (
                OverrideSpeed >= 0.0f
                ? OverrideSpeed
                : AerialActiveHeaderHorizontalSpeed
            ) * GetPlayerProfileAerialHeaderPowerMultiplier();

        const float QualityPowerScale = FMath::Lerp(
            FMath::Clamp(AerialPoorHeaderPowerScale, 0.0f, 1.0f),
            1.0f,
            Quality
        );
        const float PressurePowerScale = FMath::Lerp(
            1.0f,
            0.88f,
            Pressure
        );

        OutVelocity =
            EffectiveDirection *
            FMath::Max(
                0.0f,
                BaseHorizontalSpeed *
                QualityPowerScale *
                PressurePowerScale
            );
        OutVelocity.Z =
            AerialActiveHeaderUpwardSpeed *
            FMath::Lerp(0.72f, 1.0f, Quality);
        return !OutVelocity.IsNearlyZero();
    }

    case ESoccerAerialActionType::JumpHeaderBlock:
    {
        const float OverrideSpeed =
            ResolveAerialDefensiveBlockSpeedOverride();
        const float BaseRetention =
            FMath::Max(0.0f, AerialBlockIncomingSpeedRetention);
        const float EffectiveRetention =
            BaseRetention * FMath::Lerp(0.72f, 1.0f, Quality);

        const float RetainedHorizontalSpeed = FMath::Clamp(
            IncomingHorizontalSpeed * EffectiveRetention,
            FMath::Max(0.0f, AerialBlockMinimumHorizontalSpeed),
            FMath::Max(
                AerialBlockMinimumHorizontalSpeed,
                AerialBlockMaximumHorizontalSpeed
            )
        );
        const float HorizontalSpeed =
            (
                OverrideSpeed >= 0.0f
                ? OverrideSpeed
                : RetainedHorizontalSpeed
            ) * GetPlayerProfileAerialHeaderPowerMultiplier();

        FVector DesiredDirection = CharacterForward;
        FVector TargetLocation = FVector::ZeroVector;

        if (ResolveAerialDefensiveBlockTarget(TargetLocation))
        {
            DesiredDirection = TargetLocation - ContactLocation;
            DesiredDirection.Z = 0.0f;
            DesiredDirection = DesiredDirection.GetSafeNormal();
        }

        if (DesiredDirection.IsNearlyZero())
        {
            DesiredDirection = CharacterForward;
        }

        DesiredDirection = SoccerClampHorizontalDirectionToYaw(
            CharacterForward,
            DesiredDirection,
            AerialBlockMaximumRedirectAngle
        );

        /*
         * A defensive header is deliberately directional, but pressure or
         * imperfect forehead contact keeps part of the natural facing line.
         */
        FVector BlockDirection = FMath::Lerp(
            CharacterForward,
            DesiredDirection,
            FMath::Lerp(0.55f, 1.0f, Quality) *
                FMath::Lerp(1.0f, 0.82f, Pressure)
        ).GetSafeNormal();

        if (BlockDirection.IsNearlyZero())
        {
            BlockDirection = CharacterForward;
        }

        OutVelocity =
            BlockDirection *
            FMath::Max(
                0.0f,
                HorizontalSpeed *
                    FMath::Lerp(0.76f, 1.0f, Quality) *
                    FMath::Lerp(1.0f, 0.90f, Pressure)
            );
        OutVelocity.Z = FMath::Max(
            AerialBlockUpwardSpeed *
                FMath::Lerp(0.75f, 1.0f, Quality),
            FMath::Max(0.0f, -IncomingVelocity.Z) * 0.18f
        );
        return !OutVelocity.IsNearlyZero();
    }

    case ESoccerAerialActionType::StandingControl:
    {
        if (ContactSurface == ESoccerAerialContactSurface::Chest)
        {
            const float Retention = FMath::Lerp(
                FMath::Clamp(
                    AerialPoorControlHorizontalRetention,
                    0.0f,
                    1.0f
                ),
                FMath::Clamp(
                    AerialChestHorizontalRetention,
                    0.0f,
                    1.0f
                ),
                Quality
            );

            const float MaximumSpeed =
                FMath::Max(
                    AerialChestMinimumHorizontalSpeed,
                    AerialChestMaximumHorizontalSpeed
                ) * FMath::Lerp(1.55f, 1.0f, Quality);

            const float HorizontalSpeed = FMath::Clamp(
                IncomingHorizontalSpeed * Retention,
                FMath::Max(0.0f, AerialChestMinimumHorizontalSpeed),
                FMath::Max(0.0f, MaximumSpeed)
            );

            /*
             * IncomingHorizontalDirection normally points THROUGH the player
             * while CharacterForward points toward the side from which the
             * ball arrived. Blending those opposite vectors made low-quality
             * controls continue behind the torso. A chest control may be poor
             * or lateral, but it must never preserve a penetrating component.
             */
            FVector CharacterRight = GetActorRightVector();
            CharacterRight.Z = 0.0f;
            CharacterRight = CharacterRight.GetSafeNormal();

            if (CharacterRight.IsNearlyZero())
            {
                CharacterRight = FVector::RightVector;
            }

            const float IncomingLateralSpeed = FVector::DotProduct(
                IncomingVelocity,
                CharacterRight
            );
            const float LateralRetention = FMath::Lerp(
                0.38f,
                0.10f,
                Quality
            );

            FVector ChestHorizontalVelocity =
                CharacterForward * HorizontalSpeed +
                CharacterRight * IncomingLateralSpeed * LateralRetention;

            const float ChestHorizontalSize =
                ChestHorizontalVelocity.Size2D();

            if (ChestHorizontalSize > MaximumSpeed)
            {
                ChestHorizontalVelocity =
                    ChestHorizontalVelocity.GetSafeNormal2D() * MaximumSpeed;
            }

            /* Final guard: never allow the ball to leave behind the chest. */
            const float ForwardExitSpeed = FVector::DotProduct(
                ChestHorizontalVelocity,
                CharacterForward
            );

            if (ForwardExitSpeed < AerialChestMinimumHorizontalSpeed)
            {
                ChestHorizontalVelocity +=
                    CharacterForward *
                    (
                        AerialChestMinimumHorizontalSpeed -
                        ForwardExitSpeed
                    );
            }

            OutVelocity = ChestHorizontalVelocity;
            OutVelocity.Z = FMath::Lerp(
                AerialPoorControlVerticalSpeed,
                AerialChestVerticalSpeed,
                Quality
            );
            return true;
        }

        FVector RedirectTarget =
            ContactLocation +
            CharacterForward *
                FMath::Max(100.0f, AerialStandingHeaderTargetDistance);

        if (
            QueuedAerialPlan.bHasStandingHeaderRedirectTarget &&
            QueuedAerialPlan.PlannedContactSurface ==
                ESoccerAerialContactSurface::Head
        )
        {
            RedirectTarget =
                QueuedAerialPlan.StandingHeaderRedirectTarget;
        }

        OutVelocity = CalculatePassiveStandingHeaderVelocity(
            IncomingVelocity,
            ContactLocation,
            CharacterForward,
            RedirectTarget,
            Quality,
            Pressure
        );

        return !OutVelocity.IsNearlyZero();
    }

    case ESoccerAerialActionType::None:
    default:
        return false;
    }
}

bool ASoccerCharacterBase::RegisterAerialTouchForRules() const
{
    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return true;
    }

    for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
    {
        ASoccerMatchManager* MatchManager = *It;

        if (IsValid(MatchManager))
        {
            return MatchManager->TryRegisterIntentionalBallTouch(
                const_cast<ASoccerCharacterBase*>(this)
            );
        }
    }

    return true;
}

bool ASoccerCharacterBase::ResolveAerialActiveHeaderTarget(
    FVector& OutTargetLocation
) const
{
    OutTargetLocation =
        GetActorLocation() +
        GetActorForwardVector().GetSafeNormal2D() * 1000.0f;

    return true;
}

bool ASoccerCharacterBase::ResolveAerialDefensiveBlockTarget(
    FVector& OutTargetLocation
) const
{
    OutTargetLocation =
        GetActorLocation() +
        GetActorForwardVector().GetSafeNormal2D() * 1000.0f;

    return true;
}

bool ASoccerCharacterBase::ResolveAerialStandingHeaderRedirectTarget(
    FVector& OutTargetLocation
) const
{
    OutTargetLocation = FVector::ZeroVector;
    return false;
}

float ASoccerCharacterBase::ResolveAerialActiveHeaderSpeedOverride() const
{
    return -1.0f;
}

float ASoccerCharacterBase::ResolveAerialDefensiveBlockSpeedOverride() const
{
    return -1.0f;
}

void ASoccerCharacterBase::OnAerialBallContactResolved(
    const FSoccerAerialContactResult& ContactResult
)
{
    (void)ContactResult;
}

void ASoccerCharacterBase::DrawAerialContactDebug(
    const FSoccerAerialContactResult& ContactResult
) const
{
    if (!ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial) || GetWorld() == nullptr)
    {
        return;
    }

    const FColor ContactColor =
        ContactResult.ContactSurface ==
            ESoccerAerialContactSurface::Chest
        ? FColor::Blue
        : FColor::Green;

    DrawDebugSphere(
        GetWorld(),
        ContactResult.ContactLocation,
        18.0f,
        16,
        ContactColor,
        false,
        1.2f,
        0,
        2.5f
    );

    DrawDebugDirectionalArrow(
        GetWorld(),
        ContactResult.ContactLocation,
        ContactResult.ContactLocation +
            ContactResult.OutgoingBallVelocity.GetSafeNormal() * 150.0f,
        24.0f,
        FColor::Yellow,
        false,
        1.2f,
        0,
        2.5f
    );

    DrawDebugString(
        GetWorld(),
        ContactResult.ContactLocation + FVector(0.0f, 0.0f, 35.0f),
        FString::Printf(
            TEXT("Aerial | action %d | surface %d | t %.2f | q %.2f | pressure %.2f | rivals %d | out %.0f"),
            static_cast<int32>(ContactResult.ActionType),
            static_cast<int32>(ContactResult.ContactSurface),
            ContactResult.MontagePosition,
            ContactResult.ContactQuality,
            ContactResult.OpponentPressure,
            ContactResult.CompetingPlayerCount,
            ContactResult.OutgoingBallVelocity.Size()
        ),
        nullptr,
        FColor::White,
        1.2f,
        false
    );
}

void ASoccerCharacterBase::UpdateAerialDebugAutoStart(float DeltaTime)
{
    if (!bDebugAutoStartAerialAction || !ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
    {
        AerialDebugAutoStartAccumulator = 0.0f;
        return;
    }

    if (IsAerialActionQueuedOrPlaying())
    {
        return;
    }

    AerialDebugAutoStartAccumulator += DeltaTime;

    if (
        AerialDebugAutoStartAccumulator <
        FMath::Max(0.02f, AerialDebugAutoStartRefreshInterval)
    )
    {
        return;
    }

    AerialDebugAutoStartAccumulator = 0.0f;

    ASoccerBall* Ball = ResolveDebugInterceptionBall();

    if (!IsValid(Ball))
    {
        return;
    }

    TryStartBestAerialActionApproach(Ball, DebugAerialActionIntent);
}

void ASoccerCharacterBase::DrawAerialPlanDebug(
    const FSoccerAerialInterceptionPlan& Plan,
    const FColor& Color
) const
{
    UWorld* World = GetWorld();

    if (World == nullptr || !Plan.bValid)
    {
        return;
    }

    const float Duration = 0.14f;

    DrawDebugSphere(
        World,
        Plan.ContactLocation,
        16.0f,
        12,
        Color,
        false,
        Duration,
        0,
        2.0f
    );

    DrawDebugCylinder(
        World,
        Plan.PreparationLocation,
        Plan.PreparationLocation + FVector(0.0f, 0.0f, 90.0f),
        24.0f,
        12,
        FColor::Blue,
        false,
        Duration,
        0,
        1.5f
    );

    DrawDebugLine(
        World,
        Plan.PreparationLocation + FVector(0.0f, 0.0f, 45.0f),
        Plan.ContactLocation,
        Color,
        false,
        Duration,
        0,
        1.5f
    );

    DrawDebugDirectionalArrow(
        World,
        Plan.PreparationLocation + FVector(0.0f, 0.0f, 100.0f),
        Plan.PreparationLocation + FVector(0.0f, 0.0f, 100.0f) +
            Plan.FacingDirection.GetSafeNormal2D() * 80.0f,
        18.0f,
        FColor::White,
        false,
        Duration,
        0,
        1.5f
    );

    DrawDebugString(
        World,
        Plan.ContactLocation + FVector(0.0f, 0.0f, 25.0f),
        FString::Printf(
            TEXT("Aerial %d | surface %d | ball %.2fs | start %.2fs | contact %.2fs | margin %+.2fs | dIdeal %+.2fs | zErr %.1fcm | height %.0fcm"),
            static_cast<int32>(Plan.ActionType),
            static_cast<int32>(Plan.PlannedContactSurface),
            Plan.BallArrivalTime,
            Plan.MontageStartDelay,
            Plan.ExpectedContactTimeFromMontageStart,
            Plan.PreparationArrivalMargin,
            Plan.ContactTimingOffsetFromIdeal,
            Plan.PredictedHeadVerticalError,
            Plan.BallHeightAboveGround
        ),
        nullptr,
        Color,
        Duration,
        false,
        1.0f
    );
}
