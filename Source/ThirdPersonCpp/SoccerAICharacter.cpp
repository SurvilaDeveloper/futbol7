//SoccerAICharacter.cpp

#include "SoccerAICharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "TimerManager.h"
#include "Engine/CurveTable.h"
#include "Curves/RealCurve.h"
#include "DrawDebugHelpers.h"
#include "SoccerMatchManager.h"
#include "SoccerPlayerProfile.h"
#include "EngineUtils.h"

ASoccerAICharacter::ASoccerAICharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CurrentAIMovementMode =
		ESoccerAIMovementMode::Run;

	CurrentAIMovementReason =
		ESoccerAIMovementReason::NormalPlay;

	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	bUseControllerRotationYaw = false;
}

bool ASoccerAICharacter::StartGoalkeeperRetreatBackpedalAnimation()
{
	if (
		GetPlayerRole() != ESoccerPlayerRole::Goalkeeper ||
		GoalkeeperRetreatBackpedalAnimation == nullptr ||
		GetMesh() == nullptr
		)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (AnimInstance == nullptr)
	{
		return false;
	}

	if (
		ActiveGoalkeeperRetreatBackpedalMontage != nullptr &&
		AnimInstance->Montage_IsPlaying(
			ActiveGoalkeeperRetreatBackpedalMontage
		)
		)
	{
		return true;
	}

	ActiveGoalkeeperRetreatBackpedalMontage =
		AnimInstance->PlaySlotAnimationAsDynamicMontage(
			GoalkeeperRetreatBackpedalAnimation,
			GoalkeeperRetreatBackpedalSlotName,
			FMath::Max(0.0f, GoalkeeperRetreatBackpedalBlendInTime),
			FMath::Max(0.0f, GoalkeeperRetreatBackpedalBlendOutTime),
			FMath::Max(0.10f, GoalkeeperRetreatBackpedalPlayRate),
			FMath::Max(1, GoalkeeperRetreatBackpedalLoopCount),
			-1.0f,
			0.0f
		);

	return ActiveGoalkeeperRetreatBackpedalMontage != nullptr;
}

void ASoccerAICharacter::StopGoalkeeperRetreatBackpedalAnimation(
	float BlendOutTime
)
{
	if (ActiveGoalkeeperRetreatBackpedalMontage == nullptr)
	{
		return;
	}

	if (GetMesh() != nullptr)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			const float SafeBlendOutTime =
				BlendOutTime >= 0.0f
				? BlendOutTime
				: GoalkeeperRetreatBackpedalBlendOutTime;

			// Do not gate this with Montage_IsActive(). Dynamic montages can be
			// transitioning through the slot while that query is already false.
			// In that case clearing our pointer without issuing Montage_Stop()
			// leaves the backpedal pose alive until another montage replaces it.
			AnimInstance->Montage_Stop(
				FMath::Max(0.0f, SafeBlendOutTime),
				ActiveGoalkeeperRetreatBackpedalMontage
			);
		}
	}

	ActiveGoalkeeperRetreatBackpedalMontage = nullptr;
}

void ASoccerAICharacter::UpdateGoalkeeperRetreatBackpedalAnimationFromVelocity()
{
	if (
		GetPlayerRole() != ESoccerPlayerRole::Goalkeeper ||
		GoalkeeperRetreatBackpedalAnimation == nullptr ||
		bGoalkeeperActionActive ||
		bAIIsPossessingBall ||
		bAIIsKickingForAnimation
		)
	{
		StopGoalkeeperRetreatBackpedalAnimation();
		return;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();

	if (Movement == nullptr)
	{
		StopGoalkeeperRetreatBackpedalAnimation();
		return;
	}

	FVector HorizontalVelocity = Movement->Velocity;
	HorizontalVelocity.Z = 0.0f;

	FVector ForwardDirection = GetActorForwardVector();
	ForwardDirection.Z = 0.0f;

	if (!ForwardDirection.Normalize())
	{
		StopGoalkeeperRetreatBackpedalAnimation();
		return;
	}

	const float ForwardSpeed = FVector::DotProduct(
		HorizontalVelocity,
		ForwardDirection
	);

	const float SafeStartSpeed =
		FMath::Max(1.0f, GoalkeeperBackpedalStartSpeed);

	const float SafeStopSpeed =
		FMath::Clamp(
			GoalkeeperBackpedalStopSpeed,
			0.0f,
			SafeStartSpeed
		);

	// La referencia del montage es nuestra fuente de verdad. No usamos
	// Montage_IsPlaying() para decidir el stop porque un Dynamic Montage
	// puede seguir afectando el slot durante una transicion aunque esa
	// consulta ya haya cambiado de estado.
	const bool bBackpedalMontageTracked =
		ActiveGoalkeeperRetreatBackpedalMontage != nullptr;

	if (!bBackpedalMontageTracked)
	{
		if (ForwardSpeed <= -SafeStartSpeed)
		{
			StartGoalkeeperRetreatBackpedalAnimation();
		}

		return;
	}

	if (ForwardSpeed >= -SafeStopSpeed)
	{
		StopGoalkeeperRetreatBackpedalAnimation();
	}
}

void ASoccerAICharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyPlayerProfilePhysicalTuning();

	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;

	if (GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetCharacterMovement()->bUseControllerDesiredRotation = false;

		bUseControllerRotationYaw = false;
	}
	else
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
		GetCharacterMovement()->bUseControllerDesiredRotation = false;

		bUseControllerRotationYaw = false;
	}
	RequestAIMovementMode(
		ESoccerAIMovementMode::Run,
		ESoccerAIMovementReason::NormalPlay,
		true
	);
}

void ASoccerAICharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bAIKickMontageActive && ShouldCancelPendingAIKickAnimation())
	{
		CancelPendingAIKickAnimation(true);
	}

	if (IsAerialActionLocked() || IsAerialActionWaitingToStart())
	{
		StopGoalkeeperRetreatBackpedalAnimation();

		ScriptedLocomotionVelocity = FVector::ZeroVector;
		bScriptedLocomotionVelocityActive = false;

		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}

		UpdateAIPlayerEnergy(DeltaTime);
		return;
	}

	if (bAIIsPossessingBall && !IsValid(ControlledBall))
	{
		bAIIsPossessingBall = false;
		bGoalkeeperHoldingBall = false;
		ControlledBall = nullptr;

		RequestAIMovementReevaluation();
	}

	const bool bShouldUpdatePossessedBallLocation =
		bAIIsPossessingBall &&
		IsValid(ControlledBall) &&
		!bGoalkeeperHoldingBall &&
		(
			bAIPossessionCarryActive ||
			(
				bAIDribbleTurnAutoPassActive &&
				!bAIDribbleTurnAutoPassHasImpactedBall
			)
		);

	if (bShouldUpdatePossessedBallLocation)
	{
		UpdateAIPossessedBallLocation();
	}

	UpdateAIDribbleTurnActorRotation(DeltaTime);

	UpdateAIPlayerEnergy(DeltaTime);
	UpdateAIMovementSpeed();

	UpdateGoalkeeperSaveCurveMotion();
	UpdateGoalkeeperActionState();
	UpdateGoalkeeperRetreatBackpedalAnimationFromVelocity();
}

AActor* ASoccerAICharacter::GetHomePositionActor() const
{
	return HomePositionActor;
}

bool ASoccerAICharacter::ResolveAerialActiveHeaderTarget(
	FVector& OutTargetLocation
) const
{
	if (
		bHasAIAerialHeaderTarget &&
		(
			AIAerialHeaderTactic ==
				ESoccerAIAerialHeaderTactic::ShotAtGoal ||
			AIAerialHeaderTactic ==
				ESoccerAIAerialHeaderTactic::PassToTeammate ||
			AIAerialHeaderTactic ==
				ESoccerAIAerialHeaderTactic::ProlongAttack
		)
	)
	{
		OutTargetLocation = AIAerialHeaderTargetLocation;
		return true;
	}

	UWorld* World = GetWorld();

	if (World != nullptr)
	{
		for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
		{
			ASoccerMatchManager* MatchManager = *It;

			if (IsValid(MatchManager))
			{
				OutTargetLocation =
					MatchManager->GetShotTargetLocation(this);
				return true;
			}
		}
	}

	return Super::ResolveAerialActiveHeaderTarget(
		OutTargetLocation
	);
}

bool ASoccerAICharacter::ResolveAerialDefensiveBlockTarget(
	FVector& OutTargetLocation
) const
{
	if (
		bHasAIAerialHeaderTarget &&
		(
			AIAerialHeaderTactic ==
				ESoccerAIAerialHeaderTactic::DefensiveClearance ||
			AIAerialHeaderTactic ==
				ESoccerAIAerialHeaderTactic::DefensivePass
		)
	)
	{
		OutTargetLocation = AIAerialHeaderTargetLocation;
		return true;
	}

	return Super::ResolveAerialDefensiveBlockTarget(
		OutTargetLocation
	);
}

float ASoccerAICharacter::ResolveAerialActiveHeaderSpeedOverride() const
{
	if (
		bHasAIAerialHeaderTarget &&
		(
			AIAerialHeaderTactic ==
				ESoccerAIAerialHeaderTactic::ShotAtGoal ||
			AIAerialHeaderTactic ==
				ESoccerAIAerialHeaderTactic::PassToTeammate ||
			AIAerialHeaderTactic ==
				ESoccerAIAerialHeaderTactic::ProlongAttack
		)
	)
	{
		return AIAerialHeaderHorizontalSpeedOverride;
	}

	return Super::ResolveAerialActiveHeaderSpeedOverride();
}

float ASoccerAICharacter::ResolveAerialDefensiveBlockSpeedOverride() const
{
	if (
		bHasAIAerialHeaderTarget &&
		(
			AIAerialHeaderTactic ==
				ESoccerAIAerialHeaderTactic::DefensiveClearance ||
			AIAerialHeaderTactic ==
				ESoccerAIAerialHeaderTactic::DefensivePass
		)
	)
	{
		return AIAerialHeaderHorizontalSpeedOverride;
	}

	return Super::ResolveAerialDefensiveBlockSpeedOverride();
}

void ASoccerAICharacter::OnAerialBallContactResolved(
	const FSoccerAerialContactResult& ContactResult
)
{
	Super::OnAerialBallContactResolved(ContactResult);
	ClearAIAerialHeaderDecision();
}

void ASoccerAICharacter::ConfigureAIAerialHeaderDecision(
	ESoccerAIAerialHeaderTactic NewTactic,
	const FVector& TargetLocation,
	float HorizontalSpeedOverride
)
{
	AIAerialHeaderTactic = NewTactic;
	AIAerialHeaderTargetLocation = TargetLocation;
	AIAerialHeaderHorizontalSpeedOverride = HorizontalSpeedOverride;
	bHasAIAerialHeaderTarget =
		NewTactic != ESoccerAIAerialHeaderTactic::None &&
		!TargetLocation.ContainsNaN();
}

void ASoccerAICharacter::ClearAIAerialHeaderDecision()
{
	AIAerialHeaderTactic = ESoccerAIAerialHeaderTactic::None;
	bHasAIAerialHeaderTarget = false;
	AIAerialHeaderTargetLocation = FVector::ZeroVector;
	AIAerialHeaderHorizontalSpeedOverride = -1.0f;
}

bool ASoccerAICharacter::HasAIAerialHeaderTarget() const
{
	return bHasAIAerialHeaderTarget;
}

FVector ASoccerAICharacter::GetAIAerialHeaderTargetLocation() const
{
	return bHasAIAerialHeaderTarget
		? AIAerialHeaderTargetLocation
		: FVector::ZeroVector;
}

FVector ASoccerAICharacter::GetVelocity() const
{
	if (bScriptedLocomotionVelocityActive)
	{
		return ScriptedLocomotionVelocity;
	}

	return Super::GetVelocity();
}

void ASoccerAICharacter::SetScriptedLocomotionVelocity(
	const FVector& WorldVelocity,
	ESoccerAIMovementMode MovementMode,
	ESoccerAIMovementReason MovementReason
)
{
	ScriptedLocomotionVelocity = WorldVelocity;
	ScriptedLocomotionVelocity.Z = 0.0f;

	bScriptedLocomotionVelocityActive =
		!ScriptedLocomotionVelocity.IsNearlyZero();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->Velocity = ScriptedLocomotionVelocity;
	}

	if (bScriptedLocomotionVelocityActive)
	{
		RequestAIMovementMode(
			MovementMode,
			MovementReason,
			true
		);
	}
}

void ASoccerAICharacter::ClearScriptedLocomotionVelocity()
{
	bScriptedLocomotionVelocityActive = false;
	ScriptedLocomotionVelocity = FVector::ZeroVector;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->Velocity = FVector::ZeroVector;
	}

	RequestAIMovementReevaluation();
}

void ASoccerAICharacter::SetAIPossessionCarryActive(bool bNewActive)
{
	const bool bCanCarryBall =
		bNewActive &&
		bAIIsPossessingBall &&
		!bGoalkeeperHoldingBall &&
		IsValid(ControlledBall);

	if (bAIPossessionCarryActive == bCanCarryBall)
	{
		return;
	}

	bAIPossessionCarryActive = bCanCarryBall;

	if (!IsValid(ControlledBall))
	{
		return;
	}

	if (bAIPossessionCarryActive)
	{
		// La pelota acompana al jugador, pero sigue siendo alcanzable
		// por la logica normal de robo basada en distancia y angulo.
		ControlledBall->SetPossessed(true);
		UpdateAIPossessedBallLocation();
		return;
	}

	if (
		bAIIsPossessingBall &&
		!bGoalkeeperHoldingBall &&
		!bAIDribbleTurnAutoPassActive
		)
	{
		ControlledBall->SetPossessed(false);
		ControlledBall->StopBallKeepingPhysics();
	}
}

void ASoccerAICharacter::SetAIChasingBall(bool bNewAIChasingBall)
{
	if (bAIIsChasingBall == bNewAIChasingBall)
	{
		return;
	}

	bAIIsChasingBall = bNewAIChasingBall;

	if (bAIIsChasingBall)
	{
		RequestAIMovementMode(
			ESoccerAIMovementMode::FastRun,
			ESoccerAIMovementReason::FreeBallRace,
			true
		);
	}
	else
	{
		RequestAIMovementReevaluation();
	}
}

