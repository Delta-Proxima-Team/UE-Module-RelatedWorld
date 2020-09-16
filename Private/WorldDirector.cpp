// Copyright Delta-Proxima Team (c) 2007-2020

#include "WorldDirector.h"
#include "Net/RelatedWorldNetLocCorrectionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ShaderCompiler.h"
#include "Engine/LevelStreaming.h"
#include "TickTaskManagerInterface.h"
#include "Engine/WorldComposition.h"
#include "FXSystem.h"
#include "Components/SceneCaptureComponent.h"
#include "InGamePerformanceTracker.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY(LogWorldDirector);

bool URelatedWorld::IsTickable() const
{
	return _Context ? true : false;
}

UWorld* URelatedWorld::GetWorld() const
{
	return _Context->OwningGameInstance->GetWorld();
}

void URelatedWorld::Tick(float DeltaSeconds)
{
	ELevelTick TickType = LEVELTICK_All;
	UWorld* World = _Context->World();
	AWorldSettings* Info = World->GetWorldSettings();
	UNavigationSystemBase* NavigationSystem = World->GetNavigationSystem();

	if (GIntraFrameDebuggingGameThread)
	{
		return;
	}

	FWorldDelegates::OnWorldTickStart.Broadcast(World, TickType, DeltaSeconds);

	//Tick game and other thread trackers.
	for (int32 Tracker = 0; Tracker < (int32)EInGamePerfTrackers::Num; ++Tracker)
	{
		World->PerfTrackers->GetInGamePerformanceTracker((EInGamePerfTrackers)Tracker, EInGamePerfTrackerThreads::GameThread).Tick();
		World->PerfTrackers->GetInGamePerformanceTracker((EInGamePerfTrackers)Tracker, EInGamePerfTrackerThreads::OtherThread).Tick();
	}

	FMemMark Mark(FMemStack::Get());
	World->bInTick = true;
	bool bIsPaused = GetWorld()->IsPaused() || World->IsPaused();

	// Update time.
	World->RealTimeSeconds += DeltaSeconds;

	// Audio always plays at real-time regardless of time dilation, but only when NOT paused
	if (!bIsPaused)
	{
		World->AudioTimeSeconds += DeltaSeconds;
	}

	// Save off actual delta
	float RealDeltaSeconds = DeltaSeconds;

	// apply time multipliers
	DeltaSeconds *= Info->GetEffectiveTimeDilation();

	// Handle clamping of time to an acceptable value
	const float GameDeltaSeconds = Info->FixupDeltaSeconds(DeltaSeconds, RealDeltaSeconds);
	check(GameDeltaSeconds >= 0.0f);

	DeltaSeconds = GameDeltaSeconds;
	World->DeltaTimeSeconds = DeltaSeconds;

	World->UnpausedTimeSeconds += DeltaSeconds;

	if (!bIsPaused)
	{
		World->TimeSeconds += DeltaSeconds;
	}

	if (World->bPlayersOnly)
	{
		TickType = LEVELTICK_ViewportsOnly;
	}

	// Translate world origin if requested
	if (World->OriginLocation != World->RequestedOriginLocation)
	{
		World->SetNewWorldOrigin(World->RequestedOriginLocation);
	}
	else
	{
		World->OriginOffsetThisFrame = FVector::ZeroVector;
	}

	// update world's subsystems (NavigationSystem for now)
	if (NavigationSystem != nullptr)
	{
		NavigationSystem->Tick(DeltaSeconds);
	}

	bool bDoingActorTicks =
		(TickType != LEVELTICK_TimeOnly)
		&& !bIsPaused
		&& (!GetWorld()->NetDriver 
			|| !GetWorld()->NetDriver->ServerConnection 
			|| GetWorld()->NetDriver->ServerConnection->State == USOCK_Open);

	if (bDoingActorTicks)
	{
		{
			// Run pre-actor tick delegates that want clamped/dilated time
			FWorldDelegates::OnWorldPreActorTick.Broadcast(World, TickType, DeltaSeconds);
		}

		// Tick level sequence actors first
		for (int32 i = World->LevelSequenceActors.Num() - 1; i >= 0; --i)
		{
			if (World->LevelSequenceActors[i] != nullptr)
			{
				World->LevelSequenceActors[i]->Tick(DeltaSeconds);
			}
		}
	}

	const TArray<FLevelCollection>& LevelCollections = World->GetLevelCollections();

	for (int32 i = 0; i < LevelCollections.Num(); ++i)
	{
		// Build a list of levels from the collection that are also in the world's Levels array.
		// Collections may contain levels that aren't loaded in the world at the moment.
		TArray<ULevel*> LevelsToTick;
		for (ULevel* CollectionLevel : LevelCollections[i].GetLevels())
		{
			if (World->GetLevels().Contains(CollectionLevel))
			{
				LevelsToTick.Add(CollectionLevel);
			}
		}

		// Set up context on the world for this level collection
		FScopedLevelCollectionContextSwitch LevelContext(i, World);

		// If caller wants time update only, or we are paused, skip the rest.
		if (bDoingActorTicks)
		{
			// Actually tick actors now that context is set up
			World->SetupPhysicsTickFunctions(DeltaSeconds);
			World->TickGroup = TG_PrePhysics; // reset this to the start tick group
			FTickTaskManagerInterface::Get().StartFrame(World, DeltaSeconds, TickType, LevelsToTick);

			{
				World->RunTickGroup(TG_PrePhysics, true);
			}

			World->bInTick = false;
			World->EnsureCollisionTreeIsBuilt();
			World->bInTick = true;

			{
				World->RunTickGroup(TG_StartPhysics, true);
			}
			{
				World->RunTickGroup(TG_DuringPhysics, false); // No wait here, we should run until idle though. We don't care if all of the async ticks are done before we start running post-phys stuff
			}
			
			World->TickGroup = TG_EndPhysics; // set this here so the current tick group is correct during collision notifies, though I am not sure it matters. 'cause of the false up there^^^
			{
				World->RunTickGroup(TG_EndPhysics, true);
			}
			{
				World->RunTickGroup(TG_PostPhysics, true);
			}

		}
		else if (bIsPaused)
		{
			FTickTaskManagerInterface::Get().RunPauseFrame(World, DeltaSeconds, LEVELTICK_PauseTick, LevelsToTick);
		}

		// We only want to run the following once, so only run it for the source level collection.
		if (LevelCollections[i].GetType() == ELevelCollectionType::DynamicSourceLevels)
		{
			if (!bIsPaused)
			{
				// Issues level streaming load/unload requests based on local players being inside/outside level streaming volumes.
				if (World->IsGameWorld())
				{
					World->ProcessLevelStreamingVolumes();

					if (World->WorldComposition)
					{
						World->WorldComposition->UpdateStreamingState();
					}
					
				}
			}
		}

		if (bDoingActorTicks)
		{
			{
				World->RunTickGroup(TG_PostUpdateWork, true);
			}
			{
				World->RunTickGroup(TG_LastDemotable, true);
			}

			FTickTaskManagerInterface::Get().EndFrame();
		}
	}

	if (bDoingActorTicks)
	{
		FWorldDelegates::OnWorldPostActorTick.Broadcast(World, TickType, DeltaSeconds);
	}

	if (World->Scene)
	{
		// Update SpeedTree wind objects.
		World->Scene->UpdateSpeedTreeWind(World->TimeSeconds);
	}

	// Tick the FX system.
	if (!bIsPaused && World->FXSystem != nullptr)
	{
		World->FXSystem->Tick(DeltaSeconds);
	}

#if WITH_EDITOR
	// Finish up.
	if (World->bDebugFrameStepExecution)
	{
		World->bDebugPauseExecution = true;
		World->bDebugFrameStepExecution = false;
	}
#endif


	World->bInTick = false;

	// players only request from last frame
	if (World->bPlayersOnlyPending)
	{
		World->bPlayersOnly = World->bPlayersOnlyPending;
		World->bPlayersOnlyPending = false;
	}

#if WITH_EDITOR
	if (GIsEditor && World->bDoDelayedUpdateCullDistanceVolumes)
	{
		World->bDoDelayedUpdateCullDistanceVolumes = false;
		World->UpdateCullDistanceVolumes();
	}
#endif // WITH_EDITOR

	// Dump the viewpoints with which we were rendered last frame. They will be updated when the world is next rendered.
	World->ViewLocationsRenderedLastFrame.Reset();

	UWorld* WorldParam = World;
	ENQUEUE_RENDER_COMMAND(TickInGamePerfTrackersRT)(
		[WorldParam](FRHICommandList& RHICmdList)
		{
			//Tick game and other thread trackers.
			for (int32 Tracker = 0; Tracker < (int32)EInGamePerfTrackers::Num; ++Tracker)
			{
				WorldParam->PerfTrackers->GetInGamePerformanceTracker((EInGamePerfTrackers)Tracker, EInGamePerfTrackerThreads::RenderThread).Tick();
			}
		});

	//Update Scene Cpatures
	if (World->Scene)
	{
		World->SendAllEndOfFrameUpdates();
		USceneCaptureComponent::UpdateDeferredCaptures(World->Scene);
	}
}

