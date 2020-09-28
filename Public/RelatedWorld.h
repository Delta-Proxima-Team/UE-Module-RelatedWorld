// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RelatedWorld.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldTranslationChanged, const FIntVector&, WorldTranslation);

UCLASS()
class RELATEDWORLD_API URelatedWorldUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 
	 * Convect Actor related world location into world location
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		static FVector ActorLocationToWorldLocation(AActor* InActor);

	/** 
	 * Convert related world location into world location
	*/
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		static FVector RelatedWorldLocationToWorldLocation(class URelatedWorld* RelatedWorld, const FVector& Location);

	static FVector CONVERT_RelToWorld(const FIntVector& Translation, const FVector& Location);

	/** 
	 * Contert world location into relative world location
	*/
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		static FVector WorldLocationToRelatedWorldLocation(class URelatedWorld* RelatedWorld, const FVector& Location);

	static FVector CONVERT_WorldToRel(const FIntVector& Translation, const FVector& Location);

	/** 
	 * Convert relative location into another relative location
	*/
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		static FVector RelatedWorldLocationToRelatedWorldLocation(class URelatedWorld* From, class URelatedWorld* To, const FVector& Location);

	static FVector CONVERT_RelToRel(const FIntVector& From, const FIntVector& To, const FVector& Location);

	/** Returns the RelatedLocation component from giving actor */
	UFUNCTION(BlueprintPure, Category = "WorldDirector")
		static URelatedLocationComponent* GetRelatedLocationComponent(AActor* InActor);

//** BEGIN GameplayStatics wrapper

	/** Returns the player controller at the specified player index */
	UFUNCTION(BlueprintPure, Category = "WorldDirector|Game", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
		static class APlayerController* GetPlayerController(const UObject* WorldContextObject, int32 PlayerIndex);

	/** Returns the player controller that has the given controller ID */
	UFUNCTION(BlueprintPure, Category = "WorldDirector|Game", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
		static class APlayerController* GetPlayerControllerFromID(const UObject* WorldContextObject, int32 ControllerID);

	/** Returns the player pawn at the specified player index */
	UFUNCTION(BlueprintPure, Category = "WorldDirector|Game", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
		static class APawn* GetPlayerPawn(const UObject* WorldContextObject, int32 PlayerIndex);

	/** Returns the player character (NULL if the player pawn doesn't exist OR is not a character) at the specified player index */
	UFUNCTION(BlueprintPure, Category = "WorldDirector|Game", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
		static class ACharacter* GetPlayerCharacter(const UObject* WorldContextObject, int32 PlayerIndex);

	/** Returns the player's camera manager for the specified player index */
	UFUNCTION(BlueprintPure, Category = "WorldDirector|Game", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
		static class APlayerCameraManager* GetPlayerCameraManager(const UObject* WorldContextObject, int32 PlayerIndex);

	/** Returns the current GameModeBase or Null if it can't be retrieved, such as on the client */
	UFUNCTION(BlueprintPure, Category = "WorldDirector|Game", meta = (WorldContext = "WorldContextObject"))
		static class AGameModeBase* GetGameMode(const UObject* WorldContextObject);

	/** Returns the current GameStateBase or Null if it can't be retrieved */
	UFUNCTION(BlueprintPure, Category = "WorldDirector|Game", meta = (WorldContext = "WorldContextObject"))
		static class AGameStateBase* GetGameState(const UObject* WorldContextObject);
};

UCLASS(BlueprintType)
class RELATEDWORLD_API URelatedWorld : public UObject, public FTickableGameObject
{
	friend class UWorldDirector;
	GENERATED_BODY()

public:
	void HandleBeginPlay();

	FORCEINLINE FWorldContext* Context() const { return _Context; }

	/** Returns true if the world support networking */
	UFUNCTION(BlueprintPure, Category = "WorldDirector")
		FORCEINLINE bool IsNetworkedWorld() const { return bIsNetworkedWorld; }

	/** Returns the current translation of the world */
	UFUNCTION(BlueprintPure, Category = "WorldDirector")
		FORCEINLINE FIntVector GetWorldTranslation() const { return WorldTranslation; }

	/**
	 * Spawn Actors with given transform
	 * @return	Actor that just spawned
	 *
	 * @param	Class								Class to Spawn
	 * @param	SpawnTransform						World Transform to spawn on
	 * @param	CollisionHandlingOverride			Specifies how to handle collisions at the spawn spot
	 * @param	Owner								Actors owner
	 *
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		AActor* SpawnActor(UClass* Class, const FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, AActor* Owner);

	/** Translate world origin to specified position  */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void SetWorldOrigin(FIntVector NewOrigin);

	/** Translate world to specified position  */
	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void TranslateWorld(FIntVector NewTranslation);
private:
	void SetContext(FWorldContext* Context) { _Context = Context; }
	void SetNetworked(bool bNetworked) { bIsNetworkedWorld = bNetworked; }
	void SetPersistentWorld(UWorld* World) { PersistentWorld = World; }

/** BEGIN FTickableGameObject Interface **/
public:
	void Tick(float DeltaSeconds) override;
	bool IsTickable() const override;
	bool IsTickableInEditor() const override { return false; };
	bool IsTickableWhenPaused() const override { return true; };
	TStatId GetStatId() const override { return TStatId(); };

/** END FTickableGameObject Interface **/

/** BEGIN UObject Interface **/
public:
	UWorld* GetWorld() const override;

/** END UObject Interface **/

public:
	FOnWorldTranslationChanged OnWorldTranslationChanged;

private:
	FWorldContext* _Context;
	UWorld* PersistentWorld;
	bool bIsNetworkedWorld;
	FIntVector WorldTranslation;
};
