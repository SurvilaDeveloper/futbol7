#include "SoccerDirectorTechnicalWidget.h"

#include "SoccerDirectorTechnicalIdButton.h"
#include "SoccerFormationLibrary.h"
#include "SoccerGameInstance.h"
#include "SoccerLineupEvaluationLibrary.h"
#include "SoccerPlayerProfile.h"
#include "SoccerSquadCatalog.h"

#include "AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoccerDirectorTechnical, Log, All);

namespace
{
    const FLinearColor DirectorPanelBackground(0.020f, 0.032f, 0.045f, 0.93f);
    const FLinearColor DirectorCardBackground(0.035f, 0.052f, 0.070f, 0.92f);
    const FLinearColor DirectorButtonBackground(0.075f, 0.105f, 0.135f, 0.96f);
    const FLinearColor DirectorSelectedColor(0.08f, 0.42f, 0.60f, 1.0f);
    const FLinearColor DirectorStarterColor(0.08f, 0.27f, 0.18f, 1.0f);
    const FLinearColor DirectorFocusedSlotColor(0.10f, 0.45f, 0.62f, 1.0f);
    const FLinearColor DirectorAccent(0.28f, 0.76f, 1.0f, 1.0f);
    const FLinearColor DirectorMuted(0.66f, 0.72f, 0.78f, 1.0f);

    UTextBlock* MakeDirectorText(
        UWidgetTree* WidgetTree,
        const FString& Text,
        int32 FontSize = 16,
        const FLinearColor& Color = FLinearColor::White
    )
    {
        if (WidgetTree == nullptr)
        {
            return nullptr;
        }

        UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>();
        if (TextBlock == nullptr)
        {
            return nullptr;
        }

        TextBlock->SetText(FText::FromString(Text));
        TextBlock->SetColorAndOpacity(FSlateColor(Color));
        FSlateFontInfo FontInfo = TextBlock->Font;
        FontInfo.Size = FontSize;
        TextBlock->SetFont(FontInfo);
        return TextBlock;
    }

    void AddDirectorVerticalChild(
        UVerticalBox* Parent,
        UWidget* Child,
        const FMargin& Padding = FMargin(0.0f),
        ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic,
        EHorizontalAlignment Alignment = HAlign_Fill
    )
    {
        if (Parent == nullptr || Child == nullptr)
        {
            return;
        }

        UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
        if (Slot != nullptr)
        {
            Slot->SetPadding(Padding);
            Slot->SetSize(FSlateChildSize(SizeRule));
            Slot->SetHorizontalAlignment(Alignment);
        }
    }

    void AddDirectorHorizontalChild(
        UHorizontalBox* Parent,
        UWidget* Child,
        const FMargin& Padding = FMargin(0.0f),
        ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic,
        EVerticalAlignment Alignment = VAlign_Fill
    )
    {
        if (Parent == nullptr || Child == nullptr)
        {
            return;
        }

        UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Child);
        if (Slot != nullptr)
        {
            Slot->SetPadding(Padding);
            Slot->SetSize(FSlateChildSize(SizeRule));
            Slot->SetVerticalAlignment(Alignment);
        }
    }

    UBorder* MakeDirectorCard(UWidgetTree* WidgetTree, float Padding = 14.0f)
    {
        if (WidgetTree == nullptr)
        {
            return nullptr;
        }

        UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
        if (Border != nullptr)
        {
            Border->SetBrushColor(DirectorCardBackground);
            Border->SetPadding(FMargin(Padding));
        }
        return Border;
    }

    UButton* MakeDirectorButton(
        UWidgetTree* WidgetTree,
        const FString& Label,
        int32 FontSize = 15
    )
    {
        if (WidgetTree == nullptr)
        {
            return nullptr;
        }

        UButton* Button = WidgetTree->ConstructWidget<UButton>();
        if (Button == nullptr)
        {
            return nullptr;
        }

        Button->SetBackgroundColor(DirectorButtonBackground);
        UTextBlock* LabelText = MakeDirectorText(WidgetTree, Label, FontSize);
        if (LabelText != nullptr)
        {
            LabelText->SetJustification(ETextJustify::Center);
            Button->AddChild(LabelText);
        }
        return Button;
    }

    FString GetPreferredFootText(ESoccerPreferredFoot PreferredFoot)
    {
        switch (PreferredFoot)
        {
        case ESoccerPreferredFoot::Left: return TEXT("Izquierda");
        case ESoccerPreferredFoot::Both: return TEXT("Ambas");
        case ESoccerPreferredFoot::Right:
        default:
            return TEXT("Derecha");
        }
    }

    FString GetNaturalPositionText(ESoccerPlayerNaturalPosition Position)
    {
        switch (Position)
        {
        case ESoccerPlayerNaturalPosition::Goalkeeper: return TEXT("Arquero");
        case ESoccerPlayerNaturalPosition::LeftDefender: return TEXT("Defensor izquierdo");
        case ESoccerPlayerNaturalPosition::CentralDefender: return TEXT("Defensor central");
        case ESoccerPlayerNaturalPosition::RightDefender: return TEXT("Defensor derecho");
        case ESoccerPlayerNaturalPosition::LeftMidfielder: return TEXT("Mediocampista izquierdo");
        case ESoccerPlayerNaturalPosition::CentralMidfielder: return TEXT("Mediocampista central");
        case ESoccerPlayerNaturalPosition::RightMidfielder: return TEXT("Mediocampista derecho");
        case ESoccerPlayerNaturalPosition::LeftForward: return TEXT("Delantero izquierdo");
        case ESoccerPlayerNaturalPosition::CenterForward: return TEXT("Delantero centro");
        case ESoccerPlayerNaturalPosition::RightForward: return TEXT("Delantero derecho");
        default: return TEXT("--");
        }
    }
}

void USoccerDirectorTechnicalWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildWidgetTree();
}

void USoccerDirectorTechnicalWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshFromPersistentTeamSetup(true);
}

