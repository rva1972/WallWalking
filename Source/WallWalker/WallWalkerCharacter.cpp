// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallWalkerCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"

//////////////////////////////////////////////////////////////////////////
// AWallWalkerCharacter
	
AWallWalkerCharacter::AWallWalkerCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UWallWalkerMovementComponent>(ACharacter::CharacterMovementComponentName))
, IsWallWalking(false)
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)

	MovementComponent = Cast<UWallWalkerMovementComponent>(GetMovementComponent());
}

//////////////////////////////////////////////////////////////////////////
// Input

void AWallWalkerCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AWallWalkerCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AWallWalkerCharacter::MoveRight);

	PlayerInputComponent->BindAxis("WallWalk", this, &AWallWalkerCharacter::WallWalk);
	
	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AWallWalkerCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AWallWalkerCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AWallWalkerCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AWallWalkerCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AWallWalkerCharacter::OnResetVR);
}


void AWallWalkerCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AWallWalkerCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AWallWalkerCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AWallWalkerCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AWallWalkerCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AWallWalkerCharacter::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AWallWalkerCharacter::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void AWallWalkerCharacter::WallWalk(float Value)
{
	if ((Controller == nullptr) || ((Value == 0.0f)))
	{
		if (IsWallWalking)
			StopWallWalk();
		return;
	}

	if (!IsWallWalking)
	{
		auto Direction{ GetCapsuleComponent()->GetForwardVector() };

		FHitResult WallHit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		bool IsBlockedByWall = GetWorld()->LineTraceSingleByChannel(WallHit, GetActorLocation(), GetActorLocation() + Direction * 300.f, ECollisionChannel::ECC_WorldStatic, QueryParams);
		if (IsBlockedByWall)
		{
			FHitResult CheckedHit;
			bool IsBlocked = GetWorld()->LineTraceSingleByChannel(CheckedHit, GetActorLocation(), GetActorLocation() - WallHit.ImpactNormal * 300.f, ECollisionChannel::ECC_WorldStatic, QueryParams);
			if (IsBlocked && (CheckedHit.Location - GetActorLocation()).Size() > 50.0f)
				return;

			auto Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Direction, -WallHit.ImpactNormal)));

			SaveRotation = GetActorRotation();
			IsWallWalking = true;
			if (Angle < 30) // Run to Up
			{
				MovementComponent->GravityScale = 0.0f;
				MovementComponent->MovementMode = EMovementMode::MOVE_Falling;
				MovementComponent->Velocity.Z = MovementComponent->MaxWalkSpeed;

				DirectionWallWalking = EDirectType::Up;
				SetActorRotation(FQuat(GetActorRotation()) * FQuat(FRotator(90.f, 0.f, 0.f)));
			}
			else
			{
				DirectionWallWalking = (FVector::DotProduct(WallHit.ImpactNormal - GetActorLocation(), GetCapsuleComponent()->GetRightVector()) < 0) ? EDirectType::Left : EDirectType::Right;
				
				MovementComponent->GravityScale = 0.0f;
				MovementComponent->MovementMode = EMovementMode::MOVE_Falling;
				
				FVector RightVector = GetCapsuleComponent()->GetRightVector();
				auto NewForward = FVector::CrossProduct(RightVector, WallHit.ImpactNormal);
				auto NewRight = FVector::CrossProduct(WallHit.ImpactNormal, NewForward);

				SetActorTransform(FTransform(NewForward, NewRight, WallHit.ImpactNormal, GetActorLocation()));
				SetActorLocation(CheckedHit.Location + CheckedHit.ImpactNormal * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 1.f));
				SetActorScale3D(FVector(1.f, 1.f, 1.f));
				
				FQuat Rotator;
				if (DirectionWallWalking == EDirectType::Left)
					Rotator = FQuat(GetActorRotation()) * FQuat(FRotator(0.f, -90.f, 0.f));
				else 
					Rotator = FQuat(GetActorRotation()) * FQuat(FRotator(0.f, 90.f, 0.f));
				
				SetActorRotation(Rotator);
			}
		};
	}
	else
	{
		if (IsValidWallWalk())
		{
			if (DirectionWallWalking == EDirectType::Up)
			{
				const FRotator Rotation = Controller->GetControlRotation();
				const FRotator YawRotation(0, Rotation.Yaw, 0);
				const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Z);
				AddMovementInput(Direction, Value);
			}
			else
			{
				const auto Direction = DirectionWallWalking == EDirectType::Right ? GetActorForwardVector() : -GetActorForwardVector();
				const auto NewValue = DirectionWallWalking == EDirectType::Right ? Value : -Value;
				AddMovementInput(Direction, NewValue);
			}
		}
	}
}

void AWallWalkerCharacter::StopWallWalk()
{
	IsWallWalking = false;

	MovementComponent->GravityScale = 1.0f;
	MovementComponent->Velocity.Z = 0.f;
	MovementComponent->MovementMode = EMovementMode::MOVE_Walking;

	SetActorRotation(SaveRotation);
	//SetActorRotation(FQuat(GetActorRotation()) * FQuat(FRotator(-90.f, 0.f, 0.f)));
}

bool AWallWalkerCharacter::IsValidWallWalk()
{
	FHitResult WallHit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	bool IsBlocked = GetWorld()->LineTraceSingleByChannel(WallHit, GetActorLocation(),
		GetActorLocation() - GetActorUpVector() * 200.f, ECollisionChannel::ECC_WorldStatic, QueryParams);
	if (!IsBlocked)
	{
		auto Start = GetActorLocation() - GetActorUpVector() * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() +
			GetCapsuleComponent()->GetScaledCapsuleRadius());
		auto Finish = Start + FVector(0.f, 0.f, -50.f);

		IsBlocked = GetWorld()->LineTraceSingleByChannel(WallHit, Start, Finish, ECollisionChannel::ECC_WorldStatic, QueryParams);

		if (IsBlocked)
			SetActorLocation(WallHit.Location + WallHit.ImpactNormal * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 1.f));
					
		StopWallWalk();
		return false;

		DirectionWallWalking = EDirectType::None;
	}

	return true;
}

void AWallWalkerCharacter::Tick(float DeltaTime)
{
	
}

EDirectType AWallWalkerCharacter::CalculateDirect(FVector Normal)
{
	return (FVector::DotProduct(Normal - GetActorLocation(), GetCapsuleComponent()->GetRightVector()) < 0) ? EDirectType::Left : EDirectType::Right;
}
