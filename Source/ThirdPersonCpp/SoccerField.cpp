//SoccerField.cpp

#include "SoccerField.h"
#include "SoccerFieldDimensions.h"

#include "Components/SceneComponent.h"
#include "Components/BrushComponent.h"
#include "EngineUtils.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace SoccerFieldTags
{
	static const FName FieldLine(TEXT("FieldLine"));
	static const FName GoalFrame(TEXT("GoalFrame"));
	static const FName GoalNet(TEXT("GoalNet"));
	static const FName None(TEXT("None"));
}

ASoccerField::ASoccerField()
{
	PrimaryActorTick.bCanEverTick = false;

	FieldRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FieldRoot"));
	SetRootComponent(FieldRoot);
	FieldRoot->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube")
	);

	if (!CubeMeshFinder.Succeeded())
	{
		return;
	}

	UStaticMesh* Cube = CubeMeshFinder.Object;

	// ============================================================
	// MEDIDAS GENERALES
	// ============================================================
	// Unreal usa centímetros.
	//
	// Fútbol 7 propuesto:
	// Largo cancha: 60m = 6000 cm
	// Ancho cancha: 40m = 4000 cm
	//
	// Recinto:
	// Largo total: 80m = 8000 cm
	// Ancho total: 60m = 6000 cm
	//
	// Cubo base de Unreal:
	// 100 x 100 x 100 cm.
	// Por eso SizeCm / 100 = Scale.
	// ============================================================

	const float PitchLengthCm =
		SoccerFieldDimensions::PitchLengthCm;

	const float PitchWidthCm =
		SoccerFieldDimensions::PitchWidthCm;

	const float HalfPitchLengthCm =
		SoccerFieldDimensions::HalfPitchLengthCm;

	const float HalfPitchWidthCm =
		SoccerFieldDimensions::HalfPitchWidthCm;

	const float OuterLengthCm =
		SoccerFieldDimensions::OuterLengthCm;

	const float OuterWidthCm =
		SoccerFieldDimensions::OuterWidthCm;

	const float HalfOuterLengthCm =
		SoccerFieldDimensions::HalfOuterLengthCm;

	const float HalfOuterWidthCm =
		SoccerFieldDimensions::HalfOuterWidthCm;

	const float GroundThicknessCm = 20.0f;
	const float PitchVisualThicknessCm = 2.0f;

	const float WallHeightCm = 1000.0f;
	const float WallThicknessCm = 100.0f;

	const float LineThicknessCm = 10.0f;
	const float LineHeightCm = 2.0f;
	const float LineZ = 1.5f;

	// ============================================================
	// PISO Y PAREDES
	// ============================================================

	OuterGround = CreateBoxComponent(
		Cube,
		TEXT("OuterGround"),
		FVector(0.0f, 0.0f, -GroundThicknessCm * 0.5f),
		FVector(OuterLengthCm, OuterWidthCm, GroundThicknessCm),
		true,
		SoccerFieldTags::None,
		true
	);

	PitchSurface = CreateBoxComponent(
		Cube,
		TEXT("PitchSurface"),
		FVector(0.0f, 0.0f, 1.0f),
		FVector(PitchLengthCm, PitchWidthCm, PitchVisualThicknessCm),
		false,
		SoccerFieldTags::None,
		true
	);

	NorthWall = CreateBoxComponent(
		Cube,
		TEXT("NorthWall"),
		FVector(0.0f, HalfOuterWidthCm, WallHeightCm * 0.5f),
		FVector(OuterLengthCm, WallThicknessCm, WallHeightCm),
		true,
		SoccerFieldTags::None,
		false
	);

	SouthWall = CreateBoxComponent(
		Cube,
		TEXT("SouthWall"),
		FVector(0.0f, -HalfOuterWidthCm, WallHeightCm * 0.5f),
		FVector(OuterLengthCm, WallThicknessCm, WallHeightCm),
		true,
		SoccerFieldTags::None,
		false
	);

	EastWall = CreateBoxComponent(
		Cube,
		TEXT("EastWall"),
		FVector(HalfOuterLengthCm, 0.0f, WallHeightCm * 0.5f),
		FVector(WallThicknessCm, OuterWidthCm, WallHeightCm),
		true,
		SoccerFieldTags::None,
		false
	);

	WestWall = CreateBoxComponent(
		Cube,
		TEXT("WestWall"),
		FVector(-HalfOuterLengthCm, 0.0f, WallHeightCm * 0.5f),
		FVector(WallThicknessCm, OuterWidthCm, WallHeightCm),
		true,
		SoccerFieldTags::None,
		false
	);

	// ============================================================
	// LÍNEAS DE LA CANCHA
	// ============================================================

	const float LeftGoalLineX =
		SoccerFieldDimensions::LeftGoalLineX;

	const float RightGoalLineX =
		SoccerFieldDimensions::RightGoalLineX;

	const float NorthTouchLineY =
		SoccerFieldDimensions::NorthTouchLineY;

	const float SouthTouchLineY =
		SoccerFieldDimensions::SouthTouchLineY;

	// Laterales y líneas de fondo.
	CreateLineSegment(
		Cube,
		TEXT("Line_NorthTouchLine"),
		FVector(LeftGoalLineX, NorthTouchLineY, LineZ),
		FVector(RightGoalLineX, NorthTouchLineY, LineZ),
		LineThicknessCm,
		LineHeightCm
	);

	CreateLineSegment(
		Cube,
		TEXT("Line_SouthTouchLine"),
		FVector(LeftGoalLineX, SouthTouchLineY, LineZ),
		FVector(RightGoalLineX, SouthTouchLineY, LineZ),
		LineThicknessCm,
		LineHeightCm
	);

	CreateLineSegment(
		Cube,
		TEXT("Line_LeftGoalLine"),
		FVector(LeftGoalLineX, SouthTouchLineY, LineZ),
		FVector(LeftGoalLineX, NorthTouchLineY, LineZ),
		LineThicknessCm,
		LineHeightCm
	);

	CreateLineSegment(
		Cube,
		TEXT("Line_RightGoalLine"),
		FVector(RightGoalLineX, SouthTouchLineY, LineZ),
		FVector(RightGoalLineX, NorthTouchLineY, LineZ),
		LineThicknessCm,
		LineHeightCm
	);

	// Línea media.
	CreateLineSegment(
		Cube,
		TEXT("Line_HalfwayLine"),
		FVector(0.0f, SouthTouchLineY, LineZ),
		FVector(0.0f, NorthTouchLineY, LineZ),
		LineThicknessCm,
		LineHeightCm
	);

	// Círculo central.
	const float CenterCircleRadiusCm =
		SoccerFieldDimensions::CenterCircleRadiusCm;
	CreateCircleMarking(
		Cube,
		TEXT("Line_CenterCircle"),
		FVector(0.0f, 0.0f, LineZ),
		CenterCircleRadiusCm,
		64,
		LineThicknessCm,
		LineHeightCm
	);

	// Punto central.
	CreateBoxComponent(
		Cube,
		TEXT("Mark_CenterSpot"),
		FVector(0.0f, 0.0f, LineZ),
		FVector(22.0f, 22.0f, LineHeightCm),
		false,
		SoccerFieldTags::FieldLine,
		true
	);

	// ============================================================
	// ÁREAS PENALES FÚTBOL 7
	// ============================================================
	// Modelo práctico para juego:
	// Área rectangular:
	// Profundidad: 13m
	// Ancho: 20m
	//
	// Punto penal:
	// 9m desde línea de gol.
	// ============================================================

	const float PenaltyAreaDepthCm =
		SoccerFieldDimensions::PenaltyAreaDepthCm;

	const float PenaltyAreaWidthCm =
		SoccerFieldDimensions::PenaltyAreaWidthCm;

	const float PenaltyAreaHalfWidthCm =
		SoccerFieldDimensions::PenaltyAreaHalfWidthCm;

	const float LeftPenaltyFrontX = LeftGoalLineX + PenaltyAreaDepthCm;
	const float RightPenaltyFrontX = RightGoalLineX - PenaltyAreaDepthCm;

	// Áreas penales izquierda y derecha.
	// Las dos usan la misma construcción rectangular que las áreas de meta.
	CreateGoalLineAreaMarking(
		Cube,
		TEXT("Line_LeftPenaltyArea"),
		LeftGoalLineX,
		LeftPenaltyFrontX,
		PenaltyAreaHalfWidthCm,
		LineThicknessCm,
		LineHeightCm,
		LineZ
	);

	CreateGoalLineAreaMarking(
		Cube,
		TEXT("Line_RightPenaltyArea"),
		RightGoalLineX,
		RightPenaltyFrontX,
		PenaltyAreaHalfWidthCm,
		LineThicknessCm,
		LineHeightCm,
		LineZ
	);

	// Puntos penales.
	CreateBoxComponent(
		Cube,
		TEXT("Mark_LeftPenaltySpot"),
		SoccerFieldDimensions::GetPenaltySpotLocalLocation(-1.0f, LineZ),
		FVector(24.0f, 24.0f, LineHeightCm),
		false,
		SoccerFieldTags::FieldLine,
		true
	);

	CreateBoxComponent(
		Cube,
		TEXT("Mark_RightPenaltySpot"),
		SoccerFieldDimensions::GetPenaltySpotLocalLocation(1.0f, LineZ),
		FVector(24.0f, 24.0f, LineHeightCm),
		false,
		SoccerFieldTags::FieldLine,
		true
	);

	// Arcos penales (la "D"). Se dibuja solamente la porción del círculo
	// reglamentario que queda fuera del rectángulo del área. El mismo radio
	// gobierna después la legalidad de los jugadores durante un penal.
	const float PenaltyArcRadiusCm =
		SoccerFieldDimensions::PenaltyArcRadiusCm;
	const float PenaltyArcHalfAngleDegrees =
		SoccerFieldDimensions::GetPenaltyArcHalfAngleDegrees();

	if (
		PenaltyArcRadiusCm > KINDA_SMALL_NUMBER &&
		PenaltyArcHalfAngleDegrees > KINDA_SMALL_NUMBER
	)
	{
		CreateArcMarking(
			Cube,
			TEXT("Line_LeftPenaltyArc"),
			SoccerFieldDimensions::GetPenaltySpotLocalLocation(-1.0f, LineZ),
			PenaltyArcRadiusCm,
			-PenaltyArcHalfAngleDegrees,
			PenaltyArcHalfAngleDegrees,
			32,
			LineThicknessCm,
			LineHeightCm
		);

		CreateArcMarking(
			Cube,
			TEXT("Line_RightPenaltyArc"),
			SoccerFieldDimensions::GetPenaltySpotLocalLocation(1.0f, LineZ),
			PenaltyArcRadiusCm,
			180.0f - PenaltyArcHalfAngleDegrees,
			180.0f + PenaltyArcHalfAngleDegrees,
			32,
			LineThicknessCm,
			LineHeightCm
		);
	}

	// ============================================================
	// ÁREAS DE META / ÁREAS CHICAS
	// ============================================================
	// Se dibujan con el mismo criterio que las áreas penales:
	// la línea de fondo ya forma el cuarto lado del rectángulo.
	// ============================================================

	const float GoalAreaHalfWidthCm =
		SoccerFieldDimensions::GoalAreaHalfWidthCm;

	CreateGoalLineAreaMarking(
		Cube,
		TEXT("Line_LeftGoalArea"),
		LeftGoalLineX,
		SoccerFieldDimensions::LeftGoalAreaFrontX,
		GoalAreaHalfWidthCm,
		LineThicknessCm,
		LineHeightCm,
		LineZ
	);

	CreateGoalLineAreaMarking(
		Cube,
		TEXT("Line_RightGoalArea"),
		RightGoalLineX,
		SoccerFieldDimensions::RightGoalAreaFrontX,
		GoalAreaHalfWidthCm,
		LineThicknessCm,
		LineHeightCm,
		LineZ
	);

	// ============================================================
	// ARCOS DE ESQUINA
	// ============================================================

	const float CornerArcRadiusCm =
		SoccerFieldDimensions::CornerArcRadiusCm;

	CreateArcMarking(
		Cube,
		TEXT("Line_CornerArc_LeftSouth"),
		FVector(LeftGoalLineX, SouthTouchLineY, LineZ),
		CornerArcRadiusCm,
		0.0f,
		90.0f,
		12,
		LineThicknessCm,
		LineHeightCm
	);

	CreateArcMarking(
		Cube,
		TEXT("Line_CornerArc_LeftNorth"),
		FVector(LeftGoalLineX, NorthTouchLineY, LineZ),
		CornerArcRadiusCm,
		270.0f,
		360.0f,
		12,
		LineThicknessCm,
		LineHeightCm
	);

	CreateArcMarking(
		Cube,
		TEXT("Line_CornerArc_RightSouth"),
		FVector(RightGoalLineX, SouthTouchLineY, LineZ),
		CornerArcRadiusCm,
		90.0f,
		180.0f,
		12,
		LineThicknessCm,
		LineHeightCm
	);

	CreateArcMarking(
		Cube,
		TEXT("Line_CornerArc_RightNorth"),
		FVector(RightGoalLineX, NorthTouchLineY, LineZ),
		CornerArcRadiusCm,
		180.0f,
		270.0f,
		12,
		LineThicknessCm,
		LineHeightCm
	);

	// ============================================================
	// ARCOS / PORTERÍAS
	// ============================================================
	// Fútbol 7:
	// Arco propuesto: 6m de ancho x 2m de alto.
	//
	// La pelota rebota porque estos componentes tienen colisión:
	// QueryAndPhysics + WorldStatic + Block.
	// ============================================================

	const float GoalWidthCm =
		SoccerFieldDimensions::GoalWidthCm;

	const float GoalHalfWidthCm =
		SoccerFieldDimensions::GoalHalfWidthCm;

	const float GoalHeightCm =
		SoccerFieldDimensions::GoalHeightCm;

	const float GoalPostThicknessCm =
		SoccerFieldDimensions::GoalPostThicknessCm;

	// Arco izquierdo.
	CreateBoxComponent(
		Cube,
		TEXT("Goal_Left_NorthPost"),
		FVector(LeftGoalLineX, GoalHalfWidthCm, GoalHeightCm * 0.5f),
		FVector(GoalPostThicknessCm, GoalPostThicknessCm, GoalHeightCm),
		true,
		SoccerFieldTags::GoalFrame,
		false
	);

	CreateBoxComponent(
		Cube,
		TEXT("Goal_Left_SouthPost"),
		FVector(LeftGoalLineX, -GoalHalfWidthCm, GoalHeightCm * 0.5f),
		FVector(GoalPostThicknessCm, GoalPostThicknessCm, GoalHeightCm),
		true,
		SoccerFieldTags::GoalFrame,
		false
	);

	CreateBoxComponent(
		Cube,
		TEXT("Goal_Left_Crossbar"),
		FVector(LeftGoalLineX, 0.0f, GoalHeightCm),
		FVector(GoalPostThicknessCm, GoalWidthCm + GoalPostThicknessCm, GoalPostThicknessCm),
		true,
		SoccerFieldTags::GoalFrame,
		false
	);

	// Arco derecho.
	CreateBoxComponent(
		Cube,
		TEXT("Goal_Right_NorthPost"),
		FVector(RightGoalLineX, GoalHalfWidthCm, GoalHeightCm * 0.5f),
		FVector(GoalPostThicknessCm, GoalPostThicknessCm, GoalHeightCm),
		true,
		SoccerFieldTags::GoalFrame,
		false
	);

	CreateBoxComponent(
		Cube,
		TEXT("Goal_Right_SouthPost"),
		FVector(RightGoalLineX, -GoalHalfWidthCm, GoalHeightCm * 0.5f),
		FVector(GoalPostThicknessCm, GoalPostThicknessCm, GoalHeightCm),
		true,
		SoccerFieldTags::GoalFrame,
		false
	);

	CreateBoxComponent(
		Cube,
		TEXT("Goal_Right_Crossbar"),
		FVector(RightGoalLineX, 0.0f, GoalHeightCm),
		FVector(GoalPostThicknessCm, GoalWidthCm + GoalPostThicknessCm, GoalPostThicknessCm),
		true,
		SoccerFieldTags::GoalFrame,
		false
	);

	// ============================================================
