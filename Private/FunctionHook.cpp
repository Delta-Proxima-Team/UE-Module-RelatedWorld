// Copyright Delta-Proxima Team (c) 2007-2020

#include "FunctionHook.h"
#include "Components/RelatedLocationComponent.h"
#include "RelatedWorld.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#if DO_CHECK && !UE_BUILD_SHIPPING // Disable even if checks in shipping are enabled.
#define devCode( Code )		checkCode( Code )
#else
#define devCode(...)
#endif

IMPLEMENT_UFUNCTION_HOOK(AActor, OnRep_ReplicatedMovement)
{
	P_FINISH;
	
	P_NATIVE_BEGIN;
	{
		HOOK_COMPONENT(URelatedLocationComponent);
		FIntVector STORE_OriginLocation;

		if (p_comp != nullptr)
		{
			STORE_OriginLocation = p_this->GetWorld()->OriginLocation;
			p_this->GetWorld()->OriginLocation = FIntVector::ZeroValue;
			p_comp->AActor_OnRep_ReplicatedMovement(STORE_OriginLocation);
			p_this->GetWorld()->OriginLocation = STORE_OriginLocation;
		}
		else
		{
			p_this->OnRep_ReplicatedMovement();
		}
	}
	P_NATIVE_END;

}
END_UFUNCTION_HOOK

void URelatedLocationComponent::AActor_OnRep_ReplicatedMovement(const FIntVector& WorldOrigin)
{
	FRepMovement* LocalMovement = (FRepMovement*)&GetOwner()->GetReplicatedMovement();

	FIntVector Rebase = WorldTranslation - WorldOrigin;
	LocalMovement->Location = URelatedWorldUtils::CONVERT_RelToWorld(Rebase, LocalMovement->Location);
	GetOwner()->OnRep_ReplicatedMovement();
}


IMPLEMENT_UFUNCTION_HOOK(ACharacter, ClientAdjustPosition)
{
	P_GET_PROPERTY(FFloatProperty, TimeStamp);
	P_GET_STRUCT(FVector, NewLoc);
	P_GET_STRUCT(FVector, NewVel);
	P_GET_OBJECT(UPrimitiveComponent, NewBase);
	P_GET_PROPERTY(FNameProperty, NewBaseBoneName);
	P_GET_PROPERTY(FBoolProperty, bHasBase);
	P_GET_PROPERTY(FBoolProperty, bBaseRelativePosition);
	P_GET_PROPERTY(FByteProperty, ServerMovementMode);
	P_FINISH;

	P_NATIVE_BEGIN;
	{
		HOOK_COMPONENT(URelatedLocationComponent);

		FIntVector STORE_OriginLocation;

		if (p_comp != nullptr)
		{
			STORE_OriginLocation = p_this->GetWorld()->OriginLocation;
			p_this->GetWorld()->OriginLocation = FIntVector::ZeroValue;
			p_comp->ACharacter_ClientAdjustPosition(STORE_OriginLocation, TimeStamp, NewLoc, NewVel, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);
			p_this->GetWorld()->OriginLocation = STORE_OriginLocation;
		}
		else
		{
			p_this->ClientAdjustPosition_Implementation(TimeStamp, NewLoc, NewVel, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);
		}
	}
	P_NATIVE_END;

}
END_UFUNCTION_HOOK

void URelatedLocationComponent::ACharacter_ClientAdjustPosition(
	const FIntVector& WorldOrigin,
	float TimeStamp,
	FVector NewLoc,
	FVector NewVel,
	UPrimitiveComponent* NewBase,
	FName NewBaseBoneName,
	bool bHasBase,
	bool bBaseRelativePosition,
	uint8 ServerMovementMode)
{
	FIntVector Rebase = WorldTranslation - WorldOrigin;
	FVector Loc = URelatedWorldUtils::CONVERT_RelToWorld(Rebase, NewLoc);

	CastChecked<ACharacter>(GetOwner())->ClientAdjustPosition_Implementation(TimeStamp, Loc, NewVel, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);
}

#if ENGINE_MINOR_VERSION >= 26
IMPLEMENT_UFUNCTION_HOOK(ACharacter, ClientMoveResponsePacked)
{
	P_GET_STRUCT(FCharacterMoveResponsePackedBits, PackedBits);
	P_FINISH;

	P_NATIVE_BEGIN;
	{
		HOOK_COMPONENT(URelatedLocationComponent);

		FIntVector STORE_OriginLocation;

		if (p_comp != nullptr)
		{
			STORE_OriginLocation = p_this->GetWorld()->OriginLocation;
			p_this->GetWorld()->OriginLocation = FIntVector::ZeroValue;
			p_comp->ACharacter_ClientMoveResponsePacked(STORE_OriginLocation, PackedBits);
			p_this->GetWorld()->OriginLocation = STORE_OriginLocation;
		}
		else
		{
			p_this->ClientMoveResponsePacked_Implementation(PackedBits);
		}
	}
	P_NATIVE_END;
}
END_UFUNCTION_HOOK

