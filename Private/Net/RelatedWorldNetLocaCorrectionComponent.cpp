// Copyright Delta-Proxima Team (c) 2007-2020

#include "Net/RelatedWorldNetLocCorrectionComponent.h"
#include "Net/UnrealNetwork.h"
#include "WorldDirector.h"

void OnRep_ReplicatedMovement_Hook(UObject* Context, FFrame& TheStack, RESULT_DECL)
{
	AActor* Caller = Cast<AActor>(Context);

	if (Caller != nullptr)
	{
		URelatedWorldNetLocCorrectionComponent* HookComp = Cast<URelatedWorldNetLocCorrectionComponent>(Caller->GetComponentByClass(URelatedWorldNetLocCorrectionComponent::StaticClass()));

		if (HookComp != nullptr)
		{
			HookComp->OnRep_ReplicatedMovement();
		}
		else
		{
			Caller->OnRep_ReplicatedMovement();
		}
	}
}

URelatedWorldNetLocCorrectionComponent::URelatedWorldNetLocCorrectionComponent()
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
	bNeedCorrection = false;
}

void URelatedWorldNetLocCorrectionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URelatedWorldNetLocCorrectionComponent, bNeedCorrection);
	DOREPLIFETIME(URelatedWorldNetLocCorrectionComponent, ActorOwner);
	DOREPLIFETIME(URelatedWorldNetLocCorrectionComponent, RelatedWorldLocation);
}

void URelatedWorldNetLocCorrectionComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (IsValid(GetOwner()) && !GetOwner()->HasAnyFlags(RF_ClassDefaultObject))
	{
		ActorOwner = GetOwner();
		RelatedWorld = UWorldDirector::GetWorldDirector()->GetRelatedWorldFromActor(ActorOwner);

		if (RelatedWorld)
		{
			bNeedCorrection = true;
		}
		else
		{
			bNeedCorrection = false;
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	if (ActorOwner && !ActorOwner->IsPendingKill() && GetOwnerRole() == ROLE_Authority)
	{
		if (RelatedWorld)
		{
			FIntVector WorldLocation = RelatedWorld->GetWorldTranslation();
			
			if (WorldLocation != RelatedWorldLocation)
			{
				NotifyWorldLocationChanged(WorldLocation);
			}
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::OnRep_ReplicatedMovement()
{
	if (!ActorOwner || ActorOwner->IsPendingKill() || !ActorOwner->IsReplicatingMovement())
		return;

	if (!bNeedCorrection)
	{
		ActorOwner->OnRep_ReplicatedMovement();
	}

	const FRepMovement& LocalRepMovement = ActorOwner->GetReplicatedMovement();
	USceneComponent* RootComponent = ActorOwner->GetRootComponent();

	if (RootComponent)
	{
		//Todo Protected function. Need to override for properly work
		/*if (ActorReplication::SavedbRepPhysics != LocalRepMovement.bRepPhysics)
		{
			// Turn on/off physics sim to match server.
			SyncReplicatedPhysicsSimulation();
		}*/

		if (LocalRepMovement.bRepPhysics)
		{
			// Sync physics state
			checkSlow(RootComponent->IsSimulatingPhysics());
			// If we are welded we just want the parent's update to move us.
			UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
			if (!RootPrimComp || !RootPrimComp->IsWelded())
			{
				//Todo: reImplment in component
				ActorOwner->PostNetReceivePhysicState();
			}
		}
		else
		{
			// Attachment trumps global position updates, see GatherCurrentMovement().
			if (!RootComponent->GetAttachParent())
			{
				if (GetOwnerRole() == ROLE_SimulatedProxy)
				{
#if ENABLE_NAN_DIAGNOSTIC
					if (LocalRepMovement.Location.ContainsNaN())
					{
						logOrEnsureNanError(TEXT("AActor::OnRep_ReplicatedMovement found NaN in ReplicatedMovement.Location"));
					}
					if (LocalRepMovement.Rotation.ContainsNaN())
					{
						logOrEnsureNanError(TEXT("AActor::OnRep_ReplicatedMovement found NaN in ReplicatedMovement.Rotation"));
					}
#endif

					ActorOwner->PostNetReceiveVelocity(LocalRepMovement.LinearVelocity);
					PostNetReceiveLocationAndRotation();
				}
			}
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::PostNetReceiveLocationAndRotation()
{
	const FRepMovement& LocalRepMovement = ActorOwner->GetReplicatedMovement();

	FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(LocalRepMovement.Location, ActorOwner);
	NewLocation.X += RelatedWorldLocation.X;
	NewLocation.Y += RelatedWorldLocation.Y;
	NewLocation.Z += RelatedWorldLocation.Z;

	USceneComponent* RootComponent = ActorOwner->GetRootComponent();

	if (RootComponent && RootComponent->IsRegistered() && (NewLocation != ActorOwner->GetActorLocation() || LocalRepMovement.Rotation != ActorOwner->GetActorRotation()))
	{
		ActorOwner->SetActorLocationAndRotation(NewLocation, LocalRepMovement.Rotation, /*bSweep=*/ false);
	}
}

void URelatedWorldNetLocCorrectionComponent::NotifyWorldLocationChanged(const FIntVector& NewWorldLocation)
{
	RelatedWorldLocation = NewWorldLocation;
}
