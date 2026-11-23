#pragma once

#include "CoreMinimal.h"
#include "SoccerFieldDimensions.h"
#include "GameFramework/Actor.h"

// 6AA: Single authoritative goalkeeper positioning calculation for match and training.
// Units are centimeters, directions are world-space, and no match state is required.
struct FSoccerGoalkeeperPositioningConfig6AA
{
    bool bDynamicDepth = true;
    float MaximumDepth = 820.f;
    float FullDepthDistance = 3000.f;
    float LateralRange = 230.f;
    float BallFollowAlpha = 0.8f;
    float PostSafetyMargin = 70.f;
    float FullCoverageAngle = 55.f;
    float ResponseExponent = 0.85f;
    float NearFollowDepth = 700.f;
    float FarFollowDepth = 2200.f;
};

struct FSoccerGoalkeeperPositioningShared6AA
{
    static FVector Calculate(
        const FVector& OwnGoalCenter,
        FVector InwardDirection,
        FVector LateralDirection,
        float HomeDepthFromGoalLine,
        float CharacterZ,
        const AActor* BallActor,
        const FSoccerGoalkeeperPositioningConfig6AA& Config,
        bool bPlaying)
    {
        InwardDirection.Z = 0.f;
        LateralDirection.Z = 0.f;
        InwardDirection = InwardDirection.GetSafeNormal();
        LateralDirection = LateralDirection.GetSafeNormal();
        if (InwardDirection.IsNearlyZero()) InwardDirection = FVector::ForwardVector;
        if (LateralDirection.IsNearlyZero()) LateralDirection = FVector::RightVector;
	// HomePosition sigue definiendo la profundidad minima/de reposo, pero la
	// referencia lateral y la normal del arco proceden de ASoccerField.
	FVector GoalkeeperMoveLocation =
		OwnGoalCenter + InwardDirection * HomeDepthFromGoalLine;
	GoalkeeperMoveLocation.Z = CharacterZ;

	if (!IsValid(BallActor))
	{
		return GoalkeeperMoveLocation;
	}

	const FVector BallLocation = BallActor->GetActorLocation();
	const FVector FromGoalToBall = BallLocation - OwnGoalCenter;
	const float BallDepthFromGoalLine = FVector::DotProduct(
		FromGoalToBall,
		InwardDirection
	);

	// Si la pelota esta sobre/detras de la linea de gol, los sistemas de
	// atajada/reanudacion tienen prioridad. Como posicion base segura vuelve
	// al centro y no intenta perseguir una referencia exterior.
	if (BallDepthFromGoalLine <= 1.0f)
	{
		return GoalkeeperMoveLocation;
	}

	// Profundidad dinamica: conserva la logica anterior, ahora expresada en el
	// marco geometrico del campo en lugar de usar X mundial.
	if (
		Config.bDynamicDepth &&
		bPlaying
		)
	{
		const float MaximumDepthFromGoalLine = FMath::Max(
			HomeDepthFromGoalLine,
			Config.MaximumDepth
		);
		const float FullBallDistance = FMath::Max(
			1.0f,
			SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(
				Config.FullDepthDistance
			)
		);
		const float DistanceFactor = FMath::Clamp(
			BallDepthFromGoalLine / FullBallDistance,
			0.0f,
			1.0f
		);

		FVector ToBallDirection = FromGoalToBall;
		ToBallDirection.Z = 0.0f;
		ToBallDirection = ToBallDirection.GetSafeNormal();

		const float AngleFactor = !ToBallDirection.IsNearlyZero()
			? FMath::Clamp(
				FVector::DotProduct(ToBallDirection, InwardDirection),
				0.0f,
				1.0f
			)
			: 0.0f;
		const float DynamicDepthAlpha = DistanceFactor * AngleFactor;
		const float DynamicDepthFromGoalLine = FMath::Lerp(
			HomeDepthFromGoalLine,
			MaximumDepthFromGoalLine,
			DynamicDepthAlpha
		);

		GoalkeeperMoveLocation =
			OwnGoalCenter + InwardDirection * DynamicDepthFromGoalLine;
		GoalkeeperMoveLocation.Z = CharacterZ;
	}

	const float PostSafetyMargin = FMath::Clamp(
		Config.PostSafetyMargin,
		0.0f,
		SoccerFieldDimensions::GoalHalfWidthCm - 5.0f
	);
	const float GeometricMaximumLateralOffset =
		SoccerFieldDimensions::GoalHalfWidthCm - PostSafetyMargin;
	const float ConfiguredMaximumLateralOffset =
		FMath::Max(0.0f, Config.LateralRange);
	const float MaximumLateralOffset = FMath::Min(
		ConfiguredMaximumLateralOffset,
		GeometricMaximumLateralOffset
	);

	if (MaximumLateralOffset <= KINDA_SMALL_NUMBER)
	{
		return GoalkeeperMoveLocation;
	}

	const float BallLateralFromGoalCenter = FVector::DotProduct(
		FromGoalToBall,
		LateralDirection
	);
	const float AbsoluteBallLateral = FMath::Abs(BallLateralFromGoalCenter);

	if (AbsoluteBallLateral <= KINDA_SMALL_NUMBER)
	{
		return GoalkeeperMoveLocation;
	}

	const float BallAngleFromGoalNormalDegrees = FMath::RadiansToDegrees(
		FMath::Atan2(
			AbsoluteBallLateral,
			FMath::Max(1.0f, BallDepthFromGoalLine)
		)
	);
	const float FullCoverageAngleDegrees = FMath::Max(
		1.0f,
		Config.FullCoverageAngle
	);
	const float NormalizedCoverageAngle = FMath::Clamp(
		BallAngleFromGoalNormalDegrees / FullCoverageAngleDegrees,
		0.0f,
		1.0f
	);
	const float ResponseExponent = FMath::Max(
		0.05f,
		Config.ResponseExponent
	);
	const float CoverageResponse = FMath::Pow(
		NormalizedCoverageAngle,
		ResponseExponent
	);
	const float BaseFollowAlpha = FMath::Clamp(
		Config.BallFollowAlpha,
		0.0f,
		1.0f
	);
	const float NearDepth = FMath::Max(
		0.0f,
		Config.NearFollowDepth
	);
	const float FarDepth = FMath::Max(
		NearDepth + 1.0f,
		Config.FarFollowDepth
	);
	const float DistanceBlend = FMath::Clamp(
		(BallDepthFromGoalLine - NearDepth) /
		(FarDepth - NearDepth),
		0.0f,
		1.0f
	);
	const float EffectiveFollowAlpha = FMath::Lerp(
		1.0f,
		BaseFollowAlpha,
		DistanceBlend
	);
	const float LateralSign = BallLateralFromGoalCenter >= 0.0f
		? 1.0f
		: -1.0f;
	const float AngleBasedLateralOffset =
		LateralSign *
		MaximumLateralOffset *
		CoverageResponse *
		EffectiveFollowAlpha;

	float FinalLateralOffset = FMath::Clamp(
		AngleBasedLateralOffset,
		-MaximumLateralOffset,
		MaximumLateralOffset
	);

	// Cuando el arquero se adelanta, mezclamos la respuesta angular con la
	// bisectriz exacta pelota-postes intersectada contra SU plano de profundidad.
	if (
		Config.bDynamicDepth &&
		bPlaying
		)
	{
		const float CurrentDepthFromGoalLine = FMath::Max(
			0.0f,
			FVector::DotProduct(
				GoalkeeperMoveLocation - OwnGoalCenter,
				InwardDirection
			)
		);
		const float MaximumDepthFromGoalLine = FMath::Max(
			HomeDepthFromGoalLine,
			Config.MaximumDepth
		);
		const float DepthAdvanceRange =
			MaximumDepthFromGoalLine - HomeDepthFromGoalLine;
		const float DepthAdvanceAlpha = DepthAdvanceRange > KINDA_SMALL_NUMBER
			? FMath::Clamp(
				(CurrentDepthFromGoalLine - HomeDepthFromGoalLine) /
				DepthAdvanceRange,
				0.0f,
				1.0f
			)
			: 0.0f;

		if (
			DepthAdvanceAlpha > KINDA_SMALL_NUMBER &&
			BallDepthFromGoalLine > CurrentDepthFromGoalLine + 1.0f
			)
		{
			FVector LowerPostLocation =
				OwnGoalCenter -
				LateralDirection * SoccerFieldDimensions::GoalHalfWidthCm;
			FVector UpperPostLocation =
				OwnGoalCenter +
				LateralDirection * SoccerFieldDimensions::GoalHalfWidthCm;
			LowerPostLocation.Z = BallLocation.Z;
			UpperPostLocation.Z = BallLocation.Z;

			FVector DirectionToLowerPost = LowerPostLocation - BallLocation;
			FVector DirectionToUpperPost = UpperPostLocation - BallLocation;
			DirectionToLowerPost.Z = 0.0f;
			DirectionToUpperPost.Z = 0.0f;
			DirectionToLowerPost = DirectionToLowerPost.GetSafeNormal();
			DirectionToUpperPost = DirectionToUpperPost.GetSafeNormal();

			FVector GoalAngleBisector =
				DirectionToLowerPost + DirectionToUpperPost;
			GoalAngleBisector.Z = 0.0f;
			GoalAngleBisector = GoalAngleBisector.GetSafeNormal();

			const float BisectorDepthRate = FVector::DotProduct(
				GoalAngleBisector,
				InwardDirection
			);

			if (
				!GoalAngleBisector.IsNearlyZero() &&
				FMath::Abs(BisectorDepthRate) > KINDA_SMALL_NUMBER
				)
			{
				const float BallDepth = FVector::DotProduct(
					BallLocation - OwnGoalCenter,
					InwardDirection
				);
				const float IntersectionTime =
					(CurrentDepthFromGoalLine - BallDepth) /
					BisectorDepthRate;

				if (IntersectionTime >= 0.0f)
				{
					const FVector BisectorIntersection =
						BallLocation + GoalAngleBisector * IntersectionTime;
					const float BisectorLateralOffset = FVector::DotProduct(
						BisectorIntersection - OwnGoalCenter,
						LateralDirection
					);
					const float AdvancedMaximumLateralOffset = FMath::Lerp(
						MaximumLateralOffset,
						SoccerFieldDimensions::PenaltyAreaHalfWidthCm,
						DepthAdvanceAlpha
					);
					const float SafeBisectorLateralOffset = FMath::Clamp(
						BisectorLateralOffset,
						-AdvancedMaximumLateralOffset,
						AdvancedMaximumLateralOffset
					);

					FinalLateralOffset = FMath::Lerp(
						FinalLateralOffset,
						SafeBisectorLateralOffset,
						DepthAdvanceAlpha
					);
					FinalLateralOffset = FMath::Clamp(
						FinalLateralOffset,
						-AdvancedMaximumLateralOffset,
						AdvancedMaximumLateralOffset
					);
				}
			}
		}
	}

	const float FinalDepthFromGoalLine = FMath::Max(
		0.0f,
		FVector::DotProduct(
			GoalkeeperMoveLocation - OwnGoalCenter,
			InwardDirection
		)
	);
	GoalkeeperMoveLocation =
		OwnGoalCenter +
		InwardDirection * FinalDepthFromGoalLine +
		LateralDirection * FinalLateralOffset;
	GoalkeeperMoveLocation.Z = CharacterZ;

	return GoalkeeperMoveLocation;
    }
};
