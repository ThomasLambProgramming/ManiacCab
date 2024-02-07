#include "CarController.h"

#include "EnhancedInput/Public/EnhancedInputComponent.h"
#include "Kismet/KismetMathLibrary.h"

ACarController::ACarController()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CarChassis);
}

void ACarController::BeginPlay()
{
	Super::BeginPlay();
	CarChassis->SetSimulatePhysics(true);
	FollowCamera = FindComponentByClass<UCameraComponent>();
	OriginalFloorCheckValue = FloorCheckLimit;
	OriginalCameraFov = 90;
}

void ACarController::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateAllWheels();
	ProcessAirRotation();
	ScaleCarFOVOnSpeed();
	HandleTurningInput();
}

void ACarController::ScaleCarFOVOnSpeed()
{
	float powerCurve = FMath::Clamp(CarChassis->GetPhysicsLinearVelocity().Length() / CarTopSpeed, 0, 1.0f);
	powerCurve += 0.3f;
	FMath::Clamp(powerCurve, 0, 1);
	float goalFov = FMath::Lerp(OriginalCameraFov, CameraFOVAtMaxSpeed, powerCurve);
	FollowCamera->FieldOfView = FMath::Lerp(FollowCamera->FieldOfView, goalFov, GetWorld()->GetDeltaSeconds());
}

void ACarController::ProcessAirRotation()
{
	if (IsInAir && DEBUG_DisableAirCorrection == false)
	{
		FVector goalUp = CalculateInAirRotation();
		goalUp = goalUp.RotateAngleAxis(90, CarChassis->GetRightVector());

		FRotator rotation = UKismetMathLibrary::FindLookAtRotation(CarChassis->GetComponentLocation(), CarChassis->GetComponentLocation() + goalUp.GetSafeNormal());
		rotation.Roll = goalUp.RotateAngleAxis(90, CarChassis->GetForwardVector()).Rotation().Pitch;;

		if (FMath::Abs(rotation.Roll - CarChassis->GetComponentRotation().Roll) < 1.0f && FMath::Abs(rotation.Pitch - CarChassis->GetComponentRotation().Pitch) < 1.0f)
		{
			rotation.Roll = 0;
			rotation.Pitch = 0;
		}
		
		FVector carVel = CarChassis->GetPhysicsLinearVelocity();
		carVel.Z = 0;
		
		if (carVel.SquaredLength() > 20)
			rotation.Yaw = FMath::Lerp(CarChassis->GetComponentRotation().Yaw, CarChassis->GetPhysicsLinearVelocity().GetSafeNormal().Rotation().Yaw, GetWorld()->DeltaTimeSeconds * VelocityYawEveningSpeed);
		else
			rotation.Yaw = CarChassis->GetComponentRotation().Yaw;

		rotation = UKismetMathLibrary::RInterpTo(CarChassis->GetComponentRotation(), rotation, GetWorld()->DeltaTimeSeconds, InAirCorrectionSpeed);
		CarChassis->SetWorldRotation(rotation);
	}
}

void ACarController::UpdateAllWheels()
{
	bool FrontLeftWheelOnGround = IndividualWheelUpdate(FrontLeftWheel, true);
	bool FrontRightWheelOnGround = IndividualWheelUpdate(FrontRightWheel, true);
	bool BackLeftWheelOnGround = IndividualWheelUpdate(BackLeftWheel, false);
	bool BackRightWheelOnGround = IndividualWheelUpdate(BackRightWheel, false);

	if (FrontLeftWheelOnGround == false && FrontRightWheelOnGround == false &&
		BackLeftWheelOnGround == false && BackRightWheelOnGround == false)
	{
		if (!GetWorld()->GetTimerManager().TimerExists(InAirTimerHandle))
		{
			GetWorld()->GetTimerManager().SetTimer(InAirTimerHandle, this, &ACarController::EnableAirLogic, AirLogicEnableDelay, false);
		}
		if (GetWorld()->GetTimerManager().TimerExists(AirFloorCheckResetTimerHandle))
			GetWorld()->GetTimerManager().ClearTimer(AirFloorCheckResetTimerHandle);
			
	}
	else 
	{
		GetWorld()->GetTimerManager().ClearTimer(InAirTimerHandle);
		
		if (!GetWorld()->GetTimerManager().TimerExists(AirFloorCheckResetTimerHandle))
			GetWorld()->GetTimerManager().SetTimer(AirFloorCheckResetTimerHandle, this, &ACarController::ResetFloorCheckToOriginal, AirFloorCheckResetDelay, false);

		IsInAir = false;
		FloorCheckLimit = OriginalFloorCheckValue;
		if (IsDrifting && BackLeftTireDriftEffect != nullptr)
		{
			BackLeftTireDriftEffect->Activate();
			BackRightTireDriftEffect->Activate();
		}
	}
}

