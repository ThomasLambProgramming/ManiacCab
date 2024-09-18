#pragma once

#include "NiagaraFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "CoreMinimal.h"
#include "InputMappingContext.h"
#include "CarController.generated.h"

UCLASS()
class MANIACCAB_API ACarController : public APawn
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Camera");
	UCameraComponent* FollowCamera;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Physics")
	float SpringRestDistance = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Physics")
	float SpringStrength = 15000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Physics")
	float DampingAmount= 3000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Physics")
	float FloorCheckLimit = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Physics")
	float TireMass = 30;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Movement")
	float CarTopSpeed = 2000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Movement")
	float MaxTorque = 80000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Movement")
	float MaxTurnAngle = 35;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Movement")
	float RotateSpeed = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Movement")
	float WheelCheckHeightOffset = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Air Settings")
	float InAirCorrectionSpeed = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Air Settings")
	float AirLogicEnableDelay = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Air Settings")
	float AirFloorCheckResetDelay = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Air Settings")
	float PerAirTraceAngle = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Air Settings")
	float InAirFloorCheckLimit = 110.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Air Settings")
	float VelocityYawEveningSpeed = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Camera")
	float CameraFOVAtMaxSpeed = 100.0f;
	
	UPROPERTY(EditAnywhere, Category="Floor Checking Settings")
	TEnumAsByte<ECollisionChannel> TraceChannelProperty = ECC_Pawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Curve References")
	UCurveFloat* CarTorqueCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Curve References")
	UCurveFloat* FrontWheelFrictionCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Curve References")
	UCurveFloat* BackWheelFrictionCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Curve References")
	UCurveFloat* FrontWheelDriftFrictionCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Curve References")
	UCurveFloat* BackWheelDriftFrictionCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Asset References")
	UStaticMeshComponent* CarChassis;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Asset References")
	UStaticMeshComponent* FrontLeftWheel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Asset References")
	UStaticMeshComponent* FrontRightWheel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Asset References")
	UStaticMeshComponent* BackLeftWheel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Asset References")
	UStaticMeshComponent* BackRightWheel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Asset References")
	UNiagaraSystem* TireDriftingParticleEffect;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Car Input")
	FVector InputAxis;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input Mapping")
	UInputMappingContext* InputMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input Mapping")
	UInputAction* SteeringInputAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input Mapping")
	UInputAction* ThrottleInputAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input Mapping")
	UInputAction* BreakInputAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input Mapping")
	UInputAction* HandBrakeInputAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input Mapping")
	UInputAction* ResetCarAction;

	
	UPROPERTY(EditAnywhere, Category="Debug Settings")
	bool DrawForces = true;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Debug Settings")
	bool DisableAirCorrection = false;
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category="Debug Settings")
	bool AllowCarInput = true;

	UPROPERTY()
	FTimerHandle InAirTimerHandle;	
	UPROPERTY()
	FTimerHandle AirFloorCheckResetTimerHandle;

	UPROPERTY()
	UNiagaraComponent* BackLeftTireDriftEffect;
	UPROPERTY()
	UNiagaraComponent* BackRightTireDriftEffect;
	
private:
	float OriginalCameraFov = 0;
	float OriginalFloorCheckValue;
	bool IsInAir = false;
	bool HoldingSpace = false;
	bool IsDrifting = false;

public:
	UFUNCTION(BlueprintCallable)
	void EnableCarInput(bool Allow) {AllowCarInput = Allow;}

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	
	void ScaleCarFOVOnSpeed();
	void ProcessAirRotation();
	void UpdateAllWheels();

	bool IndividualWheelUpdate(UStaticMeshComponent* a_WheelToCheck, bool a_IsFrontWheel) const;
	bool WheelGroundCheck(const UStaticMeshComponent* a_WheelToCheck, float& o_DistanceToFloor) const;
	
	void EnableAirLogic();
	void ResetFloorCheckToOriginal();
	
	FVector CalculateInAirRotation() const;
	FVector CalculateSpringForce(const UStaticMeshComponent* a_Wheel, const float a_DistanceToFloor) const;
	FVector CalculateWheelFrictionForce(const UStaticMeshComponent* a_Wheel, const UCurveFloat* a_FrictionCurve) const;
	FVector CalculateAccelerationForce(UStaticMeshComponent* a_Wheel) const;
	
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	ACarController();

	void HandleTurningInput() const;
	void SteeringAction(const FInputActionValue& a_Value);
	void BreakAction(const FInputActionValue& a_Value);
	void ThrottleAction(const FInputActionValue& a_Value);
	void HandBrakeActionPressed(const FInputActionValue& a_Value);
	void HandBrakeActionReleased(const FInputActionValue& a_Value);
	void ResetCarActionPressed(const FInputActionValue& a_Value);
};
