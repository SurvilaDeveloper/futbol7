#include "SoccerLineupEvaluationLibrary.h"

#include "SoccerFormationLibrary.h"
#include "SoccerPlayerProfile.h"

namespace
{
    struct FWeightedAbilityAccumulator
    {
        float WeightedTotal = 0.0f;
        float WeightTotal = 0.0f;

        void Add(int32 AbilityValue, float Weight)
        {
            if (Weight <= 0.0f)
            {
                return;
            }

            WeightedTotal += static_cast<float>(FMath::Clamp(AbilityValue, 0, 100)) * Weight;
            WeightTotal += Weight;
        }

        int32 GetRoundedScore() const
        {
            if (WeightTotal <= KINDA_SMALL_NUMBER)
            {
                return 0;
            }

            return FMath::Clamp(
                FMath::RoundToInt(WeightedTotal / WeightTotal),
                0,
                100
            );
        }
    };

    int32 CalculateGoalkeeperAbilityScore(const FSoccerPlayerAttributes& Attributes)
    {
        FWeightedAbilityAccumulator Score;
        Score.Add(Attributes.Goalkeeper.Reflexes, 22.0f);
        Score.Add(Attributes.Goalkeeper.Positioning, 20.0f);
        Score.Add(Attributes.Goalkeeper.Handling, 18.0f);
        Score.Add(Attributes.Goalkeeper.Diving, 15.0f);
        Score.Add(Attributes.Goalkeeper.Distribution, 8.0f);
        Score.Add(Attributes.Tactical.DefensiveReaction, 7.0f);
        Score.Add(Attributes.Tactical.Anticipation, 5.0f);
        Score.Add(Attributes.Tactical.Composure, 5.0f);
        return Score.GetRoundedScore();
    }

    int32 CalculateDefenderAbilityScore(
        const FSoccerPlayerAttributes& Attributes,
        ESoccerFormationLane FormationLane
    )
    {
        FWeightedAbilityAccumulator Score;
        const bool bWideDefender =
            FormationLane == ESoccerFormationLane::Left ||
            FormationLane == ESoccerFormationLane::Right;

        if (bWideDefender)
        {
            Score.Add(Attributes.Tactical.DefensivePositioning, 18.0f);
            Score.Add(Attributes.Tactical.Marking, 15.0f);
            Score.Add(Attributes.Technical.Tackling, 14.0f);
            Score.Add(Attributes.Tactical.DefensiveReaction, 10.0f);
            Score.Add(Attributes.Tactical.Anticipation, 10.0f);
            Score.Add(Attributes.Physical.Pace, 10.0f);
            Score.Add(Attributes.Physical.Stamina, 8.0f);
            Score.Add(Attributes.Technical.PassingAccuracy, 6.0f);
            Score.Add(Attributes.Physical.Acceleration, 5.0f);
            Score.Add(Attributes.Physical.Strength, 2.0f);
            Score.Add(Attributes.Tactical.Composure, 2.0f);
        }
        else
        {
            Score.Add(Attributes.Tactical.DefensivePositioning, 20.0f);
            Score.Add(Attributes.Tactical.Marking, 18.0f);
            Score.Add(Attributes.Technical.Tackling, 16.0f);
            Score.Add(Attributes.Tactical.Anticipation, 12.0f);
            Score.Add(Attributes.Tactical.DefensiveReaction, 10.0f);
            Score.Add(Attributes.Physical.Strength, 8.0f);
            Score.Add(Attributes.Technical.AerialAbility, 8.0f);
            Score.Add(Attributes.Physical.Pace, 3.0f);
            Score.Add(Attributes.Technical.PassingAccuracy, 3.0f);
            Score.Add(Attributes.Tactical.Composure, 2.0f);
        }

        return Score.GetRoundedScore();
    }

