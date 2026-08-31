#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SoccerPlayerProfileTypes.h"
#include "SoccerPlayerProfile.generated.h"

class USkeletalMesh;
class UTexture2D;

/**
 * Optional appearance references for a player profile.
 * Team kit is intentionally not stored here: uniforms belong to the team/match,
 * while these values describe the individual player.
 */
USTRUCT(BlueprintType)
struct FSoccerPlayerAppearance
{
    GENERATED_BODY()

    /** Project-defined body archetype, for example Normal / TallThin / Short / Stocky. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FName BodyVariantId = NAME_None;

    /** Project-defined hair style identifier. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FName HairVariantId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FLinearColor SkinTint = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FLinearColor HairTint = FLinearColor(0.05f, 0.025f, 0.01f, 1.0f);

    /** Optional per-player mesh override. Leave empty when the body variant system chooses it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    TSoftObjectPtr<USkeletalMesh> MeshOverride;

    /** Optional image for future squad / manager UI. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    TSoftObjectPtr<UTexture2D> Portrait;
};

/**
 * Immutable/content-side definition of one football player.
 *
 * This asset describes who the player IS. It does not contain the current
 * formation slot, current fatigue, match state, or whether the player is a
 * starter/substitute. Those belong to future team/save/match layers.
 */
UCLASS(BlueprintType)
class THIRDPERSONCPP_API USoccerPlayerProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Player | Identity")
    FSoccerPlayerIdentity Identity;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Player | Appearance")
    FSoccerPlayerAppearance Appearance;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Player | Abilities")
    FSoccerPlayerAttributes Attributes;

    /**
     * Natural / secondary positions and familiarity in each one.
     * Formation slot legality will be handled later by the formation system.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Player | Positions")
    TArray<FSoccerPlayerPositionPreference> PositionPreferences;

    /** True when this profile has the stable ID required by future save data. */
    UFUNCTION(BlueprintPure, Category = "Soccer Player")
    bool HasValidPlayerId() const;

    /** Returns 0 when the position is not listed. Highest duplicate value wins. */
    UFUNCTION(BlueprintPure, Category = "Soccer Player")
    int32 GetPositionFamiliarity(ESoccerPlayerNaturalPosition Position) const;
};
