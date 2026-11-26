#include "Rondo/SoccerRondoGameMode.h"
#include "Rondo/SoccerRondoHUD.h"
#include "Rondo/SoccerRondoResultWidget8.h"
#include "Blueprint/UserWidget.h"
#include "SoccerField.h"
#include "SoccerFieldDimensions.h"
#include "SoccerBall.h"
#include "SoccerPassShared.h"
#include "SoccerCharacterBase.h"
#include "SoccerAICharacter.h"
#include "SoccerAIController.h"
#include "SoccerMatchManager.h"
#include "SoccerGameInstance.h"
#include "SoccerSquadCatalog.h"
#include "SoccerClubProfile.h"
#include "SoccerPlayerProfile.h"
#include "SoccerPlayerAppearanceCatalog.h"
#include "AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "Materials/MaterialInterface.h"
#include "ThirdPersonCppCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ASoccerRondoGameMode::ASoccerRondoGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    HUDClass = ASoccerRondoHUD::StaticClass();
    bPauseable = true;
    ParticipantKits.SetNum(7);
    PlayerControllerClass = APlayerController::StaticClass();
    DefaultPawnClass = AThirdPersonCppCharacter::StaticClass();
    static ConstructorHelpers::FClassFinder<APawn> HumanBP(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
    if (HumanBP.Succeeded()) DefaultPawnClass = HumanBP.Class;
}
ASoccerRondoGameMode* ASoccerRondoGameMode::Find(const UObject* Context)
{
    UWorld* World = Context ? Context->GetWorld() : nullptr;
    return World ? Cast<ASoccerRondoGameMode>(World->GetAuthGameMode()) : nullptr;
}
void ASoccerRondoGameMode::StartPlay()
{
    Super::StartPlay();
    FTimerDelegate Init; Init.BindUObject(this,&ASoccerRondoGameMode::InitializeRondo);
    GetWorldTimerManager().SetTimerForNextTick(Init);
}
bool ASoccerRondoGameMode::BuildClubRoster(TArray<USoccerPlayerProfile*>& OutRoster)
{
    OutRoster.Reset();
    auto* GI=Cast<USoccerGameInstance>(GetWorld()->GetGameInstance());
    if (!IsValid(GI))
    {
        UE_LOG(LogTemp,Error,TEXT("[Rondo5] SoccerGameInstance required to resolve club players."));
        return false;
    }
    auto* Squad=GI->FindSquadCatalogByClubId(RondoClubId);
    // The source documents CLUB_BLUE as an example. Some assets may instead
    // use CLUB_AZUL or another ID; accept only an unambiguous exact Azul name
    // when the default ID is missing, never an arbitrary opponent catalog.
    if (!IsValid(Squad) && RondoClubId==FName(TEXT("CLUB_BLUE")))
    {
        for (const FName Id:GI->GetAvailableClubIds())
        {
            auto* Candidate=GI->FindSquadCatalogByClubId(Id);
            if (!IsValid(Candidate) || !IsValid(Candidate->ClubProfile)) continue;
            const FString Name=Candidate->ClubProfile->DisplayName.ToString();
            const FString Short=Candidate->ClubProfile->ShortName.ToString();
            if (Id!=FName(TEXT("CLUB_AZUL")) && !Name.Equals(TEXT("Azul"),ESearchCase::IgnoreCase) &&
                !Name.Equals(TEXT("Club Azul"),ESearchCase::IgnoreCase) && !Short.Equals(TEXT("Azul"),ESearchCase::IgnoreCase)) continue;
            if (IsValid(Squad))
            {
                UE_LOG(LogTemp,Error,TEXT("[Rondo5] Multiple Azul catalogs; set RondoClubId explicitly."));
                return false;
            }
            Squad=Candidate;
        }
    }
    if (!IsValid(Squad) || !IsValid(Squad->ClubProfile))
    {
        UE_LOG(LogTemp,Error,TEXT("[Rondo5] Club %s not found. Set RondoClubId to the Azul ClubId."),*RondoClubId.ToString());
        return false;
    }
    RondoClub=Squad->ClubProfile;
    if (!RondoClub->HasCompleteKit(ESoccerClubKitType::Home))
        UE_LOG(LogTemp,Warning,TEXT("[Rondo5] Club home kit incomplete; missing slots retain existing materials."));
    TMap<FName,USoccerPlayerProfile*> Eligible;
    for (auto* Profile:Squad->PlayerProfiles)
    {
        if (!IsValid(Profile) || !Profile->HasValidPlayerId()) continue;
        const FName Id=Profile->Identity.PlayerId;
        // Use the same canonical registry as the match. Reject ambiguous IDs.
        if (GI->FindPlayerProfileById(Id)!=Profile || GI->GetInitialClubIdForPlayer(Id)!=Squad->GetClubId()) continue;
        Eligible.Add(Id,Profile);
    }
    if (Eligible.Num()<7)
    {
        UE_LOG(LogTemp,Error,TEXT("[Rondo5] Club=%s requires 7 distinct profiles, found=%d."),*Squad->GetClubId().ToString(),Eligible.Num());
        return false;
    }
    TArray<USoccerPlayerProfile*> Ordered;
    auto AddId=[&](FName Id)
    {
        if (auto* Found=Eligible.Find(Id)) Ordered.AddUnique(*Found);
    };
    // Prefer the selected lineup/bench when its IDs belong to this club.
    for (const FName Id:GI->GetStartingPlayerIdsInFormationOrder()) AddId(Id);
    for (const FName Id:GI->GetBenchPlayerIds()) AddId(Id);
    for (auto* Profile:Squad->PlayerProfiles) if (IsValid(Profile)) AddId(Profile->Identity.PlayerId);
    // Keep the human's current profile if it belongs to Azul. Otherwise choose
    // a field profile. Prefer field players for all slots; use the remaining
    // club players only if there are fewer than seven field profiles.
    if (auto* Existing=Eligible.Find(Human->GetPlayerProfileId())) OutRoster.Add(*Existing);
    auto IsGoalkeeper=[](const USoccerPlayerProfile* Profile)
    {
        const int32 GK=Profile->GetPositionFamiliarity(ESoccerPlayerNaturalPosition::Goalkeeper);
        if (GK<=0) return false;
        for (const auto& Preference:Profile->PositionPreferences)
            if (Preference.Position!=ESoccerPlayerNaturalPosition::Goalkeeper && Preference.Familiarity>=GK) return false;
        return true;
    };
    for (auto* Profile:Ordered)
        if (OutRoster.Num()<7 && !IsGoalkeeper(Profile)) OutRoster.AddUnique(Profile);
    for (auto* Profile:Ordered)
        if (OutRoster.Num()<7) OutRoster.AddUnique(Profile);
    return OutRoster.Num()==7;
}

