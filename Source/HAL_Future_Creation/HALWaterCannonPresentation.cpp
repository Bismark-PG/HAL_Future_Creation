#include "HALWaterCannon.h"

#include "Components/AudioComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "HALWaterCannonAbility.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RandomStream.h"
#include "Sound/SoundBase.h"

namespace
{
	using namespace UE::Geometry;
	constexpr int32 Lanes=AHALWaterCannon::LaneCount;
	constexpr int32 Rows=27;

	struct FSplashArc
	{
		FVector Root;
		FVector Travel;
		FVector Bow;

		FVector At(float T) const { return Root + T * Travel + (4.f*T*(1.f-T)) * Bow; }
		FVector Tangent(float T) const { return Travel + (4.f-8.f*T) * Bow; }
	};

	// Internal fan edges border an occluded lane. Surface spray is clipped to the wet
	// half-space, including the thickness of the droplets, at every angle of incidence.
	struct FSprayClip
	{
		double MinY = -UE_BIG_NUMBER, MaxY = UE_BIG_NUMBER;
		FVector Point = FVector::ZeroVector, Normal = FVector::ZeroVector;

		FVector Constrain(FVector P) const
		{
			P.Y = FMath::Clamp(P.Y,MinY,MaxY);
			return P + Normal * FMath::Max(0.,FVector::DotProduct(Point-P,Normal));
		}
	};

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

		FVector Tip(int32 Lane) const { return Water.GetVertex(Lane*Rows+Rows-1); }

		void OpenSpray(int32 First, int32 Last, float LaneWidth, float Size, float Time, float HeadX)
		{
			const int32 Count = Last-First+1;
			Size *= .38f + .62f*FMath::Sqrt(static_cast<float>(Count)/Lanes);
			FSprayClip Clip;
			if (First>0) { Clip.MinY = Tip(First).Y - .45f*LaneWidth; }
			if (Last+1<Lanes) { Clip.MaxY = Tip(Last).Y + .45f*LaneWidth; }

			// A rolling lip joins the sheets along the actual wavy ends of this open band.
			int32 Previous[3] = {};
			for (int32 Lane=First; Lane<=Last; ++Lane)
			{
				const FVector P = Tip(Lane);
				const float Crest = .55f+.30f*FMath::Sin(Lane*.86f+Time*7.f)+.15f*FMath::Sin(Lane*1.53f-Time*5.f);
				const int32 Row[] = {
					FoamVertex(Clip,P+Size*FVector(-.44,0,-.03),.05f),
					FoamVertex(Clip,P+Size*FVector(.05+.16*Crest,0,.16+.63*Crest),.47f),
					FoamVertex(Clip,P+Size*FVector(.24+.42*Crest,0,.12+.50*Crest),.48f+.50f*Crest)
				};
				for (int32 R=0; R<2 && Lane>First; ++R)
				{
					Triangle(Previous[R],Row[R],Previous[R+1]); Triangle(Row[R],Row[R+1],Previous[R+1]);
				}
				for (int32 R=0; R<3; ++R) { Previous[R]=Row[R]; }
			}
			const int32 Sheets = 1+(Count+1)/3;
			for (int32 I=0; I<Sheets; ++I)
			{
				const int32 Lane = First + (Last-First)*I/FMath::Max(1,Sheets-1);
				const float Side = 2.f*Lane/(Lanes-1)-1.f;
				const float Ripple = FMath::Sin(Time*6.4f+Lane*1.81f);
				const FVector Root = Tip(Lane) + Size*FVector(-.32,0,.01);
				const float Forward = FMath::Min(Size*(1.82f-1.25f*FMath::Square(Side)+.48f*Ripple),
					FMath::Max(0.f,HeadX-static_cast<float>(Root.X))*.72f);
				const FVector Travel(Forward,Size*(2.1f*Side+.48f*FMath::Sin(Lane*1.31f+.8f)),
					Size*(.28f+.45f*Ripple));
				const FSplashArc Arc{Root,Travel,Size*FVector(.20,.12*Side,.36+.24*Ripple)};
				const FVector Across = FVector::CrossProduct(FVector::UpVector,Travel).GetSafeNormal(UE_SMALL_NUMBER,FVector::RightVector);
				FoamCap(Clip,Tip(Lane)+Size*FVector(.12,0,.30),FVector::ForwardVector,FVector::RightVector,FVector::UpVector,
					Size*(.82f+.14f*Ripple),Size*.68f,Size*.52f,Lane,Time);
				FoldSheet(Clip,Arc,Across,Size*(.35f+.12f*(1.f+Ripple)),Lane,Time);
				EjectDrops(Clip,Arc,Across,Size,Time,Lane,FMath::Min(6,2+Count/3));
			}
		}

