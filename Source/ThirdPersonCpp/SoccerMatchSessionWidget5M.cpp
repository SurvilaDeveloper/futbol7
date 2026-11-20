#include "SoccerMatchSessionWidget5M.h"
#include "GameHUD.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace SoccerSessionUI5M
{
    static UTextBlock* Label(UWidgetTree* Tree, const FString& Text, int32 Size,
        const FLinearColor& Color)
    {
        UTextBlock* Result = Tree->ConstructWidget<UTextBlock>();
        Result->SetText(FText::FromString(Text));
        FSlateFontInfo Font = Result->Font;
        Font.Size = Size;
        Result->SetFont(Font);
        Result->SetColorAndOpacity(FSlateColor(Color));
        Result->SetAutoWrapText(true);
        Result->SetJustification(ETextJustify::Center);
        return Result;
    }
    static UButton* Action(UWidgetTree* Tree, const FString& Text,
        const FLinearColor& Color, bool bPlayAction = false)
    {
        UButton* Result = Tree->ConstructWidget<UButton>();
        Result->SetBackgroundColor(bPlayAction
            ? FLinearColor(0.32f, 0.82f, 0.52f, 1.0f) : Color);
        USizeBox* ActionContentSize = Tree->ConstructWidget<USizeBox>();
        ActionContentSize->SetMinDesiredHeight(bPlayAction ? 55.0f : 34.0f);
        ActionContentSize->SetContent(Label(Tree, Text, bPlayAction ? 21 : 19,
            bPlayAction ? FLinearColor(0.018f, 0.13f, 0.085f, 1.0f)
                        : FLinearColor::White));
        Result->AddChild(ActionContentSize);
        return Result;
    }
    static void Add(UVerticalBox* Layout, UWidget* Child, float Bottom = 13.0f)
    {
        UVerticalBoxSlot* Row = Layout->AddChildToVerticalBox(Child);
        Row->SetPadding(FMargin(12.0f, 5.0f, 12.0f, Bottom));
        Row->SetHorizontalAlignment(HAlign_Fill);
    }
}

void USoccerMatchSessionWidget5M::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    bIsFocusable = true;
    BuildScreen5M();
}

