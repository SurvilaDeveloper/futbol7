#pragma once

#include "CoreMinimal.h"
#include "SoccerAerialActionTypes.generated.h"

class ASoccerBall;
class ASoccerCharacterBase;

UENUM(BlueprintType)
enum class ESoccerAerialActionIntent : uint8
{
    Automatic UMETA(DisplayName = "Automatic"),
    Control UMETA(DisplayName = "Standing Control"),
    ActiveHeader UMETA(DisplayName = "Active Header"),
    DefensiveBlock UMETA(DisplayName = "Defensive Header Block"),
    StandingHeaderRedirect UMETA(DisplayName = "Standing Header Redirect")
};

UENUM(BlueprintType)
enum class ESoccerAerialActionType : uint8
{
    None UMETA(DisplayName = "None"),
    StandingControl UMETA(DisplayName = "Standing Aerial Control"),
    JumpHeaderKick UMETA(DisplayName = "Jump Header Kick"),
    JumpHeaderBlock UMETA(DisplayName = "Jump Header Block")
};


UENUM(BlueprintType)
enum class ESoccerAerialContactSurface : uint8
{
    None UMETA(DisplayName = "None"),
    Head UMETA(DisplayName = "Head"),
    Chest UMETA(DisplayName = "Chest")
};

USTRUCT(BlueprintType)
struct THIRDPERSONCPP_API FSoccerAerialContactResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Contact")
    bool bContactResolved = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Contact")
    bool bTouchAcceptedByRules = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Contact")
    ESoccerAerialActionType ActionType = ESoccerAerialActionType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Contact")
    ESoccerAerialContactSurface ContactSurface =
        ESoccerAerialContactSurface::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Contact")
    FVector ContactLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Contact")
    FVector OutgoingBallVelocity = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Contact")
    float MontagePosition = 0.0f;

    /* 0 = very poor contact, 1 = ideal spatial and temporal contact. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Contact")
    float ContactQuality = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Contact")
    float OpponentPressure = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Contact")
    int32 CompetingPlayerCount = 0;

};

UENUM(BlueprintType)
enum class ESoccerAerialActionPhase : uint8
{
    None UMETA(DisplayName = "None"),
    Approaching UMETA(DisplayName = "Approaching Preparation Point"),
    WaitingToStart UMETA(DisplayName = "Waiting To Start"),
    Playing UMETA(DisplayName = "Playing")
};

/*
 * Temporal and spatial calibration for one aerial montage.
 * Times are expressed in seconds from the start of the montage.
 */
