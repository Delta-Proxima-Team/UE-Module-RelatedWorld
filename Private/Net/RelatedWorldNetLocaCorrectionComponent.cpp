// Copyright Delta-Proxima Team (c) 2007-2020

#include "Net/RelatedWorldNetLocCorrectionComponent.h"
#include "Net/UnrealNetwork.h"
#include "WorldDirector.h"

URelatedWorldNetLocCorrectionComponent::URelatedWorldNetLocCorrectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void URelatedWorldNetLocCorrectionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URelatedWorldNetLocCorrectionComponent, RelatedOrigin);
	DOREPLIFETIME(URelatedWorldNetLocCorrectionComponent, RelatedLocation);
}

void URelatedWorldNetLocCorrectionComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (IsValid(GetOwner()) && !GetOwner()->HasAnyFlags(RF_ClassDefaultObject))
	{
		ActorOwner = GetOwner();
	}
}

void URelatedWorldNetLocCorrectionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ActorOwner && !ActorOwner->IsPendingKill())
	{
		URelatedWorld* rWorld = UWorldDirector::GetWorldDirector()->GetRelatedWorldFromActor(ActorOwner);

		if (rWorld)
		{
			FIntVector WorldOrigin = rWorld->Context()->World()->OriginLocation;
			
			if (WorldOrigin != RelatedOrigin)
			{
				NotifyOriginChanged(WorldOrigin);
			}

			FVector ActorLocation = ActorOwner->GetActorLocation();

			if (ActorLocation != RelatedLocation)
			{
				NotifyLocationChanged(ActorLocation);
			}
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::NotifyLocationChanged(const FVector& NewLocation)
{
	RelatedLocation = NewLocation;
}

void URelatedWorldNetLocCorrectionComponent::OnRep_RelatedLocation()
{
	FIntVector Shift = RelatedOrigin - GetWorld()->OriginLocation;
	FVector ActorLocation = FRepMovement::RebaseOntoLocalOrigin(RelatedLocation, Shift);
	ActorOwner->SetActorLocation(ActorLocation);

	GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Red, FString::Printf(TEXT("newloc %s"), *ActorLocation.ToString()));
}

void URelatedWorldNetLocCorrectionComponent::NotifyOriginChanged(const FIntVector& NewOrigin)
{
	RelatedOrigin = NewOrigin;
}

void URelatedWorldNetLocCorrectionComponent::OnRep_RelatedOrigin()
{

}