		void ImpactSpray(const FHALWaterJetContact& Hit, float Span, float Size, float Time)
		{
			FSprayClip Clip;
			Clip.Point = Hit.Point;
			Clip.Normal = Hit.Normal.GetSafeNormal(UE_SMALL_NUMBER,FVector::BackwardVector);
			const FVector Side = FVector::CrossProduct(FVector::UpVector,Clip.Normal).GetSafeNormal(UE_SMALL_NUMBER,FVector::RightVector);
			const FVector FaceUp = FVector::CrossProduct(Clip.Normal,Side);
			const FVector Skim = FVector::VectorPlaneProject(FVector::ForwardVector,Clip.Normal);
			const int32 Coverage = 1+Hit.LastLane-Hit.FirstLane;
			Size *= .42f+.58f*FMath::Sqrt(static_cast<float>(Coverage)/Lanes);
			FoamCap(Clip,Clip.Point+Clip.Normal*Size*.24+FaceUp*Size*.18,Clip.Normal,Side,FaceUp,
				Size*.81f,FMath::Max(Size*.82f,Span*.78f),Size*.48f,Hit.FirstLane+23,Time);
			const int32 Sheets = 2+Coverage/4;
			for (int32 I=0; I<Sheets; ++I)
			{
				const int32 Seed = 37+Hit.FirstLane*7+I;
				const float Hand = I%2 ? 1.f : -1.f;
				const float Tier = static_cast<float>(I/2)/FMath::Max(1,(Sheets-1)/2);
				const float Pulse = FMath::Sin(Seed*2.31f+Time*(6.f+Tier));
				// Two low, spreading wings peel away along the panel, with shorter sheets
				// rebounding towards the shooter. The tangential flow carries glancing hits.
				const FVector Root = Clip.Point + Clip.Normal*2. + Side*Hand*Span*(.25+.55*Tier);
				const FVector Travel = Size*(Side*Hand*(2.9-1.25*Tier+.36*Pulse) + Skim*1.18
					+ Clip.Normal*(.56+1.40*Tier+.18*Pulse) + FaceUp*(.12+.34*Pulse));
				const FSplashArc Arc{Root,Travel,Size*(Clip.Normal*(.27+.18*Tier)+FaceUp*(.19+.19*Tier))};
				const FVector Across = FVector::CrossProduct(FVector::UpVector,Travel).GetSafeNormal(UE_SMALL_NUMBER,Side);
				FoldSheet(Clip,Arc,Across,Size*(.56f+.20f*Tier),Seed,Time);
				EjectDrops(Clip,Arc,Across,Size,Time,Seed,FMath::Min(6,2+Coverage/3));
			}
		}

	private:
		int32 FoamVertex(const FSprayClip& Clip, const FVector& P, float Tone)
		{
			return Vertex(Clip.Constrain(P),0,2.02f+.96f*Tone);
		}

