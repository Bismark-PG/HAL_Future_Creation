#include "HALVehicleStatusFXCatalog.h"
#include "HALVehicleStatusFXDefinition.h"
#include "HALGameplayTags.h"
#include "Misc/DataValidation.h"

bool UHALVehicleStatusFXCatalog::Validate(FString& Error) const
{
	Error.Reset();
	if (Effects.IsEmpty()) { Error = TEXT("Status FX catalog must contain at least one visual effect."); return false; }
	const FGameplayTag Root = FGameplayTag::RequestGameplayTag(TEXT("State.Visual"), false);
	TSet<FGameplayTag> Tags;
	for (int32 Index = 0; Index < Effects.Num(); ++Index)
	{
		const auto& Entry = Effects[Index];
		if (!Entry.Tag.IsValid() || !Root.IsValid() || Entry.Tag == Root || !Entry.Tag.MatchesTag(Root) || Tags.Contains(Entry.Tag))
		{ Error = FString::Printf(TEXT("Entry %d: '%s' must be a unique child of State.Visual."), Index, *Entry.Tag.ToString()); return false; }
		Tags.Add(Entry.Tag);
		FString Detail;
		if (!IsValid(Entry.Definition) || !Entry.Definition->Validate(Detail))
		{ Error = FString::Printf(TEXT("Entry %d [%s], definition %s: %s"), Index, *Entry.Tag.ToString(), *GetPathNameSafe(Entry.Definition), Detail.IsEmpty() ? TEXT("missing definition") : *Detail); return false; }
		if (Entry.Definition->bUseSharedShockLook && Entry.Tag != HALGameplayTags::State_Visual_Shocked)
		{ Error = TEXT("Shared Shock Look belongs to the exact State.Visual.Shocked entry."); return false; }
	}
	return true;
}

const UHALVehicleStatusFXDefinition* UHALVehicleStatusFXCatalog::FindExact(FGameplayTag Tag) const
{
	const UHALVehicleStatusFXDefinition* Result = nullptr;
	for (const auto& Entry : Effects)
	{
		if (Entry.Tag != Tag) { continue; }
		if (Result || !Entry.Tag.IsValid() || !IsValid(Entry.Definition)) { return nullptr; }
		Result = Entry.Definition;
	}
	return Result;
}

#if WITH_EDITOR
EDataValidationResult UHALVehicleStatusFXCatalog::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}
#endif
