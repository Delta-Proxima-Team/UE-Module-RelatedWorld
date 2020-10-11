// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "Net/RwReplicationGraphBase.h"
#include "WorldDirector.h"
#include "RelatedWorld.h"

void UReplicationGraphNode_Domain::NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo)
{
	for (UReplicationGraphNode* ChildNode : AllChildNodes)
	{
		if (UReplicationGraphNode_GlobalGridSpatialization2D* ggs2DNode = Cast<UReplicationGraphNode_GlobalGridSpatialization2D>(ChildNode))
		{
			ggs2DNode->NotifyAddNetworkActor(ActorInfo);
		}
		else if (UReplicationGraphNode_GridSpatialization2D* gs2DNode = Cast<UReplicationGraphNode_GridSpatialization2D>(ChildNode))
		{
			gs2DNode->AddActor_Dormancy(ActorInfo, GraphGlobals->GlobalActorReplicationInfoMap->Get(ActorInfo.Actor));
		}
		else
		{
			ChildNode->NotifyAddNetworkActor(ActorInfo);
		}
	}
}

bool UReplicationGraphNode_Domain::NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& Actor, bool bWarnIfNotFound)
{
	return true;
}

void UReplicationGraphNode_Domain::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	for (UReplicationGraphNode* ChildNode : AllChildNodes)
	{
		ChildNode->GatherActorListsForConnection(Params);
	}
}

void UReplicationGraphNode_Domain::PrepareForReplication()
{
	for (UReplicationGraphNode* ChildNode : AllChildNodes)
	{
		if (ChildNode->GetRequiresPrepareForReplication())
		{
			ChildNode->PrepareForReplication();
		}
	}
}

void UReplicationGraphNode_GlobalGridSpatialization2D::PrepareForReplication()
{
	Super::PrepareForReplication();

	for (AActor* Actor : DynamicSpatializedActors)
	{
		FGlobalActorReplicationInfo& ActorRepInfo = GraphGlobals->GlobalActorReplicationInfoMap->Get(Actor);
		FVector Location = Actor->GetActorLocation();

		URelatedWorld* rWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(Actor);

		if (rWorld != nullptr)
		{
			FIntVector Translation = rWorld->GetWorldTranslation();
			ActorRepInfo.WorldLocation = URelatedWorldUtils::CONVERT_RelToWorld(Translation, Location);
		}
	}
}

void UReplicationGraphNode_GlobalGridSpatialization2D::NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo)
{
	AddActor_Dormancy(ActorInfo, GraphGlobals->GlobalActorReplicationInfoMap->Get(ActorInfo.Actor));
	DynamicSpatializedActors.Emplace(ActorInfo.Actor);
}

void UReplicationGraphNode_GlobalGridSpatialization2D::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	AActor* Viewer = Params.Viewer.ViewTarget;

	URelatedWorld* rWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(Viewer);

	if (rWorld != nullptr)
	{
		FIntVector Translation = rWorld->GetWorldTranslation();
		Params.Viewer.ViewLocation = URelatedWorldUtils::CONVERT_RelToWorld(Translation, Params.Viewer.ViewLocation);
	}

	Super::GatherActorListsForConnection(Params);
}

void URwReplicationGraphBase::InitGlobalActorClassSettings()
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

