#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "SoccerPlayerIdButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSoccerPlayerIdClicked, FName, PlayerId);

/** UMG button that carries its own stable profile ID. */
UCLASS()
class THIRDPERSONCPP_API USoccerPlayerIdButton : public UButton
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Soccer|Squad Viewer")
    FSoccerPlayerIdClicked OnPlayerIdClicked;

    void SetPlayerId(FName InId);

private:
    UFUNCTION()
    void BroadcastPlayerId();

    FName PlayerId = NAME_None;
};
