#include "SoccerPlayerIdButton.h"

void USoccerPlayerIdButton::SetPlayerId(FName InId)
{
    PlayerId = InId;
    OnClicked.AddUniqueDynamic(this, &USoccerPlayerIdButton::BroadcastPlayerId);
}

void USoccerPlayerIdButton::BroadcastPlayerId()
{
    OnPlayerIdClicked.Broadcast(PlayerId);
}
