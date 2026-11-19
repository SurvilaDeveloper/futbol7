#include "SoccerUITheme.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SoccerUIThemePrivate
{
    FString& ActiveTheme()
    {
        static FString Value;
        return Value;
    }

    bool& IsLoaded()
    {
        static bool bLoaded = false;
        return bLoaded;
    }

    FString ConfigPath()
    {
        return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SoccerUI"), TEXT("GlobalTheme.ini"));
    }

    FLinearColor PaletteColor(const FString& Theme, const FString& Role)
    {
        // Fully opaque RGB palette; caller retains the original alpha channel.
        const bool bBlue = Theme == TEXT("Azul profesional");
        const bool bGraphite = Theme == TEXT("Grafito");
        if (Role == TEXT("Backdrop"))
            return bBlue ? FLinearColor(.009f,.025f,.055f) : bGraphite
                ? FLinearColor(.015f,.018f,.022f) : FLinearColor(.010f,.027f,.023f);
        if (Role == TEXT("Panel"))
            return bBlue ? FLinearColor(.042f,.095f,.16f) : bGraphite
                ? FLinearColor(.060f,.068f,.080f) : FLinearColor(.037f,.096f,.078f);
        if (Role == TEXT("Card"))
            return bBlue ? FLinearColor(.075f,.14f,.23f) : bGraphite
                ? FLinearColor(.11f,.125f,.15f) : FLinearColor(.075f,.16f,.13f);
        if (Role == TEXT("Accent"))
            return bBlue ? FLinearColor(.13f,.49f,.79f) : bGraphite
                ? FLinearColor(.48f,.59f,.70f) : FLinearColor(.13f,.65f,.41f);
        if (Role == TEXT("Starter"))
            return bBlue ? FLinearColor(.09f,.29f,.43f) : bGraphite
                ? FLinearColor(.22f,.29f,.32f) : FLinearColor(.08f,.30f,.20f);
        if (Role == TEXT("SecondaryText"))
            return bBlue ? FLinearColor(.72f,.82f,.94f) : bGraphite
                ? FLinearColor(.79f,.82f,.86f) : FLinearColor(.73f,.87f,.81f);
        if (Role == TEXT("Pitch")) return FLinearColor(.035f,.16f,.11f);
        return FLinearColor::White;
    }

    FString RoleForKey(const FString& InKey)
    {
        const FString Key = InKey.ToLower();
        if (Key == TEXT("backdrop") || Key == TEXT("dimcolor")) return TEXT("Backdrop");
        if (Key == TEXT("panelcolor") || Key == TEXT("panel") || Key == TEXT("listpanel") ||
            Key == TEXT("centerpanel") || Key == TEXT("statspanel") || Key == TEXT("pagecolor"))
            return TEXT("Panel");
        if (Key == TEXT("cardcolor") || Key == TEXT("playercard") || Key == TEXT("normalcolor") ||
            Key == TEXT("playernormal") || Key == TEXT("tabnormal") || Key == TEXT("selectornormal") ||
            Key == TEXT("portraitcolor")) return TEXT("Card");
        if (Key == TEXT("selectedcolor") || Key == TEXT("selectedcard") || Key == TEXT("playerselected") ||
            Key == TEXT("tabselected") || Key == TEXT("selectorfocused") || Key == TEXT("slotfocused") ||
            Key == TEXT("accent") || Key == TEXT("meter")) return TEXT("Accent");
        if (Key == TEXT("activecolor") || Key == TEXT("playerstarter")) return TEXT("Starter");
        if (Key == TEXT("secondarytext")) return TEXT("SecondaryText");
        if (Key == TEXT("pitchcolor")) return TEXT("Pitch");
        return FString();
    }
}

bool FSoccerUITheme::IsValidTheme(const FString& InTheme)
{
    return InTheme == TEXT("Original") || InTheme == TEXT("Verde estadio") ||
           InTheme == TEXT("Azul profesional") || InTheme == TEXT("Grafito");
}

FString FSoccerUITheme::GetActiveTheme()
{
    if (!SoccerUIThemePrivate::IsLoaded())
    {
        SoccerUIThemePrivate::IsLoaded() = true;
        FString SavedTheme;
        if (FFileHelper::LoadFileToString(SavedTheme, *SoccerUIThemePrivate::ConfigPath()))
        {
            TArray<FString> ConfigLines;
            SavedTheme.ParseIntoArrayLines(ConfigLines);
            FString ConfiguredName;
            for (const FString& ConfigLine : ConfigLines)
            {
                FString EntryKey, EntryValue;
                if (ConfigLine.Split(TEXT("="), &EntryKey, &EntryValue) &&
                    EntryKey.TrimStartAndEnd().Equals(TEXT("Theme"), ESearchCase::IgnoreCase))
                {
                    ConfiguredName = EntryValue.TrimStartAndEnd();
                    break;
                }
            }
            // Also accept the first-stage plain-name format, if present.
            if (ConfiguredName.IsEmpty()) ConfiguredName = SavedTheme.TrimStartAndEnd();
            SoccerUIThemePrivate::ActiveTheme() = IsValidTheme(ConfiguredName)
                ? ConfiguredName : TEXT("Original");
        }
        else
        {
            SoccerUIThemePrivate::ActiveTheme() = TEXT("Original");
        }
    }
    return SoccerUIThemePrivate::ActiveTheme();
}

