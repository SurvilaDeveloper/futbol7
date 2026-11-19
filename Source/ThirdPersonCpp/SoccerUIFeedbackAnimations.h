#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"

/**
 * Lightweight UMG presentation effects for UE 4.27 native widgets.
 * Only render opacity / render transform are changed. Layout, focus,
 * click events and gameplay state remain untouched. Tick with NativeTick
 * so transitions continue while a menu has paused the match.
 */
class FSoccerUIFeedbackAnimations
{
public:
    void StartEntrance(UWidget* Panel, float Duration = 0.24f)
    {
        RestoreFade(Entrance);
        StartFade(Entrance, Panel, Duration, 0.0f, 14.0f);
    }

    void StartPage(UWidget* Page, float Duration = 0.16f)
    {
        RestoreFade(PageChange);
        StartFade(PageChange, Page, Duration, 0.62f, 9.0f);
    }

    void StartSelection(UWidget* Selection, float Duration = 0.17f)
    {
        RestoreSelection();
        if (!IsValid(Selection)) return;
        Selected = Selection;
        SelectionTime = 0.0f;
        SelectionDuration = FMath::Max(0.01f, Duration);
        Selection->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
        Selection->SetRenderScale(FVector2D(0.96f, 0.96f));
    }

    void Tick(float DeltaSeconds)
    {
        const float Delta = FMath::Clamp(DeltaSeconds, 0.0f, 0.10f);
        TickFade(Entrance, Delta);
        TickFade(PageChange, Delta);
        if (UWidget* Target = Selected.Get())
        {
            SelectionTime += Delta;
            const float Alpha = FMath::Clamp(SelectionTime / SelectionDuration, 0.0f, 1.0f);
            const float Ease = SmoothOut(Alpha);
            const float Scale = FMath::Lerp(0.96f, 1.0f, Ease);
            Target->SetRenderScale(FVector2D(Scale, Scale));
            if (Alpha >= 1.0f)
            {
                Target->SetRenderScale(FVector2D(1.0f, 1.0f));
                Selected.Reset();
            }
        }
        else
        {
            Selected.Reset();
        }
    }

    void Reset()
    {
        RestoreFade(Entrance);
        RestoreFade(PageChange);
        RestoreSelection();
    }

private:
    struct FFade
    {
        TWeakObjectPtr<UWidget> Target;
        float Elapsed = 0.0f;
        float Duration = 0.2f;
        float FromOpacity = 0.0f;
        float FromOffsetY = 0.0f;
    };

    static float SmoothOut(float Alpha)
    {
        const float Remaining = 1.0f - Alpha;
        return 1.0f - Remaining * Remaining * Remaining;
    }

    static void RestoreFade(FFade& State)
    {
        if (UWidget* Target = State.Target.Get())
        {
            Target->SetRenderOpacity(1.0f);
            Target->SetRenderTranslation(FVector2D::ZeroVector);
        }
        State.Target.Reset();
    }

    static void StartFade(FFade& State, UWidget* Target, float Duration,
                          float FromOpacity, float FromOffsetY)
    {
        if (!IsValid(Target)) return;
        if (Duration <= KINDA_SMALL_NUMBER)
        {
            Target->SetRenderOpacity(1.0f);
            Target->SetRenderTranslation(FVector2D::ZeroVector);
            return;
        }
        State.Target = Target;
        State.Elapsed = 0.0f;
        State.Duration = Duration;
        State.FromOpacity = FromOpacity;
        State.FromOffsetY = FromOffsetY;
        Target->SetRenderOpacity(FromOpacity);
        Target->SetRenderTranslation(FVector2D(0.0f, FromOffsetY));
    }

    static void TickFade(FFade& State, float Delta)
    {
        UWidget* Target = State.Target.Get();
        if (!Target)
        {
            State.Target.Reset();
            return;
        }
        State.Elapsed += Delta;
        const float Alpha = FMath::Clamp(State.Elapsed / State.Duration, 0.0f, 1.0f);
        const float Ease = SmoothOut(Alpha);
        Target->SetRenderOpacity(FMath::Lerp(State.FromOpacity, 1.0f, Ease));
        Target->SetRenderTranslation(FVector2D(
            0.0f, FMath::Lerp(State.FromOffsetY, 0.0f, Ease)));
        if (Alpha >= 1.0f) RestoreFade(State);
    }

    void RestoreSelection()
    {
        if (UWidget* Target = Selected.Get())
        {
            Target->SetRenderScale(FVector2D(1.0f, 1.0f));
        }
        Selected.Reset();
    }

    FFade Entrance;
    FFade PageChange;
    TWeakObjectPtr<UWidget> Selected;
    float SelectionTime = 0.0f;
    float SelectionDuration = 0.17f;
};
