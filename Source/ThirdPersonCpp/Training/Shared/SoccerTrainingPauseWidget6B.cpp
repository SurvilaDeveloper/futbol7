#include "Training/Shared/SoccerTrainingPauseWidget6B.h"
#include "Training/Shared/SoccerTrainingPlayerController6B.h"

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
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"

namespace SoccerTrainingPauseAppearance6B
{
    static const FLinearColor Green(0.32f, 0.82f, 0.52f, 1.f);
    static const FLinearColor DarkText(0.018f, 0.13f, 0.085f, 1.f);
    static const FLinearColor White(0.95f, 0.98f, 0.99f, 1.f);
    static const FLinearColor Muted(0.67f, 0.81f, 0.84f, 1.f);
    static const FLinearColor Secondary(0.12f, 0.29f, 0.35f, 1.f);

    static UTextBlock* Label(UWidgetTree* Tree, const FString& Caption,
        int32 Size, const FLinearColor& Color)
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Caption));
        FSlateFontInfo Font = Text->Font;
        Font.Size = Size;
        Text->SetFont(Font);
        Text->SetJustification(ETextJustify::Center);
        Text->SetAutoWrapText(true);
        Text->SetColorAndOpacity(FSlateColor(Color));
        return Text;
    }

    static UButton* Button(UWidgetTree* Tree, const FString& Caption, bool bPrimary)
    {
        UButton* Result = Tree->ConstructWidget<UButton>();
        Result->SetBackgroundColor(bPrimary ? Green : Secondary);
        USizeBox* Size = Tree->ConstructWidget<USizeBox>();
        Size->SetMinDesiredHeight(bPrimary ? 70.f : 52.f);
        Size->SetContent(Label(Tree, Caption, bPrimary ? 23 : 18,
            bPrimary ? DarkText : White));
        Result->AddChild(Size);
        return Result;
    }
    static void AddRow(UVerticalBox* List, UWidget* Content, float Bottom)
    {
        UVerticalBoxSlot* SlotRow6B = List->AddChildToVerticalBox(Content);
        SlotRow6B->SetHorizontalAlignment(HAlign_Fill);
        SlotRow6B->SetPadding(FMargin(3.f,0.f,3.f,Bottom));
    }
}

void USoccerTrainingPauseWidget6B::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    bIsFocusable = true;
    BuildPause6B();
}

void USoccerTrainingPauseWidget6B::BuildPause6B()
{
    if (!WidgetTree || WidgetTree->RootWidget) return;
    using namespace SoccerTrainingPauseAppearance6B;
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.012f, 0.035f, 0.047f, 0.97f));
    UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(Background);
    BGSlot->SetAnchors(FAnchors(0.f,0.f,1.f,1.f));
    BGSlot->SetOffsets(FMargin(0.f));
    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Background->SetContent(Scale);
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(610.f);
    Size->SetHeightOverride(570.f);
    Scale->AddChild(Size);
    if (UScaleBoxSlot* PanelSlot6B = Cast<UScaleBoxSlot>(Size->Slot))
    {
        PanelSlot6B->SetHorizontalAlignment(HAlign_Center);
        PanelSlot6B->SetVerticalAlignment(VAlign_Center);
    }
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrushColor(FLinearColor(0.035f,0.082f,0.106f,1.f));
    Panel->SetPadding(FMargin(40.f,30.f));
    Size->SetContent(Panel);
    UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(List);
    AddRow(List, Label(WidgetTree, TEXT("ENTRENAMIENTO EN PAUSA"), 32, White), 8.f);
    AddRow(List, Label(WidgetTree, TEXT("Los ejercicios no se guardan como partidos."),
        16, Muted), 24.f);
    ResumeButton6B = Button(WidgetTree, TEXT("VOLVER AL ENTRENAMIENTO"), true);
    ResumeButton6B->OnClicked.AddDynamic(this, &USoccerTrainingPauseWidget6B::Resume6B);
    AddRow(List, ResumeButton6B, 13.f);
    RestartButton6B = Button(WidgetTree, TEXT("REPETIR EJERCICIO"), true);
    RestartButton6B->OnClicked.AddDynamic(this, &USoccerTrainingPauseWidget6B::Restart6B);
    AddRow(List, RestartButton6B, 13.f);
    ChangeExerciseButton6B = Button(WidgetTree, TEXT("ELEGIR OTRO EJERCICIO"), false);
    ChangeExerciseButton6B->OnClicked.AddDynamic(this,
        &USoccerTrainingPauseWidget6B::ChangeExercise6B);
    AddRow(List, ChangeExerciseButton6B, 13.f);
    MainMenuButton6B = Button(WidgetTree, TEXT("MENÚ PRINCIPAL"), false);
    MainMenuButton6B->OnClicked.AddDynamic(this,
        &USoccerTrainingPauseWidget6B::MainMenu6B);
    AddRow(List, MainMenuButton6B, 18.f);
    AddRow(List, Label(WidgetTree,
        TEXT("Flechas / cruceta: navegar  ·  Enter / A: elegir  ·  Esc / B: reanudar"),
        13, Muted), 0.f);
}

