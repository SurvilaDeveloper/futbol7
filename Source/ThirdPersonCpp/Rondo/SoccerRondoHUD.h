#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SoccerRondoHUD.generated.h"
UCLASS()
class THIRDPERSONCPP_API ASoccerRondoHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
