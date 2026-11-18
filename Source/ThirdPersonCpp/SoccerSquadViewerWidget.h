#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerSquadViewerWidget.generated.h"

class ASoccerMatchManager;
class ASoccerSquadPreviewActor;
class USoccerGameInstance;
class USoccerPlayerProfile;
class USoccerPlayerIdButton;
class UButton;
class UBorder;
class UTextBlock;
class UVerticalBox;
class UImage;
class UTextureRenderTarget2D;
class USizeBox;
class UHorizontalBoxSlot;
class UCanvasPanelSlot;
class UCanvasPanel;
class UEditableTextBox;
class UWidget;

// Temporary design-time values for the read-only squad viewer. The in-game
// drawer persists these to Saved/SoccerUI/SquadViewerStyle.ini (not a game save).
struct FSoccerSquadViewerStyleSettings
{
    float PanelWidth = 1180.f;
    float PanelHeight = 685.f;
    float ListWidth = 265.f;
    float CenterWidth = 408.f;
    float StatsWidth = 400.f;
    float PreviewHeight = 370.f;
    float PanelPadding = 22.f;
    float ColumnGap = 14.f;
    float ListPadding = 10.f;
    float StatsPadding = 12.f;
    float CardPadding = 8.f;
    float FontScale = 1.f;
    float StatLabelWidth = 181.f;
    float StatMeterWidth = 105.f;
    FLinearColor Backdrop = FLinearColor(.015f, .02f, .03f, .97f);
    FLinearColor Panel = FLinearColor(.05f, .075f, .11f);
    FLinearColor ListPanel = FLinearColor(.07f, .11f, .15f);
    FLinearColor CenterPanel = FLinearColor(.045f, .08f, .11f);
    FLinearColor StatsPanel = FLinearColor(.07f, .11f, .15f);
    FLinearColor PlayerCard = FLinearColor(.09f, .13f, .17f);
    FLinearColor SelectedCard = FLinearColor(.07f, .45f, .29f);
    FLinearColor Accent = FLinearColor(.44f, .88f, .67f);
    FLinearColor Text = FLinearColor::White;
    FLinearColor SecondaryText = FLinearColor(.73f, .82f, .85f);
    FLinearColor Meter = FLinearColor(.20f, .70f, .43f);
    FLinearColor ButtonTint = FLinearColor::White;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSoccerSquadViewerClosed);

/** Read-only interactive club / player browser from the pre-match screen. */
UCLASS()
class THIRDPERSONCPP_API USoccerSquadViewerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Soccer|Squad Viewer")
    FSoccerSquadViewerClosed OnViewerClosed;

    void InitializeForClub(USoccerGameInstance* InGameInstance,
                           ASoccerMatchManager* InMatchManager, FName InitialClubId);
    void ActivateMenu();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry,
                                           const FKeyEvent& InKeyEvent) override;

