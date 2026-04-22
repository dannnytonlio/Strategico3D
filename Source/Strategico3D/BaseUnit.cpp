// copyright 2026 by danny tonlio - all rights reserved
// BaseUnit.cpp - Base class for all playable units (Sniper, Brawler)


#include "BaseUnit.h"
#include "BrawlerUnit.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABaseUnit::ABaseUnit()
{
	PrimaryActorTick.bCanEverTick = false;
	// enabling collision for the actor
	SetActorEnableCollision(true); 
	

	// mesh initialization
	UnitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UnitMesh"));
	RootComponent = UnitMesh;

	UnitMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	UnitMesh->SetCollisionResponseToAllChannels(ECR_Block);
	UnitMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// load a simple cone mesh from the engine's basic shapes
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(
		TEXT("/Engine/BasicShapes/Cone.Cone")
	);

	if (Cone.Succeeded())
	{
		UnitMesh->SetStaticMesh(Cone.Object);
	}

	// load a simple material from the engine's basic materials
	static ConstructorHelpers::FObjectFinder<UMaterial> Mat(
		TEXT("/Game/NewFolder/M_UnitColor.M_UnitColor")
	);

	if (Mat.Succeeded())
	{
		UnitMesh->SetMaterial(0, Mat.Object);
	}

	// scale down the mesh to fit better on the grid
	UnitMesh->SetWorldScale3D(FVector(0.6f));

	//----------------
	// DEFAULT VALUES
	//---------------
	Health = 10;
	AttackRange = 1;
	MinDamage = 1;
	MaxDamage = 2;
	MoveRange = 1;

	
	OwnerPlayer = 0;

	DynamicMaterial = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/// /// BEGINPLAY: INITIALIZE THE DYNAMIC MATERIAL AND APPLY THE COLOR BASED ON THE OWNER and SET HP
////////// ///////////////////////////////////////////////////////////////////////////////////////////
void ABaseUnit::BeginPlay()
{
	Super::BeginPlay(); 

	
	DynamicMaterial = UMaterialInstanceDynamic::Create(
		UnitMesh->GetMaterial(0),
		this
	);

	if (DynamicMaterial)
	{
		UnitMesh->SetMaterial(0, DynamicMaterial);

		/* Appliquer la couleur */
		FLinearColor FinalColor = UnitColor;

if (IsA(ABrawlerUnit::StaticClass()))
{
	FinalColor.R = FMath::Clamp(FinalColor.R * 1.8f, 0.f, 1.f);
	FinalColor.G = FMath::Clamp(FinalColor.G * 1.8f, 0.f, 1.f);
	FinalColor.B = FMath::Clamp(FinalColor.B * 1.8f, 0.f, 1.f);
}

DynamicMaterial->SetVectorParameterValue("Color", FinalColor);
	}

	//init HP
	CurrentHP = Health;
	MaxHP = Health;

	// to allow respawn at the same location after death, we store the initial position of the unit
	SpawnLocation = GetActorLocation();
}

/// ///////////////////////////////////////////////////
/// RESET PLAYER ACTIONS AT THE BEGINNING OF THE TURN
/// ///////////////////////////////////////////////
void ABaseUnit::ResetTurn()
{
	bHasMoved = false;
	bHasAttacked = false;
	bTurnCompleted = false;
}

/// ///////////////////////////////////////////////////////////////////////////////////////////////////
/// DEATH AND RESPAWN: when a unit dies, we reset its HP, move it back to its spawn location and reset its actions for the new turn
/// /////////////////////////////////////////////////////////////////////////////////////////////////
void ABaseUnit::Respawn()
{
	// reset HP
	CurrentHP = Health;

	// reset position
	SetActorLocation(SpawnLocation);

	// reset actions
	ResetTurn();
}

//refresh the color of the unit 
void ABaseUnit::UpdateColor()
{
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue("Color", UnitColor);
	}
}

//set the color of the unit based on its owner (player or AI)
void ABaseUnit::SetTeamColor(int Player)
{
	OwnerPlayer = Player;

	if (Player == 0)
	{
		UnitColor = FLinearColor(1.f, 0.f, 1.f); // MAGENTA (PLAYER)
	}
	else
	{
		UnitColor = FLinearColor(0.f, 0.f, 0.f); // BLACK (AI)
	}

	UpdateColor();
}