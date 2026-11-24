#include "HALBatarang.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "HALBallComicFXSettings.h"
#include "HALBatarangAbility.h"
#include "HALBatarangWidget.h"
#include "HALSmokeScreenView.h"
#include "HALVehiclePresentationComponent.h"
#include "HALVehiclePresentationConfig.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "TestVehiclePawn.h"
#include "VehicleDefinition.h"

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
		// Graphite facets and a narrow steel inlay for the procedural blade.
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
		const FLinearColor Colors[] = {FLinearColor(.008f,.009f,.012f),FLinearColor(.09f,.10f,.11f),FLinearColor(.32f,.34f,.36f)};
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
void AHALBatarang::BindPinToBody(const FTransform& Frame)
{
	const auto* Look = State.Target->FindComponentByClass<UHALVehiclePresentationComponent>();
	if (!Look) { return; }
	FHALVehicleFXTargetSnapshot Snapshot;
	Look->CaptureStatusFXTargets(Snapshot);
	if (Snapshot.Targets.IsEmpty() || Snapshot.Targets[0].Id != TEXT("Body")) { return; }
	const FVector Normal = Frame.TransformVectorNoScale(State.LocalNormal);
	const FVector Up = Frame.GetUnitAxis(EAxis::Z);
	const auto& Body = Snapshot.Targets[0];
	const auto Hull = State.Target->GetRootComponent()->CalcBounds(FTransform::Identity);
	const FVector Portion = (State.LocalAnchor-Hull.Origin)/Hull.BoxExtent.ComponentMax(FVector(1));
	const FVector Guess = Body.VehicleBounds.GetCenter() + Portion.BoundToBox(FVector(-1),FVector(1))*Body.VehicleBounds.GetExtent();
	const FVector MeshGuess = Body.MeshToWorld.InverseTransformPosition(Snapshot.VehicleToWorld.TransformPosition(Guess));
	const auto* Profile = State.Config->PinProfiles.FindByPredicate([&Body](const FHALBatarangPinProfile& P)
	{
		return P.Mesh.Get() == Body.MeshAsset.Get() && P.Radii.Num() >= 4;
	});
	const FVector Extent = Body.LocalBounds.GetExtent().ComponentMax(FVector(1));
	FVector Center = Body.LocalBounds.GetCenter();
	Center.Z -= .16*Extent.Z;
	if (Profile) { Center = Profile->Center; }
	FVector Outward = (MeshGuess-Center).GetSafeNormal2D();
	if (Outward.IsNearlyZero()) { Outward = Body.MeshToWorld.InverseTransformVectorNoScale(Normal).GetSafeNormal2D(UE_SMALL_NUMBER,FVector::ForwardVector); }
	// An unbaked/custom mesh still has a rounded waist. Production cars use a cut of their rendered triangles.
	double Radius = .92/FMath::Sqrt(FMath::Square(Outward.X/Extent.X)+FMath::Square(Outward.Y/Extent.Y));
	if (Profile)
	{
		const int32 Count = Profile->Radii.Num();
		const double Angle = FMath::Atan2(Outward.Y,Outward.X);
		const double Sample = (Angle < 0 ? Angle + UE_TWO_PI : Angle)*Count/UE_TWO_PI;
		const int32 Floor = FMath::FloorToInt(Sample);
		Radius = FMath::Lerp(Profile->Radii[Floor%Count],Profile->Radii[(Floor+1)%Count],Sample-Floor);
	}
	PinnedMesh = Body.Mesh;
	MeshAnchor = Center + Outward*Radius;
	MeshNormal = Outward;
	MeshUp = Body.MeshToWorld.InverseTransformVectorNoScale(Up);
	PinBone = NAME_None;
	const auto* Definition = State.Target->GetVehicleDefinition();
	if (const auto* Skel = Cast<USkeletalMeshComponent>(Body.Mesh.Get()); Skel && Definition && Definition->Presentation)
	{
		const FName Chassis = Definition->Presentation->ChassisBone;
		const int32 Index = Skel->GetBoneIndex(Chassis);
		if (Index != INDEX_NONE)
		{
			const FTransform Ref(Skel->GetSkeletalMeshAsset()->GetComposedRefPoseMatrix(Index));
			MeshAnchor = Ref.InverseTransformPosition(MeshAnchor);
			MeshNormal = Ref.InverseTransformVectorNoScale(MeshNormal);
			MeshUp = Ref.InverseTransformVectorNoScale(MeshUp);
			PinBone = Chassis;
		}
	}
}

