#include "SoccerDebugManager.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

TMap<UWorld*, TWeakObjectPtr<ASoccerDebugManager>>
ASoccerDebugManager::InstancesByWorld;

ASoccerDebugManager::ASoccerDebugManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASoccerDebugManager::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		InstancesByWorld.Add(World, this);
	}

	UpdateLegacyScreenMessageSuppression();
}

void ASoccerDebugManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();

	if (World != nullptr)
	{
		TWeakObjectPtr<ASoccerDebugManager>* Existing = InstancesByWorld.Find(World);
		if (Existing != nullptr && Existing->Get() == this)
		{
			InstancesByWorld.Remove(World);
		}
	}

	/* Never leave the engine globally suppressed after this manager disappears. */
	if (bLegacyMessagesAreSuppressed && GEngine != nullptr && World != nullptr)
	{
		GEngine->Exec(World, TEXT("EnableAllScreenMessages"));
	}

	Super::EndPlay(EndPlayReason);
}

void ASoccerDebugManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RemoveExpiredTransientMessages();
	UpdateLegacyScreenMessageSuppression();
}

ASoccerDebugManager* ASoccerDebugManager::Get(const UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	TWeakObjectPtr<ASoccerDebugManager>* Cached = InstancesByWorld.Find(World);
	if (Cached != nullptr && Cached->IsValid())
	{
		return Cached->Get();
	}

	for (TActorIterator<ASoccerDebugManager> It(World); It; ++It)
	{
		ASoccerDebugManager* Manager = *It;
		if (IsValid(Manager))
		{
			InstancesByWorld.Add(World, Manager);
			return Manager;
		}
	}

	return nullptr;
}

bool ASoccerDebugManager::IsEnabled(
	const UObject* WorldContextObject,
	ESoccerDebugCategory Category
)
{
	ASoccerDebugManager* Manager = Get(WorldContextObject);
	return IsValid(Manager) && Manager->IsCategoryEnabledInternal(Category);
}

bool ASoccerDebugManager::IsWorldDrawingEnabled(
	const UObject* WorldContextObject,
	ESoccerDebugCategory Category
)
{
	ASoccerDebugManager* Manager = Get(WorldContextObject);
	return
		IsValid(Manager) &&
		Manager->bShowWorldDebugDrawings &&
		Manager->IsCategoryEnabledInternal(Category);
}

void ASoccerDebugManager::Message(
	const UObject* WorldContextObject,
	ESoccerDebugCategory Category,
	const FString& Text,
	const FColor& Color,
	int32 Key
)
{
	ASoccerDebugManager* Manager = Get(WorldContextObject);
	if (
		!IsValid(Manager) ||
		!Manager->bShowDebugTextFeed ||
		!Manager->IsCategoryEnabledInternal(Category)
	)
	{
		return;
	}

	Manager->AddTransientMessage(Category, Text, Color, Key);
}

void ASoccerDebugManager::SetPersistentLine(
	const UObject* WorldContextObject,
	ESoccerDebugCategory Category,
	int32 Slot,
	const FString& Text,
	const FColor& Color,
	float Scale
)
{
	ASoccerDebugManager* Manager = Get(WorldContextObject);
	if (!IsValid(Manager) || !Manager->IsCategoryEnabledInternal(Category))
	{
		return;
	}

	Manager->SetPersistentLineInternal(Category, Slot, Text, Color, Scale);
}

void ASoccerDebugManager::DrawSphere(
	const UObject* WorldContextObject,
	ESoccerDebugCategory Category,
	const FVector& Location,
	float Radius,
	const FColor& Color,
	float Duration,
	int32 Segments,
	float Thickness
)
{
	if (!IsWorldDrawingEnabled(WorldContextObject, Category))
	{
		return;
	}

	UWorld* World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	::DrawDebugSphere(
		World,
		Location,
		Radius,
		Segments,
		Color,
		false,
		Duration,
		0,
		Thickness
	);
}