void URwReplicationGraphBase::InitGlobalGraphNodes()
{
	PreAllocateRepList(12, 3);

	AlwaysRelevantNode = CreateNewNode<UReplicationGraphNode_ActorList>();
	AddGlobalGraphNode(AlwaysRelevantNode);

	UReplicationGraphNode_GridSpatialization2D* GridNode = nullptr;

	DomainNode[(uint8)EWorldDomain::WD_PUBLIC] = CreateNewNode<UReplicationGraphNode_Domain>();
	GridNode = CreateNewDomainNode<UReplicationGraphNode_GlobalGridSpatialization2D>((uint8)EWorldDomain::WD_PUBLIC);
	GridNode->CellSize = 10000.f;
	GridNode->SpatialBias = FVector2D(-WORLD_MAX, -WORLD_MAX);
	AddGlobalGraphNode(DomainNode[(uint8)EWorldDomain::WD_PUBLIC]);

	DomainNode[(uint8)EWorldDomain::WD_PRIVATE] = CreateNewNode<UReplicationGraphNode_Domain>();
	GridNode = CreateNewDomainNode<UReplicationGraphNode_GridSpatialization2D>((uint8)EWorldDomain::WD_PRIVATE);
	GridNode->CellSize = 10000.f;
	GridNode->SpatialBias = FVector2D(-WORLD_MAX, -WORLD_MAX);
	AddGlobalGraphNode(DomainNode[(uint8)EWorldDomain::WD_PRIVATE]);

	DomainNode[(uint8)EWorldDomain::WD_ISOLATED] = CreateNewNode<UReplicationGraphNode_Domain>();
	GridNode = CreateNewDomainNode<UReplicationGraphNode_GridSpatialization2D>((uint8)EWorldDomain::WD_ISOLATED);
	GridNode->CellSize = 10000.f;
	GridNode->SpatialBias = FVector2D(-WORLD_MAX, -WORLD_MAX);
	AddGlobalGraphNode(DomainNode[(uint8)EWorldDomain::WD_ISOLATED]);
}

template<class T>
T* URwReplicationGraphBase::CreateNewDomainNode(uint8 Domain)
{
	T* Node = nullptr;

	if (UReplicationGraphNode_Domain* Dom = DomainNode[Domain])
	{
		Node = Dom->CreateChildNode<T>();
	}

	return Node;
}

void URwReplicationGraphBase::InitConnectionGraphNodes(UNetReplicationGraphConnection* ConnectionManager)
{
	Super::InitConnectionGraphNodes(ConnectionManager);

	UReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantNodeForConnection = CreateNewNode<UReplicationGraphNode_AlwaysRelevant_ForConnection>();
	AddConnectionGraphNode(AlwaysRelevantNodeForConnection, ConnectionManager);
	ConnectionRelevantNode.Emplace(ConnectionManager->NetConnection, AlwaysRelevantNodeForConnection);
}

void URwReplicationGraphBase::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo)
{
	if (ActorInfo.Actor->bAlwaysRelevant)
	{
		AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);
	}
	else if (ActorInfo.Actor->bOnlyRelevantToOwner)
	{
		ActorsWithoutConnection.Add(ActorInfo.Actor);
	}
	else
	{
		URelatedWorld* rWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(ActorInfo.Actor);

		if (rWorld != nullptr)
		{
			EWorldDomain Domain = rWorld->GetWorldDomain();
			DomainNode[(uint8)Domain]->NotifyAddNetworkActor(ActorInfo);
		}
		else
		{
			DomainNode[(uint8)EWorldDomain::WD_PUBLIC]->NotifyAddNetworkActor(ActorInfo);
		}
	}
}

int32 URwReplicationGraphBase::ServerReplicateActors(float DeltaSeconds)
{
	for (int32 i = ActorsWithoutConnection.Num() - 1; i >= 0; --i)
	{
		bool bRemove = false;

		if (AActor* Actor = ActorsWithoutConnection[i])
		{
			if (UNetConnection* Connection = Actor->GetNetConnection())
			{
				bRemove = true;
				if (UReplicationGraphNode_AlwaysRelevant_ForConnection* Node = ConnectionRelevantNode.FindRef(Connection))
				{
					Node->NotifyAddNetworkActor(FNewReplicatedActorInfo(Actor));
				}
			}
		}
		else
		{
			bRemove = true;
		}

		if (bRemove == true)
		{
			ActorsWithoutConnection.RemoveAt(i, 1, false);
		}
	}

	return Super::ServerReplicateActors(DeltaSeconds);
}