void USoccerDirectorTechnicalWidget::BuildWidgetTree()
{
    if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
    {
        return;
    }

    UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>();
    RootBorder->SetBrushColor(DirectorPanelBackground);
    RootBorder->SetPadding(FMargin(4.0f));
    WidgetTree->RootWidget = RootBorder;

    UVerticalBox* RootColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    RootBorder->AddChild(RootColumn);

    UHorizontalBox* FormationHeader = WidgetTree->ConstructWidget<UHorizontalBox>();
    AddDirectorVerticalChild(
        RootColumn,
        FormationHeader,
        FMargin(0.0f, 0.0f, 0.0f, 10.0f)
    );

    UTextBlock* PageTitle = MakeDirectorText(
        WidgetTree,
        TEXT("PLANTEL Y ALINEACIÓN"),
        23,
        FLinearColor(0.95f, 0.98f, 1.0f, 1.0f)
    );
    AddDirectorHorizontalChild(
        FormationHeader,
        PageTitle,
        FMargin(0.0f, 0.0f, 18.0f, 0.0f),
        ESlateSizeRule::Automatic,
        VAlign_Center
    );

    UButton* PreviousFormationButton = MakeDirectorButton(WidgetTree, TEXT("<"), 20);
    PreviousFormationButton->OnClicked.AddDynamic(
        this,
        &USoccerDirectorTechnicalWidget::HandlePreviousFormationClicked
    );
    AddDirectorHorizontalChild(
        FormationHeader,
        PreviousFormationButton,
        FMargin(0.0f, 0.0f, 7.0f, 0.0f),
        ESlateSizeRule::Automatic,
        VAlign_Center
    );

    FormationText = MakeDirectorText(
        WidgetTree,
        TEXT("Formación guardada: --"),
        18,
        DirectorAccent
    );
    AddDirectorHorizontalChild(
        FormationHeader,
        FormationText,
        FMargin(0.0f, 0.0f, 7.0f, 0.0f),
        ESlateSizeRule::Automatic,
        VAlign_Center
    );

    UButton* NextFormationButton = MakeDirectorButton(WidgetTree, TEXT(">"), 20);
    NextFormationButton->OnClicked.AddDynamic(
        this,
        &USoccerDirectorTechnicalWidget::HandleNextFormationClicked
    );
    AddDirectorHorizontalChild(
        FormationHeader,
        NextFormationButton,
        FMargin(0.0f, 0.0f, 18.0f, 0.0f),
        ESlateSizeRule::Automatic,
        VAlign_Center
    );

    LineupStatusText = MakeDirectorText(
        WidgetTree,
        TEXT("Titulares: 0/7"),
        16,
        DirectorMuted
    );
    AddDirectorHorizontalChild(
        FormationHeader,
        LineupStatusText,
        FMargin(0.0f),
        ESlateSizeRule::Fill,
        VAlign_Center
    );

    DataSourceText = MakeDirectorText(
        WidgetTree,
        TEXT("Plantel: --"),
        14,
        DirectorMuted
    );
    DataSourceText->SetJustification(ETextJustify::Right);
    AddDirectorHorizontalChild(
        FormationHeader,
        DataSourceText,
        FMargin(10.0f, 0.0f, 0.0f, 0.0f),
        ESlateSizeRule::Automatic,
        VAlign_Center
    );

    UHorizontalBox* BodyRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    AddDirectorVerticalChild(
        RootColumn,
        BodyRow,
        FMargin(0.0f),
        ESlateSizeRule::Fill
    );

    // Left: complete squad list.
    USizeBox* RosterSize = WidgetTree->ConstructWidget<USizeBox>();
    RosterSize->SetWidthOverride(300.0f);
    UBorder* RosterCard = MakeDirectorCard(WidgetTree);
    RosterSize->AddChild(RosterCard);
    UVerticalBox* RosterColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    RosterCard->AddChild(RosterColumn);

    UTextBlock* RosterTitle = MakeDirectorText(
        WidgetTree,
        TEXT("JUGADORES"),
        18,
        DirectorAccent
    );
    AddDirectorVerticalChild(
        RosterColumn,
        RosterTitle,
        FMargin(0.0f, 0.0f, 0.0f, 8.0f)
    );

    RosterScrollBox = WidgetTree->ConstructWidget<UScrollBox>();
    AddDirectorVerticalChild(
        RosterColumn,
        RosterScrollBox,
        FMargin(0.0f),
        ESlateSizeRule::Fill
    );

    AddDirectorHorizontalChild(
        BodyRow,
        RosterSize,
        FMargin(0.0f, 0.0f, 10.0f, 0.0f),
        ESlateSizeRule::Automatic
    );

    // Center: formation pitch and bench.
    UBorder* FormationCard = MakeDirectorCard(WidgetTree);
    UVerticalBox* FormationColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    FormationCard->AddChild(FormationColumn);

    UTextBlock* FormationTitle = MakeDirectorText(
        WidgetTree,
        TEXT("ONCE INICIAL (7)"),
        18,
        DirectorAccent
    );
    AddDirectorVerticalChild(
        FormationColumn,
        FormationTitle,
        FMargin(0.0f, 0.0f, 0.0f, 8.0f)
    );

    USizeBox* PitchSize = WidgetTree->ConstructWidget<USizeBox>();
    PitchSize->SetWidthOverride(PitchCanvasWidth);
    PitchSize->SetHeightOverride(PitchCanvasHeight);
    FormationCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    PitchSize->AddChild(FormationCanvas);
    AddDirectorVerticalChild(FormationColumn, PitchSize);

    UTextBlock* BenchTitle = MakeDirectorText(
        WidgetTree,
        TEXT("SUPLENTES"),
        17,
        DirectorAccent
    );
    AddDirectorVerticalChild(
        FormationColumn,
        BenchTitle,
        FMargin(0.0f, 10.0f, 0.0f, 5.0f)
    );

    BenchListBox = WidgetTree->ConstructWidget<UVerticalBox>();
    AddDirectorVerticalChild(FormationColumn, BenchListBox);

    UHorizontalBox* BenchActions = WidgetTree->ConstructWidget<UHorizontalBox>();
    AddDirectorVerticalChild(
        FormationColumn,
        BenchActions,
        FMargin(0.0f, 8.0f, 0.0f, 0.0f)
    );

    MoveSelectedToBenchButton = MakeDirectorButton(
        WidgetTree,
        TEXT("MANDAR SELECCIONADO AL BANCO"),
        14
    );
    MoveSelectedToBenchButton->OnClicked.AddDynamic(
        this,
        &USoccerDirectorTechnicalWidget::HandleMoveSelectedToBenchClicked
    );
    AddDirectorHorizontalChild(
        BenchActions,
        MoveSelectedToBenchButton,
        FMargin(0.0f, 0.0f, 8.0f, 0.0f),
        ESlateSizeRule::Fill,
        VAlign_Center
    );

    BenchMoveUpButton = MakeDirectorButton(WidgetTree, TEXT("BANCO ↑"), 13);
    BenchMoveUpButton->OnClicked.AddDynamic(
        this,
        &USoccerDirectorTechnicalWidget::HandleBenchMoveUpClicked
    );
    AddDirectorHorizontalChild(
        BenchActions,
        BenchMoveUpButton,
        FMargin(0.0f, 0.0f, 6.0f, 0.0f),
        ESlateSizeRule::Automatic,
        VAlign_Center
    );

    BenchMoveDownButton = MakeDirectorButton(WidgetTree, TEXT("BANCO ↓"), 13);
    BenchMoveDownButton->OnClicked.AddDynamic(
        this,
        &USoccerDirectorTechnicalWidget::HandleBenchMoveDownClicked
    );
    AddDirectorHorizontalChild(
        BenchActions,
        BenchMoveDownButton,
        FMargin(0.0f),
        ESlateSizeRule::Automatic,
        VAlign_Center
    );

    AddDirectorHorizontalChild(
        BodyRow,
        FormationCard,
        FMargin(0.0f, 0.0f, 10.0f, 0.0f),
        ESlateSizeRule::Fill
    );

    // Right: selected player portrait / metadata / abilities.
    USizeBox* DetailsSize = WidgetTree->ConstructWidget<USizeBox>();
    DetailsSize->SetWidthOverride(430.0f);
    UBorder* DetailsCard = MakeDirectorCard(WidgetTree);
    DetailsSize->AddChild(DetailsCard);
    UVerticalBox* DetailsColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    DetailsCard->AddChild(DetailsColumn);

    SelectedPlayerNameText = MakeDirectorText(
        WidgetTree,
        TEXT("Seleccioná un jugador"),
        22,
        DirectorAccent
    );
    AddDirectorVerticalChild(DetailsColumn, SelectedPlayerNameText);

    SelectedPlayerMetaText = MakeDirectorText(
        WidgetTree,
        TEXT("--"),
        14,
        DirectorMuted
    );
    SelectedPlayerMetaText->SetAutoWrapText(true);
    AddDirectorVerticalChild(
        DetailsColumn,
        SelectedPlayerMetaText,
        FMargin(0.0f, 3.0f, 0.0f, 8.0f)
    );

    USizeBox* PortraitSize = WidgetTree->ConstructWidget<USizeBox>();
    PortraitSize->SetHeightOverride(120.0f);
    UBorder* PortraitBorder = WidgetTree->ConstructWidget<UBorder>();
    PortraitBorder->SetBrushColor(FLinearColor(0.06f, 0.08f, 0.10f, 1.0f));
    PortraitBorder->SetHorizontalAlignment(HAlign_Center);
    PortraitBorder->SetVerticalAlignment(VAlign_Center);
    PortraitSize->AddChild(PortraitBorder);

    UCanvasPanel* PortraitCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    PortraitBorder->AddChild(PortraitCanvas);

    SelectedPlayerPortraitImage = WidgetTree->ConstructWidget<UImage>();
    UCanvasPanelSlot* PortraitImageSlot =
        PortraitCanvas->AddChildToCanvas(SelectedPlayerPortraitImage);
    PortraitImageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    PortraitImageSlot->SetOffsets(FMargin(0.0f));

    SelectedPlayerPortraitFallbackText = MakeDirectorText(
        WidgetTree,
        TEXT("JUGADOR"),
        28,
        FLinearColor(0.72f, 0.78f, 0.84f, 1.0f)
    );
    SelectedPlayerPortraitFallbackText->SetJustification(ETextJustify::Center);
    UCanvasPanelSlot* PortraitTextSlot =
        PortraitCanvas->AddChildToCanvas(SelectedPlayerPortraitFallbackText);
    PortraitTextSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    PortraitTextSlot->SetOffsets(FMargin(0.0f));
    PortraitTextSlot->SetAlignment(FVector2D(0.0f, 0.0f));

    AddDirectorVerticalChild(
        DetailsColumn,
        PortraitSize,
        FMargin(0.0f, 0.0f, 0.0f, 8.0f)
    );

    SelectedSlotSuitabilityText = MakeDirectorText(
        WidgetTree,
        TEXT("Aptitud para puesto: --"),
        15,
        FLinearColor(0.96f, 0.86f, 0.48f, 1.0f)
    );
    SelectedSlotSuitabilityText->SetAutoWrapText(true);
    AddDirectorVerticalChild(
        DetailsColumn,
        SelectedSlotSuitabilityText,
        FMargin(0.0f, 0.0f, 0.0f, 8.0f)
    );

    UScrollBox* AttributesScroll = WidgetTree->ConstructWidget<UScrollBox>();
    PlayerAttributesBox = WidgetTree->ConstructWidget<UVerticalBox>();
    AttributesScroll->AddChild(PlayerAttributesBox);
    AddDirectorVerticalChild(
        DetailsColumn,
        AttributesScroll,
        FMargin(0.0f),
        ESlateSizeRule::Fill
    );

    SelectionSummaryText = MakeDirectorText(
        WidgetTree,
        TEXT("Seleccioná un futbolista y después un puesto."),
        14,
        DirectorMuted
    );
    SelectionSummaryText->SetAutoWrapText(true);
    AddDirectorVerticalChild(
        RootColumn,
        SelectionSummaryText,
        FMargin(0.0f, 8.0f, 0.0f, 0.0f)
    );
}

