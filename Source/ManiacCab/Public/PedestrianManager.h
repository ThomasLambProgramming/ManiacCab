// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Landscape.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PedestrianManager.generated.h"

UCLASS()
class MANIACCAB_API APedestrianManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APedestrianManager();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	ALandscape* LandscapeActor;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
