#include "SoccerCoachPlanningLibrary.h"

#include "SoccerCoachProfile.h"
#include "SoccerFormationLibrary.h"
#include "SoccerLineupEvaluationLibrary.h"
#include "SoccerPlayerProfile.h"
#include "SoccerSquadCatalog.h"

#include <initializer_list>

namespace
{
    struct FSoccerBenchCandidate
    {
        FName PlayerId = NAME_None;
        int32 Score = 0;
    };

    int32 AverageValues(std::initializer_list<int32> Values)
    {
        if (Values.size() == 0)
        {
            return 50;
        }

        int32 Total = 0;
        for (const int32 Value : Values)
        {
            Total += FMath::Clamp(Value, 0, 100);
        }
        return FMath::Clamp(
            FMath::RoundToInt(
                static_cast<float>(Total) /
                static_cast<float>(Values.size())
            ),
            0,
            100
        );
    }

    bool IsPlayerIdBefore(FName Left, FName Right)
    {
        return Left.ToString() < Right.ToString();
    }

    FSoccerTeamTacticalPlan BuildTacticalPlan(
        const USoccerCoachProfile* CoachProfile
    )
    {
        FSoccerTeamTacticalPlan Plan;
        if (!IsValid(CoachProfile))
        {
            return Plan;
        }

        const FSoccerCoachPhilosophy& P = CoachProfile->Philosophy;
        Plan.BuildUpStyle = P.Directness >= 67
            ? ESoccerBuildUpStyle::Direct
            : (P.PossessionPreference >= 67
                ? ESoccerBuildUpStyle::ShortPossession
                : ESoccerBuildUpStyle::Balanced);
        Plan.AttackingWidth = P.TeamWidth >= 67
            ? ESoccerAttackingWidth::Wide
            : (P.TeamWidth <= 33
                ? ESoccerAttackingWidth::Narrow
                : ESoccerAttackingWidth::Balanced);
        Plan.AttackingTempo = P.Tempo >= 67
            ? ESoccerAttackingTempo::Fast
            : (P.Tempo <= 33
                ? ESoccerAttackingTempo::Patient
                : ESoccerAttackingTempo::Balanced);
        Plan.AttackingTransition = P.CounterAttackPreference >= 67
            ? ESoccerAttackingTransition::CounterAttack
            : (P.PossessionPreference >= 67
                ? ESoccerAttackingTransition::RetainPossession
                : ESoccerAttackingTransition::Balanced);
        Plan.DefensiveBlock = P.DefensiveLineHeight >= 67
            ? ESoccerDefensiveBlock::High
            : (P.DefensiveLineHeight <= 33
                ? ESoccerDefensiveBlock::Low
                : ESoccerDefensiveBlock::Medium);
        Plan.PressingIntensity = P.PressingIntensity >= 67
            ? ESoccerPressingIntensity::High
            : (P.PressingIntensity <= 33
                ? ESoccerPressingIntensity::Low
                : ESoccerPressingIntensity::Medium);
        Plan.MarkingStyle = P.Compactness >= 67
            ? ESoccerMarkingStyle::Zonal
            : (P.Compactness <= 33
                ? ESoccerMarkingStyle::ManToMan
                : ESoccerMarkingStyle::Mixed);
        Plan.DefensiveTransition = P.PressingIntensity >= 67
            ? ESoccerDefensiveTransition::CounterPress
            : (P.PressingIntensity <= 33
                ? ESoccerDefensiveTransition::Regroup
                : ESoccerDefensiveTransition::Balanced);
        return Plan;
    }
}