void USoccerDirectorTechnicalWidget::ResolveGameInstance()
{
    if (IsValid(SoccerGameInstance))
    {
        return;
    }

    UWorld* World = GetWorld();
    SoccerGameInstance = World != nullptr
        ? Cast<USoccerGameInstance>(World->GetGameInstance())
        : nullptr;
}

void USoccerDirectorTechnicalWidget::DiscoverPlayerProfiles()
{
    AvailableProfiles.Reset();
    ProfilesById.Reset();
    ActiveSquadCatalog = nullptr;

    if (IsValid(SoccerGameInstance))
    {
        ActiveSquadCatalog =
            SoccerGameInstance->GetDefaultHumanSquadCatalog();
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

    TArray<FAssetData> CatalogAssets;
    AssetRegistryModule.Get().GetAssetsByClass(
        USoccerSquadCatalog::StaticClass()->GetFName(),
        CatalogAssets,
        true
    );

    USoccerSquadCatalog* FirstValidCatalog = nullptr;
    USoccerSquadCatalog* LegacyDefaultCatalog = nullptr;
    USoccerSquadCatalog* ExplicitHumanDefaultCatalog = nullptr;

    if (ActiveSquadCatalog == nullptr)
    {
        for (const FAssetData& CatalogAssetData : CatalogAssets)
        {
            USoccerSquadCatalog* CandidateCatalog = Cast<USoccerSquadCatalog>(
                CatalogAssetData.GetAsset()
            );
            if (!IsValid(CandidateCatalog))
            {
                continue;
            }

            if (FirstValidCatalog == nullptr)
            {
                FirstValidCatalog = CandidateCatalog;
            }

            if (
                CandidateCatalog->bDefaultHumanControlledClub &&
                ExplicitHumanDefaultCatalog == nullptr
            )
            {
                ExplicitHumanDefaultCatalog = CandidateCatalog;
            }

            if (
                CandidateCatalog->bDefaultPlayerTeamCatalog &&
                LegacyDefaultCatalog == nullptr
            )
            {
                LegacyDefaultCatalog = CandidateCatalog;
            }
        }

        ActiveSquadCatalog =
            ExplicitHumanDefaultCatalog != nullptr
                ? ExplicitHumanDefaultCatalog
                : (LegacyDefaultCatalog != nullptr
                    ? LegacyDefaultCatalog
                    : FirstValidCatalog);
    }

    if (IsValid(ActiveSquadCatalog))
    {
        for (USoccerPlayerProfile* Profile : ActiveSquadCatalog->PlayerProfiles)
        {
            if (IsValid(Profile))
            {
                AvailableProfiles.Add(Profile);
            }
        }
    }
    else
    {
        // Stage-6 fallback: makes the screen immediately testable before the
        // user creates DA_PlayerTeamSquad. Once opponent profiles exist, a
        // PlayerTeam catalog should be created to keep the pools separated.
        TArray<FAssetData> ProfileAssets;
        AssetRegistryModule.Get().GetAssetsByClass(
            USoccerPlayerProfile::StaticClass()->GetFName(),
            ProfileAssets,
            true
        );

        for (const FAssetData& ProfileAssetData : ProfileAssets)
        {
            USoccerPlayerProfile* Profile = Cast<USoccerPlayerProfile>(
                ProfileAssetData.GetAsset()
            );
            if (IsValid(Profile))
            {
                AvailableProfiles.Add(Profile);
            }
        }
    }

    // Keep deterministic display order without relying on pointer-array Sort
    // predicate semantics that differ between some UE4/UE5 headers.
    for (int32 LeftIndex = 0; LeftIndex < AvailableProfiles.Num(); ++LeftIndex)
    {
        for (int32 RightIndex = LeftIndex + 1; RightIndex < AvailableProfiles.Num(); ++RightIndex)
        {
            USoccerPlayerProfile* LeftProfile = AvailableProfiles[LeftIndex];
            USoccerPlayerProfile* RightProfile = AvailableProfiles[RightIndex];
            if (!IsValid(LeftProfile) || !IsValid(RightProfile))
            {
                continue;
            }

            const bool bRightComesFirst =
                RightProfile->Identity.ShirtNumber < LeftProfile->Identity.ShirtNumber ||
                (
                    RightProfile->Identity.ShirtNumber == LeftProfile->Identity.ShirtNumber &&
                    RightProfile->Identity.PlayerId.ToString() < LeftProfile->Identity.PlayerId.ToString()
                );

            if (bRightComesFirst)
            {
                AvailableProfiles.Swap(LeftIndex, RightIndex);
            }
        }
    }

    TArray<USoccerPlayerProfile*> UniqueProfiles;
    UniqueProfiles.Reserve(AvailableProfiles.Num());

    for (USoccerPlayerProfile* Profile : AvailableProfiles)
    {
        if (!IsValid(Profile) || !Profile->HasValidPlayerId())
        {
            UE_LOG(
                LogSoccerDirectorTechnical,
                Warning,
                TEXT("[DirectorTechnical] Ignoring profile without valid PlayerId: %s"),
                Profile != nullptr ? *Profile->GetName() : TEXT("<null>")
            );
            continue;
        }

        const FName PlayerId = Profile->Identity.PlayerId;
        if (ProfilesById.Contains(PlayerId))
        {
            UE_LOG(
                LogSoccerDirectorTechnical,
                Warning,
                TEXT("[DirectorTechnical] Duplicate PlayerId '%s'. Keeping first profile."),
                *PlayerId.ToString()
            );
            continue;
        }

        ProfilesById.Add(PlayerId, Profile);
        UniqueProfiles.Add(Profile);
    }

    AvailableProfiles = MoveTemp(UniqueProfiles);
    bProfilesDiscovered = true;

    UE_LOG(
        LogSoccerDirectorTechnical,
        Display,
        TEXT("[DirectorTechnical] Player profiles ready: %d, source=%s."),
        AvailableProfiles.Num(),
        IsValid(ActiveSquadCatalog)
            ? *ActiveSquadCatalog->GetName()
            : TEXT("PlayerProfile fallback")
    );

    if (DataSourceText != nullptr)
    {
        FString SourceLabel;
        if (IsValid(ActiveSquadCatalog))
        {
            SourceLabel = ActiveSquadCatalog->DisplayName.IsEmpty()
                ? ActiveSquadCatalog->GetName()
                : ActiveSquadCatalog->DisplayName.ToString();
        }
        else
        {
            SourceLabel = TEXT("todos los PlayerProfile (fallback)");
        }

        DataSourceText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("Plantel: %s | %d jugadores"),
                    *SourceLabel,
                    AvailableProfiles.Num()
                )
            )
        );
    }
}

