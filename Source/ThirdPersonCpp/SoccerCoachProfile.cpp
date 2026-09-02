#include "SoccerCoachProfile.h"

bool USoccerCoachProfile::HasValidCoachId() const
{
    return !Identity.CoachId.IsNone();
}

int32 USoccerCoachProfile::GetFormationPreference(
    ESoccerFormationSystem FormationSystem
) const
{
    int32 HighestPreference = INDEX_NONE;
    for (const FSoccerCoachFormationPreference& Entry : FormationPreferences)
    {
        if (Entry.FormationSystem == FormationSystem)
        {
            HighestPreference = FMath::Max(
                HighestPreference,
                FMath::Clamp(Entry.Preference, 0, 100)
            );
        }
    }

    return HighestPreference == INDEX_NONE ? 50 : HighestPreference;
}
