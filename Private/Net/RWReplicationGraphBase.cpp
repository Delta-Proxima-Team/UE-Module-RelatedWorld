// Copyright Delta-Proxima Team (c) 2007-2020

#include "Net/RwReplicationGraphBase.h"
#include "WorldDirector.h"
#include "RelatedWorld.h"

void UReplicationGraphNode_Proxy::NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo)
{

}

bool UReplicationGraphNode_Proxy::NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& Actor, bool bWarnIfNotFound)
{
	return false;
}

void UReplicationGraphNode_Proxy::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	for (UReplicationGraphNode* ChildNode : AllChildNodes)
	{
		ChildNode->GatherActorListsForConnection(Params);
	}
}

void UReplicationGraphNode_Proxy::PrepareForReplication()
{
	for (UReplicationGraphNode* ChildNode : AllChildNodes)
	{
		if (ChildNode->GetRequiresPrepareForReplication())
		{
			ChildNode->PrepareForReplication();
		}
	}
}

template<class T>
T* UReplicationGraphNode_Domain::CreateRouterNode()
{
	return (T*)(RouterNode = CreateChildNode<T>());
}

template<class T>
T* UReplicationGraphNode_WorldRouter::CreateRoutedNodeTemplate()
{
	T* Template = NewObject<T>(this);
	RoutedNodeTemplates.Add(Template);
	return Template;
}

void UReplicationGraphNode_Domain::NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo)
{
	URelatedWorld* rWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(ActorInfo.Actor);

	if ((rWorld != nullptr && NodeDomain == (uint8)rWorld->GetWorldDomain())
		|| (rWorld == nullptr && NodeDomain == 0))
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
}

bool UReplicationGraphNode_Domain::NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& Actor, bool bWarnIfNotFound)
{
	bool bResult = false;

	for (UReplicationGraphNode* ChildNode : AllChildNodes)
	{
		if (UReplicationGraphNode_GlobalGridSpatialization2D* ggs2DNode = Cast<UReplicationGraphNode_GlobalGridSpatialization2D>(ChildNode))
		{
			ggs2DNode->RemoveActor_Dormancy(Actor);
		}
		else if (UReplicationGraphNode_GridSpatialization2D* gs2DNode = Cast<UReplicationGraphNode_GridSpatialization2D>(ChildNode))
		{
			gs2DNode->RemoveActor_Dormancy(Actor);
		}
		else
		{
			ChildNode->NotifyRemoveNetworkActor(Actor, bWarnIfNotFound);
		}
	}

	return true;
}

void UReplicationGraphNode_Domain::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	for (int32 i = 0; i < Params.Viewers.Num(); ++i)
	{
		FNetViewer* Viewer = (FNetViewer*)&Params.Viewers[i];
		URelatedWorld* rWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(Viewer->ViewTarget);

		if (rWorld != nullptr)
		{
			if (rWorld->GetWorldDomain() == EWorldDomain::WD_ISOLATED && NodeDomain != (uint8)EWorldDomain::WD_ISOLATED)
			{
				return;
			}
		}
		else
		{
			if (NodeDomain != (uint8)EWorldDomain::WD_PUBLIC)
			{
				return;
			}
		}
	}

	Super::GatherActorListsForConnection(Params);
}

void UReplicationGraphNode_WorldRouter::NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo)
{
	URelatedWorld* rWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(ActorInfo.Actor);

	if (rWorld != nullptr)
	{
		bool bAdded = false;
		for (const FRouterRule& Rule : RouterRule)
		{
			if (Rule.RelatedWorld == rWorld)
			{
				for (UReplicationGraphNode* RoutedNode : Rule.Node)
				{
					if (UReplicationGraphNode_GridSpatialization2D* gs2DNode = Cast<UReplicationGraphNode_GridSpatialization2D>(RoutedNode))
					{
						gs2DNode->AddActor_Dormancy(ActorInfo, GraphGlobals->GlobalActorReplicationInfoMap->Get(ActorInfo.Actor));
					}
					else
					{
						RoutedNode->NotifyAddNetworkActor(ActorInfo);
					}
				}

				bAdded = true;
				break;
			}
		}

		if (bAdded == false)
		{
			FRouterRule newRule;
			newRule.RelatedWorld = rWorld;

			for (UReplicationGraphNode* NodeTemplate : RoutedNodeTemplates)
			{
				UReplicationGraphNode* NewRoutedNode = DuplicateObject<UReplicationGraphNode>(NodeTemplate, this);
				NewRoutedNode->Initialize(GraphGlobals);
				AllChildNodes.Add(NewRoutedNode);
				newRule.Node.Add(NewRoutedNode);
			}

			RouterRule.Add(newRule);
			NotifyAddNetworkActor(ActorInfo);
		}
	}
}

bool UReplicationGraphNode_WorldRouter::NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& Actor, bool bWarnIfNotFound)
{
	bool bResult = false;

	for (UReplicationGraphNode* ChildNode : AllChildNodes)
	{
		if (UReplicationGraphNode_GlobalGridSpatialization2D* ggs2DNode = Cast<UReplicationGraphNode_GlobalGridSpatialization2D>(ChildNode))
		{
			ggs2DNode->RemoveActor_Dormancy(Actor);
		}
		else if (UReplicationGraphNode_GridSpatialization2D* gs2DNode = Cast<UReplicationGraphNode_GridSpatialization2D>(ChildNode))
		{
			gs2DNode->RemoveActor_Dormancy(Actor);
		}
		else
		{
			ChildNode->NotifyRemoveNetworkActor(Actor, bWarnIfNotFound);
		}
	}

	return true;
}

