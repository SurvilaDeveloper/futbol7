#include "SoccerAerialChestDebugTester.h"

#include "SoccerBall.h"
#include "SoccerCharacterBase.h"
#include "SoccerAICharacter.h"
#include "SoccerAIController.h"
#include "SoccerMatchManager.h"
#include "ThirdPersonCppCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

namespace
{
	float AerialDebugPointSegmentDistance(
		const FVector& Point,
		const FVector& SegmentStart,
		const FVector& SegmentEnd,
		FVector& OutClosestPoint
	)
	{
		const FVector Segment = SegmentEnd - SegmentStart;
		const float LengthSquared = Segment.SizeSquared();
		float Alpha = 0.0f;

		if (LengthSquared > KINDA_SMALL_NUMBER)
		{
			Alpha = FMath::Clamp(
				FVector::DotProduct(Point - SegmentStart, Segment) /
					LengthSquared,
				0.0f,
				1.0f
			);
		}

		OutClosestPoint = SegmentStart + Segment * Alpha;
		return FVector::Dist(Point, OutClosestPoint);
	}

	FString AerialContactSurfaceName(
		ESoccerAerialContactSurface Surface
	)
	{
		switch (Surface)
		{
		case ESoccerAerialContactSurface::Head:
			return TEXT("HEAD");

		case ESoccerAerialContactSurface::Chest:
			return TEXT("CHEST");

		case ESoccerAerialContactSurface::None:
		default:
			return TEXT("NONE");
		}
	}
}

ASoccerAerialChestDebugTester::ASoccerAerialChestDebugTester()
{
	PrimaryActorTick.bCanEverTick = true;

	DebugRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DebugRoot"));
	RootComponent = DebugRoot;

	WorldStatusTextShadow = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("WorldStatusTextShadow")
	);
	WorldStatusTextShadow->SetupAttachment(RootComponent);
	WorldStatusTextShadow->SetHorizontalAlignment(EHTA_Center);
	WorldStatusTextShadow->SetVerticalAlignment(EVRTA_TextCenter);
	WorldStatusTextShadow->SetWorldSize(WorldStatusTextSize);
	WorldStatusTextShadow->SetTextRenderColor(FColor::Black);
	WorldStatusTextShadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WorldStatusTextShadow->SetCastShadow(false);
	WorldStatusTextShadow->SetHiddenInGame(false);
	WorldStatusTextShadow->SetVisibility(true);
	WorldStatusTextShadow->SetTranslucentSortPriority(99);

	WorldStatusText = CreateDefaultSubobject<UTextRenderComponent>(
		TEXT("WorldStatusText")
	);
	WorldStatusText->SetupAttachment(RootComponent);
	WorldStatusText->SetHorizontalAlignment(EHTA_Center);
	WorldStatusText->SetVerticalAlignment(EVRTA_TextCenter);
	WorldStatusText->SetWorldSize(WorldStatusTextSize);
	WorldStatusText->SetTextRenderColor(FColor::Cyan);
	WorldStatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WorldStatusText->SetCastShadow(false);
	WorldStatusText->SetHiddenInGame(false);
	WorldStatusText->SetVisibility(true);
	WorldStatusText->SetTranslucentSortPriority(100);
}

void ASoccerAerialChestDebugTester::BeginPlay()
{
	Super::BeginPlay();

	ResolveReferences();
	CaptureInitialState();

	if (
		bAutoRunOnBeginPlay &&
		GetWorld() != nullptr
	)
	{
		GetWorldTimerManager().SetTimer(
			AutoRunTimerHandle,
			this,
			&ASoccerAerialChestDebugTester::RunConfiguredTest,
			FMath::Max(0.0f, AutoRunDelay),
			false
		);
	}
}

void ASoccerAerialChestDebugTester::EndPlay(
	const EEndPlayReason::Type EndPlayReason
)
{
	ResetConfiguredTest();
	Super::EndPlay(EndPlayReason);
}

void ASoccerAerialChestDebugTester::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (
		bEnableRuntimeHotkeys &&
		GetWorld() != nullptr &&
		IsPrimaryRuntimeHotkeyTester()
	)
	{
		APlayerController* PlayerController =
			GetWorld()->GetFirstPlayerController();

		if (IsValid(PlayerController))
		{
			if (PlayerController->WasInputKeyJustPressed(EKeys::F7))
			{
				RunConfiguredTest();
			}
			else if (PlayerController->WasInputKeyJustPressed(EKeys::F8))
			{
				ResetConfiguredTest();
			}
		}
	}

	if (bDrawDebug)
	{
		DrawTestConfiguration();
	}

	if (
		TestState == ESoccerAerialChestDebugState::WaitingForMontage ||
		TestState == ESoccerAerialChestDebugState::Running
	)
	{
		UpdateActiveTest(DeltaTime);
	}
	else if (
		bDrawDebug &&
		TestState == ESoccerAerialChestDebugState::Finished
	)
	{
		DrawResultSummary();
	}

	/*
	 * La leyenda valida es el TextRender dentro del mundo. El resumen legado
	 * de pantalla se deja sin ejecutar para que una instancia antigua del
	 * tester no vuelva a superponer mensajes aunque tuviera guardado
	 */
	UpdateWorldStatusText();
}

void ASoccerAerialChestDebugTester::RunConfiguredTest()
{
	if (
		GetWorld() == nullptr ||
		!GetWorld()->IsGameWorld()
	)
	{
		return;
	}

	/* Un solo tester puede gobernar la misma pelota durante un intento. */
	StopOtherRunningTesters();
	ResetConfiguredTest();

	if (!ResolveReferences())
	{
		MarkTestFailed(TEXT("No se encontraron personaje y pelota"));
		return;
	}

	CaptureInitialState();
	FreezeEnvironment();
	PlaceCharacterAtTestStation();
	ReleaseBallFromAnyOwner();

	/*
	 * The trajectory height is deliberate, but contact resolution is natural.
	 * The tester never filters HEAD or CHEST in this validation stage.
	 */
	ActiveExpectedContactSurface = ResolveTargetSurfaceForRun();
	TestCharacter->DebugSetForcedAerialContactSurface(
		ESoccerAerialContactSurface::None
	);
	TestCharacter->DebugSetStandingControlPlanningSurface(
		ActiveExpectedContactSurface
	);

	if (
		!TestCharacter->GetAerialActionProfileCopy(
			ESoccerAerialActionType::StandingControl,
			ActiveProfile
		)
	)
	{
		MarkTestFailed(TEXT("No existe perfil StandingControl"));
		return;
	}

	bool bStarted = false;

	if (
		TestMode ==
		ESoccerAerialChestDebugMode::ExactMontageContact
	)
	{
		IntendedContactLocation =
			BuildConfiguredContactLocation(ActiveProfile);
		bStarted = StartExactMontageContactTest();
	}
	else
	{
		/*
		 * FullPrediction must use a future crossing point independent from the
		 * character's current chest. Otherwise the calculated preparation
		 * location is already the current location and Snap has no visible or
		 * diagnostic effect.
		 */
		IntendedContactLocation =
			BuildFullPredictionContactLocation(ActiveProfile);
		bStarted = StartFullPredictionTest();
	}

	if (!bStarted)
	{
		if (TestState != ESoccerAerialChestDebugState::Failed)
		{
			MarkTestFailed(TEXT("No se pudo iniciar la prueba"));
		}
		return;
	}

	TestStartWorldTime = GetWorld()->GetTimeSeconds();
	TestState = ESoccerAerialChestDebugState::WaitingForMontage;
}

void ASoccerAerialChestDebugTester::ResetConfiguredTest()
{
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(AutoRunTimerHandle);
		GetWorldTimerManager().ClearTimer(AutoRepeatTimerHandle);
	}

	if (IsValid(TestCharacter))
	{
		TestCharacter->DebugSetForcedAerialContactSurface(
			ESoccerAerialContactSurface::None
		);
		TestCharacter->DebugSetStandingControlPlanningSurface(
			ESoccerAerialContactSurface::None
		);
		TestCharacter->CancelAerialAction();

		if (
			ASoccerAIController* AIController =
				Cast<ASoccerAIController>(TestCharacter->GetController())
		)
		{
			AIController->SetAerialDebugIsolation(false, nullptr);
		}
	}

	RestoreFrozenEnvironment();
	RestoreInitialState();

	TestState = ESoccerAerialChestDebugState::Idle;
	ActiveProfile = FSoccerAerialActionProfile();
	ActivePlan = FSoccerAerialInterceptionPlan();
	CapturedContactResult = FSoccerAerialContactResult();
	IntendedContactLocation = FVector::ZeroVector;
	ConfiguredBallStartLocation = FVector::ZeroVector;
	ConfiguredLaunchVelocity = FVector::ZeroVector;
	LaunchCalibrationResidual = FVector::ZeroVector;
	PredictionLocationBeforeSnap = FVector::ZeroVector;
	PredictionRequestedSnapLocation = FVector::ZeroVector;
	PredictionAppliedSnapLocation = FVector::ZeroVector;
	PredictionRequestedSnapDistance = 0.0f;
	PredictionSnapApplicationError = 0.0f;
	bPredictionSnapApplied = false;
	FullPredictionEstimatedArrivalTime = -1.0f;
	FullPredictionRequestedTravelTime = -1.0f;
	FullPredictionUsefulApproachTime = -1.0f;
	ExactLaunchMontageTime = -1.0f;
	ExactLaunchTravelTime = 0.0f;
	bExactBallLaunched = false;
	PlannedTrajectoryDebugPoints.Reset();
	PlannedTrajectoryDebugTimes.Reset();
	ActualTrajectoryDebugPoints.Reset();
	PlannedTrajectoryDebugRevision = INDEX_NONE;
	CurrentTrajectoryPredictionError = 0.0f;
	MaximumTrajectoryPredictionError = 0.0f;
	CurrentPredictedBallLocation = FVector::ZeroVector;
	bHasCurrentPredictedBallLocation = false;
	PlannedMontageStartWorldTime = -1000.0f;
	ActualMontageStartWorldTime = -1000.0f;
	MontageStartTimingError = 0.0f;
	bActualMontageStartCaptured = false;
	TestStartWorldTime = -1000.0f;
	BallLaunchWorldTime = -1000.0f;
	PreviousMontagePosition = -1.0f;
	ClosestChestDistance = BIG_NUMBER;
	ClosestHeadDistance = BIG_NUMBER;
	ClosestChestMontageTime = 0.0f;
	ClosestHeadMontageTime = 0.0f;
	ClosestChestDistanceInsideWindow = BIG_NUMBER;
	ClosestHeadDistanceInsideWindow = BIG_NUMBER;
	bPreviousSnapshotValid = false;
	PreviousSnapshot = FSoccerAerialDebugSnapshot();
	bMontageStarted = false;
	bIdealMomentCaptured = false;
	bContactCaptured = false;
	bBallEnteredChestVolumeInsideWindow = false;
	bBallEnteredHeadVolumeInsideWindow = false;
	bBallEnteredChestVolumeOutsideWindow = false;
	bBallEnteredHeadVolumeOutsideWindow = false;
	IdealBallLocation = FVector::ZeroVector;
	IdealChestPoint = FVector::ZeroVector;
	IdealHeadLocation = FVector::ZeroVector;
	IdealTrackToActualChestErrorLocal = FVector::ZeroVector;
	ClosestChestErrorLocal = FVector::ZeroVector;
	ClosestChestTimeError = 0.0f;
	ClosestHeadErrorLocal = FVector::ZeroVector;
	ClosestHeadTimeError = 0.0f;
	ActiveExpectedContactSurface = ESoccerAerialContactSurface::Chest;
	FailureReason.Empty();
	FinalClassification.Empty();

	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(7310, 0.01f, FColor::White, TEXT(""));
		GEngine->AddOnScreenDebugMessage(7311, 0.01f, FColor::White, TEXT(""));
		GEngine->AddOnScreenDebugMessage(7312, 0.01f, FColor::White, TEXT(""));
		GEngine->AddOnScreenDebugMessage(7313, 0.01f, FColor::White, TEXT(""));
		GEngine->AddOnScreenDebugMessage(7314, 0.01f, FColor::White, TEXT(""));
		GEngine->AddOnScreenDebugMessage(7315, 0.01f, FColor::White, TEXT(""));
	}
}

