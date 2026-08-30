// ThirdPersonCppCharacter.cpp
#include "ThirdPersonCppCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "SoccerBall.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"

#include "SoccerAICharacter.h"
#include "SoccerMatchManager.h"
#include "SoccerDebugManager.h"
#include "GameHUD.h"

AThirdPersonCppCharacter::AThirdPersonCppCharacter()
{
	SelectedMovementSpeed = JogSpeed;
	GetCharacterMovement()->MaxWalkSpeed = SelectedMovementSpeed;

	PrimaryActorTick.bCanEverTick = true;
	// set our turn rates for input
	BaseTurnRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	GetCharacterMovement()->BrakingDecelerationWalking = 350.0f; ///////////////////////////////////// *** *** ***
	GetCharacterMovement()->GroundFriction = 4.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character
	// Sube la c�mara.
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 200.0f);

	// Baja el target/pivote de la c�mara.
	CameraBoom->TargetOffset = FVector(0.0f, 0.0f, -80.0f);

	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)

	PossessionIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PossessionIndicator"));
	PossessionIndicator->SetupAttachment(RootComponent);

	PossessionIndicator->SetRelativeLocation(FVector(0.0f, 0.0f, PossessionIndicatorHeight));

	// Esta rotaci�n depende del mesh que uses.
	// Si us�s un Cone de Unreal y apunta hacia arriba, esto lo da vuelta para que apunte hacia abajo.
	PossessionIndicator->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));

	PossessionIndicator->SetRelativeScale3D(PossessionIndicatorBaseScale);

	PossessionIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PossessionIndicator->SetCollisionProfileName(TEXT("NoCollision"));
	PossessionIndicator->SetGenerateOverlapEvents(false);
	PossessionIndicator->SetHiddenInGame(true);
	PossessionIndicator->SetVisibility(false);
	PossessionIndicator->CastShadow = false;
}

//////////////////////////////////////////////////////////////////////////
// Input

void AThirdPersonCppCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction(
		"Jump",
		IE_Pressed,
		this,
		&AThirdPersonCppCharacter::HandleHumanRunningJump
	);

	PlayerInputComponent->BindAxis("MoveForward", this, &AThirdPersonCppCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AThirdPersonCppCharacter::MoveRight);

	PlayerInputComponent->BindAction("SelectWalkSpeed", IE_Pressed, this, &AThirdPersonCppCharacter::SelectWalkSpeed);
	PlayerInputComponent->BindAction("SelectJogSpeed", IE_Pressed, this, &AThirdPersonCppCharacter::SelectJogSpeed);
	PlayerInputComponent->BindAction("SelectRunSpeed", IE_Pressed, this, &AThirdPersonCppCharacter::SelectRunSpeed);
	PlayerInputComponent->BindAction("SelectFastRunSpeed", IE_Pressed, this, &AThirdPersonCppCharacter::SelectFastRunSpeed);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &AThirdPersonCppCharacter::TurnCameraWithAcceleration);
	PlayerInputComponent->BindAxis("TurnRate", this, &AThirdPersonCppCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &AThirdPersonCppCharacter::MoveAimCursorVertical);
	PlayerInputComponent->BindAction("CameraRightSide", IE_Pressed, this, &AThirdPersonCppCharacter::SetCameraRightSide);
	PlayerInputComponent->BindAction("CameraRightSide", IE_Released, this, &AThirdPersonCppCharacter::ResetCameraBehind);

	PlayerInputComponent->BindAction("CameraFront", IE_Pressed, this, &AThirdPersonCppCharacter::SetCameraFront);
	PlayerInputComponent->BindAction("CameraFront", IE_Released, this, &AThirdPersonCppCharacter::ResetCameraBehind);

	PlayerInputComponent->BindAction("CameraLeftSide", IE_Pressed, this, &AThirdPersonCppCharacter::SetCameraLeftSide);
	PlayerInputComponent->BindAction("CameraLeftSide", IE_Released, this, &AThirdPersonCppCharacter::ResetCameraBehind);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AThirdPersonCppCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AThirdPersonCppCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AThirdPersonCppCharacter::OnResetVR);
	PlayerInputComponent->BindAction("StartBallControl", IE_Pressed, this, &AThirdPersonCppCharacter::StartBallControl);
	PlayerInputComponent->BindAction("Tackle", IE_Pressed, this, &AThirdPersonCppCharacter::HandleTackleInput);
	// Direct debug binding: no Project Settings Action Mapping is required.
	PlayerInputComponent->BindKey(EKeys::NumPadSix, IE_Pressed, this, &AThirdPersonCppCharacter::DebugStartOpponentPenalty);
	PlayerInputComponent->BindKey(EKeys::NumPadSeven, IE_Pressed, this, &AThirdPersonCppCharacter::DebugStartPlayerTeamPenalty);

	// Stage 12B: one dedicated toggle is available on keyboard and gamepad.
	// When the menu is open, UIOnly input means the widget owns the same keys
	// and closes itself; gameplay never receives menu-navigation buttons.
	FInputKeyBinding& FormationMenuKeyboardBinding = PlayerInputComponent->BindKey(
		EKeys::M,
		IE_Pressed,
		this,
		&AThirdPersonCppCharacter::ToggleFormationMenu
	);
	FormationMenuKeyboardBinding.bExecuteWhenPaused = true;

	FInputKeyBinding& FormationMenuGamepadBinding = PlayerInputComponent->BindKey(
		EKeys::Gamepad_Special_Right,
		IE_Pressed,
		this,
		&AThirdPersonCppCharacter::ToggleFormationMenu
	);
	FormationMenuGamepadBinding.bExecuteWhenPaused = true;

	// Stage 16C: compact four-preset selector. It uses a different dedicated
	// toggle from the full coach menu so both surfaces remain predictable.
	FInputKeyBinding& QuickTacticsKeyboardBinding = PlayerInputComponent->BindKey(
		EKeys::Tab,
		IE_Pressed,
		this,
		&AThirdPersonCppCharacter::ToggleQuickTacticsMenu
	);
	QuickTacticsKeyboardBinding.bExecuteWhenPaused = true;

	FInputKeyBinding& QuickTacticsGamepadBinding = PlayerInputComponent->BindKey(
		EKeys::Gamepad_Special_Left,
		IE_Pressed,
		this,
		&AThirdPersonCppCharacter::ToggleQuickTacticsMenu
	);
	QuickTacticsGamepadBinding.bExecuteWhenPaused = true;
	PlayerInputComponent->BindAction("KickToTargetFollow", IE_Pressed, this, &AThirdPersonCppCharacter::HandleLeftClickTarget);
	PlayerInputComponent->BindAction("KickToTargetRelease", IE_Pressed, this, &AThirdPersonCppCharacter::StartChargedKickRelease);
	PlayerInputComponent->BindAction("KickToTargetRelease", IE_Released, this, &AThirdPersonCppCharacter::FinishChargedKickRelease);

	PlayerInputComponent->BindAction(
		"RequestNormalPass",
		IE_Pressed,
		this,
		&AThirdPersonCppCharacter::RequestNormalPassFromTeammate
	);

	PlayerInputComponent->BindAction(
		"RequestAerialPass",
		IE_Pressed,
		this,
		&AThirdPersonCppCharacter::RequestAerialPassFromTeammate
	);
}


void AThirdPersonCppCharacter::ToggleFormationMenu()
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (PlayerController == nullptr)
	{
		return;
	}

	AGameHUD* GameHUD = Cast<AGameHUD>(PlayerController->GetHUD());
	if (GameHUD == nullptr)
	{
		return;
	}

	GameHUD->ToggleFormationMenu();
}

void AThirdPersonCppCharacter::ToggleQuickTacticsMenu()
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (PlayerController == nullptr)
	{
		return;
	}

	AGameHUD* GameHUD = Cast<AGameHUD>(PlayerController->GetHUD());
	if (GameHUD == nullptr)
	{
		return;
	}

	GameHUD->ToggleQuickTacticsMenu();
}

void AThirdPersonCppCharacter::DebugStartOpponentPenalty()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
	{
		ASoccerMatchManager* SoccerMatchManager = *It;
		if (!IsValid(SoccerMatchManager))
		{
			continue;
		}

		SoccerMatchManager->DebugStartPenaltyKickForTeam(ESoccerTeam::OpponentTeam);
		return;
	}

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Restarts,
		TEXT("NUMPAD 6 PENAL TEST: no se encontro SoccerMatchManager"),
		FColor::Red
	);
}


void AThirdPersonCppCharacter::DebugStartPlayerTeamPenalty()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
	{
		ASoccerMatchManager* SoccerMatchManager = *It;
		if (!IsValid(SoccerMatchManager))
		{
			continue;
		}

		SoccerMatchManager->DebugStartPenaltyKickForTeam(ESoccerTeam::PlayerTeam);
		return;
	}

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Restarts,
		TEXT("NUMPAD 7 PENAL TEST: no se encontro SoccerMatchManager"),
		FColor::Red
	);
}


void AThirdPersonCppCharacter::OnResetVR()
{
	// If ThirdPersonCpp is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in ThirdPersonCpp.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AThirdPersonCppCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AThirdPersonCppCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AThirdPersonCppCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AThirdPersonCppCharacter::MoveForward(float Value)
{
	if (IsTackleActive() || IsTackleFallReactionActive() || IsAerialActionLocked())
	{
		return;
	}

	if (
		IsValid(MatchManager) &&
		MatchManager->IsHumanThrowInMovementLocked(this)
	)
	{
		return;
	}

	LastMoveForwardInputValue = Value;
	UpdateChaseCancelIgnoreAfterInputChanged();

	if (FMath::IsNearlyZero(Value))
	{
		return;
	}

	if (Controller == nullptr)
	{
		return;
	}

	if (
		SoccerControlState == ESoccerPlayerControlState::Kicking ||
		SoccerControlState == ESoccerPlayerControlState::DribbleTurning
		)
	{
		return;
	}

	if (SoccerControlState == ESoccerPlayerControlState::ChasingBall)
	{
		if (ShouldMovementInputCancelChase())
		{
			CancelBallChaseByManualInput();
		}
		else
		{
			return;
		}
	}

	if (
		SoccerControlState != ESoccerPlayerControlState::Manual &&
		SoccerControlState != ESoccerPlayerControlState::PossessingBall
		)
	{
		return;
	}

	FVector Direction;

	if (bCameraQuickViewActive)
	{
		Direction = GetActorForwardVector();
	}
	else
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);
		Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	}

	if (SoccerControlState == ESoccerPlayerControlState::PossessingBall)
	{
		RegisterDribbleInput(Direction, Value);
		return;
	}

	AddMovementInput(Direction, Value);
}

void AThirdPersonCppCharacter::MoveRight(float Value)
{
	if (IsTackleActive() || IsTackleFallReactionActive() || IsAerialActionLocked())
	{
		return;
	}

	if (
		IsValid(MatchManager) &&
		MatchManager->IsHumanThrowInMovementLocked(this)
	)
	{
		return;
	}

	LastMoveRightInputValue = Value;
	UpdateChaseCancelIgnoreAfterInputChanged();

	if (FMath::IsNearlyZero(Value))
	{
		return;
	}

	if (Controller == nullptr)
	{
		return;
	}

	if (
		SoccerControlState == ESoccerPlayerControlState::Kicking ||
		SoccerControlState == ESoccerPlayerControlState::DribbleTurning
		)
	{
		return;
	}

	if (SoccerControlState == ESoccerPlayerControlState::ChasingBall)
	{
		if (ShouldMovementInputCancelChase())
		{
			CancelBallChaseByManualInput();
		}
		else
		{
			return;
		}
	}

	if (
		SoccerControlState != ESoccerPlayerControlState::Manual &&
		SoccerControlState != ESoccerPlayerControlState::PossessingBall
		)
	{
		return;
	}

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (SoccerControlState == ESoccerPlayerControlState::PossessingBall)
	{
		RegisterDribbleInput(Direction, Value);
		return;
	}

	AddMovementInput(Direction, Value);
}

void AThirdPersonCppCharacter::RegisterDribbleInput(const FVector& Direction, float Value)
{
	if (FMath::IsNearlyZero(Value))
	{
		return;
	}

	FVector FlatDirection = Direction;
	FlatDirection.Z = 0.0f;
	FlatDirection = FlatDirection.GetSafeNormal();

	if (FlatDirection.IsNearlyZero())
	{
		return;
	}

	PendingDribbleInputDirection += FlatDirection * Value;
}

void AThirdPersonCppCharacter::AddScore(int32 Amount)
{
	Score += Amount;

	UE_LOG(LogTemp, Warning, TEXT("Player score: %d"), Score);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Yellow,
			FString::Printf(TEXT("Puntaje total: %d"), Score)
		);
	}
}

int32 AThirdPersonCppCharacter::GetScore() const
{
	return Score;
}

bool AThirdPersonCppCharacter::IsPossessingBall() const
{
	return SoccerControlState == ESoccerPlayerControlState::PossessingBall;
}

bool AThirdPersonCppCharacter::IsChasingBall() const
{
	return SoccerControlState == ESoccerPlayerControlState::ChasingBall;
}

bool AThirdPersonCppCharacter::IsKicking() const
{
	return SoccerControlState == ESoccerPlayerControlState::Kicking;
}

bool AThirdPersonCppCharacter::HasActiveHumanBallClaim() const
{
	if (!bHumanBallClaimActive)
	{
		return false;
	}

	return
		SoccerControlState == ESoccerPlayerControlState::ChasingBall ||
		SoccerControlState == ESoccerPlayerControlState::Kicking ||
		SoccerControlState == ESoccerPlayerControlState::DribbleTurning ||
		bIsHumanStealAttemptActive;
}

bool AThirdPersonCppCharacter::IsHumanJumpHeaderRequestActive() const
{
	return bHumanJumpHeaderRequestActive;
}

bool AThirdPersonCppCharacter::GetHumanJumpHeaderHUDStatus(
	FString& OutText,
	FLinearColor& OutColor
) const
{
	OutText.Reset();
	OutColor = FLinearColor::White;

	if (!bHumanJumpHeaderRequestActive)
	{
		return false;
	}

	if (HeaderJumpKickMontage == nullptr)
	{
		OutText = TEXT("CABEZAZO: FALTA HEADER JUMP KICK MONTAGE");
		OutColor = FLinearColor(1.0f, 0.20f, 0.15f, 1.0f);
		return true;
	}

	if (JumpHeaderKickContactTrackCurveTable == nullptr)
	{
		OutText = TEXT("CABEZAZO: FALTA CURVE TABLE DE SALTO");
		OutColor = FLinearColor(1.0f, 0.20f, 0.15f, 1.0f);
		return true;
	}

	if (GetMesh() == nullptr || GetMesh()->GetAnimInstance() == nullptr)
	{
		OutText = TEXT("CABEZAZO: FALTA ANIM INSTANCE");
		OutColor = FLinearColor(1.0f, 0.20f, 0.15f, 1.0f);
		return true;
	}

	switch (GetAerialActionPhase())
	{
	case ESoccerAerialActionPhase::Approaching:
		OutText = TEXT("CABEZAZO: YENDO AL PUNTO");
		OutColor = FLinearColor(1.0f, 0.72f, 0.15f, 1.0f);
		break;

	case ESoccerAerialActionPhase::WaitingToStart:
		OutText = TEXT("CABEZAZO: ESPERANDO SALTO");
		OutColor = FLinearColor(1.0f, 0.88f, 0.25f, 1.0f);
		break;

	case ESoccerAerialActionPhase::Playing:
		OutText = TEXT("CABEZAZO: EN EJECUCION");
		OutColor = FLinearColor(0.30f, 1.0f, 0.35f, 1.0f);
		break;

	case ESoccerAerialActionPhase::None:
	default:
	{
		OutText = TEXT("CABEZAZO: BUSCANDO SOLUCION");
		OutColor = FLinearColor(0.35f, 0.80f, 1.0f, 1.0f);

		const UWorld* World = GetWorld();
		if (World != nullptr && HumanJumpHeaderRequestStartWorldTime >= 0.0f)
		{
			const float Remaining = FMath::Max(
				0.0f,
				HumanJumpHeaderPlanSearchTimeout -
					(World->GetTimeSeconds() - HumanJumpHeaderRequestStartWorldTime)
			);
			OutText += FString::Printf(TEXT("  %.1f s"), Remaining);
		}
		break;
	}
	}

	return true;
}

void AThirdPersonCppCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Restarts and goalkeeper hand possession are authoritative. Clear any
	// direct ball action immediately, before assisted aerial/chase logic has a
	// chance to update. This also makes the cleanup resilient to a restriction
	// that begins after the user's click was already pressed.
	if (!IsHumanBallActionAllowedNow())
	{
		// After a legal human set-piece impact the no-retouch rule becomes active
		// immediately. Preserve only the already-completed kick montage; new ball
		// actions remain blocked until another player touches the ball.
		const bool bPreserveCompletedRestartKickAnimation =
			SoccerControlState == ESoccerPlayerControlState::Kicking &&
			bActiveKickHasImpactedBall &&
			bActiveKickWasHumanRestartExecution;

		if (!bPreserveCompletedRestartKickAnimation)
		{
			ClearBallActionsForMatchRestriction();
		}
	}

	/*
	 * ASoccerCharacterBase updates/cancels the predictive aerial plan in its
	 * Tick. Observe the result before processing human locomotion so a lost
	 * jump-header solution cannot leave a stale target or charge armed.
	 */
	UpdateHumanJumpHeaderRequestLifecycle(DeltaTime);

	if (IsTackleActive() || IsTackleFallReactionActive())
	{
		PendingDribbleInputDirection = FVector::ZeroVector;
		UpdatePlayerEnergy(DeltaTime);
		UpdateCameraQuickView(DeltaTime);
		UpdatePossessionIndicator();

		SetSoccerAnimationState(
			false,
			false,
			false,
			GetPlayerEnergyPercent(),
			false,
			0.0f
		);
		return;
	}

	if (IsAerialActionLocked())
	{
		PendingDribbleInputDirection = FVector::ZeroVector;
		UpdatePlayerEnergy(DeltaTime);
		UpdateCameraQuickView(DeltaTime);
		UpdatePossessionIndicator();

		SetSoccerAnimationState(
			false,
			false,
			false,
			GetPlayerEnergyPercent(),
			false,
			0.0f
		);
		return;
	}

	// Si est� activo el intento de robo humano, este sistema toma control
	// del movimiento hacia la pelota del bot rival.
	if (IsAerialActionApproaching())
	{
		MoveTowardAerialPreparation();
	}
	else if (IsAerialActionWaitingToStart())
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}
	else if (bIsHumanStealAttemptActive)
	{
		UpdateHumanStealAttempt();
	}
	else if (SoccerControlState == ESoccerPlayerControlState::ChasingBall)
	{
		MoveTowardBall();
	}
	else if (SoccerControlState == ESoccerPlayerControlState::PossessingBall)
	{
		UpdatePhysicalDribbleControl();
	}
	else if (SoccerControlState == ESoccerPlayerControlState::DribbleTurning)
	{
		if (
			!bActiveStrongRunDribbleTurnHasImpactedBall &&
			!bActiveNormalRunDribbleTurnHasImpactedBall
			)
		{
			UpdatePossessedBallLocation();
		}
	}
	else if (SoccerControlState == ESoccerPlayerControlState::Kicking)
	{
		const bool bKeepRestartBallStationary =
			IsValid(MatchManager) &&
			MatchManager->CanHumanFootRestartTakerExecuteNow(this);

		if (
			!bActiveKickHasImpactedBall &&
			!bKeepRestartBallStationary
		)
		{
			UpdatePossessedBallLocation();
		}
	}

	UpdateStrongRunDribbleTurnActorRotation(DeltaTime);
	UpdateNormalRunDribbleTurnActorRotation(DeltaTime);

	UpdateStrongRunDribbleTurnMovement(DeltaTime);
	UpdateNormalRunDribbleTurnMovement(DeltaTime);

	UpdateAutoPassCollectCarry();

	UpdatePlayerEnergy(DeltaTime);

	UpdateCameraQuickView(DeltaTime);

	UpdatePossessionIndicator();
	
	SetSoccerAnimationState(
		IsPossessingBall(),
		IsChasingBall(),
		IsKicking(),
		GetPlayerEnergyPercent(),
		ShouldForceDribbleTurnLocomotion(),
		GetForcedDribbleTurnLocomotionSpeed()
	);
}

void AThirdPersonCppCharacter::UpdatePossessionIndicator()
{
	if (PossessionIndicator == nullptr)
	{
		return;
	}

	const bool bShouldShowIndicator =
		SoccerControlState == ESoccerPlayerControlState::PossessingBall;

	PossessionIndicator->SetHiddenInGame(!bShouldShowIndicator);
	PossessionIndicator->SetVisibility(bShouldShowIndicator, true);

	if (!bShouldShowIndicator)
	{
		return;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	const float Pulse =
		1.0f
		+ FMath::Sin(CurrentTime * PossessionIndicatorPulseSpeed * 2.0f * PI)
		* PossessionIndicatorPulseAmount;

	const float BobOffset =
		FMath::Sin(CurrentTime * PossessionIndicatorBobSpeed * 2.0f * PI)
		* PossessionIndicatorBobHeight;

	PossessionIndicator->SetRelativeLocation(
		FVector(
			0.0f,
			0.0f,
			PossessionIndicatorHeight + BobOffset
		)
	);

	PossessionIndicator->SetRelativeScale3D(
		PossessionIndicatorBaseScale * Pulse
	);
}

AActor* AThirdPersonCppCharacter::SpawnTargetMarker(
	TSubclassOf<AActor> MarkerClass,
	const FVector& TargetLocation
)
{
	if (GetWorld() == nullptr || MarkerClass == nullptr)
	{
		return nullptr;
	}

	FVector MarkerLocation = TargetLocation;
	MarkerLocation.Z += KickTargetMarkerGroundOffset;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedMarker = GetWorld()->SpawnActor<AActor>(
		MarkerClass,
		MarkerLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);

	DisableMarkerCollision(SpawnedMarker);

	return SpawnedMarker;
}

void AThirdPersonCppCharacter::ShowAutoPassTargetMarker(const FVector& TargetLocation)
{
	if (AutoPassTargetCrossMarkerClass == nullptr)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.0f,
				FColor::Red,
				TEXT("AutoPassTargetCrossMarkerClass es nullptr. Asignar BP azul en el Blueprint del personaje.")
			);
		}

		return;
	}

	HideAutoPassTargetMarker();

	ActiveAutoPassTargetMarker =
		SpawnTargetMarker(AutoPassTargetCrossMarkerClass, TargetLocation);

	if (ActiveAutoPassTargetMarker == nullptr)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		AutoPassTargetMarkerTimerHandle,
		this,
		&AThirdPersonCppCharacter::HideAutoPassTargetMarker,
		KickTargetMarkerLifeTime,
		false
	);
}

