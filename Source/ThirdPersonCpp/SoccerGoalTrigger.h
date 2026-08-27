//SoccerGoalTrigger.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerTeamTypes.h"
#include "SoccerGoalTrigger.generated.h"

class USceneComponent;
class UBoxComponent;
class ASoccerBall;
class ASoccerMatchManager;
class ASoccerField;

UCLASS()
class THIRDPERSONCPP_API ASoccerGoalTrigger : public AActor
{
	GENERATED_BODY()

public:
	ASoccerGoalTrigger();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void FindMatchManager();

	const ASoccerField* ResolveSoccerField() const;

	FVector WorldToGoalGeometryLocation(const FVector& WorldLocation) const;

	FVector GoalGeometryToWorldLocation(const FVector& GeometryLocation) const;

	ASoccerBall* GetTrackedSoccerBall();

	void ResetGoalLineTracking();

	bool TryDetectGoalLineCrossing(
		const FVector& PreviousLocation,
		const FVector& CurrentLocation,
		FVector& OutCrossingLocation
	) const;

	bool IsLocationInsideGoalMouth(
		const FVector& Location
	) const;

	bool IsLocationPastGoalLineAndInsideGoal(
		const FVector& Location
	) const;

	bool IsBallBackOnFieldSide(
		const FVector& BallLocation
	) const;

	void HandleDetectedGoal(
		const FString& DetectionSource,
		const FVector& DetectionLocation
	);

	ESoccerTeam ResolveScoringTeam() const;

	float GetGoalLineX() const;

	float GetGoalCenterY() const;

	float GetGoalBaseZ() const;

	bool IsRightSideGoal() const;

	float GetGoalScoringDirectionSign() const;

	void ApplyAutomaticGoalPlacement();

	void UpdateGoalBoxVisualization();

	void DrawGoalDetectionDebug(
		const FVector& PreviousLocation,
		const FVector& CurrentLocation,
		const FVector& CrossingLocation,
		bool bDetectedGoal
	) const;

	UFUNCTION()
		void OnGoalBoxBeginOverlap(
			UPrimitiveComponent* OverlappedComponent,
			AActor* OtherActor,
			UPrimitiveComponent* OtherComp,
			int32 OtherBodyIndex,
			bool bFromSweep,
			const FHitResult& SweepResult
		);

	// Raíz separada para que el actor se pueda mover sin que el Box usado
	// para debug/visualización modifique el transform del actor.
	UPROPERTY(VisibleAnywhere, Category = "Soccer|Goal")
		USceneComponent* GoalRoot = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Soccer|Goal")
		UBoxComponent* GoalBox = nullptr;

	// Fallback de compatibilidad si no hay MatchManager disponible. Durante el
	// partido el equipo que marca se resuelve dinamicamente segun quien defiende
	// este extremo, para soportar el cambio de campo del entretiempo.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal")
		ESoccerTeam ScoringTeam = ESoccerTeam::PlayerTeam;

	// ============================================================
	// Placement
	// ============================================================
	// Recomendado en true: usa las medidas de SoccerFieldDimensions para ubicar
	// el plano lógico del arco. Así los BP pueden estar en el centro y aun así
	// detectar el arco correcto según bGoalIsOnRightSideOfField.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Placement")
		bool bUseFieldDimensionsGoalPlane = true;

	// True = arco derecho, X positiva. False = arco izquierdo, X negativa.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Placement")
		bool bGoalIsOnRightSideOfField = true;

	// En editor/BeginPlay mueve automáticamente el actor al centro de su arco.
	// Desactivalo si preferís colocar el actor a mano.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Placement")
		bool bAutoMoveActorToGoalLine = true;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Placement")
		float GoalCenterY = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Placement")
		float GoalBaseZ = 0.0f;

	// Modo viejo/manual. Solo se usa si bUseFieldDimensionsGoalPlane está en false.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Placement|Manual")
		bool bInferGoalSideFromWorldX = true;

	// ============================================================
	// Line Detection
	// ============================================================
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		bool bUseGoalLineCrossingDetection = true;

	// Offset del plano de gol. Con bUseFieldDimensionsGoalPlane=true se suma a
	// LeftGoalLineX/RightGoalLineX. Con false se suma al actor.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		float GoalLineLocalXOffset = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		float GoalDetectionHalfWidth = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		float GoalDetectionHeight = 230.0f;

	// Solo se usa en modo manual. En modo bUseFieldDimensionsGoalPlane=true se
	// usa GoalBaseZ directamente.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		float GoalDetectionBaseZOffset = 0.0f;

	// Margen lateral positivo agranda el arco lógico; negativo lo achica.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		float GoalDetectionSideMargin = 0.0f;

	// Margen superior para no perder goles que rozan el travesaño por pequeñas
	// diferencias físicas/escala de pelota.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		float GoalDetectionTopMargin = 8.0f;

	// Margen inferior para tolerar pequeñas penetraciones visuales del piso.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		float GoalDetectionBottomMargin = 30.0f;

	// Cuánto debe pasar el centro de la pelota detrás de la línea para validar.
	// 0 = gol apenas cruza el centro; 8/12 = un poco más estricto.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		float RequiredBallCenterDepthBeyondLine = 0.0f;

	// Evita registrar dos goles por el mismo cruce. Se libera cuando la pelota
	// vuelve claramente al lado de la cancha.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		float GoalLineRearmFieldSideDistance = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Line Detection")
		float GoalScoreCooldown = 1.0f;

	// Fallback opcional. Por defecto queda apagado para que el gol no dependa
	// de un overlap de caja profunda.
	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Overlap Fallback")
		bool bUseOverlapGoalFallback = false;


	UPROPERTY(EditAnywhere, Category = "Soccer|Goal|Debug")
		float GoalDebugDrawDuration = 0.05f;

	UPROPERTY()
		ASoccerMatchManager* MatchManager = nullptr;

	UPROPERTY()
		TWeakObjectPtr<ASoccerBall> TrackedBall;

	bool bHasPreviousBallLocation = false;

	FVector PreviousBallLocation = FVector::ZeroVector;

	bool bGoalLockedUntilBallReturnsToField = false;

	float LastGoalScoredTime = -1000.0f;
};
