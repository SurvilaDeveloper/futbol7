#include "SoccerFormationLibrary.h"

#include <initializer_list>

namespace
{
	FSoccerFormationSlot MakeSlot(
		const TCHAR* SlotId,
		ESoccerPlayerRole PlayerRole,
		ESoccerFormationLine FormationLine,
		ESoccerFormationLane FormationLane,
		float DepthAlpha,
		float LateralAlpha
	)
	{
		FSoccerFormationSlot Slot;
		Slot.SlotId = FName(SlotId);
		Slot.PlayerRole = PlayerRole;
		Slot.FormationLine = FormationLine;
		Slot.FormationLane = FormationLane;
		Slot.DepthAlpha = DepthAlpha;
		Slot.LateralAlpha = LateralAlpha;
		return Slot;
	}

	FSoccerFormationDefinition MakeDefinition(
		ESoccerFormationSystem System,
		const TCHAR* DisplayName,
		std::initializer_list<FSoccerFormationSlot> Slots
	)
	{
		FSoccerFormationDefinition Definition;
		Definition.System = System;
		Definition.DisplayName = FText::FromString(DisplayName);
		Definition.Slots.Reserve(static_cast<int32>(Slots.size()));

		for (const FSoccerFormationSlot& Slot : Slots)
		{
			Definition.Slots.Add(Slot);
		}

		return Definition;
	}

