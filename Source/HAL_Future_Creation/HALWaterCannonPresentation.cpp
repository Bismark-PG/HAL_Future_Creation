#include "HALWaterCannon.h"

#include "Components/AudioComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "HALWaterCannonAbility.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

void AHALWaterCannon::Present()
{
	const float Age = FMath::Max(0.,ServerTime() - StartTime);
	SetActorTransform(Frame(true));
	const bool bWithinLifetime = Age < Config->Duration;
	if (!bSirenStarted && Config->Siren && bWithinLifetime)
	{
		Alarm->SetSound(Config->Siren);
		Alarm->bOverrideAttenuation = true;
		Alarm->AttenuationOverrides.bAttenuate = true;
		Alarm->AttenuationOverrides.bSpatialize = true;
		Alarm->AttenuationOverrides.AttenuationShapeExtents = FVector(Config->SirenInnerRadius);
		Alarm->AttenuationOverrides.FalloffDistance = Config->SirenFalloff;
		Alarm->SetVolumeMultiplier(Config->SirenVolume);
		Alarm->Play(Age);
		bSirenStarted = true;
	}
	if (!bWithinLifetime && bSirenStarted) { Alarm->Stop(); bSirenStarted = false; }
	const bool bDraw = bWithinLifetime && View.bFlowing && View.Reach.Num() == LaneCount && Config->WaterMaterial;
	Mesh->SetVisibility(bDraw);
	if (!bDraw) { return; }
	if (!WaterMID)
	{
		WaterMID = UMaterialInstanceDynamic::Create(Config->WaterMaterial,this);
		Mesh->SetMaterial(0,WaterMID);
		WaterMID->SetVectorParameterValue(TEXT("ShadowColor"),Config->ShadowColor);
		WaterMID->SetVectorParameterValue(TEXT("MidColor"),Config->MidColor);
		WaterMID->SetVectorParameterValue(TEXT("LightColor"),Config->LightColor);
		WaterMID->SetScalarParameterValue(TEXT("ToneSoftness"),Config->ToneSoftness);
	}
	WaterMID->SetScalarParameterValue(TEXT("FlowMeters"),Age * Config->FlowSpeed * .01f);
	if (!(View == LastDrawn) || (View.ContactMask && Config->SplashSize > 0 && Age - LastSplashTime >= 1./30.))
	{
		BuildWaterMesh(Age);
		LastDrawn = View;
		LastSplashTime = Age;
	}
}

void AHALWaterCannon::BuildWaterMesh(float Age)
{
	using namespace UE::Geometry;
	FDynamicMesh3 Water;
	Water.EnableAttributes();
	auto* UV = Water.Attributes()->PrimaryUV();
	auto Vertex = [&](const FVector& P, float U, float V)
	{
		const int32 Id = Water.AppendVertex(P);
		UV->AppendElement(FVector2f(U,V));
		return Id;
	};
	auto Triangle = [&](int32 A, int32 B, int32 C)
	{
		const int32 Id = Water.AppendTriangle(A,B,C);
		if (Id >= 0) { UV->SetTriangle(Id,FIndex3i(A,B,C)); }
	};
	// Curved upper and lower skins. Every strip ends at its own server-confirmed obstruction.
	constexpr int32 Rows = 5;
	for (int32 Face = 0; Face < 2; ++Face)
	{
		const int32 Base = Water.VertexCount();
		for (int32 Lane = 0; Lane < LaneCount; ++Lane)
		{
			const float Side = 2.f * Lane / (LaneCount-1) - 1;
			const float Tip = (View.ContactMask & (1u << Lane)) ? 1.f : 1.f - .055f*Side*Side;
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				const float Fraction = Tip * View.Reach[Lane] / 255.f * Row / (Rows-1);
				FVector P = LanePoint(Side,Fraction);
				const float Radius = FMath::Lerp(Config->NozzleRadius,Config->HalfHeight,.82f * FMath::Sqrt(Fraction));
				P.Z = (Face ? -1.f : 1.f) * Radius * FMath::Sqrt(FMath::Max(0.f,1.f-Side*Side));
				Vertex(P,P.X * .01f,Face ? 1.5f + Side*.48f : (Side + 1.f)*.5f);
				if (!Lane || !Row) { continue; }
				const int32 D = Base + Lane*Rows + Row, C = D-1, B = D-Rows, A = B-1;
				if (Face) { Triangle(A,C,B); Triangle(B,C,D); }
				else { Triangle(A,B,C); Triangle(B,D,C); }
			}
		}
	}
	// A contact across several adjacent lanes produces one crown of short, outgoing droplets.
	for (int32 First = 0; Config->SplashSize > 0 && First < LaneCount; )
	{
		if (!(View.ContactMask & (1u << First))) { ++First; continue; }
		int32 Last = First;
		while (Last+1 < LaneCount && (View.ContactMask & (1u << (Last+1))) && FMath::Abs(int32(View.Reach[Last+1])-View.Reach[Last]) <= 14) { ++Last; }
		const int32 Middle = (First + Last) / 2;
		const FVector Contact = LanePoint(2.f*Middle/(LaneCount-1)-1,View.Reach[Middle]/255.f);
		for (int32 Drop = 0; Drop < 9; ++Drop)
		{
			const float Phase = FMath::Frac(Age * Config->FlowSpeed / 620.f + Drop*.618034f);
			const float Angle = Drop * 2.39996f + Middle*.33f;
			const FVector Travel = FVector(-.32,FMath::Cos(Angle),.45 + .65*FMath::Sin(Angle)).GetSafeNormal();
			const FVector P = Contact + Travel * (Phase * Config->SplashSize);
			const float R = Config->SplashSize * .12f * (1.f-Phase);
			const FVector Y = FVector::CrossProduct(Travel,FVector::UpVector).GetSafeNormal() * R, Z = FVector::CrossProduct(Travel,Y);
			const int32 Tip = Vertex(P + Travel*(2.6f*R),0,.25f), Tail = Vertex(P - Travel*(1.7f*R),.08f,.8f);
			const int32 Rim = Vertex(P+Y,0,.18f);
			Vertex(P+Z,.04f,.3f); Vertex(P-Y,.08f,.62f); Vertex(P-Z,.04f,.91f);
			for (int32 I = 0; I < 4; ++I) { Triangle(Tip,Rim+I,Rim+(I+1)%4); Triangle(Tail,Rim+(I+1)%4,Rim+I); }
		}
		First = Last+1;
	}
	Mesh->SetMesh(MoveTemp(Water));
}
