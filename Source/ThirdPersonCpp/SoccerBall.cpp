//SoccerBall.cpp

#include "SoccerBall.h"

#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodyInstance.h"


namespace
{
    void ClearAccumulatedPhysicsForcesUE427(
        UStaticMeshComponent* MeshComponent
    )
    {
        if (MeshComponent == nullptr)
        {
            return;
        }

        /*
         * UE 4.27 does not expose ClearAllPhysicsForces() on
         * UPrimitiveComponent/UStaticMeshComponent. Clear the pending force
         * and torque accumulators through the component's FBodyInstance.
         * Linear and angular velocities are still assigned explicitly by the
         * caller immediately afterwards.
         */
        FBodyInstance* BodyInstance =
            MeshComponent->GetBodyInstance();

        if (BodyInstance != nullptr)
        {
            BodyInstance->ClearForces(false);
            BodyInstance->ClearTorques(false);
        }
    }
}

ASoccerBall::ASoccerBall()
{
    PrimaryActorTick.bCanEverTick = true;

    BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
    RootComponent = BallMesh;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (SphereMesh.Succeeded())
    {
        BallMesh->SetStaticMesh(SphereMesh.Object);
    }

    // La esfera b�sica de Unreal mide 100 cm de di�metro.
    // 0.22 = aprox. 22 cm de di�metro, parecido a una pelota de f�tbol real.
    BallMesh->SetRelativeScale3D(FVector(0.22f));

    BallMesh->SetSimulatePhysics(true);
    BallMesh->SetEnableGravity(true);

    BallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BallMesh->SetCollisionObjectType(ECC_PhysicsBody);
    BallMesh->SetCollisionResponseToAllChannels(ECR_Block);

    // La pelota no rebota f�sicamente contra jugadores.
    // Los jugadores la tocar�n por l�gica de radio/intenci�n.
    BallMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

    BallMesh->SetNotifyRigidBodyCollision(true);

    // Ayuda a que no atraviese cosas cuando va r�pido.
    BallMesh->BodyInstance.bUseCCD = true;

    // Masa aproximada de una pelota real: 0.43 kg.
    BallMesh->SetMassOverrideInKg(NAME_None, 0.43f, true);

    // Ajustes para que no ruede eternamente.
    BallMesh->SetLinearDamping(GroundLinearDamping);
    BallMesh->SetAngularDamping(GroundAngularDamping);
}

void ASoccerBall::BeginPlay()
{
    Super::BeginPlay();

    BallMesh->OnComponentHit.AddDynamic(this, &ASoccerBall::OnBallHit);

    bPredictionPossessed = !BallMesh->IsSimulatingPhysics();
    MarkTrajectoryChanged();
}

float ASoccerBall::GetBallRadiusCm() const
{
    if (BallMesh == nullptr)
    {
        return 22.0f;
    }

    return FMath::Max(1.0f, BallMesh->Bounds.SphereRadius);
}

FVector ASoccerBall::GetBallPhysicsVelocity() const
{
    if (BallMesh == nullptr)
    {
        return FVector::ZeroVector;
    }

    return BallMesh->GetPhysicsLinearVelocity();
}

bool ASoccerBall::IsNearGroundForPrediction() const
{
    if (BallMesh == nullptr || !BallMesh->IsSimulatingPhysics())
    {
        return false;
    }

    const FVector CurrentVelocity =
        BallMesh->GetPhysicsLinearVelocity();

    return
        bHasCachedPredictionGround &&
        bCachedBallIsNearGround &&
        FMath::Abs(CurrentVelocity.Z) <=
        FMath::Max(0.0f, GroundTrajectoryMaxVerticalSpeed);
}

int32 ASoccerBall::GetTrajectoryRevision() const
{
    return static_cast<int32>(TrajectoryRevision);
}

bool ASoccerBall::IsAvailableForTrajectoryPrediction() const
{
    return
        BallMesh != nullptr &&
        BallMesh->IsSimulatingPhysics() &&
        !bPredictionPossessed;
}

void ASoccerBall::MarkTrajectoryChanged()
{
    ++TrajectoryRevision;

    if (TrajectoryRevision == 0)
    {
        TrajectoryRevision = 1;
    }

    CachedTrajectoryFrame = TNumericLimits<uint64>::Max();
    CachedTrajectoryRevision = 0;
    bCachedTrajectoryValid = false;
    CachedTrajectorySamples.Reset();
}

bool ASoccerBall::BuildPredictedGroundTrajectory(
    float MaxPredictionTime,
    float SampleInterval,
    TArray<FSoccerBallTrajectorySample>& OutSamples
) const
{
    OutSamples.Reset();

    if (!IsNearGroundForPrediction())
    {
        return false;
    }

    if (!BuildPredictedTrajectory(
        MaxPredictionTime,
        SampleInterval,
        OutSamples
    ))
    {
        return false;
    }

    if (OutSamples.Num() <= 0 || !OutSamples[0].bNearGround)
    {
        OutSamples.Reset();
        return false;
    }

    return true;
}

