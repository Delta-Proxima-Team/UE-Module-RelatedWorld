// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "IpNetDriver.h"
#include "RwIpNetDriver.generated.h"

UCLASS(transient, config = Engine)
class RELATEDWORLD_API URwIpNetDriver : public UIpNetDriver
{
	GENERATED_BODY()

public:
	virtual bool IsLevelInitializedForActor(const AActor* InActor, const UNetConnection* InConnection) const override;
};
