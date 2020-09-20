// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Modules/ModuleManager.h"

#include "RelatedWorldModuleInterface.h"
#include "WorldDirector.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldDirector, Log, All);

UCLASS(BlueprintType)
class RELATEDWORLD_API UWorldDirector : public UObject
{
	GENERATED_BODY()

public:
	/** Get World Director object */
	UFUNCTION(BlueprintPure, Category = "WorldDirector", Meta=(DisplayName="GetWorldDirctor"))
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
	 * @param	IsNetWorld				Should the world replicate actors to conneted clients
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector", Meta = (WorldContext = "WorldContextObject"))
		URelatedWorld* CreateEmptyWorld(UObject* WorldContextObject, FName WorldName, FIntVector WorldTranslation, bool IsNetWorld = true);

	/**
	 * Load related world from map
	 * @return	RelatedWorld			new RelatedWorld object
	 *
	 * @param	WorldName				Name of the loading map
	 * @param	WorldTranslation		World translation relative to the permanent world
	 * @param	IsNetWorld				Should the world replicate actors to conneted clients
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector", Meta=(WorldContext="WorldContextObject"))
		URelatedWorld* LoadRelatedWorld(UObject* WorldContextObject, FName WorldName, FIntVector WorldTranslation, bool IsNetWorld = true);

	/** Unload all loaded related worlds */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void UnloadAllRelatedWorlds();

	/** 
	 * Unload related world by name
	 * @param	WorldName	The world to be unloaded
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void UnloadRelatedWorldByName(FName WorldName);

	/**
	 * Unload related world by name
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

private:
	TMap<FName, URelatedWorld*> Worlds;

};
