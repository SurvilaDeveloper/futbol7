#include "SoccerUIStyleEditorController.h"
#include "SoccerUITheme.h"
#include "SoccerUINavigation.h"
#include "SoccerSquadViewerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/FileManager.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SoccerRuntimeStyleInternal
{
    UTextBlock* Text(UWidgetTree* Tree, const FString& Value, int32 Size)
    {
        UTextBlock* Result = Tree->ConstructWidget<UTextBlock>();
        Result->SetText(FText::FromString(Value));
        FSlateFontInfo Font = Result->Font;
        Font.Size = Size;
        Result->SetFont(Font);
        Result->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        return Result;
    }

    UButton* Action(UWidgetTree* Tree, const TCHAR* Label)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        Button->AddChild(Text(Tree, Label, 13));
        Button->SetBackgroundColor(FLinearColor(.09f, .28f, .35f, 1.f));
        return Button;
    }

    bool ParseHex(const FString& InText, FLinearColor& Out)
    {
        FString Value = InText.TrimStartAndEnd();
        if (!Value.StartsWith(TEXT("#"))) return false;
        Value.RightChopInline(1, false);
        if (Value.Len() != 6 && Value.Len() != 8) return false;
        for (int32 Index = 0; Index < Value.Len(); ++Index)
        {
            if (!FChar::IsHexDigit(Value[Index])) return false;
        }
        const FColor C = FColor::FromHex(FString(TEXT("#")) + Value);
        Out = FLinearColor(C.R / 255.f, C.G / 255.f, C.B / 255.f, C.A / 255.f);
        return true;
    }
}

void USoccerUIStyleFieldInputBridge::Initialize(USoccerUIStyleEditorController* InOwner, FName InKey)
{
    EditorOwner = InOwner;
    FieldKey = InKey;
}

void USoccerUIStyleFieldInputBridge::InitializeViewer(USoccerSquadViewerWidget* InOwner, FName InKey)
{
    ViewerOwner = InOwner;
    FieldKey = InKey;
}

void USoccerUIStyleFieldInputBridge::HandleSliderChanged(float NewValue)
{
    if (EditorOwner) EditorOwner->HandleNumberInput(FieldKey, NewValue);
    if (ViewerOwner) ViewerOwner->HandleStyleSliderInput(FieldKey, NewValue);
}

void USoccerUIStyleFieldInputBridge::HandlePaletteChanged(FString Item, ESelectInfo::Type SelectionType)
{
    if (EditorOwner) EditorOwner->HandlePaletteInput(FieldKey, Item);
    if (ViewerOwner) ViewerOwner->HandleStylePaletteInput(FieldKey, Item);
}

void USoccerUIStyleFieldInputBridge::HandleHexChanged(const FText& NewText)
{
    if (EditorOwner) EditorOwner->HandleHexInput(FieldKey, NewText);
    if (ViewerOwner) ViewerOwner->HandleStyleHexInput(FieldKey, NewText);
}

FSoccerEditableStyleField FSoccerEditableStyleField::Number(
    const TCHAR* InKey, const TCHAR* InLabel, float InDefaultValue, float InMinValue, float InMaxValue)
{
    FSoccerEditableStyleField Field;
    Field.Key = FName(InKey);
    Field.Label = InLabel;
    Field.DefaultValue = FString::SanitizeFloat(InDefaultValue);
    Field.MinValue = InMinValue;
    Field.MaxValue = InMaxValue;
    return Field;
}

FSoccerEditableStyleField FSoccerEditableStyleField::Color(
    const TCHAR* InKey, const TCHAR* InLabel, const TCHAR* InDefaultHex)
{
    FSoccerEditableStyleField Field;
    Field.Key = FName(InKey);
    Field.Label = InLabel;
    Field.DefaultValue = InDefaultHex;
    Field.bColor = true;
    return Field;
}

FString USoccerUIStyleEditorController::ConfigFilePath() const
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SoccerUI"), FileName + TEXT(".ini"));
}