void ASoccerAICharacter::UpdateSoccerAnimationState()
{
	Super::UpdateSoccerAnimationState();

	UWorld* World = GetWorld();

	const float CurrentTime =
		World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	if (bAIIsKickingForAnimation && CurrentTime >= AIKickingAnimationEndTime)
	{
		bAIIsKickingForAnimation = false;
	}

	const bool bShouldForceTurnLocomotion =
		bAIDribbleTurnAutoPassActive;

	const float ForcedTurnSpeed =
		bAIDribbleTurnAutoPassActive
		? ActiveAIDribbleTurnForcedLocomotionSpeed
		: 0.0f;

	SetSoccerAnimationState(
		bAIIsPossessingBall || bAIDribbleTurnAutoPassActive,
		bAIIsChasingBall,
		bAIIsKickingForAnimation &&
		!bAIDribbleTurnAutoPassActive &&
		!bGoalkeeperActionActive,
		GetAIPlayerEnergyPercent(),
		bShouldForceTurnLocomotion,
		ForcedTurnSpeed
	);
}

void ASoccerAICharacter::PossessAIBall(ASoccerBall* NewControlledBall)
{
	if (!IsValid(NewControlledBall))
	{
		return;
	}

	// A new possession invalidates a kick that was still waiting for impact.
	// This prevents an old timer from kicking a ball after control changed.
	if (bAIKickMontageActive)
	{
		CancelPendingAIKickAnimation(true);
	}

	// Toda posesi�n nueva comienza como posesi�n normal.
	// HoldGoalkeeperBallInHands() la marcar� despu�s
	// expl�citamente como pelota sostenida.
	bGoalkeeperHoldingBall = false;

	ControlledBall = NewControlledBall;
	bAIIsPossessingBall = true;
	bAIIsChasingBall = false;
	bAIPossessionCarryActive = false;

	LastAIPossessionStartTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	AIDribbleTurnCurrentDirection =
		GetActorForwardVector();

	AIDribbleTurnCurrentDirection.Z = 0.0f;
	AIDribbleTurnCurrentDirection =
		AIDribbleTurnCurrentDirection.GetSafeNormal();

	// El bot tiene posesi�n l�gica, pero no conduce la pelota.
	// La pelota queda quieta/f�sica hasta que el bot haga pase, autopase o remate.
	ControlledBall->SetPossessed(false);
	ControlledBall->StopBallKeepingPhysics();

	RequestAIMovementReevaluation();
}

void ASoccerAICharacter::ReleaseAIBall()
{
	// External possession cleanup (restart, reset, etc.) must also erase a
	// delayed kick. The normal montage-start path calls ReleaseAIBall before
	// bAIKickMontageActive is set, so it does not enter this branch.
	if (bAIKickMontageActive)
	{
		CancelPendingAIKickAnimation(true);
	}

	const bool bHadPossession =
		bAIIsPossessingBall;

	ASoccerBall* BallToRelease =
		ControlledBall;

	if (IsValid(BallToRelease))
	{
		const bool bWasAttachedToThisCharacter =
			BallToRelease->GetAttachParentActor() == this;

		if (
			bGoalkeeperHoldingBall ||
			bWasAttachedToThisCharacter
			)
		{
			BallToRelease->DetachFromActor(
				FDetachmentTransformRules::
				KeepWorldTransform
			);
		}

		// Mientras estaba en las manos qued� sin colisi�n.
		BallToRelease->SetActorEnableCollision(true);

		// Reactiva f�sica y gravedad.
		BallToRelease->SetPossessed(false);
	}

	ControlledBall = nullptr;
	bAIIsPossessingBall = false;
	bGoalkeeperHoldingBall = false;
	bAIPossessionCarryActive = false;

	if (bHadPossession)
	{
		LastAIBallReleasedTime =
			GetWorld() != nullptr
			? GetWorld()->GetTimeSeconds()
			: 0.0f;
	}

	RequestAIMovementReevaluation();
}

bool ASoccerAICharacter::IsAIPossessingBall() const
{
	return bAIIsPossessingBall;
}

bool ASoccerAICharacter::IsGoalkeeperHoldingBall() const
{
	return
		bGoalkeeperHoldingBall &&
		bAIIsPossessingBall &&
		IsValid(ControlledBall);
}

ASoccerBall* ASoccerAICharacter::GetControlledAIBall() const
{
	return ControlledBall;
}

float ASoccerAICharacter::PlayGoalkeeperDistributionMontage(
	ESoccerGoalkeeperDistributionType DistributionType
)
{
	if (
		GetPlayerRole() != ESoccerPlayerRole::Goalkeeper ||
		DistributionType ==
		ESoccerGoalkeeperDistributionType::None
		)
	{
		return 0.0f;
	}

	UAnimMontage* MontageToPlay =
		GetGoalkeeperDistributionMontage(
			DistributionType
		);

	if (MontageToPlay == nullptr)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.0f,
				FColor::Red,
				TEXT(
					"GK distribucion: montage no asignado"
				)
			);
		}

		return 0.0f;
	}

	if (GetMesh() == nullptr)
	{
		return 0.0f;
	}

	UAnimInstance* AnimInstance =
		GetMesh()->GetAnimInstance();

	if (AnimInstance == nullptr)
	{
		return 0.0f;
	}

	StopGoalkeeperRetreatBackpedalAnimation(0.05f);

	const float MontageDuration =
		AnimInstance->Montage_Play(
			MontageToPlay,
			1.0f
		);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.2f,
			MontageDuration > 0.0f
			? FColor::Green
			: FColor::Red,
			FString::Printf(
				TEXT(
					"GK distribucion montage: %s | %.2f s"
				),
				*MontageToPlay->GetName(),
				MontageDuration
			)
		);
	}

	return MontageDuration;
}

bool ASoccerAICharacter::
GetGoalkeeperDistributionMontagePlaybackState(
	ESoccerGoalkeeperDistributionType DistributionType,
	float& OutMontagePosition,
	float& OutMontageLength
) const
{
	OutMontagePosition = 0.0f;
	OutMontageLength = 0.0f;

	if (GetMesh() == nullptr)
	{
		return false;
	}

	UAnimMontage* DistributionMontage =
		GetGoalkeeperDistributionMontage(
			DistributionType
		);

	if (DistributionMontage == nullptr)
	{
		return false;
	}

	UAnimInstance* AnimInstance =
		GetMesh()->GetAnimInstance();

	if (AnimInstance == nullptr)
	{
		return false;
	}

	if (
		!AnimInstance->Montage_IsActive(
			DistributionMontage
		)
		)
	{
		return false;
	}

	OutMontagePosition =
		AnimInstance->Montage_GetPosition(
			DistributionMontage
		);

	OutMontageLength =
		DistributionMontage->GetPlayLength();

	return OutMontageLength > KINDA_SMALL_NUMBER;
}

bool ASoccerAICharacter::
MoveGoalkeeperDistributionByWorldDelta(
	const FVector& WorldDelta
)
{
	FVector HorizontalDelta =
		WorldDelta;

	HorizontalDelta.Z = 0.0f;

	if (HorizontalDelta.IsNearlyZero())
	{
		return true;
	}

	FHitResult MovementHit;

	AddActorWorldOffset(
		HorizontalDelta,
		true,
		&MovementHit,
		ETeleportType::None
	);

	return !MovementHit.bStartPenetrating;
}

bool ASoccerAICharacter::HoldThrowInBall(
	ASoccerBall* SoccerBall
)
{
	if (!IsValid(SoccerBall) || GetMesh() == nullptr)
	{
		return false;
	}

	if (
		ThrowInBallHoldSocketName.IsNone() ||
		!GetMesh()->DoesSocketExist(ThrowInBallHoldSocketName)
		)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				3.0f,
				FColor::Red,
				FString::Printf(
					TEXT("Throw in: no existe socket/hueso %s"),
					*ThrowInBallHoldSocketName.ToString()
				)
			);
		}

		return false;
	}

	if (
		bAIIsPossessingBall &&
		IsValid(ControlledBall) &&
		ControlledBall != SoccerBall
		)
	{
		ReleaseAIBall();
	}

	PossessAIBall(SoccerBall);

	if (!bAIIsPossessingBall || ControlledBall != SoccerBall)
	{
		return false;
	}

	ControlledBall->SetPossessed(true);
	ControlledBall->SetActorEnableCollision(false);

	ControlledBall->AttachToComponent(
		GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		ThrowInBallHoldSocketName
	);

	const bool bAttached =
		ControlledBall->GetAttachParentActor() == this;

	if (!bAttached)
	{
		ControlledBall->SetActorEnableCollision(true);
		ReleaseAIBall();
		return false;
	}

	RequestAIMovementReevaluation();
	return true;
}

float ASoccerAICharacter::PlayThrowInMontage()
{
	if (ThrowInMontage == nullptr || GetMesh() == nullptr)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.5f,
				FColor::Red,
				TEXT("Throw in: montage no asignado")
			);
		}

		return 0.0f;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (AnimInstance == nullptr)
	{
		return 0.0f;
	}

	return AnimInstance->Montage_Play(ThrowInMontage, 1.0f);
}

bool ASoccerAICharacter::GetThrowInMontagePlaybackState(
	float& OutMontagePosition,
	float& OutMontageLength
) const
{
	OutMontagePosition = 0.0f;
	OutMontageLength = 0.0f;

	if (
		ThrowInMontage == nullptr ||
		GetMesh() == nullptr
		)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	if (
		AnimInstance == nullptr ||
		!AnimInstance->Montage_IsActive(ThrowInMontage)
		)
	{
		return false;
	}

	OutMontagePosition =
		AnimInstance->Montage_GetPosition(ThrowInMontage);

	OutMontageLength = ThrowInMontage->GetPlayLength();

	return OutMontageLength > KINDA_SMALL_NUMBER;
}

bool ASoccerAICharacter::MoveThrowInByWorldDelta(
	const FVector& WorldDelta
)
{
	FVector HorizontalDelta = WorldDelta;
	HorizontalDelta.Z = 0.0f;

	if (HorizontalDelta.IsNearlyZero())
	{
		return true;
	}

	FHitResult MovementHit;

	AddActorWorldOffset(
		HorizontalDelta,
		true,
		&MovementHit,
		ETeleportType::None
	);

	return !MovementHit.bStartPenetrating;
}

bool ASoccerAICharacter::ReleaseHeldThrowInBallToAirTarget(
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime
)
{
	if (!bAIIsPossessingBall || !IsValid(ControlledBall))
	{
		return false;
	}

	ASoccerBall* BallToRelease = ControlledBall;
	ReleaseAIBall();

	if (!IsValid(BallToRelease))
	{
		return false;
	}

	BallToRelease->KickToAirTarget(
		TargetLocation,
		HorizontalSpeed,
		MinTravelTime,
		MaxTravelTime
	);

	return true;
}

bool ASoccerAICharacter::ReleaseHeldGoalkeeperBallToAirTarget(
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime
)
{
	if (!IsGoalkeeperHoldingBall())
	{
		return false;
	}

	if (!IsValid(ControlledBall))
	{
		return false;
	}

	ASoccerBall* BallToRelease =
		ControlledBall;

	ReleaseAIBall();

	if (!IsValid(BallToRelease))
	{
		return false;
	}

	BallToRelease->KickToAirTarget(
		TargetLocation,
		HorizontalSpeed,
		MinTravelTime,
		MaxTravelTime
	);

	return true;
}

bool ASoccerAICharacter::
ReleaseHeldGoalkeeperBallForDrop(
	float ForwardSpeed,
	float LateralSpeed,
	float UpwardSpeed
)
{
	if (!IsGoalkeeperHoldingBall())
	{
		return false;
	}

	if (!IsValid(ControlledBall))
	{
		return false;
	}

	ASoccerBall* BallToDrop =
		ControlledBall;

	// Eje X local del personaje:
	// hacia donde mira el arquero.
	FVector ForwardDirection =
		GetActorForwardVector();

	ForwardDirection.Z = 0.0f;
	ForwardDirection =
		ForwardDirection.GetSafeNormal();

	// Eje Y local del personaje:
	// positivo hacia la derecha del arquero.
	FVector RightDirection =
		GetActorRightVector();

	RightDirection.Z = 0.0f;
	RightDirection =
		RightDirection.GetSafeNormal();

	if (
		ForwardDirection.IsNearlyZero() ||
		RightDirection.IsNearlyZero()
		)
	{
		return false;
	}

	// Primero desadjuntamos la pelota y reactivamos
	// colisi�n, gravedad y simulaci�n f�sica.
	ReleaseAIBall();

	if (!IsValid(BallToDrop))
	{
		return false;
	}

	// Borra cualquier velocidad residual producida
	// por el socket o por el movimiento del personaje.
	BallToDrop->StopBallKeepingPhysics();

	const float SafeForwardSpeed =
		FMath::Max(
			0.0f,
			ForwardSpeed
		);

	// No se limita a valores positivos:
	// negativo significa izquierda.
	const float SafeLateralSpeed =
		LateralSpeed;

	const float SafeUpwardSpeed =
		FMath::Max(
			0.0f,
			UpwardSpeed
		);

	// Construimos la velocidad horizontal completa
	// en los ejes locales del arquero.
	const FVector HorizontalVelocity =
		ForwardDirection * SafeForwardSpeed +
		RightDirection * SafeLateralSpeed;

	const float HorizontalSpeed =
		HorizontalVelocity.Size();

	const FVector HorizontalDirection =
		HorizontalVelocity.GetSafeNormal();

	if (
		HorizontalSpeed > KINDA_SMALL_NUMBER ||
		SafeUpwardSpeed > KINDA_SMALL_NUMBER
		)
	{
		BallToDrop->Kick(
			HorizontalDirection,
			HorizontalSpeed,
			SafeUpwardSpeed
		);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Yellow,
			FString::Printf(
				TEXT(
					"GK Drop Release | Forward %.0f | Lateral %.0f | Up %.0f"
				),
				SafeForwardSpeed,
				SafeLateralSpeed,
				SafeUpwardSpeed
			)
		);
	}

	return true;
}

bool ASoccerAICharacter::
PlaceHeldGoalkeeperBallForDistribution()
{
	if (!IsGoalkeeperHoldingBall())
	{
		return false;
	}

	if (!IsValid(ControlledBall))
	{
		return false;
	}

	ASoccerBall* BallToPlace =
		ControlledBall;

	// ReleaseAIBall usa KeepWorldTransform.
	// Por lo tanto, la pelota queda exactamente donde
	// la animaci�n y el socket la dejaron.
	ReleaseAIBall();

	if (!IsValid(BallToPlace))
	{
		return false;
	}

	BallToPlace->StopBallKeepingPhysics();

	// Congela temporalmente la pelota en ese punto,
	// mientras el arquero retrocede y toma carrera.
	BallToPlace->SetPossessed(true);

	// Aunque no simule f�sica, conserva colisi�n.
	BallToPlace->SetActorEnableCollision(true);

	return true;
}

