#include "SoccerMainMenuGameMode6A.h"
#include "SoccerMainMenuWidget6A.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ASoccerMainMenuGameMode6A::ASoccerMainMenuGameMode6A()
{
    // This map is a UI-only world. SoccerMatchManager and the match HUD do not run here.
    DefaultPawnClass = nullptr;
    HUDClass = AHUD::StaticClass();
    PlayerControllerClass = APlayerController::StaticClass();
}

void ASoccerMainMenuGameMode6A::BeginPlay()
{
    Super::BeginPlay();
    // Wait until the local PlayerController is available after map load.
    FTimerDelegate OpenMenu;
    OpenMenu.BindUObject(this, &ASoccerMainMenuGameMode6A::CreateMainMenu6A);
    GetWorldTimerManager().SetTimerForNextTick(OpenMenu);
}

void ASoccerMainMenuGameMode6A::CreateMainMenu6A()
{
    if (IsValid(MainMenuWidget6A)) return;
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!IsValid(PC))
    {
        FTimerDelegate Retry;
        Retry.BindUObject(this, &ASoccerMainMenuGameMode6A::CreateMainMenu6A);
        GetWorldTimerManager().SetTimerForNextTick(Retry);
        return;
    }

    MainMenuWidget6A = CreateWidget<USoccerMainMenuWidget6A>(
        PC, USoccerMainMenuWidget6A::StaticClass());
    if (!IsValid(MainMenuWidget6A)) return;
    MainMenuWidget6A->AddToViewport(100);
    PC->bShowMouseCursor = true;
    PC->bEnableClickEvents = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(MainMenuWidget6A->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(Mode);
    MainMenuWidget6A->FocusFirstAction6A();
    UE_LOG(LogTemp, Display, TEXT("[MainMenu6A] Main menu ready"));
}