FString USoccerUIStyleEditorController::NormalizeValue(
    const FSoccerEditableStyleField& Field, const FString& Raw) const
{
    if (Field.bColor)
    {
        FLinearColor Result;
        return SoccerRuntimeStyleInternal::ParseHex(Raw, Result)
            ? Raw.TrimStartAndEnd() : Field.DefaultValue;
    }
    float Parsed = 0.f;
    if (!FDefaultValueHelper::ParseFloat(Raw.TrimStartAndEnd(), Parsed) || !FMath::IsFinite(Parsed))
    {
        return Field.DefaultValue;
    }
    return FString::SanitizeFloat(FMath::Clamp(Parsed, Field.MinValue, Field.MaxValue));
}

bool USoccerUIStyleEditorController::ReadValues(const FString& FullPath)
{
    FString SavedText;
    if (!FFileHelper::LoadFileToString(SavedText, *FullPath)) return false;
    TArray<FString> Lines;
    SavedText.ParseIntoArrayLines(Lines);
    for (const FString& Line : Lines)
    {
        FString Key, Value;
        if (!Line.Split(TEXT("="), &Key, &Value)) continue;
        Key.TrimStartAndEndInline();
        for (const FSoccerEditableStyleField& Field : Fields)
        {
            if (Field.Key.ToString() == Key)
            {
                Values.Add(Field.Key, NormalizeValue(Field, Value));
                break;
            }
        }
    }
    return true;
}

void USoccerUIStyleEditorController::Load()
{
    Values.Reset();
    for (const FSoccerEditableStyleField& Field : Fields)
        Values.Add(Field.Key, Field.DefaultValue);
    ReadValues(ConfigFilePath());
}

bool USoccerUIStyleEditorController::WriteValues(const FString& FullPath) const
{
    FString Output = TEXT("; Temporary football UI style values, editable in game.\n");
    for (const FSoccerEditableStyleField& Field : Fields)
        Output += Field.Key.ToString() + TEXT("=") + Values.FindRef(Field.Key) + TEXT("\n");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true);
    return FFileHelper::SaveStringToFile(Output, *FullPath);
}

bool USoccerUIStyleEditorController::Save() const
{
    return WriteValues(ConfigFilePath());
}

bool USoccerUIStyleEditorController::HasSnapshotChanged() const
{
    if (!Snapshots.IsValidIndex(SnapshotIndex)) return true;
    const TMap<FName, FString>& Previous = Snapshots[SnapshotIndex];
    if (Previous.Num() != Values.Num()) return true;
    for (const TPair<FName, FString>& Entry : Values)
    {
        const FString* OldValue = Previous.Find(Entry.Key);
        if (!OldValue || *OldValue != Entry.Value) return true;
    }
    return false;
}

void USoccerUIStyleEditorController::PushSnapshot()
{
    if (!HasSnapshotChanged()) return;
    if (SnapshotIndex + 1 < Snapshots.Num())
        Snapshots.RemoveAt(SnapshotIndex + 1, Snapshots.Num() - SnapshotIndex - 1);
    Snapshots.Add(Values);
    if (Snapshots.Num() > 30) Snapshots.RemoveAt(0);
    SnapshotIndex = Snapshots.Num() - 1;
}

void USoccerUIStyleEditorController::ApplySnapshot(int32 NewIndex)
{
    if (!Snapshots.IsValidIndex(NewIndex)) return;
    SnapshotIndex = NewIndex;
    Values = Snapshots[SnapshotIndex];
    RefreshEditors();
    OnApplied.Broadcast();
}

FString USoccerUIStyleEditorController::SelectedPresetPath() const
{
    const FString Chosen = LocalPresetChoice ? LocalPresetChoice->GetSelectedOption() : TEXT("A");
    const FString SafeSlot = Chosen == TEXT("B") ? TEXT("B") : Chosen == TEXT("C") ? TEXT("C") : TEXT("A");
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SoccerUI"), TEXT("Presets"),
        FileName + TEXT("_Preset_") + SafeSlot + TEXT(".ini"));
}

