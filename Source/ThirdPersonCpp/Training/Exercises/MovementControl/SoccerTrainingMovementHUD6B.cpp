#include "Training/Exercises/MovementControl/SoccerTrainingMovementHUD6B.h"
#include "Training/Exercises/MovementControl/SoccerTrainingMovementGameMode6B.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

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
    DrawRect(FLinearColor(0.015f, 0.025f, 0.035f, 0.78f), X - 12.f, Y - 12.f, 1040.f, 230.f);
    DrawText(TEXT("ENTRENAMIENTO | MOVIMIENTO Y CONTROL"), FLinearColor::White, X, Y, Font, 1.25f, false);
    const FString Progress = Mode->IsExerciseComplete6B()
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
}
