// SoccerFieldDimensions.h

#pragma once

#include "CoreMinimal.h"

namespace SoccerFieldDimensions
{
	// Unreal usa centímetros.

	// ============================================================
	// Cancha
	// ============================================================

	// Dimensiones de referencia con las que fueron calibrados originalmente
	// los HomePositionActor y los offsets tacticos expresados en centimetros.
	// NO son las dimensiones actuales de la cancha: sirven exclusivamente para
	// convertir esas calibraciones antiguas a cualquier PitchLength/PitchWidth.
	static constexpr float AuthoredReferencePitchLengthCm = 6000.0f;
	static constexpr float AuthoredReferencePitchWidthCm = 4000.0f;

	// Dimensiones actuales del campo. Cambiar solamente estas dos constantes
	// debe adaptar geometria, referencias authored y shape tactico.
	static constexpr float PitchLengthCm = 6000.0f;
	static constexpr float PitchWidthCm = 4000.0f;

	static constexpr float HalfPitchLengthCm = PitchLengthCm * 0.5f;
	static constexpr float HalfPitchWidthCm = PitchWidthCm * 0.5f;

	static constexpr float HalfwayLineX = 0.0f;
	static constexpr float CenterY = 0.0f;

	static constexpr float LeftGoalLineX = -HalfPitchLengthCm;
	static constexpr float RightGoalLineX = HalfPitchLengthCm;

	static constexpr float NorthTouchLineY = HalfPitchWidthCm;
	static constexpr float SouthTouchLineY = -HalfPitchWidthCm;

	// ============================================================
	// Escala de referencias/tuning authored
	// ============================================================
	// Muchos offsets tacticos fueron ajustados originalmente en la cancha
	// 60x40. Estas funciones mantienen exactamente esos valores en 60x40 y
	// los escalan por eje cuando cambia el tamano del campo.
	FORCEINLINE float GetAuthoredLengthScale()
	{
		return PitchLengthCm / AuthoredReferencePitchLengthCm;
	}

	FORCEINLINE float GetAuthoredWidthScale()
	{
		return PitchWidthCm / AuthoredReferencePitchWidthCm;
	}

	FORCEINLINE float ScaleAuthoredLongitudinalDistance(float DistanceCm)
	{
		return DistanceCm * GetAuthoredLengthScale();
	}

	FORCEINLINE float ScaleAuthoredLateralDistance(float DistanceCm)
	{
		return DistanceCm * GetAuthoredWidthScale();
	}

	// ============================================================
	// Recinto exterior
	// ============================================================

	static constexpr float OuterLengthCm = 14000.0f;
	static constexpr float OuterWidthCm = 12000.0f;

	static constexpr float HalfOuterLengthCm = OuterLengthCm * 0.5f;
	static constexpr float HalfOuterWidthCm = OuterWidthCm * 0.5f;

	// ============================================================
	// Distancia reglamentaria de reinicios
	// ============================================================
	// En la cancha authored de 60x40 el círculo central tenía 6 m. El radio
	// conserva esa proporción respecto del ancho cuando cambia el tamaño del
	// campo. El círculo central y los arcos penales comparten necesariamente
	// esta distancia para no desincronizar dibujo y reglas.
	static constexpr float AuthoredRestartDistanceRadiusCm = 600.0f;
	static constexpr float RestartDistanceRadiusCm =
		AuthoredRestartDistanceRadiusCm *
		(PitchWidthCm / AuthoredReferencePitchWidthCm);

	static constexpr float CenterCircleRadiusCm = RestartDistanceRadiusCm;
	static constexpr float PenaltyArcRadiusCm = RestartDistanceRadiusCm;

	// ============================================================
	// Áreas
	// ============================================================

	static constexpr float PenaltyAreaDepthCm = 1300.0f;
	static constexpr float PenaltyAreaWidthCm = 2000.0f;
	static constexpr float PenaltyAreaHalfWidthCm = PenaltyAreaWidthCm * 0.5f;

	static constexpr float PenaltySpotDistanceCm = 900.0f;

	// Distancia longitudinal entre el punto penal y el frente del área. Sirve
	// para calcular la porción visible del círculo que queda fuera del área.
	static constexpr float PenaltySpotToAreaFrontCm =
		PenaltyAreaDepthCm - PenaltySpotDistanceCm;