bool ASoccerBall::BuildPredictedTrajectory(
    float MaxPredictionTime,
    float SampleInterval,
    TArray<FSoccerBallTrajectorySample>& OutSamples
) const
{
    OutSamples.Reset();

    if (!IsAvailableForTrajectoryPrediction())
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    bool bPredictionHasGround = bHasCachedPredictionGround;
    float PredictionGroundZ = CachedPredictionGroundZ;

    if (!bPredictionHasGround)
    {
        FCollisionQueryParams GroundQueryParams;
        GroundQueryParams.AddIgnoredActor(this);

        FHitResult GroundHit;
        const FVector BallLocation = GetActorLocation();

        bPredictionHasGround = World->LineTraceSingleByChannel(
            GroundHit,
            BallLocation,
            BallLocation - FVector::UpVector *
                FMath::Max(GroundCheckDistance, PredictionGroundProbeDistance),
            ECC_Visibility,
            GroundQueryParams
        );

        if (bPredictionHasGround)
        {
            PredictionGroundZ = GroundHit.ImpactPoint.Z;
        }
    }

    const float SafeMaxPredictionTime =
        FMath::Clamp(MaxPredictionTime, 0.0f, 10.0f);

    if (SafeMaxPredictionTime <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const float SafeSampleInterval =
        FMath::Clamp(SampleInterval, 0.02f, 0.50f);

    const bool bCanReuseCachedTrajectory =
        CachedTrajectoryFrame == GFrameCounter &&
        CachedTrajectoryRevision == TrajectoryRevision &&
        FMath::IsNearlyEqual(
            CachedTrajectoryMaxTime,
            SafeMaxPredictionTime,
            KINDA_SMALL_NUMBER
        ) &&
        FMath::IsNearlyEqual(
            CachedTrajectorySampleInterval,
            SafeSampleInterval,
            KINDA_SMALL_NUMBER
        );

    if (bCanReuseCachedTrajectory)
    {
        OutSamples = CachedTrajectorySamples;
        return bCachedTrajectoryValid;
    }

    const float SafeSimulationStep =
        FMath::Clamp(
            PredictionSimulationStep,
            0.002f,
            SafeSampleInterval
        );

    const float BallRadius = GetBallRadiusCm();
    const float SweepRadius = FMath::Max(1.0f, BallRadius * 0.90f);
    const float GroundContactZ =
        PredictionGroundZ + BallRadius;

    FVector SimulatedLocation = GetActorLocation();
    FVector SimulatedVelocity = BallMesh->GetPhysicsLinearVelocity();
    float SimulatedAngularSpeed =
        BallMesh->GetPhysicsAngularVelocityInDegrees().Size();

    const float GravityZ = World->GetGravityZ();

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(SoccerBallTrajectoryPrediction),
        false,
        this
    );
    QueryParams.AddIgnoredActor(this);

    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    const FCollisionShape BallShape =
        FCollisionShape::MakeSphere(SweepRadius);

    auto IsNearGround =
        [this, bPredictionHasGround, PredictionGroundZ](
            const FVector& Location
        )
        {
            return
                bPredictionHasGround &&
                Location.Z - PredictionGroundZ <=
                GroundCheckDistance;
        };

    auto AddSample =
        [&OutSamples](
            float TimeFromNow,
            const FVector& Location,
            const FVector& Velocity,
            bool bBallStopped,
            bool bNearGround,
            bool bAfterBounce,
            bool bHitObstacle,
            bool bTrajectoryTerminated,
            int32 BounceCount,
            const FVector& LastImpactNormal
        )
        {
            FSoccerBallTrajectorySample Sample;
            Sample.TimeFromNow = TimeFromNow;
            Sample.Location = Location;
            Sample.Velocity = Velocity;
            Sample.bBallStopped = bBallStopped;
            Sample.bNearGround = bNearGround;
            Sample.bAfterBounce = bAfterBounce;
            Sample.bHitObstacle = bHitObstacle;
            Sample.bTrajectoryTerminated = bTrajectoryTerminated;
            Sample.BounceCount = BounceCount;
            Sample.LastImpactNormal = LastImpactNormal;
            OutSamples.Add(Sample);
        };

    bool bBallStopped =
        IsNearGround(SimulatedLocation) &&
        SimulatedVelocity.Size2D() <= StopRollingSpeedThreshold &&
        FMath::Abs(SimulatedVelocity.Z) <=
        PredictionMinimumVerticalBounceSpeed &&
        SimulatedAngularSpeed <= StopSpinningSpeedThreshold;

    if (bBallStopped)
    {
        SimulatedVelocity = FVector::ZeroVector;
        SimulatedAngularSpeed = 0.0f;

        if (bPredictionHasGround)
        {
            SimulatedLocation.Z =
                FMath::Max(SimulatedLocation.Z, GroundContactZ);
        }
    }

    int32 BounceCount = 0;
    bool bTrajectoryTerminated = false;
    FVector LastImpactNormal = FVector::ZeroVector;

    AddSample(
        0.0f,
        SimulatedLocation,
        SimulatedVelocity,
        bBallStopped,
        IsNearGround(SimulatedLocation),
        false,
        false,
        false,
        BounceCount,
        LastImpactNormal
    );

    float SimulatedTime = 0.0f;
    float NextSampleTime =
        FMath::Min(SafeSampleInterval, SafeMaxPredictionTime);

    bool bBounceSinceLastSample = false;
    bool bObstacleHitSinceLastSample = false;

    while (
        SimulatedTime < SafeMaxPredictionTime &&
        !bTrajectoryTerminated
    )
    {
        const float TargetSampleTime =
            FMath::Min(NextSampleTime, SafeMaxPredictionTime);

        while (
            SimulatedTime < TargetSampleTime - KINDA_SMALL_NUMBER &&
            !bTrajectoryTerminated
        )
        {
            const float StepTime =
                FMath::Min(
                    SafeSimulationStep,
                    TargetSampleTime - SimulatedTime
                );

            if (!bBallStopped)
            {
                const bool bNearGroundBeforeStep =
                    IsNearGround(SimulatedLocation);

                /*
                 * Estar geométricamente cerca del césped no convierte una
                 * pelota que está subiendo/bajando rápido en una pelota
                 * terrestre. Esto mantiene el lanzamiento aéreo bajo
                 * AirLinearDamping desde su primer paso y replica Tick().
                 */
                const bool bUseGroundDamping =
                    bNearGroundBeforeStep &&
                    FMath::Abs(SimulatedVelocity.Z) <=
                    FMath::Max(
                        0.0f,
                        GroundTrajectoryMaxVerticalSpeed
                    );

                const float LinearDamping =
                    bUseGroundDamping
                    ? GroundLinearDamping
                    : AirLinearDamping;

                const float AngularDamping =
                    bUseGroundDamping
                    ? GroundAngularDamping
                    : AirAngularDamping;

                SimulatedVelocity.Z += GravityZ * StepTime;

                SimulatedVelocity *= FMath::Exp(
                    -FMath::Max(0.0f, LinearDamping) * StepTime
                );

                SimulatedAngularSpeed *= FMath::Exp(
                    -FMath::Max(0.0f, AngularDamping) * StepTime
                );

                float RemainingStepTime = StepTime;
                int32 CollisionIteration = 0;

                while (
                    RemainingStepTime > KINDA_SMALL_NUMBER &&
                    CollisionIteration <
                    FMath::Max(1, PredictionMaximumCollisionIterationsPerStep)
                )
                {
                    ++CollisionIteration;

                    const FVector IntendedLocation =
                        SimulatedLocation +
                        SimulatedVelocity * RemainingStepTime;

                    FHitResult Hit;
                    const bool bHit = World->SweepSingleByObjectType(
                        Hit,
                        SimulatedLocation,
                        IntendedLocation,
                        FQuat::Identity,
                        ObjectQueryParams,
                        BallShape,
                        QueryParams
                    );

                    if (!bHit || !Hit.bBlockingHit)
                    {
                        SimulatedLocation = IntendedLocation;
                        RemainingStepTime = 0.0f;
                        break;
                    }

                    FVector ImpactNormal = Hit.ImpactNormal.GetSafeNormal();

                    if (ImpactNormal.IsNearlyZero())
                    {
                        ImpactNormal = -SimulatedVelocity.GetSafeNormal();
                    }

                    LastImpactNormal = ImpactNormal;
                    SimulatedLocation =
                        Hit.Location +
                        ImpactNormal * FMath::Max(0.0f, PredictionCollisionSkin);

                    const bool bGroundImpact =
                        ImpactNormal.Z >= 0.55f;

                    const UPrimitiveComponent* HitComponent =
                        Hit.Component.Get();

                    if (IsGoalNetComponent(HitComponent))
                    {
                        const float VelocityAlongNormal =
                            FVector::DotProduct(
                                SimulatedVelocity,
                                ImpactNormal
                            );

                        const FVector NormalVelocity =
                            ImpactNormal * VelocityAlongNormal;

                        const FVector TangentialVelocity =
                            SimulatedVelocity - NormalVelocity;

                        SimulatedVelocity =
                            TangentialVelocity * GoalNetSideVelocityRetention +
                            NormalVelocity * GoalNetForwardVelocityRetention;

                        SimulatedVelocity.Z *=
                            GoalNetUpVelocityRetention;

                        SimulatedVelocity =
                            SimulatedVelocity.GetClampedToMaxSize(
                                FMath::Max(0.0f, GoalNetMaxSpeedAfterHit)
                            );

                        SimulatedAngularSpeed *=
                            FMath::Clamp(
                                GoalNetAngularVelocityRetention,
                                0.0f,
                                1.0f
                            );

                        bObstacleHitSinceLastSample = true;
                        bTrajectoryTerminated = true;
                        RemainingStepTime = 0.0f;
                    }
                    else
                    {
                        const float VelocityAlongNormal =
                            FVector::DotProduct(
                                SimulatedVelocity,
                                ImpactNormal
                            );

                        if (VelocityAlongNormal < 0.0f)
                        {
                            const FVector TangentialVelocity =
                                SimulatedVelocity -
                                ImpactNormal * VelocityAlongNormal;

                            const float NormalSpeedIntoSurface =
                                -VelocityAlongNormal;

                            bool bProducedBounce = false;

                            if (bGroundImpact)
                            {
                                FVector NewNormalVelocity =
                                    ImpactNormal *
                                    NormalSpeedIntoSurface *
                                    FMath::Clamp(
                                        PredictionGroundBounceVelocityRetention,
                                        0.0f,
                                        1.0f
                                    );

                                if (
                                    NormalSpeedIntoSurface <
                                    PredictionMinimumVerticalBounceSpeed
                                )
                                {
                                    NewNormalVelocity = FVector::ZeroVector;
                                }
                                else
                                {
                                    bProducedBounce = true;
                                }

                                SimulatedVelocity =
                                    TangentialVelocity *
                                    FMath::Clamp(
                                        PredictionGroundTangentialVelocityRetention,
                                        0.0f,
                                        1.0f
                                    ) +
                                    NewNormalVelocity;
                            }
                            else
                            {
                                SimulatedVelocity =
                                    TangentialVelocity *
                                    FMath::Clamp(
                                        PredictionObstacleTangentialVelocityRetention,
                                        0.0f,
                                        1.0f
                                    ) +
                                    ImpactNormal *
                                    NormalSpeedIntoSurface *
                                    FMath::Clamp(
                                        PredictionObstacleBounceVelocityRetention,
                                        0.0f,
                                        1.0f
                                    );

                                bObstacleHitSinceLastSample = true;
                                bProducedBounce = true;
                            }

                            if (bProducedBounce)
                            {
                                ++BounceCount;
                                bBounceSinceLastSample = true;
                            }
                        }
                    }

                    const float RemainingFraction =
                        FMath::Clamp(1.0f - Hit.Time, 0.0f, 1.0f);

                    RemainingStepTime *= RemainingFraction;

                    if (Hit.Time <= KINDA_SMALL_NUMBER)
                    {
                        RemainingStepTime *= 0.5f;
                    }

                    if (
                        BounceCount >
                        FMath::Max(0, PredictionMaximumBounceCount)
                    )
                    {
                        SimulatedVelocity = FVector::ZeroVector;
                        SimulatedAngularSpeed = 0.0f;
                        bBallStopped = true;
                        bTrajectoryTerminated = true;
                        break;
                    }
                }

                /* Respaldo para campos planos si el sweep no detecta el cesped. */
                if (
                    bPredictionHasGround &&
                    SimulatedLocation.Z < GroundContactZ
                )
                {
                    SimulatedLocation.Z = GroundContactZ;

                    if (SimulatedVelocity.Z < 0.0f)
                    {
                        const float DownwardSpeed = -SimulatedVelocity.Z;

                        SimulatedVelocity.X *=
                            FMath::Clamp(
                                PredictionGroundTangentialVelocityRetention,
                                0.0f,
                                1.0f
                            );
                        SimulatedVelocity.Y *=
                            FMath::Clamp(
                                PredictionGroundTangentialVelocityRetention,
                                0.0f,
                                1.0f
                            );

                        if (
                            DownwardSpeed >=
                            PredictionMinimumVerticalBounceSpeed
                        )
                        {
                            SimulatedVelocity.Z =
                                DownwardSpeed *
                                FMath::Clamp(
                                    PredictionGroundBounceVelocityRetention,
                                    0.0f,
                                    1.0f
                                );

                            ++BounceCount;
                            bBounceSinceLastSample = true;
                            LastImpactNormal = FVector::UpVector;
                        }
                        else
                        {
                            SimulatedVelocity.Z = 0.0f;
                        }
                    }
                }

                const bool bNearGroundAfterStep =
                    IsNearGround(SimulatedLocation);

                if (
                    bNearGroundAfterStep &&
                    SimulatedVelocity.Size2D() <= StopRollingSpeedThreshold &&
                    FMath::Abs(SimulatedVelocity.Z) <=
                    PredictionMinimumVerticalBounceSpeed &&
                    SimulatedAngularSpeed <= StopSpinningSpeedThreshold
                )
                {
                    SimulatedVelocity = FVector::ZeroVector;
                    SimulatedAngularSpeed = 0.0f;
                    bBallStopped = true;

                    if (bPredictionHasGround)
                    {
                        SimulatedLocation.Z =
                            FMath::Max(
                                SimulatedLocation.Z,
                                GroundContactZ
                            );
                    }
                }
            }

            SimulatedTime += StepTime;
        }

        SimulatedTime = TargetSampleTime;

        AddSample(
            TargetSampleTime,
            SimulatedLocation,
            SimulatedVelocity,
            bBallStopped,
            IsNearGround(SimulatedLocation),
            bBounceSinceLastSample,
            bObstacleHitSinceLastSample,
            bTrajectoryTerminated,
            BounceCount,
            LastImpactNormal
        );

        bBounceSinceLastSample = false;
        bObstacleHitSinceLastSample = false;

        if (
            TargetSampleTime >=
            SafeMaxPredictionTime - KINDA_SMALL_NUMBER
        )
        {
            break;
        }

        NextSampleTime += SafeSampleInterval;
    }

    CachedTrajectoryFrame = GFrameCounter;
    CachedTrajectoryRevision = TrajectoryRevision;
    CachedTrajectoryMaxTime = SafeMaxPredictionTime;
    CachedTrajectorySampleInterval = SafeSampleInterval;
    CachedTrajectorySamples = OutSamples;
    bCachedTrajectoryValid = OutSamples.Num() > 0;

    return bCachedTrajectoryValid;
}