USTRUCT(BlueprintType)
struct THIRDPERSONCPP_API FSoccerAerialActionProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial")
    ESoccerAerialActionType ActionType = ESoccerAerialActionType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial", meta = (ClampMin = "0.0"))
    float ExpectedMontageDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial", meta = (ClampMin = "0.0"))
    float ContactWindowStart = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial", meta = (ClampMin = "0.0"))
    float IdealContactTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial", meta = (ClampMin = "0.0"))
    float ContactWindowEnd = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial", meta = (ClampMin = "0.0"))
    float MinimumBallHeight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial", meta = (ClampMin = "0.0"))
    float MaximumBallHeight = 1000.0f;

    /* Distance from actor origin to the expected contact point in front of the body. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial", meta = (ClampMin = "0.0"))
    float ContactForwardOffset = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial", meta = (ClampMin = "0.0"))
    float PreparationReachRadius = 45.0f;

    /* Time reserved to stop and settle before the montage begins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial", meta = (ClampMin = "0.0"))
    float PreparationSettleTime = 0.08f;

    /* Allows starting immediately when the ideal start instant was missed slightly. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial", meta = (ClampMin = "0.0"))
    float MaximumLateStartTime = 0.12f;

    /* Optional jump timing. Zero values mean that the montage stays grounded. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial|Jump", meta = (ClampMin = "0.0"))
    float TakeoffTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial|Jump", meta = (ClampMin = "0.0"))
    float ApexTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Aerial|Jump", meta = (ClampMin = "0.0"))
    float LandingTime = 0.0f;
};


USTRUCT(BlueprintType)
struct THIRDPERSONCPP_API FSoccerAerialDebugSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    bool bValid = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    ESoccerAerialActionType ActionType = ESoccerAerialActionType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    ESoccerAerialActionPhase Phase = ESoccerAerialActionPhase::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    float MontagePosition = 0.0f;

    /* Scheduled-start diagnostic timeline. World times use UWorld::GetTimeSeconds(). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float CurrentWorldTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float ScheduledStartWorldTime = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float DistanceToPreparation = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float PreparationTolerance = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float MaximumCommitDistance = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float DistanceAtScheduledStart = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float EnteredMaximumCommitRadiusWorldTime = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float EnteredPreparationToleranceWorldTime = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    bool bScheduledCorrectionAttempted = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float ScheduledCorrectionDistanceBefore = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float ScheduledCorrectionDistanceAfter = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float MontagePlayRequestedWorldTime = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float MontagePlayReturnedDuration = 0.0f;

    /* Position used to compensate the fraction of a frame missed after the scheduled start. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float MontageStartPosition = 0.0f;

    /* Logical start = request world time - MontageStartPosition. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float MontageLogicalStartWorldTime = -1.0f;

    /* Observation time from a later Tick; it is not the authoritative montage start time. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug|Start Timing")
    float MontageBecameActiveWorldTime = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    float ContactWindowStart = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    float IdealContactTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    float ContactWindowEnd = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    bool bContactWindowActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    FVector BallLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    FVector HeadLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    FVector ChestLowerLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    FVector ChestUpperLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    FVector ClosestChestPoint = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    float BallRadius = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    float HeadContactRadius = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    float ChestContactRadius = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    float ExtraTolerance = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    float BallToHeadDistance = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Debug")
    float BallToChestDistance = 0.0f;
};

USTRUCT(BlueprintType)
struct THIRDPERSONCPP_API FSoccerAerialInterceptionPlan
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    bool bValid = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    ESoccerAerialActionType ActionType = ESoccerAerialActionType::None;

    /*
     * Surface selected by planning. It is an intention, not a forced result:
     * the real animated contact may still resolve on the other surface.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    ESoccerAerialContactSurface PlannedContactSurface =
        ESoccerAerialContactSurface::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    ASoccerBall* Ball = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    FVector ContactLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    FVector PreparationLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    FVector FacingDirection = FVector::ForwardVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    float BallArrivalTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    float PlayerArrivalTime = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    float MontageStartDelay = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    float ExpectedContactTimeFromMontageStart = 0.0f;

    /* Positive means that the player reaches the preparation point early. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Prediction")
    float PreparationArrivalMargin = 0.0f;

    /* Signed difference between the selected contact time and the ideal montage time. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Prediction")
    float ContactTimingOffsetFromIdeal = 0.0f;

    /* Predicted vertical distance between the ball center and animated head center. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Prediction")
    float PredictedHeadVerticalError = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    float BallHeightAboveGround = 0.0f;

    /* Predicted velocity at contact, used to evaluate the second ball. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Standing Control")
    FVector PredictedIncomingBallVelocity = FVector::ZeroVector;

    /* Optional passive standing-header direction. It never forces HEAD contact. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Standing Control")
    bool bHasStandingHeaderRedirectTarget = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Standing Control")
    FVector StandingHeaderRedirectTarget = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Standing Control")
    FVector PredictedRecoveryLocation = FVector::ZeroVector;

    /* Absolute times from the planning instant. Positive advantage favours this player. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Standing Control")
    float PredictedSelfRecoveryTime = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Standing Control")
    float PredictedOpponentRecoveryTime = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Standing Control")
    float PredictedRecoveryAdvantage = -1000.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Standing Control")
    float NearestOpponentDistanceAtContact = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial|Standing Control")
    bool bPredictedStandingControlSafe = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Aerial")
    int32 TrajectoryRevision = 0;
};

/*
 * Runtime-only candidate used to arbitrate simultaneous aerial contacts.
 * It is intentionally not reflected: it exists only during one frame.
 */
struct FSoccerAerialContactCandidate
{
    bool bValid = false;
    ASoccerCharacterBase* Character = nullptr;
    ASoccerBall* Ball = nullptr;
    ESoccerAerialActionType ActionType = ESoccerAerialActionType::None;
    ESoccerAerialContactSurface ContactSurface =
        ESoccerAerialContactSurface::None;
    FVector ContactLocation = FVector::ZeroVector;
    float MontagePosition = 0.0f;
    float NormalizedContactDistance = TNumericLimits<float>::Max();
    float SpatialQuality = 0.0f;
    float TimingQuality = 0.0f;
    float FacingQuality = 0.0f;
    float SpeedQuality = 0.0f;
    float AirborneCommitment = 0.0f;
    float ContactQuality = 0.0f;
    float ContestScore = 0.0f;
};

