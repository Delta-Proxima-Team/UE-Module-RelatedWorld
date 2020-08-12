// Copyright Delta-Proxima Team (c) 2007-2020

#include "WorldDirector.h"
#include "Kismet/GameplayStatics.h"
#include "ShaderCompiler.h"
#include "Engine/LevelStreaming.h"
#include "TickTaskManagerInterface.h"
#include "Engine/WorldComposition.h"
#include "FXSystem.h"
#include "Components/SceneCaptureComponent.h"

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
	bool bIsPaused = (World->IsPaused() | GetWorld()->IsPaused());

	if (GIntraFrameDebuggingGameThread)
	{
		return;
	}

	FWorldDelegates::OnWorldTickStart.Broadcast(World, TickType, DeltaSeconds);
	World->bInTick = true;

	// Update time.
	World->RealTimeSeconds += DeltaSeconds;

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
		&& !bIsPaused;

	FLatentActionManager& CurrentLatentActionManager = World->GetLatentActionManager();

	// Reset the list of objects the LatentActionManager has processed this frame
	CurrentLatentActionManager.BeginFrame();

	if (bDoingActorTicks)
	{
		{
			// Reset Async Trace before Tick starts 
			//World->ResetAsyncTrace();
		}
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

	const TArray<FLevelCollection>* LevelCollections = &World->GetLevelCollections();
	TArray<ULevel*> Levels = World->GetLevels();

	for (int32 i = 0; i < LevelCollections->Num(); ++i)
	{
		// Build a list of levels from the collection that are also in the world's Levels array.
		// Collections may contain levels that aren't loaded in the world at the moment.
		TArray<ULevel*> LevelsToTick;
		for (ULevel* CollectionLevel : (*LevelCollections)[i].GetLevels())
		{
			if (Levels.Contains(CollectionLevel))
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
		if ((*LevelCollections)[i].GetType() == ELevelCollectionType::DynamicSourceLevels)
		{
			// Process any remaining latent actions
			if (!bIsPaused)
			{
				// This will process any latent actions that have not been processed already
				CurrentLatentActionManager.ProcessLatentActions(nullptr, DeltaSeconds);
			}
#if 0 // if you need to debug physics drawing in editor, use this. If you type pxvis collision, it will work. 
			else
			{
				// Tick our async work (physics, etc.) and tick with no elapsed time for playersonly
				TickAsyncWork(0.f);
				// Wait for async work to come back
				WaitForAsyncWork();
			}
#endif

			{
				if (TickType != LEVELTICK_TimeOnly && !bIsPaused)
				{
						World->GetTimerManager().Tick(DeltaSeconds);
				}

				{
					//FTickableGameObject::TickObjects(World, TickType, bIsPaused, DeltaSeconds);
				}
			}

			// Update cameras and streaming volumes
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
		// Update Capture components
		USceneCaptureComponent::UpdateDeferredCaptures(World->Scene);
	}

	if (!bIsPaused && World->FXSystem != nullptr)
	{
		World->FXSystem->Tick(DeltaSeconds);
	}

	World->bInTick = false;
}

AActor* URelatedWorld::SpawnActor(UClass* Class, const FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, AActor* Owner)
{
	UWorld* WorldToSpawn = Context()->World();
	check(WorldToSpawn);

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transactional;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = CollisionHandlingOverride;

	AActor* SpawnedActor = WorldToSpawn->SpawnActor<AActor>(Class, SpawnTransform, SpawnParams);

	return SpawnedActor;
}

bool URelatedWorld::MoveActorToWorld(AActor* InActor)
{
	if (!IsValid(InActor) || InActor->IsPendingKill())
	{
		return false;
	}

	UWorld* World = _Context->World();

	return InActor->Rename(nullptr, World->PersistentLevel);
}

URelatedWorld* UWorldDirector::LoadRelatedLevel(UObject* WorldContextObject, FName LevelName, bool IsNetWorld)
{
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

	Context.World()->SetGameMode(URL);

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

	Context.World()->BeginPlay();
	Context.World()->bWorldWasLoadedThisTick = true;
	Context.World()->SetShouldTick(false);

	URelatedWorld* rWorld = NewObject<URelatedWorld>();
	rWorld->AddToRoot();
	rWorld->SetContext(&Context);

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
	URelatedWorld* rWorld = *Worlds.Find(LevelName);
	
	if (rWorld != nullptr)
	{
		UnloadRelatedLevel(rWorld);
	}
}

void UWorldDirector::UnloadRelatedLevel(URelatedWorld* RelatedWorld)
{
	check(RelatedWorld);

	FWorldContext* Context = RelatedWorld->Context();
	UWorld* World = Context->World();

	GEngine->DestroyWorldContext(World);
	RelatedWorld->RemoveFromRoot();
	World->RemoveFromRoot();
	World->CleanupActors();
	World->DestroyWorld(true);
	Worlds.Remove(FName(World->URL.Map));
}

AActor* UWorldDirector::SpawnActor(URelatedWorld* TargetWorld, UClass* Class, const FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, AActor* Owner)
{
	check(TargetWorld);

	return TargetWorld->SpawnActor(Class, SpawnTransform, CollisionHandlingOverride, Owner);
}