bool ASoccerAerialChestDebugTester::ResolveReferences()
{
	if (!bAutoFindReferences)
	{
		return IsValid(TestCharacter) && IsValid(SoccerBall);
	}

	if (!IsValid(SoccerBall))
	{
		for (TActorIterator<ASoccerBall> It(GetWorld()); It; ++It)
		{
			SoccerBall = *It;
			break;
		}
	}

	if (!IsValid(TestCharacter))
	{
		ASoccerCharacterBase* BestCharacter = nullptr;
		float BestDistanceSquared = BIG_NUMBER;

		for (
			TActorIterator<ASoccerCharacterBase> It(GetWorld());
			It;
			++It
		)
		{
			ASoccerCharacterBase* Candidate = *It;

			if (!IsValid(Candidate))
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared2D(
				Candidate->GetActorLocation(),
				GetActorLocation()
			);

			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				BestCharacter = Candidate;
			}
		}

		TestCharacter = BestCharacter;
	}

	return IsValid(TestCharacter) && IsValid(SoccerBall);
}

void ASoccerAerialChestDebugTester::CaptureInitialState()
{
	if (bInitialStateCaptured || !ResolveReferences())
	{
		return;
	}

	InitialCharacterTransform = TestCharacter->GetActorTransform();
	InitialBallTransform = SoccerBall->GetActorTransform();
	InitialBallVelocity = SoccerBall->GetBallPhysicsVelocity();
	bInitialCharacterTickEnabled = TestCharacter->IsActorTickEnabled();
	bInitialCharacterCollisionEnabled = TestCharacter->GetActorEnableCollision();

	if (
		UCharacterMovementComponent* Movement =
			TestCharacter->GetCharacterMovement()
	)
	{
		InitialMovementMode = Movement->MovementMode;
		InitialCustomMovementMode = Movement->CustomMovementMode;
	}

	if (AController* Controller = TestCharacter->GetController())
	{
		bInitialControllerTickEnabled = Controller->IsActorTickEnabled();
	}

	bInitialStateCaptured = true;
}