// REDES DE LOS ARCOS
// ============================================================
// Paneles finos con colisión para que la pelota pueda quedar
// visualmente dentro del arco después del gol.
// La boca del arco queda abierta: solo hay red atrás, costados y arriba.
// ============================================================

	const float GoalDepthCm =
		SoccerFieldDimensions::GoalDepthCm;

	const float GoalNetThicknessCm =
		SoccerFieldDimensions::GoalNetThicknessCm;

	auto ConfigureGoalNetCollision =
		[](UStaticMeshComponent* NetComponent)
	{
		if (NetComponent == nullptr)
		{
			return;
		}

		NetComponent->SetCollisionEnabled(
			ECollisionEnabled::QueryAndPhysics
		);

		NetComponent->SetCollisionObjectType(
			ECC_WorldStatic
		);

		NetComponent->SetCollisionResponseToAllChannels(
			ECR_Ignore
		);

		// La pelota usa ECC_PhysicsBody en ASoccerBall.
		NetComponent->SetCollisionResponseToChannel(
			ECC_PhysicsBody,
			ECR_Block
		);

		// Los jugadores usan normalmente ECC_Pawn.
		// Esto evita que atraviesen la red.
		NetComponent->SetCollisionResponseToChannel(
			ECC_Pawn,
			ECR_Block
		);

		// Opcional para traces/debug.
		NetComponent->SetCollisionResponseToChannel(
			ECC_Visibility,
			ECR_Ignore
		);

		// Opcional: que la cámara no choque con la red.
		NetComponent->SetCollisionResponseToChannel(
			ECC_Camera,
			ECR_Ignore
		);

		NetComponent->SetGenerateOverlapEvents(false);

		NetComponent->CanCharacterStepUpOn =
			ECB_No;

		// Recomendado para que la IA no intente pathfindear atravesando la red.
		NetComponent->SetCanEverAffectNavigation(true);
	};

	// --------------------
	// Red arco izquierdo.
	// El arco izquierdo mira hacia -X por detrás de la línea.
	// --------------------

	UStaticMeshComponent* LeftBackNet =
		CreateBoxComponent(
			Cube,
			TEXT("Goal_Left_BackNet"),
			FVector(
				LeftGoalLineX - GoalDepthCm,
				0.0f,
				GoalHeightCm * 0.5f
			),
			FVector(
				GoalNetThicknessCm,
				GoalWidthCm + GoalPostThicknessCm,
				GoalHeightCm
			),
			true,
			SoccerFieldTags::GoalNet,
			false
		);

	ConfigureGoalNetCollision(LeftBackNet);

	UStaticMeshComponent* LeftNorthSideNet =
		CreateBoxComponent(
			Cube,
			TEXT("Goal_Left_NorthSideNet"),
			FVector(
				LeftGoalLineX - GoalDepthCm * 0.5f,
				GoalHalfWidthCm + GoalNetThicknessCm * 0.5f,
				GoalHeightCm * 0.5f
			),
			FVector(
				GoalDepthCm,
				GoalNetThicknessCm,
				GoalHeightCm
			),
			true,
			SoccerFieldTags::GoalNet,
			false
		);

	ConfigureGoalNetCollision(LeftNorthSideNet);

	UStaticMeshComponent* LeftSouthSideNet =
		CreateBoxComponent(
			Cube,
			TEXT("Goal_Left_SouthSideNet"),
			FVector(
				LeftGoalLineX - GoalDepthCm * 0.5f,
				-GoalHalfWidthCm - GoalNetThicknessCm * 0.5f,
				GoalHeightCm * 0.5f
			),
			FVector(
				GoalDepthCm,
				GoalNetThicknessCm,
				GoalHeightCm
			),
			true,
			SoccerFieldTags::GoalNet,
			false
		);

	ConfigureGoalNetCollision(LeftSouthSideNet);

	UStaticMeshComponent* LeftTopNet =
		CreateBoxComponent(
			Cube,
			TEXT("Goal_Left_TopNet"),
			FVector(
				LeftGoalLineX - GoalDepthCm * 0.5f,
				0.0f,
				GoalHeightCm + GoalNetThicknessCm * 0.5f
			),
			FVector(
				GoalDepthCm,
				GoalWidthCm + GoalPostThicknessCm,
				GoalNetThicknessCm
			),
			true,
			SoccerFieldTags::GoalNet,
			false
		);

	ConfigureGoalNetCollision(LeftTopNet);

	// --------------------
	// Red arco derecho.
	// El arco derecho mira hacia +X por detrás de la línea.
	// --------------------

	UStaticMeshComponent* RightBackNet =
		CreateBoxComponent(
			Cube,
			TEXT("Goal_Right_BackNet"),
			FVector(
				RightGoalLineX + GoalDepthCm,
				0.0f,
				GoalHeightCm * 0.5f
			),
			FVector(
				GoalNetThicknessCm,
				GoalWidthCm + GoalPostThicknessCm,
				GoalHeightCm
			),
			true,
			SoccerFieldTags::GoalNet,
			false
		);

	ConfigureGoalNetCollision(RightBackNet);

	UStaticMeshComponent* RightNorthSideNet =
		CreateBoxComponent(
			Cube,
			TEXT("Goal_Right_NorthSideNet"),
			FVector(
				RightGoalLineX + GoalDepthCm * 0.5f,
				GoalHalfWidthCm + GoalNetThicknessCm * 0.5f,
				GoalHeightCm * 0.5f
			),
			FVector(
				GoalDepthCm,
				GoalNetThicknessCm,
				GoalHeightCm
			),
			true,
			SoccerFieldTags::GoalNet,
			false
		);

	ConfigureGoalNetCollision(RightNorthSideNet);

	UStaticMeshComponent* RightSouthSideNet =
		CreateBoxComponent(
			Cube,
			TEXT("Goal_Right_SouthSideNet"),
			FVector(
				RightGoalLineX + GoalDepthCm * 0.5f,
				-GoalHalfWidthCm - GoalNetThicknessCm * 0.5f,
				GoalHeightCm * 0.5f
			),
			FVector(
				GoalDepthCm,
				GoalNetThicknessCm,
				GoalHeightCm
			),
			true,
			SoccerFieldTags::GoalNet,
			false
		);

	ConfigureGoalNetCollision(RightSouthSideNet);

	UStaticMeshComponent* RightTopNet =
		CreateBoxComponent(
			Cube,
			TEXT("Goal_Right_TopNet"),
			FVector(
				RightGoalLineX + GoalDepthCm * 0.5f,
				0.0f,
				GoalHeightCm + GoalNetThicknessCm * 0.5f
			),
			FVector(
				GoalDepthCm,
				GoalWidthCm + GoalPostThicknessCm,
				GoalNetThicknessCm
			),
			true,
			SoccerFieldTags::GoalNet,
			false
		);

	ConfigureGoalNetCollision(RightTopNet);
}

