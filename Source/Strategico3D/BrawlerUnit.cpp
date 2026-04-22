//copyright 2026 by danny tonlio - all rights reserved
// BrawlerUnit.cpp - Implementation of the Brawler unit, a melee unit with high HP and damage but low range and mobility


#include "BrawlerUnit.h"
#include "UObject/ConstructorHelpers.h"

ABrawlerUnit::ABrawlerUnit()
{
	// set default values for the Brawler unit
	Health = 40;
	MoveRange = 6;
	AttackRange = 1;
	MinDamage = 1;
	MaxDamage = 6;

	////////////////
	// CHANGE MESH
	//////////////
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
	);

	if (Mesh.Succeeded())
	{
		UnitMesh->SetStaticMesh(Mesh.Object);
	}

	// scale down the mesh to fit better on the grid, 
	// and make it taller than the default cone mesh 
	// to differentiate it visually from the Sniper unit
	UnitMesh->SetWorldScale3D(FVector(0.6f, 0.6f, 0.8f));
}