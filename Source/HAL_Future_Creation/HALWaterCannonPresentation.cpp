#include "HALWaterCannon.h"

#include "Components/AudioComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "HALWaterCannonAbility.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

namespace
{
	using namespace UE::Geometry;
	constexpr int32 Lanes=AHALWaterCannon::LaneCount;
	constexpr int32 Rows=27;

	class FWaterGeometry
	{
	public:
		FWaterGeometry() { Water.EnableAttributes(); }
		FDynamicMesh3 Water;

		int32 Vertex(const FVector& Point,float U,float V)
		{
			const int32 Id=Water.AppendVertex(Point);
			Water.Attributes()->PrimaryUV()->AppendElement(FVector2f(U,V));
			return Id;
		}

		void Triangle(int32 A,int32 B,int32 C)
		{
			const int32 Id=Water.AppendTriangle(A,B,C);
			if(Id>=0) { Water.Attributes()->PrimaryUV()->SetTriangle(Id,FIndex3i(A,B,C)); }
		}

		// A curved foam tongue with a short, detached teardrop. The whole path stays on the wet side
		// of the hit plane; tangent/up motion makes a glancing car hit read differently from a head-on hit.
		void Splash(const FVector& Origin,const FVector& Normal,float Span,float Size,float Time,int32 Count,float Seed,bool bSurfaceContact)
		{
			const FVector Side=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal(UE_SMALL_NUMBER,FVector::RightVector);
			const FVector Up=FVector::CrossProduct(Normal,Side).GetSafeNormal();
			for(int32 I=0;I<Count;++I)
			{
				const float Angle=I*2.39996f+Seed;
				const float Phase=FMath::Frac(Time*(1.85f+.32f*FMath::Cos(Angle))+I*.61803f+Seed);
				const float Pulse=FMath::Sin(PI*Phase);
				const float Sideward=FMath::Cos(Angle);
				// On impact the near water sheet can obscure inward-facing foam. Its crown
				// spreads from the wetted footprint, predominantly across the surface.
				const FVector Along=Side*Sideward*(bSurfaceContact?1.38f:.98f)
					+Up*(bSurfaceContact?.53f+1.12f*FMath::Sin(Angle):.32f+.78f*FMath::Sin(Angle));
				const FVector Base=Origin+Normal*1.5+Side*(Sideward*Span)+Up*(bSurfaceContact?.12f*Size:0.f);
				const auto Path=[&](float T) {
					return Base+Size*(Normal*(.18*T+.57*FMath::Sin(T*PI*.76))+Along*T-Up*.27*T*T);
				};
				const FVector Axis=(Path(.5f)-Path(.2f)).GetSafeNormal();
				const FVector Cross=FVector::CrossProduct(Axis,Normal).GetSafeNormal(UE_SMALL_NUMBER,Side);
				if((I%2)==0)
				{
					const float End=.33f+.54f*Pulse, W=Size*(.052f+.064f*Pulse);
					const int32 A=Vertex(Path(.02f)-Cross*W,0,2.12f), B=Vertex(Path(.02f)+Cross*W,0,2.60f);
					const int32 C=Vertex(Path(End*.52f)-Cross*(W*.77),.35f,2.18f), D=Vertex(Path(End*.52f)+Cross*(W*.77),.35f,2.92f);
					const int32 E=Vertex(Path(End),.7f,2.78f);
					Triangle(A,B,C); Triangle(B,D,C); Triangle(C,D,E);
				}
				const FVector P=Path(.18f+1.22f*Phase);
				const float R=Size*(.067f+.054f*FMath::Abs(FMath::Sin(Angle)))*Pulse;
				const FVector Lift=FVector::CrossProduct(Axis,Cross);
				const int32 Nose=Vertex(P+Axis*(R*1.4),.9f,2.93f), Tail=Vertex(P-Axis*(R*2.25),.1f,2.33f);
				const int32 Ring=Water.VertexCount();
				constexpr int32 Sides=6;
				for(int32 J=0;J<Sides;++J)
				{
					const float Theta=2*PI*J/Sides;
					Vertex(P+R*(Cross*FMath::Cos(Theta)+Lift*FMath::Sin(Theta)),.5f,2.56f+.32f*FMath::Sin(Theta));
				}
				for(int32 J=0;J<Sides;++J) { Triangle(Nose,Ring+J,Ring+(J+1)%Sides); Triangle(Tail,Ring+(J+1)%Sides,Ring+J); }
			}
		}
	};