bool USoccerCoachPlanningLibrary::BuildPreMatchPlan(
    const USoccerCoachProfile* CoachProfile,
    const USoccerSquadCatalog* SquadCatalog,
    FSoccerCoachMatchPlan& OutPlan
)
{
    OutPlan = FSoccerCoachMatchPlan();
    if (
        !IsValid(CoachProfile) ||
        !CoachProfile->HasValidCoachId() ||
        !IsValid(SquadCatalog) ||
        !SquadCatalog->HasValidClubAssociation()
    )
    {
        return false;
    }

    TArray<USoccerPlayerProfile*> AvailableProfiles;
    TSet<FName> SeenPlayerIds;
    for (USoccerPlayerProfile* PlayerProfile : SquadCatalog->PlayerProfiles)
    {
        if (
            !IsValid(PlayerProfile) ||
            !PlayerProfile->HasValidPlayerId() ||
            SeenPlayerIds.Contains(PlayerProfile->Identity.PlayerId)
        )
        {
            continue;
        }
        SeenPlayerIds.Add(PlayerProfile->Identity.PlayerId);
        AvailableProfiles.Add(PlayerProfile);
    }

    if (AvailableProfiles.Num() < 7)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[CoachPlan] Coach=%s club=%s cannot build a seven-player plan: available=%d."),
            *CoachProfile->Identity.CoachId.ToString(),
            *SquadCatalog->GetClubId().ToString(),
            AvailableProfiles.Num()
        );
        return false;
    }

    FSoccerCoachMatchPlan BestPlan;
    bool bFoundPlan = false;

    for (const ESoccerFormationSystem FormationSystem :
        SoccerFormationLibrary::GetAllSystems())
    {
        const FSoccerFormationDefinition& Formation =
            SoccerFormationLibrary::GetDefinition(FormationSystem);
        if (!SoccerFormationLibrary::IsValidSevenASideDefinition(Formation))
        {
            continue;
        }

        FSoccerCoachMatchPlan CandidatePlan;
        CandidatePlan.ClubId = SquadCatalog->GetClubId();
        CandidatePlan.CoachId = CoachProfile->Identity.CoachId;
        CandidatePlan.FormationSystem = FormationSystem;
        CandidatePlan.TacticalPlan = BuildTacticalPlan(CoachProfile);
        CandidatePlan.FormationPreferenceScore =
            CoachProfile->GetFormationPreference(FormationSystem);

        TArray<FSoccerFormationSlot> RemainingSlots = Formation.Slots;
        TSet<USoccerPlayerProfile*> UsedProfiles;
        int32 TotalLineupScore = 0;

        // Scarcity-first assignment: fill the slot whose best specialist is
        // hardest to replace before flexible players are consumed elsewhere.
        while (RemainingSlots.Num() > 0)
        {
            int32 SelectedSlotIndex = INDEX_NONE;
            USoccerPlayerProfile* SelectedProfile = nullptr;
            FSoccerCoachLineupDecision SelectedDecision;
            int32 SelectedUrgency = MIN_int32;

            for (int32 SlotIndex = 0; SlotIndex < RemainingSlots.Num(); ++SlotIndex)
            {
                const FSoccerFormationSlot& Slot = RemainingSlots[SlotIndex];
                USoccerPlayerProfile* BestProfileForSlot = nullptr;
                FSoccerCoachLineupDecision BestDecisionForSlot;
                int32 BestScore = MIN_int32;
                int32 SecondBestScore = MIN_int32;

                for (USoccerPlayerProfile* PlayerProfile : AvailableProfiles)
                {
                    if (UsedProfiles.Contains(PlayerProfile))
                    {
                        continue;
                    }

                    FSoccerCoachLineupDecision Decision;
                    const int32 Score = CalculateCandidateScore(
                        CoachProfile,
                        PlayerProfile,
                        FormationSystem,
                        Slot.SlotId,
                        Decision
                    );

                    const bool bWinsTie =
                        Score == BestScore &&
                        BestProfileForSlot != nullptr &&
                        IsPlayerIdBefore(
                            PlayerProfile->Identity.PlayerId,
                            BestProfileForSlot->Identity.PlayerId
                        );
                    if (BestProfileForSlot == nullptr || Score > BestScore || bWinsTie)
                    {
                        SecondBestScore = BestScore;
                        BestScore = Score;
                        BestProfileForSlot = PlayerProfile;
                        BestDecisionForSlot = Decision;
                    }
                    else if (Score > SecondBestScore)
                    {
                        SecondBestScore = Score;
                    }
                }

                if (BestProfileForSlot == nullptr)
                {
                    continue;
                }

                const int32 SafeSecondBest = SecondBestScore == MIN_int32
                    ? 0
                    : SecondBestScore;
                int32 Urgency = BestScore - SafeSecondBest;
                if (Slot.PlayerRole == ESoccerPlayerRole::Goalkeeper)
                {
                    Urgency += 1000;
                }

                const bool bSlotWinsTie =
                    Urgency == SelectedUrgency &&
                    SelectedSlotIndex != INDEX_NONE &&
                    Slot.SlotId.ToString() <
                        RemainingSlots[SelectedSlotIndex].SlotId.ToString();
                if (
                    SelectedSlotIndex == INDEX_NONE ||
                    Urgency > SelectedUrgency ||
                    bSlotWinsTie
                )
                {
                    SelectedSlotIndex = SlotIndex;
                    SelectedProfile = BestProfileForSlot;
                    SelectedDecision = BestDecisionForSlot;
                    SelectedUrgency = Urgency;
                }
            }

            if (SelectedSlotIndex == INDEX_NONE || !IsValid(SelectedProfile))
            {
                break;
            }

            CandidatePlan.StartingLineupBySlot.Add(
                RemainingSlots[SelectedSlotIndex].SlotId,
                SelectedProfile->Identity.PlayerId
            );
            CandidatePlan.LineupDecisions.Add(SelectedDecision);
            TotalLineupScore += SelectedDecision.TotalScore;
            UsedProfiles.Add(SelectedProfile);
            RemainingSlots.RemoveAt(SelectedSlotIndex);
        }

        CandidatePlan.bComplete =
            CandidatePlan.StartingLineupBySlot.Num() == Formation.Slots.Num();
        if (!CandidatePlan.bComplete)
        {
            continue;
        }

        CandidatePlan.StartingLineupScore = FMath::Clamp(
            FMath::RoundToInt(
                static_cast<float>(TotalLineupScore) /
                static_cast<float>(Formation.Slots.Num())
            ),
            0,
            100
        );
        CandidatePlan.OverallPlanScore = FMath::Clamp(
            FMath::RoundToInt(
                static_cast<float>(CandidatePlan.StartingLineupScore) * 0.85f +
                static_cast<float>(CandidatePlan.FormationPreferenceScore) * 0.15f
            ),
            0,
            100
        );

        const bool bFormationWinsTie =
            bFoundPlan &&
            CandidatePlan.OverallPlanScore == BestPlan.OverallPlanScore &&
            CandidatePlan.FormationPreferenceScore == BestPlan.FormationPreferenceScore &&
            static_cast<uint8>(CandidatePlan.FormationSystem) <
                static_cast<uint8>(BestPlan.FormationSystem);
        if (
            !bFoundPlan ||
            CandidatePlan.OverallPlanScore > BestPlan.OverallPlanScore ||
            (
                CandidatePlan.OverallPlanScore == BestPlan.OverallPlanScore &&
                CandidatePlan.FormationPreferenceScore >
                    BestPlan.FormationPreferenceScore
            ) ||
            bFormationWinsTie
        )
        {
            BestPlan = CandidatePlan;
            bFoundPlan = true;
        }
    }

    if (!bFoundPlan)
    {
        return false;
    }

    TSet<FName> StarterIds;
    for (const TPair<FName, FName>& Starter : BestPlan.StartingLineupBySlot)
    {
        StarterIds.Add(Starter.Value);
    }

    const FSoccerFormationDefinition& BestFormation =
        SoccerFormationLibrary::GetDefinition(BestPlan.FormationSystem);
    TArray<FSoccerBenchCandidate> BenchCandidates;
    for (USoccerPlayerProfile* PlayerProfile : AvailableProfiles)
    {
        if (StarterIds.Contains(PlayerProfile->Identity.PlayerId))
        {
            continue;
        }

        int32 BestSlotScore = 0;
        for (const FSoccerFormationSlot& Slot : BestFormation.Slots)
        {
            FSoccerCoachLineupDecision Decision;
            BestSlotScore = FMath::Max(
                BestSlotScore,
                CalculateCandidateScore(
                    CoachProfile,
                    PlayerProfile,
                    BestPlan.FormationSystem,
                    Slot.SlotId,
                    Decision
                )
            );
        }

        int32 FamiliarPositions = 0;
        for (const FSoccerPlayerPositionPreference& PositionPreference :
            PlayerProfile->PositionPreferences)
        {
            if (PositionPreference.Familiarity >= 60)
            {
                ++FamiliarPositions;
            }
        }

        const float VersatilityAlpha = FMath::Clamp(
            static_cast<float>(CoachProfile->SelectionCriteria.BenchVersatilityPreference) /
                100.0f,
            0.0f,
            1.0f
        );
        FSoccerBenchCandidate BenchCandidate;
        BenchCandidate.PlayerId = PlayerProfile->Identity.PlayerId;
        BenchCandidate.Score = FMath::Clamp(
            FMath::RoundToInt(
                static_cast<float>(BestSlotScore) * (1.0f - 0.15f * VersatilityAlpha) +
                static_cast<float>(FMath::Min(FamiliarPositions, 4) * 25) *
                    (0.15f * VersatilityAlpha)
            ),
            0,
            100
        );
        BenchCandidates.Add(BenchCandidate);
    }

    BenchCandidates.Sort([](
        const FSoccerBenchCandidate& Left,
        const FSoccerBenchCandidate& Right
    )
    {
        return Left.Score != Right.Score
            ? Left.Score > Right.Score
            : IsPlayerIdBefore(Left.PlayerId, Right.PlayerId);
    });
    for (const FSoccerBenchCandidate& BenchCandidate : BenchCandidates)
    {
        BestPlan.BenchPlayerIds.Add(BenchCandidate.PlayerId);
    }

    OutPlan = BestPlan;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("[CoachPlan] coach=%s club=%s formation=%d planScore=%d lineupScore=%d formationPreference=%d starters=%d bench=%d."),
        *OutPlan.CoachId.ToString(),
        *OutPlan.ClubId.ToString(),
        static_cast<int32>(OutPlan.FormationSystem),
        OutPlan.OverallPlanScore,
        OutPlan.StartingLineupScore,
        OutPlan.FormationPreferenceScore,
        OutPlan.StartingLineupBySlot.Num(),
        OutPlan.BenchPlayerIds.Num()
    );
    for (const FSoccerCoachLineupDecision& Decision : OutPlan.LineupDecisions)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("[CoachPlan] slot=%s player=%s total=%d ability=%d familiarity=%d tacticalFit=%d."),
            *Decision.FormationSlotId.ToString(),
            *Decision.PlayerId.ToString(),
            Decision.TotalScore,
            Decision.AbilityScore,
            Decision.PositionFamiliarity,
            Decision.TacticalFitScore
        );
    }
    return true;
}

