#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/ComboBoxString.h"
#include "SoccerUIStyleEditorController.generated.h"

class UWidgetTree;
class UCanvasPanel;
class UWidget;
class UBorder;
class UButton;
class UEditableTextBox;
class UTextBlock;
class UCanvasPanelSlot;
class UComboBoxString;
class USlider;
class USoccerUIStyleEditorController;
class USoccerSquadViewerWidget;

/** A deliberately temporary, runtime-only authoring helper. No gameplay state. */
struct FSoccerEditableStyleField
{
    FName Key;
    FString Label;
    FString DefaultValue;
    bool bColor = false;
    float MinValue = 0.f;
    float MaxValue = 10000.f;

    static FSoccerEditableStyleField Number(const TCHAR* InKey, const TCHAR* InLabel,
        float InDefaultValue, float InMinValue, float InMaxValue);
    static FSoccerEditableStyleField Color(const TCHAR* InKey, const TCHAR* InLabel,
        const TCHAR* InDefaultHex);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSoccerRuntimeStyleApplied);

/** One binding per field; avoids the same callback handling every slider. */
UCLASS()
class THIRDPERSONCPP_API USoccerUIStyleFieldInputBridge : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(USoccerUIStyleEditorController* InOwner, FName InKey);
    void InitializeViewer(USoccerSquadViewerWidget* InOwner, FName InKey);
    UFUNCTION() void HandleSliderChanged(float NewValue);
    UFUNCTION() void HandlePaletteChanged(FString Item, ESelectInfo::Type SelectionType);
    UFUNCTION() void HandleHexChanged(const FText& NewText);
private:
    UPROPERTY(Transient) USoccerUIStyleEditorController* EditorOwner = nullptr;
    UPROPERTY(Transient) USoccerSquadViewerWidget* ViewerOwner = nullptr;
    FName FieldKey;
};


UCLASS()
class THIRDPERSONCPP_API USoccerUIStyleEditorController : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() FSoccerRuntimeStyleApplied OnApplied;

    void Setup(UWidgetTree* Tree, UCanvasPanel* Canvas, const FString& ConfigName,
        const TArray<FSoccerEditableStyleField>& InFields);
    float GetNumber(const TCHAR* Key, float Fallback) const;
    FLinearColor GetColor(const TCHAR* Key, const FLinearColor& Fallback) const;
    void ApplyTextScale(UWidget* Root, float Scale);
    bool HasFocusedInput() const;
    void HandleNumberInput(FName Key, float SliderValue);
    void HandlePaletteInput(FName Key, const FString& PaletteName);
    void HandleHexInput(FName Key, const FText& Value);
    /** Exports existing .ini files to a single read-only text snapshot for review. */
    static bool ExportAllStyles(FString& OutPath);

private:
    void Load();
    bool Save() const;
    bool WriteValues(const FString& FullPath) const;
    bool ReadValues(const FString& FullPath);
    bool HasSnapshotChanged() const;
    void PushSnapshot();
    void ApplySnapshot(int32 NewIndex);
    FString SelectedPresetPath() const;
    void RefreshEditors();
    void SetStatus(const FString& Message);
    FString ConfigFilePath() const;
    FString NormalizeValue(const FSoccerEditableStyleField& Field, const FString& Raw) const;

    UFUNCTION() void OnToggleClicked();
    UFUNCTION() void OnApplyClicked();
    UFUNCTION() void OnSaveClicked();
    UFUNCTION() void OnResetClicked();
    UFUNCTION() void OnApplyThemeClicked();
    UFUNCTION() void OnUndoClicked();
    UFUNCTION() void OnRedoClicked();
    UFUNCTION() void OnSavePresetClicked();
    UFUNCTION() void OnLoadPresetClicked();
    UFUNCTION() void OnExportClicked();

    UPROPERTY(Transient) UButton* StyleToggleButton = nullptr;
    UPROPERTY(Transient) UBorder* Drawer = nullptr;
    UPROPERTY(Transient) UCanvasPanelSlot* DrawerCanvasSlot = nullptr;
    UPROPERTY(Transient) UComboBoxString* GlobalThemeChoice = nullptr;
    UPROPERTY(Transient) UTextBlock* Status = nullptr;
    UPROPERTY(Transient) UComboBoxString* LocalPresetChoice = nullptr;
    UPROPERTY(Transient) TMap<FName, USlider*> Sliders;
    UPROPERTY(Transient) TMap<FName, UBorder*> ColorPreviews;
    UPROPERTY(Transient) TArray<USoccerUIStyleFieldInputBridge*> InputBindings;
    TArray<TMap<FName, FString>> Snapshots;
    int32 SnapshotIndex = INDEX_NONE;
    bool bRefreshingFields = false;
    UPROPERTY(Transient) TMap<FName, UEditableTextBox*> Editors;
    TMap<FName, FString> Values;
    TMap<TWeakObjectPtr<UTextBlock>, int32> OriginalFontSizes;
    TArray<FSoccerEditableStyleField> Fields;
    FString FileName;
};
