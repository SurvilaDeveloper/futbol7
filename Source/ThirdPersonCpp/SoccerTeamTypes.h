//SoccerTeamTypes.h

#pragma once

#include "CoreMinimal.h"
#include "SoccerTeamTypes.generated.h"

UENUM(BlueprintType)
enum class ESoccerTeam : uint8
{
	PlayerTeam UMETA(DisplayName = "Player Team"),
	OpponentTeam UMETA(DisplayName = "Opponent Team")
};

UENUM(BlueprintType)
enum class ESoccerPlayerRole : uint8
{
	Goalkeeper UMETA(DisplayName = "Goalkeeper"),
	Defender UMETA(DisplayName = "Defender"),
	Midfielder UMETA(DisplayName = "Midfielder"),
	Forward UMETA(DisplayName = "Forward")
};

UENUM(BlueprintType)
enum class ESoccerPossessionTeam : uint8
{
	None UMETA(DisplayName = "None"),
	PlayerTeam UMETA(DisplayName = "Player Team"),
	OpponentTeam UMETA(DisplayName = "Opponent Team")
};

UENUM(BlueprintType)
enum class ESoccerHumanPassRequestType : uint8
{
	Normal = 0 UMETA(DisplayName = "Normal Pass"),
	AerialHeader = 1 UMETA(DisplayName = "Aerial Pass For Header"),
	None = 2 UMETA(DisplayName = "No Pass Request")
};

UENUM(BlueprintType)
enum class ESoccerTeamPhase : uint8
{
	Neutral UMETA(DisplayName = "Neutral"),
	Attacking UMETA(DisplayName = "Attacking"),
	Defending UMETA(DisplayName = "Defending")
};

UENUM(BlueprintType)
enum class ESoccerAIOrder : uint8
{
	ReturnHome UMETA(DisplayName = "Return Home"),

	ChaseBall UMETA(DisplayName = "Chase Ball"),
	SupportBall UMETA(DisplayName = "Support Ball"),

	// Ataque colectivo
	AttackRecoverBall UMETA(DisplayName = "Attack Recover Ball"),
	AttackSupportShort UMETA(DisplayName = "Attack Support Short"),
	AttackSupportForward UMETA(DisplayName = "Attack Support Forward"),
	AttackRunIntoSpace UMETA(DisplayName = "Attack Run Into Space"),
	AttackWideSupport UMETA(DisplayName = "Attack Wide Support"),
	AttackRestDefense UMETA(DisplayName = "Attack Rest Defense"),
	AttackCompensateCover UMETA(DisplayName = "Attack Compensate Cover"),

	// Defensa temporal / base nueva
	PressBall UMETA(DisplayName = "Press Ball"),
	DefendProtectGoalLane UMETA(DisplayName = "Defend Protect Goal Lane"),
	DefendCoverCenter UMETA(DisplayName = "Defend Cover Center"),
	DefendCompactShape UMETA(DisplayName = "Defend Compact Shape"),
	DefendMarkDangerousReceiver UMETA(DisplayName = "Defend Mark Dangerous Receiver"),

	MaintainTeamShape UMETA(DisplayName = "Maintain Team Shape")
};

UENUM(BlueprintType)
enum class ESoccerGoalLineRestartType : uint8
{
	None UMETA(DisplayName = "None"),
	GoalKick UMETA(DisplayName = "Goal Kick"),
	CornerKick UMETA(DisplayName = "Corner Kick")
};

UENUM(BlueprintType)
enum class ESoccerRestartType : uint8
{
	None UMETA(DisplayName = "None"),
	Kickoff UMETA(DisplayName = "Kickoff"),
	OffsideFreeKick UMETA(DisplayName = "Offside Free Kick"),
	DirectFreeKick UMETA(DisplayName = "Direct Free Kick"),
	PenaltyKick UMETA(DisplayName = "Penalty Kick"),
	ThrowIn UMETA(DisplayName = "Throw In"),
	CornerKick UMETA(DisplayName = "Corner Kick"),
	GoalKick UMETA(DisplayName = "Goal Kick")
};

