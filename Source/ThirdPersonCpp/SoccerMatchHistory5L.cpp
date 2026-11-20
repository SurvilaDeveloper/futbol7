#include "SoccerMatchHistory5L.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace SoccerHistoryStorage5L
{
    using FObject = TSharedPtr<FJsonObject>;

    double Number(const FObject& Object, const TCHAR* Key)
    {
        double Value = 0.0;
        if (Object.IsValid()) Object->TryGetNumberField(Key, Value);
        return Value;
    }
    int32 Integer(const FObject& Object, const TCHAR* Key)
    {
        return FMath::RoundToInt(Number(Object, Key));
    }
    FString String(const FObject& Object, const TCHAR* Key)
    {
        FString Value;
        if (Object.IsValid()) Object->TryGetStringField(Key, Value);
        return Value;
    }
    bool Boolean(const FObject& Object, const TCHAR* Key)
    {
        bool Value = false;
        if (Object.IsValid()) Object->TryGetBoolField(Key, Value);
        return Value;
    }
    FObject Object(const FObject& Parent, const TCHAR* Key)
    {
        if (!Parent.IsValid()) return nullptr;
        const TSharedPtr<FJsonObject>* Value = nullptr;
        return Parent->TryGetObjectField(Key, Value) && Value ? *Value : nullptr;
    }
    TArray<TSharedPtr<FJsonValue>> Array(const FObject& Parent, const TCHAR* Key)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        return Parent.IsValid() && Parent->TryGetArrayField(Key, Values) && Values
            ? *Values : TArray<TSharedPtr<FJsonValue>>();
    }

    FObject WriteTeam(const FSoccerMatchTeamStatistics5D& S)
    {
        FObject O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("PossessionSeconds"), S.ControlledPossessionSeconds);
        O->SetNumberField(TEXT("FreeKicks"), S.FreeKicksWon);
        O->SetNumberField(TEXT("Offsides"), S.OffsidesCommitted);
        O->SetNumberField(TEXT("Penalties"), S.PenaltiesWon);
        O->SetNumberField(TEXT("Corners"), S.CornersWon);
        O->SetNumberField(TEXT("ThrowIns"), S.ThrowInsWon);
        O->SetNumberField(TEXT("GoalKicks"), S.GoalKicksWon);
        O->SetNumberField(TEXT("Shots"), S.Shots);
        O->SetNumberField(TEXT("ShotsOnTarget"), S.ShotsOnTarget);
        O->SetNumberField(TEXT("Saves"), S.GoalkeeperSaves);
        return O;
    }
    FSoccerMatchTeamStatistics5D ReadTeam(const FObject& O)
    {
        FSoccerMatchTeamStatistics5D S;
        S.ControlledPossessionSeconds = static_cast<float>(Number(O, TEXT("PossessionSeconds")));
        S.FreeKicksWon = Integer(O, TEXT("FreeKicks"));
        S.OffsidesCommitted = Integer(O, TEXT("Offsides"));
        S.PenaltiesWon = Integer(O, TEXT("Penalties"));
        S.CornersWon = Integer(O, TEXT("Corners"));
        S.ThrowInsWon = Integer(O, TEXT("ThrowIns"));
        S.GoalKicksWon = Integer(O, TEXT("GoalKicks"));
        S.Shots = Integer(O, TEXT("Shots"));
        S.ShotsOnTarget = Integer(O, TEXT("ShotsOnTarget"));
        S.GoalkeeperSaves = Integer(O, TEXT("Saves"));
        return S;
    }
    FObject WritePlayer(const FSoccerPlayerMatchStatistics5G& P)
    {
        FObject O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("Key"), P.PlayerKey);
        O->SetStringField(TEXT("Id"), P.PlayerId.ToString());
        O->SetStringField(TEXT("Name"), P.DisplayName);
        O->SetNumberField(TEXT("Team"), static_cast<int32>(P.Team));
        O->SetNumberField(TEXT("Shirt"), P.ShirtNumber);
        O->SetNumberField(TEXT("Goals"), P.Goals);
        O->SetNumberField(TEXT("OwnGoals"), P.OwnGoals);
        O->SetNumberField(TEXT("Assists"), P.Assists);
        O->SetNumberField(TEXT("Shots"), P.Shots);
        O->SetNumberField(TEXT("ShotsOnTarget"), P.ShotsOnTarget);
        O->SetNumberField(TEXT("Saves"), P.Saves);
        O->SetNumberField(TEXT("PassesAttempted"), P.PassesAttempted);
        O->SetNumberField(TEXT("PassesCompleted"), P.PassesCompleted);
        O->SetNumberField(TEXT("Recoveries"), P.Recoveries);
        O->SetNumberField(TEXT("Interceptions"), P.Interceptions);
        O->SetNumberField(TEXT("CleanTackles"), P.CleanTackles);
        return O;
    }
    FSoccerPlayerMatchStatistics5G ReadPlayer(const FObject& O)
    {
        FSoccerPlayerMatchStatistics5G P;
        P.PlayerKey = String(O, TEXT("Key"));
        P.PlayerId = FName(*String(O, TEXT("Id")));
        P.DisplayName = String(O, TEXT("Name"));
        P.Team = Integer(O, TEXT("Team")) == 1
            ? ESoccerTeam::OpponentTeam : ESoccerTeam::PlayerTeam;
        P.ShirtNumber = Integer(O, TEXT("Shirt"));
        P.Goals = Integer(O, TEXT("Goals"));
        P.OwnGoals = Integer(O, TEXT("OwnGoals"));
        P.Assists = Integer(O, TEXT("Assists"));
        P.Shots = Integer(O, TEXT("Shots"));
        P.ShotsOnTarget = Integer(O, TEXT("ShotsOnTarget"));
        P.Saves = Integer(O, TEXT("Saves"));
        P.PassesAttempted = Integer(O, TEXT("PassesAttempted"));
        P.PassesCompleted = Integer(O, TEXT("PassesCompleted"));
        P.Recoveries = Integer(O, TEXT("Recoveries"));
        P.Interceptions = Integer(O, TEXT("Interceptions"));
        P.CleanTackles = Integer(O, TEXT("CleanTackles"));
        return P;
    }
    FObject WriteEvent(const FSoccerMatchTimelineEvent5I& E)
    {
        FObject O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("Sequence"), E.Sequence);
        O->SetNumberField(TEXT("Seconds"), E.MatchSeconds);
        O->SetNumberField(TEXT("Period"), static_cast<int32>(E.Period));
        O->SetBoolField(TEXT("HasTeam"), E.bHasTeam);
        O->SetNumberField(TEXT("Team"), static_cast<int32>(E.Team));
        O->SetStringField(TEXT("Title"), E.Title);
        O->SetStringField(TEXT("Detail"), E.Detail);
        return O;
    }
    FSoccerMatchTimelineEvent5I ReadEvent(const FObject& O)
    {
        FSoccerMatchTimelineEvent5I E;
        E.Sequence = Integer(O, TEXT("Sequence"));
        E.MatchSeconds = static_cast<float>(Number(O, TEXT("Seconds")));
        E.Period = static_cast<ESoccerMatchPeriod>(Integer(O, TEXT("Period")));
        E.bHasTeam = Boolean(O, TEXT("HasTeam"));
        E.Team = Integer(O, TEXT("Team")) == 1
            ? ESoccerTeam::OpponentTeam : ESoccerTeam::PlayerTeam;
        E.Title = String(O, TEXT("Title"));
        E.Detail = String(O, TEXT("Detail"));
        return E;
    }
}

