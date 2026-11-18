#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerSquadPreviewActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class USkeletalMeshComponent;
class USceneCaptureComponent2D;
class UPointLightComponent;
class UTextureRenderTarget2D;
class USoccerPlayerProfile;
class USoccerClubProfile;
class ASoccerMatchManager;

/**
 * Isolated, non-colliding presentation actor, never a gameplay character.
 * It renders only its own mesh into a render target for the prematch squad UI.
 */
UCLASS(NotBlueprintable)
class THIRDPERSONCPP_API ASoccerSquadPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    ASoccerSquadPreviewActor();

    void SetRenderTarget(UTextureRenderTarget2D* InRenderTarget);
    /** Returns false if no skeletal mesh can be resolved for this profile. */
    bool ShowPlayer(USoccerPlayerProfile* Profile, USoccerClubProfile* Club,
                    ASoccerMatchManager* MatchManager);
    void ClearPlayer();
    void RotateBy(float Degrees);
    void ZoomBy(float DistanceDelta);
    void RefreshCapture();

private:
    static int32 ResolveMaterialIndex(USkeletalMeshComponent* Mesh, FName SlotName, int32 Fallback);
    void ApplyMaterial(FName SlotName, int32 FallbackIndex, UMaterialInterface* Material);
    void RefreshCamera();

    UPROPERTY(VisibleAnywhere, Category = "Preview")
    USceneComponent* SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Preview")
    USkeletalMeshComponent* PreviewMesh = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Preview")
    USceneCaptureComponent2D* Capture = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Preview")
    UPointLightComponent* PreviewLight = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Preview")
    UPointLightComponent* PreviewFillLight = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Preview")
    UPointLightComponent* PreviewRimLight = nullptr;

    UPROPERTY(Transient)
    UTextureRenderTarget2D* RenderTarget = nullptr;

    float CameraDistance = 300.0f;
    float CameraCenterHeight = 95.0f;
    float MeshYaw = 0.0f;
};
