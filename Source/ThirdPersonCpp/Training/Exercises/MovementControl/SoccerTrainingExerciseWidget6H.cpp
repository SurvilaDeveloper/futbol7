#include "Training/Exercises/MovementControl/SoccerTrainingExerciseWidget6H.h"
#include "Training/Exercises/MovementControl/SoccerTrainingMovementGameMode6B.h"
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
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"

namespace TrainingExerciseUI6H
{
    static UTextBlock* MakeText(UWidgetTree* Tree, const FString& Value, int32 Size, FLinearColor Color)
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Value));
        FSlateFontInfo Font = Text->Font;
        Font.Size = Size;
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetJustification(ETextJustify::Center);
        Text->SetAutoWrapText(true);
        return Text;
    }
    static void Add(UVerticalBox* Box, UWidget* Child, float Bottom)
    {
        UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child);
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
    }
}

void USoccerTrainingExerciseWidget6H::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    bIsFocusable = true;
    if (!WidgetTree || WidgetTree->RootWidget) return;
    using namespace TrainingExerciseUI6H;
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
    Backdrop->SetBrushColor(FLinearColor(0.01f, 0.035f, 0.04f, 0.92f));
    UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(Backdrop);
    CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    CanvasSlot->SetOffsets(FMargin(0.f));

    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Backdrop->SetContent(Scale);
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(780.f);
    Size->SetHeightOverride(580.f);
    Scale->AddChild(Size);
    if (UScaleBoxSlot* ScaleSlot = Cast<UScaleBoxSlot>(Size->Slot))
    {
        ScaleSlot->SetHorizontalAlignment(HAlign_Center);
        ScaleSlot->SetVerticalAlignment(VAlign_Center);
    }
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetPadding(FMargin(42.f, 34.f));
    Panel->SetBrushColor(FLinearColor(0.035f, 0.09f, 0.105f, 1.f));
    Size->SetContent(Panel);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Content);
    Header6H = MakeText(WidgetTree, TEXT("CONTROL DE PELOTA"), 31, FLinearColor::White);
    Add(Content, Header6H, 22.f);
    Body6H = MakeText(WidgetTree, TEXT(""), 20, FLinearColor(0.86f, 0.94f, 0.96f));
    Add(Content, Body6H, 24.f);
    Feedback6H = MakeText(WidgetTree, TEXT(""), 20, FLinearColor(0.97f, 0.82f, 0.38f));
    Add(Content, Feedback6H, 24.f);
    PrimaryButton6H = WidgetTree->ConstructWidget<UButton>();
    PrimaryButton6H->SetBackgroundColor(FLinearColor(0.3f, 0.78f, 0.5f));
    USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>();
    ButtonSize->SetMinDesiredHeight(68.f);
    PrimaryLabel6H = MakeText(WidgetTree, TEXT("EMPEZAR EJERCICIO"), 23,
        FLinearColor(0.02f, 0.13f, 0.08f));
    ButtonSize->SetContent(PrimaryLabel6H);
    PrimaryButton6H->AddChild(ButtonSize);
    PrimaryButton6H->OnClicked.AddDynamic(this, &USoccerTrainingExerciseWidget6H::PrimaryClicked6H);
    Add(Content, PrimaryButton6H, 14.f);
    BackButton6H = WidgetTree->ConstructWidget<UButton>();
    BackButton6H->SetBackgroundColor(FLinearColor(0.12f, 0.27f, 0.32f));
    USizeBox* BackSize = WidgetTree->ConstructWidget<USizeBox>();
    BackSize->SetMinDesiredHeight(48.f);
    BackSize->SetContent(MakeText(WidgetTree, TEXT("VOLVER A ELEGIR EJERCICIO"), 16,
        FLinearColor::White));
    BackButton6H->AddChild(BackSize);
    BackButton6H->OnClicked.AddDynamic(this, &USoccerTrainingExerciseWidget6H::BackClicked6H);
    Add(Content, BackButton6H, 8.f);
    Add(Content, MakeText(WidgetTree,
        TEXT("Flechas / cruceta: elegir   |   Enter / A: confirmar   |   Esc / B: volver"),
        14, FLinearColor(0.67f, 0.81f, 0.84f)), 0.f);
}

