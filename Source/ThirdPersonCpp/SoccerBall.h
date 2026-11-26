//SoccerBall.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoccerInterceptionTypes.h"
#include "SoccerBall.generated.h"

class UStaticMeshComponent;
class ASoccerCharacterBase;
class ASoccerMatchManager;
class USoundBase;
class USoundAttenuation;

UCLASS()
class THIRDPERSONCPP_API ASoccerBall : public AActor
{
    GENERATED_BODY()

public:
    ASoccerBall();

    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintPure, Category = "Ball")
    float GetBallRadiusCm() const;

    /*
     * Etapa 3: construye una trayectoria general para pelota libre.
     * Incluye vuelo, gravedad, damping, suelo, rebotes y obstaculos.
     */
    bool BuildPredictedTrajectory(
        float MaxPredictionTime,
        float SampleInterval,
        TArray<FSoccerBallTrajectorySample>& OutSamples
    ) const;

    /* Compatibilidad con las etapas 1 y 2. Solo acepta inicio terrestre. */
    bool BuildPredictedGroundTrajectory(
        float MaxPredictionTime,
        float SampleInterval,
        TArray<FSoccerBallTrajectorySample>& OutSamples
    ) const;

    UFUNCTION(BlueprintPure, Category = "Ball|Prediction")
    FVector GetBallPhysicsVelocity() const;

    UFUNCTION(BlueprintPure, Category = "Ball|Prediction")
    bool IsNearGroundForPrediction() const;

    UFUNCTION(BlueprintPure, Category = "Ball|Prediction")
    int32 GetTrajectoryRevision() const;

    UFUNCTION(BlueprintPure, Category = "Ball|Prediction")
    bool IsAvailableForTrajectoryPrediction() const;

    /*
     * Predice cuándo la pelota cruzará un plano vertical.
     *
     * Sin PlaneNormalOverride, el plano es perpendicular a la
     * dirección horizontal actual de la pelota, como antes.
     *
     * Con PlaneNormalOverride se puede predecir el cruce de un
     * plano fijo, por ejemplo la línea del arco.
     */
    bool PredictCrossingOfHorizontalPlane(
        const FVector& PlaneLocation,
        float MaxPredictionTime,
        float& OutCrossingTime,
        FVector& OutCrossingLocation,
        const FVector& PlaneNormalOverride = FVector::ZeroVector
    ) const;

    void Kick(const FVector& Direction, float ForwardStrength, float UpwardStrength, bool bPlayKickSound = true);

    void KickToTarget(
        const FVector& TargetLocation,
        float HorizontalSpeed,
        float MinTravelTime,
        float MaxTravelTime
    );

    void KickGroundToTarget(
        const FVector& TargetLocation,
        float HorizontalSpeed
    );

    // Reuse the original 6AN ground-pass calibration with this ball's scale.
    float CalculateGroundPassInput6AN(float DistanceCm, bool* bWithinCalibration = nullptr) const;

    // bPlayKickSound=false for throw-ins and goalkeeper hand releases.
    void KickToAirTarget(
        const FVector& TargetLocation,
        float HorizontalSpeed,
        float MinTravelTime,
        float MaxTravelTime,
        bool bPlayKickSound = true
    );

    void ChargedKickToTarget(
        const FVector& TargetLocation,
        float HorizontalSpeed
    );

    void SetPossessed(bool bNewPossessed);

    // Training-only parity: override effective runtime values observed in a match.
    // Does not modify the BP_SoccerBall asset or any match instance.
    void ApplyTrainingBallParity6G(float InHorizontalLaunchScale, float InAirLinearDamping);

    /*
     * Applies a deterministic velocity after a head/chest contact.
     * The ball remains free and its trajectory revision changes immediately.
     */
    void ApplyAerialContactVelocity(
        const FVector& NewVelocity,
        float AngularVelocityRetention = 0.35f
    );

    /*
     * Continuous contact can be detected after the physics body has already
     * advanced beyond the animated head/chest during the current frame.
     * Rewinds the ball center to the calculated contact point before applying
     * the outgoing velocity, preventing a visible pass-through.
     */
    void ApplyAerialContactAtLocation(
        const FVector& ContactBallLocation,
        const FVector& NewVelocity,
        float AngularVelocityRetention = 0.35f
    );

    /*
     * Gameplay-owned sliding-tackle contact. Like aerial contact, the temporal
     * detector owns the swept contact point, but the ball may remain on the
     * grass, so damping is selected from the resulting trajectory instead of
     * being forced to the aerial values.
     */
    void ApplyTackleContactAtLocation(
        const FVector& ContactBallLocation,
        const FVector& NewVelocity,
        float AngularVelocityRetention = 0.55f
    );

    void StopBallKeepingPhysics();

    void DribbleTouch(
        const FVector& Direction,
        float TouchSpeed,
        float UpwardSpeed,
        bool bUsePreLaunchSeparation = false
    );

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Ball")
        UStaticMeshComponent* BallMesh;

    // Ball contact audio. Assign imported mono WAVs on the ball Blueprint.
    // Volume is calculated from the ball to the local human player's pawn;
    // a built-in spatialization setting provides directional placement.
    UPROPERTY(EditAnywhere, Category = "Ball|Audio")
        bool bEnableKickAudio = true;

    UPROPERTY(EditAnywhere, Category = "Ball|Audio")
        USoundBase* ShortKickSound = nullptr;

    UPROPERTY(EditAnywhere, Category = "Ball|Audio")
        USoundBase* StrongKickSound = nullptr;

    UPROPERTY(EditAnywhere, Category = "Ball|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
        float KickAudioVolume = 0.85f;

    UPROPERTY(EditAnywhere, Category = "Ball|Audio", meta = (ClampMin = "0.0"))
        float KickAudioNearDistanceCm = 300.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Audio", meta = (ClampMin = "1.0"))
        float KickAudioMaxDistanceCm = 9000.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Audio", meta = (ClampMin = "0.0"))
        float KickAudioStrongSpeedCmPerSec = 2200.0f;

    // Guard against duplicate launch calls in the same simulation instant.
    UPROPERTY(EditAnywhere, Category = "Ball|Audio", meta = (ClampMin = "0.0", ClampMax = "0.5"))
        float KickAudioCooldownSeconds = 0.07f;

    UPROPERTY(Transient)
        USoundAttenuation* KickAudioSpatialization = nullptr;

    float LastKickAudioWorldTime = -1000.0f;
    void PlayKickAudio(float LaunchSpeedCmPerSec);

    UPROPERTY(EditAnywhere, Category = "Ball|Debug")
        bool bDebugBallHitEvents = false;

    UPROPERTY(EditAnywhere, Category = "Ball|Kick")
        float GroundPassDistanceThreshold = 300.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Kick")
        float GroundPassSpeed = 1100.0f;

    UFUNCTION()
        void OnBallHit(
            UPrimitiveComponent* HitComponent,
            AActor* OtherActor,
            UPrimitiveComponent* OtherComp,
            FVector NormalImpulse,
            const FHitResult& Hit
        );

    // ============================================================
    // CONTINUOUS PASSIVE CHARACTER BODY CONTACT
    // ============================================================

    /*
     * The ball intentionally ignores Pawn/PhysicsBody collision so authored
     * kicks, controls, tackles and saves remain gameplay-owned. This sweep is
     * the passive-body fallback for a genuinely free ball: it checks the whole
     * travelled segment, so a powerful shot cannot skip a wall player between
     * two frames.
     */
    void UpdatePassiveCharacterBodyContact(float DeltaTime);

    void ResetPassiveCharacterBodyContactTracking(
        bool bSeedFromCurrentLocation
    );

    ASoccerMatchManager* ResolveMatchManager();

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact")
        bool bEnablePassiveCharacterBodyContact = true;

    /* Low-speed dribbling/control stays under the existing authored systems. */
    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.0"))
        float PassiveCharacterBodyMinimumBallSpeed = 900.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.0"))
        float PassiveCharacterBodyMinimumApproachSpeed = 80.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.50", ClampMax = "1.25"))
        float PassiveCharacterBodyBallRadiusScale = 0.92f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
        float PassiveCharacterBodyNormalVelocityRetention = 0.28f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
        float PassiveCharacterBodyTangentialVelocityRetention = 0.78f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
        float PassiveCharacterBodyPlayerVelocityTransfer = 0.30f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.0", ClampMax = "1.0"))
        float PassiveCharacterBodyAngularVelocityRetention = 0.60f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.0"))
        float PassiveCharacterBodyMinimumOutgoingNormalSpeed = 80.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "1.0"))
        float PassiveCharacterBodyMaximumOutgoingSpeedMultiplier = 1.05f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.0", ClampMax = "10.0"))
        float PassiveCharacterBodyContactSkin = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.0", ClampMax = "0.5"))
        float PassiveCharacterBodySamePlayerCooldown = 0.10f;

    /* Rejects discontinuous placement/reset movement without limiting shots. */
    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "1.0"))
        float PassiveCharacterBodySweepTravelMultiplier = 2.75f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact", meta = (ClampMin = "0.0"))
        float PassiveCharacterBodyTeleportSlack = 75.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Character Body Contact|Debug")
        bool bDebugPassiveCharacterBodyContact = false;

    UPROPERTY(Transient)
        ASoccerMatchManager* CachedMatchManager = nullptr;

    UPROPERTY(Transient)
        ASoccerCharacterBase* LastPassiveCharacterBodyContact = nullptr;

    FVector PreviousPassiveCharacterBodyBallLocation = FVector::ZeroVector;
    bool bHasPreviousPassiveCharacterBodyBallLocation = false;
    float LastPassiveCharacterBodyContactWorldTime = -1000.0f;

    // ============================================================
