#include "SoccerSquadPreviewActor.h"

#include "SoccerClubProfile.h"
#include "SoccerMatchManager.h"
#include "SoccerPlayerProfile.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Animation/AnimationAsset.h"
#include "Materials/MaterialInterface.h"

ASoccerSquadPreviewActor::ASoccerSquadPreviewActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
    SetRootComponent(SceneRoot);

    PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PlayerMesh"));
    PreviewMesh->SetupAttachment(SceneRoot);
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetGenerateOverlapEvents(false);
    PreviewMesh->SetCastShadow(false);
    PreviewMesh->SetReceivesDecals(false);

    Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PreviewCapture"));
    Capture->SetupAttachment(SceneRoot);
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->FOVAngle = 40.0f;

    // An off-field preview has no dependable ambient light. Keep exposure
    // stable when switching characters (and independent of the match scene).
    Capture->PostProcessBlendWeight = 1.0f;
    Capture->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
    Capture->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
    Capture->PostProcessSettings.AutoExposureMinBrightness = 1.0f;
    Capture->PostProcessSettings.AutoExposureMaxBrightness = 1.0f;
    Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
    Capture->PostProcessSettings.AutoExposureBias = 0.5f;

    // Small-radius point lights work as a self-contained three-point studio
    // lighting rig. They are thousands of meters away from the playable pitch,
    // so they do not change its lighting or the actual character materials.
    // Both the mesh and the lights are children of this isolated actor.
    PreviewLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PreviewKeyLight"));
    PreviewLight->SetupAttachment(SceneRoot);
    PreviewLight->SetIntensity(24000.0f);
    PreviewLight->SetAttenuationRadius(650.0f);
    PreviewLight->SetLightColor(FLinearColor(1.0f, 0.94f, 0.86f));
    PreviewLight->SetCastShadows(false);
    PreviewLight->SetRelativeLocation(FVector(195.0f, -155.0f, 205.0f));

    PreviewFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PreviewFillLight"));
    PreviewFillLight->SetupAttachment(SceneRoot);
    PreviewFillLight->SetIntensity(12500.0f);
    PreviewFillLight->SetAttenuationRadius(650.0f);
    PreviewFillLight->SetLightColor(FLinearColor(0.85f, 0.92f, 1.0f));
    PreviewFillLight->SetCastShadows(false);
    PreviewFillLight->SetRelativeLocation(FVector(175.0f, 170.0f, 120.0f));

    PreviewRimLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PreviewRimLight"));
    PreviewRimLight->SetupAttachment(SceneRoot);
    PreviewRimLight->SetIntensity(13500.0f);
    PreviewRimLight->SetAttenuationRadius(580.0f);
    PreviewRimLight->SetLightColor(FLinearColor(0.91f, 0.96f, 1.0f));
    PreviewRimLight->SetCastShadows(false);
    PreviewRimLight->SetRelativeLocation(FVector(-170.0f, 70.0f, 225.0f));
}

void ASoccerSquadPreviewActor::SetRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
    RenderTarget = InRenderTarget;
    if (Capture)
    {
        Capture->TextureTarget = RenderTarget;
        Capture->ShowOnlyActors.Reset();
        Capture->ShowOnlyActors.Add(this);
    }
    RefreshCapture();
}

int32 ASoccerSquadPreviewActor::ResolveMaterialIndex(
    USkeletalMeshComponent* Mesh, FName SlotName, int32 Fallback)
{
    if (!Mesh)
    {
        return INDEX_NONE;
    }
    int32 Index = Mesh->GetMaterialIndex(SlotName);
    if (Index == INDEX_NONE)
    {
        Index = Mesh->GetMaterialIndex(FName(*(SlotName.ToString() + TEXT("_001"))));
    }
    return Index != INDEX_NONE ? Index : Fallback;
}

void ASoccerSquadPreviewActor::ApplyMaterial(FName SlotName, int32 FallbackIndex,
                                              UMaterialInterface* Material)
{
    if (!PreviewMesh || !Material)
    {
        return;
    }
    const int32 Index = ResolveMaterialIndex(PreviewMesh, SlotName, FallbackIndex);
    if (Index >= 0 && Index < PreviewMesh->GetNumMaterials())
    {
        PreviewMesh->SetMaterial(Index, Material);
    }
}