bool ACarController::IndividualWheelUpdate(UStaticMeshComponent* a_WheelToCheck, bool a_IsFrontWheel) const
{
	float rayCastDistance = 0.0f;
	if (WheelGroundCheck(a_WheelToCheck, rayCastDistance))
	{
		const FVector springForce = CalculateSpringForce(a_WheelToCheck, rayCastDistance);
		FVector frictionForce;

		if (a_IsFrontWheel)
		{
			if (IsDrifting)
				frictionForce = CalculateWheelFrictionForce(a_WheelToCheck, FrontWheelDriftFrictionCurve);
			else
				frictionForce = CalculateWheelFrictionForce(a_WheelToCheck, FrontWheelFrictionCurve);
		}
		else
		{
			if (IsDrifting)	
				frictionForce = CalculateWheelFrictionForce(a_WheelToCheck, BackWheelDriftFrictionCurve);
			else
				frictionForce = CalculateWheelFrictionForce(a_WheelToCheck, BackWheelFrictionCurve);
		}

		const FVector accelerationForce = CalculateAccelerationForce(a_WheelToCheck);

		CarChassis->AddForceAtLocation(springForce * GetWorld()->DeltaTimeSeconds * 100, a_WheelToCheck->GetComponentLocation());
		CarChassis->AddForceAtLocation(frictionForce * GetWorld()->DeltaTimeSeconds * 100, a_WheelToCheck->GetComponentLocation());

		if (AllowCarInput)
			CarChassis->AddForceAtLocation(accelerationForce * GetWorld()->DeltaTimeSeconds * 100, a_WheelToCheck->GetComponentLocation());

		return true;
	}
	return false;
}

bool ACarController::WheelGroundCheck(const UStaticMeshComponent* a_WheelToCheck, float& o_DistanceToFloor) const
{
	FTransform wheelTransform = a_WheelToCheck->GetComponentTransform();
	FVector traceStart = wheelTransform.GetLocation() + FVector(0,0,WheelCheckHeightOffset);
	FVector traceEnd = wheelTransform.GetLocation() + FVector(0,0,WheelCheckHeightOffset) + (-CarChassis->GetUpVector() * (FloorCheckLimit + WheelCheckHeightOffset));
	FHitResult hitResult;

	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(hitResult, traceStart, traceEnd, TraceChannelProperty, queryParams))
	{
		o_DistanceToFloor = (hitResult.Distance - WheelCheckHeightOffset);
		return true;
	}
	
	o_DistanceToFloor = -1.0f;	
	return false;
}

void ACarController::EnableAirLogic()
{
	IsInAir = true;
	FloorCheckLimit = InAirFloorCheckLimit;
	if (BackRightTireDriftEffect != nullptr)
	{
		BackRightTireDriftEffect->Deactivate();
		BackLeftTireDriftEffect->Deactivate();
	}
	EnableCarInput(true);
}

void ACarController::ResetFloorCheckToOriginal()
{
	FloorCheckLimit = OriginalFloorCheckValue;
}

FVector ACarController::CalculateInAirRotation() const
{
	FVector averageNormal = FVector::Zero();
	int raysHit = 0;
	FHitResult rayCastHit;
	FVector TraceDirection =  CarChassis->GetPhysicsLinearVelocity();
	//Remove height so it doesnt factor into direction.
	TraceDirection.Z = 0;
	TraceDirection.Normalize();
	const FVector TraceStartLocation = CarChassis->GetComponentLocation() + CarChassis->GetForwardVector() * 10;
	FVector velocityRightVector = FVector::CrossProduct(TraceDirection, FVector(0,0,1));
	velocityRightVector.Normalize();
	//Flip its direction so the rotation angle is a positive degree.
	velocityRightVector = -velocityRightVector;
	TraceDirection = TraceDirection.RotateAngleAxis(10, velocityRightVector);

	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(this);

	FVector averageHitLocation = FVector::Zero();
	for (int i = 0; i < 5; i++)
	{
		if (GetWorld()->LineTraceSingleByChannel(rayCastHit, TraceStartLocation, TraceStartLocation + TraceDirection * 5000, TraceChannelProperty, queryParams))
		{
			if (rayCastHit.Normal.Dot(FVector::UpVector) > 0.2f)
			{
				averageNormal += rayCastHit.Normal;
				averageHitLocation += rayCastHit.Location;
				raysHit++;
			}
		}
		TraceDirection = TraceDirection.RotateAngleAxis(PerAirTraceAngle, velocityRightVector);
	}

	averageHitLocation /= raysHit;
	averageNormal.Normalize();
	return averageNormal;
}

