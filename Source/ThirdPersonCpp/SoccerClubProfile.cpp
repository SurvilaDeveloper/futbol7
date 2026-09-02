#include "SoccerClubProfile.h"

bool FSoccerClubKitDefinition::IsComplete() const
{
    return
        !ShirtMaterial.IsNull() &&
        !ShortsMaterial.IsNull() &&
        !SocksMaterial.IsNull();
}

bool USoccerClubProfile::HasValidClubId() const
{
    return !ClubId.IsNone();
}

bool USoccerClubProfile::HasCompleteKit(
    ESoccerClubKitType KitType
) const
{
    return GetKit(KitType).IsComplete();
}

const FSoccerClubKitDefinition& USoccerClubProfile::GetKit(
    ESoccerClubKitType KitType
) const
{
    switch (KitType)
    {
        case ESoccerClubKitType::Away:
            return AwayKit;

        case ESoccerClubKitType::Goalkeeper:
            return GoalkeeperKit;

        case ESoccerClubKitType::Home:
        default:
            return HomeKit;
    }
}
