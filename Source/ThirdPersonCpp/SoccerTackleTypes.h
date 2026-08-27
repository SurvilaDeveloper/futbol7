#pragma once

#include "CoreMinimal.h"
#include "SoccerTackleTypes.generated.h"

UENUM(BlueprintType)
enum class ESoccerTackleSide : uint8
{
	LeftLeg,
	RightLeg
};

UENUM(BlueprintType)
enum class ESoccerTacklePhase : uint8
{
	Inactive,
	Sliding,
	Recovery
};

/* Physical limb that produced the earliest registered tackle contact. */
UENUM(BlueprintType)
enum class ESoccerTackleFallSide : uint8
{
	Left,
	Right
};

UENUM(BlueprintType)
enum class ESoccerTackleFallPhase : uint8
{
	Inactive,
	Falling,
	Recovery
};

UENUM(BlueprintType)
enum class ESoccerTackleEvasionSide : uint8
{
	Left,
	Right
};

UENUM(BlueprintType)
enum class ESoccerTackleContactLimb : uint8
{
	None,
	LeftFoot,
	RightFoot,
	LeftLowerLeg,
	RightLowerLeg
};

/*
 * This describes chronology only. It deliberately does NOT decide whether
 * the challenge is legal; that belongs to the foul evaluator in a later
 * stage.
 */
UENUM(BlueprintType)
enum class ESoccerTackleContactOrder : uint8
{
	None,
	BallOnly,
	OpponentOnly,
	BallFirst,
	OpponentFirst,
	NearlySimultaneous
};

USTRUCT(BlueprintType)
struct FSoccerTackleContactSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	bool bBallContactOccurred = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	float BallContactNormalizedTime = -1.0f;


	/* True when the rules layer accepted the touch and the ball response was applied. */
	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	bool bBallContactApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	FVector IncomingBallVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	FVector OutgoingBallVelocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	bool bOpponentContactOccurred = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	float OpponentContactNormalizedTime = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	FVector OpponentContactLocation = FVector::ZeroVector;

	/* Approximate contact height above the tackler's capsule floor. */
	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	float OpponentContactHeightCm = 0.0f;

	/* Relative horizontal speed snapshot before the victim fall reaction changes movement. */
	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	float OpponentContactRelativeSpeedCmPerSec = 0.0f;

	/* True when the tackler was positioned materially behind the victim at impact. */
	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	bool bOpponentContactFromBehind = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	bool bOpponentFallReactionStarted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Soccer|Tackle|Contact")
	ESoccerTackleFallSide OpponentFallSide = ESoccerTackleFallSide::Left;
};
