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

enum class ERondoContact : uint8 { Body, Control, Kick, Dribble, Aerial, Tackle };

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
private:
    UPROPERTY(EditDefaultsOnly, Category="Rondo") TSubclassOf<ASoccerAICharacter> FieldPlayerClass;
    UPROPERTY(EditDefaultsOnly, Category="Rondo") TSubclassOf<ASoccerBall> BallClass;
    UPROPERTY(EditDefaultsOnly, Category="Rondo", meta=(ClampMin="0.2", ClampMax="1.0")) float CircleDiameterWidthFraction = 0.60f;
    UPROPERTY(EditDefaultsOnly, Category="Rondo") bool bHumanStartsAsLoco = false;
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