bool USoccerUIStyleEditorController::ExportAllStyles(FString& OutPath)
{
    const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SoccerUI"));
    TArray<FString> FileNames;
    IFileManager::Get().FindFiles(FileNames, *(FPaths::Combine(Root, TEXT("*.ini"))), true, false);
    FileNames.Sort();
    FString AllText = TEXT("; Soccer UI - exportacion para revision (no importar directamente)\n");
    int32 Count = 0;
    for (const FString& Entry : FileNames)
    {
        FString FileText;
        if (!FFileHelper::LoadFileToString(FileText, *FPaths::Combine(Root, Entry))) continue;
        AllText += TEXT("\n========== ") + Entry + TEXT(" ==========\n") + FileText + TEXT("\n");
        ++Count;
    }
    if (Count == 0) return false;
    OutPath = FPaths::Combine(Root, TEXT("SoccerUIStyles_Export.txt"));
    return FFileHelper::SaveStringToFile(AllText, *OutPath);
}

void USoccerUIStyleEditorController::Setup(UWidgetTree* Tree, UCanvasPanel* Canvas,
    const FString& ConfigName, const TArray<FSoccerEditableStyleField>& InFields)
{
    if (!Tree || !Canvas) return;
    FileName = ConfigName;
    Fields = InFields;
    Load();
    Snapshots.Reset();
    SnapshotIndex = INDEX_NONE;
    PushSnapshot();

    UButton* Toggle = SoccerRuntimeStyleInternal::Action(Tree, TEXT("ESTILOS"));
    StyleToggleButton = Toggle;
    UCanvasPanelSlot* ToggleCanvasSlot = Canvas->AddChildToCanvas(Toggle);
    ToggleCanvasSlot->SetAnchors(FAnchors(1.f, 0.f));
    ToggleCanvasSlot->SetAlignment(FVector2D(1.f, 0.f));
    ToggleCanvasSlot->SetPosition(FVector2D(-16.f, 15.f));
    ToggleCanvasSlot->SetSize(FVector2D(105.f, 38.f));
    Toggle->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnToggleClicked);

    Drawer = Tree->ConstructWidget<UBorder>();
    Drawer->SetBrushColor(FLinearColor(.028f, .055f, .074f, .985f));
    Drawer->SetPadding(FMargin(12.f));
    Drawer->SetVisibility(ESlateVisibility::Collapsed);
    DrawerCanvasSlot = Canvas->AddChildToCanvas(Drawer);
    DrawerCanvasSlot->SetAnchors(FAnchors(1.f, .5f));
    DrawerCanvasSlot->SetAlignment(FVector2D(1.f, .5f));
    DrawerCanvasSlot->SetPosition(FVector2D(-16.f, 0.f));
    DrawerCanvasSlot->SetSize(FSoccerUITheme::FitPanelToViewport(GetOuter(), FVector2D(405.f, 670.f)));

    UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>();
    Drawer->AddChild(Column);
    Column->AddChildToVerticalBox(SoccerRuntimeStyleInternal::Text(Tree,
        TEXT("ESTILOS  •  temporal"), 19));
    Column->AddChildToVerticalBox(SoccerRuntimeStyleInternal::Text(Tree,
        TEXT("Editá valores y aplicalos sin recompilar."), 12));

    // Global preset shared by all compatible menus; does not change local INI files.
    Column->AddChildToVerticalBox(SoccerRuntimeStyleInternal::Text(Tree,
        TEXT("TEMA GLOBAL (sin alterar colores personalizados)"), 12));
    GlobalThemeChoice = Tree->ConstructWidget<UComboBoxString>();
    GlobalThemeChoice->AddOption(TEXT("Original"));
    GlobalThemeChoice->AddOption(TEXT("Verde estadio"));
    GlobalThemeChoice->AddOption(TEXT("Azul profesional"));
    GlobalThemeChoice->AddOption(TEXT("Grafito"));
    GlobalThemeChoice->SetSelectedOption(FSoccerUITheme::GetActiveTheme());
    Column->AddChildToVerticalBox(GlobalThemeChoice);
    UButton* ApplyThemeButton = SoccerRuntimeStyleInternal::Action(Tree, TEXT("APLICAR TEMA GLOBAL"));
    Column->AddChildToVerticalBox(ApplyThemeButton);
    ApplyThemeButton->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnApplyThemeClicked);
    UScrollBox* Scroll = Tree->ConstructWidget<UScrollBox>();
    UVerticalBoxSlot* ScrollSlot = Column->AddChildToVerticalBox(Scroll);
    ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* Entries = Tree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(Entries);
    InputBindings.Reset();
    Editors.Reset();
    Sliders.Reset();
    ColorPreviews.Reset();
    for (const FSoccerEditableStyleField& Field : Fields)
    {
        UVerticalBox* Entry = Tree->ConstructWidget<UVerticalBox>();
        UVerticalBoxSlot* EntrySlot = Entries->AddChildToVerticalBox(Entry);
        EntrySlot->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
        Entry->AddChildToVerticalBox(SoccerRuntimeStyleInternal::Text(Tree, Field.Label, 12));
        USoccerUIStyleFieldInputBridge* Binding = NewObject<USoccerUIStyleFieldInputBridge>(this);
        Binding->Initialize(this, Field.Key);
        InputBindings.Add(Binding);

        UHorizontalBox* InputRow = Tree->ConstructWidget<UHorizontalBox>();
        Entry->AddChildToVerticalBox(InputRow);
        UEditableTextBox* Input = Tree->ConstructWidget<UEditableTextBox>();
        Input->SetText(FText::FromString(Values.FindRef(Field.Key)));
        InputRow->AddChildToHorizontalBox(Input);
        Editors.Add(Field.Key, Input);

        if (Field.bColor)
        {
            UBorder* ColorSample = Tree->ConstructWidget<UBorder>();
            ColorSample->SetPadding(FMargin(12.f, 8.f));
            FLinearColor Parsed;
            if (SoccerRuntimeStyleInternal::ParseHex(Values.FindRef(Field.Key), Parsed))
                ColorSample->SetBrushColor(Parsed);
            InputRow->AddChildToHorizontalBox(ColorSample);
            ColorPreviews.Add(Field.Key, ColorSample);
            Input->OnTextChanged.AddDynamic(Binding, &USoccerUIStyleFieldInputBridge::HandleHexChanged);

            UComboBoxString* Palette = Tree->ConstructWidget<UComboBoxString>();
            for (const TCHAR* Name : { TEXT("Personalizado"), TEXT("Blanco"), TEXT("Negro"),
                  TEXT("Verde estadio"), TEXT("Azul profesional"), TEXT("Grafito"), TEXT("Transparente") })
                Palette->AddOption(Name);
            Palette->SetSelectedOption(TEXT("Personalizado"));
            Palette->OnSelectionChanged.AddDynamic(Binding, &USoccerUIStyleFieldInputBridge::HandlePaletteChanged);
            Entry->AddChildToVerticalBox(Palette);
        }
        else
        {
            USlider* Slider = Tree->ConstructWidget<USlider>();
            Slider->SetMinValue(Field.MinValue);
            Slider->SetMaxValue(Field.MaxValue);
            Slider->SetValue(FMath::Clamp(FCString::Atof(*Values.FindRef(Field.Key)),
                Field.MinValue, Field.MaxValue));
            Slider->OnValueChanged.AddDynamic(Binding, &USoccerUIStyleFieldInputBridge::HandleSliderChanged);
            Entry->AddChildToVerticalBox(Slider);
            Sliders.Add(Field.Key, Slider);
        }
    }

    Column->AddChildToVerticalBox(SoccerRuntimeStyleInternal::Text(Tree,
        TEXT("PRESETS DE ESTA PANTALLA: A / B / C"), 12));
    LocalPresetChoice = Tree->ConstructWidget<UComboBoxString>();
    LocalPresetChoice->AddOption(TEXT("A"));
    LocalPresetChoice->AddOption(TEXT("B"));
    LocalPresetChoice->AddOption(TEXT("C"));
    LocalPresetChoice->SetSelectedOption(TEXT("A"));
    Column->AddChildToVerticalBox(LocalPresetChoice);
    UHorizontalBox* PresetActions = Tree->ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(PresetActions);
    UButton* SavePreset = SoccerRuntimeStyleInternal::Action(Tree, TEXT("GUARDAR PRESET"));
    UButton* LoadPreset = SoccerRuntimeStyleInternal::Action(Tree, TEXT("CARGAR PRESET"));
    PresetActions->AddChildToHorizontalBox(SavePreset);
    PresetActions->AddChildToHorizontalBox(LoadPreset);
    SavePreset->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnSavePresetClicked);
    LoadPreset->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnLoadPresetClicked);

    UHorizontalBox* Buttons = Tree->ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(Buttons);
    UButton* Apply = SoccerRuntimeStyleInternal::Action(Tree, TEXT("APLICAR"));
    UButton* SaveButton = SoccerRuntimeStyleInternal::Action(Tree, TEXT("GUARDAR"));
    UButton* Reset = SoccerRuntimeStyleInternal::Action(Tree, TEXT("RESTAURAR"));
    UButton* Close = SoccerRuntimeStyleInternal::Action(Tree, TEXT("CERRAR"));
    Buttons->AddChildToHorizontalBox(Apply);
    Buttons->AddChildToHorizontalBox(SaveButton);
    Buttons->AddChildToHorizontalBox(Reset);
    Buttons->AddChildToHorizontalBox(Close);
    Apply->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnApplyClicked);
    SaveButton->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnSaveClicked);
    Reset->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnResetClicked);
    Close->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnToggleClicked);
    UHorizontalBox* ExtraActions = Tree->ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(ExtraActions);
    UButton* UndoButton = SoccerRuntimeStyleInternal::Action(Tree, TEXT("DESHACER"));
    UButton* RedoButton = SoccerRuntimeStyleInternal::Action(Tree, TEXT("REHACER"));
    UButton* ExportButton = SoccerRuntimeStyleInternal::Action(Tree, TEXT("EXPORTAR TODO"));
    ExtraActions->AddChildToHorizontalBox(UndoButton);
    ExtraActions->AddChildToHorizontalBox(RedoButton);
    ExtraActions->AddChildToHorizontalBox(ExportButton);
    UndoButton->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnUndoClicked);
    RedoButton->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnRedoClicked);
    ExportButton->OnClicked.AddDynamic(this, &USoccerUIStyleEditorController::OnExportClicked);
        Status = SoccerRuntimeStyleInternal::Text(Tree, TEXT("Sin modificar"), 11);
    Column->AddChildToVerticalBox(Status);
    FSoccerUITheme::ApplyButtonStates(Drawer);
}

