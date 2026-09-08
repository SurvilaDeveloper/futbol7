//SoccerMatchManager.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerTeamTypes.h"
#include "SoccerFormationTypes.h"
#include "SoccerTacticTypes.h"
#include "SoccerPlayerInstructionTypes.h"
#include "SoccerFoulTypes.h"
#include "SoccerRestartState.h"
#include "SoccerMatchState.h"
#include "SoccerMatchPeriod.h"
#include "SoccerOpponentCoachTypes.h"
#include "SoccerPenaltyKickRestart.h"
#include "SoccerFreeKickRestart.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerCoachMatchPlanTypes.h"
#include "SoccerCoachRuntimeDecisionTypes.h"
#include "SoccerMatchSquadTypes.h"
#include "SoccerMatchManager.generated.h"

class ASoccerBall;
class ASoccerField;
class ASoccerAICharacter;
class ASoccerCharacterBase;
class AThirdPersonCppCharacter;
class ASoccerOffsideLineActor;
class ASoccerRestartRadiusActor;
class ASoccerInstantReplayManager;
class UCurveTable;
class USkeletalMesh;
class USoccerPlayerAppearanceCatalog;
class USoccerClubProfile;
class USoccerSquadCatalog;
class USoccerCoachProfile;

enum class ESoccerRestartRestrictionShape : uint8
{
	None,
	Circle,
	PenaltyArea
};

// Internal open-play pass alternatives. Kept as a plain C++ enum because the
// choice is calculated at runtime and does not need to be authored as an asset.
enum class ESoccerAttackPassType : uint8
{
	ToFeet,
	ForwardSpace,
	RetentionSpace
};


UCLASS()
class THIRDPERSONCPP_API ASoccerMatchManager : public AActor
{
	GENERATED_BODY()

	friend class FSoccerPlayingState;
	friend class FSoccerBallOutOfPlayDelayState;
	friend class FSoccerPenaltyFoulDelayState;
	friend class FSoccerOffsideReviewFreezeState;
	friend class FSoccerPenaltyKickRestart;
	friend class FSoccerFreeKickRestart;
	friend class FSoccerGoalLineRestart;
	friend class FSoccerPenaltyConfigurationState;
	friend class FSoccerPenaltyPreparationState;
	friend class FSoccerPenaltyExecutionState;
	friend class FSoccerOffsideConfigurationState;
	friend class FSoccerOffsidePreparationState;
	friend class FSoccerOffsideExecutionState;
	friend class FSoccerFaultConfigurationState;
	friend class FSoccerFaultPreparationState;
	friend class FSoccerFaultExecutionState;
	friend class FSoccerCornerConfigurationState;
	friend class FSoccerCornerPreparationState;
	friend class FSoccerCornerExecutionState;
	friend class FSoccerGoalKickConfigurationState;
	friend class FSoccerGoalKickPreparationState;
	friend class FSoccerGoalKickExecutionState;
	friend class FSoccerThrowInConfigurationState;
	friend class FSoccerThrowInPreparationState;
	friend class FSoccerThrowInExecutionState;
	friend class FSoccerKickoffConfigurationState;
	friend class FSoccerKickoffPreparationState;
	friend class FSoccerKickoffExecutionState;

public:
	ASoccerMatchManager();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match")
		ASoccerBall* GetSoccerBall() const;

	/** Stage 9B central BodyVariantId -> SkeletalMesh resolver. */
	USkeletalMesh* ResolvePlayerBodyVariantMesh(FName BodyVariantId) const;

