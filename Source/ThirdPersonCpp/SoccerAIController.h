//SoccerAIController.h

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SoccerTeamTypes.h"
#include "SoccerAerialActionTypes.h"
#include "SoccerAIController.generated.h"

class UCurveTable;
class ASoccerAICharacter;
class ASoccerBall;
class ASoccerMatchManager;
class ASoccerCharacterBase;
class AActor;


enum class ESoccerAIIntent : uint8
{
	None,
	RecoverAndPass,
	RecoverAndCarry,
	RecoverAndShoot,
	KeepPossession
};

enum class ESoccerAIPendingMainAction : uint8
{
	None,
	AutoPass,
	PassToTeammate,
	Shoot
};

struct FGoalkeeperDistributionPlan
{
	ESoccerGoalkeeperDistributionType Type =
		ESoccerGoalkeeperDistributionType::None;

	FVector TargetLocation =
		FVector::ZeroVector;

	float HorizontalSpeed = 0.0f;

	float MinTravelTime = 0.0f;

	float MaxTravelTime = 0.0f;

	bool bUrgent = false;

	bool bHasSafeTeammatePass = false;

	bool bForcedByDebug = false;

	FString DebugLabel;

	bool HasValidData() const
	{
		return
			Type !=
			ESoccerGoalkeeperDistributionType::None &&
			!TargetLocation.IsNearlyZero() &&
			HorizontalSpeed > 0.0f;
	}
};

struct FGoalkeeperAnimatedContactResult
{
	bool bHandContact = false;
	bool bBodyContact = false;

	FName ClosestHandBone = NAME_None;
	FName ClosestBodyBone = NAME_None;

	float ClosestHandDistance = BIG_NUMBER;
	float ClosestBodyDistance = BIG_NUMBER;

	bool HasAnyContact() const
	{
		return bHandContact || bBodyContact;
	}
};

struct FGoalkeeperSaveSelectionResult
{
	ESoccerGoalkeeperAction Action =
		ESoccerGoalkeeperAction::None;

	float SelectedContactTime = 0.0f;

	float RawHandDistance = BIG_NUMBER;

	// Distancia original antes de extender lateralmente la curva de la atajada.
	float RawHandDistanceBeforeAdaptiveLateral = BIG_NUMBER;

	// Factor que se congelara al comenzar el montage. 1.0 = curva original.
	float AdaptiveLateralScale = 1.0f;

	// Traslado lateral adicional real, en centimetros, en el instante de contacto.
	float AdaptiveLateralExtraDistance = 0.0f;

	// Correccion procedural final, expresada en el marco local del arquero:
	// X = Forward, Y = Lateral, Z = Up. Se congela al comenzar el montage.
	FVector NearPerfectContactCorrectionLocal = FVector::ZeroVector;

	// Distancia despues de la ayuda lateral pero antes de la correccion 3D final.
	float RawHandDistanceBeforeNearPerfectCorrection = BIG_NUMBER;

	// 1.0 = velocidad authored. Valores mayores aceleran solo el montage de
	// esta atajada cuando la pelota llegaria ligeramente antes del contacto.
	float MontagePlayRate = 1.0f;

	float FinalScore = BIG_NUMBER;

	FVector PredictedLeftHandLocation =
		FVector::ZeroVector;

	FVector PredictedRightHandLocation =
		FVector::ZeroVector;

	/*
	* Tiempo que le falta a la pelota para cruzar
	* el plano particular de la mano seleccionada.
	*/
	float BallTimeToContactPlane =
		BIG_NUMBER;

	/*
	 * Cuánto falta para tener que comenzar el montage:
	 *
	 * BallTimeToContactPlane - SelectedContactTime
	 *
	 * Positivo: todavía debe esperar.
	 * Cero: debe comenzar ahora.
	 * Negativo: la animación ya llegaría tarde.
	 */
	float RequiredMontageStartDelay =
		BIG_NUMBER;

	FVector PredictedBallLocation =
		FVector::ZeroVector;

	FVector PredictedSelectedHandLocation =
		FVector::ZeroVector;

	bool bSelectedLeftHand =
		false;

	bool bTemporallyFeasible =
		false;

	bool bEmergencySelection =
		false;

	bool IsValid() const
	{
		return
			Action !=
			ESoccerGoalkeeperAction::None;
	}
};

UCLASS(Blueprintable, BlueprintType)
class THIRDPERSONCPP_API ASoccerAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASoccerAIController();

	virtual void Tick(float DeltaTime) override;

	void HandleGoalkeeperContactWindowBegin(
		ASoccerAICharacter* SoccerCharacter
	);

	void HandleGoalkeeperContactWindowTick(
		ASoccerAICharacter* SoccerCharacter
	);

	void HandleGoalkeeperContactWindowEnd(
		ASoccerAICharacter* SoccerCharacter
	);

	void HandleGoalkeeperDistributionEvent(
		ASoccerAICharacter* SoccerCharacter,
		ESoccerGoalkeeperDistributionEvent EventType
	);

	bool StartGoalkeeperDebugSaveTest(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerGoalkeeperAction GoalkeeperAction,
		bool bAllowEmergencyBodyContact
	);

	void FinishGoalkeeperDebugSaveTest(
		ASoccerAICharacter* SoccerCharacter
	);

	bool IsGoalkeeperActivelyClaimingBall() const;

	/* Isolates one AI for the controlled aerial debug tester. */
	void SetAerialDebugIsolation(
		bool bEnabled,
		ASoccerBall* DebugBall = nullptr
	);

protected:
	virtual void BeginPlay() override;