void ASoccerField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateNavigationBounds();

	if (OuterGround != nullptr && OuterGroundMaterial != nullptr)
	{
		OuterGround->SetMaterial(0, OuterGroundMaterial);
	}

	if (PitchSurface != nullptr && PitchSurfaceMaterial != nullptr)
	{
		PitchSurface->SetMaterial(0, PitchSurfaceMaterial);
	}

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents<UStaticMeshComponent>(StaticMeshComponents);

	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		if (Component == nullptr)
		{
			continue;
		}

		if (LineMaterial != nullptr && Component->ComponentTags.Contains(SoccerFieldTags::FieldLine))
		{
			Component->SetMaterial(0, LineMaterial);
		}

		if (GoalFrameMaterial != nullptr && Component->ComponentTags.Contains(SoccerFieldTags::GoalFrame))
		{
			Component->SetMaterial(0, GoalFrameMaterial);
		}

		if (GoalNetMaterial != nullptr && Component->ComponentTags.Contains(SoccerFieldTags::GoalNet))
		{
			Component->SetMaterial(0, GoalNetMaterial);
		}
	}
}

void ASoccerField::BeginPlay()
{
	Super::BeginPlay();

	// OnConstruction keeps the editor representation synchronized, while this
	// second pass guarantees that cooked/runtime worlds use the same bounds.
	UpdateNavigationBounds();
}

