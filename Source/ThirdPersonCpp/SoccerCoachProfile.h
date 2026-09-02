#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SoccerFormationTypes.h"
#include "SoccerCoachProfile.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class ESoccerCoachPrimaryStyle : uint8
{
    Balanced UMETA(DisplayName = "Balanced"),
    Possession UMETA(DisplayName = "Possession"),
    DirectPlay UMETA(DisplayName = "Direct Play"),
    HighPress UMETA(DisplayName = "High Press"),
    CounterAttack UMETA(DisplayName = "Counter Attack"),
    DefensiveCompact UMETA(DisplayName = "Defensive Compact")
};

USTRUCT(BlueprintType)
struct FSoccerCoachIdentity
{
    GENERATED_BODY()

    /** Stable person ID. Example: COACH_001. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName CoachId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText FullName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText ShortName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FText Nationality;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    TSoftObjectPtr<UTexture2D> Portrait;
};

/** What kind of football the coach tries to produce. */
USTRUCT(BlueprintType)
struct FSoccerCoachPhilosophy
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy")
    ESoccerCoachPrimaryStyle PrimaryStyle = ESoccerCoachPrimaryStyle::Balanced;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy", meta = (ClampMin = "0", ClampMax = "100"))
    int32 AttackingIntent = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy", meta = (ClampMin = "0", ClampMax = "100"))
    int32 PossessionPreference = 50;

    /** 0 favors patient short play; 100 favors direct progression. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy", meta = (ClampMin = "0", ClampMax = "100"))
    int32 Directness = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy", meta = (ClampMin = "0", ClampMax = "100"))
    int32 PressingIntensity = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy", meta = (ClampMin = "0", ClampMax = "100"))
    int32 DefensiveLineHeight = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy", meta = (ClampMin = "0", ClampMax = "100"))
    int32 TeamWidth = 50;

    /** 100 means very little distance between lines and teammates. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy", meta = (ClampMin = "0", ClampMax = "100"))
    int32 Compactness = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy", meta = (ClampMin = "0", ClampMax = "100"))
    int32 Tempo = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy", meta = (ClampMin = "0", ClampMax = "100"))
    int32 CounterAttackPreference = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Philosophy", meta = (ClampMin = "0", ClampMax = "100"))
    int32 RiskTolerance = 50;
};

/** How the coach values available players while building a match squad. */
USTRUCT(BlueprintType)
struct FSoccerCoachSelectionCriteria
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Selection", meta = (ClampMin = "0", ClampMax = "100"))
    int32 NaturalPositionPriority = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Selection", meta = (ClampMin = "0", ClampMax = "100"))
    int32 TacticalFitPriority = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Selection", meta = (ClampMin = "0", ClampMax = "100"))
    int32 CurrentAbilityPriority = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Selection", meta = (ClampMin = "0", ClampMax = "100"))
    int32 PhysicalConditionPriority = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Selection", meta = (ClampMin = "0", ClampMax = "100"))
    int32 RotationPreference = 35;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Selection", meta = (ClampMin = "0", ClampMax = "100"))
    int32 YouthPreference = 40;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Selection", meta = (ClampMin = "0", ClampMax = "100"))
    int32 BenchVersatilityPreference = 65;
};

/** Coaching skill affects execution and adaptation, not the desired style itself. */
USTRUCT(BlueprintType)
struct FSoccerCoachAbilities
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities", meta = (ClampMin = "0", ClampMax = "100"))
    int32 PlayerEvaluation = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities", meta = (ClampMin = "0", ClampMax = "100"))
    int32 MatchReading = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities", meta = (ClampMin = "0", ClampMax = "100"))
    int32 TacticalFlexibility = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities", meta = (ClampMin = "0", ClampMax = "100"))
    int32 OffensiveCoaching = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities", meta = (ClampMin = "0", ClampMax = "100"))
    int32 DefensiveCoaching = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities", meta = (ClampMin = "0", ClampMax = "100"))
    int32 FatigueManagement = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities", meta = (ClampMin = "0", ClampMax = "100"))
    int32 SubstitutionTiming = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities", meta = (ClampMin = "0", ClampMax = "100"))
    int32 Composure = 60;
};

USTRUCT(BlueprintType)
struct FSoccerCoachFormationPreference
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Preference")
    ESoccerFormationSystem FormationSystem = ESoccerFormationSystem::OneThreeTwoOne;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation Preference", meta = (ClampMin = "0", ClampMax = "100"))
    int32 Preference = 50;
};

/** Permanent identity and football ideas of one director tecnico. */
UCLASS(BlueprintType)
class THIRDPERSONCPP_API USoccerCoachProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Coach | Identity")
    FSoccerCoachIdentity Identity;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Coach | Philosophy")
    FSoccerCoachPhilosophy Philosophy;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Coach | Squad Selection")
    FSoccerCoachSelectionCriteria SelectionCriteria;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Coach | Abilities")
    FSoccerCoachAbilities Abilities;

    /** Missing systems receive a neutral preference of 50. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Coach | Formations")
    TArray<FSoccerCoachFormationPreference> FormationPreferences;

    UFUNCTION(BlueprintPure, Category = "Soccer Coach")
    bool HasValidCoachId() const;

    UFUNCTION(BlueprintPure, Category = "Soccer Coach")
    int32 GetFormationPreference(ESoccerFormationSystem FormationSystem) const;
};