void AThirdPersonCppCharacter::HideAutoPassTargetMarker()
{
	GetWorldTimerManager().ClearTimer(AutoPassTargetMarkerTimerHandle);

	if (ActiveAutoPassTargetMarker != nullptr)
	{
		ActiveAutoPassTargetMarker->Destroy();
		ActiveAutoPassTargetMarker = nullptr;
	}
}

void AThirdPersonCppCharacter::ShowReleaseTargetMarker(const FVector& TargetLocation)
{
	if (ReleaseTargetCrossMarkerClass == nullptr)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.0f,
				FColor::Red,
				TEXT("ReleaseTargetCrossMarkerClass es nullptr. Asignar BP verde en el Blueprint del personaje.")
			);
		}

		return;
	}

	HideReleaseTargetMarker();

	ActiveReleaseTargetMarker =
		SpawnTargetMarker(ReleaseTargetCrossMarkerClass, TargetLocation);

	if (ActiveReleaseTargetMarker == nullptr)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		ReleaseTargetMarkerTimerHandle,
		this,
		&AThirdPersonCppCharacter::HideReleaseTargetMarker,
		KickTargetMarkerLifeTime,
		false
	);
}

void AThirdPersonCppCharacter::HideReleaseTargetMarker()
{
	GetWorldTimerManager().ClearTimer(ReleaseTargetMarkerTimerHandle);

	if (ActiveReleaseTargetMarker != nullptr)
	{
		ActiveReleaseTargetMarker->Destroy();
		ActiveReleaseTargetMarker = nullptr;
	}
}

void AThirdPersonCppCharacter::DisableMarkerCollision(AActor* MarkerActor) const
{
	if (MarkerActor == nullptr)
	{
		return;
	}

	MarkerActor->SetActorEnableCollision(false);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	MarkerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent == nullptr)
		{
			continue;
		}

		PrimitiveComponent->SetCollisionProfileName(TEXT("NoCollision"));
		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
	}
}

void AThirdPersonCppCharacter::SetSelectedMovementSpeed(float NewSpeed, const FString& SpeedLabel)
{
	SelectedMovementSpeed = NewSpeed;

	if (
		SoccerControlState == ESoccerPlayerControlState::Manual ||
		SoccerControlState == ESoccerPlayerControlState::PossessingBall ||
		SoccerControlState == ESoccerPlayerControlState::ChasingBall
		)
	{
		UpdateEnergyAdjustedMovementSpeed();
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Yellow,
			FString::Printf(TEXT("Velocidad seleccionada: %s"), *SpeedLabel)
		);
	}

	UE_LOG(LogTemp, Warning, TEXT("Selected movement speed: %s - %.2f"), *SpeedLabel, NewSpeed);
}

void AThirdPersonCppCharacter::SelectWalkSpeed()
{
	SetSelectedMovementSpeed(WalkSpeed, TEXT("Caminar"));
}

void AThirdPersonCppCharacter::SelectJogSpeed()
{
	SetSelectedMovementSpeed(JogSpeed, TEXT("Trotar"));
}

void AThirdPersonCppCharacter::SelectRunSpeed()
{
	SetSelectedMovementSpeed(RunSpeed, TEXT("Correr"));
}

void AThirdPersonCppCharacter::SelectFastRunSpeed()
{
	SetSelectedMovementSpeed(FastRunSpeed, TEXT("Correr fuerte"));
}

float AThirdPersonCppCharacter::GetPlayerEnergyPercent() const
{
	if (MaxPlayerEnergy <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(PlayerEnergy / MaxPlayerEnergy, 0.0f, 1.0f);
}

bool AThirdPersonCppCharacter::IsSelectedMovementSpeed(float Speed) const
{
	return FMath::IsNearlyEqual(SelectedMovementSpeed, Speed, 0.1f);
}

float AThirdPersonCppCharacter::GetEnergyAdjustedFastRunSpeed() const
{
	const float SafeMaxEnergy = FMath::Max(1.0f, MaxPlayerEnergy);

	const float FullSpeedEnergy =
		FMath::Clamp(FastRunFullSpeedEnergy, 0.0f, SafeMaxEnergy);

	const float MinimumSpeedEnergy =
		FMath::Clamp(FastRunMinimumSpeedEnergy, 0.0f, FullSpeedEnergy);

	if (PlayerEnergy >= FullSpeedEnergy)
	{
		return FastRunSpeed;
	}

	if (PlayerEnergy <= MinimumSpeedEnergy)
	{
		return FastRunMinimumSpeed;
	}

	const float EnergyRange =
		FMath::Max(0.01f, FullSpeedEnergy - MinimumSpeedEnergy);

	const float Alpha =
		(PlayerEnergy - MinimumSpeedEnergy) / EnergyRange;

	return FMath::Lerp(
		FastRunMinimumSpeed,
		FastRunSpeed,
		Alpha
	);
}

void AThirdPersonCppCharacter::UpdateEnergyAdjustedMovementSpeed()
{
	if (GetCharacterMovement() == nullptr)
	{
		return;
	}

	// Throw-in return/setup movement is scripted by the match manager. Do not
	// overwrite the temporary speed while that controlled movement is active.
	if (bHumanThrowInScriptedMovementActive)
	{
		return;
	}

	if (
		SoccerControlState != ESoccerPlayerControlState::Manual &&
		SoccerControlState != ESoccerPlayerControlState::PossessingBall &&
		SoccerControlState != ESoccerPlayerControlState::ChasingBall
		)
	{
		return;
	}

	float NewMaxWalkSpeed = SelectedMovementSpeed;

	if (IsSelectedMovementSpeed(FastRunSpeed))
	{
		NewMaxWalkSpeed = GetEnergyAdjustedFastRunSpeed();
	}

	GetCharacterMovement()->MaxWalkSpeed = NewMaxWalkSpeed;
}

void AThirdPersonCppCharacter::UpdatePlayerEnergy(float DeltaTime)
{
	if (DeltaTime <= 0.0f || MaxPlayerEnergy <= 0.0f)
	{
		return;
	}

	const bool bIsMoving =
		GetVelocity().Size2D() > EnergyMovingSpeedThreshold;

	float EnergyChangePerSecond = IdleEnergyRecoveryPerSecond;

	if (bIsMoving)
	{
		if (IsSelectedMovementSpeed(FastRunSpeed))
		{
			EnergyChangePerSecond = -FastRunEnergyDrainPerSecond;
		}
		else if (IsSelectedMovementSpeed(RunSpeed))
		{
			EnergyChangePerSecond = -RunEnergyDrainPerSecond;
		}
		else if (IsSelectedMovementSpeed(JogSpeed))
		{
			EnergyChangePerSecond = JogEnergyRecoveryPerSecond;
		}
		else if (IsSelectedMovementSpeed(WalkSpeed))
		{
			EnergyChangePerSecond = WalkEnergyRecoveryPerSecond;
		}
	}
	else
	{
		EnergyChangePerSecond = IdleEnergyRecoveryPerSecond;
	}

	PlayerEnergy = FMath::Clamp(
		PlayerEnergy + EnergyChangePerSecond * DeltaTime,
		0.0f,
		MaxPlayerEnergy
	);

	UpdateEnergyAdjustedMovementSpeed();
}

void AThirdPersonCppCharacter::BeginPlay()
{
	Super::BeginPlay();

	for (TActorIterator<ASoccerBall> It(GetWorld()); It; ++It)
	{
		ControlledBall = *It;
		break;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Controller);

	if (PlayerController)
	{
		PlayerController->bShowMouseCursor = false;
		PlayerController->bEnableClickEvents = true;
		PlayerController->bEnableMouseOverEvents = false;

		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
	}
}

bool AThirdPersonCppCharacter::ResolveAerialActiveHeaderTarget(
    FVector& OutTargetLocation
) const
{
    if (
        PendingKickMode != ESoccerPendingKickMode::None &&
        !PendingKickTarget.IsNearlyZero()
    )
    {
        OutTargetLocation = PendingKickTarget;
        return true;
    }

    return Super::ResolveAerialActiveHeaderTarget(
        OutTargetLocation
    );
}

bool AThirdPersonCppCharacter::ResolveAerialStandingHeaderRedirectTarget(
    FVector& OutTargetLocation
) const
{
    if (
        PendingKickMode != ESoccerPendingKickMode::None &&
        !PendingKickTarget.IsNearlyZero()
    )
    {
        OutTargetLocation = PendingKickTarget;
        return true;
    }

    return Super::ResolveAerialStandingHeaderRedirectTarget(
        OutTargetLocation
    );
}

float AThirdPersonCppCharacter::ResolveAerialActiveHeaderSpeedOverride() const
{
    float RequestedHorizontalSpeed = PendingKickHorizontalSpeedOverride;

    /*
     * The montage may need to start while the right button is still held.
     * Use the live charge value so starting early does not freeze the header
     * at the minimum power. Releasing the button stores the final value.
     */
    if (bHumanJumpHeaderRequestActive && bIsChargingKickRelease)
    {
        const float ChargePercent = GetCurrentKickChargePercent();
        RequestedHorizontalSpeed = FMath::Lerp(
            KickChargeMinHorizontalSpeed,
            KickChargeMaxHorizontalSpeed,
            ChargePercent
        );
    }

    if (
        !bHasPendingKickHorizontalSpeedOverride ||
        RequestedHorizontalSpeed <= 0.0f
    )
    {
        return Super::ResolveAerialActiveHeaderSpeedOverride();
    }

    const float MinimumSpeed =
        FMath::Max(0.0f, ChargedHeaderMinimumSpeed);

    const float MaximumSpeed =
        FMath::Max(MinimumSpeed, ChargedHeaderMaximumSpeed);

    return FMath::Clamp(
        RequestedHorizontalSpeed *
            FMath::Max(0.0f, ChargedHeaderSpeedMultiplier),
        MinimumSpeed,
        MaximumSpeed
    );
}

void AThirdPersonCppCharacter::OnAerialBallContactResolved(
    const FSoccerAerialContactResult& ContactResult
)
{
    Super::OnAerialBallContactResolved(ContactResult);

    const ESoccerPendingKickMode CompletedKickMode =
        PendingKickMode;
    const FVector CompletedKickTarget =
        PendingKickTarget;

    bHumanJumpHeaderRequestActive = false;
    bHumanJumpHeaderMontageStarted = false;

    PendingKickMode = ESoccerPendingKickMode::None;
    PendingKickTarget = FVector::ZeroVector;
    bHasPendingKickHorizontalSpeedOverride = false;
    PendingKickHorizontalSpeedOverride = 0.0f;
    bIsChargingKickRelease = false;

    HideAutoPassTargetMarker();
    HideReleaseTargetMarker();

    if (!ContactResult.bTouchAcceptedByRules)
    {
        EnterManualControl();
        return;
    }

    if (
        ContactResult.ActionType ==
            ESoccerAerialActionType::JumpHeaderKick
    )
    {
        if (
            CompletedKickMode ==
                ESoccerPendingKickMode::KickAndFollow &&
            !CompletedKickTarget.IsNearlyZero()
        )
        {
            StartAutoPassFollow(CompletedKickTarget);
        }
        else
        {
            EnterManualControl();
        }

        return;
    }

    if (
        ContactResult.ActionType ==
            ESoccerAerialActionType::StandingControl &&
        ContactResult.ContactSurface ==
            ESoccerAerialContactSurface::Head &&
        CompletedKickMode != ESoccerPendingKickMode::None
    )
    {
        if (
            CompletedKickMode ==
                ESoccerPendingKickMode::KickAndFollow
        )
        {
            FVector FollowTarget = CompletedKickTarget;
            const FSoccerAerialInterceptionPlan& CompletedPlan =
                GetQueuedAerialPlan();

            if (!CompletedPlan.PredictedRecoveryLocation.IsNearlyZero())
            {
                FollowTarget = CompletedPlan.PredictedRecoveryLocation;
            }

            if (!FollowTarget.IsNearlyZero())
            {
                StartAutoPassFollow(FollowTarget);
            }
            else
            {
                EnterChasingBall();
            }
        }
        else
        {
            EnterManualControl();
        }

        return;
    }

    // A chest control, automatic standing header or defensive block leaves
    // the ball free. Continue by chasing the second ball.
    EnterChasingBall();
}

void AThirdPersonCppCharacter::EnterManualControl()
{
	if (IsAerialActionApproaching() || IsAerialActionWaitingToStart())
	{
		CancelAerialAction();
	}

	if (bHumanJumpHeaderRequestActive)
	{
		ClearHumanJumpHeaderRequest(true, true);
	}

	ClearAutoPassFollowState();
	ClearBallPursuitTarget();
	ClearHumanBallClaim();

	SoccerControlState = ESoccerPlayerControlState::Manual;
	GetCharacterMovement()->MaxWalkSpeed = SelectedMovementSpeed;
}

void AThirdPersonCppCharacter::EnterChasingBall()
{
	if (SoccerControlState != ESoccerPlayerControlState::ChasingBall)
	{
		ClearBallPursuitTarget();
	}

	SoccerControlState = ESoccerPlayerControlState::ChasingBall;
	GetCharacterMovement()->MaxWalkSpeed = SelectedMovementSpeed;
}

void AThirdPersonCppCharacter::ActivateHumanBallClaim()
{
	bHumanBallClaimActive = true;
}

void AThirdPersonCppCharacter::ClearHumanBallClaim()
{
	bHumanBallClaimActive = false;
}

void AThirdPersonCppCharacter::StartBallControl()
{
	if (IsTackleActive() || IsTackleFallReactionActive())
	{
		return;
	}

	if (!IsHumanBallActionAllowedNow())
	{
		ClearBallActionsForMatchRestriction();
		return;
	}

	if (IsAerialActionLocked())
	{
		return;
	}

	if (ControlledBall == nullptr)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.5f,
				FColor::Red,
				TEXT("No se encontro SoccerBall en el nivel")
			);
		}

		return;
	}

	ClearHumanJumpHeaderRequest(true, true);
	PendingKickMode = ESoccerPendingKickMode::None;
	EnterChasingBall();

	/*
	 * Auto-control means receiving the ball, not attacking it. Try the
	 * standing chest/head control first; Automatic remains a physical
	 * fallback for a ball that can only be intercepted with a jump block.
	 */
	if (!TryStartHumanAerialApproach(
		ESoccerAerialActionIntent::Control
	))
	{
		TryStartHumanAerialApproach(
			ESoccerAerialActionIntent::Automatic
		);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Yellow,
			TEXT("Auto-control: yendo hacia la pelota")
		);
	}
}

void AThirdPersonCppCharacter::MoveTowardBall()
{
	if (ControlledBall == nullptr)
	{
		EnterManualControl();
		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	// No continua la persecucion automatica hacia una pelota asegurada por un
	// arquero. La proteccion tambien se aplica al humano del mismo equipo, para
	// que no pueda sacar la pelota de las manos de su propio arquero.
	if (
		IsValid(MatchManager) &&
		!MatchManager->CanCharacterTouchBallNow(this)
		)
	{
		EnterManualControl();
		return;
	}

	FVector ToBall = ControlledBall->GetActorLocation() - GetActorLocation();
	ToBall.Z = 0.0f;

	const float DistanceToBall = ToBall.Size();

	FVector PursuitLocation = ControlledBall->GetActorLocation();
	ResolveStableBallPursuitTarget(
		ControlledBall,
		PursuitLocation
	);

	PursuitLocation.Z = GetActorLocation().Z;

	FVector ToPursuitTarget = PursuitLocation - GetActorLocation();
	ToPursuitTarget.Z = 0.0f;

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	const bool bCanPossessBall =
		CurrentTime - LastKickTime >= RepossessDelayAfterKick;

	const bool bHasPendingKick =
		PendingKickMode != ESoccerPendingKickMode::None;

	if (bIsAutoPassFollowActive && !bHasPendingKick)
	{
		UpdateAutoPassFollowTargetState();

		if (bAutoPassCanCollect && DistanceToBall <= AutoPassCollectDistance)
		{
			CollectAutoPassBallWithoutCollision();
			return;
		}

		RedirectAutoPassBallTowardTargetIfNeeded(DistanceToBall);

		const FVector AutoPassMoveDirection =
			ToPursuitTarget.GetSafeNormal();

		if (!AutoPassMoveDirection.IsNearlyZero())
		{
			AddMovementInput(AutoPassMoveDirection, 1.0f);

			FRotator TargetRotation = AutoPassMoveDirection.Rotation();
			TargetRotation.Pitch = 0.0f;
			TargetRotation.Roll = 0.0f;

			SetActorRotation(TargetRotation);
		}

		return;
	}

	const float RequiredDistanceToBall =
		bHasPendingKick
		? KickExecutionDistance
		: BallPossessionDistance;

	if (DistanceToBall <= RequiredDistanceToBall && bCanPossessBall)
	{
		if (!bHasPendingKick)
		{
			PossessBall();
		}
		else
		{
			ExecutePendingKick();
		}

		return;
	}

	const FVector DirectionToPursuitTarget =
		ToPursuitTarget.GetSafeNormal();

	AddMovementInput(DirectionToPursuitTarget, 1.0f);

	if (!DirectionToPursuitTarget.IsNearlyZero())
	{
		FRotator TargetRotation = DirectionToPursuitTarget.Rotation();
		TargetRotation.Pitch = 0.0f;
		TargetRotation.Roll = 0.0f;

		SetActorRotation(TargetRotation);
	}
}

void AThirdPersonCppCharacter::MoveTowardAerialPreparation()
{
	FVector PreparationLocation;
	float AcceptanceRadius = 45.0f;

	if (!GetAerialPreparationTarget(
		PreparationLocation,
		AcceptanceRadius
	))
	{
		CancelAerialAction();
		return;
	}

	PreparationLocation.Z = GetActorLocation().Z;

	FVector ToPreparation =
		PreparationLocation - GetActorLocation();
	ToPreparation.Z = 0.0f;

	const float DistanceToPreparation = ToPreparation.Size();

	if (DistanceToPreparation <= AcceptanceRadius)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}

		return;
	}

	const FVector MoveDirection = ToPreparation.GetSafeNormal();

	if (MoveDirection.IsNearlyZero())
	{
		return;
	}

	AddMovementInput(MoveDirection, 1.0f);

	FRotator TargetRotation = MoveDirection.Rotation();
	TargetRotation.Pitch = 0.0f;
	TargetRotation.Roll = 0.0f;
	SetActorRotation(TargetRotation);
}

bool AThirdPersonCppCharacter::TryStartHumanAerialApproach(
	ESoccerAerialActionIntent Intent
)
{
	if (!IsValid(ControlledBall) || IsAerialActionLocked())
	{
		return false;
	}

	if (IsAerialActionApproaching() || IsAerialActionWaitingToStart())
	{
		if (Intent == ESoccerAerialActionIntent::Automatic)
		{
			return true;
		}

		return TryChangeQueuedAerialActionIntent(Intent);
	}

	return TryStartBestAerialActionApproach(
		ControlledBall,
		Intent
	);
}

bool AThirdPersonCppCharacter::ShouldArmHumanJumpHeaderFromCurrentBall() const
{
	if (
		!IsValid(ControlledBall) ||
		bIsHumanStealAttemptActive ||
		SoccerControlState == ESoccerPlayerControlState::PossessingBall ||
		SoccerControlState == ESoccerPlayerControlState::Kicking ||
		SoccerControlState == ESoccerPlayerControlState::DribbleTurning
	)
	{
		return false;
	}

	/*
	 * An active aerial request is the strongest signal: the player explicitly
	 * asked a teammate to send a ball for a header. Keep this independent from
	 * the exact first physics frame after the kick.
	 */
	if (
		IsValid(MatchManager) &&
		MatchManager->GetActiveHumanPassRequestType() ==
			ESoccerHumanPassRequestType::AerialHeader
	)
	{
		return true;
	}

	float CharacterGroundZ = GetActorLocation().Z - 96.0f;

	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		CharacterGroundZ =
			GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	}

	const float BallHeightAboveGround =
		ControlledBall->GetActorLocation().Z - CharacterGroundZ;
	const float BallVerticalSpeed =
		FMath::Abs(ControlledBall->GetVelocity().Z);

	return
		BallHeightAboveGround >=
			FMath::Max(0.0f, HumanJumpHeaderArmMinimumBallHeight) ||
		BallVerticalSpeed >=
			FMath::Max(0.0f, HumanJumpHeaderArmMinimumVerticalSpeed);
}

bool AThirdPersonCppCharacter::ArmHumanJumpHeaderRequestFromCurrentInput()
{
	if (!ShouldArmHumanJumpHeaderFromCurrentBall())
	{
		return false;
	}

	FVector FieldLocation;

	if (!GetMouseFieldLocation(FieldLocation))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	ActivateHumanBallClaim();

	const float InitialChargePercent = GetCurrentKickChargePercent();
	const float InitialHorizontalSpeed = FMath::Lerp(
		KickChargeMinHorizontalSpeed,
		KickChargeMaxHorizontalSpeed,
		InitialChargePercent
	);

	PendingKickMode = ESoccerPendingKickMode::KickAndRelease;
	PendingKickTarget = FieldLocation;
	bHasPendingKickHorizontalSpeedOverride = true;
	PendingKickHorizontalSpeedOverride = InitialHorizontalSpeed;

	bHumanJumpHeaderRequestActive = true;
	bHumanJumpHeaderMontageStarted = false;
	HumanJumpHeaderRequestStartWorldTime = World->GetTimeSeconds();
	HumanJumpHeaderPlanSearchAccumulator =
		FMath::Max(0.01f, HumanJumpHeaderPlanSearchInterval);

	// Press only arms the charged action and gives the jump-header planner a
	// provisional internal target. The visible release target belongs strictly
	// to button release (FinishChargedKickRelease), matching normal charged kicks.
	HideAutoPassTargetMarker();
	HideReleaseTargetMarker();
	ArmChaseCancelIgnoreForCurrentMovementInput();

	if (SoccerControlState == ESoccerPlayerControlState::Manual)
	{
		EnterChasingBall();
	}

	/* Try immediately, then Tick keeps retrying if the trajectory is not ready. */
	TryStartOrUpdateHumanJumpHeaderRequest(
		PendingKickTarget,
		PendingKickHorizontalSpeedOverride
	);

	return true;
}

