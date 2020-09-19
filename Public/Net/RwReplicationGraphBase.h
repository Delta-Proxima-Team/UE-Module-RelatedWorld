// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "BasicReplicationGraph.h"
#include "RwReplicationGraphBase.generated.h"

UCLASS()
class RELATEDWORLD_API UReplicationGraphNode_RwDynamicNode : public UReplicationGraphNode
{
	GENERATED_BODY()

public:
	UReplicationGraphNode_RwDynamicNode() { bRequiresPrepareForReplicationCall = true; ActorList.Reset(4); }

	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override;
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& Actor, bool bWarnIfNotFound = true) override;
	virtual void PrepareForReplication() override;
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

protected:
	FActorRepListRefView ActorList;

};

UCLASS(transient, config = Engine)
class RELATEDWORLD_API URwReplcationGraphBase : public UReplicationGraph
{
	GENERATED_BODY()

public:
	URwReplcationGraphBase();

protected:
	virtual void InitGlobalActorClassSettings() override;
	virtual void InitGlobalGraphNodes() override;
	virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection) override;
	virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;
	virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo) override;

	virtual int32 ServerReplicateActors(float DeltaSeconds) override;

	UPROPERTY()
		UReplicationGraphNode_RwDynamicNode* DynamicNode;

	UPROPERTY()
		UReplicationGraphNode_ActorList* AlwaysRelevantNode;

	UPROPERTY()
		TArray<FConnectionAlwaysRelevantNodePair> AlwaysRelevantForConnectionList;

	/** Actors that are only supposed to replicate to their owning connection, but that did not have a connection on spawn */
	UPROPERTY()
		TArray<AActor*> ActorsWithoutNetConnection;


	UReplicationGraphNode_AlwaysRelevant_ForConnection* GetAlwaysRelevantNodeForConnection(UNetConnection* Connection);
};
