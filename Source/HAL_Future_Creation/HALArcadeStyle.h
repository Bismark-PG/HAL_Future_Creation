#pragma once
#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace HALArcade
{
	inline FLinearColor Ink() { return FLinearColor(.012f,.022f,.045f,1); }
	inline FLinearColor Panel() { return FLinearColor(.028f,.047f,.078f,.98f); }
	inline FLinearColor Muted() { return FLinearColor(.58f,.67f,.76f); }
	inline TSharedRef<STextBlock> Text(const FText& Value,int32 Size=18,FLinearColor Color=FLinearColor::White)
	{ return SNew(STextBlock).Text(Value).Font(FCoreStyle::GetDefaultFontStyle("Bold",Size)).ColorAndOpacity(Color); }
	inline TSharedRef<STextBlock> Text(const TCHAR* Value,int32 Size=18,FLinearColor Color=FLinearColor::White)
	{ return Text(FText::FromString(Value),Size,Color); }
	inline TSharedRef<SButton> Button(const TCHAR* Label,FSimpleDelegate Action)
	{
		return SNew(SButton).ContentPadding(FMargin(18,10)).HAlign(HAlign_Center)
			.OnClicked_Lambda([Action]{ Action.ExecuteIfBound(); return FReply::Handled(); })[Text(Label,17)];
	}
}