bool AThirdPersonCppCharacter::TryStartOrUpdateHumanJumpHeaderRequest(
	const FVector& TargetLocation,
	float ChargedHorizontalSpeed
)
{
	if (
		!IsValid(ControlledBall) ||
		TargetLocation.IsNearlyZero() ||
		ChargedHorizontalSpeed <= 0.0f ||
		bIsHumanStealAttemptActive ||
		SoccerControlState == ESoccerPlayerControlState::PossessingBall ||
		SoccerControlState == ESoccerPlayerControlState::Kicking ||
		SoccerControlState == ESoccerPlayerControlState::DribbleTurning
	)
	{
		return false;
	}

	if (
		!bHumanJumpHeaderRequestActive &&
		!ShouldArmHumanJumpHeaderFromCurrentBall()
	)
	{
		return false;
	}

	PendingKickMode = ESoccerPendingKickMode::KickAndRelease;
	PendingKickTarget = TargetLocation;
	bHasPendingKickHorizontalSpeedOverride = true;
	PendingKickHorizontalSpeedOverride = ChargedHorizontalSpeed;

	if (!bHumanJumpHeaderRequestActive)
	{
		UWorld* World = GetWorld();
		bHumanJumpHeaderRequestActive = true;
		bHumanJumpHeaderMontageStarted = false;
		HumanJumpHeaderRequestStartWorldTime =
			World != nullptr ? World->GetTimeSeconds() : 0.0f;
	}

	/* Once Playing, only target/power updates are allowed. */
	if (IsAerialActionLocked())
	{
		return
			GetActiveAerialActionType() ==
				ESoccerAerialActionType::JumpHeaderKick;
	}

	bool bQueued = false;

	if (IsAerialActionApproaching() || IsAerialActionWaitingToStart())
	{
		bQueued = TryChangeQueuedAerialActionIntent(
			ESoccerAerialActionIntent::ActiveHeader
		);
	}
	else
	{
		bQueued = TryStartHumanAerialApproach(
			ESoccerAerialActionIntent::ActiveHeader
		);
	}

	if (
		!bQueued ||
		GetActiveAerialActionType() !=
			ESoccerAerialActionType::JumpHeaderKick
	)
	{
		return false;
	}

	HideAutoPassTargetMarker();
	ArmChaseCancelIgnoreForCurrentMovementInput();

	if (SoccerControlState == ESoccerPlayerControlState::Manual)
	{
		EnterChasingBall();
	}

	return true;
}

void AThirdPersonCppCharacter::UpdateHumanJumpHeaderRequestLifecycle(
	float DeltaTime
)
{
	if (!bHumanJumpHeaderRequestActive)
	{
		return;
	}

	const ESoccerAerialActionPhase Phase = GetAerialActionPhase();

	if (Phase == ESoccerAerialActionPhase::Playing)
	{
		bHumanJumpHeaderMontageStarted = true;
		return;
	}

	if (
		Phase == ESoccerAerialActionPhase::Approaching ||
		Phase == ESoccerAerialActionPhase::WaitingToStart
	)
	{
		/* An explicit request must remain a jumping active header. */
		if (
			GetActiveAerialActionType() !=
				ESoccerAerialActionType::JumpHeaderKick
		)
		{
			CancelAerialAction();
		}
		else
		{
			return;
		}
	}

	if (GetAerialActionPhase() != ESoccerAerialActionPhase::None)
	{
		return;
	}

	/* The montage existed and ended without OnAerialBallContactResolved. */
	if (bHumanJumpHeaderMontageStarted)
	{
		ClearHumanJumpHeaderRequest(true, true);

		if (
			bReturnToManualControlWhenHumanJumpHeaderPlanIsLost &&
			SoccerControlState == ESoccerPlayerControlState::ChasingBall
		)
		{
			EnterManualControl();
		}

		return;
	}

	UWorld* World = GetWorld();

	if (
		World == nullptr ||
		!IsValid(ControlledBall) ||
		SoccerControlState == ESoccerPlayerControlState::PossessingBall
	)
	{
		ClearHumanJumpHeaderRequest(true, true);
		return;
	}

	const float Elapsed =
		HumanJumpHeaderRequestStartWorldTime >= 0.0f
		? World->GetTimeSeconds() - HumanJumpHeaderRequestStartWorldTime
		: 0.0f;

	if (Elapsed > FMath::Max(0.1f, HumanJumpHeaderPlanSearchTimeout))
	{
		ClearHumanJumpHeaderRequest(true, true);

		if (
			bReturnToManualControlWhenHumanJumpHeaderPlanIsLost &&
			SoccerControlState == ESoccerPlayerControlState::ChasingBall
		)
		{
			EnterManualControl();
		}

		return;
	}

	HumanJumpHeaderPlanSearchAccumulator += DeltaTime;

	if (
		HumanJumpHeaderPlanSearchAccumulator <
			FMath::Max(0.01f, HumanJumpHeaderPlanSearchInterval)
	)
	{
		return;
	}

	HumanJumpHeaderPlanSearchAccumulator = 0.0f;

	float RequestedSpeed = PendingKickHorizontalSpeedOverride;

	if (bIsChargingKickRelease)
	{
		RequestedSpeed = FMath::Lerp(
			KickChargeMinHorizontalSpeed,
			KickChargeMaxHorizontalSpeed,
			GetCurrentKickChargePercent()
		);
		PendingKickHorizontalSpeedOverride = RequestedSpeed;
	}

	TryStartOrUpdateHumanJumpHeaderRequest(
		PendingKickTarget,
		RequestedSpeed
	);
}

void AThirdPersonCppCharacter::ClearHumanJumpHeaderRequest(
	bool bClearPendingKick,
	bool bHideTargetMarker
)
{
	bHumanJumpHeaderRequestActive = false;
	bHumanJumpHeaderMontageStarted = false;
	HumanJumpHeaderRequestStartWorldTime = -1.0f;
	HumanJumpHeaderPlanSearchAccumulator = 0.0f;

	if (bClearPendingKick)
	{
		PendingKickMode = ESoccerPendingKickMode::None;
		PendingKickTarget = FVector::ZeroVector;
		bHasPendingKickHorizontalSpeedOverride = false;
		PendingKickHorizontalSpeedOverride = 0.0f;
		bIsChargingKickRelease = false;
	}

	if (bHideTargetMarker)
	{
		HideAutoPassTargetMarker();
		HideReleaseTargetMarker();
	}
}

void AThirdPersonCppCharacter::CancelBallChaseByManualInput()
{
	if (SoccerControlState != ESoccerPlayerControlState::ChasingBall)
	{
		return;
	}

	CancelAerialAction();
	ClearHumanJumpHeaderRequest(true, true);
	EnterManualControl();

	PendingKickMode = ESoccerPendingKickMode::None;
	PendingKickTarget = FVector::ZeroVector;

	HideAutoPassTargetMarker();
	HideReleaseTargetMarker();

	bIgnoreChaseCancelUntilMovementInputChanges = false;
	IgnoredMovementInputForChaseCancel = FVector2D::ZeroVector;

	bIsChargingKickRelease = false;
	bHasPendingKickHorizontalSpeedOverride = false;
	PendingKickHorizontalSpeedOverride = 0.0f;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.2f,
			FColor::Orange,
			TEXT("Auto-control cancelado: control manual")
		);
	}
}

FVector2D AThirdPersonCppCharacter::GetCurrentMovementInputVector() const
{
	float ForwardValue = LastMoveForwardInputValue;
	float RightValue = LastMoveRightInputValue;

	if (FMath::Abs(ForwardValue) <= MovementInputCancelDeadZone)
	{
		ForwardValue = 0.0f;
	}

	if (FMath::Abs(RightValue) <= MovementInputCancelDeadZone)
	{
		RightValue = 0.0f;
	}

	return FVector2D(ForwardValue, RightValue);
}

bool AThirdPersonCppCharacter::ShouldMovementInputCancelChase()
{
	if (!bIgnoreChaseCancelUntilMovementInputChanges)
	{
		return true;
	}

	const FVector2D CurrentMovementInput = GetCurrentMovementInputVector();

	// Si el jugador solt� las teclas/stick que estaban presionadas
	// cuando solt� el click derecho, dejamos de ignorar futuros inputs.
	if (CurrentMovementInput.Size() <= MovementInputCancelDeadZone)
	{
		bIgnoreChaseCancelUntilMovementInputChanges = false;
		IgnoredMovementInputForChaseCancel = FVector2D::ZeroVector;
		return false;
	}

	const float InputChangeAmount =
		(CurrentMovementInput - IgnoredMovementInputForChaseCancel).Size();

	// Si cambi� el input mientras iba hacia la pelota,
	// eso s� cuenta como intenci�n nueva de cancelar.
	if (InputChangeAmount >= MovementInputChangeCancelThreshold)
	{
		bIgnoreChaseCancelUntilMovementInputChanges = false;
		IgnoredMovementInputForChaseCancel = FVector2D::ZeroVector;
		return true;
	}

	// Sigue siendo el mismo input que ya estaba presionado antes
	// de soltar el click derecho. No cancelamos.
	return false;
}

void AThirdPersonCppCharacter::ArmChaseCancelIgnoreForCurrentMovementInput()
{
	const FVector2D CurrentMovementInput = GetCurrentMovementInputVector();

	if (CurrentMovementInput.Size() > MovementInputCancelDeadZone)
	{
		bIgnoreChaseCancelUntilMovementInputChanges = true;
		IgnoredMovementInputForChaseCancel = CurrentMovementInput;
	}
	else
	{
		bIgnoreChaseCancelUntilMovementInputChanges = false;
		IgnoredMovementInputForChaseCancel = FVector2D::ZeroVector;
	}
}

void AThirdPersonCppCharacter::UpdateChaseCancelIgnoreAfterInputChanged()
{
	if (!bIgnoreChaseCancelUntilMovementInputChanges)
	{
		return;
	}

	if (SoccerControlState != ESoccerPlayerControlState::ChasingBall)
	{
		return;
	}

	const FVector2D CurrentMovementInput = GetCurrentMovementInputVector();

	if (CurrentMovementInput.Size() <= MovementInputCancelDeadZone)
	{
		bIgnoreChaseCancelUntilMovementInputChanges = false;
		IgnoredMovementInputForChaseCancel = FVector2D::ZeroVector;
	}
}

void AThirdPersonCppCharacter::PossessBall()
{
	if (IsAerialActionApproaching() || IsAerialActionWaitingToStart())
	{
		CancelAerialAction();
	}

	if (ControlledBall == nullptr)
	{
		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (
		IsValid(MatchManager) &&
		!MatchManager->CanCharacterTouchBallNow(this)
		)
	{
		return;
	}

	if (!IsBallAtPlayablePossessionHeight(ControlledBall))
	{
		return;
	}

	ClearAutoPassFollowState();
	ClearBallPursuitTarget();
	ClearHumanBallClaim();

	SoccerControlState = ESoccerPlayerControlState::PossessingBall;
	PendingKickMode = ESoccerPendingKickMode::None;

	GetCharacterMovement()->MaxWalkSpeed = SelectedMovementSpeed;

	CurrentDribbleDirection = GetActorForwardVector();
	CurrentDribbleDirection.Z = 0.0f;
	CurrentDribbleDirection = CurrentDribbleDirection.GetSafeNormal();

	DesiredDribbleDirection = CurrentDribbleDirection;
	bHasDesiredDribbleDirection = true;

	PendingDribbleInputDirection = FVector::ZeroVector;
	LastDribbleTouchTime = -1000.0f;
	bCanDribbleTouch = true;

	if (ControlledBall != nullptr)
	{
		LastDribbleTouchBallLocation = ControlledBall->GetActorLocation();
	}
	else
	{
		LastDribbleTouchBallLocation = FVector::ZeroVector;
	}

	ControlledBall->StopBallKeepingPhysics();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Green,
			TEXT("Jugador posee la pelota")
		);
	}
}

void AThirdPersonCppCharacter::ReleaseBallForAISteal()
{
	ClearHumanStealAttemptOnly();
	ClearAutoPassFollowState();
	ClearHumanJumpHeaderRequest(true, true);
	ClearHumanBallClaim();

	PendingKickMode = ESoccerPendingKickMode::None;
	PendingKickTarget = FVector::ZeroVector;

	bIsChargingKickRelease = false;

	bHasPendingKickHorizontalSpeedOverride = false;
	PendingKickHorizontalSpeedOverride = 0.0f;

	HideAutoPassTargetMarker();
	HideReleaseTargetMarker();

	PendingDribbleInputDirection = FVector::ZeroVector;
	bHasDesiredDribbleDirection = false;
	bCanDribbleTouch = false;

	if (ControlledBall != nullptr)
	{
		ControlledBall->SetPossessed(false);
	}

	SoccerControlState = ESoccerPlayerControlState::Manual;

	UpdateEnergyAdjustedMovementSpeed();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.0f,
			FColor::Orange,
			TEXT("El rival robo la pelota")
		);
	}
}

void AThirdPersonCppCharacter::ReleaseBallForMatchRestart(bool bShowFeedback)
{
	ClearBallActionsForMatchRestriction();

	if (ControlledBall != nullptr)
	{
		ControlledBall->SetPossessed(false);
	}

	if (bShowFeedback && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.0f,
			FColor::Orange,
			TEXT("Posesion perdida por reinicio de juego")
		);
	}
}

void AThirdPersonCppCharacter::ClearBallActionsForMatchRestriction()
{
	const bool bWasKickOrDribbleTurn =
		SoccerControlState == ESoccerPlayerControlState::Kicking ||
		SoccerControlState == ESoccerPlayerControlState::DribbleTurning;

	ClearHumanStealAttemptOnly();
	ClearAutoPassFollowState();

	if (IsAerialActionApproaching() || IsAerialActionWaitingToStart())
	{
		CancelAerialAction();
	}

	ClearHumanJumpHeaderRequest(true, true);
	ClearHumanBallClaim();
	ClearBallPursuitTarget();

	PendingKickMode = ESoccerPendingKickMode::None;
	PendingKickTarget = FVector::ZeroVector;
	bIsChargingKickRelease = false;
	bHasPendingKickHorizontalSpeedOverride = false;
	PendingKickHorizontalSpeedOverride = 0.0f;

	HideAutoPassTargetMarker();
	HideReleaseTargetMarker();

	// A kick montage that was armed just before the restriction must not leave a
	// delayed impact timer capable of firing after the restart has completed.
	GetWorldTimerManager().ClearTimer(KickImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(KickFinishTimerHandle);
	ActiveKickTarget = FVector::ZeroVector;
	ActiveKickHorizontalSpeed = 0.0f;
	ActiveKickMode = ESoccerPendingKickMode::None;
	bActiveKickHasImpactedBall = false;
	bActiveKickUsesChargedTrajectory = false;
	bActiveKickWasHumanRestartExecution = false;

	// The two dribble-turn systems also own delayed ball-contact timers. A
	// restart can begin between montage start and impact, so erase those timers
	// as part of the same restriction cleanup.
	GetWorldTimerManager().ClearTimer(StrongRunDribbleTurnImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(StrongRunDribbleTurnFinishTimerHandle);
	ActiveStrongRunDribbleTurnDirection = FVector::ZeroVector;
	ActiveStrongRunDribbleTurnTouchSpeed = 0.0f;
	ActiveStrongRunDribbleTurnUpwardSpeed = 0.0f;
	bActiveStrongRunDribbleTurnHasImpactedBall = false;
	bActiveStrongRunDribbleTurnShouldKickToTarget = false;
	ActiveStrongRunDribbleTurnKickTarget = FVector::ZeroVector;
	ActiveStrongRunDribbleTurnKickMode = ESoccerPendingKickMode::None;
	ActiveStrongRunDribbleTurnKickHorizontalSpeed = 0.0f;
	bActiveStrongRunDribbleTurnUsesChargedTrajectory = false;
	ActiveStrongRunDribbleTurnResumeSpeed = 0.0f;
	bIsStrongRunDribbleTurnActorRotating = false;
	ActiveStrongRunDribbleTurnDuration = 0.0f;
	ActiveStrongRunDribbleTurnElapsedTime = 0.0f;
	ActiveStrongRunDribbleTurnInitialSpeed = 0.0f;
	ActiveStrongRunDribbleTurnInitialDirection = FVector::ZeroVector;

	GetWorldTimerManager().ClearTimer(NormalRunDribbleTurnImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(NormalRunDribbleTurnFinishTimerHandle);
	ActiveNormalRunDribbleTurnDirection = FVector::ZeroVector;
	ActiveNormalRunDribbleTurnTouchSpeed = 0.0f;
	ActiveNormalRunDribbleTurnUpwardSpeed = 0.0f;
	bActiveNormalRunDribbleTurnHasImpactedBall = false;
	ActiveNormalRunDribbleTurnResumeSpeed = 0.0f;
	ActiveNormalRunDribbleTurnDuration = 0.0f;
	ActiveNormalRunDribbleTurnElapsedTime = 0.0f;
	ActiveNormalRunDribbleTurnInitialSpeed = 0.0f;
	ActiveNormalRunDribbleTurnInitialDirection = FVector::ZeroVector;

	PendingDribbleInputDirection = FVector::ZeroVector;
	bHasDesiredDribbleDirection = false;
	bCanDribbleTouch = false;

	bIgnoreChaseCancelUntilMovementInputChanges = false;
	IgnoredMovementInputForChaseCancel = FVector2D::ZeroVector;

	if (
		SoccerControlState == ESoccerPlayerControlState::ChasingBall ||
		SoccerControlState == ESoccerPlayerControlState::PossessingBall ||
		SoccerControlState == ESoccerPlayerControlState::Kicking ||
		SoccerControlState == ESoccerPlayerControlState::DribbleTurning
		)
	{
		SoccerControlState = ESoccerPlayerControlState::Manual;
	}

	if (bWasKickOrDribbleTurn && !IsTackleFallReactionActive())
	{
		/*
		 * Restart cleanup may run synchronously in the same frame in which a
		 * tackle victim starts its fall reaction. StopAnimMontage() without a
		 * montage argument stops every montage on the human, including the fall
		 * montage that ASoccerCharacterBase has just started. Bots do not pass
		 * through this human-only cleanup, which made the symptom look as if the
		 * human never received the fall reaction.
		 *
		 * The kick/dribble timers and gameplay state have already been cleared
		 * above, so while a tackle fall owns the character we deliberately leave
		 * montage playback alone and let UpdateTackleFallReaction() finish it.
		 */
		StopAnimMontage();
	}

	UpdateEnergyAdjustedMovementSpeed();
}

bool AThirdPersonCppCharacter::HoldThrowInBall(ASoccerBall* SoccerBall)
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
					TEXT("Lateral humano: no existe socket/hueso %s"),
					*ThrowInBallHoldSocketName.ToString()
				)
			);
		}
		return false;
	}

	// Any ordinary ball action must die before the restart ball is attached to
	// the hands. The throw-in runtime below is deliberately separate from the
	// normal PossessingBall/dribble state.
	ClearBallActionsForMatchRestriction();

	ControlledBall = SoccerBall;
	ControlledBall->StopBallKeepingPhysics();
	ControlledBall->SetPossessed(true);
	ControlledBall->SetActorEnableCollision(false);
	ControlledBall->AttachToComponent(
		GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		ThrowInBallHoldSocketName
	);

	bHumanThrowInHoldingBall =
		ControlledBall->GetAttachParentActor() == this;

	if (!bHumanThrowInHoldingBall)
	{
		ControlledBall->SetActorEnableCollision(true);
		ControlledBall->SetPossessed(false);
		return false;
	}

	SoccerControlState = ESoccerPlayerControlState::Manual;
	return true;
}

bool AThirdPersonCppCharacter::IsHoldingThrowInBall() const
{
	return
		bHumanThrowInHoldingBall &&
		IsValid(ControlledBall) &&
		ControlledBall->GetAttachParentActor() == this;
}

void AThirdPersonCppCharacter::CancelHeldThrowInBall()
{
	ClearThrowInScriptedMovementVelocity();

	if (ThrowInMontage != nullptr)
	{
		StopAnimMontage(ThrowInMontage);
	}

	if (IsValid(ControlledBall))
	{
		if (ControlledBall->GetAttachParentActor() == this)
		{
			ControlledBall->DetachFromActor(
				FDetachmentTransformRules::KeepWorldTransform
			);
		}

		ControlledBall->SetActorEnableCollision(true);
		ControlledBall->SetPossessed(false);
		ControlledBall->StopBallKeepingPhysics();
	}

	bHumanThrowInHoldingBall = false;
	HideAutoPassTargetMarker();
	HideReleaseTargetMarker();
}

float AThirdPersonCppCharacter::PlayThrowInMontage()
{
	if (ThrowInMontage == nullptr || GetMesh() == nullptr)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.5f,
				FColor::Red,
				TEXT("Lateral humano: Throw In Montage no asignado")
			);
		}
		return 0.0f;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return 0.0f;
	}

	ClearThrowInScriptedMovementVelocity();
	return AnimInstance->Montage_Play(ThrowInMontage, 1.0f);
}

bool AThirdPersonCppCharacter::GetThrowInMontagePlaybackState(
	float& OutMontagePosition,
	float& OutMontageLength
) const
{
	OutMontagePosition = 0.0f;
	OutMontageLength = 0.0f;

	if (ThrowInMontage == nullptr || GetMesh() == nullptr)
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

	OutMontagePosition = AnimInstance->Montage_GetPosition(ThrowInMontage);
	OutMontageLength = ThrowInMontage->GetPlayLength();
	return OutMontageLength > KINDA_SMALL_NUMBER;
}

