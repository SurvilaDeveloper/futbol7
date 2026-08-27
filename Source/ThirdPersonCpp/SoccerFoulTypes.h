#pragma once

#include "CoreMinimal.h"
#include "SoccerTeamTypes.h"
#include "SoccerTackleTypes.h"
#include "SoccerFoulTypes.generated.h"

class ASoccerCharacterBase;

UENUM(BlueprintType)
enum class ESoccerPhysicalActionType : uint8
{
    None,
    SlidingTackle
};

UENUM(BlueprintType)
enum class ESoccerFoulType : uint8
{
    None,
    SlidingChallenge,
    LateChallenge,
    DangerousChallenge
};

UENUM(BlueprintType)
enum class ESoccerFoulSeverity : uint8
{
    None,
    Careless,
    Reckless,
    ExcessiveForce
};

UENUM(BlueprintType)
enum class ESoccerFoulRestartType : uint8
{
    None,
    DirectFreeKick,
    PenaltyKick
};

/*
 * Snapshot of the physical facts surrounding a possible offence.
 * It deliberately stores observations only; the match manager decides
 * whether those facts amount to a foul and which restart would apply.
 */
USTRUCT(BlueprintType)
struct FSoccerFoulIncident
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    ASoccerCharacterBase* InstigatorCharacter = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    ASoccerCharacterBase* VictimCharacter = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    ESoccerTeam InstigatorTeam = ESoccerTeam::PlayerTeam;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    ESoccerTeam VictimTeam = ESoccerTeam::OpponentTeam;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    ESoccerPhysicalActionType PhysicalActionType = ESoccerPhysicalActionType::None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    FVector IncidentLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    float RelativeSpeedCmPerSec = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    float ContactHeightCm = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    ESoccerTackleContactOrder ContactOrder = ESoccerTackleContactOrder::None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    float BallContactNormalizedTime = -1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    float OpponentContactNormalizedTime = -1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    bool bBallContactOccurred = false;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    bool bBallContactApplied = false;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    bool bContactFromBehind = false;

};

USTRUCT(BlueprintType)
struct FSoccerFoulDecision
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    bool bIsFoul = false;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    ESoccerFoulType FoulType = ESoccerFoulType::None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    ESoccerFoulSeverity Severity = ESoccerFoulSeverity::None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    ESoccerFoulRestartType RestartType = ESoccerFoulRestartType::None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    ESoccerTeam BenefitedTeam = ESoccerTeam::PlayerTeam;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Fouls")
    FString Reason;
};
