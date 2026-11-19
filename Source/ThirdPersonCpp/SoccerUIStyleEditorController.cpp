#include "SoccerUIStyleEditorController.h"
#include "SoccerUITheme.h"
#include "SoccerUINavigation.h"

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

void USoccerUIStyleEditorController::Load()
{
    Values.Reset();
    for (const FSoccerEditableStyleField& Field : Fields)
    {
        Values.Add(Field.Key, Field.DefaultValue);
    }
    FString Saved;
    if (!FFileHelper::LoadFileToString(Saved, *ConfigFilePath())) return;
    TArray<FString> Lines;
    Saved.ParseIntoArrayLines(Lines);
    for (const FString& Line : Lines)
    {
        FString Key, Value;
        if (!Line.Split(TEXT("="), &Key, &Value)) continue;
        for (const FSoccerEditableStyleField& Field : Fields)
        {
            if (Field.Key.ToString() == Key.TrimStartAndEnd())
            {
                Values.Add(Field.Key, NormalizeValue(Field, Value));
                break;
            }
        }
    }
}

bool USoccerUIStyleEditorController::Save() const
{
    FString Output = TEXT("; Temporary football UI style values, editable in game.\n");
    for (const FSoccerEditableStyleField& Field : Fields)
    {
        Output += Field.Key.ToString() + TEXT("=") + Values.FindRef(Field.Key) + TEXT("\n");
    }
    const FString Path = ConfigFilePath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    return FFileHelper::SaveStringToFile(Output, *Path);
}

void USoccerUIStyleEditorController::Setup(UWidgetTree* Tree, UCanvasPanel* Canvas,
    const FString& ConfigName, const TArray<FSoccerEditableStyleField>& InFields)
{
    if (!Tree || !Canvas) return;
    FileName = ConfigName;
    Fields = InFields;
    Load();

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
    DrawerCanvasSlot->SetSize(FSoccerUITheme::FitPanelToViewport(GetOuter(), FVector2D(345.f, 630.f)));

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
    for (const FSoccerEditableStyleField& Field : Fields)
    {
        UVerticalBox* Entry = Tree->ConstructWidget<UVerticalBox>();
        UVerticalBoxSlot* EntrySlot = Entries->AddChildToVerticalBox(Entry);
        EntrySlot->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));
        Entry->AddChildToVerticalBox(SoccerRuntimeStyleInternal::Text(Tree, Field.Label, 12));
        UEditableTextBox* Input = Tree->ConstructWidget<UEditableTextBox>();
        Input->SetText(FText::FromString(Values.FindRef(Field.Key)));
        Entry->AddChildToVerticalBox(Input);
        Editors.Add(Field.Key, Input);
    }
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
    for (const TPair<FName, UEditableTextBox*>& Pair : Editors)
    {
        if (Pair.Value) Pair.Value->SetText(FText::FromString(Values.FindRef(Pair.Key)));
    }
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
            DrawerCanvasSlot->SetSize(FSoccerUITheme::FitPanelToViewport(GetOuter(), FVector2D(345.f, 630.f)));
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
    RefreshEditors();
    OnApplied.Broadcast();
    SetStatus(TEXT("Valores de fábrica; pulsá GUARDAR para conservarlos."));
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