bool ASoccerSquadPreviewActor::ShowPlayer(
    USoccerPlayerProfile* Profile, USoccerClubProfile* Club,
    ASoccerMatchManager* MatchManager)
{
    if (!IsValid(Profile) || !PreviewMesh)
    {
        return false;
    }

    USkeletalMesh* Mesh = nullptr;
    if (!Profile->Appearance.MeshOverride.IsNull())
    {
        Mesh = Profile->Appearance.MeshOverride.LoadSynchronous();
    }
    if (!Mesh && !Profile->Appearance.BodyVariantId.IsNone() && IsValid(MatchManager))
    {
        Mesh = MatchManager->ResolvePlayerBodyVariantMesh(Profile->Appearance.BodyVariantId);
    }
    // This mesh path exists in the supplied Content.zip. A profile override or
    // the match appearance catalog always has precedence over this fallback.
    if (!Mesh)
    {
        Mesh = LoadObject<USkeletalMesh>(nullptr,
            TEXT("/Game/SoccerPlayer/Base_Mesh_2.Base_Mesh_2"));
    }
    if (!Mesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SquadViewer] No mesh for player %s"),
            *Profile->Identity.PlayerId.ToString());
        ClearPlayer();
        return false;
    }

    PreviewMesh->SetSkeletalMesh(Mesh, true);
    // Clear all previous player/club material overrides when reusing the actor.
    for (int32 Index = 0; Index < PreviewMesh->GetNumMaterials(); ++Index)
    {
        PreviewMesh->SetMaterial(Index, nullptr);
    }

    const FSoccerPlayerAppearance& Appearance = Profile->Appearance;
    ApplyMaterial(TEXT("M_Eyelashes"), 1, Appearance.EyelashesMaterial.LoadSynchronous());
    ApplyMaterial(TEXT("M_Body"), 3, Appearance.BodyMaterial.LoadSynchronous());
    ApplyMaterial(TEXT("M_Hair"), 5, Appearance.HairMaterial.LoadSynchronous());
    ApplyMaterial(TEXT("M_Shoes"), 6, Appearance.ShoesMaterial.LoadSynchronous());

    if (IsValid(Club))
    {
        bool bGoalkeeper = false;
        for (const FSoccerPlayerPositionPreference& Preference : Profile->PositionPreferences)
        {
            bGoalkeeper |= Preference.Position == ESoccerPlayerNaturalPosition::Goalkeeper
                && Preference.Familiarity >= 50;
        }
        const FSoccerClubKitDefinition& Kit = Club->GetKit(
            bGoalkeeper ? ESoccerClubKitType::Goalkeeper : ESoccerClubKitType::Home);
        ApplyMaterial(TEXT("M_Socks"), 0, Kit.SocksMaterial.LoadSynchronous());
        ApplyMaterial(TEXT("M_Shirt"), 2, Kit.ShirtMaterial.LoadSynchronous());
        ApplyMaterial(TEXT("M_Shorts"), 4, Kit.ShortsMaterial.LoadSynchronous());
    }

    // A self-contained idle sequence does not need an AnimBP or a game character.
    // It also avoids gameplay animation notifies firing in the preview.
    UAnimationAsset* Idle = LoadObject<UAnimationAsset>(nullptr,
        TEXT("/Game/SoccerPlayer/Idle.Idle"));
    PreviewMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    if (Idle && Idle->GetSkeleton() == Mesh->GetSkeleton())
    {
        PreviewMesh->PlayAnimation(Idle, true);
        PreviewMesh->TickAnimation(0.05f, false);
        PreviewMesh->RefreshBoneTransforms();
    }
    else
    {
        PreviewMesh->SetAnimation(nullptr);
    }

    // Normalize framing for body variants without scaling the model itself.
    PreviewMesh->UpdateBounds();
    const float Height = PreviewMesh->Bounds.BoxExtent.Z * 2.0f;
    CameraCenterHeight = FMath::Clamp(Height * 0.5f, 65.0f, 140.0f);
    // Many third-person player meshes are authored facing +Y.
    MeshYaw = -90.0f;
    PreviewMesh->SetRelativeRotation(FRotator(0.0f, MeshYaw, 0.0f));
    CameraDistance = 300.0f;
    RefreshCamera();
    RefreshCapture();
    return true;
}

void ASoccerSquadPreviewActor::ClearPlayer()
{
    if (PreviewMesh)
    {
        PreviewMesh->SetSkeletalMesh(nullptr);
    }
    if (RenderTarget)
    {
        RenderTarget->UpdateResourceImmediate(true);
    }
}

void ASoccerSquadPreviewActor::RefreshCamera()
{
    if (!Capture)
    {
        return;
    }
    const FVector CameraPos(CameraDistance, 0.0f, CameraCenterHeight);
    const FVector Target(0.0f, 0.0f, CameraCenterHeight);
    Capture->SetRelativeLocation(CameraPos);
    Capture->SetRelativeRotation((Target - CameraPos).Rotation());
}

void ASoccerSquadPreviewActor::RotateBy(float Degrees)
{
    MeshYaw = FMath::Fmod(MeshYaw + Degrees, 360.0f);
    if (PreviewMesh)
    {
        PreviewMesh->SetRelativeRotation(FRotator(0.0f, MeshYaw, 0.0f));
    }
    RefreshCapture();
}

void ASoccerSquadPreviewActor::ZoomBy(float DistanceDelta)
{
    CameraDistance = FMath::Clamp(CameraDistance + DistanceDelta, 205.0f, 560.0f);
    RefreshCamera();
    RefreshCapture();
}

void ASoccerSquadPreviewActor::RefreshCapture()
{
    if (Capture && RenderTarget && IsValid(PreviewMesh->SkeletalMesh))
    {
        Capture->CaptureScene();
    }
}
