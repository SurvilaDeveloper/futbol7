#include "SoccerSquadCatalog.h"

#include "SoccerClubProfile.h"

FName USoccerSquadCatalog::GetClubId() const
{
    return IsValid(ClubProfile) && ClubProfile->HasValidClubId()
        ? ClubProfile->ClubId
        : NAME_None;
}

bool USoccerSquadCatalog::HasValidClubAssociation() const
{
    return !GetClubId().IsNone();
}