ANavMeshBoundsVolume* ASoccerField::FindNavigationBoundsVolume() const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector PitchCenter = GetPitchCenterWorldLocation();
	ANavMeshBoundsVolume* BestVolume = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		ANavMeshBoundsVolume* Candidate = *It;
		if (Candidate == nullptr)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(
			Candidate->GetActorLocation(),
			PitchCenter
		);

		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestVolume = Candidate;
		}
	}

	return BestVolume;
}

void ASoccerField::UpdateNavigationBounds()
{
	if (!bAutoSizeNavigationBounds)
	{
		return;
	}

	ANavMeshBoundsVolume* NavigationBoundsVolume = FindNavigationBoundsVolume();
	if (NavigationBoundsVolume == nullptr)
	{
		return;
	}

	UBrushComponent* BrushComponent = NavigationBoundsVolume->GetBrushComponent();
	if (BrushComponent == nullptr)
	{
		return;
	}

	// Query the brush in identity space instead of assuming the 6000x4000x200
	// dimensions currently configured in the level. This keeps the sizing
	// idempotent even after the volume has already been resized once.
	const FBoxSphereBounds LocalBrushBounds =
		BrushComponent->CalcBounds(FTransform::Identity);
	const FVector BaseBrushSize = LocalBrushBounds.BoxExtent * 2.0f;

	if (
		BaseBrushSize.X <= KINDA_SMALL_NUMBER ||
		BaseBrushSize.Y <= KINDA_SMALL_NUMBER ||
		BaseBrushSize.Z <= KINDA_SMALL_NUMBER
	)
	{
		return;
	}

	const float SafeOutsideMargin = FMath::Max(0.0f, NavigationBoundsOutsideMarginCm);
	const float SafeHeight = FMath::Max(1.0f, NavigationBoundsHeightCm);
	const FVector FieldScale = GetActorScale3D().GetAbs();

	const FVector DesiredWorldSize(
		(SoccerFieldDimensions::PitchLengthCm + SafeOutsideMargin * 2.0f) * FieldScale.X,
		(SoccerFieldDimensions::PitchWidthCm + SafeOutsideMargin * 2.0f) * FieldScale.Y,
		SafeHeight * FieldScale.Z
	);

	const FVector DesiredVolumeScale(
		DesiredWorldSize.X / BaseBrushSize.X,
		DesiredWorldSize.Y / BaseBrushSize.Y,
		DesiredWorldSize.Z / BaseBrushSize.Z
	);

	const float CenterLocalZ =
		NavigationBoundsBottomLocalZ + SafeHeight * 0.5f;
	const FVector DesiredBrushCenterWorld = GetPitchCenterWorldLocation(CenterLocalZ);
	const FRotator DesiredRotation = GetActorRotation();

	// Compensate if the editor brush pivot is not exactly at the brush center.
	const FVector ScaledLocalBrushCenter =
		LocalBrushBounds.Origin * DesiredVolumeScale;
	const FVector DesiredActorLocation =
		DesiredBrushCenterWorld - DesiredRotation.RotateVector(ScaledLocalBrushCenter);

	const bool bLocationChanged =
		!NavigationBoundsVolume->GetActorLocation().Equals(DesiredActorLocation, 0.1f);
	const bool bRotationChanged =
		!NavigationBoundsVolume->GetActorRotation().Equals(DesiredRotation, 0.01f);
	const bool bScaleChanged =
		!NavigationBoundsVolume->GetActorScale3D().Equals(DesiredVolumeScale, 0.0001f);

	if (!bLocationChanged && !bRotationChanged && !bScaleChanged)
	{
		return;
	}

	USceneComponent* NavigationRoot = NavigationBoundsVolume->GetRootComponent();
	if (NavigationRoot == nullptr)
	{
		return;
	}

	const EComponentMobility::Type OriginalMobility = NavigationRoot->Mobility;
	if (OriginalMobility == EComponentMobility::Static)
	{
		NavigationRoot->SetMobility(EComponentMobility::Stationary);
	}

	NavigationBoundsVolume->SetActorLocationAndRotation(
		DesiredActorLocation,
		DesiredRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);
	NavigationBoundsVolume->SetActorScale3D(DesiredVolumeScale);

	// UE4 registers navigation bounds from the brush component bounds. Refresh
	// them explicitly before notifying the navigation system.
	NavigationRoot->UpdateBounds();
	BrushComponent->UpdateBounds();

	if (OriginalMobility == EComponentMobility::Static)
	{
		NavigationRoot->SetMobility(EComponentMobility::Static);
	}

	UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	if (NavigationSystem != nullptr)
	{
		NavigationSystem->OnNavigationBoundsUpdated(NavigationBoundsVolume);
	}
}

