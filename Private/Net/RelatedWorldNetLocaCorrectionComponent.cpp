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

	DOREPLIFETIME(URelatedWorldNetLocCorrectionComponent, RelatedWorldLocation);
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

	if (ActorOwner && !ActorOwner->IsPendingKill() && GetOwnerRole() == ROLE_Authority)
	{
		URelatedWorld* rWorld = UWorldDirector::GetWorldDirector()->GetRelatedWorldFromActor(ActorOwner);

		if (rWorld)
		{
			FIntVector WorldLocation = rWorld->GetWorldTranslation();
			
			if (WorldLocation != RelatedWorldLocation)
			{
				NotifyWorldLocationChanged(WorldLocation);
			}

			FIntVector rOrign = rWorld->Context()->World()->OriginLocation;
			FVector ActorLocation = ActorOwner->GetActorLocation();
			FVector rActorLocation(ActorLocation.X + rOrign.X, ActorLocation.Y + rOrign.Y, ActorLocation.Z + rOrign.Z);

			if (rActorLocation != RelatedLocation)
			{
				NotifyLocationChanged(rActorLocation);
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
	FIntVector Shift = RelatedWorldLocation - GetWorld()->OriginLocation;
	FVector ActorLocation(RelatedLocation.X + Shift.X,RelatedLocation.Y + Shift.Y,RelatedLocation.Z + Shift.Z);
	ActorOwner->SetActorLocation(ActorLocation);
}

void URelatedWorldNetLocCorrectionComponent::NotifyWorldLocationChanged(const FIntVector& NewWorldLocation)
{
	RelatedWorldLocation = NewWorldLocation;
}