void ASoccerRondoGameMode::ResolveAppearanceCatalog()
{
    if (IsValid(PlayerAppearanceCatalog)) return;
    FAssetRegistryModule& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> Assets;
    Registry.Get().GetAssetsByClass(USoccerPlayerAppearanceCatalog::StaticClass()->GetFName(),Assets,true);
    if (Assets.Num()==1) PlayerAppearanceCatalog=Cast<USoccerPlayerAppearanceCatalog>(Assets[0].GetAsset());
    if (!IsValid(PlayerAppearanceCatalog))
        UE_LOG(LogTemp,Warning,TEXT("[Rondo5] Appearance catalogs=%d; assign PlayerAppearanceCatalog in GameMode for BodyVariantId meshes."),Assets.Num());
}

USkeletalMesh* ASoccerRondoGameMode::ResolvePlayerBodyVariantMesh(FName BodyVariantId) const
{
    return IsValid(PlayerAppearanceCatalog)?PlayerAppearanceCatalog->LoadBodyVariantMesh(BodyVariantId):nullptr;
}

FString ASoccerRondoGameMode::GetParticipantName(const ASoccerCharacterBase* Player)
{
    if (!IsValid(Player)) return TEXT("-");
    const auto* Profile=Player->GetPlayerProfile();
    if (!IsValid(Profile)) return GetNameSafe(Player);
    return Profile->Identity.DisplayName.IsEmpty()?Profile->Identity.PlayerId.ToString():Profile->Identity.DisplayName.ToString();
}