void USoccerTrainingExerciseWidget6H::ShowInstructions6H()
{
    bResults6H = false;
    const ASoccerTrainingMovementGameMode6B* Mode = GetWorld()
        ? Cast<ASoccerTrainingMovementGameMode6B>(GetWorld()->GetAuthGameMode()) : nullptr;
    const bool bAutoPass = Mode && Mode->IsAutoPassLesson6J();
    const bool bShot = Mode && Mode->IsChargedShotLesson6M();
    if (Header6H) Header6H->SetText(FText::FromString(bShot ? TEXT("ENTRENAMIENTO | REMATE CON CARGA") : bAutoPass
        ? TEXT("ENTRENAMIENTO | AUTOPASE")
        : TEXT("ENTRENAMIENTO | CONTROL DE PELOTA")));
    if (Body6H) Body6H->SetText(FText::FromString(bShot
        ? TEXT("Vas a realizar un remate al arco sin arquero.\n\n"
               "Haz clic izquierdo para buscar la pelota. El cono verde confirma la posesion.\n\n"
               "Con el mouse, apunta al arco usando la cruz verde (sin cursor de Windows). "
               "Mant\u00e9n presionado el boton derecho para cargar la fuerza y sueltalo para patear.\n\n"
               "Se registrara si fue gol, la fuerza de carga y el tiempo. "
               "Puedes repetir el ejercicio desde el resultado.")
        : bAutoPass
        ? TEXT("Haz clic izquierdo una vez para que el jugador se acerque y tome posesi\u00f3n de la pelota. "
               "El cono verde indica que la est\u00e1 controlando.\n\n"
               "Usa la cruz verde de punteria para apuntar a la primera marca amarilla "
               "y haz clic izquierdo otra vez: el jugador enviar\u00e1 la pelota hacia el lugar elegido "
               "y correr\u00e1 tras ella para recuperarla.\n\n"
               "Repite el autopase hacia cada marca, en orden, en el menor tiempo posible. "
               "El bot\u00f3n derecho est\u00e1 deshabilitado en este ejercicio.")
        : TEXT("Haz clic izquierdo una vez para que el jugador vaya a buscar la pelota. "
               "Cuando aparezca el cono verde sobre su cabeza, significa que tiene la posesi\u00f3n.\n\n"
               "Luego, usa I, J, K y L para moverlo y conducir la pelota. "
               "Conservar\u00e1s la posesi\u00f3n mientras la pelota permanezca dentro de la distancia m\u00e1xima permitida.\n\n"
               "Haz pasar la pelota por las cuatro marcas amarillas, en orden, "
               "en el menor tiempo posible.")));
    if (Feedback6H) Feedback6H->SetText(FText::FromString(
        TEXT("El cron\u00f3metro comienza al pulsar Empezar ejercicio.")));
    if (PrimaryLabel6H) PrimaryLabel6H->SetText(FText::FromString(TEXT("EMPEZAR EJERCICIO")));
    SetVisibility(ESlateVisibility::Visible);
    FocusPrimary6H();
}

void USoccerTrainingExerciseWidget6H::ShowResults6H()
{
    bResults6H = true;
    const ASoccerTrainingMovementGameMode6B* Mode = GetWorld()
        ? Cast<ASoccerTrainingMovementGameMode6B>(GetWorld()->GetAuthGameMode()) : nullptr;
    const bool bAutoPass = Mode && Mode->IsAutoPassLesson6J();
    const bool bShot = Mode && Mode->IsChargedShotLesson6M();
    if (Header6H) Header6H->SetText(FText::FromString(bShot ? TEXT("RESULTADO DEL REMATE") : bAutoPass
        ? TEXT("AUTOPASES COMPLETADOS") : TEXT("RECORRIDO COMPLETADO")));
    if (Mode)
    {
        if (Body6H)
        {
            const FString Summary6H = bShot
                ? FString::Printf(TEXT("%s\n\nTiempo: %.1f segundos\nCarga de fuerza: %.0f%%"),
                    Mode->WasGoalScored6M() ? TEXT("GOL") : TEXT("NO FUE GOL"),
                    Mode->GetElapsedTime6B(), Mode->GetShotCharge6M() * 100.f)
                : bAutoPass
                ? FString::Printf(TEXT("Completaste las cuatro marcas con autopases.\n\nTiempo empleado: %.1f segundos\nP\u00e9rdidas de control: %d"),
                    Mode->GetElapsedTime6B(), Mode->GetLostControlCount6B())
                : FString::Printf(TEXT("Completaste las cuatro marcas.\n\nTiempo empleado: %.1f segundos\nP\u00e9rdidas de control: %d"),
                    Mode->GetElapsedTime6B(), Mode->GetLostControlCount6B());
            Body6H->SetText(FText::FromString(Summary6H));
        }
        if (Feedback6H)
        {
            const FString Result = bShot ? TEXT("Prueba terminada. Repite para realizar un nuevo remate.")
                : !Mode->IsTimeEvaluationEnabled6H()
                ? TEXT("Evaluaci\u00f3n pendiente: todav\u00eda no definimos el tiempo de aprobaci\u00f3n.")
                : Mode->GetElapsedTime6B() <= Mode->GetApprovalTime6H()
                    ? TEXT("\u00a1Muy bien! Completaste el recorrido dentro del tiempo previsto.")
                    : TEXT("Tardaste m\u00e1s de lo previsto. \u00a1Int\u00e9ntalo nuevamente!");
            Feedback6H->SetText(FText::FromString(Result));
        }
    }
    if (PrimaryLabel6H) PrimaryLabel6H->SetText(FText::FromString(TEXT("REPETIR EJERCICIO")));
    SetVisibility(ESlateVisibility::Visible);
    FocusPrimary6H();
}