bool FSoccerUITheme::SetActiveTheme(const FString& InTheme)
{
    if (!IsValidTheme(InTheme)) return false;
    const FString FilePath = SoccerUIThemePrivate::ConfigPath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
    const FString ConfigText = TEXT("; Football UI theme. Local custom colors have priority.\n")
        TEXT("[SoccerUI]\nTheme=") + InTheme + TEXT("\n");
    if (!FFileHelper::SaveStringToFile(ConfigText, *FilePath)) return false;
    SoccerUIThemePrivate::ActiveTheme() = InTheme;
    SoccerUIThemePrivate::IsLoaded() = true;
    return true;
}

FLinearColor FSoccerUITheme::ResolveColor(const FString& Key,
    const FLinearColor& LocalColor, bool bLocallyCustomized)
{
    const FString Theme = GetActiveTheme();
    if (bLocallyCustomized || Theme == TEXT("Original")) return LocalColor;
    const FString Role = SoccerUIThemePrivate::RoleForKey(Key);
    if (Role.IsEmpty()) return LocalColor;
    FLinearColor ThemeColor = SoccerUIThemePrivate::PaletteColor(Theme, Role);
    ThemeColor.A = LocalColor.A;
    return ThemeColor;
}

FVector2D FSoccerUITheme::GetLogicalViewportSize(UObject* WorldContext)
{
    if (!WorldContext) return FVector2D::ZeroVector;
    const FVector2D PixelSize = UWidgetLayoutLibrary::GetViewportSize(WorldContext);
    const float DPIScale = FMath::Max(.1f, UWidgetLayoutLibrary::GetViewportScale(WorldContext));
    return PixelSize / DPIScale;
}

FVector2D FSoccerUITheme::FitPanelToViewport(UObject* WorldContext,
    const FVector2D& DesiredSize, float EdgeMargin)
{
    const FVector2D LogicalSize = GetLogicalViewportSize(WorldContext);
    if (LogicalSize.X <= 1.f || LogicalSize.Y <= 1.f) return DesiredSize;
    return FVector2D(FMath::Min(DesiredSize.X, FMath::Max(100.f, LogicalSize.X - 2.f * EdgeMargin)),
                     FMath::Min(DesiredSize.Y, FMath::Max(100.f, LogicalSize.Y - 2.f * EdgeMargin)));
}

float FSoccerUITheme::GetAdaptiveTextScale(UObject* WorldContext, float UserScale)
{
    // Work with DPI-independent UMG coordinates. Keep the author's choice as
    // the source of truth, applying a small automatic reduction only below 1000u.
    const FVector2D LogicalSize = GetLogicalViewportSize(WorldContext);
    const float Compact = LogicalSize.X > 1.f
        ? FMath::Clamp(LogicalSize.X / 1000.f, .82f, 1.f)
        : 1.f;
    return FMath::Clamp(UserScale, .65f, 1.8f) * Compact;
}

void FSoccerUITheme::ApplyButtonStates(UWidget* Root)
{
    if (!Root) return;
    if (UButton* Button = Cast<UButton>(Root))
    {
        // The existing SetBackgroundColor calls continue to supply each
        // menu's normal/selected/active tint. Brushes contribute only the
        // transient hover/pressed/disabled feedback to that same color.
        FButtonStyle ButtonStyle = Button->WidgetStyle;
        ButtonStyle.Hovered.TintColor = FSlateColor(FLinearColor(1.14f, 1.14f, 1.14f, 1.f));
        ButtonStyle.Pressed.TintColor = FSlateColor(FLinearColor(.78f, .78f, .78f, 1.f));
        ButtonStyle.Disabled = ButtonStyle.Normal;
        ButtonStyle.Disabled.TintColor = FSlateColor(FLinearColor(.43f, .43f, .43f, .68f));
        Button->SetStyle(ButtonStyle);
    }
    if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
    {
        for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
        {
            ApplyButtonStates(Panel->GetChildAt(ChildIndex));
        }
    }
}