		// A broken patch of whitewater at the pressure front joins the separate sheets.
		// Unequal lobes roll through the outline independently of the downstream water lines.
		void FoamCap(const FSprayClip& Clip, const FVector& Center, const FVector& Ahead, const FVector& Side,
			const FVector& Up, float Length, float Width, float Height, int32 Seed, float Time)
		{
			constexpr int32 Segments=16;
			const float Roll = Time*5.8f+Seed*1.73f;
			const int32 Hub = FoamVertex(Clip,Center+Up*(Height*.72f),.88f);
			const int32 Start = Water.VertexCount();
			for (int32 I=0; I<Segments; ++I)
			{
				const float Angle=2.f*PI*I/Segments;
				const float Curl=.52f+.27f*FMath::Sin(3.f*Angle+Roll)+.17f*FMath::Cos(5.f*Angle-Roll*.83f);
				const FVector Out=(Ahead*(Length*FMath::Cos(Angle))+Side*(Width*FMath::Sin(Angle)))*(.51f+.86f*Curl);
				FoamVertex(Clip,Center+.57*Out+Up*Height*(.63+.46*Curl),.51f+.45f*Curl);
				FoamVertex(Clip,Center+Out+Up*Height*(.18+.51*Curl),.59f+.40f*Curl);
				FoamVertex(Clip,Center+Out*1.06-Up*Height*.18,.19f+.31f*Curl);
			}
			for (int32 I=0; I<Segments; ++I)
			{
				const int32 A=Start+3*I, B=Start+3*((I+1)%Segments);
				Triangle(Hub,A,B);
				for (int32 J=0; J<2; ++J) { Triangle(A+J,A+J+1,B+J); Triangle(B+J,A+J+1,B+J+1); }
			}
		}

		// A folded film with an uneven, forked rim. The cyan underside and white crest
		// share vertices so they move together as the film tears into lobes.
		void FoldSheet(const FSprayClip& Clip, const FSplashArc& Arc, const FVector& Across, float Width, int32 Seed, float Time)
		{
			constexpr float Stations[] = {0.f,.36f,.68f,.87f,1.f};
			constexpr float Widths[] = {.52f,.79f,1.f,1.03f,.87f};
			constexpr float Shades[] = {.18f,.38f,.52f,.68f,.88f};
			constexpr int32 Columns=11;
			int32 Grid[Columns][5];
			const float Wave = Time*(6.4f+.35f*(Seed%4))+Seed*2.31f;
			const float Surge = .30f+.79f*FMath::SmoothStep(-.96f,.80f,FMath::Sin(Wave));
			for (int32 Col=0; Col<Columns; ++Col)
			{
				const float S = 2.f*Col/(Columns-1)-1.f;
				const float Shoulder = FMath::Sqrt(FMath::Max(0.f,1.f-S*S));
				const float Lobe = .5f+.5f*FMath::Sin((5.f+.55f*(Seed%4))*S +.75f*FMath::Sin(Wave)+Seed);
				const float Length = .27f+Shoulder*(.40f+.46f*FMath::SmoothStep(.05f,.95f,Lobe));
				for (int32 Row=0; Row<5; ++Row)
				{
					const float T = Stations[Row]*Length*Surge;
					const FVector P = Arc.At(T)+Across*Width*(Widths[Row]*S+.26f*FMath::Sin(Wave+3.f*T)*T)
						+ Arc.Bow*(.42f*S*S+.25f*Lobe)*T;
					Grid[Col][Row] = FoamVertex(Clip,P,Shades[Row]+.10f*Lobe-.07f*FMath::Abs(S));
					if (Col && Row)
					{
						Triangle(Grid[Col-1][Row-1],Grid[Col][Row-1],Grid[Col-1][Row]);
						Triangle(Grid[Col][Row-1],Grid[Col][Row],Grid[Col-1][Row]);
					}
				}
			}
		}

