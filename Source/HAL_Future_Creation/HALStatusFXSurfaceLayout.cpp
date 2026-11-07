#include "HALStatusFXSurfaceLayout.h"
#include "Materials/MaterialInterface.h"
UHALStatusFXSurfaceLayout::UHALStatusFXSurfaceLayout()
{
	auto& A = Channels.AddDefaulted_GetRef(); A.Id = TEXT("Primary"); A.Prefix = TEXT("FX0_");
	auto& B = Channels.AddDefaulted_GetRef(); B.Id = TEXT("Secondary"); B.Prefix = TEXT("FX1_");
}
bool UHALStatusFXSurfaceLayout::Validate(FString& Error) const
{
	Error.Reset();
	if (!Material || Channels.IsEmpty() || Channels.Num() > 16) { Error = TEXT("Surface layout needs a material and 1..16 display channels."); return false; }
	for (int32 I = 0; I < Channels.Num(); ++I)
	{
		const auto& C = Channels[I];
		if (C.Id.IsNone() || C.Prefix.IsNone()) { Error = TEXT("Channel Id and Prefix must be nonempty."); return false; }
		for (int32 J = 0; J < I; ++J)
		{
			const auto& P = Channels[J];
			if (C.Id == P.Id || C.Prefix.ToString().StartsWith(P.Prefix.ToString(), ESearchCase::IgnoreCase)
				|| P.Prefix.ToString().StartsWith(C.Prefix.ToString(), ESearchCase::IgnoreCase))
			{ Error = TEXT("Channel Ids must be unique and parameter prefixes must not overlap."); return false; }
		}
	}
	return true;
}
