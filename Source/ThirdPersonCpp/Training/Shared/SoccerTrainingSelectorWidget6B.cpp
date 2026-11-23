#include "Training/Shared/SoccerTrainingSelectorWidget6B.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

namespace SoccerTrainingSelectorAppearance6B
{
    static const FLinearColor Green(0.32f, 0.82f, 0.52f, 1.f);
    static const FLinearColor DarkText(0.018f, 0.13f, 0.085f, 1.f);
    static const FLinearColor White(0.95f, 0.98f, 0.99f, 1.f);
    static const FLinearColor Muted(0.67f, 0.81f, 0.84f, 1.f);
    static const FLinearColor Secondary(0.12f, 0.29f, 0.35f, 1.f);

    static UTextBlock* MakeLabel(UWidgetTree* Tree, const FString& Caption,
        int32 FontSize, FLinearColor Color)
    {
        UTextBlock* Result = Tree->ConstructWidget<UTextBlock>();
        Result->SetText(FText::FromString(Caption));
        FSlateFontInfo Font = Result->Font;
        Font.Size = FontSize;
        Result->SetFont(Font);
        Result->SetColorAndOpacity(FSlateColor(Color));
        Result->SetJustification(ETextJustify::Center);
        Result->SetAutoWrapText(true);
        return Result;
    }

    static UButton* MakeButton(UWidgetTree* Tree, const FString& Caption,
        bool bPrimary, bool bEnabled=true)
    {
        UButton* Result = Tree->ConstructWidget<UButton>();
        Result->SetBackgroundColor(bPrimary ? Green : Secondary);
        USizeBox* Box = Tree->ConstructWidget<USizeBox>();
        Box->SetMinDesiredHeight(bPrimary ? 70.f : 50.f);
        Box->SetContent(MakeLabel(Tree, Caption, bPrimary ? 22 : 17,
            bPrimary ? DarkText : White));
        Result->AddChild(Box);
        Result->SetIsEnabled(bEnabled);
        return Result;
    }

    static void AddRow(UVerticalBox* List, UWidget* Content, float Bottom)
    {
        UVerticalBoxSlot* RowSlot = List->AddChildToVerticalBox(Content);
        RowSlot->SetHorizontalAlignment(HAlign_Fill);
        RowSlot->SetPadding(FMargin(3.f, 0.f, 3.f, Bottom));
    }
}

void USoccerTrainingSelectorWidget6B::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    bIsFocusable = true;
    BuildSelector6B();
}

