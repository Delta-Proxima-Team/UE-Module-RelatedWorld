// Copyright Delta-Proxima Team (c) 2007-2020

#include "Components/RelatedLocationComponent.h"
#include "WorldDirector.h"
#include "RelatedWorld.h"

#include "Net/UnrealNetwork.h"

URelatedLocationComponent::URelatedLocationComponent()
{
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;
}

void URelatedLocationComponent::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URelatedLocationComponent, WorldTranslation);
}

void URelatedLocationComponent::InitializeComponent()
{
	RelatedWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(GetOwner());

	if (RelatedWorld != nullptr && GetNetMode() == NM_DedicatedServer)
	{
		WorldTranslation = RelatedWorld->GetWorldTranslation();

		RelatedWorld->OnWorldTranslationChanged.AddDynamic(this, &URelatedLocationComponent::RelatedWorldReceiveNewTranslation);
	}

	Super::InitializeComponent();
}

void URelatedLocationComponent::UninitializeComponent()
{
	if (RelatedWorld != nullptr && GetOwnerRole() == ROLE_Authority)
	{
		RelatedWorld->OnWorldTranslationChanged.RemoveDynamic(this, &URelatedLocationComponent::RelatedWorldReceiveNewTranslation);
	}
	
	Super::UninitializeComponent();
}

void URelatedLocationComponent::NotifyWorldChanged(URelatedWorld* NewWorld)
{
	RelatedWorld->OnWorldTranslationChanged.RemoveDynamic(this, &URelatedLocationComponent::RelatedWorldReceiveNewTranslation);
	NewWorld->OnWorldTranslationChanged.AddDynamic(this, &URelatedLocationComponent::RelatedWorldReceiveNewTranslation);

	RelatedWorld = NewWorld;
	OnRelatedWorldChanged.Broadcast(RelatedWorld);
	NotifyWorldTranslationChanged(RelatedWorld->GetWorldTranslation(), false);
}

void URelatedLocationComponent::NotifyWorldTranslationChanged(const FIntVector& NewWorldTranslation, bool DispatchEvent)
{
	WorldTranslation = NewWorldTranslation;

	if (DispatchEvent)
	{
		OnRelatedWorldTranslationChanged.Broadcast(WorldTranslation);
	}
}

void URelatedLocationComponent::RelatedWorldReceiveNewTranslation(const FIntVector& NewWorldTranslation)
{
	NotifyWorldTranslationChanged(NewWorldTranslation);
}

void URelatedLocationComponent::OnRep_WorldTranslation()
{
	APawn* Owner = Cast<APawn>(GetOwner());

	if (Owner != nullptr && Owner->IsLocallyControlled())
	{
		GetWorld()->SetNewWorldOrigin(WorldTranslation);
	}

	OnRelatedWorldTranslationChanged.Broadcast(WorldTranslation);
}