	/**
	 * Club identity occupying a temporary match side. PlayerTeam/OpponentTeam
	 * describe this match only and are not permanent club identities.
	 */
	UFUNCTION(BlueprintPure, Category = "Soccer|Club")
	USoccerClubProfile* GetClubProfileForTeam(ESoccerTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Club")
	FName GetClubIdForTeam(ESoccerTeam Team) const;

	void SetClubProfileForTeam(
		ESoccerTeam Team,
		USoccerClubProfile* ClubProfile
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Club Selection")
	bool ShouldShowClubSelectionAtMatchStart() const;

	/** Applies both selected rosters and their match kits to existing actors. */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Match Setup")
	bool MaterializeConfiguredMatchTeams();

	UFUNCTION(BlueprintPure, Category = "Soccer|AI Coach|Planning")
	FSoccerCoachMatchPlan GetOpponentTeamCoachPlan() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|AI Coach|Runtime")
	FSoccerCoachRuntimeDecision GetLastOpponentCoachRuntimeDecision() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match Squad")
	FSoccerMatchSquadState GetMatchSquadState(ESoccerTeam Team) const;

	/** Queues a valid substitution and executes it at the next safe match pause. */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Match Squad")
	bool RequestMatchSubstitution(
		ESoccerTeam Team,
		FName OutgoingPlayerId,
		FName IncomingPlayerId
	);

	UFUNCTION(BlueprintCallable, Category = "Soccer|Match Squad")
	bool CancelPendingMatchSubstitution(ESoccerTeam Team);

	UFUNCTION(BlueprintPure, Category = "Soccer|Match Squad")
	bool HasPendingMatchSubstitution(ESoccerTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match Squad")
	bool GetPendingMatchSubstitution(
		ESoccerTeam Team,
		FSoccerMatchSubstitutionRequest& OutRequest
	) const;

	/** Temporary Stage 9K keyboard tests. */
	void DebugRequestAutomaticSubstitution(ESoccerTeam Team);
	void DebugCyclePlayerTeamOutgoingSubstitute();
	void DebugCyclePlayerTeamIncomingSubstitute();
	void DebugConfirmPlayerTeamSubstitution();
	void DebugCancelPlayerTeamSubstitution();
	void DebugForceOpponentCoachSubstitutionDecision();

	/* Instant-replay recorder/playback manager. */
	ASoccerInstantReplayManager* GetInstantReplayManager() const;

	// Stage 10C geometry authority. Gameplay resolves goals and restarts from
	// ASoccerField instead of editor Target Points.
	const ASoccerField* GetSoccerField() const;

	float GetOwnGoalLineSign(ESoccerTeam Team) const;
	float GetOpponentGoalLineSign(ESoccerTeam Team) const;

	// Public field-space query used by AI decisions. Keeping this on the
	// MatchManager preserves team-side semantics while ASoccerField remains
	// the source of the underlying pitch geometry.
	FVector GetFieldAttackDirectionForTeam(
		ESoccerTeam Team
	) const;

	// Static level references such as HomePositionActor are authored for the
	// initial field ends. This keeps them tactical, while ASoccerField owns the
	// geometry and can rotate them 180 degrees after a future side swap.
	FVector GetTeamRebasedFieldReferenceLocation(
		ESoccerTeam Team,
		const FVector& ReferenceWorldLocation
	) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match")
		ESoccerPossessionTeam GetPossessionTeam() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match")
		ASoccerCharacterBase* GetPossessingCharacter() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match")
		bool HasActiveAttack() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match")
		ESoccerTeam GetCurrentAttackingTeam() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match")
		ESoccerTeam GetCurrentDefendingTeam() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match")
		ASoccerCharacterBase* GetLastTouchCharacter() const;

	// Shared pitch-geometry query used by match states and AI risk decisions.
	bool IsLocationInsidePenaltyAreaForTeam(
		const FVector& Location,
		ESoccerTeam DefendingTeam,
		float ExtraDepth = 0.0f,
		float ExtraHalfWidth = 0.0f
	) const;

	bool IsLocationInsideGoalAreaForTeam(
		const FVector& Location,
		ESoccerTeam DefendingTeam,
		float MarginCm = 0.0f
	) const;

	void RegisterIntentionalBallTouch(
		ASoccerCharacterBase* TouchingCharacter
	);

	// A goalkeeper deflection/body rebound is an intentional touch but the ball
	// remains loose. Besides recording the rules touch, clear stale possession and
	// refresh open-play roles immediately so defenders react in the same frame.
	bool RegisterGoalkeeperReboundTouch(
		ASoccerAICharacter* Goalkeeper
	);

	// Records exactly one rules touch and turns the current possession into a
	// physical loose ball. Claiming is briefly locked while everyone may chase.
	bool BeginIntentionalLooseBallTouch(
		ASoccerCharacterBase* TouchingCharacter
	);

	// Ends logical control without manufacturing another touch. Used when a
	// physically simulated ball naturally escapes an AI carrier's reach.
	void ReleaseControlledBallPossession(
		ASoccerCharacterBase* ReleasingCharacter
	);

	bool CanCharacterClaimLooseBallNow(
		const ASoccerCharacterBase* Character
	) const;

	bool TryRegisterIntentionalBallTouch(
		ASoccerCharacterBase* TouchingCharacter
	);

	/* Tackle referee: evaluates, records and may start the corresponding match restart. */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Fouls")
	FSoccerFoulDecision SubmitFoulIncident(const FSoccerFoulIncident& Incident);

	UFUNCTION(BlueprintPure, Category = "Soccer|Match")
		ESoccerTeamPhase GetTeamPhase(ESoccerTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match")
		ESoccerMatchPlayState GetMatchPlayState() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Score")
		int32 GetPlayerTeamScore() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Score")
		int32 GetOpponentTeamScore() const;

	// Match clock / period API. The clock starts when the kickoff of each half
	// is actually taken, then runs continuously until that half expires.
	UFUNCTION(BlueprintPure, Category = "Soccer|Match|Time")
	ESoccerMatchPeriod GetCurrentMatchPeriod() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match|Time")
	float GetCurrentHalfElapsedSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match|Time")
	float GetTotalMatchElapsedSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match|Time")
	float GetCurrentHalfProgress() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match|Time")
	float GetMatchProgress() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match|Time")
	float GetHalfTimeRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match|Time")
	bool IsMatchPeriodGameplayActive() const;

	// Half-time field transition is deliberately exposed only to C++. During the
	// break the normal football AI is disabled, but AI controllers may still ask
	// for their staged corridor target so both teams can exchange ends without
	// converging through the centre of the pitch.
	bool IsHalfTimeFieldTransitionActive() const;
	bool GetHalfTimeFieldTransitionMoveTarget(
		const ASoccerAICharacter* SoccerAICharacter,
		FVector& OutMoveLocation,
		float& OutAcceptanceRadius
	) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Match|Time")
	bool ShouldShowMatchClockHUD() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Coach|Opponent")
	bool IsOpponentCoachAIEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Coach|Opponent")
	ESoccerOpponentCoachMode GetOpponentCoachMode() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Coach|Opponent")
	FString GetOpponentCoachModeDisplayName() const;

	// Formation selection/assignment is shared by editor/UI and now drives open-play structural anchors.
	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	ESoccerFormationSystem GetFormationSystemForTeam(ESoccerTeam Team) const;

	UFUNCTION(BlueprintCallable, Category = "Soccer|Formation")
	void SetFormationSystemForTeam(
		ESoccerTeam Team,
		ESoccerFormationSystem FormationSystem
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	FSoccerFormationDefinition GetFormationDefinitionForTeam(ESoccerTeam Team) const;

	// Team tactics are deliberately independent from the structural formation.
	// Stage 9: the stored plan now feeds the existing open-play AI decisions.
	UFUNCTION(BlueprintPure, Category = "Soccer|Tactics")
	FSoccerTeamTacticalPlan GetTacticalPlanForTeam(ESoccerTeam Team) const;

	UFUNCTION(BlueprintCallable, Category = "Soccer|Tactics")
	void SetTacticalPlanForTeam(
		ESoccerTeam Team,
		const FSoccerTeamTacticalPlan& TacticalPlan
	);

	bool ShouldUseCollectiveTacticsForOpenPlay(ESoccerTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Tactics|Individual")
	FSoccerSlotTacticalInstruction GetSlotTacticalInstructionForTeam(
		ESoccerTeam Team,
		FName SlotId
	) const;

	UFUNCTION(BlueprintCallable, Category = "Soccer|Tactics|Individual")
	void SetSlotTacticalInstructionForTeam(
		ESoccerTeam Team,
		const FSoccerSlotTacticalInstruction& Instruction
	);

	// Replaces the complete instruction snapshot for the current formation.
	// Preset loading uses this instead of leaving stale instructions from slots
	// that belonged to a previously selected formation. Missing current slots are
	// rebuilt with their default Balanced instruction.
	UFUNCTION(BlueprintCallable, Category = "Soccer|Tactics|Individual")
	void SetSlotTacticalInstructionsForTeam(
		ESoccerTeam Team,
		const TArray<FSoccerSlotTacticalInstruction>& Instructions
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Tactics|Individual")
	TArray<FSoccerSlotTacticalInstruction> GetSlotTacticalInstructionsForTeam(
		ESoccerTeam Team
	) const;

	/**
	 * Pulls the persistent Director Technical setup into the live PlayerTeam.
	 * Lineup profile application can be disabled for in-match menu refreshes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Director Technical|Match Integration")
	bool ApplyPersistentDirectorTechnicalSetupToPlayerTeam(
		bool bApplyStartingLineupProfiles
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation|Menu")
	bool ShouldShowFormationMenuAtMatchStart() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation|Menu")
	bool ShouldPauseGameWhileFormationMenuOpen() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	FVector GetFormationSlotWorldLocation(
		ESoccerTeam Team,
		const FSoccerFormationSlot& Slot
	) const;

	/* Stable runtime assignment used by the Stage 3 open-play structural layer. */
	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	FName GetAssignedFormationSlotId(ASoccerCharacterBase* Character) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	bool GetAssignedFormationSlotForCharacter(
		ASoccerCharacterBase* Character,
		FSoccerFormationSlot& OutSlot
	) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	ASoccerCharacterBase* GetFormationSlotAssignedCharacter(
		ESoccerTeam Team,
		FName SlotId
	) const;

	bool IsKickoffTaker(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool IsKickoffFinalRunActiveForCharacter(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	float GetKickoffRunUpMoveAcceptanceRadius() const;

	FVector GetKickoffMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	// Destino comun usado por la IA durante cualquier reanudacion.
	// Devuelve la asignacion estable capturada para ese bot.
	FVector GetActiveRestartMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool IsPenaltyKickRestartActive() const;
	bool IsPenaltyKickTaker(const ASoccerCharacterBase* Character) const;
	bool IsHumanPenaltyTaker(const AThirdPersonCppCharacter* Character) const;
	bool IsPenaltyKickDefendingGoalkeeper(const ASoccerAICharacter* Character) const;
	float GetPenaltyKickRunUpMoveAcceptanceRadius() const;
	float GetPenaltyKickGoalkeeperMoveAcceptanceRadius() const;

	// Debug/test helper: starts a penalty without requiring a foul incident.
	UFUNCTION(BlueprintCallable, Category = "Soccer|Fouls|Penalty|Debug")
	bool DebugStartPenaltyKickForTeam(ESoccerTeam RestartTeam);

	void HandleGoalScored(ESoccerTeam ScoringTeam);

	ESoccerAIOrder GetAIOrderForCharacter(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	FVector GetMaintainTeamShapeMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	//defense
	FVector GetDefensiveMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool IsGoalAreaDefenderCoordinationActiveForCharacter(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool TryGetGoalAreaAttackerHoldingWaitLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		FVector& OutMoveLocation
	) const;

	bool TryAdjustGoalAreaAttackerBallChaseLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& DesiredBallLocation,
		FVector& OutMoveLocation
	) const;

	FVector GetGoalkeeperMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;


	FVector GetAttackShapeMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	FVector GetShotTargetLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	// Open-play carrying keeps the local dribble grid subordinate to the team
	// tactic. Shooting still uses GetShotTargetLocation(); only dribble/autopass
	// direction consumes this tactical intent target.
	FVector GetOpenPlayCarryIntentTargetLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	// Centro del arco propio de un equipo. Se expone para que la presion
	// defensiva pueda preferir el costado interior sin duplicar la geometria
	// del campo dentro de cada AIController.
	FVector GetOwnGoalCenterLocation(ESoccerTeam Team) const;

	ESoccerFieldZone GetFieldZoneForTeam(
		const FVector& WorldLocation,
		ESoccerTeam Team
	) const;

	ESoccerFieldZone GetCharacterFieldZone(
		const ASoccerCharacterBase* Character
	) const;

	ESoccerFieldZone GetBallFieldZoneForTeam(
		ESoccerTeam Team
	) const;

	ESoccerBallSituation GetBallSituationForCharacter(
		const ASoccerCharacterBase* Character
	) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Attack Shape")
		float GetBallCarrierForwardFreedomScore(
			const ASoccerAICharacter* BallCarrier
		) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Attack Shape")
		bool CanBallCarrierAdvanceAggressively(
			const ASoccerAICharacter* BallCarrier
		) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Attack Shape")
		bool ShouldBallCarrierPreferConservativeAction(
			const ASoccerAICharacter* BallCarrier
		) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Attack Shape")
		bool IsCharacterBreakingAttackingRole(
			const ASoccerCharacterBase* Character
		) const;

	FVector GetAttackCompensateCoverMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool FindBestAttackPassOption(
		const ASoccerAICharacter* BallCarrier,
		ASoccerCharacterBase*& OutReceiver,
		FVector& OutTargetLocation,
		ESoccerAttackPassType& OutPassType,
		float& OutScore,
		bool bUsePossessionRetentionThreshold = false,
		bool bRequireCurrentPossession = true
	) const;

	/*
	 * Toggles one of the two mutually exclusive human pass requests. Pressing
	 * the already active type disables it; pressing the other type switches it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Soccer|Human Pass Request")
		bool ToggleHumanPassRequest(
			AThirdPersonCppCharacter* RequestingHuman,
			ESoccerHumanPassRequestType RequestType
		);

	// Compatibility wrapper for callers created during the first pass-request checkpoint.
	// Evaluated before the ball carrier's normal shot/pass/autopass decision.
	bool TryExecuteActiveHumanPassRequest(
		ASoccerAICharacter* Passer
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Human Pass Request")
		bool HasActiveHumanPassRequest() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Human Pass Request")
		ESoccerHumanPassRequestType GetActiveHumanPassRequestType() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Human Pass Request")
		float GetActiveHumanPassRequestRemainingTime() const;

	bool ShouldShowHumanPassRequestHUD() const;

	bool GetHumanPassRequestHUDBrief(
		FString& OutMessage,
		FLinearColor& OutColor
	) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Attack Decision")
		bool ShouldBallCarrierPreferShot(
			const ASoccerAICharacter* BallCarrier
		) const;

	bool ShouldDefensivePressureAttemptSteal(
		const ASoccerAICharacter* PressureCharacter,
		const ASoccerCharacterBase* InPossessingCharacter,
		const ASoccerBall* InSoccerBall
	) const;

	bool CanCharacterTouchBallNow(
		const ASoccerCharacterBase* Character
	) const;

	// Direct human ball actions are allowed only while open play is active,
	// except for the dedicated human penalty-taker execution window. This is a
	// stronger gate than CanCharacterTouchBallNow(): it is used before a click,
	// chase, tackle or charged kick can arm an action that might otherwise remain
	// pending until a restart finishes.
	bool CanHumanStartBallActionNow(
		const AThirdPersonCppCharacter* Character
	) const;

	// Devuelve true mientras la pelota está físicamente
	// adjunta a las manos de un arquero que posee la pelota.
	bool IsBallSecuredByGoalkeeperHands() const;

	// Protege una pelota asegurada en las manos del arquero frente a cualquier
	// otro jugador, incluido un compañero humano. La protección desaparece al
	// soltarla o colocarla en el piso.
	bool IsBallProtectedFromCharacter(
		const ASoccerCharacterBase* Character
	) const;

	// Registra el toque y actualiza la posesión inmediatamente,
	// sin esperar al próximo intervalo táctico de Playing.
	bool RegisterControlledBallPossession(
		ASoccerCharacterBase* NewPossessingCharacter
	);

	void StartNoRetouchRestriction(
		ASoccerCharacterBase* RestrictedCharacter
	);

	void ClearNoRetouchRestriction();

	bool CanCharacterBePassReceiverNow(
		const ASoccerCharacterBase* Passer,
		const ASoccerCharacterBase* Receiver
	) const;

	// Registra que una pelota libre no es un autopase: fue enviada a un
	// receptor concreto. Mientras la intencion siga siendo valida, el
	// pasador deja de ser candidato a recuperar y vuelve a su funcion tactica.
	void RegisterOpenPlayPassIntent(
		ASoccerCharacterBase* Passer,
		ASoccerCharacterBase* IntendedReceiver,
		const FVector& PassTargetLocation
	);

	//offside

	void StartAttackRunReleaseForTeam(ESoccerTeam Team);

	void ClearAttackRunRelease();

	bool IsAttackRunReleaseActiveForTeam(ESoccerTeam Team) const;

	void ClearPendingOffsideSnapshot();

	bool IsOffsideRestartActive() const;

	bool IsOffsideRestartTaker(
		const ASoccerAICharacter* SoccerAICharacter
	) const;
	bool ShouldDefendingFreeKickCharacterFaceBall(
		const ASoccerAICharacter* SoccerAICharacter
	) const;
	bool IsFreeKickDefensiveWallMember(
		const ASoccerAICharacter* SoccerAICharacter
	) const;
	float GetFreeKickWallMoveAcceptanceRadius() const;

	bool IsHumanFreeKickTaker(
		const AThirdPersonCppCharacter* HumanCharacter
	) const;

	bool CanHumanFreeKickTakerExecuteNow(
		const AThirdPersonCppCharacter* HumanCharacter
	) const;

	// Common human execution query for stationary foot restarts. Free kicks keep
	// their dedicated runtime, while kickoff/goal kick/corner use the shared
	// non-free-kick claimant below. Input code should use this broader query.
	bool IsHumanFootRestartTaker(
		const AThirdPersonCppCharacter* HumanCharacter
	) const;

	bool CanHumanFootRestartTakerExecuteNow(
		const AThirdPersonCppCharacter* HumanCharacter
	) const;

	bool IsOffsideRestartFinalRunActiveForCharacter(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	float GetOffsideRestartRunUpMoveAcceptanceRadius() const;

	FVector GetOffsideRestartMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool IsThrowInRestartActive() const;

	// During the out-of-play continuation the ball remains physically outside,
	// but the AI may already move to the fixed throw-in tactical targets.
	bool IsThrowInDelayPositioningActive() const;

	bool IsThrowInTaker(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool IsHumanThrowInTaker(
		const AThirdPersonCppCharacter* HumanCharacter
	) const;

	bool CanHumanThrowInTakerExecuteNow(
		const AThirdPersonCppCharacter* HumanCharacter
	) const;

	bool IsHumanThrowInMovementLocked(
		const AThirdPersonCppCharacter* HumanCharacter
	) const;

	bool TryStartHumanThrowInToTarget(
		AThirdPersonCppCharacter* HumanCharacter,
		const FVector& RequestedTargetLocation
	);

	float GetThrowInPickupMoveAcceptanceRadius() const;

	bool IsThrowInTakerAnimationLocked(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	FVector GetThrowInMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool IsGoalLineRestartActive() const;

	bool IsGoalLineRestartTaker(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool IsGoalLineRestartTakerMovementLocked(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool IsGoalKickTakerFinalApproach(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	float GetGoalKickRunUpMoveAcceptanceRadius() const;

	FVector GetGoalLineRestartMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Goal Line Restart")
		ESoccerGoalLineRestartType GetGoalLineRestartType() const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Debug")
		bool ShouldDebugFreezeAICharacter(
			const ASoccerAICharacter* SoccerAICharacter
		) const;

	UFUNCTION(BlueprintPure, Category = "Soccer|Debug")
		bool ShouldDebugFrozenAIReleaseBall() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Shared visual catalog used by human and AI player profiles in this match. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Player Appearance")
	USoccerPlayerAppearanceCatalog* PlayerAppearanceCatalog = nullptr;

	/** Club controlled by the human side for this match. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Clubs")
	USoccerClubProfile* PlayerTeamClubProfile = nullptr;

	/** Rival club occupying OpponentTeam for this match. */
	UPROPERTY(EditAnywhere, Category = "Soccer|Clubs")
	USoccerClubProfile* OpponentTeamClubProfile = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Clubs")
	bool bShowClubSelectionAtMatchStart = true;

	UPROPERTY(Transient)
	FSoccerCoachMatchPlan OpponentTeamCoachPlan;

	UPROPERTY(Transient)
	USoccerCoachProfile* OpponentCoachProfile = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Coach|Opponent", meta = (AllowPrivateAccess = "true"))
	FSoccerCoachRuntimeDecision LastOpponentCoachRuntimeDecision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Soccer|Match Squad", meta = (AllowPrivateAccess = "true"))
	FSoccerMatchSquadState PlayerTeamMatchSquadState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Soccer|Match Squad", meta = (AllowPrivateAccess = "true"))
	FSoccerMatchSquadState OpponentTeamMatchSquadState;

	UPROPERTY(Transient)
	TArray<FSoccerMatchSubstitutionRequest> PendingMatchSubstitutions;

	UPROPERTY(EditAnywhere, Category = "Soccer|Match Squad", meta = (ClampMin = "0", UIMin = "0", UIMax = "7"))
	int32 MaximumSubstitutionsPerTeam = 3;

	FName DebugSelectedPlayerTeamOutgoingId = NAME_None;
	FName DebugSelectedPlayerTeamIncomingId = NAME_None;

	FSoccerMatchSquadState& GetMutableMatchSquadState(ESoccerTeam Team);
	const FSoccerMatchSquadState& GetMatchSquadStateRef(ESoccerTeam Team) const;
	void InitializeMatchSquadState(
		ESoccerTeam Team,
		FName ClubId,
		const TMap<FName, FName>& StartingLineupBySlot,
		const TArray<FName>& BenchPlayerIds
	);
	void UpdatePendingMatchSubstitutions();
	bool IsSafeMomentForSubstitution() const;
	bool ExecuteMatchSubstitution(const FSoccerMatchSubstitutionRequest& Request);
	void ShowDebugPlayerTeamSubstitutionSelection(const FString& Prefix) const;

	int32 ApplySquadCatalogProfilesToTeam(
		ESoccerTeam Team,
		USoccerSquadCatalog* SquadCatalog
	);
	int32 ApplyCoachMatchPlanToTeam(
		ESoccerTeam Team,
		const FSoccerCoachMatchPlan& CoachPlan
	);
	int32 ApplySelectedClubKitsToTeam(ESoccerTeam Team);
	void ApplySelectedClubKitToCharacter(ASoccerCharacterBase* Character);

	// Explicit match-state machine. Concrete restart families and Playing are
	// migrated incrementally; MatchPlayState remains only as a compatibility bridge.
	bool ActivateMatchState(TUniquePtr<ISoccerMatchState> NewState);
	void UpdateActiveMatchState(float DeltaTime);
	void RequestMatchStateTransition(ESoccerMatchStateTransition Transition);
	void ApplyPendingMatchStateTransition();
	void ClearActiveMatchState();
bool IsPenaltyMatchStateActive() const;
	bool IsOffsideMatchStateActive() const;
	bool IsFaultMatchStateActive() const;
	bool IsCornerMatchStateActive() const;
	bool IsGoalKickMatchStateActive() const;
	bool IsThrowInMatchStateActive() const;
	bool IsKickoffMatchStateActive() const;

	TUniquePtr<ISoccerMatchState> ActiveMatchState;
	ESoccerMatchStateTransition PendingMatchStateTransition = ESoccerMatchStateTransition::None;

	void FindSoccerBall();
	void InitializeInstantReplayRecorder();
	void ShutdownInstantReplayRecorder();
	void TryStartGoalInstantReplay(ESoccerTeam ScoringTeam);
	void StartPendingGoalInstantReplay();
	void FindSoccerField();
	void InitializeTeamFieldSides();
	void CaptureInitialHumanFieldReferences();
	void ApplySecondHalfFieldSideSwap();
	void RepositionHumansForCurrentFieldSide();

	void InitializeMatchClock();
	void UpdateMatchClock(float DeltaTime);
	void StartCurrentHalfClockIfNeeded();
	void BeginHalfTime();
	void CompleteHalfTime();
	void BeginFullTime();
	void PrepareForMatchPeriodBreak();
	void SetMatchPeriodHumanMoveLock(bool bLocked);

	void InitializeHalfTimeFieldTransition();
	void UpdateHalfTimeFieldTransition();
	void ResetHalfTimeFieldTransition();
	bool AreAllHalfTimeFieldTransitionBotsComplete() const;

	void InitializeOpponentCoachAI();
	void UpdateOpponentCoachAI(float DeltaTime);
	void UpdateOpponentCoachSubstitutionAI(float DeltaTime);
	void SynchronizeMatchSquadActiveSlotsFromActors(ESoccerTeam Team);
	bool CanOpponentCoachChangePlanNow() const;
	ESoccerOpponentCoachMode DetermineDesiredOpponentCoachMode(
		float& OutEffectiveThreshold,
		FString& OutReason
	) const;
	USoccerCoachProfile* ResolveOpponentCoachProfile() const;
	float GetOpponentCoachEffectiveDecisionInterval() const;
	float GetOpponentCoachEffectiveMinimumChangeGap() const;
	float GetOpponentCoachAdjustedModeThreshold(
		ESoccerOpponentCoachMode Mode,
		bool bLargeScoreDifference
	) const;
	float CalculateFormationAttackBias(
		ESoccerFormationSystem FormationSystem
	) const;
	void ApplyOpponentCoachMode(ESoccerOpponentCoachMode NewMode);
	ESoccerFormationSystem GetOpponentCoachFormationForMode(
		ESoccerOpponentCoachMode Mode
	) const;
	FSoccerTeamTacticalPlan BuildOpponentCoachTacticalPlanForMode(
		ESoccerOpponentCoachMode Mode
	) const;
	TArray<FSoccerSlotTacticalInstruction> BuildOpponentCoachIndividualInstructionsForMode(
		ESoccerOpponentCoachMode Mode,
		ESoccerFormationSystem FormationSystem
	) const;
	FString GetOpponentCoachModeDisplayNameForMode(ESoccerOpponentCoachMode Mode) const;

	void UpdateHumanPassRequestState();

	void ClearActiveHumanPassRequest(
		const FString& FeedbackMessage,
		const FLinearColor& FeedbackColor,
		bool bShowFeedback
	);

	void SetHumanPassRequestHUDBrief(
		const FString& Message,
		const FLinearColor& Color
	);

	ASoccerAICharacter* FindCurrentHumanPassRequestPasser(
		const AThirdPersonCppCharacter* RequestingHuman
	) const;


	void UpdateOpenPlayPassIntent();

	void ClearOpenPlayPassIntent();

	bool IsOpenPlayPassIntentValid() const;

	bool HasOpenPlayPassIntentForTeam(ESoccerTeam Team) const;

	void DetectPossession();

	void AssignFreeBallRoles();

	void AssignAttackDefenseRoles();

	void StartKickoff(ESoccerTeam TeamTakingKickoff);

	ASoccerAICharacter* FindKickoffTaker(
		ESoccerTeam TeamTakingKickoff
	) const;

	ASoccerAICharacter* FindKickoffReceiver(
		ESoccerTeam TeamTakingKickoff,
		const ASoccerAICharacter* KickoffTaker
	) const;

	void RecalculateKickoffRunUpGeometry();




	void ResetKickoffRunUpState();

	bool BuildRestartKickRunGeometryFromCurrentTaker(
		const ASoccerAICharacter* Taker,
		float RunThroughDistance,
		FVector& OutRunDirection,
		FVector& OutRunThroughLocation
	) const;

	void InitializeRestartKickContactTracking(
		FRestartKickContactTracker& Tracker,
		const ASoccerAICharacter* Taker
	) const;

	void ResetRestartKickContactTracking(
		FRestartKickContactTracker& Tracker
	) const;

	ERestartKickContactResult EvaluateRestartKickContact(
		const ASoccerAICharacter* Taker,
		const FVector& RunDirection,
		float MaxBallSurfaceGap,
		float MinimumFacingDot,
		FRestartKickContactTracker& Tracker
	) const;


	///////

	bool AreKickoffPlayersInLegalPositions() const;

	bool IsCharacterInLegalKickoffPosition(
		const ASoccerCharacterBase* Character
	) const;

	bool IsCharacterInOwnHalfForKickoff(
		const ASoccerCharacterBase* Character
	) const;

	bool IsCharacterInsideKickoffCenterCircle(
		const ASoccerCharacterBase* Character
	) const;

	FVector GetKickoffCenterLocation() const;

	//////

	FVector ProjectLocationToNavigation(
		const FVector& DesiredLocation,
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	ESoccerTeam GetOppositeTeam(ESoccerTeam Team) const;

	float GetFieldLateralOffsetForTeam(
		const FVector& WorldLocation,
		ESoccerTeam Team
	) const;

	FVector GetFieldRightDirectionForTeam(
		ESoccerTeam Team
	) const;

	void DrawFormationDebug() const;

	void DrawFormationDebugForTeam(
		ESoccerTeam Team
	) const;

	void DrawIndividualMarkingDebug() const;

	void DrawIndividualMarkingDebugForTeam(
		ESoccerTeam Team
	) const;

	void RebuildFormationAssignmentsForTeam(
		ESoccerTeam Team,
		ESoccerFormationSystem PreviousFormationSystem,
		bool bPreserveExistingAssignments
	);

	void EnsureSlotTacticalInstructionsForTeam(ESoccerTeam Team);

	TArray<FSoccerSlotTacticalInstruction>&
	GetMutableSlotTacticalInstructionsForTeam(ESoccerTeam Team);

	const TArray<FSoccerSlotTacticalInstruction>&
	GetSlotTacticalInstructionsForTeamInternal(ESoccerTeam Team) const;

	// Stage 7: resolve the instruction owned by the formation slot currently
	// occupied by this character. Individual instructions only participate in
	// open play; restart positioning and emergency restart logic stay isolated.
	bool TryGetOpenPlayIndividualInstructionForCharacter(
		const ASoccerCharacterBase* Character,
		FSoccerSlotTacticalInstruction& OutInstruction
	) const;

	ESoccerAIOrder ApplyIndividualAttackInstructionToOrder(
		const ASoccerAICharacter* SoccerAICharacter,
		ESoccerAIOrder BaseOrder
	) const;

	// Stage 8: explicit man-marking requests are coordinated at team level.
	// A slot may request one opponent slot, but emergency pressure/cover roles
	// remain authoritative and duplicate target assignments are avoided.
	void ClearExplicitIndividualMarkingAssignmentsForTeam(ESoccerTeam Team);

	void RebuildExplicitIndividualMarkingAssignmentsForTeam(
		ESoccerTeam DefendingTeam,
		const TArray<const ASoccerAICharacter*>& ExcludedCharacters
	);

	ASoccerCharacterBase* ResolveRequestedIndividualMarkingTarget(
		const ASoccerAICharacter* Marker
	) const;

	ASoccerCharacterBase* GetExplicitIndividualMarkingTarget(
		const ASoccerAICharacter* Marker
	) const;

	ASoccerCharacterBase* GetCollectiveManMarkingTarget(
		const ASoccerAICharacter* Marker
	) const;

	ASoccerCharacterBase* GetDefensiveMarkedReceiverForCharacter(
		const ASoccerAICharacter* Marker
	) const;

	TMap<ASoccerAICharacter*, ASoccerCharacterBase*>&
	GetMutableExplicitIndividualMarkingAssignmentsForTeam(ESoccerTeam Team);

	const TMap<ASoccerAICharacter*, ASoccerCharacterBase*>&
	GetExplicitIndividualMarkingAssignmentsForTeamInternal(
		ESoccerTeam Team
	) const;

	TMap<ASoccerAICharacter*, ASoccerCharacterBase*>&
	GetMutableCollectiveManMarkingAssignmentsForTeam(ESoccerTeam Team);

	const TMap<ASoccerAICharacter*, ASoccerCharacterBase*>&
	GetCollectiveManMarkingAssignmentsForTeamInternal(ESoccerTeam Team) const;

	void CollectFormationPlayersForTeam(
		ESoccerTeam Team,
		TArray<ASoccerCharacterBase*>& OutPlayers
	) const;

	FVector2D GetFormationReferenceCoordinatesForCharacter(
		ESoccerTeam Team,
		const ASoccerCharacterBase* Character
	) const;

	float CalculateFormationAssignmentCost(
		ESoccerTeam Team,
		const ASoccerCharacterBase* Character,
		const FSoccerFormationSlot& CandidateSlot,
		const FSoccerFormationSlot* PreviousSlot,
		FName PreviousSlotId
	) const;

	const FSoccerFormationSlot* FindFormationSlotById(
		const FSoccerFormationDefinition& Definition,
		FName SlotId
	) const;

	TMap<ASoccerCharacterBase*, FName>& GetMutableFormationAssignmentsForTeam(
		ESoccerTeam Team
	);

	const TMap<ASoccerCharacterBase*, FName>& GetFormationAssignmentsForTeamInternal(
		ESoccerTeam Team
	) const;

	bool ShouldUseFormationForOpenPlayStructure(
		const ASoccerCharacterBase* Character
	) const;

	bool TryGetFormationStructuralReference(
		const ASoccerCharacterBase* Character,
		FSoccerFormationSlot& OutSlot,
		FVector& OutWorldLocation
	) const;

	FVector BuildDynamicTeamShapeLocation(
		ESoccerTeam Team,
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	FVector BuildDynamicTeamShapeLocationRaw(
		ESoccerTeam Team,
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	void BeginLiveTacticalShapeTransitionForTeam(ESoccerTeam Team);
	void UpdateLiveTacticalShapeTransitions();
	void ClearLiveTacticalShapeTransitionForTeam(ESoccerTeam Team);
	float GetLiveTacticalShapeTransitionAlpha(ESoccerTeam Team) const;
	FVector ApplyLiveTacticalShapeTransitionToLocation(
		ESoccerTeam Team,
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& DesiredLocation
	) const;
	bool AreTeamTacticalPlansEqual(
		const FSoccerTeamTacticalPlan& A,
		const FSoccerTeamTacticalPlan& B
	) const;

	float GetDynamicShapeBaseDepthAlpha(
		ESoccerTeam Team,
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	float GetDynamicShapeBaseDepthAlphaForRole(
		ESoccerTeamPhase TeamPhase,
		ESoccerPlayerRole PlayerRole
	) const;

	float GetFormationNeutralDepthAlphaForRole(
		ESoccerPlayerRole PlayerRole
	) const;

	// Stage 9: the collective plan modifies the existing open-play systems.
	// These helpers deliberately return neutral values outside Playing/restarts.
	const FSoccerTeamTacticalPlan& GetTacticalPlanForTeamInternal(
		ESoccerTeam Team
	) const;

	bool ShouldApplyCollectiveTacticsToOpenPlay(ESoccerTeam Team) const;
	void UpdateCollectiveTacticalTransitionTracking();
	bool IsCollectiveAttackingTransitionActiveForTeam(ESoccerTeam Team) const;
	bool IsCollectiveDefensiveTransitionActiveForTeam(ESoccerTeam Team) const;

	float GetCollectiveDefensiveBlockDepthOffsetAlpha(ESoccerTeam Team) const;
	float GetCollectiveAttackWidthScale(ESoccerTeam Team) const;
	float GetCollectiveAttackChannelLateralShift(ESoccerTeam Team) const;
	float GetCollectivePassToSpaceLeadDistance(ESoccerTeam Team) const;
	float GetCollectivePassRequiredScoreAdjustment(ESoccerTeam Team) const;

	bool ShouldCollectivePressureEngageBall(
		ESoccerTeam DefendingTeam,
		const ASoccerAICharacter* PressureCharacter
	) const;

	void FindClosestTwoAICharactersToBall(
		ESoccerTeam Team,
		ASoccerAICharacter*& OutClosest,
		ASoccerAICharacter*& OutSecondClosest
	);

	float GetBallRecoveryRankingTime(
		const ASoccerCharacterBase* Candidate
	) const;

	bool ShouldAIYieldBallRecoveryToHuman(
		ESoccerTeam Team,
		const ASoccerAICharacter* CandidateAI
	) const;

	ASoccerAICharacter* FindActiveAIAutoPassRecoveryCharacterForTeam(
		ESoccerTeam Team
	) const;

	bool IsImmediateDefensiveDangerForTeam(ESoccerTeam Team) const;
	bool IsDangerousLooseBallEmergencyForTeam(ESoccerTeam Team) const;
	bool TryGetDangerousLooseBallEmergencyDefendingTeam(
		ESoccerTeam& OutDefendingTeam
	) const;

	ESoccerPossessionTeam ConvertTeamToPossessionTeam(ESoccerTeam Team) const;

	bool DoesTeamHavePossession(ESoccerTeam Team) const;

	void ClearAttackState();
	void ClearFreeBallChaserMemory();

	void UpdateActiveRestartRestrictionSystem(float DeltaTime);

	ESoccerRestartRestrictionShape
	GetActiveRestartRestrictionShape() const;

	FVector GetActiveRestartRestrictionCenter() const;

	float GetActiveRestartRestrictionRadius() const;


	bool IsCharacterIllegalForActiveRestart(
		const ASoccerCharacterBase* Character
	) const;

	bool IsLocationIllegalForActiveRestart(
		const FVector& Location,
		ESoccerTeam CharacterTeam
	) const;

	bool AreActiveRestartOpponentsLegal() const;

	FVector BuildActiveRestartOpponentLegalLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	FVector BuildCircularRestartOpponentMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& DesiredTacticalLocation,
		const FVector& RestrictionCenter,
		float RequiredDistance,
		float ExtraDistance
	) const;

	FVector BuildPenaltyAreaRestartOpponentMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& DesiredTacticalLocation
	) const;

	float GetActiveRestartRestrictionViolationDepth(
		const FVector& Location
	) const;

	void UpdateActiveRestartHumanRestrictionIndicator();

	bool IsHumanOpponentIllegalForActiveRestart() const;

	bool IsActiveRestartHumanIndicatorEnabled() const;

	void SpawnActiveRestartHumanRestrictionIndicator();

	void DestroyActiveRestartHumanRestrictionIndicator();

	void UpdateActiveRestartIllegalBotRecovery(float DeltaTime);

	FVector BuildRestartEmergencyRecoveryStep(
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& LegalTargetLocation,
		float DeltaTime
	) const;

	void ResetActiveRestartRestrictionRecovery();

	void BeginRestartContext(
		ESoccerRestartType RestartType,
		ESoccerTeam RestartTeam,
		const FVector& RestartLocation
	);

	void EndRestartContext();

	bool IsRestartContextActive() const;

	FVector BuildActiveRestartMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	void CaptureActiveRestartAITargetLocations(
		bool bLegalPhase
	);

	float GetActiveRestartAcceptanceRadius(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool AreAllActiveRestartBotsSettled() const;

	bool AreActiveRestartLegalConditionsSatisfied() const;

	bool UpdateActiveRestartReadiness(
		float SetupStartTime,
		float MinimumSetupTime
	);

	void ResetActiveRestartReadyHold();

	bool UpdateNonFreeKickHumanTakerClaimDuringPreparation(
		ESoccerRestartType ExpectedRestartType
	);

	bool IsNonFreeKickHumanTakerClaimedFor(
		ESoccerRestartType ExpectedRestartType
	) const;

	bool ShouldNonFreeKickHumanTakerKeepExecutionClaim(
		ESoccerRestartType ExpectedRestartType
	) const;

	void AuthorizeNonFreeKickHumanTakerExecution(
		ESoccerRestartType ExpectedRestartType
	);

	void ReleaseNonFreeKickHumanTakerClaim();
	void ResetNonFreeKickHumanTakerRuntime();

	FVector BuildNonFreeKickFallbackTakerMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	bool IsTeamCurrentlyAttacking(ESoccerTeam Team) const;

	bool IsTeamCurrentlyDefending(ESoccerTeam Team) const;

	FVector GetAttackReferenceLocation(
		ESoccerTeam AttackingTeam
	) const;

	FVector BuildAttackShapeLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		ESoccerAIOrder AttackOrder
	) const;

	float GetAttackDepthAlphaForLocation(
		const FVector& Location,
		ESoccerTeam Team
	) const;

	int32 CountAdvancedTeammatesForCarrier(
		const ASoccerAICharacter* BallCarrier,
		float MinDepthAlpha
	) const;

	int32 CountCoverTeammatesBehindCarrier(
		const ASoccerAICharacter* BallCarrier,
		float MinBehindDepthAlphaGap
	) const;

	int32 CountOpponentPressureAroundCarrier(
		const ASoccerAICharacter* BallCarrier,
		float PressureRadius
	) const;

	bool HasClearForwardLaneForCarrier(
		const ASoccerAICharacter* BallCarrier,
		float LookAheadDistance,
		float LaneHalfWidth
	) const;

	ASoccerCharacterBase* GetCurrentAttackingBallCarrier(
		ESoccerTeam Team
	) const;

	ASoccerAICharacter* FindBestAttackCompensationAI(
		ESoccerTeam Team
	) const;

	float GetCarrierSoftMaxDepthAlphaForRole(
		ESoccerPlayerRole PlayerRole
	) const;

	FVector BuildAttackCompensationCoverLocation(
		const ASoccerAICharacter* CompensatingCharacter,
		const ASoccerCharacterBase* AdvancedCarrier
	) const;

	FVector AdjustAttackMoveLocationUsingSpace(
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& BaseLocation,
		ESoccerAIOrder AttackOrder
	) const;

	float ScoreAttackSpaceCandidate(
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& CandidateLocation,
		const FVector& BaseLocation,
		ESoccerAIOrder AttackOrder
	) const;

	bool IsOpponentBlockingLaneToLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& TargetLocation,
		float LaneHalfWidth
	) const;

	bool BuildAttackPassTargetLocation(
		const ASoccerAICharacter* BallCarrier,
		const ASoccerCharacterBase* Receiver,
		ESoccerAIOrder ReceiverOrder,
		ESoccerAttackPassType PassType,
		FVector& OutTargetLocation
	) const;

	float ScoreAttackPassOption(
		const ASoccerAICharacter* BallCarrier,
		const ASoccerCharacterBase* Receiver,
		const FVector& PassTargetLocation,
		ESoccerAttackPassType PassType
	) const;

	float GetEarliestOpponentArrivalTimeToLocation(
		ESoccerTeam Team,
		const FVector& TargetLocation
	) const;

	// Stage 3: the tactical ReceiverAI remains responsible for preparation.
	// At execution time a bot may instead choose the human as the real pass
	// receiver when that option is safe and competitive with the planned bot.
	float ScoreRestartPassReceiverCandidate(
		ESoccerTeam RestartTeam,
		const ASoccerAICharacter* TakerAI,
		const ASoccerCharacterBase* Receiver,
		const FVector& TargetLocation,
		bool bAerialPass
	) const;

	void SelectActiveRestartExecutionReceiver(
		ESoccerTeam RestartTeam,
		ASoccerAICharacter* TakerAI,
		ASoccerAICharacter* PlannedReceiverAI,
		bool bAerialPass
	);

	FVector GetActiveRestartExecutionTargetLocation(
		const FVector& FallbackLocation
	) const;

	ASoccerCharacterBase* GetActiveRestartExecutionReceiver() const
	{
		return ActiveRestartExecutionReceiver;
	}

	bool IsActiveRestartExecutionReceiverHuman() const
	{
		return bActiveRestartExecutionReceiverIsHuman;
	}

	void ClearActiveRestartExecutionReceiver();

	FVector BuildHumanRequestedPassTargetLocation(
		const ASoccerAICharacter* Passer,
		const AThirdPersonCppCharacter* RequestingHuman,
		ESoccerHumanPassRequestType RequestType
	) const;

	bool IsHumanRequestedPassSafe(
		const ASoccerAICharacter* Passer,
		const AThirdPersonCppCharacter* RequestingHuman,
		ESoccerHumanPassRequestType RequestType,
		const FVector& PassTargetLocation,
		float& OutSafetyScore,
		FString& OutRejectReason
	) const;

	void ShowHumanPassRequestDebugMessage(
		const FString& Message,
		const FColor& Color
	);

	int32 CountOpponentsAroundLocation(
		ESoccerTeam Team,
		const FVector& Location,
		float Radius
	) const;

	int32 CountTeammatesAroundLocation(
		ESoccerTeam Team,
		const FVector& Location,
		float Radius,
		const ASoccerCharacterBase* IgnoreA,
		const ASoccerCharacterBase* IgnoreB
	) const;

	bool IsOpponentBlockingLaneBetweenLocations(
		ESoccerTeam Team,
		const FVector& StartLocation,
		const FVector& TargetLocation,
		float LaneHalfWidth
	) const;

	ASoccerAICharacter* FindBestAttackingSupportAI(
		ESoccerTeam AttackingTeam,
		const TArray<const ASoccerAICharacter*>& ExcludedCharacters
	) const;

	float ScoreAttackingSupportCandidate(
		ESoccerTeam AttackingTeam,
		const ASoccerAICharacter* Candidate
	) const;

	ASoccerAICharacter* FindBestAttackingSecondaryAI(
		ESoccerTeam AttackingTeam,
		const TArray<const ASoccerAICharacter*>& ExcludedCharacters,
		const ASoccerAICharacter* SelectedSupportAI
	) const;

	float ScoreAttackingSecondaryCandidate(
		ESoccerTeam AttackingTeam,
		const ASoccerAICharacter* Candidate,
		const ASoccerAICharacter* SelectedSupportAI
	) const;

	//defense
	FVector BuildDefendProtectGoalLaneLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	FVector BuildDefendCoverCenterLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	FVector BuildDefendCompactShapeLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	ASoccerAICharacter* FindGoalkeeperForTeam(
		ESoccerTeam Team
	) const;

	bool TryBuildGoalAreaDefenderCoordinationContext(
		const ASoccerAICharacter* SoccerAICharacter,
		ASoccerAICharacter*& OutGoalkeeper,
		FVector& OutThreatLocation,
		FVector& OutOwnGoalLocation,
		FVector& OutAttackDirection,
		FVector& OutRightDirection
	) const;

	FVector AdjustDefensiveMoveLocationForGoalAreaCoordination(
		const ASoccerAICharacter* SoccerAICharacter,
		ESoccerAIOrder CurrentOrder,
		const FVector& BaseDesiredLocation
	) const;

	FVector BuildGoalAreaFarPostCoverLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		const ASoccerAICharacter* Goalkeeper,
		const FVector& ThreatLocation,
		const FVector& OwnGoalLocation,
		const FVector& AttackDirection,
		const FVector& RightDirection
	) const;

	FVector BuildGoalAreaSecondaryCoverLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		const ASoccerAICharacter* Goalkeeper,
		const FVector& ThreatLocation,
		const FVector& OwnGoalLocation,
		const FVector& AttackDirection,
		const FVector& RightDirection
	) const;

	FVector MoveDefensiveLocationOutsideGoalkeeperCorridor(
		const ASoccerAICharacter* SoccerAICharacter,
		const ASoccerAICharacter* Goalkeeper,
		const FVector& ThreatLocation,
		const FVector& OwnGoalLocation,
		const FVector& AttackDirection,
		const FVector& RightDirection,
		const FVector& DesiredLocation
	) const;

	bool IsLocationInsideGoalkeeperMobilityCorridor2D(
		const FVector& Location,
		const FVector& GoalkeeperLocation,
		const FVector& ThreatLocation,
		float CorridorHalfWidth
	) const;

	bool TryBuildGoalAreaAttackerRespectContext(
		const ASoccerAICharacter* SoccerAICharacter,
		ASoccerAICharacter*& OutGoalkeeper,
		FVector& OutBallLocation,
		FVector& OutOpponentGoalLocation,
		FVector& OutFieldOutwardDirection,
		FVector& OutRightDirection,
		bool& bOutGoalkeeperHoldingBall,
		bool& bOutGoalkeeperActionActive
	) const;

	FVector AdjustAttackMoveLocationForGoalAreaRespect(
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& DesiredLocation
	) const;

	FVector BuildGoalAreaAttackerHoldingWaitLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		const ASoccerAICharacter* Goalkeeper,
		const FVector& OpponentGoalLocation,
		const FVector& FieldOutwardDirection,
		const FVector& RightDirection
	) const;

	FVector BuildGoalAreaAttackerSideApproachLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		const ASoccerAICharacter* Goalkeeper,
		const FVector& BallLocation,
		const FVector& OpponentGoalLocation,
		const FVector& FieldOutwardDirection,
		const FVector& RightDirection
	) const;

	FVector MoveAttackLocationOutsideGoalkeeperCorridor(
		const ASoccerAICharacter* SoccerAICharacter,
		const ASoccerAICharacter* Goalkeeper,
		const FVector& BallLocation,
		const FVector& OpponentGoalLocation,
		const FVector& FieldOutwardDirection,
		const FVector& RightDirection,
		const FVector& DesiredLocation
	) const;

	FVector GetCurrentDefensiveThreatLocation(
		ESoccerTeam DefendingTeam
	) const;

	ASoccerAICharacter* FindBestDefensivePressureAI(
		ESoccerTeam DefendingTeam
	) const;

	float ScoreDefensivePressureCandidate(
		ESoccerTeam DefendingTeam,
		const ASoccerAICharacter* Candidate,
		const FVector& ThreatLocation
	) const;

	bool HasDefensiveCoverBehindCandidate(
		const ASoccerAICharacter* Candidate,
		float MinBehindDepthAlphaGap
	) const;

	ASoccerAICharacter* FindBestDefensiveGoalLaneAI(
		ESoccerTeam DefendingTeam,
		const TArray<const ASoccerAICharacter*>& ExcludedCharacters,
		const ASoccerAICharacter* CurrentPressureAI
	) const;

	float ScoreDefensiveGoalLaneCandidate(
		ESoccerTeam DefendingTeam,
		const ASoccerAICharacter* Candidate,
		const FVector& ThreatLocation,
		const ASoccerAICharacter* CurrentPressureAI
	) const;

	ASoccerCharacterBase* FindMostDangerousAttackingReceiver(
		ESoccerTeam DefendingTeam,
		const TArray<const ASoccerCharacterBase*>& ExcludedReceivers
	) const;

	float ScoreDangerousAttackingReceiver(
		ESoccerTeam DefendingTeam,
		const ASoccerCharacterBase* CandidateReceiver
	) const;

	ASoccerAICharacter* FindBestDefensiveMarkerAI(
		ESoccerTeam DefendingTeam,
		const ASoccerCharacterBase* ReceiverToMark,
		const TArray<const ASoccerAICharacter*>& ExcludedCharacters
	) const;

	float ScoreDefensiveMarkerCandidate(
		ESoccerTeam DefendingTeam,
		const ASoccerAICharacter* CandidateMarker,
		const ASoccerCharacterBase* ReceiverToMark
	) const;

	FVector BuildDefendMarkReceiverLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	float ScoreDefensiveStealOpportunity(
		const ASoccerAICharacter* PressureCharacter,
		const ASoccerCharacterBase* InPossessingCharacter,
		const ASoccerBall* InSoccerBall
	) const;

	bool HasDefensiveCoverBehindPressureCharacter(
		const ASoccerAICharacter* PressureCharacter,
		float MinBehindDepthAlphaGap
	) const;

	ASoccerAICharacter* FindBestDefensiveCoverCenterAI(
		ESoccerTeam DefendingTeam,
		const TArray<const ASoccerAICharacter*>& ExcludedCharacters
	) const;

	float ScoreDefensiveCoverCenterCandidate(
		ESoccerTeam DefendingTeam,
		const ASoccerAICharacter* Candidate
	) const;

	//offside

	bool ShouldApplyOffsideSafetyToAttackOrder(
		ESoccerAIOrder CurrentOrder
	) const;

	bool TryGetAttackFieldFrame(
		ESoccerTeam AttackingTeam,
		FVector& OutOwnGoalLocation,
		FVector& OutAttackDirection,
		float& OutFieldLength
	) const;

	bool TryGetOffsideSafeProgressForTeam(
		ESoccerTeam AttackingTeam,
		float& OutSafeProgress,
		FVector& OutOwnGoalLocation,
		FVector& OutAttackDirection
	) const;

	FVector ApplyOffsideSafetyToAttackMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& DesiredLocation,
		ESoccerAIOrder CurrentOrder
	) const;

	void RefreshPendingOffsideSnapshotForTouch(
		ASoccerCharacterBase* TouchingCharacter
	);

	bool TryHandlePendingOffsideTouch(
		ASoccerCharacterBase* TouchingCharacter
	);

	bool IsCharacterInOffsidePositionForCurrentTouch(
		const ASoccerCharacterBase* Candidate,
		ESoccerTeam AttackingTeam,
		float BallProgress,
		float LastFieldDefenderProgress,
		float MidfieldProgress,
		const FVector& OwnGoalLocation,
		const FVector& AttackDirection
	) const;

	bool IsCharacterStillInOffsidePositionAtReception(
		const ASoccerCharacterBase* Candidate,
		ESoccerTeam AttackingTeam
	) const;

	void HandleOffsideOffense(
		ASoccerCharacterBase* OffendingCharacter
	);

	void StartOffsideFreezePresentation(
		ESoccerTeam RestartTeam,
		const FVector& RestartLocation
	);


	void SpawnOffsideFreezeLine(const FVector& RestartLocation);

	void DestroyOffsideFreezeLine();

	void StartDirectFreeKickRestart(
		ESoccerTeam RestartTeam,
		const FVector& RestartLocation
	);






	bool BeginPenaltyFoulDelay(
		ESoccerTeam RestartTeam,
		const FVector& IncidentLocation
	);

	bool StartPenaltyKickRestart(
		ESoccerTeam RestartTeam,
		const FVector& IncidentLocation
	);

	void CancelPenaltyKickRestart();
	FVector BuildPenaltyKickMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;
	AThirdPersonCppCharacter* FindHumanCharacterForTeam(ESoccerTeam Team) const;

	ESoccerAIOrder GetDefaultRestartAttackOrderForCharacter(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	FVector BuildOffsideRestartOpponentMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	// ============================================================
	// THROW IN
	// ============================================================

	void UpdateBallBoundaryTrackingAndDetectOutOfPlay();

	bool TryFindFirstPitchBoundaryCrossing(
		const FVector& PreviousLocation,
		const FVector& CurrentLocation,
		float BallRadius,
		bool& bOutTouchline,
		float& OutBoundarySign,
		FVector& OutCrossingLocation
	) const;

	bool IsPotentialGoalCrossing(
		const FVector& PreviousLocation,
		const FVector& CurrentLocation,
		float GoalLineSign
	) const;

	ESoccerTeam GetDefendingTeamForGoalLine(float GoalLineSign) const;

	bool IsBallOutOfPlayDelayActive() const;

	void BeginBallOutOfPlayDelay(
		ESoccerRestartType RestartType,
		ESoccerTeam RestartTeam,
		const FVector& RestartReferenceLocation,
		const FVector& RequestedThrowInInwardDirection,
		float GoalLineSign
	);

	void CompleteBallOutOfPlayDelay();

	void CancelBallOutOfPlayDelay();

	void StartThrowIn(
		ESoccerTeam RestartTeam,
		const FVector& TouchlineLocation,
		const FVector& InwardDirection
	);

	bool ConfigureThrowInRestart(
		ESoccerTeam RestartTeam,
		const FVector& TouchlineLocation,
		const FVector& InwardDirection
	);

	bool UpdateHumanThrowInTakerClaimDuringPreparation();
	void ResetHumanThrowInTakerRuntime();
	bool PrepareHumanThrowInDirectionAndStartLocation(
		const FVector& RequestedTargetLocation
	);
	bool CompleteHumanThrowInRelease();

	bool StageThrowInDuringBallOutOfPlayDelay(
		ESoccerTeam RestartTeam,
		const FVector& TouchlineLocation,
		const FVector& InwardDirection
	);







	void UpdateThrowInReturnToField(float DeltaTime);


	void FinishThrowInExecution();

	void CancelThrowInRestart();

	ASoccerAICharacter* FindBestThrowInTakerForTeam(
		ESoccerTeam Team,
		const FVector& TouchlineLocation
	) const;

	ASoccerAICharacter* FindBestThrowInReceiverForTeam(
		ESoccerTeam Team,
		const ASoccerAICharacter* Thrower,
		const FVector& TouchlineLocation
	) const;

	FVector BuildThrowInReceiverMoveLocation() const;

	FVector BuildThrowInOpponentMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	void RecalculateThrowInGeometry();

	bool EvaluateThrowInLocalDisplacementNormalized(
		float NormalizedTime,
		FVector2D& OutLocalDisplacement
	) const;

	bool ApplyThrowInCurveMotionNormalized(float NormalizedTime);

	// ============================================================
	// GOAL LINE RESTARTS: GOAL KICK / CORNER KICK
	// ============================================================















	void UpdateCornerKickReturnToField(float DeltaTime);

	void CompleteGoalLineRestart();

	void CancelGoalLineRestart();

	bool IsGoalLineRestartDelayPositioningActive() const;

	bool ConfigureGoalLineRestart(
		ESoccerGoalLineRestartType RestartType,
		ESoccerTeam RestartTeam,
		const FVector& CrossingLocation,
		float GoalLineSign
	);

	bool StageGoalLineRestartDuringBallOutOfPlayDelay(
		ESoccerRestartType RestartType,
		ESoccerTeam RestartTeam,
		const FVector& CrossingLocation,
		float GoalLineSign
	);

	void RecalculateGoalLineRestartGeometry();

	ASoccerAICharacter* FindBestGoalLineRestartTakerForTeam(
		ESoccerTeam Team,
		ESoccerGoalLineRestartType RestartType,
		const FVector& RestartLocation
	) const;

	ASoccerAICharacter* FindBestGoalLineRestartReceiverForTeam(
		ESoccerTeam Team,
		ESoccerGoalLineRestartType RestartType,
		const ASoccerAICharacter* Taker,
		const FVector& RestartLocation
	) const;

	FVector BuildGoalKickBallLocation(
		const FVector& CrossingLocation,
		float GoalLineSign
	) const;

	FVector BuildCornerKickBallLocation(
		const FVector& CrossingLocation,
		float GoalLineSign
	) const;

	FVector BuildGoalLineRestartReceiverMoveLocation() const;

	FVector BuildGoalLineRestartOpponentMoveLocation(
		const ASoccerAICharacter* SoccerAICharacter
	) const;


	UPROPERTY()
		ASoccerCharacterBase* NoRetouchRestrictedCharacter = nullptr;

	bool bNoRetouchRestrictionActive = false;



    // ============================================================
    // FOULS - STAGE 5: TACKLE REFEREE EVALUATION
    // ============================================================

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Tackle")
    bool bEnableTackleFoulEvaluation = true;

    /* Ball-first needs at least this normalized lead to receive the clean-contact benefit. */
    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Tackle", meta = (ClampMin = "0.0", ClampMax = "0.30"))
    float TackleFoulCleanBallFirstLeadNormalized = 0.035f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Tackle", meta = (ClampMin = "0.0"))
    float TackleFoulCarelessRelativeSpeed = 420.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Tackle", meta = (ClampMin = "0.0"))
    float TackleFoulRecklessRelativeSpeed = 650.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Tackle", meta = (ClampMin = "0.0"))
    float TackleFoulExcessiveRelativeSpeed = 900.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Tackle", meta = (ClampMin = "0.0"))
    float TackleFoulHighContactHeight = 30.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Tackle", meta = (ClampMin = "0.0"))
    float TackleFoulVeryHighContactHeight = 45.0f;

    /* Ball-first can remain legal only below this speed unless other danger signals are absent. */
    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Tackle", meta = (ClampMin = "0.0"))
    float TackleFoulBallFirstLegalMaxRelativeSpeed = 720.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Restart")
    bool bEnableFoulMatchStoppage = true;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Restart")
    bool bEnableDirectFreeKickRestartFromFouls = true;

    // True today: PlayerTeam penalties are taken by the human. Setting this
    // false already enables the future coach-selected AI taker path.
    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty")
    bool bPlayerTeamPenaltyUsesHuman = true;

    // Presentation window after a live-play foul is classified as a penalty.
    // The tackle/fall/ball physics are allowed to finish before the penalty
    // configuration state repositions the ball and the players.
    UPROPERTY(
        EditAnywhere,
        Category = "Soccer|Fouls|Penalty",
        meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "6.0")
    )
    float PenaltyFoulContinuationDuration = 3.5f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty", meta = (ClampMin = "0.0"))
    float PenaltyKickMinimumSetupTime = 0.65f;

    // Safety valve for the setup phase. A penalty must not remain blocked forever
    // because one non-critical bot is a few centimeters outside the generic
    // restart acceptance radius or still has residual movement.
    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty", meta = (ClampMin = "1.0", UIMin = "1.0"))
    float PenaltyKickMaximumSetupWaitTime = 4.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty", meta = (ClampMin = "50.0"))
    float PenaltyKickRunUpDistance = 280.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty", meta = (ClampMin = "10.0"))
    float PenaltyKickRunUpMoveAcceptanceRadius = 55.0f;

    // The defending goalkeeper is a critical participant in a penalty. Unlike
    // generic restart bots, it must be very close to the geometric center of
    // the goal before the kick may be enabled.
    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty|Goalkeeper", meta = (ClampMin = "5.0", UIMin = "5.0", UIMax = "100.0"))
    float PenaltyKickGoalkeeperCenterAcceptanceRadius = 35.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty|Goalkeeper", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0"))
    float PenaltyKickGoalkeeperReadyMaximumSpeed = 20.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty", meta = (ClampMin = "20.0"))
    float PenaltyKickBallContactDistance = 115.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty", meta = (ClampMin = "100.0"))
    float PenaltyKickOtherPlayersExtraDepth = 240.0f;

    // Separation between the defending and attacking outfield lines during a penalty.
    // Both lines remain outside the penalty area/arc, but they no longer share one target band.
    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty|Positioning", meta = (ClampMin = "50.0"))
    float PenaltyKickTeamLineSeparation = 260.0f;

    // Horizontal distance between adjacent dynamically calculated penalty slots.
    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty|Positioning", meta = (ClampMin = "100.0"))
    float PenaltyKickSlotLateralSpacing = 390.0f;

    // The goalkeeper of the team taking the penalty stays near its own goal instead of
    // joining the outfield players around the opposite penalty area.
    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty|Positioning", meta = (ClampMin = "0.0"))
    float PenaltyKickAttackingGoalkeeperForwardOffset = 180.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty", meta = (ClampMin = "100.0"))
    float PenaltyKickAIShotHorizontalSpeed = 2050.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Fouls|Penalty", meta = (ClampMin = "0.0"))
    float PenaltyKickAITargetSideMargin = 75.0f;

	// Pending data owned only by the presentation delay between a live foul
	// and the actual penalty configuration state.
	ESoccerTeam PendingPenaltyFoulRestartTeam = ESoccerTeam::PlayerTeam;
	FVector PendingPenaltyFoulIncidentLocation = FVector::ZeroVector;

	// Penalty-specific runtime state lives in the extracted restart module.
	FSoccerPenaltyKickRestart PenaltyKickRestart;
	FSoccerFreeKickRestart FreeKickRestart;
	FSoccerGoalLineRestart GoalLineRestart;
	bool bGoalLineRestartStagedDuringBallOutOfPlayDelay = false;

	// ============================================================
	// FORMATION - structural preset + stable assignment + open-play structure (Stage 3)
	// ============================================================

	// Stage 7 (Director Technical integration): the user's saved formation,
	// tactics and lineup become the authoritative PlayerTeam setup at match start.
	UPROPERTY(EditAnywhere, Category = "Soccer|Director Technical|Match Integration")
	bool bUsePersistentDirectorTechnicalSetupForPlayerTeam = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation", meta = (AllowPrivateAccess = "true"))
	ESoccerFormationSystem PlayerTeamFormationSystem =
		ESoccerFormationSystem::OneThreeTwoOne;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation", meta = (AllowPrivateAccess = "true"))
	ESoccerFormationSystem OpponentTeamFormationSystem =
		ESoccerFormationSystem::OneThreeTwoOne;

	// Stage 5 created the plans; Stage 9 makes the existing open-play systems
	// consume them without changing restart positioning or emergency rules.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics", meta = (AllowPrivateAccess = "true"))
	FSoccerTeamTacticalPlan PlayerTeamTacticalPlan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics", meta = (AllowPrivateAccess = "true"))
	FSoccerTeamTacticalPlan OpponentTeamTacticalPlan;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime")
	bool bUseCollectiveTacticsInOpenPlay = true;

	// Absolute block shift. Low subtracts this alpha; High adds it. Medium is 0.
	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float CollectiveDefensiveBlockDepthShiftAlpha = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "0.5", ClampMax = "8.0"))
	float CollectiveTransitionDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "0.0", ClampMax = "0.20"))
	float CollectiveRegroupExtraDepthShiftAlpha = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "0.0", ClampMax = "0.20"))
	float CollectiveCounterPressExtraDepthShiftAlpha = 0.04f;

	// Maximum distance from the coordinated pressure player to the active threat.
	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "100.0"))
	float CollectiveLowPressMaxEngagementDistance = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "100.0"))
	float CollectiveMediumPressMaxEngagementDistance = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "100.0"))
	float CollectiveHighPressMaxEngagementDistance = 6000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "100.0"))
	float CollectiveRegroupPressMaxEngagementDistance = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollectiveLowPressEmergencyThreatDepthAlpha = 0.42f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "0.4", ClampMax = "1.0"))
	float CollectiveNarrowAttackWidthScale = 0.78f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "1.0", ClampMax = "1.8"))
	float CollectiveWideAttackWidthScale = 1.20f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "0.0"))
	float CollectiveAttackChannelShapeShift = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "0.0"))
	float CollectiveAttackChannelCarryLateralOffset = 850.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "200.0"))
	float CollectiveAttackChannelCarryLookAheadDistance = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "0.0"))
	float CollectivePassStyleScoreStrength = 190.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Collective|Runtime", meta = (ClampMin = "0.0"))
	float CollectiveTransitionPassScoreStrength = 230.0f;

	// Transition state is runtime-only and is updated only when controlled
	// possession actually changes team; a same-team pass through a free ball
	// does not falsely start a new transition.
	bool bHasCollectiveLastControlledPossessionTeam = false;
	ESoccerTeam CollectiveLastControlledPossessionTeam = ESoccerTeam::PlayerTeam;
	float PlayerTeamLastCollectivePossessionGainTime = -1000.0f;
	float OpponentTeamLastCollectivePossessionGainTime = -1000.0f;
	float PlayerTeamLastCollectivePossessionLossTime = -1000.0f;
	float OpponentTeamLastCollectivePossessionLossTime = -1000.0f;

	// Stage 6: instructions belong to formation slots, not permanently to a
	// character. Matching slot IDs are intentionally remembered across formation
	// changes so switching back to a previous shape restores the coach choices.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Individual", meta = (AllowPrivateAccess = "true"))
	TArray<FSoccerSlotTacticalInstruction> PlayerTeamSlotTacticalInstructions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Individual", meta = (AllowPrivateAccess = "true"))
	TArray<FSoccerSlotTacticalInstruction> OpponentTeamSlotTacticalInstructions;

	// Stage 7: individual instructions influence the existing open-play roles.
	// They never replace restart logic, possession decisions, emergency coverage
	// or the single-player pressure/cover coordination owned by the team system.
	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Individual|Runtime")
	bool bUseIndividualTacticalInstructionsInOpenPlay = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Individual|Runtime", meta = (ClampMin = "0.0"))
	float IndividualPreferredRoleScoreBonus = 2200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Individual|Runtime", meta = (ClampMin = "0.0"))
	float IndividualSecondaryRoleScoreBonus = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Individual|Runtime", meta = (ClampMin = "0.0"))
	float IndividualAvoidRoleScorePenalty = 1600.0f;

	// Stage 8: explicit marking instructions are one-to-one team assignments.
	// The pressure player, goal-lane protector and central cover player are
	// selected first and can temporarily override a requested man-marking duty.
	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Individual|Marking")
	bool bUseExplicitIndividualMarking = true;

	// Prevents two same-target candidates from alternating every tactical update.
	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Individual|Marking", meta = (ClampMin = "0.0"))
	float IndividualMarkingCurrentAssignmentBonus = 650.0f;

	// A requested marker may leave its structural slot, but not chase a rival
	// indefinitely across the whole pitch. Zero disables this structural leash.
	UPROPERTY(EditAnywhere, Category = "Soccer|Tactics|Individual|Marking", meta = (ClampMin = "0.0"))
	float IndividualMarkingMaxFollowDistanceFromStructure = 1600.0f;

	// Stage 4: the human can choose the structural system before kickoff and
	// reopen the same screen during the match with M.
	UPROPERTY(EditAnywhere, Category = "Soccer|Formation|Menu")
	bool bShowFormationMenuAtMatchStart = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Formation|Menu")
	bool bPauseGameWhileFormationMenuOpen = true;

	/* Runtime-only, non-owning mapping. Stage 3 uses it as the open-play structural anchor. */
	TMap<ASoccerCharacterBase*, FName> PlayerTeamFormationAssignments;
	TMap<ASoccerCharacterBase*, FName> OpponentTeamFormationAssignments;

	// Stage 8 runtime-only one-to-one explicit man-marking assignments.
	// World ownership keeps the actors alive; every read is still guarded by IsValid.
	TMap<ASoccerAICharacter*, ASoccerCharacterBase*>
		PlayerTeamExplicitIndividualMarkingAssignments;
	TMap<ASoccerAICharacter*, ASoccerCharacterBase*>
		OpponentTeamExplicitIndividualMarkingAssignments;

	// Stage 9 collective man-to-man assignments are kept separate from Stage 8
	// explicit slot instructions so Debug Individual Marking can remain focused
	// only on the user's "Mark Tightly" requests.
	TMap<ASoccerAICharacter*, ASoccerCharacterBase*>
		PlayerTeamCollectiveManMarkingAssignments;
	TMap<ASoccerAICharacter*, ASoccerCharacterBase*>
		OpponentTeamCollectiveManMarkingAssignments;

	// Stage 3: formation slots become structural anchors only during open play.
	// Restarts keep their dedicated positioning logic and special tactical orders
	// (press, marking, goal-lane protection, etc.) remain authoritative.
	UPROPERTY(EditAnywhere, Category = "Soccer|Formation|Runtime Structure")
	bool bUseFormationForOpenPlayStructure = true;

	// Formation lateral identity should remain visible while the whole block can
	// still slide toward the ball side. The legacy HomePositionActor path keeps
	// using DynamicShapeHomeLateralKeepAlpha when formation is disabled.
	UPROPERTY(EditAnywhere, Category = "Soccer|Formation|Runtime Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FormationDynamicShapeLateralKeepAlpha = 0.90f;

	// Existing attacking orders remain dynamic. This value only pulls their
	// destination back toward the assigned formation structure.
	UPROPERTY(EditAnywhere, Category = "Soccer|Formation|Runtime Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FormationAttackShapeInfluence = 0.50f;

	// Rest-defense is the order that most directly protects the base structure,
	// so it intentionally follows the formation more strongly.
	UPROPERTY(EditAnywhere, Category = "Soccer|Formation|Runtime Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FormationAttackRestDefenseInfluence = 0.80f;

	// Live strategy changes do not teleport the structural target from the old
	// shape to the new one. Instead, each field bot receives a moving structural
	// reference that eases from the pre-change target toward the new tactic.
	// Emergency/ball-pressure orders remain authoritative and are not locked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Live Transition", meta = (AllowPrivateAccess = "true"))
	bool bUseLiveTacticalShapeTransition = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Live Transition", meta = (AllowPrivateAccess = "true", ClampMin = "0.10", ClampMax = "10.0"))
	float LiveTacticalShapeTransitionDuration = 3.50f;

	// Runtime-only snapshots. They are deliberately non-owning: world ownership
	// keeps the AI characters alive and every read is guarded with IsValid.
	bool bPlayerTeamLiveTacticalShapeTransitionActive = false;
	bool bOpponentTeamLiveTacticalShapeTransitionActive = false;
	float PlayerTeamLiveTacticalShapeTransitionStartTime = -1000.0f;
	float OpponentTeamLiveTacticalShapeTransitionStartTime = -1000.0f;
	TMap<const ASoccerAICharacter*, FVector> PlayerTeamLiveTacticalShapeStartLocations;
	TMap<const ASoccerAICharacter*, FVector> OpponentTeamLiveTacticalShapeStartLocations;

	UPROPERTY(EditAnywhere, Category = "Soccer|Formation|Debug", meta = (ClampMin = "1.0"))
	float FormationDebugSphereRadius = 48.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Formation|Debug", meta = (ClampMin = "0.0"))
	float FormationDebugHeight = 24.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Match")
		float MatchStateUpdateInterval = 0.15f;

	// Any uncontrolled/opponent-controlled ball this deep toward a team's own
	// goal activates faster defensive reaction. If the ball is specifically
	// loose, it also overrides last-touch tactical phase so a goalkeeper rebound
	// cannot make his own team behave as the attacking side.
	UPROPERTY(EditAnywhere, Category = "Soccer|Defense|Emergency Reaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float DefensiveEmergencyReactionDepthAlpha = 0.42f;

	// Normal tactical role assignment runs at MatchStateUpdateInterval. Close to
	// goal we temporarily use a faster cadence so rebounds and second balls are
	// reacted to without making the whole match-state loop more expensive.
	UPROPERTY(EditAnywhere, Category = "Soccer|Defense|Emergency Reaction", meta = (ClampMin = "0.01"))
		float DefensiveEmergencyReactionUpdateInterval = 0.05f;

	// Todas las reanudaciones esperan que cada bot alcance su destino y
	// reduzca la velocidad antes de autorizar al ejecutor.
	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Preparation", meta = (ClampMin = "1.0"))
		float RestartDefaultBotAcceptanceRadius = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Preparation", meta = (ClampMin = "0.0"))
		float RestartReadyMaximumBotSpeed = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Preparation", meta = (ClampMin = "0.0"))
		float RestartReadyHoldTime = 0.35f;

	// Si algun bot queda bloqueado, se recalculan los destinos. Esto no
	// autoriza el saque: todos igualmente deben llegar y quedar estables.
	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Preparation", meta = (ClampMin = "0.5"))
		float RestartPositioningRecoveryInterval = 3.0f;

	// Luego de este tiempo, un bot que sigue en una zona ilegal recibe
	// locomocion asistida. El saque continua bloqueado hasta que salga.
	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Bot Recovery", meta = (ClampMin = "0.0"))
		float RestartIllegalBotEmergencyDelay = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Bot Recovery", meta = (ClampMin = "1.0"))
		float RestartIllegalBotEmergencySpeed = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Bot Recovery", meta = (ClampMin = "5.0", ClampMax = "80.0"))
		float RestartRestrictionEscapeAngleStepDegrees = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Bot Recovery", meta = (ClampMin = "50.0"))
		float RestartRestrictionEscapeBodyClearance = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Bot Recovery", meta = (ClampMin = "0.0"))
		float RestartRestrictionFieldInset = 90.0f;

	// Free-kick opponents may improve their position once after completing a
	// mandatory escape, but the new NavMesh route must remain outside the
	// protected circle. These values control that optional second movement.
	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Opponent Positioning")
		bool bAllowFreeKickOpponentLegalReposition = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Opponent Positioning", meta = (ClampMin = "0.0"))
		float FreeKickOpponentLegalRepositionMinDistance = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Opponent Positioning", meta = (ClampMin = "1.0"))
		float FreeKickOpponentEscapeArrivalDistance = 85.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Opponent Positioning", meta = (ClampMin = "0.0"))
		float FreeKickOpponentPathSafetyMargin = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall")
		bool bEnableFreeKickDefensiveWall = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "100.0", UIMin = "100.0"))
		float FreeKickWallMaximumGoalDistance = 3200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "100.0", UIMin = "100.0"))
		float FreeKickWallMinimumGoalDistanceForMaximumPlayers = 1200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "0.0", ClampMax = "89.0", UIMin = "0.0", UIMax = "89.0"))
		float FreeKickWallMaximumGoalAngleDegrees = 58.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "1", ClampMax = "6", UIMin = "1", UIMax = "6"))
		int32 FreeKickWallMinimumPlayers = 1;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "1", ClampMax = "6", UIMin = "1", UIMax = "6"))
		int32 FreeKickWallMaximumPlayers = 4;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "0", ClampMax = "5", UIMin = "0", UIMax = "5"))
		int32 FreeKickWallMinimumNonWallOutfieldPlayers = 2;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "40.0", UIMin = "40.0"))
		float FreeKickWallPlayerSpacing = 85.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "0.0", UIMin = "0.0"))
		float FreeKickWallMinimumBodyGap = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "1.0", ClampMax = "50.0", UIMin = "1.0", UIMax = "50.0"))
		float FreeKickWallMoveAcceptanceRadius = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "1.0", ClampMax = "80.0", UIMin = "1.0", UIMax = "80.0"))
		float FreeKickWallReadyAcceptanceRadius = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "0.0", UIMin = "0.0"))
		float FreeKickWallExtraDistanceFromBall = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
		float FreeKickWallProtectedGoalLateralAlpha = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Defensive Wall", meta = (ClampMin = "0.0", UIMin = "0.0"))
		float FreeKickWallGoalkeeperOppositeSideOffset = 95.0f;

	// A teammate human inside this radius claims a free kick during Preparation.
	// The AI chosen in Configuration remains available as the fallback taker.
	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Human Taker", meta = (ClampMin = "50.0"))
		float FreeKickHumanTakerClaimRadius = 300.0f;

	// Hysteresis: after claiming the restart, the human must move beyond this
	// larger radius before the fallback AI retakes responsibility.
	UPROPERTY(EditAnywhere, Category = "Soccer|Free Kick|Human Taker", meta = (ClampMin = "50.0"))
		float FreeKickHumanTakerReleaseRadius = 380.0f;

	// Kickoff, goal kick and corner share the same claim/release behavior. The
	// free-kick values above remain separate so Stage 2 tuning is preserved.
	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Human Taker", meta = (ClampMin = "50.0"))
		float RestartHumanTakerClaimRadius = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Human Taker", meta = (ClampMin = "50.0"))
		float RestartHumanTakerReleaseRadius = 380.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Human Indicator")
		bool bEnableThrowInHumanRestrictionIndicator = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Human Indicator")
		bool bEnableCornerKickHumanRestrictionIndicator = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Human Indicator")
		bool bEnableGoalKickHumanPenaltyAreaIndicator = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Human Indicator", meta = (ClampMin = "1.0"))
		float RestartHumanRestrictionIndicatorThickness = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Human Indicator")
		float RestartHumanRestrictionIndicatorCenterZ = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Human Indicator")
		FLinearColor RestartHumanRestrictionIndicatorColor =
			FLinearColor(0.62f, 0.56f, 0.18f, 0.34f);

	UPROPERTY(EditAnywhere, Category = "Soccer|Restart Restriction|Human Indicator")
		TSubclassOf<ASoccerRestartRadiusActor>
			RestartHumanRestrictionIndicatorActorClass;

	// ============================================================
	// RESTART RECEIVER SELECTION - STAGE 3
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Receiver Selection")
		bool bEnableRestartHumanReceiverSelection = true;

	// A good human option is not forced every time. This probability is rolled
	// once when the bot commits to the restart execution.
	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Receiver Selection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float RestartHumanReceiverSelectionChance = 0.40f;

	// Small preference that lets the human win close comparisons without
	// overriding a clearly safer/better planned AI receiver.
	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Receiver Selection", meta = (ClampMin = "0.0"))
		float RestartHumanReceiverPreferenceBonus = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Receiver Selection", meta = (ClampMin = "0.0"))
		float RestartReceiverMinimumPassDistance = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Receiver Selection", meta = (ClampMin = "1.0"))
		float RestartReceiverMaximumPassDistance = 4200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Receiver Selection", meta = (ClampMin = "0.0"))
		float RestartReceiverCriticalOpponentRadius = 145.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Receiver Selection", meta = (ClampMin = "0.0"))
		float RestartReceiverOpponentPressureRadius = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Receiver Selection", meta = (ClampMin = "0.0"))
		float RestartReceiverGroundLaneHalfWidth = 245.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Receiver Selection", meta = (ClampMin = "0.0"))
		float RestartReceiverAerialLaneHalfWidth = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Restarts|Receiver Selection")
		float RestartReceiverMinimumCandidateScore = 420.0f;

	bool bRestartContextActive = false;
	ESoccerRestartType ActiveRestartType = ESoccerRestartType::None;
	ESoccerTeam ActiveRestartTeam = ESoccerTeam::PlayerTeam;
	FVector ActiveRestartLocation = FVector::ZeroVector;
	float ActiveRestartAllBotsReadySince = -1000.0f;
	float ActiveRestartLastPositioningRecoveryTime = -1000.0f;
	bool bActiveRestartPreviousLegalConditionsSatisfied = false;
	bool bCapturingActiveRestartAITargetLocations = false;
	TMap<const ASoccerAICharacter*, FVector> ActiveRestartAITargetLocations;
	TMap<ASoccerAICharacter*, float> ActiveRestartIllegalBotSinceTimes;
	TMap<ASoccerAICharacter*, FVector>
		ActiveRestartEmergencyTargetLocations;
	TSet<ASoccerAICharacter*> ActiveRestartEmergencyAssistedBots;

	// Shared human-taker runtime for kickoff, goal kick and corner. The bot
	// selected during Configuration remains the fallback and is never destroyed.
	UPROPERTY()
		AThirdPersonCppCharacter* ActiveNonFreeKickHumanTaker = nullptr;

	ESoccerRestartType ActiveNonFreeKickHumanTakerType = ESoccerRestartType::None;
	bool bActiveNonFreeKickHumanTakerClaimed = false;
	bool bActiveNonFreeKickHumanExecutionAuthorized = false;

	// Real receiver chosen for the execution only. ReceiverAI fields elsewhere
	// remain untouched so bot preparation/positioning keeps its existing plan.
	UPROPERTY()
		ASoccerCharacterBase* ActiveRestartExecutionReceiver = nullptr;

	bool bActiveRestartExecutionReceiverIsHuman = false;

	UPROPERTY()
		ASoccerRestartRadiusActor* ActiveRestartHumanRestrictionActor = nullptr;

	UPROPERTY()
		ASoccerCharacterBase* LastTouchCharacter = nullptr;

	UPROPERTY()
		FVector LastTouchLocation = FVector::ZeroVector;

	UPROPERTY()
		bool bHasLastTouchTeam = false;

	ESoccerTeam LastTouchTeam = ESoccerTeam::PlayerTeam;

	// Intencion temporal del pase de juego abierto. No representa posesion:
	// solo evita que la IA trate inmediatamente el pase como una pelota
	// neutral y vuelva a mandar al pasador a perseguirla.
	bool bOpenPlayPassIntentActive = false;

	UPROPERTY()
		ASoccerCharacterBase* OpenPlayPasser = nullptr;

	UPROPERTY()
		ASoccerCharacterBase* OpenPlayIntendedReceiver = nullptr;

	float OpenPlayPassIntentStartTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Pass Intent", meta = (ClampMin = "0.1"))
		float OpenPlayPassIntentMaxLifetime = 2.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Pass Intent", meta = (ClampMin = "0.0"))
		float OpenPlayPassIntentSlowBallGraceTime = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Pass Intent", meta = (ClampMin = "0.0"))
		float OpenPlayPassIntentMinimumBallSpeed = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Pass Intent", meta = (ClampMin = "0.0"))
		float OpenPlayPassIntentAbandonedDistance = 280.0f;


	// ============================================================
	// HUMAN PASS REQUESTS
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request")
		bool bEnableHumanPassRequests = true;

	// Minimum interval between repeated safety evaluations for the same active request.
	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request", meta = (DisplayName = "Human Pass Request Evaluation Cooldown", ClampMin = "0.0"))
		float HumanPassRequestCooldown = 0.20f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request", meta = (ClampMin = "0.0"))
		float HumanPassRequestAutoCancelTime = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Distance", meta = (ClampMin = "0.0"))
		float HumanPassRequestMinDistance = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Distance", meta = (ClampMin = "0.0"))
		float HumanPassRequestMaxDistance = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Safety", meta = (ClampMin = "0.0"))
		float HumanPassRequestPasserPressureRadius = 185.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Safety", meta = (ClampMin = "0.0"))
		float HumanPassRequestCriticalReceiverPressureRadius = 135.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Safety", meta = (ClampMin = "0.0"))
		float HumanPassRequestReceiverPressureRadius = 320.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Safety", meta = (ClampMin = "0.0"))
		float HumanPassRequestNormalLaneHalfWidth = 245.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Safety", meta = (ClampMin = "0.0"))
		float HumanPassRequestAerialLaneHalfWidth = 125.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Safety", meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float HumanPassRequestNormalMinimumSafetyScore = 0.58f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Safety", meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float HumanPassRequestAerialMinimumSafetyScore = 0.48f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Target", meta = (ClampMin = "0.0"))
		float HumanPassRequestNormalLeadTime = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Target", meta = (ClampMin = "0.0"))
		float HumanPassRequestAerialLeadTime = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Target", meta = (ClampMin = "0.0"))
		float HumanPassRequestMaximumLeadDistance = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Target", meta = (ClampMin = "100.0", ClampMax = "260.0"))
		float HumanPassRequestAerialArrivalHeight = 205.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Target", meta = (ClampMin = "0.0"))
		float HumanPassRequestFieldInset = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Normal", meta = (ClampMin = "100.0"))
		float HumanPassRequestNormalHorizontalSpeed = 1650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Normal", meta = (ClampMin = "0.05"))
		float HumanPassRequestNormalMinTravelTime = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Normal", meta = (ClampMin = "0.05"))
		float HumanPassRequestNormalMaxTravelTime = 1.15f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Aerial", meta = (ClampMin = "100.0"))
		float HumanPassRequestAerialHorizontalSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Aerial", meta = (ClampMin = "0.05"))
		float HumanPassRequestAerialMinTravelTime = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|Aerial", meta = (ClampMin = "0.05"))
		float HumanPassRequestAerialMaxTravelTime = 1.65f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|HUD")
		bool bShowHumanPassRequestHUD = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Pass Request|HUD", meta = (ClampMin = "0.1"))
		float HumanPassRequestHUDBriefDuration = 1.8f;


	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Soccer|Human Pass Request", meta = (AllowPrivateAccess = "true"))
		ESoccerHumanPassRequestType ActiveHumanPassRequestType =
			ESoccerHumanPassRequestType::None;

	UPROPERTY()
		AThirdPersonCppCharacter* ActiveHumanPassRequestingHuman = nullptr;

	float LastHumanPassRequestInputTime = -1000.0f;
	float LastHumanPassRequestEvaluationTime = -1000.0f;

	UPROPERTY()
		ASoccerAICharacter* LastHumanPassRequestEvaluatedPasser = nullptr;

	FString HumanPassRequestHUDBriefText;
	FLinearColor HumanPassRequestHUDBriefColor = FLinearColor::White;
	float HumanPassRequestHUDBriefExpireTime = -1000.0f;

	UPROPERTY()
		ASoccerBall* SoccerBall = nullptr;

	// ============================================================
	// INSTANT REPLAY - STAGE 1: CONTINUOUS RECORDER
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Recording")
		bool bEnableInstantReplayRecording = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Recording", meta = (ClampMin = "2.0", UIMin = "2.0"))
		float InstantReplayHistorySeconds = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Recording", meta = (ClampMin = "5.0", ClampMax = "60.0", UIMin = "5.0", UIMax = "60.0"))
		float InstantReplaySamplesPerSecond = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Manual Playback", meta = (ClampMin = "1.0", ClampMax = "10.0", UIMin = "1.0", UIMax = "10.0"))
		float InstantReplayManualPlaybackSeconds = 5.0f;

	// Stage 3: first automatic consumer of the generic instant-replay system.
	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback")
		bool bEnableInstantReplayAfterGoal = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback", meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0", DisplayName = "Goal Seconds Before Event"))
		float InstantReplayGoalPreEventSeconds = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback", meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0", DisplayName = "Goal Seconds After Event"))
		float InstantReplayGoalPostEventSeconds = 1.0f;

	// Stage 4/6: four TV-style goal replay viewpoints. Every take has its
	// own tuning because side/front/behind cameras usually need different
	// framing in a real stadium.
	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Left", meta = (ClampMin = "500.0", UIMin = "500.0"))
		float InstantReplayGoalLeftCameraDistance = 2600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Left", meta = (ClampMin = "0.0", UIMin = "0.0"))
		float InstantReplayGoalLeftCameraInfieldOffset = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Left", meta = (ClampMin = "200.0", UIMin = "200.0"))
		float InstantReplayGoalLeftCameraHeight = 1100.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Left", meta = (ClampMin = "30.0", ClampMax = "120.0", UIMin = "30.0", UIMax = "120.0"))
		float InstantReplayGoalLeftCameraFOV = 78.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Right", meta = (ClampMin = "500.0", UIMin = "500.0"))
		float InstantReplayGoalRightCameraDistance = 2600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Right", meta = (ClampMin = "0.0", UIMin = "0.0"))
		float InstantReplayGoalRightCameraInfieldOffset = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Right", meta = (ClampMin = "200.0", UIMin = "200.0"))
		float InstantReplayGoalRightCameraHeight = 1100.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Right", meta = (ClampMin = "30.0", ClampMax = "120.0", UIMin = "30.0", UIMax = "120.0"))
		float InstantReplayGoalRightCameraFOV = 78.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Front", meta = (ClampMin = "500.0", UIMin = "500.0"))
		float InstantReplayGoalFrontCameraDistance = 3200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Front", meta = (ClampMin = "200.0", UIMin = "200.0"))
		float InstantReplayGoalFrontCameraHeight = 1100.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Front", meta = (ClampMin = "30.0", ClampMax = "120.0", UIMin = "30.0", UIMax = "120.0"))
		float InstantReplayGoalFrontCameraFOV = 78.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Behind", meta = (ClampMin = "500.0", UIMin = "500.0"))
		float InstantReplayGoalBehindCameraDistance = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Behind", meta = (ClampMin = "200.0", UIMin = "200.0"))
		float InstantReplayGoalBehindCameraHeight = 950.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Camera|Behind", meta = (ClampMin = "30.0", ClampMax = "120.0", UIMin = "30.0", UIMax = "120.0"))
		float InstantReplayGoalBehindCameraFOV = 82.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Presentation", meta = (ClampMin = "0.0", ClampMax = "0.5", UIMin = "0.0", UIMax = "0.5", DisplayName = "Camera Fade Seconds"))
		float InstantReplayGoalCameraFadeSeconds = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Instant Replay|Goal Playback|Presentation", meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Camera Collision Padding"))
		float InstantReplayGoalCameraCollisionPadding = 80.0f;

	UPROPERTY()
		ASoccerInstantReplayManager* InstantReplayManager = nullptr;

	bool bOwnsInstantReplayManager = false;

	UPROPERTY()
		ASoccerField* SoccerField = nullptr;

	// Local goal-line ownership inside ASoccerField. Stage 10C initializes
	// these from the actual starting goalkeeper positions so no goal Target
	// Point is required. Stage 10D will swap these signs at halftime.
	float PlayerTeamOwnGoalLineSign = -1.0f;
	float OpponentTeamOwnGoalLineSign = 1.0f;

	// Immutable baseline captured at BeginPlay. Stage 10D changes only the
	// current signs; these retain how static level references were authored.
	float PlayerTeamInitialOwnGoalLineSign = -1.0f;
	float OpponentTeamInitialOwnGoalLineSign = 1.0f;

	// Human characters do not use HomePositionActor. Preserve their authored
	// first-half kickoff references so halftime can place them at the exact
	// 180-degree equivalent after the teams change ends.
	bool bHasPlayerTeamInitialHumanFieldReference = false;
	FVector PlayerTeamInitialHumanFieldReferenceLocation = FVector::ZeroVector;
	bool bHasOpponentTeamInitialHumanFieldReference = false;
	FVector OpponentTeamInitialHumanFieldReferenceLocation = FVector::ZeroVector;

	UPROPERTY()
		ASoccerCharacterBase* PossessingCharacter = nullptr;

	ESoccerPossessionTeam PossessionTeam = ESoccerPossessionTeam::None;

	UPROPERTY(EditAnywhere, Category = "Soccer|Loose Ball|Defensive Contact", meta = (ClampMin = "0.0", ClampMax = "0.5"))
		float IntentionalLooseBallClaimDelay = 0.12f;

	float IntentionalLooseBallClaimUnlockTime = -1000.0f;

	UPROPERTY()
		ASoccerAICharacter* PlayerTeamPressureAI = nullptr;

	UPROPERTY()
		ASoccerAICharacter* OpponentTeamPressureAI = nullptr;

	UPROPERTY()
		ASoccerAICharacter* LastPlayerTeamFreeBallChaser = nullptr;

	UPROPERTY()
		ASoccerAICharacter* LastOpponentTeamFreeBallChaser = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Role Selection", meta = (ClampMin = "0.0"))
		float FreeBallChaserSwitchRequiredTimeAdvantage = 0.18f;

	// Once the current free-ball chaser is already this close, do not hand the
	// chase to a slightly better teammate. The commitment releases naturally if
	// the ball moves away and the chaser leaves this radius.
	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Role Selection", meta = (ClampMin = "0.0"))
		float FreeBallChaserCommitDistance = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Role Selection", meta = (ClampMin = "0.0"))
		float FreeBallUnreachableCandidatePenalty = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Interception|Role Selection", meta = (ClampMin = "0.0"))
		float FreeBallNoPredictionCandidatePenalty = 8.0f;

	// A click-driven human recovery participates in the same predictive timing
	// comparison as the AI. A teammate keeps chasing only when it reaches the
	// ball by at least the configured advantage.
	UPROPERTY(EditAnywhere, Category = "Soccer|Human Ball Claim")
		bool bEnableHumanBallClaimPriority = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Human Ball Claim", meta = (ClampMin = "0.0"))
		float HumanBallClaimAIRequiredTimeAdvantage = 0.20f;

	// Near the team's own goal, defensive safety remains stronger than the
	// anti-crowding preference and a bot may help even if the human arrives first.
	UPROPERTY(EditAnywhere, Category = "Soccer|Human Ball Claim")
		bool bAllowAIHumanClaimDefensiveEmergencyHelp = true;

	UPROPERTY()
		ASoccerAICharacter* PlayerTeamSupportAI = nullptr;

	UPROPERTY()
		ASoccerAICharacter* OpponentTeamSupportAI = nullptr;

	UPROPERTY()
		ASoccerAICharacter* PlayerTeamCoverAI = nullptr;

	UPROPERTY()
		ASoccerAICharacter* OpponentTeamCoverAI = nullptr;

	UPROPERTY()
		ASoccerAICharacter* PlayerTeamDefensiveMarkerAI = nullptr;

	UPROPERTY()
		ASoccerAICharacter* OpponentTeamDefensiveMarkerAI = nullptr;

	UPROPERTY()
		ASoccerCharacterBase* PlayerTeamDefensiveMarkedReceiver = nullptr;

	UPROPERTY()
		ASoccerCharacterBase* OpponentTeamDefensiveMarkedReceiver = nullptr;


	float MatchStateUpdateAccumulator = 0.0f;

	// Profundidad dinamica del arquero durante el posicionamiento normal.
	// El HomePositionActor sigue siendo la profundidad minima/de reposo.
	// Al alejarse la pelota, el arquero puede adelantarse progresivamente;
	// el factor angular reduce ese adelantamiento cuando la pelota esta abierta.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning")
		bool bUseGoalkeeperDynamicDepth = true;

	// Distancia maxima del centro del arquero a su propia linea de gol.
	// 820 cm lo deja apenas antes del punto penal actual (900 cm).
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1300.0"))
		float GoalkeeperDynamicMaximumDepthFromGoal = 820.0f;

	// Profundidad de pelota desde la linea de gol a partir de la cual el
	// factor de distancia llega a 1.0. Con 3000 cm alcanza el maximo en
	// mitad de cancha y lo conserva si la pelota esta aun mas lejos.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning", meta = (ClampMin = "1.0", UIMin = "500.0", UIMax = "6000.0"))
		float GoalkeeperDynamicDepthFullBallDistance = 3000.0f;

	// Hard cap for lateral positioning. The geometric post margin below is
	// also enforced, so a larger Blueprint value can never put the goalkeeper
	// almost on top of a post.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning", meta = (ClampMin = "0.0"))
		float GoalkeeperLateralMoveRange = 230.0f;

	// Blend between the exact center of the goal (0) and the geometric
	// ball-to-post angle-bisector solution (1). Kept with its existing name
	// so current Blueprint overrides remain compatible.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
		float GoalkeeperBallFollowAlpha = 0.8f;

	// Minimum lateral clearance between the goalkeeper center target and a
	// goal post. With a 300 cm half-goal and the default 70 cm margin, the
	// geometric target is limited to +/-230 cm even if an old Blueprint still
	// has GoalkeeperLateralMoveRange = 280.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0"))
		float GoalkeeperPostSafetyMargin = 70.0f;

	// Angulo lateral (medido desde la normal del centro del arco) a partir
	// del cual el arquero ya usa todo el rango permitido hacia el primer palo.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning", meta = (ClampMin = "1.0", UIMin = "20.0", UIMax = "85.0"))
		float GoalkeeperFullLateralCoverageAngleDegrees = 55.0f;

	// Curva de respuesta angular. Valores menores que 1 hacen que se desplace
	// mas en angulos intermedios; 1.0 seria una respuesta lineal.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning", meta = (ClampMin = "0.05", UIMin = "0.25", UIMax = "2.0"))
		float GoalkeeperLateralResponseExponent = 0.85f;

	// A esta profundidad o menos el posicionamiento lateral usa seguimiento
	// completo (alpha efectivo 1.0), evitando quedarse centrado cuando la
	// pelota esta abierta y muy cerca del arco.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning", meta = (ClampMin = "0.0"))
		float GoalkeeperNearGoalFullFollowDepth = 700.0f;

	// Desde esta profundidad hacia afuera vuelve gradualmente al
	// GoalkeeperBallFollowAlpha configurado.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning", meta = (ClampMin = "1.0"))
		float GoalkeeperFarGoalBaseFollowDepth = 2200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack")
		float AttackFallbackForwardDistance = 500.0f;

	void ResetAfterGoal();

	void ResetBallToCenter();

	void ReleaseAllAIBallPossessions();

	void ReleaseAllHumanBallPossessions();

	void ClearAssignedAI();

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackShortSupportBackDistance = 480.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackShortSupportSideOffset = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackForwardSupportDistance = 760.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackForwardSupportSideOffset = 560.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackRunIntoSpaceDistance = 1250.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackRunIntoSpaceSideOffset = 430.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackWideSupportDistance = 850.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackWideSupportSideOffset = 1050.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackRestDefenseMaxDepthAlpha = 0.48f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackShapeHomeLateralKeepAlpha = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackShapeMaxLateralOffset = 1150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackShapeRoleLateralSeparation = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackShapeRoleDepthSeparation = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackShapePersonalLateralSpacing = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackShapePersonalDepthSpacing = 80.0f;
	////
	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackNoBallDefenderMaxDepthAlpha = 0.52f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackNoBallMidfielderMaxDepthAlpha = 0.72f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackNoBallForwardMaxDepthAlpha = 0.94f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackNoBallCrowdedFinalThirdAlpha = 0.72f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		int32 AttackNoBallCrowdedFinalThirdCount = 3;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Shape")
		float AttackNoBallRoleDisciplineStrength = 0.90f;