int32 USoccerCoachPlanningLibrary::CalculateTacticalFitScore(
    const USoccerCoachProfile* CoachProfile,
    const USoccerPlayerProfile* PlayerProfile
)
{
    if (!IsValid(CoachProfile) || !IsValid(PlayerProfile))
    {
        return 50;
    }

    const FSoccerPlayerAttributes& A = PlayerProfile->Attributes;
    const FSoccerCoachPhilosophy& P = CoachProfile->Philosophy;
    float WeightedTotal = 0.0f;
    float WeightTotal = 0.0f;
    auto Add = [&WeightedTotal, &WeightTotal](int32 Score, int32 Weight)
    {
        const float SafeWeight = static_cast<float>(FMath::Clamp(Weight, 0, 100));
        WeightedTotal += static_cast<float>(FMath::Clamp(Score, 0, 100)) * SafeWeight;
        WeightTotal += SafeWeight;
    };

    Add(AverageValues({
        A.Technical.PassingAccuracy,
        A.Technical.BallControl,
        A.Tactical.DecisionMaking,
        A.Tactical.Composure
    }), P.PossessionPreference);
    Add(AverageValues({
        A.Physical.Pace,
        A.Physical.Acceleration,
        A.Tactical.OffBallPositioning,
        A.Technical.ShotPower,
        A.Technical.AerialAbility
    }), P.Directness);
    Add(AverageValues({
        A.Physical.Stamina,
        A.Physical.StaminaRecovery,
        A.Tactical.DefensiveReaction,
        A.Technical.Tackling,
        A.Tactical.Marking
    }), P.PressingIntensity);
    Add(AverageValues({
        A.Tactical.DefensivePositioning,
        A.Tactical.Marking,
        A.Technical.Tackling,
        A.Tactical.Anticipation,
        A.Physical.Strength
    }), FMath::RoundToInt(
        (static_cast<float>(100 - P.AttackingIntent) +
         static_cast<float>(P.Compactness)) * 0.5f
    ));
    Add(AverageValues({
        A.Technical.ShootingAccuracy,
        A.Tactical.OffBallPositioning,
        A.Technical.Dribbling,
        A.Physical.Pace,
        A.Tactical.DecisionMaking
    }), P.AttackingIntent);
    Add(AverageValues({
        A.Physical.Pace,
        A.Physical.Acceleration,
        A.Tactical.OffBallPositioning,
        A.Tactical.Anticipation,
        A.Technical.PassingAccuracy
    }), P.CounterAttackPreference);

    return WeightTotal > KINDA_SMALL_NUMBER
        ? FMath::Clamp(FMath::RoundToInt(WeightedTotal / WeightTotal), 0, 100)
        : 50;
}