bool ASoccerAICharacter::
KickReleasedGoalkeeperDistributionBall(
	ASoccerBall* SoccerBall,
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime,
	bool bUseGroundPass
)
{
	if (!IsValid(SoccerBall))
	{
		return false;
	}

	if (TargetLocation.IsNearlyZero())
	{
		return false;
	}

	SoccerBall->SetActorEnableCollision(true);

	if (bUseGroundPass)
	{
		SoccerBall->KickGroundToTarget(
			TargetLocation,
			HorizontalSpeed
		);
	}
	else
	{
		SoccerBall->KickToAirTarget(
			TargetLocation,
			HorizontalSpeed,
			MinTravelTime,
			MaxTravelTime
		);
	}

	return true;
}

bool ASoccerAICharacter::HoldGoalkeeperBallInHands(
	ASoccerBall* SoccerBall
)
{
	if (
		GetPlayerRole() != ESoccerPlayerRole::Goalkeeper ||
		!IsValid(SoccerBall)
		)
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh =
		GetMesh();

	if (CharacterMesh == nullptr)
	{
		return false;
	}

	if (
		GoalkeeperBallHoldSocketName.IsNone() ||
		!CharacterMesh->DoesSocketExist(
			GoalkeeperBallHoldSocketName
		)
		)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				3.0f,
				FColor::Red,
				FString::Printf(
					TEXT("GK: no existe el socket %s"),
					*GoalkeeperBallHoldSocketName.ToString()
				)
			);
		}

		return false;
	}

	// Si por alg�n caso excepcional ya controlaba otra pelota,
	// liberamos primero la posesi�n anterior.
	if (
		bAIIsPossessingBall &&
		IsValid(ControlledBall) &&
		ControlledBall != SoccerBall
		)
	{
		ReleaseAIBall();
	}

	// Registra la posesi�n l�gica y actualiza sus tiempos.
	PossessAIBall(SoccerBall);

	if (
		!bAIIsPossessingBall ||
		ControlledBall != SoccerBall
		)
	{
		return false;
	}

	// La pelota deja de simular f�sica mientras permanece
	// adjunta a la mano.
	ControlledBall->SetPossessed(true);

	// Evita que la pelota sostenida choque con postes,
	// paredes, red u otros elementos.
	ControlledBall->SetActorEnableCollision(false);

	ControlledBall->AttachToComponent(
		CharacterMesh,
		FAttachmentTransformRules::
		SnapToTargetNotIncludingScale,
		GoalkeeperBallHoldSocketName
	);

	const bool bAttached =
		ControlledBall->GetAttachParentActor() == this;

	if (!bAttached)
	{
		ControlledBall->SetActorEnableCollision(true);

		ReleaseAIBall();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.0f,
				FColor::Red,
				TEXT("GK: no se pudo adjuntar la pelota")
			);
		}

		return false;
	}

	bGoalkeeperHoldingBall = true;

	RequestAIMovementReevaluation();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.2f,
			FColor::Green,
			TEXT("GK: pelota adjunta a las manos")
		);
	}

	return true;
}

void ASoccerAICharacter::UpdateAIPossessedBallLocation()
{
	if (!bAIIsPossessingBall || !IsValid(ControlledBall))
	{
		bAIIsPossessingBall = false;
		ControlledBall = nullptr;
		return;
	}

	const FVector Forward = GetActorForwardVector();

	const float GroundZ =
		GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FVector BallLocation =
		GetActorLocation()
		+ Forward * AIPossessedBallForwardOffset;

	BallLocation.Z = GroundZ + AIPossessedBallHeight;

	ControlledBall->SetActorLocation(
		BallLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);
}

void ASoccerAICharacter::KickAIBallToTarget(
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime
)
{
	if (
		bAIKickMontageActive ||
		!bAIIsPossessingBall ||
		!IsValid(ControlledBall)
		)
	{
		return;
	}

	float HorizontalSpeedToUse = HorizontalSpeed;

	UAnimMontage* KickMontage = SelectAIKickMontageForTarget(
		TargetLocation,
		HorizontalSpeedToUse
	);

	if (
		KickMontage != nullptr &&
		StartAIKickMontage(
			KickMontage,
			TargetLocation,
			HorizontalSpeedToUse,
			MinTravelTime,
			MaxTravelTime,
			false
		)
		)
	{
		return;
	}

	// Short kick, unassigned montage, or montage playback failure: preserve
	// the previous immediate AI behavior as a safe fallback.
	StartAIKickAnimation();

	ASoccerBall* BallToKick = ControlledBall;

	ReleaseAIBall();

	BallToKick->KickToTarget(
		TargetLocation,
		HorizontalSpeed,
		MinTravelTime,
		MaxTravelTime
	);
}

void ASoccerAICharacter::KickAIBallToAirTarget(
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime
)
{
	if (
		bAIKickMontageActive ||
		!bAIIsPossessingBall ||
		!IsValid(ControlledBall)
		)
	{
		return;
	}

	float HorizontalSpeedToUse = HorizontalSpeed;

	UAnimMontage* KickMontage = SelectAIKickMontageForTarget(
		TargetLocation,
		HorizontalSpeedToUse
	);

	if (
		KickMontage != nullptr &&
		StartAIKickMontage(
			KickMontage,
			TargetLocation,
			HorizontalSpeedToUse,
			MinTravelTime,
			MaxTravelTime,
			true
		)
		)
	{
		return;
	}

	StartAIKickAnimation();

	ASoccerBall* BallToKick = ControlledBall;

	ReleaseAIBall();

	BallToKick->KickToAirTarget(
		TargetLocation,
		HorizontalSpeed,
		MinTravelTime,
		MaxTravelTime
	);
}

void ASoccerAICharacter::PlayAIKickAnimationForRestart()
{
	StartAIKickAnimation();
}

bool ASoccerAICharacter::StartAIKickMontageForRestart(
	ASoccerBall* BallToKick,
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime,
	bool bUseAirTarget
)
{
	if (!IsValid(BallToKick) || bAIKickMontageActive)
	{
		return false;
	}

	float HorizontalSpeedToUse = HorizontalSpeed;
	UAnimMontage* KickMontage = SelectAIKickMontageForBallLocation(
		BallToKick->GetActorLocation(),
		TargetLocation,
		HorizontalSpeedToUse
	);

	if (KickMontage == nullptr)
	{
		return false;
	}

	return StartAIKickMontageWithBall(
		BallToKick,
		KickMontage,
		TargetLocation,
		HorizontalSpeedToUse,
		MinTravelTime,
		MaxTravelTime,
		bUseAirTarget,
		false,
		false,
		true
	);
}

bool ASoccerAICharacter::IsAIKickMontageActive() const
{
	return bAIKickMontageActive;
}

bool ASoccerAICharacter::HasAIKickMontageImpactedBall() const
{
	return
		bPendingAIKickHasImpactedBall ||
		bLastAIKickMontageEndedAfterImpact;
}

bool ASoccerAICharacter::ShouldAIKickMontageLockController() const
{
	return
		bAIKickMontageActive &&
		!(bPendingAIKickIsAutoPass && bPendingAIKickHasImpactedBall);
}

FVector ASoccerAICharacter::GetPendingAIKickTarget() const
{
	return PendingAIKickTarget;
}

void ASoccerAICharacter::CancelAIKickMontageBeforeImpact()
{
	if (bAIKickMontageActive && !bPendingAIKickHasImpactedBall)
	{
		CancelPendingAIKickAnimation(true);
	}
}

float ASoccerAICharacter::GetTimeSinceAIPossessionStarted() const
{
	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	return CurrentTime - LastAIPossessionStartTime;
}

float ASoccerAICharacter::GetTimeSinceAIBallReleased() const
{
	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	return CurrentTime - LastAIBallReleasedTime;
}

bool ASoccerAICharacter::IsAIPossessionProtected(float ProtectionTime) const
{
	if (!bAIIsPossessingBall)
	{
		return false;
	}

	return GetTimeSinceAIPossessionStarted() < ProtectionTime;
}

bool ASoccerAICharacter::IsAIStealRecoveryActive(float RecoveryTime) const
{
	return GetTimeSinceAIBallReleased() < RecoveryTime;
}

void ASoccerAICharacter::StartAIAutoPassToLocation(
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime
)
{
	if (!bAIIsPossessingBall || !IsValid(ControlledBall))
	{
		return;
	}

	RequestAIMovementMode(
		ESoccerAIMovementMode::FastRun,
		ESoccerAIMovementReason::ChaseOwnAutoPass,
		true
	);

	if (
		TryStartAIDribbleTurnAutoPassToLocation(
			TargetLocation,
			HorizontalSpeed,
			MinTravelTime,
			MaxTravelTime
		)
		)
	{
		return;
	}

	float HorizontalSpeedToUse = HorizontalSpeed;
	UAnimMontage* AutoPassMontage = SelectAIKickMontageForTarget(
		TargetLocation,
		HorizontalSpeedToUse
	);

	if (
		AutoPassMontage != nullptr &&
		StartAIKickMontage(
			AutoPassMontage,
			TargetLocation,
			HorizontalSpeedToUse,
			MinTravelTime,
			MaxTravelTime,
			false,
			true
		)
		)
	{
		return;
	}

	ExecuteAIAutoPassImmediate(
		TargetLocation,
		HorizontalSpeed,
		MinTravelTime,
		MaxTravelTime,
		true
	);
}

bool ASoccerAICharacter::IsAIAutoPassActive() const
{
	return bAIAutoPassActive;
}

ASoccerBall* ASoccerAICharacter::GetAIAutoPassBall() const
{
	return AIAutoPassBall;
}

FVector ASoccerAICharacter::GetAIAutoPassTargetLocation() const
{
	return AIAutoPassTargetLocation;
}

FVector ASoccerAICharacter::GetAIAutoPassStartBallLocation() const
{
	return AIAutoPassStartBallLocation;
}

float ASoccerAICharacter::GetTimeSinceAIAutoPassStarted() const
{
	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	return CurrentTime - AIAutoPassStartTime;
}

void ASoccerAICharacter::ClearAIAutoPassState()
{
	bAIAutoPassActive = false;
	AIAutoPassBall = nullptr;
	AIAutoPassTargetLocation = FVector::ZeroVector;
	AIAutoPassStartBallLocation = FVector::ZeroVector;
	AIAutoPassStartTime = 0.0f;
}

bool ASoccerAICharacter::TryCollectAIAutoPassIfClose(float CollectDistance)
{
	if (!bAIAutoPassActive || !IsValid(AIAutoPassBall))
	{
		ClearAIAutoPassState();
		return false;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (CurrentTime - AIAutoPassStartTime < AIAutoPassMinCollectDelay)
	{
		return false;
	}

	const float BallTravelDistance = FVector::Dist2D(
		AIAutoPassStartBallLocation,
		AIAutoPassBall->GetActorLocation()
	);

	if (BallTravelDistance < AIAutoPassMinBallTravelDistanceBeforeCollect)
	{
		return false;
	}

	const float CharacterGroundZ =
		GetActorLocation().Z -
		GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	const float BallHeightFromGround =
		AIAutoPassBall->GetActorLocation().Z - CharacterGroundZ;

	if (
		BallHeightFromGround < -20.0f ||
		BallHeightFromGround > AIAutoPassMaxCollectBallHeight
		)
	{
		return false;
	}

	const float DistanceToBall = FVector::Dist2D(
		GetActorLocation(),
		AIAutoPassBall->GetActorLocation()
	);

	if (DistanceToBall > CollectDistance)
	{
		return false;
	}

	ASoccerBall* BallToCollect = AIAutoPassBall;

	ClearAIAutoPassState();

	PossessAIBall(BallToCollect);

	RequestAIMovementReevaluation();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.8f,
			FColor::Green,
			TEXT("AI recupero su autopase")
		);
	}

	return true;
}

////////////

UAnimMontage* ASoccerAICharacter::SelectAIKickMontageForTarget(
	const FVector& TargetLocation,
	float& OutHorizontalSpeed
) const
{
	if (!IsValid(ControlledBall))
	{
		return nullptr;
	}

	return SelectAIKickMontageForBallLocation(
		ControlledBall->GetActorLocation(),
		TargetLocation,
		OutHorizontalSpeed
	);
}

UAnimMontage* ASoccerAICharacter::SelectAIKickMontageForBallLocation(
	const FVector& BallLocation,
	const FVector& TargetLocation,
	float& OutHorizontalSpeed
) const
{
	const float Distance2D = FVector::Dist2D(
		BallLocation,
		TargetLocation
	);

	// Keep the same initial thresholds as the human, but in AI-owned
	// properties so future AI tuning does not affect player controls.
	if (Distance2D <= AIShortKickMaxDistance)
	{
		return nullptr;
	}

	FVector PlayerForward = GetActorForwardVector();
	PlayerForward.Z = 0.0f;
	PlayerForward = PlayerForward.GetSafeNormal();

	FVector BallToTarget = TargetLocation - BallLocation;
	BallToTarget.Z = 0.0f;
	BallToTarget = BallToTarget.GetSafeNormal();

	if (PlayerForward.IsNearlyZero() || BallToTarget.IsNearlyZero())
	{
		return nullptr;
	}

	const float Dot = FMath::Clamp(
		FVector::DotProduct(PlayerForward, BallToTarget),
		-1.0f,
		1.0f
	);

	const float AngleDegrees = FMath::RadiansToDegrees(
		FMath::Acos(Dot)
	);

	const float CrossZ = FVector::CrossProduct(
		PlayerForward,
		BallToTarget
	).Z;

	// Same leg convention used by the human selection logic.
	const bool bUseRightLeg = CrossZ < 0.0f;

	const bool bSideKick =
		AngleDegrees > AISideKickMinAngleDegrees;

	const bool bPlayerIsMoving =
		GetVelocity().Size2D() >= AIRunningKickMinSpeed;

	const bool bLongDistance =
		Distance2D >= AILongKickMinDistance;

	const bool bCanUseStrike =
		bLongDistance && !bSideKick;

	if (bCanUseStrike)
	{
		return bUseRightLeg
			? StrikeRightLegForwardJogMontage
			: StrikeLeftLegForwardJogMontage;
	}

	if (bLongDistance && bSideKick)
	{
		OutHorizontalSpeed = AIForcedLongAnglePassSpeed;
	}

	if (bPlayerIsMoving)
	{
		if (bSideKick)
		{
			return bUseRightLeg
				? RunningRightLegSidePassMontage
				: RunningLeftLegSidePassMontage;
		}

		return bUseRightLeg
			? RunningRightLegPassMontage
			: RunningLeftLegPassMontage;
	}

	if (bSideKick)
	{
		return bUseRightLeg
			? StandRightLegSidePassMontage
			: StandLeftLegSidePassMontage;
	}

	return bUseRightLeg
		? StandRightLegPassMontage
		: StandLeftLegPassMontage;
}

