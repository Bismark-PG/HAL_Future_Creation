#include "HALVehicleStatusFXRenderer.h"
#include "Components/MeshComponent.h"
#include "HALVehicleStatusFXComponent.h"
#include "HALVehicleStatusFXDefinition.h"
#include "HALStatusFXSurfaceLayout.h"
#include "HALGameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"

bool HALStatusFXRender::UsesShockDriver(FGameplayTag Tag) { return Tag.MatchesTag(HALGameplayTags::State_Visual_Shocked); }
float HALStatusFXRender::ResidueWeight(const FHALStatusFXRenderRequest& R)
{
	const auto* D = R.Recipe.Get();
	return D && R.State.Phase == EHALStatusFXPhase::Residue && D->ResidueSeconds > 0
		? float(FMath::Clamp(1.0 - R.State.PhaseSeconds / D->ResidueSeconds, 0.0, 1.0)) : 0.f;
}

namespace
{
	bool IsPreferredSurface(FGameplayTag A, int32 APriority, bool bAResidue, FGameplayTag B, int32 BPriority, bool bBResidue)
	{
		if (bAResidue != bBResidue) { return !bAResidue; }
		if (APriority != BPriority) { return APriority > BPriority; }
		return A.ToString() < B.ToString();
	}
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
		if (!D || !D->bEnableRendering || HALStatusFXRender::UsesShockDriver(R.State.Tag) || !R.Target.Mesh.IsValid() || !R.Surface.IsValid()
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
		return IsPreferredSurface(A.State.Tag, A.Recipe->SurfacePriority, A.State.Phase == EHALStatusFXPhase::Residue,
			B.State.Tag, B.Recipe->SurfacePriority, B.State.Phase == EHALStatusFXPhase::Residue);
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
		Owner.GenericTag = Lead->State.Tag;
		Owner.GenericPriority = Lead->Recipe->SurfacePriority;
		Owner.bGenericResidue = Lead->State.Phase == EHALStatusFXPhase::Residue;
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
		UMaterialInterface* Desired = O.Original;
		if (O.Legacy) { Desired = O.Legacy; }
		else if (ShouldUseShock(O)) { Desired = O.Shock; }
		else if (O.Generic) { Desired = O.Generic; }
		if (O.Mesh->GetOverlayMaterial() != Desired) { O.Mesh->SetOverlayMaterial(Desired); }
		O.Applied = Desired;
	}
	Owners.RemoveAll([](const auto& O) { return !O.Mesh.IsValid() || (!O.Legacy && !O.Shock && !O.Generic); });
}

bool UHALVehicleStatusFXRenderer::ShouldUseShock(const FHALStatusFXSurfaceOwner& O)
{
	return O.Shock && (!O.Generic || IsPreferredSurface(HALGameplayTags::State_Visual_Shocked, O.ShockPriority, O.bShockResidue,
		O.GenericTag, O.GenericPriority, O.bGenericResidue));
}

void UHALVehicleStatusFXRenderer::SetShockSurface(UMeshComponent* Mesh, UMaterialInterface* Material, int32 Priority, bool bResidue)
{
	check(IsInGameThread());
	if (!IsValid(Mesh)) { return; }
	if (Material)
	{
		auto& Owner = FindOrAdd(Mesh);
		if (Owner.Shock == Material && Owner.ShockPriority == Priority && Owner.bShockResidue == bResidue) { return; }
		Owner.Shock = Material;
		Owner.ShockPriority = Priority;
		Owner.bShockResidue = bResidue;
	}
	else if (auto* Owner = Owners.FindByPredicate([&](const auto& O) { return O.Mesh == Mesh && O.Shock; }))
	{ Owner->Shock = nullptr; }
	else { return; }
	ApplyAndPrune();
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
	for (auto& O : Owners) { O.Legacy = nullptr; O.Shock = nullptr; O.Generic = nullptr; }
	ApplyAndPrune();
}

int32 UHALVehicleStatusFXRenderer::GetGenericChannelCount() const
{
	int32 Count = 0;
	for (const auto& O : Owners) { if (O.Generic && !O.Legacy && !ShouldUseShock(O)) { Count += O.Contributors.Num(); } }
	return Count;
}
