#include "SoccerTacticalPresetManager.h"

#include "SoccerFormationLibrary.h"
#include "SoccerMatchManager.h"
#include "SoccerTacticalPresetSaveGame.h"
#include "SoccerTeamTypes.h"

#include "Kismet/GameplayStatics.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	const FString TacticalPresetSaveSlotName = TEXT("SoccerTacticalPresets");
	const int32 TacticalPresetSaveUserIndex = 0;
	const int32 CurrentPresetDataVersion = 1;
	const int32 CurrentSaveFormatVersion = 2;
	const int32 CurrentJsonExchangeFormatVersion = 1;
	const FString TacticalPresetJsonFileType = TEXT("SoccerTacticalPreset");
	const int32 QuickPresetSlotCount = 4;
	const int32 MaximumPresetNameLength = 48;
	const int64 MaximumJsonImportBytes = 512 * 1024;

	bool IsKnownFormationSystem(ESoccerFormationSystem FormationSystem)
	{
		switch (FormationSystem)
		{
		case ESoccerFormationSystem::OneThreeTwoOne:
		case ESoccerFormationSystem::OneTwoThreeOne:
		case ESoccerFormationSystem::OneThreeThree:
		case ESoccerFormationSystem::OneTwoTwoTwo:
		case ESoccerFormationSystem::OneThreeOneTwo:
		case ESoccerFormationSystem::OneTwoOneTwoOne:
		case ESoccerFormationSystem::OneTwoOneThree:
		case ESoccerFormationSystem::OneFourOneOne:
		case ESoccerFormationSystem::OneFiveOne:
			return true;
		default:
			return false;
		}
	}

	FString MakeSafeJsonFileStem(const FString& InName)
	{
		FString Result;
		Result.Reserve(InName.Len());

		for (const TCHAR Character : InName)
		{
			if (
				FChar::IsAlnum(Character) ||
				Character == TEXT('_') ||
				Character == TEXT('-')
			)
			{
				Result.AppendChar(Character);
			}
			else if (Character == TEXT(' '))
			{
				Result.AppendChar(TEXT('_'));
			}
		}

		while (Result.Contains(TEXT("__")))
		{
			Result.ReplaceInline(TEXT("__"), TEXT("_"));
		}

		Result.TrimStartAndEndInline();
		if (Result.IsEmpty())
		{
			Result = TEXT("Preset");
		}

		return Result.Left(48);
	}

	bool IsKnownBuildUpStyle(ESoccerBuildUpStyle Value)
	{
		switch (Value)
		{
		case ESoccerBuildUpStyle::ShortPossession:
		case ESoccerBuildUpStyle::Balanced:
		case ESoccerBuildUpStyle::Direct:
			return true;
		default:
			return false;
		}
	}

	bool IsKnownAttackChannel(ESoccerAttackChannel Value)
	{
		switch (Value)
		{
		case ESoccerAttackChannel::Balanced:
		case ESoccerAttackChannel::Left:
		case ESoccerAttackChannel::Center:
		case ESoccerAttackChannel::Right:
			return true;
		default:
			return false;
		}
	}

	bool IsKnownAttackingWidth(ESoccerAttackingWidth Value)
	{
		return
			Value == ESoccerAttackingWidth::Narrow ||
			Value == ESoccerAttackingWidth::Balanced ||
			Value == ESoccerAttackingWidth::Wide;
	}

	bool IsKnownAttackingTempo(ESoccerAttackingTempo Value)
	{
		return
			Value == ESoccerAttackingTempo::Patient ||
			Value == ESoccerAttackingTempo::Balanced ||
			Value == ESoccerAttackingTempo::Fast;
	}

	bool IsKnownAttackingTransition(ESoccerAttackingTransition Value)
	{
		return
			Value == ESoccerAttackingTransition::RetainPossession ||
			Value == ESoccerAttackingTransition::Balanced ||
			Value == ESoccerAttackingTransition::CounterAttack;
	}

	bool IsKnownDefensiveBlock(ESoccerDefensiveBlock Value)
	{
		return
			Value == ESoccerDefensiveBlock::Low ||
			Value == ESoccerDefensiveBlock::Medium ||
			Value == ESoccerDefensiveBlock::High;
	}

	bool IsKnownPressingIntensity(ESoccerPressingIntensity Value)
	{
		return
			Value == ESoccerPressingIntensity::Low ||
			Value == ESoccerPressingIntensity::Medium ||
			Value == ESoccerPressingIntensity::High;
	}

	bool IsKnownMarkingStyle(ESoccerMarkingStyle Value)
	{
		return
			Value == ESoccerMarkingStyle::Zonal ||
			Value == ESoccerMarkingStyle::Mixed ||
			Value == ESoccerMarkingStyle::ManToMan;
	}

	bool IsKnownDefensiveTransition(ESoccerDefensiveTransition Value)
	{
		return
			Value == ESoccerDefensiveTransition::Regroup ||
			Value == ESoccerDefensiveTransition::Balanced ||
			Value == ESoccerDefensiveTransition::CounterPress;
	}

	bool IsKnownIndividualAttackInstruction(
		ESoccerIndividualAttackInstruction Value
	)
	{
		switch (Value)
		{
		case ESoccerIndividualAttackInstruction::Balanced:
		case ESoccerIndividualAttackInstruction::HoldPosition:
		case ESoccerIndividualAttackInstruction::LinkPlay:
		case ESoccerIndividualAttackInstruction::MakeForwardRuns:
		case ESoccerIndividualAttackInstruction::StayWide:
		case ESoccerIndividualAttackInstruction::ComeShort:
		case ESoccerIndividualAttackInstruction::TargetPlayer:
			return true;
		default:
			return false;
		}
	}

	bool IsKnownIndividualDefensiveInstruction(
		ESoccerIndividualDefensiveInstruction Value
	)
	{
		switch (Value)
		{
		case ESoccerIndividualDefensiveInstruction::Balanced:
		case ESoccerIndividualDefensiveInstruction::HoldPosition:
		case ESoccerIndividualDefensiveInstruction::PressBall:
		case ESoccerIndividualDefensiveInstruction::Cover:
		case ESoccerIndividualDefensiveInstruction::ProtectCenter:
		case ESoccerIndividualDefensiveInstruction::MarkTightly:
			return true;
		default:
			return false;
		}
	}

	bool AreTacticalPlansEqual(
		const FSoccerTeamTacticalPlan& A,
		const FSoccerTeamTacticalPlan& B
	)
	{
		return
			A.BuildUpStyle == B.BuildUpStyle &&
			A.AttackChannel == B.AttackChannel &&
			A.AttackingWidth == B.AttackingWidth &&
			A.AttackingTempo == B.AttackingTempo &&
			A.AttackingTransition == B.AttackingTransition &&
			A.DefensiveBlock == B.DefensiveBlock &&
			A.PressingIntensity == B.PressingIntensity &&
			A.MarkingStyle == B.MarkingStyle &&
			A.DefensiveTransition == B.DefensiveTransition;
	}

	bool AreInstructionsEqual(
		const FSoccerSlotTacticalInstruction& A,
		const FSoccerSlotTacticalInstruction& B
	)
	{
		return
			A.SlotId == B.SlotId &&
			A.AttackInstruction == B.AttackInstruction &&
			A.DefensiveInstruction == B.DefensiveInstruction &&
			A.MarkingTargetSlotId == B.MarkingTargetSlotId;
	}

	bool AreInstructionSetsEqual(
		const TArray<FSoccerSlotTacticalInstruction>& A,
		const TArray<FSoccerSlotTacticalInstruction>& B
	)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (const FSoccerSlotTacticalInstruction& InstructionA : A)
		{
			const FSoccerSlotTacticalInstruction* InstructionB = B.FindByPredicate(
				[&InstructionA](const FSoccerSlotTacticalInstruction& Candidate)
				{
					return Candidate.SlotId == InstructionA.SlotId;
				}
			);

			if (InstructionB == nullptr || !AreInstructionsEqual(InstructionA, *InstructionB))
			{
				return false;
			}
		}

		return true;
	}
}

