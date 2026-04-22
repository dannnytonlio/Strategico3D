//copyright 2026 by danny tonlio - all rights reserved
// StrategicoCameraPawn.cpp - Pawn class representing the camera in the game, allowing for an orthographic top-down view of the grid

#include "StrategicoCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "StrategicoCameraPawn.h"

AStrategicoCameraPawn::AStrategicoCameraPawn()
{
    PrimaryActorTick.bCanEverTick = false;
	// Create a root component for the camera pawn
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RootComponent);

	// configure the camera for a top-down orthographic view
    Camera->ProjectionMode = ECameraProjectionMode::Orthographic;

    Camera->OrthoWidth = 2000.f;

   
}

//////////////////////////////////////////////////
/// BEGINPLAY  PUT THE CAMERA IN THE RIGHT PLACE
///////////////////////////////////////////////
void AStrategicoCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	// center the camera above the grid (assuming a 25x25 grid with 100 units per tile)
	float Center = (25 * 100) / 2.f;

	SetActorLocation(FVector(Center, Center, 3000.f));

	// rotate the camera to look straight down
	SetActorRotation(FRotator(-90.f, 0.f, 0.f));
}