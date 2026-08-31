#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "SoccerDirectorTechnicalIdButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSoccerDirectorTechnicalIdClicked,
    FName,
    Identifier
);

/** Small native UMG helper: a button that reports the stable ID it represents. */
UCLASS()
class THIRDPERSONCPP_API USoccerDirectorTechnicalIdButton : public UButton
{
    GENERATED_BODY()

public:
    void SetIdentifier(FName InIdentifier);
    FName GetIdentifier() const;

    UPROPERTY(BlueprintAssignable)
    FSoccerDirectorTechnicalIdClicked OnIdentifierClicked;

private:
    UFUNCTION()
    void HandleButtonClicked();

    UPROPERTY()
    FName Identifier = NAME_None;
};
