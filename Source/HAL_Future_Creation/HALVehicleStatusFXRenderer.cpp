#include "HALVehicleStatusFXRenderer.h"
#include "Components/MeshComponent.h"
#include "HALVehicleStatusFXComponent.h"
#include "HALVehicleStatusFXDefinition.h"
#include "HALStatusFXSurfaceLayout.h"
#include "HALGameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"

bool HALStatusFXRender::IsLegacyTag(FGameplayTag Tag) { return Tag.MatchesTag(HALGameplayTags::State_Visual_Shocked); }
float HALStatusFXRender::ResidueWeight(const FHALStatusFXRenderRequest& R)
{
	const auto* D = R.Recipe.Get();
	return D && R.State.Phase == EHALStatusFXPhase::Residue && D->ResidueSeconds > 0
		? float(FMath::Clamp(1.0 - R.State.PhaseSeconds / D->ResidueSeconds, 0.0, 1.0)) : 0.f;
}

namespace
{
	FName Param(const UHALVehicleStatusFXDefinition* D, FName Name)
	{
		if (D->SurfaceLayout)
		{
			const auto* C = D->SurfaceLayout->Channels.FindByPredicate([&](const auto& Channel) { return Channel.Id == D->SurfaceChannel; });
			if (C) { return FName(*(C->Prefix.ToString() + Name.ToString())); }
		}
		return Name;
	}
	bool HasParameter(UMaterialInterface* M, FName Name, bool bVector)
	{
		TArray<FMaterialParameterInfo> Names; TArray<FGuid> Ids;
		if (bVector) { M->GetAllVectorParameterInfo(Names, Ids); } else { M->GetAllScalarParameterInfo(Names, Ids); }
		return Names.ContainsByPredicate([&](const auto& P) { return P.Name == Name && P.Association == EMaterialParameterAssociation::GlobalParameter; });
	}
	bool Contract(const UHALVehicleStatusFXDefinition* D, FString& Error)
	{
		auto* M = D->GetSurfaceMaterial();
		const auto Need = [&](FName N, bool Vector = false)
		{
			if (HasParameter(M, N, Vector)) { return true; }
			Error = FString::Printf(TEXT("Material %s missing %s parameter %s"), *GetNameSafe(M), Vector ? TEXT("vector") : TEXT("scalar"), *N.ToString()); return false;
		};
		if (D->SurfaceLayout)
		{
			for (const auto& C : D->SurfaceLayout->Channels)
			{ for (const TCHAR* S : {TEXT("Intensity"), TEXT("Age"), TEXT("Residue")}) { if (!Need(FName(*(C.Prefix.ToString() + S)))) { return false; } } }
		}
		else if (!Need(D->IntensityParameter) || !Need(D->AgeParameter) || !Need(D->ResidueParameter)) { return false; }
		for (const auto& P : D->Scalars) { if (!Need(Param(D, P.Parameter))) { return false; } }
		for (const auto& P : D->Colors) { if (!Need(Param(D, P.Parameter), true)) { return false; } }
		for (const auto& P : D->Curves) { if (!Need(Param(D, P.Parameter))) { return false; } }
		return true;
	}
}