bool AThirdPersonCppCharacter::MoveThrowInByWorldDelta(
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

bool AThirdPersonCppCharacter::ReleaseHeldThrowInBallToAirTarget(
	const FVector& TargetLocation,
	float HorizontalSpeed,
	float MinTravelTime,
	float MaxTravelTime
)
{
	if (!IsHoldingThrowInBall())
	{
		return false;
	}

	ASoccerBall* BallToRelease = ControlledBall;
	BallToRelease->DetachFromActor(
		FDetachmentTransformRules::KeepWorldTransform
	);
	BallToRelease->SetActorEnableCollision(true);
	BallToRelease->SetPossessed(false);
	BallToRelease->StopBallKeepingPhysics();
	bHumanThrowInHoldingBall = false;

	BallToRelease->KickToAirTarget(
		TargetLocation,
		HorizontalSpeed,
		MinTravelTime,
		MaxTravelTime
	);

	HideAutoPassTargetMarker();
	HideReleaseTargetMarker();
	return true;
}

void AThirdPersonCppCharacter::SetThrowInScriptedMovementVelocity(
	const FVector& WorldVelocity
)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement == nullptr)
	{
		return;
	}

	FVector HorizontalVelocity = WorldVelocity;
	HorizontalVelocity.Z = Movement->Velocity.Z;

	if (!bHumanThrowInScriptedMovementActive)
	{
		HumanThrowInSavedMaxWalkSpeed = Movement->MaxWalkSpeed;
		bHumanThrowInScriptedMovementActive = true;
	}

	Movement->MaxWalkSpeed = FMath::Max(
		HumanThrowInSavedMaxWalkSpeed,
		HorizontalVelocity.Size2D()
	);
	Movement->Velocity = HorizontalVelocity;
}

void AThirdPersonCppCharacter::ClearThrowInScriptedMovementVelocity()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement != nullptr)
	{
		Movement->StopMovementImmediately();
		if (bHumanThrowInScriptedMovementActive)
		{
			Movement->MaxWalkSpeed = HumanThrowInSavedMaxWalkSpeed;
		}
	}

	bHumanThrowInScriptedMovementActive = false;
	HumanThrowInSavedMaxWalkSpeed = 0.0f;
}

bool AThirdPersonCppCharacter::IsHumanBallActionAllowedNow()
{
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	// Preserve the old standalone/debug behavior when no manager exists.
	return
		!IsValid(MatchManager) ||
		MatchManager->CanHumanStartBallActionNow(this);
}

void AThirdPersonCppCharacter::UpdatePossessedBallLocation()
{
	if (ControlledBall == nullptr)
	{
		return;
	}

	const FVector Forward = GetActorForwardVector();
	const FVector Right = GetActorRightVector();

	const float GroundZ =
		GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	const float Speed2D = GetVelocity().Size2D();
	const bool bIsDribbling = Speed2D >= DribbleMinSpeed;

	float ExtraForwardOffset = 0.0f;
	float SideOffset = 0.0f;

	if (bIsDribbling)
	{
		const float Time = GetWorld()->GetTimeSeconds();

		const float Phase =
			Time * DribbleFrequency * 2.0f * PI;

		const float ForwardPulse =
			FMath::Max(0.0f, FMath::Sin(Phase));

		ExtraForwardOffset =
			ForwardPulse * DribbleForwardExtraOffset;

		SideOffset =
			FMath::Sin(Phase * 0.5f) * DribbleSideOffset;
	}

	FVector BallLocation =
		GetActorLocation()
		+ Forward * (PossessedBallForwardOffset + ExtraForwardOffset)
		+ Right * SideOffset;

	BallLocation.Z = GroundZ + PossessedBallHeight;

	ControlledBall->SetActorLocation(BallLocation);
}

void AThirdPersonCppCharacter::ClearAutoPassFollowState()
{
	bIsAutoPassFollowActive = false;
	bAutoPassCanCollect = false;
	AutoPassTargetLocation = FVector::ZeroVector;
	LastAutoPassBallLocation = FVector::ZeroVector;
	LastAutoPassRedirectTime = -1000.0f;
}

void AThirdPersonCppCharacter::StartAutoPassFollow(const FVector& TargetLocation)
{
	bIsAutoPassFollowActive = true;
	bAutoPassCanCollect = false;

	AutoPassTargetLocation = TargetLocation;
	AutoPassTargetLocation.Z = 0.0f;

	if (ControlledBall != nullptr)
	{
		LastAutoPassBallLocation = ControlledBall->GetActorLocation();
		LastAutoPassBallLocation.Z = 0.0f;
	}
	else
	{
		LastAutoPassBallLocation = FVector::ZeroVector;
	}

	EnterChasingBall();
}

void AThirdPersonCppCharacter::PrepareAutoPassBallForCleanKick(const FVector& KickTarget)
{
	if (ControlledBall == nullptr)
	{
		return;
	}

	FVector LaunchDirection = KickTarget - GetActorLocation();
	LaunchDirection.Z = 0.0f;

	const float DistanceFromPlayerToTarget = LaunchDirection.Size();

	LaunchDirection = LaunchDirection.GetSafeNormal();

	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = GetActorForwardVector();
		LaunchDirection.Z = 0.0f;
		LaunchDirection = LaunchDirection.GetSafeNormal();
	}

	if (LaunchDirection.IsNearlyZero())
	{
		return;
	}

	float LaunchDistance = AutoPassPreKickBallDistanceFromPlayer;

	if (DistanceFromPlayerToTarget > 10.0f)
	{
		LaunchDistance = FMath::Min(
			LaunchDistance,
			DistanceFromPlayerToTarget * 0.5f
		);
	}

	const float GroundZ =
		GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FVector CleanBallLocation =
		GetActorLocation() + LaunchDirection * LaunchDistance;

	CleanBallLocation.Z = GroundZ + PossessedBallHeight;

	ControlledBall->SetPossessed(true);

	ControlledBall->SetActorLocation(
		CleanBallLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);
}

bool AThirdPersonCppCharacter::HasAutoPassBallReachedTarget(
	const FVector& PreviousBallLocation,
	const FVector& CurrentBallLocation
) const
{
	FVector Previous = PreviousBallLocation;
	Previous.Z = 0.0f;

	FVector Current = CurrentBallLocation;
	Current.Z = 0.0f;

	FVector Target = AutoPassTargetLocation;
	Target.Z = 0.0f;

	const float CurrentDistanceToTarget =
		FVector::Dist2D(Current, Target);

	if (CurrentDistanceToTarget <= AutoPassTargetArrivalRadius)
	{
		return true;
	}

	const FVector Segment = Current - Previous;
	const float SegmentLengthSquared = Segment.SizeSquared2D();

	if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Alpha = FMath::Clamp(
		FVector::DotProduct(Target - Previous, Segment) / SegmentLengthSquared,
		0.0f,
		1.0f
	);

	const FVector ClosestPoint = Previous + Segment * Alpha;

	const float SegmentDistanceToTarget =
		FVector::Dist2D(ClosestPoint, Target);

	return SegmentDistanceToTarget <= AutoPassTargetArrivalRadius;
}

void AThirdPersonCppCharacter::UpdateAutoPassFollowTargetState()
{
	if (!bIsAutoPassFollowActive)
	{
		return;
	}

	if (bAutoPassCanCollect)
	{
		return;
	}

	if (ControlledBall == nullptr)
	{
		ClearAutoPassFollowState();
		return;
	}

	const FVector CurrentBallLocation = ControlledBall->GetActorLocation();

	const bool bReachedTarget =
		HasAutoPassBallReachedTarget(
			LastAutoPassBallLocation,
			CurrentBallLocation
		);

	const float BallSpeed2D =
		ControlledBall->GetVelocity().Size2D();

	const bool bBallBecameLoose =
		BallSpeed2D <= AutoPassLooseBallCollectSpeed;

	if (bReachedTarget || bBallBecameLoose)
	{
		bAutoPassCanCollect = true;

		if (GEngine)
		{
			const FString Message =
				bReachedTarget
				? TEXT("Auto-pase: pelota llego a la zona objetivo")
				: TEXT("Auto-pase: pelota desviada/lenta, captura habilitada");

			GEngine->AddOnScreenDebugMessage(
				-1,
				1.0f,
				FColor::Cyan,
				Message
			);
		}
	}

	LastAutoPassBallLocation = CurrentBallLocation;
}

void AThirdPersonCppCharacter::CollectAutoPassBallWithoutCollision()
{
	if (ControlledBall == nullptr)
	{
		EnterManualControl();
		return;
	}

	ClearAutoPassFollowState();

	StartAutoPassCollectCarry();

	PossessBall();

	// Importante:
	// PossessBall() llama a StopBallKeepingPhysics(), que deja f�sica activa.
	// Para este caso queremos que la pelota quede controlada y sin impulso f�sico.
	ControlledBall->SetPossessed(true);

	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();

	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const float GroundZ =
		GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FVector BallLocation =
		GetActorLocation()
		+ Forward * PossessedBallForwardOffset;

	BallLocation.Z = GroundZ + PossessedBallHeight;

	ControlledBall->SetActorLocation(
		BallLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	// En vez de dejarla congelada, le damos un toque muy suave hacia adelante.
	// Esto simula que el jugador la control�/fren�, pero sin matarla de golpe.
	ControlledBall->DribbleTouch(
		Forward,
		AutoPassCollectSoftTouchSpeed,
		0.0f
	);

	CurrentDribbleDirection = Forward;
	DesiredDribbleDirection = Forward;
	bHasDesiredDribbleDirection = true;

	LastDribbleTouchBallLocation = ControlledBall->GetActorLocation();
	LastDribbleTouchTime = GetWorld()->GetTimeSeconds();

	// Evita que el sistema le d� otro toque inmediatamente en el frame siguiente.
	bCanDribbleTouch = false;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.2f,
			FColor::Green,
			TEXT("Auto-pase controlado: pelota cerca de los pies")
		);
	}
}

void AThirdPersonCppCharacter::RedirectAutoPassBallTowardTargetIfNeeded(float DistanceToBall)
{
	if (ControlledBall == nullptr)
	{
		return;
	}

	if (!bIsAutoPassFollowActive || bAutoPassCanCollect)
	{
		return;
	}

	if (DistanceToBall > AutoPassRedirectDistance)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	if (CurrentTime - LastAutoPassRedirectTime < AutoPassRedirectCooldown)
	{
		return;
	}

	FVector DirectionToTarget =
		AutoPassTargetLocation - ControlledBall->GetActorLocation();

	DirectionToTarget.Z = 0.0f;
	DirectionToTarget = DirectionToTarget.GetSafeNormal();

	if (DirectionToTarget.IsNearlyZero())
	{
		return;
	}

	const float PlayerSpeed2D = GetVelocity().Size2D();
	const float BallSpeed2D = ControlledBall->GetVelocity().Size2D();

	const float DesiredRedirectSpeed = FMath::Clamp(
		FMath::Max(
			BallSpeed2D,
			PlayerSpeed2D * AutoPassRedirectPlayerSpeedMultiplier
		),
		AutoPassRedirectMinSpeed,
		AutoPassRedirectMaxSpeed
	);

	ControlledBall->DribbleTouch(
		DirectionToTarget,
		DesiredRedirectSpeed,
		0.0f
	);

	LastAutoPassRedirectTime = CurrentTime;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.5f,
			FColor::Cyan,
			TEXT("Auto-pase: contacto redirigido hacia la marca")
		);
	}
}

void AThirdPersonCppCharacter::StartAutoPassCollectCarry()
{
	if (GetWorld() == nullptr)
	{
		return;
	}

	FVector CurrentVelocity = GetVelocity();
	CurrentVelocity.Z = 0.0f;

	AutoPassCollectCarryDirection = CurrentVelocity.GetSafeNormal();

	if (AutoPassCollectCarryDirection.IsNearlyZero())
	{
		AutoPassCollectCarryDirection = GetActorForwardVector();
		AutoPassCollectCarryDirection.Z = 0.0f;
		AutoPassCollectCarryDirection = AutoPassCollectCarryDirection.GetSafeNormal();
	}

	if (AutoPassCollectCarryDirection.IsNearlyZero())
	{
		return;
	}

	const float CurrentSpeed = GetVelocity().Size2D();

	AutoPassCollectCarryStartSpeed = FMath::Max(
		CurrentSpeed * AutoPassCollectCarryStartSpeedMultiplier,
		AutoPassCollectCarryMinSpeed
	);

	bIsAutoPassCollectCarrying = true;

	AutoPassCollectCarryStartTime = GetWorld()->GetTimeSeconds();

	AutoPassCollectCarryEndTime =
		AutoPassCollectCarryStartTime + AutoPassCollectCarryDuration;
}

void AThirdPersonCppCharacter::UpdateAutoPassCollectCarry()
{
	if (!bIsAutoPassCollectCarrying)
	{
		return;
	}

	if (GetWorld() == nullptr || GetCharacterMovement() == nullptr)
	{
		bIsAutoPassCollectCarrying = false;
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	if (CurrentTime >= AutoPassCollectCarryEndTime)
	{
		bIsAutoPassCollectCarrying = false;
		AutoPassCollectCarryStartTime = 0.0f;
		AutoPassCollectCarryEndTime = 0.0f;
		AutoPassCollectCarryStartSpeed = 0.0f;
		AutoPassCollectCarryDirection = FVector::ZeroVector;
		return;
	}

	const float Alpha = FMath::Clamp(
		(CurrentTime - AutoPassCollectCarryStartTime) / AutoPassCollectCarryDuration,
		0.0f,
		1.0f
	);

	const float SmoothAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);

	const float CurrentCarrySpeed = FMath::Lerp(
		AutoPassCollectCarryStartSpeed,
		0.0f,
		SmoothAlpha
	);

	FVector NewVelocity = AutoPassCollectCarryDirection * CurrentCarrySpeed;
	NewVelocity.Z = GetCharacterMovement()->Velocity.Z;

	GetCharacterMovement()->Velocity = NewVelocity;
}

void AThirdPersonCppCharacter::UpdatePhysicalDribbleControl()
{
	if (ControlledBall == nullptr)
	{
		EnterManualControl();
		return;
	}

	FVector InputDirection = PendingDribbleInputDirection;
	InputDirection.Z = 0.0f;

	const bool bHasInputThisFrame = !InputDirection.IsNearlyZero();

	if (bHasInputThisFrame)
	{
		DesiredDribbleDirection = InputDirection.GetSafeNormal();
		bHasDesiredDribbleDirection = true;
	}

	PendingDribbleInputDirection = FVector::ZeroVector;

	FVector ToBall = ControlledBall->GetActorLocation() - GetActorLocation();
	ToBall.Z = 0.0f;

	const float DistanceToBall = ToBall.Size();

	// Si la pelota se fue demasiado lejos, dejamos de considerarla pose�da.
	if (DistanceToBall > DribbleMaxPossessionDistance)
	{
		EnterManualControl();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Red,
				TEXT("Posesion perdida: pelota demasiado lejos")
			);
		}

		return;
	}

	// Sin input: no perseguir y no tocar.
	// Esto evita el bucle: alcanza pelota -> la toca -> la sigue -> la toca...
	if (!bHasInputThisFrame)
	{
		if (bIsAutoPassCollectCarrying)
		{
			return;
		}

		if (GetVelocity().Size2D() <= DribbleNoInputStopSpeed)
		{
			GetCharacterMovement()->StopMovementImmediately();
		}

		return;
	}

	if (!bHasDesiredDribbleDirection)
	{
		return;
	}

	// Con input: el jugador va hacia la pelota.
	FVector MoveDirection = ToBall.GetSafeNormal();

	if (MoveDirection.IsNearlyZero())
	{
		MoveDirection = CurrentDribbleDirection;
	}

	if (MoveDirection.IsNearlyZero())
	{
		MoveDirection = GetActorForwardVector();
		MoveDirection.Z = 0.0f;
		MoveDirection = MoveDirection.GetSafeNormal();
	}

	AddMovementInput(MoveDirection, 1.0f);

	// Mientras todav�a no lleg� a la pelota, mira hacia la pelota.
	if (!MoveDirection.IsNearlyZero())
	{
		FRotator TargetRotation = MoveDirection.Rotation();
		TargetRotation.Pitch = 0.0f;
		TargetRotation.Roll = 0.0f;

		SetActorRotation(TargetRotation);
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	const float TimeSinceLastTouch =
		CurrentTime - LastDribbleTouchTime;

	const float BallTravelSinceLastTouch =
		FVector::Dist2D(
			ControlledBall->GetActorLocation(),
			LastDribbleTouchBallLocation
		);

	const float BallSpeed2D =
		ControlledBall->GetVelocity().Size2D();

	// Despu�s de un toque, esperamos a que la pelota se aleje.
	// Pero no bloqueamos para siempre: tambi�n liberamos por tiempo
	// o si la pelota qued� casi trabada/frenada.
	if (!bCanDribbleTouch)
	{
		const bool bBallMovedEnough =
			BallTravelSinceLastTouch >= DribbleRearmDistance;

		const bool bWaitedTooLong =
			TimeSinceLastTouch >= DribbleMaxTouchWaitTime;

		const bool bBallIsStuck =
			BallSpeed2D <= DribbleStuckBallSpeed;

		if (bBallMovedEnough || bWaitedTooLong || bBallIsStuck)
		{
			bCanDribbleTouch = true;
		}
	}

	const bool bCooldownReady =
		TimeSinceLastTouch >= DribbleTouchCooldown;

	FVector TouchPointDirection = MoveDirection;
	TouchPointDirection.Z = 0.0f;
	TouchPointDirection = TouchPointDirection.GetSafeNormal();

	if (TouchPointDirection.IsNearlyZero())
	{
		TouchPointDirection = GetActorForwardVector();
		TouchPointDirection.Z = 0.0f;
		TouchPointDirection = TouchPointDirection.GetSafeNormal();
	}

	const FVector DribbleTouchPoint =
		GetActorLocation() + TouchPointDirection * DribbleForwardExtraOffset;

	if (ASoccerDebugManager::IsWorldDrawingEnabled(this, ESoccerDebugCategory::PlayerInput))
	{
		DrawDebugSphere(
			GetWorld(),
			DribbleTouchPoint,
			DribbleTouchDistance,
			12,
			FColor::Green,
			false,
			0.0f,
			0,
			1.5f
		);
	}

	const float DistanceToDribbleTouchPoint =
		FVector::Dist2D(
			DribbleTouchPoint,
			ControlledBall->GetActorLocation()
		);

	if (DistanceToDribbleTouchPoint <= DribbleTouchDistance && bCooldownReady && bCanDribbleTouch)
	{
		FVector TouchDirection = DesiredDribbleDirection;
		TouchDirection.Z = 0.0f;
		TouchDirection = TouchDirection.GetSafeNormal();

		if (TouchDirection.IsNearlyZero())
		{
			TouchDirection = CurrentDribbleDirection;
			TouchDirection.Z = 0.0f;
			TouchDirection = TouchDirection.GetSafeNormal();
		}

		if (TouchDirection.IsNearlyZero())
		{
			TouchDirection = GetActorForwardVector();
			TouchDirection.Z = 0.0f;
			TouchDirection = TouchDirection.GetSafeNormal();
		}

		if (TryStartStrongRunDribbleTurnForDirection(TouchDirection))
		{
			return;
		}

		if (TryStartNormalRunDribbleTurnForDirection(TouchDirection))
		{
			return;
		}


		CurrentDribbleDirection = TouchDirection;

		// Reci�n en el contacto cambia hacia la nueva direcci�n.
		if (!CurrentDribbleDirection.IsNearlyZero())
		{
			FRotator NewDirectionRotation = CurrentDribbleDirection.Rotation();
			NewDirectionRotation.Pitch = 0.0f;
			NewDirectionRotation.Roll = 0.0f;

			SetActorRotation(NewDirectionRotation);
		}

		const float PlayerSpeed2D = GetVelocity().Size2D();

		const float TouchSpeedToUse = FMath::Clamp(
			DribbleTouchSpeed + PlayerSpeed2D * DribbleTouchPlayerSpeedMultiplier,
			DribbleTouchSpeed,
			DribbleTouchMaxSpeed
		);

		ControlledBall->DribbleTouch(
			CurrentDribbleDirection,
			TouchSpeedToUse,
			DribbleTouchUpwardSpeed
		);

		LastDribbleTouchTime = CurrentTime;
		LastDribbleTouchBallLocation = ControlledBall->GetActorLocation();

		bCanDribbleTouch = false;
	}
}

UAnimMontage* AThirdPersonCppCharacter::SelectStrongRunDribbleTurnMontageForDirection(
	const FVector& DesiredDirection,
	float& OutAngleDegrees
) const
{
	OutAngleDegrees = 0.0f;

	FVector CurrentDirection = CurrentDribbleDirection;
	CurrentDirection.Z = 0.0f;
	CurrentDirection = CurrentDirection.GetSafeNormal();

	if (CurrentDirection.IsNearlyZero())
	{
		CurrentDirection = GetActorForwardVector();
		CurrentDirection.Z = 0.0f;
		CurrentDirection = CurrentDirection.GetSafeNormal();
	}

	FVector NewDirection = DesiredDirection;
	NewDirection.Z = 0.0f;
	NewDirection = NewDirection.GetSafeNormal();

	if (CurrentDirection.IsNearlyZero() || NewDirection.IsNearlyZero())
	{
		return nullptr;
	}

	const float Dot = FMath::Clamp(
		FVector::DotProduct(CurrentDirection, NewDirection),
		-1.0f,
		1.0f
	);

	OutAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));

	if (OutAngleDegrees < StrongRunDribbleTurnMinAngleDegrees)
	{
		return nullptr;
	}

	const float CrossZ =
		FVector::CrossProduct(CurrentDirection, NewDirection).Z;

	// Si al probar queda invertido izquierda/derecha,
	// cambiamos este signo.
	const bool bTurnRight = CrossZ > 0.0f;

	const bool bHardTurn =
		OutAngleDegrees >= StrongRunDribbleTurnHardAngleDegrees;

	if (bHardTurn)
	{
		return bTurnRight
			? StrongRunDribbleTurnRightOver90Montage
			: StrongRunDribbleTurnLeftOver90Montage;
	}

	return bTurnRight
		? StrongRunDribbleTurnRight45To90Montage
		: StrongRunDribbleTurnLeft45To90Montage;
}

