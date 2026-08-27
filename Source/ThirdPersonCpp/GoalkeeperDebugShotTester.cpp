#include "GoalkeeperDebugShotTester.h"

#include "SoccerAICharacter.h"
#include "SoccerAIController.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "AnimNotifyState_GKContactWindow.h"

#include "Animation/AnimMontage.h"
//#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"

AGoalkeeperDebugShotTester::AGoalkeeperDebugShotTester()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGoalkeeperDebugShotTester::BeginPlay()
{
	Super::BeginPlay();

	ResolveReferences();

	if (IsValid(GoalkeeperCharacter))
	{
		InitialGoalkeeperTransform =
			GoalkeeperCharacter->GetActorTransform();

		bInitialGoalkeeperTransformCaptured =
			true;
	}

	if (
		bAutoRunOnBeginPlay &&
		GetWorld() != nullptr
		)
	{
		GetWorldTimerManager().SetTimer(
			AutoRunTimerHandle,
			this,
			&AGoalkeeperDebugShotTester::RunConfiguredTest,
			FMath::Max(0.0f, AutoRunDelay),
			false
		);
	}
}

void AGoalkeeperDebugShotTester::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	(void)DeltaTime;

	if (
		bEnableRuntimeHotkeys &&
		GetWorld() != nullptr
		)
	{
		APlayerController* PlayerController =
			GetWorld()->GetFirstPlayerController();

		if (IsValid(PlayerController))
		{
			if (
				PlayerController->
				WasInputKeyJustPressed(
					EKeys::F9
				)
				)
			{
				RunConfiguredTest();
			}
			else if (
				PlayerController->
				WasInputKeyJustPressed(
					EKeys::F10
				)
				)
			{
				ResetConfiguredTest();
			}
		}
	}

	if (bDrawTarget)
	{
		DrawConfiguredTarget();
	}

	switch (TestState)
	{
	case EGoalkeeperDebugShotTestState::WaitingForLaunch:
		UpdateWaitingForLaunch();
		break;

	case EGoalkeeperDebugShotTestState::ShotLaunched:
		UpdateShotLaunched();
		break;

	default:
		break;
	}
}

