//SoccerGoalTrigger.cpp

#include "SoccerGoalTrigger.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "SoccerBall.h"
#include "SoccerFieldDimensions.h"
#include "SoccerField.h"
#include "SoccerMatchManager.h"
#include "SoccerDebugManager.h"

ASoccerGoalTrigger::ASoccerGoalTrigger()
{
	PrimaryActorTick.bCanEverTick = true;

	GoalDetectionHalfWidth = SoccerFieldDimensions::GoalHalfWidthCm;
	GoalDetectionHeight = SoccerFieldDimensions::GoalHeightCm;
	GoalCenterY = SoccerFieldDimensions::CenterY;
	GoalBaseZ = 0.0f;

	GoalRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GoalRoot"));
	RootComponent = GoalRoot;

	GoalBox = CreateDefaultSubobject<UBoxComponent>(TEXT("GoalBox"));
	GoalBox->SetupAttachment(GoalRoot);

	GoalBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GoalBox->SetCollisionObjectType(ECC_WorldDynamic);
	GoalBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	GoalBox->SetGenerateOverlapEvents(false);
	GoalBox->SetHiddenInGame(true);

	UpdateGoalBoxVisualization();
}

void ASoccerGoalTrigger::BeginPlay()
{
	Super::BeginPlay();

	FindMatchManager();

	ApplyAutomaticGoalPlacement();
	UpdateGoalBoxVisualization();

	if (GoalBox != nullptr)
	{
		if (bUseOverlapGoalFallback)
		{
			GoalBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			GoalBox->SetGenerateOverlapEvents(true);

			GoalBox->OnComponentBeginOverlap.AddDynamic(
				this,
				&ASoccerGoalTrigger::OnGoalBoxBeginOverlap
			);
		}
		else
		{
			GoalBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GoalBox->SetGenerateOverlapEvents(false);
		}
	}
}

void ASoccerGoalTrigger::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyAutomaticGoalPlacement();
	UpdateGoalBoxVisualization();
}

void ASoccerGoalTrigger::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bUseGoalLineCrossingDetection)
	{
		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		ResetGoalLineTracking();
		return;
	}

	if (MatchManager->GetMatchPlayState() != ESoccerMatchPlayState::Playing)
	{
		ResetGoalLineTracking();
		return;
	}

	ASoccerBall* SoccerBall = GetTrackedSoccerBall();

	if (!IsValid(SoccerBall))
	{
		ResetGoalLineTracking();
		return;
	}

	const FVector CurrentBallLocation =
		SoccerBall->GetActorLocation();

	if (TrackedBall.Get() != SoccerBall)
	{
		TrackedBall = SoccerBall;
		PreviousBallLocation = CurrentBallLocation;
		bHasPreviousBallLocation = true;
		bGoalLockedUntilBallReturnsToField = false;
		return;
	}

	if (!bHasPreviousBallLocation)
	{
		PreviousBallLocation = CurrentBallLocation;
		bHasPreviousBallLocation = true;
		return;
	}

	if (bGoalLockedUntilBallReturnsToField)
	{
		if (IsBallBackOnFieldSide(CurrentBallLocation))
		{
			bGoalLockedUntilBallReturnsToField = false;
		}

		PreviousBallLocation = CurrentBallLocation;
		return;
	}

	FVector CrossingLocation = FVector::ZeroVector;

	const bool bDetectedGoal =
		TryDetectGoalLineCrossing(
			PreviousBallLocation,
			CurrentBallLocation,
			CrossingLocation
		);

	if (ASoccerDebugManager::IsWorldDrawingEnabled(this, ESoccerDebugCategory::MatchRules))
	{
		DrawGoalDetectionDebug(
			PreviousBallLocation,
			CurrentBallLocation,
			CrossingLocation,
			bDetectedGoal
		);
	}

	if (bDetectedGoal)
	{
		HandleDetectedGoal(
			TEXT("GoalLineCrossing"),
			CrossingLocation
		);
	}

	PreviousBallLocation = CurrentBallLocation;
}

void ASoccerGoalTrigger::FindMatchManager()
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


const ASoccerField* ASoccerGoalTrigger::ResolveSoccerField() const
{
	if (IsValid(MatchManager))
	{
		const ASoccerField* SoccerField = MatchManager->GetSoccerField();
		if (IsValid(SoccerField))
		{
			return SoccerField;
		}
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<ASoccerField> It(World); It; ++It)
	{
		const ASoccerField* SoccerField = *It;
		if (IsValid(SoccerField))
		{
			return SoccerField;
		}
	}

	return nullptr;
}

