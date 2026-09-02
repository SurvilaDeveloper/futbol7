#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SoccerPlayerAppearanceCatalog.generated.h"

class USkeletalMesh;

/** One reusable body archetype shared by any number of player profiles. */
USTRUCT(BlueprintType)
struct FSoccerBodyVariantDefinition
{
    GENERATED_BODY()

    /** Stable identifier used by FSoccerPlayerAppearance::BodyVariantId. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Variant")
    FName BodyVariantId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Variant")
    FText DisplayName;

    /** All body meshes are expected to use the common player Skeleton/Anim BP. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Variant")
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
};

/**
 * Central content catalog for player appearance variants.
 * Profiles store stable IDs; asset references live here only once.
 */
UCLASS(BlueprintType)
class THIRDPERSONCPP_API USoccerPlayerAppearanceCatalog : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Appearance")
    FName CatalogId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Appearance")
    TArray<FSoccerBodyVariantDefinition> BodyVariants;

    /** Returns null for an empty, missing, duplicate-invalid or unloadable ID. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Player Appearance")
    USkeletalMesh* LoadBodyVariantMesh(FName BodyVariantId) const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Player Appearance")
    bool ContainsBodyVariant(FName BodyVariantId) const;
};
