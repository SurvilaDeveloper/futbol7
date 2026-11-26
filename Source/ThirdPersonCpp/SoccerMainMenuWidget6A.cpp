#include "SoccerMainMenuWidget6A.h"
#include "SoccerMatchHistoryWidget.h"
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
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"

namespace SoccerFrontMenuUI6A
{
    const FLinearColor White(0.96f, 0.98f, 0.99f, 1.0f);
    const FLinearColor Muted(0.67f, 0.81f, 0.84f, 1.0f);
    const FLinearColor Green(0.32f, 0.82f, 0.52f, 1.0f);
    const FLinearColor DarkText(0.018f, 0.13f, 0.085f, 1.0f);
    const FLinearColor Secondary(0.12f, 0.29f, 0.35f, 1.0f);

    static UTextBlock* Label(UWidgetTree* Tree, const FString& Caption,
        int32 FontSize, const FLinearColor& Color)
    {
        UTextBlock* Result = Tree->ConstructWidget<UTextBlock>();
        Result->SetText(FText::FromString(Caption));
        FSlateFontInfo NewFont = Result->Font;
        NewFont.Size = FontSize;
        Result->SetFont(NewFont);
        Result->SetColorAndOpacity(FSlateColor(Color));
        Result->SetJustification(ETextJustify::Center);
        Result->SetAutoWrapText(true);
        return Result;
    }

    static UButton* Action(UWidgetTree* Tree, const FString& Title,
        bool bPrimary = false, bool bEnabled = true)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        Button->SetBackgroundColor(bPrimary ? Green : Secondary);
        USizeBox* Frame = Tree->ConstructWidget<USizeBox>();
        Frame->SetMinDesiredHeight(bPrimary ? 67.0f : 49.0f);
        Frame->SetContent(Label(Tree, Title, bPrimary ? 23 : 18,
            bPrimary ? DarkText : White));
        Button->AddChild(Frame);
        Button->SetIsEnabled(bEnabled);
        return Button;
    }

    static void AddRow(UVerticalBox* Parent, UWidget* Child, float SpaceBelow)
    {
        UVerticalBoxSlot* MenuRowSlot = Parent->AddChildToVerticalBox(Child);
        MenuRowSlot->SetHorizontalAlignment(HAlign_Fill);
        MenuRowSlot->SetPadding(FMargin(3.0f, 0.0f, 3.0f, SpaceBelow));
    }
}

void USoccerMainMenuWidget6A::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    bIsFocusable = true;
    BuildMenu6A();
}

void USoccerMainMenuWidget6A::BuildMenu6A()
{
    if (!WidgetTree || WidgetTree->RootWidget) return;
    using namespace SoccerFrontMenuUI6A;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
    Backdrop->SetBrushColor(FLinearColor(0.012f, 0.035f, 0.047f, 1.0f));
    UCanvasPanelSlot* BackdropCanvasSlot = Root->AddChildToCanvas(Backdrop);
    BackdropCanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    BackdropCanvasSlot->SetOffsets(FMargin(0));

    UScaleBox* Scaler = WidgetTree->ConstructWidget<UScaleBox>();
    Scaler->SetStretch(EStretch::ScaleToFit);
    Scaler->SetStretchDirection(EStretchDirection::DownOnly);
    Backdrop->SetContent(Scaler);

    USizeBox* MenuSize = WidgetTree->ConstructWidget<USizeBox>();
    MenuSize->SetWidthOverride(660.0f);
    MenuSize->SetHeightOverride(710.0f);
    Scaler->AddChild(MenuSize);
    if (UScaleBoxSlot* MenuScaleSlot = Cast<UScaleBoxSlot>(MenuSize->Slot))
    {
        MenuScaleSlot->SetHorizontalAlignment(HAlign_Center);
        MenuScaleSlot->SetVerticalAlignment(VAlign_Center);
    }

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrushColor(FLinearColor(0.035f, 0.082f, 0.106f, 1.0f));
    Card->SetPadding(FMargin(48.0f, 32.0f));
    MenuSize->SetContent(Card);

    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->SetContent(Content);
    AddRow(Content, Label(WidgetTree, TEXT("FÚTBOL 7"), 46, White), 4.0f);
    AddRow(Content, Label(WidgetTree,
        TEXT("PARTIDOS  ·  ENTRENAMIENTO  ·  EL LOCO  ·  ESTADÍSTICAS"), 15, Muted), 25.0f);

    PlayButton6A = Action(WidgetTree, TEXT("JUGAR PARTIDO"), true);
    PlayButton6A->OnClicked.AddDynamic(this, &USoccerMainMenuWidget6A::HandlePlay6A);
    AddRow(Content, PlayButton6A, 12.0f);

    TrainingButton6B = Action(WidgetTree, TEXT("ENTRENAMIENTO"));
    TrainingButton6B->OnClicked.AddDynamic(this, &USoccerMainMenuWidget6A::HandleTraining6B);
    AddRow(Content, TrainingButton6B, 12.0f);

    RondoButton8 = Action(WidgetTree, TEXT("JUGAR EL LOCO"));
    RondoButton8->OnClicked.AddDynamic(this, &USoccerMainMenuWidget6A::HandleRondo8);
    AddRow(Content, RondoButton8, 12.0f);

    HistoryButton6A = Action(WidgetTree, TEXT("HISTORIAL DE PARTIDOS"));
    HistoryButton6A->OnClicked.AddDynamic(this, &USoccerMainMenuWidget6A::HandleHistory6A);
    AddRow(Content, HistoryButton6A, 12.0f);

    AddRow(Content, Action(WidgetTree,
        TEXT("OPCIONES  ·  PRÓXIMAMENTE"), false, false), 12.0f);

    QuitButton6A = Action(WidgetTree, TEXT("SALIR DEL JUEGO"));
    QuitButton6A->OnClicked.AddDynamic(this, &USoccerMainMenuWidget6A::HandleQuit6A);
    AddRow(Content, QuitButton6A, 21.0f);

    AddRow(Content, Label(WidgetTree,
        TEXT("Flechas / cruceta: navegar   ·   Enter / A: elegir   ·   F11: pantalla completa"),
        13, Muted), 0.0f);
}