FVector ASoccerGoalTrigger::WorldToGoalGeometryLocation(
	const FVector& WorldLocation
) const
{
	if (bUseFieldDimensionsGoalPlane)
	{
		const ASoccerField* SoccerField = ResolveSoccerField();
		if (IsValid(SoccerField))
		{
			return SoccerField->WorldToPitchLocal(WorldLocation);
		}
	}

	return WorldLocation;
}

FVector ASoccerGoalTrigger::GoalGeometryToWorldLocation(
	const FVector& GeometryLocation
) const
{
	if (bUseFieldDimensionsGoalPlane)
	{
		const ASoccerField* SoccerField = ResolveSoccerField();
		if (IsValid(SoccerField))
		{
			return SoccerField->PitchLocalToWorld(GeometryLocation);
		}
	}

	return GeometryLocation;
}

ASoccerBall* ASoccerGoalTrigger::GetTrackedSoccerBall()
{
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	return IsValid(MatchManager)
		? MatchManager->GetSoccerBall()
		: nullptr;
}

void ASoccerGoalTrigger::ResetGoalLineTracking()
{
	TrackedBall = nullptr;
	bHasPreviousBallLocation = false;
	PreviousBallLocation = FVector::ZeroVector;
	bGoalLockedUntilBallReturnsToField = false;
}

bool ASoccerGoalTrigger::TryDetectGoalLineCrossing(
	const FVector& PreviousLocation,
	const FVector& CurrentLocation,
	FVector& OutCrossingLocation
) const
{
	OutCrossingLocation = FVector::ZeroVector;

	const FVector PreviousGeometryLocation =
		WorldToGoalGeometryLocation(PreviousLocation);
	const FVector CurrentGeometryLocation =
		WorldToGoalGeometryLocation(CurrentLocation);
	const float DeltaX =
		CurrentGeometryLocation.X - PreviousGeometryLocation.X;

	if (FMath::Abs(DeltaX) <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float GoalLineX = GetGoalLineX();
	const float DirectionSign = GetGoalScoringDirectionSign();
	const float PreviousSignedDistance =
		(PreviousGeometryLocation.X - GoalLineX) * DirectionSign;
	const float CurrentSignedDistance =
		(CurrentGeometryLocation.X - GoalLineX) * DirectionSign;

	if (PreviousSignedDistance > RequiredBallCenterDepthBeyondLine)
	{
		return false;
	}
	if (CurrentSignedDistance < RequiredBallCenterDepthBeyondLine)
	{
		return false;
	}

	const float CrossingAlpha =
		(GoalLineX - PreviousGeometryLocation.X) / DeltaX;

	if (CrossingAlpha < 0.0f || CrossingAlpha > 1.0f)
	{
		return false;
	}

	OutCrossingLocation = FMath::Lerp(
		PreviousLocation,
		CurrentLocation,
		CrossingAlpha
	);

	return IsLocationInsideGoalMouth(OutCrossingLocation);
}

bool ASoccerGoalTrigger::IsLocationInsideGoalMouth(
	const FVector& Location
) const
{
	const FVector GeometryLocation = WorldToGoalGeometryLocation(Location);
	const float AllowedHalfWidth = FMath::Max(
		0.0f,
		GoalDetectionHalfWidth + GoalDetectionSideMargin
	);
	const float LateralDistance = FMath::Abs(
		GeometryLocation.Y - GetGoalCenterY()
	);

	if (LateralDistance > AllowedHalfWidth)
	{
		return false;
	}

	const float ResolvedGoalBaseZ = GetGoalBaseZ();
	const float MinGoalZ = ResolvedGoalBaseZ - GoalDetectionBottomMargin;
	const float MaxGoalZ =
		ResolvedGoalBaseZ + GoalDetectionHeight + GoalDetectionTopMargin;

	return
		GeometryLocation.Z >= MinGoalZ &&
		GeometryLocation.Z <= MaxGoalZ;
}

bool ASoccerGoalTrigger::IsLocationPastGoalLineAndInsideGoal(
	const FVector& Location
) const
{
	const FVector GeometryLocation = WorldToGoalGeometryLocation(Location);
	const float SignedDistance =
		(GeometryLocation.X - GetGoalLineX()) * GetGoalScoringDirectionSign();

	if (SignedDistance < RequiredBallCenterDepthBeyondLine)
	{
		return false;
	}

	return IsLocationInsideGoalMouth(Location);
}

bool ASoccerGoalTrigger::IsBallBackOnFieldSide(
	const FVector& BallLocation
) const
{
	const FVector GeometryLocation = WorldToGoalGeometryLocation(BallLocation);
	const float SignedDistance =
		(GeometryLocation.X - GetGoalLineX()) * GetGoalScoringDirectionSign();

	return SignedDistance <= -FMath::Abs(GoalLineRearmFieldSideDistance);
}

void ASoccerGoalTrigger::HandleDetectedGoal(
	const FString& DetectionSource,
	const FVector& DetectionLocation
)
{
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		return;
	}

	if (MatchManager->GetMatchPlayState() != ESoccerMatchPlayState::Playing)
	{
		return;
	}

	UWorld* World = GetWorld();
	const float CurrentTime =
		World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	if (CurrentTime - LastGoalScoredTime < GoalScoreCooldown)
	{
		return;
	}

	LastGoalScoredTime = CurrentTime;
	bGoalLockedUntilBallReturnsToField = true;

	const ESoccerTeam ResolvedScoringTeam = ResolveScoringTeam();

	if (GEngine)
	{
		const FString DebugText = FString::Printf(
			TEXT("Goal detected by %s | Team %d | Cross %.0f %.0f %.0f"),
			*DetectionSource,
			static_cast<int32>(ResolvedScoringTeam),
			DetectionLocation.X,
			DetectionLocation.Y,
			DetectionLocation.Z
		);

		GEngine->AddOnScreenDebugMessage(
			-1,
			1.2f,
			FColor::Green,
			DebugText
		);
	}

	MatchManager->HandleGoalScored(ResolvedScoringTeam);
}