void UReplicationGraphNode_WorldRouter::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	for (int32 i = 0; i < Params.Viewers.Num(); ++i)
	{
		FNetViewer* Viewer = (FNetViewer*)&Params.Viewers[i];
		URelatedWorld* rWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(Viewer->ViewTarget);

		if (rWorld != nullptr)
		{
			for (const FRouterRule Rule : RouterRule)
			{
				if (Rule.RelatedWorld == rWorld)
				{
					for (UReplicationGraphNode* Node : Rule.Node)
					{
						Node->GatherActorListsForConnection(Params);
					}
				}
			}
		}
	}
}

void UReplicationGraphNode_GlobalGridSpatialization2D::PrepareForReplication()
{
	Super::PrepareForReplication();
	//ToDo: Better use relative coordinate system with origin in player position
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
	for (int32 i = 0; i < Params.Viewers.Num(); ++i)
	{
		FNetViewer* Viewer = (FNetViewer*)&Params.Viewers[i];
		AActor* aViewer = Viewer->ViewTarget;

		URelatedWorld* rWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(aViewer);

		if (rWorld != nullptr)
		{
			FIntVector Translation = rWorld->GetWorldTranslation();
			Viewer->ViewLocation = URelatedWorldUtils::CONVERT_RelToWorld(Translation, Viewer->ViewLocation);
		}
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

	UWorldDirector::Get()->OnMoveActorToWorld.AddDynamic(this, &URwReplicationGraphBase::OnMoveActorToWorld);
}

void URwReplicationGraphBase::InitGlobalGraphNodes()
{
	PreAllocateRepList(12, 3);

	AlwaysRelevantNode = CreateNewNode<UReplicationGraphNode_ActorList>();
	AddGlobalGraphNode(AlwaysRelevantNode);

	UReplicationGraphNode_GridSpatialization2D* GridNode = nullptr;
	UReplicationGraphNode_WorldRouter* RouterNode = nullptr;

	DomainNode[(uint8)EWorldDomain::WD_PUBLIC] = CreateNewNode<UReplicationGraphNode_Domain>();
	DomainNode[(uint8)EWorldDomain::WD_PUBLIC]->SetDomain((uint8)EWorldDomain::WD_PUBLIC);
	GridNode = CreateNewDomainNode<UReplicationGraphNode_GlobalGridSpatialization2D>((uint8)EWorldDomain::WD_PUBLIC);
	GridNode->CellSize = 10000.f;
	GridNode->SpatialBias = FVector2D(-WORLD_MAX, -WORLD_MAX);
	AddGlobalGraphNode(DomainNode[(uint8)EWorldDomain::WD_PUBLIC]);

	DomainNode[(uint8)EWorldDomain::WD_PRIVATE] = CreateNewNode<UReplicationGraphNode_Domain>();
	DomainNode[(uint8)EWorldDomain::WD_PRIVATE]->SetDomain((uint8)EWorldDomain::WD_PRIVATE);
	RouterNode = DomainNode[(uint8)EWorldDomain::WD_PRIVATE]->CreateRouterNode<UReplicationGraphNode_WorldRouter>();
	GridNode = RouterNode->CreateRoutedNodeTemplate<UReplicationGraphNode_GridSpatialization2D>();
	GridNode->CellSize = 10000.f;
	GridNode->SpatialBias = FVector2D(-WORLD_MAX, -WORLD_MAX);
	AddGlobalGraphNode(DomainNode[(uint8)EWorldDomain::WD_PRIVATE]);

	DomainNode[(uint8)EWorldDomain::WD_ISOLATED] = CreateNewNode<UReplicationGraphNode_Domain>();
	DomainNode[(uint8)EWorldDomain::WD_ISOLATED]->SetDomain((uint8)EWorldDomain::WD_ISOLATED);
	RouterNode = DomainNode[(uint8)EWorldDomain::WD_ISOLATED]->CreateRouterNode<UReplicationGraphNode_WorldRouter>();
	GridNode = RouterNode->CreateRoutedNodeTemplate<UReplicationGraphNode_GridSpatialization2D>();
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

void URwReplicationGraphBase::OnMoveActorToWorld(AActor* InActor, URelatedWorld* OldWorld, URelatedWorld* NewWorld)
{
	uint8 OldDomain = OldWorld ? (uint8)OldWorld->GetWorldDomain() : 0;
	uint8 NewDomain = NewWorld ? (uint8)NewWorld->GetWorldDomain() : 0;

	DomainNode[OldDomain]->NotifyRemoveNetworkActor(FNewReplicatedActorInfo(InActor), false);

	WorldChangePendingActors.Add(InActor);
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

	for (int32 i = WorldChangePendingActors.Num() - 1; i >= 0; --i)
	{
		if (AActor* Actor = WorldChangePendingActors[i])
		{
			URelatedWorld* rWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(Actor);
			uint8 Domain = rWorld ? (uint8)rWorld->GetWorldDomain() : 0;
			DomainNode[Domain]->NotifyAddNetworkActor(FNewReplicatedActorInfo(Actor));
			WorldChangePendingActors.RemoveAt(i, 1, false);
		}
	}

	return Super::ServerReplicateActors(DeltaSeconds);
}