int32 USoccerCoachPlanningLibrary::CalculateCandidateScore(
    const USoccerCoachProfile* CoachProfile,
    const USoccerPlayerProfile* PlayerProfile,
    ESoccerFormationSystem FormationSystem,
    FName FormationSlotId,
    FSoccerCoachLineupDecision& OutDecision
)
{
    OutDecision = FSoccerCoachLineupDecision();
    OutDecision.FormationSlotId = FormationSlotId;
    OutDecision.PlayerId = IsValid(PlayerProfile)
        ? PlayerProfile->Identity.PlayerId
        : NAME_None;
    if (!IsValid(CoachProfile) || !IsValid(PlayerProfile))
    {
        return 0;
    }

    const FSoccerPlayerSlotSuitability Suitability =
        USoccerLineupEvaluationLibrary::EvaluatePlayerForFormationSlot(
            PlayerProfile,
            FormationSystem,
            FormationSlotId
        );
    if (!Suitability.bValid)
    {
        return 0;
    }

    OutDecision.AbilityScore = Suitability.AbilityScore;
    OutDecision.PositionFamiliarity = Suitability.PositionFamiliarity;
    OutDecision.TacticalFitScore = CalculateTacticalFitScore(
        CoachProfile,
        PlayerProfile
    );

    const FSoccerCoachSelectionCriteria& Criteria =
        CoachProfile->SelectionCriteria;
    const int32 PhysicalReadinessProxy = AverageValues({
        PlayerProfile->Attributes.Physical.Stamina,
        PlayerProfile->Attributes.Physical.StaminaRecovery
    });

    float WeightedTotal = 0.0f;
    float WeightTotal = 0.0f;
    auto Add = [&WeightedTotal, &WeightTotal](int32 Score, int32 Weight)
    {
        const float SafeWeight = static_cast<float>(FMath::Clamp(Weight, 0, 100));
        WeightedTotal += static_cast<float>(FMath::Clamp(Score, 0, 100)) * SafeWeight;
        WeightTotal += SafeWeight;
    };
    Add(OutDecision.AbilityScore, Criteria.CurrentAbilityPriority);
    Add(OutDecision.PositionFamiliarity, Criteria.NaturalPositionPriority);
    Add(OutDecision.TacticalFitScore, Criteria.TacticalFitPriority);
    Add(PhysicalReadinessProxy, Criteria.PhysicalConditionPriority);

    const int32 WeightedScore = WeightTotal > KINDA_SMALL_NUMBER
        ? FMath::RoundToInt(WeightedTotal / WeightTotal)
        : Suitability.OverallScore;
    const float EvaluationAlpha = FMath::Clamp(
        static_cast<float>(CoachProfile->Abilities.PlayerEvaluation) / 100.0f,
        0.0f,
        1.0f
    );
    OutDecision.TotalScore = FMath::Clamp(
        FMath::RoundToInt(
            static_cast<float>(Suitability.OverallScore) * (1.0f - EvaluationAlpha) +
            static_cast<float>(WeightedScore) * EvaluationAlpha
        ),
        0,
        100
    );
    return OutDecision.TotalScore;
}