	FORCEINLINE float GetPenaltyArcHalfAngleDegrees()
	{
		if (PenaltyArcRadiusCm <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float IntersectionRatio = FMath::Clamp(
			PenaltySpotToAreaFrontCm / PenaltyArcRadiusCm,
			-1.0f,
			1.0f
		);

		return FMath::RadiansToDegrees(FMath::Acos(IntersectionRatio));
	}

	// Area de meta. Mantiene aproximadamente la proporcion reglamentaria
	// respecto del area penal usada por esta cancha reducida.
	static constexpr float GoalAreaDepthCm = 430.0f;
	static constexpr float GoalAreaWidthCm = 900.0f;
	static constexpr float GoalAreaHalfWidthCm = GoalAreaWidthCm * 0.5f;

	static constexpr float LeftGoalAreaFrontX =
		LeftGoalLineX + GoalAreaDepthCm;

	static constexpr float RightGoalAreaFrontX =
		RightGoalLineX - GoalAreaDepthCm;

	static constexpr float LeftGoalAreaCenterX =
		(LeftGoalLineX + LeftGoalAreaFrontX) * 0.5f;

	static constexpr float RightGoalAreaCenterX =
		(RightGoalLineX + RightGoalAreaFrontX) * 0.5f;

	FORCEINLINE bool IsLocationInsideLeftGoalArea2D(
		const FVector& Location,
		float MarginCm = 0.0f
	)
	{
		return
			Location.X >= LeftGoalLineX - MarginCm &&
			Location.X <= LeftGoalAreaFrontX + MarginCm &&
			FMath::Abs(Location.Y) <= GoalAreaHalfWidthCm + MarginCm;
	}

	FORCEINLINE bool IsLocationInsideRightGoalArea2D(
		const FVector& Location,
		float MarginCm = 0.0f
	)
	{
		return
			Location.X <= RightGoalLineX + MarginCm &&
			Location.X >= RightGoalAreaFrontX - MarginCm &&
			FMath::Abs(Location.Y) <= GoalAreaHalfWidthCm + MarginCm;
	}

	// GoalLineSign: -1 para el arco izquierdo, +1 para el derecho.
	FORCEINLINE bool IsLocationInsideGoalArea2D(
		const FVector& Location,
		float GoalLineSign,
		float MarginCm = 0.0f
	)
	{
		return GoalLineSign < 0.0f
			? IsLocationInsideLeftGoalArea2D(Location, MarginCm)
			: IsLocationInsideRightGoalArea2D(Location, MarginCm);
	}


	// ============================================================
	// Esquinas
	// ============================================================

	static constexpr float CornerArcRadiusCm = 100.0f;

	// ============================================================
	// Arcos
	// ============================================================

	static constexpr float GoalWidthCm = 600.0f;
	static constexpr float GoalHalfWidthCm = GoalWidthCm * 0.5f;

	static constexpr float GoalHeightCm = 230.0f;
	static constexpr float GoalPostThicknessCm = 12.0f;

	static constexpr float GoalDepthCm = 120.0f;
	static constexpr float GoalNetThicknessCm = 4.0f;

	// ============================================================
	// Helpers geometricos locales
	// ============================================================
	// SoccerFieldDimensions describe el campo en el espacio LOCAL de
	// ASoccerField. ASoccerField es quien transforma luego estos puntos al
	// mundo. Esto permite mover/rotar la cancha sin volver a codificar
	// coordenadas de reglas en SoccerMatchManager.

	FORCEINLINE float NormalizeGoalLineSign(float GoalLineSign)
	{
		return GoalLineSign < 0.0f ? -1.0f : 1.0f;
	}

	FORCEINLINE float GetGoalLineX(float GoalLineSign)
	{
		return NormalizeGoalLineSign(GoalLineSign) * HalfPitchLengthCm;
	}

	FORCEINLINE FVector GetGoalCenterLocalLocation(
		float GoalLineSign,
		float LocalZ = 0.0f
	)
	{
		return FVector(
			GetGoalLineX(GoalLineSign),
			CenterY,
			LocalZ
		);
	}

	FORCEINLINE FVector GetPenaltySpotLocalLocation(
		float GoalLineSign,
		float LocalZ = 0.0f
	)
	{
		const float Sign = NormalizeGoalLineSign(GoalLineSign);

		return FVector(
			Sign * (HalfPitchLengthCm - PenaltySpotDistanceCm),
			CenterY,
			LocalZ
		);
	}

	FORCEINLINE float GetPenaltySpotBehindProgress2D(
		const FVector& Location,
		float GoalLineSign
	)
	{
		const float Sign = NormalizeGoalLineSign(GoalLineSign);
		const FVector PenaltySpot = GetPenaltySpotLocalLocation(Sign);

		// Positivo significa hacia el centro del campo, es decir, detrás del
		// punto penal con respecto al arco defendido.
		return (Location.X - PenaltySpot.X) * -Sign;
	}

	FORCEINLINE bool IsLocationInsidePenaltyDistanceCircle2D(
		const FVector& Location,
		float GoalLineSign,
		float ExtraRadiusCm = 0.0f
	)
	{
		const FVector PenaltySpot = GetPenaltySpotLocalLocation(GoalLineSign);
		const float Radius = FMath::Max(
			0.0f,
			PenaltyArcRadiusCm + ExtraRadiusCm
		);
		const float DeltaX = Location.X - PenaltySpot.X;
		const float DeltaY = Location.Y - PenaltySpot.Y;

		return DeltaX * DeltaX + DeltaY * DeltaY < Radius * Radius;
	}

	FORCEINLINE FVector GetCornerLocalLocation(
		float GoalLineSign,
		float TouchlineSign,
		float InsetCm = 0.0f,
		float LocalZ = 0.0f
	)
	{
		const float GoalSign = NormalizeGoalLineSign(GoalLineSign);
		const float TouchSign = NormalizeGoalLineSign(TouchlineSign);
		const float SafeInset = FMath::Clamp(
			InsetCm,
			0.0f,
			FMath::Min(HalfPitchLengthCm, HalfPitchWidthCm)
		);

		return FVector(
			GoalSign * (HalfPitchLengthCm - SafeInset),
			TouchSign * (HalfPitchWidthCm - SafeInset),
			LocalZ
		);
	}

	FORCEINLINE bool IsLocationInsidePenaltyArea2D(
		const FVector& Location,
		float GoalLineSign,
		float ExtraDepthCm = 0.0f,
		float ExtraHalfWidthCm = 0.0f
	)
	{
		const float Sign = NormalizeGoalLineSign(GoalLineSign);
		const float GoalLineX = GetGoalLineX(Sign);
		const float InwardSign = -Sign;
		const float Depth = (Location.X - GoalLineX) * InwardSign;

		return
			Depth >= -25.0f &&
			Depth <= PenaltyAreaDepthCm + FMath::Max(0.0f, ExtraDepthCm) &&
			FMath::Abs(Location.Y - CenterY) <=
				PenaltyAreaHalfWidthCm + FMath::Max(0.0f, ExtraHalfWidthCm);
	}

	FORCEINLINE bool IsLocationInsidePitch2D(
		const FVector& Location,
		float InsetCm = 0.0f
	)
	{
		const float RequestedInset = FMath::Max(0.0f, InsetCm);
		const float SafeLengthInset = FMath::Min(
			RequestedInset,
			FMath::Max(0.0f, HalfPitchLengthCm - 1.0f)
		);
		const float SafeWidthInset = FMath::Min(
			RequestedInset,
			FMath::Max(0.0f, HalfPitchWidthCm - 1.0f)
		);

		return
			FMath::Abs(Location.X) <= HalfPitchLengthCm - SafeLengthInset &&
			FMath::Abs(Location.Y) <= HalfPitchWidthCm - SafeWidthInset;
	}

	FORCEINLINE FVector ClampLocationInsidePitch(
		const FVector& Location,
		float InsetCm = 0.0f
	)
	{
		const float RequestedInset = FMath::Max(0.0f, InsetCm);
		const float SafeLengthInset = FMath::Min(
			RequestedInset,
			FMath::Max(0.0f, HalfPitchLengthCm - 1.0f)
		);
		const float SafeWidthInset = FMath::Min(
			RequestedInset,
			FMath::Max(0.0f, HalfPitchWidthCm - 1.0f)
		);

		FVector Result = Location;
		Result.X = FMath::Clamp(
			Result.X,
			-HalfPitchLengthCm + SafeLengthInset,
			HalfPitchLengthCm - SafeLengthInset
		);
		Result.Y = FMath::Clamp(
			Result.Y,
			-HalfPitchWidthCm + SafeWidthInset,
			HalfPitchWidthCm - SafeWidthInset
		);
		return Result;
	}
}
