#pragma once

#include "CoreMinimal.h"
#include "SoccerInterceptionTypes.generated.h"

/*
 * Muestra temporal de una trayectoria prevista de la pelota.
 * TimeFromNow siempre se expresa en segundos desde la consulta.
 */
USTRUCT(BlueprintType)
struct THIRDPERSONCPP_API FSoccerBallTrajectorySample
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    float TimeFromNow = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    bool bBallStopped = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    bool bNearGround = false;

    /* Hubo al menos un rebote desde la muestra anterior. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    bool bAfterBounce = false;

    /* Hubo contacto con poste, pared, red u otro obstaculo. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    bool bHitObstacle = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    bool bTrajectoryTerminated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    int32 BounceCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    FVector LastImpactNormal = FVector::ZeroVector;
};

/*
 * Resultado de comparar la trayectoria futura con el tiempo de llegada
 * estimado de un jugador.
 */
USTRUCT(BlueprintType)
struct THIRDPERSONCPP_API FSoccerBallInterceptionResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    bool bHasSolution = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    bool bCanArriveInTime = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    FVector InterceptionLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    float BallArrivalTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    float PlayerArrivalTime = 0.0f;

    /* Positivo: el jugador llega antes. Negativo: llegaria tarde. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    float ArrivalTimeMargin = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    bool bBallStoppedAtSample = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    int32 BounceCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    float BallHeightAboveGround = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Interception")
    int32 TrajectoryRevision = 0;
};
