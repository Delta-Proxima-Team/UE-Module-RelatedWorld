// Copyright Delta-Proxima Team (c) 2007-2020

#include "Net/RelatedWorldNetLocCorrectionComponent.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsReplication.h"
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
	bInitialReplication = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
	bNeedCorrection = false;
}

void URelatedWorldNetLocCorrectionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(URelatedWorldNetLocCorrectionComponent, bInitialReplication, COND_InitialOnly);
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
			bInitialReplication = true;
			bNeedCorrection = true;
		}
		else
		{
			bNeedCorrection = false;
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::OnRep_Initial()
{
	if (ActorOwner && !ActorOwner->IsPendingKill() && bNeedCorrection)
	{
		USceneComponent* RootComponent = ActorOwner->GetRootComponent();

		if (RootComponent)
		{
			FVector Location = RootComponent->GetComponentLocation();
			UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(ActorOwner->GetRootComponent());

			if ((PrimComp && PrimComp->IsSimulatingPhysics()) || Location == FVector::ZeroVector)
			{
				Location.X += RelatedWorldLocation.X;
				Location.Y += RelatedWorldLocation.Y;
				Location.Z += RelatedWorldLocation.Z;

				RootComponent->SetWorldLocation(Location);
			}
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
		if (SavedbRepPhysics != LocalRepMovement.bRepPhysics)
		{
			// Turn on/off physics sim to match server.
			SyncReplicatedPhysicsSimulation();
			SavedbRepPhysics = LocalRepMovement.bRepPhysics;
		}

		if (LocalRepMovement.bRepPhysics)
		{
			// Sync physics state
			checkSlow(RootComponent->IsSimulatingPhysics());
			// If we are welded we just want the parent's update to move us.
			UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
			if (!RootPrimComp || !RootPrimComp->IsWelded())
			{
				PostNetReceivePhysicState();
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

void URelatedWorldNetLocCorrectionComponent::SyncReplicatedPhysicsSimulation()
{
	const FRepMovement& LocalRepMovement = ActorOwner->GetReplicatedMovement();
	USceneComponent* RootComponent = ActorOwner->GetRootComponent();

	if (ActorOwner->IsReplicatingMovement() && RootComponent && (RootComponent->IsSimulatingPhysics() != LocalRepMovement.bRepPhysics))
	{
		UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
		if (RootPrimComp)
		{
			RootPrimComp->SetSimulatePhysics(LocalRepMovement.bRepPhysics);

			if (!LocalRepMovement.bRepPhysics)
			{
				if (UWorld* World = GetWorld())
				{
					if (FPhysScene* PhysScene = World->GetPhysicsScene())
					{
						if (FPhysicsReplication* PhysicsReplication = PhysScene->GetPhysicsReplication())
						{
							PhysicsReplication->RemoveReplicatedTarget(RootPrimComp);
						}
					}
				}
			}
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::PostNetReceivePhysicState()
{
	const FRepMovement& LocalRepMovement = ActorOwner->GetReplicatedMovement();
	UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(ActorOwner->GetRootComponent());
	if (RootPrimComp)
	{
		FRigidBodyState NewState;
		
		FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(LocalRepMovement.Location, ActorOwner);
		NewLocation.X += RelatedWorldLocation.X;
		NewLocation.Y += RelatedWorldLocation.Y;
		NewLocation.Z += RelatedWorldLocation.Z;

		NewState.Position = NewLocation;
		NewState.Quaternion = LocalRepMovement.Rotation.Quaternion();
		NewState.LinVel = LocalRepMovement.LinearVelocity;
		NewState.AngVel = LocalRepMovement.AngularVelocity;
		NewState.Flags = (LocalRepMovement.bSimulatedPhysicSleep ? ERigidBodyFlags::Sleeping : ERigidBodyFlags::None) | ERigidBodyFlags::NeedsUpdate;

		RootPrimComp->SetRigidBodyReplicatedTarget(NewState);
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