float USoccerUIStyleEditorController::GetNumber(const TCHAR* Key, float Fallback) const
{
    const FString* Value = Values.Find(FName(Key));
    return Value ? FCString::Atof(**Value) : Fallback;
}

FLinearColor USoccerUIStyleEditorController::GetColor(const TCHAR* Key,
    const FLinearColor& Fallback) const
{
    const FString* Value = Values.Find(FName(Key));
    FLinearColor Result = Fallback;
    if (Value) SoccerRuntimeStyleInternal::ParseHex(*Value, Result);
    bool bLocallyCustomized = false;
    for (const FSoccerEditableStyleField& Field : Fields)
    {
        if (Field.Key == FName(Key) && Field.bColor)
        {
            bLocallyCustomized = Value && !Value->Equals(Field.DefaultValue, ESearchCase::IgnoreCase);
            break;
        }
    }
    return FSoccerUITheme::ResolveColor(Key, Result, bLocallyCustomized);
}

void USoccerUIStyleEditorController::RefreshEditors()
{
    bRefreshingFields = true;
    for (const FSoccerEditableStyleField& Field : Fields)
    {
        const FString Current = Values.FindRef(Field.Key);
        if (UEditableTextBox* const* Input = Editors.Find(Field.Key))
            if (IsValid(*Input)) (*Input)->SetText(FText::FromString(Current));
        if (USlider* const* Slider = Sliders.Find(Field.Key))
            if (IsValid(*Slider)) (*Slider)->SetValue(FMath::Clamp(FCString::Atof(*Current), Field.MinValue, Field.MaxValue));
        if (UBorder* const* Swatch = ColorPreviews.Find(Field.Key))
        {
            FLinearColor Color;
            if (IsValid(*Swatch) && SoccerRuntimeStyleInternal::ParseHex(Current, Color))
                (*Swatch)->SetBrushColor(Color);
        }
    }
    bRefreshingFields = false;
}