bool ASoccerAICharacter::StartAIKickMontage(
	UAnimMontage* KickMontage,
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime,
	bool bUseAirTarget,
	bool bTreatAsAutoPass
)
{
	if (
		KickMontage == nullptr ||
		bAIKickMontageActive ||
		!bAIIsPossessingBall ||
		!IsValid(ControlledBall)
		)
	{
		return false;
	}

	return StartAIKickMontageWithBall(
		ControlledBall,
		KickMontage,
		TargetLocation,
		HorizontalSpeed,
		MinTravelTime,
		MaxTravelTime,
		bUseAirTarget,
		true,
		bTreatAsAutoPass,
		false
	);
}

bool ASoccerAICharacter::StartAIKickMontageWithBall(
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
)
{
	if (
		KickMontage == nullptr ||
		bAIKickMontageActive ||
		!IsValid(BallToKick)
		)
	{
		return false;
	}

	const float MontageDuration = PlayAnimMontage(KickMontage);

	if (MontageDuration <= 0.0f)
	{
		return false;
	}

	if (bReleaseCurrentAIPossession)
	{
		if (!bAIIsPossessingBall || ControlledBall != BallToKick)
		{
			StopAnimMontage(KickMontage);
			return false;
		}

		ReleaseAIBall();
	}

	if (!IsValid(BallToKick))
	{
		StopAnimMontage(KickMontage);
		return false;
	}

	// The restart/autopass ball remains exactly where the tactical decision
	// placed it until the authored foot-contact time. KickToTarget/KickToAirTarget
	// releases this temporary physics lock at impact.
	BallToKick->SetPossessed(true);
	BallToKick->StopBallKeepingPhysics();

	PendingAIKickBall = BallToKick;
	ActiveAIKickMontage = KickMontage;
	PendingAIKickTarget = TargetLocation;
	PendingAIKickStartBallLocation = BallToKick->GetActorLocation();
	PendingAIKickHorizontalSpeed = HorizontalSpeed;
	PendingAIKickMinTravelTime = MinTravelTime;
	PendingAIKickMaxTravelTime = MaxTravelTime;
	bPendingAIKickUsesAirTarget = bUseAirTarget;
	bPendingAIKickHasImpactedBall = false;
	bLastAIKickMontageEndedAfterImpact = false;
	bPendingAIKickIsAutoPass = bTreatAsAutoPass;
	bPendingAIKickIsRestart = bTreatAsRestart;
	bAIKickMontageActive = true;

	SetAIChasingBall(false);
	ClearScriptedLocomotionVelocity();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	StartAIKickAnimation();

	if (UWorld* World = GetWorld())
	{
		AIKickingAnimationEndTime =
			World->GetTimeSeconds() +
			MontageDuration +
			FMath::Max(0.0f, AIKickAnimationFinishExtraDelay);
	}

	const float ImpactDelay = GetAIKickImpactDelayForMontage(
		KickMontage,
		MontageDuration
	);

	GetWorldTimerManager().ClearTimer(AIKickImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(AIKickFinishTimerHandle);

	GetWorldTimerManager().SetTimer(
		AIKickImpactTimerHandle,
		this,
		&ASoccerAICharacter::PerformPendingAIKickImpact,
		ImpactDelay,
		false
	);

	GetWorldTimerManager().SetTimer(
		AIKickFinishTimerHandle,
		this,
		&ASoccerAICharacter::FinishPendingAIKickAnimation,
		MontageDuration + FMath::Max(0.0f, AIKickAnimationFinishExtraDelay),
		false
	);

	return true;
}

float ASoccerAICharacter::GetAIKickImpactDelayForMontage(
	UAnimMontage* KickMontage,
	float MontageDuration
) const
{
	float DesiredDelay = AIPassKickImpactDelay;

	if (
		KickMontage == StrikeLeftLegForwardJogMontage ||
		KickMontage == StrikeRightLegForwardJogMontage
		)
	{
		DesiredDelay = AIStrikeKickImpactDelay;
	}

	const float MaxSafeDelay = FMath::Max(
		0.05f,
		MontageDuration - 0.05f
	);

	return FMath::Clamp(
		DesiredDelay,
		0.05f,
		MaxSafeDelay
	);
}

void ASoccerAICharacter::PerformPendingAIKickImpact()
{
	if (!bAIKickMontageActive || bPendingAIKickHasImpactedBall)
	{
		return;
	}

	if (ShouldCancelPendingAIKickAnimation())
	{
		CancelPendingAIKickAnimation(true);
		return;
	}

	ASoccerBall* BallToKick = PendingAIKickBall;

	if (!IsValid(BallToKick))
	{
		CancelPendingAIKickAnimation(false);
		return;
	}

	bPendingAIKickHasImpactedBall = true;

	if (bPendingAIKickIsAutoPass)
	{
		AIAutoPassBall = BallToKick;
		bAIAutoPassActive = true;
		AIAutoPassTargetLocation = PendingAIKickTarget;
		AIAutoPassStartBallLocation = BallToKick->GetActorLocation();
		AIAutoPassStartTime =
			GetWorld() != nullptr
			? GetWorld()->GetTimeSeconds()
			: 0.0f;

		FVector NewCurrentDirection = PendingAIKickTarget - GetActorLocation();
		NewCurrentDirection.Z = 0.0f;
		NewCurrentDirection = NewCurrentDirection.GetSafeNormal();

		if (!NewCurrentDirection.IsNearlyZero())
		{
			AIDribbleTurnCurrentDirection = NewCurrentDirection;
		}
	}

	if (bPendingAIKickUsesAirTarget)
	{
		BallToKick->KickToAirTarget(
			PendingAIKickTarget,
			PendingAIKickHorizontalSpeed,
			PendingAIKickMinTravelTime,
			PendingAIKickMaxTravelTime
		);
	}
	else
	{
		BallToKick->KickToTarget(
			PendingAIKickTarget,
			PendingAIKickHorizontalSpeed,
			PendingAIKickMinTravelTime,
			PendingAIKickMaxTravelTime
		);
	}
}

void ASoccerAICharacter::FinishPendingAIKickAnimation()
{
	if (!bAIKickMontageActive)
	{
		return;
	}

	if (!bPendingAIKickHasImpactedBall)
	{
		PerformPendingAIKickImpact();
	}

	if (!bAIKickMontageActive)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AIKickImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(AIKickFinishTimerHandle);

	bLastAIKickMontageEndedAfterImpact = bPendingAIKickHasImpactedBall;

	PendingAIKickBall = nullptr;
	ActiveAIKickMontage = nullptr;
	PendingAIKickTarget = FVector::ZeroVector;
	PendingAIKickStartBallLocation = FVector::ZeroVector;
	PendingAIKickHorizontalSpeed = 0.0f;
	PendingAIKickMinTravelTime = 0.0f;
	PendingAIKickMaxTravelTime = 0.0f;
	bPendingAIKickUsesAirTarget = false;
	bPendingAIKickHasImpactedBall = false;
	bPendingAIKickIsAutoPass = false;
	bPendingAIKickIsRestart = false;
	bAIKickMontageActive = false;
	bAIIsKickingForAnimation = false;
	AIKickingAnimationEndTime = -1000.0f;

	RequestAIMovementReevaluation();
}

void ASoccerAICharacter::CancelPendingAIKickAnimation(
	bool bReleaseFrozenBall
)
{
	if (!bAIKickMontageActive)
	{
		return;
	}

	ASoccerBall* BallToRelease = PendingAIKickBall;
	UAnimMontage* MontageToStop = ActiveAIKickMontage;
	const bool bBallAlreadyKicked = bPendingAIKickHasImpactedBall;

	bool bBallClaimedByAnotherCharacter = false;

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
		{
			ASoccerMatchManager* MatchManager = *It;

			if (!IsValid(MatchManager))
			{
				continue;
			}

			ASoccerCharacterBase* PossessingCharacter =
				MatchManager->GetPossessingCharacter();

			bBallClaimedByAnotherCharacter =
				IsValid(PossessingCharacter) &&
				PossessingCharacter != this;

			break;
		}
	}

	GetWorldTimerManager().ClearTimer(AIKickImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(AIKickFinishTimerHandle);

	PendingAIKickBall = nullptr;
	ActiveAIKickMontage = nullptr;
	PendingAIKickTarget = FVector::ZeroVector;
	PendingAIKickStartBallLocation = FVector::ZeroVector;
	PendingAIKickHorizontalSpeed = 0.0f;
	PendingAIKickMinTravelTime = 0.0f;
	PendingAIKickMaxTravelTime = 0.0f;
	bPendingAIKickUsesAirTarget = false;
	bPendingAIKickHasImpactedBall = false;
	bLastAIKickMontageEndedAfterImpact = false;
	bPendingAIKickIsAutoPass = false;
	bPendingAIKickIsRestart = false;
	bAIKickMontageActive = false;
	bAIIsKickingForAnimation = false;
	AIKickingAnimationEndTime = -1000.0f;

	if (
		MontageToStop != nullptr &&
		GetMesh() != nullptr
		)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.08f, MontageToStop);
		}
	}

	if (
		bReleaseFrozenBall &&
		!bBallAlreadyKicked &&
		IsValid(BallToRelease) &&
		!bBallClaimedByAnotherCharacter &&
		BallToRelease->GetAttachParentActor() == nullptr
		)
	{
		BallToRelease->SetPossessed(false);
		BallToRelease->StopBallKeepingPhysics();
	}

	RequestAIMovementReevaluation();
}

bool ASoccerAICharacter::ShouldCancelPendingAIKickAnimation() const
{
	if (!bAIKickMontageActive)
	{
		return true;
	}

	if (
		IsTackleFallReactionActive() ||
		IsTackleActive() ||
		IsTackleEvasionActive() ||
		IsAerialActionLocked() ||
		IsAerialActionWaitingToStart()
		)
	{
		return true;
	}

	// After the authored foot contact the ball is expected to leave this spot
	// and may immediately become another player's possession. Keep only the
	// animation lock until blend-out; ball-validity checks below are pre-impact.
	if (bPendingAIKickHasImpactedBall)
	{
		return false;
	}

	if (!IsValid(PendingAIKickBall))
	{
		return true;
	}

	if (
		FVector::Dist2D(
			PendingAIKickBall->GetActorLocation(),
			PendingAIKickStartBallLocation
		) > FMath::Max(10.0f, AIPendingKickMaxBallDriftDistance)
		)
	{
		return true;
	}

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
		{
			ASoccerMatchManager* MatchManager = *It;

			if (!IsValid(MatchManager))
			{
				continue;
			}

			ASoccerCharacterBase* PossessingCharacter =
				MatchManager->GetPossessingCharacter();

			if (
				IsValid(PossessingCharacter) &&
				PossessingCharacter != this
				)
			{
				return true;
			}

			break;
		}
	}

	return false;
}

void ASoccerAICharacter::StartAIKickAnimation()
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	bAIIsKickingForAnimation = true;

	AIKickingAnimationEndTime =
		World->GetTimeSeconds() + AIKickAnimationDuration;
}

//x//////////

bool ASoccerAICharacter::IsAIDribbleTurnAutoPassActive() const
{
	return bAIDribbleTurnAutoPassActive;
}

void ASoccerAICharacter::ExecuteAIAutoPassImmediate(
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime,
	bool bPlayGenericKickAnimation
)
{
	if (!bAIIsPossessingBall || !IsValid(ControlledBall))
	{
		return;
	}

	if (bPlayGenericKickAnimation)
	{
		StartAIKickAnimation();
	}

	ASoccerBall* BallToKick = ControlledBall;

	AIAutoPassBall = BallToKick;
	bAIAutoPassActive = true;
	AIAutoPassTargetLocation = TargetLocation;
	AIAutoPassStartBallLocation = BallToKick->GetActorLocation();

	AIAutoPassStartTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	FVector NewCurrentDirection =
		TargetLocation - GetActorLocation();

	NewCurrentDirection.Z = 0.0f;
	NewCurrentDirection =
		NewCurrentDirection.GetSafeNormal();

	if (!NewCurrentDirection.IsNearlyZero())
	{
		AIDribbleTurnCurrentDirection =
			NewCurrentDirection;
	}

	ReleaseAIBall();

	BallToKick->KickToTarget(
		TargetLocation,
		HorizontalSpeed,
		MinTravelTime,
		MaxTravelTime
	);
}