FVector ACarController::CalculateSpringForce(UStaticMeshComponent* a_Wheel, const float a_DistanceToFloor) const
{
	FVector springDirection = CarChassis->GetUpVector();
	FVector tireWorldVel = CarChassis->GetPhysicsLinearVelocityAtPoint(a_Wheel->GetComponentLocation());
	float offset = SpringRestDistance - a_DistanceToFloor;
	float vel = FVector::DotProduct(springDirection, tireWorldVel);
	float force = (offset * SpringStrength) - (vel * DampingAmount);
	
	FVector forceToApply = force * springDirection;
	forceToApply.X = 0;
	forceToApply.Y = 0;
	return forceToApply;
}

FVector ACarController::CalculateWheelFrictionForce(const UStaticMeshComponent* a_Wheel, const UCurveFloat* a_FrictionCurve) const
{
	FVector steeringDir;
	FVector tireWorldVel = CarChassis->GetPhysicsLinearVelocityAtPoint(a_Wheel->GetComponentLocation());

	float dotRight = FVector::DotProduct(tireWorldVel, a_Wheel->GetRightVector());
	float dotLeft = FVector::DotProduct(tireWorldVel, -a_Wheel->GetRightVector());

	if (dotRight > dotLeft)
		steeringDir = a_Wheel->GetRightVector();
	else
		steeringDir = -a_Wheel->GetRightVector();
	
	float steeringVel = FVector::DotProduct(steeringDir, tireWorldVel);

	float desiredVelChange = -steeringVel * a_FrictionCurve->GetFloatValue(FVector::DotProduct(steeringDir.GetSafeNormal(), tireWorldVel.GetSafeNormal()));
	float desiredAcceleration = desiredVelChange / GetWorld()->DeltaTimeSeconds;
	return steeringDir * TireMass * desiredAcceleration;
}

FVector ACarController::CalculateAccelerationForce(UStaticMeshComponent* a_Wheel) const
{
	if (InputAxis.SquaredLength() == 0)
	{
		FVector currentVel = CarChassis->GetPhysicsLinearVelocity();
		currentVel.Z = 0;
		currentVel *= 0.98f;
		if (currentVel.SquaredLength() < 10)
		{
			currentVel.X = 0;
			currentVel.Y = 0;
		}
		CarChassis->SetPhysicsLinearVelocity(currentVel);
	}

	float powerCurve = FMath::Clamp(FVector::DotProduct(CarChassis->GetPhysicsLinearVelocity(),a_Wheel->GetForwardVector())/ CarTopSpeed, 0, 1);

	powerCurve = CarTorqueCurve->GetFloatValue(powerCurve);
	powerCurve = powerCurve * MaxTorque;
	powerCurve = InputAxis.Y * powerCurve;

	return a_Wheel->GetForwardVector() * powerCurve;
}

void ACarController::HandleTurningInput() const 
{
	if (AllowCarInput == false)
		return;
	
	FVector rotateVector = CarChassis->GetForwardVector().RotateAngleAxis(InputAxis.X * MaxTurnAngle, CarChassis->GetRelativeTransform().GetUnitAxis(EAxis::Z));
	rotateVector.Normalize();

	FRotator GoalRotation = rotateVector.Rotation();
	GoalRotation.Pitch = CarChassis->GetComponentRotation().Pitch;
	GoalRotation.Roll = CarChassis->GetComponentRotation().Roll;
	
	FRotator rotation = UKismetMathLibrary::RInterpTo(FrontLeftWheel->GetComponentRotation(), GoalRotation , GetWorld()->DeltaTimeSeconds, RotateSpeed);
	FrontLeftWheel->SetWorldRotation(rotation);
	FrontRightWheel->SetWorldRotation(rotation);
}

void ACarController::SteeringAction(const FInputActionValue& a_Value)
{
	if (Controller == nullptr)
		return;
	InputAxis.X = a_Value.Get<float>();
	if (FMath::Abs(InputAxis.X) < 0.1f)
		InputAxis.X = 0;
}

void ACarController::BreakAction(const FInputActionValue& a_Value)
{
	if (Controller == nullptr)
		return;
	InputAxis.Y = -a_Value.Get<float>();
}

void ACarController::ThrottleAction(const FInputActionValue& a_Value)
{
	if (Controller == nullptr)
		return;
	InputAxis.Y = a_Value.Get<float>();
}

