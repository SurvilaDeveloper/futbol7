#include "Rondo/SoccerRondoResultWidget8.h"
#include "Rondo/SoccerRondoGameMode.h"
#include "SoccerCharacterBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

namespace SoccerRondoResultUI8
{
    const FLinearColor White(0.96f,0.98f,0.99f,1.f);
    const FLinearColor Muted(0.67f,0.81f,0.84f,1.f);
    const FLinearColor Green(0.32f,0.82f,0.52f,1.f);
    const FLinearColor Secondary(0.12f,0.29f,0.35f,1.f);
    const FLinearColor DarkText(0.018f,0.13f,0.085f,1.f);

    static UTextBlock* Label(UWidgetTree* Tree, const FString& Text,
        int32 FontSize, const FLinearColor& Color,
        ETextJustify::Type Justify = ETextJustify::Center)
    {
        UTextBlock* Result = Tree->ConstructWidget<UTextBlock>();
        Result->SetText(FText::FromString(Text));
        FSlateFontInfo Font = Result->Font;
        Font.Size = FontSize;
        Result->SetFont(Font);
        Result->SetColorAndOpacity(FSlateColor(Color));
        Result->SetJustification(Justify);
        return Result;
    }

    static void AddRow(UVerticalBox* List, UWidget* Child, float Bottom)
    {
        UVerticalBoxSlot* RowSlot = List->AddChildToVerticalBox(Child);
        RowSlot->SetHorizontalAlignment(HAlign_Fill);
        RowSlot->SetPadding(FMargin(0.f,0.f,0.f,Bottom));
    }

    static UButton* Action(UWidgetTree* Tree, const FString& Text, bool bPrimary)
    {
        UButton* Result = Tree->ConstructWidget<UButton>();
        Result->SetBackgroundColor(bPrimary ? Green : Secondary);
        USizeBox* Frame = Tree->ConstructWidget<USizeBox>();
        Frame->SetMinDesiredHeight(49.f);
        Frame->SetContent(Label(Tree,Text,20,bPrimary ? DarkText : White));
        Result->AddChild(Frame);
        return Result;
    }

    static void AddScoreRow(UWidgetTree* Tree, UVerticalBox* List,
        const FString& PlayerLabel, const FString& ScoreLabel,
        const FLinearColor& Color, bool bHeader = false)
    {
        UBorder* Back = Tree->ConstructWidget<UBorder>();
        Back->SetBrushColor(bHeader ? FLinearColor(0.035f,0.082f,0.106f,1.f)
            : FLinearColor(0.065f,0.15f,0.18f,1.f));
        Back->SetPadding(FMargin(12.f,5.f));
        UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>();
        Back->SetContent(Row);
        UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(
            Label(Tree,PlayerLabel,bHeader ? 15 : 18,Color,ETextJustify::Left));
        NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        NameSlot->SetVerticalAlignment(VAlign_Center);
        USizeBox* ScoreWidth = Tree->ConstructWidget<USizeBox>();
        ScoreWidth->SetWidthOverride(90.f);
        ScoreWidth->SetContent(Label(Tree,ScoreLabel,bHeader ? 15 : 18,Color,ETextJustify::Right));
        UHorizontalBoxSlot* ScoreSlot = Row->AddChildToHorizontalBox(ScoreWidth);
        ScoreSlot->SetVerticalAlignment(VAlign_Center);
        AddRow(List,Back,4.f);
    }
}

void USoccerRondoResultWidget8::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    bIsFocusable = true;
    BuildResult();
}

