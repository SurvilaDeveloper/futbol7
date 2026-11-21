#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SoccerMainMenuGameMode6A.generated.h"

class USoccerMainMenuWidget6A;

/** GameMode exclusive to SoccerMainMenuMap: no soccer match rules or pawns. */
UCLASS()
class THIRDPERSONCPP_API ASoccerMainMenuGameMode6A : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASoccerMainMenuGameMode6A();
    virtual void InitGame(const FString& MapName, const FString& Options,
        FString& ErrorMessage) override;
    virtual void BeginPlay() override;

private:
    void CreateMainMenu6A();
    bool bOpenTrainingSelectorOnStart6B = false;

    UPROPERTY(Transient)
    USoccerMainMenuWidget6A* MainMenuWidget6A = nullptr;
};