UENUM(BlueprintType)
enum class ESoccerMatchPlayState : uint8
{
	Playing UMETA(DisplayName = "Playing"),
	GoalScored UMETA(DisplayName = "Goal Scored"),
	Resetting UMETA(DisplayName = "Resetting"),
	KickoffSetup UMETA(DisplayName = "Kickoff Setup"),
	KickoffTaking UMETA(DisplayName = "Kickoff Taking"),

	OffsideReviewFreeze UMETA(DisplayName = "Offside Review Freeze"),
	OffsideRestartSetup UMETA(DisplayName = "Offside Restart Setup"),
	OffsideRestartTaking UMETA(DisplayName = "Offside Restart Taking"),

	PenaltyKickSetup UMETA(DisplayName = "Penalty Kick Setup"),
	PenaltyKickTaking UMETA(DisplayName = "Penalty Kick Taking"),

	ThrowInSetup UMETA(DisplayName = "Throw In Setup"),
	ThrowInPositioning UMETA(DisplayName = "Throw In Positioning"),
	ThrowInExecuting UMETA(DisplayName = "Throw In Executing"),

	GoalLineRestartSetup UMETA(DisplayName = "Goal Line Restart Setup"),
	GoalLineRestartPositioning UMETA(DisplayName = "Goal Line Restart Positioning"),
	GoalLineRestartTaking UMETA(DisplayName = "Goal Line Restart Taking"),

	// The ball remains physically free for a short presentation window
	// before a throw-in, corner kick or goal kick is prepared.
	BallOutOfPlayDelay UMETA(DisplayName = "Ball Out Of Play Delay"),

	// A penalty foul has already been awarded, but the physical aftermath of
	// the tackle is allowed to finish before penalty setup begins.
	PenaltyFoulDelay UMETA(DisplayName = "Penalty Foul Delay")
};

UENUM(BlueprintType)
enum class ESoccerFieldZone : uint8
{
	ZoneA UMETA(DisplayName = "Zone A"),
	ZoneB UMETA(DisplayName = "Zone B"),
	ZoneC UMETA(DisplayName = "Zone C"),
	ZoneD UMETA(DisplayName = "Zone D"),
	ZoneE UMETA(DisplayName = "Zone E"),
	ZoneF UMETA(DisplayName = "Zone F")
};

UENUM(BlueprintType)
enum class ESoccerBallSituation : uint8
{
	FreeBall UMETA(DisplayName = "Free Ball"),
	SelfPossession UMETA(DisplayName = "Self Possession"),
	OwnTeamPossession UMETA(DisplayName = "Own Team Possession"),
	OpponentTeamPossession UMETA(DisplayName = "Opponent Team Possession")
};

UENUM(BlueprintType)
enum class ESoccerGoalkeeperAction : uint8
{
	None = 0 UMETA(DisplayName = "None"),

	/*
	 * Valores legacy conservados temporalmente.
	 *
	 * Est�n ocultos en los desplegables de Blueprint, pero todav�a
	 * existen porque el selector y algunos sistemas del controller
	 * los siguen utilizando durante este checkpoint.
	 *
	 * Los eliminaremos cuando terminemos de migrar toda la l�gica.
	 */
	CatchLow = 1 UMETA(Hidden),
	CatchChest = 2 UMETA(Hidden),
	CatchHighForward = 3 UMETA(Hidden),
	CatchHighRight = 4 UMETA(Hidden),

	BodyBlockLeft = 5 UMETA(Hidden),
	BodyBlockRight = 6 UMETA(Hidden),
	BodyBlockLeftAlt = 7 UMETA(Hidden),

	DivingSaveLeft = 8 UMETA(Hidden),
	DivingSaveRight = 9 UMETA(Hidden),