	const FSoccerFormationDefinition& FormationOneThreeTwoOne()
	{
		static const FSoccerFormationDefinition Definition = MakeDefinition(
			ESoccerFormationSystem::OneThreeTwoOne,
			TEXT("1-3-2-1"),
			{
				MakeSlot(TEXT("GK"), ESoccerPlayerRole::Goalkeeper, ESoccerFormationLine::Goalkeeper, ESoccerFormationLane::Center, 0.055f, 0.00f),
				MakeSlot(TEXT("DEF_L"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Left, 0.26f, -0.66f),
				MakeSlot(TEXT("DEF_C"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Center, 0.24f, 0.00f),
				MakeSlot(TEXT("DEF_R"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Right, 0.26f, 0.66f),
				MakeSlot(TEXT("MID_L"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::Midfield, ESoccerFormationLane::Left, 0.49f, -0.43f),
				MakeSlot(TEXT("MID_R"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::Midfield, ESoccerFormationLane::Right, 0.49f, 0.43f),
				MakeSlot(TEXT("FWD_C"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Center, 0.73f, 0.00f)
			}
		);
		return Definition;
	}

	const FSoccerFormationDefinition& FormationOneTwoThreeOne()
	{
		static const FSoccerFormationDefinition Definition = MakeDefinition(
			ESoccerFormationSystem::OneTwoThreeOne,
			TEXT("1-2-3-1"),
			{
				MakeSlot(TEXT("GK"), ESoccerPlayerRole::Goalkeeper, ESoccerFormationLine::Goalkeeper, ESoccerFormationLane::Center, 0.055f, 0.00f),
				MakeSlot(TEXT("DEF_L"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Left, 0.27f, -0.48f),
				MakeSlot(TEXT("DEF_R"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Right, 0.27f, 0.48f),
				MakeSlot(TEXT("MID_L"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::Midfield, ESoccerFormationLane::Left, 0.49f, -0.68f),
				MakeSlot(TEXT("MID_C"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::Midfield, ESoccerFormationLane::Center, 0.47f, 0.00f),
				MakeSlot(TEXT("MID_R"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::Midfield, ESoccerFormationLane::Right, 0.49f, 0.68f),
				MakeSlot(TEXT("FWD_C"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Center, 0.73f, 0.00f)
			}
		);
		return Definition;
	}

	const FSoccerFormationDefinition& FormationOneThreeThree()
	{
		static const FSoccerFormationDefinition Definition = MakeDefinition(
			ESoccerFormationSystem::OneThreeThree,
			TEXT("1-3-3"),
			{
				MakeSlot(TEXT("GK"), ESoccerPlayerRole::Goalkeeper, ESoccerFormationLine::Goalkeeper, ESoccerFormationLane::Center, 0.055f, 0.00f),
				MakeSlot(TEXT("DEF_L"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Left, 0.25f, -0.66f),
				MakeSlot(TEXT("DEF_C"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Center, 0.23f, 0.00f),
				MakeSlot(TEXT("DEF_R"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Right, 0.25f, 0.66f),
				MakeSlot(TEXT("FWD_L"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Left, 0.66f, -0.70f),
				MakeSlot(TEXT("FWD_C"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Center, 0.68f, 0.00f),
				MakeSlot(TEXT("FWD_R"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Right, 0.66f, 0.70f)
			}
		);
		return Definition;
	}

	const FSoccerFormationDefinition& FormationOneTwoTwoTwo()
	{
		static const FSoccerFormationDefinition Definition = MakeDefinition(
			ESoccerFormationSystem::OneTwoTwoTwo,
			TEXT("1-2-2-2"),
			{
				MakeSlot(TEXT("GK"), ESoccerPlayerRole::Goalkeeper, ESoccerFormationLine::Goalkeeper, ESoccerFormationLane::Center, 0.055f, 0.00f),
				MakeSlot(TEXT("DEF_L"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Left, 0.27f, -0.48f),
				MakeSlot(TEXT("DEF_R"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Right, 0.27f, 0.48f),
				MakeSlot(TEXT("MID_L"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::Midfield, ESoccerFormationLane::Left, 0.49f, -0.46f),
				MakeSlot(TEXT("MID_R"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::Midfield, ESoccerFormationLane::Right, 0.49f, 0.46f),
				MakeSlot(TEXT("FWD_L"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Left, 0.70f, -0.40f),
				MakeSlot(TEXT("FWD_R"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Right, 0.70f, 0.40f)
			}
		);
		return Definition;
	}

	const FSoccerFormationDefinition& FormationOneThreeOneTwo()
	{
		static const FSoccerFormationDefinition Definition = MakeDefinition(
			ESoccerFormationSystem::OneThreeOneTwo,
			TEXT("1-3-1-2"),
			{
				MakeSlot(TEXT("GK"), ESoccerPlayerRole::Goalkeeper, ESoccerFormationLine::Goalkeeper, ESoccerFormationLane::Center, 0.055f, 0.00f),
				MakeSlot(TEXT("DEF_L"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Left, 0.26f, -0.66f),
				MakeSlot(TEXT("DEF_C"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Center, 0.24f, 0.00f),
				MakeSlot(TEXT("DEF_R"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Right, 0.26f, 0.66f),
				MakeSlot(TEXT("MID_C"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::Midfield, ESoccerFormationLane::Center, 0.49f, 0.00f),
				MakeSlot(TEXT("FWD_L"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Left, 0.70f, -0.40f),
				MakeSlot(TEXT("FWD_R"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Right, 0.70f, 0.40f)
			}
		);
		return Definition;
	}

	const FSoccerFormationDefinition& FormationOneTwoOneTwoOne()
	{
		static const FSoccerFormationDefinition Definition = MakeDefinition(
			ESoccerFormationSystem::OneTwoOneTwoOne,
			TEXT("1-2-1-2-1"),
			{
				MakeSlot(TEXT("GK"), ESoccerPlayerRole::Goalkeeper, ESoccerFormationLine::Goalkeeper, ESoccerFormationLane::Center, 0.055f, 0.00f),
				MakeSlot(TEXT("DEF_L"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Left, 0.26f, -0.48f),
				MakeSlot(TEXT("DEF_R"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Right, 0.26f, 0.48f),
				MakeSlot(TEXT("DM_C"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::DefensiveMidfield, ESoccerFormationLane::Center, 0.41f, 0.00f),
				MakeSlot(TEXT("AM_L"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::AttackingMidfield, ESoccerFormationLane::Left, 0.59f, -0.43f),
				MakeSlot(TEXT("AM_R"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::AttackingMidfield, ESoccerFormationLane::Right, 0.59f, 0.43f),
				MakeSlot(TEXT("FWD_C"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Center, 0.74f, 0.00f)
			}
		);
		return Definition;
	}

	const FSoccerFormationDefinition& FormationOneTwoOneThree()
	{
		static const FSoccerFormationDefinition Definition = MakeDefinition(
			ESoccerFormationSystem::OneTwoOneThree,
			TEXT("1-2-1-3"),
			{
				MakeSlot(TEXT("GK"), ESoccerPlayerRole::Goalkeeper, ESoccerFormationLine::Goalkeeper, ESoccerFormationLane::Center, 0.055f, 0.00f),
				MakeSlot(TEXT("DEF_L"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Left, 0.27f, -0.48f),
				MakeSlot(TEXT("DEF_R"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Right, 0.27f, 0.48f),
				MakeSlot(TEXT("MID_C"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::Midfield, ESoccerFormationLane::Center, 0.44f, 0.00f),
				MakeSlot(TEXT("FWD_L"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Left, 0.70f, -0.70f),
				MakeSlot(TEXT("FWD_C"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Center, 0.72f, 0.00f),
				MakeSlot(TEXT("FWD_R"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Right, 0.70f, 0.70f)
			}
		);
		return Definition;
	}

	const FSoccerFormationDefinition& FormationOneFourOneOne()
	{
		static const FSoccerFormationDefinition Definition = MakeDefinition(
			ESoccerFormationSystem::OneFourOneOne,
			TEXT("1-4-1-1"),
			{
				MakeSlot(TEXT("GK"), ESoccerPlayerRole::Goalkeeper, ESoccerFormationLine::Goalkeeper, ESoccerFormationLane::Center, 0.055f, 0.00f),
				MakeSlot(TEXT("DEF_L"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Left, 0.23f, -0.74f),
				MakeSlot(TEXT("DEF_LC"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::LeftCenter, 0.22f, -0.25f),
				MakeSlot(TEXT("DEF_RC"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::RightCenter, 0.22f, 0.25f),
				MakeSlot(TEXT("DEF_R"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Right, 0.23f, 0.74f),
				MakeSlot(TEXT("MID_C"), ESoccerPlayerRole::Midfielder, ESoccerFormationLine::Midfield, ESoccerFormationLane::Center, 0.44f, 0.00f),
				MakeSlot(TEXT("FWD_C"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Center, 0.69f, 0.00f)
			}
		);
		return Definition;
	}

	const FSoccerFormationDefinition& FormationOneFiveOne()
	{
		static const FSoccerFormationDefinition Definition = MakeDefinition(
			ESoccerFormationSystem::OneFiveOne,
			TEXT("1-5-1"),
			{
				MakeSlot(TEXT("GK"), ESoccerPlayerRole::Goalkeeper, ESoccerFormationLine::Goalkeeper, ESoccerFormationLane::Center, 0.055f, 0.00f),
				MakeSlot(TEXT("DEF_L"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Left, 0.22f, -0.78f),
				MakeSlot(TEXT("DEF_LC"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::LeftCenter, 0.21f, -0.39f),
				MakeSlot(TEXT("DEF_C"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Center, 0.20f, 0.00f),
				MakeSlot(TEXT("DEF_RC"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::RightCenter, 0.21f, 0.39f),
				MakeSlot(TEXT("DEF_R"), ESoccerPlayerRole::Defender, ESoccerFormationLine::Defense, ESoccerFormationLane::Right, 0.22f, 0.78f),
				MakeSlot(TEXT("FWD_C"), ESoccerPlayerRole::Forward, ESoccerFormationLine::Attack, ESoccerFormationLane::Center, 0.67f, 0.00f)
			}
		);
		return Definition;
	}
}

const FSoccerFormationDefinition& SoccerFormationLibrary::GetDefinition(
	ESoccerFormationSystem System
)
{
	switch (System)
	{
	case ESoccerFormationSystem::OneTwoThreeOne:
		return FormationOneTwoThreeOne();
	case ESoccerFormationSystem::OneThreeThree:
		return FormationOneThreeThree();
	case ESoccerFormationSystem::OneTwoTwoTwo:
		return FormationOneTwoTwoTwo();
	case ESoccerFormationSystem::OneThreeOneTwo:
		return FormationOneThreeOneTwo();
	case ESoccerFormationSystem::OneTwoOneTwoOne:
		return FormationOneTwoOneTwoOne();
	case ESoccerFormationSystem::OneTwoOneThree:
		return FormationOneTwoOneThree();
	case ESoccerFormationSystem::OneFourOneOne:
		return FormationOneFourOneOne();
	case ESoccerFormationSystem::OneFiveOne:
		return FormationOneFiveOne();
	case ESoccerFormationSystem::OneThreeTwoOne:
	default:
		return FormationOneThreeTwoOne();
	}
}

bool SoccerFormationLibrary::IsValidSevenASideDefinition(
	const FSoccerFormationDefinition& Definition
)
{
	if (Definition.Slots.Num() != 7)
	{
		return false;
	}

	int32 GoalkeeperCount = 0;
	TSet<FName> UsedSlotIds;

	for (const FSoccerFormationSlot& Slot : Definition.Slots)
	{
		if (Slot.SlotId.IsNone() || UsedSlotIds.Contains(Slot.SlotId))
		{
			return false;
		}

		UsedSlotIds.Add(Slot.SlotId);

		if (Slot.PlayerRole == ESoccerPlayerRole::Goalkeeper)
		{
			GoalkeeperCount++;
		}

		if (
			Slot.DepthAlpha < 0.0f ||
			Slot.DepthAlpha > 1.0f ||
			Slot.LateralAlpha < -1.0f ||
			Slot.LateralAlpha > 1.0f
		)
		{
			return false;
		}
	}

	return GoalkeeperCount == 1;
}
