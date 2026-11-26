#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SoccerTeamTypes.h"
#include "SoccerRondoGameMode.generated.h"

class ASoccerBall;
class ASoccerField;
class ASoccerAICharacter;
class ASoccerCharacterBase;
class AThirdPersonCppCharacter;
class USoccerPlayerProfile;
class USoccerPlayerAppearanceCatalog;
class USoccerClubProfile;
class USkeletalMesh;
class UMaterialInterface;
class USoundBase;

enum class ERondoContact : uint8 { Body, Control, Kick, Dribble, Aerial, Tackle };

/** Optional per-participant material overrides, exclusive to Rondo. */
USTRUCT(BlueprintType)
struct FSoccerRondoParticipantKit
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category="Rondo Kit")
    UMaterialInterface* ShirtMaterial = nullptr;

    UPROPERTY(EditAnywhere, Category="Rondo Kit")
    UMaterialInterface* ShortsMaterial = nullptr;
};

/** Standalone rules and tactical orchestration; characters and ball remain shared. */
UCLASS()
class THIRDPERSONCPP_API ASoccerRondoGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASoccerRondoGameMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    static ASoccerRondoGameMode* Find(const UObject* Context);
    static void Report(ASoccerCharacterBase* Player, ASoccerBall* ContactBall,
        ERondoContact Kind, const FVector& ContactLocation);
    bool ContainsPlayer(const ASoccerCharacterBase* Player) const;
    bool IsOurBall(const ASoccerBall* Candidate) const;
    ASoccerCharacterBase* GetCarrier() const;
    ASoccerAICharacter* GetOpponentCarrier(const ASoccerCharacterBase* Player) const;
    ASoccerBall* GetBall() const { return Ball; }
    bool CanHumanClaim(const ASoccerCharacterBase* Player) const;
    void TogglePassRequest(AThirdPersonCppCharacter* Player, ESoccerHumanPassRequestType Type);
    bool IsWaitingForAerialPass(const AThirdPersonCppCharacter* Player) const;
    FString GetStatus() const;
    ASoccerCharacterBase* GetLoco() const { return Loco; }
    ASoccerCharacterBase* GetAtRisk() const { return AtRisk; }
    const TArray<ASoccerCharacterBase*>& GetPlayers() const { return Players; }
    bool IsReady() const { return bReady; }
    bool IsFinished() const { return bFinished; }
    int32 GetTouches(const ASoccerCharacterBase* Player) const;
    TArray<ASoccerCharacterBase*> GetRankedPlayers() const;
    int32 GetRemainingSeconds() const;
    USkeletalMesh* ResolvePlayerBodyVariantMesh(FName BodyVariantId) const;
    static FString GetParticipantName(const ASoccerCharacterBase* Player);