void USoccerTacticalPresetManager::Initialize(
	ASoccerMatchManager* InMatchManager
)
{
	MatchManager = InMatchManager;
	BuildBuiltInPresets();

	FString LoadMessage;
	ReloadFromDisk(LoadMessage);
}

const TArray<FSoccerTacticalPreset>&
USoccerTacticalPresetManager::GetPresets() const
{
	return Presets;
}

const TArray<FSoccerTacticalPreset>&
USoccerTacticalPresetManager::GetBuiltInPresets() const
{
	return BuiltInPresets;
}

bool USoccerTacticalPresetManager::SaveCurrentPlayerStrategyAsNewPreset(
	const FString& RequestedName,
	FGuid& OutPresetId,
	FString& OutMessage
)
{
	OutPresetId.Invalidate();

	FString NormalizedName;
	if (!ValidateAndNormalizeName(RequestedName, NormalizedName, OutMessage))
	{
		return false;
	}

	if (IsNameUsedByAnotherPreset(NormalizedName))
	{
		OutMessage = FString::Printf(
			TEXT("Ya existe un preset llamado '%s'. Seleccionalo y usá SOBRESCRIBIR."),
			*NormalizedName
		);
		return false;
	}

	FSoccerTacticalPreset NewPreset;
	if (!CaptureCurrentPlayerStrategy(NormalizedName, NewPreset))
	{
		OutMessage = TEXT("No se pudo capturar la estrategia actual del equipo.");
		return false;
	}

	NewPreset.PresetId = FGuid::NewGuid();
	const FGuid PreviousActivePresetId = ActivePresetId;
	Presets.Add(NewPreset);
	ActivePresetId = NewPreset.PresetId;

	if (!PersistLibrary(OutMessage))
	{
		Presets.RemoveAt(Presets.Num() - 1);
		ActivePresetId = PreviousActivePresetId;
		return false;
	}

	OutPresetId = NewPreset.PresetId;
	OutMessage = FString::Printf(
		TEXT("Preset '%s' guardado en disco."),
		*NewPreset.PresetName
	);
	return true;
}

bool USoccerTacticalPresetManager::OverwritePresetWithCurrentPlayerStrategy(
	const FGuid& PresetId,
	FString& OutMessage
)
{
	FSoccerTacticalPreset* ExistingPreset = Presets.FindByPredicate(
		[&PresetId](const FSoccerTacticalPreset& Candidate)
		{
			return Candidate.PresetId == PresetId;
		}
	);

	if (ExistingPreset == nullptr)
	{
		OutMessage = TEXT("No hay un preset seleccionado para sobrescribir.");
		return false;
	}

	FSoccerTacticalPreset Replacement;
	if (!CaptureCurrentPlayerStrategy(ExistingPreset->PresetName, Replacement))
	{
		OutMessage = TEXT("No se pudo capturar la estrategia actual del equipo.");
		return false;
	}

	Replacement.PresetId = ExistingPreset->PresetId;
	const FSoccerTacticalPreset PreviousPreset = *ExistingPreset;
	const FGuid PreviousActivePresetId = ActivePresetId;
	*ExistingPreset = Replacement;
	ActivePresetId = Replacement.PresetId;

	if (!PersistLibrary(OutMessage))
	{
		*ExistingPreset = PreviousPreset;
		ActivePresetId = PreviousActivePresetId;
		return false;
	}

	OutMessage = FString::Printf(
		TEXT("Preset '%s' actualizado con la estrategia actual."),
		*Replacement.PresetName
	);
	return true;
}

bool USoccerTacticalPresetManager::RenamePreset(
	const FGuid& PresetId,
	const FString& RequestedName,
	FString& OutMessage
)
{
	FSoccerTacticalPreset* ExistingPreset = Presets.FindByPredicate(
		[&PresetId](const FSoccerTacticalPreset& Candidate)
		{
			return Candidate.PresetId == PresetId;
		}
	);

	if (ExistingPreset == nullptr)
	{
		OutMessage = TEXT("No hay un preset seleccionado para renombrar.");
		return false;
	}

	FString NormalizedName;
	if (!ValidateAndNormalizeName(RequestedName, NormalizedName, OutMessage))
	{
		return false;
	}

	if (IsNameUsedByAnotherPreset(NormalizedName, PresetId))
	{
		OutMessage = FString::Printf(
			TEXT("Ya existe otro preset llamado '%s'."),
			*NormalizedName
		);
		return false;
	}

	const FString PreviousName = ExistingPreset->PresetName;
	ExistingPreset->PresetName = NormalizedName;

	if (!PersistLibrary(OutMessage))
	{
		ExistingPreset->PresetName = PreviousName;
		return false;
	}

	OutMessage = FString::Printf(
		TEXT("Preset renombrado como '%s'."),
		*NormalizedName
	);
	return true;
}

bool USoccerTacticalPresetManager::DuplicatePreset(
	const FGuid& PresetId,
	FGuid& OutDuplicatePresetId,
	FString& OutMessage
)
{
	OutDuplicatePresetId.Invalidate();

	const FSoccerTacticalPreset* SourcePresetPtr = FindAnyPreset(PresetId);
	if (SourcePresetPtr == nullptr)
	{
		OutMessage = TEXT("No hay un preset seleccionado para duplicar.");
		return false;
	}

	const FSoccerTacticalPreset SourcePreset = *SourcePresetPtr;
	const int32 SourceUserPresetIndex = Presets.IndexOfByPredicate(
		[&PresetId](const FSoccerTacticalPreset& Candidate)
		{
			return Candidate.PresetId == PresetId;
		}
	);

	// The copy is always user-authored, even when its source is a read-only
	// built-in preset. It receives a fresh GUID and never inherits quick slots
	// or active identity from the source.
	FString DuplicateName;
	for (int32 CopyNumber = 1; CopyNumber < 100000; ++CopyNumber)
	{
		const FString Suffix =
			CopyNumber == 1
			? TEXT(" - Copia")
			: FString::Printf(TEXT(" - Copia %d"), CopyNumber);

		const int32 MaximumBaseLength = FMath::Max(
			1,
			MaximumPresetNameLength - Suffix.Len()
		);
		FString TruncatedBaseName = SourcePreset.PresetName.Left(MaximumBaseLength);
		TruncatedBaseName.TrimStartAndEndInline();
		if (TruncatedBaseName.IsEmpty())
		{
			TruncatedBaseName = TEXT("Preset");
		}

		const FString CandidateName = TruncatedBaseName + Suffix;
		if (!IsNameUsedByAnotherPreset(CandidateName))
		{
			DuplicateName = CandidateName;
			break;
		}
	}

	if (DuplicateName.IsEmpty())
	{
		OutMessage = TEXT("No se pudo generar un nombre único para la copia del preset.");
		return false;
	}

	FSoccerTacticalPreset DuplicatePreset = SourcePreset;
	DuplicatePreset.PresetId = FGuid::NewGuid();
	DuplicatePreset.PresetName = DuplicateName;

	// A user preset is inserted next to its user source. A copy of a built-in
	// template is appended to MIS PRESETS because the built-in list itself is
	// immutable and is not serialized.
	const int32 DuplicateIndex =
		SourceUserPresetIndex != INDEX_NONE
		? SourceUserPresetIndex + 1
		: Presets.Num();
	Presets.Insert(DuplicatePreset, DuplicateIndex);

	if (!PersistLibrary(OutMessage))
	{
		Presets.RemoveAt(DuplicateIndex);
		return false;
	}

	OutDuplicatePresetId = DuplicatePreset.PresetId;
	OutMessage = FString::Printf(
		TEXT("Preset '%s' duplicado como '%s' en MIS PRESETS."),
		*SourcePreset.PresetName,
		*DuplicatePreset.PresetName
	);
	return true;
}

