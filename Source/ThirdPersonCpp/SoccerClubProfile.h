#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SoccerClubProfile.generated.h"

class UMaterialInterface;
class UTexture2D;
class USoccerCoachProfile;

UENUM(BlueprintType)
enum class ESoccerClubKitType : uint8
{
    Home UMETA(DisplayName = "Home"),
    Away UMETA(DisplayName = "Away"),
    Goalkeeper UMETA(DisplayName = "Goalkeeper")
};

/** Materials that belong to a club kit, never to an individual player. */
USTRUCT(BlueprintType)
struct FSoccerClubKitDefinition
{
    GENERATED_BODY()

    /** Stable content ID, for example KIT_HOME_2026. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Club Kit")
    FName KitId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Club Kit")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Club Kit")
    TSoftObjectPtr<UMaterialInterface> ShirtMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Club Kit")
    TSoftObjectPtr<UMaterialInterface> ShortsMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Club Kit")
    TSoftObjectPtr<UMaterialInterface> SocksMaterial;

    /** True when the three required outfield material references are assigned. */
    bool IsComplete() const;
};

/**
 * Immutable/content-side identity of one football club.
 * Players, contracts, current roster and match lineup intentionally live in
 * separate data so a person can change clubs without changing PlayerProfile.
 */
UCLASS(BlueprintType)
class THIRDPERSONCPP_API USoccerClubProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Stable identity used by saves and future competitions. Example: CLUB_BLUE. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Club | Identity")
    FName ClubId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Club | Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Club | Identity")
    FText ShortName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Club | Identity")
    TSoftObjectPtr<UTexture2D> Crest;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Club | Colors")
    FLinearColor PrimaryColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Club | Colors")
    FLinearColor SecondaryColor = FLinearColor::Black;

    /** Current employment relationship. The coach profile remains independent. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Club | Staff")
    USoccerCoachProfile* CurrentCoach = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Club | Kits")
    FSoccerClubKitDefinition HomeKit;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Club | Kits")
    FSoccerClubKitDefinition AwayKit;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Club | Kits")
    FSoccerClubKitDefinition GoalkeeperKit;

    UFUNCTION(BlueprintPure, Category = "Soccer Club")
    bool HasValidClubId() const;

    UFUNCTION(BlueprintPure, Category = "Soccer Club")
    bool HasCompleteKit(ESoccerClubKitType KitType) const;

    const FSoccerClubKitDefinition& GetKit(
        ESoccerClubKitType KitType
    ) const;
};
