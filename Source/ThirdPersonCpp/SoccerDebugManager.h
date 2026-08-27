#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerDebugManager.generated.h"

/*
 * Central categories for every soccer debug system.
 * Gameplay HUD information (score, pass request, etc.) does not belong here.
 */
UENUM(BlueprintType)
enum class ESoccerDebugCategory : uint8
{
	General UMETA(DisplayName = "General"),
	PlayerInput UMETA(DisplayName = "Player Input"),
	PlayerMovement UMETA(DisplayName = "Player Movement"),
	AI UMETA(DisplayName = "AI General"),
	Formation UMETA(DisplayName = "Formation / Team Shape"),
	AIInterception UMETA(DisplayName = "AI Interception / Prediction"),
	Aerial UMETA(DisplayName = "Aerial / Headers"),
	Tackle UMETA(DisplayName = "Tackle"),
	Ball UMETA(DisplayName = "Ball"),
	MatchRules UMETA(DisplayName = "Match Rules"),
	Restarts UMETA(DisplayName = "Restarts"),
	Offside UMETA(DisplayName = "Offside"),
	Animation UMETA(DisplayName = "Animation"),
	GoalkeeperGeneral UMETA(DisplayName = "Goalkeeper General"),
	GoalkeeperSave UMETA(DisplayName = "Goalkeeper Save"),
	GoalkeeperDistribution UMETA(DisplayName = "Goalkeeper Distribution"),
	GoalkeeperTest UMETA(DisplayName = "Goalkeeper Test"),
	IndividualMarking UMETA(DisplayName = "Individual Marking")
};

struct FSoccerDebugPanelLine
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float Scale = 1.0f;
};

class UWorld;

UCLASS(Blueprintable)
class THIRDPERSONCPP_API ASoccerDebugManager : public AActor
{
	GENERATED_BODY()

public:
	ASoccerDebugManager();

	virtual void Tick(float DeltaTime) override;

	static ASoccerDebugManager* Get(const UObject* WorldContextObject);

	/* Master + category gate. */
	static bool IsEnabled(
		const UObject* WorldContextObject,
		ESoccerDebugCategory Category
	);

	static bool IsWorldDrawingEnabled(
		const UObject* WorldContextObject,
		ESoccerDebugCategory Category
	);

	/*
	 * Text messages are stored by the manager and drawn by GameHUD.
	 * They never depend on AddOnScreenDebugMessage.
	 */
	static void Message(
		const UObject* WorldContextObject,
		ESoccerDebugCategory Category,
		const FString& Text,
		const FColor& Color = FColor::White,
		int32 Key = INDEX_NONE
	);

	static void SetPersistentLine(
		const UObject* WorldContextObject,
		ESoccerDebugCategory Category,
		int32 Slot,
		const FString& Text,
		const FColor& Color = FColor::White,
		float Scale = 1.0f
	);

	static void DrawSphere(
		const UObject* WorldContextObject,
		ESoccerDebugCategory Category,
		const FVector& Location,
		float Radius,
		const FColor& Color,
		float Duration = 0.15f,
		int32 Segments = 12,
		float Thickness = 1.0f
	);

	static void DrawLine(
		const UObject* WorldContextObject,
		ESoccerDebugCategory Category,
		const FVector& Start,
		const FVector& End,
		const FColor& Color,
		float Duration = 0.15f,
		float Thickness = 1.0f
	);

	static void DrawString(
		const UObject* WorldContextObject,
		ESoccerDebugCategory Category,
		const FVector& Location,
		const FString& Text,
		const FColor& Color = FColor::White,
		float Duration = 0.15f
	);

	static bool ShouldShowAllGoalkeeperContactVolumes(
		const UObject* WorldContextObject
	);

	static bool ShouldShowTouchedGoalkeeperContactVolumes(
		const UObject* WorldContextObject
	);

	static bool ShouldShowGoalkeeperContactPath(
		const UObject* WorldContextObject
	);

	void BuildVisiblePanelLines(
		TArray<FSoccerDebugPanelLine>& OutLines
	) const;

	void BuildVisibleTextFeedLines(
		TArray<FSoccerDebugPanelLine>& OutLines
	) const;

	bool ShouldDrawPanel() const;
	bool ShouldDrawTextFeed() const;

	float GetPanelWidth() const;
	float GetPanelRightMargin() const;
	float GetPanelTop() const;
	float GetPanelPadding() const;
	float GetPanelLineHeight() const;
	FLinearColor GetPanelBackgroundColor() const;

	float GetTextFeedLeft() const;
	float GetTextFeedBottomMargin() const;
	float GetTextFeedLineHeight() const;
	float GetTextFeedBackgroundAlpha() const;
	UFUNCTION(BlueprintCallable, Category = "Soccer|Debug")
	void ClearGoalkeeperDebugMessages();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FTransientMessage
	{
		ESoccerDebugCategory Category = ESoccerDebugCategory::General;
		int32 Key = INDEX_NONE;
		FString Text;
		FColor Color = FColor::White;
		float ExpireTime = 0.0f;
		uint64 Sequence = 0;
	};

