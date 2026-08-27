//SoccerOffsideLineActor.cpp

#include "SoccerOffsideLineActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASoccerOffsideLineActor::ASoccerOffsideLineActor()
{
	PrimaryActorTick.bCanEverTick = false;

	LineRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LineRoot"));
	SetRootComponent(LineRoot);
	LineRoot->SetMobility(EComponentMobility::Movable);

	LineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LineMesh"));
	LineMesh->SetupAttachment(LineRoot);
	LineMesh->SetMobility(EComponentMobility::Movable);
	LineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LineMesh->SetGenerateOverlapEvents(false);
	LineMesh->SetCanEverAffectNavigation(false);
	LineMesh->SetCastShadow(false);
	LineMesh->SetReceivesDecals(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube")
	);

	if (CubeMeshFinder.Succeeded())
	{
		LineMesh->SetStaticMesh(CubeMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
	);

	if (MaterialFinder.Succeeded())
	{
		BaseLineMaterial = MaterialFinder.Object;
		LineMesh->SetMaterial(0, BaseLineMaterial);
	}
}

void ASoccerOffsideLineActor::ConfigureLine(
	const FVector& CenterLocation,
	const FVector& SizeCm,
	const FLinearColor& LineColor
)
{
	SetActorLocation(
		CenterLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	// El cubo basico de Unreal mide 100 cm por lado.
	LineMesh->SetRelativeScale3D(
		FVector(
			FMath::Max(1.0f, SizeCm.X) / 100.0f,
			FMath::Max(1.0f, SizeCm.Y) / 100.0f,
			FMath::Max(1.0f, SizeCm.Z) / 100.0f
		)
	);

	if (BaseLineMaterial != nullptr)
	{
		DynamicLineMaterial =
			UMaterialInstanceDynamic::Create(BaseLineMaterial, this);

		if (DynamicLineMaterial != nullptr)
		{
			// BasicShapeMaterial usa el parametro Color. Se asigna tambien
			// BaseColor para permitir reemplazar el material por uno propio.
			DynamicLineMaterial->SetVectorParameterValue(
				TEXT("Color"),
				LineColor
			);

			DynamicLineMaterial->SetVectorParameterValue(
				TEXT("BaseColor"),
				LineColor
			);

			LineMesh->SetMaterial(0, DynamicLineMaterial);
		}
	}

	LineMesh->SetVisibility(true, true);
	SetActorHiddenInGame(false);
}
