// Copyright Delta-Proxima Team (c) 2007-2020

#include "Net/RWReplicationGraphBase.h"
#include "WorldDirector.h"

void UReplicationGraphNode_RwDynamicNode::NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo)
{
	AActor* Actor = ActorInfo.Actor;
	FVector Location = Actor->GetActorLocation();
	FVector GlobalLocation = Location;

	FGlobalActorReplicationInfo& ActorRepInfo = GraphGlobals->GlobalActorReplicationInfoMap->Get(Actor);
	//ActorRepInfo.WorldLocation = Location;

	URelatedWorld* rWorld = UWorldDirector::GetWorldDirector()->GetRelatedWorldFromActor(Actor);

	if (rWorld != nullptr)
	{
		FIntVector rTranslation = rWorld->GetWorldTranslation();
		GlobalLocation = FRepMovement::RebaseOntoZeroOrigin(Location, Actor);
		GlobalLocation.X -= rTranslation.X;
		GlobalLocation.Y -= rTranslation.Y;
		GlobalLocation.Z -= rTranslation.Z;
		GlobalLocation = FRepMovement::RebaseOntoLocalOrigin(Location, rWorld->GetWorld()->OriginLocation);
	}
	ActorRepInfo.WorldLocation = GlobalLocation;
	ActorList.Add(Actor);
}

bool UReplicationGraphNode_RwDynamicNode::NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound)
{
	ActorList.Remove(ActorInfo.Actor);
	return true;
}

void UReplicationGraphNode_RwDynamicNode::PrepareForReplication()
{
	for (AActor* Actor : ActorList)
	{
		FGlobalActorReplicationInfo& ActorRepInfo = GraphGlobals->GlobalActorReplicationInfoMap->Get(Actor);

		FVector Location = Actor->GetActorLocation();
		FVector GlobalLocation = Location;
		
		URelatedWorld* rWorld = UWorldDirector::GetWorldDirector()->GetRelatedWorldFromActor(Actor);

		if (rWorld != nullptr)
		{
			const FIntVector rTranslation = rWorld->GetWorldTranslation();
			GlobalLocation = FRepMovement::RebaseOntoZeroOrigin(Location, Actor);
			GlobalLocation.X += rTranslation.X;
			GlobalLocation.Y += rTranslation.Y;
			GlobalLocation.Z += rTranslation.Z;
			GlobalLocation = FRepMovement::RebaseOntoLocalOrigin(GlobalLocation, rWorld->GetWorld()->OriginLocation);
		}
		ActorRepInfo.WorldLocation = GlobalLocation;

	}
}

void UReplicationGraphNode_RwDynamicNode::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	Params.OutGatheredReplicationLists.AddReplicationActorList(ActorList);

	for (UReplicationGraphNode* ChildNode : AllChildNodes)
	{
		ChildNode->GatherActorListsForConnection(Params);
	}
}

URwReplcationGraphBase::URwReplcationGraphBase()
{

}

void URwReplcationGraphBase::InitGlobalActorClassSettings()
{
	Super::InitGlobalActorClassSettings();

	// ReplicationGraph stores internal associative data for actor classes. 
	// We build this data here based on actor CDO values.
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		AActor* ActorCDO = Cast<AActor>(Class->GetDefaultObject());
		if (!ActorCDO || !ActorCDO->GetIsReplicated())
		{
			continue;
		}

		// Skip SKEL and REINST classes.
		if (Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}

		FClassReplicationInfo ClassInfo;

		// Replication Graph is frame based. Convert NetUpdateFrequency to ReplicationPeriodFrame based on Server MaxTickRate.
		ClassInfo.ReplicationPeriodFrame = GetReplicationPeriodFrameForFrequency(ActorCDO->NetUpdateFrequency);

		if (ActorCDO->bAlwaysRelevant || ActorCDO->bOnlyRelevantToOwner)
		{
			ClassInfo.SetCullDistanceSquared(0.f);
		}
		else
		{
			ClassInfo.SetCullDistanceSquared(ActorCDO->NetCullDistanceSquared);
		}

		GlobalActorReplicationInfoMap.SetClassInfo(Class, ClassInfo);
	}
}

void URwReplcationGraphBase::InitGlobalGraphNodes()
{
	// Preallocate some replication lists.


	// -----------------------------------------------
	//	Spatial Actors
	// -----------------------------------------------

	DynamicNode = CreateNewNode<UReplicationGraphNode_RwDynamicNode>();
	AddGlobalGraphNode(DynamicNode);

	// -----------------------------------------------
	//	Always Relevant (to everyone) Actors
	// -----------------------------------------------
	AlwaysRelevantNode = CreateNewNode<UReplicationGraphNode_ActorList>();
	AddGlobalGraphNode(AlwaysRelevantNode);
}