void ASoccerDebugManager::DrawLine(
	const UObject* WorldContextObject,
	ESoccerDebugCategory Category,
	const FVector& Start,
	const FVector& End,
	const FColor& Color,
	float Duration,
	float Thickness
)
{
	if (!IsWorldDrawingEnabled(WorldContextObject, Category))
	{
		return;
	}

	UWorld* World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	::DrawDebugLine(World, Start, End, Color, false, Duration, 0, Thickness);
}

void ASoccerDebugManager::DrawString(
	const UObject* WorldContextObject,
	ESoccerDebugCategory Category,
	const FVector& Location,
	const FString& Text,
	const FColor& Color,
	float Duration
)
{
	if (!IsWorldDrawingEnabled(WorldContextObject, Category))
	{
		return;
	}

	UWorld* World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}

	::DrawDebugString(
		World,
		Location,
		Text,
		nullptr,
		Color,
		Duration,
		false
	);
}

bool ASoccerDebugManager::ShouldShowAllGoalkeeperContactVolumes(const UObject* WorldContextObject)
{
	ASoccerDebugManager* Manager = Get(WorldContextObject);
	return
		IsValid(Manager) &&
		Manager->bShowWorldDebugDrawings &&
		Manager->IsCategoryEnabledInternal(ESoccerDebugCategory::GoalkeeperSave) &&
		Manager->bShowAllGoalkeeperContactVolumes;
}

bool ASoccerDebugManager::ShouldShowTouchedGoalkeeperContactVolumes(const UObject* WorldContextObject)
{
	ASoccerDebugManager* Manager = Get(WorldContextObject);
	return
		IsValid(Manager) &&
		Manager->bShowWorldDebugDrawings &&
		Manager->IsCategoryEnabledInternal(ESoccerDebugCategory::GoalkeeperSave) &&
		Manager->bShowTouchedGoalkeeperContactVolumes;
}

bool ASoccerDebugManager::ShouldShowGoalkeeperContactPath(const UObject* WorldContextObject)
{
	ASoccerDebugManager* Manager = Get(WorldContextObject);
	return
		IsValid(Manager) &&
		Manager->bShowWorldDebugDrawings &&
		Manager->IsCategoryEnabledInternal(ESoccerDebugCategory::GoalkeeperSave) &&
		Manager->bShowGoalkeeperContactPath;
}

bool ASoccerDebugManager::IsCategoryEnabledInternal(ESoccerDebugCategory Category) const
{
	if (!bEnableSoccerDebug)
	{
		return false;
	}

	if (bEnableAllDebugCategories)
	{
		return true;
	}

	switch (Category)
	{
	case ESoccerDebugCategory::General: return bDebugGeneral;
	case ESoccerDebugCategory::PlayerInput: return bDebugPlayerInput;
	case ESoccerDebugCategory::PlayerMovement: return bDebugPlayerMovement;
	case ESoccerDebugCategory::AI: return bDebugAI;
	case ESoccerDebugCategory::Formation: return bDebugFormation;
	case ESoccerDebugCategory::AIInterception: return bDebugAIInterception;
	case ESoccerDebugCategory::Aerial: return bDebugAerial;
	case ESoccerDebugCategory::Tackle: return bDebugTackle;
	case ESoccerDebugCategory::Ball: return bDebugBall;
	case ESoccerDebugCategory::MatchRules: return bDebugMatchRules;
	case ESoccerDebugCategory::Restarts: return bDebugRestarts;
	case ESoccerDebugCategory::Offside: return bDebugOffside;
	case ESoccerDebugCategory::Animation: return bDebugAnimation;
	case ESoccerDebugCategory::GoalkeeperGeneral: return bDebugGoalkeeperGeneral;
	case ESoccerDebugCategory::GoalkeeperSave: return bDebugGoalkeeperSave;
	case ESoccerDebugCategory::GoalkeeperDistribution: return bDebugGoalkeeperDistribution;
	case ESoccerDebugCategory::GoalkeeperTest: return bDebugGoalkeeperTest;
	case ESoccerDebugCategory::IndividualMarking: return bDebugIndividualMarking;
	default: return false;
	}
}