void ASoccerBall::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (
        BallMesh == nullptr ||
        !BallMesh->IsSimulatingPhysics()
        )
    {
        return;
    }

    UWorld* World =
        GetWorld();

    if (World == nullptr)
    {
        return;
    }

    const FVector BallLocation =
        GetActorLocation();

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    FHitResult NearGroundHit;

    const bool bIsNearGround =
        World->LineTraceSingleByChannel(
            NearGroundHit,
            BallLocation,
            BallLocation -
            FVector::UpVector *
            GroundCheckDistance,
            ECC_Visibility,
            QueryParams
        );

    bCachedBallIsNearGround =
        bIsNearGround;

    /*
     * Guardamos el nivel aproximado del suelo para que
     * el predictor no necesite hacer una traza por cada
     * mano y muestra de animaci�n.
     */
    bHasCachedPredictionGround =
        false;

    if (bIsNearGround)
    {
        bHasCachedPredictionGround =
            true;

        CachedPredictionGroundZ =
            NearGroundHit.ImpactPoint.Z;
    }
    else
    {
        FHitResult PredictionGroundHit;

        const float SafeGroundProbeDistance =
            FMath::Max(
                GroundCheckDistance,
                PredictionGroundProbeDistance
            );

        bHasCachedPredictionGround =
            World->LineTraceSingleByChannel(
                PredictionGroundHit,
                BallLocation,
                BallLocation -
                FVector::UpVector *
                SafeGroundProbeDistance,
                ECC_Visibility,
                QueryParams
            );

        if (bHasCachedPredictionGround)
        {
            CachedPredictionGroundZ =
                PredictionGroundHit.
                ImpactPoint.Z;
        }
    }

    const FVector LinearVelocity =
        BallMesh->
        GetPhysicsLinearVelocity();

    const FVector AngularVelocity =
        BallMesh->
        GetPhysicsAngularVelocityInDegrees();

    /*
     * Un pase aéreo comienza cerca del césped, pero con una velocidad vertical
     * elevada. En ese caso debe utilizar inmediatamente el damping del aire;
     * de lo contrario el primer Tick puede sustituir AirLinearDamping por el
     * GroundLinearDamping mucho más fuerte y romper la trayectoria calculada.
     */
    const bool bUseGroundDamping =
        bIsNearGround &&
        FMath::Abs(LinearVelocity.Z) <=
        FMath::Max(
            0.0f,
            GroundTrajectoryMaxVerticalSpeed
        );

    if (bUseGroundDamping)
    {
        BallMesh->SetLinearDamping(
            GroundLinearDamping
        );

        BallMesh->SetAngularDamping(
            GroundAngularDamping
        );
    }
    else
    {
        BallMesh->SetLinearDamping(
            AirLinearDamping
        );

        BallMesh->SetAngularDamping(
            AirAngularDamping
        );
    }

    const float Speed2D =
        LinearVelocity.Size2D();

    const float SpinSpeed =
        AngularVelocity.Size();

    if (
        Speed2D > 0.0f &&
        Speed2D <=
        StopRollingSpeedThreshold &&
        SpinSpeed <=
        StopSpinningSpeedThreshold
        )
    {
        BallMesh->SetPhysicsLinearVelocity(
            FVector::ZeroVector
        );

        BallMesh->
            SetPhysicsAngularVelocityInDegrees(
                FVector::ZeroVector
            );

        BallMesh->WakeAllRigidBodies();
    }
}

