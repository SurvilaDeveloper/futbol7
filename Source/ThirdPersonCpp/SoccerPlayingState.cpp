#include "SoccerPlayingState.h"

#include "SoccerMatchManager.h"
#include "SoccerBall.h"

bool FSoccerPlayingState::Enter(ASoccerMatchManager& Manager)
{
	Manager.MatchPlayState = ESoccerMatchPlayState::Playing;
	Manager.MatchStateUpdateAccumulator = 0.0f;

	// Playing never owns a restart context. Concrete restart states are
	// responsible for ending their own context before returning here.
	if (Manager.IsRestartContextActive())
	{
		Manager.EndRestartContext();
	}

	return true;
}

void FSoccerPlayingState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	// Some animations/locomotion intentionally survive the exact restart state.
	// They are accessory open-play behavior and therefore continue here.
	if (
		Manager.bThrowInExecutionActive &&
		Manager.bThrowInReturningToField
	)
	{
		Manager.UpdateThrowInReturnToField(DeltaTime);
	}

	if (Manager.GoalLineRestart.IsCornerReturnToFieldActive())
	{
		Manager.UpdateCornerKickReturnToField(DeltaTime);
	}

	Manager.UpdateBallBoundaryTrackingAndDetectOutOfPlay();

	if (Manager.PendingMatchStateTransition != ESoccerMatchStateTransition::None)
	{
		return;
	}

	Manager.MatchStateUpdateAccumulator += DeltaTime;
	if (Manager.MatchStateUpdateAccumulator < Manager.MatchStateUpdateInterval)
	{
		return;
	}

	Manager.MatchStateUpdateAccumulator = 0.0f;

	if (!IsValid(Manager.SoccerBall))
	{
		Manager.FindSoccerBall();
	}

	if (!IsValid(Manager.SoccerBall))
	{
		Manager.ClearAssignedAI();
		return;
	}

	const ESoccerPossessionTeam PreviousPossessionTeam =
		Manager.PossessionTeam;

	ASoccerCharacterBase* PreviousPossessingCharacter =
		Manager.PossessingCharacter;

	Manager.DetectPossession();

	if (IsValid(Manager.PossessingCharacter))
	{
		if (!Manager.TryRegisterIntentionalBallTouch(
			Manager.PossessingCharacter
		))
		{
			Manager.ClearAssignedAI();
			return;
		}

		if (
			Manager.PendingMatchStateTransition !=
				ESoccerMatchStateTransition::None ||
			Manager.MatchPlayState != ESoccerMatchPlayState::Playing
		)
		{
			Manager.ClearAssignedAI();
			return;
		}
	}

	Manager.UpdateOpenPlayPassIntent();

	const bool bPossessionChanged =
		PreviousPossessionTeam != Manager.PossessionTeam ||
		PreviousPossessingCharacter != Manager.PossessingCharacter;

	if (bPossessionChanged)
	{
		Manager.ClearAssignedAI();
	}

	if (Manager.HasActiveAttack())
	{
		Manager.AssignAttackDefenseRoles();
	}
	else
	{
		Manager.ClearAssignedAI();
		Manager.AssignFreeBallRoles();
	}

	// Any transition requested above is applied by MatchManager after this Tick.
}

void FSoccerPlayingState::Exit(ASoccerMatchManager& Manager)
{
	// Open-play intent is meaningful only while Playing owns the match.
	if (Manager.MatchPlayState != ESoccerMatchPlayState::Playing)
	{
		Manager.ClearOpenPlayPassIntent();
	}
}