void USoccerTrainingSelectorWidget6B::BuildSelector6B()
{
    if (!WidgetTree || WidgetTree->RootWidget) return;
    using namespace SoccerTrainingSelectorAppearance6B;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
    Backdrop->SetBrushColor(FLinearColor(0.012f, 0.035f, 0.047f, 1.f));
    UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(Backdrop);
    BGSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    BGSlot->SetOffsets(FMargin(0.f));

    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Backdrop->SetContent(Scale);

    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(740.f);
    Size->SetHeightOverride(1100.f);
    Scale->AddChild(Size);
    if (UScaleBoxSlot* ScaleSlot = Cast<UScaleBoxSlot>(Size->Slot))
    {
        ScaleSlot->SetHorizontalAlignment(HAlign_Center);
        ScaleSlot->SetVerticalAlignment(VAlign_Center);
    }

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrushColor(FLinearColor(0.035f, 0.082f, 0.106f, 1.f));
    Card->SetPadding(FMargin(46.f, 30.f));
    Size->SetContent(Card);
    UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->SetContent(List);
    AddRow(List, MakeLabel(WidgetTree, TEXT("ENTRENAMIENTO"), 40, White), 5.f);
    AddRow(List, MakeLabel(WidgetTree, TEXT("Elegí el ejercicio que querés practicar"),
        17, Muted), 25.f);

    MovementButton6B = MakeButton(WidgetTree, TEXT("MOVIMIENTO Y CONTROL DE PELOTA"), true);
    MovementButton6B->OnClicked.AddDynamic(this,
        &USoccerTrainingSelectorWidget6B::OpenMovementControl6B);
    AddRow(List, MovementButton6B, 12.f);
    AutoPassButton6J = MakeButton(WidgetTree, TEXT("AUTOPASE: APUNTAR Y RECOGER"), true);
    AutoPassButton6J->OnClicked.AddDynamic(this,
        &USoccerTrainingSelectorWidget6B::OpenAutoPass6J);
    AddRow(List, AutoPassButton6J, 12.f);
    AddRow(List, MakeButton(WidgetTree, TEXT("PASES  ·  PRÓXIMAMENTE"), false, false), 10.f);
    ChargedShotButton6M = MakeButton(WidgetTree, TEXT("REMATE CON CARGA DE FUERZA"), true);
    ChargedShotButton6M->OnClicked.AddDynamic(this, &USoccerTrainingSelectorWidget6B::OpenChargedShot6M);
    AddRow(List, ChargedShotButton6M, 10.f);
    GoalkeeperShotButton6T = MakeButton(WidgetTree, TEXT("REMATES CONTRA ARQUERO"), true);
    GoalkeeperShotButton6T->OnClicked.AddDynamic(this, &USoccerTrainingSelectorWidget6B::OpenGoalkeeperShots6T);
    AddRow(List, GoalkeeperShotButton6T, 12.f);
    ZoneFinishButton6AH = MakeButton(WidgetTree, TEXT("CONDUCCIÓN Y DEFINICIÓN POR ZONAS"), true);
    ZoneFinishButton6AH->OnClicked.AddDynamic(this, &USoccerTrainingSelectorWidget6B::OpenZoneFinish6AH);
    AddRow(List, ZoneFinishButton6AH, 12.f);
    AvoidTackleButton6AK = MakeButton(WidgetTree, TEXT("ESQUIVAR TACKLES"), true);
    AvoidTackleButton6AK->OnClicked.AddDynamic(this, &USoccerTrainingSelectorWidget6B::OpenAvoidTackle6AK);
    AddRow(List, AvoidTackleButton6AK, 12.f);
    WinTackleButton6AK = MakeButton(WidgetTree, TEXT("RECUPERAR PELOTA CON TACKLE"), true);
    WinTackleButton6AK->OnClicked.AddDynamic(this, &USoccerTrainingSelectorWidget6B::OpenWinTackle6AK);
    AddRow(List, WinTackleButton6AK, 12.f);

    BackButton6B = MakeButton(WidgetTree, TEXT("VOLVER AL MENÚ PRINCIPAL"), false);
    BackButton6B->OnClicked.AddDynamic(this,
        &USoccerTrainingSelectorWidget6B::Back6B);
    AddRow(List, BackButton6B, 18.f);
    AddRow(List, MakeLabel(WidgetTree,
        TEXT("Flechas / cruceta: navegar   ·   Enter / A: elegir   ·   Esc / B: volver"),
        13, Muted), 0.f);
}

void USoccerTrainingSelectorWidget6B::FocusFirstExercise6B()
{
    if (IsValid(MovementButton6B)) MovementButton6B->SetKeyboardFocus();
}

void USoccerTrainingSelectorWidget6B::Navigate6B(int32 Offset)
{
    UButton* Items[8] = { MovementButton6B, AutoPassButton6J, ChargedShotButton6M, GoalkeeperShotButton6T, ZoneFinishButton6AH, AvoidTackleButton6AK, WinTackleButton6AK, BackButton6B };
    const int32 Current = IsValid(BackButton6B) && BackButton6B->HasKeyboardFocus() ? 7
        : IsValid(WinTackleButton6AK) && WinTackleButton6AK->HasKeyboardFocus() ? 6
        : IsValid(AvoidTackleButton6AK) && AvoidTackleButton6AK->HasKeyboardFocus() ? 5
        : IsValid(ZoneFinishButton6AH) && ZoneFinishButton6AH->HasKeyboardFocus() ? 4
        : IsValid(GoalkeeperShotButton6T) && GoalkeeperShotButton6T->HasKeyboardFocus() ? 3
        : IsValid(ChargedShotButton6M) && ChargedShotButton6M->HasKeyboardFocus() ? 2
        : IsValid(AutoPassButton6J) && AutoPassButton6J->HasKeyboardFocus() ? 1 : 0;
    const int32 Next = (Current + Offset + 8) % 8;
    if (IsValid(Items[Next])) Items[Next]->SetKeyboardFocus();
}