bool USoccerTacticalPresetManager::MovePreset(
	const FGuid& PresetId,
	int32 Direction,
	FString& OutMessage
)
{
	const int32 CurrentIndex = Presets.IndexOfByPredicate(
		[&PresetId](const FSoccerTacticalPreset& Candidate)
		{
			return Candidate.PresetId == PresetId;
		}
	);

	if (CurrentIndex == INDEX_NONE)
	{
		OutMessage = TEXT("No hay un preset seleccionado para reordenar.");
		return false;
	}

	const int32 Step = Direction < 0 ? -1 : (Direction > 0 ? 1 : 0);
	if (Step == 0)
	{
		OutMessage = TEXT("No se indicó una dirección válida para mover el preset.");
		return false;
	}

	const int32 NewIndex = CurrentIndex + Step;
	if (!Presets.IsValidIndex(NewIndex))
	{
		OutMessage = Step < 0
			? TEXT("El preset ya está primero en la biblioteca.")
			: TEXT("El preset ya está último en la biblioteca.");
		return false;
	}

	const FString PresetName = Presets[CurrentIndex].PresetName;
	Presets.Swap(CurrentIndex, NewIndex);

	if (!PersistLibrary(OutMessage))
	{
		Presets.Swap(CurrentIndex, NewIndex);
		return false;
	}

	if (Step < 0)
	{
		OutMessage = FString::Printf(
			TEXT("Preset '%s' movido hacia arriba."),
			*PresetName
		);
	}
	else
	{
		OutMessage = FString::Printf(
			TEXT("Preset '%s' movido hacia abajo."),
			*PresetName
		);
	}
	return true;
}

bool USoccerTacticalPresetManager::ExportPresetToJson(
	const FGuid& PresetId,
	FString& OutExportedFileName,
	FString& OutMessage
) const
{
	OutExportedFileName.Empty();

	const FSoccerTacticalPreset* Preset = FindPreset(PresetId);
	if (Preset == nullptr)
	{
		OutMessage = TEXT("No hay un preset seleccionado para exportar.");
		return false;
	}

	if (!IsPresetDataSupported(*Preset, OutMessage))
	{
		return false;
	}

	FSoccerTacticalPresetExchangeDocument Document;
	Document.FileType = TacticalPresetJsonFileType;
	Document.ExchangeFormatVersion = CurrentJsonExchangeFormatVersion;
	Document.bContainsCompleteStrategy = true;
	Document.SourcePresetId = Preset->PresetId.ToString();
	Document.PresetName = Preset->PresetName;
	Document.DataVersion = Preset->DataVersion;
	Document.FormationSystem = Preset->FormationSystem;
	Document.TacticalPlan = Preset->TacticalPlan;
	Document.SlotInstructions = Preset->SlotInstructions;

	FString JsonString;
	if (!FJsonObjectConverter::UStructToJsonObjectString(
		Document,
		JsonString
	))
	{
		OutMessage = TEXT("No se pudo convertir el preset a JSON.");
		return false;
	}

	const FString ExchangeDirectory = GetJsonExchangeDirectory();
	if (!IFileManager::Get().MakeDirectory(*ExchangeDirectory, true))
	{
		OutMessage = TEXT("No se pudo crear la carpeta de intercambio de presets.");
		return false;
	}

	const FString SafeStem = MakeSafeJsonFileStem(Preset->PresetName);
	const FString GuidText = Preset->PresetId.ToString();
	const FString GuidPrefix = GuidText.Left(8);
	const FString FileName = FString::Printf(
		TEXT("%s_%s.json"),
		*SafeStem,
		*GuidPrefix
	);
	const FString AbsoluteFilePath = FPaths::Combine(
		ExchangeDirectory,
		FileName
	);

	if (!FFileHelper::SaveStringToFile(
		JsonString,
		*AbsoluteFilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM
	))
	{
		OutMessage = TEXT("No se pudo escribir el archivo JSON en disco.");
		return false;
	}

	OutExportedFileName = FileName;
	OutMessage = FString::Printf(
		TEXT("Preset '%s' exportado a Saved/TacticalPresets/Exchange/%s"),
		*Preset->PresetName,
		*FileName
	);
	return true;
}

bool USoccerTacticalPresetManager::ImportPresetFromJson(
	const FString& JsonFileName,
	FGuid& OutImportedPresetId,
	FString& OutMessage
)
{
	OutImportedPresetId.Invalidate();

	const FString CleanFileName = FPaths::GetCleanFilename(JsonFileName);
	if (
		CleanFileName.IsEmpty() ||
		CleanFileName != JsonFileName ||
		!FPaths::GetExtension(CleanFileName).Equals(TEXT("json"), ESearchCase::IgnoreCase)
	)
	{
		OutMessage = TEXT("El nombre del archivo JSON no es válido.");
		return false;
	}

	const FString AbsoluteFilePath = FPaths::Combine(
		GetJsonExchangeDirectory(),
		CleanFileName
	);

	const int64 FileSize = IFileManager::Get().FileSize(*AbsoluteFilePath);
	if (FileSize < 0)
	{
		OutMessage = TEXT("El archivo JSON seleccionado ya no existe.");
		return false;
	}
	if (FileSize > MaximumJsonImportBytes)
	{
		OutMessage = TEXT("El archivo JSON es demasiado grande para ser un preset táctico.");
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *AbsoluteFilePath))
	{
		OutMessage = TEXT("No se pudo leer el archivo JSON seleccionado.");
		return false;
	}

	FSoccerTacticalPresetExchangeDocument Document;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(
		JsonString,
		&Document,
		0,
		0
	))
	{
		OutMessage = TEXT("El archivo no contiene un preset JSON válido.");
		return false;
	}

	if (
		Document.FileType != TacticalPresetJsonFileType ||
		!Document.bContainsCompleteStrategy
	)
	{
		OutMessage = TEXT("El archivo JSON no identifica una estrategia táctica completa de este juego.");
		return false;
	}

	if (Document.ExchangeFormatVersion != CurrentJsonExchangeFormatVersion)
	{
		OutMessage = FString::Printf(
			TEXT("El archivo usa formato de intercambio %d; esta versión admite %d."),
			Document.ExchangeFormatVersion,
			CurrentJsonExchangeFormatVersion
		);
		return false;
	}

	FSoccerTacticalPreset ImportedPreset;
	ImportedPreset.PresetId = FGuid::NewGuid();
	ImportedPreset.PresetName = BuildUniqueImportedPresetName(Document.PresetName);
	ImportedPreset.DataVersion = Document.DataVersion;
	ImportedPreset.FormationSystem = Document.FormationSystem;
	ImportedPreset.TacticalPlan = Document.TacticalPlan;
	ImportedPreset.SlotInstructions = Document.SlotInstructions;

	if (!ValidateImportedPresetData(ImportedPreset, OutMessage))
	{
		return false;
	}

	Presets.Add(ImportedPreset);
	if (!PersistLibrary(OutMessage))
	{
		Presets.RemoveAt(Presets.Num() - 1);
		return false;
	}

	OutImportedPresetId = ImportedPreset.PresetId;
	OutMessage = FString::Printf(
		TEXT("Preset '%s' importado desde %s. No se aplicó al equipo."),
		*ImportedPreset.PresetName,
		*CleanFileName
	);
	return true;
}