void ASoccerDebugManager::AddTransientMessage(
	ESoccerDebugCategory Category,
	const FString& Text,
	const FColor& Color,
	int32 Key
)
{
	const float CurrentTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float SafeDuration = FMath::Max(0.01f, DefaultMessageDuration);

	if (Key != INDEX_NONE)
	{
		for (FTransientMessage& Existing : TransientMessages)
		{
			if (Existing.Category == Category && Existing.Key == Key)
			{
				Existing.Text = Text;
				Existing.Color = Color;
				Existing.ExpireTime = CurrentTime + SafeDuration;
				Existing.Sequence = NextMessageSequence++;
				return;
			}
		}
	}

	FTransientMessage NewMessage;
	NewMessage.Category = Category;
	NewMessage.Key = Key;
	NewMessage.Text = Text;
	NewMessage.Color = Color;
	NewMessage.ExpireTime = CurrentTime + SafeDuration;
	NewMessage.Sequence = NextMessageSequence++;
	TransientMessages.Add(NewMessage);

	while (MaxTransientMessages > 0 && TransientMessages.Num() > MaxTransientMessages)
	{
		TransientMessages.RemoveAt(0);
	}
}

void ASoccerDebugManager::SetPersistentLineInternal(
	ESoccerDebugCategory Category,
	int32 Slot,
	const FString& Text,
	const FColor& Color,
	float Scale
)
{
	const int64 PersistentKey = MakePersistentKey(Category, Slot);
	FPersistentMessage& MessageEntry = PersistentMessages.FindOrAdd(PersistentKey);
	MessageEntry.Category = Category;
	MessageEntry.Slot = Slot;
	MessageEntry.Text = Text;
	MessageEntry.Color = Color;
	MessageEntry.Scale = FMath::Max(0.1f, Scale);
}

void ASoccerDebugManager::ClearPersistentCategoryInternal(ESoccerDebugCategory Category)
{
	for (auto It = PersistentMessages.CreateIterator(); It; ++It)
	{
		if (It.Value().Category == Category)
		{
			It.RemoveCurrent();
		}
	}
}

void ASoccerDebugManager::RemoveExpiredTransientMessages()
{
	const float CurrentTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
	TransientMessages.RemoveAll(
		[CurrentTime](const FTransientMessage& MessageEntry)
		{
			return MessageEntry.ExpireTime <= CurrentTime;
		}
	);
}

void ASoccerDebugManager::BuildVisibleTextFeedLines(
	TArray<FSoccerDebugPanelLine>& OutLines
) const
{
	OutLines.Reset();

	if (!bEnableSoccerDebug || !bShowDebugTextFeed)
	{
		return;
	}

	TArray<const FTransientMessage*> Visible;
	for (const FTransientMessage& MessageEntry : TransientMessages)
	{
		if (IsCategoryEnabledInternal(MessageEntry.Category))
		{
			Visible.Add(&MessageEntry);
		}
	}

	Visible.Sort(
		[](const FTransientMessage& A, const FTransientMessage& B)
		{
			return A.Sequence > B.Sequence;
		}
	);

	const int32 Count = FMath::Min(TextFeedMaxLines, Visible.Num());
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		const FTransientMessage* MessageEntry = Visible[Index];
		if (MessageEntry == nullptr)
		{
			continue;
		}

		FSoccerDebugPanelLine Line;
		Line.Text = MessageEntry->Text;
		Line.Color = FLinearColor(MessageEntry->Color);
		Line.Scale = 0.95f;
		OutLines.Add(Line);
	}
}

void ASoccerDebugManager::BuildVisiblePanelLines(
	TArray<FSoccerDebugPanelLine>& OutLines
) const
{
	OutLines.Reset();
	if (!ShouldDrawPanel())
	{
		return;
	}

	FSoccerDebugPanelLine Header;
	Header.Text = TEXT("SOCCER DEBUG");
	Header.Color = FLinearColor(FColor::Cyan);
	Header.Scale = 1.1f;
	OutLines.Add(Header);

	TArray<const FPersistentMessage*> VisiblePersistent;
	for (const TPair<int64, FPersistentMessage>& Pair : PersistentMessages)
	{
		if (IsCategoryEnabledInternal(Pair.Value.Category))
		{
			VisiblePersistent.Add(&Pair.Value);
		}
	}

	VisiblePersistent.Sort(
		[](const FPersistentMessage& A, const FPersistentMessage& B)
		{
			if (A.Category != B.Category)
			{
				return static_cast<uint8>(A.Category) < static_cast<uint8>(B.Category);
			}
			return A.Slot < B.Slot;
		}
	);

	for (const FPersistentMessage* Entry : VisiblePersistent)
	{
		if (Entry == nullptr) continue;
		FSoccerDebugPanelLine Line;
		Line.Text = Entry->Text;
		Line.Color = FLinearColor(Entry->Color);
		Line.Scale = Entry->Scale;
		OutLines.Add(Line);
	}

	TArray<FSoccerDebugPanelLine> FeedLines;
	BuildVisibleTextFeedLines(FeedLines);
	OutLines.Append(FeedLines);
}