UAnimMontage* ASoccerAICharacter::SelectAIDribbleTurnMontageForDirection(
	const FVector& DesiredDirection,
	float CurrentSpeed,
	float& OutAngleDegrees,
	float& OutForcedLocomotionSpeed
) const
{
	OutAngleDegrees = 0.0f;
	OutForcedLocomotionSpeed = 0.0f;

	FVector CurrentDirection =
		AIDribbleTurnCurrentDirection;

	CurrentDirection.Z = 0.0f;
	CurrentDirection =
		CurrentDirection.GetSafeNormal();

	if (CurrentDirection.IsNearlyZero())
	{
		CurrentDirection =
			GetActorForwardVector();

		CurrentDirection.Z = 0.0f;
		CurrentDirection =
			CurrentDirection.GetSafeNormal();
	}

	FVector NewDirection =
		DesiredDirection;

	NewDirection.Z = 0.0f;
	NewDirection =
		NewDirection.GetSafeNormal();

	if (CurrentDirection.IsNearlyZero() || NewDirection.IsNearlyZero())
	{
		return nullptr;
	}

	const float Dot =
		FMath::Clamp(
			FVector::DotProduct(
				CurrentDirection,
				NewDirection
			),
			-1.0f,
			1.0f
		);

	OutAngleDegrees =
		FMath::RadiansToDegrees(
			FMath::Acos(Dot)
		);

	const float CrossZ =
		FVector::CrossProduct(
			CurrentDirection,
			NewDirection
		).Z;

	const bool bTurnRight =
		CrossZ > 0.0f;

	if (CurrentSpeed >= AIStrongRunDribbleTurnMinPlayerSpeed)
	{
		if (OutAngleDegrees < AIStrongRunDribbleTurnMinAngleDegrees)
		{
			return nullptr;
		}

		OutForcedLocomotionSpeed =
			AIStrongRunDribbleTurnForcedSpeed;

		const bool bHardTurn =
			OutAngleDegrees >= AIStrongRunDribbleTurnHardAngleDegrees;

		if (bHardTurn)
		{
			return bTurnRight
				? AIStrongRunDribbleTurnRightOver90Montage
				: AIStrongRunDribbleTurnLeftOver90Montage;
		}

		return bTurnRight
			? AIStrongRunDribbleTurnRight45To90Montage
			: AIStrongRunDribbleTurnLeft45To90Montage;
	}

	if (
		CurrentSpeed >= AINormalRunDribbleTurnMinPlayerSpeed &&
		CurrentSpeed < AINormalRunDribbleTurnMaxPlayerSpeed
		)
	{
		if (OutAngleDegrees < AINormalRunDribbleTurnMinAngleDegrees)
		{
			return nullptr;
		}

		OutForcedLocomotionSpeed =
			AINormalRunDribbleTurnForcedSpeed;

		const bool bHardTurn =
			OutAngleDegrees >= AINormalRunDribbleTurnHardAngleDegrees;

		if (bHardTurn)
		{
			return bTurnRight
				? AINormalRunDribbleTurnRightOver90Montage
				: AINormalRunDribbleTurnLeftOver90Montage;
		}

		return bTurnRight
			? AINormalRunDribbleTurnRight45To90Montage
			: AINormalRunDribbleTurnLeft45To90Montage;
	}

	return nullptr;
}

bool ASoccerAICharacter::TryStartAIDribbleTurnAutoPassToLocation(
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime
)
{
	if (!bAIIsPossessingBall || !IsValid(ControlledBall))
	{
		return false;
	}

	if (bAIDribbleTurnAutoPassActive)
	{
		return true;
	}

	const float CurrentSpeed =
		GetVelocity().Size2D();

	FVector DesiredDirection =
		TargetLocation - GetActorLocation();

	DesiredDirection.Z = 0.0f;
	DesiredDirection =
		DesiredDirection.GetSafeNormal();

	if (DesiredDirection.IsNearlyZero())
	{
		return false;
	}

	float TurnAngleDegrees = 0.0f;
	float ForcedLocomotionSpeed = 0.0f;

	UAnimMontage* TurnMontage =
		SelectAIDribbleTurnMontageForDirection(
			DesiredDirection,
			CurrentSpeed,
			TurnAngleDegrees,
			ForcedLocomotionSpeed
		);

	if (TurnMontage == nullptr)
	{
		return false;
	}

	StartAIDribbleTurnAutoPass(
		DesiredDirection,
		TurnMontage,
		TargetLocation,
		HorizontalSpeed,
		MinTravelTime,
		MaxTravelTime,
		ForcedLocomotionSpeed
	);

	return true;
}

void ASoccerAICharacter::StartAIDribbleTurnAutoPass(
	const FVector& DesiredDirection,
	UAnimMontage* TurnMontage,
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime,
	float ForcedLocomotionSpeed
)
{
	if (!bAIIsPossessingBall || !IsValid(ControlledBall) || TurnMontage == nullptr)
	{
		return;
	}

	FVector SafeDesiredDirection =
		DesiredDirection;

	SafeDesiredDirection.Z = 0.0f;
	SafeDesiredDirection =
		SafeDesiredDirection.GetSafeNormal();

	if (SafeDesiredDirection.IsNearlyZero())
	{
		return;
	}

	bAIDribbleTurnAutoPassActive = true;
	bAIDribbleTurnAutoPassHasImpactedBall = false;

	ActiveAIDribbleTurnDirection =
		SafeDesiredDirection;

	ActiveAIDribbleTurnAutoPassTargetLocation =
		TargetLocation;

	ActiveAIDribbleTurnAutoPassHorizontalSpeed =
		HorizontalSpeed;

	ActiveAIDribbleTurnAutoPassMinTravelTime =
		MinTravelTime;

	ActiveAIDribbleTurnAutoPassMaxTravelTime =
		MaxTravelTime;

	ActiveAIDribbleTurnForcedLocomotionSpeed =
		ForcedLocomotionSpeed;

	if (GetCharacterMovement() != nullptr)
	{
		GetCharacterMovement()->StopMovementImmediately();
	}

	ControlledBall->SetPossessed(true);
	UpdateAIPossessedBallLocation();

	const float MontageDuration =
		PlayAnimMontage(TurnMontage);

	if (MontageDuration <= 0.0f)
	{
		PerformAIDribbleTurnAutoPassImpact();
		FinishAIDribbleTurnAutoPassAnimation();
		return;
	}

	const bool bStrongTurn =
		ForcedLocomotionSpeed >= AIStrongRunDribbleTurnForcedSpeed - 1.0f;

	const float ImpactDelay =
		bStrongTurn
		? AIStrongRunDribbleTurnImpactDelay
		: AINormalRunDribbleTurnImpactDelay;

	const float FinishExtraDelay =
		bStrongTurn
		? AIStrongRunDribbleTurnFinishExtraDelay
		: AINormalRunDribbleTurnFinishExtraDelay;

	const float ActorRotationDuration =
		FMath::Max(
			bStrongTurn
			? AIStrongRunDribbleTurnQuickRotationDuration
			: AINormalRunDribbleTurnQuickRotationDuration,
			MontageDuration
		);

	StartAIDribbleTurnActorRotation(
		SafeDesiredDirection,
		ActorRotationDuration
	);

	GetWorldTimerManager().ClearTimer(AIDribbleTurnImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(AIDribbleTurnFinishTimerHandle);

	const float SafeImpactDelay =
		FMath::Clamp(
			ImpactDelay,
			0.0f,
			MontageDuration
		);

	GetWorldTimerManager().SetTimer(
		AIDribbleTurnImpactTimerHandle,
		this,
		&ASoccerAICharacter::PerformAIDribbleTurnAutoPassImpact,
		SafeImpactDelay,
		false
	);

	GetWorldTimerManager().SetTimer(
		AIDribbleTurnFinishTimerHandle,
		this,
		&ASoccerAICharacter::FinishAIDribbleTurnAutoPassAnimation,
		MontageDuration + FinishExtraDelay,
		false
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.8f,
			FColor::Cyan,
			TEXT("AI giro con autopase")
		);
	}
}

void ASoccerAICharacter::PerformAIDribbleTurnAutoPassImpact()
{
	if (bAIDribbleTurnAutoPassHasImpactedBall)
	{
		return;
	}

	if (!bAIIsPossessingBall || !IsValid(ControlledBall))
	{
		return;
	}

	bAIDribbleTurnAutoPassHasImpactedBall = true;

	AIDribbleTurnCurrentDirection =
		ActiveAIDribbleTurnDirection;

	ExecuteAIAutoPassImmediate(
		ActiveAIDribbleTurnAutoPassTargetLocation,
		ActiveAIDribbleTurnAutoPassHorizontalSpeed,
		ActiveAIDribbleTurnAutoPassMinTravelTime,
		ActiveAIDribbleTurnAutoPassMaxTravelTime,
		false
	);
}