void USoccerDirectorTechnicalWidget::SynchronizePersistentSquad()
{
    ResolveGameInstance();
    if (!IsValid(SoccerGameInstance))
    {
        return;
    }

    TArray<FName> AuthoritativePlayerIds;
    AuthoritativePlayerIds.Reserve(AvailableProfiles.Num());
    for (USoccerPlayerProfile* Profile : AvailableProfiles)
    {
        if (IsValid(Profile) && Profile->HasValidPlayerId())
        {
            AuthoritativePlayerIds.Add(Profile->Identity.PlayerId);
        }
    }

    if (AuthoritativePlayerIds.Num() > 0)
    {
        SoccerGameInstance->SynchronizeSquadWithPlayerIds(AuthoritativePlayerIds);
    }
}

void USoccerDirectorTechnicalWidget::RefreshFromPersistentTeamSetup(
    bool bRediscoverProfiles
)
{
    ResolveGameInstance();

    if (bRediscoverProfiles || !bProfilesDiscovered)
    {
        DiscoverPlayerProfiles();
        SynchronizePersistentSquad();
    }

    if (!IsValid(SoccerGameInstance))
    {
        if (SelectionSummaryText != nullptr)
        {
            SelectionSummaryText->SetText(
                FText::FromString(
                    TEXT("SoccerGameInstance no está activo. Revisá Project Settings > Maps & Modes > Game Instance Class.")
                )
            );
        }
        return;
    }

    const TArray<FName> OrderedPlayerIds = GetOrderedAvailablePlayerIds();
    if (SelectedPlayerId.IsNone() || !ProfilesById.Contains(SelectedPlayerId))
    {
        SelectedPlayerId = OrderedPlayerIds.Num() > 0
            ? OrderedPlayerIds[0]
            : NAME_None;
    }

    FocusedFormationSlotId = ResolveSafeFocusedSlotId();

    RefreshFormationHeader();
    RefreshRosterList();
    RefreshFormationPitch();
    RefreshBenchList();
    RefreshSelectedPlayerDetails();
    RefreshSelectionSummary();
}

void USoccerDirectorTechnicalWidget::RefreshFormationHeader()
{
    if (!IsValid(SoccerGameInstance))
    {
        return;
    }

    const FSoccerTeamSetup TeamSetup = SoccerGameInstance->GetCurrentTeamSetup();
    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(TeamSetup.FormationSystem);

    if (FormationText != nullptr)
    {
        FormationText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("Formación guardada: %s"),
                    *FormationDefinition.DisplayName.ToString()
                )
            )
        );
    }

    if (LineupStatusText != nullptr)
    {
        LineupStatusText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("Titulares: %d/7  |  Banco: %d"),
                    SoccerGameInstance->GetStartingPlayerCount(),
                    SoccerGameInstance->GetBenchPlayerIds().Num()
                )
            )
        );
        LineupStatusText->SetColorAndOpacity(
            FSlateColor(
                SoccerGameInstance->HasCompleteStartingLineup()
                ? FLinearColor(0.48f, 0.92f, 0.62f, 1.0f)
                : DirectorMuted
            )
        );
    }
}

void USoccerDirectorTechnicalWidget::RefreshRosterList()
{
    if (RosterScrollBox == nullptr)
    {
        return;
    }

    RosterScrollBox->ClearChildren();

    const TArray<FName> OrderedPlayerIds = GetOrderedAvailablePlayerIds();
    if (OrderedPlayerIds.Num() == 0)
    {
        UTextBlock* EmptyText = MakeDirectorText(
            WidgetTree,
            TEXT("No hay perfiles de jugador disponibles."),
            15,
            DirectorMuted
        );
        EmptyText->SetAutoWrapText(true);
        RosterScrollBox->AddChild(EmptyText);
        return;
    }

    for (const FName PlayerId : OrderedPlayerIds)
    {
        USoccerPlayerProfile* Profile = FindProfile(PlayerId);
        if (!IsValid(Profile))
        {
            continue;
        }

        USoccerDirectorTechnicalIdButton* PlayerButton =
            WidgetTree->ConstructWidget<USoccerDirectorTechnicalIdButton>();
        PlayerButton->SetIdentifier(PlayerId);
        PlayerButton->OnIdentifierClicked.AddDynamic(
            this,
            &USoccerDirectorTechnicalWidget::HandleRosterPlayerClicked
        );

        const bool bSelected = PlayerId == SelectedPlayerId;
        const bool bStarter =
            IsValid(SoccerGameInstance) &&
            !SoccerGameInstance->FindStartingSlotForPlayer(PlayerId).IsNone();

        PlayerButton->SetBackgroundColor(
            bSelected
            ? DirectorSelectedColor
            : (bStarter ? DirectorStarterColor : DirectorButtonBackground)
        );

        const FString ButtonLabel = FString::Printf(
            TEXT("#%02d  %s\n%s"),
            Profile->Identity.ShirtNumber,
            *GetPlayerDisplayName(PlayerId),
            *GetPlayerStatusLabel(PlayerId)
        );

        UTextBlock* PlayerText = MakeDirectorText(
            WidgetTree,
            ButtonLabel,
            14,
            FLinearColor::White
        );
        PlayerButton->AddChild(PlayerText);
        RosterScrollBox->AddChild(PlayerButton);
    }
}