void UHALVehicleStatusFXRenderer::RenderSurfaces(const TArray<FHALStatusFXRenderRequest>& Requests)
{
	check(IsInGameThread()); RejectedSurfaces = 0; LastError.Reset();
	for (auto& O : Owners) { O.Generic = nullptr; }
	TArray<const FHALStatusFXRenderRequest*> Candidates;
	for (const auto& R : Requests)
	{
		const auto* D = R.Recipe.Get();
		if (!D || !D->bEnableRendering || HALStatusFXRender::IsLegacyTag(R.State.Tag) || !R.Target.Mesh.IsValid() || !R.Surface.IsValid()
			|| !R.State.bAvailable || FMath::Max(R.State.Intensity, HALStatusFXRender::ResidueWeight(R)) <= KINDA_SMALL_NUMBER) { continue; }
		const TWeakObjectPtr<const UHALVehicleStatusFXDefinition> Key(D);
		auto* Error = Contracts.Find(Key);
		if (!Error)
		{
			FString Check;
			if (D->Validate(Check)) { Contract(D, Check); }
			Error = &Contracts.Add(Key, MoveTemp(Check));
		}
		if (!Error->IsEmpty()) { ++RejectedSurfaces; LastError = *Error; continue; }
		Candidates.Add(&R);
	}
	Candidates.Sort([](const FHALStatusFXRenderRequest& A, const FHALStatusFXRenderRequest& B)
	{
		const bool AR = A.State.Phase == EHALStatusFXPhase::Residue, BR = B.State.Phase == EHALStatusFXPhase::Residue;
		if (AR != BR) { return !AR; }
		if (A.Recipe->SurfacePriority != B.Recipe->SurfacePriority) { return A.Recipe->SurfacePriority > B.Recipe->SurfacePriority; }
		return A.State.Tag.ToString() < B.State.Tag.ToString();
	});
	TSet<UMeshComponent*> Visited;
	for (const auto* Lead : Candidates)
	{
		auto* Mesh = Lead->Target.Mesh.Get(); if (Visited.Contains(Mesh)) { continue; } Visited.Add(Mesh);
		auto* MID = Lead->Surface.Get(); const auto* Layout = Lead->Recipe->SurfaceLayout.Get();
		TSet<FName> Taken;
		TArray<const FHALStatusFXRenderRequest*> Selected;
		TArray<TWeakObjectPtr<const UHALVehicleStatusFXDefinition>> Contributors;
		for (const auto* R : Candidates)
		{
			const auto* D = R->Recipe.Get();
			if (R->Target.Mesh != Mesh || (Layout ? D->SurfaceLayout != Layout : R != Lead) || Taken.Contains(D->SurfaceChannel)) { continue; }
			Taken.Add(D->SurfaceChannel); Selected.Add(R); Contributors.Add(D);
		}
		auto& Owner = FindOrAdd(Mesh);
		if (Owner.PreviousGeneric != MID || Owner.Contributors != Contributors)
		{
			MID->ClearParameterValues(); // Reset overrides only when contributors change.
			Owner.PreviousGeneric = MID; Owner.Contributors = MoveTemp(Contributors);
		}
		if (Layout)
		{
			for (const auto& C : Layout->Channels)
			{ for (const TCHAR* N : {TEXT("Intensity"), TEXT("Age"), TEXT("Residue")}) { MID->SetScalarParameterValue(FName(*(C.Prefix.ToString() + N)), 0); } }
		}
		for (const auto* R : Selected)
		{
			const auto* D = R->Recipe.Get();
			MID->SetScalarParameterValue(Layout ? Param(D, TEXT("Intensity")) : D->IntensityParameter, R->State.Intensity);
			MID->SetScalarParameterValue(Layout ? Param(D, TEXT("Age")) : D->AgeParameter, float(R->State.ClockSeconds));
			MID->SetScalarParameterValue(Layout ? Param(D, TEXT("Residue")) : D->ResidueParameter, HALStatusFXRender::ResidueWeight(*R));
			for (const auto& P : D->Scalars) { MID->SetScalarParameterValue(Param(D, P.Parameter), P.Value); }
			for (const auto& P : D->Colors) { MID->SetVectorParameterValue(Param(D, P.Parameter), P.Value); }
			for (const auto& P : D->Curves)
			{
				const auto* Curve = P.Value.GetRichCurveConst();
				const float Age = float(FMath::Clamp(R->State.ClockSeconds, double(Curve->GetFirstKey().Time), double(Curve->GetLastKey().Time)));
				const float V = Curve->Eval(Age);
				MID->SetScalarParameterValue(Param(D, P.Parameter), FMath::IsFinite(V) ? V : 0.f);
			}
		}
		Owner.Generic = MID;
	}
	ApplyAndPrune();
}

FHALStatusFXSurfaceOwner& UHALVehicleStatusFXRenderer::FindOrAdd(UMeshComponent* Mesh)
{
	if (auto* Existing = Owners.FindByPredicate([&](const auto& O) { return O.Mesh == Mesh; })) { return *Existing; }
	FHALStatusFXSurfaceOwner New; New.Mesh = Mesh; New.Original = Mesh->GetOverlayMaterial(); New.Applied = New.Original;
	return Owners.Add_GetRef(MoveTemp(New));
}

void UHALVehicleStatusFXRenderer::ApplyAndPrune()
{
	check(IsInGameThread());
	for (auto& O : Owners)
	{
		if (!O.Mesh.IsValid()) { continue; }
		// Vehicle assembly can seed a new base overlay before it broadcasts a mesh rebind.
		if (O.Mesh->GetOverlayMaterial() != O.Applied) { O.Original = O.Mesh->GetOverlayMaterial(); }
		auto* Desired = O.Legacy ? O.Legacy.Get() : O.Generic ? O.Generic.Get() : O.Original.Get();
		if (O.Mesh->GetOverlayMaterial() != Desired) { O.Mesh->SetOverlayMaterial(Desired); }
		O.Applied = Desired;
	}
	Owners.RemoveAll([](const auto& O) { return !O.Mesh.IsValid() || (!O.Legacy && !O.Generic); });
}

void UHALVehicleStatusFXRenderer::SetLegacy(UMeshComponent* Mesh, UMaterialInterface* Material)
{
	check(IsInGameThread());
	if (!IsValid(Mesh)) { return; }
	if (Material) { FindOrAdd(Mesh).Legacy = Material; }
	else if (auto* O = Owners.FindByPredicate([&](const auto& S) { return S.Mesh == Mesh; })) { O->Legacy = nullptr; }
	ApplyAndPrune();
}

void UHALVehicleStatusFXRenderer::ClearGeneric()
{
	Contracts.Reset(); RejectedSurfaces = 0; LastError.Reset();
	for (auto& O : Owners) { O.Generic = nullptr; }
	ApplyAndPrune();
}

void UHALVehicleStatusFXRenderer::ClearAll()
{
	Contracts.Reset(); RejectedSurfaces = 0; LastError.Reset();
	for (auto& O : Owners) { O.Legacy = nullptr; O.Generic = nullptr; }
	ApplyAndPrune();
}

int32 UHALVehicleStatusFXRenderer::GetGenericChannelCount() const
{
	int32 Count = 0;
	for (const auto& O : Owners) { if (O.Generic && !O.Legacy) { Count += O.Contributors.Num(); } }
	return Count;
}
