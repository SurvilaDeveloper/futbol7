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

    TArray<FName> AvailableClubIds;
    int32 SelectedClubIndex = INDEX_NONE;
    int32 SelectedPlayerIndex = INDEX_NONE;
};
