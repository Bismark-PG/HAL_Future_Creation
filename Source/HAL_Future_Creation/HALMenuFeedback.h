#pragma once
#include "CoreMinimal.h"
namespace HALMenuFeedback
{
	/** Internal connection diagnostics stay in the log; the screen offers an actionable message. */
	FText Connection(const FText& Detail);
}
