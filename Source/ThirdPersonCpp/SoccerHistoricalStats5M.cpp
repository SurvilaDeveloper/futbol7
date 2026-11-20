#include "SoccerHistoricalStats5M.h"

namespace SoccerHistoricalAggregate5M
{
    static FString ClubKey(const FString& Club) { return Club.TrimStartAndEnd().ToLower(); }
    static FString PlayerKey(const FSoccerPlayerMatchStatistics5G& P)
    {
        // PlayerKey from the live game can include team-specific prefixes.
        // Archived PlayerId is stable inside the club, including substitutions.
        return !P.PlayerId.IsNone() ? P.PlayerId.ToString().ToLower()
            : (P.DisplayName.TrimStartAndEnd().ToLower() + TEXT("#") + FString::FromInt(P.ShirtNumber));
    }
}

void FSoccerHistoricalStats5M::Aggregate(
    const TArray<FSoccerArchivedMatch5L>& Matches,
    TArray<FSoccerClubTotals5M>& Clubs,
    TArray<FSoccerPlayerTotals5M>& Players)
{
    Clubs.Reset(); Players.Reset();
    TMap<FString, int32> ClubIndices;
    TMap<FString, int32> PlayerIndices;
    TSet<FString> SeenMatches;
    for (const FSoccerArchivedMatch5L& Match : Matches)
    {
        if (!Match.MatchId.IsEmpty() && SeenMatches.Contains(Match.MatchId)) continue;
        if (!Match.MatchId.IsEmpty()) SeenMatches.Add(Match.MatchId);
        const FSoccerFinalMatchReport5K& Report = Match.Report;
        if (Report.LocalClubName.IsEmpty() || Report.RivalClubName.IsEmpty()) continue;
        const FString Names[2] = {Report.LocalClubName, Report.RivalClubName};
        const int32 Scores[2] = {Report.LocalScore, Report.RivalScore};
        const FSoccerMatchTeamStatistics5D* Stats[2] = {
            &Report.TeamStatistics.PlayerTeam, &Report.TeamStatistics.OpponentTeam};
        for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
        {
            const FString Key = SoccerHistoricalAggregate5M::ClubKey(Names[TeamIndex]);
            int32* Existing = ClubIndices.Find(Key);
            int32 Index = INDEX_NONE;
            if (Existing) Index = *Existing;
            else
            {
                Index = Clubs.AddDefaulted();
                Clubs[Index].Name = Names[TeamIndex];
                ClubIndices.Add(Key, Index);
            }
            FSoccerClubTotals5M& C = Clubs[Index];
            const int32 A = Scores[TeamIndex], B = Scores[1-TeamIndex];
            ++C.Played;
            if (A > B) ++C.Wins; else if (A < B) ++C.Losses; else ++C.Draws;
            C.GoalsFor += A; C.GoalsAgainst += B;
            C.Shots += Stats[TeamIndex]->Shots;
            C.ShotsOnTarget += Stats[TeamIndex]->ShotsOnTarget;
            C.Saves += Stats[TeamIndex]->GoalkeeperSaves;
            C.Corners += Stats[TeamIndex]->CornersWon;
            C.FreeKicks += Stats[TeamIndex]->FreeKicksWon;
        }
        for (const FSoccerPlayerMatchStatistics5G& P : Report.Players)
        {
            const FString Club = (P.Team == ESoccerTeam::PlayerTeam)
                ? Report.LocalClubName : Report.RivalClubName;
            const FString Stable = SoccerHistoricalAggregate5M::ClubKey(Club) + TEXT("|") +
                SoccerHistoricalAggregate5M::PlayerKey(P);
            int32* Existing = PlayerIndices.Find(Stable);
            int32 Index = INDEX_NONE;
            if (Existing) Index = *Existing;
            else
            {
                Index = Players.AddDefaulted();
                FSoccerPlayerTotals5M& N = Players[Index];
                N.Club = Club; N.PlayerKey = Stable; N.Name = P.DisplayName;
                PlayerIndices.Add(Stable, Index);
            }
            FSoccerPlayerTotals5M& N = Players[Index];
            ++N.Records;
            N.Goals += P.Goals; N.Assists += P.Assists; N.OwnGoals += P.OwnGoals;
            N.Shots += P.Shots; N.ShotsOnTarget += P.ShotsOnTarget;
            N.Saves += P.Saves; N.PassesAttempted += P.PassesAttempted;
            N.PassesCompleted += P.PassesCompleted; N.Recoveries += P.Recoveries;
            N.Interceptions += P.Interceptions; N.CleanTackles += P.CleanTackles;
            N.RatingSum += ASoccerMatchManager::CalculatePlayerRating5J(P);
        }
    }
    Clubs.Sort([](const FSoccerClubTotals5M& A, const FSoccerClubTotals5M& B)
    {
        if (A.Played != B.Played) return A.Played > B.Played;
        return A.Name < B.Name;
    });
    Players.Sort([](const FSoccerPlayerTotals5M& A, const FSoccerPlayerTotals5M& B)
    {
        if (A.Goals != B.Goals) return A.Goals > B.Goals;
        if (A.Assists != B.Assists) return A.Assists > B.Assists;
        return A.Name < B.Name;
    });
}