bool ASoccerBall::
PredictCrossingOfHorizontalPlane(
    const FVector& PlaneLocation,
    float MaxPredictionTime,
    float& OutCrossingTime,
    FVector& OutCrossingLocation,
    const FVector& PlaneNormalOverride
) const
{
    OutCrossingTime =
        0.0f;

    OutCrossingLocation =
        FVector::ZeroVector;

    if (
        BallMesh == nullptr ||
        !BallMesh->IsSimulatingPhysics()
        )
    {
        return false;
    }

    const UWorld* World =
        GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    const float SafeMaxPredictionTime =
        FMath::Max(
            0.0f,
            MaxPredictionTime
        );

    if (
        SafeMaxPredictionTime <=
        KINDA_SMALL_NUMBER
        )
    {
        return false;
    }

    const float SafeSimulationStep =
        FMath::Clamp(
            PredictionSimulationStep,
            0.002f,
            0.050f
        );

    FVector SimulatedLocation =
        GetActorLocation();

    FVector SimulatedVelocity =
        BallMesh->
        GetPhysicsLinearVelocity();

    FVector InitialHorizontalVelocity =
        SimulatedVelocity;

    InitialHorizontalVelocity.Z =
        0.0f;

    const float InitialHorizontalSpeed =
        InitialHorizontalVelocity.Size();

    if (
        InitialHorizontalSpeed <=
        KINDA_SMALL_NUMBER
        )
    {
        return false;
    }

    FVector PlaneNormal =
        PlaneNormalOverride;

    PlaneNormal.Z = 0.0f;
    PlaneNormal = PlaneNormal.GetSafeNormal();

    /*
     * Sin normal expl�cita conservamos el comportamiento
     * anterior: el plano es perpendicular a la direcci�n
     * horizontal inicial de la pelota.
     */
    if (PlaneNormal.IsNearlyZero())
    {
        PlaneNormal =
            InitialHorizontalVelocity /
            InitialHorizontalSpeed;
    }

    /*
     * La pelota debe estar avanzando hacia el lado positivo
     * del plano. Esto evita aceptar un cruce ubicado detr�s
     * de su trayectoria actual.
     */
    if (
        FVector::DotProduct(
            InitialHorizontalVelocity,
            PlaneNormal
        ) <= KINDA_SMALL_NUMBER
        )
    {
        return false;
    }

    float PreviousSignedDistance =
        FVector::DotProduct(
            SimulatedLocation -
            PlaneLocation,
            PlaneNormal
        );

    /*
     * Valor positivo:
     * la pelota ya cruz� el plano.
     */
    if (PreviousSignedDistance > 0.0f)
    {
        return false;
    }

    if (
        FMath::IsNearlyZero(
            PreviousSignedDistance,
            0.5f
        )
        )
    {
        OutCrossingTime =
            0.0f;

        OutCrossingLocation =
            SimulatedLocation;

        return true;
    }

    float SimulatedAngularSpeed =
        BallMesh->
        GetPhysicsAngularVelocityInDegrees().
        Size();

    const float GravityZ =
        World->GetGravityZ();

    const float BallRadius =
        FMath::Max(
            1.0f,
            BallMesh->Bounds.SphereRadius
        );

    const float GroundContactZ =
        CachedPredictionGroundZ +
        BallRadius;

    float SimulatedTime =
        0.0f;

    while (
        SimulatedTime <
        SafeMaxPredictionTime
        )
    {
        const float StepTime =
            FMath::Min(
                SafeSimulationStep,
                SafeMaxPredictionTime -
                SimulatedTime
            );

        const float PreviousTime =
            SimulatedTime;

        const FVector PreviousLocation =
            SimulatedLocation;

        PreviousSignedDistance =
            FVector::DotProduct(
                PreviousLocation -
                PlaneLocation,
                PlaneNormal
            );

        /*
         * Imitamos la misma condici�n utilizada por el
         * Tick real: cuando el suelo est� dentro de
         * GroundCheckDistance, se usa el damping de c�sped.
         */
        const bool bPredictedNearGround =
            bHasCachedPredictionGround &&
            (
                SimulatedLocation.Z -
                CachedPredictionGroundZ
                ) <=
            GroundCheckDistance;

        const bool bUseGroundDamping =
            bPredictedNearGround &&
            FMath::Abs(SimulatedVelocity.Z) <=
            FMath::Max(
                0.0f,
                GroundTrajectoryMaxVerticalSpeed
            );

        const float LinearDamping =
            bUseGroundDamping
            ? GroundLinearDamping
            : AirLinearDamping;

        const float AngularDamping =
            bUseGroundDamping
            ? GroundAngularDamping
            : AirAngularDamping;

        /*
         * Aplicamos gravedad y una aproximaci�n estable
         * del amortiguamiento lineal.
         *
         * Esta f�rmula evita que una velocidad cambie de
         * signo cuando el damping o el paso son grandes.
         */
        SimulatedVelocity.Z +=
            GravityZ *
            StepTime;

        const float LinearDampingFactor =
            FMath::Exp(
                -FMath::Max(
                    0.0f,
                    LinearDamping
                ) *
                StepTime
            );

        const float AngularDampingFactor =
            FMath::Exp(
                -FMath::Max(
                    0.0f,
                    AngularDamping
                ) *
                StepTime
            );

        SimulatedVelocity *=
            LinearDampingFactor;

        SimulatedAngularSpeed *=
            AngularDampingFactor;

        SimulatedLocation +=
            SimulatedVelocity *
            StepTime;

        /*
         * Aproximaci�n sencilla de contacto con el suelo.
         *
         * Todav�a no reproduce fricci�n y restituci�n del
         * Physical Material; evita que la predicci�n
         * atraviese el c�sped.
         */
        if (
            bHasCachedPredictionGround &&
            SimulatedLocation.Z <
            GroundContactZ
            )
        {
            SimulatedLocation.Z =
                GroundContactZ;

            if (SimulatedVelocity.Z < 0.0f)
            {
                SimulatedVelocity.Z =
                    -SimulatedVelocity.Z *
                    FMath::Clamp(
                        PredictionGroundBounceVelocityRetention,
                        0.0f,
                        1.0f
                    );
            }
        }

        const bool bNowNearGround =
            bHasCachedPredictionGround &&
            (
                SimulatedLocation.Z -
                CachedPredictionGroundZ
                ) <=
            GroundCheckDistance;

        /*
         * Reproducimos tambi�n la detenci�n forzada que
         * hace el Tick real de la pelota.
         */
        const float SimulatedSpeed2D =
            SimulatedVelocity.Size2D();

        if (
            bNowNearGround &&
            SimulatedSpeed2D > 0.0f &&
            SimulatedSpeed2D <=
            StopRollingSpeedThreshold &&
            SimulatedAngularSpeed <=
            StopSpinningSpeedThreshold
            )
        {
            SimulatedVelocity.X =
                0.0f;

            SimulatedVelocity.Y =
                0.0f;

            SimulatedAngularSpeed =
                0.0f;
        }

        SimulatedTime +=
            StepTime;

        const float CurrentSignedDistance =
            FVector::DotProduct(
                SimulatedLocation -
                PlaneLocation,
                PlaneNormal
            );

        /*
         * La posici�n anterior estaba antes del plano y
         * la nueva est� en �l o despu�s de �l.
         */
        if (
            PreviousSignedDistance <= 0.0f &&
            CurrentSignedDistance >= 0.0f
            )
        {
            const float SignedDistanceChange =
                CurrentSignedDistance -
                PreviousSignedDistance;

            const float CrossingAlpha =
                SignedDistanceChange >
                KINDA_SMALL_NUMBER
                ? FMath::Clamp(
                    -PreviousSignedDistance /
                    SignedDistanceChange,
                    0.0f,
                    1.0f
                )
                : 1.0f;

            OutCrossingTime =
                PreviousTime +
                StepTime *
                CrossingAlpha;

            OutCrossingLocation =
                FMath::Lerp(
                    PreviousLocation,
                    SimulatedLocation,
                    CrossingAlpha
                );

            return true;
        }

        /*
         * La pelota dej� de avanzar horizontalmente antes
         * de alcanzar el plano.
         */
        if (
            SimulatedVelocity.Size2D() <=
            KINDA_SMALL_NUMBER
            )
        {
            return false;
        }
    }

    return false;
}

