#include "SoccerDirectorTechnicalWidget.h"
#include "SoccerUITheme.h"

#include "SoccerDirectorTechnicalIdButton.h"
#include "SoccerFormationLibrary.h"
#include "SoccerGameInstance.h"
#include "SoccerLineupEvaluationLibrary.h"
#include "SoccerPlayerProfile.h"
#include "SoccerSquadCatalog.h"
#include "SoccerUIStyleEditorController.h"

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

    // Draw the compact formation pitch with the same horizontal orientation as
    // the Formation tab: the player's goal is at the left, the opponent at right.
    // Decoration must not capture clicks intended for the position buttons.
    void AddDirectorPitchLine(
        UWidgetTree* InWidgetTree,
        UCanvasPanel* InCanvas,
        const FVector2D& Position,
        const FVector2D& Size,
        const FLinearColor& Color)
    {
        if (InWidgetTree == nullptr || InCanvas == nullptr)
        {
            return;
        }

        UBorder* PitchLine = InWidgetTree->ConstructWidget<UBorder>();
        PitchLine->SetBrushColor(Color);
        PitchLine->SetVisibility(ESlateVisibility::HitTestInvisible);
        UCanvasPanelSlot* PitchLineSlot = InCanvas->AddChildToCanvas(PitchLine);
        PitchLineSlot->SetPosition(Position);
        PitchLineSlot->SetSize(Size);
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

float USoccerDirectorTechnicalWidget::StyleNumber(const TCHAR* Key, float DefaultValue) const
{
    return StyleEditor ? StyleEditor->GetNumber(Key, DefaultValue) : DefaultValue;
}

FLinearColor USoccerDirectorTechnicalWidget::StyleColor(const TCHAR* Key, const FLinearColor& DefaultColor) const
{
    return StyleEditor ? StyleEditor->GetColor(Key, DefaultColor) : DefaultColor;
}

bool USoccerDirectorTechnicalWidget::HasFocusedStyleInput() const
{
    return StyleEditor && StyleEditor->HasFocusedInput();
}

void USoccerDirectorTechnicalWidget::HandleStyleApplied()
{
    ApplyLiveStyle();
}

void USoccerDirectorTechnicalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UIAnimations.Tick(InDeltaTime);
    const FVector2D LogicalViewport = FSoccerUITheme::GetLogicalViewportSize(this);
    if (LogicalViewport.X > 1.f && !LastResponsiveViewport.Equals(LogicalViewport, 1.f))
    {
        LastResponsiveViewport = LogicalViewport;
        ApplyLiveStyle();
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

    UIAnimations.StartEntrance(StyleRootBorder, 0.20f);
}

void USoccerDirectorTechnicalWidget::BuildWidgetTree()
{
    if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
    {
        return;
    }

    UCanvasPanel* StyleCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = StyleCanvas;
    UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>();
    StyleRootBorder = RootBorder;
    RootBorder->SetBrushColor(DirectorPanelBackground);
    RootBorder->SetPadding(FMargin(4.0f));
    UCanvasPanelSlot* BodyCanvasSlot = StyleCanvas->AddChildToCanvas(RootBorder);
    BodyCanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    BodyCanvasSlot->SetOffsets(FMargin(0.f));

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
    StyleRosterSize = RosterSize;
    RosterSize->SetWidthOverride(300.0f);
    UBorder* RosterCard = MakeDirectorCard(WidgetTree);
    StyleRosterCard = RosterCard;
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
    StyleFormationCard = FormationCard;
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
    StylePitchSize = PitchSize;
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

    // The bench list must not grow with the number of substitutes: scrolling
    // remains inside this fixed-height viewport, keeping the action buttons
    // and the parent menu footer in place.
    USizeBox* BenchScrollViewport = WidgetTree->ConstructWidget<USizeBox>();
    StyleBenchScrollSize = BenchScrollViewport;
    BenchScrollViewport->SetHeightOverride(72.0f);
    BenchScrollBox = WidgetTree->ConstructWidget<UScrollBox>();
    BenchScrollViewport->AddChild(BenchScrollBox);
    BenchListBox = WidgetTree->ConstructWidget<UVerticalBox>();
    BenchScrollBox->AddChild(BenchListBox);
    AddDirectorVerticalChild(FormationColumn, BenchScrollViewport);

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
    StyleDetailsSize = DetailsSize;
    DetailsSize->SetWidthOverride(430.0f);
    UBorder* DetailsCard = MakeDirectorCard(WidgetTree);
    StyleDetailsCard = DetailsCard;
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
    StylePortraitSize = PortraitSize;
    PortraitSize->SetHeightOverride(120.0f);
    UBorder* PortraitBorder = WidgetTree->ConstructWidget<UBorder>();
    StylePortraitBorder = PortraitBorder;
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

    StyleEditor = NewObject<USoccerUIStyleEditorController>(this);
    TArray<FSoccerEditableStyleField> EditableStyleFields;
    EditableStyleFields.Add(FSoccerEditableStyleField::Number(TEXT("RosterWidth"), TEXT("Ancho lista jugadores"), 300.f, 180.f, 510.f));
    EditableStyleFields.Add(FSoccerEditableStyleField::Number(TEXT("DetailsWidth"), TEXT("Ancho ficha jugador"), 430.f, 280.f, 680.f));
    EditableStyleFields.Add(FSoccerEditableStyleField::Number(TEXT("PitchWidth"), TEXT("Ancho cancha horizontal"), 700.f, 350.f, 950.f));
    EditableStyleFields.Add(FSoccerEditableStyleField::Number(TEXT("PitchHeight"), TEXT("Alto cancha horizontal"), 320.f, 180.f, 600.f));
    EditableStyleFields.Add(FSoccerEditableStyleField::Number(TEXT("BenchListHeight"), TEXT("Alto lista suplentes (scroll)"), 72.f, 48.f, 160.f));
    EditableStyleFields.Add(FSoccerEditableStyleField::Number(TEXT("PortraitHeight"), TEXT("Alto retrato"), 120.f, 65.f, 270.f));
    EditableStyleFields.Add(FSoccerEditableStyleField::Number(TEXT("CardPadding"), TEXT("Padding de tarjetas"), 14.f, 0.f, 38.f));
    EditableStyleFields.Add(FSoccerEditableStyleField::Number(TEXT("FontScale"), TEXT("Escala textos"), 1.f, .65f, 1.6f));
    EditableStyleFields.Add(FSoccerEditableStyleField::Color(TEXT("PanelColor"), TEXT("Color fondo plantel"), TEXT("#05080BED")));
    EditableStyleFields.Add(FSoccerEditableStyleField::Color(TEXT("CardColor"), TEXT("Color tarjetas"), TEXT("#090D12EB")));
    EditableStyleFields.Add(FSoccerEditableStyleField::Color(TEXT("PortraitColor"), TEXT("Color retrato"), TEXT("#0F141AFF")));
    EditableStyleFields.Add(FSoccerEditableStyleField::Color(TEXT("PitchColor"), TEXT("Color mini cancha"), TEXT("#09251BF0")));
    EditableStyleFields.Add(FSoccerEditableStyleField::Color(TEXT("PlayerNormal"), TEXT("Jugador normal"), TEXT("#131B22F5")));
    EditableStyleFields.Add(FSoccerEditableStyleField::Color(TEXT("PlayerStarter"), TEXT("Jugador titular"), TEXT("#14452EFF")));
    EditableStyleFields.Add(FSoccerEditableStyleField::Color(TEXT("PlayerSelected"), TEXT("Jugador seleccionado"), TEXT("#146B99FF")));
    EditableStyleFields.Add(FSoccerEditableStyleField::Color(TEXT("SlotFocused"), TEXT("Puesto enfocado"), TEXT("#1A739EFF")));
    StyleEditor->Setup(WidgetTree, StyleCanvas, TEXT("DirectorTechnicalStyle"), EditableStyleFields);
    StyleEditor->OnApplied.AddDynamic(this, &USoccerDirectorTechnicalWidget::HandleStyleApplied);
    ApplyLiveStyle();
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
    if (StyleEditor && StyleRootBorder)
    {
        StyleEditor->ApplyTextScale(StyleRootBorder, FSoccerUITheme::GetAdaptiveTextScale(
            this, StyleNumber(TEXT("FontScale"), 1.f)));
        FSoccerUITheme::ApplyButtonStates(StyleRootBorder);
    }
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
    RosterButtonsByPlayerId.Reset();

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
            ? StyleColor(TEXT("PlayerSelected"), DirectorSelectedColor)
            : (bStarter ? StyleColor(TEXT("PlayerStarter"), DirectorStarterColor) : StyleColor(TEXT("PlayerNormal"), DirectorButtonBackground))
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
        RosterButtonsByPlayerId.Add(PlayerId, PlayerButton);
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
    FieldBackground->SetBrushColor(
        StyleColor(TEXT("PitchColor"), FLinearColor(0.035f, 0.145f, 0.105f, 0.94f)));
    FieldBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
    UCanvasPanelSlot* FieldCanvasSlot = FormationCanvas->AddChildToCanvas(FieldBackground);
    FieldCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    FieldCanvasSlot->SetOffsets(FMargin(0.0f));

    // Horizontal pitch markings, mirroring the Formation page.
    const float PitchLineInset = FMath::Max(5.0f, PitchCanvasHeight * 0.025f);
    const float PitchLineThickness = 1.5f;
    const float PitchInnerWidth = FMath::Max(1.0f, PitchCanvasWidth - 2.0f * PitchLineInset);
    const float PitchInnerHeight = FMath::Max(1.0f, PitchCanvasHeight - 2.0f * PitchLineInset);
    const FLinearColor PitchLineColor(0.82f, 0.90f, 0.84f, 0.36f);

    AddDirectorPitchLine(WidgetTree, FormationCanvas,
        FVector2D(PitchLineInset, PitchLineInset),
        FVector2D(PitchInnerWidth, PitchLineThickness), PitchLineColor);
    AddDirectorPitchLine(WidgetTree, FormationCanvas,
        FVector2D(PitchLineInset, PitchCanvasHeight - PitchLineInset - PitchLineThickness),
        FVector2D(PitchInnerWidth, PitchLineThickness), PitchLineColor);
    AddDirectorPitchLine(WidgetTree, FormationCanvas,
        FVector2D(PitchLineInset, PitchLineInset),
        FVector2D(PitchLineThickness, PitchInnerHeight), PitchLineColor);
    AddDirectorPitchLine(WidgetTree, FormationCanvas,
        FVector2D(PitchCanvasWidth - PitchLineInset - PitchLineThickness, PitchLineInset),
        FVector2D(PitchLineThickness, PitchInnerHeight), PitchLineColor);
    AddDirectorPitchLine(WidgetTree, FormationCanvas,
        FVector2D(PitchCanvasWidth * 0.5f, PitchLineInset),
        FVector2D(PitchLineThickness, PitchInnerHeight), PitchLineColor);

    const float AreaWidth = PitchInnerWidth * (105.0f / 640.0f);
    const float AreaHeight = PitchInnerHeight * (205.0f / 330.0f);
    const float AreaTop = (PitchCanvasHeight - AreaHeight) * 0.5f;
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float AreaEdgeX = SideIndex == 0
            ? PitchLineInset + AreaWidth
            : PitchCanvasWidth - PitchLineInset - AreaWidth;
        AddDirectorPitchLine(WidgetTree, FormationCanvas,
            FVector2D(AreaEdgeX, AreaTop),
            FVector2D(PitchLineThickness, AreaHeight), PitchLineColor);
        const float AreaStartX = SideIndex == 0 ? PitchLineInset : AreaEdgeX;
        AddDirectorPitchLine(WidgetTree, FormationCanvas,
            FVector2D(AreaStartX, AreaTop),
            FVector2D(AreaWidth, PitchLineThickness), PitchLineColor);
        AddDirectorPitchLine(WidgetTree, FormationCanvas,
            FVector2D(AreaStartX, AreaTop + AreaHeight),
            FVector2D(AreaWidth, PitchLineThickness), PitchLineColor);
    }

    // Goal labels are at the two ends of the pitch, not above/below the players.
    const float GoalLabelWidth = FMath::Min(110.0f, PitchCanvasWidth * 0.22f);
    const float GoalLabelHeight = 18.0f;
    const float GoalLabelY = FMath::Max(0.0f, PitchCanvasHeight - 26.0f);
    UTextBlock* OwnGoalLabel = MakeDirectorText(
        WidgetTree, TEXT("ARCO PROPIO"), 10,
        FLinearColor(0.78f, 0.88f, 0.82f, 0.86f));
    OwnGoalLabel->SetJustification(ETextJustify::Left);
    UCanvasPanelSlot* OwnGoalCanvasSlot = FormationCanvas->AddChildToCanvas(OwnGoalLabel);
    OwnGoalCanvasSlot->SetPosition(FVector2D(PitchLineInset + 7.0f, GoalLabelY));
    OwnGoalCanvasSlot->SetSize(FVector2D(GoalLabelWidth, GoalLabelHeight));

    UTextBlock* OpponentGoalLabel = MakeDirectorText(
        WidgetTree, TEXT("ARCO RIVAL"), 10,
        FLinearColor(0.78f, 0.88f, 0.82f, 0.86f));
    OpponentGoalLabel->SetJustification(ETextJustify::Right);
    UCanvasPanelSlot* OpponentGoalCanvasSlot = FormationCanvas->AddChildToCanvas(OpponentGoalLabel);
    OpponentGoalCanvasSlot->SetPosition(FVector2D(
        PitchCanvasWidth - PitchLineInset - GoalLabelWidth - 7.0f, GoalLabelY));
    OpponentGoalCanvasSlot->SetSize(FVector2D(GoalLabelWidth, GoalLabelHeight));

    const FSoccerTeamSetup TeamSetup = SoccerGameInstance->GetCurrentTeamSetup();
    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(TeamSetup.FormationSystem);

    // DepthAlpha goes from goal to attack; LateralAlpha describes field width.
    // Normalize the existing slot definitions over the usable horizontal area.
    // Reserve enough room for the GK, 5-defender lines and the goal captions.
    const float MarginX = FMath::Clamp(PitchCanvasWidth * 0.095f, 32.0f, 78.0f);
    const float MarginY = FMath::Clamp(PitchCanvasHeight * 0.14f, 25.0f, 50.0f);
    const float UsableWidth = FMath::Max(1.0f, PitchCanvasWidth - MarginX * 2.0f);
    const float UsableHeight = FMath::Max(1.0f, PitchCanvasHeight - MarginY * 2.0f);
    const float ButtonWidth = FMath::Clamp(PitchCanvasWidth * 0.14f, 54.0f, 108.0f);
    // Some formations have five players on one line (lateral spacing = 0.39).
    // Size cards to avoid overlap even in a reduced viewport.
    const float ButtonHeight = FMath::Max(16.0f, FMath::Min(
        PitchCanvasHeight * 0.13f, UsableHeight * 0.195f - 3.0f));
    const bool bShowSuitabilityOnPitch = ButtonHeight >= 37.0f && ButtonWidth >= 82.0f;
    const int32 ButtonFontSize = ButtonHeight < 26.0f ? 8 : (ButtonHeight < 37.0f ? 9 : 10);
    const int32 MaxNameLength = FMath::Max(5,
        FMath::FloorToInt((ButtonWidth - 7.0f) / (ButtonFontSize * 0.58f)));

    USoccerPlayerProfile* SelectedProfile = FindProfile(SelectedPlayerId);

    for (const FSoccerFormationSlot& FormationSlotDefinition : FormationDefinition.Slots)
    {
        const FName SlotId = FormationSlotDefinition.SlotId;
        const FName OccupantPlayerId = SoccerGameInstance->GetPlayerInStartingSlot(SlotId);

        USoccerDirectorTechnicalIdButton* FormationSlotButton =
            WidgetTree->ConstructWidget<USoccerDirectorTechnicalIdButton>();
        FormationSlotButton->SetIdentifier(SlotId);
        FormationSlotButton->OnIdentifierClicked.AddDynamic(
            this, &USoccerDirectorTechnicalWidget::HandleFormationSlotClicked);

        const bool bFocused = SlotId == FocusedFormationSlotId;
        const bool bSelectedOccupant =
            !SelectedPlayerId.IsNone() && OccupantPlayerId == SelectedPlayerId;
        FormationSlotButton->SetBackgroundColor(
            bFocused
                ? StyleColor(TEXT("SlotFocused"), DirectorFocusedSlotColor)
                : (bSelectedOccupant
                    ? StyleColor(TEXT("PlayerSelected"), DirectorSelectedColor)
                    : StyleColor(TEXT("PlayerNormal"), DirectorButtonBackground)));

        FString SuitabilitySuffix;
        if (IsValid(SelectedProfile))
        {
            const FSoccerPlayerSlotSuitability Suitability =
                USoccerLineupEvaluationLibrary::EvaluatePlayerForFormationSlot(
                    SelectedProfile, TeamSetup.FormationSystem, SlotId);
            if (Suitability.bValid)
            {
                SuitabilitySuffix = FString::Printf(TEXT("\nApt. %d"), Suitability.OverallScore);
            }
        }

        const FString FullOccupantName = OccupantPlayerId.IsNone()
            ? TEXT("[ VACÍO ]")
            : GetPlayerDisplayName(OccupantPlayerId);
        FString ShortOccupantName = FullOccupantName;
        if (ShortOccupantName.Len() > MaxNameLength)
        {
            ShortOccupantName = ShortOccupantName.Left(FMath::Max(1, MaxNameLength - 3)) + TEXT("...");
        }

        const FString ButtonLabel = FString::Printf(
            TEXT("%s\n%s%s"),
            *SlotId.ToString(), *ShortOccupantName,
            bShowSuitabilityOnPitch ? *SuitabilitySuffix : TEXT(""));
        UTextBlock* SlotText = MakeDirectorText(
            WidgetTree, ButtonLabel, ButtonFontSize, FLinearColor::White);
        SlotText->SetJustification(ETextJustify::Center);
        SlotText->SetAutoWrapText(false);
        FormationSlotButton->AddChild(SlotText);
        FormationSlotButton->SetToolTipText(FText::FromString(FString::Printf(
            TEXT("%s - %s%s"), *SlotId.ToString(), *FullOccupantName, *SuitabilitySuffix)));

        const float NormalizedDepth = FMath::Clamp(
            (FormationSlotDefinition.DepthAlpha - 0.055f) / 0.70f, 0.0f, 1.0f);
        const float NormalizedLateral = FMath::Clamp(
            FormationSlotDefinition.LateralAlpha * 0.5f + 0.5f, 0.0f, 1.0f);
        const float CenterX = MarginX + NormalizedDepth * UsableWidth;
        const float CenterY = MarginY + NormalizedLateral * UsableHeight;

        UCanvasPanelSlot* ButtonCanvasSlot =
            FormationCanvas->AddChildToCanvas(FormationSlotButton);
        ButtonCanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        ButtonCanvasSlot->SetPosition(FVector2D(CenterX, CenterY));
        ButtonCanvasSlot->SetSize(FVector2D(ButtonWidth, ButtonHeight));
    }
}