void ASoccerRondoGameMode::InitializeRondo()
{
    UWorld* World = GetWorld();
    if (!World || bReady) return;
    for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
    { UE_LOG(LogTemp,Error,TEXT("[Rondo1] Remove SoccerMatchManager from this map.")); return; }
    for (TActorIterator<ASoccerField> It(World); It; ++It) { Field=*It; break; }
    for (TActorIterator<ASoccerBall> It(World); It; ++It)
    {
        if (Ball) { UE_LOG(LogTemp,Error,TEXT("[Rondo1] Exactly one ball required.")); return; }
        Ball=*It;
    }
    auto* PC=World->GetFirstPlayerController();
    Human=PC?Cast<AThirdPersonCppCharacter>(PC->GetPawn()):nullptr;
    if (!Field || !Human || !FieldPlayerClass)
    { UE_LOG(LogTemp,Error,TEXT("[Rondo1] Missing field, human or FieldPlayerClass.")); return; }
    for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
    { UE_LOG(LogTemp,Error,TEXT("[Rondo1] Remove preplaced AI characters; mode spawns six.")); return; }
    TArray<USoccerPlayerProfile*> Roster;
    if (!BuildClubRoster(Roster)) return;
    ResolveAppearanceCatalog();
    Radius=SoccerFieldDimensions::PitchWidthCm*FMath::Clamp(CircleDiameterWidthFraction,0.2f,1.f)*0.5f;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (!Ball && BallClass) Ball=World->SpawnActor<ASoccerBall>(BallClass,Field->GetPitchCenterWorldLocation(70),Field->GetActorRotation(),Params);
    if (!Ball) { UE_LOG(LogTemp,Error,TEXT("[Rondo1] Place BP_SoccerBall in map or assign BallClass.")); return; }
    Ball->SetSessionDribbleHorizontalScale(DribbleHorizontalScale);
    Ball->ConfigureSessionKickAudio(RondoShortKickSound, RondoStrongKickSound, BallClass);
    Human->BindRondoBall(Ball);
    // Initial placements are the only teleports performed by this mode.
    Human->SetActorLocation(Field->PitchLocalToWorld(FVector(Radius*0.58f,0,Human->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f)),false,nullptr,ETeleportType::TeleportPhysics);
    Players.Add(Human);
    for(int32 i=0;i<6;++i)
    {
        const float Angle=2.f*PI*float(i+1)/7.f;
        const FVector Local(Radius*0.58f*FMath::Cos(Angle),Radius*0.58f*FMath::Sin(Angle),115.f);
        auto* Bot=World->SpawnActor<ASoccerAICharacter>(FieldPlayerClass,Field->PitchLocalToWorld(Local),Field->GetActorRotation(),Params);
        if (!Bot) { UE_LOG(LogTemp,Error,TEXT("[Rondo1] Bot spawn failed.")); return; }
        Bot->SpawnDefaultController();
        auto* AI=Cast<ASoccerAIController>(Bot->GetController());
        if (!AI) { UE_LOG(LogTemp,Error,TEXT("[Rondo1] FieldPlayerClass needs SoccerAIController.")); return; }
        AI->SetActorTickEnabled(false);
        Players.Add(Bot);
    }
    // Use the existing match profile setter once per actor, never on role changes.
    for (int32 Index=0;Index<Players.Num();++Index)
    {
        auto* P=Players[Index];
        P->SetPlayerProfileForMatch(Roster[Index]);
        TouchScores.Add(P->GetPlayerProfileId(), 0);
        UE_LOG(LogTemp,Display,TEXT("[Rondo5] PLAYER club=%s id=%s name=%s number=%d actor=%s human=%d"),
            *RondoClub->ClubId.ToString(),*P->GetPlayerProfileId().ToString(),*GetParticipantName(P),
            Roster[Index]->Identity.ShirtNumber,*GetNameSafe(P),Index==0?1:0);
    }
    Loco=bHumanStartsAsLoco?static_cast<ASoccerCharacterBase*>(Human):Players.Last();
    Loco->SetActorLocation(Field->GetPitchCenterWorldLocation(Loco->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f),false,nullptr,ETeleportType::TeleportPhysics);
    auto* InitialCarrier=Cast<ASoccerAICharacter>(Players[1]);
    FVector InitialBallLocal=Field->WorldToPitchLocal(InitialCarrier->GetActorLocation()+InitialCarrier->GetActorForwardVector().GetSafeNormal2D()*45.f);
    InitialBallLocal.Z=Ball->GetBallRadiusCm()+2.f;
    Ball->SetActorLocation(Field->PitchLocalToWorld(InitialBallLocal),false,nullptr,ETeleportType::TeleportPhysics);
    Ball->StopBallKeepingPhysics();
    UpdateTeams();
    PC->bShowMouseCursor=false;
    PC->SetInputMode(FInputModeGameOnly());
    StartTime=World->GetTimeSeconds();
    bReady=true;
    // The ball starts at the feet of a non-loco; use the existing logical possession.
    InitialCarrier->PossessAIBall(Ball);
    // Initial scripted placement is not earned contact.
    bScoringActive=true;
    UE_LOG(LogTemp,Display,TEXT("[Rondo7] SESSION duration=%.1f players=%d"),SessionDurationSeconds,Players.Num());
    UE_LOG(LogTemp,Display,TEXT("[Rondo1] READY players=%d radius=%.1f loco=%s"),Players.Num(),Radius,*GetNameSafe(Loco));
    UE_LOG(LogTemp,Display,TEXT("[Rondo5] READY club=%s participants=%d locoId=%s"),*RondoClub->ClubId.ToString(),Players.Num(),*Loco->GetPlayerProfileId().ToString());
}
bool ASoccerRondoGameMode::ContainsPlayer(const ASoccerCharacterBase* Player) const
{ return bReady && IsValid(Player) && Players.Contains(const_cast<ASoccerCharacterBase*>(Player)); }
bool ASoccerRondoGameMode::IsOurBall(const ASoccerBall* Candidate) const { return bReady && !bFinished && IsValid(Ball) && Ball==Candidate; }
bool ASoccerRondoGameMode::Inside(const FVector& WorldLocation,float Inset) const
{ return Field && Field->WorldToPitchLocal(WorldLocation).SizeSquared2D() <= FMath::Square(FMath::Max(0.f,Radius-Inset)); }
FVector ASoccerRondoGameMode::InsideTarget(const FVector& WorldLocation,float Inset) const
{
    FVector P=Field->WorldToPitchLocal(WorldLocation);
    const float Z=P.Z; P.Z=0; P=P.GetClampedToMaxSize(FMath::Max(0.f,Radius-Inset)); P.Z=Z;
    return Field->PitchLocalToWorld(P);
}
ASoccerCharacterBase* ASoccerRondoGameMode::GetCarrier() const
{
    if (!bReady) return nullptr;
    if (IsValid(Human) && Human->HasHumanLogicalBallControl()) return Human;
    for (auto* P:Players) if (auto* AI=Cast<ASoccerAICharacter>(P))
        if (AI->IsAIPossessingBall() && AI->GetControlledAIBall()==Ball) return AI;
    return nullptr;
}
ASoccerAICharacter* ASoccerRondoGameMode::GetOpponentCarrier(const ASoccerCharacterBase* Player) const
{
    auto* Carrier=Cast<ASoccerAICharacter>(GetCarrier());
    return Carrier && Player && Carrier!=Player && Carrier->GetTeam()!=Player->GetTeam()?Carrier:nullptr;
}
bool ASoccerRondoGameMode::CanHumanClaim(const ASoccerCharacterBase* Player) const
{
    if (bFinished || !ContainsPlayer(Player)) return false;
    auto* Carrier=GetCarrier();
    return !Carrier || Carrier==Player; // steals use the existing explicit steal path
}
void ASoccerRondoGameMode::TogglePassRequest(AThirdPersonCppCharacter* Player,ESoccerHumanPassRequestType Type)
{
    if (bFinished || !ContainsPlayer(Player)||Player==Loco) return;
    PassRequest=PassRequest==Type?ESoccerHumanPassRequestType::None:Type;
    PassRequestUntil=GetWorld()->GetTimeSeconds()+6.f;
}
bool ASoccerRondoGameMode::IsWaitingForAerialPass(const AThirdPersonCppCharacter* Player) const
{ return bReady && !bFinished && Player==Human && PassRequest==ESoccerHumanPassRequestType::AerialHeader && GetWorld()->GetTimeSeconds()<PassRequestUntil && GetCarrier() && GetCarrier()!=Human && GetCarrier()->GetTeam()==Human->GetTeam(); }
void ASoccerRondoGameMode::Report(ASoccerCharacterBase* Player, ASoccerBall* ContactBall, ERondoContact Kind, const FVector& ContactLocation)
{
    if (auto* Mode=Find(Player)) if (Mode->ContainsPlayer(Player)&&Mode->IsOurBall(ContactBall)) Mode->OnContact(Player,Kind,ContactLocation);
}
void ASoccerRondoGameMode::RefreshBoundary(const FVector& Location)
{
    if (bChallenge && ChallengePasser)
    {
        if (Inside(Location)) bChallengePassEntered=true;
        else if (bChallengePassEntered) { ChallengePasser=nullptr; ChallengeReceiverCandidate=nullptr; bChallengePassEntered=false; }
    }
    if (!bChallenge && !Inside(Location) && IsValid(LastTouch) && LastTouch!=Loco)
    {
        bChallenge=true; AtRisk=LastTouch; ChallengePasser=nullptr; ChallengeReceiverCandidate=nullptr; bChallengePassEntered=false; LastNormalPasser=nullptr; PassStreak=0;
        UE_LOG(LogTemp,Display,TEXT("[Rondo1] OUT responsible=%s loco=%s"),*GetNameSafe(AtRisk),*GetNameSafe(Loco));
    }
}
void ASoccerRondoGameMode::OnContact(ASoccerCharacterBase* Player, ERondoContact Kind, const FVector& Location)
{
    if (bFinished || (bScoringActive && GetWorld()->GetTimeSeconds()-StartTime >= SessionDurationSeconds)) return;
    RefreshBoundary(Location);
    // Do not count repeated reports from the same continuous body contact,
    // even if a second player touched the ball in the meantime. Deliberate
    // kicks/controls remain independent events; no global swap cooldown.
    if (Kind==ERondoContact::Body)
    {
        if (Player==GetCarrier() || ActiveBodyContacts.Contains(Player)) return;
        ActiveBodyContacts.Add(Player);
    }
    else if (IsNearBodyContact(Player, Location)) ActiveBodyContacts.Add(Player);
    // Score the role at the instant of contact, BEFORE a swap or challenge resolution.
    RecordTouch(Player, Kind);
    if (FlightReceiver && (Player!=FlightPasser || Kind!=ERondoContact::Body))
        EndPassFlight(TEXT("contact"),Player);
    const bool bReception=Kind==ERondoContact::Control||Kind==ERondoContact::Kick||Kind==ERondoContact::Aerial;
    // A resolved contact can dislodge logical possession; never leave two owners.
    if (Kind==ERondoContact::Body || Kind==ERondoContact::Aerial || Kind==ERondoContact::Tackle)
    {
        if (auto* PreviousCarrier=GetCarrier()) if (PreviousCarrier!=Player)
        {
            // A real loss must start the same recovery cooldown as a match steal.
            if (auto* Bot=Cast<ASoccerAICharacter>(PreviousCarrier)) Bot->ReleaseAIBall();
            else if (auto* H=Cast<AThirdPersonCppCharacter>(PreviousCarrier)) H->ReleaseBallForAISteal();
        }
    }
    if (bChallenge)
    {
        if (ChallengePasser)
        {
            const bool bFromLoco=ChallengePasser==Loco;
            const bool bValidReceiver=Player!=ChallengePasser && Player!=Loco && (!bFromLoco || Player!=AtRisk);
            const bool bInside=Inside(Location)&&Inside(Player->GetActorLocation());
            const bool bSameReceiver=!ChallengeReceiverCandidate||ChallengeReceiverCandidate==Player;
            if (bValidReceiver && bInside && bSameReceiver && Kind==ERondoContact::Body)
            {
                // A first body bounce is not a completed pass. Keep the candidate
                // only until this same receiver actually controls/plays the ball.
                ChallengeReceiverCandidate=Player;
            }
            else
            {
                const bool bCompleted=bReception&&bValidReceiver&&bInside&&bSameReceiver;
                ChallengePasser=nullptr; ChallengeReceiverCandidate=nullptr; bChallengePassEntered=false;
                if (bCompleted) FinishChallenge(bFromLoco);
            }
        }
        if (bChallenge && Kind==ERondoContact::Kick)
        {
            const bool bNeedsInside=Player==Loco||Player==AtRisk;
            if (!bNeedsInside||(Inside(Location)&&Inside(Player->GetActorLocation())))
            {
                ChallengePasser=Player; ChallengeReceiverCandidate=nullptr; bChallengePassEntered=Inside(Location);
                UE_LOG(LogTemp,Display,TEXT("[Rondo1] CHALLENGE PASS player=%s inside=%d"),*GetNameSafe(Player),Inside(Location)?1:0);
            }
        }
    }
    else
    {
        if (Player==Loco && LastTouch && LastTouch!=Loco)
        {
            UE_LOG(LogTemp,Display,TEXT("[Rondo2] SWAP CONTACT player=%s previous=%s kind=%d speed=%.1f"),
                *GetNameSafe(Player),*GetNameSafe(LastTouch),static_cast<int32>(Kind),Ball->GetBallPhysicsVelocity().Size2D());
            SwapLoco(LastTouch); LastNormalPasser=nullptr; PassStreak=0;
        }
        else if (LastNormalPasser && LastNormalPasser!=Player)
        {
            if (bReception && Inside(Location)) { ++PassStreak; BestStreak=FMath::Max(BestStreak,PassStreak); }
            LastNormalPasser=nullptr;
        }
        if (Kind==ERondoContact::Kick) LastNormalPasser=Player;
        else if (Kind!=ERondoContact::Body && LastNormalPasser==Player) LastNormalPasser=nullptr;
    }
    LastTouch=Player;
}
void ASoccerRondoGameMode::SwapLoco(ASoccerCharacterBase* NewLoco)
{
    if (!ContainsPlayer(NewLoco)||NewLoco==Loco) return;
    UE_LOG(LogTemp,Display,TEXT("[Rondo1] SWAP old=%s new=%s"),*GetNameSafe(Loco),*GetNameSafe(NewLoco));
    UE_LOG(LogTemp,Display,TEXT("[Rondo5] ROLE oldId=%s newId=%s"),*Loco->GetPlayerProfileId().ToString(),*NewLoco->GetPlayerProfileId().ToString());
    Loco=NewLoco; ++RoleChanges; PassRequest=ESoccerHumanPassRequestType::None; UpdateTeams();
}
void ASoccerRondoGameMode::FinishChallenge(bool bLocoWins)
{
    UE_LOG(LogTemp,Display,TEXT("[Rondo1] CHALLENGE COMPLETE winner=%s responsible=%s"),bLocoWins?TEXT("LOCO"):TEXT("GROUP"),*GetNameSafe(AtRisk));
    if(bLocoWins) SwapLoco(AtRisk);
    bChallenge=false; AtRisk=nullptr; ChallengePasser=nullptr; ChallengeReceiverCandidate=nullptr; bChallengePassEntered=false; LastNormalPasser=nullptr; PassStreak=0;
}
void ASoccerRondoGameMode::UpdateTeams()
{
    const FSoccerClubKitDefinition* Kit=IsValid(RondoClub)?&RondoClub->GetKit(ESoccerClubKitType::Home):nullptr;
    auto* Socks=Kit?Kit->SocksMaterial.LoadSynchronous():nullptr;
    auto* Shirt=Kit?Kit->ShirtMaterial.LoadSynchronous():nullptr;
    auto* Shorts=Kit?Kit->ShortsMaterial.LoadSynchronous():nullptr;
    for (int32 Index=0; Index<Players.Num(); ++Index)
    {
        auto* P=Players[Index];
        if (!IsValid(P)) continue;
        P->SetTeamForTraining(P==Loco?ESoccerTeam::OpponentTeam:ESoccerTeam::PlayerTeam);

        // SetTeamForTraining may reset the shirt/shorts to team materials.
        // Reapply the participant's visual kit AFTER the logical team change.
        // Players keeps its original order throughout the session; role changes
        // never move an entry or change the selected player's profile.
        const FSoccerRondoParticipantKit* Override=ParticipantKits.IsValidIndex(Index)
            ? &ParticipantKits[Index] : nullptr;
        UMaterialInterface* ParticipantShirt=Override && IsValid(Override->ShirtMaterial)
            ? Override->ShirtMaterial : Shirt;
        UMaterialInterface* ParticipantShorts=Override && IsValid(Override->ShortsMaterial)
            ? Override->ShortsMaterial : Shorts;
        P->ApplyClubKitMaterials(Socks,ParticipantShirt,ParticipantShorts);

        // Initial assignment only: bReady becomes true after initialization.
        if (!bReady)
        {
            UE_LOG(LogTemp,Display,TEXT("[Rondo6] KIT slot=%d id=%s human=%d shirt=%s shorts=%s"),
                Index,*P->GetPlayerProfileId().ToString(),Index==0?1:0,
                *GetNameSafe(ParticipantShirt),*GetNameSafe(ParticipantShorts));
        }
    }
}
bool ASoccerRondoGameMode::IsNearBodyContact(const ASoccerCharacterBase* Player, const FVector& BallLocation) const
{
    if (!IsValid(Player) || !IsValid(Ball)) return false;
    const UCapsuleComponent* Capsule=Player->GetCapsuleComponent();
    if (!Capsule) return false;
    const float R=Capsule->GetScaledCapsuleRadius();
    const float AxisHalf=FMath::Max(0.f,Capsule->GetScaledCapsuleHalfHeight()-R);
    FVector D=BallLocation-Capsule->GetComponentLocation();
    D.Z-=FMath::Clamp(D.Z,-AxisHalf,AxisHalf);
    // Small separation margin prevents rearming on the contact skin itself.
    return D.SizeSquared()<=FMath::Square(R+Ball->GetBallRadiusCm()+8.f);
}
void ASoccerRondoGameMode::RefreshContactEpisodes(const FVector& BallLocation)
{
    for (auto It=ActiveBodyContacts.CreateIterator(); It; ++It)
        if (!IsNearBodyContact(*It,BallLocation)) It.RemoveCurrent();
    // Foot control may extend beyond the capsule; wait for actual separation
    // before counting a new body/control episode for the same player.
    for (auto It=ScoredContactEpisodes.CreateIterator(); It; ++It)
    {
        const auto* P=*It;
        if (!IsValid(P) || (FVector::Dist2D(P->GetActorLocation(),BallLocation)>110.f &&
            !IsNearBodyContact(P,BallLocation))) It.RemoveCurrent();
    }
}
void ASoccerRondoGameMode::EndPassFlight(const TCHAR* Reason, ASoccerCharacterBase* ContactPlayer)
{
    if (!FlightReceiver) return;
    UE_LOG(LogTemp,Display,TEXT("[Rondo3] PASS END reason=%s passer=%s receiver=%s contact=%s traveled=%.1f speed=%.1f"),
        Reason,*GetNameSafe(FlightPasser),*GetNameSafe(FlightReceiver),*GetNameSafe(ContactPlayer),
        FVector::Dist2D(FlightOrigin,Ball->GetActorLocation()),Ball->GetBallPhysicsVelocity().Size2D());
    FlightReceiver=nullptr; FlightPasser=nullptr; FlightStarted=0.f;
}
FVector ASoccerRondoGameMode::PassTarget(const ASoccerCharacterBase* Receiver,bool bAir) const
{
    FVector Target=InsideTarget(Receiver->GetActorLocation()+Receiver->GetVelocity()*0.15f,80.f);
    Target.Z=Field->GetPitchCenterWorldLocation(bAir?205.f:Ball->GetBallRadiusCm()).Z;
    return Target;
}
void ASoccerRondoGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bReady||bFinished||!IsValid(Ball)||!IsValid(Loco)) return;
    if (GetWorld()->GetTimeSeconds()-StartTime >= SessionDurationSeconds)
    {
        FinishSession();
        return;
    }
    RefreshContactEpisodes(Ball->GetActorLocation());
    RefreshBoundary(Ball->GetActorLocation()); DrawBoundary();
    ResolveBotFootContacts();
    const float Now=GetWorld()->GetTimeSeconds();
    if (FlightReceiver && (!IsValid(FlightReceiver) || GetCarrier() || FlightReceiver==Loco ||
        !SoccerPassShared::IsPassTravelActive(Now-FlightStarted, Ball->GetBallPhysicsVelocity().Size2D(),
            FVector::Dist2D(Ball->GetActorLocation(), FlightReceiver->GetActorLocation()),
            PassIntentMaxLifetime, PassIntentSlowGrace, PassIntentMinimumSpeed, PassIntentAbandonedDistance)))
        EndPassFlight(TEXT("intent ended"));
    if (Now>=PassRequestUntil) PassRequest=ESoccerHumanPassRequestType::None;
    if(Now>=NextDecision) { NextDecision=Now+DecisionInterval; DecideBots(); }
}
FVector ASoccerRondoGameMode::PredictChase(const ASoccerCharacterBase* Player) const
{
    TArray<FSoccerBallTrajectorySample> Samples;
    if(Ball->BuildPredictedTrajectory(1.8f,0.08f,Samples))
    {
        const float Speed=FMath::Max(350.f,Player->GetCharacterMovement()->MaxWalkSpeed);
        for(const auto& S:Samples)
            if(S.bNearGround && FVector::Dist2D(Player->GetActorLocation(),S.Location)<=Speed*S.TimeFromNow+65.f) return S.Location;
    }
    return Ball->GetActorLocation();
}
FVector ASoccerRondoGameMode::SupportTarget(const ASoccerCharacterBase* Player,const FVector& Anchor) const
{
    FVector Best=InsideTarget(Player->GetActorLocation()); float BestScore=-BIG_NUMBER;
    // Score a local set of dynamic support points, not fixed circle slots.
    for(int32 i=0;i<16;++i)
    {
        const float A=2.f*PI*float(i)/16.f;
        for(float Dist:{450.f,900.f,1350.f})
        {
            const FVector P=InsideTarget(Anchor+Field->GetPitchLengthWorldDirection()*(FMath::Cos(A)*Dist)+Field->GetPitchWidthWorldDirection()*(FMath::Sin(A)*Dist));
            float Separation=Radius;
            for(auto* Other:Players) if(Other!=Player&&Other!=Loco&&IsValid(Other)) Separation=FMath::Min(Separation,FVector::Dist2D(P,Other->GetActorLocation()));
            const FVector AB=(P-Anchor).GetSafeNormal2D();
            const FVector D=Loco->GetActorLocation()-Anchor;
            const float T=FMath::Clamp(FVector::DotProduct(D,AB),0.f,FVector::Dist2D(Anchor,P));
            const float Lane=FVector::Dist2D(Anchor+AB*T,Loco->GetActorLocation());
            const float Score=FMath::Min(Separation,650.f)*1.2f+FMath::Min(Lane,650.f)*0.8f-0.35f*FVector::Dist2D(P,Player->GetActorLocation())-0.15f*FMath::Abs(FVector::Dist2D(P,Anchor)-850.f);
            if(Score>BestScore) {BestScore=Score;Best=P;}
        }
    }
    return Best;
}
ASoccerCharacterBase* ASoccerRondoGameMode::ChoosePassReceiver(ASoccerAICharacter* Player, bool& bOutAir, bool& bOutBlocked) const
{
    ASoccerCharacterBase* Best=nullptr;
    float BestScore=-BIG_NUMBER;
    int32 BestPriority=-1;
    bOutAir=false; bOutBlocked=false;
    const FVector Origin=Ball->GetActorLocation();
    const auto* Rival=Player==Loco?AtRisk:Loco;
    const auto* Controller=Cast<ASoccerAIController>(Player->GetController());
    if (!Controller) return nullptr;
    for(auto* P:Players)
    {
        if(!IsValid(P)||P==Player||P==Loco||(Player==Loco&&P==AtRisk)||!Inside(P->GetActorLocation(),80.f)) continue;
        const bool bRequestedAir=P==Human&&PassRequest==ESoccerHumanPassRequestType::AerialHeader;
        const bool bRequestedGround=P==Human&&PassRequest==ESoccerHumanPassRequestType::Normal;
        const FVector Target=PassTarget(P,false);
        const float Dist=FVector::Dist2D(Origin,Target);
        if(Dist<220.f) continue;
        const FVector D=(Target-Origin).GetSafeNormal2D();
        const FVector R=Rival?Rival->GetActorLocation():Origin;
        const float T=FMath::Clamp(FVector::DotProduct(R-Origin,D),0.f,Dist);
        const float Lane=Rival?FVector::Dist2D(Origin+D*T,R):900.f;
        const bool bBlocked=IsValid(Rival)&&SoccerPassShared::IsPlayerInPassLane(Origin,Target,R,Controller->GetPassLaneHalfWidth());
        bool bGroundReachable=false;
        Ball->CalculateGroundPassInput6AN(Dist,&bGroundReachable);
        // Do not lob merely because a ground pass is too long; find a closer
        // teammate or continue dribbling. F never changes into an aerial pass.
        if (bRequestedGround && (bBlocked || !bGroundReachable)) continue;
        const bool bAir=bRequestedAir||bBlocked;
        if (!bAir && !bGroundReachable) continue;
        // A clear, reachable ground pass takes precedence over a tactical lob.
        // Explicit R is still honored by the normal human-request bonus.
        const int32 Priority=(bRequestedAir||!bAir)?2:1;
        const float CandidateScore=FMath::Min(Lane,900.f)*1.4f-0.20f*Dist+
            (P==Human&&PassRequest!=ESoccerHumanPassRequestType::None?350.f:0.f);
        if(Priority>BestPriority||(Priority==BestPriority&&CandidateScore>BestScore))
        {
            BestPriority=Priority; BestScore=CandidateScore; Best=P;
            bOutAir=bAir; bOutBlocked=bBlocked;
        }
    }
    return Best;
}
void ASoccerRondoGameMode::MoveBot(ASoccerAICharacter* Bot,const FVector& Target,bool bChase)
{
    auto* AI=Cast<ASoccerAIController>(Bot->GetController()); if(!AI)return;
    Bot->SetAIChasingBall(bChase);
    Bot->RequestAIMovementMode(bChase?ESoccerAIMovementMode::FastRun:ESoccerAIMovementMode::Run,bChase?ESoccerAIMovementReason::FreeBallRace:ESoccerAIMovementReason::ShapeSupport);
    FVector Dest=Target; Dest.Z=Bot->GetActorLocation().Z;
    FVector* Old=MoveTargets.Find(Bot);
    if(!Old||FVector::Dist2D(*Old,Dest)>65.f||AI->GetMoveStatus()==EPathFollowingStatus::Idle)
    { AI->MoveToLocation(Dest,bChase?15.f:80.f,false,true,true,false,nullptr,true); MoveTargets.Add(Bot,Dest); }
}
void ASoccerRondoGameMode::ResolveBotFootContacts()
{
    // Physical contact must be checked per frame, not at the tactical refresh
    // rate: a first-time pass can traverse the entire pickup zone in 0.16 s.
    auto* Carrier=GetCarrier();
    ASoccerAICharacter* Best=nullptr; float BestDistance=BIG_NUMBER;
    const FVector BallLoc=Ball->GetActorLocation();
    for (auto* P:Players)
    {
        auto* Bot=Cast<ASoccerAICharacter>(P);
        if(!IsValid(Bot)||Bot==Carrier||Bot->IsAIKickMontageActive()||Bot->IsAIDribbleTurnAutoPassActive()||
            Bot->IsTackleActive()||Bot->IsTackleFallReactionActive()||Bot->IsAerialActionQueuedOrPlaying()||Bot->IsTackleEvasionActive()) continue;
        if(Carrier&&Carrier->GetTeam()==Bot->GetTeam()) continue;
        auto* C=Cast<ASoccerAIController>(Bot->GetController());
        if(!C||!C->CanRondoFootContact(Bot,Ball)) continue;
        if (!Bot->IsAIAutoPassActive() && !C->CanRondoRecoverReleasedBall(Bot)) continue;
        if (Carrier && Bot->IsAIAutoPassActive()) continue;
        const float D=FVector::Dist2D(Bot->GetActorLocation(),BallLoc);
        if(!Carrier&&Human->HasActiveHumanBallClaim()&&Human->GetTeam()==Bot->GetTeam()&&FVector::Dist2D(Human->GetActorLocation(),BallLoc)<D) continue;
        if(D<BestDistance){BestDistance=D;Best=Bot;}
    }
    if(!Best)return;
    auto* C=Cast<ASoccerAIController>(Best->GetController());
    if(!Carrier&&Best->IsAIAutoPassActive()&&Best->TryCollectAIAutoPassIfClose(55.f)) return;
    if(C) C->TryRondoCollect(Best,Ball,Carrier);
}
void ASoccerRondoGameMode::DecideBots()
{
    auto* Carrier=GetCarrier(); const FVector BallLoc=Ball->GetActorLocation(); const float Now=GetWorld()->GetTimeSeconds();
    if(auto* AI=Cast<ASoccerAICharacter>(Carrier))
    {
        auto* C=Cast<ASoccerAIController>(AI->GetController());
        if(C && !C->HasRondoPhysicalControl(AI,Ball)) {AI->ReleaseAIBall(false);Carrier=nullptr;}
    }
    ASoccerCharacterBase* Helper=nullptr; float Best=BIG_NUMBER;
    if(!Carrier||Carrier==Loco)
        for(auto* P:Players) if(P!=Loco&&IsValid(P))
        {
            if (auto* Candidate=Cast<ASoccerAICharacter>(P))
            {
                auto* Controller=Cast<ASoccerAIController>(Candidate->GetController());
                if (!Controller || Candidate->IsAIKickMontageActive() ||
                    (!Candidate->IsAIAutoPassActive() && !Controller->CanRondoRecoverReleasedBall(Candidate))) continue;
            }
            const float D=FVector::Dist2D(P->GetActorLocation(),BallLoc);
            if(D<Best) {Best=D;Helper=P;}
        }
    if(!Carrier&&Human!=Loco&&Human->HasActiveHumanBallClaim()&&FVector::Dist2D(Human->GetActorLocation(),BallLoc)<=Best+100.f) Helper=Human;
    if (!Carrier && IsValid(FlightReceiver) && FlightReceiver!=Loco) Helper=FlightReceiver;
    for(auto* P:Players)
    {
        auto* Bot=Cast<ASoccerAICharacter>(P); if(!IsValid(Bot))continue;
        auto* C=Cast<ASoccerAIController>(Bot->GetController()); if(!C)continue;
        if(Bot->ShouldAIKickMontageLockController()||Bot->IsAIDribbleTurnAutoPassActive()||Bot->IsTackleActive()||Bot->IsTackleFallReactionActive()||Bot->IsAerialActionQueuedOrPlaying()||Bot->IsTackleEvasionActive()) { C->StopMovement(); MoveTargets.Remove(Bot); continue; }
        Carrier=GetCarrier();
        if(Carrier!=Bot && Bot->IsAIAutoPassActive() && (Carrier || LastTouch!=Bot)) Bot->ClearAIAutoPassState();
        if(Carrier==Bot)
        {
            const bool bMustReturn=bChallenge&&(Bot==AtRisk||Bot==Loco)&&(!Inside(BallLoc,80.f)||!Inside(Bot->GetActorLocation(),80.f));
            bool bAir=false, bBlocked=false;
            auto* Receiver=bMustReturn?nullptr:ChoosePassReceiver(Bot,bAir,bBlocked);
            float& Last=LastPassAttempt.FindOrAdd(Bot);
            if(Receiver&&Now-Last>0.35f&&C->CanRondoFootContact(Bot,Ball))
            {
                Last=Now;
                const bool bRequestedAir=Receiver==Human&&PassRequest==ESoccerHumanPassRequestType::AerialHeader;
                const FVector Origin=Ball->GetActorLocation();
                // Tactical lobs land at the receiver's feet; explicit R still
                // targets the aerial reception height from the match.
                const FVector Target=PassTarget(Receiver,bRequestedAir);
                const bool bGround=!bAir;
                float Speed=0.f, MinTime=0.f, MaxTime=0.f;
                if (bGround)
                {
                    Speed=Ball->CalculateGroundPassInput6AN(FVector::Dist2D(Origin,Target));
                    MinTime=0.3f; MaxTime=1.5f;
                }
                else if (bRequestedAir)
                {
                    Speed=1800.f; MinTime=0.75f; MaxTime=1.65f;
                }
                else
                {
                    // Match simple-pass settings, executed with the shared
                    // damping-aware aerial solver instead of a forced arc.
                    C->GetSimplePassKickSettings(Speed,MinTime,MaxTime);
                }
                const bool bStarted=Bot->TryStartAIKickMontageForOpenPlay(Ball,Target,Speed,MinTime,MaxTime,bAir,bGround,true);
                if(bStarted)
                {
                    if (LastTouch==Bot && !GetCarrier())
                    {
                        FlightReceiver=Receiver; FlightPasser=Bot; FlightOrigin=Origin;
                        FlightStarted=Now;
                        Helper=Receiver;
                    }
                    UE_LOG(LogTemp,Display,TEXT("[Rondo4] PASS mode=%s blocked=%d actor=%s receiver=%s distance=%.1f input=%.1f actualXY=%.1f actualZ=%.1f minTime=%.2f maxTime=%.2f"),
                        bGround?TEXT("6AN_GROUND"):bRequestedAir?TEXT("MATCH_REQUEST_R"):TEXT("MATCH_TACTICAL_LOB"), bBlocked?1:0,
                        *GetNameSafe(Bot),*GetNameSafe(Receiver),FVector::Dist2D(Origin,Target),Speed,
                        Ball->GetBallPhysicsVelocity().Size2D(),Ball->GetBallPhysicsVelocity().Z,MinTime,MaxTime);
                    C->StopMovement(); MoveTargets.Remove(Bot); if(Receiver==Human) PassRequest=ESoccerHumanPassRequestType::None; continue;
                }
            }
            FVector Target=bMustReturn?InsideTarget(BallLoc,450.f):InsideTarget(BallLoc+(BallLoc-Loco->GetActorLocation()).GetSafeNormal2D()*400.f,250.f);
            if(FVector::Dist2D(Target,BallLoc)<120.f) Target=SupportTarget(Bot,BallLoc);
            if(C->CanRondoFootContact(Bot,Ball) && Now-Last>0.25f)
            {
                Last=Now;
                float Speed=0.f, MinTime=0.f, MaxTime=0.f;
                C->GetAutoPassKickSettings(Speed,MinTime,MaxTime);
                Bot->StartAIAutoPassToLocation(Target,Speed,MinTime,MaxTime);
            }
            MoveBot(Bot,Target,true); continue;
        }
        const bool bChase=(Bot==Loco)||(Bot==Helper);
        if(bChase || (!Carrier && Bot->IsAIAutoPassActive() && LastTouch==Bot))
        {
            if(!Carrier && Bot->IsAIAutoPassActive() && Bot->TryCollectAIAutoPassIfClose(55.f)) { C->StopMovement(); continue; }
            if(C->TryRondoCollect(Bot,Ball,Carrier)) { C->StopMovement(); MoveTargets.Remove(Bot); continue; }
            MoveBot(Bot,PredictChase(Bot),true);
        }
        else { Bot->ClearAIAutoPassState(); MoveBot(Bot,SupportTarget(Bot,BallLoc),false); }
    }
}
void ASoccerRondoGameMode::DrawBoundary() const
{
    // Development-build world marker, derived from the shared pitch transform.
    for(int32 i=0;i<96;++i)
    {
        const float A=2.f*PI*i/96.f,B=2.f*PI*(i+1)/96.f;
        DrawDebugLine(GetWorld(),Field->PitchLocalToWorld(FVector(Radius*FMath::Cos(A),Radius*FMath::Sin(A),4.f)),Field->PitchLocalToWorld(FVector(Radius*FMath::Cos(B),Radius*FMath::Sin(B),4.f)),bChallenge?FColor::Orange:FColor::Yellow,false,0.f,0,5.f);
    }
}
int32 ASoccerRondoGameMode::GetTouches(const ASoccerCharacterBase* Player) const
{
    if (!IsValid(Player)) return 0;
    return TouchScores.FindRef(Player->GetPlayerProfileId());
}

