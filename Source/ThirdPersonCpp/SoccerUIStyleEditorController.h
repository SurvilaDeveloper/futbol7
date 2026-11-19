#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
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

private:
    void Load();
    bool Save() const;
    void RefreshEditors();
    void SetStatus(const FString& Message);
    FString ConfigFilePath() const;
    FString NormalizeValue(const FSoccerEditableStyleField& Field, const FString& Raw) const;

    UFUNCTION() void OnToggleClicked();
    UFUNCTION() void OnApplyClicked();
    UFUNCTION() void OnSaveClicked();
    UFUNCTION() void OnResetClicked();
    UFUNCTION() void OnApplyThemeClicked();

    UPROPERTY(Transient) UButton* StyleToggleButton = nullptr;
    UPROPERTY(Transient) UBorder* Drawer = nullptr;
    UPROPERTY(Transient) UCanvasPanelSlot* DrawerCanvasSlot = nullptr;
    UPROPERTY(Transient) UComboBoxString* GlobalThemeChoice = nullptr;
    UPROPERTY(Transient) UTextBlock* Status = nullptr;
    UPROPERTY(Transient) TMap<FName, UEditableTextBox*> Editors;
    TMap<FName, FString> Values;
    TMap<TWeakObjectPtr<UTextBlock>, int32> OriginalFontSizes;
    TArray<FSoccerEditableStyleField> Fields;
    FString FileName;
};