void USoccerTacticalPresetManager::GetAvailableJsonExchangeFiles(
	TArray<FString>& OutFileNames
) const
{
	OutFileNames.Empty();

	const FString ExchangeDirectory = GetJsonExchangeDirectory();
	IFileManager::Get().MakeDirectory(*ExchangeDirectory, true);

	const FString SearchPattern = FPaths::Combine(
		ExchangeDirectory,
		TEXT("*.json")
	);
	IFileManager::Get().FindFiles(
		OutFileNames,
		*SearchPattern,
		true,
		false
	);

	OutFileNames.Sort(
		[](const FString& A, const FString& B)
		{
			return A < B;
		}
	);
}

bool USoccerTacticalPresetManager::DeletePreset(
	const FGuid& PresetId,
	FString& OutMessage
)
{
	const int32 PresetIndex = Presets.IndexOfByPredicate(
		[&PresetId](const FSoccerTacticalPreset& Candidate)
		{
			return Candidate.PresetId == PresetId;
		}
	);

	if (PresetIndex == INDEX_NONE)
	{
		OutMessage = TEXT("No hay un preset seleccionado para eliminar.");
		return false;
	}

	const FSoccerTacticalPreset RemovedPreset = Presets[PresetIndex];
	const FGuid PreviousActivePresetId = ActivePresetId;
	const TArray<FGuid> PreviousQuickPresetSlotIds = QuickPresetSlotIds;
	Presets.RemoveAt(PresetIndex);

	if (ActivePresetId == PresetId)
	{
		ActivePresetId.Invalidate();
	}

	for (FGuid& QuickPresetId : QuickPresetSlotIds)
	{
		if (QuickPresetId == PresetId)
		{
			QuickPresetId.Invalidate();
		}
	}

	if (!PersistLibrary(OutMessage))
	{
		Presets.Insert(RemovedPreset, PresetIndex);
		ActivePresetId = PreviousActivePresetId;
		QuickPresetSlotIds = PreviousQuickPresetSlotIds;
		return false;
	}

	OutMessage = FString::Printf(
		TEXT("Preset '%s' eliminado."),
		*RemovedPreset.PresetName
	);
	return true;
}

bool USoccerTacticalPresetManager::ApplyPresetToPlayerTeam(
	const FGuid& PresetId,
	FString& OutMessage
)
{
	if (!IsValid(MatchManager))
	{
		OutMessage = TEXT("No hay MatchManager disponible para aplicar el preset.");
		return false;
	}

	const FSoccerTacticalPreset* Preset = FindAnyPreset(PresetId);
	if (Preset == nullptr)
	{
		OutMessage = TEXT("No hay un preset seleccionado para aplicar.");
		return false;
	}

	if (!IsPresetDataSupported(*Preset, OutMessage))
	{
		return false;
	}

	// Order matters: formation defines the valid slot set; collective tactics are
	// independent; then the complete instruction snapshot replaces old slot data.
	MatchManager->SetFormationSystemForTeam(
		ESoccerTeam::PlayerTeam,
		Preset->FormationSystem
	);
	MatchManager->SetTacticalPlanForTeam(
		ESoccerTeam::PlayerTeam,
		Preset->TacticalPlan
	);
	MatchManager->SetSlotTacticalInstructionsForTeam(
		ESoccerTeam::PlayerTeam,
		Preset->SlotInstructions
	);

	ActivePresetId = Preset->PresetId;
	OutMessage = FString::Printf(
		TEXT("Preset '%s' aplicado."),
		*Preset->PresetName
	);
	return true;
}

bool USoccerTacticalPresetManager::AssignPresetToQuickSlot(
	int32 QuickSlotIndex,
	const FGuid& PresetId,
	FString& OutMessage
)
{
	if (
		QuickSlotIndex < 0 ||
		QuickSlotIndex >= QuickPresetSlotCount
	)
	{
		OutMessage = TEXT("El acceso rápido seleccionado no es válido.");
		return false;
	}

	const FSoccerTacticalPreset* Preset = FindPreset(PresetId);
	if (Preset == nullptr)
	{
		OutMessage = TEXT("No hay un preset válido seleccionado para asignar.");
		return false;
	}

	if (QuickPresetSlotIds.Num() != QuickPresetSlotCount)
	{
		QuickPresetSlotIds.SetNum(QuickPresetSlotCount);
	}

	const TArray<FGuid> PreviousQuickPresetSlotIds = QuickPresetSlotIds;

	// Keep choices unique. Moving a preset to another slot automatically clears
	// its previous assignment instead of duplicating the same strategy twice.
	for (int32 Index = 0; Index < QuickPresetSlotIds.Num(); ++Index)
	{
		if (Index != QuickSlotIndex && QuickPresetSlotIds[Index] == PresetId)
		{
			QuickPresetSlotIds[Index].Invalidate();
		}
	}
	QuickPresetSlotIds[QuickSlotIndex] = PresetId;

	if (!PersistLibrary(OutMessage))
	{
		QuickPresetSlotIds = PreviousQuickPresetSlotIds;
		return false;
	}

	OutMessage = FString::Printf(
		TEXT("Acceso rápido %d asignado a '%s'."),
		QuickSlotIndex + 1,
		*Preset->PresetName
	);
	return true;
}

bool USoccerTacticalPresetManager::ClearQuickPresetSlot(
	int32 QuickSlotIndex,
	FString& OutMessage
)
{
	if (
		QuickSlotIndex < 0 ||
		QuickSlotIndex >= QuickPresetSlotCount
	)
	{
		OutMessage = TEXT("El acceso rápido seleccionado no es válido.");
		return false;
	}

	if (QuickPresetSlotIds.Num() != QuickPresetSlotCount)
	{
		QuickPresetSlotIds.SetNum(QuickPresetSlotCount);
	}

	if (!QuickPresetSlotIds[QuickSlotIndex].IsValid())
	{
		OutMessage = FString::Printf(
			TEXT("El acceso rápido %d ya está vacío."),
			QuickSlotIndex + 1
		);
		return true;
	}

	const FGuid PreviousPresetId = QuickPresetSlotIds[QuickSlotIndex];
	QuickPresetSlotIds[QuickSlotIndex].Invalidate();

	if (!PersistLibrary(OutMessage))
	{
		QuickPresetSlotIds[QuickSlotIndex] = PreviousPresetId;
		return false;
	}

	OutMessage = FString::Printf(
		TEXT("Acceso rápido %d liberado."),
		QuickSlotIndex + 1
	);
	return true;
}

