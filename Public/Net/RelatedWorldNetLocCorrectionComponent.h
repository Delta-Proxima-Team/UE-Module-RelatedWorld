// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RelatedWorldNetLocCorrectionComponent.generated.h"

class URelatedWorld;

UCLASS(Meta=(BlueprintSpawnableComponent))
class RELATEDWORLD_API URelatedWorldNetLocCorrectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URelatedWorldNetLocCorrectionComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void InitializeComponent() override;
	virtual void NotifyWorldLocationChanged(const FIntVector& NewWorldLocation);

	UFUNCTION()
		void OnRep_ReplicatedMovement();

private:
	void SyncReplicatedPhysicsSimulation();
	void PostNetReceivePhysicState();
	void PostNetReceiveLocationAndRotation();

private:
	URelatedWorld* RelatedWorld;
	bool SavedbRepPhysics;
	
	UPROPERTY(Replicated)
		bool bNeedCorrection;

	UPROPERTY(Replicated)
		FIntVector RelatedWorldLocation;

	UPROPERTY(Replicated)
		AActor* ActorOwner;


};