void USoccerMainMenuWidget6A::FocusFirstAction6A()
{
    if (IsValid(PlayButton6A)) PlayButton6A->SetKeyboardFocus();
}

void USoccerMainMenuWidget6A::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    UpdateFocusColors6A();
}

void USoccerMainMenuWidget6A::UpdateFocusColors6A()
{
    using namespace SoccerFrontMenuUI6A;
    if (IsValid(PlayButton6A))
        PlayButton6A->SetBackgroundColor(PlayButton6A->HasKeyboardFocus()
            ? FLinearColor(0.46f, 0.98f, 0.62f, 1.0f) : Green);
    if (IsValid(TrainingButton6B))
        TrainingButton6B->SetBackgroundColor(TrainingButton6B->HasKeyboardFocus()
            ? FLinearColor(0.22f, 0.52f, 0.55f, 1.0f) : Secondary);
    if (IsValid(RondoButton8))
        RondoButton8->SetBackgroundColor(RondoButton8->HasKeyboardFocus()
            ? FLinearColor(0.22f, 0.52f, 0.55f, 1.0f) : Secondary);
    if (IsValid(HistoryButton6A))
        HistoryButton6A->SetBackgroundColor(HistoryButton6A->HasKeyboardFocus()
            ? FLinearColor(0.22f, 0.52f, 0.55f, 1.0f) : Secondary);
    if (IsValid(QuitButton6A))
        QuitButton6A->SetBackgroundColor(QuitButton6A->HasKeyboardFocus()
            ? FLinearColor(0.22f, 0.52f, 0.55f, 1.0f) : Secondary);
}

void USoccerMainMenuWidget6A::Navigate6A(int32 Step)
{
    TArray<UButton*> Choices;
    Choices.Add(PlayButton6A);
    Choices.Add(TrainingButton6B);
    Choices.Add(RondoButton8);
    Choices.Add(HistoryButton6A);
    Choices.Add(QuitButton6A);
    int32 Current = 0;
    for (int32 Index = 0; Index < Choices.Num(); ++Index)
    {
        if (IsValid(Choices[Index]) && Choices[Index]->HasKeyboardFocus())
        {
            Current = Index;
            break;
        }
    }
    Current = (Current + Step + Choices.Num()) % Choices.Num();
    if (IsValid(Choices[Current])) Choices[Current]->SetKeyboardFocus();
}

void USoccerMainMenuWidget6A::HandlePlay6A()
{
    if (!GetWorld()) return;
    UE_LOG(LogTemp, Display, TEXT("[MainMenu6A] Opening soccer match map"));
    UGameplayStatics::OpenLevel(this,
        FName(TEXT("/Game/ThirdPersonCPP/Maps/SoccerMainMap")), true);
}

void USoccerMainMenuWidget6A::HandleRondo8()
{
    if (!GetWorld()) return;
    const FString MapPackage = TEXT("/Game/ThirdPersonCPP/Maps/Rondo/SoccerRondoMap");
    if (!FPackageName::DoesPackageExist(MapPackage))
    {
        UE_LOG(LogTemp, Error, TEXT("[Rondo8] Map not found: %s"), *MapPackage);
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("[Rondo8] Opening SoccerRondoMap from main menu"));
    UGameplayStatics::OpenLevel(this, FName(*MapPackage), true);
}

void USoccerMainMenuWidget6A::HandleTraining6B()
{
    OpenTrainingSelector6B();
}

