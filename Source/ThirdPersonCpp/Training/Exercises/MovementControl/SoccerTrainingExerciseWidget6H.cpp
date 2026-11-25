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
#include "Components/ScrollBox.h"
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
    Size->SetWidthOverride(900.f);
    Size->SetHeightOverride(580.f);
    Scale->AddChild(Size);
    if (UScaleBoxSlot* ScaleSlot = Cast<UScaleBoxSlot>(Size->Slot))
    {
        ScaleSlot->SetHorizontalAlignment(HAlign_Center);
        ScaleSlot->SetVerticalAlignment(VAlign_Center);
    }
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetPadding(FMargin(30.f, 22.f));
    Panel->SetBrushColor(FLinearColor(0.035f, 0.09f, 0.105f, 1.f));
    Size->SetContent(Panel);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Content);
    // 6Q: The title and controls stay visible. Only the instructions/results scroll.
    Header6H = MakeText(WidgetTree, TEXT("CONTROL DE PELOTA"), 28, FLinearColor::White);
    Add(Content, Header6H, 12.f);

    UScrollBox* TextScroll6Q = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("InstructionsScroll6Q"));
    TextScroll6Q->SetOrientation(EOrientation::Orient_Vertical);
    TextScroll6Q->SetScrollBarVisibility(ESlateVisibility::Visible);
    TextScroll6Q->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    UVerticalBoxSlot* ScrollSlot6Q = Content->AddChildToVerticalBox(TextScroll6Q);
    ScrollSlot6Q->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ScrollSlot6Q->SetHorizontalAlignment(HAlign_Fill);
    ScrollSlot6Q->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    UVerticalBox* ScrollContent6Q = WidgetTree->ConstructWidget<UVerticalBox>();
    TextScroll6Q->AddChild(ScrollContent6Q);
    Body6H = MakeText(WidgetTree, TEXT(""), 19, FLinearColor(0.86f, 0.94f, 0.96f));
    Add(ScrollContent6Q, Body6H, 18.f);
    Feedback6H = MakeText(WidgetTree, TEXT(""), 18, FLinearColor(0.97f, 0.82f, 0.38f));
    Add(ScrollContent6Q, Feedback6H, 8.f);
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
    if (Header6H) Header6H->SetText(FText::FromString(Mode && Mode->IsAerialPassLesson6AO() ? TEXT("ENTRENAMIENTO | PASE AÉREO") : Mode && Mode->IsGroundPassLesson6AN() ? TEXT("ENTRENAMIENTO | PASE RASANTE") : Mode && Mode->IsAvoidTackleLesson6AK() ? TEXT("ENTRENAMIENTO | ESQUIVAR TACKLES") : Mode && Mode->IsWinTackleLesson6AK() ? TEXT("ENTRENAMIENTO | RECUPERAR CON TACKLE") : Mode && Mode->IsZoneFinishLesson6AH() ? TEXT("ENTRENAMIENTO | CONDUCCIÓN Y DEFINICIÓN") : Mode && Mode->IsGoalkeeperShotLesson6T() ? TEXT("ENTRENAMIENTO | REMATES CONTRA ARQUERO") : bShot ? TEXT("ENTRENAMIENTO | REMATE CON CARGA") : bAutoPass
        ? TEXT("ENTRENAMIENTO | AUTOPASE")
        : TEXT("ENTRENAMIENTO | CONTROL DE PELOTA")));
    if (Body6H)
    {
        FSlateFontInfo BodyFont6P = Body6H->Font;
        BodyFont6P.Size = 19;
        Body6H->SetFont(BodyFont6P);
    }
    if (Body6H) Body6H->SetText(FText::FromString((Mode && Mode->IsAerialPassLesson6AO())
        ? TEXT("PEDIDO DE PASE AÉREO Y DEFINICIÓN — 6AO\n\n"
               "1. El compañero conduce cerca de la banda y el defensor lo marca sin hacerle tackles. Pulsá R para pedir un pase aéreo; el pedido espera hasta que pueda patear.\n\n"
               "2. Cuando salga el pase, hacé clic izquierdo para pedir una recepción controlada con el pecho o la cabeza. También podés apuntar al arco, mantener el clic derecho y soltar para intentar un cabezazo dirigido (con salto si el plan lo requiere) o un remate de primera. La IA del personaje selecciona el contacto viable.\n\n"
               "3. Si recibís, controlá la pelota y definí con clic derecho. El defensor puede interceptar y hacerte tackles. Q permite recuperar una pelota suelta; Espacio permite esquivar.\n\n"
               "Diez intentos con altura y presión crecientes. Solo cuenta el gol tras un golpeo real del humano. Diez segundos para recibir o rematar, veinte segundos desde la recepción para definir. Las faltas del defensor y fallos técnicos no descuentan intentos.")
        : (Mode && Mode->IsGroundPassLesson6AN())
        ? TEXT("PEDIDO DE PASE Y DEFINICIÓN BAJO PRESIÓN — 6AN.2\n\n"
               "1. El compañero conduce cerca de una banda mientras un rival lo marca sin hacerle tackles. Empezás más lejos: acercate si hace falta y pulsá F para pedir un pase rasante. El pedido queda pendiente hasta que pueda patear.\n\n"
               "2. Cuando salga el pase, hacé clic izquierdo para ir a recibirlo. También podés rematar de primera con clic derecho. El defensor cambia de objetivo, puede interceptar el pase y hacerte tackles cuando controlás la pelota.\n\n"
               "3. Conducí, usá autopases o esquivá al defensor y rematá al arco rival vacío con clic derecho. Q permite intentar recuperar una pelota suelta. Espacio permite saltar.\n\n"
               "Diez intentos con presión creciente y bandas alternadas. Solo vale el gol tras un remate real del humano. Tenés ocho segundos para recibir o rematar y veinte segundos desde la recepción para definir. Si la pelota sale antes del pase o el defensor comete falta, se repite sin penalización.")
        : (Mode && Mode->IsTackleLesson6AK())
        ? (Mode && Mode->IsAvoidTackleLesson6AK()
            ? TEXT("ESQUIVAR TACKLES — 6AL.6.3\n\n"
                   "Empezás junto a tu arco y buscás la pelota en el punto penal. "
                   "Atravesá cuatro zonas: la zona 0 es libre y las zonas 1, 2 y 3 tienen un defensor cada una. "
                   "Solo te persigue el defensor de la zona donde está la pelota. "
                   "Los otros dos se acomodan para interceptar la trayectoria hacia el arco rival.\n\n"
                   "Los tres defensores corren un 12,5 % más rápido en este ejercicio. "
                   "Podés saltar los tackles o cambiar de dirección. "
                   "Para tener éxito, vos y la pelota deben cruzar la línea de gol e ingresar juntos al arco rival CON POSESIÓN. "
                   "El intento también termina si un defensor recupera y despeja afuera, hay falta o la pelota sale del campo. "
                   "Diez intentos; los saltos se registran por separado.")
            : TEXT("RECUPERAR CON TACKLE — 6AM.8\n\n"
                   "Diez intentos. El rival comienza cerca de su arco y conduce hasta el tuyo "
                   "con autopases cortos y largos, sin detenerse para patear. Puede saltar para "
                   "esquivar tus tackles. Aparecés cerca de mitad de cancha, alternando de lateral.\n\n"
                   "Objetivo: recuperá realmente la pelota mediante un tackle LIMPIO. "
                   "Si la tocás sin falta y después obtenés la posesión: 3 puntos. "
                   "Si la desviás limpiamente pero no la recuperás: 1 punto. "
                   "Si cometés falta, no tocás la pelota o la recogés sin un tackle "
                   "limpio: 0 puntos.\n\n"
                   "Cada intento termina al recuperar la posesión, cometer falta, "
                   "recibir gol o salir la pelota. Al décimo intento verás "
                   "la puntuación y el detalle de cada jugada."))
        : (Mode && Mode->IsZoneFinishLesson6AH())
        ? TEXT("Diez jugadas de conducción y definición. Empezás en el centro de la cancha, con la pelota unos metros adelante.\n\n"
               "Con clic izquierdo podés buscar la pelota y hacer autopases. También podés conducirla con I, J, K y L. "
               "Llevá la pelota hasta el círculo amarillo de cada intento: cuando quede COMPLETAMENTE dentro se pondrá verde.\n\n"
               "Podés apuntar y mantener el botón derecho para cargar fuerza desde cualquier lugar, incluso mientras la pelota se mueve. "
               "Al golpear, la pelota debe SALIR desde dentro del círculo: un remate desde fuera no vale, aunque entre al arco. "
               "Si pateás desde fuera, la jugada continúa pero el remate no cuenta como válido.\n\n"
               "El arquero puede salir a interceptar la pelota si queda suelta cerca del arco. "
               "Las diez zonas cambian de lugar y al terminar verás las estadísticas.")
        : (Mode && Mode->IsGoalkeeperShotLesson6T())
        ? TEXT("Practicarás diez remates contra un arquero, desde diferentes posiciones.\n\n"
               "No hace falta tomar posesión: apunta con la cruz verde, mantén presionado el botón derecho "
               "para cargar la fuerza y suéltalo. El jugador irá hacia la pelota para rematar.\n\n"
               "El arquero intentará cubrir el arco. Observa sus desplazamientos, varía la puntería "
               "y aprovecha la barra de fuerza. Después de cada intento, el jugador y la pelota "
               "cambiarán de posición. Al terminar verás goles, atajadas y disparos fallidos.\n\n"
               "Este primer entrenamiento utiliza un arquero autónomo simplificado: "
               "sus animaciones y su aspecto provienen del arquero del partido, pero "
               "sus decisiones todavía no son las mismas que las del SoccerMatchManager.")
        : bShot
        ? TEXT("Vas a realizar diez remates al arco desde distintas posiciones. Los primeros cuatro son libres; los seis siguientes incluyen barreras.\n\n"
               "No necesitas tomar posesión. Con el mouse, apunta al arco usando la cruz verde "
               "(sin cursor de Windows). Mantén presionado el botón derecho para cargar "
               "la fuerza y suéltalo: el jugador correrá hacia la pelota y rematará.\n\n"
               "Después de cada intento, la pelota aparecerá en otra posición. "
               "Apunta al punto de caída y observa la barra de fuerza mientras cargas: una carga menor puede producir una trayectoria más alta. "
               "Intenta superar las barreras sin pasar por encima del arco. Al final verás el total de aciertos.")
        : bAutoPass
        ? TEXT("Haz clic izquierdo una vez para que el jugador se acerque y tome posesión de la pelota. "
               "El cono verde indica que la está controlando.\n\n"
               "Usa la cruz verde de punteria para apuntar a la primera marca amarilla "
               "y haz clic izquierdo otra vez: el jugador enviará la pelota hacia el lugar elegido "
               "y correrá tras ella para recuperarla.\n\n"
               "Repite el autopase hacia cada marca, en orden, en el menor tiempo posible. "
               "El botón derecho está deshabilitado en este ejercicio.")
        : TEXT("Haz clic izquierdo una vez para que el jugador vaya a buscar la pelota. "
               "Cuando aparezca el cono verde sobre su cabeza, significa que tiene la posesión.\n\n"
               "Luego, usa I, J, K y L para moverlo y conducir la pelota. "
               "Conservarás la posesión mientras la pelota permanezca dentro de la distancia máxima permitida.\n\n"
               "Haz pasar la pelota por las cuatro marcas amarillas, en orden, "
               "en el menor tiempo posible.")));
    if (Feedback6H) Feedback6H->SetText(FText::FromString(
        TEXT("El cronómetro comienza al pulsar Empezar ejercicio.")));
    if (PrimaryLabel6H) PrimaryLabel6H->SetText(FText::FromString(TEXT("EMPEZAR EJERCICIO")));
    SetVisibility(ESlateVisibility::Visible);
    // Scroll from the beginning when showing a new screen.
    if (UScrollBox* Scroll = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("InstructionsScroll6Q"))))
    {
        Scroll->ScrollToStart();
    }
    FocusPrimary6H();
}