/////

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		float AttackCarrierDefenderSoftMaxDepthAlpha = 0.54f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		float AttackCarrierDefenderHardMaxDepthAlpha = 0.70f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		float AttackCarrierMidfielderSoftMaxDepthAlpha = 0.74f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		float AttackCarrierMidfielderHardMaxDepthAlpha = 0.88f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		float AttackCarrierCrowdedHighDepthAlpha = 0.72f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		int32 AttackCarrierCrowdedHighCount = 3;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		int32 AttackCarrierRequiredCoverBehindCount = 2;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		float AttackCarrierCoverBehindDepthGapAlpha = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		float AttackCarrierPressureRadius = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		float AttackCarrierForwardLaneLookAheadDistance = 1400.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		float AttackCarrierForwardLaneHalfWidth = 480.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Carrier")
		float AttackCarrierAdvanceFreedomThreshold = 0.58f;

	//////

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Compensation")
		float AttackCompensationTriggerExtraDepthAlpha = 0.04f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Compensation")
		float AttackCompensationMinCarrierDepthAlpha = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Compensation")
		float AttackCompensationCoverBehindGapAlpha = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Compensation")
		float AttackCompensationMinCoverDepthAlpha = 0.26f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Compensation")
		float AttackCompensationMaxCoverDepthAlpha = 0.56f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Compensation")
		float AttackCompensationLateralFollowAlpha = 0.55f;

	//////

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Space")
		float AttackSpaceDepthSearchStep = 280.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Space")
		float AttackSpaceLateralSearchStep = 380.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Space")
		float AttackSpaceOpponentAvoidRadius = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Space")
		float AttackSpaceTeammateAvoidRadius = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Space")
		float AttackSpaceLaneBlockHalfWidth = 340.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Space")
		float AttackSpaceBaseLocationPenaltyWeight = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Space")
		float AttackSpaceOpponentPenaltyWeight = 1.4f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Space")
		float AttackSpaceTeammatePenaltyWeight = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Space")
		float AttackSpaceBlockedLanePenalty = 700.0f;

	/////

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionMinPassScore = 620.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionMinForcedPassScore = 430.0f;

	// Only used after the local dribble grid confirms that every immediate
	// carry direction is unsafe. The pass evaluator itself remains unchanged;
	// this threshold merely allows a safer lateral/back recycle to preserve
	// possession instead of insisting on another dribble.
	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionMinRetentionPassScore = 430.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionMinPassDistance = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionMaxPassDistance = 2600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionPassToSpaceLeadDistance = 650.0f;

	// A moving receiver can receive ahead even when it is the human player and
	// therefore has no ESoccerAIOrder assigned by the collective AI.
	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		bool bEnableAttackForwardSpacePass = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassForwardRunMinSpeed = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassForwardRunMinDot = 0.40f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassForwardLeadTime = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassForwardMinLeadDistance = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassForwardMaxLeadDistance = 750.0f;

	// Negative values permit a small, deliberately imperfect optimistic pass.
	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassForwardMinArrivalMargin = -0.08f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassToFeetLeadTime = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassToFeetMaxLeadDistance = 160.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassToFeetCriticalOpponentRadius = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassToFeetCriticalPressurePenalty = 850.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassRetentionLeadDistance = 160.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassRetentionMaxDepthAdvantage = 0.04f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassRetentionSafetyBonus = 380.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision|Pass Type")
		float AttackPassTargetFieldInset = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionPassTargetOpponentRadius = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionPassTargetTeammateRadius = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionPassLaneHalfWidth = 320.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionShotMinDepthAlpha = 0.78f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Decision")
		float AttackDecisionShotLaneHalfWidth = 430.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSupportCurrentRoleBonus = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSecondaryCurrentRoleBonus = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSupportDistanceWeight = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSecondaryDistanceWeight = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSupportMidfielderBonus = 850.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSupportForwardBonus = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSupportDefenderPenalty = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSecondaryForwardBonus = 620.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSecondaryMidfielderBonus = 480.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSecondaryDefenderPenalty = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSupportMinDistanceToCarrier = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSupportMaxDistanceToCarrier = 2100.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSupportTooClosePenalty = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSupportTooFarPenalty = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSecondaryMinDistanceFromSupport = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Attack Stability")
		float AttackSecondaryTooCloseToSupportPenalty = 720.0f;

	/////

	UPROPERTY(EditAnywhere, Category = "Soccer|Reset")
		float BallResetHeight = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Reset")
		float GoalResetDelay = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff")
		float KickoffHomeAcceptanceRadius = 120.0f;

	// Distance behind the ball where the taker waits before the final run.
	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff|Run Up", meta = (ClampMin = "50.0"))
		float KickoffRunUpDistance = 250.0f;

	// Distance beyond the ball used as the navigation target during the run.
	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff|Run Up", meta = (ClampMin = "50.0"))
		float KickoffRunThroughDistance = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff|Run Up", meta = (ClampMin = "1.0"))
		float KickoffRunUpMoveAcceptanceRadius = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff|Run Up", meta = (ClampMin = "5.0"))
		float KickoffRunUpAcceptanceRadius = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff|Run Up|Contact", meta = (ClampMin = "0.0"))
		float KickoffMaxBallSurfaceGapForKick = 22.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff|Run Up|Contact", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
		float KickoffMinimumFacingDot = 0.72f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff")
		float KickoffReceiverMinDistanceFromCenter = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff")
		float KickoffPassHorizontalSpeed = 850.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff")
		float KickoffPassMinTravelTime = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff")
		float KickoffPassMaxTravelTime = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff")
		bool bKickoffRequiresLegalPlayerPositions = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff")
		float KickoffOwnHalfTolerance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Kickoff")
		float KickoffCenterCircleExtraDistance = 40.0f;

	ESoccerTeam PendingKickoffTeam = ESoccerTeam::PlayerTeam;

	UPROPERTY()
		ASoccerAICharacter* KickoffTakerAI = nullptr;

	UPROPERTY()
		ASoccerAICharacter* KickoffReceiverAI = nullptr;

	bool bKickoffFinalRunActive = false;
	bool bKickoffAIKickMontageStarted = false;
	FVector KickoffPendingAIKickTargetLocation = FVector::ZeroVector;

	FVector KickoffKickDirection = FVector::ForwardVector;

	// Direccion geometrica de la carrera final. Se calcula desde la
	// posicion real del ejecutor hacia la pelota al comenzar la carrera.
	FVector KickoffRunDirection = FVector::ForwardVector;

	FVector KickoffRunUpStartLocation = FVector::ZeroVector;

	FVector KickoffRunThroughLocation = FVector::ZeroVector;

	// Estado comun de contacto para la carrera final. Detecta tanto
	// proximidad instantanea como cruce entre frames.
	FRestartKickContactTracker KickoffKickContactTracker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Score", meta = (AllowPrivateAccess = "true"))
		int32 PlayerTeamScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Score", meta = (AllowPrivateAccess = "true"))
		int32 OpponentTeamScore = 0;

	// ============================================================
	// MATCH TIME / PERIODS
	// Stage 10A intentionally does not swap field ends yet. That migration
	// belongs to the SoccerField-driven geometry stages that follow.
	// ============================================================
	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Time")
	bool bEnableMatchClock = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Time", meta = (ClampMin = "1.0", UIMin = "5.0"))
	float HalfDurationSeconds = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Time", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HalfTimeDurationSeconds = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Time")
	bool bShowMatchClockHUD = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Time")
	ESoccerTeam FirstHalfKickoffTeam = ESoccerTeam::PlayerTeam;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Match|Time", meta = (AllowPrivateAccess = "true"))
	ESoccerMatchPeriod CurrentMatchPeriod = ESoccerMatchPeriod::FirstHalf;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Match|Time", meta = (AllowPrivateAccess = "true"))
	float CurrentHalfElapsedSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Match|Time", meta = (AllowPrivateAccess = "true"))
	float TotalMatchElapsedSeconds = 0.0f;

	float HalfTimeElapsedSeconds = 0.0f;
	bool bCurrentHalfClockStarted = false;
	bool bMatchPeriodAppliedMoveInputLock = false;

	// ============================================================
	// HALF-TIME FIELD TRANSITION
	// Each team uses a different touchline-side corridor. Bots first fan out on
	// their current half, then cross the halfway line through individual lanes.
	// Normal kickoff positioning only starts after this exchange-of-ends phase.
	// ============================================================
	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Half Time Transition")
	bool bEnableHalfTimeFieldTransition = true;

	// Absolute lateral fraction of the pitch half-width used as the centre of
	// each team's corridor. PlayerTeam uses +width, OpponentTeam uses -width.
	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Half Time Transition", meta = (ClampMin = "0.15", ClampMax = "0.90", UIMin = "0.25", UIMax = "0.80"))
	float HalfTimeFieldTransitionCorridorLateralAlpha = 0.55f;

	// Half-spread around the corridor centre. Players are distributed across it
	// according to their lateral order at the whistle, avoiding a single queue.
	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Half Time Transition", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "800.0"))
	float HalfTimeFieldTransitionLaneHalfSpreadCm = 450.0f;

	// First waypoint remains in the old half so the two teams separate before
	// they actually cross each other. Geometry is local to ASoccerField.
	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Half Time Transition", meta = (ClampMin = "50.0", UIMin = "100.0", UIMax = "1000.0"))
	float HalfTimeFieldTransitionEntryDepthFromHalfwayCm = 450.0f;

	// Exit waypoint lies clearly inside the future own half. Once reached, the
	// bot waits there until the regular second-half kickoff setup takes over.
	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Half Time Transition", meta = (ClampMin = "100.0", UIMin = "200.0", UIMax = "1500.0"))
	float HalfTimeFieldTransitionExitDepthFromHalfwayCm = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Half Time Transition", meta = (ClampMin = "20.0", UIMin = "40.0", UIMax = "250.0"))
	float HalfTimeFieldTransitionAcceptanceRadiusCm = 100.0f;

	// Safety only: the halftime may extend beyond HalfTimeDurationSeconds while
	// bots finish crossing, but it will never wait forever for a blocked pawn.
	UPROPERTY(EditAnywhere, Category = "Soccer|Match|Half Time Transition", meta = (ClampMin = "1.0", UIMin = "3.0", UIMax = "30.0"))
	float HalfTimeFieldTransitionMaximumDurationSeconds = 12.0f;

	bool bHalfTimeFieldTransitionRuntimeActive = false;
	TMap<ASoccerAICharacter*, FVector> HalfTimeFieldTransitionEntryTargets;
	TMap<ASoccerAICharacter*, FVector> HalfTimeFieldTransitionExitTargets;
	TSet<ASoccerAICharacter*> HalfTimeFieldTransitionEntryCompleted;
	TSet<ASoccerAICharacter*> HalfTimeFieldTransitionExitCompleted;

	// ============================================================
	// OPPONENT COACH AI
	// Stage 11 changes only high-level opponent configuration. It reuses the
	// existing formation, collective tactics and slot-instruction systems.
	// ============================================================
	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent")
	bool bEnableOpponentCoachAI = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent")
	bool bOpponentCoachControlsFormation = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent")
	bool bOpponentCoachControlsCollectiveTactics = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent")
	bool bOpponentCoachControlsIndividualInstructions = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent", meta = (ClampMin = "0.10", ClampMax = "5.0"))
	float OpponentCoachDecisionIntervalSeconds = 0.50f;

	// Anti-oscillation gap for time-only changes. A score change may bypass it.
	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float OpponentCoachMinimumProgressBetweenChanges = 0.06f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpponentCoachProtectLeadProgress = 0.60f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpponentCoachProtectTwoGoalLeadProgress = 0.40f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpponentCoachLockDownProgress = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpponentCoachLockDownTwoGoalLeadProgress = 0.70f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpponentCoachChaseGameProgress = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpponentCoachChaseTwoGoalDeficitProgress = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpponentCoachAllOutAttackProgress = 0.80f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpponentCoachAllOutAttackTwoGoalDeficitProgress = 0.60f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Coach|Opponent", meta = (AllowPrivateAccess = "true"))
	ESoccerOpponentCoachMode CurrentOpponentCoachMode =
		ESoccerOpponentCoachMode::Baseline;

	ESoccerFormationSystem OpponentCoachBaselineFormation =
		ESoccerFormationSystem::OneThreeTwoOne;
	FSoccerTeamTacticalPlan OpponentCoachBaselineTacticalPlan;
	TArray<FSoccerSlotTacticalInstruction> OpponentCoachBaselineSlotInstructions;
	float OpponentCoachDecisionAccumulator = 0.0f;
	float OpponentCoachLastModeChangeProgress = -1.0f;
	int32 OpponentCoachLastObservedPlayerScore = 0;
	int32 OpponentCoachLastObservedOpponentScore = 0;
	bool bOpponentCoachInitialized = false;

	// Stage 9L: the rival coach uses the Stage 9K safe substitution queue.
	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Substitutions")
	bool bEnableOpponentCoachAutomaticSubstitutions = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Substitutions", meta = (ClampMin = "0.25", ClampMax = "10.0"))
	float OpponentCoachSubstitutionEvaluationIntervalSeconds = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Substitutions", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpponentCoachMinimumProgressBetweenSubstitutions = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Coach|Opponent|Substitutions|Debug")
	bool bLogOpponentCoachSubstitutionEvaluations = true;

	float OpponentCoachSubstitutionEvaluationAccumulator = 0.0f;
	float OpponentCoachLastSubstitutionDecisionProgress = -1.0f;
	float OpponentCoachLastSubstitutionDiagnosticProgress = -1.0f;
	bool bForceOpponentCoachSubstitutionEvaluation = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Match", meta = (AllowPrivateAccess = "true"))
		ESoccerMatchPlayState MatchPlayState = ESoccerMatchPlayState::KickoffSetup;

	FTimerHandle GoalResetTimerHandle;
	FTimerHandle GoalReplayStartTimerHandle;
	ESoccerTeam PendingGoalReplayScoringTeam = ESoccerTeam::PlayerTeam;

	FVector GetOwnGoalReferenceLocation(ESoccerTeam Team) const;

	FVector GetOpponentGoalReferenceLocation(ESoccerTeam Team) const;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeHomeLateralKeepAlpha = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeBallSideShiftAlpha = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeMaxLateralShift = 760.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeDepthBlend = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeDefensiveBallDepthInfluence = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeAttackingBallDepthInfluence = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeDefendingDefenderDepthAlpha = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeDefendingMidfielderDepthAlpha = 0.34f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeDefendingForwardDepthAlpha = 0.58f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeAttackingDefenderDepthAlpha = 0.44f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeAttackingMidfielderDepthAlpha = 0.62f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeAttackingForwardDepthAlpha = 0.80f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Dynamic Shape")
		float DynamicShapeNeutralHomeToBallAlpha = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Navigation")
		float NavigationProjectionExtent = 350.0f;

	//defense
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination")
		bool bUseGoalAreaDefenderCoordination = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination", meta = (ClampMin = "0.0"))
		float GoalAreaDefenderCoordinationActivationMargin = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination", meta = (ClampMin = "50.0"))
		float GoalAreaDefenderCoordinationParticipationDistance = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination|Goalkeeper Corridor", meta = (ClampMin = "30.0"))
		float GoalAreaGoalkeeperMobilityCorridorHalfWidth = 165.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination|Goalkeeper Corridor", meta = (ClampMin = "0.0"))
		float GoalAreaGoalkeeperCorridorBackExtension = 70.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination|Goalkeeper Corridor", meta = (ClampMin = "0.0"))
		float GoalAreaGoalkeeperCorridorFrontExtension = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination|Goalkeeper Corridor", meta = (ClampMin = "0.0"))
		float GoalAreaDefenderCorridorClearance = 85.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination|Goalkeeper Corridor", meta = (ClampMin = "0.0"))
		float GoalAreaDefenderMinDistanceFromGoalkeeper = 190.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination|Cover Positions", meta = (ClampMin = "0.0"))
		float GoalAreaFarPostCoverDepth = 95.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination|Cover Positions", meta = (ClampMin = "0.0"))
		float GoalAreaFarPostInsetFromPost = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination|Cover Positions", meta = (ClampMin = "0.0"))
		float GoalAreaSecondaryCoverDepth = 285.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination|Cover Positions", meta = (ClampMin = "0.0"))
		float GoalAreaSecondaryCoverExtraLateralClearance = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Defender Coordination|Cover Positions", meta = (ClampMin = "0.0"))
		float GoalAreaCoordinationMaxLateralBeyondArea = 170.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect")
		bool bUseGoalAreaAttackerRespect = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect", meta = (ClampMin = "0.0"))
		float GoalAreaAttackerRespectActivationMargin = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect", meta = (ClampMin = "50.0"))
		float GoalAreaAttackerRespectParticipationDistance = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect|Goalkeeper Holding Ball", meta = (ClampMin = "0.0"))
		float GoalAreaAttackerHoldingWaitOutsideDepth = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect|Goalkeeper Holding Ball", meta = (ClampMin = "0.0"))
		float GoalAreaAttackerHoldingMinGoalkeeperDistance = 340.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect|Goalkeeper Holding Ball", meta = (ClampMin = "0.0"))
		float GoalAreaAttackerHoldingLateralSpread = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect|Free Ball Approach", meta = (ClampMin = "0.0"))
		float GoalAreaAttackerDirectContestDistance = 315.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect|Free Ball Approach", meta = (ClampMin = "30.0"))
		float GoalAreaAttackerApproachLateralOffset = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect|Free Ball Approach", meta = (ClampMin = "0.0"))
		float GoalAreaAttackerApproachFieldOffset = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect|Goalkeeper Corridor", meta = (ClampMin = "30.0"))
		float GoalAreaAttackerCorridorHalfWidth = 175.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect|Goalkeeper Corridor", meta = (ClampMin = "0.0"))
		float GoalAreaAttackerCorridorClearance = 75.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect|Goalkeeper Corridor", meta = (ClampMin = "0.0"))
		float GoalAreaAttackerMinDistanceFromGoalkeeper = 190.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Area|Attacker Respect|Goalkeeper Corridor", meta = (ClampMin = "0.0"))
		float GoalAreaAttackerMaxLateralBeyondArea = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense")
		float DefensiveProtectGoalLaneMinDistanceFromGoal = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense")
		float DefensiveProtectGoalLaneMaxDistanceFromGoal = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense")
		float DefensiveCoverCenterNearGoalDepthAlpha = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense")
		float DefensiveCoverCenterMidfieldDepthAlpha = 0.48f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense")
		float DefensiveCoverCenterBallSideShiftAlpha = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense")
		float DefensiveCoverCenterMaxLateralOffset = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Pressure")
		float DefensivePressureDistanceWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Pressure")
		float DefensivePressureWrongSidePenalty = 550.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Pressure")
		float DefensivePressureNoCoverPenalty = 750.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Pressure")
		float DefensivePressureCoverBehindDepthGapAlpha = 0.06f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Pressure")
		float DefensivePressureNearOwnGoalDepthAlpha = 0.42f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Pressure")
		float DefensivePressureFarFromOwnGoalDepthAlpha = 0.58f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Pressure")
		float DefensivePressureDeepDefenderPenalty = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Goal Lane")
		float DefensiveGoalLaneDistanceWeight = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Goal Lane")
		float DefensiveGoalLaneDefenderBonus = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Goal Lane")
		float DefensiveGoalLaneMidfielderBonus = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Goal Lane")
		float DefensiveGoalLaneForwardPenalty = 550.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Goal Lane")
		float DefensiveGoalLaneWrongSidePenalty = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Goal Lane")
		float DefensiveGoalLaneTooCloseToPressurePenalty = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Goal Lane")
		float DefensiveGoalLaneMinDistanceFromPressure = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Goal Lane")
		float DefensiveGoalLaneBlockDistanceNearGoal = 720.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Goal Lane")
		float DefensiveGoalLaneBlockDistanceMidfield = 1250.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Goal Lane")
		float DefensiveGoalLaneBallSideLateralInfluence = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveDangerousReceiverMinScore = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveDangerousReceiverDepthWeight = 1400.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveDangerousReceiverForwardBonus = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveDangerousReceiverMidfielderBonus = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveDangerousReceiverNearGoalDepthAlpha = 0.72f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveDangerousReceiverNearGoalBonus = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveDangerousReceiverDefenderNearPenalty = 280.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveDangerousReceiverFreeRadius = 620.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveDangerousReceiverPassLaneHalfWidth = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveDangerousReceiverPassLaneBlockedPenalty = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveMarkReceiverGoalSideDistance = 320.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveMarkReceiverMinDistanceFromOwnGoal = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveMarkerDistanceWeight = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveMarkerDefenderBonus = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveMarkerMidfielderBonus = 340.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveMarkerForwardPenalty = 480.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Marking")
		float DefensiveMarkerWrongSidePenalty = 520.0f;



	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealMinOpportunityScore = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealGoalSideDotThreshold = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealGoalSideBonus = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealWrongSidePenalty = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealCoverBehindDepthGapAlpha = 0.07f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealCoverBehindBonus = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealNoCoverPenalty = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealDangerNearGoalDepthAlpha = 0.46f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealDangerNearGoalBonus = 1400.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealFarFromGoalDepthAlpha = 0.58f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealDefenderFarFromGoalPenalty = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealForwardPressureBonus = 320.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealMidfielderPressureBonus = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealDefenderPressureBonus = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealIdealDistance = 95.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealDistanceWeight = 4.5f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealEmergencyDepthAlpha = 0.36f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Steal")
		float DefensiveStealEmergencyBallDistance = 145.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Stability")
		float DefensivePressureCurrentRoleBonus = 650.0f;

	// A designated presser that is already within this distance finishes the
	// pressure instead of turning away because another teammate's tactical score
	// became marginally better on the next role-selection update.
	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Stability", meta = (ClampMin = "0.0"))
		float DefensivePressureCommitDistance = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Stability")
		float DefensiveGoalLaneCurrentRoleBonus = 820.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Stability")
		float DefensiveCoverCenterCurrentRoleBonus = 620.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Stability")
		float DefensiveMarkerCurrentRoleBonus = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Cover Center")
		float DefensiveCoverCenterDistanceWeight = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Cover Center")
		float DefensiveCoverCenterDefenderBonus = 320.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Cover Center")
		float DefensiveCoverCenterMidfielderBonus = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Defense Cover Center")
		float DefensiveCoverCenterForwardPenalty = 700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		bool bEnableOffsideAwareAttackPositioning = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsideSafetyDistanceBehindLastFieldPlayer = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float AttackRunReleaseDuration = 1.15f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		bool bEnableOffsideRule = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsidePositionTolerance = 65.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Freeze")
		bool bEnableOffsideFreezePresentation = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Freeze", meta = (ClampMin = "0.0"))
		float OffsideFreezeDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Freeze", meta = (ClampMin = "1.0"))
		float OffsideFreezeLineThickness = 18.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Freeze", meta = (ClampMin = "1.0"))
		float OffsideFreezeLineHeight = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Freeze")
		float OffsideFreezeLineCenterZ = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Freeze")
		FLinearColor OffsideFreezeLineColor = FLinearColor(1.0f, 0.03f, 0.01f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Freeze")
		TSubclassOf<ASoccerOffsideLineActor> OffsideLineActorClass;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsideRestartPassHorizontalSpeed = 1350.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsideRestartPassMinTravelTime = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsideRestartPassMaxTravelTime = 0.85f;

	//

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsideRestartMinSetupTime = 1.35f;

	// Conservado para compatibilidad con instancias existentes. Ya no
	// autoriza a ejecutar si algun bot todavia no esta acomodado.
	// Punto de partida de la carrera final, medido detras de la pelota
	// respecto de la direccion elegida para el pase.
	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Run Up", meta = (ClampMin = "50.0"))
		float OffsideRestartRunUpDistance = 250.0f;

	// Destino virtual situado despues de la pelota. Mantiene al bot corriendo
	// hasta alcanzar el punto de contacto, sin frenar sobre el balon.
	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Run Up", meta = (ClampMin = "50.0"))
		float OffsideRestartRunThroughDistance = 140.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Run Up", meta = (ClampMin = "1.0"))
		float OffsideRestartRunUpMoveAcceptanceRadius = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Run Up", meta = (ClampMin = "5.0"))
		float OffsideRestartRunUpAcceptanceRadius = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Run Up|Contact", meta = (ClampMin = "0.0"))
		float OffsideRestartMaxBallSurfaceGapForKick = 22.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Run Up|Contact", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
		float OffsideRestartMinimumFacingDot = 0.72f;

	// Conservado para compatibilidad con Blueprints o instancias existentes.
	// La preparacion nueva usa OffsideRestartRunUpAcceptanceRadius.
	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsideRestartReceiverReadyDistance = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsideRestartReceiverForwardDistance = 850.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsideRestartReceiverLateralDistance = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsideRestartOpponentRequiredDistance = 760.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside")
		float OffsideRestartOpponentMoveExtraDistance = 280.0f;

	// AI-only hysteresis for free-kick positioning. A bot is not considered
	// fully clear the instant it crosses the legal circle; it must leave this
	// extra buffer as well. Human restriction geometry remains unchanged.
	UPROPERTY(EditAnywhere, Category = "Soccer|Offside", meta = (ClampMin = "0.0"))
		float OffsideRestartOpponentLegalBuffer = 90.0f;

	// Mientras un rival siga dentro del radio, el ejecutor espera detrás
	// y a un costado de la pelota para no bloquear su salida.
	UPROPERTY(EditAnywhere, Category = "Soccer|Offside", meta = (ClampMin = "0.0"))
		float OffsideRestartTakerWaitingBackDistance = 280.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside", meta = (ClampMin = "0.0"))
		float OffsideRestartTakerWaitingLateralDistance = 220.0f;

	// Apertura entre las rutas alternativas que prueba un rival para salir
	// del radio cuando otro personaje bloquea el camino radial directo.
	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Restart Radius Reminder")
		bool bEnableOffsideRestartHumanRadiusIndicator = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Restart Radius Reminder", meta = (ClampMin = "1.0", AdvancedDisplay))
		float OffsideRestartHumanRadiusCircleThickness = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Restart Radius Reminder", meta = (AdvancedDisplay))
		float OffsideRestartHumanRadiusCircleCenterZ = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Restart Radius Reminder", meta = (AdvancedDisplay))
		FLinearColor OffsideRestartHumanRadiusCircleColor =
			FLinearColor(0.62f, 0.56f, 0.18f, 0.34f);

	UPROPERTY(EditAnywhere, Category = "Soccer|Offside|Restart Radius Reminder", meta = (AdvancedDisplay))
		TSubclassOf<ASoccerRestartRadiusActor>
			OffsideRestartHumanRadiusActorClass;


	// ============================================================
	// OUT OF PLAY PRESENTATION
	// ============================================================

	// Time during which the ball keeps its real physics after completely
	// crossing a touchline or goal line. Setting this to zero restores the
	// previous immediate teleport behavior. Goals use GoalResetDelay instead.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Out Of Play|Presentation",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "6.0")
	)
		float BallOutOfPlayContinuationDuration = 3.5f;


	ESoccerRestartType PendingBallOutOfPlayRestartType =
		ESoccerRestartType::None;

	ESoccerTeam PendingBallOutOfPlayRestartTeam =
		ESoccerTeam::PlayerTeam;

	FVector PendingBallOutOfPlayRestartReferenceLocation =
		FVector::ZeroVector;

	FVector PendingBallOutOfPlayThrowInInwardDirection =
		FVector::ZeroVector;

	float PendingBallOutOfPlayGoalLineSign = 0.0f;


	// ============================================================
	// THROW IN SETTINGS
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In")
		bool bEnableThrowInRule = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In")
		float ThrowInMinSetupTime = 1.0f;

	// Conservado para compatibilidad con instancias existentes. Ya no
	// autoriza a ejecutar si algun bot todavia no esta acomodado.
	// Conservado para compatibilidad con Blueprints existentes. La quinta
	// migracion usa las propiedades de Pickup para el ejecutor.
	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In")
		float ThrowInReceiverReadyDistance = 420.0f;

	// Human claim/release uses hysteresis like the foot restarts, but stays
	// throw-in-specific because hand possession has different input rules.
	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Human Taker", meta = (ClampMin = "50.0"))
		float ThrowInHumanTakerClaimRadius = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Human Taker", meta = (ClampMin = "50.0"))
		float ThrowInHumanTakerReleaseRadius = 380.0f;

	// Claiming does not teleport the ball: the human must actually walk up to
	// the pickup staging point before the restart becomes committed.
	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Human Taker", meta = (ClampMin = "10.0"))
		float ThrowInHumanPickupReadyDistance = 90.0f;

	// Conservado para compatibilidad. Ya no define el punto previo.
	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In", meta = (AdvancedDisplay))
		float ThrowInStagingInsideDistance = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Pickup", meta = (ClampMin = "20.0"))
		float ThrowInPickupInsideDistance = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Pickup", meta = (ClampMin = "1.0"))
		float ThrowInPickupMoveAcceptanceRadius = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Pickup", meta = (ClampMin = "1.0"))
		float ThrowInPickupReadyDistance = 28.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In")
		float ThrowInOutsidePositioningSpeed = 430.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In")
		float ThrowInReturnToFieldSpeed = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In")
		float ThrowInDesiredCapsuleOutsideOffsetAtRelease = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In")
		float ThrowInFallbackOutsideDistance = 230.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In")
		float ThrowInMaximumSideAngleDegrees = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In")
		float ThrowInOpponentRequiredDistance = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In")
		float ThrowInOpponentMoveExtraDistance = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Pass")
		float ThrowInPassHorizontalSpeed = 1150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Pass")
		float ThrowInPassMinTravelTime = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Pass")
		float ThrowInPassMaxTravelTime = 1.15f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Throw In|Animation",
		meta = (ClampMin = "0.0", ClampMax = "1.0")
	)
		float ThrowInReleaseNormalizedTime = 0.84f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Curve Motion")
		bool bUseThrowInCurveMotion = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Curve Motion")
		UCurveTable* ThrowInMotionCurveTable = nullptr;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Curve Motion")
		float ThrowInForwardMotionScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Curve Motion")
		float ThrowInLateralMotionScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Selection")
		float ThrowInMidfielderRolePenalty = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Selection")
		float ThrowInForwardRolePenalty = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Selection")
		float ThrowInGoalkeeperRolePenalty = 10000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Throw In|Selection")
		float ThrowInHomeDisruptionWeight = 0.10f;


	bool bBallBoundarySampleInitialized = false;
	bool bBallOutOfPlayLatched = false;
	FVector PreviousBallBoundarySampleLocation = FVector::ZeroVector;

	UPROPERTY()
		ASoccerAICharacter* ThrowInTakerAI = nullptr;

	UPROPERTY()
		ASoccerAICharacter* ThrowInReceiverAI = nullptr;

	// The configured AI remains the fallback taker. The human claim is kept
	// separate so walking away before pickup can hand responsibility back cleanly.
	UPROPERTY()
		AThirdPersonCppCharacter* ThrowInHumanTaker = nullptr;

	bool bThrowInHumanTakerClaimed = false;
	bool bThrowInHumanTakerCommitted = false;
	bool bThrowInHumanExecutionAuthorized = false;
	bool bThrowInHumanTargetSelected = false;
	bool bThrowInHumanRepositioningForTarget = false;
	bool bThrowInHumanMontageStarted = false;
	FVector ThrowInHumanTargetLocation = FVector::ZeroVector;

	ESoccerTeam ThrowInTeam = ESoccerTeam::PlayerTeam;
	FVector ThrowInLocation = FVector::ZeroVector;
	FVector ThrowInInwardDirection = FVector::RightVector;
	FVector ThrowInDirection = FVector::ForwardVector;
	FVector ThrowInRightDirection = FVector::RightVector;
	FVector ThrowInStagingLocation = FVector::ZeroVector;
	FVector ThrowInOutsideStartLocation = FVector::ZeroVector;
	FVector ThrowInReceiverMoveLocation = FVector::ZeroVector;
	float ThrowInSetupStartTime = -1000.0f;
	bool bThrowInStagedDuringBallOutOfPlayDelay = false;

	bool bThrowInExecutionActive = false;
	bool bThrowInBallReleased = false;
	bool bThrowInReturningToField = false;
	bool bThrowInCurveMotionInitialized = false;
	FVector ThrowInCurveMotionStartLocation = FVector::ZeroVector;

	// ============================================================
	// GOAL LINE RESTART SETTINGS
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart")
		bool bEnableGoalLineRestartRule = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart")
		float GoalLineRestartMinSetupTime = 1.0f;

	// Conservado para compatibilidad con instancias existentes. Ya no
	// autoriza a ejecutar si algun bot todavia no esta acomodado.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart")
		float GoalLineRestartTakerReadyDistance = 145.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart")
		float GoalLineRestartReceiverReadyDistance = 430.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Detection Safety")
		float GoalLinePotentialGoalSideMargin = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Detection Safety")
		float GoalLinePotentialGoalTopMargin = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Detection Safety")
		float GoalLinePotentialGoalBottomMargin = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick")
		float GoalKickBallInwardDistance = 280.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick")
		float GoalKickBallLateralOffset = 260.0f;

	// Conservado para compatibilidad con instancias existentes.
	// La nueva preparacion usa GoalKickRunUpDistance.
	// Distancia detras de la pelota desde la que comienza la carrera final.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick|Run Up", meta = (ClampMin = "50.0"))
		float GoalKickRunUpDistance = 250.0f;

	// Destino virtual situado despues de la pelota para evitar el frenado previo.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick|Run Up", meta = (ClampMin = "50.0"))
		float GoalKickRunThroughDistance = 140.0f;

	// Precision de navegacion para llegar al punto inicial de carrera.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick|Run Up", meta = (ClampMin = "1.0"))
		float GoalKickRunUpMoveAcceptanceRadius = 12.0f;

	// Tolerancia usada por la preparacion comun para considerar listo al ejecutor.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick|Run Up", meta = (ClampMin = "5.0"))
		float GoalKickRunUpAcceptanceRadius = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick|Contact", meta = (ClampMin = "0.0"))
		float GoalKickMaxBallSurfaceGapForKick = 22.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick|Contact", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
		float GoalKickMinimumFacingDot = 0.80f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick")
		float GoalKickOpponentPenaltyAreaExtraDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick|Pass")
		float GoalKickPassHorizontalSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick|Pass")
		float GoalKickPassMinTravelTime = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Goal Kick|Pass")
		float GoalKickPassMaxTravelTime = 1.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick")
		float CornerKickBallInsetFromLines = 42.0f;

	// Conservado para compatibilidad con instancias existentes.
	// La nueva ejecucion usa CornerKickRunUpDistance.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick")
		float CornerKickStagingInsideDistance = 105.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick")
		float CornerKickOutsidePositioningSpeed = 360.0f;

	// Distancia exterior desde la que empieza la carrera hacia la pelota.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick|Run Up", meta = (ClampMin = "50.0"))
		float CornerKickRunUpDistance = 250.0f;

	// Punto virtual posterior a la pelota que evita frenar antes del golpe.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick|Run Up", meta = (ClampMin = "50.0"))
		float CornerKickRunThroughDistance = 140.0f;

	// Velocidad constante de la carrera manual fuera del NavMesh.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick|Run Up", meta = (ClampMin = "1.0"))
		float CornerKickFinalRunSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick|Contact", meta = (ClampMin = "0.0"))
		float CornerKickMaxBallSurfaceGapForKick = 22.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick|Contact", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
		float CornerKickMinimumFacingDot = 0.80f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick")
		float CornerKickOpponentRequiredDistance = 915.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick")
		float CornerKickOpponentMoveExtraDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick|Pass")
		float CornerKickPassHorizontalSpeed = 1325.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick|Pass")
		float CornerKickPassMinTravelTime = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Corner Kick|Pass")
		float CornerKickPassMaxTravelTime = 1.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Selection")
		float GoalLineRestartGoalkeeperCornerPenalty = 10000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Selection")
		float GoalLineRestartGoalkeeperGoalKickBonus = 5000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal Line Restart|Selection")
		float GoalLineRestartHomeDisruptionWeight = 0.08f;


	// Goal-line restart runtime state now lives in FSoccerGoalLineRestart.

	//

	UPROPERTY()
		bool bHasPendingOffsideSnapshot = false;

	UPROPERTY()
		ESoccerTeam PendingOffsideAttackingTeam = ESoccerTeam::PlayerTeam;

	UPROPERTY()
		TArray<ASoccerCharacterBase*> PendingOffsideRestrictedPlayers;











	UPROPERTY()
		ASoccerOffsideLineActor* ActiveOffsideLineActor = nullptr;

	ESoccerTeam PendingOffsideFreezeRestartTeam = ESoccerTeam::PlayerTeam;

	FVector PendingOffsideFreezeRestartLocation = FVector::ZeroVector;

	double OffsideFreezeStartRealTime = -1.0;

	bool bOffsideFreezeAppliedGamePause = false;


	UPROPERTY()
		bool bAttackRunReleaseActive = false;

	UPROPERTY()
		ESoccerTeam AttackRunReleaseTeam = ESoccerTeam::PlayerTeam;

	float AttackRunReleaseStartTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug")
		bool bDebugFreezeOpponentTeam = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug")
		bool bDebugFreezePlayerTeam = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug")
		bool bDebugFreezeAllFieldPlayers = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug")
		bool bDebugKeepGoalkeepersActiveWhenFreezing = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Debug")
		bool bDebugFrozenTeamReleaseBall = true;
};