void USoccerRondoResultWidget8::BuildResult()
{
    if (!WidgetTree || WidgetTree->RootWidget) return;
    ASoccerRondoGameMode* Mode = ASoccerRondoGameMode::Find(this);
    if (!IsValid(Mode) || !Mode->IsFinished()) return;
    using namespace SoccerRondoResultUI8;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.012f,0.035f,0.047f,0.97f));
    UCanvasPanelSlot* BGSlot = Root->AddChildToCanvas(Background);
    BGSlot->SetAnchors(FAnchors(0.f,0.f,1.f,1.f));
    BGSlot->SetOffsets(FMargin(0.f));
    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Background->SetContent(Scale);
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(690.f);
    // Natural height lets the scalable panel accommodate all seven rows.
    Scale->AddChild(Size);
    if (UScaleBoxSlot* PanelSlot = Cast<UScaleBoxSlot>(Size->Slot))
    {
        PanelSlot->SetHorizontalAlignment(HAlign_Center);
        PanelSlot->SetVerticalAlignment(VAlign_Center);
    }
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetBrushColor(FLinearColor(0.035f,0.082f,0.106f,1.f));
    Panel->SetPadding(FMargin(32.f,24.f));
    Size->SetContent(Panel);
    UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(List);
    AddRow(List,Label(WidgetTree,TEXT("EL LOCO"),38,White),3.f);
    AddRow(List,Label(WidgetTree,TEXT("RESULTADO FINAL"),18,Muted),15.f);
    AddScoreRow(WidgetTree,List,TEXT("Puesto / Jugador"),TEXT("Toques"),Muted,true);
    const auto Ranked = Mode->GetRankedPlayers();
    const APawn* HumanPawn = GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr;
    int32 Place = 0, PreviousScore = -1;
    for (int32 Index=0; Index<Ranked.Num(); ++Index)
    {
        const auto* Player = Ranked[Index];
        const int32 Score = Mode->GetTouches(Player);
        if (Score != PreviousScore) Place = Index+1;
        PreviousScore = Score;
        const bool bHuman = Player == HumanPawn;
        const FLinearColor Color = Place==1 ? FLinearColor(1.f,0.82f,0.3f,1.f)
            : bHuman ? Green : White;
        AddScoreRow(WidgetTree,List,FString::Printf(TEXT("%d. %s%s"),Place,
            *ASoccerRondoGameMode::GetParticipantName(Player),bHuman ? TEXT(" (Vos)") : TEXT("")),
            FString::FromInt(Score),Color);
    }
    ReplayButton = Action(WidgetTree,TEXT("VOLVER A JUGAR"),true);
    ReplayButton->OnClicked.AddDynamic(this,&USoccerRondoResultWidget8::Replay);
    AddRow(List,ReplayButton,8.f);
    MainMenuButton = Action(WidgetTree,TEXT("MENÚ PRINCIPAL"),false);
    MainMenuButton->OnClicked.AddDynamic(this,&USoccerRondoResultWidget8::MainMenu);
    AddRow(List,MainMenuButton,12.f);
    AddRow(List,Label(WidgetTree,TEXT("Flechas / cruceta: navegar  ·  Enter / A: elegir"),13,Muted),0.f);
    ErrorLabel = Label(WidgetTree,TEXT(""),14,FLinearColor(1.f,0.5f,0.4f,1.f));
    ErrorLabel->SetAutoWrapText(true);
    ErrorLabel->SetVisibility(ESlateVisibility::Collapsed);
    AddRow(List,ErrorLabel,0.f);
}

void USoccerRondoResultWidget8::FocusFirstAction()
{
    if (IsValid(ReplayButton)) ReplayButton->SetKeyboardFocus();
}

void USoccerRondoResultWidget8::ToggleFocus()
{
    if (IsValid(ReplayButton) && ReplayButton->HasKeyboardFocus() && IsValid(MainMenuButton))
        MainMenuButton->SetKeyboardFocus();
    else FocusFirstAction();
}