void USoccerTrainingPauseWidget6B::ShowPause6B(bool bVisible)
{
    SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void USoccerTrainingPauseWidget6B::FocusResume6B()
{
    if (IsValid(ResumeButton6B)) ResumeButton6B->SetKeyboardFocus();
}

void USoccerTrainingPauseWidget6B::Navigate6B(int32 Offset)
{
    UButton* Items[4] = { ResumeButton6B, RestartButton6B,
        ChangeExerciseButton6B, MainMenuButton6B };
    int32 Index = 0;
    for (int32 ItemIndex=0; ItemIndex<4; ++ItemIndex)
    {
        if (IsValid(Items[ItemIndex]) && Items[ItemIndex]->HasKeyboardFocus())
        {
            Index = ItemIndex;
            break;
        }
    }
    Index = (Index + Offset + 4) % 4;
    if (IsValid(Items[Index])) Items[Index]->SetKeyboardFocus();
}

void USoccerTrainingPauseWidget6B::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    using namespace SoccerTrainingPauseAppearance6B;
    if (IsValid(ResumeButton6B)) ResumeButton6B->SetBackgroundColor(
        ResumeButton6B->HasKeyboardFocus() ? FLinearColor(0.48f,0.99f,0.66f,1.f) : Green);
    if (IsValid(RestartButton6B)) RestartButton6B->SetBackgroundColor(
        RestartButton6B->HasKeyboardFocus() ? FLinearColor(0.48f,0.99f,0.66f,1.f) : Green);
    if (IsValid(ChangeExerciseButton6B)) ChangeExerciseButton6B->SetBackgroundColor(
        ChangeExerciseButton6B->HasKeyboardFocus() ? FLinearColor(0.22f,0.52f,0.55f,1.f) : Secondary);
    if (IsValid(MainMenuButton6B)) MainMenuButton6B->SetBackgroundColor(
        MainMenuButton6B->HasKeyboardFocus() ? FLinearColor(0.22f,0.52f,0.55f,1.f) : Secondary);
}

void USoccerTrainingPauseWidget6B::Resume6B()
{
    if (ASoccerTrainingPlayerController6B* PC = Cast<ASoccerTrainingPlayerController6B>(GetOwningPlayer()))
        PC->ResumeTraining6B();
}

void USoccerTrainingPauseWidget6B::Restart6B()
{
    if (ASoccerTrainingPlayerController6B* PC = Cast<ASoccerTrainingPlayerController6B>(GetOwningPlayer()))
        PC->RestartTraining6B();
}

void USoccerTrainingPauseWidget6B::ChangeExercise6B()
{
    if (ASoccerTrainingPlayerController6B* PC = Cast<ASoccerTrainingPlayerController6B>(GetOwningPlayer()))
        PC->ReturnToExerciseSelector6B();
}

void USoccerTrainingPauseWidget6B::MainMenu6B()
{
    if (ASoccerTrainingPlayerController6B* PC = Cast<ASoccerTrainingPlayerController6B>(GetOwningPlayer()))
        PC->ReturnToMainMenu6B();
}

FReply USoccerTrainingPauseWidget6B::NativeOnPreviewKeyDown(
    const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    const FKey Key = KeyEvent.GetKey();
    if (Key == EKeys::F11)
    {
        if (APlayerController* PC = GetOwningPlayer())
            PC->ConsoleCommand(TEXT("fullscreen"),true);
        return FReply::Handled();
    }
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::P)
    {
        Resume6B();
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
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (IsValid(RestartButton6B) && RestartButton6B->HasKeyboardFocus()) Restart6B();
        else if (IsValid(ChangeExerciseButton6B) && ChangeExerciseButton6B->HasKeyboardFocus()) ChangeExercise6B();
        else if (IsValid(MainMenuButton6B) && MainMenuButton6B->HasKeyboardFocus()) MainMenu6B();
        else Resume6B();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

FReply USoccerTrainingPauseWidget6B::NativeOnAnalogValueChanged(
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