void URwReplcationGraphBase::InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection)
{
	Super::InitConnectionGraphNodes(RepGraphConnection);

	UReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantNodeForConnection = CreateNewNode<UReplicationGraphNode_AlwaysRelevant_ForConnection>();
	AddConnectionGraphNode(AlwaysRelevantNodeForConnection, RepGraphConnection);

	AlwaysRelevantForConnectionList.Emplace(RepGraphConnection->NetConnection, AlwaysRelevantNodeForConnection);
}

void URwReplcationGraphBase::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo)
{
	if (ActorInfo.Actor->bAlwaysRelevant)
	{
		AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);
	}
	else if (ActorInfo.Actor->bOnlyRelevantToOwner)
	{
		ActorsWithoutNetConnection.Add(ActorInfo.Actor);
	}
	else
	{
		// Note that UReplicationGraphNode_GridSpatialization2D has 3 methods for adding actor based on the mobility of the actor. Since AActor lacks this information, we will
		// add all spatialized actors as dormant actors: meaning they will be treated as possibly dynamic (moving) when not dormant, and as static (not moving) when dormant.
		DynamicNode->NotifyAddNetworkActor(ActorInfo);
	}
}

void URwReplcationGraphBase::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
	if (ActorInfo.Actor->bAlwaysRelevant)
	{
		AlwaysRelevantNode->NotifyRemoveNetworkActor(ActorInfo);
		SetActorDestructionInfoToIgnoreDistanceCulling(ActorInfo.GetActor());
	}
	else if (ActorInfo.Actor->bOnlyRelevantToOwner)
	{
		if (UReplicationGraphNode* Node = ActorInfo.Actor->GetNetConnection() ? GetAlwaysRelevantNodeForConnection(ActorInfo.Actor->GetNetConnection()) : nullptr)
		{
			Node->NotifyRemoveNetworkActor(ActorInfo);
		}
	}
	else
	{
		DynamicNode->NotifyRemoveNetworkActor(ActorInfo);
	}
}

UReplicationGraphNode_AlwaysRelevant_ForConnection* URwReplcationGraphBase::GetAlwaysRelevantNodeForConnection(UNetConnection* Connection)
{
	UReplicationGraphNode_AlwaysRelevant_ForConnection* Node = nullptr;
	if (Connection)
	{
		if (FConnectionAlwaysRelevantNodePair* Pair = AlwaysRelevantForConnectionList.FindByKey(Connection))
		{
			if (Pair->Node)
			{
				Node = Pair->Node;
			}
			else
			{
				UE_LOG(LogNet, Warning, TEXT("AlwaysRelevantNode for connection %s is null."), *GetNameSafe(Connection));
			}
		}
		else
		{
			UE_LOG(LogNet, Warning, TEXT("Could not find AlwaysRelevantNode for connection %s. This should have been created in UBasicReplicationGraph::InitConnectionGraphNodes."), *GetNameSafe(Connection));
		}
	}
	else
	{
		// Basic implementation requires owner is set on spawn that never changes. A more robust graph would have methods or ways of listening for owner to change
		UE_LOG(LogNet, Warning, TEXT("Actor: %s is bOnlyRelevantToOwner but does not have an owning Netconnection. It will not be replicated"));
	}

	return Node;
}

int32 URwReplcationGraphBase::ServerReplicateActors(float DeltaSeconds)
{
	// Route Actors needing owning net connections to appropriate nodes
	for (int32 idx = ActorsWithoutNetConnection.Num() - 1; idx >= 0; --idx)
	{
		bool bRemove = false;
		if (AActor* Actor = ActorsWithoutNetConnection[idx])
		{
			if (UNetConnection* Connection = Actor->GetNetConnection())
			{
				bRemove = true;
				if (UReplicationGraphNode_AlwaysRelevant_ForConnection* Node = GetAlwaysRelevantNodeForConnection(Actor->GetNetConnection()))
				{
					Node->NotifyAddNetworkActor(FNewReplicatedActorInfo(Actor));
				}
			}
		}
		else
		{
			bRemove = true;
		}

		if (bRemove)
		{
			ActorsWithoutNetConnection.RemoveAtSwap(idx, 1, false);
		}
	}


	return Super::ServerReplicateActors(DeltaSeconds);
}

bool FConnectionAlwaysRelevantNodePair::operator==(UNetConnection* InConnection) const
{
	// Any children should be looking at their parent connections instead.
	if (InConnection->GetUChildConnection() != nullptr)
	{
		InConnection = ((UChildConnection*)InConnection)->Parent;
	}

	return InConnection == NetConnection;
}
