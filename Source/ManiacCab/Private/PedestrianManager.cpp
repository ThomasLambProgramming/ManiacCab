// Fill out your copyright notice in the Description page of Project Settings.


#include "PedestrianManager.h"

#include "Kismet/GameplayStatics.h"


// Sets default values
APedestrianManager::APedestrianManager()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void APedestrianManager::BeginPlay()
{
	Super::BeginPlay();
	TArray<AActor*> lands;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ALandscape::StaticClass(), lands);
	LandscapeActor = Cast<ALandscape>(lands[0]);

	FLandscapeLayer* splineLayer = LandscapeActor->GetLandscapeSplinesReservedLayer();
}

// Called every frame
void APedestrianManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