	struct FPersistentMessage
	{
		ESoccerDebugCategory Category = ESoccerDebugCategory::General;
		int32 Slot = 0;
		FString Text;
		FColor Color = FColor::White;
		float Scale = 1.0f;
	};

	static TMap<UWorld*, TWeakObjectPtr<ASoccerDebugManager>> InstancesByWorld;

	bool IsCategoryEnabledInternal(ESoccerDebugCategory Category) const;

	void AddTransientMessage(
		ESoccerDebugCategory Category,
		const FString& Text,
		const FColor& Color,
		int32 Key
	);

	void SetPersistentLineInternal(
		ESoccerDebugCategory Category,
		int32 Slot,
		const FString& Text,
		const FColor& Color,
		float Scale
	);

	void ClearPersistentCategoryInternal(ESoccerDebugCategory Category);
	void RemoveExpiredTransientMessages();
	void UpdateLegacyScreenMessageSuppression();

	int64 MakePersistentKey(
		ESoccerDebugCategory Category,
		int32 Slot
	) const;

	// ============================================================
	// MASTER
	// ============================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Debug|00 Master", meta = (AllowPrivateAccess = "true"))
	bool bEnableSoccerDebug = false;

	/* When true every category below is enabled; useful for a full diagnostic pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Debug|00 Master", meta = (AllowPrivateAccess = "true"))
	bool bEnableAllDebugCategories = false;

	/* Small HUD feed for transient debug messages. No large panel required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Debug|00 Master", meta = (AllowPrivateAccess = "true"))
	bool bShowDebugTextFeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Debug|00 Master", meta = (AllowPrivateAccess = "true"))
	bool bShowWorldDebugDrawings = true;

	/* Optional old-style large diagnostics panel. Disabled by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Debug|00 Master", meta = (AllowPrivateAccess = "true"))
	bool bShowDebugPanel = false;

	/*
	 * Old code still contains AddOnScreenDebugMessage calls. Keep them suppressed
	 * by default so they cannot fight with the centralized HUD feed.
	 * Enable temporarily only while migrating an old subsystem.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Debug|00 Master", meta = (AllowPrivateAccess = "true"))
	bool bAllowLegacyEngineScreenMessages = false;

	// ============================================================
	// CATEGORIES - THE ONLY PER-SYSTEM ENABLE SWITCHES
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugGeneral = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugPlayerInput = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugPlayerMovement = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugAI = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugFormation = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugIndividualMarking = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugAIInterception = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugAerial = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugTackle = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugBall = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugMatchRules = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugRestarts = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugOffside = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugAnimation = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugGoalkeeperGeneral = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugGoalkeeperSave = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugGoalkeeperDistribution = false;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|10 Categories")
	bool bDebugGoalkeeperTest = false;

	// ============================================================
	// TEXT FEED
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|20 Text Feed", meta = (ClampMin = "1"))
	int32 MaxTransientMessages = 12;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|20 Text Feed", meta = (ClampMin = "1"))
	int32 TextFeedMaxLines = 8;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|20 Text Feed", meta = (ClampMin = "0.0"))
	float DefaultMessageDuration = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|20 Text Feed")
	float TextFeedLeft = 24.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|20 Text Feed")
	float TextFeedBottomMargin = 28.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|20 Text Feed", meta = (ClampMin = "10.0"))
	float TextFeedLineHeight = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|20 Text Feed", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TextFeedBackgroundAlpha = 0.45f;

	// ============================================================
	// OPTIONAL LARGE PANEL
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|30 Panel") float PanelWidth = 650.0f;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|30 Panel") float PanelRightMargin = 26.0f;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|30 Panel") float PanelTop = 110.0f;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|30 Panel") float PanelPadding = 14.0f;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|30 Panel") float PanelLineHeight = 22.0f;
	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|30 Panel")
	FLinearColor PanelBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.72f);

	// ============================================================
	// GOALKEEPER SPECIAL DRAWING FILTERS
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|40 Goalkeeper Drawing Detail")
	bool bShowAllGoalkeeperContactVolumes = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|40 Goalkeeper Drawing Detail")
	bool bShowTouchedGoalkeeperContactVolumes = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug|40 Goalkeeper Drawing Detail")
	bool bShowGoalkeeperContactPath = true;

	TArray<FTransientMessage> TransientMessages;
	TMap<int64, FPersistentMessage> PersistentMessages;
	uint64 NextMessageSequence = 1;
	bool bLegacyMessagesAreSuppressed = false;
};