void ASoccerAerialChestDebugTester::RestoreInitialState()
{
	if (!bInitialStateCaptured)
	{
		return;
	}

	if (IsValid(TestCharacter))
	{
		TestCharacter->SetActorTickEnabled(bInitialCharacterTickEnabled);
		TestCharacter->SetActorEnableCollision(bInitialCharacterCollisionEnabled);
		TestCharacter->SetActorTransform(
			InitialCharacterTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		if (
			UCharacterMovementComponent* Movement =
				TestCharacter->GetCharacterMovement()
		)
		{
			Movement->SetMovementMode(
				InitialMovementMode,
				InitialCustomMovementMode
			);
			Movement->StopMovementImmediately();
		}

		if (AController* Controller = TestCharacter->GetController())
		{
			Controller->SetActorTickEnabled(bInitialControllerTickEnabled);
			Controller->StopMovement();
		}
	}

	if (IsValid(SoccerBall))
	{
		SoccerBall->SetPossessed(false);
		SoccerBall->SetActorTransform(
			InitialBallTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
		SoccerBall->ApplyAerialContactVelocity(
			InitialBallVelocity,
			0.0f
		);
	}
}

void ASoccerAerialChestDebugTester::FreezeEnvironment()
{
	FrozenCharacters.Empty();
	FrozenMatchManager.Reset();

	if (bFreezeOtherCharacters)
	{
		for (
			TActorIterator<ASoccerCharacterBase> It(GetWorld());
			It;
			++It
		)
		{
			ASoccerCharacterBase* Character = *It;

			if (!IsValid(Character) || Character == TestCharacter)
			{
				continue;
			}

			FSoccerAerialFrozenCharacterState State;
			State.Character = Character;
			State.Transform = Character->GetActorTransform();
			State.bActorTickEnabled = Character->IsActorTickEnabled();
			State.bActorCollisionEnabled = Character->GetActorEnableCollision();
			State.Controller = Character->GetController();

			if (AController* Controller = State.Controller.Get())
			{
				State.bControllerTickEnabled = Controller->IsActorTickEnabled();
				Controller->StopMovement();

				/* Keep the PlayerController alive so F7/F8 still work. */
				if (!Controller->IsA<APlayerController>())
				{
					Controller->SetActorTickEnabled(false);
				}
			}

			if (
				UCharacterMovementComponent* Movement =
					Character->GetCharacterMovement()
			)
			{
				State.bHadMovementComponent = true;
				State.MovementMode = Movement->MovementMode;
				State.CustomMovementMode = Movement->CustomMovementMode;
				Movement->StopMovementImmediately();
				Movement->DisableMovement();
			}

			Character->CancelAerialAction();
			Character->SetActorEnableCollision(false);
			Character->SetActorTickEnabled(false);
			FrozenCharacters.Add(State);
		}
	}

	if (bFreezeMatchManager)
	{
		for (TActorIterator<ASoccerMatchManager> It(GetWorld()); It; ++It)
		{
			ASoccerMatchManager* MatchManager = *It;

			if (IsValid(MatchManager))
			{
				FrozenMatchManager = MatchManager;
				bFrozenMatchManagerTickEnabled =
					MatchManager->IsActorTickEnabled();
				MatchManager->SetActorTickEnabled(false);
				break;
			}
		}
	}
}

void ASoccerAerialChestDebugTester::RestoreFrozenEnvironment()
{
	for (const FSoccerAerialFrozenCharacterState& State : FrozenCharacters)
	{
		ASoccerCharacterBase* Character = State.Character.Get();

		if (!IsValid(Character))
		{
			continue;
		}

		Character->SetActorTransform(
			State.Transform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
		Character->SetActorEnableCollision(State.bActorCollisionEnabled);
		Character->SetActorTickEnabled(State.bActorTickEnabled);

		if (
			State.bHadMovementComponent &&
			Character->GetCharacterMovement() != nullptr
		)
		{
			Character->GetCharacterMovement()->SetMovementMode(
				State.MovementMode,
				State.CustomMovementMode
			);
		}

		if (AController* Controller = State.Controller.Get())
		{
			Controller->SetActorTickEnabled(State.bControllerTickEnabled);
		}
	}

	FrozenCharacters.Empty();

	if (ASoccerMatchManager* MatchManager = FrozenMatchManager.Get())
	{
		MatchManager->SetActorTickEnabled(
			bFrozenMatchManagerTickEnabled
		);
	}

	FrozenMatchManager.Reset();
}

void ASoccerAerialChestDebugTester::PlaceCharacterAtTestStation()
{
	if (!IsValid(TestCharacter))
	{
		return;
	}

	FVector Location = TestCharacter->GetActorLocation();
	FRotator Rotation = TestCharacter->GetActorRotation();

	if (bUseTesterTransformAsCharacterTransform)
	{
		Location.X = GetActorLocation().X;
		Location.Y = GetActorLocation().Y;
		Rotation = GetActorRotation();
	}

	Rotation.Pitch = 0.0f;
	Rotation.Roll = 0.0f;
	Rotation.Yaw += CharacterYawOffsetDegrees;

	TestCharacter->SetActorTickEnabled(true);
	TestCharacter->SetActorEnableCollision(true);
	TestCharacter->SetActorLocationAndRotation(
		Location,
		Rotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	if (
		UCharacterMovementComponent* Movement =
			TestCharacter->GetCharacterMovement()
	)
	{
		Movement->SetMovementMode(MOVE_Walking);
		Movement->StopMovementImmediately();
	}

	if (AController* Controller = TestCharacter->GetController())
	{
		Controller->SetActorTickEnabled(true);
		Controller->StopMovement();
	}
}

void ASoccerAerialChestDebugTester::ReleaseBallFromAnyOwner()
{
	for (TActorIterator<ASoccerAICharacter> It(GetWorld()); It; ++It)
	{
		ASoccerAICharacter* AICharacter = *It;

		if (IsValid(AICharacter) && AICharacter->IsAIPossessingBall())
		{
			AICharacter->ReleaseAIBall();
		}
	}

	for (TActorIterator<AThirdPersonCppCharacter> It(GetWorld()); It; ++It)
	{
		AThirdPersonCppCharacter* HumanCharacter = *It;

		if (IsValid(HumanCharacter) && HumanCharacter->IsPossessingBall())
		{
			HumanCharacter->ReleaseBallForMatchRestart();
		}
	}

	if (IsValid(SoccerBall))
	{
		SoccerBall->SetActorEnableCollision(true);
		SoccerBall->SetPossessed(false);
		SoccerBall->StopBallKeepingPhysics();
	}
}

bool ASoccerAerialChestDebugTester::StartExactMontageContactTest()
{
    /*
     * Match GoalkeeperDebugShotTester: start the montage first. The ball is
     * launched later, from UpdateActiveTest(), a short time before the ideal
     * chest-contact frame. This exact mode tests animation/contact only; it
     * no longer asks a long ballistic arc to stay accurate for ~0.9 seconds.
     */
    SoccerBall->SetActorEnableCollision(true);
    SoccerBall->SetPossessed(false);
    SoccerBall->StopBallKeepingPhysics();

    ExactLaunchTravelTime = FMath::Clamp(
        ExactModeBallTravelTime,
        0.05f,
        FMath::Max(0.05f, ActiveProfile.IdealContactTime - 0.02f)
    );
    ExactLaunchMontageTime = FMath::Max(
        0.0f,
        ActiveProfile.IdealContactTime - ExactLaunchTravelTime
    );
    bExactBallLaunched = false;

    ActivePlan = FSoccerAerialInterceptionPlan();
    ActivePlan.bValid = true;
    ActivePlan.ActionType = ESoccerAerialActionType::StandingControl;
    ActivePlan.Ball = SoccerBall;
    ActivePlan.ContactLocation = IntendedContactLocation;
    ActivePlan.PreparationLocation = TestCharacter->GetActorLocation();
    ActivePlan.FacingDirection =
        TestCharacter->GetActorForwardVector().GetSafeNormal2D();
    ActivePlan.BallArrivalTime = ActiveProfile.IdealContactTime;
    ActivePlan.PlayerArrivalTime = 0.0f;
    ActivePlan.MontageStartDelay = 0.0f;
    ActivePlan.ExpectedContactTimeFromMontageStart =
        ActiveProfile.IdealContactTime;
    ActivePlan.BallHeightAboveGround =
        IntendedContactLocation.Z - GetCharacterGroundLocation().Z;
    ActivePlan.TrajectoryRevision = SoccerBall->GetTrajectoryRevision();

    return QueuePlanForTest(ActivePlan, false, true);
}

bool ASoccerAerialChestDebugTester::StartFullPredictionTest()
{
    /*
     * SNAP is a spatial isolation test, not a long-flight test.
     *
     * The previous implementation launched the ball first and asked the
     * complete predictor to find any reachable sample from the character's
     * original location. If that character started far from the tester, the
     * predictor quite correctly selected a much later point. We then snapped
     * the character there but kept the old long-flight timing. The ball could
     * hit the ground/change trajectory before the montage start, so the test
     * finished with MontageTime == 0.0 and a meaningless position error.
     *
     * For Predictor + Snap we now isolate exactly one question: does the
     * spatial preparation calculation put the animated chest on the chosen
     * future contact point? We calculate that actor location from the same
     * converted Mixamo chest track used by normal planning, teleport there,
     * then reuse the goalkeeper-style short launch already validated by the
     * exact test. Navigation mode keeps the complete predictor path.
     */
    if (bSnapCharacterToExactPreparationLocation)
    {
        FVector FacingDirection =
            TestCharacter->GetActorForwardVector().GetSafeNormal2D();

        if (FacingDirection.IsNearlyZero())
        {
            FacingDirection = FVector::ForwardVector;
        }

        FVector ChestLowerAtCurrentActor;
        FVector ChestUpperAtCurrentActor;
        FVector HeadAtCurrentActor;

        if (!TestCharacter->GetStandingAerialControlTrackWorldLocations(
            ActiveProfile.IdealContactTime,
            FacingDirection,
            TestCharacter->GetActorLocation(),
            ChestLowerAtCurrentActor,
            ChestUpperAtCurrentActor,
            HeadAtCurrentActor
        ))
        {
            MarkTestFailed(
                TEXT("No se pudo evaluar el track del pecho para el Snap")
            );
            return false;
        }

        const FVector PredictedContactAtCurrentActor =
            SelectStandingControlTrackPoint(
                ChestLowerAtCurrentActor,
                ChestUpperAtCurrentActor,
                HeadAtCurrentActor
            );

        PredictionLocationBeforeSnap =
            TestCharacter->GetActorLocation();

        /*
         * Translating the actor by this delta translates the complete
         * animated chest track by the same amount. This is the same spatial
         * relation used by BuildAerialPlanForProfile(), but without letting
         * current travel time choose a different trajectory sample.
         */
        const FVector RequiredTranslation =
            IntendedContactLocation - PredictedContactAtCurrentActor;

        FVector SnapLocation =
            PredictionLocationBeforeSnap + RequiredTranslation;

        /* Characters remain grounded; the track supplies the chest height. */
        SnapLocation.Z = PredictionLocationBeforeSnap.Z;

        PredictionRequestedSnapLocation = SnapLocation;
        PredictionRequestedSnapDistance = FVector::Dist2D(
            PredictionLocationBeforeSnap,
            PredictionRequestedSnapLocation
        );

        FRotator FacingRotation = FacingDirection.Rotation();
        FacingRotation.Pitch = 0.0f;
        FacingRotation.Roll = 0.0f;

        const bool bSetLocationSucceeded =
            TestCharacter->SetActorLocationAndRotation(
                SnapLocation,
                FacingRotation,
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            );

        PredictionAppliedSnapLocation =
            TestCharacter->GetActorLocation();
        PredictionSnapApplicationError = FVector::Dist2D(
            PredictionAppliedSnapLocation,
            PredictionRequestedSnapLocation
        );
        bPredictionSnapApplied =
            bSetLocationSucceeded &&
            PredictionSnapApplicationError <= 2.0f;

        if (TestCharacter->GetCharacterMovement() != nullptr)
        {
            TestCharacter->GetCharacterMovement()->StopMovementImmediately();
        }

        if (!bPredictionSnapApplied)
        {
            MarkTestFailed(FString::Printf(
                TEXT("Snap no aplicado. Distancia pedida %.1f cm, error %.1f cm"),
                PredictionRequestedSnapDistance,
                PredictionSnapApplicationError
            ));
            return false;
        }

        if (bDrawDebug)
        {
            DrawDebugSphere(
                GetWorld(),
                PredictionLocationBeforeSnap,
                22.0f,
                12,
                FColor::Red,
                false,
                5.0f,
                0,
                2.5f
            );
            DrawDebugSphere(
                GetWorld(),
                PredictionRequestedSnapLocation,
                28.0f,
                12,
                FColor::Blue,
                false,
                5.0f,
                0,
                3.0f
            );
            DrawDebugLine(
                GetWorld(),
                PredictionLocationBeforeSnap,
                PredictionAppliedSnapLocation,
                FColor::Green,
                false,
                5.0f,
                0,
                4.0f
            );
        }

        /*
         * Start the montage now and launch shortly before the ideal frame,
         * exactly like ExactMontageContact. This keeps the spatial Snap test
         * independent from long-flight drift, bounces and trajectory revision
         * changes. UpdateActiveTest() treats Snap as a short-launch mode.
         */
        return StartExactMontageContactTest();
    }

    /*
     * NAV needs a real time budget. The old fixed 0.95 s flight left only:
     *
     *   0.95 - 0.78 ideal contact - 0.10 settle = 0.07 s
     *
     * for the character to run more than three metres, so the gameplay
     * predictor correctly rejected every sample before navigation even began.
     * Calculate the intended preparation point first, estimate character
     * arrival exactly as gameplay does, and then give the ball enough future
     * time for approach + settle + the montage's internal contact time.
     */
    FVector FacingDirection =
        TestCharacter->GetActorForwardVector().GetSafeNormal2D();

    if (FacingDirection.IsNearlyZero())
    {
        FacingDirection = FVector::ForwardVector;
    }

    FVector EstimatedPreparationLocation =
        IntendedContactLocation -
        FacingDirection * ActiveProfile.ContactForwardOffset;
    EstimatedPreparationLocation.Z = TestCharacter->GetActorLocation().Z;

    FVector ChestLowerAtCurrentActor;
    FVector ChestUpperAtCurrentActor;
    FVector HeadAtCurrentActor;

    if (TestCharacter->GetStandingAerialControlTrackWorldLocations(
        ActiveProfile.IdealContactTime,
        FacingDirection,
        TestCharacter->GetActorLocation(),
        ChestLowerAtCurrentActor,
        ChestUpperAtCurrentActor,
        HeadAtCurrentActor
    ))
    {
        const FVector PredictedContactAtCurrentActor =
            SelectStandingControlTrackPoint(
                ChestLowerAtCurrentActor,
                ChestUpperAtCurrentActor,
                HeadAtCurrentActor
            );

        const FVector RequiredTranslation =
            IntendedContactLocation - PredictedContactAtCurrentActor;

        EstimatedPreparationLocation =
            TestCharacter->GetActorLocation() + RequiredTranslation;
        EstimatedPreparationLocation.Z =
            TestCharacter->GetActorLocation().Z;
    }

    FullPredictionEstimatedArrivalTime =
        TestCharacter->EstimateArrivalTimeToLocation(
            EstimatedPreparationLocation,
            ActiveProfile.PreparationReachRadius
        );

    if (!FMath::IsFinite(FullPredictionEstimatedArrivalTime))
    {
        MarkTestFailed(TEXT("No se pudo estimar el tiempo de llegada del bot"));
        return false;
    }

    const float RequiredTravelTime =
        FullPredictionEstimatedArrivalTime +
        ActiveProfile.PreparationSettleTime +
        ActiveProfile.IdealContactTime +
        FMath::Max(0.0f, FullPredictionNavSafetyMargin);

    FullPredictionRequestedTravelTime = FMath::Max(
        0.15f,
        FMath::Max(
            FullPredictionBallTravelTime,
            FMath::Max(
                FullPredictionSnapMinimumTravelTime,
                RequiredTravelTime
            )
        )
    );

    /* The gameplay aerial trajectory horizon is 4 s by default. */
    FullPredictionRequestedTravelTime = FMath::Min(
        FullPredictionRequestedTravelTime,
        3.75f
    );

    FullPredictionUsefulApproachTime = FMath::Max(
        0.0f,
        FullPredictionRequestedTravelTime -
        ActiveProfile.IdealContactTime -
        ActiveProfile.PreparationSettleTime
    );

    if (!LaunchBallToContact(
        IntendedContactLocation,
        FullPredictionRequestedTravelTime
    ))
    {
        MarkTestFailed(TEXT("No se pudo lanzar la pelota"));
        return false;
    }

    if (
        !TestCharacter->FindBestAerialInterceptionPlan(
            SoccerBall,
            ESoccerAerialActionIntent::Control,
            ActivePlan
        )
    )
    {
        MarkTestFailed(FString::Printf(
            TEXT(
                "Sin plan: llegada %.2f s | tiempo util %.2f s | vuelo %.2f s"
            ),
            FullPredictionEstimatedArrivalTime,
            FullPredictionUsefulApproachTime,
            FullPredictionRequestedTravelTime
        ));
        return false;
    }

    if (
        Cast<ASoccerAIController>(TestCharacter->GetController()) == nullptr
    )
    {
        MarkTestFailed(
            TEXT("Sin Snap se necesita un bot con SoccerAIController")
        );
        return false;
    }

    /*
     * El tester NAV valida una sola solucion. El gameplay normal puede
     * refrescar planes, pero aqui bloquear el plan evita que el objetivo
     * salte entre muestras mientras el bot corre y vuelve la prueba
     * determinista.
     */
    return QueuePlanForTest(
        ActivePlan,
        true,
        true
    );
}

bool ASoccerAerialChestDebugTester::QueuePlanForTest(
	const FSoccerAerialInterceptionPlan& Plan,
	bool bAllowApproach,
	bool bLockPlan
)
{
	if (!IsValid(TestCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	if (
		ASoccerAIController* AIController =
			Cast<ASoccerAIController>(TestCharacter->GetController())
	)
	{
		AIController->SetAerialDebugIsolation(true, SoccerBall);
	}

	const bool bQueued = TestCharacter->DebugQueueAerialPlan(
		Plan,
		ESoccerAerialActionIntent::Control,
		bAllowApproach,
		bLockPlan
	);

	if (!bQueued)
	{
		MarkTestFailed(
			TEXT("No se pudo encolar el montage header_chest")
		);
	}

	return bQueued;
}

FVector ASoccerAerialChestDebugTester::GetCharacterGroundLocation() const
{
	if (!IsValid(TestCharacter))
	{
		return FVector::ZeroVector;
	}

	FVector GroundLocation = TestCharacter->GetActorLocation();

	if (const UCapsuleComponent* Capsule = TestCharacter->GetCapsuleComponent())
	{
		GroundLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	return GroundLocation;
}

ESoccerAerialContactSurface
ASoccerAerialChestDebugTester::ResolveTargetSurfaceForRun()
{
	switch (StandingControlTestTarget)
	{
	case ESoccerStandingControlTestTarget::Head:
		return ESoccerAerialContactSurface::Head;

	case ESoccerStandingControlTestTarget::Alternate:
	{
		const ESoccerAerialContactSurface Result =
			bAlternateNextRunUsesHead
			? ESoccerAerialContactSurface::Head
			: ESoccerAerialContactSurface::Chest;

		bAlternateNextRunUsesHead = !bAlternateNextRunUsesHead;
		return Result;
	}

	case ESoccerStandingControlTestTarget::Chest:
	default:
		return ESoccerAerialContactSurface::Chest;
	}
}

FString ASoccerAerialChestDebugTester::GetExpectedSurfaceName() const
{
	return AerialContactSurfaceName(ActiveExpectedContactSurface);
}

bool ASoccerAerialChestDebugTester::DidResolveExpectedSurface() const
{
	return
		bContactCaptured &&
		CapturedContactResult.ContactSurface == ActiveExpectedContactSurface;
}

FVector ASoccerAerialChestDebugTester::SelectStandingControlTrackPoint(
	const FVector& ChestLower,
	const FVector& ChestUpper,
	const FVector& Head
) const
{
	if (ActiveExpectedContactSurface == ESoccerAerialContactSurface::Head)
	{
		return Head;
	}

	return FMath::Lerp(ChestLower, ChestUpper, 0.50f);
}

FVector ASoccerAerialChestDebugTester::BuildConfiguredContactLocation(
	const FSoccerAerialActionProfile& Profile
) const
{
	if (IsValid(TestCharacter))
	{
		FVector ChestLowerWorld;
		FVector ChestUpperWorld;
		FVector HeadWorld;

		if (TestCharacter->GetStandingAerialControlTrackWorldLocations(
			Profile.IdealContactTime,
			TestCharacter->GetActorForwardVector(),
			TestCharacter->GetActorLocation(),
			ChestLowerWorld,
			ChestUpperWorld,
			HeadWorld
		))
		{
			/*
			 * Same method as goalkeeper hand tracks: the CSV was already
			 * converted to Forward/Lateral/Up before Unreal import.
			 * We aim at the animated torso track, not at guessed world axes.
			 */
			return SelectStandingControlTrackPoint(
				ChestLowerWorld,
				ChestUpperWorld,
				HeadWorld
			);
		}
	}

	/* Compatibility fallback while the new CurveTable is not assigned. */
	const FVector GroundLocation = GetCharacterGroundLocation();
	FVector Forward = TestCharacter->GetActorForwardVector().GetSafeNormal2D();

	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	FVector ContactLocation =
		TestCharacter->GetActorLocation() +
		Forward * Profile.ContactForwardOffset;
	ContactLocation.Z =
		GroundLocation.Z + DesiredContactHeightAboveGround;

	return ContactLocation;
}

FVector ASoccerAerialChestDebugTester::BuildFullPredictionContactLocation(
	const FSoccerAerialActionProfile& Profile
) const
{
	/*
	 * Start from the correctly converted Mixamo chest track so height and
	 * mesh-origin offsets remain identical to the exact test. Then translate
	 * that world point horizontally to create a trajectory that does not pass
	 * through the character's current position.
	 */
	FVector ContactLocation = BuildConfiguredContactLocation(Profile);

	if (!IsValid(TestCharacter))
	{
		return ContactLocation;
	}

	FVector Forward =
		TestCharacter->GetActorForwardVector().GetSafeNormal2D();
	FVector Right =
		TestCharacter->GetActorRightVector().GetSafeNormal2D();

	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	if (Right.IsNearlyZero())
	{
		Right = FVector::RightVector;
	}

	ContactLocation +=
		Forward * FullPredictionContactForwardOffset +
		Right * FullPredictionContactLateralOffset;

	return ContactLocation;
}

FVector ASoccerAerialChestDebugTester::BuildConfiguredBallStartLocation(
	const FVector& ContactLocation
) const
{
	FVector Forward = TestCharacter->GetActorForwardVector().GetSafeNormal2D();
	FVector Right = TestCharacter->GetActorRightVector().GetSafeNormal2D();

	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	if (Right.IsNearlyZero())
	{
		Right = FVector::RightVector;
	}

	FVector StartLocation =
		ContactLocation +
		Forward * BallStartDistance +
		Right * BallLateralOffset;
	StartLocation.Z =
		GetCharacterGroundLocation().Z +
		BallStartHeightAboveGround;

	return StartLocation;
}

bool ASoccerAerialChestDebugTester::LaunchBallToContact(
    const FVector& ContactLocation,
    float TravelTime
)
{
    if (!IsValid(SoccerBall) || GetWorld() == nullptr)
    {
        return false;
    }

    const float SafeTravelTime = FMath::Max(0.05f, TravelTime);
    ConfiguredBallStartLocation =
        BuildConfiguredBallStartLocation(ContactLocation);

    const FVector GravityAcceleration(
        0.0f,
        0.0f,
        GetWorld()->GetGravityZ()
    );

    FVector CalibratedVelocity =
        (ContactLocation - ConfiguredBallStartLocation) /
            SafeTravelTime -
        0.5f * GravityAcceleration * SafeTravelTime;

    LaunchCalibrationResidual = FVector::ZeroVector;

    const int32 SafeIterations = FMath::Clamp(
        LaunchCalibrationIterations,
        1,
        12
    );
    const float SafeTolerance = FMath::Max(
        0.1f,
        LaunchCalibrationTolerance
    );
    const float SafeGain = FMath::Clamp(
        LaunchCalibrationGain,
        0.1f,
        1.5f
    );

    /*
     * The analytic projectile equation does not include all settings used by
     * SoccerBall/Chaos. Refine the launch using the same predictor used by
     * gameplay until its location at TravelTime is the requested contact.
     */
    for (int32 Iteration = 0; Iteration < SafeIterations; ++Iteration)
    {
        SoccerBall->SetActorLocation(
            ConfiguredBallStartLocation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
        SoccerBall->ApplyAerialContactVelocity(
            CalibratedVelocity,
            0.0f
        );

        FVector PredictedLocation;

        if (!SamplePredictedBallLocationAtTime(
            SafeTravelTime,
            PredictedLocation
        ))
        {
            break;
        }

        LaunchCalibrationResidual =
            ContactLocation - PredictedLocation;

        if (LaunchCalibrationResidual.Size() <= SafeTolerance)
        {
            break;
        }

        CalibratedVelocity +=
            LaunchCalibrationResidual /
            SafeTravelTime * SafeGain;
    }

    ConfiguredLaunchVelocity = CalibratedVelocity;

    SoccerBall->SetActorEnableCollision(true);
    SoccerBall->SetPossessed(false);
    SoccerBall->SetActorLocation(
        ConfiguredBallStartLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );
    SoccerBall->ApplyAerialContactVelocity(
        ConfiguredLaunchVelocity,
        0.0f
    );

    BallLaunchWorldTime = GetWorld()->GetTimeSeconds();
    CapturePlannedTrajectoryDebug(SafeTravelTime + 0.35f);
    return true;
}

bool ASoccerAerialChestDebugTester::LaunchExactBallLikeGoalkeeper(
    const FVector& ContactLocation,
    float TravelTime
)
{
    if (!IsValid(SoccerBall) || !IsValid(TestCharacter))
    {
        return false;
    }

    FVector IncomingFromCharacter =
        TestCharacter->GetActorForwardVector().GetSafeNormal2D();
    FVector Right =
        TestCharacter->GetActorRightVector().GetSafeNormal2D();

    if (IncomingFromCharacter.IsNearlyZero())
    {
        IncomingFromCharacter = FVector::ForwardVector;
    }

    if (Right.IsNearlyZero())
    {
        Right = FVector::RightVector;
    }

    ConfiguredBallStartLocation =
        ContactLocation +
        IncomingFromCharacter * FMath::Max(40.0f, ExactModeBallStartDistance) +
        Right * BallLateralOffset;
    ConfiguredBallStartLocation.Z =
        ContactLocation.Z + ExactModeBallStartHeightOffset;

    const FVector ShotDirection =
        (ContactLocation - ConfiguredBallStartLocation).GetSafeNormal();

    if (ShotDirection.IsNearlyZero())
    {
        return false;
    }

    const float SafeTravelTime = FMath::Max(0.02f, TravelTime);
    const float ShotSpeed =
        FVector::Dist(ConfiguredBallStartLocation, ContactLocation) /
        SafeTravelTime;

    SoccerBall->SetActorEnableCollision(true);
    SoccerBall->SetPossessed(false);
    SoccerBall->StopBallKeepingPhysics();
    SoccerBall->SetActorLocation(
        ConfiguredBallStartLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    /*
     * Use the deterministic aerial-velocity path for the short launch too.
     * This clears stale forces and synchronizes Chaos with the same air
     * damping used by BuildPredictedTrajectory() before the first step.
     */
    SoccerBall->ApplyAerialContactVelocity(
        ShotDirection * ShotSpeed,
        0.0f
    );

    ConfiguredLaunchVelocity = ShotDirection * ShotSpeed;
    LaunchCalibrationResidual = FVector::ZeroVector;
    BallLaunchWorldTime =
        GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;

    /*
     * Launching changes TrajectoryRevision after the montage has started.
     * The tester explicitly adopts that new revision and restarts the sweep
     * at the new ball position, avoiding a false sweep across the teleport.
     */
    TestCharacter->DebugRefreshAerialContactTrackingForCurrentBallTrajectory();
    CapturePlannedTrajectoryDebug(SafeTravelTime + 0.20f);

    return true;
}

void ASoccerAerialChestDebugTester::ResetMeasurementsAtExactLaunch()
{
    ActualTrajectoryDebugPoints.Reset();
    CurrentTrajectoryPredictionError = 0.0f;
    MaximumTrajectoryPredictionError = 0.0f;
    CurrentPredictedBallLocation = FVector::ZeroVector;
    bHasCurrentPredictedBallLocation = false;
    ClosestChestDistance = BIG_NUMBER;
    ClosestHeadDistance = BIG_NUMBER;
    ClosestChestMontageTime = 0.0f;
    ClosestHeadMontageTime = 0.0f;
    ClosestChestDistanceInsideWindow = BIG_NUMBER;
    ClosestHeadDistanceInsideWindow = BIG_NUMBER;
    bPreviousSnapshotValid = false;
    PreviousSnapshot = FSoccerAerialDebugSnapshot();
    bIdealMomentCaptured = false;
    bContactCaptured = false;
    bBallEnteredChestVolumeInsideWindow = false;
    bBallEnteredHeadVolumeInsideWindow = false;
    bBallEnteredChestVolumeOutsideWindow = false;
    bBallEnteredHeadVolumeOutsideWindow = false;
    IdealBallLocation = FVector::ZeroVector;
    IdealChestPoint = FVector::ZeroVector;
    IdealHeadLocation = FVector::ZeroVector;
    IdealTrackToActualChestErrorLocal = FVector::ZeroVector;
    ClosestChestErrorLocal = FVector::ZeroVector;
    ClosestChestTimeError = 0.0f;
}

bool ASoccerAerialChestDebugTester::SamplePredictedBallLocationAtTime(
    float TimeFromNow,
    FVector& OutLocation
) const
{
    OutLocation = FVector::ZeroVector;

    if (!IsValid(SoccerBall))
    {
        return false;
    }

    const float SafeTime = FMath::Max(0.0f, TimeFromNow);
    const float PredictionHorizon = FMath::Max(0.10f, SafeTime + 0.08f);
    const float SampleInterval = FMath::Clamp(
        SafeTime / 32.0f,
        0.02f,
        0.04f
    );

    TArray<FSoccerBallTrajectorySample> Samples;

    if (!SoccerBall->BuildPredictedTrajectory(
        PredictionHorizon,
        SampleInterval,
        Samples
    ) || Samples.Num() <= 0)
    {
        return false;
    }

    if (SafeTime <= Samples[0].TimeFromNow)
    {
        OutLocation = Samples[0].Location;
        return true;
    }

    for (int32 Index = 1; Index < Samples.Num(); ++Index)
    {
        const FSoccerBallTrajectorySample& Previous = Samples[Index - 1];
        const FSoccerBallTrajectorySample& Current = Samples[Index];

        if (SafeTime <= Current.TimeFromNow + KINDA_SMALL_NUMBER)
        {
            const float TimeSpan = FMath::Max(
                KINDA_SMALL_NUMBER,
                Current.TimeFromNow - Previous.TimeFromNow
            );
            const float Alpha = FMath::Clamp(
                (SafeTime - Previous.TimeFromNow) / TimeSpan,
                0.0f,
                1.0f
            );

            OutLocation = FMath::Lerp(
                Previous.Location,
                Current.Location,
                Alpha
            );
            return true;
        }
    }

    OutLocation = Samples.Last().Location;
    return true;
}

void ASoccerAerialChestDebugTester::CapturePlannedTrajectoryDebug(
    float PredictionHorizon
)
{
    PlannedTrajectoryDebugPoints.Reset();
    PlannedTrajectoryDebugTimes.Reset();
    PlannedTrajectoryDebugRevision = INDEX_NONE;

    if (!IsValid(SoccerBall))
    {
        return;
    }

    TArray<FSoccerBallTrajectorySample> Samples;
    const float SafeHorizon = FMath::Max(0.15f, PredictionHorizon);

    if (!SoccerBall->BuildPredictedTrajectory(
        SafeHorizon,
        0.04f,
        Samples
    ))
    {
        return;
    }

    PlannedTrajectoryDebugPoints.Reserve(Samples.Num());

    for (const FSoccerBallTrajectorySample& Sample : Samples)
    {
        PlannedTrajectoryDebugPoints.Add(Sample.Location);
        PlannedTrajectoryDebugTimes.Add(Sample.TimeFromNow);
    }

    PlannedTrajectoryDebugRevision = SoccerBall->GetTrajectoryRevision();
}

int32 ASoccerAerialChestDebugTester::CountRuntimeHotkeyTesters() const
{
    if (GetWorld() == nullptr)
    {
        return 0;
    }

    int32 Count = 0;

    for (TActorIterator<ASoccerAerialChestDebugTester> It(GetWorld()); It; ++It)
    {
        const ASoccerAerialChestDebugTester* Tester = *It;

        if (IsValid(Tester) && Tester->bEnableRuntimeHotkeys)
        {
            ++Count;
        }
    }

    return Count;
}

bool ASoccerAerialChestDebugTester::IsPrimaryRuntimeHotkeyTester() const
{
    if (GetWorld() == nullptr || !bEnableRuntimeHotkeys)
    {
        return false;
    }

    const ASoccerAerialChestDebugTester* Primary = nullptr;

    for (TActorIterator<ASoccerAerialChestDebugTester> It(GetWorld()); It; ++It)
    {
        const ASoccerAerialChestDebugTester* Tester = *It;

        if (!IsValid(Tester) || !Tester->bEnableRuntimeHotkeys)
        {
            continue;
        }

        if (Primary == nullptr || Tester->GetUniqueID() < Primary->GetUniqueID())
        {
            Primary = Tester;
        }
    }

    return Primary == this;
}

void ASoccerAerialChestDebugTester::StopOtherRunningTesters()
{
    if (GetWorld() == nullptr)
    {
        return;
    }

    for (TActorIterator<ASoccerAerialChestDebugTester> It(GetWorld()); It; ++It)
    {
        ASoccerAerialChestDebugTester* Other = *It;

        if (!IsValid(Other) || Other == this)
        {
            continue;
        }

        if (
            Other->TestState == ESoccerAerialChestDebugState::WaitingForMontage ||
            Other->TestState == ESoccerAerialChestDebugState::Running
        )
        {
            Other->ResetConfiguredTest();
        }
    }
}

void ASoccerAerialChestDebugTester::UpdateActiveTest(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsValid(TestCharacter) || !IsValid(SoccerBall))
	{
		MarkTestFailed(TEXT("Se perdio una referencia durante la prueba"));
		return;
	}

	const float Elapsed =
		GetWorld()->GetTimeSeconds() - TestStartWorldTime;

	if (Elapsed > FMath::Max(1.0f, TestTimeout))
	{
		MarkTestFailed(TEXT("Timeout de la prueba"));
		return;
	}

	/*
	 * En NAV usamos una unica revision y un unico plan. Si la pelota rebota o
	 * recibe otro impulso antes de comenzar el montage, informarlo de forma
	 * inmediata en lugar de terminar con un timeout ambiguo.
	 */
	if (
		TestMode == ESoccerAerialChestDebugMode::FullPrediction &&
		!bSnapCharacterToExactPreparationLocation &&
		!bMontageStarted &&
		PlannedTrajectoryDebugRevision != INDEX_NONE &&
		SoccerBall->GetTrajectoryRevision() != PlannedTrajectoryDebugRevision
	)
	{
		MarkTestFailed(TEXT("La trayectoria cambio antes del montage (rebote o segundo impulso)"));
		return;
	}

	FSoccerAerialDebugSnapshot Snapshot;
	const bool bHasSnapshot =
		TestCharacter->GetAerialDebugSnapshot(Snapshot);

	if (!bHasSnapshot)
	{
		FName ResolvedHeadBoneName;
		FName ResolvedChestLowerBoneName;
		FName ResolvedChestUpperBoneName;
		FString BoneResolutionFailure;

		if (
			!TestCharacter->ResolveAerialContactBoneNames(
				ResolvedHeadBoneName,
				ResolvedChestLowerBoneName,
				ResolvedChestUpperBoneName,
				&BoneResolutionFailure
			)
		)
		{
			MarkTestFailed(BoneResolutionFailure);
			return;
		}
	}

	if (bHasSnapshot)
	{
		if (Snapshot.Phase == ESoccerAerialActionPhase::Playing)
		{
			if (!bMontageStarted)
			{
				bMontageStarted = true;
				PlannedMontageStartWorldTime =
					BallLaunchWorldTime + ActivePlan.MontageStartDelay;

				/*
				 * The tester may observe Phase::Playing one Tick after Montage_Play.
				 * Use the logical start reported by the character (request minus
				 * catch-up position) instead of that delayed observation time.
				 */
				ActualMontageStartWorldTime =
					Snapshot.MontageLogicalStartWorldTime >= 0.0f
					? Snapshot.MontageLogicalStartWorldTime
					: GetWorld()->GetTimeSeconds();

				MontageStartTimingError =
					ActualMontageStartWorldTime - PlannedMontageStartWorldTime;
				bActualMontageStartCaptured = true;
			}

			TestState = ESoccerAerialChestDebugState::Running;
		}

		const bool bUsesGoalkeeperStyleShortLaunch =
			TestMode == ESoccerAerialChestDebugMode::ExactMontageContact ||
			(
				TestMode == ESoccerAerialChestDebugMode::FullPrediction &&
				bSnapCharacterToExactPreparationLocation
			);

		if (
			bUsesGoalkeeperStyleShortLaunch &&
			Snapshot.Phase == ESoccerAerialActionPhase::Playing &&
			!bExactBallLaunched &&
			Snapshot.MontagePosition >= ExactLaunchMontageTime
		)
		{
			/*
			 * Rebuild at launch time in case the actor/mesh transform changed.
			 * The track is already Mixamo->Forward/Lateral/Up converted.
			 */
			IntendedContactLocation =
				BuildConfiguredContactLocation(ActiveProfile);
			ActivePlan.ContactLocation = IntendedContactLocation;

			if (!LaunchExactBallLikeGoalkeeper(
				IntendedContactLocation,
				ExactLaunchTravelTime
			))
			{
				MarkTestFailed(TEXT("No se pudo hacer el lanzamiento corto tipo arquero"));
				return;
			}

			bExactBallLaunched = true;
			ResetMeasurementsAtExactLaunch();
		}

		const bool bCanMeasure =
			!bUsesGoalkeeperStyleShortLaunch ||
			bExactBallLaunched;

		if (bCanMeasure)
		{
			UpdateDiagnosticMeasurements(Snapshot);
		}

		if (bDrawDebug)
		{
			DrawRuntimeDiagnostics(Snapshot);
		}

		PreviousMontagePosition = Snapshot.MontagePosition;
	}

	if (
		TestCharacter->HasResolvedAerialBallContact() &&
		!bContactCaptured
	)
	{
		CapturedContactResult =
			TestCharacter->GetLastAerialContactResult();
		bContactCaptured = CapturedContactResult.bContactResolved;
	}

	if (
		bMontageStarted &&
		TestCharacter->GetAerialActionPhase() ==
			ESoccerAerialActionPhase::None
	)
	{
		FinalizeTest();
	}
}

void ASoccerAerialChestDebugTester::UpdateDiagnosticMeasurements(
	const FSoccerAerialDebugSnapshot& Snapshot
)
{
	const float ChestCombinedRadius =
		Snapshot.BallRadius +
		Snapshot.ChestContactRadius +
		Snapshot.ExtraTolerance;
	const float HeadCombinedRadius =
		Snapshot.BallRadius +
		Snapshot.HeadContactRadius +
		Snapshot.ExtraTolerance;

	/*
	 * Guardar tambien el recorrido real. La curva cian es la prediccion
	 * congelada al lanzar; la magenta sera lo que Chaos hizo de verdad.
	 */
	if (
		ActualTrajectoryDebugPoints.Num() == 0 ||
		FVector::DistSquared(
			ActualTrajectoryDebugPoints.Last(),
			Snapshot.BallLocation
		) >= FMath::Square(2.0f)
	)
	{
		ActualTrajectoryDebugPoints.Add(Snapshot.BallLocation);

		const int32 MaximumRecordedPoints = 360;
		if (ActualTrajectoryDebugPoints.Num() > MaximumRecordedPoints)
		{
			ActualTrajectoryDebugPoints.RemoveAt(
				0,
				ActualTrajectoryDebugPoints.Num() - MaximumRecordedPoints,
				false
			);
		}
	}

	bHasCurrentPredictedBallLocation = false;

	if (
		GetWorld() != nullptr &&
		BallLaunchWorldTime > -999.0f &&
		PlannedTrajectoryDebugPoints.Num() >= 2 &&
		PlannedTrajectoryDebugTimes.Num() == PlannedTrajectoryDebugPoints.Num() &&
		PlannedTrajectoryDebugRevision != INDEX_NONE &&
		SoccerBall->GetTrajectoryRevision() == PlannedTrajectoryDebugRevision
	)
	{
		const float BallElapsedTime = FMath::Max(
			0.0f,
			GetWorld()->GetTimeSeconds() - BallLaunchWorldTime
		);

		for (int32 Index = 1; Index < PlannedTrajectoryDebugTimes.Num(); ++Index)
		{
			if (BallElapsedTime <= PlannedTrajectoryDebugTimes[Index] + KINDA_SMALL_NUMBER)
			{
				const float TimeSpan = FMath::Max(
					KINDA_SMALL_NUMBER,
					PlannedTrajectoryDebugTimes[Index] -
					PlannedTrajectoryDebugTimes[Index - 1]
				);
				const float Alpha = FMath::Clamp(
					(BallElapsedTime - PlannedTrajectoryDebugTimes[Index - 1]) /
						TimeSpan,
					0.0f,
					1.0f
				);

				CurrentPredictedBallLocation = FMath::Lerp(
					PlannedTrajectoryDebugPoints[Index - 1],
					PlannedTrajectoryDebugPoints[Index],
					Alpha
				);
				bHasCurrentPredictedBallLocation = true;
				break;
			}
		}

		if (
			!bHasCurrentPredictedBallLocation &&
			BallElapsedTime <= PlannedTrajectoryDebugTimes.Last() + 0.05f
		)
		{
			CurrentPredictedBallLocation = PlannedTrajectoryDebugPoints.Last();
			bHasCurrentPredictedBallLocation = true;
		}

		if (bHasCurrentPredictedBallLocation)
		{
			CurrentTrajectoryPredictionError = FVector::Dist(
				Snapshot.BallLocation,
				CurrentPredictedBallLocation
			);
			MaximumTrajectoryPredictionError = FMath::Max(
				MaximumTrajectoryPredictionError,
				CurrentTrajectoryPredictionError
			);
		}
	}

	/*
	 * Mirror the runtime sweep so a fast ball cannot disappear between two
	 * diagnostic frames. This is intentionally independent of the real
	 * contact resolver.
	 */
	if (bPreviousSnapshotValid)
	{
		const int32 TemporalSamples = 9;

		for (int32 SampleIndex = 0; SampleIndex < TemporalSamples; ++SampleIndex)
		{
			const float Alpha = static_cast<float>(SampleIndex) /
				static_cast<float>(TemporalSamples - 1);
			const float MontageTime = FMath::Lerp(
				PreviousSnapshot.MontagePosition,
				Snapshot.MontagePosition,
				Alpha
			);
			const bool bWindowActive =
				Snapshot.Phase == ESoccerAerialActionPhase::Playing &&
				MontageTime >= Snapshot.ContactWindowStart &&
				MontageTime <= Snapshot.ContactWindowEnd;

			const FVector BallLocation = FMath::Lerp(
				PreviousSnapshot.BallLocation,
				Snapshot.BallLocation,
				Alpha
			);
			const FVector HeadLocation = FMath::Lerp(
				PreviousSnapshot.HeadLocation,
				Snapshot.HeadLocation,
				Alpha
			);
			const FVector ChestLower = FMath::Lerp(
				PreviousSnapshot.ChestLowerLocation,
				Snapshot.ChestLowerLocation,
				Alpha
			);
			const FVector ChestUpper = FMath::Lerp(
				PreviousSnapshot.ChestUpperLocation,
				Snapshot.ChestUpperLocation,
				Alpha
			);

			FVector ClosestChestPoint;
			const float ChestDistance = AerialDebugPointSegmentDistance(
				BallLocation,
				ChestLower,
				ChestUpper,
				ClosestChestPoint
			);
			const float HeadDistance = FVector::Dist(BallLocation, HeadLocation);

			if (ChestDistance < ClosestChestDistance)
			{
				ClosestChestDistance = ChestDistance;
				ClosestChestMontageTime = MontageTime;
				ClosestChestTimeError =
					MontageTime - Snapshot.IdealContactTime;
				ClosestChestErrorLocal =
					TestCharacter->GetActorTransform()
					.InverseTransformVectorNoScale(
						BallLocation - ClosestChestPoint
					);
			}

			if (HeadDistance < ClosestHeadDistance)
			{
				ClosestHeadDistance = HeadDistance;
				ClosestHeadMontageTime = MontageTime;
				ClosestHeadTimeError =
					MontageTime - Snapshot.IdealContactTime;
				ClosestHeadErrorLocal =
					TestCharacter->GetActorTransform()
					.InverseTransformVectorNoScale(
						BallLocation - HeadLocation
					);
			}

			if (bWindowActive)
			{
				ClosestChestDistanceInsideWindow = FMath::Min(
					ClosestChestDistanceInsideWindow,
					ChestDistance
				);
				ClosestHeadDistanceInsideWindow = FMath::Min(
					ClosestHeadDistanceInsideWindow,
					HeadDistance
				);
				bBallEnteredChestVolumeInsideWindow |=
					ChestDistance <= ChestCombinedRadius;
				bBallEnteredHeadVolumeInsideWindow |=
					HeadDistance <= HeadCombinedRadius;
			}
			else
			{
				bBallEnteredChestVolumeOutsideWindow |=
					ChestDistance <= ChestCombinedRadius;
				bBallEnteredHeadVolumeOutsideWindow |=
					HeadDistance <= HeadCombinedRadius;
			}
		}
	}

	if (Snapshot.BallToChestDistance < ClosestChestDistance)
	{
		ClosestChestDistance = Snapshot.BallToChestDistance;
		ClosestChestMontageTime = Snapshot.MontagePosition;
		ClosestChestTimeError =
			Snapshot.MontagePosition - Snapshot.IdealContactTime;

		const FVector WorldError =
			Snapshot.BallLocation - Snapshot.ClosestChestPoint;
		ClosestChestErrorLocal =
			TestCharacter->GetActorTransform()
			.InverseTransformVectorNoScale(WorldError);
	}

	if (Snapshot.BallToHeadDistance < ClosestHeadDistance)
	{
		ClosestHeadDistance = Snapshot.BallToHeadDistance;
		ClosestHeadMontageTime = Snapshot.MontagePosition;
		ClosestHeadTimeError =
			Snapshot.MontagePosition - Snapshot.IdealContactTime;
		ClosestHeadErrorLocal =
			TestCharacter->GetActorTransform()
			.InverseTransformVectorNoScale(
				Snapshot.BallLocation - Snapshot.HeadLocation
			);
	}

	if (Snapshot.bContactWindowActive)
	{
		ClosestChestDistanceInsideWindow = FMath::Min(
			ClosestChestDistanceInsideWindow,
			Snapshot.BallToChestDistance
		);
		ClosestHeadDistanceInsideWindow = FMath::Min(
			ClosestHeadDistanceInsideWindow,
			Snapshot.BallToHeadDistance
		);

		if (Snapshot.BallToChestDistance <= ChestCombinedRadius)
		{
			bBallEnteredChestVolumeInsideWindow = true;
		}

		if (Snapshot.BallToHeadDistance <= HeadCombinedRadius)
		{
			bBallEnteredHeadVolumeInsideWindow = true;
		}
	}
	else
	{
		if (Snapshot.BallToChestDistance <= ChestCombinedRadius)
		{
			bBallEnteredChestVolumeOutsideWindow = true;
		}

		if (Snapshot.BallToHeadDistance <= HeadCombinedRadius)
		{
			bBallEnteredHeadVolumeOutsideWindow = true;
		}
	}

	if (
		!bIdealMomentCaptured &&
		Snapshot.Phase == ESoccerAerialActionPhase::Playing &&
		Snapshot.MontagePosition >= Snapshot.IdealContactTime &&
		(
			PreviousMontagePosition < 0.0f ||
			PreviousMontagePosition <= Snapshot.IdealContactTime
		)
	)
	{
		CaptureIdealMoment(Snapshot);
	}

	PreviousSnapshot = Snapshot;
	bPreviousSnapshotValid = true;
}

void ASoccerAerialChestDebugTester::CaptureIdealMoment(
	const FSoccerAerialDebugSnapshot& Snapshot
)
{
	bIdealMomentCaptured = true;
	IdealBallLocation = Snapshot.BallLocation;
	IdealChestPoint = Snapshot.ClosestChestPoint;
	IdealHeadLocation = Snapshot.HeadLocation;

	const FVector ActualTargetPoint =
		ActiveExpectedContactSurface == ESoccerAerialContactSurface::Head
		? IdealHeadLocation
		: IdealChestPoint;
	const FVector TrackToActualTargetWorldError =
		ActualTargetPoint - ActivePlan.ContactLocation;
	IdealTrackToActualChestErrorLocal =
		TestCharacter->GetActorTransform()
		.InverseTransformVectorNoScale(
			TrackToActualTargetWorldError
		);
}

void ASoccerAerialChestDebugTester::FinalizeTest()
{
	if (TestState == ESoccerAerialChestDebugState::Finished)
	{
		return;
	}

	if (!bContactCaptured)
	{
		const FSoccerAerialContactResult LastResult =
			TestCharacter->GetLastAerialContactResult();

		if (LastResult.bContactResolved)
		{
			CapturedContactResult = LastResult;
			bContactCaptured = true;
		}
	}

	FinalClassification = BuildFailureClassification();
	TestState = ESoccerAerialChestDebugState::Finished;

	if (IsValid(TestCharacter))
	{
		TestCharacter->DebugSetForcedAerialContactSurface(
			ESoccerAerialContactSurface::None
		);
		TestCharacter->DebugSetStandingControlPlanningSurface(
			ESoccerAerialContactSurface::None
		);
	}

	if (
		ASoccerAIController* AIController =
			Cast<ASoccerAIController>(TestCharacter->GetController())
	)
	{
		AIController->SetAerialDebugIsolation(false, nullptr);
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("HEADER_CHEST TEST: %s | launch residual %.1f cm"),
		*FinalClassification,
		LaunchCalibrationResidual.Size()
	);

	ScheduleAutomaticRepeat();
}

void ASoccerAerialChestDebugTester::MarkTestFailed(
	const FString& Reason
)
{
	FailureReason = Reason;
	FinalClassification = FString::Printf(
		TEXT("PRUEBA FALLIDA: %s"),
		*Reason
	);
	TestState = ESoccerAerialChestDebugState::Failed;

	if (IsValid(TestCharacter))
	{
		TestCharacter->DebugSetForcedAerialContactSurface(
			ESoccerAerialContactSurface::None
		);
		TestCharacter->DebugSetStandingControlPlanningSurface(
			ESoccerAerialContactSurface::None
		);
		TestCharacter->CancelAerialAction();

		if (
			ASoccerAIController* AIController =
				Cast<ASoccerAIController>(TestCharacter->GetController())
		)
		{
			AIController->SetAerialDebugIsolation(false, nullptr);
		}
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT("HEADER_CHEST TEST: %s"),
		*FinalClassification
	);

	ScheduleAutomaticRepeat();
}

void ASoccerAerialChestDebugTester::ScheduleAutomaticRepeat()
{
	if (
		bAutoRepeat &&
		GetWorld() != nullptr
	)
	{
		GetWorldTimerManager().SetTimer(
			AutoRepeatTimerHandle,
			this,
			&ASoccerAerialChestDebugTester::RunConfiguredTest,
			FMath::Max(0.25f, AutoRepeatDelay),
			false
		);
	}
}

void ASoccerAerialChestDebugTester::DrawTestConfiguration() const
{
	if (GetWorld() == nullptr)
	{
		return;
	}

	if (!IntendedContactLocation.IsNearlyZero())
	{
		DrawDebugSphere(
			GetWorld(),
			IntendedContactLocation,
			12.0f,
			12,
			FColor::Yellow,
			false,
			DebugDrawingLifetime,
			0,
			1.5f
		);
	}

	if (!ConfiguredBallStartLocation.IsNearlyZero())
	{
		DrawDebugSphere(
			GetWorld(),
			ConfiguredBallStartLocation,
			10.0f,
			10,
			FColor::White,
			false,
			DebugDrawingLifetime
		);

		DrawDebugLine(
			GetWorld(),
			ConfiguredBallStartLocation,
			IntendedContactLocation,
			FColor(192, 192, 192),
			false,
			DebugDrawingLifetime,
			0,
			1.0f
		);
	}

	if (ActivePlan.bValid)
	{
		DrawDebugSphere(
			GetWorld(),
			ActivePlan.PreparationLocation,
			18.0f,
			12,
			FColor::Blue,
			false,
			DebugDrawingLifetime,
			0,
			2.0f
		);

		DrawDebugSphere(
			GetWorld(),
			ActivePlan.ContactLocation,
			15.0f,
			12,
			FColor(0, 200, 120),
			false,
			DebugDrawingLifetime,
			0,
			2.0f
		);
	}
}

void ASoccerAerialChestDebugTester::DrawRuntimeDiagnostics(
	const FSoccerAerialDebugSnapshot& Snapshot
) const
{
	if (GetWorld() == nullptr)
	{
		return;
	}

	const FColor WindowColor =
		Snapshot.bContactWindowActive
		? FColor::Green
		: FColor::Red;

	const float HeadCombinedRadius =
		Snapshot.BallRadius +
		Snapshot.HeadContactRadius +
		Snapshot.ExtraTolerance;
	const float ChestCombinedRadius =
		Snapshot.BallRadius +
		Snapshot.ChestContactRadius +
		Snapshot.ExtraTolerance;

	DrawDebugSphere(
		GetWorld(),
		Snapshot.HeadLocation,
		HeadCombinedRadius,
		16,
		FColor(160, 80, 220),
		false,
		DebugDrawingLifetime,
		0,
		1.5f
	);

	DrawDebugLine(
		GetWorld(),
		Snapshot.ChestLowerLocation,
		Snapshot.ChestUpperLocation,
		WindowColor,
		false,
		DebugDrawingLifetime,
		0,
		ChestCombinedRadius * 0.35f
	);

	DrawDebugSphere(
		GetWorld(),
		Snapshot.ChestLowerLocation,
		ChestCombinedRadius,
		12,
		WindowColor,
		false,
		DebugDrawingLifetime,
		0,
		1.0f
	);

	DrawDebugSphere(
		GetWorld(),
		Snapshot.ChestUpperLocation,
		ChestCombinedRadius,
		12,
		WindowColor,
		false,
		DebugDrawingLifetime,
		0,
		1.0f
	);

	DrawDebugPoint(
		GetWorld(),
		Snapshot.ClosestChestPoint,
		12.0f,
		FColor::Cyan,
		false,
		DebugDrawingLifetime
	);

	DrawDebugLine(
		GetWorld(),
		Snapshot.ClosestChestPoint,
		Snapshot.BallLocation,
		FColor::Cyan,
		false,
		DebugDrawingLifetime,
		0,
		1.0f
	);

	/*
	 * Dibujar exclusivamente la trayectoria capturada al lanzar. Duracion 0
	 * significa un solo frame; como se redibuja cada Tick no quedan curvas
	 * antiguas superpuestas.
	 */
	for (int32 Index = 1; Index < PlannedTrajectoryDebugPoints.Num(); ++Index)
	{
		DrawDebugLine(
			GetWorld(),
			PlannedTrajectoryDebugPoints[Index - 1],
			PlannedTrajectoryDebugPoints[Index],
			FColor(64, 224, 208),
			false,
			0.0f,
			0,
			1.5f
		);
	}

	/* Recorrido fisico real de la pelota. */
	for (int32 Index = 1; Index < ActualTrajectoryDebugPoints.Num(); ++Index)
	{
		DrawDebugLine(
			GetWorld(),
			ActualTrajectoryDebugPoints[Index - 1],
			ActualTrajectoryDebugPoints[Index],
			FColor::Magenta,
			false,
			0.0f,
			0,
			2.5f
		);
	}

	if (bHasCurrentPredictedBallLocation)
	{
		DrawDebugPoint(
			GetWorld(),
			CurrentPredictedBallLocation,
			10.0f,
			FColor::Cyan,
			false,
			0.0f
		);

		DrawDebugLine(
			GetWorld(),
			CurrentPredictedBallLocation,
			Snapshot.BallLocation,
			CurrentTrajectoryPredictionError <= 12.0f
				? FColor::Green
				: FColor::Red,
			false,
			0.0f,
			0,
			2.0f
		);
	}

	if (bShowLiveMeasurements)
	{
		const FString RuntimeText = FString::Printf(
			TEXT("header_chest | t %.3f | ventana %s\nChest %.1f / %.1f | Head %.1f / %.1f"),
			Snapshot.MontagePosition,
			Snapshot.bContactWindowActive ? TEXT("ACTIVA") : TEXT("INACTIVA"),
			Snapshot.BallToChestDistance,
			ChestCombinedRadius,
			Snapshot.BallToHeadDistance,
			HeadCombinedRadius
		);

		DrawDebugString(
			GetWorld(),
			TestCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 235.0f),
			RuntimeText,
			nullptr,
			WindowColor,
			DebugDrawingLifetime,
			false,
			1.0f
		);
	}
}

void ASoccerAerialChestDebugTester::DrawResultSummary() const
{
	if (!IsValid(TestCharacter) || GetWorld() == nullptr)
	{
		return;
	}

	/*
	 * Mantener visibles al terminar las dos rutas que ahora nos interesan:
	 *
	 * - cian: trayectoria congelada que predijo BuildPredictedTrajectory();
	 * - magenta: recorrido que hizo realmente la pelota durante la prueba.
	 *
	 * Antes solo quedaba la esfera naranja y era facil interpretarla como el
	 * objetivo elegido por el plan. En realidad representa la ubicacion real de
	 * la pelota cuando el montage alcanzo IdealContactTime.
	 */
	for (int32 Index = 1; Index < PlannedTrajectoryDebugPoints.Num(); ++Index)
	{
		DrawDebugLine(
			GetWorld(),
			PlannedTrajectoryDebugPoints[Index - 1],
			PlannedTrajectoryDebugPoints[Index],
			FColor(64, 224, 208),
			false,
			0.0f,
			0,
			1.5f
		);
	}

	for (int32 Index = 1; Index < ActualTrajectoryDebugPoints.Num(); ++Index)
	{
		DrawDebugLine(
			GetWorld(),
			ActualTrajectoryDebugPoints[Index - 1],
			ActualTrajectoryDebugPoints[Index],
			FColor::Magenta,
			false,
			0.0f,
			0,
			2.5f
		);
	}

	if (ActivePlan.bValid)
	{
		DrawDebugSphere(
			GetWorld(),
			ActivePlan.ContactLocation,
			15.0f,
			12,
			FColor(0, 200, 120),
			false,
			0.0f,
			0,
			2.0f
		);
	}

	if (bIdealMomentCaptured)
	{
		DrawDebugSphere(
			GetWorld(),
			IdealBallLocation,
			10.0f,
			12,
			FColor::Orange,
			false,
			0.0f
		);

		if (ActivePlan.bValid)
		{
			DrawDebugLine(
				GetWorld(),
				ActivePlan.ContactLocation,
				IdealBallLocation,
				FColor::Red,
				false,
				0.0f,
				0,
				2.0f
			);
		}
	}
}


void ASoccerAerialChestDebugTester::UpdateWorldStatusText()
{
	if (!IsValid(WorldStatusText))
	{
		return;
	}

	const bool bShowShadow =
		bShowWorldStatusText &&
		bShowWorldStatusTextShadow &&
		IsValid(WorldStatusTextShadow);

	WorldStatusText->SetVisibility(bShowWorldStatusText, true);
	WorldStatusText->SetHiddenInGame(!bShowWorldStatusText);

	if (IsValid(WorldStatusTextShadow))
	{
		WorldStatusTextShadow->SetVisibility(bShowShadow, true);
		WorldStatusTextShadow->SetHiddenInGame(!bShowShadow);
	}

	if (!bShowWorldStatusText)
	{
		return;
	}

	const FVector AnchorLocation =
		IsValid(TestCharacter)
		? TestCharacter->GetActorLocation()
		: GetActorLocation();

	const FVector TextLocation =
		AnchorLocation + FVector(0.0f, 0.0f, WorldStatusTextHeight);
	const FString StatusString = BuildWorldStatusText();
	const FText StatusText = FText::FromString(StatusString);
	const float SafeWorldSize = FMath::Max(8.0f, WorldStatusTextSize);

	WorldStatusText->SetWorldLocation(TextLocation);
	WorldStatusText->SetWorldSize(SafeWorldSize);
	WorldStatusText->SetTextRenderColor(GetWorldStatusColor());
	WorldStatusText->SetText(StatusText);

	if (IsValid(WorldStatusTextShadow))
	{
		WorldStatusTextShadow->SetWorldSize(SafeWorldSize);
		WorldStatusTextShadow->SetTextRenderColor(FColor::Black);
		WorldStatusTextShadow->SetText(StatusText);
	}

	if (GetWorld() == nullptr)
	{
		return;
	}

	APlayerController* PlayerController =
		GetWorld()->GetFirstPlayerController();

	if (
		!IsValid(PlayerController) ||
		!IsValid(PlayerController->PlayerCameraManager)
	)
	{
		return;
	}

	const FVector CameraLocation =
		PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector ToCamera = CameraLocation - TextLocation;

	if (ToCamera.IsNearlyZero())
	{
		return;
	}

	const FRotator TextRotation = ToCamera.Rotation();
	WorldStatusText->SetWorldRotation(TextRotation);

	if (IsValid(WorldStatusTextShadow))
	{
		const FVector TextRight =
			TextRotation.RotateVector(FVector::RightVector);
		const FVector TextUp =
			TextRotation.RotateVector(FVector::UpVector);
		const FVector CameraDirection = ToCamera.GetSafeNormal();
		const float SafeShadowOffset =
			FMath::Max(0.0f, WorldStatusTextShadowOffset);

		const FVector ShadowLocation =
			TextLocation +
			TextRight * SafeShadowOffset -
			TextUp * SafeShadowOffset -
			CameraDirection * 0.75f;

		WorldStatusTextShadow->SetWorldLocation(ShadowLocation);
		WorldStatusTextShadow->SetWorldRotation(TextRotation);
	}
}

FString ASoccerAerialChestDebugTester::BuildWorldStatusText() const
{
	const float DisplayClosestChestDistance =
		ClosestChestDistance < BIG_NUMBER * 0.5f
		? ClosestChestDistance
		: -1.0f;
	const bool bExpectHead =
		ActiveExpectedContactSurface == ESoccerAerialContactSurface::Head;
	const float DisplayClosestTargetDistance = bExpectHead
		? (ClosestHeadDistance < BIG_NUMBER * 0.5f ? ClosestHeadDistance : -1.0f)
		: DisplayClosestChestDistance;
	const float ClosestTargetMontageTime = bExpectHead
		? ClosestHeadMontageTime
		: ClosestChestMontageTime;
	const float ClosestTargetTimeError = bExpectHead
		? ClosestHeadTimeError
		: ClosestChestTimeError;
	const FVector ClosestTargetErrorLocal = bExpectHead
		? ClosestHeadErrorLocal
		: ClosestChestErrorLocal;

	const float DisplayLaunchResidual =
		LaunchCalibrationResidual.Size();

	const float BallToIntendedContactDistance =
		IsValid(SoccerBall) && !IntendedContactLocation.IsNearlyZero()
		? FVector::Dist(
			SoccerBall->GetActorLocation(),
			IntendedContactLocation
		)
		: -1.0f;

	FString ModeDescription;

	if (TestMode == ESoccerAerialChestDebugMode::ExactMontageContact)
	{
		ModeDescription =
			TEXT("MODO EXACTO TIPO ARQUERO: montage primero | tiro corto al objetivo animado");
	}
	else if (bSnapCharacterToExactPreparationLocation)
	{
		ModeDescription =
			TEXT("MODO SNAP ESPACIAL: objetivo calculado | montage + tiro corto");
	}
	else
	{
		ModeDescription =
			TEXT("MODO PREDICTOR + NAV: una trayectoria | plan fijo | navegacion");

		if (
			FullPredictionRequestedTravelTime >= 0.0f ||
			FullPredictionEstimatedArrivalTime >= 0.0f
		)
		{
			ModeDescription += FString::Printf(
				TEXT(
					"\nPresupuesto NAV: vuelo %.2f s | llegada %.2f s | util %.2f s"
				),
				FullPredictionRequestedTravelTime,
				FullPredictionEstimatedArrivalTime,
				FullPredictionUsefulApproachTime
			);
		}
	}

	if (
		TestMode == ESoccerAerialChestDebugMode::FullPrediction &&
		!bSnapCharacterToExactPreparationLocation
	)
	{
		ModeDescription +=
			TEXT("\nCian=prevista | magenta=real | naranja=pelota en t ideal");
		ModeDescription +=
			TEXT("\nFisica lanzamiento: damping AIR sincronizado desde F7 inicial");

		if (bActualMontageStartCaptured)
		{
			ModeDescription += FString::Printf(
				TEXT("\nInicio montage plan/real/error: %.3f / %.3f / %+.3f s"),
				PlannedMontageStartWorldTime - BallLaunchWorldTime,
				ActualMontageStartWorldTime - BallLaunchWorldTime,
				MontageStartTimingError
			);
		}

		if (bPreviousSnapshotValid)
		{
			const FSoccerAerialDebugSnapshot& Timing = PreviousSnapshot;
			const auto RelativeTime = [this](float WorldTime)
			{
				return WorldTime >= 0.0f && BallLaunchWorldTime > -999.0f
					? WorldTime - BallLaunchWorldTime
					: -1.0f;
			};

			ModeDescription += FString::Printf(
				TEXT("\nRADIOS: max %.1f cm | final %.1f cm | dist en hora %.1f cm"),
				Timing.MaximumCommitDistance,
				Timing.PreparationTolerance,
				Timing.DistanceAtScheduledStart
			);

			ModeDescription += FString::Printf(
				TEXT("\nTIMELINE: entra max %.3f | entra final %.3f | hora plan %.3f s"),
				RelativeTime(Timing.EnteredMaximumCommitRadiusWorldTime),
				RelativeTime(Timing.EnteredPreparationToleranceWorldTime),
				RelativeTime(Timing.ScheduledStartWorldTime)
			);

			ModeDescription += FString::Printf(
				TEXT("\nMONTAGE: request %.3f | catch-up %.3f | logico %.3f s"),
				RelativeTime(Timing.MontagePlayRequestedWorldTime),
				Timing.MontageStartPosition,
				RelativeTime(Timing.MontageLogicalStartWorldTime)
			);

			ModeDescription += FString::Printf(
				TEXT("\nOBSERVADOR: activo %.3f | retorno %.3f s"),
				RelativeTime(Timing.MontageBecameActiveWorldTime),
				Timing.MontagePlayReturnedDuration
			);

			if (Timing.bScheduledCorrectionAttempted)
			{
				ModeDescription += FString::Printf(
					TEXT("\nCORRECCION EN HORA: %.1f -> %.1f cm"),
					Timing.ScheduledCorrectionDistanceBefore,
					Timing.ScheduledCorrectionDistanceAfter
				);
			}
			else
			{
				ModeDescription +=
					TEXT("\nCORRECCION EN HORA: no intentada");
			}
		}
	}

	const int32 RuntimeTesterCount = CountRuntimeHotkeyTesters();
	if (RuntimeTesterCount > 1)
	{
		ModeDescription += FString::Printf(
			TEXT("\nADVERTENCIA: %d testers con F7; solo este actor primario responde"),
			RuntimeTesterCount
		);
	}

	if (IsValid(TestCharacter))
	{
		FVector ChestLowerLocal;
		FVector ChestUpperLocal;
		FVector HeadLocal;

		if (TestCharacter->EvaluateStandingAerialControlContactTrack(
			ActiveProfile.IdealContactTime,
			ChestLowerLocal,
			ChestUpperLocal,
			HeadLocal
		))
		{
			const FVector TargetTrackLocal =
				SelectStandingControlTrackPoint(
					ChestLowerLocal,
					ChestUpperLocal,
					HeadLocal
				);

			ModeDescription += FString::Printf(
				TEXT("\nObjetivo deliberado: %s | seleccion de contacto: NATURAL")
				TEXT("\nTrack objetivo Mixamo F/L/U: %+.1f / %+.1f / %+.1f cm"),
				*GetExpectedSurfaceName(),
				TargetTrackLocal.X,
				TargetTrackLocal.Y,
				TargetTrackLocal.Z
			);
		}
		else
		{
			ModeDescription +=
				TEXT("\nTrack F/L/U: NO ASIGNADO (usando fallback fijo)");
		}
	}

	if (TestMode == ESoccerAerialChestDebugMode::ExactMontageContact)
	{
		ModeDescription += FString::Printf(
			TEXT("\nLanzamiento corto: montage %.3f s | viaje %.3f s | distancia %.0f cm"),
			ExactLaunchMontageTime,
			ExactLaunchTravelTime,
			ExactModeBallStartDistance
		);
	}
	else if (bSnapCharacterToExactPreparationLocation)
	{
		const float AppliedSnapDistance = FVector::Dist2D(
			PredictionLocationBeforeSnap,
			PredictionAppliedSnapLocation
		);

		ModeDescription += FString::Printf(
			TEXT("\nSnap pedido/aplicado/error: %.1f / %.1f / %.1f cm"),
			PredictionRequestedSnapDistance,
			AppliedSnapDistance,
			PredictionSnapApplicationError
		);
	}

	switch (TestState)
	{
	case ESoccerAerialChestDebugState::WaitingForMontage:
		return FString::Printf(
			TEXT(
				"HEADER_CHEST TESTER\n"
				"%s\n"
				"Preparando animacion\n"
				"Pelota a objetivo: %.1f cm\n"
				"Residual predictor (solo modo predictor): %.1f cm"
			),
			*ModeDescription,
			BallToIntendedContactDistance,
			DisplayLaunchResidual
		);

	case ESoccerAerialChestDebugState::Running:
		return FString::Printf(
			TEXT(
				"HEADER_CHEST TESTER - EN CURSO\n"
				"%s\n"
				"Distancia minima a %s: %.1f cm\n"
				"Tiempo real / ideal / error: %.3f / %.3f / %+.3f s\n"
				"Adelante: %+.1f cm | lateral: %+.1f cm\n"
				"Vertical: %+.1f cm | residual predictor: %.1f cm\n"
				"Error trayectoria actual/max: %.1f / %.1f cm\n"
				"Plan->objetivo real F/L/U: %+.1f / %+.1f / %+.1f cm"
			),
			*ModeDescription,
			*GetExpectedSurfaceName(),
			DisplayClosestTargetDistance,
			ClosestTargetMontageTime,
			ActiveProfile.IdealContactTime,
			ClosestTargetTimeError,
			ClosestTargetErrorLocal.X,
			ClosestTargetErrorLocal.Y,
			ClosestTargetErrorLocal.Z,
			DisplayLaunchResidual,
			CurrentTrajectoryPredictionError,
			MaximumTrajectoryPredictionError,
			IdealTrackToActualChestErrorLocal.X,
			IdealTrackToActualChestErrorLocal.Y,
			IdealTrackToActualChestErrorLocal.Z
		);

	case ESoccerAerialChestDebugState::Finished:
		if (bContactCaptured)
		{
			const bool bExpectedSurfaceResolved = DidResolveExpectedSurface();

			return FString::Printf(
				TEXT(
					"RESULTADO: %s\n"
					"%s\n"
					"Objetivo: %s | superficie natural: %s\n"
					"Calidad: %.2f | tiempo: %.3f s\n"
					"Distancia minima a %s: %.1f cm\n"
					"Residual predictor (solo modo predictor): %.1f cm\n"
					"Error trayectoria actual/max: %.1f / %.1f cm\n"
					"F7 repetir | F8 restaurar"
				),
				bExpectedSurfaceResolved
					? TEXT("SUPERFICIE CORRECTA")
					: TEXT("SUPERFICIE INCORRECTA"),
				*ModeDescription,
				*GetExpectedSurfaceName(),
				*AerialContactSurfaceName(
					CapturedContactResult.ContactSurface
				),
				CapturedContactResult.ContactQuality,
				CapturedContactResult.MontagePosition,
				*GetExpectedSurfaceName(),
				DisplayClosestTargetDistance,
				DisplayLaunchResidual,
				CurrentTrajectoryPredictionError,
				MaximumTrajectoryPredictionError
			);
		}

		if (
			bBallEnteredChestVolumeInsideWindow ||
			bBallEnteredHeadVolumeInsideWindow
		)
		{
			return FString::Printf(
				TEXT(
					"RESULTADO: ATRAVESO EL VOLUMEN\n"
					"%s\n"
					"Distancia minima a %s: %.1f cm\n"
					"Tiempo real / ideal / error: %.3f / %.3f / %+.3f s\n"
					"Adelante: %+.1f cm | lateral: %+.1f cm\n"
					"Vertical: %+.1f cm | residual predictor: %.1f cm\n"
					"Error trayectoria actual/max: %.1f / %.1f cm\n"
					"Plan->objetivo real F/L/U: %+.1f / %+.1f / %+.1f cm\n"
					"F7 repetir | F8 restaurar"
				),
				*ModeDescription,
				*GetExpectedSurfaceName(),
				DisplayClosestTargetDistance,
				ClosestTargetMontageTime,
				ActiveProfile.IdealContactTime,
				ClosestTargetTimeError,
				ClosestTargetErrorLocal.X,
				ClosestTargetErrorLocal.Y,
				ClosestTargetErrorLocal.Z,
				DisplayLaunchResidual,
				CurrentTrajectoryPredictionError,
				MaximumTrajectoryPredictionError,
				IdealTrackToActualChestErrorLocal.X,
				IdealTrackToActualChestErrorLocal.Y,
				IdealTrackToActualChestErrorLocal.Z
			);
		}

		if (
			bBallEnteredChestVolumeOutsideWindow ||
			bBallEnteredHeadVolumeOutsideWindow
		)
		{
			return FString::Printf(
				TEXT(
					"RESULTADO: ERROR DE TIEMPO\n"
					"%s\n"
					"Distancia minima a %s: %.1f cm\n"
					"Tiempo real / ideal / error: %.3f / %.3f / %+.3f s\n"
					"Adelante: %+.1f cm | lateral: %+.1f cm\n"
					"Vertical: %+.1f cm | residual predictor: %.1f cm\n"
					"Error trayectoria actual/max: %.1f / %.1f cm\n"
					"Plan->objetivo real F/L/U: %+.1f / %+.1f / %+.1f cm\n"
					"F7 repetir | F8 restaurar"
				),
				*ModeDescription,
				*GetExpectedSurfaceName(),
				DisplayClosestTargetDistance,
				ClosestTargetMontageTime,
				ActiveProfile.IdealContactTime,
				ClosestTargetTimeError,
				ClosestTargetErrorLocal.X,
				ClosestTargetErrorLocal.Y,
				ClosestTargetErrorLocal.Z,
				DisplayLaunchResidual,
				CurrentTrajectoryPredictionError,
				MaximumTrajectoryPredictionError,
				IdealTrackToActualChestErrorLocal.X,
				IdealTrackToActualChestErrorLocal.Y,
				IdealTrackToActualChestErrorLocal.Z
			);
		}

		return FString::Printf(
			TEXT(
				"RESULTADO: ERROR DE POSICION\n"
				"%s\n"
				"Distancia minima a %s: %.1f cm\n"
				"Tiempo real / ideal / error: %.3f / %.3f / %+.3f s\n"
				"Adelante: %+.1f cm | lateral: %+.1f cm\n"
				"Vertical: %+.1f cm | residual predictor: %.1f cm\n"
				"Error trayectoria actual/max: %.1f / %.1f cm\n"
				"Plan->objetivo real F/L/U: %+.1f / %+.1f / %+.1f cm\n"
				"F7 repetir | F8 restaurar"
			),
			*ModeDescription,
			*GetExpectedSurfaceName(),
			DisplayClosestTargetDistance,
			ClosestTargetMontageTime,
			ActiveProfile.IdealContactTime,
			ClosestTargetTimeError,
			ClosestTargetErrorLocal.X,
			ClosestTargetErrorLocal.Y,
			ClosestTargetErrorLocal.Z,
			DisplayLaunchResidual,
			CurrentTrajectoryPredictionError,
			MaximumTrajectoryPredictionError,
			IdealTrackToActualChestErrorLocal.X,
			IdealTrackToActualChestErrorLocal.Y,
			IdealTrackToActualChestErrorLocal.Z
		);

	case ESoccerAerialChestDebugState::Failed:
		return FString::Printf(
			TEXT(
				"RESULTADO: PRUEBA DETENIDA\n"
				"%s\n"
				"Motivo: %s\n"
				"Pelota a objetivo: %.1f cm\n"
				"Distancia minima a %s: %.1f cm\n"
				"Residual predictor (solo modo predictor): %.1f cm\n"
				"F7 repetir | F8 restaurar"
			),
			*ModeDescription,
			*FailureReason,
			BallToIntendedContactDistance,
			*GetExpectedSurfaceName(),
			DisplayClosestTargetDistance,
			DisplayLaunchResidual
		);

	case ESoccerAerialChestDebugState::Idle:
	default:
		{
			FName ResolvedHeadBoneName;
			FName ResolvedChestLowerBoneName;
			FName ResolvedChestUpperBoneName;
			FString BoneFailure;

			const bool bBonesResolved =
				IsValid(TestCharacter) &&
				TestCharacter->ResolveAerialContactBoneNames(
					ResolvedHeadBoneName,
					ResolvedChestLowerBoneName,
					ResolvedChestUpperBoneName,
					&BoneFailure
				);

			if (bBonesResolved)
			{
				return FString::Printf(
					TEXT(
						"HEADER_CHEST TESTER - LISTO\n"
						"%s\n"
						"Huesos: %s | %s | %s\n"
						"F7 ejecutar | F8 restaurar"
					),
					*ModeDescription,
					*ResolvedHeadBoneName.ToString(),
					*ResolvedChestLowerBoneName.ToString(),
					*ResolvedChestUpperBoneName.ToString()
				);
			}

			return FString::Printf(
				TEXT(
					"HEADER_CHEST TESTER - ERROR\n"
					"%s\n"
					"%s\n"
					"Revise el Skeletal Mesh"
				),
				*ModeDescription,
				*BoneFailure
			);
		}
	}
}

FColor ASoccerAerialChestDebugTester::GetWorldStatusColor() const
{
	switch (TestState)
	{
	case ESoccerAerialChestDebugState::WaitingForMontage:
		return FColor::Yellow;

	case ESoccerAerialChestDebugState::Running:
		return FColor::White;

	case ESoccerAerialChestDebugState::Finished:
		return DidResolveExpectedSurface() ? FColor::Green : FColor::Orange;

	case ESoccerAerialChestDebugState::Failed:
		return FColor::Red;

	case ESoccerAerialChestDebugState::Idle:
	default:
		return FColor::Cyan;
	}
}

FString ASoccerAerialChestDebugTester::BuildFailureClassification() const
{
	if (bContactCaptured)
	{
		if (DidResolveExpectedSurface())
		{
			return FString::Printf(
				TEXT("CONTACTO NATURAL CORRECTO: objetivo %s | calidad %.2f | montage %.3fs"),
				*GetExpectedSurfaceName(),
				CapturedContactResult.ContactQuality,
				CapturedContactResult.MontagePosition
			);
		}

		return FString::Printf(
			TEXT("SUPERFICIE NATURAL INCORRECTA: objetivo %s | resultado %s | calidad %.2f"),
			*GetExpectedSurfaceName(),
			*AerialContactSurfaceName(CapturedContactResult.ContactSurface),
			CapturedContactResult.ContactQuality
		);
	}

	if (
		bBallEnteredChestVolumeInsideWindow ||
		bBallEnteredHeadVolumeInsideWindow
	)
	{
		return TEXT(
			"FALLO DE DETECCION/ARBITRAJE: la pelota entro al volumen durante la ventana"
		);
	}

	if (
		bBallEnteredChestVolumeOutsideWindow ||
		bBallEnteredHeadVolumeOutsideWindow
	)
	{
		return FString::Printf(
			TEXT("ERROR TEMPORAL: paso por el cuerpo fuera de ventana | min chest @ %.3fs | ideal %.3fs"),
			ClosestChestMontageTime,
			ActiveProfile.IdealContactTime
		);
	}

	return FString::Printf(
		TEXT("ERROR ESPACIAL: min chest %.1fcm | local adelante %+.1f derecha %+.1f arriba %+.1f"),
		ClosestChestDistance < BIG_NUMBER * 0.5f
			? ClosestChestDistance
			: -1.0f,
		ClosestChestErrorLocal.X,
		ClosestChestErrorLocal.Y,
		ClosestChestErrorLocal.Z
	);
}