FVector ASoccerField::PitchLocalToWorld(const FVector& LocalLocation) const
{
	return GetActorTransform().TransformPosition(LocalLocation);
}

FVector ASoccerField::WorldToPitchLocal(const FVector& WorldLocation) const
{
	return GetActorTransform().InverseTransformPosition(WorldLocation);
}

FVector ASoccerField::GetPitchCenterWorldLocation(float LocalZ) const
{
	return PitchLocalToWorld(FVector(
		SoccerFieldDimensions::HalfwayLineX,
		SoccerFieldDimensions::CenterY,
		LocalZ
	));
}

FVector ASoccerField::GetGoalCenterWorldLocation(
	float GoalLineSign,
	float LocalZ
) const
{
	return PitchLocalToWorld(
		SoccerFieldDimensions::GetGoalCenterLocalLocation(GoalLineSign, LocalZ)
	);
}

FVector ASoccerField::GetPenaltySpotWorldLocation(
	float GoalLineSign,
	float LocalZ
) const
{
	return PitchLocalToWorld(
		SoccerFieldDimensions::GetPenaltySpotLocalLocation(GoalLineSign, LocalZ)
	);
}

FVector ASoccerField::GetCornerWorldLocation(
	float GoalLineSign,
	float TouchlineSign,
	float InsetCm,
	float LocalZ
) const
{
	return PitchLocalToWorld(
		SoccerFieldDimensions::GetCornerLocalLocation(
			GoalLineSign,
			TouchlineSign,
			InsetCm,
			LocalZ
		)
	);
}