void USoccerDirectorTechnicalWidget::RefreshBenchList()
{
    if (BenchListBox == nullptr || !IsValid(SoccerGameInstance))
    {
        return;
    }

    // Preserve the user's scroll position when selection/order/style refreshes
    // rebuild the list (instead of unexpectedly jumping to the first reserve).
    const float PreviousBenchOffset = BenchScrollBox ? BenchScrollBox->GetScrollOffset() : 0.0f;
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
        if (BenchScrollBox)
        {
            BenchScrollBox->SetScrollOffset(0.0f);
        }
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
            ? StyleColor(TEXT("PlayerSelected"), DirectorSelectedColor)
            : StyleColor(TEXT("PlayerNormal"), DirectorButtonBackground)
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
    if (BenchScrollBox)
    {
        BenchScrollBox->SetScrollOffset(PreviousBenchOffset);
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
    UIAnimations.StartPage(StyleDetailsCard);
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
    FSoccerUINavigation::FocusAndReveal(
        this, RosterButtonsByPlayerId.FindRef(PlayerIds[NewIndex]), false);
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

void USoccerDirectorTechnicalWidget::ApplyLiveStyle()
{
    if (!StyleEditor || !StyleRootBorder) return;
    StyleRootBorder->SetBrushColor(StyleColor(TEXT("PanelColor"), DirectorPanelBackground));
    const FLinearColor CardTint = StyleColor(TEXT("CardColor"), DirectorCardBackground);
    const float CardInnerPadding = StyleNumber(TEXT("CardPadding"), 14.f);
    UBorder* StyleCards[] = { StyleRosterCard, StyleFormationCard, StyleDetailsCard };
    for (UBorder* Card : StyleCards)
    {
        if (Card)
        {
            Card->SetBrushColor(CardTint);
            Card->SetPadding(FMargin(CardInnerPadding));
        }
    }
    if (StylePortraitBorder)
    {
        StylePortraitBorder->SetBrushColor(StyleColor(TEXT("PortraitColor"), FLinearColor(.06f, .08f, .10f, 1.f)));
    }
    const float DesiredRosterWidth = StyleNumber(TEXT("RosterWidth"), 300.f);
    const float DesiredDetailsWidth = StyleNumber(TEXT("DetailsWidth"), 430.f);
    const float DesiredPitchWidth = StyleNumber(TEXT("PitchWidth"), 700.f);
    const float RequestedPitchHeight = StyleNumber(TEXT("PitchHeight"), 320.f);
    // Keep the mini pitch landscape even when an older saved .ini contains the
    // original portrait PitchHeight. The saved value itself is not overwritten.
    const float DesiredPitchHeight = FMath::Min(RequestedPitchHeight, DesiredPitchWidth * 0.52f);
    const FVector2D LogicalViewport = FSoccerUITheme::GetLogicalViewportSize(this);
    // This widget lives inside the formation menu, which uses percentage anchors.
    // Reserve space for its parent padding, borders and the three inter-column gaps.
    const float AvailableBodyWidth = LogicalViewport.X > 1.f
        ? FMath::Max(440.f, LogicalViewport.X * .88f - 90.f)
        : DesiredRosterWidth + DesiredDetailsWidth + DesiredPitchWidth + 32.f;
    const float DesiredBodyWidth = DesiredRosterWidth + DesiredDetailsWidth + DesiredPitchWidth + 32.f;
    const float WidthScale = FMath::Min(1.f, AvailableBodyWidth / FMath::Max(1.f, DesiredBodyWidth));
    // Preserve pitch aspect ratio while reserving space for tabs/header on 720p.
    const float AvailablePitchHeight = LogicalViewport.Y > 1.f
        ? FMath::Max(260.f, LogicalViewport.Y * .70f - 90.f)
        : DesiredPitchHeight;
    const float BodyScale = FMath::Min(WidthScale,
        FMath::Min(1.f, AvailablePitchHeight / FMath::Max(1.f, DesiredPitchHeight)));
    if (StyleRosterSize) StyleRosterSize->SetWidthOverride(DesiredRosterWidth * BodyScale);
    if (StyleDetailsSize) StyleDetailsSize->SetWidthOverride(DesiredDetailsWidth * BodyScale);
    if (StyleBenchScrollSize)
    {
        // Reserve a predictable area for the bench at smaller resolutions.
        // The stored style value stays intact if the current viewport is short.
        const float RequestedBenchHeight = StyleNumber(TEXT("BenchListHeight"), 72.f);
        const float ResponsiveBenchLimit = LogicalViewport.Y > 1.f
            ? FMath::Max(52.f, LogicalViewport.Y * 0.14f)
            : 160.f;
        StyleBenchScrollSize->SetHeightOverride(
            FMath::Clamp(RequestedBenchHeight, 48.f, ResponsiveBenchLimit));
    }
    if (StylePortraitSize)
    {
        const float DesiredPortraitHeight = StyleNumber(TEXT("PortraitHeight"), 120.f);
        StylePortraitSize->SetHeightOverride(FMath::Min(DesiredPortraitHeight,
            LogicalViewport.Y > 1.f ? FMath::Max(70.f, LogicalViewport.Y * .16f) : DesiredPortraitHeight));
    }
    PitchCanvasWidth = DesiredPitchWidth * BodyScale;
    PitchCanvasHeight = DesiredPitchHeight * BodyScale;
    if (StylePitchSize)
    {
        StylePitchSize->SetWidthOverride(PitchCanvasWidth);
        StylePitchSize->SetHeightOverride(PitchCanvasHeight);
    }
    // Regenerate only presentation buttons/lines, not the persistent lineup.
    RefreshRosterList();
    RefreshBenchList();
    StyleEditor->ApplyTextScale(StyleRootBorder, FSoccerUITheme::GetAdaptiveTextScale(
        this, StyleNumber(TEXT("FontScale"), 1.f)));
    // Pitch labels are sized to their responsive cards instead of inheriting
    // potentially oversized global text scales. Keep them readable without overlap.
    RefreshFormationPitch();
    FSoccerUITheme::ApplyButtonStates(StyleRootBorder);
}