bool AThirdPersonCppCharacter::TryStartStrongRunDribbleTurnForDirection(
	const FVector& DesiredDirection
)
{
	if (ControlledBall == nullptr)
	{
		return false;
	}

	if (SoccerControlState != ESoccerPlayerControlState::PossessingBall)
	{
		return false;
	}

	const float PlayerSpeed2D = GetVelocity().Size2D();

	if (PlayerSpeed2D < StrongRunDribbleTurnMinPlayerSpeed)
	{
		return false;
	}

	FVector SafeDesiredDirection = DesiredDirection;
	SafeDesiredDirection.Z = 0.0f;
	SafeDesiredDirection = SafeDesiredDirection.GetSafeNormal();

	if (SafeDesiredDirection.IsNearlyZero())
	{
		return false;
	}

	float TurnAngleDegrees = 0.0f;

	UAnimMontage* TurnMontage = SelectStrongRunDribbleTurnMontageForDirection(
		SafeDesiredDirection,
		TurnAngleDegrees
	);

	if (TurnMontage == nullptr)
	{
		return false;
	}

	const float TouchSpeedToUse = FMath::Clamp(
		DribbleTouchSpeed + PlayerSpeed2D * DribbleTouchPlayerSpeedMultiplier,
		DribbleTouchSpeed,
		DribbleTouchMaxSpeed
	);

	StartStrongRunDribbleTurn(
		SafeDesiredDirection,
		TurnMontage,
		TouchSpeedToUse,
		DribbleTouchUpwardSpeed
	);

	return true;
}

void AThirdPersonCppCharacter::StartStrongRunDribbleTurn(
	const FVector& DesiredDirection,
	UAnimMontage* TurnMontage,
	float TouchSpeed,
	float UpwardSpeed,
	bool bShouldKickToTargetAfterTurn,
	const FVector& KickTarget,
	ESoccerPendingKickMode KickMode,
	float KickHorizontalSpeed,
	bool bUsesChargedTrajectory
)
{
	if (ControlledBall == nullptr || TurnMontage == nullptr)
	{
		return;
	}

	FVector SafeDesiredDirection = DesiredDirection;
	SafeDesiredDirection.Z = 0.0f;
	SafeDesiredDirection = SafeDesiredDirection.GetSafeNormal();

	if (SafeDesiredDirection.IsNearlyZero())
	{
		return;
	}

	ActiveStrongRunDribbleTurnDirection = SafeDesiredDirection;
	ActiveStrongRunDribbleTurnTouchSpeed = TouchSpeed;
	ActiveStrongRunDribbleTurnUpwardSpeed = UpwardSpeed;
	ActiveStrongRunDribbleTurnResumeSpeed = FastRunSpeed;

	bActiveStrongRunDribbleTurnHasImpactedBall = false;

	bActiveStrongRunDribbleTurnShouldKickToTarget =
		bShouldKickToTargetAfterTurn;

	ActiveStrongRunDribbleTurnKickTarget = KickTarget;
	ActiveStrongRunDribbleTurnKickMode = KickMode;
	ActiveStrongRunDribbleTurnKickHorizontalSpeed = KickHorizontalSpeed;
	bActiveStrongRunDribbleTurnUsesChargedTrajectory =
		bUsesChargedTrajectory;

	SoccerControlState = ESoccerPlayerControlState::DribbleTurning;

	const FVector CurrentVelocity = GetVelocity();
	ActiveStrongRunDribbleTurnInitialSpeed = CurrentVelocity.Size2D();

	ActiveStrongRunDribbleTurnInitialDirection = CurrentVelocity;
	ActiveStrongRunDribbleTurnInitialDirection.Z = 0.0f;
	ActiveStrongRunDribbleTurnInitialDirection =
		ActiveStrongRunDribbleTurnInitialDirection.GetSafeNormal();

	if (ActiveStrongRunDribbleTurnInitialDirection.IsNearlyZero())
	{
		ActiveStrongRunDribbleTurnInitialDirection = GetActorForwardVector();
		ActiveStrongRunDribbleTurnInitialDirection.Z = 0.0f;
		ActiveStrongRunDribbleTurnInitialDirection =
			ActiveStrongRunDribbleTurnInitialDirection.GetSafeNormal();
	}
	ControlledBall->SetPossessed(true);
	UpdatePossessedBallLocation();

	const float MontageDuration = PlayAnimMontage(TurnMontage);

	ActiveStrongRunDribbleTurnDuration = MontageDuration;
	ActiveStrongRunDribbleTurnElapsedTime = 0.0f;

	if (MontageDuration <= 0.0f)
	{
		PerformStrongRunDribbleTurnImpact();
		FinishStrongRunDribbleTurnAnimation();
		return;
	}

	const float ActorRotationDuration = FMath::Max(
		StrongRunDribbleTurnQuickRotationDuration,
		MontageDuration
	);

	StartStrongRunDribbleTurnActorRotation(
		SafeDesiredDirection,
		ActorRotationDuration
	);

	GetWorldTimerManager().ClearTimer(StrongRunDribbleTurnImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(StrongRunDribbleTurnFinishTimerHandle);

	const float SafeImpactDelay = FMath::Clamp(
		StrongRunDribbleTurnImpactDelay,
		0.0f,
		MontageDuration
	);

	GetWorldTimerManager().SetTimer(
		StrongRunDribbleTurnImpactTimerHandle,
		this,
		&AThirdPersonCppCharacter::PerformStrongRunDribbleTurnImpact,
		SafeImpactDelay,
		false
	);

	GetWorldTimerManager().SetTimer(
		StrongRunDribbleTurnFinishTimerHandle,
		this,
		&AThirdPersonCppCharacter::FinishStrongRunDribbleTurnAnimation,
		MontageDuration + StrongRunDribbleTurnFinishExtraDelay,
		false
	);
}

void AThirdPersonCppCharacter::PerformStrongRunDribbleTurnImpact()
{
	if (ControlledBall == nullptr)
	{
		return;
	}

	if (bActiveStrongRunDribbleTurnHasImpactedBall)
	{
		return;
	}

	bActiveStrongRunDribbleTurnHasImpactedBall = true;

	FVector SafeDirection = ActiveStrongRunDribbleTurnDirection;
	SafeDirection.Z = 0.0f;
	SafeDirection = SafeDirection.GetSafeNormal();

	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	CurrentDribbleDirection = SafeDirection;
	DesiredDribbleDirection = SafeDirection;
	bHasDesiredDribbleDirection = true;

	if (bActiveStrongRunDribbleTurnShouldKickToTarget)
	{
		if (!TryRegisterHumanKickTouchForRules())
		{
			bActiveStrongRunDribbleTurnShouldKickToTarget = false;
			ActiveStrongRunDribbleTurnKickMode = ESoccerPendingKickMode::None;
			return;
		}
		if (ActiveStrongRunDribbleTurnKickMode == ESoccerPendingKickMode::KickAndFollow)
		{
			PrepareAutoPassBallForCleanKick(ActiveStrongRunDribbleTurnKickTarget);
		}
		if (bActiveStrongRunDribbleTurnUsesChargedTrajectory)
		{
			ControlledBall->ChargedKickToTarget(
				ActiveStrongRunDribbleTurnKickTarget,
				ActiveStrongRunDribbleTurnKickHorizontalSpeed
			);
		}
		else
		{
			ControlledBall->KickToTarget(
				ActiveStrongRunDribbleTurnKickTarget,
				ActiveStrongRunDribbleTurnKickHorizontalSpeed,
				TargetKickMinTravelTime,
				TargetKickMaxTravelTime
			);
		}

		if (
			ActiveStrongRunDribbleTurnKickMode !=
				ESoccerPendingKickMode::KickAndFollow
			)
		{
			ClearHumanBallClaim();
		}

		LastKickTime = GetWorld()->GetTimeSeconds();

		return;
	}

	ControlledBall->DribbleTouch(
		SafeDirection,
		ActiveStrongRunDribbleTurnTouchSpeed,
		ActiveStrongRunDribbleTurnUpwardSpeed,
		true
	);
}

void AThirdPersonCppCharacter::FinishStrongRunDribbleTurnAnimation()
{
	if (!bActiveStrongRunDribbleTurnHasImpactedBall)
	{
		PerformStrongRunDribbleTurnImpact();
	}

	const bool bShouldKickToTarget =
		bActiveStrongRunDribbleTurnShouldKickToTarget;

	const ESoccerPendingKickMode FinishedKickMode =
		ActiveStrongRunDribbleTurnKickMode;

	const FVector FinishedDirection = ActiveStrongRunDribbleTurnDirection;
	const float FinishedResumeSpeed = ActiveStrongRunDribbleTurnResumeSpeed;
	const FVector FinishedKickTarget = ActiveStrongRunDribbleTurnKickTarget;

	ActiveStrongRunDribbleTurnDirection = FVector::ZeroVector;
	ActiveStrongRunDribbleTurnTouchSpeed = 0.0f;
	ActiveStrongRunDribbleTurnUpwardSpeed = 0.0f;

	bActiveStrongRunDribbleTurnHasImpactedBall = false;

	bActiveStrongRunDribbleTurnShouldKickToTarget = false;
	ActiveStrongRunDribbleTurnKickTarget = FVector::ZeroVector;
	ActiveStrongRunDribbleTurnKickMode = ESoccerPendingKickMode::None;
	ActiveStrongRunDribbleTurnKickHorizontalSpeed = 0.0f;
	bActiveStrongRunDribbleTurnUsesChargedTrajectory = false;

	if (bShouldKickToTarget)
	{
		if (FinishedKickMode == ESoccerPendingKickMode::KickAndFollow)
		{
			StartAutoPassFollow(FinishedKickTarget);
		}
		else
		{
			EnterManualControl();
		}

		return;
	}

	ClearHumanBallClaim();
	SoccerControlState = ESoccerPlayerControlState::PossessingBall;
	GetCharacterMovement()->MaxWalkSpeed = SelectedMovementSpeed;

	ResumeDribbleMovementAfterTurn(FinishedDirection, FinishedResumeSpeed);

	ActiveStrongRunDribbleTurnDuration = 0.0f;
	ActiveStrongRunDribbleTurnElapsedTime = 0.0f;
	ActiveStrongRunDribbleTurnInitialSpeed = 0.0f;
	ActiveStrongRunDribbleTurnInitialDirection = FVector::ZeroVector;
}

bool AThirdPersonCppCharacter::TryStartStrongRunDribbleTurnForPendingKick()
{
	if (ControlledBall == nullptr)
	{
		return false;
	}

	// Por ahora lo aplicamos solamente al click izquierdo / autopase.
	if (PendingKickMode != ESoccerPendingKickMode::KickAndFollow)
	{
		return false;
	}

	if (
		SoccerControlState == ESoccerPlayerControlState::Kicking ||
		SoccerControlState == ESoccerPlayerControlState::DribbleTurning
		)
	{
		return false;
	}

	const float PlayerSpeed2D = GetVelocity().Size2D();

	if (PlayerSpeed2D < StrongRunDribbleTurnMinPlayerSpeed)
	{
		return false;
	}

	FVector DesiredKickDirection =
		PendingKickTarget - ControlledBall->GetActorLocation();

	DesiredKickDirection.Z = 0.0f;
	DesiredKickDirection = DesiredKickDirection.GetSafeNormal();

	if (DesiredKickDirection.IsNearlyZero())
	{
		return false;
	}

	float TurnAngleDegrees = 0.0f;

	UAnimMontage* TurnMontage =
		SelectStrongRunDribbleTurnMontageForDirection(
			DesiredKickDirection,
			TurnAngleDegrees
		);

	if (TurnMontage == nullptr)
	{
		return false;
	}

	const float TouchSpeedToUse = FMath::Clamp(
		DribbleTouchSpeed + PlayerSpeed2D * DribbleTouchPlayerSpeedMultiplier,
		DribbleTouchSpeed,
		DribbleTouchMaxSpeed
	);

	float HorizontalSpeedToUse = TargetKickHorizontalSpeed;

	// Usamos la animaci�n de giro como animaci�n de autopase.
	// Por eso guardamos el destino y pateamos en el impacto del giro.
	const FVector KickTargetToUse = PendingKickTarget;
	const ESoccerPendingKickMode KickModeToUse = PendingKickMode;

	PendingKickMode = ESoccerPendingKickMode::None;

	bHasPendingKickHorizontalSpeedOverride = false;
	PendingKickHorizontalSpeedOverride = 0.0f;

	StartStrongRunDribbleTurn(
		DesiredKickDirection,
		TurnMontage,
		TouchSpeedToUse,
		DribbleTouchUpwardSpeed,
		true,
		KickTargetToUse,
		KickModeToUse,
		HorizontalSpeedToUse,
		false
	);

	return true;
}

UAnimMontage* AThirdPersonCppCharacter::SelectNormalRunDribbleTurnMontageForDirection(
	const FVector& DesiredDirection,
	float& OutAngleDegrees
) const
{
	OutAngleDegrees = 0.0f;

	FVector CurrentDirection = CurrentDribbleDirection;
	CurrentDirection.Z = 0.0f;
	CurrentDirection = CurrentDirection.GetSafeNormal();

	if (CurrentDirection.IsNearlyZero())
	{
		CurrentDirection = GetActorForwardVector();
		CurrentDirection.Z = 0.0f;
		CurrentDirection = CurrentDirection.GetSafeNormal();
	}

	FVector NewDirection = DesiredDirection;
	NewDirection.Z = 0.0f;
	NewDirection = NewDirection.GetSafeNormal();

	if (CurrentDirection.IsNearlyZero() || NewDirection.IsNearlyZero())
	{
		return nullptr;
	}

	const float Dot = FMath::Clamp(
		FVector::DotProduct(CurrentDirection, NewDirection),
		-1.0f,
		1.0f
	);

	OutAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));

	if (OutAngleDegrees < NormalRunDribbleTurnMinAngleDegrees)
	{
		return nullptr;
	}

	const float CrossZ =
		FVector::CrossProduct(CurrentDirection, NewDirection).Z;

	// Si queda invertido izquierda/derecha, cambiar > por <.
	const bool bTurnRight = CrossZ > 0.0f;

	const bool bHardTurn =
		OutAngleDegrees >= NormalRunDribbleTurnHardAngleDegrees;

	if (bHardTurn)
	{
		return bTurnRight
			? NormalRunDribbleTurnRightOver90Montage
			: NormalRunDribbleTurnLeftOver90Montage;
	}

	return bTurnRight
		? NormalRunDribbleTurnRight45To90Montage
		: NormalRunDribbleTurnLeft45To90Montage;
}

bool AThirdPersonCppCharacter::TryStartNormalRunDribbleTurnForDirection(
	const FVector& DesiredDirection
)
{
	if (ControlledBall == nullptr)
	{
		return false;
	}

	if (SoccerControlState != ESoccerPlayerControlState::PossessingBall)
	{
		return false;
	}
	const float PlayerSpeed2D = GetVelocity().Size2D();

	if (PlayerSpeed2D < NormalRunDribbleTurnMinPlayerSpeed)
	{
		return false;
	}

	if (PlayerSpeed2D >= NormalRunDribbleTurnMaxPlayerSpeed)
	{
		return false;
	}

	FVector SafeDesiredDirection = DesiredDirection;
	SafeDesiredDirection.Z = 0.0f;
	SafeDesiredDirection = SafeDesiredDirection.GetSafeNormal();

	if (SafeDesiredDirection.IsNearlyZero())
	{
		return false;
	}

	float TurnAngleDegrees = 0.0f;

	UAnimMontage* TurnMontage = SelectNormalRunDribbleTurnMontageForDirection(
		SafeDesiredDirection,
		TurnAngleDegrees
	);

	if (TurnMontage == nullptr)
	{
		return false;
	}

	const float TouchSpeedToUse = FMath::Clamp(
		DribbleTouchSpeed + PlayerSpeed2D * DribbleTouchPlayerSpeedMultiplier,
		DribbleTouchSpeed,
		DribbleTouchMaxSpeed
	);

	StartNormalRunDribbleTurn(
		SafeDesiredDirection,
		TurnMontage,
		TouchSpeedToUse,
		DribbleTouchUpwardSpeed
	);

	return true;
}