bool ASoccerField::IsWorldLocationInsidePitch(
	const FVector& WorldLocation,
	float InsetCm
) const
{
	return SoccerFieldDimensions::IsLocationInsidePitch2D(
		WorldToPitchLocal(WorldLocation),
		InsetCm
	);
}

bool ASoccerField::IsWorldLocationInsideGoalArea(
	const FVector& WorldLocation,
	float GoalLineSign,
	float MarginCm
) const
{
	return SoccerFieldDimensions::IsLocationInsideGoalArea2D(
		WorldToPitchLocal(WorldLocation),
		GoalLineSign,
		MarginCm
	);
}

bool ASoccerField::IsWorldLocationInsidePenaltyArea(
	const FVector& WorldLocation,
	float GoalLineSign,
	float ExtraDepthCm,
	float ExtraHalfWidthCm
) const
{
	return SoccerFieldDimensions::IsLocationInsidePenaltyArea2D(
		WorldToPitchLocal(WorldLocation),
		GoalLineSign,
		ExtraDepthCm,
		ExtraHalfWidthCm
	);
}

FVector ASoccerField::ClampWorldLocationInsidePitch(
	const FVector& WorldLocation,
	float InsetCm
) const
{
	const FVector LocalLocation = WorldToPitchLocal(WorldLocation);
	return PitchLocalToWorld(
		SoccerFieldDimensions::ClampLocationInsidePitch(LocalLocation, InsetCm)
	);
}