void ASoccerBall::Kick(
    const FVector& Direction,
    float ForwardStrength,
    float UpwardStrength
)
{
    if (BallMesh == nullptr)
    {
        return;
    }

    const FVector KickDirection =
        Direction.GetSafeNormal();

    if (KickDirection.IsNearlyZero())
    {
        return;
    }

    FVector Impulse =
        KickDirection *
        ForwardStrength
        +
        FVector::UpVector *
        UpwardStrength;

    /*
     * Reducimos exclusivamente X e Y.
     *
     * La componente vertical conserva el valor que
     * ten�a antes.
     */
    const float SafeHorizontalScale =
        FMath::Max(
            0.0f,
            HorizontalLaunchSpeedScale
        );

    Impulse.X *=
        SafeHorizontalScale;

    Impulse.Y *=
        SafeHorizontalScale;

    BallMesh->AddImpulse(
        Impulse,
        NAME_None,
        true
    );

    MarkTrajectoryChanged();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            1.0f,
            FColor::Cyan,
            FString::Printf(
                TEXT(
                    "Pelota pateada | escala horizontal %.2f"
                ),
                SafeHorizontalScale
            )
        );
    }
}

void ASoccerBall::OnBallHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit
)
{
    if (bDebugBallHitEvents)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Ball hit: %s"),
            *GetNameSafe(OtherActor)
        );
    }

    const FVector CurrentVelocity =
        BallMesh != nullptr
        ? BallMesh->GetPhysicsLinearVelocity()
        : FVector::ZeroVector;

    const FVector SafeImpactNormal =
        Hit.ImpactNormal.GetSafeNormal();

    const float HitNormalSpeed = FMath::Abs(
        FVector::DotProduct(
            CurrentVelocity,
            SafeImpactNormal
        )
    );

    const bool bMeaningfulTrajectoryChange =
        IsGoalNetComponent(OtherComp) ||
        NormalImpulse.Size() >=
            TrajectoryRevisionMinimumHitImpulse ||
        HitNormalSpeed >=
            TrajectoryRevisionMinimumHitNormalSpeed;

    if (bMeaningfulTrajectoryChange)
    {
        MarkTrajectoryChanged();
    }

    if (IsGoalNetComponent(OtherComp))
    {
        ApplyGoalNetSoftCollision(Hit);
    }
}

bool ASoccerBall::IsGoalNetComponent(
    const UPrimitiveComponent* OtherComp
) const
{
    if (OtherComp == nullptr)
    {
        return false;
    }

    return OtherComp->ComponentTags.Contains(
        FName(TEXT("GoalNet"))
    );
}

void ASoccerBall::ApplyGoalNetSoftCollision(
    const FHitResult& Hit
)
{
    if (
        !bUseGoalNetSoftCollision ||
        BallMesh == nullptr ||
        !BallMesh->IsSimulatingPhysics()
        )
    {
        return;
    }

    const FVector CurrentVelocity =
        BallMesh->GetPhysicsLinearVelocity();

    const float CurrentSpeed =
        CurrentVelocity.Size();

    if (CurrentSpeed < GoalNetMinimumHitSpeed)
    {
        return;
    }

    // Normal de impacto. La usamos para separar velocidad "contra la red"
    // de velocidad tangencial. Eso evita que rebote como pared.
    FVector NetNormal =
        Hit.ImpactNormal.GetSafeNormal();

    if (NetNormal.IsNearlyZero())
    {
        NetNormal = -CurrentVelocity.GetSafeNormal();
    }

    const float VelocityAlongNormal =
        FVector::DotProduct(
            CurrentVelocity,
            NetNormal
        );

    const FVector NormalVelocity =
        NetNormal * VelocityAlongNormal;

    const FVector TangentialVelocity =
        CurrentVelocity - NormalVelocity;

    FVector NewVelocity =
        TangentialVelocity * GoalNetSideVelocityRetention +
        NormalVelocity * GoalNetForwardVelocityRetention;

    // Bajamos tambi�n el componente vertical para que no rebote hacia arriba
    // como si hubiera pegado en una pared r�gida.
    NewVelocity.Z =
        CurrentVelocity.Z * GoalNetUpVelocityRetention;

    if (NewVelocity.Size() > GoalNetMaxSpeedAfterHit)
    {
        NewVelocity =
            NewVelocity.GetSafeNormal() * GoalNetMaxSpeedAfterHit;
    }

    BallMesh->SetPhysicsLinearVelocity(
        NewVelocity,
        false
    );

    BallMesh->SetPhysicsAngularVelocityInDegrees(
        BallMesh->GetPhysicsAngularVelocityInDegrees() *
        GoalNetAngularVelocityRetention,
        false
    );

    BallMesh->WakeAllRigidBodies();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            0.6f,
            FColor::Cyan,
            TEXT("Pelota amortiguada por red")
        );
    }
}