void USoccerTrainingExerciseWidget6H::ShowResults6H()
{
    bResults6H = true;
    const ASoccerTrainingMovementGameMode6B* Mode = GetWorld()
        ? Cast<ASoccerTrainingMovementGameMode6B>(GetWorld()->GetAuthGameMode()) : nullptr;
    const bool bAutoPass = Mode && Mode->IsAutoPassLesson6J();
    const bool bShot = Mode && Mode->IsChargedShotLesson6M();
    if (Header6H) Header6H->SetText(FText::FromString(Mode && Mode->IsAerialPassLesson6AO() ? TEXT("RESULTADOS | PASE AÉREO") : Mode && Mode->IsGroundPassLesson6AN() ? TEXT("RESULTADOS | PASE RASANTE") : Mode && Mode->IsAvoidTackleLesson6AK() ? TEXT("RESULTADOS | ESQUIVAR TACKLES") : Mode && Mode->IsWinTackleLesson6AK() ? TEXT("RESULTADOS | RECUPERAR CON TACKLE") : Mode && Mode->IsZoneFinishLesson6AH() ? TEXT("RESULTADOS | DEFINICIÓN POR ZONAS") : Mode && Mode->IsGoalkeeperShotLesson6T() ? TEXT("RESULTADOS CONTRA ARQUERO") : bShot ? TEXT("RESULTADO DEL REMATE") : bAutoPass
        ? TEXT("AUTOPASES COMPLETADOS") : TEXT("RECORRIDO COMPLETADO")));
    if (Mode)
    {
        if (Body6H)
        {
            // The ten-shot report needs a smaller font to remain visible
            // inside the existing 6H results panel.
            FSlateFontInfo BodyFont6P = Body6H->Font;
            BodyFont6P.Size = (bShot || (Mode && Mode->IsTackleLesson6AK())) ? 17 : 19;
            Body6H->SetFont(BodyFont6P);
            const FString Summary6H = (Mode && Mode->IsPassLesson6AO())
                ? Mode->GetGroundPassReport6AN()
                : (Mode && Mode->IsAvoidTackleLesson6AK())
                ? Mode->GetAvoidTackleReport6AL2()
                : (Mode && Mode->IsWinTackleLesson6AK())
                ? Mode->GetWinTackleReport6AM8()
                : bShot
                ? FString::Printf(TEXT("Remates realizados: %d / %d\nGoles: %d\nFallos: %d\nEfectividad: %.0f%%\n\nTiempo total: %.1f segundos"),
                    Mode->GetShotsCompleted6N(), Mode->GetShotAttemptsTotal6N(),
                    Mode->GetGoalsScored6N(), Mode->GetShotsCompleted6N() - Mode->GetGoalsScored6N(),
                    100.f * Mode->GetGoalsScored6N() / FMath::Max(1, Mode->GetShotsCompleted6N()),
                    Mode->GetElapsedTime6B()) + TEXT("\n\n") + Mode->GetShotResultsReport6P()
                : bAutoPass
                ? FString::Printf(TEXT("Completaste las cuatro marcas con autopases.\n\nTiempo empleado: %.1f segundos\nPérdidas de control: %d"),
                    Mode->GetElapsedTime6B(), Mode->GetLostControlCount6B())
                : FString::Printf(TEXT("Completaste las cuatro marcas.\n\nTiempo empleado: %.1f segundos\nPérdidas de control: %d"),
                    Mode->GetElapsedTime6B(), Mode->GetLostControlCount6B());
            Body6H->SetText(FText::FromString(Summary6H));
        }
        if (Feedback6H)
        {
            const FString Result = (Mode && Mode->IsPassLesson6AO())
                ? TEXT("Un acierto requiere un gol tras un remate del humano. La recepción y el remate de primera son válidos.")
                : (Mode && Mode->IsAvoidTackleLesson6AK())
                ? TEXT("Se evaluaron diez recorridos: entrada al arco rival con la pelota, recuperación y despeje, falta o pelota fuera.")
                : (Mode && Mode->IsWinTackleLesson6AK())
                ? TEXT("3 pts: tackle limpio y recuperacion. 1 pto: toque limpio sin posesion. 0 pts: falta o fallo.")
                : bShot ? TEXT("Revisa la causa de cada resultado. Sin resolucion significa que no se observo el cruce ni un contacto con barrera.")
                : !Mode->IsTimeEvaluationEnabled6H()
                ? TEXT("Evaluación pendiente: todavía no definimos el tiempo de aprobación.")
                : Mode->GetElapsedTime6B() <= Mode->GetApprovalTime6H()
                    ? TEXT("¡Muy bien! Completaste el recorrido dentro del tiempo previsto.")
                    : TEXT("Tardaste más de lo previsto. ¡Inténtalo nuevamente!");
            Feedback6H->SetText(FText::FromString(Result));
        }
    }
    if (PrimaryLabel6H) PrimaryLabel6H->SetText(FText::FromString(TEXT("REPETIR EJERCICIO")));
    SetVisibility(ESlateVisibility::Visible);
    // Scroll from the beginning when showing a new screen.
    if (UScrollBox* Scroll = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("InstructionsScroll6Q"))))
    {
        Scroll->ScrollToStart();
    }
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
    // PageUp / PageDown also work while a button holds keyboard focus.
    if (Key == EKeys::PageDown || Key == EKeys::PageUp)
    {
        if (UScrollBox* Scroll = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("InstructionsScroll6Q"))))
        {
            const float Step = Key == EKeys::PageDown ? 220.f : -220.f;
            Scroll->SetScrollOffset(FMath::Max(0.f, Scroll->GetScrollOffset() + Step));
        }
        return FReply::Handled();
    }
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