void USoccerRondoResultWidget8::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    using namespace SoccerRondoResultUI8;
    if (IsValid(ReplayButton)) ReplayButton->SetBackgroundColor(ReplayButton->HasKeyboardFocus()
        ? FLinearColor(0.46f,0.98f,0.62f,1.f) : Green);
    if (IsValid(MainMenuButton)) MainMenuButton->SetBackgroundColor(MainMenuButton->HasKeyboardFocus()
        ? FLinearColor(0.22f,0.52f,0.55f,1.f) : Secondary);
}

void USoccerRondoResultWidget8::Replay()
{
    if (!GetWorld()) return;
    // Same active-map resolution used by the training restart; strips PIE prefix.
    const FString Folder = FPackageName::GetLongPackagePath(GetWorld()->GetOutermost()->GetName());
    const FString ShortName = UGameplayStatics::GetCurrentLevelName(this,true);
    if (!Folder.IsEmpty() && !ShortName.IsEmpty()) TravelTo(Folder + TEXT("/") + ShortName);
    else UE_LOG(LogTemp,Error,TEXT("[Rondo8] Cannot resolve current map for replay"));
}

void USoccerRondoResultWidget8::MainMenu()
{
    TravelTo(TEXT("/Game/ThirdPersonCPP/Maps/SoccerMainMenuMap"));
}

void USoccerRondoResultWidget8::TravelTo(const FString& MapPackage)
{
    ASoccerRondoGameMode* Mode = ASoccerRondoGameMode::Find(this);
    if (bTravelRequested || !IsValid(Mode) || !Mode->IsFinished()) return;
    // Keep the finished session paused if a required map is missing.
    if (!FPackageName::DoesPackageExist(MapPackage))
    {
        UE_LOG(LogTemp,Error,TEXT("[Rondo8] Map not found: %s"),*MapPackage);
        if (IsValid(ErrorLabel))
        {
            ErrorLabel->SetText(FText::FromString(TEXT("No se encontró el mapa: ") + MapPackage));
            ErrorLabel->SetVisibility(ESlateVisibility::Visible);
        }
        return;
    }
    bTravelRequested = true;
    if (IsValid(ReplayButton)) ReplayButton->SetIsEnabled(false);
    if (IsValid(MainMenuButton)) MainMenuButton->SetIsEnabled(false);
    UE_LOG(LogTemp,Display,TEXT("[Rondo8] Opening %s"),*MapPackage);
    UGameplayStatics::SetGamePaused(this,false);
    // Do not reactivate the old pawn. The new map initializes its own input and scores.
    UGameplayStatics::OpenLevel(this,FName(*MapPackage),true);
}

FReply USoccerRondoResultWidget8::NativeOnPreviewKeyDown(
    const FGeometry& Geometry,const FKeyEvent& KeyEvent)
{
    const FKey Key = KeyEvent.GetKey();
    if (Key == EKeys::P)
    {
        MainMenu();
        return FReply::Handled();
    }
    if (Key == EKeys::F11)
    {
        if (APlayerController* PC = GetOwningPlayer()) PC->ConsoleCommand(TEXT("fullscreen"),true);
        return FReply::Handled();
    }
    if (Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::W || Key == EKeys::S ||
        Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down)
    {
        ToggleFocus();
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (IsValid(MainMenuButton) && MainMenuButton->HasKeyboardFocus()) MainMenu();
        else Replay();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry,KeyEvent);
}

FReply USoccerRondoResultWidget8::NativeOnAnalogValueChanged(
    const FGeometry& Geometry,const FAnalogInputEvent& AnalogEvent)
{
    if (AnalogEvent.GetKey() == EKeys::Gamepad_LeftY && FMath::Abs(AnalogEvent.GetAnalogValue()) >= 0.6f)
    {
        const double Now = FPlatformTime::Seconds();
        if (LastAnalogNavigation < 0.0 || Now-LastAnalogNavigation >= 0.23)
        {
            ToggleFocus();
            LastAnalogNavigation = Now;
        }
        return FReply::Handled();
    }
    return Super::NativeOnAnalogValueChanged(Geometry,AnalogEvent);
}