		void EjectDrops(const FSprayClip& Clip, const FSplashArc& Arc, const FVector& Across,
			float Size, float Time, int32 Seed, int32 Count)
		{
			for (int32 I=0; I<Count; ++I)
			{
				// Stagger a large drop and its satellites. Each birth gets a stable throw
				// until the droplet dies, independent of frame rate and of other contacts.
				const float Clock = Time*(1.62f+.12f*(Seed%4))+Seed*.381966f+(I/3)*.48f-(I%3)*.102f;
				const int32 Cycle = FMath::FloorToInt(Clock);
				const float Phase = Clock-Cycle;
				FRandomStream Random(1973*Seed+7907*(Cycle+4)+131*I);
				const float Distance = Random.FRandRange(1.35f,2.10f);
				const float Scatter = Random.FRandRange(-1.f,1.f)*(I<3 ? .95f : 1.74f);
				const float Rise = Random.FRandRange(.21f,1.36f);
				const FSplashArc Flight{Arc.At(.55f),Arc.Travel*Distance+Across*(Scatter*Size),Arc.Bow*.50+FVector::UpVector*(Rise*Size)};
				const float Envelope = FMath::SmoothStep(0.f,.13f,Phase)*(1.f-FMath::SmoothStep(.63f,1.f,Phase));
				const float Radius = Size*(I%3==0 ? .20f : Random.FRandRange(.075f,.158f))*Envelope;
				if (Radius < .3f) { continue; }
				const FVector Axis = Flight.Tangent(Phase).GetSafeNormal(UE_SMALL_NUMBER,FVector::UpVector);
				const FVector Cross = FVector::CrossProduct(FVector::UpVector,Axis).GetSafeNormal(UE_SMALL_NUMBER,FVector::RightVector);
				const FVector Lift = FVector::CrossProduct(Axis,Cross);
				const FVector P = Flight.At(Phase);
				const int32 Nose = FoamVertex(Clip,P+Axis*Radius*1.35,.97f);
				const int32 Tail = FoamVertex(Clip,P-Axis*Radius*(I%3==1 ? 3.8f : 1.9f),.46f);
				const int32 Ring = Water.VertexCount();
				constexpr int32 Sides=7;
				for (int32 J=0; J<Sides; ++J)
				{
					const float Angle = 2.f*PI*J/Sides;
					const FVector Radial=Cross*FMath::Cos(Angle)+Lift*FMath::Sin(Angle);
					FoamVertex(Clip,P+Radius*(Axis*.73+.80*Radial),.77f+.19f*FMath::Sin(Angle));
					FoamVertex(Clip,P+Radius*(-Axis*.29+Radial),.64f+.28f*FMath::Sin(Angle));
				}
				for (int32 J=0; J<Sides; ++J)
				{
					const int32 A=Ring+2*J, B=Ring+2*((J+1)%Sides);
					Triangle(Nose,A,B); Triangle(A,A+1,B); Triangle(B,A+1,B+1); Triangle(Tail,B+1,A+1);
				}
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
		const float FreeSize=C.SplashSize*C.FreeSplashScale*Remaining*FMath::SmoothStep(0.f,.25f,Head);
		if(FreeSize>.1f)
		{
			const auto HasOpenTip=[&](int32 Lane)
			{
				return !(View.ContactMask & (1u<<Lane)) && Shape.Tip(Lane).X>Rear*C.Range+.1f;
			};
			for(int32 Lane=0;Lane<Lanes;)
			{
				if(!HasOpenTip(Lane)) { ++Lane; continue; }
				const int32 First=Lane++;
				while(Lane<Lanes && HasOpenTip(Lane)) { ++Lane; }
				const float Spacing=2.f*HalfWidth(Head*C.Range)*Remaining/(Lanes-1);
				Shape.OpenSpray(First,Lane-1,Spacing,FreeSize,Age,Head*C.Range);
			}
		}
		const float ImpactSize=C.SplashSize*C.ImpactSplashScale*Remaining;
		if(ImpactSize>.1f)
		{
			for(const auto& Hit:View.Contacts)
			{
				const float X=Hit.Point.X;
				if(Head*C.Range+2<X) { continue; }
				const float HitFraction=FMath::Max(0.f,X/C.Range);
				const float Size=ImpactSize*(1.f-FMath::SmoothStep(HitFraction,HitFraction+.2f,Rear));
				if(Size<=.1f) { continue; }
				const float Span=HalfWidth(FMath::Max(0.f,X))*(Hit.LastLane-Hit.FirstLane)/(Lanes-1)*.83f;
				Shape.ImpactSpray(Hit,Span*Remaining,Size,Age);
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
