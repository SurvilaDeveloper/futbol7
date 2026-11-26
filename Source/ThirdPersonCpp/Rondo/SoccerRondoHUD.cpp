#include "Rondo/SoccerRondoHUD.h"
#include "Rondo/SoccerRondoGameMode.h"
#include "ThirdPersonCppCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

void ASoccerRondoHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* Mode = ASoccerRondoGameMode::Find(this);
    if (!Mode || !Canvas || !GEngine || !PlayerOwner) return;
    DrawRect(FLinearColor(0.02f,0.05f,0.04f,0.8f),20,20, FMath::Min(900.f, Canvas->ClipX-40.f),65);
    DrawText(Mode->GetStatus(), FLinearColor::White,30,30,GEngine->GetSmallFont(),1.15f);
    for (auto* Player : Mode->GetPlayers())
    {
        if (!IsValid(Player)) continue;
        const bool bLoco = Player == Mode->GetLoco();
        const bool bRisk = Player == Mode->GetAtRisk();

        FVector2D Screen;
        if (PlayerOwner->ProjectWorldLocationToScreen(Player->GetActorLocation()+FVector(0,0,125),Screen))
        {
            const FString Name=ASoccerRondoGameMode::GetParticipantName(Player);
            float Width=0.f,Height=0.f;
            GetTextSize(Name,Width,Height,GEngine->GetSmallFont(),1.f);
            DrawText(Name,FLinearColor::White,Screen.X-Width*0.5f,Screen.Y,GEngine->GetSmallFont(),1.f);
            if (bLoco || bRisk)
            {
                const FString RoleLabel=bLoco?TEXT("LOCO"):TEXT("EN RIESGO");
                GetTextSize(RoleLabel,Width,Height,GEngine->GetSmallFont(),1.1f);
                DrawText(RoleLabel,bLoco?FLinearColor::Red:FLinearColor::Yellow,Screen.X-Width*0.5f,Screen.Y-Height,GEngine->GetSmallFont(),1.1f);
            }
        }
    }
    if (auto* Human = Cast<AThirdPersonCppCharacter>(PlayerOwner->GetPawn()))
    {
        float X=0,Y=0,Charge=0;
        if (Human->GetAimCursorScreenPosition(X,Y))
        {
            DrawLine(X-35,Y,X+35,Y,FLinearColor::Green,2);
            DrawLine(X,Y-10,X,Y+10,FLinearColor::Green,2);
        }
        if (Human->GetKickChargePercent(Charge))
        {
            DrawRect(FLinearColor::Black,Canvas->ClipX*0.5f-101,Canvas->ClipY-90,202,16);
            DrawRect(FLinearColor::Green,Canvas->ClipX*0.5f-100,Canvas->ClipY-89,200*FMath::Clamp(Charge,0.f,1.f),14);
        }
    }
}