void ACarController::HandBrakeActionPressed(const FInputActionValue& a_Value)
{
	IsDrifting = true;

	if (BackLeftTireDriftEffect == nullptr)
	{
		BackLeftTireDriftEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(TireDriftingParticleEffect, BackLeftWheel, NAME_None, FVector(-10,0,-20),FRotator::ZeroRotator, EAttachLocation::Type::SnapToTarget, false);
		BackRightTireDriftEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(TireDriftingParticleEffect, BackRightWheel, NAME_None, FVector::Zero(), FRotator::ZeroRotator, EAttachLocation::Type::SnapToTarget, false);

		if (IsInAir)
		{
			BackLeftTireDriftEffect->Deactivate();
			BackRightTireDriftEffect->Deactivate();
		}
	}
	else if (IsInAir == false)
	{
		BackLeftTireDriftEffect->Activate();
		BackRightTireDriftEffect->Activate();
	}
}

void ACarController::HandBrakeActionReleased(const FInputActionValue& a_Value)
{
	IsDrifting = false;
	holdingSpace = false;

	if (BackRightTireDriftEffect != nullptr)
	{
		BackRightTireDriftEffect->Deactivate();
		BackLeftTireDriftEffect->Deactivate();
	}
}

void ACarController::ResetCarActionPressed(const FInputActionValue& a_Value)
{
	CarChassis->SetPhysicsLinearVelocity(FVector(0,0,0));
	CarChassis->SetPhysicsAngularVelocityInDegrees(FVector(0,0,0));
	CarChassis->AddLocalOffset(FVector(0,0,200));
	FRotator rotation = CarChassis->GetComponentRotation();
	rotation.Pitch = 0;
	rotation.Roll = 0;
	CarChassis->SetWorldRotation(rotation);
}

void ACarController::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	EnhancedInputComponent->BindAction(SteeringInputAction, ETriggerEvent::Triggered, this, &ACarController::SteeringAction);
	EnhancedInputComponent->BindAction(SteeringInputAction, ETriggerEvent::Canceled, this, &ACarController::SteeringAction);
	EnhancedInputComponent->BindAction(SteeringInputAction, ETriggerEvent::Ongoing, this, &ACarController::SteeringAction);
	EnhancedInputComponent->BindAction(SteeringInputAction, ETriggerEvent::Started, this, &ACarController::SteeringAction);
	EnhancedInputComponent->BindAction(SteeringInputAction, ETriggerEvent::Completed, this, &ACarController::SteeringAction);
	
	EnhancedInputComponent->BindAction(ThrottleInputAction, ETriggerEvent::Triggered, this, &ACarController::ThrottleAction);
	EnhancedInputComponent->BindAction(ThrottleInputAction, ETriggerEvent::Canceled, this, &ACarController::ThrottleAction);
	EnhancedInputComponent->BindAction(ThrottleInputAction, ETriggerEvent::Ongoing, this, &ACarController::ThrottleAction);
	EnhancedInputComponent->BindAction(ThrottleInputAction, ETriggerEvent::Started, this, &ACarController::ThrottleAction);
	EnhancedInputComponent->BindAction(ThrottleInputAction, ETriggerEvent::Completed, this, &ACarController::ThrottleAction);
	
	EnhancedInputComponent->BindAction(BreakInputAction, ETriggerEvent::Triggered, this, &ACarController::BreakAction);
	EnhancedInputComponent->BindAction(BreakInputAction, ETriggerEvent::Canceled, this, &ACarController::BreakAction);
	EnhancedInputComponent->BindAction(BreakInputAction, ETriggerEvent::Ongoing, this, &ACarController::BreakAction);
	EnhancedInputComponent->BindAction(BreakInputAction, ETriggerEvent::Started, this, &ACarController::BreakAction);
	EnhancedInputComponent->BindAction(BreakInputAction, ETriggerEvent::Completed, this, &ACarController::BreakAction);
	
	EnhancedInputComponent->BindAction(HandBrakeInputAction, ETriggerEvent::Triggered, this, &ACarController::HandBrakeActionPressed);
	EnhancedInputComponent->BindAction(HandBrakeInputAction, ETriggerEvent::Ongoing, this, &ACarController::HandBrakeActionPressed);
	EnhancedInputComponent->BindAction(HandBrakeInputAction, ETriggerEvent::Started, this, &ACarController::HandBrakeActionPressed);
	
	EnhancedInputComponent->BindAction(HandBrakeInputAction, ETriggerEvent::Canceled, this, &ACarController::HandBrakeActionReleased);
	EnhancedInputComponent->BindAction(HandBrakeInputAction, ETriggerEvent::Completed, this, &ACarController::HandBrakeActionReleased);

	EnhancedInputComponent->BindAction(ResetCarAction, ETriggerEvent::Completed, this, &ACarController::ResetCarActionPressed);
}
