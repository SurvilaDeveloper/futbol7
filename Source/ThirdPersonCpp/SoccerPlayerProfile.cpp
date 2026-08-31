#include "SoccerPlayerProfile.h"

bool USoccerPlayerProfile::HasValidPlayerId() const
{
    return !Identity.PlayerId.IsNone();
}

int32 USoccerPlayerProfile::GetPositionFamiliarity(ESoccerPlayerNaturalPosition Position) const
{
    int32 BestFamiliarity = 0;

    for (const FSoccerPlayerPositionPreference& Preference : PositionPreferences)
    {
        if (Preference.Position == Position)
        {
            BestFamiliarity = FMath::Max(BestFamiliarity, FMath::Clamp(Preference.Familiarity, 0, 100));
        }
    }

    return BestFamiliarity;
}
