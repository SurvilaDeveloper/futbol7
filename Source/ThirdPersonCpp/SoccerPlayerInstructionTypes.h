#pragma once

#include "CoreMinimal.h"
#include "SoccerPlayerInstructionTypes.generated.h"

/*
 * Slot-level instructions are deliberately separate from both formation and
 * team tactics. Formation says WHERE the team is structured; the collective
 * tactical plan says HOW the team wants to play; these values say what the
 * player currently occupying a formation slot should prioritize.
 *
 * Stage 6 stores/edits these instructions only. Later stages will connect them
 * to the existing open-play AI orders.
 */

UENUM(BlueprintType)
enum class ESoccerIndividualAttackInstruction : uint8
{
	Balanced UMETA(DisplayName = "Balanced"),
	HoldPosition UMETA(DisplayName = "Hold Position"),
	LinkPlay UMETA(DisplayName = "Link Play"),
	MakeForwardRuns UMETA(DisplayName = "Make Forward Runs"),
	StayWide UMETA(DisplayName = "Stay Wide"),
	ComeShort UMETA(DisplayName = "Come Short"),
	TargetPlayer UMETA(DisplayName = "Target Player")
};

UENUM(BlueprintType)
enum class ESoccerIndividualDefensiveInstruction : uint8
{
	Balanced UMETA(DisplayName = "Balanced"),
	HoldPosition UMETA(DisplayName = "Hold Position"),
	PressBall UMETA(DisplayName = "Press Ball"),
	Cover UMETA(DisplayName = "Cover"),
	ProtectCenter UMETA(DisplayName = "Protect Center"),
	MarkTightly UMETA(DisplayName = "Mark Tightly")
};

USTRUCT(BlueprintType)
struct FSoccerSlotTacticalInstruction
{
	GENERATED_BODY()

	/* Formation slot receiving this instruction, e.g. DEF_C, MID_L, FWD_C. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Individual")
	FName SlotId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Individual")
	ESoccerIndividualAttackInstruction AttackInstruction =
		ESoccerIndividualAttackInstruction::Balanced;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Individual")
	ESoccerIndividualDefensiveInstruction DefensiveInstruction =
		ESoccerIndividualDefensiveInstruction::Balanced;

	/*
	 * Optional opponent formation-slot target used by a later marking stage.
	 * NAME_None means automatic/no explicit individual marking target.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Individual")
	FName MarkingTargetSlotId = NAME_None;
};