private:
    void BuildWidgetTree();
    void RebuildClub();
    void RebuildPlayerButtons();
    void SelectPlayerIndex(int32 Index);
    void RefreshSelectedPlayer();
    void CycleClub(int32 Direction);
    void CyclePlayer(int32 Direction);
    void CreatePreviewIfNeeded();
    FString GetClubDisplayName(FName ClubId) const;
    FString GetPlayerDisplayName(const USoccerPlayerProfile* Profile) const;
    FString GetNaturalPositionName(uint8 Position) const;
    void AddStatGroup(const FString& GroupName);
    void AddStat(const FString& Label, int32 Rating);
    void AddDetail(const FString& Text);

    // In-game temporary live-style editor (no Blueprint or recompilation per tweak).
    void BuildStyleEditor(UCanvasPanel* Root);
    void AddStyleField(class UVerticalBox* Parent, const TCHAR* Key,
                       const TCHAR* Caption, const FString& CurrentValue);
    void SyncStyleEditors();
    void LoadStyle();
    bool SaveStyle() const;
    void ApplyViewerStyle();
    void StyleMainWidget(UWidget* Widget);
    FString StyleConfigPath() const;

    UFUNCTION() void HandleToggleStyleEditor();
    UFUNCTION() void HandleApplyStyle();
    UFUNCTION() void HandleSaveStyle();
    UFUNCTION() void HandleResetStyle();

    UFUNCTION() void HandlePreviousClub();
    UFUNCTION() void HandleNextClub();
    UFUNCTION() void HandlePreviousPlayer();
    UFUNCTION() void HandleNextPlayer();
    UFUNCTION() void HandlePlayerClicked(FName PlayerId);
    UFUNCTION() void HandleRotateLeft();
    UFUNCTION() void HandleRotateRight();
    UFUNCTION() void HandleZoomIn();
    UFUNCTION() void HandleZoomOut();
    UFUNCTION() void HandleClose();

    UPROPERTY(Transient) USoccerGameInstance* SoccerGameInstance = nullptr;
    UPROPERTY(Transient) ASoccerMatchManager* MatchManager = nullptr;
    UPROPERTY(Transient) ASoccerSquadPreviewActor* PreviewActor = nullptr;
    UPROPERTY(Transient) UTextureRenderTarget2D* PreviewTexture = nullptr;
    UPROPERTY(Transient) TArray<USoccerPlayerProfile*> CurrentPlayers;
    UPROPERTY(Transient) TMap<FName, UBorder*> PlayerBorders;
    UPROPERTY(Transient) UTextBlock* ClubNameText = nullptr;
    UPROPERTY(Transient) UTextBlock* ClubCounterText = nullptr;
    UPROPERTY(Transient) UTextBlock* PlayerNameText = nullptr;
    UPROPERTY(Transient) UTextBlock* PlayerCounterText = nullptr;
    UPROPERTY(Transient) UTextBlock* PreviewStatusText = nullptr;
    UPROPERTY(Transient) UVerticalBox* PlayerButtonColumn = nullptr;
    UPROPERTY(Transient) UVerticalBox* AttributeColumn = nullptr;
    UPROPERTY(Transient) UImage* PreviewImage = nullptr;
    UPROPERTY(Transient) UButton* CloseButton = nullptr;
    UPROPERTY(Transient) UBorder* StyleDrawer = nullptr;
    UPROPERTY(Transient) UTextBlock* StyleStatus = nullptr;
    UPROPERTY(Transient) UBorder* MainPanel = nullptr;
    UPROPERTY(Transient) UBorder* MainDim = nullptr;
    UPROPERTY(Transient) UBorder* ListPanel = nullptr;
    UPROPERTY(Transient) UBorder* CenterPanel = nullptr;
    UPROPERTY(Transient) UBorder* StatsPanel = nullptr;
    UPROPERTY(Transient) USizeBox* MainSizeBox = nullptr;
    UPROPERTY(Transient) USizeBox* ListSizeBox = nullptr;
    UPROPERTY(Transient) USizeBox* CenterSizeBox = nullptr;
    UPROPERTY(Transient) USizeBox* StatsSizeBox = nullptr;
    UPROPERTY(Transient) USizeBox* PreviewSizeBox = nullptr;
    UPROPERTY(Transient) UHorizontalBoxSlot* ListColumnSlot = nullptr;
    UPROPERTY(Transient) UHorizontalBoxSlot* CenterColumnSlot = nullptr;
    UPROPERTY(Transient) TArray<USizeBox*> StatLabelBoxes;
    UPROPERTY(Transient) TArray<USizeBox*> StatMeterBoxes;
    UPROPERTY(Transient) TMap<FName, UEditableTextBox*> StyleEditors;

    TMap<TWeakObjectPtr<UTextBlock>, int32> DefaultTextSizes;
    TArray<TWeakObjectPtr<UTextBlock>> AccentTextNodes;
    TArray<TWeakObjectPtr<UTextBlock>> SecondaryTextNodes;
    FSoccerSquadViewerStyleSettings LiveStyle;

    TArray<FName> AvailableClubIds;
    int32 SelectedClubIndex = INDEX_NONE;
    int32 SelectedPlayerIndex = INDEX_NONE;
};
