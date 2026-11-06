#include "HALVehicleStatusFXDefinition.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Misc/DataValidation.h"

bool UHALVehicleStatusFXDefinition::Validate(FString& Error) const
{
	Error.Reset();
	const auto InRange = [](float V, float Max) { return FMath::IsFinite(V) && V >= 0 && V <= Max; };
	if (!InRange(FadeInSeconds,10) || !InRange(FadeOutSeconds,10) || !InRange(ResidueSeconds,30)
		|| SurfacePriority < -1000 || SurfacePriority > 1000)
	{ Error = TEXT("Timing must be finite: fades 0..10s, residue 0..30s; surface priority -1000..1000."); return false; }
	if (!OverlayMaterial && NiagaraLayers.IsEmpty())
	{ Error = TEXT("Provide an Overlay Material or at least one valid Niagara layer."); return false; }
	if (!OverlayMaterial && (!Scalars.IsEmpty() || !Colors.IsEmpty() || !Curves.IsEmpty()))
	{ Error = TEXT("Surface parameters require an Overlay Material."); return false; }
	TSet<FName> Parameters;
	const auto AddParameter = [&Parameters, &Error](FName Name)
	{
		if (Name.IsNone() || Parameters.Contains(Name))
		{ Error = FString::Printf(TEXT("Surface parameter '%s' is empty or duplicated across scalar/color/curve lists."), *Name.ToString()); return false; }
		Parameters.Add(Name); return true;
	};
	for (const auto& P : Scalars)
	{
		if (!AddParameter(P.Parameter)) { return false; }
		if (!FMath::IsFinite(P.Value)) { Error = TEXT("Surface scalar values must be finite."); return false; }
	}
	for (const auto& P : Colors)
	{
		if (!AddParameter(P.Parameter)) { return false; }
		if (!FMath::IsFinite(P.Value.R) || !FMath::IsFinite(P.Value.G) || !FMath::IsFinite(P.Value.B) || !FMath::IsFinite(P.Value.A))
		{ Error = TEXT("Surface colors must be finite."); return false; }
	}
	for (const auto& P : Curves)
	{
		if (!AddParameter(P.Parameter)) { return false; }
		const FRichCurve* Curve = P.Value.GetRichCurveConst();
		if (!Curve || Curve->GetNumKeys() == 0) { Error = TEXT("Surface curves must contain at least one key."); return false; }
		for (const auto& Key : Curve->GetConstRefOfKeys())
		{
			if (!FMath::IsFinite(Key.Time) || Key.Time < 0 || !FMath::IsFinite(Key.Value)
				|| !FMath::IsFinite(Key.ArriveTangent) || !FMath::IsFinite(Key.LeaveTangent)
				|| !FMath::IsFinite(Key.ArriveTangentWeight) || !FMath::IsFinite(Key.LeaveTangentWeight))
			{ Error = TEXT("Surface curve keys/tangents must be finite; times are nonnegative seconds."); return false; }
		}
	}
	TSet<FName> LayerIds;
	for (int32 Index = 0; Index < NiagaraLayers.Num(); ++Index)
	{
		const auto& L = NiagaraLayers[Index];
		if (L.LayerId.IsNone() || LayerIds.Contains(L.LayerId) || !InRange(L.PaddingFraction,1))
		{ Error = FString::Printf(TEXT("Niagara layer %d needs a unique nonempty Id and padding 0..1."), Index); return false; }
		LayerIds.Add(L.LayerId);
		const bool bBounds = L.Binding == EHALStatusFXBinding::Bounds && L.BoundsSystem && !L.StaticSurfaceSystem && !L.SkeletalSurfaceSystem;
		const bool bSurface = L.Binding == EHALStatusFXBinding::MeshSurface && !L.BoundsSystem && L.StaticSurfaceSystem && L.SkeletalSurfaceSystem;
		if (!bBounds && !bSurface)
		{ Error = FString::Printf(TEXT("Layer '%s': Bounds needs only Bounds System; Mesh Surface needs both Static/Skeletal templates and no Bounds System."), *L.LayerId.ToString()); return false; }
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UHALVehicleStatusFXDefinition::IsDataValid(FDataValidationContext& Context) const
{
	FString Error;
	if (!Validate(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}
#endif