private:
    UPROPERTY(EditDefaultsOnly, Category="Rondo") TSubclassOf<ASoccerAICharacter> FieldPlayerClass;
    UPROPERTY(EditDefaultsOnly, Category="Rondo") TSubclassOf<ASoccerBall> BallClass;
    // Club membership is independent of the temporary loco/non-loco team.
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Roster") FName RondoClubId = FName(TEXT("CLUB_BLUE"));
    // Optional explicit override: otherwise use the unique existing appearance catalog.
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Roster") USoccerPlayerAppearanceCatalog* PlayerAppearanceCatalog = nullptr;
    UPROPERTY(Transient) USoccerClubProfile* RondoClub = nullptr;
    // Slot 0 is the human; 1-6 are the bots in roster/spawn order.
    // Null materials preserve the club Home kit independently for each garment.
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Appearance")
    TArray<FSoccerRondoParticipantKit> ParticipantKits;
    UPROPERTY(EditDefaultsOnly, Category="Rondo", meta=(ClampMin="0.2", ClampMax="1.0")) float CircleDiameterWidthFraction = 0.60f;
    UPROPERTY(EditDefaultsOnly, Category="Rondo") bool bHumanStartsAsLoco = false;
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Session", meta=(ClampMin="1.0")) float SessionDurationSeconds = 300.f;
    // 6G training parity (1.0), but isolated to dribble impulses so Rondo.4 passes stay calibrated.
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Session", meta=(ClampMin="0.1", ClampMax="2.0")) float DribbleHorizontalScale = 1.f;
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Audio") USoundBase* RondoShortKickSound = nullptr;
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Audio") USoundBase* RondoStrongKickSound = nullptr;
    UPROPERTY(EditDefaultsOnly, Category="Rondo", meta=(ClampMin="0.05")) float DecisionInterval = 0.16f;
    // Same initial pass-intent timing as the match; mode owns its own rules.
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Passing", meta=(ClampMin="0.1")) float PassIntentMaxLifetime = 2.25f;
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Passing", meta=(ClampMin="0.0")) float PassIntentSlowGrace = 0.35f;
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Passing", meta=(ClampMin="0.0")) float PassIntentMinimumSpeed = 90.f;
    UPROPERTY(EditDefaultsOnly, Category="Rondo|Passing", meta=(ClampMin="0.0")) float PassIntentAbandonedDistance = 280.f;
    UPROPERTY() ASoccerField* Field = nullptr;
    UPROPERTY() ASoccerBall* Ball = nullptr;
    UPROPERTY() AThirdPersonCppCharacter* Human = nullptr;
    UPROPERTY() TArray<ASoccerCharacterBase*> Players;
    UPROPERTY() ASoccerCharacterBase* Loco = nullptr;
    UPROPERTY() ASoccerCharacterBase* LastTouch = nullptr;
    UPROPERTY() ASoccerCharacterBase* AtRisk = nullptr;
    UPROPERTY() ASoccerCharacterBase* ChallengePasser = nullptr;
    UPROPERTY() ASoccerCharacterBase* ChallengeReceiverCandidate = nullptr;
    bool bChallengePassEntered = false;
    // Tactical reception intent only; never grants possession or immunity.
    UPROPERTY() ASoccerCharacterBase* FlightReceiver = nullptr;
    UPROPERTY() ASoccerCharacterBase* FlightPasser = nullptr;
    FVector FlightOrigin = FVector::ZeroVector;
    float FlightStarted = 0.f;
    void EndPassFlight(const TCHAR* Reason, ASoccerCharacterBase* ContactPlayer = nullptr);
    FVector PassTarget(const ASoccerCharacterBase* Receiver, bool bAir) const;

    UPROPERTY() ASoccerCharacterBase* LastNormalPasser = nullptr;
    bool bReady = false;
    bool bFinished = false;
    bool bScoringActive = false;
    // Score identity is permanent, independent of the temporary logical team.
    TMap<FName, int32> TouchScores;
    // Body+control of the same continuous contact is one touch. Intentional
    // kicks/dribbles/aerial/tackles are separate, but duplicate same-frame reports are not.
    TSet<ASoccerCharacterBase*> ScoredContactEpisodes;
    TMap<ASoccerCharacterBase*, float> LastActionContactTime;
    void RecordTouch(ASoccerCharacterBase* Player, ERondoContact Kind);
    void FinishSession();
    bool bChallenge = false;
    float Radius = 0.f;
    float NextDecision = 0.f;
    float StartTime = 0.f;
    // Body hits from the native solver and the analytic sweep can overlap.
    TSet<ASoccerCharacterBase*> ActiveBodyContacts;
    int32 PassStreak = 0;
    int32 BestStreak = 0;
    int32 RoleChanges = 0;
    ESoccerHumanPassRequestType PassRequest = ESoccerHumanPassRequestType::None;
    float PassRequestUntil = 0.f;
    TMap<ASoccerCharacterBase*, float> LastPassAttempt;
    TMap<ASoccerCharacterBase*, FVector> MoveTargets;
    void InitializeRondo();
    bool BuildClubRoster(TArray<USoccerPlayerProfile*>& OutRoster);
    void ResolveAppearanceCatalog();
    bool Inside(const FVector& WorldLocation, float Inset=0.f) const;
    FVector InsideTarget(const FVector& WorldLocation, float Inset=150.f) const;
    void RefreshBoundary(const FVector& BallLocation);
    void RefreshContactEpisodes(const FVector& BallLocation);
    bool IsNearBodyContact(const ASoccerCharacterBase* Player, const FVector& BallLocation) const;
    void OnContact(ASoccerCharacterBase* Player, ERondoContact Kind, const FVector& Location);
    void SwapLoco(ASoccerCharacterBase* NewLoco);
    void FinishChallenge(bool bLocoWins);
    void UpdateTeams();
    void DecideBots();
    void ResolveBotFootContacts();
    FVector PredictChase(const ASoccerCharacterBase* Player) const;
    FVector SupportTarget(const ASoccerCharacterBase* Player, const FVector& Anchor) const;
    ASoccerCharacterBase* ChoosePassReceiver(ASoccerAICharacter* Player, bool& bOutAir, bool& bOutBlocked) const;
    void MoveBot(ASoccerAICharacter* Bot, const FVector& Target, bool bChase);
    void DrawBoundary() const;
};
