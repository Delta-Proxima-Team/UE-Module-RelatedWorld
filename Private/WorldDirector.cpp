// Copyright Delta-Proxima Team (c) 2007-2020

#include "WorldDirector.h"
#include "RelatedWorld.h"
#include "Net/RelatedWorldNetLocCorrectionComponent.h"

#include "EngineUtils.h"
#include "ShaderCompiler.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LevelStreaming.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY(LogWorldDirector);

URelatedWorld* UWorldDirector::CreateEmptyWorld(UObject* WorldContextObject, FName WorldName, FIntVector WorldTranslation, bool IsNetWorld)
{
	URelatedWorld* rWorld = nullptr;

	if (Worlds.Num())
	{
		rWorld = Worlds.FindRef(WorldName);

		if (rWorld != nullptr)
		{
			UE_LOG(LogWorldDirector, Warning, TEXT("World %s already loaded as %s"), *WorldName.ToString(), *rWorld->GetName());
			return nullptr;
		}
	}

	UWorld* World = nullptr;
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.PIEInstance = GPlayInEditorID;
	Context.OwningGameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);

	World = UWorld::CreateWorld(EWorldType::Game, true, WorldName);
	World->SetGameInstance(Context.OwningGameInstance);
	Context.SetCurrentWorld(World);
	Context.World()->URL.Map = WorldName.ToString();

	if (!Context.World()->bIsWorldInitialized)
	{
		Context.World()->InitWorld();
	}

	if (IsNetWorld)
	{
		Context.ActiveNetDrivers = GEngine->GetWorldContextFromWorld(WorldContextObject->GetWorld())->ActiveNetDrivers;
		Context.World()->NetDriver = WorldContextObject->GetWorld()->NetDriver;
	}
	else
	{
		Context.ActiveNetDrivers.Empty();
		Context.World()->NetDriver = nullptr;
	}

	Context.World()->CreateAISystem();
	Context.World()->InitializeActorsForPlay(Context.World()->URL, true);
	FNavigationSystem::AddNavigationSystemToWorld(*Context.World(), FNavigationSystemRunMode::GameMode);

	Context.World()->bWorldWasLoadedThisTick = true;
	Context.World()->SetShouldTick(false);

	rWorld = NewObject<URelatedWorld>(this);
	rWorld->AddToRoot();
	rWorld->SetContext(&Context);
	rWorld->SetNetworked(IsNetWorld);
	rWorld->TranslateWorld(WorldTranslation);
	rWorld->SetPersistentWorld(Context.OwningGameInstance->GetWorld());
	rWorld->HandleBeginPlay();

	Worlds.Add(WorldName, rWorld);

	return rWorld;
}

