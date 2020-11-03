// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "RwReplicationGraphBase.generated.h"

class URelatedWorld;

UCLASS()
class RELATEDWORLD_API UReplicationGraphNode_Proxy : public UReplicationGraphNode
{
	GENERATED_BODY()
public:
	UReplicationGraphNode_Proxy() { bRequiresPrepareForReplicationCall = true; };
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override;
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& Actor, bool bWarnIfNotFound = true) override;
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;
	virtual void PrepareForReplication() override;
};

UCLASS()
class RELATEDWORLD_API UReplicationGraphNode_Domain : public UReplicationGraphNode_Proxy
{
	GENERATED_BODY()

public:
	UReplicationGraphNode_Domain() { bRequiresPrepareForReplicationCall = true; };
	virtual void SetDomain(uint8 Domain) { NodeDomain = Domain; };
	template<class T>
	T* CreateRouterNode();
	FORCEINLINE UReplicationGraphNode* GetRouterNode() const { return RouterNode; };
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override;
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& Actor, bool bWarnIfNotFound = true) override;
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

private:
	uint8 NodeDomain;
	UPROPERTY()
		UReplicationGraphNode* RouterNode;
};

struct FRouterRule
{
	URelatedWorld* RelatedWorld;
	TArray<UReplicationGraphNode*> Node;
};

UCLASS()
class RELATEDWORLD_API UReplicationGraphNode_WorldRouter : public UReplicationGraphNode_Proxy
{
	GENERATED_BODY()

public:
	template<class T>
	T* CreateRoutedNodeTemplate();
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override;
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& Actor, bool bWarnIfNotFound = true) override;
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

private:
	UPROPERTY()
		TArray<UReplicationGraphNode*> RoutedNodeTemplates;

	TArray<FRouterRule> RouterRule;
};

UCLASS()
class RELATEDWORLD_API UReplicationGraphNode_GlobalGridSpatialization2D : public UReplicationGraphNode_GridSpatialization2D
{
	GENERATED_BODY()

public:
	UReplicationGraphNode_GlobalGridSpatialization2D() { bRequiresPrepareForReplicationCall = true; };
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& Actor) override;
	virtual void PrepareForReplication() override;
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

protected:
	TArray<FActorRepListType> DynamicSpatializedActors;
};

UCLASS(Transient, Config = Engine)
class RELATEDWORLD_API URwReplicationGraphBase : public UReplicationGraph
{
	GENERATED_BODY()

public:
	template<class T>
	T* CreateNewDomainNode(uint8 Domain);
	virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;

	virtual int32 ServerReplicateActors(float DeltaSeconds) override;

	UFUNCTION()
		virtual void OnMoveActorToWorld(AActor* InActor, URelatedWorld* OldWorld, URelatedWorld* NewWorld);

protected:
	virtual void InitGlobalActorClassSettings() override;
	virtual void InitGlobalGraphNodes() override;
	virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* ConnectionManager) override;

private:
	UPROPERTY()
		UReplicationGraphNode_ActorList* AlwaysRelevantNode;
	UPROPERTY()
		TArray<AActor*> ActorsWithoutConnection;
	UPROPERTY()
		TArray<AActor*> WorldChangePendingActors;

	UPROPERTY()
		TMap<UNetConnection*, UReplicationGraphNode_AlwaysRelevant_ForConnection*> ConnectionRelevantNode;
	UPROPERTY()
		UReplicationGraphNode_Domain* DomainNode[3];
};
