#pragma once

#include "CoreMinimal.h"
#include "SoccerPlayerProfileTypes.generated.h"

/**
 * Dominant foot is part of the player's permanent football profile.
 * It does not force every action to use that foot; gameplay can use it later
 * as a preference / quality modifier.
 */
UENUM(BlueprintType)
enum class ESoccerPreferredFoot : uint8
{
    Right UMETA(DisplayName = "Right"),
    Left UMETA(DisplayName = "Left"),
    Both UMETA(DisplayName = "Both")
};

/**
 * Natural football positions for a player profile.
 *
 * IMPORTANT: these are NOT formation slots. A future formation such as
 * 1-2-2-2 may simply have no Central Midfielder slot even if a player has
 * CentralMidfielder as a natural/preferred position.
 */
UENUM(BlueprintType)
enum class ESoccerPlayerNaturalPosition : uint8
{
    Goalkeeper UMETA(DisplayName = "Goalkeeper"),

    LeftDefender UMETA(DisplayName = "Left Defender"),
    CentralDefender UMETA(DisplayName = "Central Defender"),
    RightDefender UMETA(DisplayName = "Right Defender"),

    LeftMidfielder UMETA(DisplayName = "Left Midfielder"),
    CentralMidfielder UMETA(DisplayName = "Central Midfielder"),
    RightMidfielder UMETA(DisplayName = "Right Midfielder"),

    LeftForward UMETA(DisplayName = "Left Forward"),
    CenterForward UMETA(DisplayName = "Center Forward"),
    RightForward UMETA(DisplayName = "Right Forward")
};

/** How familiar / comfortable a player is in one natural position. */
USTRUCT(BlueprintType)
struct FSoccerPlayerPositionPreference
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position")
    ESoccerPlayerNaturalPosition Position = ESoccerPlayerNaturalPosition::CentralMidfielder;

    /** 100 = natural position, lower values = increasingly improvised. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Familiarity = 100;
};

USTRUCT(BlueprintType)
struct FSoccerPlayerIdentity
{
    GENERATED_BODY()

    /**
     * Stable identifier used by future SaveGame / lineup systems.
     * Once a profile is used by persisted data, this ID should not be renamed.
     * Example: PLAYER_0001
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName PlayerId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (ClampMin = "0", ClampMax = "99", UIMin = "0", UIMax = "99"))
    int32 ShirtNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    ESoccerPreferredFoot PreferredFoot = ESoccerPreferredFoot::Right;

    /** Metadata for future presentation / physical tuning. Not connected to gameplay yet. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (ClampMin = "120.0", ClampMax = "230.0", UIMin = "140.0", UIMax = "210.0"))
    float HeightCm = 175.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (ClampMin = "35.0", ClampMax = "150.0", UIMin = "45.0", UIMax = "120.0"))
    float WeightKg = 75.0f;
};

USTRUCT(BlueprintType)
struct FSoccerPlayerPhysicalAttributes
{
    GENERATED_BODY()

    /** Maximum running pace. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Pace = 50;

    /** How quickly the player reaches running speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Acceleration = 50;

    /** Capacity to sustain repeated running and demanding actions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Stamina = 50;

    /** How quickly energy recovers when effort is reduced. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 StaminaRecovery = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Strength = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Agility = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Balance = 50;
};

USTRUCT(BlueprintType)
struct FSoccerPlayerTechnicalAttributes
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Technical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 BallControl = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Technical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Dribbling = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Technical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 PassingAccuracy = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Technical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 ShootingAccuracy = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Technical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 ShotPower = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Technical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Tackling = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Technical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 AerialAbility = 50;
};

USTRUCT(BlueprintType)
struct FSoccerPlayerTacticalAttributes
{
    GENERATED_BODY()

    /** How quickly the player reacts to a new defensive threat / change of possession. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 DefensiveReaction = 50;

    /** Ability to read developing plays before they fully happen. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Anticipation = 50;

    /** Quality of movement into useful spaces to receive / support attacks. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 OffBallPositioning = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 DefensivePositioning = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Marking = 50;

    /** Quality of choosing among plausible football actions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 DecisionMaking = 50;

    /** Ability to execute under pressure without excessive error. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactical", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Composure = 50;
};

USTRUCT(BlueprintType)
struct FSoccerGoalkeeperAttributes
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goalkeeper", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Reflexes = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goalkeeper", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Positioning = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goalkeeper", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Handling = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goalkeeper", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Diving = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goalkeeper", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
    int32 Distribution = 50;
};

/** Permanent ability values. Current match energy/fatigue is intentionally NOT stored here. */
USTRUCT(BlueprintType)
struct FSoccerPlayerAttributes
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FSoccerPlayerPhysicalAttributes Physical;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FSoccerPlayerTechnicalAttributes Technical;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FSoccerPlayerTacticalAttributes Tactical;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    FSoccerGoalkeeperAttributes Goalkeeper;
};