void ASoccerDebugManager::UpdateLegacyScreenMessageSuppression()
{
	UWorld* World = GetWorld();
	if (GEngine == nullptr || World == nullptr)
	{
		return;
	}

	/* Legacy messages are opt-in and still obey the master switch. */
	const bool bShouldAllowLegacy = bEnableSoccerDebug && bAllowLegacyEngineScreenMessages;
	const bool bShouldSuppress = !bShouldAllowLegacy;

	if (bShouldSuppress == bLegacyMessagesAreSuppressed)
	{
		return;
	}

	GEngine->Exec(
		World,
		bShouldSuppress ? TEXT("DisableAllScreenMessages") : TEXT("EnableAllScreenMessages")
	);
	bLegacyMessagesAreSuppressed = bShouldSuppress;
}

int64 ASoccerDebugManager::MakePersistentKey(ESoccerDebugCategory Category, int32 Slot) const
{
	const int64 CategoryPart = static_cast<int64>(static_cast<uint8>(Category));
	const int64 SlotPart = static_cast<int64>(static_cast<uint32>(Slot));
	return (CategoryPart << 32) | SlotPart;
}

bool ASoccerDebugManager::ShouldDrawPanel() const
{
	return bEnableSoccerDebug && bShowDebugPanel;
}

bool ASoccerDebugManager::ShouldDrawTextFeed() const
{
	return bEnableSoccerDebug && bShowDebugTextFeed;
}

float ASoccerDebugManager::GetPanelWidth() const { return PanelWidth; }
float ASoccerDebugManager::GetPanelRightMargin() const { return PanelRightMargin; }
float ASoccerDebugManager::GetPanelTop() const { return PanelTop; }
float ASoccerDebugManager::GetPanelPadding() const { return PanelPadding; }
float ASoccerDebugManager::GetPanelLineHeight() const { return PanelLineHeight; }
FLinearColor ASoccerDebugManager::GetPanelBackgroundColor() const { return PanelBackgroundColor; }
float ASoccerDebugManager::GetTextFeedLeft() const { return TextFeedLeft; }
float ASoccerDebugManager::GetTextFeedBottomMargin() const { return TextFeedBottomMargin; }
float ASoccerDebugManager::GetTextFeedLineHeight() const { return TextFeedLineHeight; }
float ASoccerDebugManager::GetTextFeedBackgroundAlpha() const { return TextFeedBackgroundAlpha; }
void ASoccerDebugManager::ClearGoalkeeperDebugMessages()
{
	ClearPersistentCategoryInternal(ESoccerDebugCategory::GoalkeeperGeneral);
	ClearPersistentCategoryInternal(ESoccerDebugCategory::GoalkeeperSave);
	ClearPersistentCategoryInternal(ESoccerDebugCategory::GoalkeeperDistribution);
	ClearPersistentCategoryInternal(ESoccerDebugCategory::GoalkeeperTest);

	TransientMessages.RemoveAll(
		[](const FTransientMessage& MessageEntry)
		{
			return
				MessageEntry.Category == ESoccerDebugCategory::GoalkeeperGeneral ||
				MessageEntry.Category == ESoccerDebugCategory::GoalkeeperSave ||
				MessageEntry.Category == ESoccerDebugCategory::GoalkeeperDistribution ||
				MessageEntry.Category == ESoccerDebugCategory::GoalkeeperTest;
		}
	);
}