void ASoccerAICharacter::FinishAIDribbleTurnAutoPassAnimation()
{
	if (!bAIDribbleTurnAutoPassHasImpactedBall)
	{
		PerformAIDribbleTurnAutoPassImpact();
	}

	bAIDribbleTurnAutoPassActive = false;
	bAIDribbleTurnAutoPassHasImpactedBall = false;

	ActiveAIDribbleTurnDirection = FVector::ZeroVector;
	ActiveAIDribbleTurnAutoPassTargetLocation = FVector::ZeroVector;

	ActiveAIDribbleTurnAutoPassHorizontalSpeed = 0.0f;
	ActiveAIDribbleTurnAutoPassMinTravelTime = 0.0f;
	ActiveAIDribbleTurnAutoPassMaxTravelTime = 0.0f;

	ActiveAIDribbleTurnForcedLocomotionSpeed = 0.0f;

	bIsAIDribbleTurnActorRotating = false;

	AIDribbleTurnActorRotationElapsedTime = 0.0f;
	AIDribbleTurnActorRotationCurrentDuration = 0.0f;
	AIDribbleTurnActorTotalYawDelta = 0.0f;

	GetWorldTimerManager().ClearTimer(AIDribbleTurnImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(AIDribbleTurnFinishTimerHandle);
}

void ASoccerAICharacter::StartAIDribbleTurnActorRotation(
	const FVector& DesiredDirection,
	float Duration
)
{
	FVector SafeDirection =
		DesiredDirection;

	SafeDirection.Z = 0.0f;
	SafeDirection =
		SafeDirection.GetSafeNormal();

	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	AIDribbleTurnActorStartRotation =
		GetActorRotation();

	AIDribbleTurnActorTargetRotation =
		SafeDirection.Rotation();

	AIDribbleTurnActorTargetRotation.Pitch = 0.0f;
	AIDribbleTurnActorTargetRotation.Roll = 0.0f;

	AIDribbleTurnActorTotalYawDelta =
		FMath::FindDeltaAngleDegrees(
			AIDribbleTurnActorStartRotation.Yaw,
			AIDribbleTurnActorTargetRotation.Yaw
		);

	AIDribbleTurnActorRotationElapsedTime = 0.0f;

	AIDribbleTurnActorRotationCurrentDuration =
		FMath::Max(
			0.01f,
			Duration
		);

	bIsAIDribbleTurnActorRotating = true;
}

void ASoccerAICharacter::UpdateAIDribbleTurnActorRotation(float DeltaTime)
{
	if (!bIsAIDribbleTurnActorRotating)
	{
		return;
	}

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	AIDribbleTurnActorRotationElapsedTime += DeltaTime;

	const float Alpha =
		FMath::Clamp(
			AIDribbleTurnActorRotationElapsedTime /
			AIDribbleTurnActorRotationCurrentDuration,
			0.0f,
			1.0f
		);

	const float SmoothAlpha =
		FMath::SmoothStep(
			0.0f,
			1.0f,
			Alpha
		);

	FRotator NewRotation =
		AIDribbleTurnActorStartRotation;

	NewRotation.Pitch = 0.0f;
	NewRotation.Roll = 0.0f;

	NewRotation.Yaw =
		AIDribbleTurnActorStartRotation.Yaw +
		AIDribbleTurnActorTotalYawDelta * SmoothAlpha;

	SetActorRotation(NewRotation);

	if (Alpha >= 1.0f)
	{
		bIsAIDribbleTurnActorRotating = false;

		SetActorRotation(
			AIDribbleTurnActorTargetRotation
		);
	}
}

float ASoccerAICharacter::GetAIPlayerEnergyPercent() const
{
	if (MaxAIPlayerEnergy <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		AIPlayerEnergy / MaxAIPlayerEnergy,
		0.0f,
		1.0f
	);
}

void ASoccerAICharacter::RequestAIMovementReevaluation()
{
	bAIMovementReevaluationRequested = true;
}

void ASoccerAICharacter::RequestAIMovementMode(
	ESoccerAIMovementMode NewMovementMode,
	ESoccerAIMovementReason NewMovementReason,
	bool bForceUpdate
)
{
	const ESoccerAIMovementMode LimitedMovementMode =
		LimitAIMovementModeByEnergy(
			NewMovementMode,
			NewMovementReason
		);

	if (
		!bForceUpdate &&
		CurrentAIMovementMode == LimitedMovementMode &&
		CurrentAIMovementReason == NewMovementReason
		)
	{
		return;
	}

	CurrentAIMovementMode = LimitedMovementMode;
	CurrentAIMovementReason = NewMovementReason;

	LastAIMovementDecisionTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	bAIMovementReevaluationRequested = false;

	UpdateAIMovementSpeed();
}

void ASoccerAICharacter::UpdateAIMovementModeForOrder(
	ESoccerAIOrder CurrentOrder,
	const FVector& DesiredMoveLocation,
	bool bHasDesiredMoveLocation
)
{
	if (
		ShouldKeepCurrentAIMovementDecision(
			CurrentOrder,
			DesiredMoveLocation,
			bHasDesiredMoveLocation
		)
		)
	{
		return;
	}

	ESoccerAIMovementMode DesiredMode =
		ESoccerAIMovementMode::Run;

	ESoccerAIMovementReason DesiredReason =
		ESoccerAIMovementReason::NormalPlay;

	ChooseAIMovementModeAndReasonForOrder(
		CurrentOrder,
		DesiredMoveLocation,
		bHasDesiredMoveLocation,
		DesiredMode,
		DesiredReason
	);

	DesiredMode =
		LimitAIMovementModeByEnergy(
			DesiredMode,
			DesiredReason
		);

	ApplyAIMovementDecision(
		DesiredMode,
		DesiredReason,
		CurrentOrder,
		DesiredMoveLocation,
		bHasDesiredMoveLocation
	);
}

bool ASoccerAICharacter::ShouldKeepCurrentAIMovementDecision(
	ESoccerAIOrder CurrentOrder,
	const FVector& DesiredMoveLocation,
	bool bHasDesiredMoveLocation
) const
{
	if (bAIMovementReevaluationRequested)
	{
		return false;
	}

	UWorld* World = GetWorld();

	const float CurrentTime =
		World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	if (
		CurrentTime - LastAIMovementDecisionTime >=
		AIMovementPassiveReviewInterval
		)
	{
		return false;
	}

	if (LastAIMovementOrderAtDecision != CurrentOrder)
	{
		return false;
	}

	if (bHadAIMovementTargetAtDecision != bHasDesiredMoveLocation)
	{
		return false;
	}

	if (bHasDesiredMoveLocation)
	{
		const float TargetShiftDistance =
			FVector::Dist2D(
				LastAIMovementTargetLocation,
				DesiredMoveLocation
			);

		if (
			TargetShiftDistance >=
			AIMovementTargetShiftReevaluationDistance
			)
		{
			return false;
		}

		const float DistanceToTarget =
			FVector::Dist2D(
				GetActorLocation(),
				DesiredMoveLocation
			);

		if (
			DistanceToTarget <= AIMovementTargetReachedDistance &&
			CurrentAIMovementMode != ESoccerAIMovementMode::Walk &&
			CurrentAIMovementMode != ESoccerAIMovementMode::Jog
			)
		{
			return false;
		}
	}

	if (
		CurrentAIMovementMode == ESoccerAIMovementMode::FastRun &&
		!IsAIMovementEmergencyReason(CurrentAIMovementReason) &&
		GetAIPlayerEnergyPercent() < AINormalFastRunMinimumEnergyPercent
		)
	{
		return false;
	}

	return true;
}

void ASoccerAICharacter::ApplyAIMovementDecision(
	ESoccerAIMovementMode NewMovementMode,
	ESoccerAIMovementReason NewMovementReason,
	ESoccerAIOrder CurrentOrder,
	const FVector& DesiredMoveLocation,
	bool bHasDesiredMoveLocation
)
{
	CurrentAIMovementMode = NewMovementMode;
	CurrentAIMovementReason = NewMovementReason;

	LastAIMovementOrderAtDecision = CurrentOrder;
	LastAIMovementTargetLocation = DesiredMoveLocation;
	bHadAIMovementTargetAtDecision = bHasDesiredMoveLocation;

	LastAIMovementDecisionTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	bAIMovementReevaluationRequested = false;

	UpdateAIMovementSpeed();
}

void ASoccerAICharacter::ChooseAIMovementModeAndReasonForOrder(
	ESoccerAIOrder CurrentOrder,
	const FVector& DesiredMoveLocation,
	bool bHasDesiredMoveLocation,
	ESoccerAIMovementMode& OutMovementMode,
	ESoccerAIMovementReason& OutMovementReason
) const
{
	const float DistanceToTarget =
		bHasDesiredMoveLocation
		? FVector::Dist2D(
			GetActorLocation(),
			DesiredMoveLocation
		)
		: 0.0f;

	const bool bShortMove =
		bHasDesiredMoveLocation &&
		DistanceToTarget <= AIMovementShortDistance;

	const bool bMediumMove =
		bHasDesiredMoveLocation &&
		DistanceToTarget <= AIMovementMediumDistance;

	const bool bLongMove =
		bHasDesiredMoveLocation &&
		DistanceToTarget >= AIMovementLongDistance;

	if (bAIAutoPassActive)
	{
		OutMovementMode = ESoccerAIMovementMode::FastRun;
		OutMovementReason = ESoccerAIMovementReason::ChaseOwnAutoPass;
		return;
	}

	if (GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		if (
			CurrentOrder == ESoccerAIOrder::ChaseBall ||
			CurrentOrder == ESoccerAIOrder::PressBall
			)
		{
			OutMovementMode =
				bShortMove
				? ESoccerAIMovementMode::Run
				: ESoccerAIMovementMode::FastRun;

			OutMovementReason =
				ESoccerAIMovementReason::GoalkeeperEmergency;

			return;
		}
	}

	switch (CurrentOrder)
	{
	case ESoccerAIOrder::ChaseBall:
	case ESoccerAIOrder::AttackRecoverBall:
		OutMovementMode =
			bShortMove
			? ESoccerAIMovementMode::Run
			: ESoccerAIMovementMode::FastRun;

		OutMovementReason =
			ESoccerAIMovementReason::FreeBallRace;

		return;

	case ESoccerAIOrder::PressBall:
		OutMovementMode =
			bShortMove
			? ESoccerAIMovementMode::Run
			: ESoccerAIMovementMode::FastRun;

		OutMovementReason =
			ESoccerAIMovementReason::PressBallCarrier;

		return;

	case ESoccerAIOrder::AttackRunIntoSpace:
		OutMovementMode =
			bMediumMove
			? ESoccerAIMovementMode::Run
			: ESoccerAIMovementMode::FastRun;

		OutMovementReason =
			ESoccerAIMovementReason::AttackRunIntoSpace;

		return;

	case ESoccerAIOrder::AttackSupportForward:
	case ESoccerAIOrder::AttackWideSupport:
		OutMovementMode =
			bShortMove
			? ESoccerAIMovementMode::Jog
			: ESoccerAIMovementMode::Run;

		OutMovementReason =
			ESoccerAIMovementReason::NormalPlay;

		return;

	case ESoccerAIOrder::DefendProtectGoalLane:
	case ESoccerAIOrder::DefendCoverCenter:
	case ESoccerAIOrder::DefendCompactShape:
	case ESoccerAIOrder::DefendMarkDangerousReceiver:
		if (bLongMove)
		{
			OutMovementMode =
				ESoccerAIMovementMode::FastRun;

			OutMovementReason =
				ESoccerAIMovementReason::DefensiveEmergencyRecovery;

			return;
		}

		if (bMediumMove)
		{
			OutMovementMode =
				ESoccerAIMovementMode::Run;

			OutMovementReason =
				ESoccerAIMovementReason::NormalPlay;

			return;
		}

		OutMovementMode =
			bShortMove
			? ESoccerAIMovementMode::Walk
			: ESoccerAIMovementMode::Jog;

		OutMovementReason =
			ESoccerAIMovementReason::ShapeSupport;

		return;

	case ESoccerAIOrder::AttackSupportShort:
	case ESoccerAIOrder::AttackRestDefense:
	case ESoccerAIOrder::AttackCompensateCover:
	case ESoccerAIOrder::SupportBall:
	case ESoccerAIOrder::MaintainTeamShape:
	case ESoccerAIOrder::ReturnHome:
		if (bShortMove)
		{
			OutMovementMode =
				ESoccerAIMovementMode::Walk;

			OutMovementReason =
				ESoccerAIMovementReason::NearbyReposition;

			return;
		}

		if (bMediumMove)
		{
			OutMovementMode =
				ESoccerAIMovementMode::Jog;

			OutMovementReason =
				ESoccerAIMovementReason::ShapeSupport;

			return;
		}

		OutMovementMode =
			ESoccerAIMovementMode::Run;

		OutMovementReason =
			ESoccerAIMovementReason::NormalPlay;

		return;

	default:
		break;
	}

	if (bShortMove)
	{
		OutMovementMode =
			ESoccerAIMovementMode::Walk;

		OutMovementReason =
			ESoccerAIMovementReason::NearbyReposition;

		return;
	}

	if (bMediumMove)
	{
		OutMovementMode =
			ESoccerAIMovementMode::Jog;

		OutMovementReason =
			ESoccerAIMovementReason::ShapeSupport;

		return;
	}

	OutMovementMode =
		ESoccerAIMovementMode::Run;

	OutMovementReason =
		ESoccerAIMovementReason::NormalPlay;
}

bool ASoccerAICharacter::IsAIMovementEmergencyReason(
	ESoccerAIMovementReason MovementReason
) const
{
	return
		MovementReason == ESoccerAIMovementReason::FreeBallRace ||
		MovementReason == ESoccerAIMovementReason::PressBallCarrier ||
		MovementReason == ESoccerAIMovementReason::DefensiveEmergencyRecovery ||
		MovementReason == ESoccerAIMovementReason::ChaseOwnAutoPass ||
		MovementReason == ESoccerAIMovementReason::GoalkeeperEmergency;
}

ESoccerAIMovementMode ASoccerAICharacter::LimitAIMovementModeByEnergy(
	ESoccerAIMovementMode DesiredMode,
	ESoccerAIMovementReason DesiredReason
) const
{
	if (DesiredMode != ESoccerAIMovementMode::FastRun)
	{
		return DesiredMode;
	}

	const float EnergyPercent =
		GetAIPlayerEnergyPercent();

	const bool bEmergency =
		IsAIMovementEmergencyReason(DesiredReason);

	if (bEmergency)
	{
		if (EnergyPercent < AIEmergencyFastRunMinimumEnergyPercent)
		{
			return ESoccerAIMovementMode::Run;
		}

		return ESoccerAIMovementMode::FastRun;
	}

	if (EnergyPercent < AINormalFastRunMinimumEnergyPercent)
	{
		return ESoccerAIMovementMode::Run;
	}

	return ESoccerAIMovementMode::FastRun;
}

void ASoccerAICharacter::ApplyPlayerProfilePhysicalTuning()
{
	if (!HasPlayerProfile())
	{
		return;
	}

	WalkSpeed = GetProfileAdjustedPaceSpeed(WalkSpeed);
	JogSpeed = GetProfileAdjustedPaceSpeed(JogSpeed);
	RunSpeed = GetProfileAdjustedPaceSpeed(RunSpeed);
	FastRunSpeed = GetProfileAdjustedPaceSpeed(FastRunSpeed);
	AIFastRunMinimumSpeed = GetProfileAdjustedPaceSpeed(AIFastRunMinimumSpeed);

	if (UCharacterMovementComponent* ProfileCharacterMovement = GetCharacterMovement())
	{
		// UE path following otherwise tends to request an immediate velocity.
		// Enabling requested-move acceleration makes the profile's Acceleration
		// value observable for bots as well as for the human player.
		ProfileCharacterMovement->bRequestedMoveUseAcceleration = true;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[PlayerProfile] AI %s applied: Pace=%d Acceleration=%d Stamina=%d Recovery=%d, FastRun=%.1f, MaxAcceleration=%.1f."),
		*GetPlayerProfileId().ToString(),
		GetPlayerProfile()->Attributes.Physical.Pace,
		GetPlayerProfile()->Attributes.Physical.Acceleration,
		GetPlayerProfile()->Attributes.Physical.Stamina,
		GetPlayerProfile()->Attributes.Physical.StaminaRecovery,
		FastRunSpeed,
		GetCharacterMovement() != nullptr ? GetCharacterMovement()->MaxAcceleration : 0.0f
	);
}

float ASoccerAICharacter::GetAIBaseSpeedForMode(
	ESoccerAIMovementMode MovementMode
) const
{
	switch (MovementMode)
	{
	case ESoccerAIMovementMode::Walk:
		return WalkSpeed;

	case ESoccerAIMovementMode::Jog:
		return JogSpeed;

	case ESoccerAIMovementMode::Run:
		return RunSpeed;

	case ESoccerAIMovementMode::FastRun:
		return FastRunSpeed;

	default:
		return RunSpeed;
	}
}


float ASoccerAICharacter::GetEnergyAdjustedAIFastRunSpeed() const
{
	const float SafeMaxEnergy =
		FMath::Max(
			1.0f,
			MaxAIPlayerEnergy
		);

	const float FullSpeedEnergy =
		FMath::Clamp(
			AIFastRunFullSpeedEnergy,
			0.0f,
			SafeMaxEnergy
		);

	const float MinimumSpeedEnergy =
		FMath::Clamp(
			AIFastRunMinimumSpeedEnergy,
			0.0f,
			FullSpeedEnergy
		);

	if (AIPlayerEnergy >= FullSpeedEnergy)
	{
		return FastRunSpeed;
	}

	if (AIPlayerEnergy <= MinimumSpeedEnergy)
	{
		return AIFastRunMinimumSpeed;
	}

	const float EnergyRange =
		FMath::Max(
			0.01f,
			FullSpeedEnergy - MinimumSpeedEnergy
		);

	const float Alpha =
		(AIPlayerEnergy - MinimumSpeedEnergy) / EnergyRange;

	return FMath::Lerp(
		AIFastRunMinimumSpeed,
		FastRunSpeed,
		Alpha
	);
}

void ASoccerAICharacter::UpdateAIMovementSpeed()
{
	if (GetCharacterMovement() == nullptr)
	{
		return;
	}

	float NewMaxWalkSpeed =
		GetAIBaseSpeedForMode(CurrentAIMovementMode);

	if (CurrentAIMovementMode == ESoccerAIMovementMode::FastRun)
	{
		NewMaxWalkSpeed =
			GetEnergyAdjustedAIFastRunSpeed();
	}

	GetCharacterMovement()->MaxWalkSpeed =
		NewMaxWalkSpeed;
}

void ASoccerAICharacter::UpdateAIPlayerEnergy(float DeltaTime)
{
	if (DeltaTime <= 0.0f || MaxAIPlayerEnergy <= 0.0f)
	{
		return;
	}

	const bool bIsMoving =
		GetVelocity().Size2D() > AIEnergyMovingSpeedThreshold;

	float EnergyChangePerSecond =
		AIIdleEnergyRecoveryPerSecond;

	if (bIsMoving)
	{
		switch (CurrentAIMovementMode)
		{
		case ESoccerAIMovementMode::FastRun:
			EnergyChangePerSecond =
				-AIFastRunEnergyDrainPerSecond;
			break;

		case ESoccerAIMovementMode::Run:
			EnergyChangePerSecond =
				-AIRunEnergyDrainPerSecond;
			break;

		case ESoccerAIMovementMode::Jog:
			EnergyChangePerSecond =
				AIJogEnergyRecoveryPerSecond;
			break;

		case ESoccerAIMovementMode::Walk:
			EnergyChangePerSecond =
				AIWalkEnergyRecoveryPerSecond;
			break;

		default:
			break;
		}
	}

	if (EnergyChangePerSecond < 0.0f)
	{
		EnergyChangePerSecond *= GetPlayerProfileStaminaDrainMultiplier();
	}
	else if (EnergyChangePerSecond > 0.0f)
	{
		EnergyChangePerSecond *= GetPlayerProfileStaminaRecoveryMultiplier();
	}

	AIPlayerEnergy =
		FMath::Clamp(
			AIPlayerEnergy + EnergyChangePerSecond * DeltaTime,
			0.0f,
			MaxAIPlayerEnergy
		);

	UpdateAIMovementSpeed();

	if (
		CurrentAIMovementMode == ESoccerAIMovementMode::FastRun &&
		CurrentAIMovementReason != ESoccerAIMovementReason::ChaseOwnAutoPass
		)
	{
		const ESoccerAIMovementMode LimitedMode =
			LimitAIMovementModeByEnergy(
				CurrentAIMovementMode,
				CurrentAIMovementReason
			);

		if (LimitedMode != CurrentAIMovementMode)
		{
			CurrentAIMovementMode = LimitedMode;
			UpdateAIMovementSpeed();
		}
	}
}

//arquero
bool ASoccerAICharacter::IsGoalkeeperActionActive() const
{
	return bGoalkeeperActionActive;
}

ESoccerGoalkeeperAction ASoccerAICharacter::GetCurrentGoalkeeperAction() const
{
	return CurrentGoalkeeperAction;
}

void ASoccerAICharacter::ClearGoalkeeperAction()
{
	ResetGoalkeeperSaveCurveMotion();

	GoalkeeperSaveAdaptiveLateralScale = 1.0f;
	GoalkeeperSaveAdaptiveLateralMaximumExtraDistance = 0.0f;
	ClearGoalkeeperSaveNearPerfectContactCorrection();

	bGoalkeeperActionActive = false;

	CurrentGoalkeeperAction =
		ESoccerGoalkeeperAction::None;

	GoalkeeperActionEndTime = -1000.0f;
}

void ASoccerAICharacter::UpdateGoalkeeperActionState()
{
	if (!bGoalkeeperActionActive)
	{
		return;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (CurrentTime >= GoalkeeperActionEndTime)
	{
		/*
		 * Aplicamos una �ltima evaluaci�n para evitar
		 * perder el peque�o desplazamiento existente
		 * entre el �ltimo Tick y el final del montage.
		 */
		CommitGoalkeeperSaveCurveMotionToEnd();

		ClearGoalkeeperAction();
	}
}

//Reemplazada para Debug

void ASoccerAICharacter::SetGoalkeeperSaveAdaptiveLateralScale(
	float NewScale,
	float MaximumExtraDistance
)
{
	// El controller puede ampliar o acortar el traslado lateral. El minimo
	// absoluto es 0.0 para que nunca se invierta la direccion de la animacion.
	GoalkeeperSaveAdaptiveLateralScale =
		FMath::Max(0.0f, NewScale);

	GoalkeeperSaveAdaptiveLateralMaximumExtraDistance =
		FMath::Max(0.0f, MaximumExtraDistance);
}

float ASoccerAICharacter::GetGoalkeeperSaveAdaptiveLateralScale() const
{
	return GoalkeeperSaveAdaptiveLateralScale;
}

void ASoccerAICharacter::SetGoalkeeperSaveNearPerfectContactCorrection(
	const FVector& NewLocalCorrection,
	float ContactMontageTime,
	float BlendInTime,
	float ReleaseTime
)
{
	GoalkeeperSaveNearPerfectContactCorrectionLocal =
		NewLocalCorrection;

	GoalkeeperSaveNearPerfectContactMontageTime =
		FMath::Max(0.0f, ContactMontageTime);

	GoalkeeperSaveNearPerfectCorrectionBlendInTime =
		FMath::Max(0.01f, BlendInTime);

	GoalkeeperSaveNearPerfectCorrectionReleaseTime =
		FMath::Max(0.01f, ReleaseTime);

	bGoalkeeperSaveNearPerfectCorrectionConfigured =
		!NewLocalCorrection.IsNearlyZero();
}

void ASoccerAICharacter::ClearGoalkeeperSaveNearPerfectContactCorrection()
{
	GoalkeeperSaveNearPerfectContactCorrectionLocal =
		FVector::ZeroVector;

	GoalkeeperSaveNearPerfectContactMontageTime = 0.0f;
	GoalkeeperSaveNearPerfectCorrectionBlendInTime = 0.18f;
	GoalkeeperSaveNearPerfectCorrectionReleaseTime = 0.22f;
	bGoalkeeperSaveNearPerfectCorrectionConfigured = false;
}

bool ASoccerAICharacter::StartGoalkeeperAction(
	ESoccerGoalkeeperAction NewGoalkeeperAction,
	float MontagePlayRate
)
{
	if (NewGoalkeeperAction == ESoccerGoalkeeperAction::None)
	{
		ClearGoalkeeperAction();
		return false;
	}

	if (bGoalkeeperActionActive)
	{
		ASoccerDebugManager::Message(
			this,
			ESoccerDebugCategory::GoalkeeperSave,
			TEXT(
				"Accion bloqueada: ya hay otra atajada activa"
			),
			FColor::Yellow,
			1001
		);

		return false;
	}

	ASoccerDebugManager* DebugManager =
		ASoccerDebugManager::Get(this);

	if (IsValid(DebugManager))
	{
		DebugManager->ClearGoalkeeperDebugMessages();
	}

	const UEnum* GoalkeeperActionEnum =
		StaticEnum<ESoccerGoalkeeperAction>();

	const FString GoalkeeperActionName =
		GoalkeeperActionEnum != nullptr
		? GoalkeeperActionEnum->GetNameStringByValue(
			static_cast<int64>(
				NewGoalkeeperAction
				)
		)
		: FString::FromInt(
			static_cast<int32>(
				NewGoalkeeperAction
				)
		);

	ASoccerDebugManager::SetPersistentLine(
		this,
		ESoccerDebugCategory::GoalkeeperSave,
		0,
		TEXT("ATAJADA DE ARQUERO"),
		FColor::Cyan,
		1.1f
	);

	ASoccerDebugManager::SetPersistentLine(
		this,
		ESoccerDebugCategory::GoalkeeperSave,
		10,
		FString::Printf(
			TEXT("Accion: %s"),
			*GoalkeeperActionName
		),
		FColor::White
	);

	ASoccerDebugManager::SetPersistentLine(
		this,
		ESoccerDebugCategory::GoalkeeperSave,
		90,
		TEXT("Resultado: PREPARANDO"),
		FColor::Cyan
	);

	UAnimMontage* MontageToPlay =
		GetGoalkeeperMontageForAction(NewGoalkeeperAction);

	if (MontageToPlay == nullptr)
	{
		ASoccerDebugManager::SetPersistentLine(
			this,
			ESoccerDebugCategory::GoalkeeperSave,
			20,
			TEXT("Montage: NULL"),
			FColor::Red
		);

		ASoccerDebugManager::SetPersistentLine(
			this,
			ESoccerDebugCategory::GoalkeeperSave,
			90,
			TEXT(
				"Resultado: CONFIGURACION INVALIDA"
			),
			FColor::Red
		);

		return false;
	}

	if (GetMesh() == nullptr)
	{
		ASoccerDebugManager::SetPersistentLine(
			this,
			ESoccerDebugCategory::GoalkeeperSave,
			90,
			TEXT("Resultado: GET MESH ES NULL"),
			FColor::Red
		);

		return false;
	}

	UAnimInstance* AnimInstance =
		GetMesh()->GetAnimInstance();

	if (AnimInstance == nullptr)
	{
		ASoccerDebugManager::SetPersistentLine(
			this,
			ESoccerDebugCategory::GoalkeeperSave,
			90,
			TEXT("Resultado: ANIM INSTANCE ES NULL"),
			FColor::Red
		);

		return false;
	}

	StopGoalkeeperRetreatBackpedalAnimation(0.05f);

	const float SafeMontagePlayRate =
		FMath::Clamp(MontagePlayRate, 0.1f, 4.0f);

	const float MontagePlayResult =
		AnimInstance->Montage_Play(
			MontageToPlay,
			SafeMontagePlayRate
		);

	const float MontageDuration =
		MontagePlayResult > 0.0f
		? MontageToPlay->GetPlayLength() / SafeMontagePlayRate
		: 0.0f;

	ASoccerDebugManager::SetPersistentLine(
		this,
		ESoccerDebugCategory::GoalkeeperSave,
		20,
		FString::Printf(
			TEXT("Montage: %s | %.3f s | rate x%.3f"),
			*MontageToPlay->GetName(),
			MontageDuration,
			SafeMontagePlayRate
		),
		MontageDuration > 0.0f
		? FColor::Green
		: FColor::Red
	);

	if (MontageDuration <= 0.0f)
	{
		return false;
	}

	ASoccerDebugManager::SetPersistentLine(
		this,
		ESoccerDebugCategory::GoalkeeperSave,
		90,
		TEXT("Resultado: ESPERANDO CONTACTO"),
		FColor::Cyan
	);

	bGoalkeeperActionActive = true;

	CurrentGoalkeeperAction =
		NewGoalkeeperAction;

	GoalkeeperActionEndTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds() +
		MontageDuration
		: MontageDuration;

	/*
	 * Eliminamos cualquier velocidad anterior de navegaci�n.
	 * Desde este punto el movimiento horizontal ser� controlado
	 * por la CurveTable de la atajada.
	 */
	if (GetCharacterMovement() != nullptr)
	{
		GetCharacterMovement()->
			StopMovementImmediately();
	}

	/*
	 * Que no exista una curva no impide reproducir la animaci�n.
	 * Simplemente se ejecutar� como una animaci�n in place.
	 */
	InitializeGoalkeeperSaveCurveMotion();

	return true;
}

UAnimMontage* ASoccerAICharacter::GetGoalkeeperMontageForAction(
	ESoccerGoalkeeperAction GoalkeeperAction
) const
{
	switch (GoalkeeperAction)
	{
		// Nuevas acciones.

	case ESoccerGoalkeeperAction::BodyBlockCatchToLeft:
		return GoalkeeperBodyBlockCatchToLeftMontage;

	case ESoccerGoalkeeperAction::BodyBlockCatchToRight:
		return GoalkeeperBodyBlockCatchToRightMontage;

	case ESoccerGoalkeeperAction::BodyBlockDeflectToLeft:
		return GoalkeeperBodyBlockDeflectToLeftMontage;

	case ESoccerGoalkeeperAction::BodyBlockDeflectToRight:
		return GoalkeeperBodyBlockDeflectToRightMontage;

	case ESoccerGoalkeeperAction::CatchAbdomen:
		return GoalkeeperCatchAbdomenMontage;

	case ESoccerGoalkeeperAction::CatchFaceToLeft:
		return GoalkeeperCatchFaceToLeftMontage;

	case ESoccerGoalkeeperAction::CatchFaceToRight:
		return GoalkeeperCatchFaceToRightMontage;

	case ESoccerGoalkeeperAction::CatchOverHeadJumpToLeft:
		return GoalkeeperCatchOverHeadJumpToLeftMontage;

	case ESoccerGoalkeeperAction::CatchOverHeadJumpToRight:
		return GoalkeeperCatchOverHeadJumpToRightMontage;

	case ESoccerGoalkeeperAction::CatchOverHeadRunJump:
		return GoalkeeperCatchOverHeadRunJumpMontage;

	case ESoccerGoalkeeperAction::DivingSaveFloorToLeft:
		return GoalkeeperDivingSaveFloorToLeftMontage;

	case ESoccerGoalkeeperAction::DivingSaveFloorToRight:
		return GoalkeeperDivingSaveFloorToRightMontage;

	case ESoccerGoalkeeperAction::DivingSaveOneMeterToLeft:
		return GoalkeeperDivingSaveOneMeterToLeftMontage;

	case ESoccerGoalkeeperAction::DivingSaveOneMeterToRight:
		return GoalkeeperDivingSaveOneMeterToRightMontage;

	case ESoccerGoalkeeperAction::ScoopToLeft:
		return GoalkeeperScoopToLeftMontage;

	case ESoccerGoalkeeperAction::ScoopToRight:
		return GoalkeeperScoopToRightMontage;

	case ESoccerGoalkeeperAction::Miss:
		return GoalkeeperMissMontage;

		/*
		 * Casos legacy temporales.
		 *
		 * Estos se eliminar�n cuando el selector autom�tico y la l�gica
		 * de contacto ya no produzcan ninguna acci�n anterior.
		 */
	case ESoccerGoalkeeperAction::CatchLow:
		return GoalkeeperCatchLowMontage;

	case ESoccerGoalkeeperAction::CatchChest:
		return GoalkeeperCatchChestMontage;

	case ESoccerGoalkeeperAction::CatchHighForward:
		return GoalkeeperCatchHighForwardMontage;

	case ESoccerGoalkeeperAction::CatchHighRight:
		return GoalkeeperCatchHighRightMontage;

	case ESoccerGoalkeeperAction::BodyBlockLeft:
		return GoalkeeperBodyBlockLeftMontage;

	case ESoccerGoalkeeperAction::BodyBlockRight:
		return GoalkeeperBodyBlockRightMontage;

	case ESoccerGoalkeeperAction::BodyBlockLeftAlt:
		return GoalkeeperBodyBlockLeftAltMontage;

	case ESoccerGoalkeeperAction::DivingSaveLeft:
		return GoalkeeperDivingSaveLeftMontage;

	case ESoccerGoalkeeperAction::DivingSaveRight:
		return GoalkeeperDivingSaveRightMontage;

	default:
		return nullptr;
	}
}

UCurveTable*
ASoccerAICharacter::
GetGoalkeeperSaveMotionCurveTable(
	ESoccerGoalkeeperAction GoalkeeperAction
) const
{
	UCurveTable* const* FoundCurveTable =
		GoalkeeperSaveMotionCurveTables.Find(
			GoalkeeperAction
		);

	return
		FoundCurveTable != nullptr
		? *FoundCurveTable
		: nullptr;
}

bool ASoccerAICharacter::
EvaluateGoalkeeperSaveLocalDisplacement(
	ESoccerGoalkeeperAction GoalkeeperAction,
	float MontageTime,
	FVector2D& OutLocalDisplacement
) const
{
	OutLocalDisplacement =
		FVector2D::ZeroVector;

	UCurveTable* CurveTable =
		GetGoalkeeperSaveMotionCurveTable(
			GoalkeeperAction
		);

	if (CurveTable == nullptr)
	{
		return false;
	}

	static const FName ForwardRowName(
		TEXT("Forward")
	);

	static const FName LateralRowName(
		TEXT("Lateral")
	);

	const FString ContextString =
		TEXT("GoalkeeperSaveCurveMotion");

	const FRealCurve* ForwardCurve =
		CurveTable->FindCurve(
			ForwardRowName,
			ContextString,
			false
		);

	const FRealCurve* LateralCurve =
		CurveTable->FindCurve(
			LateralRowName,
			ContextString,
			false
		);

	if (
		ForwardCurve == nullptr ||
		LateralCurve == nullptr
		)
	{
		return false;
	}

	const float SafeMontageTime =
		FMath::Max(
			0.0f,
			MontageTime
		);

	OutLocalDisplacement.X =
		ForwardCurve->Eval(
			SafeMontageTime
		);

	OutLocalDisplacement.Y =
		LateralCurve->Eval(
			SafeMontageTime
		);

	return true;
}

bool ASoccerAICharacter::
TryGetGoalkeeperSaveCurveMotionLocalOffset(
	ESoccerGoalkeeperAction GoalkeeperAction,
	float MontageTime,
	FVector2D& OutLocalOffset
) const
{
	OutLocalOffset =
		FVector2D::ZeroVector;

	/*
	 * Si el movimiento por curvas est� apagado,
	 * la c�psula realmente no se desplazar�.
	 *
	 * Por lo tanto, un desplazamiento cero es una
	 * evaluaci�n v�lida.
	 */
	if (!bUseGoalkeeperSaveCurveMotion)
	{
		return true;
	}

	if (
		GoalkeeperAction ==
		ESoccerGoalkeeperAction::None
		)
	{
		return false;
	}

	FVector2D InitialLocalDisplacement =
		FVector2D::ZeroVector;

	FVector2D LocalDisplacementAtTime =
		FVector2D::ZeroVector;

	if (
		!EvaluateGoalkeeperSaveLocalDisplacement(
			GoalkeeperAction,
			0.0f,
			InitialLocalDisplacement
		)
		)
	{
		return false;
	}

	if (
		!EvaluateGoalkeeperSaveLocalDisplacement(
			GoalkeeperAction,
			FMath::Max(
				0.0f,
				MontageTime
			),
			LocalDisplacementAtTime
		)
		)
	{
		return false;
	}

	/*
	 * Debe coincidir exactamente con la l�gica utilizada
	 * al mover la c�psula durante el montage.
	 */
	OutLocalOffset =
		(
			LocalDisplacementAtTime -
			InitialLocalDisplacement
			) *
		GoalkeeperSaveCurveMotionScale;

	return true;
}

bool ASoccerAICharacter::
InitializeGoalkeeperSaveCurveMotion()
{
	ResetGoalkeeperSaveCurveMotion();

	if (
		!bUseGoalkeeperSaveCurveMotion ||
		!bGoalkeeperActionActive ||
		CurrentGoalkeeperAction ==
		ESoccerGoalkeeperAction::None
		)
	{
		return false;
	}

	FVector2D InitialLocalDisplacement;

	if (
		!EvaluateGoalkeeperSaveLocalDisplacement(
			CurrentGoalkeeperAction,
			0.0f,
			InitialLocalDisplacement
		)
		)
	{
		return false;
	}

	GoalkeeperSaveMotionStartLocation =
		GetActorLocation();

	if (GetMesh() != nullptr)
	{
		GoalkeeperSaveMotionInitialMeshRelativeLocation =
			GetMesh()->GetRelativeLocation();
	}

	GoalkeeperSaveMotionForwardDirection =
		GetActorForwardVector();

	GoalkeeperSaveMotionForwardDirection.Z =
		0.0f;

	GoalkeeperSaveMotionForwardDirection =
		GoalkeeperSaveMotionForwardDirection.
		GetSafeNormal();

	GoalkeeperSaveMotionRightDirection =
		GetActorRightVector();

	GoalkeeperSaveMotionRightDirection.Z =
		0.0f;

	GoalkeeperSaveMotionRightDirection =
		GoalkeeperSaveMotionRightDirection.
		GetSafeNormal();

	if (
		GoalkeeperSaveMotionForwardDirection.
		IsNearlyZero() ||
		GoalkeeperSaveMotionRightDirection.
		IsNearlyZero()
		)
	{
		ResetGoalkeeperSaveCurveMotion();
		return false;
	}

	/*
	 * Aunque los CSV comienzan en cero, guardamos la
	 * primera muestra para evitar cualquier salto si una
	 * tabla futura comienza con un valor diferente.
	 */
	GoalkeeperSaveMotionInitialLocalDisplacement =
		InitialLocalDisplacement;

	bGoalkeeperSaveCurveMotionInitialized =
		true;

	return true;
}

float ASoccerAICharacter::
GetGoalkeeperSaveNearPerfectContactCorrectionAlpha(
	float MontageTime
) const
{
	if (!bGoalkeeperSaveNearPerfectCorrectionConfigured)
	{
		return 0.0f;
	}

	const float ContactTime =
		FMath::Max(0.0f, GoalkeeperSaveNearPerfectContactMontageTime);

	UAnimMontage* GoalkeeperMontage =
		GetGoalkeeperMontageForAction(CurrentGoalkeeperAction);

	const float MontageLength =
		IsValid(GoalkeeperMontage)
		? GoalkeeperMontage->GetPlayLength()
		: ContactTime + GoalkeeperSaveNearPerfectCorrectionReleaseTime;

	const float BlendInStart =
		FMath::Max(
			0.0f,
			ContactTime - GoalkeeperSaveNearPerfectCorrectionBlendInTime
		);

	if (MontageTime <= BlendInStart)
	{
		return 0.0f;
	}

	if (MontageTime < ContactTime)
	{
		const float Denominator =
			FMath::Max(KINDA_SMALL_NUMBER, ContactTime - BlendInStart);

		const float Alpha =
			FMath::Clamp((MontageTime - BlendInStart) / Denominator, 0.0f, 1.0f);

		return Alpha * Alpha * (3.0f - 2.0f * Alpha);
	}

	const float ReleaseEnd =
		FMath::Min(
			MontageLength,
			ContactTime + GoalkeeperSaveNearPerfectCorrectionReleaseTime
		);

	if (ReleaseEnd <= ContactTime + KINDA_SMALL_NUMBER)
	{
		return MontageTime <= ContactTime ? 1.0f : 0.0f;
	}

	if (MontageTime >= ReleaseEnd)
	{
		return 0.0f;
	}

	const float ReleaseAlpha =
		FMath::Clamp(
			(MontageTime - ContactTime) / (ReleaseEnd - ContactTime),
			0.0f,
			1.0f
		);

	const float SmoothRelease =
		ReleaseAlpha * ReleaseAlpha * (3.0f - 2.0f * ReleaseAlpha);

	return 1.0f - SmoothRelease;
}

bool ASoccerAICharacter::
ApplyGoalkeeperSaveCurveMotionAtTime(
	float MontageTime
)
{
	if (
		!bUseGoalkeeperSaveCurveMotion ||
		!bGoalkeeperSaveCurveMotionInitialized ||
		!bGoalkeeperActionActive
		)
	{
		return false;
	}

	FVector2D LocalDisplacement;

	if (
		!EvaluateGoalkeeperSaveLocalDisplacement(
			CurrentGoalkeeperAction,
			MontageTime,
			LocalDisplacement
		)
		)
	{
		return false;
	}

	LocalDisplacement -=
		GoalkeeperSaveMotionInitialLocalDisplacement;

	LocalDisplacement *=
		GoalkeeperSaveCurveMotionScale;

	// La adaptacion conserva exactamente la forma temporal de la curva y solo
	// escala su componente lateral. Forward y Z permanecen sin cambios.
	const float BaseLateralDisplacement =
		LocalDisplacement.Y;

	const float ScaledLateralDisplacement =
		BaseLateralDisplacement *
		GoalkeeperSaveAdaptiveLateralScale;

	const float RequestedExtraLateral =
		ScaledLateralDisplacement -
		BaseLateralDisplacement;

	const float ClampedExtraLateral =
		FMath::Clamp(
			RequestedExtraLateral,
			-GoalkeeperSaveAdaptiveLateralMaximumExtraDistance,
			GoalkeeperSaveAdaptiveLateralMaximumExtraDistance
		);

	LocalDisplacement.Y =
		BaseLateralDisplacement +
		ClampedExtraLateral;

	const float NearPerfectCorrectionAlpha =
		GetGoalkeeperSaveNearPerfectContactCorrectionAlpha(MontageTime);

	const FVector EffectiveNearPerfectCorrectionLocal =
		GoalkeeperSaveNearPerfectContactCorrectionLocal *
		NearPerfectCorrectionAlpha;

	const FVector DesiredHorizontalOffset =
		GoalkeeperSaveMotionForwardDirection *
		(LocalDisplacement.X + EffectiveNearPerfectCorrectionLocal.X)
		+
		GoalkeeperSaveMotionRightDirection *
		(LocalDisplacement.Y + EffectiveNearPerfectCorrectionLocal.Y);

	FVector DesiredActorLocation =
		GoalkeeperSaveMotionStartLocation +
		DesiredHorizontalOffset;

	// La capsula conserva su altura. La correccion Up mueve solamente el Mesh
	// alrededor del contacto: asi una pequena correccion hacia abajo puede existir
	// aunque la capsula este apoyada sobre el piso.
	DesiredActorLocation.Z = GetActorLocation().Z;

	if (GetMesh() != nullptr)
	{
		FVector DesiredMeshRelativeLocation =
			GoalkeeperSaveMotionInitialMeshRelativeLocation;

		DesiredMeshRelativeLocation.Z +=
			EffectiveNearPerfectCorrectionLocal.Z;

		GetMesh()->SetRelativeLocation(DesiredMeshRelativeLocation);
	}

	FVector MovementDelta =
		DesiredActorLocation -
		GetActorLocation();

	MovementDelta.Z = 0.0f;

	if (!MovementDelta.IsNearlyZero())
	{
		FHitResult MovementHit;

		AddActorWorldOffset(
			MovementDelta,
			true,
			&MovementHit,
			ETeleportType::None
		);
	}

	if (
		ASoccerDebugManager::IsWorldDrawingEnabled(this, ESoccerDebugCategory::GoalkeeperSave) &&
		bDebugGoalkeeperSaveCurveMotion &&
		GetWorld() != nullptr
		)
	{
		DrawDebugLine(
			GetWorld(),
			GoalkeeperSaveMotionStartLocation,
			DesiredActorLocation,
			FColor::Magenta,
			false,
			0.06f,
			0,
			2.0f
		);
	}

	return true;
}

void ASoccerAICharacter::
UpdateGoalkeeperSaveCurveMotion()
{
	if (
		!bUseGoalkeeperSaveCurveMotion ||
		!bGoalkeeperSaveCurveMotionInitialized ||
		!bGoalkeeperActionActive
		)
	{
		return;
	}

	float MontagePosition = 0.0f;
	float MontageLength = 0.0f;

	/*
	 * Este m�todo ya fue agregado para el tester.
	 * No vuelvas a declararlo ni a implementarlo.
	 */
	if (
		!GetGoalkeeperActionMontagePlaybackState(
			CurrentGoalkeeperAction,
			MontagePosition,
			MontageLength
		)
		)
	{
		return;
	}

	const float SafeMontagePosition =
		FMath::Clamp(
			MontagePosition,
			0.0f,
			MontageLength
		);

	ApplyGoalkeeperSaveCurveMotionAtTime(
		SafeMontagePosition
	);
}

void ASoccerAICharacter::
CommitGoalkeeperSaveCurveMotionToEnd()
{
	if (
		!bGoalkeeperSaveCurveMotionInitialized ||
		CurrentGoalkeeperAction ==
		ESoccerGoalkeeperAction::None
		)
	{
		return;
	}

	UAnimMontage* GoalkeeperMontage =
		GetGoalkeeperMontageForAction(
			CurrentGoalkeeperAction
		);

	if (GoalkeeperMontage == nullptr)
	{
		return;
	}

	ApplyGoalkeeperSaveCurveMotionAtTime(
		GoalkeeperMontage->GetPlayLength()
	);
}

void ASoccerAICharacter::
ResetGoalkeeperSaveCurveMotion()
{
	if (GetMesh() != nullptr && bGoalkeeperSaveCurveMotionInitialized)
	{
		GetMesh()->SetRelativeLocation(
			GoalkeeperSaveMotionInitialMeshRelativeLocation
		);
	}

	bGoalkeeperSaveCurveMotionInitialized =
		false;

	GoalkeeperSaveMotionStartLocation =
		FVector::ZeroVector;

	GoalkeeperSaveMotionInitialMeshRelativeLocation =
		FVector::ZeroVector;

	GoalkeeperSaveMotionForwardDirection =
		FVector::ForwardVector;

	GoalkeeperSaveMotionRightDirection =
		FVector::RightVector;

	GoalkeeperSaveMotionInitialLocalDisplacement =
		FVector2D::ZeroVector;

}

bool ASoccerAICharacter::
GetGoalkeeperActionMontagePlaybackState(
	ESoccerGoalkeeperAction GoalkeeperAction,
	float& OutMontagePosition,
	float& OutMontageLength
) const
{
	OutMontagePosition = 0.0f;
	OutMontageLength = 0.0f;

	if (GetMesh() == nullptr)
	{
		return false;
	}

	UAnimMontage* GoalkeeperMontage =
		GetGoalkeeperMontageForAction(
			GoalkeeperAction
		);

	if (GoalkeeperMontage == nullptr)
	{
		return false;
	}

	UAnimInstance* AnimInstance =
		GetMesh()->GetAnimInstance();

	if (AnimInstance == nullptr)
	{
		return false;
	}

	if (
		!AnimInstance->Montage_IsActive(
			GoalkeeperMontage
		)
		)
	{
		return false;
	}

	OutMontagePosition =
		AnimInstance->Montage_GetPosition(
			GoalkeeperMontage
		);

	OutMontageLength =
		GoalkeeperMontage->GetPlayLength();

	return
		OutMontageLength >
		KINDA_SMALL_NUMBER;
}

void ASoccerAICharacter::
StopGoalkeeperActionMontage(
	float BlendOutTime
)
{
	if (GetMesh() != nullptr)
	{
		UAnimInstance* AnimInstance =
			GetMesh()->GetAnimInstance();

		UAnimMontage* GoalkeeperMontage =
			GetGoalkeeperMontageForAction(
				CurrentGoalkeeperAction
			);

		if (
			AnimInstance != nullptr &&
			GoalkeeperMontage != nullptr &&
			AnimInstance->Montage_IsActive(
				GoalkeeperMontage
			)
			)
		{
			AnimInstance->Montage_Stop(
				FMath::Max(
					0.0f,
					BlendOutTime
				),
				GoalkeeperMontage
			);
		}
	}

	ClearGoalkeeperAction();
}

UAnimMontage*
ASoccerAICharacter::GetGoalkeeperDistributionMontage(
	ESoccerGoalkeeperDistributionType DistributionType
) const
{
	switch (DistributionType)
	{
	case ESoccerGoalkeeperDistributionType::OverhandThrow:
		return GoalkeeperOverhandThrowMontage;

	case ESoccerGoalkeeperDistributionType::DropKick:
		return GoalkeeperDropKickMontage;

	case ESoccerGoalkeeperDistributionType::PlacingBallShort:
		return GoalkeeperPlacingBallShortMontage;

	case ESoccerGoalkeeperDistributionType::PlacingBallLong:
		return GoalkeeperPlacingBallLongMontage;

	default:
		return nullptr;
	}
}