void URelatedWorld::HandleBeginPlay()
{
	AGameModeBase* GM = GetWorld()->GetAuthGameMode();
	AWorldSettings* Info = Context()->World()->GetWorldSettings();

	if (GM != nullptr)
	{
		Info->NotifyBeginPlay();
		Info->NotifyMatchStarted();
	}
}

AActor* URelatedWorld::SpawnActor(UClass* Class, const FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, AActor* Owner)
{
	UWorld* WorldToSpawn = Context()->World();
	check(WorldToSpawn);

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = CollisionHandlingOverride;

	AActor* SpawnedActor = WorldToSpawn->SpawnActor<AActor>(Class, SpawnTransform, SpawnParams);

	return SpawnedActor;
}

bool URelatedWorld::MoveActorToWorld(AActor* InActor, bool bTranslateLocation)
{
	if (!IsValid(InActor) || InActor->IsPendingKill())
	{
		return false;
	}

	UWorldDirector* Director = GetTypedOuter<UWorldDirector>();
	URelatedWorld* OldRWorld = Director->GetRelatedWorldFromActor(InActor);

	if (OldRWorld != nullptr)
	{
		if (!(bIsNetworkedWorld & OldRWorld->bIsNetworkedWorld))
		{
			if (OldRWorld->bIsNetworkedWorld)
			{
				if (PersistentWorld->NetDriver->ShouldClientDestroyActor(InActor))
				{
					PersistentWorld->NetDriver->NotifyActorDestroyed(InActor);
				}
				PersistentWorld->NetDriver->RemoveNetworkActor(InActor);
			}

			if (bIsNetworkedWorld)
			{
				PersistentWorld->NetDriver->AddNetworkActor(InActor);
			}
		}
	}

	//Translate coordinates into new one
	USceneComponent* RootComponent = InActor->GetRootComponent();

	if (RootComponent)
	{
		FIntVector OldTranslation = OldRWorld != nullptr ? OldRWorld->GetWorldTranslation() : FIntVector::ZeroValue;

		FVector Location = FRepMovement::RebaseOntoZeroOrigin(RootComponent->GetComponentLocation(), RootComponent);
		
		if (bTranslateLocation)
		{
			Location.X += OldTranslation.X - WorldLocation.X;
			Location.Y += OldTranslation.Y - WorldLocation.Y;
			Location.Z += OldTranslation.Z - WorldLocation.Z;
		}

		FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(Location, Context()->World()->OriginLocation);

		RootComponent->SetWorldLocation(NewLocation);
	}

	URelatedWorldNetLocCorrectionComponent* CorrectionComp = Cast<URelatedWorldNetLocCorrectionComponent>(InActor->GetComponentByClass(URelatedWorldNetLocCorrectionComponent::StaticClass()));

	if (CorrectionComp != nullptr)
	{
		CorrectionComp->NotifyWorldChanged(this);
	}

	return InActor->Rename(nullptr, Context()->World()->PersistentLevel);
}