URelatedWorld* UWorldDirector::LoadRelatedWorld(UObject* WorldContextObject, FName WorldName, FIntVector WorldTranslation, bool IsNetWorld)
{
	URelatedWorld* rWorld = nullptr;
	
	if (Worlds.Num())
	{
		rWorld = Worlds.FindRef(WorldName);

		if (rWorld != nullptr)
		{
			UE_LOG(LogWorldDirector, Warning, TEXT("World %s already loaded as %s"), *WorldName.ToString(), *rWorld->GetName());
			return nullptr;
		}
	}

	UPackage* WorldPackage = nullptr;
	UWorld* World = nullptr;
	FString MapName = WorldName.ToString();
	FURL URL(*MapName);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.PIEInstance = GPlayInEditorID;
	Context.OwningGameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	Context.OwningGameInstance->PreloadContentForURL(URL);

	if (GPlayInEditorID > -1)
	{
		const FString PIEPackageName = UWorld::ConvertToPIEPackageName(MapName, GPlayInEditorID);
		const FName PIEPackageFName = FName(*PIEPackageName);
		UWorld::WorldTypePreLoadMap.FindOrAdd(PIEPackageFName) = Context.WorldType;
		FSoftObjectPath::AddPIEPackageName(PIEPackageFName);
		UPackage* NewPackage = CreatePackage(nullptr, *PIEPackageName);
		NewPackage->SetPackageFlags(PKG_PlayInEditor);
		WorldPackage = LoadPackage(NewPackage, *MapName, LOAD_PackageForPIE);
		UWorld::WorldTypePreLoadMap.Remove(PIEPackageFName);

		if (WorldPackage == nullptr)
		{
			return nullptr;
		}

		World = UWorld::FindWorldInPackage(WorldPackage);
		WorldPackage->PIEInstanceID = GPlayInEditorID;
		WorldPackage->SetPackageFlags(PKG_PlayInEditor);

		for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
		{
			StreamingLevel->RenameForPIE(GPlayInEditorID);
		}
	}

	if (World == nullptr)
	{
		UWorld::WorldTypePreLoadMap.FindOrAdd(WorldName) = Context.WorldType;

		WorldPackage = FindPackage(nullptr, *MapName);

		if (WorldPackage == nullptr)
		{
			WorldPackage = LoadPackage(nullptr, *MapName, LOAD_None);
		}

		UWorld::WorldTypePreLoadMap.Remove(WorldName);

		if (WorldPackage == nullptr)
		{
			return nullptr;
		}

		World = UWorld::FindWorldInPackage(WorldPackage);

		if (World == nullptr)
		{
			return nullptr;
		}

		World->PersistentLevel->HandleLegacyMapBuildData();
	}

	World->SetGameInstance(Context.OwningGameInstance);
	Context.SetCurrentWorld(World);
	Context.World()->WorldType = Context.WorldType;

	if (GPlayInEditorID > -1)
	{
		check(Context.World()->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor));
		Context.World()->ClearFlags(RF_Standalone);
	}
	else
	{
		Context.World()->AddToRoot();
	}

	if (!Context.World()->bIsWorldInitialized)
	{
		Context.World()->InitWorld();
	}

	if (IsNetWorld)
	{
		UNetDriver* SharedNetDriver = WorldContextObject->GetWorld()->GetNetDriver();
		Context.ActiveNetDrivers = GEngine->GetWorldContextFromWorld(WorldContextObject->GetWorld())->ActiveNetDrivers;
		Context.World()->SetNetDriver(SharedNetDriver);
		
		FLevelCollection* LevelCollection = (FLevelCollection*)Context.World()->GetActiveLevelCollection();
		LevelCollection->SetNetDriver(SharedNetDriver);
	}
	else
	{
		Context.ActiveNetDrivers.Empty();
		Context.World()->NetDriver = nullptr;
	}

	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->ProcessAsyncResults(false, true);
	}

	GEngine->LoadPackagesFully(Context.World(), FULLYLOAD_Map, Context.World()->PersistentLevel->GetOutermost()->GetName());
	Context.World()->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);

	if (!GIsEditor && !IsRunningDedicatedServer())
	{
		// If requested, duplicate dynamic levels here after the source levels are created.
		Context.World()->DuplicateRequestedLevels(FName(*URL.Map));
	}

	Context.World()->CreateAISystem();
	Context.World()->InitializeActorsForPlay(URL, true);
	FNavigationSystem::AddNavigationSystemToWorld(*Context.World(), FNavigationSystemRunMode::GameMode);

	Context.LastURL = URL;
	Context.LastURL.Map = MapName;

	Context.World()->bWorldWasLoadedThisTick = true;
	Context.World()->SetShouldTick(false);

	rWorld = NewObject<URelatedWorld>(this);
	rWorld->AddToRoot();
	rWorld->SetContext(&Context);
	rWorld->SetNetworked(IsNetWorld);
	rWorld->TranslateWorld(WorldTranslation);
	rWorld->SetPersistentWorld(Context.OwningGameInstance->GetWorld());
	rWorld->HandleBeginPlay();

	Worlds.Add(WorldName, rWorld);

	return rWorld;
}

void UWorldDirector::UnloadAllRelatedWorlds()
{
	TArray<FName> LevelNames;
	Worlds.GetKeys(LevelNames);
	
	for (FName LevelName : LevelNames)
	{
		UnloadRelatedWorldByName(LevelName);
	}
}

void UWorldDirector::UnloadRelatedWorldByName(FName WorldName)
{
	if (!Worlds.Num())
	{
		return;
	}

	URelatedWorld* rWorld = Worlds.FindRef(WorldName);
	
	if (rWorld != nullptr)
	{
		UnloadRelatedWorld(rWorld);
	}
}