TArray<ASoccerCharacterBase*> ASoccerRondoGameMode::GetRankedPlayers() const
{
    TArray<ASoccerCharacterBase*> Ranked;
    for (auto* P:Players) if (IsValid(P)) Ranked.Add(P);
    // Stable display order for ties; the actual position is shared.
    Ranked.StableSort([this](const ASoccerCharacterBase& A,const ASoccerCharacterBase& B)
    { return GetTouches(&A)>GetTouches(&B); });
    return Ranked;
}

int32 ASoccerRondoGameMode::GetRemainingSeconds() const
{
    if (bFinished) return 0;
    const float Elapsed=bReady && GetWorld()?GetWorld()->GetTimeSeconds()-StartTime:0.f;
    return FMath::Max(0,FMath::CeilToInt(SessionDurationSeconds-Elapsed));
}

void ASoccerRondoGameMode::RecordTouch(ASoccerCharacterBase* Player, ERondoContact Kind)
{
    if (!IsValid(Player)) return;
    if (Kind==ERondoContact::Body || Kind==ERondoContact::Control)
    {
        // Mark the episode even if this contact was made as loco or was the
        // initial scripted possession: a subsequent role change cannot award it.
        if (ScoredContactEpisodes.Contains(Player)) return;
        ScoredContactEpisodes.Add(Player);
    }
    else
    {
        const float Now=GetWorld()->GetTimeSeconds();
        const float* Last=LastActionContactTime.Find(Player);
        if (Last && *Last==Now) return;
        LastActionContactTime.Add(Player,Now);
        // A subsequent body/control report in the same contact does not add a point.
        ScoredContactEpisodes.Add(Player);
    }
    if (!bScoringActive || bFinished || Player==Loco) return;
    int32& Score=TouchScores.FindOrAdd(Player->GetPlayerProfileId());
    ++Score;
    UE_LOG(LogTemp,Display,TEXT("[Rondo7] TOUCH id=%s name=%s kind=%d total=%d remaining=%d"),
        *Player->GetPlayerProfileId().ToString(),*GetParticipantName(Player),static_cast<int32>(Kind),Score,GetRemainingSeconds());
}