void USoccerTrainingSelectorWidget6B::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    using namespace SoccerTrainingSelectorAppearance6B;
    if (IsValid(MovementButton6B))
    {
        MovementButton6B->SetBackgroundColor(MovementButton6B->HasKeyboardFocus()
            ? FLinearColor(0.48f, 0.99f, 0.66f, 1.f) : Green);
    }
    if (IsValid(AutoPassButton6J))
    {
        AutoPassButton6J->SetBackgroundColor(AutoPassButton6J->HasKeyboardFocus()
            ? FLinearColor(0.48f, 0.99f, 0.66f, 1.f) : Green);
    }
    if (IsValid(ChargedShotButton6M))
    {
        ChargedShotButton6M->SetBackgroundColor(ChargedShotButton6M->HasKeyboardFocus()
            ? FLinearColor(0.48f, 0.99f, 0.66f, 1.f) : Green);
    }
    if (IsValid(GoalkeeperShotButton6T))
    {
        GoalkeeperShotButton6T->SetBackgroundColor(GoalkeeperShotButton6T->HasKeyboardFocus()
            ? FLinearColor(0.48f, 0.99f, 0.66f, 1.f) : Green);
    }
    if (IsValid(ZoneFinishButton6AH))
        ZoneFinishButton6AH->SetBackgroundColor(ZoneFinishButton6AH->HasKeyboardFocus()
            ? FLinearColor(0.48f, 0.99f, 0.66f, 1.f) : Green);
    if (IsValid(AvoidTackleButton6AK))
        AvoidTackleButton6AK->SetBackgroundColor(AvoidTackleButton6AK->HasKeyboardFocus()
            ? FLinearColor(0.48f, 0.99f, 0.66f, 1.f) : Green);
    if (IsValid(WinTackleButton6AK))
        WinTackleButton6AK->SetBackgroundColor(WinTackleButton6AK->HasKeyboardFocus()
            ? FLinearColor(0.48f, 0.99f, 0.66f, 1.f) : Green);
    if (IsValid(BackButton6B))
    {
        BackButton6B->SetBackgroundColor(BackButton6B->HasKeyboardFocus()
            ? FLinearColor(0.22f, 0.52f, 0.55f, 1.f) : Secondary);
    }
}

void USoccerTrainingSelectorWidget6B::OpenMovementControl6B()
{
    UE_LOG(LogTemp, Display, TEXT("[Training6B] Loading MovementControl map"));
    UGameplayStatics::OpenLevel(this,
        FName(TEXT("/Game/ThirdPersonCPP/Maps/Training/MovementControl/SoccerTrainingMovementMap")),
        true);
}

void USoccerTrainingSelectorWidget6B::OpenAutoPass6J()
{
    UE_LOG(LogTemp, Display, TEXT("[Training6J] Loading AutoPass lesson"));
    // Reuses the existing training map, lighting and physical parity.
    UGameplayStatics::OpenLevel(this,
        FName(TEXT("/Game/ThirdPersonCPP/Maps/Training/MovementControl/SoccerTrainingMovementMap")),
        true, TEXT("TrainingLesson=AutoPass"));
}

void USoccerTrainingSelectorWidget6B::OpenChargedShot6M()
{
    UGameplayStatics::OpenLevel(this,
        FName(TEXT("/Game/ThirdPersonCPP/Maps/Training/MovementControl/SoccerTrainingMovementMap")),
        true, TEXT("TrainingLesson=ChargedShot"));
}