FVector ASoccerField::GetPitchLengthWorldDirection() const
{
	return GetActorTransform()
		.TransformVectorNoScale(FVector::ForwardVector)
		.GetSafeNormal();
}

FVector ASoccerField::GetPitchWidthWorldDirection() const
{
	return GetActorTransform()
		.TransformVectorNoScale(FVector::RightVector)
		.GetSafeNormal();
}

float ASoccerField::GetNearestGoalLineSign(const FVector& WorldLocation) const
{
	const FVector LocalLocation = WorldToPitchLocal(WorldLocation);

	const float DistanceToLeftGoalLine =
		FMath::Abs(LocalLocation.X - SoccerFieldDimensions::LeftGoalLineX);

	const float DistanceToRightGoalLine =
		FMath::Abs(LocalLocation.X - SoccerFieldDimensions::RightGoalLineX);

	return DistanceToLeftGoalLine <= DistanceToRightGoalLine
		? -1.0f
		: 1.0f;
}

UStaticMeshComponent* ASoccerField::CreateBoxComponent(
	UStaticMesh* Mesh,
	const FString& ComponentName,
	const FVector& Location,
	const FVector& SizeCm,
	bool bEnableCollision,
	FName ComponentTag,
	bool bCanCharacterStepUp
)
{
	UStaticMeshComponent* Component =
		CreateDefaultSubobject<UStaticMeshComponent>(FName(*ComponentName));

	if (Component == nullptr)
	{
		return nullptr;
	}

	Component->SetupAttachment(RootComponent);
	Component->SetStaticMesh(Mesh);
	Component->SetRelativeLocation(Location);
	Component->SetRelativeScale3D(SizeCm / 100.0f);
	Component->SetMobility(EComponentMobility::Static);

	if (ComponentTag != SoccerFieldTags::None)
	{
		Component->ComponentTags.Add(ComponentTag);
	}

	if (bEnableCollision)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Component->SetCollisionObjectType(ECC_WorldStatic);
		Component->SetCollisionResponseToAllChannels(ECR_Block);

		if (ComponentTag == SoccerFieldTags::GoalFrame)
		{
			Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		else
		{
			Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		}

		Component->SetGenerateOverlapEvents(false);

		Component->CanCharacterStepUpOn =
			bCanCharacterStepUp ? ECB_Yes : ECB_No;
	}
	else
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	return Component;
}