void UWorldDirector::UnloadRelatedWorld(URelatedWorld* RelatedWorld)
{
	check(RelatedWorld);

	FWorldContext* Context = RelatedWorld->Context();

	RelatedWorld->SetContext(nullptr);
	RelatedWorld->RemoveFromRoot();
	Worlds.Remove(FName(Context->World()->URL.Map));

	for (FActorIterator ActorIt(Context->World()); ActorIt; ++ActorIt)
	{
		ActorIt->RouteEndPlay(EEndPlayReason::LevelTransition);
	}

	Context->World()->CleanupWorld();
	GEngine->WorldDestroyed(Context->World());

	// mark everything else contained in the world to be deleted
	for (auto LevelIt(Context->World()->GetLevelIterator()); LevelIt; ++LevelIt)
	{
		const ULevel* Level = *LevelIt;
		if (Level)
		{
			CastChecked<UWorld>(Level->GetOuter())->MarkObjectsPendingKill();
		}
	}

	for (ULevelStreaming* LevelStreaming : Context->World()->GetStreamingLevels())
	{
		// If an unloaded levelstreaming still has a loaded level we need to mark its objects to be deleted as well
		if (LevelStreaming->GetLoadedLevel() && (!LevelStreaming->ShouldBeLoaded() || !LevelStreaming->ShouldBeVisible()))
		{
			CastChecked<UWorld>(LevelStreaming->GetLoadedLevel()->GetOuter())->MarkObjectsPendingKill();
		}
	}

	Context->World()->RemoveFromRoot();
	Context->SetCurrentWorld(nullptr);
	GEngine->DestroyWorldContext(Context->World());
}

AActor* UWorldDirector::SpawnActor(URelatedWorld* TargetWorld, UClass* Class, const FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, AActor* Owner)
{
	check(TargetWorld);

	return TargetWorld->SpawnActor(Class, SpawnTransform, CollisionHandlingOverride, Owner);
}

URelatedWorld* UWorldDirector::GetRelatedWorldFromActor(AActor* InActor) const
{

	if (!Worlds.Num() || InActor == nullptr || !IsValid(InActor))
	{
		return nullptr;
	}

	UWorld* ActorWorld = InActor->GetWorld();

	return Worlds.FindRef(FName(ActorWorld->URL.Map));
}

URelatedWorld* UWorldDirector::GetRelatedWorldByName(FName WorldName) const
{
	return Worlds.FindRef(WorldName);
}

bool UWorldDirector::MoveActorToWorld(URelatedWorld* World, AActor* InActor, bool bTranslateLocation)
{
	if (!IsValid(InActor) || InActor->IsPendingKill())
	{
		return false;
	}

	URelatedWorld* OldRWorld = GetRelatedWorldFromActor(InActor);
	UWorld* MainWorld = OldRWorld ? OldRWorld->GetWorld() : InActor->GetWorld();
	
	bool bNet = ((World != nullptr && World->IsNetworkedWorld()) || MainWorld->NetDriver != nullptr);
	bool bOldNet = ((OldRWorld != nullptr && OldRWorld->IsNetworkedWorld()) || MainWorld->NetDriver != nullptr);

	if (!(bNet & bOldNet))
	{
		if (bOldNet)
		{
			if (MainWorld->NetDriver->ShouldClientDestroyActor(InActor))
			{
				MainWorld->NetDriver->NotifyActorDestroyed(InActor);
			}
			
			MainWorld->NetDriver->RemoveNetworkActor(InActor);
		}

		if (bNet)
		{
			MainWorld->NetDriver->AddNetworkActor(InActor);
		}
	}

	//Translate coordinates into new one
	USceneComponent* RootComponent = InActor->GetRootComponent();

	if (RootComponent)
	{
		FIntVector OldTranslation = OldRWorld != nullptr ? OldRWorld->GetWorldTranslation() : FIntVector::ZeroValue;
		FIntVector NewTranslation = World != nullptr ? World->GetWorldTranslation() : FIntVector::ZeroValue;

		FVector Location = FRepMovement::RebaseOntoZeroOrigin(RootComponent->GetComponentLocation(), RootComponent);

		if (bTranslateLocation)
		{
			Location = URelatedWorldUtils::CONVERT_RelToRel(OldTranslation, NewTranslation, Location);
		}

		FIntVector Origin = World != nullptr ? World->Context()->World()->OriginLocation : MainWorld->OriginLocation;
		FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(Location, Origin);

		RootComponent->SetWorldLocation(NewLocation);
	}

	URelatedWorldNetLocCorrectionComponent* CorrectionComp = Cast<URelatedWorldNetLocCorrectionComponent>(InActor->GetComponentByClass(URelatedWorldNetLocCorrectionComponent::StaticClass()));

	if (CorrectionComp != nullptr)
	{
		CorrectionComp->NotifyWorldChanged(World);
	}

	ULevel* NewOuter = World != nullptr ? World->Context()->World()->PersistentLevel : MainWorld->PersistentLevel;
	return InActor->Rename(nullptr, NewOuter);
}