FString FSoccerMatchHistory5L::GetHistoryDirectory()
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SoccerMatchHistory"));
}

bool FSoccerMatchHistory5L::SaveCompletedMatch(const FSoccerFinalMatchReport5K& Report)
{
    using namespace SoccerHistoryStorage5L;
    const FString Directory = GetHistoryDirectory();
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
    {
        UE_LOG(LogTemp, Warning, TEXT("Soccer 5L: cannot create history folder: %s"), *Directory);
        return false;
    }
    const FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    const FString Filename = FPaths::Combine(Directory, Timestamp + TEXT("_") + Id + TEXT(".json"));
    FObject Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("Version"), 1);
    Root->SetStringField(TEXT("MatchId"), Id);
    Root->SetStringField(TEXT("PlayedAt"), FDateTime::Now().ToString(TEXT("%d/%m/%Y %H:%M")));
    Root->SetStringField(TEXT("LocalClub"), Report.LocalClubName);
    Root->SetStringField(TEXT("RivalClub"), Report.RivalClubName);
    Root->SetNumberField(TEXT("LocalScore"), Report.LocalScore);
    Root->SetNumberField(TEXT("RivalScore"), Report.RivalScore);
    Root->SetNumberField(TEXT("PlayedSeconds"), Report.PlayedSeconds);
    Root->SetBoolField(TEXT("HasBestPlayer"), Report.bHasBestPlayer);
    Root->SetStringField(TEXT("BestPlayer"), Report.BestPlayerName);
    Root->SetNumberField(TEXT("BestTeam"), static_cast<int32>(Report.BestPlayerTeam));
    Root->SetNumberField(TEXT("BestRating"), Report.BestPlayerRating);
    Root->SetObjectField(TEXT("LocalStats"), WriteTeam(Report.TeamStatistics.PlayerTeam));
    Root->SetObjectField(TEXT("RivalStats"), WriteTeam(Report.TeamStatistics.OpponentTeam));
    TArray<TSharedPtr<FJsonValue>> Players;
    for (const FSoccerPlayerMatchStatistics5G& P : Report.Players)
        Players.Add(MakeShared<FJsonValueObject>(WritePlayer(P)));
    Root->SetArrayField(TEXT("Players"), Players);
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const FSoccerMatchTimelineEvent5I& E : Report.Events)
        Events.Add(MakeShared<FJsonValueObject>(WriteEvent(E)));
    Root->SetArrayField(TEXT("Events"), Events);

    FString Contents;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Contents);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer)) return false;
    const bool bSaved = FFileHelper::SaveStringToFile(Contents, *Filename,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (!bSaved)
    {
        UE_LOG(LogTemp, Warning, TEXT("Soccer 5L: failed to save %s"), *Filename);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Soccer 5L: saved %s"), *Filename);
    }
    return bSaved;
}