	void DrawWater(UDynamicMeshComponent& Mesh,const UHALWaterCannonConfig& C,const FHALWaterJetView& View,
		float Age,float Head,float Rear,float Remaining)
	{
		if(View.Reach.Num()!=Lanes) { return; }
		FWaterGeometry Shape;
		const float Opening=FMath::Tan(FMath::DegreesToRadians(.5f*C.FanAngle));
		const float Travel=Age*C.FlowSpeed*.01f;
		const auto HalfWidth=[&](float X){ return C.NozzleRadius+X*Opening; };
		for(int32 Face=0;Face<2;++Face)
		{
			const int32 Base=Shape.Water.VertexCount();
			for(int32 Lane=0;Lane<Lanes;++Lane)
			{
				const float S=2.f*Lane/(Lanes-1)-1.f;
				const float Reach=View.Reach[Lane]/255.f;
				const bool Hit=(View.ContactMask&(1u<<Lane))!=0;
				// Three unequal fingers and travelling notches break the free lip.
				const float Lip=.92f-.058f*S*S+.039f*FMath::Sin(8.4f*S-Travel*.59f)+.026f*FMath::Sin(17.f*S+Travel*.39f);
				const float End=FMath::Min(Reach,Hit?Head:Head*Lip);
				const float RearWave=.042f*FMath::Sin(9*S+Travel*.42f)+.019f*FMath::Sin(16*S-Travel*.51f);
				const float Begin=FMath::Clamp(Rear+FMath::Sin(PI*Rear)*RearWave,0.f,End);
				for(int32 Row=0;Row<Rows;++Row)
				{
					const float T=FMath::Lerp(Begin,End,float(Row)/(Rows-1)), X=T*C.Range;
					const float Wave=X*.029f-Travel*1.30f;
					const float Edge=FMath::Abs(S), Grow=FMath::SmoothStep(0.f,.22f,T);
					const float OnSurface=Hit?FMath::SmoothStep(Reach-.08f,Reach,T):0;
					const float Shift=Grow*(1-OnSurface)*(.092f*FMath::Sin(Wave*.65f)
						+Edge*(.115f*FMath::Sin(Wave+S*2.4f)+.067f*FMath::Sin(Wave*1.77f-S*4.2f)));
					const float CoreScale=FMath::Lerp(.80f+.05f*FMath::Sin(Wave*.48f),1.f,OnSurface);
					const float Y=HalfWidth(X)*FMath::Clamp(S*CoreScale+Shift,-1.f,1.f)*Remaining;
					const float Radius=FMath::Lerp(C.NozzleRadius,C.HalfHeight,.73f*FMath::Sqrt(T));
					const float Roll=Grow*(1-OnSurface)*(FMath::Sin(Wave*.82f+S*3.4f)*.27f+FMath::Sin(Wave*1.6f-S)*.11f);
					const float Z=Radius*Remaining*((Face?-1.f:1.f)*FMath::Sqrt(FMath::Max(0.f,1-S*S))+Roll);
					Shape.Vertex(FVector(X,Y,Z),X*.01f,(S+1.f)*.5f+(Face?1.f:0.f));
					// Do not stretch triangles across a deep step between a wall and open air.
					if(!Row||!Lane||FMath::Abs(int32(View.Reach[Lane])-View.Reach[Lane-1])>24) { continue; }
					const int32 D=Base+Lane*Rows+Row, C0=D-1, B=D-Rows, A=B-1;
					if(Face) { Shape.Triangle(A,C0,B); Shape.Triangle(B,C0,D); }
					else { Shape.Triangle(A,B,C0); Shape.Triangle(B,D,C0); }
				}
			}
		}
		if(C.SplashSize>0)
		{
			for(int32 Lane=1;Lane<Lanes;Lane+=3)
			{
				if(View.ContactMask&(1u<<Lane)) { continue; }
				const FVector Tip=Shape.Water.GetVertex(Lane*Rows+Rows-1);
				if(Tip.X<=Rear*C.Range+.1f) { continue; }
				const float S=2.f*Lane/(Lanes-1)-1.f;
				Shape.Splash(Tip,FVector(.81,S*.44,.38).GetSafeNormal(),0,
					C.SplashSize*.80f*Remaining*FMath::SmoothStep(0.f,.15f,Head),Age,4,Lane*.39f,false);
			}
			for(const auto& Hit:View.Contacts)
			{
				const float X=Hit.Point.X;
				if(Head*C.Range+2<X) { continue; }
				const float Span=HalfWidth(FMath::Max(0.f,X))*(Hit.LastLane-Hit.FirstLane)/(Lanes-1)*.83f;
				Shape.Splash(Hit.Point,Hit.Normal.GetSafeNormal(UE_SMALL_NUMBER,FVector::BackwardVector),Span,
					C.SplashSize*1.92f*Remaining,Age,11+(Hit.LastLane-Hit.FirstLane)/2,Hit.FirstLane*.31f,true);
			}
		}
		Mesh.SetMesh(MoveTemp(Shape.Water));
	}
}

