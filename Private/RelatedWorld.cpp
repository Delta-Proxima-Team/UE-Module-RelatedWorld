// Copyright Delta-Proxima Team (c) 2007-2020

#include "RelatedWorld.h"
#include "WorldDirector.h"
#include "Components/RelatedLocationComponent.h"

#include "FxSystem.h"
#include "InGamePerformanceTracker.h"
#include "TickTaskManagerInterface.h"
#include "Engine/WorldComposition.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SceneCaptureComponent.h"
#include "GameFramework/Character.h"

FVector URelatedWorldUtils::ActorLocationToWorldLocation(AActor* InActor)
{
	FVector ActorLocation = InActor->GetActorLocation();

	URelatedWorld* rWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(InActor);

	if (rWorld != nullptr)
	{
		ActorLocation = RelatedWorldLocationToWorldLocation(rWorld, ActorLocation);
	}

	return ActorLocation;
}

FVector URelatedWorldUtils::RelatedWorldLocationToWorldLocation(URelatedWorld* RelatedWorld, const FVector& Location)
{
	FIntVector Translation = RelatedWorld->GetWorldTranslation();

	FVector rAbsLocation = FRepMovement::RebaseOntoZeroOrigin(Location, RelatedWorld->Context()->World()->OriginLocation);
	FVector pAbsLocation = CONVERT_RelToWorld(Translation, rAbsLocation);
	return FRepMovement::RebaseOntoLocalOrigin(pAbsLocation, RelatedWorld->GetWorld()->OriginLocation);
}

FVector URelatedWorldUtils::WorldLocationToRelatedWorldLocation(URelatedWorld* RelatedWorld, const FVector& Location)
{
	FIntVector Translation = RelatedWorld->GetWorldTranslation();

	FVector pAbsLocation = FRepMovement::RebaseOntoZeroOrigin(Location, RelatedWorld->GetWorld()->OriginLocation);
	FVector rAbsLocation = CONVERT_WorldToRel(Translation, pAbsLocation);
	return FRepMovement::RebaseOntoLocalOrigin(rAbsLocation, RelatedWorld->Context()->World()->OriginLocation);
}

FVector URelatedWorldUtils::RelatedWorldLocationToRelatedWorldLocation(class URelatedWorld* From, class URelatedWorld* To, const FVector& Location)
{
	FIntVector TranslationFrom = From->GetWorldTranslation();
	FIntVector TranslationTo = To->GetWorldTranslation();

	FVector rfAbsLocation = FRepMovement::RebaseOntoZeroOrigin(Location, From->Context()->World()->OriginLocation);
	FVector rtAbsLocation = CONVERT_RelToRel(TranslationFrom, TranslationTo, rfAbsLocation);
	return FRepMovement::RebaseOntoLocalOrigin(rfAbsLocation, To->Context()->World()->OriginLocation);
}

FVector URelatedWorldUtils::CONVERT_RelToWorld(const FIntVector& Translation, const FVector& Location)
{
	return FVector(Location.X + Translation.X, Location.Y + Translation.Y, Location.Z + Translation.Z);
}

FVector URelatedWorldUtils::CONVERT_WorldToRel(const FIntVector& Translation, const FVector& Location)
{
	return FVector(Location.X - Translation.X, Location.Y - Translation.Y, Location.Z - Translation.Z);
}

FVector URelatedWorldUtils::CONVERT_RelToRel(const FIntVector& From, const FIntVector& To,  const FVector& Location)
{
	return FVector
	(
		Location.X + From.X - To.X,
		Location.Y + From.Y - To.Y,
		Location.Z + From.Z - To.Z
	);
}

URelatedLocationComponent* URelatedWorldUtils::GetRelatedLocationComponent(AActor* InActor)
{
	return Cast<URelatedLocationComponent>(InActor->GetComponentByClass(URelatedLocationComponent::StaticClass()));
}

APlayerController* URelatedWorldUtils::GetPlayerController(const UObject* WorldContextObject, int32 PlayerIndex)
{
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();

	return UGameplayStatics::GetPlayerController(GameInstance, PlayerIndex);
}

APlayerController* URelatedWorldUtils::GetPlayerControllerFromID(const UObject* WorldContextObject, int32 ControllerID)
{
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();

	return UGameplayStatics::GetPlayerControllerFromID(GameInstance, ControllerID);
}

APawn* URelatedWorldUtils::GetPlayerPawn(const UObject* WorldContextObject, int32 PlayerIndex)
{
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();

	return UGameplayStatics::GetPlayerPawn(GameInstance, PlayerIndex);
}

ACharacter* URelatedWorldUtils::GetPlayerCharacter(const UObject* WorldContextObject, int32 PlayerIndex)
{
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();

	return UGameplayStatics::GetPlayerCharacter(GameInstance, PlayerIndex);
}

APlayerCameraManager* URelatedWorldUtils::GetPlayerCameraManager(const UObject* WorldContextObject, int32 PlayerIndex)
{
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();

	return UGameplayStatics::GetPlayerCameraManager(GameInstance, PlayerIndex);
}

AGameModeBase* URelatedWorldUtils::GetGameMode(const UObject* WorldContextObject)
{
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();

	return UGameplayStatics::GetGameMode(GameInstance);
}

AGameStateBase* URelatedWorldUtils::GetGameState(const UObject* WorldContextObject)
{
	UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance();

	return UGameplayStatics::GetGameState(GameInstance);
}

bool URelatedWorld::IsTickable() const
{
	return Context() ? true : false;
}

UWorld* URelatedWorld::GetWorld() const
{
	return Context()->OwningGameInstance->GetWorld();
}

void URelatedWorld::Tick(float DeltaSeconds)
{
	ELevelTick TickType = LEVELTICK_All;
	UWorld* World = Context()->World();
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
#if ENGINE_MINOR_VERSION < 26 
		// Tick level sequence actors first
		for (int32 i = World->LevelSequenceActors.Num() - 1; i >= 0; --i)
		{
			if (World->LevelSequenceActors[i] != nullptr)
			{
				World->LevelSequenceActors[i]->Tick(DeltaSeconds);
			}
		}
#else
		//World->MovieSceneSequenceTick.Broadcast(DeltaSeconds);
#endif
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

	if (SpawnedActor->GetNetMode() == NM_DedicatedServer && IsNetworkedWorld())
	{
		URelatedLocationComponent* LocationComponent = NewObject<URelatedLocationComponent>(SpawnedActor, TEXT("LocationComponent"), RF_Transient);
		LocationComponent->RegisterComponent();
	}

	return SpawnedActor;
}

void URelatedWorld::SetWorldOrigin(FIntVector NewOrigin)
{
	UWorld* World = Context()->World();
	check(World);
	World->SetNewWorldOrigin(NewOrigin);
}

void URelatedWorld::TranslateWorld(FIntVector NewTranslation)
{
	WorldTranslation = NewTranslation;

	OnWorldTranslationChanged.Broadcast(WorldTranslation);
}
