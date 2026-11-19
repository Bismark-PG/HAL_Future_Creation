#include "HALVehicleHUDStyleLibrary.h"
#include "HALVehicleCatalog.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	bool HUDColorFinite(const FLinearColor& C)
	{
		return FMath::IsFinite(C.R) && FMath::IsFinite(C.G) && FMath::IsFinite(C.B)
			&& C.R >= 0 && C.G >= 0 && C.B >= 0;
	}
}

bool HALVehicleHUD::DominantColor(TConstArrayView<FColorAreaSample> Samples, FLinearColor& OutColor)
{
	struct FBin { double Area = 0; FVector3d Sum = FVector3d::ZeroVector; };
	TMap<uint32, FBin> Bins;
	for (const auto& Sample : Samples)
	{
		if (!FMath::IsFinite(Sample.Area) || Sample.Area <= 0 || Sample.Color.A == 0) { continue; }
		const uint32 Key = (uint32(Sample.Color.R >> 4) << 8) | (uint32(Sample.Color.G >> 4) << 4) | (Sample.Color.B >> 4);
		const FLinearColor C(Sample.Color);
		auto& Bin = Bins.FindOrAdd(Key); Bin.Area += Sample.Area;
		Bin.Sum += FVector3d(C.R, C.G, C.B) * Sample.Area;
	}
	uint32 BestKey = MAX_uint32; double BestArea = 0;
	for (const auto& Pair : Bins)
	{
		if (Pair.Value.Area > BestArea || (Pair.Value.Area == BestArea && Pair.Key < BestKey))
		{ BestKey = Pair.Key; BestArea = Pair.Value.Area; }
	}
	OutColor = FLinearColor::White;
	if (BestArea <= 0) { return false; }
	const FVector3d Mean = Bins[BestKey].Sum / BestArea;
	OutColor = FLinearColor(float(Mean.X), float(Mean.Y), float(Mean.Z), 1);
	return true;
}

FLinearColor HALVehicleHUD::BrightOutline(FLinearColor Color)
{
	if (!HUDColorFinite(Color)) { return FLinearColor::White; }
	FLinearColor HSV = Color.GetClamped().LinearRGBToHSV();
	HSV.B = FMath::Max(HSV.B, .85f);
	Color = HSV.HSVToLinearRGB(); Color.A = 1;
	return Color;
}

bool UHALVehicleHUDStyleLibrary::GetVehicleOutlineColor(const UHALVehicleCatalog* Catalog, FName VehicleId, FLinearColor& Color)
{
	Color = FLinearColor::White;
	const auto* Entry = Catalog && !VehicleId.IsNone()
		? Catalog->Vehicles.FindByPredicate([VehicleId](const FHALVehicleCatalogEntry& E) { return E.VehicleId == VehicleId; }) : nullptr;
	if (!Entry || !Entry->bHasHUDDominantColor || Entry->HUDColorSource.IsNull() || !HUDColorFinite(Entry->HUDDominantColor)) { return false; }
	Color = HALVehicleHUD::BrightOutline(Entry->HUDDominantColor);
	return true;
}

bool UHALVehicleHUDStyleLibrary::ApplyVehicleOutline(UImage* Background, const UHALVehicleCatalog* Catalog, FName VehicleId)
{
	if (!IsValid(Background)) { return false; }
	const auto* Material = Cast<UMaterialInterface>(Background->GetBrush().GetResourceObject());
	FLinearColor Existing;
	const FMaterialParameterInfo Parameter(TEXT("OutlineColor"));
	if (!Material || !Material->GetVectorParameterValue(Parameter, Existing)) { return false; }
	FLinearColor Color; const bool bFound = GetVehicleOutlineColor(Catalog, VehicleId, Color);
	if (auto* Dynamic = Background->GetDynamicMaterial()) { Dynamic->SetVectorParameterValue(TEXT("OutlineColor"), Color); }
	return bFound;
}
