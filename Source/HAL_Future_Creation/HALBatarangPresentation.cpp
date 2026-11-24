#include "HALBatarang.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "HALBallComicFXSettings.h"
#include "HALBatarangAbility.h"
#include "HALBatarangWidget.h"
#include "HALPlayerIdentitySettings.h"
#include "HALSmokeScreenView.h"
#include "HALVehiclePresentationComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "TestVehiclePawn.h"

void AHALBatarang::BuildGeometry(bool bBurst)
{
	using namespace UE::Geometry;
	FDynamicMesh3 Mesh;
	Mesh.EnableAttributes();
	Mesh.Attributes()->EnableMaterialID();
	auto Triangle = [&](FVector A,FVector B,FVector C,int32 Slot)
	{
		const int32 I=Mesh.AppendVertex(A), J=Mesh.AppendVertex(B), K=Mesh.AppendVertex(C);
		const int32 Face = Mesh.AppendTriangle(I,J,K);
		if (Face >= 0) { Mesh.Attributes()->GetMaterialID()->SetValue(Face,Slot); }
	};
	const double Span = State.Config->WingSpan;
	if (bBurst)
	{
		// Broken shock ring with longer shards on the pushing side. Mesh X is the resolved impulse.
		for (int32 I=0; I<16; ++I)
		{
			const double A = UE_TWO_PI*I/16., B = A+.30;
			const FVector D(FMath::Cos(A),FMath::Sin(A),0), E(FMath::Cos(B),FMath::Sin(B),0);
			Triangle(D*Span*.85,E*Span*.87,D*Span,1);
			Triangle(D*Span,E*Span*.87,E*Span,2);
			if (I%2 == 0)
			{
				const FVector Across(-D.Y,D.X,0);
				const double Reach = Span*(1.32+.48*D.X);
				Triangle(D*Span*.88+Across*Span*.07,D*Reach+FVector(0,0,Span*.13),D*Span*.92-Across*Span*.04,2);
			}
		}
	}
	else
	{
		// Swept crescent wings, graphite facets, silver cutting edges and a seat-colour stripe.
		for (const int32 Sign : {-1,1})
		{
			auto P = [&](double X,double Y,double Z) { return Span*FVector(X,Y*Sign,Z); };
			const FVector Nose=P(.23,0,.04), Spine=P(.01,0,.095), Tail=P(-.17,0,0);
			const FVector Shoulder=P(.07,.23,.025), Tip=P(-.22,.5,0), Notch=P(-.07,.24,.01);
			Triangle(Nose,Shoulder,Spine,1); Triangle(Spine,Shoulder,Notch,0);
			Triangle(Spine,Notch,Tail,0); Triangle(Shoulder,Tip,Notch,1);
			Triangle(P(.14,.038,.07),P(.045,.205,.052),P(-.13,.395,.022),2);
			Triangle(P(.14,.038,.07),P(-.13,.395,.022),P(.045,.13,.068),2);
		}
	}
	if (!bBuiltGeometry)
	{
		const FLinearColor Colors[] = {FLinearColor(.055f,.075f,.12f),FLinearColor(.52f,.67f,.70f),FLinearColor(.78f,1.f,.12f)};
		for (int32 Slot=0; Slot<3; ++Slot)
		{
			auto* MID = UMaterialInstanceDynamic::Create(State.Config->DartMaterial,this);
			if (!MID) { continue; }
			MID->SetVectorParameterValue(TEXT("Tint"),Colors[Slot]);
			Shape->SetMaterial(Slot,MID);
			if (Slot == 2) { Accent = MID; }
		}
	}
	Shape->SetMesh(MoveTemp(Mesh));
	bBuiltGeometry = true;
	bBuiltBurst = bBurst;
}
void AHALBatarang::Present(double Now)
{
	const bool bPinned = State.Phase == EHALDartPhase::Pinned;
	const bool bBurst = State.Phase == EHALDartPhase::Burst;
	const float Age = FMath::Max(0.f,float(Now-State.PhaseStart));
	auto* Car = Cast<ATestVehiclePawn>(GetOwner());
	auto* PC = Car ? Cast<APlayerController>(Car->GetController()) : nullptr;
	const bool bOwnerView = PC && PC->IsLocalController() && PC->GetLocalPlayer() && PC->GetPawn() == Car;
	if (bPinned && bOwnerView && !OwnerView)
	{
		OwnerView = CreateWidget<UHALBatarangWidget>(PC,UHALBatarangWidget::StaticClass());
		if (OwnerView)
		{
			OwnerView->Dart = this;
			OwnerView->AddToPlayerScreen(GetDefault<UHALBallComicFXSettings>()->Comic.IncomingZOrder-1);
		}
	}
	else if (OwnerView && (!bPinned || !bOwnerView)) { OwnerView->RemoveFromParent(); OwnerView = nullptr; }
	if (bPinned && bOwnerView && !bPlayedPin)
	{
		if (State.Config->PinSignal) { UGameplayStatics::PlaySound2D(this,State.Config->PinSignal,.64f); }
		bPlayedPin = true;
	}
	const bool bConcealed = (bPinned && !IsValid(State.Target))
		|| ((bPinned || bBurst) && IsValid(State.Target) && UHALSmokeScreenView::ShouldHideWorldTarget(State.Target));
	Shape->SetVisibility(!bConcealed);
	if (bConcealed) { return; }
	if (!bBuiltGeometry || bBuiltBurst != bBurst) { BuildGeometry(bBurst); }
	if (bBurst)
	{
		const float T = FMath::Clamp(Age/.45f,0.f,1.f);
		Shape->SetWorldLocationAndRotation(State.BurstPoint+FVector(0,0,6),State.BurstDirection.Rotation());
		Shape->SetWorldScale3D(FVector(.25+2.1*FMath::Sqrt(T)));
		if (Accent) { Accent->SetScalarParameterValue(TEXT("Brightness"),2.5f*(1-T)); }
		Shape->SetVisibility(T<.96f);
		return;
	}
	if (bPinned)
	{
		const FTransform Frame = State.Target->GetActorTransform();
		FVector Point = Frame.TransformPosition(State.LocalAnchor);
		FVector Normal = Frame.TransformVectorNoScale(State.LocalNormal);
		if (!PinnedMesh.IsValid())
		{
			if (const auto* Look = State.Target->FindComponentByClass<UHALVehiclePresentationComponent>())
			{
				FHALVehicleFXTargetSnapshot Snapshot;
				Look->CaptureStatusFXTargets(Snapshot);
				if (!Snapshot.Targets.IsEmpty())
				{
					const auto& Body = Snapshot.Targets[0];
					const auto Hull = State.Target->GetRootComponent()->CalcBounds(FTransform::Identity);
					const FVector Portion = (State.LocalAnchor-Hull.Origin)/Hull.BoxExtent.ComponentMax(FVector(1));
					const FBox& Silhouette = Body.VehicleBounds;
					const FVector RootAnchor = Silhouette.GetCenter()+Portion.BoundToBox(FVector(-1),FVector(1))*Silhouette.GetExtent()*.98;
					PinnedMesh = Body.Mesh;
					MeshAnchor = Body.MeshToWorld.InverseTransformPosition(Frame.TransformPosition(RootAnchor));
					MeshNormal = Body.MeshToWorld.InverseTransformVectorNoScale(Normal);
				}
			}
		}
		if (const auto* Mesh = PinnedMesh.Get())
		{
			Point = Mesh->GetComponentTransform().TransformPosition(MeshAnchor);
			Normal = Mesh->GetComponentTransform().TransformVectorNoScale(MeshNormal);
		}
		Shape->SetWorldLocationAndRotation(Point+Normal*State.Config->WingSpan*.14,(-Normal).Rotation());
		if (Accent)
		{
			FLinearColor Tint(.78f,1.f,.12f);
			UHALPlayerIdentitySettings::ResolvePlayerColor(GetWorld(),Car,INDEX_NONE,Tint);
			Accent->SetVectorParameterValue(TEXT("Tint"),Tint);
			Accent->SetScalarParameterValue(TEXT("Brightness"),1.2f+.8f*FMath::Sin(Age*11.f));
		}
		return;
	}
	const float T = FMath::Clamp(Age/State.Config->FlightSeconds,0.f,1.f);
	Shape->SetWorldLocationAndRotation(FlightPoint(T),FRotator(0,State.Yaw+State.Mirror*Age*840.,14));
}