void ASoccerRondoGameMode::FinishSession()
{
    if (bFinished) return;
    bFinished=true;
    bScoringActive=false;
    PassRequest=ESoccerHumanPassRequestType::None;
    EndPassFlight(TEXT("session finished"));
    for (auto* P:Players)
    {
        if (!IsValid(P)) continue;
        if (auto* C=Cast<ASoccerAIController>(P->GetController())) C->StopMovement();
        if (auto* Movement=P->GetCharacterMovement()) Movement->StopMovementImmediately();
    }
    if (IsValid(Ball)) Ball->StopBallKeepingPhysics();
    const auto Ranked=GetRankedPlayers();
    int32 Place=0,PreviousScore=-1;
    UE_LOG(LogTemp,Display,TEXT("[Rondo7] FINAL duration=%.1f"),SessionDurationSeconds);
    for (int32 Index=0;Index<Ranked.Num();++Index)
    {
        const int32 Score=GetTouches(Ranked[Index]);
        if (Score!=PreviousScore) Place=Index+1;
        PreviousScore=Score;
        UE_LOG(LogTemp,Display,TEXT("[Rondo7] RESULT place=%d id=%s name=%s touches=%d"),
            Place,*Ranked[Index]->GetPlayerProfileId().ToString(),*GetParticipantName(Ranked[Index]),Score);
    }
    if (auto* PC=GetWorld()->GetFirstPlayerController())
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        if (IsValid(Human)) Human->DisableInput(PC);
    }
    // Pause the simulation, including physics, animations and pending kick timers;
    // Slate/UMG remains interactive while gameplay is paused.
    bPauseable=true;
    const bool bPaused=UGameplayStatics::SetGamePaused(this,true);
    UE_LOG(LogTemp,Display,TEXT("[Rondo7] FINISH paused=%d"),bPaused?1:0);

    if (auto* PC=GetWorld()->GetFirstPlayerController())
    {
        ResultWidget8 = CreateWidget<USoccerRondoResultWidget8>(
            PC, USoccerRondoResultWidget8::StaticClass());
        if (IsValid(ResultWidget8))
        {
            ResultWidget8->AddToViewport(200);
            PC->bShowMouseCursor = true;
            PC->bEnableClickEvents = true;
            PC->bEnableMouseOverEvents = true;
            FInputModeUIOnly Mode;
            Mode.SetWidgetToFocus(ResultWidget8->TakeWidget());
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(Mode);
            ResultWidget8->FocusFirstAction();
            UE_LOG(LogTemp,Display,TEXT("[Rondo8] RESULTS UI ready"));
        }
        else
        {
            UE_LOG(LogTemp,Error,TEXT("[Rondo8] Could not create results widget; using Canvas fallback"));
        }
    }
}

bool ASoccerRondoGameMode::HasResultScreen() const
{
    return IsValid(ResultWidget8) && ResultWidget8->IsInViewport();
}

FString ASoccerRondoGameMode::GetStatus() const
{
    if(!bReady) return TEXT("EL LOCO | Preparacion: revisar Output Log [Rondo1]");
    if(bFinished) return TEXT("EL LOCO | FIN DEL JUEGO");
    const FString RoleLabel=Human==Loco?TEXT("SOS EL LOCO"):Human==AtRisk?TEXT("ESTAS EN RIESGO"):TEXT("NO LOCO");
    const FString Phase=bChallenge?FString::Printf(TEXT("Disputa: %s"),*GetParticipantName(AtRisk)):TEXT("Juego normal");
    const int32 Remaining=GetRemainingSeconds();
    return FString::Printf(TEXT("EL LOCO | %02d:%02d | %s | %s\nPases: %d | Record: %d | Cambios: %d"),Remaining/60,Remaining%60,*RoleLabel,*Phase,PassStreak,BestStreak,RoleChanges);
}