FTransform AHALBatarang::PinTransform()
{
	const FTransform Frame = State.Target->GetActorTransform();
	FVector Point = Frame.TransformPosition(State.LocalAnchor);
	FVector Normal = Frame.TransformVectorNoScale(State.LocalNormal);
	FVector Up = Frame.GetUnitAxis(EAxis::Z);
	if (!PinnedMesh.IsValid()) { BindPinToBody(Frame); }
	if (const auto* Mesh = PinnedMesh.Get())
	{
		const FTransform Pose = PinBone.IsNone() ? Mesh->GetComponentTransform() : Mesh->GetSocketTransform(PinBone);
		Point = Pose.TransformPosition(MeshAnchor);
		Normal = Pose.TransformVectorNoScale(MeshNormal);
		Up = Pose.TransformVectorNoScale(MeshUp);
	}
	return FTransform(FRotationMatrix::MakeFromXZ(-Normal,Up).ToQuat(),Point);
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
		const auto WidgetClass = State.Config->PinWidgetClass ? State.Config->PinWidgetClass.Get() : UHALBatarangWidget::StaticClass();
		OwnerView = CreateWidget<UHALBatarangWidget>(PC,WidgetClass);
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
	const bool bHiddenTarget = (bPinned || bBurst) && (!IsValid(State.Target)
		|| UHALSmokeScreenView::ShouldHideWorldTarget(State.Target));
	if (bPinned) { bPinWasConcealed = bHiddenTarget; }
	// The crest stays at the contact point even if the impulse carries the target out of smoke.
	if (bBurst && (bPinWasConcealed || bHiddenTarget)) { bBurstConcealed = true; }
	if (bHiddenTarget || bBurstConcealed)
	{
		Shape->SetVisibility(false);
		if (Blade) { Blade->SetVisibility(false); }
		if (Wake) { Wake->SetVisibility(false); }
		for (const auto& Cloud : BurstClouds) { Cloud->SetVisibility(false); }
		return;
	}
	FTransform Pin = FTransform::Identity;
	// A late observer can first receive Burst. Reconstruct the shell offset in the target's
	// current pose, then apply it to the published pre-impact position and direction.
	if (bPinned || (bBurst && !bHasBurstContactOffset))
	{
		Pin = PinTransform();
		const FVector HullPoint = State.Target->GetActorTransform().TransformPosition(State.LocalAnchor);
		BurstContactOffset = PushDirection().Rotation().UnrotateVector(Pin.GetLocation()-HullPoint);
		bHasBurstContactOffset = true;
	}
	// Start preparing the hidden fire materials while the fuse is running.
	if (bPinned && BurstClouds.IsEmpty()) { PrepareBurstClouds(); }
	if (State.Config->BladeMesh)
	{
		if (!bArtReady) { PrepareArt(); }
		PresentArt(Age,Pin);
		return;
	}
	if (bBurst && State.Config->BurstCloudMesh)
	{
		const FVector Origin = State.BurstPoint+State.BurstDirection.Rotation().RotateVector(BurstContactOffset);
		Shape->SetVisibility(false);
		PresentBurstClouds(Origin,FMath::Clamp(Age/float(HALBatarang::BurstSeconds),0.f,1.f));
		return;
	}
	Shape->SetVisibility(true);
	if (!bBuiltGeometry || bBuiltBurst != bBurst) { BuildGeometry(bBurst); }
	if (bBurst)
	{
		const float T = FMath::Clamp(Age/float(HALBatarang::BurstSeconds),0.f,1.f);
		const FVector Origin = State.BurstPoint+State.BurstDirection.Rotation().RotateVector(BurstContactOffset);
		Shape->SetWorldLocationAndRotation(Origin+FVector(0,0,6),State.BurstDirection.Rotation());
		Shape->SetWorldScale3D(FVector(.25+2.1*FMath::Sqrt(T)));
		if (Accent) { Accent->SetScalarParameterValue(TEXT("Brightness"),2.5f*(1-T)); }
		Shape->SetVisibility(T<.96f);
		return;
	}
	if (bPinned)
	{
		const double TipOffset = State.Config->WingSpan*(State.Config->PinDepthPercent*.01-.23);
		Shape->SetWorldLocationAndRotation(Pin.GetLocation()+Pin.GetUnitAxis(EAxis::X)*TipOffset,Pin.GetRotation());
		if (Accent)
		{
			Accent->SetScalarParameterValue(TEXT("Brightness"),.65f+1.35f*State.Config->FusePulse(State.Config->FuseSeconds-Age));
		}
		return;
	}
	const float T = FMath::Clamp(Age/State.Config->FlightSeconds,0.f,1.f);
	Shape->SetWorldLocationAndRotation(FlightPoint(T),FRotator(0,State.Yaw+State.Mirror*Age*840.,14));
}