bool USoccerTacticalPresetManager::ApplyQuickPresetSlotToPlayerTeam(
	int32 QuickSlotIndex,
	FString& OutMessage
)
{
	const FGuid PresetId = GetQuickPresetIdForSlot(QuickSlotIndex);
	if (!PresetId.IsValid())
	{
		OutMessage = FString::Printf(
			TEXT("El acceso rápido %d está vacío."),
			QuickSlotIndex + 1
		);
		return false;
	}

	return ApplyPresetToPlayerTeam(PresetId, OutMessage);
}

FGuid USoccerTacticalPresetManager::GetQuickPresetIdForSlot(
	int32 QuickSlotIndex
) const
{
	if (
		QuickSlotIndex < 0 ||
		QuickSlotIndex >= QuickPresetSlotIds.Num()
	)
	{
		return FGuid();
	}

	return QuickPresetSlotIds[QuickSlotIndex];
}

const FSoccerTacticalPreset*
USoccerTacticalPresetManager::GetQuickPresetForSlot(
	int32 QuickSlotIndex
) const
{
	return FindPreset(GetQuickPresetIdForSlot(QuickSlotIndex));
}

int32 USoccerTacticalPresetManager::GetQuickPresetSlotCount()
{
	return QuickPresetSlotCount;
}

bool USoccerTacticalPresetManager::ReloadFromDisk(FString& OutMessage)
{
	if (!UGameplayStatics::DoesSaveGameExist(
		TacticalPresetSaveSlotName,
		TacticalPresetSaveUserIndex
	))
	{
		Presets.Empty();
		QuickPresetSlotIds.SetNum(QuickPresetSlotCount);
		for (FGuid& QuickPresetId : QuickPresetSlotIds)
		{
			QuickPresetId.Invalidate();
		}
		ActivePresetId.Invalidate();
		OutMessage = TEXT("Todavía no hay presets guardados.");
		return true;
	}

	USaveGame* LoadedBase = UGameplayStatics::LoadGameFromSlot(
		TacticalPresetSaveSlotName,
		TacticalPresetSaveUserIndex
	);
	USoccerTacticalPresetSaveGame* LoadedSave =
		Cast<USoccerTacticalPresetSaveGame>(LoadedBase);

	if (LoadedSave == nullptr)
	{
		OutMessage = TEXT("El archivo local de presets no tiene un formato válido.");
		return false;
	}

	if (LoadedSave->SaveFormatVersion > CurrentSaveFormatVersion)
	{
		OutMessage = TEXT("Los presets fueron creados por una versión más nueva del juego.");
		return false;
	}

	TArray<FSoccerTacticalPreset> LoadedPresets = LoadedSave->Presets;
	TSet<FGuid> UsedIds;
	for (const FSoccerTacticalPreset& BuiltInPreset : BuiltInPresets)
	{
		UsedIds.Add(BuiltInPreset.PresetId);
	}
	TSet<FString> UsedNamesLower;

	for (int32 Index = 0; Index < LoadedPresets.Num(); ++Index)
	{
		FSoccerTacticalPreset& Preset = LoadedPresets[Index];

		if (!Preset.PresetId.IsValid() || UsedIds.Contains(Preset.PresetId))
		{
			Preset.PresetId = FGuid::NewGuid();
		}
		UsedIds.Add(Preset.PresetId);

		Preset.PresetName.TrimStartAndEndInline();
		if (Preset.PresetName.IsEmpty())
		{
			Preset.PresetName = FString::Printf(TEXT("Preset %d"), Index + 1);
		}

		if (Preset.PresetName.Len() > MaximumPresetNameLength)
		{
			Preset.PresetName.LeftInline(MaximumPresetNameLength, false);
		}

		FString CandidateName = Preset.PresetName;
		FString CandidateLower = CandidateName.ToLower();
		int32 Suffix = 2;
		while (UsedNamesLower.Contains(CandidateLower))
		{
			const FString SuffixText = FString::Printf(TEXT(" (%d)"), Suffix++);
			const int32 BaseLength = FMath::Max(
				1,
				MaximumPresetNameLength - SuffixText.Len()
			);
			CandidateName = Preset.PresetName.Left(BaseLength) + SuffixText;
			CandidateLower = CandidateName.ToLower();
		}
		Preset.PresetName = CandidateName;
		UsedNamesLower.Add(CandidateLower);

		if (Preset.DataVersion <= 0)
		{
			Preset.DataVersion = 1;
		}
	}

	Presets = MoveTemp(LoadedPresets);

	// Save-format v1 had no quick slots. Loading it naturally yields an empty
	// array, which is expanded here to four invalid IDs without touching presets.
	QuickPresetSlotIds = LoadedSave->QuickPresetSlotIds;
	QuickPresetSlotIds.SetNum(QuickPresetSlotCount);

	TSet<FGuid> UsedQuickPresetIds;
	for (FGuid& QuickPresetId : QuickPresetSlotIds)
	{
		if (
			!QuickPresetId.IsValid() ||
			FindPreset(QuickPresetId) == nullptr ||
			UsedQuickPresetIds.Contains(QuickPresetId)
		)
		{
			QuickPresetId.Invalidate();
			continue;
		}

		UsedQuickPresetIds.Add(QuickPresetId);
	}

	if (ActivePresetId.IsValid() && FindAnyPreset(ActivePresetId) == nullptr)
	{
		ActivePresetId.Invalidate();
	}

	OutMessage = FString::Printf(
		TEXT("%d preset(s) cargado(s) desde disco."),
		Presets.Num()
	);
	return true;
}

const FSoccerTacticalPreset* USoccerTacticalPresetManager::FindPreset(
	const FGuid& PresetId
) const
{
	if (!PresetId.IsValid())
	{
		return nullptr;
	}

	return Presets.FindByPredicate(
		[&PresetId](const FSoccerTacticalPreset& Candidate)
		{
			return Candidate.PresetId == PresetId;
		}
	);
}

const FSoccerTacticalPreset* USoccerTacticalPresetManager::FindBuiltInPreset(
	const FGuid& PresetId
) const
{
	if (!PresetId.IsValid())
	{
		return nullptr;
	}

	return BuiltInPresets.FindByPredicate(
		[&PresetId](const FSoccerTacticalPreset& Candidate)
		{
			return Candidate.PresetId == PresetId;
		}
	);
}

const FSoccerTacticalPreset* USoccerTacticalPresetManager::FindAnyPreset(
	const FGuid& PresetId
) const
{
	const FSoccerTacticalPreset* UserPreset = FindPreset(PresetId);
	return UserPreset != nullptr ? UserPreset : FindBuiltInPreset(PresetId);
}

bool USoccerTacticalPresetManager::IsBuiltInPreset(
	const FGuid& PresetId
) const
{
	return FindBuiltInPreset(PresetId) != nullptr;
}

FGuid USoccerTacticalPresetManager::GetActivePresetId() const
{
	return ActivePresetId;
}

bool USoccerTacticalPresetManager::IsActivePresetModified() const
{
	const FSoccerTacticalPreset* ActivePreset = FindAnyPreset(ActivePresetId);
	return
		ActivePreset != nullptr &&
		!DoesPresetMatchCurrentPlayerStrategy(*ActivePreset);
}

