#include "SoccerClubIdButton.h"

void USoccerClubIdButton::SetClubId(FName InClubId)
{
    ClubId = InClubId;
    OnClicked.AddUniqueDynamic(this, &USoccerClubIdButton::HandleClicked);
}

FName USoccerClubIdButton::GetClubId() const
{
    return ClubId;
}

void USoccerClubIdButton::HandleClicked()
{
    OnClubIdClicked.Broadcast(ClubId);
}