void FSoccerMatchHistory5L::LoadRecent(TArray<FSoccerArchivedMatch5L>& OutMatches, int32 MaxMatches)
{
    using namespace SoccerHistoryStorage5L;
    OutMatches.Empty();
    const FString Directory = GetHistoryDirectory();
    TArray<FString> Names;
    IFileManager::Get().FindFiles(Names, *FPaths::Combine(Directory, TEXT("*.json")), true, false);
    Names.Sort([](const FString& A, const FString& B) { return A > B; });
    for (const FString& Name : Names)
    {
        if (OutMatches.Num() >= FMath::Clamp(MaxMatches, 1, 500)) break;
        FString Contents;
        if (!FFileHelper::LoadFileToString(Contents, *FPaths::Combine(Directory, Name))) continue;
        FObject Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
            Integer(Root, TEXT("Version")) != 1) continue;
        FSoccerArchivedMatch5L Saved;
        Saved.MatchId = String(Root, TEXT("MatchId"));
        Saved.PlayedAt = String(Root, TEXT("PlayedAt"));
        Saved.Report.LocalClubName = String(Root, TEXT("LocalClub"));
        Saved.Report.RivalClubName = String(Root, TEXT("RivalClub"));
        Saved.Report.LocalScore = Integer(Root, TEXT("LocalScore"));
        Saved.Report.RivalScore = Integer(Root, TEXT("RivalScore"));
        Saved.Report.PlayedSeconds = static_cast<float>(Number(Root, TEXT("PlayedSeconds")));
        Saved.Report.bHasBestPlayer = Boolean(Root, TEXT("HasBestPlayer"));
        Saved.Report.BestPlayerName = String(Root, TEXT("BestPlayer"));
        Saved.Report.BestPlayerTeam = Integer(Root, TEXT("BestTeam")) == 1
            ? ESoccerTeam::OpponentTeam : ESoccerTeam::PlayerTeam;
        Saved.Report.BestPlayerRating = static_cast<float>(Number(Root, TEXT("BestRating")));
        Saved.Report.TeamStatistics.PlayerTeam = ReadTeam(Object(Root, TEXT("LocalStats")));
        Saved.Report.TeamStatistics.OpponentTeam = ReadTeam(Object(Root, TEXT("RivalStats")));
        for (const TSharedPtr<FJsonValue>& Item : Array(Root, TEXT("Players")))
            if (Item.IsValid() && Item->AsObject().IsValid())
                Saved.Report.Players.Add(ReadPlayer(Item->AsObject()));
        for (const TSharedPtr<FJsonValue>& Item : Array(Root, TEXT("Events")))
            if (Item.IsValid() && Item->AsObject().IsValid())
                Saved.Report.Events.Add(ReadEvent(Item->AsObject()));
        if (!Saved.Report.LocalClubName.IsEmpty() && !Saved.Report.RivalClubName.IsEmpty())
            OutMatches.Add(MoveTemp(Saved));
    }
}
