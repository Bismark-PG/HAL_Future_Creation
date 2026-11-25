#include "HALBuildVehicleHUDStyleCommandlet.h"
#include "HALVehicleCatalog.h"
#include "HALVehicleHUDStyleLibrary.h"
#include "HALVehiclePresentationConfig.h"
#include "TestVehiclePawn.h"
#include "VehicleDefinition.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "EditorFramework/AssetImportData.h"
#include "Components/SkeletalMeshComponent.h"
#include "IMaterialBakingModule.h"
#include "MaterialBakingStructures.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"

namespace
{
	constexpr int32 HUDWidth = 1280, HUDHeight = 256, HUDBakeSize = 256;
	bool HUDSave(UObject& Object)
	{
		Object.MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Object.GetPackage(), &Object,
			*FPackageName::LongPackageNameToFilename(Object.GetPackage()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
	}
	template<class T> T* HUDNode(UMaterial& Material)
	{ auto* Node = NewObject<T>(&Material); Material.GetExpressionCollection().AddExpression(Node); return Node; }
	void HUDInput(UMaterialExpressionCustom& Code, FName Name, UMaterialExpression* Node)
	{ auto& Input = Code.Inputs.AddDefaulted_GetRef(); Input.InputName = Name; Input.Input.Connect(0, Node); }
	UMaterial* HUDMaterial(const FString& Path, FColor Fill)
	{
		// Existing material is left intact on repeat runs so hand-tuned defaults survive.
		if (FPackageName::DoesPackageExist(Path)) { return LoadObject<UMaterial>(nullptr, *Path); }
		auto* M = NewObject<UMaterial>(CreatePackage(*Path), *FPackageName::GetShortName(Path), RF_Public | RF_Standalone);
		M->MaterialDomain = MD_UI; M->BlendMode = BLEND_Translucent; M->SetShadingModel(MSM_Unlit);
		auto* H = HUDNode<UMaterialExpressionCustom>(*M); H->OutputType = CMOT_Float4;
		HUDInput(*H, TEXT("UV"), HUDNode<UMaterialExpressionTextureCoordinate>(*M));
		const auto Color = [&](const TCHAR* Name, FLinearColor Value)
		{ auto* N = HUDNode<UMaterialExpressionVectorParameter>(*M); N->ParameterName = Name; N->DefaultValue = Value; HUDInput(*H, Name, N); };
		const auto Scalar = [&](const TCHAR* Name, float Value)
		{ auto* N = HUDNode<UMaterialExpressionScalarParameter>(*M); N->ParameterName = Name; N->DefaultValue = Value; HUDInput(*H, Name, N); };
		Color(TEXT("FillColor"), FLinearColor(Fill)); Color(TEXT("OutlineColor"), FLinearColor(.05f, .85f, .7f, 1));
		Scalar(TEXT("FillOpacity"), Fill.A / 255.f); Scalar(TEXT("OutlineOpacity"), 1);
		Scalar(TEXT("OutlineWidth"), .025f); Scalar(TEXT("EndAccentWidth"), .055f); Scalar(TEXT("Slant"), 1.f);
		H->Code = TEXT(R"HUD(
float slope = clamp(Slant, -1.4, 1.4);
float2 p = UV * float2(5.0, 1.0);
float y = p.y - 0.0625, height = 0.875;
float shift = abs(slope) * height;
float left = 0.0625 + max(-slope, 0.0) * height + slope * y;
float right = left + 4.875 - shift;
float side = min(p.x-left, right-p.x) / sqrt(1.0+slope*slope);
float vertical = min(y, height-y);
float d = min(vertical, side);
float aa = max(fwidth(d), 0.00001);
float outer = saturate(d / aa + 0.5);
float innerD = min(vertical-max(OutlineWidth,0.0), side-max(EndAccentWidth,OutlineWidth));
float inner = min(outer, saturate(innerD / aa + 0.5));
float baseAlpha = inner * saturate(FillOpacity);
float edgeAlpha = (outer-inner) * saturate(OutlineOpacity);
float alpha = baseAlpha + edgeAlpha;
float3 rgb = (FillColor.rgb*baseAlpha + OutlineColor.rgb*edgeAlpha) / max(alpha,0.00001);
return float4(rgb,alpha);
)HUD");
		auto* RGB = HUDNode<UMaterialExpressionComponentMask>(*M); RGB->Input.Connect(0, H); RGB->R = RGB->G = RGB->B = true;
		auto* Alpha = HUDNode<UMaterialExpressionComponentMask>(*M); Alpha->Input.Connect(0, H); Alpha->R = Alpha->G = Alpha->B = false; Alpha->A = true;
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB); M->GetEditorOnlyData()->Opacity.Connect(0, Alpha);
		M->PostEditChange(); FAssetRegistryModule::AssetCreated(M); return M;
	}
	bool HUDTexture(UTexture2D& Texture, FColor Fill, const FString& PNGPath)
	{
		TArray<FColor> Pixels; Pixels.SetNumUninitialized(HUDWidth * HUDHeight);
		for (int32 Y = 0; Y < HUDHeight; ++Y) for (int32 X = 0; X < HUDWidth; ++X)
		{
			int32 Coverage = 0;
			for (int32 J = 0; J < 4; ++J) for (int32 I = 0; I < 4; ++I)
			{
				const float PY = Y + (J + .5f) / 4 - 16, PX = X + (I + .5f) / 4;
				Coverage += PY >= 0 && PY <= 224 && PX >= 16 + PY && PX <= 1040 + PY;
			}
			auto& P = Pixels[Y * HUDWidth + X]; P = Fill; P.A = uint8(FMath::RoundToInt(Fill.A * Coverage / 16.f));
		}
		Texture.Source.Init(HUDWidth, HUDHeight, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
		// Preserve the original texture's sampling, sRGB and compression settings.
		Texture.PostEditChange();
		auto& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		auto PNG = Images.CreateImageWrapper(EImageFormat::PNG);
		if (!PNG->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), HUDWidth, HUDHeight, ERGBFormat::BGRA, 8)) { return false; }
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(PNGPath), true);
		if (!FFileHelper::SaveArrayToFile(PNG->GetCompressed(), *PNGPath)) { return false; }
		if (!Texture.AssetImportData) { Texture.AssetImportData = NewObject<UAssetImportData>(&Texture); }
		Texture.AssetImportData->Update(PNGPath);
		return true;
	}
	bool HUDBake(FHALVehicleCatalogEntry& Entry)
	{
		auto* Class = Entry.VehicleClass.LoadSynchronous(); const auto* Pawn = Class ? Cast<ATestVehiclePawn>(Class->GetDefaultObject()) : nullptr;
		const auto* Definition = Pawn ? Pawn->GetVehicleDefinition() : nullptr;
		auto* Mesh = Definition && Definition->Presentation ? Definition->Presentation->Mesh.Get() : nullptr;
		FAssetCompilingManager::Get().FinishAllCompilation();
		const FMeshDescription* Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
		if (!Description) { UE_LOG(LogTemp, Error, TEXT("HUD palette: %s has no editable skeletal LOD0."), *Entry.VehicleId.ToString()); return false; }
		FSkeletalMeshConstAttributes Attr(*Description); const auto UVs = Attr.GetVertexInstanceUVs(); const auto Positions = Attr.GetVertexPositions();
		const auto Slots = Attr.GetPolygonGroupMaterialSlotNames();
		auto& Baking = FModuleManager::LoadModuleChecked<IMaterialBakingModule>(TEXT("MaterialBaking"));
		TArray<HALVehicleHUD::FColorAreaSample> Samples;
		for (const auto Group : Description->PolygonGroups().GetElementIDs())
		{
			if (Description->GetPolygonGroupTriangles(Group).Num() == 0) { continue; }
			if (UVs.GetNumChannels() == 0) { return false; }
			FBox2D UVBounds(ForceInit);
			for (const auto Triangle : Description->GetPolygonGroupTriangles(Group))
			{
				for (const auto Instance : Description->GetTriangleVertexInstances(Triangle))
				{
					const auto UV = UVs.Get(Instance,0);
					if (!FMath::IsFinite(UV.X) || !FMath::IsFinite(UV.Y)) { return false; }
					UVBounds += FVector2D(UV.X,UV.Y);
				}
			}
			if (!UVBounds.bIsValid) { return false; }
			const bool bQuad = UVBounds.GetSize().X <= UE_SMALL_NUMBER || UVBounds.GetSize().Y <= UE_SMALL_NUMBER;
			if (bQuad)
			{
				// Flat-colour imported material groups can use a single UV point. Evaluate the real material at that coordinate.
				if (UVBounds.GetSize().X <= UE_SMALL_NUMBER) { UVBounds.Min.X -= .5; UVBounds.Max.X += .5; }
				if (UVBounds.GetSize().Y <= UE_SMALL_NUMBER) { UVBounds.Min.Y -= .5; UVBounds.Max.Y += .5; }
				UE_LOG(LogTemp, Display, TEXT("HUD palette %s: material group %d uses point/line UV sampling."), *Entry.VehicleId.ToString(), Group.GetValue());
			}
			UVBounds = UVBounds.ExpandBy(.0001);
			const int32 MaterialIndex = Mesh->GetMaterials().IndexOfByPredicate([&](const FSkeletalMaterial& M) { return M.ImportedMaterialSlotName == Slots[Group] || M.MaterialSlotName == Slots[Group]; });
			if (MaterialIndex == INDEX_NONE) { UE_LOG(LogTemp, Error, TEXT("HUD palette: unmatched material slot %s"), *Slots[Group].ToString()); return false; }
			UMaterialInterface* Material = Mesh->GetMaterials()[MaterialIndex].MaterialInterface;
			TInlineComponentArray<USkeletalMeshComponent*> Components; Pawn->GetComponents(Components);
			for (const auto* C : Components) if (C->GetSkeletalMeshAsset() == Mesh && C->GetMaterial(MaterialIndex)) { Material = C->GetMaterial(MaterialIndex); break; }
			if (!Material) { return false; }
			FMaterialData Settings; Settings.Material = Material; Settings.PropertySizes.Add(MP_BaseColor, FIntPoint(HUDBakeSize));
			// Uncovered texels must not vote for the baking module's magenta clear colour.
			Settings.BackgroundColor = FColor(255, 0, 255, 0);
			Settings.bPerformBorderSmear = true; Settings.bPerformShrinking = false;
			FMeshData Geometry; Geometry.MeshDescription = bQuad ? nullptr : Description; Geometry.MaterialIndices.Add(Group.GetValue()); Geometry.PrimitiveData = FPrimitiveData(Mesh); Geometry.TextureCoordinateBox = UVBounds;
			TArray<FBakeOutput> Output; Baking.BakeMaterials(TArray<FMaterialData*>{&Settings}, TArray<FMeshData*>{&Geometry}, Output);
			if (Output.Num() != 1 || !Output[0].PropertyData.Contains(MP_BaseColor)) { return false; }
			const auto& Colors = Output[0].PropertyData[MP_BaseColor]; const auto Size = Output[0].PropertySizes[MP_BaseColor];
			if (Colors.Num() != Size.X * Size.Y || UVs.GetNumChannels() == 0) { return false; }
			const bool bLinear = Output[0].PropertyIsLinearColor.FindRef(MP_BaseColor);
			for (const auto Triangle : Description->GetPolygonGroupTriangles(Group))
			{
				const auto Vertices = Description->GetTriangleVertices(Triangle); const auto Instances = Description->GetTriangleVertexInstances(Triangle);
				const FVector3f A = Positions[Vertices[0]], B = Positions[Vertices[1]], C = Positions[Vertices[2]];
				const double Area = FVector3f::CrossProduct(B-A, C-A).Size() * .5;
				if (!FMath::IsFinite(Area) || Area <= UE_SMALL_NUMBER) { continue; }
				for (int32 I = 0; I < 4; ++I) for (int32 J = 0; J < 4; ++J)
				{
					const float Root = FMath::Sqrt((I + .5f) / 4), V = (J + .5f) / 4;
					const FVector2f UV = UVs.Get(Instances[0],0) * (1-Root) + UVs.Get(Instances[1],0) * (Root*(1-V)) + UVs.Get(Instances[2],0) * (Root*V);
					// Bake over actual bounds while leaving original shader UVs, including negative/tiled coordinates, unchanged.
					const FVector2D InBake = (FVector2D(UV.X,UV.Y) - UVBounds.Min) / UVBounds.GetSize();
					FColor Color = Colors[FMath::Clamp(int32(InBake.Y * Size.Y),0,Size.Y-1)*Size.X + FMath::Clamp(int32(InBake.X * Size.X),0,Size.X-1)];
					if (Color == Settings.BackgroundColor) { continue; }
					if (bLinear) { Color = Color.ReinterpretAsLinear().ToFColorSRGB(); } Color.A = 255;
					Samples.Add({Color, Area / 16});
				}
			}
		}
		FLinearColor Color; if (!HALVehicleHUD::DominantColor(Samples, Color)) { return false; }
		Entry.HUDDominantColor = Color; Entry.HUDColorSource = Mesh; Entry.bHasHUDDominantColor = true;
		UE_LOG(LogTemp, Display, TEXT("HUD palette %s: mesh=%s samples=%d dominant=#%s outline=%s"), *Entry.VehicleId.ToString(), *Mesh->GetPathName(), Samples.Num(), *Color.ToFColorSRGB().ToHex(), *HALVehicleHUD::BrightOutline(Color).ToString());
		return true;
	}
}