private:
	void FindMatchManager();

	bool TryRegisterAIKickTouchForRules(
		ASoccerAICharacter* SoccerCharacter
	);

	void ReturnToHomePosition();

	void FacePawnTowardLocation(const FVector& TargetLocation, float DeltaTime);

	bool TryPossessBallIfClose(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	bool TryStealBallIfClose(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	bool TryHandleGoalAreaAttackerHoldingRespect(
		ASoccerAICharacter* SoccerCharacter,
		ESoccerAIOrder CurrentOrder
	);

	bool TryShootBallIfClose(
		ASoccerAICharacter* SoccerCharacter
	);

	bool TryExecuteZoneBasedPossessionDecision(
		ASoccerAICharacter* SoccerCharacter,
		ESoccerFieldZone CharacterZone,
		float DeltaTime
	);

	bool IsBallAtAIPossessionHeight(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall
	) const;

	FVector GetDelayedObservedBallLocation(ASoccerBall* SoccerBall);

	void ClearDelayedBallObservation();

	bool ResolvePredictiveBallChaseLocation(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		FVector& OutMoveLocation
	);

	void UpdatePredictiveBallChaseMovement(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerAIOrder CurrentOrder
	);

	// Presion especifica sobre un conductor real o sobre el corredor que
	// intenta recoger su propio autopase. Devuelve true cuando reemplaza la
	// persecucion directa de la pelota por una trayectoria lateral.
	bool UpdateDefensivePressureOvertakeMovement(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	ASoccerCharacterBase* FindDefensivePressureCarrier(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		bool& bOutCarrierIsAutoPassing
	) const;

	bool ShouldDirectlyInterceptAutoPass(
		const ASoccerAICharacter* PressingCharacter,
		const ASoccerAICharacter* AutoPassCarrier,
		const ASoccerBall* SoccerBall
	) const;

	bool BuildDefensivePressureOvertakeLocation(
		ASoccerAICharacter* PressingCharacter,
		ASoccerCharacterBase* Carrier,
		ASoccerBall* SoccerBall,
		bool bCarrierIsAutoPassing,
		FVector& OutMoveLocation
	);

	bool ProjectDefensivePressureLocationToNavigation(
		const FVector& DesiredLocation,
		FVector& OutProjectedLocation
	) const;

	void ClearDefensivePressureOvertakeState();

	void ClearPredictiveBallChaseMovement(
		ASoccerAICharacter* SoccerCharacter = nullptr
	);

	// Stage 1: conservative AI tackle used only to intercept a genuinely
	// loose moving ball when sliding reaches an earlier safe point than running.
	bool TryStartBasicAITackleInterception(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerAIOrder CurrentOrder
	);

	bool IsBasicAITackleLaneSafe(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& TargetLocation,
		const ASoccerCharacterBase* IgnoredOpponent = nullptr
	) const;

	// Stage 2: deliberate tackle against an opponent who currently possesses
	// the ball. Unlike Stage 1, the carrier may occupy the tackle corridor.
	bool TryStartContestedAITackle(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerAIOrder CurrentOrder
	);

	bool TryStartAITackleEvasion(
		ASoccerAICharacter* SoccerCharacter
	);

	bool UpdateAerialBallInterceptionMovement(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerAIOrder CurrentOrder
	);

	void ClearAerialBallInterceptionMovement();

	bool ConfigureAIAerialHeaderDecisionForIntent(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerAerialActionIntent Intent
	);

	bool BuildAIOffensiveHeaderDecision(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	bool BuildAIDefensiveHeaderDecision(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	void ApplyAIAerialHeaderPassExecutionError(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& IntendedTarget,
		float IntendedHorizontalSpeed,
		FVector& OutExecutedTarget,
		float& OutExecutedHorizontalSpeed,
		float& OutDirectionErrorDegrees,
		float& OutPowerErrorFraction
	) const;

	ASoccerCharacterBase* FindBestAerialHeaderTeammate(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		bool bDefensiveDecision,
		FVector& OutTargetLocation,
		float& OutScore
	) const;

	float GetNearestOpponentDistanceToAerialLocation(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& Location
	) const;

	bool IsAerialHeaderPassLaneBlocked(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& StartLocation,
		const FVector& TargetLocation
	) const;

	FVector BuildAIOffensiveHeaderProlongTarget(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall
	) const;

	FVector BuildAIDefensiveHeaderClearanceTarget(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall
	) const;

	FVector ClampAIAerialHeaderTargetInsideField(
		const FVector& TargetLocation
	) const;

	bool FindPrimaryAerialContestCharacter(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerAerialActionIntent Intent,
		const FSoccerAerialInterceptionPlan& SelfPlan,
		ASoccerCharacterBase*& OutPrimaryCharacter,
		FSoccerAerialInterceptionPlan& OutPrimaryPlan,
		float& OutSelfScore,
		float& OutPrimaryScore,
		int32& OutCandidateCount
	) const;

	bool IsAerialContestCandidateEligible(
		const ASoccerAICharacter* Candidate,
		ESoccerAerialActionIntent Intent
	) const;

	float ScoreAerialContestPlan(
		const ASoccerCharacterBase* Candidate,
		const FSoccerAerialInterceptionPlan& Plan
	) const;

	bool UpdateAerialContestSecondBallSupport(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ASoccerCharacterBase* PrimaryCharacter,
		const FSoccerAerialInterceptionPlan& PrimaryPlan,
		ESoccerAIOrder CurrentOrder
	);

	FVector BuildAerialContestSecondBallSupportLocation(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerCharacterBase* PrimaryCharacter,
		const FSoccerAerialInterceptionPlan& PrimaryPlan
	) const;

	void ClearAerialContestCoordinationState();

	UPROPERTY(EditAnywhere, Category = "Soccer|AI")
		float MoveAcceptanceRadius = 80.0f;

	// El arquero necesita correcciones laterales bastante mas finas que un
	// jugador de campo; con el radio generico de 80 cm muchos ajustes de
	// posicionamiento se daban por completados sin que llegara a hacerlos.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Positioning", meta = (ClampMin = "1.0"))
		float GoalkeeperPositioningAcceptanceRadius = 30.0f;

	// ============================================================
	// PREPOSICIONAMIENTO POR TRAYECTORIA HACIA EL ARCO
	// ============================================================
	// Esta capa NO decide si una pelota es un remate ni si se puede atajar.
	// Solo prolonga la direccion horizontal actual de una pelota suelta y,
	// si esa recta cruza la boca del arco, hace que el arquero se alinee
	// lateralmente mientras la logica normal de Save todavia no intervino.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Trajectory Prepositioning")
		bool bUseGoalkeeperTrajectoryPrepositioning = true;

	// Evita interpretar ruido casi estacionario como una direccion estable.
	// Sigue siendo deliberadamente muy inferior al minimo de un remate real.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Trajectory Prepositioning",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0")
	)
		float GoalkeeperTrajectoryPrepositionMinBallSpeed = 40.0f;

	// 3000 cm equivale a media cancha con las dimensiones actuales.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Trajectory Prepositioning",
		meta = (ClampMin = "100.0", UIMin = "100.0", UIMax = "6000.0")
	)
		float GoalkeeperTrajectoryPrepositionMaxDepthFromGoal = 3000.0f;

	// Margen opcional fuera de los postes para activar la anticipacion.
	// Cero significa literalmente: la recta debe atravesar la boca del arco.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Trajectory Prepositioning",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "150.0")
	)
		float GoalkeeperTrajectoryPrepositionGoalSideMargin = 0.0f;

	// El centro de la capsula nunca intenta pegarse al poste aunque la recta
	// cruce muy cerca de el. Mantiene el mismo criterio fisico de Positioning.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Trajectory Prepositioning",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "250.0")
	)
		float GoalkeeperTrajectoryPrepositionPostSafetyMargin = 70.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Trajectory Prepositioning",
		meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "100.0")
	)
		float GoalkeeperTrajectoryPrepositionAcceptanceRadius = 30.0f;

	// ============================================================
	// CARRERA LATERAL DE EMERGENCIA PREVIA A UNA ATAJADA
	// ============================================================
	// Solo se activa cuando el selector ya determino que ninguna animacion
	// disponible puede alcanzar correctamente la pelota desde la posicion
	// actual y, por lo tanto, eligio una EmergencySelection visual.
	// Mientras aun falte tiempo para iniciar el montage, el arquero intenta
	// ganar terreno exclusivamente sobre su eje lateral local.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Emergency Lateral Run")
		bool bUseGoalkeeperEmergencyLateralRun = true;

	// Evita carreras laterales por errores minimos o por selecciones centrales.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Emergency Lateral Run",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "150.0")
	)
		float GoalkeeperEmergencyLateralRunMinSideOffset = 20.0f;

	// Radio pequeno para no considerar terminada la correccion demasiado pronto.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Emergency Lateral Run",
		meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "100.0")
	)
		float GoalkeeperEmergencyLateralRunAcceptanceRadius = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI")
		float BallChaseAcceptanceRadius = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Possession")
		float AIBallPossessionDistance = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Possession")
		float AIPostReleaseRepossessCooldown = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Possession")
		float AIBallStealDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Possession")
		float AIBallPossessionMaxHeight = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Goal Area|Attacker Respect", meta = (ClampMin = "1.0"))
		float GoalAreaAttackerHoldingMoveAcceptanceRadius = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Goal Area|Attacker Respect", meta = (ClampMin = "0.0"))
		float GoalAreaAttackerHoldingRunDistance = 320.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Reaction")
		bool bUseAIReactionDelay = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Reaction")
		float AIReactionDelayMin = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Reaction")
		float AIReactionDelayMax = 0.50f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Reaction")
		float AIReactionDelayTeleportDistance = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Interception")
		bool bUsePredictiveBallChase = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Interception", meta = (ClampMin = "0.02"))
		float AIPredictiveTargetRefreshInterval = 0.16f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Interception", meta = (ClampMin = "0.0"))
		float AIPredictiveMoveRepathMinInterval = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Interception", meta = (ClampMin = "0.0"))
		float AIPredictiveMoveRepathDistance = 65.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Interception", meta = (ClampMin = "0.02"))
		float AIPredictiveMoveForcedRefreshInterval = 0.45f;

	// ============================================================
	// BASIC AI TACKLE INTERCEPTION - STAGE 1
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception")
		bool bUseBasicAITackleInterception = true;

	// Goalkeepers stay out of this first validation stage. Their tackle decision
	// will later consider hand use, sweeper logic and penalty risk explicitly.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception")
		bool bAllowGoalkeeperBasicAITackle = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0"))
		float AITackleDecisionCooldown = 1.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0"))
		float AITackleMinimumBallSpeed = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0"))
		float AITackleMinimumRunnerSpeed = 170.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0"))
		float AITackleMinimumBallDistance = 190.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "1.0"))
		float AITackleMaximumBallDistance = 720.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.10"))
		float AITacklePredictionHorizon = 1.00f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.02"))
		float AITacklePredictionSampleInterval = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0"))
		float AITackleMinimumSampleTime = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.05"))
		float AITackleMaximumSampleTime = 0.85f;

	// Conservative average horizontal speed over the useful part of the slide.
	// It intentionally stays below the typical initial tackle speed because the
	// montage decelerates progressively.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "1.0"))
		float AITackleEstimatedAverageSlideSpeed = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0"))
		float AITackleEstimatedLaunchDelay = 0.03f;

	// Running must be this late before the bot is allowed to replace running
	// with a tackle. This prevents gratuitous slides when ordinary pursuit works.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0"))
		float AITackleRequiredRunLateMargin = 0.06f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0"))
		float AITackleArrivalSafetyMargin = 0.025f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0", ClampMax = "180.0"))
		float AITackleMaximumStartAngleDegrees = 55.0f;

	// In this first stage a rival in the slide corridor vetoes the action.
	// Rival-contact decisions belong to the next tackle-AI stage.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0"))
		float AITackleOpponentLaneHalfWidth = 170.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Basic Interception", meta = (ClampMin = "0.0"))
		float AITackleOpponentTargetSafetyRadius = 230.0f;

	float LastBasicAITackleAttemptTime = -1000.0f;

	// ============================================================
	// CONTESTED AI TACKLE - STAGE 2
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested")
		bool bUseContestedAITackle = true;

	// Goalkeeper tackle decisions require separate sweeper/hand logic.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested")
		bool bAllowGoalkeeperContestedAITackle = false;

	// Conservative default: a defender does not voluntarily slide inside its
	// own penalty area. A later danger model may override this for emergencies.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested")
		bool bAllowContestedAITackleInOwnPenaltyArea = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "0.0"))
		float AIContestedTackleDecisionCooldown = 1.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "0.0"))
		float AIContestedTackleMinimumDistance = 145.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "1.0"))
		float AIContestedTackleMaximumDistance = 520.0f;

	// Short prediction toward the ball position the carrier is likely to have
	// when the slide reaches the challenge.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "0.0", ClampMax = "0.50"))
		float AIContestedTackleLeadTime = 0.14f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "1.0"))
		float AIContestedTackleEstimatedAverageSlideSpeed = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "0.0"))
		float AIContestedTackleEstimatedLaunchDelay = 0.03f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "0.0", ClampMax = "180.0"))
		float AIContestedTackleMaximumStartAngleDegrees = 72.0f;

	// If the tackler is clearly behind the carrier, the risk is too high.
	// Dot is carrierForward · direction(Carrier -> Tackler).
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
		float AIContestedTackleMinimumCarrierSideDot = -0.30f;

	// Ball must not be completely hidden behind the carrier.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
		float AIContestedTackleMinimumBallFrontDot = -0.10f;

	// Used to score how exposed the ball is from the carrier body.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "0.0"))
		float AIContestedTackleLowExposureDistance = 28.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "1.0"))
		float AIContestedTackleHighExposureDistance = 115.0f;

	// 0 = very aggressive, 1 = only near-perfect opportunities.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Contested", meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float AIContestedTackleMinimumDecisionScore = 0.58f;

	float LastContestedAITackleAttemptTime = -1000.0f;

	// ============================================================
	// TACKLE EVASION - STAGE 3
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Evasion")
		bool bUseAITackleEvasion = true;

	// Victim must already be moving; this first version is a running jump.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Evasion", meta = (ClampMin = "0.0"))
		float AITackleEvasionMinimumRunnerSpeed = 120.0f;

	// Threat must be close enough laterally to the projected slide line.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Evasion", meta = (ClampMin = "1.0"))
		float AITackleEvasionThreatHalfWidth = 125.0f;

	// Too little time = surprised; too much time = do not jump prematurely.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Evasion", meta = (ClampMin = "0.0"))
		float AITackleEvasionMinimumReactionTime = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Evasion", meta = (ClampMin = "0.05"))
		float AITackleEvasionMaximumReactionTime = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Evasion", meta = (ClampMin = "1.0"))
		float AITackleEvasionMinimumIncomingSlideSpeed = 180.0f;

	// Ignore challenges that have already progressed too far into recovery.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Tackle|Evasion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float AITackleEvasionMaximumTacklerNormalizedTime = 0.68f;

	// ============================================================
	// DEFENSIVE PRESSURE OVERTAKE
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake")
		bool bUseAIDefensivePressureOvertake = true;

	// Solo se inicia la maniobra cuando el perseguidor ya esta lo bastante
	// cerca como para que perseguir exactamente la pelota lo deje bloqueado
	// detras del conductor.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeActivationDistance = 760.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeMinimumBehindDistance = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeMaximumLateralSeparation = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeMinimumCarrierSpeed = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeLeadTime = 0.38f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeMinimumLeadDistance = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeMaximumLeadDistance = 310.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeForwardDistance = 145.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeLateralDistance = 165.0f;

	// Reduce el score del candidato situado hacia el centro del arco propio.
	// Es una preferencia, no una prohibicion de usar el lado exterior.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeInnerSideScoreBonus = 135.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeSideCommitDuration = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeSideSwitchRequiredAdvantage = 110.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
		float AIPressOvertakeDirectionResetDot = 0.35f;

	// En un autopase se conserva la intercepcion directa si el defensor puede
	// llegar al punto elegido antes que el corredor por este margen.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressAutoPassDirectInterceptionAdvantage = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "1.0"))
		float AIPressOvertakeAcceptanceRadius = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeMoveRepathMinInterval = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.0"))
		float AIPressOvertakeMoveRepathDistance = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defensive Pressure|Overtake", meta = (ClampMin = "0.02"))
		float AIPressOvertakeMoveForcedRefreshInterval = 0.28f;

	TWeakObjectPtr<ASoccerCharacterBase> DefensivePressureOvertakeCarrier;
	bool bDefensivePressureOvertakeAgainstAutoPass = false;
	int32 DefensivePressureOvertakeSideSign = 0;
	FVector DefensivePressureOvertakeCarrierDirection = FVector::ZeroVector;
	float DefensivePressureOvertakeSideCommitEndTime = -1000.0f;

	bool bHasDefensivePressureOvertakeMoveTarget = false;
	FVector LastDefensivePressureOvertakeMoveTarget = FVector::ZeroVector;
	float LastDefensivePressureOvertakeMoveRequestTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial")
		bool bUseAIAerialActions = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial")
		bool bAllowGoalkeeperAerialActions = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision")
		bool bUseAIOffensiveHeaders = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision")
		bool bAllowDefenderOffensiveHeaders = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision", meta = (ClampMin = "0.0"))
		float AIAerialActiveHeaderMaximumGoalDistance = 3400.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision")
		bool bUseAIOffensiveHeaderPasses = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision")
		bool bUseAIOffensiveHeaderProlongations = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision")
		bool bUseAIDefensiveHeaderPasses = true;


	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Target", meta = (ClampMin = "0.0"))
		float AIAerialHeaderPassMinimumDistance = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Target", meta = (ClampMin = "100.0"))
		float AIAerialHeaderPassMaximumDistance = 2200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Target", meta = (ClampMin = "0.0"))
		float AIAerialHeaderTeammateLeadTime = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Target", meta = (ClampMin = "0.0"))
		float AIAerialHeaderMaximumLeadDistance = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Safety", meta = (ClampMin = "0.0"))
		float AIAerialHeaderReceiverCriticalPressureDistance = 155.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Safety", meta = (ClampMin = "1.0"))
		float AIAerialHeaderReceiverSpaceReferenceDistance = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Safety", meta = (ClampMin = "0.0"))
		float AIAerialHeaderPassLaneHalfWidth = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Safety")
		float AIAerialHeaderMinimumTeammateScore = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Defensive", meta = (ClampMin = "0.0"))
		float AIAerialDefensivePressureDistance = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Defensive", meta = (ClampMin = "0.0"))
		float AIAerialDefensiveClearanceOwnGoalDistance = 2600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Target", meta = (ClampMin = "100.0"))
		float AIAerialOffensiveProlongDistance = 1300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Target", meta = (ClampMin = "100.0"))
		float AIAerialDefensiveClearanceForwardDistance = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Target", meta = (ClampMin = "100.0"))
		float AIAerialDefensiveClearanceLateralDistance = 1650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Target", meta = (ClampMin = "0.0"))
		float AIAerialHeaderTargetFieldInset = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Power", meta = (ClampMin = "0.0"))
		float AIAerialHeaderShotHorizontalSpeed = 1600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Shot Error")
		bool bUseAIAerialHeaderShotExecutionError = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Shot Error", meta = (ClampMin = "0.0", ClampMax = "20.0"))
		float AIAerialHeaderShotDirectionErrorDegrees = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Shot Error", meta = (ClampMin = "0.0", ClampMax = "0.50"))
		float AIAerialHeaderShotPowerErrorFraction = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Power", meta = (ClampMin = "0.0"))
		float AIAerialHeaderPassHorizontalSpeed = 1150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Pass Error")
		bool bUseAIAerialHeaderPassExecutionError = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Pass Error", meta = (ClampMin = "0.0", ClampMax = "20.0"))
		float AIAerialHeaderPassDirectionErrorDegrees = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Pass Error", meta = (ClampMin = "0.0", ClampMax = "0.50"))
		float AIAerialHeaderPassPowerErrorFraction = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Power", meta = (ClampMin = "0.0"))
		float AIAerialHeaderProlongHorizontalSpeed = 1350.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Power", meta = (ClampMin = "0.0"))
		float AIAerialDefensivePassHorizontalSpeed = 950.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Decision|Power", meta = (ClampMin = "0.0"))
		float AIAerialDefensiveClearanceHorizontalSpeed = 1450.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial", meta = (ClampMin = "0.0"))
		float AIAerialMoveRepathMinInterval = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial", meta = (ClampMin = "0.0"))
		float AIAerialMoveRepathDistance = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial", meta = (ClampMin = "0.02"))
		float AIAerialMoveForcedRefreshInterval = 0.30f;

	// ============================================================
	// AERIAL CONTEST COORDINATION - STAGE 5
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination")
		bool bUseAIAerialContestCoordination = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Scoring", meta = (ClampMin = "0.0"))
		float AIAerialContestBallArrivalWeight = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Scoring", meta = (ClampMin = "0.0"))
		float AIAerialContestPlayerArrivalWeight = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Scoring", meta = (ClampMin = "0.0"))
		float AIAerialContestLateArrivalPenaltyWeight = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Scoring", meta = (ClampMin = "0.0"))
		float AIAerialContestTimingErrorWeight = 0.20f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Scoring", meta = (ClampMin = "0.0"))
		float AIAerialContestPositiveMarginBonusWeight = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Scoring", meta = (ClampMin = "0.0"))
		float AIAerialContestApproachingCommitmentBonus = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Scoring", meta = (ClampMin = "0.0"))
		float AIAerialContestWaitingCommitmentBonus = 0.24f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Scoring", meta = (ClampMin = "0.0"))
		float AIAerialContestPlayingCommitmentBonus = 0.60f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Scoring", meta = (ClampMin = "0.0"))
		float AIAerialContestHumanRequestBonus = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Second Ball", meta = (ClampMin = "0.0"))
		float AIAerialContestSecondBallForwardDistance = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Second Ball", meta = (ClampMin = "0.0"))
		float AIAerialContestSecondBallLateralDistance = 330.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Second Ball", meta = (ClampMin = "1.0"))
		float AIAerialContestSecondBallAcceptanceRadius = 70.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Second Ball", meta = (ClampMin = "0.0"))
		float AIAerialContestSecondBallRepathMinInterval = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Second Ball", meta = (ClampMin = "0.0"))
		float AIAerialContestSecondBallRepathDistance = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Contest Coordination|Second Ball", meta = (ClampMin = "0.02"))
		float AIAerialContestSecondBallForcedRefreshInterval = 0.35f;


	// ============================================================
	// AERIAL POST-CONTACT RECOVERY - STAGE 6
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Post Contact Recovery")
		bool bUseAIAerialPostContactRecovery = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Post Contact Recovery", meta = (ClampMin = "0.0"))
		float AIAerialPostContactReactionWindow = 0.80f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Post Contact Recovery", meta = (ClampMin = "0.05"))
		float AIAerialPostContactPredictionTime = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Post Contact Recovery", meta = (ClampMin = "0.05"))
		float AIAerialPostContactPoorQualityPredictionTime = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Post Contact Recovery", meta = (ClampMin = "0.0"))
		float AIAerialPostContactPoorQualityThreshold = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Post Contact Recovery", meta = (ClampMin = "0.0"))
		float AIAerialPostContactSupportLateralOffset = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Aerial|Post Contact Recovery", meta = (ClampMin = "0.0"))
		float AIAerialPostContactSupportForwardBias = 0.20f;


	TWeakObjectPtr<ASoccerBall> AerialContestCoordinationBall;
	TWeakObjectPtr<ASoccerCharacterBase> AerialContestPrimaryCharacter;
	bool bAerialContestSecondarySupportActive = false;
	FVector LastAerialContestSecondBallTarget = FVector::ZeroVector;
	float LastAerialContestSecondBallMoveRequestTime = -1000.0f;

	bool bHasDelayedBallObservation = false;

	bool bHasPredictiveBallMoveTarget = false;
	FVector LastPredictiveBallMoveTarget = FVector::ZeroVector;
	float LastPredictiveBallMoveRequestTime = -1000.0f;
	ESoccerAIOrder LastPredictiveBallMoveOrder = ESoccerAIOrder::ReturnHome;

	bool bAerialDebugIsolationEnabled = false;
	TWeakObjectPtr<ASoccerBall> AerialDebugIsolationBall;

	bool bHasAerialBallMoveTarget = false;
	FVector LastAerialBallMoveTarget = FVector::ZeroVector;
	float LastAerialBallMoveRequestTime = -1000.0f;
	ESoccerAIOrder LastAerialBallMoveOrder = ESoccerAIOrder::ReturnHome;

	FVector DelayedObservedBallLocation = FVector::ZeroVector;

	float LastDelayedBallObservationTime = -1000.0f;

	float CurrentAIReactionDelay = 0.0f;

	bool IsBallShieldedByPossessor(
		const ASoccerAICharacter* StealingCharacter,
		const ASoccerCharacterBase* PossessingCharacter,
		const ASoccerBall* SoccerBall
	) const;

	bool CanStealPossessedBall(
		const ASoccerAICharacter* StealingCharacter,
		const ASoccerCharacterBase* PossessingCharacter,
		const ASoccerBall* SoccerBall
	) const;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Steal")
		float AIBallShieldRadius = 95.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Steal")
		float AIBallShieldBehindDotThreshold = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Steal")
		float AIBallStealReachAdvantage = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Steal")
		float AIBallStealBehindBlockDot = -0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Steal")
		float AIBallStealFromHumanDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Steal")
		float AIBallStealFromHumanChance = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Steal")
		float AIBallStealFromHumanCooldown = 1.0f;

	// Breve gracia para evitar el ping-pong instantaneo al cambiar la
	// posesion. Luego de este lapso, la salida bajo presion puede ser robada.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Steal")
		float AIPossessionProtectionTime = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Steal")
		float AILostBallStealRecoveryTime = 1.1f;

	float LastHumanStealAttemptTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot")
		float AIShotDistanceToTarget = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot")
		float AIShotHorizontalSpeed = 2200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot")
		float AIShotMinTravelTime = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot")
		float AIShotMaxTravelTime = 2.2f;

	FVector BuildAIShotTargetLocation(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	FVector ApplyAIShotExecutionError(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& IntendedShotTargetLocation
	) const;

	// Smart goal-sector selection. All target locations are expressed as
	// fractions of SoccerFieldDimensions so changing the physical goal keeps
	// the shooting model aligned with the stadium geometry.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection", meta = (ClampMin = "0.0", ClampMax = "0.95"))
		float AIShotSectorLateralFraction = 0.84f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection", meta = (ClampMin = "0.05", ClampMax = "0.90"))
		float AIShotBottomHeightFraction = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection", meta = (ClampMin = "0.05", ClampMax = "0.90"))
		float AIShotCenterHeightFraction = 0.43f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection", meta = (ClampMin = "0.10", ClampMax = "0.95"))
		float AIShotTopHeightFraction = 0.76f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection")
		float AIShotGoalkeeperClearanceDistance = 175.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection")
		float AIShotGoalkeeperClearanceWeight = 1.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection")
		float AIShotGoalkeeperGoalSeparationWeight = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection")
		float AIShotDefenderClearanceDistance = 145.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection")
		float AIShotDefenderClearanceWeight = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection")
		float AIShotFarPostPreferenceWeight = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection")
		float AIShotDistancePrecisionPenalty = 0.22f;

	// Score noise keeps two similarly good sectors from producing the exact
	// same decision every time. It changes selection, not the field geometry.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection", meta = (ClampMin = "0.0"))
		float AIShotSectorScoreRandomness = 0.18f;

	// Small final placement error after a sector is selected. The target is
	// clamped back inside the real goal mouth so this stage improves choice
	// without yet introducing deliberate off-target misses.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection", meta = (ClampMin = "0.0", ClampMax = "0.20"))
		float AIShotAimErrorLateralFraction = 0.035f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Target Selection", meta = (ClampMin = "0.0", ClampMax = "0.20"))
		float AIShotAimErrorVerticalFraction = 0.040f;

	// Stage 14: execution error is applied after the intelligent sector and
	// intended placement have already been chosen. Unlike the small placement
	// variation above, this error is NOT clamped to the goal mouth, so difficult
	// shots can naturally finish wide or over the crossbar.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error")
		bool bUseAIShotExecutionError = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error", meta = (ClampMin = "0.0"))
		float AIShotExecutionErrorScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error", meta = (ClampMin = "0.0"))
		float AIShotExecutionBaseErrorCm = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error", meta = (ClampMin = "0.0"))
		float AIShotExecutionDistanceAdditionalErrorCm = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error", meta = (ClampMin = "0.0"))
		float AIShotExecutionPressureAdditionalErrorCm = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error", meta = (ClampMin = "0.0"))
		float AIShotExecutionWideAngleAdditionalErrorCm = 22.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error", meta = (ClampMin = "0.0"))
		float AIShotExecutionDifficultTargetAdditionalErrorCm = 18.0f;

	// Inside this distance the shooter is considered fully pressured. Between
	// FullDistance and MaxDistance pressure fades progressively to zero.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error", meta = (ClampMin = "0.0"))
		float AIShotExecutionPressureFullDistanceCm = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error", meta = (ClampMin = "1.0"))
		float AIShotExecutionPressureMaxDistanceCm = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error", meta = (ClampMin = "0.0", ClampMax = "2.0"))
		float AIShotExecutionVerticalErrorScale = 0.80f;

	// Prevents the mathematical target from going below the grass. There is no
	// upper or lateral clamp: those are the directions that create real misses.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Execution Error", meta = (ClampMin = "0.0", ClampMax = "0.25"))
		float AIShotExecutionMinimumHeightFraction = 0.03f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Shot|Debug")
		bool bDebugAIShotSectorSelection = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperTurnSpeed = 8.0f;

	UPROPERTY()
		ASoccerMatchManager* MatchManager = nullptr;

	bool UpdateInitialPossessionEscape(
		ASoccerAICharacter* SoccerCharacter,
		float DeltaTime
	);

	bool TryStartInitialPossessionEscape(
		ASoccerAICharacter* SoccerCharacter
	);

	ASoccerCharacterBase* FindClosestOpponentPressure(
		const ASoccerAICharacter* SoccerCharacter,
		float MaxDistance
	) const;

	FVector BuildInitialEscapeLocation(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerCharacterBase* ClosestOpponent
	) const;

	void ClearInitialPossessionEscape();

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Escape")
		bool bUseInitialPossessionEscape = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Escape")
		float InitialPossessionEscapePressureRadius = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Escape")
		float InitialPossessionEscapeDistance = 480.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Escape")
		float InitialPossessionEscapeDuration = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Escape")
		float InitialPossessionEscapeAcceptanceRadius = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Escape")
		float InitialPossessionEscapeSideWeight = 0.55f;

	bool bInitialPossessionEscapeActive = false;

	bool bInitialPossessionEscapeStartedForCurrentPossession = false;

	FVector InitialPossessionEscapeLocation = FVector::ZeroVector;

	float InitialPossessionEscapeStartTime = 0.0f;

	bool UpdateAIAutoPassFollow(
		ASoccerAICharacter* SoccerCharacter
	);

	bool TryStartAIAutoPass(
		ASoccerAICharacter* SoccerCharacter,
		bool* bOutRejectedByLocalSafety = nullptr
	);

	FVector BuildAIAutoPassTargetLocation(
		const ASoccerAICharacter* SoccerCharacter,
		bool* bOutUsedLocalDribbleDirection = nullptr,
		bool* bOutRejectedByLocalSafety = nullptr
	) const;

	/*
	 * The 15 x 10 grid is only a local direction probe. It never becomes
	 * the real auto-pass target: the existing auto-pass distance, speed,
	 * turn montage and follow logic remain authoritative.
	 */
	bool FindBestLocalAIDribbleDirection(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& PreferredDirection,
		bool bConservativeMode,
		FVector& OutBestDirection,
		bool& bOutDirectionIsSafe
	) const;

	float EstimateLocalAIDribbleBallArrivalTime(
		const FVector& BallLocation,
		const FVector& EvaluationLocation
	) const;

	bool IsAIAutoPassStillValid(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall
	) const;

	FVector BuildAIAutoPassFollowMoveLocation(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall
	) const;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		bool bUseAIAutoPass = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassDistance = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassHorizontalSpeed = 1150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassMinTravelTime = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassMaxTravelTime = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassCollectDistance = 65.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassFollowAcceptanceRadius = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		bool bUseAIAutoPassIntentFollow = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassPathTolerance = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassMaxFollowTime = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassCancelIfBallBehindDistance = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass")
		float AIAutoPassFollowTargetLeadDistance = 180.0f;

	// Local 15 x 10 (4 m x 4 m) direction selector. The grid only chooses
	// a direction; StartAIAutoPassToLocation still determines the real touch.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass|Local Dribble Direction")
		bool bUseAILocalDribbleDirection = true;

	// Only characters capable of influencing the surrounding 3 x 3 cells
	// are considered in the ETA comparison.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass|Local Dribble Direction", meta = (ClampMin = "400.0"))
		float AILocalDribbleNearbyPlayerRadius = 1400.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass|Local Dribble Direction", meta = (ClampMin = "0.0"))
		float AILocalDribbleArrivalReachRadius = 70.0f;

	// Corridor used only to reject a direction whose immediate ball path is
	// occupied by another player. It does not replace the auto-pass trajectory.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass|Local Dribble Direction", meta = (ClampMin = "0.0"))
		float AILocalDribbleLaneHalfWidth = 115.0f;

	// Minimum advantage required over the fastest nearby opponent at the
	// evaluation point. Values are seconds.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass|Local Dribble Direction", meta = (ClampMin = "0.0"))
		float AILocalDribbleMinimumSafetyMargin = 0.12f;

	// A teammate that can occupy the same probe almost as early as the carrier
	// makes that direction locally congested, even though possession is safe.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass|Local Dribble Direction", meta = (ClampMin = "0.0"))
		float AILocalDribbleTeammateCongestionTime = 0.18f;

	// When team discipline asks the carrier to be conservative, local dribble
	// may still unstick him, but it cannot choose a strongly forward direction.
	UPROPERTY(EditAnywhere, Category = "Soccer|AI Auto Pass|Local Dribble Direction", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
		float AILocalDribbleConservativeMaxAlignment = 0.25f;

	bool TryPrepareMainActionIfBlocked(
		ASoccerAICharacter* SoccerCharacter,
		ESoccerAIPendingMainAction MainAction,
		const FVector& MainActionTargetLocation,
		ASoccerCharacterBase* OptionalTargetCharacter
	);

	bool TryExecutePendingMainActionAfterPreparation(
		ASoccerAICharacter* SoccerCharacter
	);

	void ClearPendingMainActionAfterPreparation();

	bool IsMainActionLineBlockedByOpponent(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& FromLocation,
		const FVector& TargetLocation,
		float BlockRadius
	) const;

	FVector BuildLateralPreparationTouchTargetLocation(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& MainActionTargetLocation
	) const;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Preparation")
		bool bUseAIMainActionPreparation = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Preparation")
		float AIMainActionPreparationBlockRadius = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Preparation")
		float AIMainActionPreparationMinBlockDistance = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Preparation")
		float AIMainActionPreparationMaxBlockDistance = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Preparation")
		float AIMainActionPreparationTouchDistance = 230.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Preparation")
		float AIMainActionPreparationHorizontalSpeed = 950.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Preparation")
		float AIMainActionPreparationMinTravelTime = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Preparation")
		float AIMainActionPreparationMaxTravelTime = 0.70f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Preparation")
		float AIMainActionPreparationTimeout = 2.0f;

	ESoccerAIPendingMainAction PendingMainActionAfterPreparation =
		ESoccerAIPendingMainAction::None;

	FVector PendingMainActionTargetLocation = FVector::ZeroVector;

	TWeakObjectPtr<ASoccerCharacterBase> PendingMainActionTargetCharacter;

	float PendingMainActionStartTime = -1000.0f;

	void UpdateAIPossessionStuckTracking(
		ASoccerAICharacter* SoccerCharacter
	);

	bool TryForceAIPossessionActionIfStuck(
		ASoccerAICharacter* SoccerCharacter
	);

	void ClearAIPossessionStuckTracking();

	FVector BuildAIStuckAutoPassTargetLocation(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Failsafe")
		bool bUseAIPossessionFailsafe = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Failsafe")
		float AIPossessionStuckSeconds = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Failsafe")
		float AIPossessionStuckMinSpeed = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Failsafe")
		float AIPossessionStuckForceCooldown = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Failsafe")
		float AIPossessionStuckAutoPassDistance = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Failsafe")
		float AIPossessionStuckAutoPassHorizontalSpeed = 1050.0f;

	bool bAIPossessionStuckTrackingActive = false;

	float LastAIPossessionMovementTime = -1000.0f;

	float LastAIPossessionForcedActionTime = -1000.0f;

	bool TryPassToTeammateIfReady(
		ASoccerAICharacter* SoccerCharacter
	);

	ASoccerCharacterBase* FindSimplePassTeammate(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	FVector BuildSimplePassTargetLocation(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerCharacterBase* Teammate
	) const;

	int32 GetFieldZoneIndex(ESoccerFieldZone Zone) const;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		bool bUseAISimpleTeammatePass = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		float AISimplePassMinDistance = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		float AISimplePassMaxDistance = 2600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		float AISimplePassHorizontalSpeed = 1600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		float AISimplePassMinTravelTime = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		float AISimplePassMaxTravelTime = 1.8f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		float AISimplePassForwardPreferenceWeight = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		float AISimplePassTargetForwardLead = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		int32 AIBetterPassMinZoneAdvantage = 1;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		float AISimplePassBetterZoneWeight = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Pass")
		float AISimplePassHumanTestBonus = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Smart Pass")
		float SmartAttackPassToFeetHorizontalSpeed = 950.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Smart Pass")
		float SmartAttackPassToSpaceHorizontalSpeed = 1150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Smart Pass")
		float SmartAttackPassMinTravelTime = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Smart Pass")
		float SmartAttackPassMaxTravelTime = 0.85f;

	void CreateOrUpdateRecoveryIntent(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	bool TryExecuteCurrentRecoveryIntent(
		ASoccerAICharacter* SoccerCharacter
	);

	void ClearCurrentRecoveryIntent();

	bool IsCurrentRecoveryIntentExpired() const;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Intent")
		bool bUseAIRecoveryIntent = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Intent")
		float AIRecoveryIntentLifetime = 1.50f;

	ESoccerAIIntent CurrentRecoveryIntent = ESoccerAIIntent::None;

	ESoccerAIPendingMainAction RecoveryIntentPlannedAction =
		ESoccerAIPendingMainAction::None;

	TWeakObjectPtr<ASoccerCharacterBase> RecoveryIntentTargetCharacter;

	FVector RecoveryIntentTargetLocation = FVector::ZeroVector;

	float RecoveryIntentCreatedTime = -1000.0f;


	UPROPERTY(EditAnywhere, Category = "Soccer|AI Debug")
		bool bDebugOnlyPrintRelevantAISituation = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Debug")
		float DebugAISituationInterval = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Debug")
		float DebugAISituationDuration = 2.0f;

	float LastAISituationDebugPrintTime = -1000.0f;

	void DebugPrintAISituation(
		ASoccerAICharacter* SoccerCharacter,
		ESoccerAIOrder CurrentOrder
	);

	FString GetDebugFieldZoneText(ESoccerFieldZone Zone) const;

	FString GetDebugBallSituationText(ESoccerBallSituation Situation) const;

	FString GetDebugAIOrderText(ESoccerAIOrder Order) const;

	FString GetDebugTeamText(ESoccerTeam Team) const;

	FString GetDebugRoleText(ESoccerPlayerRole PlayerRole) const;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement")
		float AttackMoveRepathDistanceThreshold = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement")
		float AttackMoveRepathMinInterval = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement")
		float AttackMoveForcedRefreshInterval = 1.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Movement")
		float AttackMoveForcedRefreshJitter = 0.30f;

	float BuildRandomAttackMoveRefreshInterval() const;

	FVector LastFilteredMoveTarget = FVector::ZeroVector;

	float LastFilteredMoveRequestTime = -1000.0f;

	bool bHasLastFilteredMoveTarget = false;

	ESoccerAIOrder LastFilteredMoveOrder = ESoccerAIOrder::ReturnHome;

	float CurrentAttackMoveForcedRefreshInterval = 0.0f;

	void ClearFilteredMoveRequest();

	bool MoveToLocationFilteredForAttack(
		const FVector& TargetLocation,
		float AcceptanceRadius,
		ESoccerAIOrder CurrentOrder
	);

	bool TrySmartAttackPass(
		ASoccerAICharacter* SoccerCharacter,
		bool bUsePossessionRetentionThreshold = false
	);

	void ClearFilteredDefenseMoveRequest();

	bool MoveToLocationFilteredForDefense(
		const FVector& TargetLocation,
		float AcceptanceRadius,
		ESoccerAIOrder CurrentOrder,
		bool bGoalAreaEmergency
	);

	float BuildRandomDefenseMoveRefreshInterval() const;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defense Movement")
		float DefenseMoveRepathDistanceThreshold = 130.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defense Movement")
		float DefenseMoveRepathMinInterval = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defense Movement")
		float DefenseMoveForcedRefreshIntervalMin = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defense Movement")
		float DefenseMoveForcedRefreshIntervalMax = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defense Movement|Goal Area Emergency", meta = (ClampMin = "1.0"))
		float GoalAreaDefenseMoveRepathDistanceThreshold = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defense Movement|Goal Area Emergency", meta = (ClampMin = "0.0"))
		float GoalAreaDefenseMoveRepathMinInterval = 0.06f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defense Movement|Goal Area Emergency", meta = (ClampMin = "0.01"))
		float GoalAreaDefenseMoveForcedRefreshInterval = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defense Movement|Goal Area Emergency", meta = (ClampMin = "1.0"))
		float GoalAreaDefenseMoveAcceptanceRadius = 35.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Defense Movement|Goal Area Emergency", meta = (ClampMin = "0.0"))
		float GoalAreaDefenseFastRunDistance = 520.0f;

	FVector LastFilteredDefenseMoveTarget = FVector::ZeroVector;

	float LastFilteredDefenseMoveRequestTime = -1000.0f;

	bool bHasLastFilteredDefenseMoveTarget = false;

	ESoccerAIOrder LastFilteredDefenseMoveOrder = ESoccerAIOrder::ReturnHome;

	float CurrentDefenseMoveForcedRefreshInterval = 0.0f;

	void UpdateAIMovementForMove(
		ESoccerAIOrder CurrentOrder,
		const FVector& MoveLocation,
		bool bHasMoveLocation
	);

	void MoveToLocationWithAIMovement(
		ESoccerAIOrder CurrentOrder,
		const FVector& MoveLocation,
		float AcceptanceRadius,
		bool bStopOnOverlap
	);

	void MoveToActorWithAIMovement(
		ESoccerAIOrder CurrentOrder,
		AActor* TargetActor,
		float AcceptanceRadius,
		bool bStopOnOverlap = true
	);

	/*
	 * Si una animación o un desplazamiento físico deja al personaje fuera
	 * del NavMesh, un MoveTo normal puede fallar porque no existe un
	 * polígono de navegación válido desde la posición inicial.
	 *
	 * Esta recuperación mueve temporalmente la cápsula hacia el punto
	 * navegable más cercano y, cuando vuelve a entrar, permite que el
	 * path following continúe normalmente.
	 */
	bool TryRecoverPawnToNavigation(
		ESoccerAIOrder CurrentOrder,
		const FVector& DesiredMoveLocation
	);

	void ClearNavigationRecoveryState();

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Navigation Recovery")
		bool bEnableNavigationRecovery = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Navigation Recovery")
		float NavigationRecoverySearchExtentXY = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Navigation Recovery")
		float NavigationRecoverySearchExtentZ = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Navigation Recovery")
		float NavigationRecoveryOnMeshTolerance = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Navigation Recovery")
		float NavigationRecoveryManualMoveSpeed = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Navigation Recovery")
		float NavigationRecoveryEmergencyTeleportDelay = 1.50f;

	UPROPERTY(EditAnywhere, Category = "Soccer|AI Navigation Recovery")
		float NavigationRecoveryEmergencyTeleportMaxDistance = 650.0f;


	bool bNavigationRecoveryActive = false;

	FVector NavigationRecoveryTarget = FVector::ZeroVector;

	float NavigationRecoveryStartTime = -1000.0f;

	float NavigationRecoveryLastUpdateTime = -1000.0f;

	// Goalkeeper

	bool UpdateGoalkeeperBehavior(
		ASoccerAICharacter* SoccerCharacter,
		float DeltaTime
	);

	bool TryStartGoalkeeperSaveForIncomingShot(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	// Si el selector solo dispone de una EmergencySelection y todavia falta
	// tiempo para iniciar el montage, corre lateralmente hacia el lado donde
	// se encuentra el punto previsto de contacto. El selector se reevalua cada
	// Tick, por lo que esta carrera puede convertir la emergencia en alcanzable.
	bool TryUpdateGoalkeeperEmergencySaveLateralRun(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		float DeltaTime
	);

	// Capa previa a la atajada: si una pelota suelta dentro del propio
	// medio campo apunta, por su velocidad horizontal actual, hacia la boca
	// del arco, el arquero se alinea lateralmente con ese cruce futuro.
	// No inicia ninguna animacion ni reemplaza Save/Sweeper/Retreat.
	bool TryUpdateGoalkeeperTrajectoryPrepositioning(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		float DeltaTime
	);

	bool TryBuildGoalkeeperTrajectoryPrepositionTarget(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		FVector& OutMoveTarget,
		FVector& OutGoalLineCrossing
	) const;

	void ApplyGoalkeeperSaveEffect(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerGoalkeeperAction GoalkeeperAction,
		const FGoalkeeperAnimatedContactResult& ContactResult
	);

	void ApplyGoalkeeperCatchEffect(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	void ApplyGoalkeeperDeflectEffect(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	void ApplyGoalkeeperBodyReboundEffect(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	bool IsGoalkeeperCatchAction(
		ESoccerGoalkeeperAction GoalkeeperAction
	) const;

	bool IsGoalkeeperDeflectAction(
		ESoccerGoalkeeperAction GoalkeeperAction
	) const;

	bool TryGoalkeeperClearCaughtBall(
		ASoccerAICharacter* SoccerCharacter
	);

	bool TryBuildGoalkeeperDistributionPlan(
		const ASoccerAICharacter* SoccerCharacter,
		FGoalkeeperDistributionPlan& OutPlan
	) const;

	ESoccerGoalkeeperDistributionType
		SelectGoalkeeperDistributionType(
			bool bUrgentDistribution,
			bool bHasSafeTeammatePass,
			bool& bOutForcedByDebug
		) const;

	FVector BuildGoalkeeperShortFallbackTargetLocation(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	FString GetGoalkeeperDistributionTypeText(
		ESoccerGoalkeeperDistributionType DistributionType
	) const;

	bool UpdateGoalkeeperDistribution(
		ASoccerAICharacter* SoccerCharacter,
		float DeltaTime
	);

	UCurveTable* GetGoalkeeperDistributionMotionCurveTable(
		ESoccerGoalkeeperDistributionType DistributionType
	) const;

	void GetGoalkeeperDistributionMotionScales(
		ESoccerGoalkeeperDistributionType DistributionType,
		float& OutForwardScale,
		float& OutLateralScale
	) const;

	bool EvaluateGoalkeeperDistributionLocalDisplacement(
		ESoccerGoalkeeperDistributionType DistributionType,
		float MontageTime,
		FVector2D& OutLocalDisplacement
	) const;

	bool InitializeGoalkeeperDistributionCurveMotion(
		ASoccerAICharacter* SoccerCharacter
	);

	bool ApplyGoalkeeperDistributionCurveMotionAtTime(
		ASoccerAICharacter* SoccerCharacter,
		float MontageTime
	);

	bool UpdateGoalkeeperDistributionCurveMotion(
		ASoccerAICharacter* SoccerCharacter
	);

	bool CommitGoalkeeperDistributionCurveMotionToEnd(
		ASoccerAICharacter* SoccerCharacter
	);

	bool HasActiveGoalkeeperDistributionFor(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	void ClearGoalkeeperDistribution();

	bool TryBuildGoalkeeperDistributionTarget(
		const ASoccerAICharacter* SoccerCharacter,
		bool bUseSweeperClearance,
		FVector& OutTargetLocation,
		float& OutHorizontalSpeed,
		float& OutMinTravelTime,
		float& OutMaxTravelTime,
		FString& OutDebugLabel
	) const;

	ASoccerCharacterBase* FindBestGoalkeeperDistributionTeammate(
		const ASoccerAICharacter* SoccerCharacter,
		FVector& OutTargetLocation,
		float& OutScore
	) const;

	float ScoreGoalkeeperDistributionTeammate(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerCharacterBase* Teammate,
		const FVector& TargetLocation
	) const;

	bool IsGoalkeeperDistributionLaneBlockedByOpponent(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& FromLocation,
		const FVector& TargetLocation
	) const;

	int32 CountGoalkeeperDistributionOpponentsAroundLocation(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& Location,
		float Radius
	) const;

	bool ShouldGoalkeeperUseUrgentDistribution(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	FVector BuildGoalkeeperLongClearanceTargetLocation(
		const ASoccerAICharacter* SoccerCharacter,
		bool bUseSweeperClearance
	) const;

	void PreparePendingGoalkeeperSaveAction(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerGoalkeeperAction GoalkeeperAction,
		const FVector& PredictedInterventionLocation,
		float TimeToIntervention,
		float ContactWindowStartTime,
		bool bEmergencySelection
	);

	bool UpdatePendingGoalkeeperSaveAction(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		float DeltaTime
	);

	void ClearPendingGoalkeeperSaveAction();

	bool HasPendingGoalkeeperSaveActionFor(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	void PreparePendingGoalkeeperSaveImpact(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerGoalkeeperAction GoalkeeperAction
	);

	void ClearPendingGoalkeeperSaveImpact();

	bool HasPendingGoalkeeperSaveImpactFor(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	bool TryExecutePendingGoalkeeperSaveImpact(
		ASoccerAICharacter* SoccerCharacter
	);

	bool TryExecuteGoalkeeperEmergencyBodyContact(
		ASoccerAICharacter* SoccerCharacter
	);

	bool DetectGoalkeeperAnimatedContact(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		FGoalkeeperAnimatedContactResult& OutContactResult
	);

	void ResetGoalkeeperBallContactTracking();

	ESoccerGoalkeeperAction
		ChooseGoalkeeperActionLegacy(
			const ASoccerAICharacter* SoccerCharacter,
			const FVector& PredictedContactLocation,
			bool bCanUseHands
		) const;

	bool TryGetGoalkeeperContactWindowRangeForAction(
		const ASoccerAICharacter* SoccerCharacter,
		ESoccerGoalkeeperAction GoalkeeperAction,
		float& OutContactWindowStartTime,
		float& OutContactWindowEndTime
	) const;

	UCurveTable*
		GetGoalkeeperSaveHandTrackCurveTable(
			ESoccerGoalkeeperAction GoalkeeperAction
		) const;

	bool EvaluateGoalkeeperSaveHandTrack(
		ESoccerGoalkeeperAction GoalkeeperAction,
		float MontageTime,
		FVector& OutLeftHandLocalPosition,
		FVector& OutRightHandLocalPosition
	) const;

	bool IsGoalkeeperActionToLeft(
		ESoccerGoalkeeperAction GoalkeeperAction
	) const;

	bool IsGoalkeeperActionToRight(
		ESoccerGoalkeeperAction GoalkeeperAction
	) const;

	bool IsGoalkeeperHandlingAllowedAtLocation(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& Location
	) const;

	bool IsLocationInsideGoalkeeperPenaltyArea(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& Location
	) const;

	bool IsLocationInsideGoalkeeperGoalArea(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& Location,
		float ExtraMargin = 0.0f
	) const;

	bool IsGoalkeeperGoalAreaAuthorityActiveAtLocation(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& Location
	) const;

	float GetGoalkeeperRequiredShotMinSpeed(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& BallLocation
	) const;

	bool TryUpdateGoalkeeperSweeperBehavior(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		float DeltaTime
	);

	bool ShouldStartOrKeepGoalkeeperSweeping(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		FVector& OutSweepTargetLocation
	);

	ASoccerCharacterBase* FindGoalkeeperBreakawayThreat(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall
	) const;

	bool IsGoalkeeperThreatInsideSweeperTriggerZone(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& ThreatLocation
	) const;

	bool HasDefensiveHelpAgainstGoalkeeperThreat(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerCharacterBase* ThreatCharacter,
		const FVector& ThreatLocation
	) const;

	bool IsGoalkeeperThreatDrivingTowardGoal(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerCharacterBase* ThreatCharacter
	) const;

	bool CanGoalkeeperWinSweeperRace(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		const ASoccerCharacterBase* ThreatCharacter,
		const FVector& SweepTargetLocation
	) const;

	float EstimateGoalkeeperGroundTravelTime(
		const FVector& FromLocation,
		const FVector& ToLocation
	) const;

	float EstimateThreatGroundTravelTime(
		const FVector& FromLocation,
		const FVector& ToLocation
	) const;

	float EstimateClosestDefenderGroundTravelTime(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& TargetLocation
	) const;

	FVector PredictGoalkeeperSweeperBallLocation(
		const ASoccerBall* SoccerBall
	) const;

	FVector BuildGoalkeeperSweepTargetLocation(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		const ASoccerCharacterBase* ThreatCharacter
	) const;

	bool TryResolveGoalkeeperSweeperBallContact(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	bool TryStartGoalkeeperSweeperSmotherAction(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ASoccerCharacterBase* ThreatCharacter
	);

	ESoccerGoalkeeperAction ChooseGoalkeeperSweeperSmotherAction(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		const ASoccerCharacterBase* ThreatCharacter,
		bool bCanUseHands
	) const;

	bool IsGoalkeeperBallLooseEnoughForHandSmother(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		const ASoccerCharacterBase* ThreatCharacter
	) const;

	void PreparePendingGoalkeeperSweeperSaveImpact(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerGoalkeeperAction GoalkeeperAction
	);

	void ReleaseGoalkeeperOpponentBallPossession(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	bool TryGoalkeeperCollectLooseBallWithHands(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	bool TryGoalkeeperClearBallWithFeet(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	bool IsGoalkeeperAtFootClearanceContact(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall
	) const;

	bool TryGoalkeeperTakeBallForFootClearance(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	void StartGoalkeeperSweeperMode(
		ASoccerCharacterBase* ThreatCharacter,
		const FVector& SweepTargetLocation
	);

	void ClearGoalkeeperSweeperMode();

	bool TryUpdateGoalkeeperRetreatBehavior(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		float DeltaTime
	);

	bool ShouldGoalkeeperRetreatToHome(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	void ClearGoalkeeperRetreatMode();

	void MarkGoalkeeperSweeperResolved();

	void SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode NewMode
	);

	FVector GetGoalkeeperHomeLocation(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	FVector GetGoalkeeperRetreatTargetLocation(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	FVector GetGoalkeeperOutfieldDirection(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	FVector GetGoalkeeperRightDirection(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	float GetGoalkeeperDepthFromGoal(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& Location
	) const;

	float GetGoalkeeperLateralOffsetFromGoal(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& Location
	) const;

	// Prueba controlada de atajadas.
	bool bGoalkeeperDebugSaveTestActive = false;

	bool bGoalkeeperDebugSaveTestAllowEmergencyBodyContact =
		false;

	UPROPERTY()
		ASoccerAICharacter*
		GoalkeeperDebugSaveTestCharacter = nullptr;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Debug"
	)
		bool bForceGoalkeeperSaveAction = false;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Debug",
		meta = (
			EditCondition = "bForceGoalkeeperSaveAction",
			EditConditionHides
			)
	)
		ESoccerGoalkeeperAction ForcedGoalkeeperSaveAction =
		ESoccerGoalkeeperAction::CatchAbdomen;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		bool bUseGoalkeeperShotReaction = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperShotMinSpeed = 900.0f;

	/*
	 * Dentro del área chica el arquero asume autoridad prioritaria.
	 * Estas variables no le dan inmunidad ni garantizan la atajada:
	 * solamente evitan que una pelota lenta o un compañero cercano
	 * cancelen prematuramente una intervención urgente.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Goal Area Authority"
	)
		bool bUseGoalkeeperGoalAreaAuthority = true;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Goal Area Authority",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "250.0")
	)
		float GoalkeeperGoalAreaAuthorityMargin = 80.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Goal Area Authority",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1200.0")
	)
		float GoalkeeperGoalAreaShotMinSpeed = 450.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Goal Area Authority"
	)
		bool bGoalkeeperGoalAreaIgnoreOwnTeamLooseBallSuppression = true;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Goal Area Authority"
	)
		bool bGoalkeeperGoalAreaIgnoreDefensiveHelpSuppression = true;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Goal Area Authority",
		meta = (ClampMin = "-0.20", UIMin = "-0.20", UIMax = "0.20")
	)
		float GoalkeeperGoalAreaLooseBallClaimArrivalMargin = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperGoalHalfWidth = 300.0f;

	/*
	 * Margen de peligro fuera de cada poste.
	 * 100 cm = el arquero reacciona hasta un metro más
	 * allá del ancho reglado del arco, por cada lado.
	 */
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Save Selector|Decision",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "200.0"
			)
	)
		float GoalkeeperShotDangerSideMargin = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperCentralCatchHalfWidth = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperStandingCatchHalfWidth = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperLowBallMaxHeight = 75.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperCrotchCatchMaxHeight = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperChestCatchMaxHeight = 175.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperHighCatchMaxHeight = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperCatchBallForwardOffset = 85.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperCatchBallHeight = 95.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperDeflectForwardStrength = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperDeflectUpwardStrength = 180.0f;

	// ============================================================
	// GOALKEEPER SAVE TIMING DEBUG
	// ============================================================

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Timing Debug"
	)
		bool bDebugGoalkeeperSaveTiming = true;

	bool bGoalkeeperSaveTimingDebugHasData =
		false;

	ESoccerGoalkeeperAction
		GoalkeeperSaveTimingDebugAction =
		ESoccerGoalkeeperAction::None;

	FVector
		GoalkeeperSaveTimingDebugPredictedInterventionLocation =
		FVector::ZeroVector;

	float GoalkeeperSaveTimingDebugTimeToIntervention =
		0.0f;

	float GoalkeeperSaveTimingDebugSelectedContactTime =
		0.0f;

	float GoalkeeperSaveTimingDebugStartDelay =
		0.0f;

	float GoalkeeperSaveTimingDebugScheduledImpactWorldTime =
		-1000.0f;

	void RecordGoalkeeperSaveTimingDebug(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		ESoccerGoalkeeperAction GoalkeeperAction,
		const FVector& PredictedInterventionLocation,
		float TimeToIntervention,
		float SelectedContactTime,
		float TimeUntilMontageShouldStart
	);

	void UpdateGoalkeeperSaveTimingDebugPanel(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	// ============================================================
	// GOALKEEPER HAND TRACK SAVE SELECTOR
	// ============================================================

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector"
	)
		bool bFallbackToLegacyGoalkeeperSaveSelector =
		true;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector"
	)
		bool bDebugGoalkeeperHandTrackSelector =
		true;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector",
		meta = (
			ClampMin = "2",
			ClampMax = "11",
			UIMin = "2",
			UIMax = "11"
			)
	)
		int32 GoalkeeperHandTrackWindowSamples = 5;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector",
		meta = (
			ClampMin = "1.0",
			UIMin = "1.0",
			UIMax = "200.0"
			)
	)
		float GoalkeeperHandTrackMaxSelectionDistance =
		85.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "100.0"
			)
	)
		float GoalkeeperHandTrackDeflectPenalty =
		15.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "500.0"
			)
	)
		float GoalkeeperHandTrackWrongSidePenalty =
		180.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "100.0"
			)
	)
		float GoalkeeperHandTrackSideDeadZone =
		25.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector",
		meta = (
			ClampMin = "0.1",
			UIMin = "0.1",
			UIMax = "2.0"
			)
	)
		float GoalkeeperHandTrackScale = 1.0f;

	// ============================================================
	// GOALKEEPER ADAPTIVE LATERAL REACH
	// ============================================================
	// Amplia solamente el desplazamiento Lateral de la CurveTable de atajada.
	// No modifica Forward, no mueve la capsula en Z y no necesita curvas nuevas.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Adaptive Lateral"
	)
		bool bUseGoalkeeperAdaptiveLateralReach = true;

	// Limite inferior del factor lateral. 1.0 conserva el comportamiento de la
	// primera version (solo extender). Valores menores permiten acortar una
	// atajada cuando eso acerca las manos a la pelota. Nunca se permite invertir.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Adaptive Lateral",
		meta = (
			ClampMin = "0.0",
			ClampMax = "1.0",
			UIMin = "0.50",
			UIMax = "1.0"
			)
	)
		float GoalkeeperAdaptiveLateralMinimumScale = 1.0f;

	// 1.20 permite extender como maximo un 20% el traslado lateral original.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Adaptive Lateral",
		meta = (
			ClampMin = "1.0",
			ClampMax = "1.50",
			UIMin = "1.0",
			UIMax = "1.35"
			)
	)
		float GoalkeeperAdaptiveLateralMaximumScale = 1.20f;

	// Segundo limite: aunque el porcentaje lo permitiera, nunca agregamos mas
	// de esta distancia al desplazamiento lateral de la capsula.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Adaptive Lateral",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "100.0"
			)
	)
		float GoalkeeperAdaptiveLateralMaximumExtraDistance = 45.0f;

	// Evita intentar obtener grandes factores a partir de una curva que casi
	// no se esta desplazando lateralmente en ese instante del montage.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Adaptive Lateral",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "100.0"
			)
	)
		float GoalkeeperAdaptiveLateralMinimumCurveOffset = 12.0f;

	// No corregimos errores diminutos; conserva variacion natural y evita
	// microajustes diferentes entre muestras casi equivalentes.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Adaptive Lateral",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "50.0"
			)
	)
		float GoalkeeperAdaptiveLateralMinimumCorrectionDistance = 4.0f;

	// Costo de seleccion por cada centimetro extra. Hace que, si dos atajadas
	// llegan parecido, gane la que necesita menos ayuda adaptativa.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Adaptive Lateral",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "1.0"
			)
	)
		float GoalkeeperAdaptiveLateralExtraDistancePenalty = 0.15f;

	// ============================================================
	// GOALKEEPER NEAR-PERFECT SAVE ASSIST - STAGE 15B
	// ============================================================
	// El selector conserva las animaciones y curvas authored, pero puede aplicar
	// una pequena correccion procedural 3D en el instante de contacto. No hay
	// azar en esta etapa: el objetivo es construir primero un arquero fiable.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Near Perfect"
	)
		bool bUseGoalkeeperNearPerfectSaveAssist = true;

	// Correccion lateral residual maxima, despues del Adaptive Lateral.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Near Perfect",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "120.0"
			)
	)
		float GoalkeeperNearPerfectMaximumLateralCorrection = 55.0f;

	// Correccion vertical maxima del Mesh alrededor del contacto; la capsula no se hunde.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Near Perfect",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "100.0"
			)
	)
		float GoalkeeperNearPerfectMaximumVerticalCorrection = 45.0f;

	// Correccion Forward/Backward maxima. Se mantiene menor que la lateral para
	// no convertir una atajada en una traslacion artificial hacia la pelota.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Near Perfect",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "80.0"
			)
	)
		float GoalkeeperNearPerfectMaximumForwardCorrection = 28.0f;

	// Por debajo de este error dejamos intacta la animacion.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Near Perfect",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "20.0"
			)
	)
		float GoalkeeperNearPerfectMinimumCorrectionDistance = 1.5f;

	// Penaliza en el score las animaciones que necesitan mucha ayuda para que,
	// entre dos soluciones buenas, gane la mas natural. No limita fisicamente.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Near Perfect",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "1.0"
			)
	)
		float GoalkeeperNearPerfectCorrectionDistancePenalty = 0.08f;

	// Si el contacto authored llegaria apenas tarde, permitimos acelerar el
	// montage solamente cuando la diferencia temporal no supera este valor.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Near Perfect",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "0.25"
			)
	)
		float GoalkeeperNearPerfectMaximumLateTimingCorrection = 0.10f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Near Perfect",
		meta = (
			ClampMin = "1.0",
			ClampMax = "2.0",
			UIMin = "1.0",
			UIMax = "1.5"
			)
	)
		float GoalkeeperNearPerfectMaximumMontagePlayRate = 1.30f;

	// La correccion 3D entra y sale alrededor del contacto; no queda acumulada
	// al finalizar el montage.
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Near Perfect",
		meta = (
			ClampMin = "0.01",
			UIMin = "0.05",
			UIMax = "0.40"
			)
	)
		float GoalkeeperNearPerfectCorrectionBlendInTime = 0.18f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Near Perfect",
		meta = (
			ClampMin = "0.01",
			UIMin = "0.05",
			UIMax = "0.50"
			)
	)
		float GoalkeeperNearPerfectCorrectionReleaseTime = 0.22f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Curves"
	)
		TMap<
		ESoccerGoalkeeperAction,
		UCurveTable*
		> GoalkeeperSaveHandTrackCurveTables;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Impact"
	)
		float GoalkeeperBodyReboundForwardStrength = 520.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Impact"
	)
		float GoalkeeperBodyReboundUpwardStrength = 85.0f;

	// ============================================================
	// GOALKEEPER PER-ACTION CONTACT PLANE SELECTOR
	// ============================================================

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Decision",
		meta = (
			ClampMin = "0.20",
			UIMin = "0.20",
			UIMax = "3.00"
			)
	)
		float GoalkeeperSaveDecisionTimeHorizon =
		1.50f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Decision",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "0.15"
			)
	)
		float GoalkeeperSaveMontageStartTolerance =
		0.04f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Decision",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "500.0"
			)
	)
		float GoalkeeperSaveEmergencyLatePenaltyPerSecond =
		120.0f;

	/*
	 * Miss solo puede utilizarse para una pelota alta
	 * y casi central.
	 */
	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Miss",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "250.0"
			)
	)
		float GoalkeeperMissMaximumLateralOffset =
		80.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Miss",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "300.0"
			)
	)
		float GoalkeeperMissMinimumHeightAboveHead =
		70.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Selector|Miss",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "500.0"
			)
	)
		float GoalkeeperMissMaximumHeightAboveHead =
		220.0f;

	bool TryPredictGoalkeeperReferencePlaneCrossing(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		FVector& OutPredictedLocation,
		float& OutTimeToReferencePlane,
		float MaxReferenceTimeOverride = -1.0f
	) const;

	bool TryPredictBallAtGoalkeeperCandidatePlane(
		const ASoccerBall* SoccerBall,
		const FVector& CandidatePlaneLocation,
		float& OutTimeToCandidatePlane,
		FVector& OutPredictedBallLocation
	) const;

	bool TryScoreGoalkeeperSaveActionAgainstBall(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		ESoccerGoalkeeperAction GoalkeeperAction,
		FGoalkeeperSaveSelectionResult&
		OutBestFeasibleSelection,
		FGoalkeeperSaveSelectionResult&
		OutBestVisualSelection
	) const;

	bool IsGoalkeeperOverheadMissSituation(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& ReferencePredictedLocation
	) const;

	FGoalkeeperSaveSelectionResult
		SelectGoalkeeperActionForIncomingBall(
			const ASoccerAICharacter* SoccerCharacter,
			const ASoccerBall* SoccerBall,
			bool bCanUseHands
		) const;

	// ============================================================
	// GOALKEEPER SAVE COMMIT TRACKING
	// ============================================================

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper")
		float GoalkeeperHoldBallBeforeClearTime = 1.15f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Timing")
		float GoalkeeperFallbackContactWindowStartTime = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Timing")
		float GoalkeeperLivePendingSaveMaxPredictionTime = 4.0f;

	bool bGoalkeeperSaveCoordinationDebugHasSnapshot =
		false;

	ESoccerGoalkeeperAction
		GoalkeeperSaveCoordinationDebugAction =
		ESoccerGoalkeeperAction::None;

	bool bGoalkeeperSaveCoordinationDebugSelectedLeftHand =
		false;

	bool bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone =
		false;

	float GoalkeeperSaveCoordinationDebugCatchZoneAlpha =
		0.5f;

	/*
	 * Datos congelados en COMMIT.
	 */
	float GoalkeeperSaveCoordinationDebugCommitWorldTime =
		-1000.0f;

	float GoalkeeperSaveCoordinationDebugBallTimeToContact =
		0.0f;

	float GoalkeeperSaveCoordinationDebugPredictedContactWorldTime =
		-1000.0f;

	float GoalkeeperSaveCoordinationDebugSelectedContactMontageTime =
		0.0f;

	float GoalkeeperSaveCoordinationDebugRequiredStartDelay =
		0.0f;

	FVector GoalkeeperSaveCoordinationDebugPredictedBallLocation =
		FVector::ZeroVector;
	FVector GoalkeeperSaveCoordinationDebugPredictedSelectedHandLocation =
		FVector::ZeroVector;
	/*
	 * Posiciones de las manos y ejes antes de iniciar
	 * el montage. Sirven para reconstruir la predicción
	 * en cualquier MontagePosition posterior.
	 */
	FVector GoalkeeperSaveCoordinationDebugBaseLeftHandLocation =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugBaseRightHandLocation =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugForwardDirection =
		FVector::ForwardVector;

	FVector GoalkeeperSaveCoordinationDebugRightDirection =
		FVector::RightVector;

	/*
	 * Evento A:
	 * qué ocurrió cuando llegó el instante mundial
	 * previsto para el contacto.
	 */
	bool bGoalkeeperSaveCoordinationDebugCapturedPredictedTime =
		false;
	FVector GoalkeeperSaveCoordinationDebugRealBallAtPredictedTime =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugRealHandAtPredictedTime =
		FVector::ZeroVector;
	/*
	 * Evento B:
	 * qué ocurrió cuando el montage alcanzó
	 * el tiempo de contacto seleccionado.
	 */
	bool bGoalkeeperSaveCoordinationDebugCapturedMontageContact =
		false;

	FVector GoalkeeperSaveCoordinationDebugRealBallAtMontageContact =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugRealHandAtMontageContact =
		FVector::ZeroVector;
	/*
 * EVENTO C:
 * seguimiento del cruce real del plano particular
 * de la mano elegido en COMMIT.
 */
	FVector GoalkeeperSaveCoordinationDebugContactPlaneLocation =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugContactPlaneNormal =
		FVector::ForwardVector;

	bool bGoalkeeperSaveCoordinationDebugPlaneTrackingInitialized =
		false;

	bool bGoalkeeperSaveCoordinationDebugPlaneTrackingInvalidated =
		false;

	FVector GoalkeeperSaveCoordinationDebugPreviousBallLocation =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugPreviousSelectedHandLocation =
		FVector::ZeroVector;

	float GoalkeeperSaveCoordinationDebugPreviousBallSignedDistance =
		0.0f;

	float GoalkeeperSaveCoordinationDebugPreviousWorldTime =
		-1000.0f;

	float GoalkeeperSaveCoordinationDebugPreviousMontagePosition =
		0.0f;

	bool bGoalkeeperSaveCoordinationDebugPreviousHadMontage =
		false;

	float GoalkeeperSaveCoordinationDebugPreviousHorizontalSpeed =
		0.0f;

	bool bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing =
		false;
	float GoalkeeperSaveCoordinationDebugActualCrossingElapsedTime =
		0.0f;

	float GoalkeeperSaveCoordinationDebugCrossingTimeError =
		0.0f;

	FVector GoalkeeperSaveCoordinationDebugRealBallAtPlaneCrossing =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugRealHandAtPlaneCrossing =
		FVector::ZeroVector;

	float GoalkeeperSaveCoordinationDebugBallErrorAtPlaneCrossing =
		0.0f;

	float GoalkeeperSaveCoordinationDebugBallHandDistanceAtPlaneCrossing =
		0.0f;

	bool bGoalkeeperSaveCoordinationDebugHadMontageAtPlaneCrossing =
		false;

	float GoalkeeperSaveCoordinationDebugMontagePositionAtPlaneCrossing =
		0.0f;

	float GoalkeeperSaveCoordinationDebugMontageErrorAtPlaneCrossing =
		0.0f;

	float GoalkeeperSaveCoordinationDebugCurveHandErrorAtPlaneCrossing =
		0.0f;

	int32 GoalkeeperSaveCoordinationDebugMotionSampleCount =
		0;

	float GoalkeeperSaveCoordinationDebugMinReportedApproachSpeed =
		0.0f;

	float GoalkeeperSaveCoordinationDebugMaxReportedApproachSpeed =
		0.0f;

	float GoalkeeperSaveCoordinationDebugMinMeasuredApproachSpeed =
		0.0f;

	float GoalkeeperSaveCoordinationDebugMaxMeasuredApproachSpeed =
		0.0f;
	float GoalkeeperSaveCoordinationDebugMaxMotionSampleDeltaTime =
		0.0f;

	float GoalkeeperSaveCoordinationDebugMaxApproachSpeedDifference =
		0.0f;

	/*
	 * Comparación viva:
	 * mano reconstruida con curvas contra hueso real
	 * en el mismo MontagePosition.
	 */
	bool bGoalkeeperSaveCoordinationDebugLiveHandValid =
		false;

	float GoalkeeperSaveCoordinationDebugLiveMontagePosition =
		0.0f;
	float GoalkeeperSaveCoordinationDebugLiveHandError =
		0.0f;

	FVector GoalkeeperSaveCoordinationDebugLivePredictedHandLocation =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugLiveActualHandLocation =
		FVector::ZeroVector;

	void ResetGoalkeeperSaveCoordinationDebug();

	void BeginGoalkeeperSaveCoordinationDebug(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		const FGoalkeeperSaveSelectionResult& Selection
	);

	void UpdateGoalkeeperSaveCoordinationDebug(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall
	);

	bool TryGetGoalkeeperSaveDebugActualHands(
		const ASoccerAICharacter* SoccerCharacter,
		FVector& OutLeftHandLocation,
		FVector& OutRightHandLocation
	) const;

	bool TryEvaluateGoalkeeperSaveDebugPredictedHands(
		const ASoccerAICharacter* SoccerCharacter,
		float MontageTime,
		FVector& OutPredictedLeftHandLocation,
		FVector& OutPredictedRightHandLocation
	) const;

	bool bGoalkeeperSaveCoordinationDebugCapturedFirstMontageHandSample =
		false;

	float GoalkeeperSaveCoordinationDebugFirstMontageHandSampleTime =
		0.0f;

	float GoalkeeperSaveCoordinationDebugFirstMontageHandError =
		0.0f;

	/*
	 * Coordenadas locales:
	 * X = Forward
	 * Y = Lateral hacia la derecha
	 * Z = Up
	 */
	FVector GoalkeeperSaveCoordinationDebugFirstMontageHandErrorLocal =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugFirstActualHandMotionLocal =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugFirstCurveHandMotionLocal =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugFirstCapsuleMotionLocal =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugLiveHandErrorLocal =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugLiveActualHandMotionLocal =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugLiveCurveHandMotionLocal =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugLiveCapsuleMotionLocal =
		FVector::ZeroVector;

	FVector GoalkeeperSaveCoordinationDebugBaseMeshOriginLocation =
		FVector::ZeroVector;
	// Radios medidos desde el centro de cada hueso.
	// Incluyen aproximadamente el radio de la pelota y un pequeño margen.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Impact|Animated Contact"
	)
		float GoalkeeperHandContactRadius = 38.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Impact|Animated Contact"
	)
		float GoalkeeperArmContactRadius = 42.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Impact|Animated Contact"
	)
		float GoalkeeperTorsoContactRadius = 48.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Impact|Animated Contact"
	)
		float GoalkeeperLegContactRadius = 45.0f;

	// Evita usar un segmento anterior incorrecto si la pelota fue
	// teletransportada o reseteada.
	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Impact|Animated Contact"
	)
		float GoalkeeperMaxContactSweepDistance = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Impact")
		bool bEnableGoalkeeperEmergencyBodyContact = true;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Impact|Animated Contact"
	)
		bool bDebugGoalkeeperAnimatedContact = false;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Intervention")
		float GoalkeeperInterventionMinTowardGoalDot = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Area")
		float GoalkeeperPenaltyAreaDepth = 1650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Area")
		float GoalkeeperPenaltyAreaHalfWidth = 1150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Area")
		float GoalkeeperPenaltyAreaHandlingMargin = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		bool bUseGoalkeeperSweeper = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperTriggerDepthFromGoal = 1850.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperTriggerHalfWidth = 1250.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperMaxDistanceFromHome = 2200.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperMinimumCommitTime = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperAcceptanceRadius = 70.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperCutTowardGoalOffset = 160.0f;

	// Distancia amplia usada solamente para comenzar a intentar el corte.
	// La patada real exige contacto mediante los valores de Foot Clearance.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperFootCutDistance = 135.0f;

	// Separacion maxima entre la superficie de la capsula y la superficie
	// de la pelota para considerar que el pie realmente puede tocarla.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Foot Clearance", meta = (ClampMin = "0.0"))
		float GoalkeeperFootClearanceMaxSurfaceGap = 22.0f;

	// Evita patear una pelota claramente ubicada detras del arquero.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Foot Clearance", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
		float GoalkeeperFootClearanceMinFacingDot = -0.05f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Foot Clearance")
		float GoalkeeperFootClearanceMinBallHeight = -20.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Foot Clearance")
		float GoalkeeperFootClearanceMaxBallHeight = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperHandCollectDistance = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Smother")
		bool bUseGoalkeeperSweeperSmother = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Smother")
		float GoalkeeperSweeperSmotherStartDistance = 340.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Smother")
		float GoalkeeperSweeperSmotherBallDistance = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Smother")
		float GoalkeeperSweeperSmotherLooseBallFromThreatDistance = 135.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Smother")
		float GoalkeeperSweeperSmotherMaxBallHeightForHands = 175.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Smother")
		float GoalkeeperSweeperSmotherMaxBallHeightForBodyBlock = 135.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Smother")
		float GoalkeeperSweeperSmotherCooldown = 0.85f;

	float LastGoalkeeperSweeperSmotherTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperMaxBallHeightForFeet = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperMaxBallHeightForHands = 210.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperAttackerMaxDistanceToBall = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperLooseBallExtraTriggerDepth = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperDefensiveHelpLaneHalfWidth = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperDefensiveHelpMaxDistanceToThreat = 760.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperDefensiveHelpGoalSideDepthMargin = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Direction")
		bool bUseGoalkeeperSweeperThreatDirectionCheck = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Direction")
		float GoalkeeperSweeperThreatMinSpeedForDirectionCheck = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Direction")
		float GoalkeeperSweeperThreatTowardGoalMinDot = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Race")
		bool bUseGoalkeeperSweeperRaceCheck = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Race")
		float GoalkeeperSweeperGoalkeeperTravelSpeed = 760.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Race")
		float GoalkeeperSweeperThreatTravelSpeed = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Race")
		float GoalkeeperSweeperDefenderTravelSpeed = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Race")
		float GoalkeeperSweeperRaceWinMargin = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Race")
		float GoalkeeperSweeperRaceLoseTolerance = 0.28f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Race")
		float GoalkeeperSweeperDefenderHelpArrivalMargin = 0.20f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper|Race")
		float GoalkeeperSweeperLooseBallPredictionTime = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperClearanceForwardDistance = 1900.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperClearanceLateralRandomRange = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperClearanceHorizontalSpeed = 1700.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperClearanceMinTravelTime = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperClearanceMaxTravelTime = 1.05f;

	ESoccerGoalkeeperBehaviorMode CurrentGoalkeeperBehaviorMode =
		ESoccerGoalkeeperBehaviorMode::Positioning;

	bool bGoalkeeperSweeperActive = false;

	float GoalkeeperSweeperStartTime = -1000.0f;

	FVector GoalkeeperSweeperTargetLocation = FVector::ZeroVector;

	TWeakObjectPtr<ASoccerCharacterBase> GoalkeeperSweeperThreatCharacter;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Sweeper")
		float GoalkeeperSweeperPostActionCooldown = 0.55f;

	float LastGoalkeeperSweeperResolvedTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Retreat")
		bool bUseGoalkeeperSmartRetreat = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Retreat")
		float GoalkeeperRetreatStartDistanceFromHome = 260.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Retreat")
		float GoalkeeperRetreatFinishedDistanceFromHome = 115.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Retreat")
		float GoalkeeperRetreatAcceptanceRadius = 90.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Retreat")
		float GoalkeeperRetreatMoveRefreshInterval = 0.30f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Retreat")
		float GoalkeeperRetreatRepathDistanceThreshold = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Retreat")
		float GoalkeeperRetreatFaceBallMaxDistance = 3600.0f;

	bool bGoalkeeperRetreatActive = false;

	FVector LastGoalkeeperRetreatMoveTarget = FVector::ZeroVector;

	float LastGoalkeeperRetreatMoveRequestTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Clearance")
		float GoalkeeperClearanceForwardDistance = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Clearance")
		float GoalkeeperClearanceLateralRandomRange = 550.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Clearance")
		float GoalkeeperClearanceHorizontalSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Clearance")
		float GoalkeeperClearanceMinTravelTime = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Clearance")
		float GoalkeeperClearanceMaxTravelTime = 1.15f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		bool bUseGoalkeeperSmartDistribution = true;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Distribution Debug"
	)
		ESoccerGoalkeeperDistributionDebugMode
		GoalkeeperDistributionDebugMode =
		ESoccerGoalkeeperDistributionDebugMode::Auto;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Distribution Debug"
	)
		bool bDebugGoalkeeperDistribution = false;

	// ============================================================
