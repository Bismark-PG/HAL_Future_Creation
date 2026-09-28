// Copyright Epic Games, Inc. All Rights Reserved.

#include "HALNetworkMenuWidget.h"

#include "HALNetworkGameInstance.h"

bool UHALNetworkMenuWidget::HostMatch()
{
	UHALNetworkGameInstance* Network = GetGameInstance<UHALNetworkGameInstance>();
	return Network && Network->HostMatch();
}

bool UHALNetworkMenuWidget::JoinMatch(const FString& HostIPv4)
{
	UHALNetworkGameInstance* Network = GetGameInstance<UHALNetworkGameInstance>();
	return Network && Network->JoinMatch(HostIPv4);
}

FText UHALNetworkMenuWidget::GetConnectionMessage() const
{
	const UHALNetworkGameInstance* Network = GetGameInstance<UHALNetworkGameInstance>();
	return Network ? Network->GetConnectionMessage() : FText::GetEmpty();
}