UStaticMeshComponent* ASoccerField::CreateLineSegment(
	UStaticMesh* Mesh,
	const FString& ComponentName,
	const FVector& Start,
	const FVector& End,
	float ThicknessCm,
	float HeightCm
)
{
	const FVector Delta = End - Start;
	const float LengthCm = Delta.Size2D();

	if (LengthCm <= 1.0f)
	{
		return nullptr;
	}

	const FVector Center = (Start + End) * 0.5f;

	UStaticMeshComponent* Component = CreateBoxComponent(
		Mesh,
		ComponentName,
		Center,
		FVector(LengthCm, ThicknessCm, HeightCm),
		false,
		SoccerFieldTags::FieldLine,
		true
	);

	if (Component == nullptr)
	{
		return nullptr;
	}

	const float YawDegrees =
		FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));

	Component->SetRelativeRotation(FRotator(0.0f, YawDegrees, 0.0f));

	return Component;
}

void ASoccerField::CreateGoalLineAreaMarking(
	UStaticMesh* Mesh,
	const FString& Prefix,
	float GoalLineX,
	float FrontLineX,
	float HalfWidthCm,
	float ThicknessCm,
	float HeightCm,
	float Z
)
{
	const float SafeHalfWidthCm = FMath::Max(0.0f, HalfWidthCm);

	CreateLineSegment(
		Mesh,
		Prefix + TEXT("_Front"),
		FVector(FrontLineX, -SafeHalfWidthCm, Z),
		FVector(FrontLineX, SafeHalfWidthCm, Z),
		ThicknessCm,
		HeightCm
	);

	CreateLineSegment(
		Mesh,
		Prefix + TEXT("_North"),
		FVector(GoalLineX, SafeHalfWidthCm, Z),
		FVector(FrontLineX, SafeHalfWidthCm, Z),
		ThicknessCm,
		HeightCm
	);

	CreateLineSegment(
		Mesh,
		Prefix + TEXT("_South"),
		FVector(GoalLineX, -SafeHalfWidthCm, Z),
		FVector(FrontLineX, -SafeHalfWidthCm, Z),
		ThicknessCm,
		HeightCm
	);
}

void ASoccerField::CreateCircleMarking(
	UStaticMesh* Mesh,
	const FString& Prefix,
	const FVector& Center,
	float RadiusCm,
	int32 Segments,
	float ThicknessCm,
	float HeightCm
)
{
	const int32 SafeSegments = FMath::Max(8, Segments);

	for (int32 Index = 0; Index < SafeSegments; ++Index)
	{
		const float StartAngleRadians =
			2.0f * PI * static_cast<float>(Index) / static_cast<float>(SafeSegments);

		const float EndAngleRadians =
			2.0f * PI * static_cast<float>(Index + 1) / static_cast<float>(SafeSegments);

		const FVector StartPoint(
			Center.X + FMath::Cos(StartAngleRadians) * RadiusCm,
			Center.Y + FMath::Sin(StartAngleRadians) * RadiusCm,
			Center.Z
		);

		const FVector EndPoint(
			Center.X + FMath::Cos(EndAngleRadians) * RadiusCm,
			Center.Y + FMath::Sin(EndAngleRadians) * RadiusCm,
			Center.Z
		);

		const FString SegmentName =
			FString::Printf(TEXT("%s_%02d"), *Prefix, Index);

		CreateLineSegment(
			Mesh,
			SegmentName,
			StartPoint,
			EndPoint,
			ThicknessCm,
			HeightCm
		);
	}
}

void ASoccerField::CreateArcMarking(
	UStaticMesh* Mesh,
	const FString& Prefix,
	const FVector& Center,
	float RadiusCm,
	float StartDegrees,
	float EndDegrees,
	int32 Segments,
	float ThicknessCm,
	float HeightCm
)
{
	const int32 SafeSegments = FMath::Max(2, Segments);

	for (int32 Index = 0; Index < SafeSegments; ++Index)
	{
		const float AlphaA =
			static_cast<float>(Index) / static_cast<float>(SafeSegments);

		const float AlphaB =
			static_cast<float>(Index + 1) / static_cast<float>(SafeSegments);

		const float AngleADegrees =
			FMath::Lerp(StartDegrees, EndDegrees, AlphaA);

		const float AngleBDegrees =
			FMath::Lerp(StartDegrees, EndDegrees, AlphaB);

		const float AngleARadians =
			FMath::DegreesToRadians(AngleADegrees);

		const float AngleBRadians =
			FMath::DegreesToRadians(AngleBDegrees);

		const FVector StartPoint(
			Center.X + FMath::Cos(AngleARadians) * RadiusCm,
			Center.Y + FMath::Sin(AngleARadians) * RadiusCm,
			Center.Z
		);

		const FVector EndPoint(
			Center.X + FMath::Cos(AngleBRadians) * RadiusCm,
			Center.Y + FMath::Sin(AngleBRadians) * RadiusCm,
			Center.Z
		);

		const FString SegmentName =
			FString::Printf(TEXT("%s_%02d"), *Prefix, Index);

		CreateLineSegment(
			Mesh,
			SegmentName,
			StartPoint,
			EndPoint,
			ThicknessCm,
			HeightCm
		);
	}
}

