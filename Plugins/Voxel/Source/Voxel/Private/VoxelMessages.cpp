// Copyright 2020 Phyronnaz

#include "VoxelMessages.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MessageDialog.h"

FVoxelMessages::FLogMessageDelegate FVoxelMessages::LogMessageDelegate;
FVoxelMessages::FShowNotificationDelegate FVoxelMessages::ShowNotificationDelegate;

void FVoxelMessages::LogMessage(const TSharedRef<FTokenizedMessage>& Message, EVoxelShowNotification ShouldShow)
{
	if (LogMessageDelegate.IsBound())
	{
		LogMessageDelegate.Broadcast(Message, ShouldShow);
	}
	else
	{
		FMessageLog("PIE").AddMessage(Message);
	}
}

void FVoxelMessages::LogMessage(const FText& Message, EMessageSeverity::Type Severity, EVoxelShowNotification ShouldShow, const UObject* Object)
{
	TSharedRef<FTokenizedMessage> NewMessage = FTokenizedMessage::Create(Severity);
	if (Object)
	{
		NewMessage->AddToken(FUObjectToken::Create(Object));
		NewMessage->AddToken(FTextToken::Create(FText::FromString(": ")));
	}
	NewMessage->AddToken(FTextToken::Create(Message));
	LogMessage(NewMessage, ShouldShow);
}

void FVoxelMessages::ShowNotification(
	uint64 UniqueId,
	const FText& Message,
	const FText& ButtonText,
	const FText& ButtonTooltip,
	const FSimpleDelegate& OnClick,
	bool bShowCloseButton)
{
	if (ShowNotificationDelegate.IsBound())
	{
		ShowNotificationDelegate.Broadcast(UniqueId, Message, ButtonText, ButtonTooltip, OnClick, bShowCloseButton);
	}
}