void AThirdPersonCppCharacter::StartNormalRunDribbleTurn(
	const FVector& DesiredDirection,
	UAnimMontage* TurnMontage,
	float TouchSpeed,
	float UpwardSpeed
)
{
	if (ControlledBall == nullptr || TurnMontage == nullptr)
	{
		return;
	}

	FVector SafeDesiredDirection = DesiredDirection;
	SafeDesiredDirection.Z = 0.0f;
	SafeDesiredDirection = SafeDesiredDirection.GetSafeNormal();

	if (SafeDesiredDirection.IsNearlyZero())
	{
		return;
	}

	const FVector CurrentVelocity = GetVelocity();

	ActiveNormalRunDribbleTurnInitialSpeed = CurrentVelocity.Size2D();

	ActiveNormalRunDribbleTurnInitialDirection = CurrentVelocity;
	ActiveNormalRunDribbleTurnInitialDirection.Z = 0.0f;
	ActiveNormalRunDribbleTurnInitialDirection =
		ActiveNormalRunDribbleTurnInitialDirection.GetSafeNormal();

	if (ActiveNormalRunDribbleTurnInitialDirection.IsNearlyZero())
	{
		ActiveNormalRunDribbleTurnInitialDirection = GetActorForwardVector();
		ActiveNormalRunDribbleTurnInitialDirection.Z = 0.0f;
		ActiveNormalRunDribbleTurnInitialDirection =
			ActiveNormalRunDribbleTurnInitialDirection.GetSafeNormal();
	}

	ActiveNormalRunDribbleTurnDirection = SafeDesiredDirection;
	ActiveNormalRunDribbleTurnTouchSpeed = TouchSpeed;
	ActiveNormalRunDribbleTurnUpwardSpeed = UpwardSpeed;
	ActiveNormalRunDribbleTurnResumeSpeed = RunSpeed;

	bActiveNormalRunDribbleTurnHasImpactedBall = false;

	SoccerControlState = ESoccerPlayerControlState::DribbleTurning;

	ControlledBall->SetPossessed(true);
	UpdatePossessedBallLocation();

	const float MontageDuration = PlayAnimMontage(TurnMontage);

	ActiveNormalRunDribbleTurnDuration = MontageDuration;
	ActiveNormalRunDribbleTurnElapsedTime = 0.0f;

	if (MontageDuration <= 0.0f)
	{
		PerformNormalRunDribbleTurnImpact();
		FinishNormalRunDribbleTurnAnimation();
		return;
	}

	const float ActorRotationDuration = FMath::Max(
		NormalRunDribbleTurnQuickRotationDuration,
		MontageDuration
	);

	StartNormalRunDribbleTurnActorRotation(
		SafeDesiredDirection,
		ActorRotationDuration
	);

	GetWorldTimerManager().ClearTimer(NormalRunDribbleTurnImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(NormalRunDribbleTurnFinishTimerHandle);

	const float SafeImpactDelay = FMath::Clamp(
		NormalRunDribbleTurnImpactDelay,
		0.0f,
		MontageDuration
	);

	GetWorldTimerManager().SetTimer(
		NormalRunDribbleTurnImpactTimerHandle,
		this,
		&AThirdPersonCppCharacter::PerformNormalRunDribbleTurnImpact,
		SafeImpactDelay,
		false
	);

	GetWorldTimerManager().SetTimer(
		NormalRunDribbleTurnFinishTimerHandle,
		this,
		&AThirdPersonCppCharacter::FinishNormalRunDribbleTurnAnimation,
		MontageDuration + NormalRunDribbleTurnFinishExtraDelay,
		false
	);
}

void AThirdPersonCppCharacter::PerformNormalRunDribbleTurnImpact()
{
	if (ControlledBall == nullptr)
	{
		return;
	}

	if (bActiveNormalRunDribbleTurnHasImpactedBall)
	{
		return;
	}

	bActiveNormalRunDribbleTurnHasImpactedBall = true;

	FVector SafeDirection = ActiveNormalRunDribbleTurnDirection;
	SafeDirection.Z = 0.0f;
	SafeDirection = SafeDirection.GetSafeNormal();

	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	CurrentDribbleDirection = SafeDirection;
	DesiredDribbleDirection = SafeDirection;
	bHasDesiredDribbleDirection = true;

	ControlledBall->DribbleTouch(
		SafeDirection,
		ActiveNormalRunDribbleTurnTouchSpeed,
		ActiveNormalRunDribbleTurnUpwardSpeed,
		true
	);

}

void AThirdPersonCppCharacter::FinishNormalRunDribbleTurnAnimation()
{
	if (!bActiveNormalRunDribbleTurnHasImpactedBall)
	{
		PerformNormalRunDribbleTurnImpact();
	}

	const FVector FinishedDirection = ActiveNormalRunDribbleTurnDirection;
	const float FinishedResumeSpeed = ActiveNormalRunDribbleTurnResumeSpeed;

	ActiveNormalRunDribbleTurnDirection = FVector::ZeroVector;
	ActiveNormalRunDribbleTurnTouchSpeed = 0.0f;
	ActiveNormalRunDribbleTurnUpwardSpeed = 0.0f;
	ActiveNormalRunDribbleTurnResumeSpeed = 0.0f;

	bActiveNormalRunDribbleTurnHasImpactedBall = false;

	ClearHumanBallClaim();
	SoccerControlState = ESoccerPlayerControlState::PossessingBall;
	GetCharacterMovement()->MaxWalkSpeed = SelectedMovementSpeed;

	ResumeDribbleMovementAfterTurn(FinishedDirection, FinishedResumeSpeed);

	ActiveNormalRunDribbleTurnDuration = 0.0f;
	ActiveNormalRunDribbleTurnElapsedTime = 0.0f;
	ActiveNormalRunDribbleTurnInitialSpeed = 0.0f;
	ActiveNormalRunDribbleTurnInitialDirection = FVector::ZeroVector;

	bIsNormalRunDribbleTurnActorRotating = false;
	NormalRunDribbleTurnActorRotationElapsedTime = 0.0f;
	NormalRunDribbleTurnActorRotationCurrentDuration = 0.0f;
	NormalRunDribbleTurnActorTotalYawDelta = 0.0f;
}

void AThirdPersonCppCharacter::StartStrongRunDribbleTurnActorRotation(
	const FVector& DesiredDirection,
	float Duration
)
{
	FVector SafeDirection = DesiredDirection;
	SafeDirection.Z = 0.0f;
	SafeDirection = SafeDirection.GetSafeNormal();

	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	StrongRunDribbleTurnActorStartRotation = GetActorRotation();

	StrongRunDribbleTurnActorTargetRotation = SafeDirection.Rotation();
	StrongRunDribbleTurnActorTargetRotation.Pitch = 0.0f;
	StrongRunDribbleTurnActorTargetRotation.Roll = 0.0f;

	StrongRunDribbleTurnActorTotalYawDelta = FMath::FindDeltaAngleDegrees(
		StrongRunDribbleTurnActorStartRotation.Yaw,
		StrongRunDribbleTurnActorTargetRotation.Yaw
	);

	StrongRunDribbleTurnActorRotationElapsedTime = 0.0f;

	StrongRunDribbleTurnActorRotationCurrentDuration =
		FMath::Max(
			StrongRunDribbleTurnQuickRotationDuration,
			Duration
		);

	bIsStrongRunDribbleTurnActorRotating = true;
}


void AThirdPersonCppCharacter::UpdateStrongRunDribbleTurnActorRotation(float DeltaTime)
{
	if (!bIsStrongRunDribbleTurnActorRotating)
	{
		return;
	}

	StrongRunDribbleTurnActorRotationElapsedTime += DeltaTime;

	const float TotalDuration = FMath::Max(
		0.01f,
		StrongRunDribbleTurnActorRotationCurrentDuration
	);

	const float QuickDuration = FMath::Clamp(
		StrongRunDribbleTurnQuickRotationDuration,
		0.01f,
		TotalDuration
	);

	const float AbsTotalYawDelta =
		FMath::Abs(StrongRunDribbleTurnActorTotalYawDelta);

	const float DirectionSign =
		StrongRunDribbleTurnActorTotalYawDelta >= 0.0f ? 1.0f : -1.0f;

	const float QuickYawDegrees = FMath::Min(
		StrongRunDribbleTurnQuickRotationDegrees,
		AbsTotalYawDelta
	);

	const float RemainingYawDegrees =
		AbsTotalYawDelta - QuickYawDegrees;

	float AppliedYawDegrees = 0.0f;

	if (
		StrongRunDribbleTurnActorRotationElapsedTime <= QuickDuration ||
		RemainingYawDegrees <= KINDA_SMALL_NUMBER
		)
	{
		const float QuickAlpha = FMath::Clamp(
			StrongRunDribbleTurnActorRotationElapsedTime / QuickDuration,
			0.0f,
			1.0f
		);

		const float SmoothQuickAlpha = FMath::InterpEaseOut(
			0.0f,
			1.0f,
			QuickAlpha,
			2.0f
		);

		AppliedYawDegrees = QuickYawDegrees * SmoothQuickAlpha;
	}
	else
	{
		const float RemainingDuration = FMath::Max(
			0.01f,
			TotalDuration - QuickDuration
		);

		const float RemainingAlpha = FMath::Clamp(
			(StrongRunDribbleTurnActorRotationElapsedTime - QuickDuration) / RemainingDuration,
			0.0f,
			1.0f
		);

		const float SmoothRemainingAlpha = FMath::InterpEaseInOut(
			0.0f,
			1.0f,
			RemainingAlpha,
			2.0f
		);

		AppliedYawDegrees =
			QuickYawDegrees +
			RemainingYawDegrees * SmoothRemainingAlpha;
	}

	FRotator NewRotation = GetActorRotation();
	NewRotation.Pitch = 0.0f;
	NewRotation.Roll = 0.0f;
	NewRotation.Yaw =
		StrongRunDribbleTurnActorStartRotation.Yaw +
		DirectionSign * AppliedYawDegrees;

	SetActorRotation(NewRotation);

	if (StrongRunDribbleTurnActorRotationElapsedTime >= TotalDuration)
	{
		FRotator FinalRotation = StrongRunDribbleTurnActorTargetRotation;
		FinalRotation.Pitch = 0.0f;
		FinalRotation.Roll = 0.0f;

		SetActorRotation(FinalRotation);

		bIsStrongRunDribbleTurnActorRotating = false;
		StrongRunDribbleTurnActorTotalYawDelta = 0.0f;
	}
}

void AThirdPersonCppCharacter::StartNormalRunDribbleTurnActorRotation(
	const FVector& DesiredDirection,
	float Duration
)
{
	FVector SafeDirection = DesiredDirection;
	SafeDirection.Z = 0.0f;
	SafeDirection = SafeDirection.GetSafeNormal();

	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	NormalRunDribbleTurnActorStartRotation = GetActorRotation();

	NormalRunDribbleTurnActorTargetRotation = SafeDirection.Rotation();
	NormalRunDribbleTurnActorTargetRotation.Pitch = 0.0f;
	NormalRunDribbleTurnActorTargetRotation.Roll = 0.0f;

	NormalRunDribbleTurnActorTotalYawDelta = FMath::FindDeltaAngleDegrees(
		NormalRunDribbleTurnActorStartRotation.Yaw,
		NormalRunDribbleTurnActorTargetRotation.Yaw
	);

	NormalRunDribbleTurnActorRotationElapsedTime = 0.0f;

	NormalRunDribbleTurnActorRotationCurrentDuration =
		FMath::Max(
			NormalRunDribbleTurnQuickRotationDuration,
			Duration
		);

	bIsNormalRunDribbleTurnActorRotating = true;
}

void AThirdPersonCppCharacter::UpdateNormalRunDribbleTurnActorRotation(float DeltaTime)
{
	if (!bIsNormalRunDribbleTurnActorRotating)
	{
		return;
	}

	NormalRunDribbleTurnActorRotationElapsedTime += DeltaTime;

	const float TotalDuration = FMath::Max(
		0.01f,
		NormalRunDribbleTurnActorRotationCurrentDuration
	);

	const float QuickDuration = FMath::Clamp(
		NormalRunDribbleTurnQuickRotationDuration,
		0.01f,
		TotalDuration
	);

	const float AbsTotalYawDelta =
		FMath::Abs(NormalRunDribbleTurnActorTotalYawDelta);

	const float DirectionSign =
		NormalRunDribbleTurnActorTotalYawDelta >= 0.0f ? 1.0f : -1.0f;

	const float QuickYawDegrees = FMath::Min(
		NormalRunDribbleTurnQuickRotationDegrees,
		AbsTotalYawDelta
	);

	const float RemainingYawDegrees =
		AbsTotalYawDelta - QuickYawDegrees;

	float AppliedYawDegrees = 0.0f;

	if (
		NormalRunDribbleTurnActorRotationElapsedTime <= QuickDuration ||
		RemainingYawDegrees <= KINDA_SMALL_NUMBER
		)
	{
		const float QuickAlpha = FMath::Clamp(
			NormalRunDribbleTurnActorRotationElapsedTime / QuickDuration,
			0.0f,
			1.0f
		);

		const float SmoothQuickAlpha = FMath::InterpEaseOut(
			0.0f,
			1.0f,
			QuickAlpha,
			2.0f
		);

		AppliedYawDegrees = QuickYawDegrees * SmoothQuickAlpha;
	}
	else
	{
		const float RemainingDuration = FMath::Max(
			0.01f,
			TotalDuration - QuickDuration
		);

		const float RemainingAlpha = FMath::Clamp(
			(NormalRunDribbleTurnActorRotationElapsedTime - QuickDuration) / RemainingDuration,
			0.0f,
			1.0f
		);

		const float SmoothRemainingAlpha = FMath::InterpEaseInOut(
			0.0f,
			1.0f,
			RemainingAlpha,
			2.0f
		);

		AppliedYawDegrees =
			QuickYawDegrees +
			RemainingYawDegrees * SmoothRemainingAlpha;
	}

	FRotator NewRotation = GetActorRotation();
	NewRotation.Pitch = 0.0f;
	NewRotation.Roll = 0.0f;
	NewRotation.Yaw =
		NormalRunDribbleTurnActorStartRotation.Yaw +
		DirectionSign * AppliedYawDegrees;

	SetActorRotation(NewRotation);

	if (NormalRunDribbleTurnActorRotationElapsedTime >= TotalDuration)
	{
		FRotator FinalRotation = NormalRunDribbleTurnActorTargetRotation;
		FinalRotation.Pitch = 0.0f;
		FinalRotation.Roll = 0.0f;

		SetActorRotation(FinalRotation);

		bIsNormalRunDribbleTurnActorRotating = false;
		NormalRunDribbleTurnActorTotalYawDelta = 0.0f;
	}
}

void AThirdPersonCppCharacter::UpdateNormalRunDribbleTurnMovement(float DeltaTime)
{
	if (SoccerControlState != ESoccerPlayerControlState::DribbleTurning)
	{
		return;
	}

	if (ActiveNormalRunDribbleTurnDuration <= 0.0f)
	{
		return;
	}

	ActiveNormalRunDribbleTurnElapsedTime += DeltaTime;

	const float ZeroTime = FMath::Clamp(
		NormalRunDribbleTurnZeroSpeedTime,
		0.01f,
		ActiveNormalRunDribbleTurnDuration - 0.01f
	);

	// El giro normal ya no fuerza una parada completa. Bajamos hasta una
	// velocidad minima configurable y desde ahi aceleramos en la nueva direccion.
	// Ademas limitamos el piso por las velocidades inicial/de reanudacion para
	// evitar que una configuracion alta acelere accidentalmente la fase de frenado.
	const float MinimumTurnSpeed = FMath::Clamp(
		NormalRunDribbleTurnMinimumSpeed,
		0.0f,
		FMath::Min(
			ActiveNormalRunDribbleTurnInitialSpeed,
			ActiveNormalRunDribbleTurnResumeSpeed
		)
	);

	float SpeedToUse = MinimumTurnSpeed;
	FVector DirectionToUse = ActiveNormalRunDribbleTurnInitialDirection;

	if (ActiveNormalRunDribbleTurnElapsedTime <= ZeroTime)
	{
		const float Alpha = FMath::Clamp(
			ActiveNormalRunDribbleTurnElapsedTime / ZeroTime,
			0.0f,
			1.0f
		);

		const float SmoothAlpha = FMath::InterpEaseInOut(
			0.0f,
			1.0f,
			Alpha,
			2.0f
		);

		SpeedToUse = FMath::Lerp(
			ActiveNormalRunDribbleTurnInitialSpeed,
			MinimumTurnSpeed,
			SmoothAlpha
		);

		DirectionToUse = ActiveNormalRunDribbleTurnInitialDirection;
	}
	else
	{
		const float AccelDuration =
			FMath::Max(0.01f, ActiveNormalRunDribbleTurnDuration - ZeroTime);

		const float Alpha = FMath::Clamp(
			(ActiveNormalRunDribbleTurnElapsedTime - ZeroTime) / AccelDuration,
			0.0f,
			1.0f
		);

		const float SmoothAlpha = FMath::InterpEaseInOut(
			0.0f,
			1.0f,
			Alpha,
			2.0f
		);

		SpeedToUse = FMath::Lerp(
			MinimumTurnSpeed,
			ActiveNormalRunDribbleTurnResumeSpeed,
			SmoothAlpha
		);

		DirectionToUse = ActiveNormalRunDribbleTurnDirection;
	}

	DirectionToUse.Z = 0.0f;
	DirectionToUse = DirectionToUse.GetSafeNormal();

	if (DirectionToUse.IsNearlyZero())
	{
		return;
	}

	FVector NewVelocity = DirectionToUse * SpeedToUse;
	NewVelocity.Z = GetCharacterMovement()->Velocity.Z;

	GetCharacterMovement()->Velocity = NewVelocity;
}


bool AThirdPersonCppCharacter::FindHumanRunningJumpTackleThreat(
    ASoccerCharacterBase*& OutThreat,
    float& OutTimeToCrossing,
    bool& bOutAmbiguous
) const
{
    OutThreat = nullptr;
    OutTimeToCrossing = TNumericLimits<float>::Max();
    bOutAmbiguous = false;

    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return false;
    }

    const FVector HumanLocation = GetActorLocation();
    const float ThreatHalfWidth =
        FMath::Max(1.0f, HumanRunningJumpThreatHalfWidth);
    const float MinimumSlideSpeed =
        FMath::Max(1.0f, HumanRunningJumpMinimumIncomingSlideSpeed);
    const float MaximumThreatTime =
        FMath::Max(0.05f, HumanRunningJumpMaximumThreatTime);
    const float TieTime =
        FMath::Max(0.0f, HumanRunningJumpThreatTieTime);

    float SecondBestTime = TNumericLimits<float>::Max();

    for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
    {
        ASoccerCharacterBase* Tackler = *It;
        if (
            !IsValid(Tackler) ||
            Tackler == this ||
            Tackler->GetTeam() == GetTeam() ||
            !Tackler->IsTackleActive() ||
            Tackler->GetTacklePhase() != ESoccerTacklePhase::Sliding ||
            Tackler->GetTackleNormalizedTime() >
                FMath::Clamp(HumanRunningJumpMaximumTacklerNormalizedTime, 0.0f, 1.0f)
        )
        {
            continue;
        }

        const FVector SlideDirection = Tackler->GetActiveTackleDirection();
        const float SlideSpeed = Tackler->GetCurrentTackleHorizontalSpeed();
        if (SlideDirection.IsNearlyZero() || SlideSpeed < MinimumSlideSpeed)
        {
            continue;
        }

        FVector ToHuman = HumanLocation - Tackler->GetActorLocation();
        ToHuman.Z = 0.0f;
        const float AlongDistance = FVector::DotProduct(ToHuman, SlideDirection);
        if (AlongDistance <= 0.0f)
        {
            continue;
        }

        const float TimeToCrossing = AlongDistance / SlideSpeed;
        if (TimeToCrossing > MaximumThreatTime)
        {
            continue;
        }

        const FVector ClosestPoint =
            Tackler->GetActorLocation() + SlideDirection * AlongDistance;
        const float LateralDistance = FVector::Dist2D(HumanLocation, ClosestPoint);
        if (LateralDistance > ThreatHalfWidth)
        {
            continue;
        }

        if (TimeToCrossing < OutTimeToCrossing)
        {
            SecondBestTime = OutTimeToCrossing;
            OutTimeToCrossing = TimeToCrossing;
            OutThreat = Tackler;
        }
        else if (TimeToCrossing < SecondBestTime)
        {
            SecondBestTime = TimeToCrossing;
        }
    }

    if (!IsValid(OutThreat))
    {
        return false;
    }

    bOutAmbiguous =
        FMath::IsFinite(SecondBestTime) &&
        FMath::Abs(SecondBestTime - OutTimeToCrossing) <= TieTime;
    return true;
}

void AThirdPersonCppCharacter::HandleHumanRunningJump()
{
    if (
        IsValid(MatchManager) &&
        MatchManager->IsHumanThrowInMovementLocked(this)
    )
    {
        return;
    }

    if (bHumanJumpHeaderRequestActive)
    {
        ClearHumanJumpHeaderRequest(true, true);
    }

    if (IsAerialActionQueuedOrPlaying())
    {
        CancelAerialAction();
    }

    if (IsTackleActive() || IsTackleFallReactionActive() || IsTackleEvasionActive())
    {
        return;
    }

    ASoccerCharacterBase* Threat = nullptr;
    float TimeToCrossing = TNumericLimits<float>::Max();
    bool bAmbiguousThreat = false;
    const bool bHasThreat = FindHumanRunningJumpTackleThreat(
        Threat,
        TimeToCrossing,
        bAmbiguousThreat
    );

    bool bStarted = false;
    if (bHasThreat && !bAmbiguousThreat && IsValid(Threat))
    {
        bStarted = TryStartTackleEvasion(Threat);
    }
    else
    {
        bStarted = TryStartRunningJump(DefaultHumanRunningJumpSide);
    }

    if (!bStarted)
    {
        return;
    }

    if (bHasThreat && bAmbiguousThreat)
    {
        ASoccerDebugManager::Message(
            this,
            ESoccerDebugCategory::Tackle,
            FString::Printf(TEXT("HUMAN JUMP: amenaza ambigua -> DEFAULT | t=%.2f"), TimeToCrossing),
            FColor::Green
        );
    }
    else if (bHasThreat && IsValid(Threat))
    {
        ASoccerDebugManager::Message(
            this,
            ESoccerDebugCategory::Tackle,
            FString::Printf(TEXT("HUMAN JUMP: evade %s | t=%.2f"), *Threat->GetName(), TimeToCrossing),
            FColor::Green
        );
    }
    else
    {
        ASoccerDebugManager::Message(
            this,
            ESoccerDebugCategory::Tackle,
            TEXT("HUMAN JUMP: DEFAULT"),
            FColor::Green
        );
    }
}

void AThirdPersonCppCharacter::HandleTackleInput()
{
    if (
        (IsTackleActive() || IsTackleFallReactionActive()) ||
        IsAerialActionQueuedOrPlaying() ||
        SoccerControlState == ESoccerPlayerControlState::PossessingBall ||
        SoccerControlState == ESoccerPlayerControlState::DribbleTurning ||
        SoccerControlState == ESoccerPlayerControlState::Kicking ||
        bIsHumanStealAttemptActive
    )
    {
        return;
    }

	if (!IsHumanBallActionAllowedNow())
	{
		ClearBallActionsForMatchRestriction();
		return;
	}

    if (SoccerControlState == ESoccerPlayerControlState::ChasingBall)
    {
        CancelBallChaseByManualInput();
    }

    if (!IsValid(MatchManager))
    {
        FindMatchManager();
    }

    FVector TackleTarget =
        GetActorLocation() + GetActorForwardVector() * 500.0f;

    if (IsValid(MatchManager))
    {
        ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

        if (IsValid(SoccerBall))
        {
            TackleTarget = SoccerBall->GetActorLocation();
        }
    }

    TryStartTackleTowardLocation(TackleTarget);
}

void AThirdPersonCppCharacter::RequestNormalPassFromTeammate()
{
	if (IsTackleActive() || IsTackleFallReactionActive())
	{
		return;
	}

	RequestPassFromTeammate(
		ESoccerHumanPassRequestType::Normal
	);
}

void AThirdPersonCppCharacter::RequestAerialPassFromTeammate()
{
	if (IsTackleActive() || IsTackleFallReactionActive())
	{
		return;
	}

	RequestPassFromTeammate(
		ESoccerHumanPassRequestType::AerialHeader
	);
}

void AThirdPersonCppCharacter::RequestPassFromTeammate(
	ESoccerHumanPassRequestType RequestType
)
{
	// The pass request is a persistent tactical state, not a one-frame action.
	// Always forward the key press so the same key can turn it off and the
	// other key can switch its type, even while another assisted action exists.
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Red,
				TEXT("No se encontro SoccerMatchManager")
			);
		}

		return;
	}

	if (
		IsValid(MatchManager) &&
		MatchManager->IsHumanThrowInMovementLocked(this)
	)
	{
		return;
	}

	MatchManager->ToggleHumanPassRequest(this, RequestType);
}

void AThirdPersonCppCharacter::HandleLeftClickTarget()
{
	if (IsTackleActive() || IsTackleFallReactionActive())
	{
		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	// Throw-ins are deliberately not ordinary ball actions. During the human
	// execution window the left click only selects the throw target; all normal
	// chase/kick/tackle input remains locked by the restart system.
	if (
		IsValid(MatchManager) &&
		MatchManager->CanHumanThrowInTakerExecuteNow(this)
	)
	{
		FVector ThrowTarget;
		if (
			GetMouseFieldLocation(ThrowTarget) &&
			MatchManager->TryStartHumanThrowInToTarget(this, ThrowTarget)
		)
		{
			HideAutoPassTargetMarker();
			ShowReleaseTargetMarker(ThrowTarget);
		}
		return;
	}

	if (!IsHumanBallActionAllowedNow())
	{
		ClearBallActionsForMatchRestriction();
		return;
	}

	if (
		IsValid(MatchManager) &&
		MatchManager->IsPenaltyKickRestartActive() &&
		!MatchManager->IsHumanPenaltyTaker(this)
	)
	{
		return;
	}

	if (IsAerialActionLocked())
	{
		return;
	}

	if (SoccerControlState == ESoccerPlayerControlState::Kicking)
	{
		return;
	}

	// A human set-piece taker is already at a dead-ball restart: the first left click
	// must arm the actual pass/kick, not the generic "collect the loose ball"
	// action used in open play.
	if (
		IsValid(MatchManager) &&
		MatchManager->CanHumanFootRestartTakerExecuteNow(this)
	)
	{
		ArmChaseCancelIgnoreForCurrentMovementInput();
		StoreKickTarget(ESoccerPendingKickMode::KickAndFollow);
		return;
	}

	if (bIsHumanStealAttemptActive)
	{
		StoreKickTarget(ESoccerPendingKickMode::KickAndFollow);
		ArmChaseCancelIgnoreForCurrentMovementInput();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.0f,
				FColor::Cyan,
				TEXT("Destino de autopase guardado para despu�s del robo")
			);
		}

		return;
	}

	if (SoccerControlState != ESoccerPlayerControlState::PossessingBall)
	{
		ASoccerAICharacter* OpponentPossessingAI =
			GetOpponentPossessingAICharacter();

		if (IsValid(OpponentPossessingAI))
		{
			StartHumanStealAttempt(OpponentPossessingAI);
			return;
		}
	}

	if (ControlledBall == nullptr)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.5f,
				FColor::Red,
				TEXT("No se encontro SoccerBall en el nivel")
			);
		}

		return;
	}

	// Primer click izquierdo sin pelota:
	// solo va a buscar la pelota para dominarla.
	if (SoccerControlState == ESoccerPlayerControlState::Manual)
	{
		PendingKickMode = ESoccerPendingKickMode::None;
		PendingKickTarget = FVector::ZeroVector;

		ActivateHumanBallClaim();
		EnterChasingBall();

		/* First click asks for a controlled reception: chest, then soft head. */
		if (!TryStartHumanAerialApproach(
			ESoccerAerialActionIntent::Control
		))
		{
			TryStartHumanAerialApproach(
				ESoccerAerialActionIntent::Automatic
			);
		}
		ArmChaseCancelIgnoreForCurrentMovementInput();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Yellow,
				TEXT("Primer click: yendo a buscar la pelota")
			);
		}

		return;
	}

	// Segundo click izquierdo mientras ya va hacia la pelota:
	// ahora s� marca destino y prepara KickAndFollow.
	if (SoccerControlState == ESoccerPlayerControlState::ChasingBall)
	{
		StoreKickTarget(ESoccerPendingKickMode::KickAndFollow);
		ArmChaseCancelIgnoreForCurrentMovementInput();

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Cyan,
				TEXT("Segundo click: patear y seguir al llegar")
			);
		}

		return;
	}

	// Si ya domina la pelota:
	// click izquierdo = marcar destino, patear y seguir.
	if (SoccerControlState == ESoccerPlayerControlState::PossessingBall)
	{
		ArmChaseCancelIgnoreForCurrentMovementInput();
		StoreKickTarget(ESoccerPendingKickMode::KickAndFollow);
		return;
	}
}

