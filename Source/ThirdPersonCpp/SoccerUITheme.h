#pragma once

#include "CoreMinimal.h"

/**
 * Shared, optional presentation theme for the temporary in-game style editors.
 * "Original" retains the exact per-widget look. Custom local values take precedence
 * over a theme; only fields that still have their original value inherit it.
 * No gameplay data, actor state, or Blueprint asset is modified.
 */
class UWidget;

class THIRDPERSONCPP_API FSoccerUITheme
{
public:
    static FString GetActiveTheme();
    static bool SetActiveTheme(const FString& InTheme);
    static bool IsValidTheme(const FString& InTheme);
    static FLinearColor ResolveColor(const FString& Key,
                                     const FLinearColor& LocalColor,
                                     bool bLocallyCustomized);

    /** Limit sizes in logical UMG units, accounting for viewport DPI. */
    static FVector2D FitPanelToViewport(UObject* WorldContext,
                                      const FVector2D& DesiredSize,
                                      float EdgeMargin = 16.f);
    static FVector2D GetLogicalViewportSize(UObject* WorldContext);

    /** Runtime presentation helpers only: do not modify navigation/selection. */
    static float GetAdaptiveTextScale(UObject* WorldContext, float UserScale);
    static void ApplyButtonStates(UWidget* Root);
};
