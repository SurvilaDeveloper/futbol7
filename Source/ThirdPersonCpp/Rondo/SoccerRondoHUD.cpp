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
    if (Mode->IsFinished())
    {
        // UMG owns the final ranking and buttons. Keep the old Canvas as fallback.
        if (Mode->HasResultScreen()) return;
        const auto Ranked=Mode->GetRankedPlayers();
        const float Width=FMath::Min(620.f,Canvas->ClipX-30.f);
        const float Height=FMath::Min(370.f,Canvas->ClipY-30.f);
        const float X=(Canvas->ClipX-Width)*0.5f, Y=(Canvas->ClipY-Height)*0.5f;
        const float RowHeight=FMath::Min(31.f,(Height-90.f)/FMath::Max(1,Ranked.Num()));
        DrawRect(FLinearColor(0.015f,0.035f,0.03f,0.96f),X,Y,Width,Height);
        DrawText(TEXT("EL LOCO - RESULTADO FINAL"),FLinearColor::White,X+18.f,Y+14.f,GEngine->GetSmallFont(),1.35f);
        DrawText(TEXT("Puesto / Jugador"),FLinearColor::White,X+18.f,Y+44.f,GEngine->GetSmallFont(),1.f);
        DrawText(TEXT("Toques"),FLinearColor::White,X+Width-85.f,Y+44.f,GEngine->GetSmallFont(),1.f);
        int32 Place=0,PreviousScore=-1;
        for (int32 Index=0;Index<Ranked.Num();++Index)
        {
            auto* P=Ranked[Index];
            const int32 Score=Mode->GetTouches(P);
            if (Score!=PreviousScore) Place=Index+1;
            PreviousScore=Score;
            const float RowY=Y+66.f+RowHeight*Index;
            const bool bHuman=P==PlayerOwner->GetPawn();
            DrawRect(FLinearColor(0.10f,0.20f,0.14f,0.8f),X+10.f,RowY,Width-20.f,RowHeight-2.f);
            const FLinearColor TextColor=Place==1?FLinearColor::Yellow:bHuman?FLinearColor::Green:FLinearColor::White;
            DrawText(FString::Printf(TEXT("%d. %s%s"),Place,*ASoccerRondoGameMode::GetParticipantName(P),bHuman?TEXT(" (Vos)"):TEXT("")),
                TextColor,X+18.f,RowY+4.f,GEngine->GetSmallFont(),1.05f);
            DrawText(FString::FromInt(Score),TextColor,X+Width-72.f,RowY+4.f,GEngine->GetSmallFont(),1.05f);
        }
        return;
    }
    DrawRect(FLinearColor(0.02f,0.05f,0.04f,0.8f),20,20, FMath::Min(900.f, Canvas->ClipX-40.f),65);
    DrawText(Mode->GetStatus(), FLinearColor::White,30,30,GEngine->GetSmallFont(),1.15f);
    if (Mode->IsReady())
    {
        const auto Ranked=Mode->GetRankedPlayers();
        const float PanelWidth=FMath::Min(280.f,Canvas->ClipX*0.35f);
        const float X=Canvas->ClipX-PanelWidth-20.f,Y=92.f;
        DrawRect(FLinearColor(0.02f,0.05f,0.04f,0.76f),X,Y,PanelWidth,27.f+22.f*Ranked.Num());
        DrawText(TEXT("TOQUES (sin ser loco)"),FLinearColor::White,X+9.f,Y+5.f,GEngine->GetSmallFont(),1.f);
        for (int32 Index=0;Index<Ranked.Num();++Index)
        {
            const auto* P=Ranked[Index];
            const bool bHuman=P==PlayerOwner->GetPawn();
            const FLinearColor Color=P==Mode->GetLoco()?FLinearColor::Red:bHuman?FLinearColor::Green:FLinearColor::White;
            const float RowY=Y+27.f+22.f*Index;
            DrawText(ASoccerRondoGameMode::GetParticipantName(P),Color,X+9.f,RowY,GEngine->GetSmallFont(),1.f);
            DrawText(FString::FromInt(Mode->GetTouches(P)),Color,X+PanelWidth-36.f,RowY,GEngine->GetSmallFont(),1.f);
        }
    }
    for (auto* Player : Mode->GetPlayers())
    {
        if (!IsValid(Player)) continue;
        FVector2D Screen;
        if (PlayerOwner->ProjectWorldLocationToScreen(Player->GetActorLocation()+FVector(0,0,125),Screen))
        {
            const FString Name=ASoccerRondoGameMode::GetParticipantName(Player);
            float Width=0.f,Height=0.f;
            GetTextSize(Name,Width,Height,GEngine->GetSmallFont(),1.f);
            DrawText(Name,FLinearColor::White,Screen.X-Width*0.5f,Screen.Y,GEngine->GetSmallFont(),1.f);
        }
    }
    if (Mode->IsStartCountdownActive())
    {
        const FString Caption=TEXT("PREPARADOS");
        const FString Count=FString::FromInt(Mode->GetStartCountdownSeconds());
        float Width=0.f,Height=0.f;
        const float CX=Canvas->ClipX*0.5f,CY=Canvas->ClipY*0.5f;
        DrawRect(FLinearColor(0.015f,0.035f,0.03f,0.85f),CX-110.f,CY-68.f,220.f,136.f);
        GetTextSize(Caption,Width,Height,GEngine->GetSmallFont(),1.4f);
        DrawText(Caption,FLinearColor::White,CX-Width*0.5f,CY-48.f,GEngine->GetSmallFont(),1.4f);
        GetTextSize(Count,Width,Height,GEngine->GetSmallFont(),4.f);
        DrawText(Count,FLinearColor::Yellow,CX-Width*0.5f,CY-Height*0.5f+10.f,GEngine->GetSmallFont(),4.f);
        return;
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
