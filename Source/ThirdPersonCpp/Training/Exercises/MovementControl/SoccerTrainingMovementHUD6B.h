#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SoccerTrainingMovementHUD6B.generated.h"

/** Minimal exercise-only HUD. No match scoreboards or team widgets. */
UCLASS()
class THIRDPERSONCPP_API ASoccerTrainingMovementHUD6B : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