FString USoccerTacticalPresetManager::GetLocalSaveSlotName()
{
	return TacticalPresetSaveSlotName;
}

FString USoccerTacticalPresetManager::GetJsonExchangeDirectory()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("TacticalPresets"),
		TEXT("Exchange")
	);
}

int32 USoccerTacticalPresetManager::GetCurrentPresetDataVersion()
{
	return CurrentPresetDataVersion;
}

int32 USoccerTacticalPresetManager::GetCurrentSaveFormatVersion()
{
	return CurrentSaveFormatVersion;
}

void USoccerTacticalPresetManager::BuildBuiltInPresets()
{
	BuiltInPresets.Reset();
	BuiltInPresets.Reserve(5);

	auto BuildDefaultInstructions = [](
		ESoccerFormationSystem FormationSystem
	)
	{
		TArray<FSoccerSlotTacticalInstruction> Instructions;
		const FSoccerFormationDefinition& Definition =
			SoccerFormationLibrary::GetDefinition(FormationSystem);
		Instructions.Reserve(Definition.Slots.Num());

		for (const FSoccerFormationSlot& FormationSlot : Definition.Slots)
		{
			FSoccerSlotTacticalInstruction Instruction;
			Instruction.SlotId = FormationSlot.SlotId;
			Instructions.Add(Instruction);
		}
		return Instructions;
	};

	auto ConfigureInstruction = [](
		TArray<FSoccerSlotTacticalInstruction>& Instructions,
		const TCHAR* SlotId,
		ESoccerIndividualAttackInstruction AttackInstruction,
		ESoccerIndividualDefensiveInstruction DefensiveInstruction
	)
	{
		FSoccerSlotTacticalInstruction* Instruction = Instructions.FindByPredicate(
			[SlotId](const FSoccerSlotTacticalInstruction& Candidate)
			{
				return Candidate.SlotId == FName(SlotId);
			}
		);
		if (Instruction != nullptr)
		{
			Instruction->AttackInstruction = AttackInstruction;
			Instruction->DefensiveInstruction = DefensiveInstruction;
		}
	};

	auto AddBuiltIn = [this, &BuildDefaultInstructions](
		const FGuid& PresetId,
		const TCHAR* PresetName,
		ESoccerFormationSystem FormationSystem,
		const FSoccerTeamTacticalPlan& TacticalPlan,
		TArray<FSoccerSlotTacticalInstruction> Instructions
	)
	{
		FSoccerTacticalPreset Preset;
		Preset.PresetId = PresetId;
		Preset.PresetName = PresetName;
		Preset.DataVersion = CurrentPresetDataVersion;
		Preset.FormationSystem = FormationSystem;
		Preset.TacticalPlan = TacticalPlan;
		if (Instructions.Num() <= 0)
		{
			Instructions = BuildDefaultInstructions(FormationSystem);
		}
		Preset.SlotInstructions = MoveTemp(Instructions);
		BuiltInPresets.Add(MoveTemp(Preset));
	};

	// 1) Neutral reference strategy. Intentionally contains no individual
	// overrides so it is also a clean starting point for DUPLICAR.
	{
		FSoccerTeamTacticalPlan Plan;
		AddBuiltIn(
			FGuid(0x160F0001, 0xA1100001, 0x20260823, 0x00000001),
			TEXT("Equilibrado"),
			ESoccerFormationSystem::OneThreeTwoOne,
			Plan,
			BuildDefaultInstructions(ESoccerFormationSystem::OneThreeTwoOne)
		);
	}

	// 2) Aggressive high block with three attackers and immediate counterpress.
	{
		FSoccerTeamTacticalPlan Plan;
		Plan.BuildUpStyle = ESoccerBuildUpStyle::Direct;
		Plan.AttackChannel = ESoccerAttackChannel::Balanced;
		Plan.AttackingWidth = ESoccerAttackingWidth::Wide;
		Plan.AttackingTempo = ESoccerAttackingTempo::Fast;
		Plan.AttackingTransition = ESoccerAttackingTransition::CounterAttack;
		Plan.DefensiveBlock = ESoccerDefensiveBlock::High;
		Plan.PressingIntensity = ESoccerPressingIntensity::High;
		Plan.MarkingStyle = ESoccerMarkingStyle::Mixed;
		Plan.DefensiveTransition = ESoccerDefensiveTransition::CounterPress;

		TArray<FSoccerSlotTacticalInstruction> Instructions =
			BuildDefaultInstructions(ESoccerFormationSystem::OneTwoOneThree);
		ConfigureInstruction(Instructions, TEXT("DEF_L"), ESoccerIndividualAttackInstruction::HoldPosition, ESoccerIndividualDefensiveInstruction::Cover);
		ConfigureInstruction(Instructions, TEXT("DEF_R"), ESoccerIndividualAttackInstruction::HoldPosition, ESoccerIndividualDefensiveInstruction::Cover);
		ConfigureInstruction(Instructions, TEXT("MID_C"), ESoccerIndividualAttackInstruction::LinkPlay, ESoccerIndividualDefensiveInstruction::ProtectCenter);
		ConfigureInstruction(Instructions, TEXT("FWD_L"), ESoccerIndividualAttackInstruction::StayWide, ESoccerIndividualDefensiveInstruction::PressBall);
		ConfigureInstruction(Instructions, TEXT("FWD_C"), ESoccerIndividualAttackInstruction::MakeForwardRuns, ESoccerIndividualDefensiveInstruction::PressBall);
		ConfigureInstruction(Instructions, TEXT("FWD_R"), ESoccerIndividualAttackInstruction::StayWide, ESoccerIndividualDefensiveInstruction::PressBall);
		AddBuiltIn(
			FGuid(0x160F0002, 0xA1100002, 0x20260823, 0x00000002),
			TEXT("Presión alta"),
			ESoccerFormationSystem::OneTwoOneThree,
			Plan,
			MoveTemp(Instructions)
		);
	}

	// 3) Compact defending with direct vertical exits into two forwards.
	{
		FSoccerTeamTacticalPlan Plan;
		Plan.BuildUpStyle = ESoccerBuildUpStyle::Direct;
		Plan.AttackChannel = ESoccerAttackChannel::Balanced;
		Plan.AttackingWidth = ESoccerAttackingWidth::Balanced;
		Plan.AttackingTempo = ESoccerAttackingTempo::Fast;
		Plan.AttackingTransition = ESoccerAttackingTransition::CounterAttack;
		Plan.DefensiveBlock = ESoccerDefensiveBlock::Low;
		Plan.PressingIntensity = ESoccerPressingIntensity::Medium;
		Plan.MarkingStyle = ESoccerMarkingStyle::Zonal;
		Plan.DefensiveTransition = ESoccerDefensiveTransition::Regroup;

		TArray<FSoccerSlotTacticalInstruction> Instructions =
			BuildDefaultInstructions(ESoccerFormationSystem::OneThreeOneTwo);
		ConfigureInstruction(Instructions, TEXT("DEF_L"), ESoccerIndividualAttackInstruction::HoldPosition, ESoccerIndividualDefensiveInstruction::Cover);
		ConfigureInstruction(Instructions, TEXT("DEF_C"), ESoccerIndividualAttackInstruction::HoldPosition, ESoccerIndividualDefensiveInstruction::ProtectCenter);
		ConfigureInstruction(Instructions, TEXT("DEF_R"), ESoccerIndividualAttackInstruction::HoldPosition, ESoccerIndividualDefensiveInstruction::Cover);
		ConfigureInstruction(Instructions, TEXT("MID_C"), ESoccerIndividualAttackInstruction::LinkPlay, ESoccerIndividualDefensiveInstruction::ProtectCenter);
		ConfigureInstruction(Instructions, TEXT("FWD_L"), ESoccerIndividualAttackInstruction::MakeForwardRuns, ESoccerIndividualDefensiveInstruction::Balanced);
		ConfigureInstruction(Instructions, TEXT("FWD_R"), ESoccerIndividualAttackInstruction::MakeForwardRuns, ESoccerIndividualDefensiveInstruction::Balanced);
		AddBuiltIn(
			FGuid(0x160F0003, 0xA1100003, 0x20260823, 0x00000003),
			TEXT("Contraataque"),
			ESoccerFormationSystem::OneThreeOneTwo,
			Plan,
			MoveTemp(Instructions)
		);
	}

	// 4) Protect a lead: five defenders, low block and patient possession.
	{
		FSoccerTeamTacticalPlan Plan;
		Plan.BuildUpStyle = ESoccerBuildUpStyle::ShortPossession;
		Plan.AttackChannel = ESoccerAttackChannel::Balanced;
		Plan.AttackingWidth = ESoccerAttackingWidth::Narrow;
		Plan.AttackingTempo = ESoccerAttackingTempo::Patient;
		Plan.AttackingTransition = ESoccerAttackingTransition::RetainPossession;
		Plan.DefensiveBlock = ESoccerDefensiveBlock::Low;
		Plan.PressingIntensity = ESoccerPressingIntensity::Low;
		Plan.MarkingStyle = ESoccerMarkingStyle::Zonal;
		Plan.DefensiveTransition = ESoccerDefensiveTransition::Regroup;

		TArray<FSoccerSlotTacticalInstruction> Instructions =
			BuildDefaultInstructions(ESoccerFormationSystem::OneFiveOne);
		for (FSoccerSlotTacticalInstruction& Instruction : Instructions)
		{
			if (Instruction.SlotId != FName(TEXT("GK")) && Instruction.SlotId != FName(TEXT("FWD_C")))
			{
				Instruction.AttackInstruction = ESoccerIndividualAttackInstruction::HoldPosition;
				Instruction.DefensiveInstruction = ESoccerIndividualDefensiveInstruction::Cover;
			}
		}
		ConfigureInstruction(Instructions, TEXT("DEF_C"), ESoccerIndividualAttackInstruction::HoldPosition, ESoccerIndividualDefensiveInstruction::ProtectCenter);
		ConfigureInstruction(Instructions, TEXT("FWD_C"), ESoccerIndividualAttackInstruction::ComeShort, ESoccerIndividualDefensiveInstruction::Balanced);
		AddBuiltIn(
			FGuid(0x160F0004, 0xA1100004, 0x20260823, 0x00000004),
			TEXT("Defender resultado"),
			ESoccerFormationSystem::OneFiveOne,
			Plan,
			MoveTemp(Instructions)
		);
	}

	// 5) Maximum attacking intent. Defensive values are also aggressive so the
	// team tries to recover immediately instead of retreating after losing it.
	{
		FSoccerTeamTacticalPlan Plan;
		Plan.BuildUpStyle = ESoccerBuildUpStyle::Direct;
		Plan.AttackChannel = ESoccerAttackChannel::Balanced;
		Plan.AttackingWidth = ESoccerAttackingWidth::Wide;
		Plan.AttackingTempo = ESoccerAttackingTempo::Fast;
		Plan.AttackingTransition = ESoccerAttackingTransition::CounterAttack;
		Plan.DefensiveBlock = ESoccerDefensiveBlock::High;
		Plan.PressingIntensity = ESoccerPressingIntensity::High;
		Plan.MarkingStyle = ESoccerMarkingStyle::ManToMan;
		Plan.DefensiveTransition = ESoccerDefensiveTransition::CounterPress;

		TArray<FSoccerSlotTacticalInstruction> Instructions =
			BuildDefaultInstructions(ESoccerFormationSystem::OneTwoOneThree);
		ConfigureInstruction(Instructions, TEXT("DEF_L"), ESoccerIndividualAttackInstruction::Balanced, ESoccerIndividualDefensiveInstruction::PressBall);
		ConfigureInstruction(Instructions, TEXT("DEF_R"), ESoccerIndividualAttackInstruction::Balanced, ESoccerIndividualDefensiveInstruction::PressBall);
		ConfigureInstruction(Instructions, TEXT("MID_C"), ESoccerIndividualAttackInstruction::MakeForwardRuns, ESoccerIndividualDefensiveInstruction::PressBall);
		ConfigureInstruction(Instructions, TEXT("FWD_L"), ESoccerIndividualAttackInstruction::StayWide, ESoccerIndividualDefensiveInstruction::PressBall);
		ConfigureInstruction(Instructions, TEXT("FWD_C"), ESoccerIndividualAttackInstruction::MakeForwardRuns, ESoccerIndividualDefensiveInstruction::PressBall);
		ConfigureInstruction(Instructions, TEXT("FWD_R"), ESoccerIndividualAttackInstruction::StayWide, ESoccerIndividualDefensiveInstruction::PressBall);
		AddBuiltIn(
			FGuid(0x160F0005, 0xA1100005, 0x20260823, 0x00000005),
			TEXT("Todo al ataque"),
			ESoccerFormationSystem::OneTwoOneThree,
			Plan,
			MoveTemp(Instructions)
		);
	}
}

