#include "ShooterCharacterMovement.h"

UShooterCharacterMovement::UShooterCharacterMovement(const FObjectInitializer& P) : Super(P)
{
}

void UShooterCharacterMovement::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}