void URelatedWorld::SetWorldOrigin(FIntVector NewOrigin)
{
	UWorld* World = Context()->World();
	check(World);
	World->SetNewWorldOrigin(NewOrigin);
}

void URelatedWorld::TranslateWorld(FIntVector NewLocation)
{
	WorldLocation = NewLocation;
}

URelatedWorld* UWorldDirector::CreateAbstractWorld(UObject* WorldContextObject, FName WorldName, FIntVector LevelLocation, bool IsNetWorld)
{
	URelatedWorld* rWorld = nullptr;

	if (Worlds.Num())
	{
		rWorld = Worlds.FindRef(WorldName);

		if (rWorld != nullptr)
		{
			UE_LOG(LogWorldDirector, Warning, TEXT("Abstract level %s already created as %s"), *WorldName.ToString(), *rWorld->GetName());
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
	rWorld->TranslateWorld(LevelLocation);
	rWorld->SetPersistentWorld(Context.OwningGameInstance->GetWorld());
	rWorld->HandleBeginPlay();

	Worlds.Add(WorldName, rWorld);

	return rWorld;
}

URelatedWorld* UWorldDirector::LoadRelatedLevel(UObject* WorldContextObject, FName LevelName, FIntVector LevelLocation, bool IsNetWorld)
{
	URelatedWorld* rWorld = nullptr;
	
	if (Worlds.Num())
	{
		rWorld = Worlds.FindRef(LevelName);

		if (rWorld != nullptr)
		{
			UE_LOG(LogWorldDirector, Warning, TEXT("Level %s already loaded as %s"), *LevelName.ToString(), *rWorld->GetName());
			return nullptr;
		}
	}

	UPackage* WorldPackage = nullptr;
	UWorld* World = nullptr;
	FString MapName = LevelName.ToString();
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
		UWorld::WorldTypePreLoadMap.FindOrAdd(LevelName) = Context.WorldType;

		WorldPackage = FindPackage(nullptr, *MapName);

		if (WorldPackage == nullptr)
		{
			WorldPackage = LoadPackage(nullptr, *MapName, LOAD_None);
		}

		UWorld::WorldTypePreLoadMap.Remove(LevelName);

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
		Context.ActiveNetDrivers = GEngine->GetWorldContextFromWorld(WorldContextObject->GetWorld())->ActiveNetDrivers;
		Context.World()->NetDriver = WorldContextObject->GetWorld()->NetDriver;
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
	rWorld->TranslateWorld(LevelLocation);
	rWorld->SetPersistentWorld(Context.OwningGameInstance->GetWorld());
	rWorld->HandleBeginPlay();

	Worlds.Add(LevelName, rWorld);

	return rWorld;
}

void UWorldDirector::UnloadAllRelatedLevels()
{
	TArray<FName> LevelNames;
	Worlds.GetKeys(LevelNames);
	
	for (FName LevelName : LevelNames)
	{
		UnloadRelatedLevelByName(LevelName);
	}
}

void UWorldDirector::UnloadRelatedLevelByName(FName LevelName)
{
	if (!Worlds.Num())
	{
		return;
	}

	URelatedWorld* rWorld = Worlds.FindRef(LevelName);
	
	if (rWorld != nullptr)
	{
		UnloadRelatedLevel(rWorld);
	}
}

void UWorldDirector::UnloadRelatedLevel(URelatedWorld* RelatedWorld)
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
