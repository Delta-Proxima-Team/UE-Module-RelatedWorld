// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RelatedWorldNetLocCorrectionComponent.generated.h"

class URelatedWorld;

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FRelatedWorldNetLocCorrectionComponentWorldLocationChanged, URelatedWorldNetLocCorrectionComponent, OnWorldLocationChanged, FIntVector, NewWorldLocation);

UCLASS(Meta=(BlueprintSpawnableComponent))
class RELATEDWORLD_API URelatedWorldNetLocCorrectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URelatedWorldNetLocCorrectionComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void InitializeComponent() override;
	virtual void NotifyWorldChanged(URelatedWorld* NewWorld);
	virtual void NotifyWorldLocationChanged(const FIntVector& NewWorldLocation);

	UPROPERTY(BlueprintAssignable, Category = "WorldDirector")
		FRelatedWorldNetLocCorrectionComponentWorldLocationChanged OnWorldLocationChanged;

	UFUNCTION()
		void OnRep_RelatedWorldLocation();

	UFUNCTION()
		void OnRep_ReplicatedMovement();

	UFUNCTION()
		void ServerMoveNoBase(FVector& ClientLoc);

	UFUNCTION()
		void ClientAdjustPosition(FVector& NewLoc, bool bBaseRelativePosition);

	UFUNCTION()
		void OnRep_Initial();

	UFUNCTION()
		void PreRPC_ServerUpdateCamera(APlayerController* Context, FVector_NetQuantize& CamLoc, int32 CamPitchAndYaw);

	UFUNCTION(Server, UnReliable, WithValidation)
		void ServerUpdateCamera(APlayerController* Context, const FVector_NetQuantize& CamLoc, int32 CamPitchAndYaw);
		

private:
	void SyncReplicatedPhysicsSimulation();
	void PostNetReceivePhysicState();
	void PostNetReceiveLocationAndRotation();

private:
	URelatedWorld* RelatedWorld;
	bool SavedbRepPhysics;
	
	UPROPERTY(ReplicatedUsing = OnRep_Initial)
		bool bInitialReplication;

	UPROPERTY(Replicated)
		bool bNeedCorrection;

	UPROPERTY(ReplicatedUsing = OnRep_RelatedWorldLocation)
		FIntVector RelatedWorldLocation;

	UPROPERTY(Replicated)
		AActor* ActorOwner;


};