void AHALWaterCannon::Present()
{
	const float Age=FMath::Max(0.,ServerTime()-StartTime);
	const bool bAlive=Age<Config->Duration;
	if(!bSirenStarted&&Config->Siren&&bAlive)
	{
		Alarm->SetSound(Config->Siren);
		Alarm->bOverrideAttenuation=true;
		Alarm->AttenuationOverrides.bAttenuate=true; Alarm->AttenuationOverrides.bSpatialize=true;
		Alarm->AttenuationOverrides.AttenuationShapeExtents=FVector(Config->SirenInnerRadius);
		Alarm->AttenuationOverrides.FalloffDistance=Config->SirenFalloff;
		Alarm->SetVolumeMultiplier(Config->SirenVolume); Alarm->Play(Age); bSirenStarted=true;
	}
	if(!bAlive && bSirenStarted) { Alarm->Stop(); bSirenStarted=false; }
	if(!bAlive||!View.bFlowing||View.Reach.Num()!=LaneCount||!Config->WaterMaterial)
	{
		ReleaseVisual();
		SetActorTransform(Frame(true));
		return;
	}
	SetActorTransform(Frame(true));
	if(bRestartFlow) { FlowStartAge=Age; bRestartFlow=false; }
	Mesh->SetVisibility(true);
	if(!WaterMID)
	{
		WaterMID=UMaterialInstanceDynamic::Create(Config->WaterMaterial,GetTransientPackage());
		Mesh->SetMaterial(0,WaterMID);
		WaterMID->SetVectorParameterValue(TEXT("ShadowColor"),Config->ShadowColor);
		WaterMID->SetVectorParameterValue(TEXT("MidColor"),Config->MidColor);
		WaterMID->SetVectorParameterValue(TEXT("LightColor"),Config->LightColor);
		WaterMID->SetScalarParameterValue(TEXT("ToneSoftness"),Config->ToneSoftness);
	}
	WaterMID->SetScalarParameterValue(TEXT("FlowMeters"),Age*Config->FlowSpeed*.01f);
	if(LastMeshAge<0 || Age<LastMeshAge || Age-LastMeshAge>=1.f/30 || LastDrawn.ContactMask!=View.ContactMask
		|| LastDrawn.Contacts.Num()!=View.Contacts.Num())
	{
		BuildWaterMesh(Age);
		LastDrawn=View; LastMeshAge=Age;
	}
}

void AHALWaterCannon::BuildWaterMesh(float Age)
{
	const float Head=FMath::SmoothStep(0.f,FMath::Max(.03f,Config->SprayInSeconds),Age-FlowStartAge);
	DrawWater(*Mesh,*Config,View,Age,Head,0,1);
}

void AHALWaterCannon::ReleaseVisual()
{
	Mesh->SetVisibility(false);
	if(LastMeshAge<0) { bRestartFlow=true; return; }
	if(WaterMID&&Config&&LastMeshAge>FlowStartAge&&GetWorld()&&!GetWorld()->bIsTearingDown&&FApp::CanEverRender())
	{
		FActorSpawnParameters Spawn;
		Spawn.OverrideLevel=GetLevel(); Spawn.ObjectFlags|=RF_Transient;
		if(auto* Tail=GetWorld()->SpawnActor<AHALWaterCannonTail>(AHALWaterCannonTail::StaticClass(),GetActorTransform(),Spawn))
		{
			const float Head=FMath::SmoothStep(0.f,FMath::Max(.03f,Config->SprayInSeconds),LastMeshAge-FlowStartAge);
			Tail->Start(*Config,LastDrawn,LastMeshAge,Head,*WaterMID);
		}
	}
	WaterMID=nullptr; LastDrawn={}; LastMeshAge=-1; bRestartFlow=true;
}

AHALWaterCannonTail::AHALWaterCannonTail()
{
	PrimaryActorTick.bCanEverTick=true;
	PrimaryActorTick.TickGroup=TG_PostPhysics; PrimaryActorTick.TickInterval=1.f/30.f;
	Mesh=CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DrainingWater")); SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false); Mesh->SetCastShadow(false);
}

void AHALWaterCannonTail::Start(UHALWaterCannonConfig& Settings,const FHALWaterJetView& LastView,float Age,float Head,UMaterialInstanceDynamic& Material)
{
	Config=&Settings; View=LastView; InitialAge=Age; InitialHead=Head;
	WaterMID=&Material; Mesh->SetMaterial(0,WaterMID);
	DrawWater(*Mesh,*Config,View,Age,Head,0,1);
}

void AHALWaterCannonTail::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); Elapsed+=DeltaSeconds;
	if(!Config || Elapsed>=Config->SprayOutSeconds) { Destroy(); return; }
	const float Phase=Elapsed/FMath::Max(Config->SprayOutSeconds,.03f);
	const float Head=FMath::Min(1.f,InitialHead+Elapsed/FMath::Max(Config->SprayInSeconds,.03f));
	const float Rear=FMath::SmoothStep(0.f,1.f,Phase);
	const float Remaining=1.f-FMath::SmoothStep(.12f,1.f,Phase);
	WaterMID->SetScalarParameterValue(TEXT("SprayOpacity"),Remaining);
	WaterMID->SetScalarParameterValue(TEXT("FlowMeters"),(InitialAge+Elapsed)*Config->FlowSpeed*.01f);
	DrawWater(*Mesh,*Config,View,InitialAge+Elapsed,Head,Rear,Remaining);
}