bool USoccerTacticalPresetManager::CaptureCurrentPlayerStrategy(
	const FString& PresetName,
	FSoccerTacticalPreset& OutPreset
) const
{
	if (!IsValid(MatchManager))
	{
		return false;
	}

	OutPreset.PresetName = PresetName;
	OutPreset.DataVersion = CurrentPresetDataVersion;
	OutPreset.FormationSystem =
		MatchManager->GetFormationSystemForTeam(ESoccerTeam::PlayerTeam);
	OutPreset.TacticalPlan =
		MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	OutPreset.SlotInstructions =
		MatchManager->GetSlotTacticalInstructionsForTeam(ESoccerTeam::PlayerTeam);
	return true;
}

bool USoccerTacticalPresetManager::PersistLibrary(FString& OutMessage) const
{
	USoccerTacticalPresetSaveGame* SaveObject = Cast<USoccerTacticalPresetSaveGame>(
		UGameplayStatics::CreateSaveGameObject(
			USoccerTacticalPresetSaveGame::StaticClass()
		)
	);

	if (SaveObject == nullptr)
	{
		OutMessage = TEXT("No se pudo crear el contenedor local de presets.");
		return false;
	}

	SaveObject->SaveFormatVersion = CurrentSaveFormatVersion;
	SaveObject->Presets = Presets;
	SaveObject->QuickPresetSlotIds = QuickPresetSlotIds;

	if (!UGameplayStatics::SaveGameToSlot(
		SaveObject,
		TacticalPresetSaveSlotName,
		TacticalPresetSaveUserIndex
	))
	{
		OutMessage = TEXT("No se pudo escribir el archivo local de presets en disco.");
		return false;
	}

	return true;
}