ESoccerTeam ASoccerGoalTrigger::ResolveScoringTeam() const
{
	if (!IsValid(MatchManager))
	{
		return ScoringTeam;
	}

	const float ThisGoalLineSign =
		bGoalIsOnRightSideOfField ? 1.0f : -1.0f;

	const float PlayerOwnGoalSign =
		MatchManager->GetOwnGoalLineSign(ESoccerTeam::PlayerTeam);

	if (FMath::IsNearlyEqual(ThisGoalLineSign, PlayerOwnGoalSign))
	{
		return ESoccerTeam::OpponentTeam;
	}

	const float OpponentOwnGoalSign =
		MatchManager->GetOwnGoalLineSign(ESoccerTeam::OpponentTeam);

	if (FMath::IsNearlyEqual(ThisGoalLineSign, OpponentOwnGoalSign))
	{
		return ESoccerTeam::PlayerTeam;
	}

	// Defensive fallback for malformed side configuration. Existing levels keep
	// their authored behavior instead of silently awarding a goal incorrectly.
	return ScoringTeam;
}

float ASoccerGoalTrigger::GetGoalLineX() const
{
	if (bUseFieldDimensionsGoalPlane)
	{
		const float GoalLineSign = bGoalIsOnRightSideOfField ? 1.0f : -1.0f;
		return SoccerFieldDimensions::GetGoalLineX(GoalLineSign) +
			GoalLineLocalXOffset;
	}

	return GetActorLocation().X + GoalLineLocalXOffset;
}

float ASoccerGoalTrigger::GetGoalCenterY() const
{
	return bUseFieldDimensionsGoalPlane
		? GoalCenterY
		: GetActorLocation().Y;
}

float ASoccerGoalTrigger::GetGoalBaseZ() const
{
	return bUseFieldDimensionsGoalPlane
		? GoalBaseZ
		: GetActorLocation().Z + GoalDetectionBaseZOffset;
}

bool ASoccerGoalTrigger::IsRightSideGoal() const
{
	if (bUseFieldDimensionsGoalPlane)
	{
		return bGoalIsOnRightSideOfField;
	}

	if (bInferGoalSideFromWorldX)
	{
		return GetGoalLineX() >= 0.0f;
	}

	return bGoalIsOnRightSideOfField;
}

float ASoccerGoalTrigger::GetGoalScoringDirectionSign() const
{
	return IsRightSideGoal() ? 1.0f : -1.0f;
}