// GLOBAL BALL LAUNCH TUNING
// ============================================================

/*
 * Multiplicador central de las velocidades horizontales
 * generadas por patadas, pases, autopases y conducción.
 *
 * 1.00 = velocidades originales configuradas.
 * 0.50 = aproximadamente la mitad.
 *
 * No modifica la componente vertical configurada
 * directamente en Kick() o DribbleTouch().
 */
    UPROPERTY(
        EditAnywhere,
        Category = "Ball|Tuning",
        meta = (
            ClampMin = "0.10",
            ClampMax = "2.00",
            UIMin = "0.10",
            UIMax = "2.00"
            )
    )
        float HorizontalLaunchSpeedScale =
        0.50f;

    float GetScaledHorizontalLaunchSpeed(
        float UnscaledHorizontalSpeed,
        float MinimumSpeed = 0.0f
    ) const;

    /*
     * Calcula la velocidad inicial necesaria para alcanzar un destino en un
     * tiempo conocido bajo gravedad y damping lineal. Usa el mismo modelo
     * exponencial que BuildPredictedTrajectory(), de modo que un pase aéreo
     * conserve su punto/altura de llegada aunque AirLinearDamping sea mayor
     * que cero.
     */
    bool CalculateDampingAwareLaunchVelocity(
        const FVector& StartLocation,
        const FVector& TargetLocation,
        float TravelTime,
        float LinearDamping,
        FVector& OutLaunchVelocity
    ) const;

    UPROPERTY(EditAnywhere, Category = "Ball|Grass Feel")
        float GroundLinearDamping = 1.8f;

    UPROPERTY(EditAnywhere, Category = "Ball|Grass Feel")
        float GroundAngularDamping = 3.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Air Feel")
        float AirLinearDamping = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Air Feel")
        float AirAngularDamping = 0.05f;

    UPROPERTY(EditAnywhere, Category = "Ball|Ground Check")
        float GroundCheckDistance = 25.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Grass Feel")
        float StopRollingSpeedThreshold = 20.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Grass Feel")
        float StopSpinningSpeedThreshold = 35.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Pre Launch Separation")
        float PreLaunchSeparationAtSlowSpeed = 50.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Pre Launch Separation")
        float PreLaunchSeparationAtFastSpeed = 15.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Pre Launch Separation")
        float PreLaunchSeparationSlowSpeed = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Pre Launch Separation")
        float PreLaunchSeparationFastSpeed = 2500.0f;

    // Legacy positional correction. Disabled because it visibly moves the
    // ball 15-50 cm before velocity is applied.
    UPROPERTY(EditAnywhere, Category = "Ball|Pre Launch Separation")
        bool bEnablePreLaunchSeparation = false;

    float GetPreLaunchSeparationDistance(float LaunchSpeed) const;

    void ApplyPreLaunchSeparation(
        const FVector& Direction,
        float LaunchSpeed,
        float MaxSeparationDistance = -1.0f
    );

    UPROPERTY(EditAnywhere, Category = "Ball|Goal Net")
        bool bUseGoalNetSoftCollision = true;

    UPROPERTY(EditAnywhere, Category = "Ball|Goal Net")
        float GoalNetForwardVelocityRetention = 0.08f;

    UPROPERTY(EditAnywhere, Category = "Ball|Goal Net")
        float GoalNetSideVelocityRetention = 0.35f;

    UPROPERTY(EditAnywhere, Category = "Ball|Goal Net")
        float GoalNetUpVelocityRetention = 0.25f;

    UPROPERTY(EditAnywhere, Category = "Ball|Goal Net")
        float GoalNetMaxSpeedAfterHit = 420.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Goal Net")
        float GoalNetAngularVelocityRetention = 0.20f;

    UPROPERTY(EditAnywhere, Category = "Ball|Goal Net")
        float GoalNetMinimumHitSpeed = 80.0f;

    bool IsGoalNetComponent(
        const UPrimitiveComponent* OtherComp
    ) const;

    void ApplyGoalNetSoftCollision(
        const FHitResult& Hit
    );

    // ============================================================
    // BALL TRAJECTORY PREDICTION - ETAPA 3
    // ============================================================

    UPROPERTY(
        EditAnywhere,
        Category = "Ball|Prediction",
        meta = (ClampMin = "0.002", UIMin = "0.002", UIMax = "0.050")
    )
    float PredictionSimulationStep = 0.008333333f;

    UPROPERTY(
        EditAnywhere,
        Category = "Ball|Prediction",
        meta = (ClampMin = "100.0", UIMin = "100.0", UIMax = "10000.0")
    )
    float PredictionGroundProbeDistance = 5000.0f;

    /* Retencion vertical de un rebote contra suelo. */
    UPROPERTY(
        EditAnywhere,
        Category = "Ball|Prediction|Bounce",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float PredictionGroundBounceVelocityRetention = 0.42f;

    /* Retencion de la velocidad paralela al suelo al rebotar. */
    UPROPERTY(
        EditAnywhere,
        Category = "Ball|Prediction|Bounce",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float PredictionGroundTangentialVelocityRetention = 0.82f;

    UPROPERTY(
        EditAnywhere,
        Category = "Ball|Prediction|Bounce",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float PredictionObstacleBounceVelocityRetention = 0.58f;

    UPROPERTY(
        EditAnywhere,
        Category = "Ball|Prediction|Bounce",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float PredictionObstacleTangentialVelocityRetention = 0.78f;

    UPROPERTY(EditAnywhere, Category = "Ball|Prediction|Bounce", meta = (ClampMin = "0.0"))
    float PredictionMinimumVerticalBounceSpeed = 85.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Prediction|Bounce", meta = (ClampMin = "0", ClampMax = "12"))
    int32 PredictionMaximumBounceCount = 6;

    UPROPERTY(EditAnywhere, Category = "Ball|Prediction|Collision", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float PredictionCollisionSkin = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Ball|Prediction|Collision", meta = (ClampMin = "1", ClampMax = "6"))
    int32 PredictionMaximumCollisionIterationsPerStep = 3;

    /* Evita invalidar el objetivo por contactos suaves mientras rueda. */
    UPROPERTY(EditAnywhere, Category = "Ball|Prediction|Collision", meta = (ClampMin = "0.0"))
    float TrajectoryRevisionMinimumHitNormalSpeed = 70.0f;

    UPROPERTY(EditAnywhere, Category = "Ball|Prediction|Collision", meta = (ClampMin = "0.0"))
    float TrajectoryRevisionMinimumHitImpulse = 20.0f;

    /* Compatibilidad: indica cuando una trayectoria puede tratarse como rasante. */
    UPROPERTY(EditAnywhere, Category = "Ball|Prediction", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "500.0"))
    float GroundTrajectoryMaxVerticalSpeed = 120.0f;

    bool bHasCachedPredictionGround = false;
    bool bCachedBallIsNearGround = false;
    float CachedPredictionGroundZ = 0.0f;
    bool bPredictionPossessed = false;

    /* Cambia ante patadas, toques, posesion, teletransporte o impactos. */
    uint32 TrajectoryRevision = 1;

    void MarkTrajectoryChanged();

    mutable uint64 CachedTrajectoryFrame = TNumericLimits<uint64>::Max();
    mutable uint32 CachedTrajectoryRevision = 0;
    mutable float CachedTrajectoryMaxTime = -1.0f;
    mutable float CachedTrajectorySampleInterval = -1.0f;
    mutable bool bCachedTrajectoryValid = false;
    mutable TArray<FSoccerBallTrajectorySample> CachedTrajectorySamples;
};