void USoccerMatchSessionWidget5M::BuildScreen5M()
{
    if (!WidgetTree || WidgetTree->RootWidget) return;
    using namespace SoccerSessionUI5M;
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
    Dim->SetBrushColor(FLinearColor(0.009f, 0.025f, 0.034f, 0.91f));
    UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim);
    DimSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    DimSlot->SetOffsets(FMargin(0));
    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Dim->SetContent(Scale);
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(570.0f);
    Size->SetHeightOverride(370.0f);
    Scale->AddChild(Size);
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrushColor(FLinearColor(0.042f, 0.104f, 0.133f, 1.0f));
    Card->SetPadding(FMargin(32.0f, 26.0f));
    Size->SetContent(Card);
    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->SetContent(Layout);
    TitleText5M = Label(WidgetTree, TEXT("PARTIDO EN PAUSA"), 29, FLinearColor::White);
    Add(Layout, TitleText5M);
    HintText5M = Label(WidgetTree, TEXT(""), 15,
        FLinearColor(0.76f, 0.87f, 0.90f, 1.0f));
    Add(Layout, HintText5M);
    ResumeButton5M = Action(WidgetTree, TEXT("REANUDAR PARTIDO"), FLinearColor(0.13f, 0.56f, 0.40f), true);
    AbortButton5M = Action(WidgetTree, TEXT("ABANDONAR PARTIDO"), FLinearColor(0.52f, 0.20f, 0.20f));
    ConfirmAbortButton5M = Action(WidgetTree, TEXT("SÍ, ABANDONAR SIN GUARDAR"), FLinearColor(0.58f, 0.18f, 0.18f));
    CancelAbortButton5M = Action(WidgetTree, TEXT("NO, VOLVER A LA PAUSA"), FLinearColor(0.13f, 0.42f, 0.47f));
    PlayButton5M = Action(WidgetTree, TEXT("JUGAR OTRO PARTIDO"), FLinearColor(0.13f, 0.56f, 0.40f), true);
    SeeFieldButton5M = Action(WidgetTree, TEXT("VER CANCHA"), FLinearColor(0.12f, 0.31f, 0.40f));
    MainMenuButton6A = Action(WidgetTree, TEXT("MENÚ PRINCIPAL"), FLinearColor(0.12f, 0.31f, 0.40f));
    ResumeButton5M->OnClicked.AddDynamic(this, &USoccerMatchSessionWidget5M::HandleResume5M);
    AbortButton5M->OnClicked.AddDynamic(this, &USoccerMatchSessionWidget5M::HandleAbort5M);
    ConfirmAbortButton5M->OnClicked.AddDynamic(this, &USoccerMatchSessionWidget5M::HandleConfirmAbort5M);
    CancelAbortButton5M->OnClicked.AddDynamic(this, &USoccerMatchSessionWidget5M::HandleCancelAbort5M);
    PlayButton5M->OnClicked.AddDynamic(this, &USoccerMatchSessionWidget5M::HandlePlay5M);
    SeeFieldButton5M->OnClicked.AddDynamic(this, &USoccerMatchSessionWidget5M::HandleSeeField5M);
    MainMenuButton6A->OnClicked.AddDynamic(this, &USoccerMatchSessionWidget5M::HandleMainMenu6A);
    Add(Layout, ResumeButton5M, 7.0f);
    Add(Layout, AbortButton5M, 7.0f);
    Add(Layout, ConfirmAbortButton5M, 7.0f);
    Add(Layout, CancelAbortButton5M, 7.0f);
    Add(Layout, PlayButton5M, 7.0f);
    Add(Layout, SeeFieldButton5M, 7.0f);
    Add(Layout, MainMenuButton6A, 7.0f);
    SetMode5M(0);
}

