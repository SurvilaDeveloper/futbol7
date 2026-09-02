#include "SoccerPlayerAppearanceCatalog.h"

#include "Engine/SkeletalMesh.h"

USkeletalMesh* USoccerPlayerAppearanceCatalog::LoadBodyVariantMesh(
    FName BodyVariantId
) const
{
    if (BodyVariantId.IsNone())
    {
        return nullptr;
    }

    const FSoccerBodyVariantDefinition* MatchingVariant = nullptr;

    for (const FSoccerBodyVariantDefinition& BodyVariant : BodyVariants)
    {
        if (BodyVariant.BodyVariantId != BodyVariantId)
        {
            continue;
        }

        if (MatchingVariant != nullptr)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("[PlayerAppearance] Duplicate BodyVariantId '%s' in catalog %s; variant rejected."),
                *BodyVariantId.ToString(),
                *GetName()
            );
            return nullptr;
        }

        MatchingVariant = &BodyVariant;
    }

    if (MatchingVariant == nullptr)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[PlayerAppearance] BodyVariantId '%s' was not found in catalog %s."),
            *BodyVariantId.ToString(),
            *GetName()
        );
        return nullptr;
    }

    if (MatchingVariant->SkeletalMesh.IsNull())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[PlayerAppearance] BodyVariantId '%s' has no SkeletalMesh in catalog %s."),
            *BodyVariantId.ToString(),
            *GetName()
        );
        return nullptr;
    }

    USkeletalMesh* ResolvedMesh =
        MatchingVariant->SkeletalMesh.LoadSynchronous();

    if (ResolvedMesh == nullptr)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[PlayerAppearance] SkeletalMesh for BodyVariantId '%s' could not be loaded from catalog %s."),
            *BodyVariantId.ToString(),
            *GetName()
        );
    }

    return ResolvedMesh;
}

bool USoccerPlayerAppearanceCatalog::ContainsBodyVariant(
    FName BodyVariantId
) const
{
    if (BodyVariantId.IsNone())
    {
        return false;
    }

    int32 MatchCount = 0;
    for (const FSoccerBodyVariantDefinition& BodyVariant : BodyVariants)
    {
        if (BodyVariant.BodyVariantId == BodyVariantId)
        {
            ++MatchCount;
        }
    }

    return MatchCount == 1;
}