void URelatedLocationComponent::ACharacter_ClientMoveResponsePacked(const FIntVector& WorldOrigin, const FCharacterMoveResponsePackedBits& PackedBits)
{
	UCharacterMovementComponent* MovementComp = Cast<UCharacterMovementComponent>(GetTypedOuter<ACharacter>()->GetMovementComponent());
	if (MovementComp == nullptr || !MovementComp->HasValidData() || !MovementComp->IsActive())
	{
		return;
	}

	const int32 NumBits = PackedBits.DataBits.Num();
	const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("p.NetPackedMovementMaxBits"));
	int32 NetUsePackedMovementRPCs = CVar->GetInt();

	if (!ensure(NumBits <= NetUsePackedMovementRPCs))
	{
		// Protect against bad data that could cause client to allocate way too much memory.
		devCode(UE_LOG(LogNetPlayerMovement, Error, TEXT("MoveResponsePacked_ClientReceive: NumBits (%d) exceeds allowable limit!"), NumBits));
		return;
	}

	// Reuse bit reader to avoid allocating memory each time.
	MoveResponseBitReader.SetData((uint8*)PackedBits.DataBits.GetData(), NumBits);
	MoveResponseBitReader.PackageMap = PackedBits.GetPackageMap();

	// Deserialize bits to response data struct.
	// We had to wait until now and use the temp bit stream because the RPC doesn't know about the virtual overrides on the possibly custom struct that is our data container.
	if (!MoveResponseDataContainer.Serialize(*MovementComp, MoveResponseBitReader, MoveResponseBitReader.PackageMap) || MoveResponseBitReader.IsError())
	{
		devCode(UE_LOG(LogNetPlayerMovement, Error, TEXT("MoveResponsePacked_ClientReceive: Failed to serialize response data!")));
		return;
	}

	FIntVector Rebase = WorldTranslation - WorldOrigin;
	MoveResponseDataContainer.ClientAdjustment.NewLoc = URelatedWorldUtils::CONVERT_RelToWorld(Rebase, MoveResponseDataContainer.ClientAdjustment.NewLoc);

	MovementComp->ClientHandleMoveResponse(MoveResponseDataContainer);
}

#endif

IMPLEMENT_UFUNCTION_HOOK(APlayerController, ServerUpdateCamera)
{
	P_GET_STRUCT(FVector_NetQuantize, CamLoc);
	P_GET_PROPERTY(FIntProperty, CamPitchAndYaw);
	P_FINISH;

	P_NATIVE_BEGIN;
	{
		HOOK_CONTROLLER_COMPONENT(URelatedLocationComponent);

		if (p_comp != nullptr)
		{
			p_comp->APlayerController_ServerUpdateCamera(CamLoc, CamPitchAndYaw);
		}
		else
		{
			UFunction* f_this = p_this->FindFunction(TEXT("ServerUpdateCamera"));

			if (p_this->GetNetMode() == NM_Client)
			{
				f_this->FunctionFlags &= ~FUNC_Static;
				p_this->ServerUpdateCamera(CamLoc, CamPitchAndYaw);
				f_this->FunctionFlags |= FUNC_Static;
			}
			else
			{
				if (!p_this->ServerUpdateCamera_Validate(CamLoc, CamPitchAndYaw))
				{
					RPC_ValidateFailed(TEXT("Server_Test_Validate"));
					return;
				}

				p_this->ServerUpdateCamera_Implementation(CamLoc, CamPitchAndYaw);
			}
		}

	}
	P_NATIVE_END;

}
END_UFUNCTION_HOOK

void URelatedLocationComponent::APlayerController_ServerUpdateCamera(FVector_NetQuantize CamLoc, int32 CamPitchAndYaw)
{
	APlayerController* PC = Cast<APlayerController>(Cast<ACharacter>(GetOwner())->Controller);

	if (PC == nullptr)
		return;

	FMinimalViewInfo POV = PC->PlayerCameraManager->GetCameraCachePOV();
	
	FIntVector Rebase = WorldTranslation - GetWorld()->OriginLocation;
	FVector CameraLocation = URelatedWorldUtils::CONVERT_WorldToRel(Rebase, POV.Location);
	Server_APlayerController_ServerUpdateCamera(CameraLocation, CamPitchAndYaw);
}

bool URelatedLocationComponent::Server_APlayerController_ServerUpdateCamera_Validate(FVector_NetQuantize CamLoc, int32 CamPitchAndYaw)
{
	return true;
}

void URelatedLocationComponent::Server_APlayerController_ServerUpdateCamera_Implementation(FVector_NetQuantize CamLoc, int32 CamPitchAndYaw)
{
	APawn* PawnOwner = Cast<APawn>(GetOwner());

	if (PawnOwner != nullptr)
	{
		APlayerController* PC = Cast<APlayerController>(PawnOwner->Controller);

		if (PC != nullptr)
		{
			PC->ServerUpdateCamera_Implementation(CamLoc, CamPitchAndYaw);
		}
	}	
}