void USoccerDirectorTechnicalWidget::RefreshFormationPitch()
{
    if (FormationCanvas == nullptr || !IsValid(SoccerGameInstance))
    {
        return;
    }

    FormationCanvas->ClearChildren();

    UBorder* FieldBackground = WidgetTree->ConstructWidget<UBorder>();
    FieldBackground->SetBrushColor(FLinearColor(0.035f, 0.145f, 0.105f, 0.94f));
    UCanvasPanelSlot* FieldSlot = FormationCanvas->AddChildToCanvas(FieldBackground);
    FieldSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    FieldSlot->SetOffsets(FMargin(0.0f));

    UTextBlock* OpponentGoalLabel = MakeDirectorText(
        WidgetTree,
        TEXT("ARCO RIVAL"),
        12,
        FLinearColor(0.78f, 0.88f, 0.82f, 0.85f)
    );
    OpponentGoalLabel->SetJustification(ETextJustify::Center);
    UCanvasPanelSlot* OpponentGoalSlot =
        FormationCanvas->AddChildToCanvas(OpponentGoalLabel);
    OpponentGoalSlot->SetPosition(FVector2D(PitchCanvasWidth * 0.5f - 70.0f, 10.0f));
    OpponentGoalSlot->SetSize(FVector2D(140.0f, 20.0f));

    UTextBlock* OwnGoalLabel = MakeDirectorText(
        WidgetTree,
        TEXT("ARCO PROPIO"),
        12,
        FLinearColor(0.78f, 0.88f, 0.82f, 0.85f)
    );
    OwnGoalLabel->SetJustification(ETextJustify::Center);
    UCanvasPanelSlot* OwnGoalSlot = FormationCanvas->AddChildToCanvas(OwnGoalLabel);
    OwnGoalSlot->SetPosition(
        FVector2D(PitchCanvasWidth * 0.5f - 70.0f, PitchCanvasHeight - 30.0f)
    );
    OwnGoalSlot->SetSize(FVector2D(140.0f, 20.0f));

    const FSoccerTeamSetup TeamSetup = SoccerGameInstance->GetCurrentTeamSetup();
    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(TeamSetup.FormationSystem);

    const float HorizontalTravel = PitchCanvasWidth * 0.68f;
    const float VerticalTop = 48.0f;
    const float VerticalBottom = PitchCanvasHeight - 55.0f;
    const float VerticalTravel = VerticalBottom - VerticalTop;

    for (const FSoccerFormationSlot& FormationSlotDefinition : FormationDefinition.Slots)
    {
        const FName SlotId = FormationSlotDefinition.SlotId;
        const FName OccupantPlayerId =
            SoccerGameInstance->GetPlayerInStartingSlot(SlotId);

        USoccerDirectorTechnicalIdButton* FormationSlotButton =
            WidgetTree->ConstructWidget<USoccerDirectorTechnicalIdButton>();
        FormationSlotButton->SetIdentifier(SlotId);
        FormationSlotButton->OnIdentifierClicked.AddDynamic(
            this,
            &USoccerDirectorTechnicalWidget::HandleFormationSlotClicked
        );

        const bool bFocused = SlotId == FocusedFormationSlotId;
        const bool bSelectedOccupant =
            !SelectedPlayerId.IsNone() && OccupantPlayerId == SelectedPlayerId;

        FormationSlotButton->SetBackgroundColor(
            bFocused
            ? DirectorFocusedSlotColor
            : (bSelectedOccupant ? DirectorSelectedColor : DirectorButtonBackground)
        );

        FString SuitabilitySuffix;
        USoccerPlayerProfile* SelectedProfile = FindProfile(SelectedPlayerId);
        if (IsValid(SelectedProfile))
        {
            const FSoccerPlayerSlotSuitability Suitability =
                USoccerLineupEvaluationLibrary::EvaluatePlayerForFormationSlot(
                    SelectedProfile,
                    TeamSetup.FormationSystem,
                    SlotId
                );
            if (Suitability.bValid)
            {
                SuitabilitySuffix = FString::Printf(
                    TEXT("\nApt. %d"),
                    Suitability.OverallScore
                );
            }
        }

        const FString OccupantName = OccupantPlayerId.IsNone()
            ? TEXT("[ VACÍO ]")
            : GetPlayerDisplayName(OccupantPlayerId);

        const FString ButtonLabel = FString::Printf(
            TEXT("%s\n%s%s"),
            *SlotId.ToString(),
            *OccupantName,
            *SuitabilitySuffix
        );

        UTextBlock* SlotText = MakeDirectorText(
            WidgetTree,
            ButtonLabel,
            12,
            FLinearColor::White
        );
        SlotText->SetJustification(ETextJustify::Center);
        FormationSlotButton->AddChild(SlotText);

        const float CenterX =
            PitchCanvasWidth * 0.5f +
            FormationSlotDefinition.LateralAlpha * HorizontalTravel * 0.5f;
        const float CenterY =
            VerticalBottom -
            FMath::Clamp(FormationSlotDefinition.DepthAlpha, 0.0f, 1.0f) * VerticalTravel;

        UCanvasPanelSlot* ButtonCanvasSlot =
            FormationCanvas->AddChildToCanvas(FormationSlotButton);
        ButtonCanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        ButtonCanvasSlot->SetPosition(FVector2D(CenterX, CenterY));
        ButtonCanvasSlot->SetSize(FVector2D(145.0f, 58.0f));
    }
}

void USoccerDirectorTechnicalWidget::RefreshBenchList()
{
    if (BenchListBox == nullptr || !IsValid(SoccerGameInstance))
    {
        return;
    }

    BenchListBox->ClearChildren();
    const TArray<FName> BenchPlayerIds = SoccerGameInstance->GetBenchPlayerIds();

    if (BenchPlayerIds.Num() == 0)
    {
        UTextBlock* EmptyBenchText = MakeDirectorText(
            WidgetTree,
            TEXT("Sin suplentes."),
            14,
            DirectorMuted
        );
        AddDirectorVerticalChild(BenchListBox, EmptyBenchText);
        return;
    }

    for (int32 BenchIndex = 0; BenchIndex < BenchPlayerIds.Num(); ++BenchIndex)
    {
        const FName BenchPlayerId = BenchPlayerIds[BenchIndex];
        USoccerDirectorTechnicalIdButton* BenchButton =
            WidgetTree->ConstructWidget<USoccerDirectorTechnicalIdButton>();
        BenchButton->SetIdentifier(BenchPlayerId);
        BenchButton->OnIdentifierClicked.AddDynamic(
            this,
            &USoccerDirectorTechnicalWidget::HandleBenchPlayerClicked
        );
        BenchButton->SetBackgroundColor(
            BenchPlayerId == SelectedPlayerId
            ? DirectorSelectedColor
            : DirectorButtonBackground
        );

        UTextBlock* BenchText = MakeDirectorText(
            WidgetTree,
            FString::Printf(
                TEXT("%d. %s"),
                BenchIndex + 1,
                *GetPlayerDisplayName(BenchPlayerId)
            ),
            13
        );
        BenchButton->AddChild(BenchText);
        AddDirectorVerticalChild(
            BenchListBox,
            BenchButton,
            FMargin(0.0f, 2.0f)
        );
    }
}