void ASoccerGoalTrigger::ApplyAutomaticGoalPlacement()
{
	if (!bUseFieldDimensionsGoalPlane || !bAutoMoveActorToGoalLine)
	{
		return;
	}

	const FVector DesiredGeometryLocation(
		GetGoalLineX(),
		GoalCenterY,
		GoalBaseZ
	);
	const FVector DesiredWorldLocation =
		GoalGeometryToWorldLocation(DesiredGeometryLocation);

	const ASoccerField* SoccerField = ResolveSoccerField();
	const FRotator DesiredRotation = IsValid(SoccerField)
		? SoccerField->GetActorRotation()
		: GetActorRotation();

	if (
		!GetActorLocation().Equals(DesiredWorldLocation, 0.1f) ||
		!GetActorRotation().Equals(DesiredRotation, 0.1f)
		)
	{
		SetActorLocationAndRotation(
			DesiredWorldLocation,
			DesiredRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
	}
}

void ASoccerGoalTrigger::UpdateGoalBoxVisualization()
{
	if (GoalBox == nullptr)
	{
		return;
	}

	const float HalfWidth = FMath::Max(
		1.0f,
		GoalDetectionHalfWidth + GoalDetectionSideMargin
	);
	GoalBox->SetBoxExtent(
		FVector(
			20.0f,
			HalfWidth,
			FMath::Max(1.0f, GoalDetectionHeight * 0.5f)
		)
	);

	const FVector BoxCenterGeometry(
		GetGoalLineX(),
		GetGoalCenterY(),
		GetGoalBaseZ() + GoalDetectionHeight * 0.5f
	);
	const FVector BoxCenterWorld =
		GoalGeometryToWorldLocation(BoxCenterGeometry);
	GoalBox->SetWorldLocation(BoxCenterWorld);

	if (bUseFieldDimensionsGoalPlane)
	{
		const ASoccerField* SoccerField = ResolveSoccerField();
		if (IsValid(SoccerField))
		{
			GoalBox->SetWorldRotation(SoccerField->GetActorRotation());
		}
	}

	GoalBox->ShapeColor = FColor::Green;
}

void ASoccerGoalTrigger::DrawGoalDetectionDebug(
	const FVector& PreviousLocation,
	const FVector& CurrentLocation,
	const FVector& CrossingLocation,
	bool bDetectedGoal
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	const float GoalLineX = GetGoalLineX();
	const float HalfWidth = FMath::Max(
		0.0f,
		GoalDetectionHalfWidth + GoalDetectionSideMargin
	);
	const float ResolvedGoalBaseZ = GetGoalBaseZ();
	const float ResolvedGoalCenterY = GetGoalCenterY();

	const FVector BottomLeft = GoalGeometryToWorldLocation(FVector(
		GoalLineX,
		ResolvedGoalCenterY - HalfWidth,
		ResolvedGoalBaseZ
	));
	const FVector BottomRight = GoalGeometryToWorldLocation(FVector(
		GoalLineX,
		ResolvedGoalCenterY + HalfWidth,
		ResolvedGoalBaseZ
	));
	const FVector TopLeft = GoalGeometryToWorldLocation(FVector(
		GoalLineX,
		ResolvedGoalCenterY - HalfWidth,
		ResolvedGoalBaseZ + GoalDetectionHeight
	));
	const FVector TopRight = GoalGeometryToWorldLocation(FVector(
		GoalLineX,
		ResolvedGoalCenterY + HalfWidth,
		ResolvedGoalBaseZ + GoalDetectionHeight
	));

	const FColor DebugColor =
		bDetectedGoal ? FColor::Green : FColor::Yellow;

	DrawDebugLine(World, BottomLeft, BottomRight, DebugColor, false, GoalDebugDrawDuration, 0, 3.0f);
	DrawDebugLine(World, TopLeft, TopRight, DebugColor, false, GoalDebugDrawDuration, 0, 3.0f);
	DrawDebugLine(World, BottomLeft, TopLeft, DebugColor, false, GoalDebugDrawDuration, 0, 3.0f);
	DrawDebugLine(World, BottomRight, TopRight, DebugColor, false, GoalDebugDrawDuration, 0, 3.0f);

	DrawDebugLine(
		World,
		PreviousLocation,
		CurrentLocation,
		bDetectedGoal ? FColor::Green : FColor::Cyan,
		false,
		GoalDebugDrawDuration,
		0,
		2.0f
	);

	if (!CrossingLocation.IsNearlyZero())
	{
		DrawDebugSphere(
			World,
			CrossingLocation,
			18.0f,
			12,
			DebugColor,
			false,
			GoalDebugDrawDuration
		);
	}
}

void ASoccerGoalTrigger::OnGoalBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!bUseOverlapGoalFallback)
	{
		return;
	}

	ASoccerBall* SoccerBall = Cast<ASoccerBall>(OtherActor);

	if (!IsValid(SoccerBall))
	{
		return;
	}

	const FVector BallLocation =
		SoccerBall->GetActorLocation();

	// Incluso como fallback, el overlap no alcanza: validamos que la pelota
	// esté detrás de la línea y dentro de la boca del arco.
	if (!IsLocationPastGoalLineAndInsideGoal(BallLocation))
	{
		return;
	}

	HandleDetectedGoal(
		TEXT("OverlapFallback"),
		BallLocation
	);
}