void USoccerTrainingSelectorWidget6B::OpenGoalkeeperShots6T()
{
    UGameplayStatics::OpenLevel(this,
        FName(TEXT("/Game/ThirdPersonCPP/Maps/Training/MovementControl/SoccerTrainingMovementMap")),
        true, TEXT("TrainingLesson=GoalkeeperShot"));
}

void USoccerTrainingSelectorWidget6B::OpenZoneFinish6AH()
{
    UGameplayStatics::OpenLevel(this,
        FName(TEXT("/Game/ThirdPersonCPP/Maps/Training/MovementControl/SoccerTrainingMovementMap")),
        true, TEXT("TrainingLesson=ZoneFinish"));
}

void USoccerTrainingSelectorWidget6B::OpenAvoidTackle6AK()
{
    UGameplayStatics::OpenLevel(this,
        FName(TEXT("/Game/ThirdPersonCPP/Maps/Training/MovementControl/SoccerTrainingMovementMap")),
        true, TEXT("TrainingLesson=AvoidTackle"));
}

void USoccerTrainingSelectorWidget6B::OpenWinTackle6AK()
{
    UGameplayStatics::OpenLevel(this,
        FName(TEXT("/Game/ThirdPersonCPP/Maps/Training/MovementControl/SoccerTrainingMovementMap")),
        true, TEXT("TrainingLesson=WinTackle"));
}

void USoccerTrainingSelectorWidget6B::Back6B()
{
    OnTrainingSelectorClosed6B.Broadcast();
    RemoveFromParent();
}

FReply USoccerTrainingSelectorWidget6B::NativeOnPreviewKeyDown(
    const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    const FKey Key = KeyEvent.GetKey();
    if (Key == EKeys::F11)
    {
        if (APlayerController* PC = GetOwningPlayer())
            PC->ConsoleCommand(TEXT("fullscreen"), true);
        return FReply::Handled();
    }
    if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::W)
    {
        Navigate6B(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::S)
    {
        Navigate6B(1);
        return FReply::Handled();
    }
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    {
        Back6B();
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar ||
        Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (IsValid(BackButton6B) && BackButton6B->HasKeyboardFocus()) Back6B();
        else if (IsValid(WinTackleButton6AK) && WinTackleButton6AK->HasKeyboardFocus()) OpenWinTackle6AK();
        else if (IsValid(AvoidTackleButton6AK) && AvoidTackleButton6AK->HasKeyboardFocus()) OpenAvoidTackle6AK();
        else if (IsValid(ZoneFinishButton6AH) && ZoneFinishButton6AH->HasKeyboardFocus()) OpenZoneFinish6AH();
        else if (IsValid(GoalkeeperShotButton6T) && GoalkeeperShotButton6T->HasKeyboardFocus()) OpenGoalkeeperShots6T();
        else if (IsValid(ChargedShotButton6M) && ChargedShotButton6M->HasKeyboardFocus()) OpenChargedShot6M();
        else if (IsValid(AutoPassButton6J) && AutoPassButton6J->HasKeyboardFocus()) OpenAutoPass6J();
        else OpenMovementControl6B();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

FReply USoccerTrainingSelectorWidget6B::NativeOnAnalogValueChanged(
    const FGeometry& Geometry, const FAnalogInputEvent& AnalogEvent)
{
    if (AnalogEvent.GetKey() == EKeys::Gamepad_LeftY &&
        FMath::Abs(AnalogEvent.GetAnalogValue()) >= 0.6f)
    {
        const double Now = FPlatformTime::Seconds();
        if (LastAnalogTime6B < 0.0 || Now - LastAnalogTime6B >= 0.23)
        {
            Navigate6B(AnalogEvent.GetAnalogValue() > 0.f ? -1 : 1);
            LastAnalogTime6B = Now;
        }
        return FReply::Handled();
    }
    return Super::NativeOnAnalogValueChanged(Geometry, AnalogEvent);
}