void ASoccerBall::ApplyAerialContactVelocity(
    const FVector& NewVelocity,
    float AngularVelocityRetention
)
{
    if (BallMesh == nullptr)
    {
        return;
    }

    bPredictionPossessed = false;

    BallMesh->SetSimulatePhysics(true);

    /*
     * A launch may happen immediately after teleporting the ball from the
     * grass to an aerial start point. Tick() has not run yet, so Chaos can
     * still retain GroundLinearDamping/GroundAngularDamping from the old
     * position while BuildPredictedTrajectory() correctly simulates with the
     * aerial damping selected from the new position. That made the first F7
     * attempt diverge and the second one work only because the previous run
     * had already switched the body to air damping.
     *
     * Aerial contact/launch velocity is always applied above the ground.
     * Synchronize the real rigid body before assigning the velocity so the
     * first physics step uses exactly the same damping as the predictor.
     */
    ClearAccumulatedPhysicsForcesUE427(BallMesh);
    BallMesh->SetLinearDamping(AirLinearDamping);
    BallMesh->SetAngularDamping(AirAngularDamping);
    BallMesh->WakeAllRigidBodies();

    BallMesh->SetPhysicsLinearVelocity(
        NewVelocity,
        false
    );

    const float SafeAngularRetention =
        FMath::Clamp(AngularVelocityRetention, 0.0f, 1.0f);

    BallMesh->SetPhysicsAngularVelocityInDegrees(
        BallMesh->GetPhysicsAngularVelocityInDegrees() *
            SafeAngularRetention,
        false
    );

    MarkTrajectoryChanged();
}