void USoccerMatchSessionWidget5M::SetMode5M(int32 NewMode)
{
    CurrentMode5M = NewMode;
    if (!TitleText5M) return;
    TitleText5M->SetText(FText::FromString(NewMode == 0 ? TEXT("PARTIDO EN PAUSA") :
        NewMode == 1 ? TEXT("CONFIRMAR ABANDONO") :
        NewMode == 2 ? TEXT("PARTIDO ABANDONADO") : TEXT("PARTIDO FINALIZADO")));
    HintText5M->SetText(FText::FromString(NewMode == 0
        ? TEXT("Esc / P: reanudar  |  Podés abandonar el partido sin guardar registros.")
        : NewMode == 1 ? TEXT("Esta partida se perderá. No se guardarán resultado ni estadísticas.")
        : NewMode == 2 ? TEXT("No se guardaron datos. JUGAR vuelve a la selección de club rival.")
        : TEXT("El resultado ya fue guardado. Podés iniciar otro encuentro.")));
    ResumeButton5M->SetVisibility(NewMode == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    AbortButton5M->SetVisibility(NewMode == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ConfirmAbortButton5M->SetVisibility(NewMode == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    CancelAbortButton5M->SetVisibility(NewMode == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    PlayButton5M->SetVisibility(NewMode >= 2 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    SeeFieldButton5M->SetVisibility(NewMode == 3 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    MainMenuButton6A->SetVisibility(NewMode >= 2 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (UButton* Focus = NewMode == 0 ? ResumeButton5M : NewMode == 1
        ? CancelAbortButton5M : PlayButton5M)
        Focus->SetKeyboardFocus();
}

void USoccerMatchSessionWidget5M::ShowPaused5M() { SetMode5M(0); }
void USoccerMatchSessionWidget5M::ShowAborted5M() { SetMode5M(2); }
void USoccerMatchSessionWidget5M::ShowFinished5M() { SetMode5M(3); }

void USoccerMatchSessionWidget5M::HandleResume5M()
{
    if (APlayerController* PC = GetOwningPlayer())
        if (AGameHUD* HUD = Cast<AGameHUD>(PC->GetHUD())) HUD->ResumeMatch5M();
}
void USoccerMatchSessionWidget5M::HandleAbort5M() { SetMode5M(1); }
void USoccerMatchSessionWidget5M::HandleCancelAbort5M() { SetMode5M(0); }
void USoccerMatchSessionWidget5M::HandleConfirmAbort5M()
{
    if (APlayerController* PC = GetOwningPlayer())
        if (AGameHUD* HUD = Cast<AGameHUD>(PC->GetHUD())) HUD->AbortMatch5M();
}
void USoccerMatchSessionWidget5M::HandlePlay5M()
{
    if (APlayerController* PC = GetOwningPlayer())
        if (AGameHUD* HUD = Cast<AGameHUD>(PC->GetHUD())) HUD->StartNewMatch5M();
}
void USoccerMatchSessionWidget5M::HandleMainMenu6A()
{
    if (APlayerController* PC = GetOwningPlayer())
        if (AGameHUD* Hud = Cast<AGameHUD>(PC->GetHUD())) Hud->ReturnToMainMenu6A();
}

void USoccerMatchSessionWidget5M::HandleSeeField5M()
{
    if (APlayerController* PC = GetOwningPlayer())
        if (AGameHUD* HUD = Cast<AGameHUD>(PC->GetHUD())) HUD->DismissPostMatchMenu5M();
}
FReply USoccerMatchSessionWidget5M::NativeOnPreviewKeyDown(
    const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    const FKey Key = KeyEvent.GetKey();
    if (Key == EKeys::Escape || Key == EKeys::P || Key == EKeys::Gamepad_FaceButton_Right)
    {
        if (CurrentMode5M == 0) HandleResume5M();
        else if (CurrentMode5M == 1) HandleCancelAbort5M();
        else if (CurrentMode5M == 3) HandleSeeField5M();
        return FReply::Handled();
    }
    if (Key == EKeys::Up || Key == EKeys::Down ||
        Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down)
    {
        const bool bSecond = Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down;
        UButton* Target = nullptr;
        if (CurrentMode5M == 0)
            Target = bSecond ? AbortButton5M : ResumeButton5M;
        else if (CurrentMode5M == 1)
            Target = bSecond ? CancelAbortButton5M : ConfirmAbortButton5M;
        else if (CurrentMode5M == 2)
            Target = bSecond ? MainMenuButton6A : PlayButton5M;
        else
        {
            if (!bSecond) Target = PlayButton5M;
            else if (PlayButton5M && PlayButton5M->HasKeyboardFocus())
                Target = MainMenuButton6A;
            else Target = SeeFieldButton5M;
        }
        if (Target) Target->SetKeyboardFocus();
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom ||
        Key == EKeys::SpaceBar)
    {
        if (CurrentMode5M == 0)
        {
            if (AbortButton5M && AbortButton5M->HasKeyboardFocus()) HandleAbort5M();
            else HandleResume5M();
        }
        else if (CurrentMode5M == 1)
        {
            if (ConfirmAbortButton5M && ConfirmAbortButton5M->HasKeyboardFocus())
                HandleConfirmAbort5M();
            else HandleCancelAbort5M();
        }
        else if (CurrentMode5M >= 2 && MainMenuButton6A &&
            MainMenuButton6A->HasKeyboardFocus()) HandleMainMenu6A();
        else if (CurrentMode5M == 3 && SeeFieldButton5M &&
            SeeFieldButton5M->HasKeyboardFocus()) HandleSeeField5M();
        else HandlePlay5M();
        return FReply::Handled();
    }
    if (Key == EKeys::F11)
    {
        if (APlayerController* PC = GetOwningPlayer()) PC->ConsoleCommand(TEXT("fullscreen"), true);
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}