// GOALKEEPER SAVE COORDINATION DEBUG
// ============================================================

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Coordination Debug"
	)
		bool bDebugGoalkeeperSaveCoordination =
		true;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Coordination Debug",
		meta = (
			ClampMin = "1.0",
			UIMin = "1.0",
			UIMax = "50.0"
			)
	)
		float GoalkeeperSaveCoordinationPredictedSphereRadius =
		18.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Coordination Debug",
		meta = (
			ClampMin = "1.0",
			UIMin = "1.0",
			UIMax = "50.0"
			)
	)
		float GoalkeeperSaveCoordinationActualSphereRadius =
		13.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Coordination Debug",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "10.0"
			)
	)
		float GoalkeeperSaveCoordinationLineThickness =
		2.5f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Save Coordination Debug",
		meta = (
			ClampMin = "100.0",
			UIMin = "100.0",
			UIMax = "2000.0"
			)
	)
		float GoalkeeperSaveCoordinationMinimumTeleportDistance =
		500.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Distribution"
	)
		float GoalkeeperDistributionFallbackShortForwardDistance =
		1100.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Distribution"
	)
		float GoalkeeperDistributionFallbackShortLateralRange =
		220.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperUrgentHoldBallBeforeDistributionTime = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionUrgentPressureRadius = 520.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionMinPassDistance = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionMaxPassDistance = 2600.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionIdealPassDistance = 1450.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionTeammateLeadDistance = 130.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionLaneBlockRadius = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionOpponentPressureRadius = 460.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionMinPassScore = 0.46f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionGroundPassHorizontalSpeed = 1350.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionGroundPassMinTravelTime = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Distribution")
		float GoalkeeperDistributionGroundPassMaxTravelTime = 1.35f;


	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Distribution Animation"
	)
		float GoalkeeperDistributionFacingToleranceDegrees = 5.0f;

	UPROPERTY(
		EditAnywhere,
		Category = "Soccer|Goalkeeper|Distribution Animation"
	)
		float GoalkeeperDistributionMontageTimeoutExtraTime = 0.50f;

	bool bGoalkeeperDistributionActive = false;

	bool bGoalkeeperDistributionMontageStarted = false;

	bool bGoalkeeperDistributionBallReleased = false;

	bool bGoalkeeperDistributionBallPlaced = false;

	bool bGoalkeeperDistributionBallKicked = false;

	bool bGoalkeeperDistributionRuleTouchRegistered = false;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Drop Kick",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "800.0"
			)
	)
		float GoalkeeperDropKickReleaseForwardSpeed =
		350.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Drop Kick",
		meta = (
			ClampMin = "0.0",
			UIMin = "0.0",
			UIMax = "500.0"
			)
	)
		float GoalkeeperDropKickReleaseUpwardSpeed =
		40.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Drop Kick",
		meta = (
			ClampMin = "-800.0",
			ClampMax = "800.0",
			UIMin = "-400.0",
			UIMax = "400.0"
			)
	)
		float GoalkeeperDropKickReleaseLateralSpeed =
		0.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion"
	)
		bool bUseGoalkeeperDistributionCurveMotion = true;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion"
	)
		bool bDebugGoalkeeperDistributionCurveMotion = false;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Curves"
	)
		UCurveTable* GoalkeeperOverhandThrowMotionCurveTable =
		nullptr;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Curves"
	)
		UCurveTable* GoalkeeperDropKickMotionCurveTable =
		nullptr;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Curves"
	)
		UCurveTable* GoalkeeperPlacingBallShortMotionCurveTable =
		nullptr;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Curves"
	)
		UCurveTable* GoalkeeperPlacingBallLongMotionCurveTable =
		nullptr;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Overhand Throw"
	)
		float GoalkeeperOverhandThrowForwardMotionScale = 1.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Overhand Throw"
	)
		float GoalkeeperOverhandThrowLateralMotionScale = 1.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Drop Kick"
	)
		float GoalkeeperDropKickForwardMotionScale = 1.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Drop Kick"
	)
		float GoalkeeperDropKickLateralMotionScale = 1.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Placing Ball Short"
	)
		float GoalkeeperPlacingBallShortForwardMotionScale = 1.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Placing Ball Short"
	)
		float GoalkeeperPlacingBallShortLateralMotionScale = 1.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Placing Ball Long"
	)
		float GoalkeeperPlacingBallLongForwardMotionScale = 1.0f;

	UPROPERTY(
		EditAnywhere,
		Category =
		"Soccer|Goalkeeper|Distribution|Curve Motion|Placing Ball Long"
	)
		float GoalkeeperPlacingBallLongLateralMotionScale = 1.0f;

	bool bGoalkeeperDistributionCurveMotionInitialized =
		false;

	FVector ActiveGoalkeeperDistributionMotionStartLocation =
		FVector::ZeroVector;

	FVector ActiveGoalkeeperDistributionMotionForwardDirection =
		FVector::ForwardVector;

	FVector ActiveGoalkeeperDistributionMotionRightDirection =
		FVector::RightVector;

	ESoccerGoalkeeperDistributionType
		ActiveGoalkeeperDistributionType =
		ESoccerGoalkeeperDistributionType::None;

	UPROPERTY()
		ASoccerAICharacter* ActiveGoalkeeperDistributionCharacter =
		nullptr;

	UPROPERTY()
		ASoccerBall* ActiveGoalkeeperDistributionBall =
		nullptr;

	FVector ActiveGoalkeeperDistributionTargetLocation =
		FVector::ZeroVector;

	float ActiveGoalkeeperDistributionHorizontalSpeed = 0.0f;

	float ActiveGoalkeeperDistributionMinTravelTime = 0.0f;

	float ActiveGoalkeeperDistributionMaxTravelTime = 0.0f;

	float ActiveGoalkeeperDistributionFinishDeadline = -1000.0f;

	float GoalkeeperSaveCoordinationDebugPreviousApproachSpeed =
		0.0f;
	UPROPERTY()
		ASoccerAICharacter* PendingGoalkeeperSaveActionCharacter = nullptr;

	UPROPERTY()
		ASoccerBall* PendingGoalkeeperSaveActionBall = nullptr;

	ESoccerGoalkeeperAction PendingGoalkeeperScheduledSaveAction =
		ESoccerGoalkeeperAction::None;

	FVector PendingGoalkeeperSaveActionPredictedInterventionLocation =
		FVector::ZeroVector;

	float PendingGoalkeeperSaveActionTimeToIntervention = 0.0f;

	float PendingGoalkeeperSaveActionContactWindowStartTime = 0.0f;

	// Se actualiza en cada reevaluacion de ShotPending. Permite que la capa
	// exterior distinga una reaccion visual imposible de una atajada alcanzable.
	bool bPendingGoalkeeperSaveActionEmergencySelection = false;

	UPROPERTY()
		ASoccerAICharacter* PendingGoalkeeperSaveCharacter = nullptr;

	UPROPERTY()
		ASoccerBall* PendingGoalkeeperSaveBall = nullptr;

	ESoccerGoalkeeperAction PendingGoalkeeperSaveAction =
		ESoccerGoalkeeperAction::None;

	FVector PendingGoalkeeperPreviousBallLocation =
		FVector::ZeroVector;

	bool bHasPendingGoalkeeperPreviousBallLocation = false;

	bool ShouldGoalkeeperFaceBallDuringAction(
		const ASoccerAICharacter* SoccerCharacter
	) const;

	bool TryUpdateGoalkeeperLooseBallClaimBehavior(
		ASoccerAICharacter* SoccerCharacter,
		ASoccerBall* SoccerBall,
		float DeltaTime
	);

	bool ShouldGoalkeeperClaimLooseBall(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall,
		FVector& OutClaimTargetLocation
	) const;

	bool CanGoalkeeperReachBallBeforeOpponents(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& BallTargetLocation,
		float& OutGoalkeeperTime,
		float& OutBestOpponentTime
	) const;

	bool IsGoalkeeperClaimBallAtFootHeight(
		const ASoccerAICharacter* SoccerCharacter,
		const ASoccerBall* SoccerBall
	) const;

	void ClearGoalkeeperLooseBallClaimMode();

	bool ShouldGoalkeeperClaimLooseBallConsideringOwnTeam(
		const ASoccerAICharacter* SoccerCharacter,
		const FVector& BallTargetLocation
	) const;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		bool bUseGoalkeeperLooseBallClaim = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		float GoalkeeperLooseBallClaimMaxDepthFromGoal = 2100.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		float GoalkeeperLooseBallClaimHalfWidth = 1300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		float GoalkeeperLooseBallClaimArrivalMargin = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		float GoalkeeperLooseBallClaimAcceptanceRadius = 70.0f;

	// Cuando el arquero ya esta cerca de la pelota y se comprometio a
	// despejarla, dejamos de apuntar a la prediccion y vamos a la ubicacion
	// real con un radio mucho mas chico. Esto evita que MoveTo considere
	// terminada la carrera unos centimetros antes del contacto de pie.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim", meta = (ClampMin = "0.0"))
		float GoalkeeperLooseBallClaimFinalApproachAcceptanceRadius = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		float GoalkeeperLooseBallClaimKickDistance = 145.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		float GoalkeeperLooseBallClaimMaxBallHeightForFeet = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		float GoalkeeperLooseBallClaimMoveRefreshInterval = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		float GoalkeeperLooseBallClaimRepathDistanceThreshold = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		float GoalkeeperLooseBallClaimOwnTeamCloserMargin = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goalkeeper|Loose Ball Claim")
		float GoalkeeperLooseBallClaimOwnTeamGoalSideDepthMargin = 80.0f;

	// Esta bandera conserva el compromiso de la salida durante los ultimos
	// centimetros, para que el arquero no abandone el despeje justo al llegar.
	bool bGoalkeeperLooseBallClaimActive = false;

	FVector LastGoalkeeperLooseBallClaimMoveTarget =
		FVector::ZeroVector;

	float LastGoalkeeperLooseBallClaimMoveRequestTime =
		-1000.0f;
};