void USoccerUIStyleEditorController::HandleNumberInput(FName Key, float SliderValue)
{
    if (bRefreshingFields) return;
    for (const FSoccerEditableStyleField& Field : Fields)
    {
        if (Field.Key != Key || Field.bColor) continue;
        if (UEditableTextBox* const* Input = Editors.Find(Key))
            if (IsValid(*Input)) (*Input)->SetText(FText::FromString(
                FString::SanitizeFloat(FMath::Clamp(SliderValue, Field.MinValue, Field.MaxValue))));
        break;
    }
}

void USoccerUIStyleEditorController::HandlePaletteInput(FName Key, const FString& Choice)
{
    const FString Hex = Choice == TEXT("Blanco") ? TEXT("#FFFFFFFF")
        : Choice == TEXT("Negro") ? TEXT("#000000FF")
        : Choice == TEXT("Verde estadio") ? TEXT("#21A669FF")
        : Choice == TEXT("Azul profesional") ? TEXT("#237FCAFF")
        : Choice == TEXT("Grafito") ? TEXT("#333C45FF")
        : Choice == TEXT("Transparente") ? TEXT("#00000000") : FString();
    if (!Hex.IsEmpty())
    {
        if (UEditableTextBox* const* Input = Editors.Find(Key))
            if (IsValid(*Input)) (*Input)->SetText(FText::FromString(Hex));
    }
}