void AThirdPersonCppCharacter::StartChargedKickRelease()
{
	if (IsTackleActive() || IsTackleFallReactionActive())
	{
		return;
	}

	if (!IsHumanBallActionAllowedNow())
	{
		ClearBallActionsForMatchRestriction();
		return;
	}

	if (IsValid(MatchManager) && MatchManager->IsPenaltyKickRestartActive())
	{
		if (
			!MatchManager->IsHumanPenaltyTaker(this) ||
			MatchManager->GetMatchPlayState() !=
				ESoccerMatchPlayState::PenaltyKickTaking
		)
		{
			return;
		}

		// During a penalty this button is only the normal charged shot.
		// Do not arm the jump-header planner for the stationary ball.
		bIsChargingKickRelease = true;
		KickChargeStartTime = GetWorld()->GetTimeSeconds();
		return;
	}

	if (
		IsValid(MatchManager) &&
		MatchManager->CanHumanFootRestartTakerExecuteNow(this)
	)
	{
		if (SoccerControlState == ESoccerPlayerControlState::Kicking)
		{
			return;
		}

		// This is a stationary set piece, so right click
		// charges a normal ground kick instead of arming aerial interception logic.
		bIsChargingKickRelease = true;
		KickChargeStartTime = GetWorld()->GetTimeSeconds();
		return;
	}

	if (IsAerialActionLocked())
	{
		return;
	}

	if (SoccerControlState == ESoccerPlayerControlState::Kicking)
	{
		return;
	}

	bIsChargingKickRelease = true;
	KickChargeStartTime = GetWorld()->GetTimeSeconds();

	/*
	 * A jump header cannot wait for button release: the animation reaches the
	 * head contact about 1.23 seconds after it begins. Arm and search on press,
	 * while release remains responsible for final aim and power.
	 */
	ArmHumanJumpHeaderRequestFromCurrentInput();
}

void AThirdPersonCppCharacter::FinishChargedKickRelease()
{
	if (IsTackleActive() || IsTackleFallReactionActive())
	{
		return;
	}

	if (!IsHumanBallActionAllowedNow())
	{
		ClearBallActionsForMatchRestriction();
		return;
	}

	if (
		IsValid(MatchManager) &&
		MatchManager->IsPenaltyKickRestartActive() &&
		!MatchManager->IsHumanPenaltyTaker(this)
	)
	{
		bIsChargingKickRelease = false;
		return;
	}

	if (!bIsChargingKickRelease)
	{
		return;
	}

	const float ChargePercent = GetCurrentKickChargePercent();
	bIsChargingKickRelease = false;

	const float ChargedHorizontalSpeed = FMath::Lerp(
		KickChargeMinHorizontalSpeed,
		KickChargeMaxHorizontalSpeed,
		ChargePercent
	);

	bHasPendingKickHorizontalSpeedOverride = true;
	PendingKickHorizontalSpeedOverride = ChargedHorizontalSpeed;

	/*
	 * The montage may already be queued or even playing because the request was
	 * armed on press. Release is still allowed to finalize the outgoing target
	 * and power until the actual head contact occurs.
	 */
	if (bHumanJumpHeaderRequestActive)
	{
		FVector FieldLocation;

		if (GetMouseFieldLocation(FieldLocation))
		{
			PendingKickTarget = FieldLocation;
			PendingKickMode = ESoccerPendingKickMode::KickAndRelease;
			HideAutoPassTargetMarker();
			ShowReleaseTargetMarker(PendingKickTarget);
		}

		ArmChaseCancelIgnoreForCurrentMovementInput();

		if (!IsAerialActionLocked())
		{
			TryStartOrUpdateHumanJumpHeaderRequest(
				PendingKickTarget,
				PendingKickHorizontalSpeedOverride
			);
		}

		return;
	}

	if (IsAerialActionLocked())
	{
		return;
	}

	if (SoccerControlState == ESoccerPlayerControlState::Kicking)
	{
		return;
	}

	// Caso 1:
	// Ya esta corriendo a robar por click izquierdo.
	// Entonces el click derecho sirve para guardar destino verde release.
	if (bIsHumanStealAttemptActive)
	{
		StoreKickTarget(ESoccerPendingKickMode::KickAndRelease);
		ArmChaseCancelIgnoreForCurrentMovementInput();
		return;
	}

	// Caso 2:
	// No tiene pelota, pero un bot rival si.
	// El click derecho inicia intento de robo y guarda destino verde.
	if (SoccerControlState != ESoccerPlayerControlState::PossessingBall)
	{
		ASoccerAICharacter* OpponentPossessingAI =
			GetOpponentPossessingAICharacter();

		if (IsValid(OpponentPossessingAI))
		{
			StartHumanStealAttempt(OpponentPossessingAI);

			// StartHumanStealAttempt limpia pendientes.
			bHasPendingKickHorizontalSpeedOverride = true;
			PendingKickHorizontalSpeedOverride = ChargedHorizontalSpeed;

			StoreKickTarget(ESoccerPendingKickMode::KickAndRelease);
			ArmChaseCancelIgnoreForCurrentMovementInput();
			return;
		}
	}

	// Flujo generico de disparo/patada cargada.
	StoreKickTarget(ESoccerPendingKickMode::KickAndRelease);
	ArmChaseCancelIgnoreForCurrentMovementInput();
}

float AThirdPersonCppCharacter::GetCurrentKickChargePercent() const
{
	if (!bIsChargingKickRelease)
	{
		return 0.0f;
	}

	const float SafeFullTime = FMath::Max(0.01f, KickChargeFullTime);

	const float HeldTime =
		GetWorld()->GetTimeSeconds() - KickChargeStartTime;

	return FMath::Clamp(HeldTime / SafeFullTime, 0.0f, 1.0f);
}

bool AThirdPersonCppCharacter::GetKickChargePercent(float& OutPercent) const
{
	if (!bIsChargingKickRelease)
	{
		OutPercent = 0.0f;
		return false;
	}

	OutPercent = GetCurrentKickChargePercent();
	return true;
}

void AThirdPersonCppCharacter::StoreKickTarget(ESoccerPendingKickMode KickMode)
{
	if (!IsHumanBallActionAllowedNow())
	{
		ClearBallActionsForMatchRestriction();
		return;
	}

	FVector FieldLocation;

	if (!GetMouseFieldLocation(FieldLocation))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.0f,
				FColor::Red,
				TEXT("No se pudo seleccionar un punto del campo")
			);
		}

		return;
	}

	// StoreKickTarget is reached only from the left/right click handlers. Once
	// the field target is valid, the human owns this recovery attempt until the
	// action finishes or manual input cancels it.
	ActivateHumanBallClaim();

	/*
	 * While an assisted jump header is already queued, another click only
	 * updates the outgoing target/mode. It must not replace the jumping plan
	 * with a standing reception. A left click can therefore request follow-up
	 * after the header, while a charged release updates target and power.
	 */
	if (
		bHumanJumpHeaderRequestActive &&
		(IsAerialActionApproaching() || IsAerialActionWaitingToStart()) &&
		GetActiveAerialActionType() ==
			ESoccerAerialActionType::JumpHeaderKick
	)
	{
		PendingKickTarget = FieldLocation;
		PendingKickMode = KickMode;

		HideAutoPassTargetMarker();
		HideReleaseTargetMarker();

		if (PendingKickMode == ESoccerPendingKickMode::KickAndFollow)
		{
			ShowAutoPassTargetMarker(PendingKickTarget);
		}
		else
		{
			ShowReleaseTargetMarker(PendingKickTarget);
		}

		ArmChaseCancelIgnoreForCurrentMovementInput();
		return;
	}

	/*
	 * A charged right-button release is the explicit human jump-header input.
	 * Try only ActiveHeader here. If no synchronized jump solution exists,
	 * continue with the existing generic chase/ground-kick behavior; never
	 * silently downgrade this request to a standing header.
	 */
	const bool bExplicitJumpHeaderRequest =
		KickMode == ESoccerPendingKickMode::KickAndRelease &&
		bHasPendingKickHorizontalSpeedOverride &&
		PendingKickHorizontalSpeedOverride > 0.0f &&
		SoccerControlState != ESoccerPlayerControlState::PossessingBall &&
		!bIsHumanStealAttemptActive;

	if (
		bExplicitJumpHeaderRequest &&
		TryStartOrUpdateHumanJumpHeaderRequest(
			FieldLocation,
			PendingKickHorizontalSpeedOverride
		)
	)
	{
		return;
	}

	PendingKickTarget = FieldLocation;
	PendingKickMode = KickMode;

	if (
		KickMode == ESoccerPendingKickMode::KickAndFollow &&
		SoccerControlState != ESoccerPlayerControlState::PossessingBall &&
		IsValid(ControlledBall)
	)
	{
		/*
		 * Left-click remains the finesse request: passive standing redirect
		 * first, with an uncharged jump header only as a physical fallback.
		 */
		const ESoccerAerialActionIntent PrimaryIntent =
			ESoccerAerialActionIntent::StandingHeaderRedirect;
		const ESoccerAerialActionIntent FallbackIntent =
			ESoccerAerialActionIntent::ActiveHeader;

		bool bPrimaryQueued = false;

		if (IsAerialActionApproaching() || IsAerialActionWaitingToStart())
		{
			bPrimaryQueued = TryChangeQueuedAerialActionIntent(
				PrimaryIntent
			);

			if (!bPrimaryQueued)
			{
				TryChangeQueuedAerialActionIntent(FallbackIntent);
			}
		}
		else
		{
			bPrimaryQueued = TryStartHumanAerialApproach(
				PrimaryIntent
			);

			if (!bPrimaryQueued)
			{
				TryStartHumanAerialApproach(FallbackIntent);
			}
		}
	}

	if (PendingKickMode == ESoccerPendingKickMode::KickAndFollow)
	{
		ShowAutoPassTargetMarker(PendingKickTarget);
	}
	else if (PendingKickMode == ESoccerPendingKickMode::KickAndRelease)
	{
		ShowReleaseTargetMarker(PendingKickTarget);
	}

	if (SoccerControlState == ESoccerPlayerControlState::PossessingBall)
	{
		if (ControlledBall == nullptr)
		{
			EnterManualControl();
			return;
		}

		const float DistanceToBall = FVector::Dist2D(
			GetActorLocation(),
			ControlledBall->GetActorLocation()
		);

		if (DistanceToBall <= KickExecutionDistance)
		{
			ExecutePendingKick();
		}
		else
		{
			EnterChasingBall();

			if (GEngine)
			{
				const FString Message =
					KickMode == ESoccerPendingKickMode::KickAndFollow
					? TEXT("Objetivo guardado: buscando pelota para auto-pase")
					: TEXT("Objetivo guardado: buscando pelota para disparo");

				GEngine->AddOnScreenDebugMessage(
					-1,
					1.2f,
					FColor::Cyan,
					Message
				);
			}
		}
	}
	else if (SoccerControlState == ESoccerPlayerControlState::ChasingBall)
	{
		if (GEngine)
		{
			const FString Message =
				KickMode == ESoccerPendingKickMode::KickAndFollow
				? TEXT("Objetivo guardado: patear y seguir")
				: TEXT("Objetivo guardado: patear y soltar control");

			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Cyan,
				Message
			);
		}
	}
	else if (SoccerControlState == ESoccerPlayerControlState::Manual)
	{
		EnterChasingBall();

		if (GEngine)
		{
			const FString Message =
				KickMode == ESoccerPendingKickMode::KickAndFollow
				? TEXT("Objetivo guardado: yendo a la pelota para patear y seguir")
				: TEXT("Objetivo guardado: yendo a la pelota para patear y soltar control");

			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Yellow,
				Message
			);
		}
	}
}

bool AThirdPersonCppCharacter::GetMouseFieldLocation(FVector& OutLocation) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(Controller);

	if (PlayerController == nullptr)
	{
		return false;
	}

	float ScreenX = 0.0f;
	float ScreenY = 0.0f;

	if (!GetAimCursorScreenPosition(ScreenX, ScreenY))
	{
		return false;
	}

	FVector WorldLocation;
	FVector WorldDirection;

	const bool bDeprojected = PlayerController->DeprojectScreenPositionToWorld(
		ScreenX,
		ScreenY,
		WorldLocation,
		WorldDirection
	);

	if (!bDeprojected)
	{
		return false;
	}

	const FVector TraceStart = WorldLocation;
	const FVector TraceEnd = TraceStart + WorldDirection * 50000.0f;

	FHitResult Hit;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	if (!bHit)
	{
		return false;
	}

	OutLocation = Hit.ImpactPoint;
	return true;
}

void AThirdPersonCppCharacter::ExecutePendingKick()
{

	if (ControlledBall == nullptr)
	{
		EnterManualControl();
		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (
		IsValid(MatchManager) &&
		!MatchManager->CanCharacterTouchBallNow(this)
		)
	{
		PendingKickMode = ESoccerPendingKickMode::None;
		bHasPendingKickHorizontalSpeedOverride = false;
		PendingKickHorizontalSpeedOverride = 0.0f;
		EnterManualControl();
		return;
	}

	const bool bHumanRestartExecution =
		IsValid(MatchManager) &&
		MatchManager->CanHumanFootRestartTakerExecuteNow(this);

	// A set piece never starts a dribble-turn carry. The ball must remain on the
	// restart spot until the actual kick impact.
	if (
		!bHumanRestartExecution &&
		TryStartStrongRunDribbleTurnForPendingKick()
	)
	{
		return;
	}

	const bool bUseChargedTrajectory =
		bHasPendingKickHorizontalSpeedOverride &&
		PendingKickMode == ESoccerPendingKickMode::KickAndRelease;

	float HorizontalSpeedToUse = TargetKickHorizontalSpeed;

	UAnimMontage* KickMontage = SelectKickMontageForTarget(
		PendingKickTarget,
		HorizontalSpeedToUse
	);

	// Si es disparo cargado, la carga manda sobre cualquier ajuste
	// que haya hecho SelectKickMontageForTarget.
	if (bUseChargedTrajectory)
	{
		HorizontalSpeedToUse = GetDistanceAdjustedChargedKickSpeed(
			PendingKickHorizontalSpeedOverride,
			PendingKickTarget
		);
	}

	// Si no hay montaje, por ejemplo pase corto, pateamos inmediatamente.
	if (KickMontage == nullptr)
	{
		const FVector ExecutedTarget = PendingKickTarget;
		const ESoccerPendingKickMode ExecutedMode = PendingKickMode;

		if (!TryRegisterHumanKickTouchForRules())
		{
			PendingKickMode = ESoccerPendingKickMode::None;

			bHasPendingKickHorizontalSpeedOverride = false;
			PendingKickHorizontalSpeedOverride = 0.0f;

			EnterManualControl();

			return;
		}

		if (
			ExecutedMode == ESoccerPendingKickMode::KickAndFollow &&
			!bHumanRestartExecution
		)
		{
			PrepareAutoPassBallForCleanKick(ExecutedTarget);
		}

		if (bUseChargedTrajectory)
		{
			ControlledBall->ChargedKickToTarget(
				ExecutedTarget,
				HorizontalSpeedToUse
			);
		}
		else
		{
			ControlledBall->KickToTarget(
				ExecutedTarget,
				HorizontalSpeedToUse,
				TargetKickMinTravelTime,
				TargetKickMaxTravelTime
			);
		}

		LastKickTime = GetWorld()->GetTimeSeconds();
		PendingKickMode = ESoccerPendingKickMode::None;

		bHasPendingKickHorizontalSpeedOverride = false;
		PendingKickHorizontalSpeedOverride = 0.0f;

		if (
			ExecutedMode == ESoccerPendingKickMode::KickAndFollow &&
			!bHumanRestartExecution
		)
		{
			StartAutoPassFollow(ExecutedTarget);
		}
		else
		{
			EnterManualControl();
		}

		return;
	}

	ActiveKickTarget = PendingKickTarget;
	ActiveKickHorizontalSpeed = HorizontalSpeedToUse;
	ActiveKickMode = PendingKickMode;
	bActiveKickHasImpactedBall = false;
	bActiveKickUsesChargedTrajectory = bUseChargedTrajectory;
	bActiveKickWasHumanRestartExecution = bHumanRestartExecution;

	PendingKickMode = ESoccerPendingKickMode::None;

	bHasPendingKickHorizontalSpeedOverride = false;
	PendingKickHorizontalSpeedOverride = 0.0f;

	SoccerControlState = ESoccerPlayerControlState::Kicking;

	GetCharacterMovement()->StopMovementImmediately();

	if (!bHumanRestartExecution)
	{
		ControlledBall->SetPossessed(true);
		UpdatePossessedBallLocation();
	}
	else
	{
		// Preparation/Execution owns the restart spot until the impact callback.
		// Do not attach or dribble the ball toward the human foot.
		ControlledBall->SetPossessed(false);
		ControlledBall->StopBallKeepingPhysics();
	}

	const float MontageDuration = PlayAnimMontage(KickMontage);

	if (MontageDuration <= 0.0f)
	{
		PerformPendingKickImpact();
		FinishPendingKickAnimation();
		return;
	}

	const float ImpactDelay = GetKickImpactDelayForMontage(
		KickMontage,
		MontageDuration
	);

	GetWorldTimerManager().ClearTimer(KickImpactTimerHandle);
	GetWorldTimerManager().ClearTimer(KickFinishTimerHandle);

	GetWorldTimerManager().SetTimer(
		KickImpactTimerHandle,
		this,
		&AThirdPersonCppCharacter::PerformPendingKickImpact,
		ImpactDelay,
		false
	);

	GetWorldTimerManager().SetTimer(
		KickFinishTimerHandle,
		this,
		&AThirdPersonCppCharacter::FinishPendingKickAnimation,
		MontageDuration + KickAnimationFinishExtraDelay,
		false
	);
}

float AThirdPersonCppCharacter::GetKickImpactDelayForMontage(
	UAnimMontage* KickMontage,
	float MontageDuration
) const
{
	float DesiredDelay = PassKickImpactDelay;

	if (
		KickMontage == StrikeLeftLegForwardJogMontage ||
		KickMontage == StrikeRightLegForwardJogMontage
		)
	{
		DesiredDelay = StrikeKickImpactDelay;
	}

	const float MaxSafeDelay = FMath::Max(0.05f, MontageDuration - 0.05f);

	return FMath::Clamp(
		DesiredDelay,
		0.05f,
		MaxSafeDelay
	);
}

void AThirdPersonCppCharacter::PerformPendingKickImpact()
{
	if (ControlledBall == nullptr)
	{
		return;
	}

	if (bActiveKickHasImpactedBall)
	{
		return;
	}

	bActiveKickHasImpactedBall = true;

	if (!TryRegisterHumanKickTouchForRules())
	{
		EnterManualControl();
		return;
	}

	if (
		ActiveKickMode == ESoccerPendingKickMode::KickAndFollow &&
		!bActiveKickWasHumanRestartExecution
	)
	{
		PrepareAutoPassBallForCleanKick(ActiveKickTarget);
	}

	if (bActiveKickUsesChargedTrajectory)
	{
		ControlledBall->ChargedKickToTarget(
			ActiveKickTarget,
			ActiveKickHorizontalSpeed
		);
	}
	else
	{
		ControlledBall->KickToTarget(
			ActiveKickTarget,
			ActiveKickHorizontalSpeed,
			TargetKickMinTravelTime,
			TargetKickMaxTravelTime
		);
	}

	if (ActiveKickMode != ESoccerPendingKickMode::KickAndFollow)
	{
		ClearHumanBallClaim();
	}

	LastKickTime = GetWorld()->GetTimeSeconds();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.0f,
			FColor::Cyan,
			TEXT("Impacto de patada")
		);
	}
}

void AThirdPersonCppCharacter::FinishPendingKickAnimation()
{
	if (!bActiveKickHasImpactedBall)
	{
		PerformPendingKickImpact();
	}

	const ESoccerPendingKickMode FinishedKickMode = ActiveKickMode;
	const FVector FinishedKickTarget = ActiveKickTarget;
	const bool bFinishedHumanRestartExecution =
		bActiveKickWasHumanRestartExecution;

	ActiveKickMode = ESoccerPendingKickMode::None;
	ActiveKickHorizontalSpeed = 0.0f;
	bActiveKickHasImpactedBall = false;
	bActiveKickUsesChargedTrajectory = false;
	bActiveKickWasHumanRestartExecution = false;

	if (
		FinishedKickMode == ESoccerPendingKickMode::KickAndFollow &&
		!bFinishedHumanRestartExecution
	)
	{
		StartAutoPassFollow(FinishedKickTarget);
	}
	else
	{
		EnterManualControl();
	}
}