	/*
	 * Miss conserva su valor anterior para no alterar datos
	 * serializados que ya pudieran contener esta acci�n.
	 */
	Miss = 10 UMETA(DisplayName = "Miss"),

	// Nuevas acciones de atajada.
	BodyBlockCatchToLeft = 11
	UMETA(DisplayName = "Body Block Catch To Left"),

	BodyBlockCatchToRight = 12
	UMETA(DisplayName = "Body Block Catch To Right"),

	BodyBlockDeflectToLeft = 13
	UMETA(DisplayName = "Body Block Deflect To Left"),

	BodyBlockDeflectToRight = 14
	UMETA(DisplayName = "Body Block Deflect To Right"),

	CatchAbdomen = 15
	UMETA(DisplayName = "Catch Abdomen"),

	CatchFaceToLeft = 16
	UMETA(DisplayName = "Catch Face To Left"),

	CatchFaceToRight = 17
	UMETA(DisplayName = "Catch Face To Right"),

	CatchOverHeadJumpToLeft = 18
	UMETA(DisplayName = "Catch Over Head Jump To Left"),

	CatchOverHeadJumpToRight = 19
	UMETA(DisplayName = "Catch Over Head Jump To Right"),

	CatchOverHeadRunJump = 20
	UMETA(DisplayName = "Catch Over Head Run Jump"),

	DivingSaveFloorToLeft = 21
	UMETA(DisplayName = "Diving Save Floor To Left"),

	DivingSaveFloorToRight = 22
	UMETA(DisplayName = "Diving Save Floor To Right"),

	DivingSaveOneMeterToLeft = 23
	UMETA(DisplayName = "Diving Save One Meter To Left"),

	DivingSaveOneMeterToRight = 24
	UMETA(DisplayName = "Diving Save One Meter To Right"),

	ScoopToLeft = 25
	UMETA(DisplayName = "Scoop To Left"),

	ScoopToRight = 26
	UMETA(DisplayName = "Scoop To Right")
};

UENUM(BlueprintType)
enum class ESoccerGoalkeeperDistributionType : uint8
{
	None UMETA(DisplayName = "None"),

	OverhandThrow UMETA(DisplayName = "Overhand Throw"),
	DropKick UMETA(DisplayName = "Drop Kick"),
	PlacingBallShort UMETA(DisplayName = "Placing Ball Short"),
	PlacingBallLong UMETA(DisplayName = "Placing Ball Long")
};

UENUM(BlueprintType)
enum class ESoccerGoalkeeperDistributionDebugMode : uint8
{
	Auto UMETA(DisplayName = "Auto"),

	ForceOverhandThrow
	UMETA(DisplayName = "Force Overhand Throw"),

	ForceDropKick
	UMETA(DisplayName = "Force Drop Kick"),

	ForcePlacingBallShort
	UMETA(DisplayName = "Force Placing Ball Short"),

	ForcePlacingBallLong
	UMETA(DisplayName = "Force Placing Ball Long")
};

UENUM(BlueprintType)
enum class ESoccerGoalkeeperDistributionEvent : uint8
{
	ReleaseBall UMETA(DisplayName = "Release Ball"),
	PlaceBall UMETA(DisplayName = "Place Ball"),
	KickBall UMETA(DisplayName = "Kick Ball"),
	Finished UMETA(DisplayName = "Finished")
};

UENUM(BlueprintType)
enum class ESoccerGoalkeeperBehaviorMode : uint8
{
	Positioning UMETA(DisplayName = "Positioning"),
	ShotPending UMETA(DisplayName = "Shot Pending"),
	Saving UMETA(DisplayName = "Saving"),
	HoldingBall UMETA(DisplayName = "Holding Ball"),

	Distributing UMETA(DisplayName = "Distributing"),

	Sweeping UMETA(DisplayName = "Sweeping"),
	Smothering UMETA(DisplayName = "Smothering"),
	Retreating UMETA(DisplayName = "Retreating")
};