    int32 CalculateMidfielderAbilityScore(
        const FSoccerPlayerAttributes& Attributes,
        ESoccerFormationLine FormationLine
    )
    {
        FWeightedAbilityAccumulator Score;

        if (FormationLine == ESoccerFormationLine::DefensiveMidfield)
        {
            Score.Add(Attributes.Technical.PassingAccuracy, 15.0f);
            Score.Add(Attributes.Physical.Stamina, 12.0f);
            Score.Add(Attributes.Tactical.DefensivePositioning, 14.0f);
            Score.Add(Attributes.Technical.Tackling, 10.0f);
            Score.Add(Attributes.Tactical.Anticipation, 10.0f);
            Score.Add(Attributes.Tactical.DecisionMaking, 10.0f);
            Score.Add(Attributes.Technical.BallControl, 10.0f);
            Score.Add(Attributes.Tactical.OffBallPositioning, 5.0f);
            Score.Add(Attributes.Tactical.Marking, 5.0f);
            Score.Add(Attributes.Tactical.Composure, 5.0f);
            Score.Add(Attributes.Physical.Pace, 2.0f);
            Score.Add(Attributes.Physical.Strength, 2.0f);
        }
        else if (FormationLine == ESoccerFormationLine::AttackingMidfield)
        {
            Score.Add(Attributes.Technical.PassingAccuracy, 18.0f);
            Score.Add(Attributes.Technical.BallControl, 15.0f);
            Score.Add(Attributes.Technical.Dribbling, 12.0f);
            Score.Add(Attributes.Tactical.OffBallPositioning, 14.0f);
            Score.Add(Attributes.Tactical.DecisionMaking, 12.0f);
            Score.Add(Attributes.Tactical.Composure, 8.0f);
            Score.Add(Attributes.Technical.ShootingAccuracy, 7.0f);
            Score.Add(Attributes.Physical.Acceleration, 5.0f);
            Score.Add(Attributes.Physical.Pace, 4.0f);
            Score.Add(Attributes.Physical.Stamina, 3.0f);
            Score.Add(Attributes.Tactical.Anticipation, 2.0f);
        }
        else
        {
            Score.Add(Attributes.Technical.PassingAccuracy, 20.0f);
            Score.Add(Attributes.Technical.BallControl, 15.0f);
            Score.Add(Attributes.Tactical.DecisionMaking, 12.0f);
            Score.Add(Attributes.Tactical.OffBallPositioning, 10.0f);
            Score.Add(Attributes.Physical.Stamina, 10.0f);
            Score.Add(Attributes.Technical.Dribbling, 8.0f);
            Score.Add(Attributes.Tactical.Anticipation, 8.0f);
            Score.Add(Attributes.Tactical.DefensivePositioning, 7.0f);
            Score.Add(Attributes.Tactical.Composure, 5.0f);
            Score.Add(Attributes.Physical.Pace, 3.0f);
            Score.Add(Attributes.Physical.Acceleration, 2.0f);
        }

        return Score.GetRoundedScore();
    }

    int32 CalculateForwardAbilityScore(
        const FSoccerPlayerAttributes& Attributes,
        ESoccerFormationLane FormationLane
    )
    {
        FWeightedAbilityAccumulator Score;
        const bool bWideForward =
            FormationLane == ESoccerFormationLane::Left ||
            FormationLane == ESoccerFormationLane::Right;

        if (bWideForward)
        {
            Score.Add(Attributes.Technical.ShootingAccuracy, 18.0f);
            Score.Add(Attributes.Tactical.OffBallPositioning, 16.0f);
            Score.Add(Attributes.Physical.Pace, 12.0f);
            Score.Add(Attributes.Physical.Acceleration, 10.0f);
            Score.Add(Attributes.Technical.BallControl, 10.0f);
            Score.Add(Attributes.Technical.Dribbling, 10.0f);
            Score.Add(Attributes.Tactical.Composure, 10.0f);
            Score.Add(Attributes.Tactical.DecisionMaking, 7.0f);
            Score.Add(Attributes.Technical.PassingAccuracy, 4.0f);
            Score.Add(Attributes.Technical.AerialAbility, 3.0f);
        }
        else
        {
            Score.Add(Attributes.Technical.ShootingAccuracy, 22.0f);
            Score.Add(Attributes.Tactical.OffBallPositioning, 18.0f);
            Score.Add(Attributes.Technical.ShotPower, 10.0f);
            Score.Add(Attributes.Technical.BallControl, 10.0f);
            Score.Add(Attributes.Tactical.Composure, 10.0f);
            Score.Add(Attributes.Technical.AerialAbility, 10.0f);
            Score.Add(Attributes.Physical.Strength, 8.0f);
            Score.Add(Attributes.Tactical.DecisionMaking, 7.0f);
            Score.Add(Attributes.Physical.Pace, 5.0f);
        }

        return Score.GetRoundedScore();
    }
}

