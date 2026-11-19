#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/PanelWidget.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "HAL/PlatformTime.h"

// One lightweight, non-UObject navigation helper for all native soccer menus.
// Keeps gameplay controls separate from menus and works in UIOnly + paused games.
struct FSoccerMenuAnalogRepeat
{
    int32 HorizontalDirection = 0;
    int32 VerticalDirection = 0;
    double LastHorizontalAt = -1000.0;
    double LastVerticalAt = -1000.0;
};

struct FSoccerUINavigation
{
    static bool IsAccept(const FKey& Key)
    {
        return Key == EKeys::Enter || Key == EKeys::SpaceBar ||
            Key == EKeys::Gamepad_FaceButton_Bottom;
    }

    static bool IsBack(const FKey& Key)
    {
        return Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right;
    }

    static int32 Horizontal(const FKey& Key)
    {
        if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) return -1;
        if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) return 1;
        return 0;
    }

    static int32 Vertical(const FKey& Key)
    {
        if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up) return -1;
        if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down) return 1;
        return 0;
    }

    // Prevent arrows/Enter typed inside temporary style editors from acting on gameplay menus.
    static bool IsEditingText(UWidgetTree* Tree)
    {
        if (!IsValid(Tree)) return false;
        TArray<UWidget*> Widgets;
        Tree->GetAllWidgets(Widgets);
        for (UWidget* Widget : Widgets)
        {
            if (!IsValid(Widget) || !Widget->HasKeyboardFocus()) continue;
            if (Widget->IsA<UEditableTextBox>() || Widget->IsA<UEditableText>() ||
                Widget->IsA<UMultiLineEditableTextBox>() || Widget->IsA<UComboBoxString>())
            {
                return true;
            }
        }
        return false;
    }

    // Used by temporary style drawers so their own buttons never trigger
    // a match confirmation or another underlying menu shortcut.
    static bool HasFocusedDescendant(const UWidget* Root)
    {
        if (!IsValid(Root)) return false;
        if (Root->HasKeyboardFocus()) return true;
        const UPanelWidget* Panel = Cast<UPanelWidget>(Root);
        if (!Panel) return false;
        for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
        {
            if (HasFocusedDescendant(Panel->GetChildAt(Index))) return true;
        }
        return false;
    }

    // UMG/Slate owns the focus outline; the scrolling container follows it.
    static void FocusAndReveal(UUserWidget* Menu, UWidget* Target, bool bSetFocus = true)
    {
        if (!IsValid(Target)) return;
        if (bSetFocus && IsValid(Menu))
        {
            if (APlayerController* PC = Menu->GetOwningPlayer())
                Target->SetUserFocus(PC);
            else
                Target->SetKeyboardFocus();
        }
        for (UWidget* Ancestor = Target->GetParent(); Ancestor != nullptr;
             Ancestor = Ancestor->GetParent())
        {
            if (UScrollBox* Scroll = Cast<UScrollBox>(Ancestor))
            {
                Scroll->ScrollWidgetIntoView(Target);
                break;
            }
        }
    }

    // A single repeat policy for stick navigation (no frame-rate-driven auto-repeat).
    // Returns true if this is one of the handled axes even when it is neutral.
    static bool ReadStick(const FAnalogInputEvent& Event, FSoccerMenuAnalogRepeat& State,
                          int32& OutX, int32& OutY, float Threshold = 0.65f,
                          double RepeatInterval = 0.20)
    {
        OutX = OutY = 0;
        const FKey Key = Event.GetKey();
        const bool bX = Key == EKeys::Gamepad_LeftX;
        const bool bY = Key == EKeys::Gamepad_LeftY;
        if (!bX && !bY) return false;
        const float Value = Event.GetAnalogValue();
        const int32 Direction = Value >= Threshold ? 1 : (Value <= -Threshold ? -1 : 0);
        int32& Previous = bX ? State.HorizontalDirection : State.VerticalDirection;
        double& LastAt = bX ? State.LastHorizontalAt : State.LastVerticalAt;
        const double Now = FPlatformTime::Seconds();
        if (Direction == 0)
        {
            Previous = 0;
            return true;
        }
        if (Direction != Previous || Now - LastAt >= RepeatInterval)
        {
            if (bX) OutX = Direction;
            else OutY = -Direction; // Positive gamepad Y points up, UI indices grow downward.
            LastAt = Now;
        }
        Previous = Direction;
        return true;
    }
};
