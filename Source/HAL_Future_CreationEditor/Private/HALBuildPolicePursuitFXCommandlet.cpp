#include "HALBuildPolicePursuitFXCommandlet.h"
#include "HALPolicePursuitAbility.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "EditorFramework/AssetImportData.h"
#include "Serialization/BufferArchive.h"
#include "Factories/SoundFactory.h"
#include "Sound/SoundWave.h"
namespace
{
	const FString Root(TEXT("/Game/Assets/FX/PolicePursuit/"));
	template<class T> T* Asset(const FString& Path) { return NewObject<T>(CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone); }
	template<class T> T* Node(UMaterial& M) { auto* N=NewObject<T>(&M); M.GetExpressionCollection().AddExpression(N); return N; }
	void Input(UMaterialExpressionCustom& H,const TCHAR* Name,UMaterialExpression* E) { auto& I=H.Inputs.AddDefaulted_GetRef(); I.InputName=Name; I.Input.Connect(0,E); }
	void Scalar(UMaterial& M,UMaterialExpressionCustom& H,const TCHAR* Name,float V) { auto* N=Node<UMaterialExpressionScalarParameter>(M); N->ParameterName=Name; N->DefaultValue=V; Input(H,Name,N); }
	UMaterial* Material(bool Signal)
	{
		auto* M=Asset<UMaterial>(Root+(Signal?TEXT("M_Pursuit_Signal"):TEXT("M_Pursuit_Range")));
		M->SetShadingModel(MSM_Unlit); M->TwoSided=true; M->BlendMode=Signal?BLEND_Opaque:BLEND_Translucent;
		auto* H=Node<UMaterialExpressionCustom>(*M); H->OutputType=CMOT_Float4;
		auto* Color=Node<UMaterialExpressionVectorParameter>(*M); Color->ParameterName=TEXT("Color"); Color->DefaultValue=FLinearColor(1,.035f,.06f); Input(*H,TEXT("Color"),Color);
		if(Signal) { Scalar(*M,*H,TEXT("Glow"),2); H->Code=TEXT("return float4(Color.rgb*Glow,1);"); }
		else
		{
			Input(*H,TEXT("UV"),Node<UMaterialExpressionTextureCoordinate>(*M)); Scalar(*M,*H,TEXT("Time"),0);
			Scalar(*M,*H,TEXT("Opacity"),.12f); Scalar(*M,*H,TEXT("RingOpacity"),.36f); Scalar(*M,*H,TEXT("ScanSpeed"),.65f);
			Scalar(*M,*H,TEXT("ScanWidth"),.12f); Scalar(*M,*H,TEXT("ScanOpacity"),.18f); Scalar(*M,*H,TEXT("RingWidth"),.025f);
			H->Code=TEXT("float2 p=UV*2-1; float r=length(p); float a=atan2(p.y,p.x); "
				"float scan=pow(saturate(.5+.5*cos(a-Time*ScanSpeed*6.2831853)),1/max(.02,ScanWidth)); "
				"float ring=smoothstep(1-RingWidth-.01,1-RingWidth,r); float ripple=pow(.5+.5*cos((r-Time*.3)*18.84956),12); "
				"float mask=1-smoothstep(.985,1,r); return float4(lerp(Color.rgb,float3(1,.5,.5),scan*.35),(Opacity+ring*RingOpacity+ScanOpacity*(scan+ripple*.2))*mask);");
		}
		auto* RGB=Node<UMaterialExpressionComponentMask>(*M); RGB->R=RGB->G=RGB->B=true; RGB->A=false; RGB->Input.Connect(0,H);
		auto* A=Node<UMaterialExpressionComponentMask>(*M); A->R=A->G=A->B=false; A->A=true; A->Input.Connect(0,H);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0,RGB); if(!Signal) { M->GetEditorOnlyData()->Opacity.Connect(0,A); } M->PostEditChange(); return M;
	}
	UStaticMesh* Mesh(UMaterial& M,bool Cuffs)
	{
		auto* S=Asset<UStaticMesh>(Root+(Cuffs?TEXT("SM_Pursuit_Handcuffs"):TEXT("SM_Pursuit_Range")));
		FMeshDescription D; FStaticMeshAttributes A(D); A.Register(); auto Pos=A.GetVertexPositions(); auto Norm=A.GetVertexInstanceNormals();
		auto Tangent=A.GetVertexInstanceTangents(); auto Sign=A.GetVertexInstanceBinormalSigns(); auto UV=A.GetVertexInstanceUVs(); UV.SetNumChannels(1);
		const auto Group=D.CreatePolygonGroup(); A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Surface");
		const auto Triangle=[&](FVector3f P,FVector3f Q,FVector3f R)
		{
			TArray<FVertexInstanceID,TInlineAllocator<3>> IDs;
			for(auto V:{P,Q,R}) { const auto I=D.CreateVertex(); Pos[I]=V; const auto VI=D.CreateVertexInstance(I); IDs.Add(VI); Norm[VI]=FVector3f::UpVector; Tangent[VI]=FVector3f::ForwardVector; Sign[VI]=1; UV.Set(VI,0,FVector2f(V.X/200+.5f,V.Y/200+.5f)); }
			D.CreateTriangle(Group,IDs);
		};
		for(int32 I=0;I<64;++I)
		{
			const float A0=I*2*UE_PI/64,A1=(I+1)*2*UE_PI/64;
			if(!Cuffs) { Triangle(FVector3f::ZeroVector,FVector3f(FMath::Cos(A0)*100,FMath::Sin(A0)*100,0),FVector3f(FMath::Cos(A1)*100,FMath::Sin(A1)*100,0)); }
			else for(float X:{-26.f,26.f})
			{
				const auto P=[&](float A,float R){return FVector3f(X+FMath::Cos(A)*R,FMath::Sin(A)*R,0);};
				Triangle(P(A0,22),P(A1,22),P(A1,14)); Triangle(P(A0,22),P(A1,14),P(A0,14));
			}
		}
		if(Cuffs) { Triangle(FVector3f(-12,-3,0),FVector3f(12,-3,0),FVector3f(12,3,0)); Triangle(FVector3f(-12,-3,0),FVector3f(12,3,0),FVector3f(-12,3,0)); }
		S->GetStaticMaterials().Add(FStaticMaterial(&M)); S->SetNumSourceModels(1); auto& B=S->GetSourceModel(0).BuildSettings; B.bRecomputeNormals=B.bRecomputeTangents=B.bGenerateLightmapUVs=false;
		UStaticMesh::FBuildMeshDescriptionsParams P; P.bBuildSimpleCollision=false; P.bCommitMeshDescription=true; return S->BuildFromMeshDescriptions({&D},P)?S:nullptr;
	}
	USoundWave* Siren()
	{
		FBufferArchive Wave; const auto Four=[&](const ANSICHAR* S){Wave.Serialize(const_cast<ANSICHAR*>(S),4);};
		uint32 Size=44100*2*2+36,FormatSize=16,Rate=44100,ByteRate=88200,DataSize=44100*2*2; uint16 PCM=1,Channels=1,Align=2,Bits=16;
		Four("RIFF"); Wave<<Size; Four("WAVE"); Four("fmt "); Wave<<FormatSize<<PCM<<Channels<<Rate<<ByteRate<<Align<<Bits; Four("data"); Wave<<DataSize;
		double Phase=0;
		for(int32 I=0;I<88200;++I) { const double T=I/44100.,F=800+200*FMath::Sin(UE_TWO_PI*T); Phase+=UE_TWO_PI*F/44100.; int16 V=int16(FMath::Sin(Phase)*5500); Wave<<V; }
		UFactory* Factory=NewObject<USoundFactory>(); CastChecked<USoundFactory>(Factory)->SuppressImportDialogs();
		const FString Filename=FPaths::ProjectContentDir()/TEXT("Assets/FX/PolicePursuit/Source/Pursuit_Siren.wav");
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
		if(!FFileHelper::SaveArrayToFile(Wave,*Filename)) { return nullptr; } bool Cancelled=false;
		auto* Sound=Cast<USoundWave>(Factory->ImportObject(USoundWave::StaticClass(),CreatePackage(*(Root+TEXT("SW_Pursuit_Siren"))),TEXT("SW_Pursuit_Siren"),RF_Public|RF_Standalone,Filename,nullptr,Cancelled));
		if(Sound) { Sound->bLooping=true; Sound->Volume=.6f; } return Sound;
	}
	bool Save(UObject& O)
	{
		FAssetRegistryModule::AssetCreated(&O); O.GetPackage()->MarkPackageDirty(); FSavePackageArgs A; A.TopLevelFlags=RF_Public|RF_Standalone; A.SaveFlags=SAVE_NoError;
		return UPackage::SavePackage(O.GetPackage(),&O,*FPackageName::LongPackageNameToFilename(O.GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()),A);
	}
}
int32 UHALBuildPolicePursuitFXCommandlet::Main(const FString& Params)
{
	if(FParse::Param(*Params,TEXT("SirenSource")))
	{
		auto* S=LoadObject<USoundWave>(nullptr,*(Root+TEXT("SW_Pursuit_Siren")));
		const FString Filename=FPaths::ProjectContentDir()/TEXT("Assets/FX/PolicePursuit/Source/Pursuit_Siren.wav");
		if(!S||!S->AssetImportData||!FPaths::FileExists(Filename)) { return 4; }
		S->AssetImportData->UpdateFilenameOnly(Filename); S->GetPackage()->MarkPackageDirty();
		FSavePackageArgs A; A.TopLevelFlags=RF_Public|RF_Standalone; A.SaveFlags=SAVE_NoError;
		return UPackage::SavePackage(S->GetPackage(),S,*FPackageName::LongPackageNameToFilename(S->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()),A)?0:5;
	}
	const FString ConfigPath(TEXT("/Game/Data/Abilities/DA_Ability_Car06_PolicePursuit"));
	for(auto P:{Root+TEXT("M_Pursuit_Range"),Root+TEXT("M_Pursuit_Signal"),Root+TEXT("SM_Pursuit_Range"),Root+TEXT("SM_Pursuit_Handcuffs"),Root+TEXT("SW_Pursuit_Siren"),ConfigPath})
	{ if(FPackageName::DoesPackageExist(P)) { UE_LOG(LogTemp,Error,TEXT("Refusing to overwrite %s"),*P); return 1; } }
	auto* R=Material(false); auto* S=Material(true); auto* Disk=Mesh(*R,false); auto* Cuffs=Mesh(*S,true); auto* Sound=Siren();
	if(!Disk||!Cuffs||!Sound) { return 2; }
	auto* Config=Asset<UHALPolicePursuitConfig>(ConfigPath); Config->FX.RangeMaterial=R; Config->FX.SignalMaterial=S; Config->FX.RangeMesh=Disk; Config->FX.ArrestMesh=Cuffs; Config->FX.SirenSound=Sound;
	Config->FX.BeaconMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	for(auto* O:TArray<UObject*>{R,S,Disk,Cuffs,Sound,Config}) { if(!Save(*O)) { return 3; } }
	UE_LOG(LogTemp,Display,TEXT("Created six native pursuit resources. No BP, vehicle definition or existing assets saved.")); return 0;
}
