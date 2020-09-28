// Copyright Delta-Proxima Team (c) 2007-2020

#include "FunctionHook.h"
#include "Components/RelatedLocationComponent.h"
#include "RelatedWorld.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

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
	Server_AplayerController_ServerUpdateCamera(CameraLocation, CamPitchAndYaw);
}

bool URelatedLocationComponent::Server_AplayerController_ServerUpdateCamera_Validate(FVector_NetQuantize CamLoc, int32 CamPitchAndYaw)
{
	return true;
}

void URelatedLocationComponent::Server_AplayerController_ServerUpdateCamera_Implementation(FVector_NetQuantize CamLoc, int32 CamPitchAndYaw)
{
	APlayerController* PC = Cast<APlayerController>(Cast<ACharacter>(GetOwner())->Controller);

	if (PC == nullptr)
		return;

	PC->ServerUpdateCamera_Implementation(CamLoc, CamPitchAndYaw);
}