FSoccerPlayerSlotSuitability USoccerLineupEvaluationLibrary::EvaluatePlayerForFormationSlot(
    const USoccerPlayerProfile* PlayerProfile,
    ESoccerFormationSystem FormationSystem,
    FName FormationSlotId
)
{
    FSoccerPlayerSlotSuitability Result;
    Result.SlotId = FormationSlotId;

    if (PlayerProfile == nullptr || FormationSlotId.IsNone())
    {
        return Result;
    }

    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(FormationSystem);

    if (!SoccerFormationLibrary::IsValidSevenASideDefinition(FormationDefinition))
    {
        return Result;
    }

    const FSoccerFormationSlot* FormationSlot =
        SoccerFormationLibrary::FindSlotById(FormationDefinition, FormationSlotId);

    if (FormationSlot == nullptr)
    {
        return Result;
    }

    Result.bValid = true;
    Result.NaturalPosition = SoccerFormationLibrary::GetNaturalPositionForSlot(*FormationSlot);
    Result.PositionFamiliarity = FMath::Clamp(
        PlayerProfile->GetPositionFamiliarity(Result.NaturalPosition),
        0,
        100
    );
    Result.AbilityScore = CalculateAbilityScoreForSlot(PlayerProfile, *FormationSlot);

    // Abilities explain most of the football quality; familiarity applies a meaningful
    // positional penalty without forbidding the coach from improvising.
    Result.OverallScore = FMath::Clamp(
        FMath::RoundToInt(
            static_cast<float>(Result.AbilityScore) * 0.65f +
            static_cast<float>(Result.PositionFamiliarity) * 0.35f
        ),
        0,
        100
    );
    Result.SuitabilityBand = GetSuitabilityBandForScore(Result.OverallScore);

    return Result;
}

ESoccerLineupSuitabilityBand USoccerLineupEvaluationLibrary::GetSuitabilityBandForScore(
    int32 OverallScore
)
{
    const int32 SafeScore = FMath::Clamp(OverallScore, 0, 100);

    if (SafeScore >= 90)
    {
        return ESoccerLineupSuitabilityBand::Ideal;
    }
    if (SafeScore >= 80)
    {
        return ESoccerLineupSuitabilityBand::VeryGood;
    }
    if (SafeScore >= 70)
    {
        return ESoccerLineupSuitabilityBand::Good;
    }
    if (SafeScore >= 60)
    {
        return ESoccerLineupSuitabilityBand::Acceptable;
    }
    if (SafeScore >= 45)
    {
        return ESoccerLineupSuitabilityBand::Improvised;
    }

    return ESoccerLineupSuitabilityBand::Poor;
}

FText USoccerLineupEvaluationLibrary::GetSuitabilityBandDisplayName(
    ESoccerLineupSuitabilityBand SuitabilityBand
)
{
    switch (SuitabilityBand)
    {
    case ESoccerLineupSuitabilityBand::Ideal:
        return FText::FromString(TEXT("Ideal"));
    case ESoccerLineupSuitabilityBand::VeryGood:
        return FText::FromString(TEXT("Very Good"));
    case ESoccerLineupSuitabilityBand::Good:
        return FText::FromString(TEXT("Good"));
    case ESoccerLineupSuitabilityBand::Acceptable:
        return FText::FromString(TEXT("Acceptable"));
    case ESoccerLineupSuitabilityBand::Improvised:
        return FText::FromString(TEXT("Improvised"));
    case ESoccerLineupSuitabilityBand::Poor:
    default:
        return FText::FromString(TEXT("Poor"));
    }
}

int32 USoccerLineupEvaluationLibrary::CalculateAbilityScoreForSlot(
    const USoccerPlayerProfile* PlayerProfile,
    const FSoccerFormationSlot& FormationSlot
)
{
    if (PlayerProfile == nullptr)
    {
        return 0;
    }

    const FSoccerPlayerAttributes& Attributes = PlayerProfile->Attributes;

    switch (FormationSlot.PlayerRole)
    {
    case ESoccerPlayerRole::Goalkeeper:
        return CalculateGoalkeeperAbilityScore(Attributes);

    case ESoccerPlayerRole::Defender:
        return CalculateDefenderAbilityScore(Attributes, FormationSlot.FormationLane);

    case ESoccerPlayerRole::Forward:
        return CalculateForwardAbilityScore(Attributes, FormationSlot.FormationLane);

    case ESoccerPlayerRole::Midfielder:
    default:
        return CalculateMidfielderAbilityScore(Attributes, FormationSlot.FormationLine);
    }
}