void AGoalkeeperDebugShotTester::RunConfiguredTest()
{
	if (
		GetWorld() == nullptr ||
		!GetWorld()->IsGameWorld()
		)
	{
		return;
	}

	ResetConfiguredTest();

	if (!ResolveReferences())
	{
		MarkTestFailed(
			TEXT("No se encontraron arquero y pelota")
		);
		return;
	}

	if (
		TestAction == ESoccerGoalkeeperAction::None
		)
	{
		MarkTestFailed(
			TEXT("La accion elegida es None")
		);
		return;
	}

	ASoccerAIController* GoalkeeperController =
		GetGoalkeeperController();

	if (!IsValid(GoalkeeperController))
	{
		MarkTestFailed(
			TEXT("El arquero no usa SoccerAIController")
		);
		return;
	}

	USkeletalMeshComponent* CharacterMesh =
		GoalkeeperCharacter->GetMesh();

	if (CharacterMesh == nullptr)
	{
		MarkTestFailed(
			TEXT("El arquero no tiene Skeletal Mesh")
		);
		return;
	}

	FVector InitialTargetLocation = FVector::ZeroVector;

	if (!GetCurrentTargetLocation(InitialTargetLocation))
	{
		MarkTestFailed(
			FString::Printf(
				TEXT("Objetivo invalido: %s"),
				*GetTargetName()
			)
		);
		return;
	}

	UAnimMontage* GoalkeeperMontage =
		GoalkeeperCharacter->GetGoalkeeperMontageForAction(
			TestAction
		);

	if (!IsValid(GoalkeeperMontage))
	{
		MarkTestFailed(
			TEXT("La accion no tiene montage asignado")
		);
		return;
	}

	if (
		!FindContactWindowTimes(
			GoalkeeperMontage,
			ContactWindowStartTime,
			ContactWindowEndTime
		)
		)
	{
		MarkTestFailed(
			TEXT("El montage no tiene GKContactWindow")
		);
		return;
	}

	const float SafeWindowAlpha =
		FMath::Clamp(
			ContactWindowAlpha,
			0.0f,
			1.0f
		);

	DesiredImpactMontageTime =
		FMath::Lerp(
			ContactWindowStartTime,
			ContactWindowEndTime,
			SafeWindowAlpha
		);

	ScheduledLaunchMontageTime =
		FMath::Max(
			0.0f,
			DesiredImpactMontageTime -
			FMath::Max(0.02f, BallTravelTime)
		);

	if (GoalkeeperCharacter->IsAIPossessingBall())
	{
		GoalkeeperCharacter->ReleaseAIBall();
	}

	SoccerBall->SetActorEnableCollision(true);
	SoccerBall->SetPossessed(false);
	SoccerBall->StopBallKeepingPhysics();

	FVector ParkingDirection =
		GoalkeeperCharacter->GetActorForwardVector();

	ParkingDirection.Z = 0.0f;
	ParkingDirection = ParkingDirection.GetSafeNormal();

	if (ParkingDirection.IsNearlyZero())
	{
		ParkingDirection = FVector::ForwardVector;
	}

	FVector ParkingLocation =
		GoalkeeperCharacter->GetActorLocation() +
		ParkingDirection * 700.0f;

	ParkingLocation.Z = 
		InitialTargetLocation.Z;

	SoccerBall->SetActorLocation(
		ParkingLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	SoccerBall->SetActorLocation(
		ParkingLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	const bool bStarted =
		GoalkeeperController->StartGoalkeeperDebugSaveTest(
			GoalkeeperCharacter,
			SoccerBall,
			TestAction,
			bAllowEmergencyBodyContactOutsideWindow
		);

	if (!bStarted)
	{
		MarkTestFailed(
			TEXT("No se pudo iniciar la atajada")
		);
		return;
	}

	TestState =
		EGoalkeeperDebugShotTestState::WaitingForLaunch;

	SetTestPanelLine(
		100,
		TEXT("PRUEBA CONTROLADA"),
		FColor::Cyan,
		1.1f
	);

	SetTestPanelLine(
		110,
		FString::Printf(
			TEXT("Accion: %s"),
			*GetActionName()
		),
		FColor::White
	);

	SetTestPanelLine(
		120,
		FString::Printf(
			TEXT("Objetivo: %s"),
			*GetTargetName()
		),
		FColor::White
	);

	SetTestPanelLine(
		130,
		FString::Printf(
			TEXT("Ventana: %.3f - %.3f s"),
			ContactWindowStartTime,
			ContactWindowEndTime
		),
		FColor::White
	);

	SetTestPanelLine(
		140,
		FString::Printf(
			TEXT("Impacto pedido: %.0f%% | %.3f s"),
			SafeWindowAlpha * 100.0f,
			DesiredImpactMontageTime
		),
		FColor::Cyan
	);

	SetTestPanelLine(
		190,
		TEXT("Tester: ESPERANDO LANZAMIENTO"),
		FColor::Cyan
	);
}

void AGoalkeeperDebugShotTester::ResetConfiguredTest()
{
	ASoccerAIController* GoalkeeperController =
		GetGoalkeeperController();

	if (IsValid(GoalkeeperController))
	{
		GoalkeeperController->FinishGoalkeeperDebugSaveTest(
			GoalkeeperCharacter
		);
	}

	if (IsValid(GoalkeeperCharacter))
	{
		if (GoalkeeperCharacter->IsAIPossessingBall())
		{
			GoalkeeperCharacter->ReleaseAIBall();
		}

		GoalkeeperCharacter->StopGoalkeeperActionMontage(
			0.08f
		);
		if (bInitialGoalkeeperTransformCaptured)
		{
			if (
				GoalkeeperCharacter->
				GetCharacterMovement() != nullptr
				)
			{
				GoalkeeperCharacter->
					GetCharacterMovement()->
					StopMovementImmediately();
			}

			GoalkeeperCharacter->SetActorTransform(
				InitialGoalkeeperTransform,
				false,
				nullptr,
				ETeleportType::TeleportPhysics
			);
		}
	}

	if (IsValid(SoccerBall))
	{
		SoccerBall->SetActorEnableCollision(true);
		SoccerBall->SetPossessed(false);
		SoccerBall->StopBallKeepingPhysics();
	}

	TestState =
		EGoalkeeperDebugShotTestState::Idle;

	ContactWindowStartTime = 0.0f;
	ContactWindowEndTime = 0.0f;
	DesiredImpactMontageTime = 0.0f;
	ScheduledLaunchMontageTime = 0.0f;
	ShotLaunchWorldTime = -1000.0f;
}

bool AGoalkeeperDebugShotTester::ResolveReferences()
{
	if (
		IsValid(GoalkeeperCharacter) &&
		IsValid(SoccerBall)
		)
	{
		return true;
	}

	if (!bAutoFindReferences || GetWorld() == nullptr)
	{
		return false;
	}

	if (!IsValid(GoalkeeperCharacter))
	{
		for (
			TActorIterator<ASoccerAICharacter> It(GetWorld());
			It;
			++It
			)
		{
			ASoccerAICharacter* Candidate = *It;

			if (
				IsValid(Candidate) &&
				Candidate->GetPlayerRole() ==
				ESoccerPlayerRole::Goalkeeper
				)
			{
				GoalkeeperCharacter = Candidate;
				break;
			}
		}
	}

	if (!IsValid(SoccerBall))
	{
		for (
			TActorIterator<ASoccerBall> It(GetWorld());
			It;
			++It
			)
		{
			SoccerBall = *It;
			break;
		}
	}

	return
		IsValid(GoalkeeperCharacter) &&
		IsValid(SoccerBall);
}

ASoccerAIController*
AGoalkeeperDebugShotTester::GetGoalkeeperController() const
{
	return IsValid(GoalkeeperCharacter)
		? Cast<ASoccerAIController>(
			GoalkeeperCharacter->GetController()
		)
		: nullptr;
}

bool AGoalkeeperDebugShotTester::FindContactWindowTimes(
	UAnimMontage* Montage,
	float& OutWindowStartTime,
	float& OutWindowEndTime
) const
{
	OutWindowStartTime = 0.0f;
	OutWindowEndTime = 0.0f;

	if (!IsValid(Montage))
	{
		return false;
	}

	for (
		const FAnimNotifyEvent& NotifyEvent :
		Montage->Notifies
		)
	{
		if (
			NotifyEvent.NotifyStateClass != nullptr &&
			NotifyEvent.NotifyStateClass->IsA<
				UAnimNotifyState_GKContactWindow
			>()
			)
		{
			OutWindowStartTime =
				NotifyEvent.GetTriggerTime();

			OutWindowEndTime =
				OutWindowStartTime +
				NotifyEvent.GetDuration();

			return
				OutWindowEndTime >
				OutWindowStartTime;
		}
	}

	return false;
}

bool AGoalkeeperDebugShotTester::GetCurrentTargetLocation(
	FVector& OutTargetLocation
) const
{
	OutTargetLocation = FVector::ZeroVector;

	if (
		!IsValid(GoalkeeperCharacter) ||
		GoalkeeperCharacter->GetMesh() == nullptr
		)
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh =
		GoalkeeperCharacter->GetMesh();

	if (
		TargetMode ==
		EGoalkeeperDebugTargetMode::HandsMidpoint
		)
	{
		const FName LeftHandBone(TEXT("LeftHand"));
		const FName RightHandBone(TEXT("RightHand"));

		if (
			CharacterMesh->GetBoneIndex(LeftHandBone) == INDEX_NONE ||
			CharacterMesh->GetBoneIndex(RightHandBone) == INDEX_NONE
			)
		{
			return false;
		}

		OutTargetLocation =
			(
				CharacterMesh->GetBoneLocation(
					LeftHandBone,
					EBoneSpaces::WorldSpace
				) +
				CharacterMesh->GetBoneLocation(
					RightHandBone,
					EBoneSpaces::WorldSpace
				)
			) * 0.5f;

		return true;
	}

	if (
		TargetBoneName.IsNone() ||
		CharacterMesh->GetBoneIndex(TargetBoneName) == INDEX_NONE
		)
	{
		return false;
	}

	OutTargetLocation =
		CharacterMesh->GetBoneLocation(
			TargetBoneName,
			EBoneSpaces::WorldSpace
		);

	return true;
}

void AGoalkeeperDebugShotTester::UpdateWaitingForLaunch()
{
	if (
		!IsValid(GoalkeeperCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		MarkTestFailed(
			TEXT("Se perdio la referencia de prueba")
		);
		return;
	}

	float MontagePosition = 0.0f;
	float MontageLength = 0.0f;

	if (
		!GoalkeeperCharacter->
		GetGoalkeeperActionMontagePlaybackState(
			TestAction,
			MontagePosition,
			MontageLength
		)
		)
	{
		MarkTestFailed(
			TEXT("El montage dejo de estar activo antes del lanzamiento")
		);
		return;
	}

	SetTestPanelLine(
		150,
		FString::Printf(
			TEXT("Montage: %.3f / %.3f s"),
			MontagePosition,
			MontageLength
		),
		FColor::White
	);

	if (
		MontagePosition >=
		ScheduledLaunchMontageTime
		)
	{
		LaunchBallTowardCurrentTarget();
	}
}

void AGoalkeeperDebugShotTester::LaunchBallTowardCurrentTarget()
{
	FVector TargetLocation = FVector::ZeroVector;

	if (!GetCurrentTargetLocation(TargetLocation))
	{
		MarkTestFailed(
			TEXT("No se pudo obtener el objetivo al lanzar")
		);
		return;
	}

	FVector IncomingDirection =
		GoalkeeperCharacter->GetActorForwardVector();

	IncomingDirection.Z = 0.0f;
	IncomingDirection = IncomingDirection.GetSafeNormal();

	if (IncomingDirection.IsNearlyZero())
	{
		IncomingDirection = FVector::ForwardVector;
	}

	FVector StartLocation =
		TargetLocation +
		IncomingDirection *
		FMath::Max(40.0f, BallStartDistance);

	StartLocation.Z +=
		BallStartHeightOffset;

	const FVector ShotDirection =
		(TargetLocation - StartLocation).GetSafeNormal();

	if (ShotDirection.IsNearlyZero())
	{
		MarkTestFailed(
			TEXT("La direccion del disparo es invalida")
		);
		return;
	}

	const float SafeTravelTime =
		FMath::Max(0.02f, BallTravelTime);

	const float ShotSpeed =
		FVector::Dist(
			StartLocation,
			TargetLocation
		) /
		SafeTravelTime;

	SoccerBall->SetActorEnableCollision(true);
	SoccerBall->SetPossessed(false);
	SoccerBall->StopBallKeepingPhysics();

	SoccerBall->SetActorLocation(
		StartLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	SoccerBall->Kick(
		ShotDirection,
		ShotSpeed,
		0.0f
	);

	ShotLaunchWorldTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	TestState =
		EGoalkeeperDebugShotTestState::ShotLaunched;

	ASoccerDebugManager::DrawLine(
		this,
		ESoccerDebugCategory::GoalkeeperTest,
		StartLocation,
		TargetLocation,
		FColor::Cyan,
		2.0f,
		2.5f
	);

	ASoccerDebugManager::DrawSphere(
		this,
		ESoccerDebugCategory::GoalkeeperTest,
		TargetLocation,
		TargetSphereRadius,
		FColor::Green,
		2.0f,
		16,
		2.0f
	);

	SetTestPanelLine(
		160,
		FString::Printf(
			TEXT("Lanzamiento: %.0f cm/s | %.3f s"),
			ShotSpeed,
			SafeTravelTime
		),
		FColor::Cyan
	);

	SetTestPanelLine(
		190,
		TEXT("Tester: PELOTA LANZADA"),
		FColor::Green
	);
}

void AGoalkeeperDebugShotTester::UpdateShotLaunched()
{
	float MontagePosition = 0.0f;
	float MontageLength = 0.0f;

	const bool bMontageStillActive =
		IsValid(GoalkeeperCharacter) &&
		GoalkeeperCharacter->
		GetGoalkeeperActionMontagePlaybackState(
			TestAction,
			MontagePosition,
			MontageLength
		);

	if (bMontageStillActive)
	{
		SetTestPanelLine(
			150,
			FString::Printf(
				TEXT("Montage: %.3f / %.3f s"),
				MontagePosition,
				MontageLength
			),
			FColor::White
		);
	}

	const float CurrentWorldTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (
		!bMontageStillActive ||
		CurrentWorldTime - ShotLaunchWorldTime > 2.5f
		)
	{
		ASoccerAIController* GoalkeeperController =
			GetGoalkeeperController();

		if (IsValid(GoalkeeperController))
		{
			GoalkeeperController->FinishGoalkeeperDebugSaveTest(
				GoalkeeperCharacter
			);
		}

		TestState =
			EGoalkeeperDebugShotTestState::Finished;

		SetTestPanelLine(
			190,
			TEXT("Tester: PRUEBA FINALIZADA"),
			FColor::White
		);
	}
}

void AGoalkeeperDebugShotTester::DrawConfiguredTarget() const
{
	FVector TargetLocation = FVector::ZeroVector;

	if (!GetCurrentTargetLocation(TargetLocation))
	{
		return;
	}

	ASoccerDebugManager::DrawSphere(
		this,
		ESoccerDebugCategory::GoalkeeperTest,
		TargetLocation,
		TargetSphereRadius,
		FColor::Cyan,
		FMath::Max(0.02f, DebugDrawingLifetime),
		16,
		1.5f
	);
}

void AGoalkeeperDebugShotTester::SetTestPanelLine(
	int32 Slot,
	const FString& Text,
	const FColor& Color,
	float Scale
) const
{
	ASoccerDebugManager::SetPersistentLine(
		this,
		ESoccerDebugCategory::GoalkeeperTest,
		Slot,
		Text,
		Color,
		Scale
	);
}

void AGoalkeeperDebugShotTester::MarkTestFailed(
	const FString& Reason
)
{
	TestState =
		EGoalkeeperDebugShotTestState::Failed;

	SetTestPanelLine(
		100,
		TEXT("PRUEBA CONTROLADA"),
		FColor::Red,
		1.1f
	);

	SetTestPanelLine(
		190,
		FString::Printf(
			TEXT("Tester: ERROR - %s"),
			*Reason
		),
		FColor::Red
	);
}

FString AGoalkeeperDebugShotTester::GetActionName() const
{
	const UEnum* ActionEnum =
		StaticEnum<ESoccerGoalkeeperAction>();

	return ActionEnum != nullptr
		? ActionEnum->GetNameStringByValue(
			static_cast<int64>(TestAction)
		)
		: FString::FromInt(
			static_cast<int32>(TestAction)
		);
}

FString AGoalkeeperDebugShotTester::GetTargetName() const
{
	if (
		TargetMode ==
		EGoalkeeperDebugTargetMode::HandsMidpoint
		)
	{
		return TEXT("HandsMidpoint");
	}

	return TargetBoneName.ToString();
}