UHALBuildVehicleHUDStyleCommandlet::UHALBuildVehicleHUDStyleCommandlet()
{ IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true; }

int32 UHALBuildVehicleHUDStyleCommandlet::Main(const FString& Params)
{
	FString CatalogPath, TexturePath, MaterialPath, ArtPath;
	const bool bFillMissing = FParse::Param(*Params, TEXT("FillMissingColors"));
	if (!FParse::Value(*Params,TEXT("Catalog="),CatalogPath) || (!bFillMissing && (!FParse::Value(*Params,TEXT("Background="),TexturePath)
		|| !FParse::Value(*Params,TEXT("Material="),MaterialPath) || !FParse::Value(*Params,TEXT("PNG="),ArtPath))))
	{ UE_LOG(LogTemp, Error, TEXT("Specify -Catalog= with either -FillMissingColors or -Background= -Material= -PNG=. Add -WriteAssets to persist.")); return 1; }
	if (!FApp::CanEverRender()) { UE_LOG(LogTemp, Error, TEXT("Material baking requires a real RHI; omit -nullrhi and add -AllowCommandletRendering.")); return 2; }
	auto* Catalog = LoadObject<UHALVehicleCatalog>(nullptr, *CatalogPath);
	if (!Catalog) { return 3; }
	// Do not publish a partially populated palette if an individual mesh cannot be baked.
	TArray<FHALVehicleCatalogEntry> Baked = Catalog->Vehicles;
	for (auto& Entry : Baked)
	{
		if (bFillMissing && Entry.bHasHUDDominantColor && !Entry.HUDColorSource.IsNull()) { continue; }
		if (!HUDBake(Entry)) { return 6; }
	}
	const bool bWrite = FParse::Param(*Params, TEXT("WriteAssets"));
	if (bFillMissing)
	{
		if (!bWrite) { return 0; }
		Catalog->Vehicles = MoveTemp(Baked);
		return HUDSave(*Catalog) ? 0 : 8;
	}
	auto* Background = LoadObject<UTexture2D>(nullptr, *TexturePath);
	if (!Background || Background->Source.GetFormat() != TSF_BGRA8) { return 3; }
	TArray64<uint8> Bytes; if (!Background->Source.GetMipData(Bytes,0)) { return 4; }
	const int64 Index = ((int64(Background->Source.GetSizeY()/2)*Background->Source.GetSizeX()) + Background->Source.GetSizeX()/2)*4;
	if (Bytes.Num() < Index+4) { return 5; }
	const FColor Fill(Bytes[Index+2], Bytes[Index+1], Bytes[Index], Bytes[Index+3]);
	UE_LOG(LogTemp, Display, TEXT("HUD background preserved: RGB=%d/%d/%d alpha=%d/255"), Fill.R,Fill.G,Fill.B,Fill.A);
	if (!bWrite) { UE_LOG(LogTemp, Display, TEXT("HUD palette dry run passed; no assets saved.")); return 0; }
	if (!HUDTexture(*Background,Fill,ArtPath)) { return 7; }
	auto* Material = HUDMaterial(MaterialPath,Fill);
	if (!Material) { UE_LOG(LogTemp, Error, TEXT("HUD material path must identify a Material, not a material instance or another asset type.")); return 8; }
	FAssetCompilingManager::Get().FinishAllCompilation();
	Catalog->Vehicles = MoveTemp(Baked);
	if (!HUDSave(*Background) || !HUDSave(*Material) || !HUDSave(*Catalog)) { return 8; }
	UE_LOG(LogTemp, Display, TEXT("Saved slanted background, parameterized UI outline material and mesh-derived catalog colours. Blueprint hookup remains manual."));
	return 0;
}