void AThirdPersonCppCharacter::MoveAimCursorVertical(float Value)
{
	const float AbsValue = FMath::Abs(Value);

	if (AbsValue <= AimCursorInputDeadZone)
	{
		AimCursorAccelerationAlpha = 0.0f;
		LastAimCursorInputSign = 0.0f;
		return;
	}

	const float CurrentInputSign = Value > 0.0f ? 1.0f : -1.0f;

	if (
		LastAimCursorInputSign != 0.0f &&
		CurrentInputSign != LastAimCursorInputSign
		)
	{
		AimCursorAccelerationAlpha = 0.0f;
	}

	LastAimCursorInputSign = CurrentInputSign;

	const float SafeAccelerationTime =
		FMath::Max(0.01f, AimCursorAccelerationTime);

	AimCursorAccelerationAlpha = FMath::Clamp(
		AimCursorAccelerationAlpha + GetWorld()->GetDeltaSeconds() / SafeAccelerationTime,
		0.0f,
		1.0f
	);

	const float SmoothAlpha = FMath::InterpEaseInOut(
		0.0f,
		1.0f,
		AimCursorAccelerationAlpha,
		2.0f
	);

	const float SmoothedValue = Value * SmoothAlpha;

	AimCursorScreenYPercent = FMath::Clamp(
		AimCursorScreenYPercent + SmoothedValue * AimCursorVerticalSensitivity,
		AimCursorMinScreenYPercent,
		AimCursorMaxScreenYPercent
	);
}

bool AThirdPersonCppCharacter::GetAimCursorScreenPosition(float& OutScreenX, float& OutScreenY) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(Controller);

	if (PlayerController == nullptr)
	{
		return false;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;

	PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return false;
	}

	OutScreenX = ViewportSizeX * 0.5f;
	OutScreenY = ViewportSizeY * AimCursorScreenYPercent;

	return true;
}

void AThirdPersonCppCharacter::ApplyCameraYaw(float NewYaw)
{
	if (Controller == nullptr)
	{
		return;
	}

	FRotator NewControlRotation = Controller->GetControlRotation();

	NewControlRotation.Pitch = 0.0f;
	NewControlRotation.Roll = 0.0f;
	NewControlRotation.Yaw = NewYaw;

	Controller->SetControlRotation(NewControlRotation);
}

void AThirdPersonCppCharacter::StartCameraRotationToRelativeYaw(float RelativeYawDegrees)
{
	if (Controller == nullptr)
	{
		return;
	}

	CameraRotationStartYaw = Controller->GetControlRotation().Yaw;
	CameraRotationTargetYaw = CameraQuickViewBaseYaw + RelativeYawDegrees;
	CameraRotationElapsedTime = 0.0f;
	bCameraRotationTransitionActive = true;
}

void AThirdPersonCppCharacter::SetCameraRightSide()
{
	bCameraQuickViewActive = true;

	CameraQuickViewBaseYaw = GetActorRotation().Yaw;
	CameraQuickViewRelativeYaw = -90.0f;

	StartCameraRotationToRelativeYaw(CameraQuickViewRelativeYaw);
}

void AThirdPersonCppCharacter::SetCameraLeftSide()
{
	bCameraQuickViewActive = true;

	CameraQuickViewBaseYaw = GetActorRotation().Yaw;
	CameraQuickViewRelativeYaw = 90.0f;

	StartCameraRotationToRelativeYaw(CameraQuickViewRelativeYaw);
}

void AThirdPersonCppCharacter::SetCameraFront()
{
	bCameraQuickViewActive = true;

	CameraQuickViewBaseYaw = GetActorRotation().Yaw;
	CameraQuickViewRelativeYaw = 180.0f;

	StartCameraRotationToRelativeYaw(CameraQuickViewRelativeYaw);
}

void AThirdPersonCppCharacter::ResetCameraBehind()
{
	bCameraQuickViewActive = false;

	CameraQuickViewBaseYaw = GetActorRotation().Yaw;
	CameraQuickViewRelativeYaw = 0.0f;

	StartCameraRotationToRelativeYaw(0.0f);
}

void AThirdPersonCppCharacter::UpdateCameraQuickView(float DeltaTime)
{
	if (!bCameraRotationTransitionActive)
	{
		return;
	}

	const float SafeTransitionTime = FMath::Max(0.01f, CameraQuickViewTransitionTime);

	CameraRotationElapsedTime += DeltaTime;

	const float RawAlpha = FMath::Clamp(
		CameraRotationElapsedTime / SafeTransitionTime,
		0.0f,
		1.0f
	);

	// Suaviza el inicio y el final del giro.
	const float SmoothAlpha = FMath::InterpEaseInOut(
		0.0f,
		1.0f,
		RawAlpha,
		2.0f
	);

	const float DeltaYaw = FMath::FindDeltaAngleDegrees(
		CameraRotationStartYaw,
		CameraRotationTargetYaw
	);

	const float NewYaw = CameraRotationStartYaw + DeltaYaw * SmoothAlpha;

	ApplyCameraYaw(NewYaw);

	if (RawAlpha >= 1.0f)
	{
		ApplyCameraYaw(CameraRotationTargetYaw);
		bCameraRotationTransitionActive = false;
	}
}

UAnimMontage* AThirdPersonCppCharacter::SelectKickMontageForTarget(
	const FVector& TargetLocation,
	float& OutHorizontalSpeed
) const
{
	if (ControlledBall == nullptr)
	{
		return nullptr;
	}

	const FVector BallLocation = ControlledBall->GetActorLocation();

	const float Distance2D = FVector::Dist2D(BallLocation, TargetLocation);

	// Distancia corta: no usamos animaci�n especial.
	if (Distance2D <= ShortKickMaxDistance)
	{
		return nullptr;
	}

	FVector PlayerForward = GetActorForwardVector();
	PlayerForward.Z = 0.0f;
	PlayerForward.Normalize();

	FVector BallToTarget = TargetLocation - BallLocation;
	BallToTarget.Z = 0.0f;
	BallToTarget.Normalize();

	const float Dot = FMath::Clamp(
		FVector::DotProduct(PlayerForward, BallToTarget),
		-1.0f,
		1.0f
	);

	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));

	const float CrossZ = FVector::CrossProduct(PlayerForward, BallToTarget).Z;

	// Si el destino est� hacia la izquierda, preferimos pierna derecha.
	// Si est� hacia la derecha, preferimos pierna izquierda.
	const bool bUseRightLeg = CrossZ < 0.0f;

	const bool bSideKick = AngleDegrees > SideKickMinAngleDegrees;

	const bool bPlayerIsMoving =
		GetVelocity().Size2D() >= RunningKickMinSpeed;

	const bool bLongDistance =
		Distance2D >= LongKickMinDistance;

	const bool bCanUseStrike =
		bLongDistance && !bSideKick;

	// Disparo lejano recto: usamos Strike.
	if (bCanUseStrike)
	{
		return bUseRightLeg
			? StrikeRightLegForwardJogMontage
			: StrikeLeftLegForwardJogMontage;
	}

	// Si era un tiro lejano pero demasiado abierto,
	// no permitimos Strike y reducimos la fuerza.
	if (bLongDistance && bSideKick)
	{
		OutHorizontalSpeed = ForcedLongAnglePassSpeed;
	}

	// Distancia media o tiro lejano forzado a pase.
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

	// Jugador parado.
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

float AThirdPersonCppCharacter::GetDistanceAdjustedChargedKickSpeed(
	float ChargedSpeed,
	const FVector& TargetLocation
) const
{
	if (ControlledBall == nullptr)
	{
		return ChargedSpeed;
	}

	const float Distance2D = FVector::Dist2D(
		ControlledBall->GetActorLocation(),
		TargetLocation
	);

	const float SafeReferenceDistance =
		FMath::Max(1.0f, KickChargeDistanceReference);

	const float SafeDistanceMultiplier =
		FMath::Max(0.0f, KickChargeDistanceMultiplier);

	const float DistanceFactor =
		1.0f + SafeDistanceMultiplier * (Distance2D / SafeReferenceDistance);

	return FMath::Clamp(
		ChargedSpeed * DistanceFactor,
		KickChargeMinHorizontalSpeed,
		KickChargeMaxHorizontalSpeed
	);
}

void AThirdPersonCppCharacter::TurnCameraWithAcceleration(float Value)
{
	if (Controller == nullptr)
	{
		return;
	}

	const float AbsValue = FMath::Abs(Value);

	if (AbsValue <= CameraTurnInputDeadZone)
	{
		CameraTurnAccelerationAlpha = 0.0f;
		LastCameraTurnInputSign = 0.0f;
		return;
	}

	const float CurrentInputSign = Value > 0.0f ? 1.0f : -1.0f;

	if (
		LastCameraTurnInputSign != 0.0f &&
		CurrentInputSign != LastCameraTurnInputSign
		)
	{
		CameraTurnAccelerationAlpha = 0.0f;
	}

	LastCameraTurnInputSign = CurrentInputSign;

	const float SafeAccelerationTime =
		FMath::Max(0.01f, CameraTurnAccelerationTime);

	CameraTurnAccelerationAlpha = FMath::Clamp(
		CameraTurnAccelerationAlpha + GetWorld()->GetDeltaSeconds() / SafeAccelerationTime,
		0.0f,
		1.0f
	);

	const float SmoothAlpha = FMath::InterpEaseInOut(
		0.0f,
		1.0f,
		CameraTurnAccelerationAlpha,
		2.0f
	);

	AddControllerYawInput(Value * SmoothAlpha);
}

void AThirdPersonCppCharacter::ResumeDribbleMovementAfterTurn(
	const FVector& Direction,
	float ResumeSpeed
)
{
	FVector SafeDirection = Direction;
	SafeDirection.Z = 0.0f;
	SafeDirection = SafeDirection.GetSafeNormal();

	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	const float SafeResumeSpeed = FMath::Max(0.0f, ResumeSpeed);

	if (SafeResumeSpeed <= 0.0f)
	{
		return;
	}

	CurrentDribbleDirection = SafeDirection;
	DesiredDribbleDirection = SafeDirection;
	PendingDribbleInputDirection = SafeDirection;
	bHasDesiredDribbleDirection = true;

	GetCharacterMovement()->MaxWalkSpeed = SelectedMovementSpeed;

	FVector NewVelocity = SafeDirection * SafeResumeSpeed;
	NewVelocity.Z = GetCharacterMovement()->Velocity.Z;

	GetCharacterMovement()->Velocity = NewVelocity;

	AddMovementInput(SafeDirection, 1.0f);
}

bool AThirdPersonCppCharacter::ShouldForceDribbleTurnLocomotion() const
{
	if (!bForceDribbleTurnLocomotion)
	{
		return false;
	}

	if (GetWorld() == nullptr)
	{
		return false;
	}

	return GetWorld()->GetTimeSeconds() < ForcedDribbleTurnLocomotionEndTime;
}

float AThirdPersonCppCharacter::GetForcedDribbleTurnLocomotionSpeed() const
{
	if (!ShouldForceDribbleTurnLocomotion())
	{
		return 0.0f;
	}

	return ForcedDribbleTurnLocomotionSpeed;
}

void AThirdPersonCppCharacter::UpdateStrongRunDribbleTurnMovement(float DeltaTime)
{
	if (SoccerControlState != ESoccerPlayerControlState::DribbleTurning)
	{
		return;
	}

	if (ActiveStrongRunDribbleTurnDuration <= 0.0f)
	{
		return;
	}

	ActiveStrongRunDribbleTurnElapsedTime += DeltaTime;

	const float ZeroTime = FMath::Clamp(
		StrongRunDribbleTurnZeroSpeedTime,
		0.01f,
		ActiveStrongRunDribbleTurnDuration - 0.01f
	);

	float SpeedToUse = 0.0f;
	FVector DirectionToUse = ActiveStrongRunDribbleTurnInitialDirection;

	if (ActiveStrongRunDribbleTurnElapsedTime <= ZeroTime)
	{
		const float Alpha = ActiveStrongRunDribbleTurnElapsedTime / ZeroTime;

		SpeedToUse = FMath::Lerp(
			ActiveStrongRunDribbleTurnInitialSpeed,
			0.0f,
			Alpha
		);

		DirectionToUse = ActiveStrongRunDribbleTurnInitialDirection;
	}
	else
	{
		const float AccelDuration =
			FMath::Max(0.01f, ActiveStrongRunDribbleTurnDuration - ZeroTime);

		const float Alpha =
			(ActiveStrongRunDribbleTurnElapsedTime - ZeroTime) / AccelDuration;

		const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

		SpeedToUse = FMath::Lerp(
			0.0f,
			ActiveStrongRunDribbleTurnResumeSpeed,
			ClampedAlpha
		);

		DirectionToUse = ActiveStrongRunDribbleTurnDirection;
	}

	DirectionToUse.Z = 0.0f;
	DirectionToUse = DirectionToUse.GetSafeNormal();

	if (DirectionToUse.IsNearlyZero())
	{
		return;
	}

	FVector NewVelocity = DirectionToUse * SpeedToUse;
	NewVelocity.Z = GetCharacterMovement()->Velocity.Z;

	GetCharacterMovement()->Velocity = NewVelocity;
}

void AThirdPersonCppCharacter::FindMatchManager()
{
	MatchManager = nullptr;

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
	{
		MatchManager = *It;
		return;
	}
}

bool AThirdPersonCppCharacter::TryRegisterHumanKickTouchForRules()
{
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		// Si no hay MatchManager, mantenemos el comportamiento anterior.
		return true;
	}

	if (!MatchManager->TryRegisterIntentionalBallTouch(this))
	{
		return false;
	}

	MatchManager->StartAttackRunReleaseForTeam(GetTeam());

	return true;
}

ASoccerAICharacter* AThirdPersonCppCharacter::GetOpponentPossessingAICharacter()
{
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		return nullptr;
	}

	ASoccerCharacterBase* PossessingCharacter =
		MatchManager->GetPossessingCharacter();

	if (!IsValid(PossessingCharacter))
	{
		return nullptr;
	}

	if (PossessingCharacter == this)
	{
		return nullptr;
	}

	if (PossessingCharacter->GetTeam() == GetTeam())
	{
		return nullptr;
	}

	ASoccerAICharacter* PossessingAICharacter =
		Cast<ASoccerAICharacter>(PossessingCharacter);

	if (!IsValid(PossessingAICharacter))
	{
		return nullptr;
	}

	if (!PossessingAICharacter->IsAIPossessingBall())
	{
		return nullptr;
	}

	// Una pelota asegurada por el arquero no ofrece una acci�n
	// de robo al jugador humano.
	if (PossessingAICharacter->IsGoalkeeperHoldingBall())
	{
		return nullptr;
	}

	return PossessingAICharacter;
}

void AThirdPersonCppCharacter::StartHumanStealAttempt(
	ASoccerAICharacter* TargetAICharacter
)
{
	if (!IsHumanBallActionAllowedNow())
	{
		ClearBallActionsForMatchRestriction();
		return;
	}

	if (!IsValid(TargetAICharacter))
	{
		return;
	}

	if (TargetAICharacter->IsGoalkeeperHoldingBall())
	{
		return;
	}

	ActivateHumanBallClaim();

	bIsHumanStealAttemptActive = true;
	HumanStealTargetAICharacter = TargetAICharacter;

	HumanStealAttemptStartTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	PendingKickMode = ESoccerPendingKickMode::None;
	PendingKickTarget = FVector::ZeroVector;
	bHasPendingKickHorizontalSpeedOverride = false;
	PendingKickHorizontalSpeedOverride = 0.0f;
	bIsChargingKickRelease = false;

	HideAutoPassTargetMarker();
	HideReleaseTargetMarker();

	ClearBallPursuitTarget();
	EnterChasingBall();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.0f,
			FColor::Yellow,
			TEXT("Intento de robo iniciado")
		);
	}
}

void AThirdPersonCppCharacter::UpdateHumanStealAttempt()
{
	if (!bIsHumanStealAttemptActive)
	{
		return;
	}

	if (!IsValid(HumanStealTargetAICharacter))
	{
		FailHumanStealAttempt();
		return;
	}

	if (!HumanStealTargetAICharacter->IsAIPossessingBall())
	{
		FailHumanStealAttempt();
		return;
	}

	if (HumanStealTargetAICharacter->IsGoalkeeperHoldingBall())
	{
		FailHumanStealAttempt();
		return;
	}

	if (HumanStealTargetAICharacter->GetTeam() == GetTeam())
	{
		FailHumanStealAttempt();
		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		FailHumanStealAttempt();
		return;
	}

	ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

	if (!IsValid(SoccerBall))
	{
		FailHumanStealAttempt();
		return;
	}

	MoveTowardHumanStealTarget();

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	const float ElapsedTime = CurrentTime - HumanStealAttemptStartTime;

	if (ElapsedTime > HumanStealMaxAttemptTime)
	{
		FailHumanStealAttempt();
		return;
	}

	const float DistanceToBall = FVector::Dist2D(
		GetActorLocation(),
		SoccerBall->GetActorLocation()
	);

	if (DistanceToBall > HumanStealSuccessDistance)
	{
		return;
	}

	const bool bStealSucceeded =
		FMath::FRand() <= HumanStealSuccessChance;

	if (!bStealSucceeded)
	{
		FailHumanStealAttempt();
		return;
	}

	CompleteHumanStealAttempt(
		HumanStealTargetAICharacter,
		SoccerBall
	);
}

void AThirdPersonCppCharacter::MoveTowardHumanStealTarget()
{
	if (!IsValid(MatchManager))
	{
		return;
	}

	ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

	if (!IsValid(SoccerBall))
	{
		return;
	}

	FVector PursuitLocation = SoccerBall->GetActorLocation();
	ResolveStableBallPursuitTarget(
		SoccerBall,
		PursuitLocation
	);

	PursuitLocation.Z = GetActorLocation().Z;

	FVector DirectionToPursuitTarget =
		PursuitLocation - GetActorLocation();

	DirectionToPursuitTarget.Z = 0.0f;

	if (DirectionToPursuitTarget.IsNearlyZero())
	{
		return;
	}

	AddMovementInput(
		DirectionToPursuitTarget.GetSafeNormal(),
		1.0f
	);
}

void AThirdPersonCppCharacter::CompleteHumanStealAttempt(
	ASoccerAICharacter* TargetAICharacter,
	ASoccerBall* SoccerBall
)
{
	if (!IsValid(TargetAICharacter) || !IsValid(SoccerBall))
	{
		FailHumanStealAttempt();
		return;
	}

	if (
		TargetAICharacter->IsGoalkeeperHoldingBall() ||
		(
			IsValid(MatchManager) &&
			!MatchManager->CanCharacterTouchBallNow(this)
		)
		)
	{
		FailHumanStealAttempt();
		return;
	}

	const ESoccerPendingKickMode SavedPendingKickMode = PendingKickMode;
	const FVector SavedPendingKickTarget = PendingKickTarget;
	const bool bSavedHasPendingKickHorizontalSpeedOverride =
		bHasPendingKickHorizontalSpeedOverride;
	const float SavedPendingKickHorizontalSpeedOverride =
		PendingKickHorizontalSpeedOverride;

	// El bot suelta la pelota.
	TargetAICharacter->ReleaseAIBall();

	// Limpiamos solo el estado de intento de robo.
	ClearHumanStealAttemptOnly();

	// Tu PossessBall() no recibe par�metros:
	// primero asignamos la pelota al ControlledBall del humano.
	ControlledBall = SoccerBall;

	if (ControlledBall != nullptr)
	{
		ControlledBall->SetPossessed(true);
	}

	// Ahora s�, usamos tu PossessBall() existente.
	PossessBall();

	UpdatePossessedBallLocation();

	// PossessBall() limpia PendingKickMode, as� que restauramos
	// el destino que el jugador pudo haber marcado durante el robo.
	PendingKickMode = SavedPendingKickMode;
	PendingKickTarget = SavedPendingKickTarget;
	bHasPendingKickHorizontalSpeedOverride =
		bSavedHasPendingKickHorizontalSpeedOverride;
	PendingKickHorizontalSpeedOverride =
		SavedPendingKickHorizontalSpeedOverride;

	// Si durante la carrera de robo el usuario marc� autopase,
	// ejecutamos el pase apenas roba.
	if (
		PendingKickMode == ESoccerPendingKickMode::KickAndFollow ||
		PendingKickMode == ESoccerPendingKickMode::KickAndRelease
		)
	{
		ExecutePendingKick();
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.0f,
			FColor::Green,
			TEXT("Robo exitoso")
		);
	}
}

void AThirdPersonCppCharacter::FailHumanStealAttempt()
{
	ClearHumanStealAttemptOnly();

	ClearPendingKickAfterFailedSteal();

	EnterManualControl();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.0f,
			FColor::Red,
			TEXT("Robo fallido")
		);
	}
}

void AThirdPersonCppCharacter::ClearHumanStealAttemptOnly()
{
	bIsHumanStealAttemptActive = false;
	HumanStealTargetAICharacter = nullptr;
	HumanStealAttemptStartTime = 0.0f;
	ClearBallPursuitTarget();
}

void AThirdPersonCppCharacter::ClearPendingKickAfterFailedSteal()
{
	PendingKickMode = ESoccerPendingKickMode::None;
	PendingKickTarget = FVector::ZeroVector;

	bHasPendingKickHorizontalSpeedOverride = false;
	PendingKickHorizontalSpeedOverride = 0.0f;

	bIsChargingKickRelease = false;

	HideAutoPassTargetMarker();
	HideReleaseTargetMarker();
}

bool AThirdPersonCppCharacter::IsBallAtPlayablePossessionHeight(
	const ASoccerBall* SoccerBall
) const
{
	if (!IsValid(SoccerBall))
	{
		return false;
	}

	const UCapsuleComponent* CharacterCapsule = GetCapsuleComponent();

	if (CharacterCapsule == nullptr)
	{
		return false;
	}

	const float CharacterGroundZ =
		GetActorLocation().Z -
		CharacterCapsule->GetScaledCapsuleHalfHeight();

	const float BallHeightFromGround =
		SoccerBall->GetActorLocation().Z - CharacterGroundZ;

	return
		BallHeightFromGround >= -20.0f &&
		BallHeightFromGround <= BallPossessionMaxHeight;
}
