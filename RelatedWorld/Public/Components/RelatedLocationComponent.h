// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementReplication.h"
#include "RelatedLocationComponent.generated.h"

class UWorldDirector;
class URelatedWorld;

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FRelatedLocationComponentWorldChanged, URelatedLocationComponent, OnRelatedWorldChanged, URelatedWorld*, RelatedWorld);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FRelatedLocationComponentWorldTranslationChanged, URelatedLocationComponent, OnRelatedWorldTranslationChanged, const FIntVector&, WorldTranslation);

UCLASS()
class RELATEDWORLD_API URelatedLocationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URelatedLocationComponent();
	virtual void NotifyWorldChanged(URelatedWorld* NewWorld);
	virtual void NotifyWorldTranslationChanged(const FIntVector& NewWorldTranslation, bool DispatchEvent = true);

	UFUNCTION()
		void RelatedWorldReceiveNewTranslation(const FIntVector& NewWorldTranslation);

	UFUNCTION()
		void OnRep_WorldTranslation();

/** BEGIN HOOKS **/

	void AActor_OnRep_ReplicatedMovement(const FIntVector& WorldOrigin);

	void ACharacter_ClientMoveResponsePacked(const FIntVector& WorldOrigin, const struct FCharacterMoveResponsePackedBits& PackedBits);

	void APlayerController_ServerUpdateCamera(FVector_NetQuantize CamLoc, int32 CamPitchAndYaw);

	UFUNCTION(Server, UnReliable, WithValidation)
		void Server_APlayerController_ServerUpdateCamera(FVector_NetQuantize CamLoc, int32 CamPitchAndYaw);

/** END HOOKS **/

/** BEGIN UActorComponent Interface **/

public:
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent();

/** END UActorComponent Interface **/

//** BEGIN UObject Interface **/

public:
	virtual void GetLifetimeReplicatedProps(TArray< class FLifetimeProperty >& OutLifetimeProps) const override;

//** END UObject Interface **/

public:
	UPROPERTY(BlueprintAssignable, Category = "WorldDirector")
		FRelatedLocationComponentWorldChanged OnRelatedWorldChanged;

	UPROPERTY(BlueprintAssignable, Category = "WorldDirector")
		FRelatedLocationComponentWorldTranslationChanged OnRelatedWorldTranslationChanged;

private:
	URelatedWorld* RelatedWorld;

	UPROPERTY(ReplicatedUsing=OnRep_WorldTranslation)
		FIntVector WorldTranslation;

	FNetBitReader MoveResponseBitReader;
	FCharacterMoveResponseDataContainer MoveResponseDataContainer;

};