void ASoccerBall::ApplyAerialContactAtLocation(
    const FVector& ContactBallLocation,
    const FVector& NewVelocity,
    float AngularVelocityRetention
)
{
    if (BallMesh == nullptr)
    {
        return;
    }

    bPredictionPossessed = false;

    BallMesh->SetSimulatePhysics(true);
    ClearAccumulatedPhysicsForcesUE427(BallMesh);
    BallMesh->SetLinearDamping(AirLinearDamping);
    BallMesh->SetAngularDamping(AirAngularDamping);
    BallMesh->WakeAllRigidBodies();

    /*
     * The character and mesh ignore the PhysicsBody channel by design.
     * Therefore the gameplay detector, not Chaos, owns the contact point.
     * Teleporting to the swept point prevents the ball from appearing on the
     * far side of the torso before its outgoing velocity is applied.
     */
    SetActorLocation(
        ContactBallLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    BallMesh->SetPhysicsLinearVelocity(
        NewVelocity,
        false
    );

    const float SafeAngularRetention =
        FMath::Clamp(AngularVelocityRetention, 0.0f, 1.0f);

    BallMesh->SetPhysicsAngularVelocityInDegrees(
        BallMesh->GetPhysicsAngularVelocityInDegrees() *
            SafeAngularRetention,
        false
    );

    BallMesh->WakeAllRigidBodies();
    MarkTrajectoryChanged();
}

void ASoccerBall::ApplyTackleContactAtLocation(
    const FVector& ContactBallLocation,
    const FVector& NewVelocity,
    float AngularVelocityRetention
)
{
    if (BallMesh == nullptr)
    {
        return;
    }

    bPredictionPossessed = false;
    BallMesh->SetSimulatePhysics(true);
    ClearAccumulatedPhysicsForcesUE427(BallMesh);

    SetActorLocation(
        ContactBallLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    UWorld* World = GetWorld();
    bool bNearGround = false;

    if (World != nullptr)
    {
        FCollisionQueryParams QueryParams(
            SCENE_QUERY_STAT(TackleBallGroundProbe),
            false,
            this
        );
        QueryParams.AddIgnoredActor(this);

        FHitResult GroundHit;
        bNearGround = World->LineTraceSingleByChannel(
            GroundHit,
            ContactBallLocation,
            ContactBallLocation -
                FVector::UpVector * FMath::Max(5.0f, GroundCheckDistance),
            ECC_Visibility,
            QueryParams
        );
    }

    const bool bUseGroundDamping =
        bNearGround &&
        FMath::Abs(NewVelocity.Z) <=
            FMath::Max(0.0f, GroundTrajectoryMaxVerticalSpeed);

    BallMesh->SetLinearDamping(
        bUseGroundDamping ? GroundLinearDamping : AirLinearDamping
    );
    BallMesh->SetAngularDamping(
        bUseGroundDamping ? GroundAngularDamping : AirAngularDamping
    );

    BallMesh->WakeAllRigidBodies();
    BallMesh->SetPhysicsLinearVelocity(NewVelocity, false);

    const float SafeAngularRetention =
        FMath::Clamp(AngularVelocityRetention, 0.0f, 1.0f);

    BallMesh->SetPhysicsAngularVelocityInDegrees(
        BallMesh->GetPhysicsAngularVelocityInDegrees() * SafeAngularRetention,
        false
    );

    BallMesh->WakeAllRigidBodies();
    MarkTrajectoryChanged();
}

void ASoccerBall::SetPossessed(bool bNewPossessed)
{
    if (BallMesh == nullptr)
    {
        return;
    }

    bPredictionPossessed = bNewPossessed;

    if (bNewPossessed)
    {
        BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        BallMesh->SetSimulatePhysics(false);
    }
    else
    {
        BallMesh->SetSimulatePhysics(true);
        BallMesh->WakeAllRigidBodies();
    }

    MarkTrajectoryChanged();
}

float ASoccerBall::GetPreLaunchSeparationDistance(float LaunchSpeed) const
{
    const float SafeSlowSpeed = FMath::Max(1.0f, PreLaunchSeparationSlowSpeed);
    const float SafeFastSpeed = FMath::Max(SafeSlowSpeed + 1.0f, PreLaunchSeparationFastSpeed);

    const float SpeedAlpha = FMath::Clamp(
        (LaunchSpeed - SafeSlowSpeed) / (SafeFastSpeed - SafeSlowSpeed),
        0.0f,
        1.0f
    );

    return FMath::Lerp(
        PreLaunchSeparationAtSlowSpeed,
        PreLaunchSeparationAtFastSpeed,
        SpeedAlpha
    );
}

float ASoccerBall::
GetScaledHorizontalLaunchSpeed(
    float UnscaledHorizontalSpeed,
    float MinimumSpeed
) const
{
    const float SafeScale =
        FMath::Max(
            0.0f,
            HorizontalLaunchSpeedScale
        );

    const float ScaledSpeed =
        FMath::Max(
            0.0f,
            UnscaledHorizontalSpeed
        ) *
        SafeScale;

    return FMath::Max(
        MinimumSpeed,
        ScaledSpeed
    );
}

bool ASoccerBall::CalculateDampingAwareLaunchVelocity(
    const FVector& StartLocation,
    const FVector& TargetLocation,
    float TravelTime,
    float LinearDamping,
    FVector& OutLaunchVelocity
) const
{
    OutLaunchVelocity = FVector::ZeroVector;

    const UWorld* World = GetWorld();

    if (World == nullptr || TravelTime <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const FVector Displacement =
        TargetLocation - StartLocation;

    const FVector GravityAcceleration(
        0.0f,
        0.0f,
        World->GetGravityZ()
    );

    const float SafeLinearDamping =
        FMath::Max(0.0f, LinearDamping);

    /*
     * Sin damping conservamos la ecuación balística clásica. Esta rama evita
     * pérdida de precisión numérica cuando el valor se aproxima a cero.
     */
    if (SafeLinearDamping <= 0.0001f)
    {
        OutLaunchVelocity =
            (
                Displacement -
                0.5f * GravityAcceleration *
                TravelTime * TravelTime
            ) /
            TravelTime;

        return !OutLaunchVelocity.ContainsNaN();
    }

    /*
     * Modelo continuo usado también por el predictor:
     *
     *     dV/dt = Gravity - Damping * V
     *
     * Integrando velocidad y posición se obtiene el factor de desplazamiento
     * producido por la velocidad inicial. Resolver la ecuación para V0 permite
     * que la pelota llegue al destino pese a perder velocidad en el aire.
     */
    const float DampingDecay =
        FMath::Exp(-SafeLinearDamping * TravelTime);

    const float InitialVelocityDisplacementFactor =
        (1.0f - DampingDecay) / SafeLinearDamping;

    if (InitialVelocityDisplacementFactor <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const float GravityDisplacementFactor =
        TravelTime / SafeLinearDamping -
        (1.0f - DampingDecay) /
        (SafeLinearDamping * SafeLinearDamping);

    OutLaunchVelocity =
        (
            Displacement -
            GravityAcceleration * GravityDisplacementFactor
        ) /
        InitialVelocityDisplacementFactor;

    return !OutLaunchVelocity.ContainsNaN();
}

void ASoccerBall::ApplyPreLaunchSeparation(
    const FVector& /*Direction*/,
    float /*LaunchSpeed*/,
    float /*MaxSeparationDistance*/
)
{
	/*
	 * Kept as a compatibility no-op because existing Blueprint instances may
	 * still serialize the legacy pre-launch settings. Open-play kicks now start
	 * at the ball's real contact location and change velocity only.
	 */
}

void ASoccerBall::KickToTarget(
    const FVector& TargetLocation,
    float HorizontalSpeed,
    float MinTravelTime,
    float MaxTravelTime
)
{
    if (BallMesh == nullptr)
    {
        return;
    }

    SetPossessed(false);

    const FVector Start =
        GetActorLocation();

    FVector End =
        TargetLocation;

    End.Z =
        Start.Z;

    const float Distance2D =
        FVector::Dist2D(
            Start,
            End
        );

    if (Distance2D < 10.0f)
    {
        return;
    }

    /*
     * Los pases cortos usan GroundPassSpeed, pero ahora
     * tambi�n pasan por el multiplicador central.
     */
    if (
        Distance2D <=
        GroundPassDistanceThreshold
        )
    {
        FVector Direction =
            End -
            Start;

        Direction.Z =
            0.0f;

        Direction =
            Direction.GetSafeNormal();

        if (Direction.IsNearlyZero())
        {
            return;
        }

        const float ScaledGroundPassSpeed =
            GetScaledHorizontalLaunchSpeed(
                GroundPassSpeed,
                1.0f
            );

        BallMesh->SetPhysicsLinearVelocity(
            Direction *
            ScaledGroundPassSpeed,
            false
        );

        BallMesh->
            SetPhysicsAngularVelocityInDegrees(
                FVector::ZeroVector
            );

        BallMesh->WakeAllRigidBodies();

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                1.2f,
                FColor::Green,
                FString::Printf(
                    TEXT(
                        "Pase rasante: %.0f cm/s"
                    ),
                    ScaledGroundPassSpeed
                )
            );
        }

        return;
    }

    const FVector Displacement =
        End -
        Start;

    const FVector HorizontalDisplacement(
        Displacement.X,
        Displacement.Y,
        0.0f
    );

    const float SafeRequestedHorizontalSpeed =
        FMath::Max(
            100.0f,
            HorizontalSpeed
        );

    const float BaseTravelTime =
        FMath::Clamp(
            Distance2D /
            SafeRequestedHorizontalSpeed,
            MinTravelTime,
            MaxTravelTime
        );

    /*
     * Esta es la velocidad que habr�a producido
     * originalmente la funci�n, incluyendo el Clamp.
     */
    const FVector BaseVelocityXY =
        HorizontalDisplacement /
        BaseTravelTime;

    const float SafeHorizontalScale =
        FMath::Max(
            0.01f,
            HorizontalLaunchSpeedScale
        );

    /*
     * Aplicamos la escala sobre la velocidad final real,
     * no solamente sobre la velocidad solicitada.
     */
    const FVector VelocityXY =
        BaseVelocityXY *
        SafeHorizontalScale;

    /*
     * Recalculamos el tiempo de viaje correspondiente a
     * la nueva velocidad horizontal, para que la par�bola
     * siga llegando al destino.
     */
    const float ScaledHorizontalSpeed =
        FMath::Max(
            1.0f,
            VelocityXY.Size()
        );

    const float TravelTime =
        Distance2D /
        ScaledHorizontalSpeed;

    const float GravityZ =
        GetWorld()->GetGravityZ();

    const float VelocityZ =
        (
            Displacement.Z -
            0.5f *
            GravityZ *
            TravelTime *
            TravelTime
            ) /
        TravelTime;

    const FVector LaunchVelocity =
        VelocityXY +
        FVector::UpVector *
        VelocityZ;

    BallMesh->SetPhysicsLinearVelocity(
        LaunchVelocity,
        false
    );

    BallMesh->
        SetPhysicsAngularVelocityInDegrees(
            FVector::ZeroVector
        );

    BallMesh->WakeAllRigidBodies();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            1.2f,
            FColor::Cyan,
            FString::Printf(
                TEXT(
                    "Pase/tiro: horizontal %.0f cm/s"
                ),
                VelocityXY.Size()
            )
        );
    }
}

