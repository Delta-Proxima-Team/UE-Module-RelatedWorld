// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Modules/ModuleManager.h"

#include "RelatedWorldModuleInterface.h"
#include "WorldDirector.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldDirector, Log, All);

enum class EWorldDomain : uint8;
class URelatedWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMoveActorToWorld, AActor*, Actor, URelatedWorld*, OldWorld, URelatedWorld*, NewWorld);

UCLASS(BlueprintType)
class RELATEDWORLD_API UWorldDirector : public UObject
{
	GENERATED_BODY()

public:
	/** Get World Director object */
	UFUNCTION(BlueprintPure, Category = "WorldDirector", Meta=(DisplayName="GetWorldDirector"))
		static UWorldDirector* Get()
		{
			return FModuleManager::LoadModuleChecked<IRelatedWorldModule>("RelatedWorld").GetWorldDirector();
		}

	/** 
	 * Create empty related world without any actors
	 * @return	RelatedWorld			new RelatedWorld object
	 * 
	 * @param	WorldName				Name of the new world
	 * @param	WorldTranslation		World translation relative to the permanent world
	 * @param	IsNetWorld				Should the world replicate actors to connected clients
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector", Meta = (WorldContext = "WorldContextObject"))
		URelatedWorld* CreateEmptyWorld(UObject* WorldContextObject, FName WorldName, FIntVector WorldTranslation, EWorldDomain WorldDomain, bool IsNetWorld = true);

	/**
	 * Load related world from map
	 * @return	RelatedWorld			new RelatedWorld object
	 *
	 * @param	WorldName				Name of the loading map
	 * @param	WorldTranslation		World translation relative to the permanent world
	 * @param	IsNetWorld				Should the world replicate actors to connected clients
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector", Meta=(WorldContext="WorldContextObject"))
		URelatedWorld* LoadRelatedWorld(UObject* WorldContextObject, FName WorldName, FIntVector WorldTranslation, EWorldDomain WorldDomain, bool IsNetWorld = true);

	/** Unload all loaded related worlds */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void UnloadAllRelatedWorlds();

	/** 
	 * Unload related world by name
	 * 
	 * @param	WorldName	The world to be unloaded
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void UnloadRelatedWorldByName(FName WorldName);

	/**
	 * Unload related world by name
	 * 
	 * @param	RelatedWorld	The world to be unloaded
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void UnloadRelatedWorld(URelatedWorld* RelatedWorld);

	/**
	 * Spawn Actors with given transform
	 * @return	Actor that just spawned
	 *
	 * @param	TargetWorld							World to be spawned on
	 * @param	Class								Class to Spawn
	 * @param	SpawnTransform						World Transform to spawn on
	 *
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		AActor* SpawnActor(URelatedWorld* TargetWorld, UClass* Class, const FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, AActor* Owner);

	/**
	 * Returns the related world if the actor is on it or NULL if not
	 * @return	RelatedWorld	RelatedWorld object of NULL
	 * 
	 * @param	InActor			Actor for check
	 * 
	 */
	UFUNCTION(BlueprintPure, Category = "WorldDirector")
		URelatedWorld* GetRelatedWorldFromActor(AActor* InActor) const;

	/**
	 * Returns the related world if world with giving name are loaded
	 * @return	RelatedWorld		RelatedWorld object of NULL
	 *
	 * @param	WorldName			World name
	 *
	 */
	UFUNCTION(BlueprintPure, Category = "WorldDirector")
		URelatedWorld* GetRelatedWorldByName(FName WorldName) const;

	/**
	 * Returns the related world if the actor is on it or NULL if not
	 *
	 * @param	World					World to move. If NULL then actor will be moved into main world
	 * @param	InActor					Actor for move
	 * @param	bTranslateLocation		Translate current location into target space
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		bool MoveActorToWorld(URelatedWorld* World, AActor* InActor, bool bTranslateLocation);

	FOnMoveActorToWorld OnMoveActorToWorld;

private:
	TMap<FName, URelatedWorld*> Worlds;

};
