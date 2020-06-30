// Copyright Delta-Proxima Team (c) 2007-2020

#include "Net/RwIpNetDriver.h"

bool URwIpNetDriver::IsLevelInitializedForActor(const AActor* InActor, const UNetConnection* InConnection) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	check(InActor);
	check(InConnection);
#endif

	// we can't create channels while the client is in the wrong world
	const bool bCorrectWorld = GetWorldPackage() != nullptr && (InConnection->GetClientWorldPackageName() == GetWorldPackage()->GetFName()) && InConnection->ClientHasInitializedLevelFor(InActor);

	// exception: Special case for PlayerControllers as they are required for the client to travel to the new world correctly			
	const bool bIsConnectionPC = (InActor == InConnection->PlayerController);
	return bCorrectWorld || bIsConnectionPC;
}