void USoccerMainMenuWidget6A::OpenTrainingSelector6B()
{
    APlayerController* PC = GetOwningPlayer();
    if (!IsValid(PC)) return;
    if (!IsValid(TrainingWidget6B))
        TrainingWidget6B = CreateWidget<USoccerTrainingSelectorWidget6B>(
            PC, USoccerTrainingSelectorWidget6B::StaticClass());
    if (!IsValid(TrainingWidget6B)) return;
    TrainingWidget6B->OnTrainingSelectorClosed6B.RemoveDynamic(
        this, &USoccerMainMenuWidget6A::HandleTrainingClosed6B);
    TrainingWidget6B->OnTrainingSelectorClosed6B.AddDynamic(
        this, &USoccerMainMenuWidget6A::HandleTrainingClosed6B);
    SetVisibility(ESlateVisibility::Hidden);
    if (!TrainingWidget6B->IsInViewport()) TrainingWidget6B->AddToViewport(160);
    TrainingWidget6B->SetVisibility(ESlateVisibility::Visible);
    PC->bShowMouseCursor = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(TrainingWidget6B->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(Mode);
    TrainingWidget6B->FocusFirstExercise6B();
}

void USoccerMainMenuWidget6A::HandleTrainingClosed6B()
{
    SetVisibility(ESlateVisibility::Visible);
    APlayerController* PC = GetOwningPlayer();
    if (!IsValid(PC)) return;
    PC->bShowMouseCursor = true;
    FInputModeUIOnly Mode;
    if (IsValid(TrainingButton6B))
        Mode.SetWidgetToFocus(TrainingButton6B->TakeWidget());
    else Mode.SetWidgetToFocus(TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(Mode);
    if (IsValid(TrainingButton6B)) TrainingButton6B->SetKeyboardFocus();
}

void USoccerMainMenuWidget6A::HandleHistory6A()
{
    APlayerController* PC = GetOwningPlayer();
    if (!IsValid(PC)) return;
    if (!IsValid(HistoryWidget6A))
        HistoryWidget6A = CreateWidget<USoccerMatchHistoryWidget>(
            PC, USoccerMatchHistoryWidget::StaticClass());
    if (!IsValid(HistoryWidget6A)) return;
    HistoryWidget6A->OnHistoryClosed.RemoveDynamic(
        this, &USoccerMainMenuWidget6A::HandleHistoryClosed6A);
    HistoryWidget6A->OnHistoryClosed.AddDynamic(
        this, &USoccerMainMenuWidget6A::HandleHistoryClosed6A);
    SetVisibility(ESlateVisibility::Hidden);
    if (!HistoryWidget6A->IsInViewport()) HistoryWidget6A->AddToViewport(160);
    HistoryWidget6A->OpenHistory(true); // Opened from the main menu.
}

void USoccerMainMenuWidget6A::HandleHistoryClosed6A()
{
    SetVisibility(ESlateVisibility::Visible);
    APlayerController* PC = GetOwningPlayer();
    if (IsValid(PC))
    {
        PC->bShowMouseCursor = true;
        FInputModeUIOnly Mode;
        if (IsValid(HistoryButton6A))
            Mode.SetWidgetToFocus(HistoryButton6A->TakeWidget());
        else Mode.SetWidgetToFocus(TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode);
        if (IsValid(HistoryButton6A)) HistoryButton6A->SetKeyboardFocus();
    }
}

void USoccerMainMenuWidget6A::HandleQuit6A()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

FReply USoccerMainMenuWidget6A::NativeOnPreviewKeyDown(
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
        Navigate6A(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::S)
    {
        Navigate6A(1);
        return FReply::Handled();
    }
    if (Key == EKeys::H)
    {
        HandleHistory6A();
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar ||
        Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (IsValid(TrainingButton6B) && TrainingButton6B->HasKeyboardFocus())
            HandleTraining6B();
        else if (IsValid(RondoButton8) && RondoButton8->HasKeyboardFocus())
            HandleRondo8();
        else if (IsValid(HistoryButton6A) && HistoryButton6A->HasKeyboardFocus())
            HandleHistory6A();
        else if (IsValid(QuitButton6A) && QuitButton6A->HasKeyboardFocus())
            HandleQuit6A();
        else HandlePlay6A();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

FReply USoccerMainMenuWidget6A::NativeOnAnalogValueChanged(
    const FGeometry& Geometry, const FAnalogInputEvent& InAnalogInputEvent)
{
    if (InAnalogInputEvent.GetKey() == EKeys::Gamepad_LeftY &&
        FMath::Abs(InAnalogInputEvent.GetAnalogValue()) >= 0.6f)
    {
        const double Now = FPlatformTime::Seconds();
        if (LastAnalogNavigation6A < 0 || Now - LastAnalogNavigation6A >= 0.23)
        {
            Navigate6A(InAnalogInputEvent.GetAnalogValue() > 0 ? -1 : 1);
            LastAnalogNavigation6A = Now;
        }
        return FReply::Handled();
    }
    return Super::NativeOnAnalogValueChanged(Geometry, InAnalogInputEvent);
}