void USoccerTrainingExerciseWidget6H::FocusPrimary6H()
{
    if (IsValid(PrimaryButton6H))
    {
        PrimaryButton6H->SetKeyboardFocus();
        if (APlayerController* PC = GetOwningPlayer()) PrimaryButton6H->SetUserFocus(PC);
    }
}

void USoccerTrainingExerciseWidget6H::PrimaryClicked6H()
{
    if (ASoccerTrainingPlayerController6B* PC = Cast<ASoccerTrainingPlayerController6B>(GetOwningPlayer()))
    {
        if (bResults6H) PC->RestartTraining6B();
        else PC->StartExercise6H();
    }
}

void USoccerTrainingExerciseWidget6H::BackClicked6H()
{
    if (ASoccerTrainingPlayerController6B* PC = Cast<ASoccerTrainingPlayerController6B>(GetOwningPlayer()))
        PC->ReturnToExerciseSelector6B();
}

void USoccerTrainingExerciseWidget6H::Navigate6H(int32 Offset)
{
    UButton* Buttons[2] = {PrimaryButton6H, BackButton6H};
    const int32 Current = IsValid(BackButton6H) && BackButton6H->HasKeyboardFocus() ? 1 : 0;
    const int32 Next = (Current + Offset + 2) % 2;
    if (IsValid(Buttons[Next]))
    {
        Buttons[Next]->SetKeyboardFocus();
        if (APlayerController* PC = GetOwningPlayer()) Buttons[Next]->SetUserFocus(PC);
    }
}

void USoccerTrainingExerciseWidget6H::ActivateSelected6H()
{
    if (IsValid(BackButton6H) && BackButton6H->HasKeyboardFocus()) BackClicked6H();
    else PrimaryClicked6H();
}

FReply USoccerTrainingExerciseWidget6H::NativeOnPreviewKeyDown(
    const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    const FKey Key = KeyEvent.GetKey();
    if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::W)
    {
        Navigate6H(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::S)
    {
        Navigate6H(1);
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar ||
        Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        ActivateSelected6H();
        return FReply::Handled();
    }
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    {
        BackClicked6H();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

FReply USoccerTrainingExerciseWidget6H::NativeOnAnalogValueChanged(
    const FGeometry& Geometry, const FAnalogInputEvent& AnalogEvent)
{
    if (AnalogEvent.GetKey() == EKeys::Gamepad_LeftY &&
        FMath::Abs(AnalogEvent.GetAnalogValue()) >= 0.6f)
    {
        const double Now = FPlatformTime::Seconds();
        if (LastAnalogTime6H < 0.0 || Now - LastAnalogTime6H >= 0.23)
        {
            Navigate6H(AnalogEvent.GetAnalogValue() > 0.f ? -1 : 1);
            LastAnalogTime6H = Now;
        }
        return FReply::Handled();
    }
    return Super::NativeOnAnalogValueChanged(Geometry, AnalogEvent);
}

void USoccerTrainingExerciseWidget6H::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (IsValid(PrimaryButton6H))
        PrimaryButton6H->SetBackgroundColor(PrimaryButton6H->HasKeyboardFocus()
            ? FLinearColor(0.55f, 0.99f, 0.69f, 1.f)
            : FLinearColor(0.3f, 0.78f, 0.5f, 1.f));
    if (IsValid(BackButton6H))
        BackButton6H->SetBackgroundColor(BackButton6H->HasKeyboardFocus()
            ? FLinearColor(0.24f, 0.54f, 0.58f, 1.f)
            : FLinearColor(0.12f, 0.27f, 0.32f, 1.f));
}
