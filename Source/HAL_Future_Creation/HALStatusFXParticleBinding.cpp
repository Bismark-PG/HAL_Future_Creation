#include "HALStatusFXParticleBinding.h"
#include "HALVehicleFXTargets.h"
#include "HALVehicleStatusFXDefinition.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraEmitter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "StaticMeshResources.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/UnrealType.h"

bool HALStatusFXParticle::Configure(UNiagaraComponent* Component, const FHALVehicleFXTarget& Target,
	const FHALStatusFXNiagaraLayer& Layer, FString& Error)
{
	Error.Reset();
	const auto Fail = [&](const FString& E) { Error = E; return false; };
	auto* System = Component ? Component->GetAsset() : nullptr;
	if (!System || !Target.Mesh.IsValid()) { return Fail(TEXT("Missing system or mesh")); }
	const auto& Params = System->GetExposedParameters();
	for (const TCHAR* Name : {TEXT("User.FX_Intensity"), TEXT("User.FX_Age"), TEXT("User.FX_Residue"), TEXT("User.FX_SpawnRate")})
	{ if (Params.IndexOf(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), FName(Name))) == INDEX_NONE) { return Fail(FString(TEXT("Missing float ")) + Name); } }
	for (const TCHAR* Name : {TEXT("User.FX_BoundsCenter"), TEXT("User.FX_BoundsExtent")})
	{ if (Params.IndexOf(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), FName(Name))) == INDEX_NONE) { return Fail(FString(TEXT("Missing vector ")) + Name); } }
	if (Params.IndexOf(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("User.FX_MaxParticles"))) == INDEX_NONE) { return Fail(TEXT("Missing int User.FX_MaxParticles")); }
	for (const auto& Handle : System->GetEmitterHandles())
	{
		if (const auto* Data = Handle.GetEmitterData(); Data && (!Data->bLocalSpace || Data->SimTarget != ENiagaraSimTarget::CPUSim))
		{ return Fail(TEXT("FX-4 templates require CPU simulation and Local Space")); }
	}
	if (Layer.Binding == EHALStatusFXBinding::MeshSurface)
	{
		// The user-redirection view enumerates short aliases; raw variables retain User. and the DI UClass.
		TArray<FNiagaraVariable> Variables; Params.GetParameters(Variables);
		const auto* Source = Variables.FindByPredicate([](const auto& P) { return P.GetName() == TEXT("User.FX_SourceMesh"); });
		UClass* SourceClass = Source ? Source->GetType().GetClass() : nullptr;
		const FName Type = Target.Kind == EHALVehicleFXMeshKind::Static ? TEXT("NiagaraDataInterfaceStaticMesh") : TEXT("NiagaraDataInterfaceSkeletalMesh");
		if (!SourceClass || SourceClass->GetFName() != Type) { return Fail(TEXT("User.FX_SourceMesh has missing/wrong mesh Data Interface type")); }
		if (auto* Static = Cast<UStaticMeshComponent>(Target.Mesh.Get()))
		{
			UStaticMesh* Mesh = Static->GetStaticMesh();
			if (!Mesh || !Mesh->bAllowCPUAccess || !Mesh->GetRenderData() || !Mesh->GetRenderData()->LODResources.IsValidIndex(Layer.SamplingLOD)
				|| Mesh->GetRenderData()->LODResources[Layer.SamplingLOD].GetNumTriangles() == 0)
			{ return Fail(TEXT("Static mesh needs rebuilt Allow CPU Access and a valid sampling LOD")); }
			const auto& Data = Mesh->GetRenderData()->LODResources[Layer.SamplingLOD];
			if (!Data.IndexBuffer.GetAllowCPUAccess() || Data.IndexBuffer.GetArrayView().Num() < 3
				|| !Data.VertexBuffers.PositionVertexBuffer.GetAllowCPUAccess() || Data.VertexBuffers.PositionVertexBuffer.GetNumVertices() == 0)
			{ return Fail(TEXT("Static sampling LOD has no retained CPU triangle buffers; rebuild after Allow CPU Access")); }
			UNiagaraFunctionLibrary::OverrideSystemUserVariableStaticMeshComponent(Component, TEXT("User.FX_SourceMesh"), Static);
			auto* DI = UNiagaraFunctionLibrary::GetDataInterface(SourceClass, Component, TEXT("User.FX_SourceMesh"));
			auto* LOD = DI ? FindFProperty<FIntProperty>(DI->GetClass(), TEXT("LODIndex")) : nullptr;
			auto* Mode = DI ? FindFProperty<FEnumProperty>(DI->GetClass(), TEXT("SourceMode")) : nullptr;
			const int64 ExplicitSource = Mode ? Mode->GetEnum()->GetValueByNameString(TEXT("Source")) : INDEX_NONE;
			if (!LOD || ExplicitSource == INDEX_NONE) { return Fail(TEXT("Static mesh DI LOD/source interface is unavailable")); }
			LOD->SetPropertyValue_InContainer(DI, Layer.SamplingLOD);
			Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(DI), ExplicitSource);
		}
		else if (auto* Skeletal = Cast<USkeletalMeshComponent>(Target.Mesh.Get()))
		{
			auto* Mesh = Skeletal->GetSkeletalMeshAsset(); const auto* Info = Mesh ? Mesh->GetLODInfo(Layer.SamplingLOD) : nullptr;
			if (!Info || !Info->bAllowCPUAccess || !Mesh->GetResourceForRendering()
				|| !Mesh->GetResourceForRendering()->LODRenderData.IsValidIndex(Layer.SamplingLOD))
			{ return Fail(TEXT("Skeletal mesh sampling LOD needs rebuilt Allow CPU Access")); }
			const auto& LOD = Mesh->GetResourceForRendering()->LODRenderData[Layer.SamplingLOD];
			if (LOD.GetNumVertices() == 0 || LOD.GetTotalFaces() == 0 || LOD.SkinWeightVertexBuffer.GetNumVertices() == 0
				|| !LOD.MultiSizeIndexContainer.IsIndexBufferValid() || !LOD.MultiSizeIndexContainer.GetIndexBuffer()
				|| LOD.MultiSizeIndexContainer.GetIndexBuffer()->Num() < 3)
			{ return Fail(TEXT("Skeletal sampling LOD has no retained triangle/skin-weight data")); }
			UNiagaraFunctionLibrary::OverrideSystemUserVariableSkeletalMeshComponent(Component, TEXT("User.FX_SourceMesh"), Skeletal);
			auto* DI = UNiagaraFunctionLibrary::GetSkeletalMeshDataInterface(Component, TEXT("User.FX_SourceMesh"));
			if (!DI) { return Fail(TEXT("Skeletal mesh DI unavailable")); }
			DI->SourceMode = ENDISkeletalMesh_SourceMode::Source;
			DI->SkinningMode = ENDISkeletalMesh_SkinningMode::SkinOnTheFly;
			DI->bRequireCurrentFrameData = true; DI->SetWholeMeshLODFromBlueprints(Layer.SamplingLOD);
		}
		else { return Fail(TEXT("Unsupported mesh component")); }
	}
	Component->AttachToComponent(Target.Mesh.Get(), FAttachmentTransformRules::SnapToTargetIncludingScale);
	Component->AddTickPrerequisiteComponent(Target.Mesh.Get());
	return true;
}
