#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "SoccerClubIdButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSoccerClubIdClicked,
    FName,
    ClubId
);

/** Reusable UMG button that reports the stable club represented by its card. */
UCLASS()
class THIRDPERSONCPP_API USoccerClubIdButton : public UButton
{
    GENERATED_BODY()

public:
    void SetClubId(FName InClubId);
    FName GetClubId() const;

    UPROPERTY(BlueprintAssignable)
    FSoccerClubIdClicked OnClubIdClicked;

private:
    UFUNCTION()
    void HandleClicked();

    UPROPERTY()
    FName ClubId = NAME_None;
};