void ASoccerBall::KickGroundToTarget(
    const FVector& TargetLocation,
    float HorizontalSpeed
)
{
    if (BallMesh == nullptr)
    {
        return;
    }

    SetPossessed(false);

    FVector Direction =
        TargetLocation -
        GetActorLocation();

    Direction.Z =
        0.0f;

    Direction =
        Direction.GetSafeNormal();

    if (Direction.IsNearlyZero())
    {
        return;
    }

    const float ScaledHorizontalSpeed =
        GetScaledHorizontalLaunchSpeed(
            HorizontalSpeed,
            100.0f
        );

    BallMesh->SetPhysicsLinearVelocity(
        Direction *
        ScaledHorizontalSpeed,
        false
    );

    BallMesh->
        SetPhysicsAngularVelocityInDegrees(
            FVector::ZeroVector,
            false
        );

    BallMesh->WakeAllRigidBodies();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            1.0f,
            FColor::Green,
            FString::Printf(
                TEXT(
                    "Pase rasante forzado: %.0f cm/s"
                ),
                ScaledHorizontalSpeed
            )
        );
    }
}

void ASoccerBall::KickToAirTarget(
    const FVector& TargetLocation,
    float HorizontalSpeed,
    float MinTravelTime,
    float MaxTravelTime
)
{
    if (BallMesh == nullptr)
    {
        return;
    }

    SetPossessed(false);

    const FVector Start =
        GetActorLocation();

    const FVector End =
        TargetLocation;

    FVector HorizontalDisplacement =
        End -
        Start;

    HorizontalDisplacement.Z =
        0.0f;

    const float Distance2D =
        HorizontalDisplacement.Size();

    if (Distance2D < 10.0f)
    {
        return;
    }

    const float SafeRequestedHorizontalSpeed =
        FMath::Max(
            100.0f,
            HorizontalSpeed
        );

    const float BaseTravelTime =
        FMath::Clamp(
            Distance2D /
            SafeRequestedHorizontalSpeed,
            MinTravelTime,
            MaxTravelTime
        );

    const FVector BaseVelocityXY =
        HorizontalDisplacement /
        BaseTravelTime;

    const float SafeHorizontalScale =
        FMath::Max(
            0.01f,
            HorizontalLaunchSpeedScale
        );

    const FVector UndampedScaledVelocityXY =
        BaseVelocityXY *
        SafeHorizontalScale;

    const float UndampedScaledHorizontalSpeed =
        FMath::Max(
            1.0f,
            UndampedScaledVelocityXY.Size()
        );

    /*
     * HorizontalLaunchSpeedScale sigue determinando el tiempo deseado de
     * vuelo. La velocidad inicial real puede ser mayor porque ahora compensa
     * la pérdida producida por AirLinearDamping durante ese mismo tiempo.
     */
    const float TravelTime =
        Distance2D /
        UndampedScaledHorizontalSpeed;

    FVector LaunchVelocity = FVector::ZeroVector;

    if (!CalculateDampingAwareLaunchVelocity(
        Start,
        End,
        TravelTime,
        AirLinearDamping,
        LaunchVelocity
    ))
    {
        return;
    }

    /*
     * La pelota puede haber estado poseída cerca del césped y conservar el
     * damping terrestre hasta el próximo Tick. Sincronizamos el rigid body
     * antes de asignar la velocidad, igual que en los contactos aéreos.
     */
    ClearAccumulatedPhysicsForcesUE427(BallMesh);
    BallMesh->SetLinearDamping(AirLinearDamping);
    BallMesh->SetAngularDamping(AirAngularDamping);
    BallMesh->WakeAllRigidBodies();

    BallMesh->SetPhysicsLinearVelocity(
        LaunchVelocity,
        false
    );

    BallMesh->
        SetPhysicsAngularVelocityInDegrees(
            FVector::ZeroVector
        );

    BallMesh->WakeAllRigidBodies();
    MarkTrajectoryChanged();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            1.5f,
            FColor::Yellow,
            FString::Printf(
                TEXT(
                    "AirTarget | solicitada %.0f | "
                    "base final %.0f | inicial damping %.0f | "
                    "scale %.2f"
                ),
                HorizontalSpeed,
                BaseVelocityXY.Size(),
                LaunchVelocity.Size2D(),
                SafeHorizontalScale
            )
        );
    }
}

void ASoccerBall::ChargedKickToTarget(
    const FVector& TargetLocation,
    float HorizontalSpeed
)
{
    if (BallMesh == nullptr)
    {
        return;
    }

    SetPossessed(false);

    const float ScaledHorizontalSpeed =
        GetScaledHorizontalLaunchSpeed(
            HorizontalSpeed,
            100.0f
        );

    const FVector Start =
        GetActorLocation();

    FVector End =
        TargetLocation;

    End.Z =
        Start.Z;

    const FVector Displacement =
        End -
        Start;

    FVector HorizontalDisplacement =
        Displacement;

    HorizontalDisplacement.Z =
        0.0f;

    const float Distance2D =
        HorizontalDisplacement.Size();

    if (Distance2D < 10.0f)
    {
        return;
    }

    /*
     * En el disparo cargado no usamos los l�mites de
     * tiempo de KickToTarget.
     */
    const float TravelTime =
        Distance2D /
        ScaledHorizontalSpeed;

    const FVector DirectionXY =
        HorizontalDisplacement.
        GetSafeNormal();

    const FVector VelocityXY =
        DirectionXY *
        ScaledHorizontalSpeed;

    const float GravityZ =
        GetWorld()->GetGravityZ();

    const float VelocityZ =
        (
            Displacement.Z -
            0.5f *
            GravityZ *
            TravelTime *
            TravelTime
            ) /
        TravelTime;

    const FVector LaunchVelocity =
        VelocityXY +
        FVector::UpVector *
        VelocityZ;

    BallMesh->SetPhysicsLinearVelocity(
        LaunchVelocity,
        false
    );

    BallMesh->
        SetPhysicsAngularVelocityInDegrees(
            FVector::ZeroVector
        );

    BallMesh->WakeAllRigidBodies();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1,
            1.2f,
            FColor::Purple,
            FString::Printf(
                TEXT(
                    "Disparo cargado: %.0f cm/s"
                ),
                ScaledHorizontalSpeed
            )
        );
    }
}

void ASoccerBall::StopBallKeepingPhysics()
{
    if (BallMesh == nullptr)
    {
        return;
    }

    BallMesh->SetSimulatePhysics(true);
    ClearAccumulatedPhysicsForcesUE427(BallMesh);
    BallMesh->WakeAllRigidBodies();

    BallMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
    BallMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    MarkTrajectoryChanged();
}

void ASoccerBall::DribbleTouch(
    const FVector& Direction,
    float TouchSpeed,
    float UpwardSpeed,
    bool /*bUsePreLaunchSeparation*/
)
{
    if (BallMesh == nullptr)
    {
        return;
    }

    FVector SafeDirection =
        Direction;

    SafeDirection.Z =
        0.0f;

    SafeDirection =
        SafeDirection.GetSafeNormal();

    if (SafeDirection.IsNearlyZero())
    {
        return;
    }

    const float ScaledTouchSpeed =
        GetScaledHorizontalLaunchSpeed(
            TouchSpeed,
            0.0f
        );

    BallMesh->SetSimulatePhysics(
        true
    );

    BallMesh->WakeAllRigidBodies();

    FVector NewVelocity =
        SafeDirection *
        ScaledTouchSpeed;

    /*
     * El impulso vertical de conducci�n no pasa por el
     * multiplicador horizontal.
     */
    NewVelocity.Z =
        UpwardSpeed;

    BallMesh->SetPhysicsLinearVelocity(
        NewVelocity,
        false
    );

    MarkTrajectoryChanged();
}
