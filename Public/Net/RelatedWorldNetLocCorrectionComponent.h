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
	virtual void InitializeComponent() override;
	virtual void NotifyLocationChanged(const FVector& NewLocation);
	virtual void NotifyOriginChanged(const FIntVector& NewOrigin);
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
		void OnRep_RelatedOrigin();

	UFUNCTION()
		void OnRep_RelatedLocation();

private:
	UPROPERTY(ReplicatedUsing=OnRep_RelatedOrigin)
		FIntVector RelatedOrigin;

	UPROPERTY(ReplicatedUsing=OnRep_RelatedLocation)
		FVector RelatedLocation;

	AActor* ActorOwner;
};