bool USoccerTacticalPresetManager::ValidateAndNormalizeName(
	const FString& RequestedName,
	FString& OutNormalizedName,
	FString& OutMessage
) const
{
	OutNormalizedName = RequestedName;
	OutNormalizedName.TrimStartAndEndInline();

	if (OutNormalizedName.IsEmpty())
	{
		OutMessage = TEXT("Escribí un nombre para el preset.");
		return false;
	}

	if (OutNormalizedName.Len() > MaximumPresetNameLength)
	{
		OutMessage = FString::Printf(
			TEXT("El nombre puede tener como máximo %d caracteres."),
			MaximumPresetNameLength
		);
		return false;
	}

	return true;
}

bool USoccerTacticalPresetManager::IsNameUsedByAnotherPreset(
	const FString& Name,
	const FGuid& IgnoredPresetId
) const
{
	for (const FSoccerTacticalPreset& Preset : Presets)
	{
		if (
			Preset.PresetId != IgnoredPresetId &&
			Preset.PresetName.Equals(Name, ESearchCase::IgnoreCase)
		)
		{
			return true;
		}
	}

	for (const FSoccerTacticalPreset& Preset : BuiltInPresets)
	{
		if (
			Preset.PresetId != IgnoredPresetId &&
			Preset.PresetName.Equals(Name, ESearchCase::IgnoreCase)
		)
		{
			return true;
		}
	}

	return false;
}

bool USoccerTacticalPresetManager::IsPresetDataSupported(
	const FSoccerTacticalPreset& Preset,
	FString& OutMessage
) const
{
	if (Preset.DataVersion > CurrentPresetDataVersion)
	{
		OutMessage = FString::Printf(
			TEXT("El preset '%s' usa una versión de datos más nueva."),
			*Preset.PresetName
		);
		return false;
	}

	if (!IsKnownFormationSystem(Preset.FormationSystem))
	{
		OutMessage = FString::Printf(
			TEXT("El preset '%s' contiene una formación desconocida."),
			*Preset.PresetName
		);
		return false;
	}

	const FSoccerFormationDefinition& Definition =
		SoccerFormationLibrary::GetDefinition(Preset.FormationSystem);
	if (!SoccerFormationLibrary::IsValidSevenASideDefinition(Definition))
	{
		OutMessage = FString::Printf(
			TEXT("El preset '%s' contiene una formación inválida."),
			*Preset.PresetName
		);
		return false;
	}

	return true;
}

bool USoccerTacticalPresetManager::ValidateImportedPresetData(
	const FSoccerTacticalPreset& Preset,
	FString& OutMessage
) const
{
	if (Preset.DataVersion <= 0)
	{
		OutMessage = TEXT("El preset importado no declara una versión de datos válida.");
		return false;
	}

	if (!IsPresetDataSupported(Preset, OutMessage))
	{
		return false;
	}

	const FSoccerTeamTacticalPlan& Plan = Preset.TacticalPlan;
	if (
		!IsKnownBuildUpStyle(Plan.BuildUpStyle) ||
		!IsKnownAttackChannel(Plan.AttackChannel) ||
		!IsKnownAttackingWidth(Plan.AttackingWidth) ||
		!IsKnownAttackingTempo(Plan.AttackingTempo) ||
		!IsKnownAttackingTransition(Plan.AttackingTransition) ||
		!IsKnownDefensiveBlock(Plan.DefensiveBlock) ||
		!IsKnownPressingIntensity(Plan.PressingIntensity) ||
		!IsKnownMarkingStyle(Plan.MarkingStyle) ||
		!IsKnownDefensiveTransition(Plan.DefensiveTransition)
	)
	{
		OutMessage = TEXT("El preset importado contiene un valor de táctica colectiva desconocido.");
		return false;
	}

	const FSoccerFormationDefinition& Definition =
		SoccerFormationLibrary::GetDefinition(Preset.FormationSystem);
	TSet<FName> ValidSlotIds;
	for (const FSoccerFormationSlot& FormationSlot : Definition.Slots)
	{
		ValidSlotIds.Add(FormationSlot.SlotId);
	}

	TSet<FName> ImportedSlotIds;
	for (const FSoccerSlotTacticalInstruction& Instruction : Preset.SlotInstructions)
	{
		if (Instruction.SlotId.IsNone() || !ValidSlotIds.Contains(Instruction.SlotId))
		{
			OutMessage = FString::Printf(
				TEXT("El preset importado contiene una instrucción para el puesto desconocido '%s'."),
				*Instruction.SlotId.ToString()
			);
			return false;
		}

		if (ImportedSlotIds.Contains(Instruction.SlotId))
		{
			OutMessage = FString::Printf(
				TEXT("El preset importado repite las instrucciones del puesto '%s'."),
				*Instruction.SlotId.ToString()
			);
			return false;
		}
		ImportedSlotIds.Add(Instruction.SlotId);

		if (
			!IsKnownIndividualAttackInstruction(Instruction.AttackInstruction) ||
			!IsKnownIndividualDefensiveInstruction(Instruction.DefensiveInstruction)
		)
		{
			OutMessage = FString::Printf(
				TEXT("El preset importado contiene una instrucción desconocida para '%s'."),
				*Instruction.SlotId.ToString()
			);
			return false;
		}
	}

	return true;
}

FString USoccerTacticalPresetManager::BuildUniqueImportedPresetName(
	const FString& RequestedName
) const
{
	FString BaseName = RequestedName;
	BaseName.TrimStartAndEndInline();
	if (BaseName.IsEmpty())
	{
		BaseName = TEXT("Preset importado");
	}
	if (BaseName.Len() > MaximumPresetNameLength)
	{
		BaseName.LeftInline(MaximumPresetNameLength, false);
	}

	if (!IsNameUsedByAnotherPreset(BaseName))
	{
		return BaseName;
	}

	for (int32 ImportNumber = 1; ImportNumber < 100000; ++ImportNumber)
	{
		const FString Suffix = ImportNumber == 1
			? TEXT(" - Importado")
			: FString::Printf(TEXT(" - Importado %d"), ImportNumber);
		const int32 MaximumBaseLength = FMath::Max(
			1,
			MaximumPresetNameLength - Suffix.Len()
		);
		FString CandidateBase = BaseName.Left(MaximumBaseLength);
		CandidateBase.TrimStartAndEndInline();
		if (CandidateBase.IsEmpty())
		{
			CandidateBase = TEXT("Preset");
		}

		const FString Candidate = CandidateBase + Suffix;
		if (!IsNameUsedByAnotherPreset(Candidate))
		{
			return Candidate;
		}
	}

	return FString::Printf(TEXT("Preset importado %d"), Presets.Num() + 1);
}

bool USoccerTacticalPresetManager::DoesPresetMatchCurrentPlayerStrategy(
	const FSoccerTacticalPreset& Preset
) const
{
	if (!IsValid(MatchManager))
	{
		return false;
	}

	if (
		MatchManager->GetFormationSystemForTeam(ESoccerTeam::PlayerTeam) !=
		Preset.FormationSystem
	)
	{
		return false;
	}

	if (!AreTacticalPlansEqual(
		MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam),
		Preset.TacticalPlan
	))
	{
		return false;
	}

	return AreInstructionSetsEqual(
		MatchManager->GetSlotTacticalInstructionsForTeam(ESoccerTeam::PlayerTeam),
		Preset.SlotInstructions
	);
}