void USoccerDirectorTechnicalWidget::RefreshSelectedPlayerDetails()
{
    USoccerPlayerProfile* Profile = FindProfile(SelectedPlayerId);

    if (SelectedPlayerNameText != nullptr)
    {
        SelectedPlayerNameText->SetText(
            FText::FromString(
                IsValid(Profile)
                ? GetPlayerDisplayName(SelectedPlayerId)
                : TEXT("Seleccioná un jugador")
            )
        );
    }

    if (!IsValid(Profile))
    {
        if (SelectedPlayerMetaText != nullptr)
        {
            SelectedPlayerMetaText->SetText(FText::FromString(TEXT("--")));
        }
        if (SelectedSlotSuitabilityText != nullptr)
        {
            SelectedSlotSuitabilityText->SetText(
                FText::FromString(TEXT("Aptitud para puesto: --"))
            );
        }
        if (PlayerAttributesBox != nullptr)
        {
            PlayerAttributesBox->ClearChildren();
        }
        if (SelectedPlayerPortraitImage != nullptr)
        {
            SelectedPlayerPortraitImage->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (SelectedPlayerPortraitFallbackText != nullptr)
        {
            SelectedPlayerPortraitFallbackText->SetVisibility(ESlateVisibility::Visible);
        }
        return;
    }

    if (SelectedPlayerMetaText != nullptr)
    {
        SelectedPlayerMetaText->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT("ID: %s | #%d | Pierna: %s | %.0f cm | %.0f kg | %s"),
                    *Profile->Identity.PlayerId.ToString(),
                    Profile->Identity.ShirtNumber,
                    *GetPreferredFootText(Profile->Identity.PreferredFoot),
                    Profile->Identity.HeightCm,
                    Profile->Identity.WeightKg,
                    *GetPlayerStatusLabel(SelectedPlayerId)
                )
            )
        );
    }

    UTexture2D* PortraitTexture = Profile->Appearance.Portrait.LoadSynchronous();
    if (SelectedPlayerPortraitImage != nullptr)
    {
        if (PortraitTexture != nullptr)
        {
            SelectedPlayerPortraitImage->SetBrushFromTexture(PortraitTexture, true);
            SelectedPlayerPortraitImage->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            SelectedPlayerPortraitImage->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (SelectedPlayerPortraitFallbackText != nullptr)
    {
        const bool bHasPortrait = PortraitTexture != nullptr;
        SelectedPlayerPortraitFallbackText->SetVisibility(
            bHasPortrait ? ESlateVisibility::Collapsed : ESlateVisibility::Visible
        );
        if (!bHasPortrait)
        {
            SelectedPlayerPortraitFallbackText->SetText(
                FText::FromString(
                    FString::Printf(
                        TEXT("#%d\n%s"),
                        Profile->Identity.ShirtNumber,
                        *GetPlayerDisplayName(SelectedPlayerId)
                    )
                )
            );
        }
    }

    if (
        SelectedSlotSuitabilityText != nullptr &&
        IsValid(SoccerGameInstance) &&
        !FocusedFormationSlotId.IsNone()
    )
    {
        const FSoccerTeamSetup TeamSetup = SoccerGameInstance->GetCurrentTeamSetup();
        const FSoccerPlayerSlotSuitability Suitability =
            USoccerLineupEvaluationLibrary::EvaluatePlayerForFormationSlot(
                Profile,
                TeamSetup.FormationSystem,
                FocusedFormationSlotId
            );

        if (Suitability.bValid)
        {
            const FString BandName =
                USoccerLineupEvaluationLibrary::GetSuitabilityBandDisplayName(
                    Suitability.SuitabilityBand
                ).ToString();

            SelectedSlotSuitabilityText->SetText(
                FText::FromString(
                    FString::Printf(
                        TEXT("%s: %d/100 (%s) | Habilidad %d | Familiaridad %d"),
                        *FocusedFormationSlotId.ToString(),
                        Suitability.OverallScore,
                        *BandName,
                        Suitability.AbilityScore,
                        Suitability.PositionFamiliarity
                    )
                )
            );
        }
    }

    if (PlayerAttributesBox == nullptr)
    {
        return;
    }

    PlayerAttributesBox->ClearChildren();

    AddPositionPreferenceRows(PlayerAttributesBox, Profile);

    AddAttributeGroupHeader(PlayerAttributesBox, TEXT("FÍSICOS"));
    AddAttributeRow(PlayerAttributesBox, TEXT("Rapidez"), Profile->Attributes.Physical.Pace);
    AddAttributeRow(PlayerAttributesBox, TEXT("Aceleración"), Profile->Attributes.Physical.Acceleration);
    AddAttributeRow(PlayerAttributesBox, TEXT("Resistencia"), Profile->Attributes.Physical.Stamina);
    AddAttributeRow(PlayerAttributesBox, TEXT("Recuperación"), Profile->Attributes.Physical.StaminaRecovery);
    AddAttributeRow(PlayerAttributesBox, TEXT("Fuerza"), Profile->Attributes.Physical.Strength);
    AddAttributeRow(PlayerAttributesBox, TEXT("Agilidad"), Profile->Attributes.Physical.Agility);
    AddAttributeRow(PlayerAttributesBox, TEXT("Equilibrio"), Profile->Attributes.Physical.Balance);

    AddAttributeGroupHeader(PlayerAttributesBox, TEXT("TÉCNICOS"));
    AddAttributeRow(PlayerAttributesBox, TEXT("Control"), Profile->Attributes.Technical.BallControl);
    AddAttributeRow(PlayerAttributesBox, TEXT("Dribbling"), Profile->Attributes.Technical.Dribbling);
    AddAttributeRow(PlayerAttributesBox, TEXT("Precisión de pase"), Profile->Attributes.Technical.PassingAccuracy);
    AddAttributeRow(PlayerAttributesBox, TEXT("Precisión de remate"), Profile->Attributes.Technical.ShootingAccuracy);
    AddAttributeRow(PlayerAttributesBox, TEXT("Potencia de remate"), Profile->Attributes.Technical.ShotPower);
    AddAttributeRow(PlayerAttributesBox, TEXT("Tackle"), Profile->Attributes.Technical.Tackling);
    AddAttributeRow(PlayerAttributesBox, TEXT("Juego aéreo"), Profile->Attributes.Technical.AerialAbility);

    AddAttributeGroupHeader(PlayerAttributesBox, TEXT("TÁCTICOS / MENTALES"));
    AddAttributeRow(PlayerAttributesBox, TEXT("Reacción defensiva"), Profile->Attributes.Tactical.DefensiveReaction);
    AddAttributeRow(PlayerAttributesBox, TEXT("Anticipación"), Profile->Attributes.Tactical.Anticipation);
    AddAttributeRow(PlayerAttributesBox, TEXT("Posicionamiento ofensivo"), Profile->Attributes.Tactical.OffBallPositioning);
    AddAttributeRow(PlayerAttributesBox, TEXT("Posicionamiento defensivo"), Profile->Attributes.Tactical.DefensivePositioning);
    AddAttributeRow(PlayerAttributesBox, TEXT("Marcaje"), Profile->Attributes.Tactical.Marking);
    AddAttributeRow(PlayerAttributesBox, TEXT("Toma de decisiones"), Profile->Attributes.Tactical.DecisionMaking);
    AddAttributeRow(PlayerAttributesBox, TEXT("Compostura"), Profile->Attributes.Tactical.Composure);

    AddAttributeGroupHeader(PlayerAttributesBox, TEXT("ARQUERO"));
    AddAttributeRow(PlayerAttributesBox, TEXT("Reflejos"), Profile->Attributes.Goalkeeper.Reflexes);
    AddAttributeRow(PlayerAttributesBox, TEXT("Posicionamiento"), Profile->Attributes.Goalkeeper.Positioning);
    AddAttributeRow(PlayerAttributesBox, TEXT("Manejo de manos"), Profile->Attributes.Goalkeeper.Handling);
    AddAttributeRow(PlayerAttributesBox, TEXT("Estirada"), Profile->Attributes.Goalkeeper.Diving);
    AddAttributeRow(PlayerAttributesBox, TEXT("Distribución"), Profile->Attributes.Goalkeeper.Distribution);
}

void USoccerDirectorTechnicalWidget::RefreshSelectionSummary()
{
    if (SelectionSummaryText == nullptr)
    {
        return;
    }

    if (SelectedPlayerId.IsNone())
    {
        SelectionSummaryText->SetText(
            FText::FromString(
                TEXT("No hay jugadores disponibles. Creá perfiles o un SoccerSquadCatalog.")
            )
        );
        return;
    }

    SelectionSummaryText->SetText(
        FText::FromString(
            FString::Printf(
                TEXT("Seleccionado: %s | Puesto objetivo: %s. Click en un puesto (o Enter/A) para asignar; el autosave es inmediato."),
                *GetPlayerDisplayName(SelectedPlayerId),
                FocusedFormationSlotId.IsNone()
                    ? TEXT("--")
                    : *FocusedFormationSlotId.ToString()
            )
        )
    );
}

void USoccerDirectorTechnicalWidget::AddAttributeGroupHeader(
    UVerticalBox* Parent,
    const FString& Label
)
{
    UTextBlock* HeaderText = MakeDirectorText(
        WidgetTree,
        Label,
        14,
        DirectorAccent
    );
    AddDirectorVerticalChild(
        Parent,
        HeaderText,
        FMargin(0.0f, 8.0f, 0.0f, 3.0f)
    );
}

void USoccerDirectorTechnicalWidget::AddAttributeRow(
    UVerticalBox* Parent,
    const FString& Label,
    int32 Value
)
{
    if (Parent == nullptr)
    {
        return;
    }

    const int32 ClampedValue = FMath::Clamp(Value, 0, 100);

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    AddDirectorVerticalChild(Parent, Row, FMargin(0.0f, 1.0f));

    USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>();
    LabelSize->SetWidthOverride(160.0f);
    UTextBlock* LabelText = MakeDirectorText(WidgetTree, Label, 12, DirectorMuted);
    LabelSize->AddChild(LabelText);
    AddDirectorHorizontalChild(Row, LabelSize, FMargin(0.0f, 0.0f, 6.0f, 0.0f));

    UProgressBar* AttributeBar = WidgetTree->ConstructWidget<UProgressBar>();
    AttributeBar->SetPercent(static_cast<float>(ClampedValue) / 100.0f);
    AttributeBar->SetFillColorAndOpacity(FLinearColor(0.20f, 0.68f, 0.92f, 1.0f));
    AddDirectorHorizontalChild(
        Row,
        AttributeBar,
        FMargin(0.0f, 3.0f, 7.0f, 3.0f),
        ESlateSizeRule::Fill,
        VAlign_Center
    );

    UTextBlock* ValueText = MakeDirectorText(
        WidgetTree,
        FString::Printf(TEXT("%d"), ClampedValue),
        12,
        FLinearColor::White
    );
    AddDirectorHorizontalChild(Row, ValueText, FMargin(0.0f), ESlateSizeRule::Automatic, VAlign_Center);
}

void USoccerDirectorTechnicalWidget::AddPositionPreferenceRows(
    UVerticalBox* Parent,
    const USoccerPlayerProfile* Profile
)
{
    if (Parent == nullptr || !IsValid(Profile))
    {
        return;
    }

    AddAttributeGroupHeader(Parent, TEXT("POSICIONES"));

    if (Profile->PositionPreferences.Num() == 0)
    {
        UTextBlock* NoPositionsText = MakeDirectorText(
            WidgetTree,
            TEXT("Sin posiciones preferidas declaradas."),
            12,
            DirectorMuted
        );
        AddDirectorVerticalChild(Parent, NoPositionsText);
        return;
    }

    for (const FSoccerPlayerPositionPreference& PositionPreference : Profile->PositionPreferences)
    {
        AddAttributeRow(
            Parent,
            GetNaturalPositionText(PositionPreference.Position),
            PositionPreference.Familiarity
        );
    }
}

USoccerPlayerProfile* USoccerDirectorTechnicalWidget::FindProfile(FName PlayerId) const
{
    USoccerPlayerProfile* const* FoundProfile = ProfilesById.Find(PlayerId);
    return FoundProfile != nullptr ? *FoundProfile : nullptr;
}

FString USoccerDirectorTechnicalWidget::GetPlayerDisplayName(FName PlayerId) const
{
    const USoccerPlayerProfile* Profile = FindProfile(PlayerId);
    if (!IsValid(Profile))
    {
        return PlayerId.IsNone() ? TEXT("--") : PlayerId.ToString();
    }

    return Profile->Identity.DisplayName.IsEmpty()
        ? Profile->Identity.PlayerId.ToString()
        : Profile->Identity.DisplayName.ToString();
}

FString USoccerDirectorTechnicalWidget::GetPlayerStatusLabel(FName PlayerId) const
{
    if (!IsValid(SoccerGameInstance) || PlayerId.IsNone())
    {
        return TEXT("--");
    }

    const FName StartingSlot = SoccerGameInstance->FindStartingSlotForPlayer(PlayerId);
    if (!StartingSlot.IsNone())
    {
        return FString::Printf(TEXT("Titular · %s"), *StartingSlot.ToString());
    }

    if (SoccerGameInstance->IsPlayerOnBench(PlayerId))
    {
        return TEXT("Suplente");
    }

    return TEXT("Plantel");
}

TArray<FName> USoccerDirectorTechnicalWidget::GetOrderedAvailablePlayerIds() const
{
    TArray<FName> Result;
    Result.Reserve(AvailableProfiles.Num());

    for (USoccerPlayerProfile* Profile : AvailableProfiles)
    {
        if (IsValid(Profile) && Profile->HasValidPlayerId())
        {
            Result.Add(Profile->Identity.PlayerId);
        }
    }

    return Result;
}

TArray<FName> USoccerDirectorTechnicalWidget::GetCurrentFormationSlotIds() const
{
    TArray<FName> Result;
    if (!IsValid(SoccerGameInstance))
    {
        return Result;
    }

    const ESoccerFormationSystem FormationSystem =
        SoccerGameInstance->GetCurrentTeamSetup().FormationSystem;
    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(FormationSystem);

    Result.Reserve(FormationDefinition.Slots.Num());
    for (const FSoccerFormationSlot& FormationSlotDefinition : FormationDefinition.Slots)
    {
        Result.Add(FormationSlotDefinition.SlotId);
    }
    return Result;
}

FName USoccerDirectorTechnicalWidget::ResolveSafeFocusedSlotId() const
{
    const TArray<FName> SlotIds = GetCurrentFormationSlotIds();
    if (SlotIds.Contains(FocusedFormationSlotId))
    {
        return FocusedFormationSlotId;
    }

    if (IsValid(SoccerGameInstance) && !SelectedPlayerId.IsNone())
    {
        const FName CurrentPlayerSlot =
            SoccerGameInstance->FindStartingSlotForPlayer(SelectedPlayerId);
        if (SlotIds.Contains(CurrentPlayerSlot))
        {
            return CurrentPlayerSlot;
        }
    }

    return SlotIds.Num() > 0 ? SlotIds[0] : NAME_None;
}

void USoccerDirectorTechnicalWidget::SelectPlayer(FName PlayerId)
{
    if (!ProfilesById.Contains(PlayerId))
    {
        return;
    }

    SelectedPlayerId = PlayerId;

    if (IsValid(SoccerGameInstance))
    {
        const FName StartingSlot =
            SoccerGameInstance->FindStartingSlotForPlayer(PlayerId);
        if (!StartingSlot.IsNone())
        {
            FocusedFormationSlotId = StartingSlot;
        }
    }

    RefreshRosterList();
    RefreshFormationPitch();
    RefreshBenchList();
    RefreshSelectedPlayerDetails();
    RefreshSelectionSummary();
}

void USoccerDirectorTechnicalWidget::SelectFormationSlot(FName FormationSlotId)
{
    const TArray<FName> SlotIds = GetCurrentFormationSlotIds();
    if (!SlotIds.Contains(FormationSlotId))
    {
        return;
    }

    FocusedFormationSlotId = FormationSlotId;
    RefreshFormationPitch();
    RefreshSelectedPlayerDetails();
    RefreshSelectionSummary();
}

void USoccerDirectorTechnicalWidget::AssignSelectedPlayerToSlot(
    FName FormationSlotId
)
{
    if (
        !IsValid(SoccerGameInstance) ||
        SelectedPlayerId.IsNone() ||
        FormationSlotId.IsNone()
    )
    {
        return;
    }

    const FName CurrentStartingSlot =
        SoccerGameInstance->FindStartingSlotForPlayer(SelectedPlayerId);

    bool bChanged = false;
    if (!CurrentStartingSlot.IsNone())
    {
        if (CurrentStartingSlot == FormationSlotId)
        {
            FocusedFormationSlotId = FormationSlotId;
            RefreshFromPersistentTeamSetup(false);
            return;
        }

        bChanged = SoccerGameInstance->SwapStartingSlots(
            CurrentStartingSlot,
            FormationSlotId
        );
    }
    else if (SoccerGameInstance->IsPlayerOnBench(SelectedPlayerId))
    {
        const FName CurrentOccupant =
            SoccerGameInstance->GetPlayerInStartingSlot(FormationSlotId);
        bChanged = CurrentOccupant.IsNone()
            ? SoccerGameInstance->AssignPlayerToStartingSlot(
                SelectedPlayerId,
                FormationSlotId
            )
            : SoccerGameInstance->SwapStarterWithBench(
                FormationSlotId,
                SelectedPlayerId
            );
    }
    else
    {
        bChanged = SoccerGameInstance->AssignPlayerToStartingSlot(
            SelectedPlayerId,
            FormationSlotId
        );
    }

    if (bChanged)
    {
        FocusedFormationSlotId = FormationSlotId;
    }

    RefreshFromPersistentTeamSetup(false);
}

void USoccerDirectorTechnicalWidget::NavigatePlayer(int32 Direction)
{
    const TArray<FName> PlayerIds = GetOrderedAvailablePlayerIds();
    if (PlayerIds.Num() == 0)
    {
        return;
    }

    int32 CurrentIndex = PlayerIds.IndexOfByKey(SelectedPlayerId);
    if (CurrentIndex == INDEX_NONE)
    {
        CurrentIndex = 0;
    }

    const int32 Step = Direction >= 0 ? 1 : -1;
    const int32 NewIndex = (CurrentIndex + Step + PlayerIds.Num()) % PlayerIds.Num();
    SelectPlayer(PlayerIds[NewIndex]);
}

void USoccerDirectorTechnicalWidget::NavigateFormationSlot(int32 Direction)
{
    const TArray<FName> SlotIds = GetCurrentFormationSlotIds();
    if (SlotIds.Num() == 0)
    {
        return;
    }

    int32 CurrentIndex = SlotIds.IndexOfByKey(FocusedFormationSlotId);
    if (CurrentIndex == INDEX_NONE)
    {
        CurrentIndex = 0;
    }

    const int32 Step = Direction >= 0 ? 1 : -1;
    const int32 NewIndex = (CurrentIndex + Step + SlotIds.Num()) % SlotIds.Num();
    SelectFormationSlot(SlotIds[NewIndex]);
}

void USoccerDirectorTechnicalWidget::ConfirmFocusedSlotAssignment()
{
    AssignSelectedPlayerToSlot(FocusedFormationSlotId);
}

void USoccerDirectorTechnicalWidget::HandleRosterPlayerClicked(FName PlayerId)
{
    SelectPlayer(PlayerId);
}

void USoccerDirectorTechnicalWidget::HandleBenchPlayerClicked(FName PlayerId)
{
    SelectPlayer(PlayerId);
}

void USoccerDirectorTechnicalWidget::HandleFormationSlotClicked(
    FName FormationSlotId
)
{
    if (SelectedPlayerId.IsNone() && IsValid(SoccerGameInstance))
    {
        const FName Occupant =
            SoccerGameInstance->GetPlayerInStartingSlot(FormationSlotId);
        if (!Occupant.IsNone())
        {
            SelectPlayer(Occupant);
        }
        SelectFormationSlot(FormationSlotId);
        return;
    }

    FocusedFormationSlotId = FormationSlotId;
    AssignSelectedPlayerToSlot(FormationSlotId);
}

void USoccerDirectorTechnicalWidget::HandlePreviousFormationClicked()
{
    if (!IsValid(SoccerGameInstance))
    {
        return;
    }

    const TArray<ESoccerFormationSystem>& Systems =
        SoccerFormationLibrary::GetAllSystems();
    if (Systems.Num() == 0)
    {
        return;
    }

    const ESoccerFormationSystem CurrentSystem =
        SoccerGameInstance->GetCurrentTeamSetup().FormationSystem;
    int32 CurrentIndex = Systems.IndexOfByKey(CurrentSystem);
    if (CurrentIndex == INDEX_NONE)
    {
        CurrentIndex = 0;
    }

    const int32 NewIndex = (CurrentIndex - 1 + Systems.Num()) % Systems.Num();
    SoccerGameInstance->SetFormationSystem(Systems[NewIndex]);
    FocusedFormationSlotId = NAME_None;
    RefreshFromPersistentTeamSetup(false);
}

void USoccerDirectorTechnicalWidget::HandleNextFormationClicked()
{
    if (!IsValid(SoccerGameInstance))
    {
        return;
    }

    const TArray<ESoccerFormationSystem>& Systems =
        SoccerFormationLibrary::GetAllSystems();
    if (Systems.Num() == 0)
    {
        return;
    }

    const ESoccerFormationSystem CurrentSystem =
        SoccerGameInstance->GetCurrentTeamSetup().FormationSystem;
    int32 CurrentIndex = Systems.IndexOfByKey(CurrentSystem);
    if (CurrentIndex == INDEX_NONE)
    {
        CurrentIndex = 0;
    }

    const int32 NewIndex = (CurrentIndex + 1) % Systems.Num();
    SoccerGameInstance->SetFormationSystem(Systems[NewIndex]);
    FocusedFormationSlotId = NAME_None;
    RefreshFromPersistentTeamSetup(false);
}

void USoccerDirectorTechnicalWidget::HandleMoveSelectedToBenchClicked()
{
    if (!IsValid(SoccerGameInstance) || SelectedPlayerId.IsNone())
    {
        return;
    }

    SoccerGameInstance->MovePlayerToBench(SelectedPlayerId);
    RefreshFromPersistentTeamSetup(false);
}

void USoccerDirectorTechnicalWidget::HandleBenchMoveUpClicked()
{
    if (
        !IsValid(SoccerGameInstance) ||
        SelectedPlayerId.IsNone() ||
        !SoccerGameInstance->IsPlayerOnBench(SelectedPlayerId)
    )
    {
        return;
    }

    const TArray<FName> BenchPlayerIds = SoccerGameInstance->GetBenchPlayerIds();
    const int32 CurrentIndex = BenchPlayerIds.IndexOfByKey(SelectedPlayerId);
    if (CurrentIndex > 0)
    {
        SoccerGameInstance->ReorderBenchPlayer(SelectedPlayerId, CurrentIndex - 1);
    }
    RefreshFromPersistentTeamSetup(false);
}

void USoccerDirectorTechnicalWidget::HandleBenchMoveDownClicked()
{
    if (
        !IsValid(SoccerGameInstance) ||
        SelectedPlayerId.IsNone() ||
        !SoccerGameInstance->IsPlayerOnBench(SelectedPlayerId)
    )
    {
        return;
    }

    const TArray<FName> BenchPlayerIds = SoccerGameInstance->GetBenchPlayerIds();
    const int32 CurrentIndex = BenchPlayerIds.IndexOfByKey(SelectedPlayerId);
    if (CurrentIndex != INDEX_NONE && CurrentIndex < BenchPlayerIds.Num() - 1)
    {
        SoccerGameInstance->ReorderBenchPlayer(SelectedPlayerId, CurrentIndex + 1);
    }
    RefreshFromPersistentTeamSetup(false);
}
