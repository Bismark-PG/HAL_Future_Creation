#include "HALMenuFeedback.h"
FText HALMenuFeedback::Connection(const FText& Detail)
{
	const FString S=Detail.ToString();
	if (S.IsEmpty()) { return FText::FromString(TEXT("Create a room, or enter the host's IPv4 address to join.")); }
	if (S.Contains(TEXT("Connecting to")) || S.Contains(TEXT("Opening Listen"))) { return FText::FromString(TEXT("Connecting to your room...")); }
	if (S.Contains(TEXT("started")) || S.Contains(TEXT("locked"))) { return FText::FromString(TEXT("This match has already started. Join the next round.")); }
	if (S.Contains(TEXT("full"))) { return FText::FromString(TEXT("This room is full. Choose another host or create a room.")); }
	if (S.Contains(TEXT("Host closed"))) { return FText::FromString(TEXT("The host closed the room. Create or join another room.")); }
	if (S.Contains(TEXT("Left the room"))) { return FText::FromString(TEXT("You left the room. Ready for another match?")); }
	if (S.Contains(TEXT("numeric host IPv4"))) { return FText::FromString(TEXT("Enter an IPv4 address, for example 192.168.1.10. On the same computer use 127.0.0.1.")); }
	if (S.Contains(TEXT("cancelled"))) { return FText::FromString(TEXT("Connection cancelled. You can try another room.")); }
	if (S.Contains(TEXT("not configured")) || S.Contains(TEXT("destination"))) { return FText::FromString(TEXT("This arena is unavailable. Ask the host to choose an available arena.")); }
	return FText::FromString(TEXT("Unable to connect. Check the host address and connection, then try again."));
}
