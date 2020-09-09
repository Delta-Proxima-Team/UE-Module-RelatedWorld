// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RelatedWorldNetLocCorrectionComponent.generated.h"

UCLASS(Meta=(BlueprintSpawnableComponent))
class RELATEDWORLD_API URelatedWorldNetLocCorrectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URelatedWorldNetLocCorrectionComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void InitializeComponent() override;
	virtual void NotifyLocationChanged(const FVector& NewLocation);
	virtual void NotifyWorldLocationChanged(const FIntVector& NewWorldLocation);

private:
	UFUNCTION()
		void OnRep_RelatedLocation();

private:
	UPROPERTY(Replicated)
		FIntVector RelatedWorldLocation;

	UPROPERTY(ReplicatedUsing=OnRep_RelatedLocation)
		FVector RelatedLocation;

	AActor* ActorOwner;
};