void USoccerUIStyleEditorController::HandleHexInput(FName Key, const FText& Value)
{
    if (bRefreshingFields) return;
    FLinearColor Parsed;
    if (!SoccerRuntimeStyleInternal::ParseHex(Value.ToString(), Parsed)) return;
    if (UBorder* const* Swatch = ColorPreviews.Find(Key))
        if (IsValid(*Swatch)) (*Swatch)->SetBrushColor(Parsed);
}

void USoccerUIStyleEditorController::SetStatus(const FString& Message)
{
    if (Status) Status->SetText(FText::FromString(Message));
}

void USoccerUIStyleEditorController::OnToggleClicked()
{
    if (!Drawer) return;
    const bool bOpening = Drawer->GetVisibility() == ESlateVisibility::Collapsed;
    if (bOpening)
    {
        if (DrawerCanvasSlot)
            DrawerCanvasSlot->SetSize(FSoccerUITheme::FitPanelToViewport(GetOuter(), FVector2D(405.f, 670.f)));
        if (GlobalThemeChoice) GlobalThemeChoice->SetSelectedOption(FSoccerUITheme::GetActiveTheme());
    }
    Drawer->SetVisibility(bOpening ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void USoccerUIStyleEditorController::OnApplyThemeClicked()
{
    if (!GlobalThemeChoice) return;
    const bool bSaved = FSoccerUITheme::SetActiveTheme(GlobalThemeChoice->GetSelectedOption());
    if (bSaved) OnApplied.Broadcast();
    SetStatus(bSaved ? TEXT("Tema global guardado; los otros menús lo usarán al abrirse.")
                     : TEXT("No se pudo guardar el tema global."));
}

void USoccerUIStyleEditorController::OnApplyClicked()
{
    for (const FSoccerEditableStyleField& Field : Fields)
    {
        UEditableTextBox* const* Entry = Editors.Find(Field.Key);
        if (Entry && *Entry)
        {
            Values.Add(Field.Key, NormalizeValue(Field, (*Entry)->GetText().ToString()));
        }
    }
    PushSnapshot();
    RefreshEditors();
    OnApplied.Broadcast();
    SetStatus(TEXT("Cambios aplicados (sin guardar)."));
}

void USoccerUIStyleEditorController::OnSaveClicked()
{
    OnApplyClicked();
    SetStatus(Save() ? TEXT("Guardado en Saved/SoccerUI.") : TEXT("ERROR al guardar."));
}

void USoccerUIStyleEditorController::OnResetClicked()
{
    for (const FSoccerEditableStyleField& Field : Fields)
    {
        Values.Add(Field.Key, Field.DefaultValue);
    }
    PushSnapshot();
    RefreshEditors();
    OnApplied.Broadcast();
    SetStatus(TEXT("Valores de fábrica; pulsá GUARDAR para conservarlos."));
}

void USoccerUIStyleEditorController::OnUndoClicked()
{
    if (SnapshotIndex > 0)
    {
        ApplySnapshot(SnapshotIndex - 1);
        SetStatus(TEXT("Deshecho (sin guardar)."));
    }
    else SetStatus(TEXT("No hay cambios anteriores."));
}

void USoccerUIStyleEditorController::OnRedoClicked()
{
    if (SnapshotIndex + 1 < Snapshots.Num())
    {
        ApplySnapshot(SnapshotIndex + 1);
        SetStatus(TEXT("Rehecho (sin guardar)."));
    }
    else SetStatus(TEXT("No hay cambios posteriores."));
}

void USoccerUIStyleEditorController::OnSavePresetClicked()
{
    OnApplyClicked();
    SetStatus(WriteValues(SelectedPresetPath())
        ? TEXT("Preset guardado. Los estilos activos no se modificaron.")
        : TEXT("No se pudo guardar el preset."));
}

void USoccerUIStyleEditorController::OnLoadPresetClicked()
{
    TMap<FName, FString> Before = Values;
    if (!ReadValues(SelectedPresetPath()))
    {
        Values = Before;
        SetStatus(TEXT("Ese preset todavía no existe."));
        return;
    }
    PushSnapshot();
    RefreshEditors();
    OnApplied.Broadcast();
    SetStatus(TEXT("Preset cargado (sin guardar como estilo activo)."));
}

void USoccerUIStyleEditorController::OnExportClicked()
{
    FString Path;
    if (ExportAllStyles(Path))
        SetStatus(TEXT("Exportado: Saved/SoccerUI/SoccerUIStyles_Export.txt"));
    else SetStatus(TEXT("Guardá al menos un menú antes de exportar."));
}

void USoccerUIStyleEditorController::ApplyTextScale(UWidget* Root, float Scale)
{
    if (!Root) return;
    if (UTextBlock* TextBlock = Cast<UTextBlock>(Root))
    {
        TWeakObjectPtr<UTextBlock> Key(TextBlock);
        int32* Base = OriginalFontSizes.Find(Key);
        if (!Base)
        {
            FSlateFontInfo FontInfo = TextBlock->Font;
            OriginalFontSizes.Add(Key, FontInfo.Size);
            Base = OriginalFontSizes.Find(Key);
        }
        if (Base)
        {
            FSlateFontInfo FontInfo = TextBlock->Font;
            FontInfo.Size = FMath::Max(8, FMath::RoundToInt(*Base * FMath::Clamp(Scale, .65f, 1.8f)));
            TextBlock->SetFont(FontInfo);
        }
    }
    if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
    {
        for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
        {
            ApplyTextScale(Panel->GetChildAt(i), Scale);
        }
    }
}

// The parent coach widget intercepts navigation in NativeOnPreviewKeyDown.
// Let typing in the temporary style editor reach the focused text field.
bool USoccerUIStyleEditorController::HasFocusedInput() const
{
    if (IsValid(StyleToggleButton) && StyleToggleButton->HasKeyboardFocus()) return true;
    if (!IsValid(Drawer) || Drawer->GetVisibility() != ESlateVisibility::Visible) return false;
    if (IsValid(GlobalThemeChoice) && GlobalThemeChoice->HasKeyboardFocus()) return true;
    return FSoccerUINavigation::HasFocusedDescendant(Drawer);
}
