#include "SoccerDirectorTechnicalIdButton.h"

void USoccerDirectorTechnicalIdButton::SetIdentifier(FName InIdentifier)
{
    Identifier = InIdentifier;
    OnClicked.AddUniqueDynamic(
        this,
        &USoccerDirectorTechnicalIdButton::HandleButtonClicked
    );
}

FName USoccerDirectorTechnicalIdButton::GetIdentifier() const
{
    return Identifier;
}

void USoccerDirectorTechnicalIdButton::HandleButtonClicked()
{
    OnIdentifierClicked.Broadcast(Identifier);
}
