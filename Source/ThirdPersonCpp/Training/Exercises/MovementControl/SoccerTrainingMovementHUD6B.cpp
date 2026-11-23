#include "Training/Exercises/MovementControl/SoccerTrainingMovementHUD6B.h"
#include "Training/Exercises/MovementControl/SoccerTrainingMovementGameMode6B.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "ThirdPersonCppCharacter.h"

void ASoccerTrainingMovementHUD6B::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GetWorld() || !GEngine) return;
    const ASoccerTrainingMovementGameMode6B* Mode =
        Cast<ASoccerTrainingMovementGameMode6B>(GetWorld()->GetAuthGameMode());
    if (!Mode || !Mode->HasExerciseStarted6H() || Mode->IsExerciseComplete6B()) return;
    UFont* Font = GEngine->GetSmallFont();
    if (!Font) return;

    const float X = 32.f;
    const float Y = 40.f;
    DrawRect(FLinearColor(0.015f, 0.025f, 0.035f, 0.78f), X - 12.f, Y - 12.f, 1040.f, 260.f);
    DrawText(Mode->IsZoneFinishLesson6AH() ? TEXT("ENTRENAMIENTO | DEFINICIÓN POR ZONAS") : Mode->IsGoalkeeperShotLesson6T() ? TEXT("ENTRENAMIENTO | REMATES CONTRA ARQUERO") : Mode->IsChargedShotLesson6M() ? TEXT("ENTRENAMIENTO | REMATE CON CARGA") : Mode->IsAutoPassLesson6J() ? TEXT("ENTRENAMIENTO | AUTOPASE")
        : TEXT("ENTRENAMIENTO | MOVIMIENTO Y CONTROL"), FLinearColor::White, X, Y, Font, 1.25f, false);
    const FString Progress = Mode->IsChargedShotLesson6M()
        ? FString::Printf(TEXT("Intento %d / %d  |  Goles %d  |  Carga %.0f%%"),
            Mode->GetShotAttempt6N(), Mode->GetShotAttemptsTotal6N(), Mode->GetGoalsScored6N(), Mode->GetShotCharge6M() * 100.f)
        : Mode->IsExerciseComplete6B()
        ? TEXT("Recorrido terminado: 4/4")
        : FString::Printf(TEXT("Punto de control: %d / %d"),
            Mode->GetCheckpointIndex6B() + 1, Mode->GetCheckpointCount6B());
    DrawText(Progress, FLinearColor::Yellow, X, Y + 32.f, Font, 1.2f, false);
    DrawText(FString::Printf(TEXT("Tiempo: %.1f s   Perdidas de control: %d"),
        Mode->GetElapsedTime6B(), Mode->GetLostControlCount6B()),
        FLinearColor::White, X, Y + 61.f, Font, 1.05f, false);
    DrawText(Mode->GetExerciseMessage6B(), FLinearColor(0.7f, 0.95f, 1.f),
        X, Y + 91.f, Font, 1.0f, false);
    DrawText(Mode->GetControlDiagnostics6D(), FLinearColor(0.75f, 1.f, 0.7f),
        X, Y + 120.f, Font, 0.95f, false);
    DrawText(Mode->GetSpeedDiagnostics6E(), FLinearColor(0.85f, 0.95f, 1.f),
        X, Y + 148.f, Font, 0.95f, false);
    DrawText(Mode->GetBallDiagnostics6E(), FLinearColor(0.85f, 0.95f, 1.f),
        X, Y + 177.f, Font, 0.95f, false);
    if (Mode->IsChargedShotLesson6M())
    {
        if (const AThirdPersonCppCharacter* Shooter = Cast<AThirdPersonCppCharacter>(
            UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
        {
            float Charge = 0.f;
            if (Shooter->GetKickChargePercent(Charge))
            {
                // Same amber progress bar and charge source as GameHUD in matches.
                const float ClampedCharge = FMath::Clamp(Charge, 0.f, 1.f);
                DrawText(FString::Printf(TEXT("FUERZA: %.0f%%"), ClampedCharge * 100.f),
                    FLinearColor::Yellow, X, Y + 205.f, Font, 1.2f, false);
                const float BarX = X + 180.f;
                const float BarY = Y + 213.f;
                const float BarWidth = 260.f;
                const float BarHeight = 12.f;
                DrawRect(FLinearColor(0.08f, 0.13f, 0.16f, 1.f),
                    BarX, BarY, BarWidth, BarHeight);
                DrawRect(FLinearColor(1.f, 0.69f, 0.19f, 1.f),
                    BarX, BarY, BarWidth * ClampedCharge, BarHeight);
            }
        }
    }

    // Same green crosshair as GameHUD during a match. This is a virtual
    // reticle, not the operating system mouse cursor; X is always centered.
    if ((Mode->IsAutoPassLesson6J() || Mode->IsChargedShotLesson6M()) && Mode->IsExerciseRunning6H())
    {
        const AThirdPersonCppCharacter* Trainee = Cast<AThirdPersonCppCharacter>(
            UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
        if (IsValid(Trainee))
        {
            float AimScreenX = 0.0f;
            float AimScreenY = 0.0f;
            if (Trainee->GetAimCursorScreenPosition(AimScreenX, AimScreenY))
            {
                const float CursorSize = 10.0f;
                DrawLine(AimScreenX - CursorSize * 6.0f, AimScreenY,
                    AimScreenX + CursorSize * 6.0f, AimScreenY,
                    FLinearColor::Green, 2.0f);
                DrawLine(AimScreenX, AimScreenY - CursorSize,
                    AimScreenX, AimScreenY + CursorSize,
                    FLinearColor::Green, 2.0f);
            }
        }
    }
}
