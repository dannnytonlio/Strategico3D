// copyright 2026 by danny tonlio - all rights reserved
// GridManager.cpp
// Core gameplay manager:
// - Grid generation (Perlin noise)
// - Unit placement
// - Turn system
// - Tower control system
// - Combat system
// - AI logic



#include "GridManager.h"
#include "BaseUnit.h"
#include "SniperUnit.h"
#include "BrawlerUnit.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "WBP_StartMenu.h"
#include "WBP_GameHUD.h"
#include "Components/TextBlock.h"
#include "StrategicoGameMode.h"
#include "StrategicoCameraPawn.h" 


/////////////////////////////////////////////////////////////
/// CONSTRUCTOR: INITILIZATION OF COMPONENTS AND DEFAULT VALUES
/////////////////////////////////////////////////////////////////
AGridManager::AGridManager()
{
	PrimaryActorTick.bCanEverTick = true;


	GridMeshVisual = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GridMeshVisual"));
	RootComponent = GridMeshVisual;

	//to store custom data (ex : base color) for each instance
	GridMeshVisual->NumCustomDataFloats = 3;
	//to avoid issues with navigation system trying to use the instanced mesh (causing warnings in the log)
	GridMeshVisual->SetCanEverAffectNavigation(false);


	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// use a simple cube mesh from the engine's basic shapes for the grid cells
	if (Cube.Succeeded())
	{
		GridMeshVisual->SetStaticMesh(Cube.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial"));

	// use a simple vertex color material for the grid cells (allows us to change color per instance)
	if (Mat.Succeeded())
	{
		GridMeshVisual->SetMaterial(0, Mat.Object);
	}
}

/*///////////////////////////////////////////////////////////////////
/////////BEGINPLAY ; SET THE START MENU WIDGET AND SHOW MOUSE CURSOR
*////////////////////////////////////////////////////////////////////
void AGridManager::BeginPlay()
{
    Super::BeginPlay();

    if (StartMenuClass)
    {
		// important create the widget instance here and not in StartGame()
		//to ensure the reference to GridManager is set before the widget is added to the viewport

		StartMenuInstance = CreateWidget<UWBP_StartMenu>(GetWorld(), StartMenuClass);

		if (StartMenuInstance)
		{     // set the reference to this GridManager in the start menu widget
			StartMenuInstance->GridManagerRef2 = this; 

			// add the widget to the viewport
			StartMenuInstance->AddToViewport();

			UE_LOG(LogTemp, Warning, TEXT("GRID REF SET SUCCESS %p"), this);

			APlayerController* PC = GetWorld()->GetFirstPlayerController();

			if (PC)
			{
				PC->bShowMouseCursor = true;

				FInputModeUIOnly InputMode;
				PC->SetInputMode(InputMode);
			}
		}
    }
    else
    {

    }
}

/*//////////////////////////////////////////////////////////////
TICK CALLS EVERY FRAME AND MANAGES THE MAIN GAME LOOP:
tourn system, phase management, player input, AI turns...
*///////////////////////////////////////////////////////////////
void AGridManager::Tick(float DeltaTime)
{     
	if (!bGameStarted)
	{
		return;
	}

	UpdateUI();
	Super::Tick(DeltaTime);

	//////////////////////////
	//  PHASE PLACEMENT (IA)
	//////////////////////////
	if (CurrentPhase == EGamePhase::Placement && CurrentPlayer == 1)
	{
		AIPlaceUnit();
		return;
	}

	// =========================
	// PHASE GAMEPLAY (IA)
	// =========================
	if (CurrentPhase == EGamePhase::Gameplay && CurrentPlayer == 1)
	{   
		if (!bIsAIPlaying)
		{
			bIsAIPlaying = true;
			PlayAITurn();
		}
		return;
	}

	///////////////////////
	// HUMAN PLAYER INPUT
	//////////////////////

	if (GetWorld()->GetFirstPlayerController()->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		HandleMouseClick();
	}
	
	UpdateUI();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
/// TO MANAGE THE CLICK ON THE GRID CELLS, INCLUDING SELECTION, DESELECTION, PLACEMENT, MOVEMENT AND ATTACK
////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AGridManager::HandleMouseClick()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	// detect the clicked cell using a line trace from the mouse cursor
	FHitResult Hit;
	PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit);
	if (!Hit.bBlockingHit) return;

	FVector LocalPosition = Hit.Location - GetActorLocation();
	float HalfTile = TileSpacing * 0.5f;

	int32 LogicalY = FMath::FloorToInt((LocalPosition.X + HalfTile) / TileSpacing);
	int32 LogicalX = FMath::FloorToInt((LocalPosition.Y + HalfTile) / TileSpacing);

	LogicalX = FMath::Clamp(LogicalX, 0, GridSize - 1);
	LogicalY = FMath::Clamp(LogicalY, 0, GridSize - 1);
	// convert hit location to grid coordinates
	FIntPoint GridPos(LogicalX, LogicalY);

	///--------------
	// PHASE PLACEMENT
	//----------------
	if (CurrentPhase == EGamePhase::Placement)
	{
		TryPlaceUnit(GridPos);
		return;
	}

	// current player safety check (should never happen since the UI should be blocked for the non-active player, but just in case)
	if (CurrentPlayer != 0)
	{

		SelectedUnit = nullptr;
		SelectedUnitGridPos = FIntPoint(-1, -1);
		return;
	}
	///-------------------------------
	//  DETECTS IF AN UNIT IS CLICKED
	//------------------------------- 
	ABaseUnit* ClickedUnit = Cast<ABaseUnit>(Hit.GetActor());

	// the player clicked on an unit (either friendly or enemy)
	if (ClickedUnit)
	{
		// priority to attack if the clicked unit is an enemy and there is a selected unit that can attack
		if (ClickedUnit->OwnerPlayer != CurrentPlayer)
		{
			if (SelectedUnit && TryAttack(GridPos))
				return;

			return;
		}

		// deselects if the same unit is clicked again
		if (SelectedUnit == ClickedUnit)
		{
			if (GridMap.Contains(SelectedUnitGridPos))
			{
				FTileData& OldTile = GridMap[SelectedUnitGridPos];
				SafeSetTileColor(OldTile,
					OldTile.BaseColor.R,
					OldTile.BaseColor.G,
					OldTile.BaseColor.B);
			}

			HideMovementRange();

			SelectedUnit = nullptr;
			SelectedUnitGridPos = FIntPoint(-1, -1);
			return;
		}

		// unit clicked is friendly but different from the currently selected unit → change selection
		if (SelectedUnit)
		{
			if (GridMap.Contains(SelectedUnitGridPos))
			{
				FTileData& OldTile = GridMap[SelectedUnitGridPos];

				SafeSetTileColor(OldTile,
					OldTile.BaseColor.R,
					OldTile.BaseColor.G,
					OldTile.BaseColor.B);
			}
			
			if (SelectedUnit->bHasMoved || SelectedUnit->bHasAttacked)
			{
				EndUnitTurn(SelectedUnit);
			}
			HideMovementRange();
		}

		// select the new unit
		SelectedUnit = ClickedUnit;
		SelectedUnitGridPos = GridPos;

		
		SafeSetTileColor(GridMap[GridPos], 1.f, 0.f, 1.f);

		ShowMovementRange();
		return;
	}

	//--------------------------------------------
	// SELECTION, DESELECTION, ATTACK, DEPLACEMENT
	//---------------------------------------------
	if (!SelectedUnit)
		return;
	// security check: if the selected unit is not on the grid anymore (ex: killed but not deselected),
	// just deselect it without trying to do anything
	if (SelectedUnit && !GridMap.Contains(SelectedUnitGridPos))
	{
		SelectedUnit = nullptr;
		SelectedUnitGridPos = FIntPoint(-1, -1);
		return;
	}
	// priotity to attack 
	if (TryAttack(GridPos))
	{
		HideMovementRange();
		SelectedUnit = nullptr;
		SelectedUnitGridPos = FIntPoint(-1, -1);
		return;
	}

	// move oniy if the unit has not moved yet (but can still attack after moving)
	if (!SelectedUnit->bHasMoved)
	{
		TArray<FIntPoint> Path = FindPathAStar(SelectedUnitGridPos, GridPos);

		if (Path.Num() > 0)
		{
			FIntPoint OldPos = SelectedUnitGridPos;
			FIntPoint FinalPos = Path.Last();

			// reset color of the old tile
			if (GridMap.Contains(SelectedUnitGridPos))
			{
				FTileData& OldTile = GridMap[SelectedUnitGridPos];
				SafeSetTileColor(OldTile,
					OldTile.BaseColor.R,
					OldTile.BaseColor.G,
					OldTile.BaseColor.B);
			}

			GridMap[OldPos].OccupyingUnit = nullptr;
			GridMap[FinalPos].OccupyingUnit = SelectedUnit;

			FVector NewWorldPos =
				GridMap[FinalPos].WorldPosition + FVector(0, 0, 80);

			SelectedUnit->SetActorLocation(NewWorldPos);
			LogMove(SelectedUnit, OldPos, FinalPos);
			SelectedUnitGridPos = FinalPos;

			SelectedUnit->bHasMoved = true;

			// maintaint selection color after moving
			SafeSetTileColor(GridMap[FinalPos], 1.f, 0.f, 1.f);
			// hide movement range after moving but keep attack range visible
			HideMovementRange();
			ShowAttackRange();

			// end turn immediately if the unit has no attack or has already attacked (ex: moved after attacking)
			if (SelectedUnit->bHasMoved && SelectedUnit->bHasAttacked)
			{
				EndUnitTurn(SelectedUnit);
			}

			return;
		}
	}

	// end turn if the unit has already moved and attacked 
	if (SelectedUnit && SelectedUnit->bHasMoved && !SelectedUnit->bHasAttacked)
	{
		EndUnitTurn(SelectedUnit);
	}

	//  deselectthe unit if the player clicks on an empty cell without moving or attacking
	if (GridMap.Contains(SelectedUnitGridPos))
	{
		FTileData& OldTile = GridMap[SelectedUnitGridPos];

		SafeSetTileColor(OldTile,
			OldTile.BaseColor.R,
			OldTile.BaseColor.G,
			OldTile.BaseColor.B);
	}

	HideMovementRange();

	SelectedUnit = nullptr;
	SelectedUnitGridPos = FIntPoint(-1, -1);
}

/*////////////////////////////////
MAP GENERATION USING PERLIN NOISE:
*/////////////////////////////////
void AGridManager::GenerateMap()
{
	GridMeshVisual->ClearInstances();
	GridMap.Empty();

	float MapOffset = 2000.f + Seed;

	for (int32 x = 0; x < GridSize; x++)
	{
		for (int32 y = 0; y < GridSize; y++)
		{
			// use Perlin noise to generate a height value for this cell based on its coordinates and the seed
			float ReferenceSize = 25.f; 
			float ScaleFactor = NoiseScale * (ReferenceSize / (float)GridSize);

			float SampleX = x * ScaleFactor + Seed;
			float SampleY = y * ScaleFactor + Seed;

			//  add a large offset to avoid sampling the low-quality area of Perlin noise around (0,0)
			float Noise = FMath::PerlinNoise2D(FVector2D(SampleX, SampleY));

			float Normalized = (Noise + 1.f) * 0.5f;
			// quantize the height into 5 levels (0 to 4) to create distinct terrain types
			int32 ZLevel = FMath::Clamp(
				FMath::FloorToInt(Normalized * 5.f),
				0,
				4
			);

			FTileData Tile;

			Tile.CoordName = GetColLetter(x) + FString::FromInt(y);
			Tile.Elevation = ZLevel;
			Tile.bIsWalkable = (ZLevel != 0);
			// calculate the world position of the tile based on its grid coordinates and the tile spacing
			Tile.WorldPosition =
				GetActorLocation() +
				FVector(y * TileSpacing, x * TileSpacing, ZLevel * 50.f);


			FTransform T;
			T.SetLocation(Tile.WorldPosition);
			T.SetScale3D(FVector((TileSpacing / 100.f) * 0.90f));

			int32 InstanceIndex = GridMeshVisual->AddInstance(T);

			if (InstanceIndex < 0)
			{
				continue;
			}

			Tile.InstanceIndex = InstanceIndex;

			//------------------------------------------------
			// COLOR ASSIGNMENT BASED ON HEIGHT WITH VARIATION
			// ------------------------------------------------
			FLinearColor Color;

			switch (ZLevel)
			{
			case 0: Color = FLinearColor::Blue; break;
			case 1: Color = FLinearColor::Green; break;
			case 2: Color = FLinearColor::Yellow; break;
			case 3: Color = FLinearColor(1.f, 0.5f, 0.f); break;
			default: Color = FLinearColor::Red; break;
			}
			// attenuate the color to avoid overly bright colors that can be harsh on the eyes
			Color *= 0.6f;
			// add random variation to avoid overly uniform colors
			float Variation = FMath::FRandRange(0.9f, 1.1f);
			Color *= Variation;
			// store the color
			Tile.BaseColor = Color;

			// set the instance color using custom data (the material will read this to color the tile)
			GridMeshVisual->SetCustomDataValue(InstanceIndex, 0, Color.R, false);
			GridMeshVisual->SetCustomDataValue(InstanceIndex, 1, Color.G, false);
			GridMeshVisual->SetCustomDataValue(InstanceIndex, 2, Color.B, false);

			GridMap.Add(FIntPoint(x, y), Tile);
		}
	}

	// force the render state to update
	GridMeshVisual->MarkRenderStateDirty();

	UE_LOG(LogTemp, Warning, TEXT("GRID GENERATED: %d tiles"), GridMap.Num());
}

/*/////////////////////////////////////////////////////////////////////////////////////
PLACEMENT OF TOWERS AT FIXED POSITIONS WITH ADAPTATIVE SEARCH TO HANDLE UNWALKABLE TILES
*///////////////////////////////////////////////////////////////////////////////////////
void AGridManager::SpawnTowers()
{
	if (GridMap.Num() == 0)
	{
		
		return;
	}
	int32 Center = GridSize / 2;
	int32 IdealOffset = GridSize / 4;
	int32 MaxSearch = 10;


	TowerPositions.Empty();

	// ---------------
	// ADAPTATIVE CENTRAL TOWER 
	// --------------
	FIntPoint CenterIdeal(Center, Center);
	FIntPoint CenterFinal = FindNearestWalkableTile(CenterIdeal);

	// security check: if there is no walkable tile at all 
	// (should never happen since the map is generated with walkable tiles), just skip tower placement to avoid errors
	if (!GridMap.Contains(CenterFinal))
	{
		return;
	}

	// security check: if the nearest walkable tile to the center is too far (ex: in a corner) 
	// or if the tile is not walkable (ex: surrounded by unwalkable tiles), skip tower placement to avoid unfair situations for the player
	if (!GridMap[CenterFinal].bIsWalkable)
	{
	    return;
	}

	// march the central tower position and make it unwalkable to prevent units from spawning on it
	TowerPositions.Add(CenterFinal);
	GridMap[CenterFinal].bIsWalkable = false;
	
	// -----------------------------------
	// ADAPTATIVE PLACEMENT OF SIDE TOWERS
	// ------------------------------------
	bool bPlaced = false;

	for (int delta = 0; delta <= MaxSearch && !bPlaced; delta++)
	{
		int32 OffsetPlus = IdealOffset + delta;
		int32 OffsetMinus = IdealOffset - delta;

		TArray<int32> OffsetsToTest = { OffsetPlus, OffsetMinus };
		// test both the positive and negative offsets to find the closest valid positions for the side towers
		for (int32 Offset : OffsetsToTest)
		{
			if (Offset <= 0)
				continue;

			FIntPoint Left(Center - Offset, Center);
			FIntPoint Right(Center + Offset, Center);

			if (!GridMap.Contains(Left) || !GridMap.Contains(Right))
				continue;

			if (GridMap[Left].bIsWalkable && GridMap[Right].bIsWalkable)
			{
				TowerPositions.Add(Left);
				TowerPositions.Add(Right);
				// mark the tower positions as unwalkable to prevent units from spawning on them
				if (GridMap.Contains(Left))
				{
					GridMap[Left].bIsWalkable = false;
				}

				if (GridMap.Contains(Right))
				{
					GridMap[Right].bIsWalkable = false;
				}

				bPlaced = true;
				break;
			}
		}
	}

	// -----------------------------------------------------------------
	// FALLBACK PLACEMENT IF NO VALID POSITION FOUND IN THE SEARCH AREA
	// ------------------------------------------------------------------
	if (!bPlaced)
	{
		FIntPoint LeftFallback = FindNearestWalkableTile(FIntPoint(Center - IdealOffset, Center));
		FIntPoint RightFallback = FindNearestWalkableTile(FIntPoint(Center + IdealOffset, Center));
		// even if the fallback positions are not ideal, 
		// we still want to place the towers to ensure the gameplay works,
		// so we just take the nearest walkable tiles to the ideal positions without checking for distance or walkability (since FindNearestWalkableTile already returns the nearest walkable tile)
		if (GridMap.Contains(LeftFallback))
		{
			TowerPositions.Add(LeftFallback);
			GridMap[LeftFallback].bIsWalkable = false;
		}

		if (GridMap.Contains(RightFallback))
		{
			TowerPositions.Add(RightFallback);
			GridMap[RightFallback].bIsWalkable = false;
		}
	}

	RedrawTowers();
}

/*///////////////////////////////////////////
PLACEMENT OF UNITS DURING THE PLACEMENT PHASE:
*////////////////////////////////////////////
void AGridManager::TryPlaceUnit(FIntPoint Pos)
{    
	// security checks to ensure the placement is valid:
	if (!GridMap.Contains(Pos))
		return;

	if (!GridMap[Pos].bIsWalkable)
		return;

	if (UnitsPlaced >= MaxUnitsPerPlayer * 2)
		return;

	// determine which player is placing based on the number of units already placed (alternating between player 0 and 1)
	int CurrentPlacingPlayer = (FirstPlayer + UnitsPlaced) % 2;

	if (CurrentPlacingPlayer == 0)
	{
		// human player 3 lines 
		if (Pos.Y < GridSize - 3)
			return;
	}
	else
	{
		// AI player lines
		if (Pos.Y > 2)
			return;
	}


	FVector SpawnLoc = GridMap[Pos].WorldPosition + FVector(0, 0, 80);

	ABaseUnit* NewUnit = nullptr;

	// detect which type of unit to spawn 
	// based on the number of units already placed by the player (alternating between sniper and brawler)
	int UnitsPlacedByPlayer = UnitsPlaced / 2;

	if (UnitsPlacedByPlayer % 2 == 0)
	{
		NewUnit = GetWorld()->SpawnActor<ASniperUnit>(
			ASniperUnit::StaticClass(),
			SpawnLoc,
			FRotator::ZeroRotator
		);
	}
	else
	{
		NewUnit = GetWorld()->SpawnActor<ABrawlerUnit>(
			ABrawlerUnit::StaticClass(),
			SpawnLoc,
			FRotator::ZeroRotator
		);
	}
	// assign the correct team color to the unit based on the current placing player
	if (!NewUnit)
		return;

	NewUnit->SetTeamColor(CurrentPlacingPlayer);

	GridMap[Pos].bIsWalkable = false;

	UnitsPlaced++;
	CurrentPlayer = 1 - CurrentPlayer;
	// record the unit in the grid map to keep track of which unit is on which tile (used for movement, attack, AI decisions...)
	if (NewUnit)
	{
		NewUnit->OwnerPlayer = CurrentPlacingPlayer;
		NewUnit->SetTeamColor(CurrentPlacingPlayer);
		GridMap[Pos].OccupyingUnit = NewUnit; 
		NewUnit->InitialGridPosition = Pos;
	}
	// transition to the gameplay phase after all units are placed, with the correct starting player based on the first player setting
	if (UnitsPlaced >= MaxUnitsPerPlayer * 2)
	{
		CurrentPhase = EGamePhase::Gameplay;
		CurrentPlayer = FirstPlayer;
		//reset the unit placement counter for potential future use
		bIsAIPlaying = false;

		
	}
}
/*///////////////////////////////////////////////////////////////
SELECTION AND DESELECTION OF UNITS:
When the player clicks on a cell, we check if there is an unit on it.
*/////////////////////////////////////////////////////////////////////

void AGridManager::SelectUnit(FIntPoint Pos)
{
	if (!GridMap.Contains(Pos))
		return;

	FTileData& Tile = GridMap[Pos];

	// click on a cell with an unit → select/deselect or attack
	for (TActorIterator<ABaseUnit> It(GetWorld()); It; ++It)
	{
		ABaseUnit* Unit = *It;

		FVector UnitLoc = Unit->GetActorLocation();
		FVector TileLoc = Tile.WorldPosition + FVector(0, 0, 80);

		float Dist2D = FVector::Dist2D(UnitLoc, TileLoc);

		if (Dist2D < 80.f)
		{

			// cannot select enemy units
			if (Unit->OwnerPlayer != CurrentPlayer)
			{
				return;
			}

			if (Unit->bTurnCompleted)
			{
				return;
			}

			// deselect if the same unit is clicked again
			if (SelectedUnit == Unit)
			{
				GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 0, Tile.BaseColor.R);
				GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 1, Tile.BaseColor.G);
				GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 2, Tile.BaseColor.B);

				HideMovementRange();


				SelectedUnitGridPos = FIntPoint(-1, -1);
				return;
			}

			// Reset color of previously selected unit's tile
			if (SelectedUnit)
			{
				if (GridMap.Contains(SelectedUnitGridPos))
				{
					FTileData& OldTile = GridMap[SelectedUnitGridPos];

					SafeSetTileColor(OldTile,
						OldTile.BaseColor.R,
						OldTile.BaseColor.G,
						OldTile.BaseColor.B);
				}
				HideMovementRange();
			}
			// 
			if (SelectedUnit && SelectedUnit != Unit)
			{
				if (SelectedUnit->bHasAttacked)
				{
					EndUnitTurn(SelectedUnit);
				}
			}
			// new unit selected → update selection and show movement range
			SelectedUnit = Unit;
			SelectedUnitGridPos = Pos;

			GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 0, 1.f);
			GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 1, 0.f);
			GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 2, 1.f);

			ShowMovementRange();

			return;
		}
	}

	// click on an empty cell → deselect current unit if any and hide movement range
	if (SelectedUnit)
	{
		if (GridMap.Contains(SelectedUnitGridPos))
		{
			FTileData& OldTile = GridMap[SelectedUnitGridPos];

			SafeSetTileColor(OldTile,
				OldTile.BaseColor.R,
				OldTile.BaseColor.G,
				OldTile.BaseColor.B);
		}

		HideMovementRange();

		SelectedUnit = nullptr;
		SelectedUnitGridPos = FIntPoint(-1, -1);
	}
}
/*///////////////////////////////////////
SHOW MOVEMENT RANGE OF THE SELECTED UNIT
*///////////////////////////////////////
void AGridManager::ShowMovementRange()
{
	if (!SelectedUnit)
		return;
	// security check: if the selected unit is not on the grid anymore (ex: killed but not deselected),
	if (!GridMap.Contains(SelectedUnitGridPos))
	{
		SelectedUnit = nullptr;
		SelectedUnitGridPos = FIntPoint(-1, -1);
		return;
	}
	// log the selected unit's type and attack range 
	UE_LOG(LogTemp, Warning, TEXT("AttackRange = %d"), SelectedUnit->AttackRange);
	if (SelectedUnit->IsA(ASniperUnit::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Unit = SNIPER"));
	}
	else if (SelectedUnit->IsA(ABrawlerUnit::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Unit = BRAWLER"));
	}

	HideMovementRange();

	CurrentRangeTiles.Empty();

	TMap<FIntPoint, int> Reachable = GetReachableTiles(SelectedUnit);

	for (auto& Elem : Reachable)
	{
		FIntPoint Pos = Elem.Key;

		CurrentRangeTiles.Add(Pos);
		// skip the tile of the selected unit
		if (Pos == SelectedUnitGridPos)
			continue;

		SafeSetTileColor(
			GridMap[Pos],
			0.f, 1.f, 1.f   // CYAN
		);
	}
}

/*///////////////////////////////////////////////////////////////////////////////////
CANCELS THE MOVEMENT RANGE HIGHLIGHTING AND RESTORE THE ORIGINAL COLORS OF THE TILES
*////////////////////////////////////////////////////////////////////////////////////
void AGridManager::HideMovementRange()
{
	for (FIntPoint Pos : CurrentRangeTiles)
	{
		if (!GridMap.Contains(Pos)) continue;

		const FTileData& Tile = GridMap[Pos];

		SafeSetTileColor(
			Tile,
			Tile.BaseColor.R,
			Tile.BaseColor.G,
			Tile.BaseColor.B
		);
	}

	CurrentRangeTiles.Empty();

	UpdateTowers();
}

/*//////////////////////////////////////////////////////////////////////
SEARCH ALGORITHM TO FIND THE NEAREST WALKABLE TILE TO A TARGET POSITION
*///////////////////////////////////////////////////////////////////////
FIntPoint AGridManager::FindNearestWalkableTile(FIntPoint TargetPos)
{
	if (GridMap.Contains(TargetPos) && GridMap[TargetPos].bIsWalkable)
		return TargetPos;

	for (int Radius = 1; Radius <= 8; Radius++)
	{
		for (int dx = -Radius; dx <= Radius; dx++)
		{
			for (int dy = -Radius; dy <= Radius; dy++)
			{
				FIntPoint Check = TargetPos + FIntPoint(dx, dy);

				if (GridMap.Contains(Check) && GridMap[Check].bIsWalkable)
					return Check;
			}
		}
	}

	return TargetPos;
}

/*////////////////////////////////////////////////////////////////////////////////////////////////////////
BFS TO FIND ALL REACHABLE TILES FOR THE SELECTED UNIT BASED ON ITS MOVEMENT RANGE AND THE TERRAIN COSTS
 AND RETURN A MAP OF THESE TILES WITH THE COST TO REACH THEM (USED FOR MOVEMENT HIGHLIGHTING AND AI DECISIONS)
*/////////////////////////////////////////////////////////////////////////////////////////////////////////////

TMap<FIntPoint, int> AGridManager::GetReachableTiles(ABaseUnit* Unit)
{
	// Map to store the reachable tiles and the cost to reach them (used for movement range highlighting and AI pathfinding)
	TMap<FIntPoint, int> Reachable;

	if (!Unit)
		return Reachable;

	FIntPoint Start = SelectedUnitGridPos;
	// Queue for BFS
	TQueue<FIntPoint> Queue;

	Queue.Enqueue(Start);

	// initialize the reachable map with the starting position and a cost of 0
	Reachable.Add(Start, 0);

	// While there are tiles to explore
	while (!Queue.IsEmpty())
	{
		FIntPoint Current;
		Queue.Dequeue(Current);

		
		int CurrentCost = Reachable[Current];

		
		TArray<FIntPoint> Directions =
		{
			FIntPoint(1,0),   // right
			FIntPoint(-1,0),  // left
			FIntPoint(0,1),   // top
			FIntPoint(0,-1)   // bottom
		};

		for (FIntPoint Dir : Directions)
		{
			FIntPoint Next = Current + Dir;

			
			if (!GridMap.Contains(Next))
				continue;

			if (!GridMap[Next].bIsWalkable)
				continue;

			if (GridMap[Next].OccupyingUnit != nullptr)
				continue;

			int HeightDiff =
				GridMap[Next].Elevation -
				GridMap[Current].Elevation;

			// moving uphill costs 2, moving on flat or downhill costs 1
			int MoveCost = (HeightDiff > 0) ? 2 : 1;

			int NewCost = CurrentCost + MoveCost;

			// If it exceeds the unit's movement capacity
			if (NewCost > Unit->MoveRange)
				continue;

		
			//Update the cost if:
			// - the tile has never been visited
			// - or if a less costly path is found
	
			if (!Reachable.Contains(Next) || NewCost < Reachable[Next])
			{
				Reachable.Add(Next, NewCost);
				Queue.Enqueue(Next);
			}
		}
	}

	return Reachable;
}
/*/////////////////////////////////////////////////////////////////////////////////
UTILITY FUNCTION TO CONVERT A COLUMN INDEX TO A LETTER (0 → A, 1 → B, ..., 25 → Z)
*//////////////////////////////////////////////////////////////////////////////////
FString AGridManager::GetColLetter(int32 Index)
{
	if (Index < 0 || Index>25)
		return "Z";

	TCHAR Letter = TCHAR('A' + Index);

	return FString::Printf(TEXT("%c"), Letter);
}

/*/////////////////////////////////////
DISTANCE HEURISTIC FOR A* PATHFINDING:
*//////////////////////////////////////
int AGridManager::Heuristic(FIntPoint A, FIntPoint B)
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

/*/////////////////////////////////////////////////////////////////////////////////////////////////
RETURN THE PATH FROM START TO GOAL AS AN ARRAY OF GRID COORDINATES USING A* PATHFINDING ALGORITHM
*/////////////////////////////////////////////////////////////////////////////////////////////////
TArray<FIntPoint> AGridManager::FindPathAStar(FIntPoint Start, FIntPoint Goal)
{
	TArray<FIntPoint> EmptyResult;
	// security check: if the start or goal positions are not on the grid, return an empty path
	if (!GridMap.Contains(Start) || !GridMap.Contains(Goal))
		return EmptyResult;
	
	TArray<FPathNode> OpenSet;
	TSet<FIntPoint> ClosedSet;
	TMap<FIntPoint, FIntPoint> CameFrom;
	TMap<FIntPoint, int> GScore;

	FPathNode StartNode;
	StartNode.Position = Start;
	StartNode.GCost = 0;
	StartNode.HCost = Heuristic(Start, Goal);

	OpenSet.Add(StartNode);
	GScore.Add(Start, 0);

	while (OpenSet.Num() > 0)
	{
		// find the node in the open set with the lowest F cost
		int BestIndex = 0;

		for (int i = 1; i < OpenSet.Num(); i++)
		{
			if (OpenSet[i].FCost() < OpenSet[BestIndex].FCost())
				BestIndex = i;
		}

		FPathNode Current = OpenSet[BestIndex];
		OpenSet.RemoveAt(BestIndex);

		if (Current.Position == Goal)
		{
			// reconstruct the path by backtracking from the goal to the start using the CameFrom map
			TArray<FIntPoint> Path;
			FIntPoint Step = Goal;

			while (CameFrom.Contains(Step))
			{
				Path.Add(Step);
				Step = CameFrom[Step];
			}

			Path.Add(Start);
			Algo::Reverse(Path);
			return Path;
		}

		ClosedSet.Add(Current.Position);

		TArray<FIntPoint> Directions =
		{
			FIntPoint(1,0),
			FIntPoint(-1,0),
			FIntPoint(0,1),
			FIntPoint(0,-1)
		};

		// explore the neighbors of the current node
		for (FIntPoint Dir : Directions)
		{
			FIntPoint Neighbor = Current.Position + Dir;

			if (!GridMap.Contains(Neighbor))
				continue;

			if (!GridMap[Neighbor].bIsWalkable)
				continue;

			if (ClosedSet.Contains(Neighbor))
				continue;

			int HeightDiff =
				GridMap[Neighbor].Elevation -
				GridMap[Current.Position].Elevation;

			int MoveCost = (HeightDiff > 0) ? 2 : 1;

			int TentativeG = GScore[Current.Position] + MoveCost;

			if (!SelectedUnit)
				return EmptyResult;

			if (TentativeG > SelectedUnit->MoveRange)
				continue;

			// if the neighbor is not in the open set
			// or if a cheaper path to the neighbor is found, update the path and costs
			if (!GScore.Contains(Neighbor) || TentativeG < GScore[Neighbor])
			{
				CameFrom.Add(Neighbor, Current.Position);
				GScore.Add(Neighbor, TentativeG);

				FPathNode NeighborNode;
				NeighborNode.Position = Neighbor;
				NeighborNode.GCost = TentativeG;
				NeighborNode.HCost = Heuristic(Neighbor, Goal);

				OpenSet.Add(NeighborNode);
			}
		}
	}

	return EmptyResult;
}

/*////////////////////////////////////////////////////////////////////////////////////////////////
REDRAW TOWERS WITH DYNAMIC COLOR AND SIZE BASED ON THEIR STATE AND THE NUMBER OF UNITS AROUND THEM
*//////////////////////////////////////////////////////////////////////////////////////////////////
void AGridManager::RedrawTowers()
{
	// IF NO TOWER DATA, DRAW PLACEHOLDERS AT THE TOWER POSITIONS
	if (Towers.Num() == 0)
	{
		for (FIntPoint Pos : TowerPositions)
		{
			if (!GridMap.Contains(Pos)) continue;

			float Scale = TileSpacing * 0.4f;

			DrawDebugBox(
				GetWorld(),
				GridMap[Pos].WorldPosition + FVector(0, 0, Scale * 2),
				FVector(Scale, Scale, Scale * 4),
				FColor::White,
				true,
				-1.f,
				0,
				15.f
			);
		}
		return;
	}

	// shows towers with different colors and sizes based on their state and the number of units around them
	for (const FTowerData& Tower : Towers)
	{
		if (!GridMap.Contains(Tower.Position)) continue;

		// Assign tower color based on controlling player
		FColor Color;

		switch (Tower.State)
		{
		case ETowerState::Neutral:
			Color = FColor::White;
			break;

		case ETowerState::Controlled:
			Color = (Tower.OwnerPlayer == 0)
				? FColor::Cyan   // Human player color
				: FColor::Black; // AI color
			break;

		case ETowerState::Contested:
			Color = FColor::Yellow;
			break;

		default:
			Color = FColor::White;
			break;
		}

		// set size based on the number of units around (more units = bigger tower, with a max size to avoid excessive sizes)
		float Scale = TileSpacing * 0.4f;

		DrawDebugBox(
			GetWorld(),
			GridMap[Tower.Position].WorldPosition + FVector(0, 0, Scale * 2),
			FVector(Scale, Scale, Scale * 4),
			Color,
			true,
			-1.f,
			0,
			15.f
		);
	}
}
/*////////////////////////////////////////////////////////////////////////////////////////////////////////
UPDATE THE STATE OF EACH TOWER BASED ON THE NUMBER OF UNITS AROUND THEM AND THEIR OWNERS, THEN REDRAW THEM
*//////////////////////////////////////////////////////////////////////////////////////////////////////////
void AGridManager::UpdateTowers()
{
	//sync the Towers array with the TowerPositions to ensure we have a tower data for each tower position,
	if (Towers.Num() != TowerPositions.Num())
	{
		Towers.Empty();

		for (FIntPoint Pos : TowerPositions)
		{
			FTowerData T;
			T.Position = Pos;
			Towers.Add(T);
		}
	}

	for (FTowerData& Tower : Towers)
	{
		if (!GridMap.Contains(Tower.Position))
			continue;

		int PlayerCount[2] = { 0, 0 };

		for (auto& Elem : GridMap)
		{
			FIntPoint Pos = Elem.Key;
			FTileData& Tile = Elem.Value;

			if (!Tile.OccupyingUnit)
				continue;

			if (!IsValid(Tile.OccupyingUnit))
				continue;

			// zone of control of the tower is a 5x5 square centered on the tower (distance ≤ 2 in any direction)
			int dx = FMath::Abs(Pos.X - Tower.Position.X);
			int dy = FMath::Abs(Pos.Y - Tower.Position.Y);

			int Dist = FMath::Max(dx, dy);

			if (Dist <= 2)
			{
				int Player = Tile.OccupyingUnit->OwnerPlayer;

				if (Player == 0 || Player == 1)
				{
					PlayerCount[Player]++;

					UE_LOG(LogTemp, Warning,
						TEXT("Tower [%d,%d] sees unit Player %d at [%d,%d]"),
						Tower.Position.X,
						Tower.Position.Y,
						Player,
						Pos.X,
						Pos.Y
					);
				}
			}
		}

		////------------
		// TOWER STATES
		// -------------
		if (PlayerCount[0] > 0 && PlayerCount[1] == 0)
		{
			Tower.State = ETowerState::Controlled;
			Tower.OwnerPlayer = 0;
		}
		else if (PlayerCount[1] > 0 && PlayerCount[0] == 0)
		{
			Tower.State = ETowerState::Controlled;
			Tower.OwnerPlayer = 1;
		}
		else if (PlayerCount[0] > 0 && PlayerCount[1] > 0)
		{
			Tower.State = ETowerState::Contested;
			Tower.OwnerPlayer = -1;
		}
		else
		{
			Tower.State = ETowerState::Neutral;
			Tower.OwnerPlayer = -1;
		}

	
		UE_LOG(LogTemp, Warning,
			TEXT("Tower [%d,%d] => Player 0:%d Player 1:%d"),
			Tower.Position.X,
			Tower.Position.Y,
			PlayerCount[0],
			PlayerCount[1]
		);
	}

	RedrawTowers();
}

/*/////////////////////////////////////////////////////////////////////////////////////////////////
* UTILITY FUNCTION TO SAFELY SET THE COLOR OF A TILE INSTANCE, CHECKING IF THE INSTANCE INDEX IS VALID
*////////////////////////////////////////////////////////////////////////////////////////////////////
void AGridManager::SafeSetTileColor(const FTileData& Tile, float R, float G, float B)
{
	if (Tile.InstanceIndex == -1)
		return;

	GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 0, R);
	GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 1, G);
	GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 2, B);
}

/*/////////////////////////////////////////////////////////////////////////////////////////////////
* RESET UNITS FOR NEW TURN
*////////////////////////////////////////////////////////////////////////////////////////////////////
void AGridManager::ResetUnitsForNewTurn()
{
	for (TActorIterator<ABaseUnit> It(GetWorld()); It; ++It)

	{
      if (It->OwnerPlayer == CurrentPlayer)
		{
           It->ResetTurn();
		}
	}
}

/*///////////////
* END UNIT TURN
*/////////////////
void AGridManager::EndUnitTurn(ABaseUnit* Unit)
{
	if (!Unit) return;
	if (Unit->bTurnCompleted) return;

	Unit->bTurnCompleted = true;

	int PlayerIndex = Unit->OwnerPlayer;
	UnitsPlayedThisTurn[PlayerIndex]++;

	// if the player has played 2 units this turn, end the turn and switch to the other player
	if (UnitsPlayedThisTurn[PlayerIndex] >= 2)
	{
		UpdateTowers();
		CheckVictoryCondition();
		UnitsPlayedThisTurn[PlayerIndex] = 0;

		if (CurrentPlayer == PlayerIndex)
		{
			CurrentPlayer = 1 - CurrentPlayer;
		}

		ResetUnitsForNewTurn();

		// if the new current player is the AI, start the AI turn
		if (CurrentPlayer == 0)
		{
			bIsAIPlaying = false;
		}
	}
}

/*/////////////////////////////////////////////////////////////////
* ATTEMPT ATTACK ON A TARGET TILE,
CHECKING ALL CONDITIONS (RANGE, HEIGHT, LOS, UNIT TYPES...) AND APPLY DAMAGE AND COUNTER-ATTACKS IF VALID
*/////////////////////////////////////////////////////////////////////////////////////////////////////////
bool AGridManager::TryAttack(FIntPoint TargetPos)
{
	if (!SelectedUnit)
		return false;
	
	if (!GridMap.Contains(SelectedUnitGridPos))
	{
		SelectedUnit = nullptr;
		SelectedUnitGridPos = FIntPoint(-1, -1);
		return false;
	}
	//if the selected unit is not on the grid anymore, cancel the attack to avoid errors
	bool bIsSniper = SelectedUnit->IsA(ASniperUnit::StaticClass());

	// cannot attack if the unit has already attacked this turn
	if (SelectedUnit->bHasAttacked)
		return false;

	if (!GridMap.Contains(TargetPos))
		return false;

	FTileData& Tile = GridMap[TargetPos];

	ABaseUnit* TargetUnit = Tile.OccupyingUnit;

	if (!TargetUnit)
		return false;

	// cannot attack empty tiles
	if (TargetUnit->OwnerPlayer == SelectedUnit->OwnerPlayer)
		return false;

	// --------
	// DISTANCE
	// --------
	int dx = FMath::Abs(TargetPos.X - SelectedUnitGridPos.X);
	int dy = FMath::Abs(TargetPos.Y - SelectedUnitGridPos.Y);

	int Distance = FMath::Max(dx, dy);

	if (Distance > SelectedUnit->AttackRange)
		return false;

	//---------
	// HEIGHT
	// -------
	int AttackerZ = GridMap[SelectedUnitGridPos].Elevation;
	int TargetZ = GridMap[TargetPos].Elevation;

	if (TargetZ > AttackerZ)
		return false;


	if (!bIsSniper)
	{
		if (!GridMap[TargetPos].bIsWalkable)
		{
			return false;
		}
	}

	/// ---------------------------------------------------------------------------------------
//AVOIDING ATTACK THROUGH OBSTACLES (LOS) FOR SNIPER AND BRAWLER 
// (Brawler cannot attack through obstacles, 
// sniper can attack through walkable tiles but not through unwalkable tiles like towers or water)
// -------------------------------------------------------------------------------------------------

	int dxStep = TargetPos.X - SelectedUnitGridPos.X;
	int dyStep = TargetPos.Y - SelectedUnitGridPos.Y;

	dxStep = (dxStep == 0) ? 0 : (dxStep > 0 ? 1 : -1);
	dyStep = (dyStep == 0) ? 0 : (dyStep > 0 ? 1 : -1);



	// if there is a direction (not attacking on the same tile), check the tiles along the line of attack for obstacles
	if (dxStep != 0 || dyStep != 0)
	{
		FIntPoint CheckPos = SelectedUnitGridPos;

		while (true)
		{
			CheckPos += FIntPoint(dxStep, dyStep);

			if (CheckPos == TargetPos)
				break;

			if (!GridMap.Contains(CheckPos))
				break;


			// not sniper = LOS blocked by any unwalkable tile (tower, water...) or any unit
			if (!bIsSniper)
			{
				// LOS blocked by unwalkable tile
				if (!GridMap[CheckPos].bIsWalkable)
				{
					return false;
				}

				// LOS blocked by any unit
				if (GridMap[CheckPos].OccupyingUnit != nullptr)
				{
					return false;
				}
			}

		}
	}
	///-------
	// DAMAGE
	///-------
	int Damage = FMath::RandRange(
		SelectedUnit->MinDamage,
		SelectedUnit->MaxDamage
	);

	TargetUnit->CurrentHP = FMath::Max(0, TargetUnit->CurrentHP - Damage);
	LogAttack(SelectedUnit, TargetPos, Damage);

	UE_LOG(LogTemp, Warning, TEXT("Attack: %d damage"), Damage);

	// -----------
	// DEATH
	///----------
	if (TargetUnit->CurrentHP <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Unit killed"));

		// remove the unit from the grid
		GridMap[TargetPos].OccupyingUnit = nullptr;

		// reset HP
		TargetUnit->CurrentHP = TargetUnit->MaxHP;

		FIntPoint RespawnPos = TargetUnit->InitialGridPosition;

		// if the initial spawn position is occupied or not walkable (ex: tower), find the nearest walkable tile to respawn
		if (!GridMap.Contains(RespawnPos) || GridMap[RespawnPos].OccupyingUnit != nullptr)
		{
			RespawnPos = FindNearestWalkableTile(RespawnPos);
		}

		FVector NewWorldPos =
			GridMap[RespawnPos].WorldPosition + FVector(0, 0, 80);

		GridMap[RespawnPos].OccupyingUnit = TargetUnit;
		TargetUnit->SetActorLocation(NewWorldPos);
	}


	////----------------------
	/// COUNTER ATTACK (SNIPER)
	///---------------------------


	if (bIsSniper)
	{
		bool bCounter =
			TargetUnit->IsA(ASniperUnit::StaticClass()) ||
			(Distance == 1 && TargetUnit->IsA(ABrawlerUnit::StaticClass()));
		// sniper can counter attack if the target is a sniper or if it's a brawler attacking at melee range (distance = 1)
		if (bCounter)
		{
			int CounterDamage = FMath::RandRange(
				TargetUnit->MinDamage,
				TargetUnit->MaxDamage
			);
			SelectedUnit->CurrentHP = FMath::Max(0, SelectedUnit->CurrentHP - CounterDamage);

			UE_LOG(LogTemp, Warning, TEXT("Counter: %d"), CounterDamage);
		}
	}

	///--------------
	/// END ACTION
	///------------
	SelectedUnit->bHasAttacked = true;
	
	
	HideMovementRange();
	// deselect the unit after attacking and recover its tile color
	ABaseUnit* UnitRef = SelectedUnit;
	SelectedUnit = nullptr;
	SelectedUnitGridPos = FIntPoint(-1, -1);

	if (UnitRef->bTurnCompleted)
	{
		return false;
	}

	EndUnitTurn(UnitRef);

	return true;
}

///---------------------------------------
// SHOW ATTACK RANGE OF THE SELECTED UNIT, 
// DIFFERENTIATING SNIPER AND BRAWLER 
///-----------------------------------------

void AGridManager::ShowAttackRange()
{
	if (!SelectedUnit)
		return;

	if (!GridMap.Contains(SelectedUnitGridPos))
		return ;

	CurrentRangeTiles.Empty();

	int Range = SelectedUnit->AttackRange;
	
	
	bool bIsBrawler = SelectedUnit->IsA(ABrawlerUnit::StaticClass());

	// loop through a square area around the unit based on its attack range, 
	// and check each tile for validity (walkable, within range, height difference, LOS...)
	// to determine if it's a valid attack target and should be highlighted
	for (int x = -Range; x <= Range; x++)
	{
		for (int y = -Range; y <= Range; y++)
		{
			FIntPoint Pos = SelectedUnitGridPos + FIntPoint(x, y);

			if (!GridMap.Contains(Pos))
				continue;
		
		
			int Distance = FMath::Max(FMath::Abs(x), FMath::Abs(y));

			if (Distance == 0 || Distance > Range)
				continue;

			//only show melee range (distance = 1) for brawlers, they cannot attack at range
			if (bIsBrawler && Distance != 1)
				continue;
			int AttackerZ = GridMap[SelectedUnitGridPos].Elevation;
			int TargetZ = GridMap[Pos].Elevation;

			if (TargetZ > AttackerZ)
				continue;
			CurrentRangeTiles.Add(Pos);

			if (Pos == SelectedUnitGridPos)
				continue;
			SafeSetTileColor(
				GridMap[Pos],
				0.8f, 0.9f, 1.f   // white-blue for attack range (lighter than movement range)
			);
		}
	}
}

/*////////////////////////////////////////////////////////////////////////////////////////////////////////////////
* AI PLACEMENT: THE AI PLACES UNITS IN THE FIRST 3 LINES OF THE GRID (ITS STARTING ZONE) ON RANDOM WALKABLE TILES,
*///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AGridManager::AIPlaceUnit()
{
	if (UnitsPlaced >= MaxUnitsPerPlayer * 2)
		return;

	// only place units for the AI player
	if (CurrentPlayer != 1)
		return;

	// collect all valid tiles for placement (walkable, within the first 3 lines of the grid)
	TArray<FIntPoint> ValidTiles;

	for (auto& Elem : GridMap)
	{
		FIntPoint Pos = Elem.Key;
		FTileData& Tile = Elem.Value;

		if (!Tile.bIsWalkable)
			continue;

		if (Pos.Y > 2) // restrict placement to the first 3 lines (Y = 0, 1, 2)
			continue;

		ValidTiles.Add(Pos);
	}

	if (ValidTiles.Num() == 0)
		return;

	// choose a random tile from the valid tiles for placement
	int Index = FMath::RandRange(0, ValidTiles.Num() - 1);
	FIntPoint ChosenPos = ValidTiles[Index];

	// call your existing system
	TryPlaceUnit(ChosenPos);
}

/////////////////////////////////////////
// HANDLES THE AI'S TURN TO PLAY A UNIT:
/////////////////////////////////////////
void AGridManager::PlayAIUnit(ABaseUnit* Unit)
{
	///--------
	//  SAFETY
	////--------
	if (!Unit || Unit->bTurnCompleted || Unit->bHasAttacked)
		return;

	///-------------------------------------------------
	// fetch the grid position of the unit to play, 
	// if it's not found , cancel the action to avoid errors
	///-----------------------------------------------------
	SelectedUnit = Unit;

	bool bFound = false;

	for (auto& TileElem : GridMap)
	{
		if (TileElem.Value.OccupyingUnit == Unit)
		{
			SelectedUnitGridPos = TileElem.Key;
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		SelectedUnit = nullptr;
		SelectedUnitGridPos = FIntPoint(-1, -1);
		return;
	}

	///--------------------------------------------------------------------------------------------
	// attempt to attack first without moving if there is a valid target in range (IA agressive)
	///---------------------------------------------------------------------------------------------
	for (auto& TileElem : GridMap)
	{
		if (TryAttack(TileElem.Key))
		{
			return;
		}
	}

	///------------------------------------
	// find the best target to move towards 
	///------------------------------------
	FIntPoint EnemyPos = FindBestTarget(Unit);
	FIntPoint Target = SelectedUnitGridPos;

	bool bIsTowerTarget = false;
	FIntPoint TowerPos = FIntPoint(-1, -1);

	for (const FTowerData& Tower : Towers)
	{
		if (Tower.Position == EnemyPos)
		{
			bIsTowerTarget = true;
			TowerPos = Tower.Position;
			break;
		}
	}

	///-------------------------------------------------------------------------------------------------------
	// AI SMART MOVE TOWARDS THE TARGET (TOWER OR ENEMY UNIT) BASED ON A SCORING SYSTEM THAT TAKES INTO ACCOUNT:
	///-------------------------------------------------------------------------------------------------------------
	if (bIsTowerTarget)
	{
		for (auto& TileElem : GridMap)
		{
			FIntPoint Pos = TileElem.Key;

			int dx = FMath::Abs(Pos.X - TowerPos.X);
			int dy = FMath::Abs(Pos.Y - TowerPos.Y);
			int Dist = FMath::Max(dx, dy);

			if (Dist <= 2 &&
				GridMap[Pos].bIsWalkable &&
				GridMap[Pos].OccupyingUnit == nullptr)
			{
				Target = Pos;
				break;
			}
		}
	}
	else
	{
		TArray<FIntPoint> DirectionsAI =
		{
			FIntPoint(1,0),
			FIntPoint(-1,0),
			FIntPoint(0,1),
			FIntPoint(0,-1)
		};
		// if the target is an enemy unit, try to move to a tile adjacent to it to be able to attack next turn,
		for (FIntPoint Dir : DirectionsAI)
		{
			FIntPoint Test = EnemyPos + Dir;

			if (!GridMap.Contains(Test))
				continue;

			if (!GridMap[Test].bIsWalkable)
				continue;

			if (GridMap[Test].OccupyingUnit != nullptr)
				continue;

			Target = Test;
			break;
		}
	}

	///----------------------------
	/// SMART MOVE BASED ON SCORING
	///----------------------------
	TMap<FIntPoint, int> Reachable = GetReachableTiles(Unit);

	FIntPoint BestMove = SelectedUnitGridPos;
	int BestScore = -999999;

	for (auto& ReachElem : Reachable)
	{
		FIntPoint Pos = ReachElem.Key;

		if (!GridMap.Contains(Pos))
			continue;

		int Score = 0;

		// distance to target (closer is better)
		int Dist = Heuristic(Pos, Target);
		Score -= Dist * 3;

		// elevation
		Score += GridMap[Pos].Elevation * 2;

		// towers
		for (const FTowerData& Tower : Towers)
		{
			int TowerDist = Heuristic(Pos, Tower.Position);

			if (Tower.OwnerPlayer != 1)
			{
				Score -= TowerDist * 2;
			}
		}

		// danger from enemy units (closer enemies are more dangerous, so avoid them)
		for (auto& TileElem : GridMap)
		{
			ABaseUnit* Other = TileElem.Value.OccupyingUnit;

			if (Other && Other->OwnerPlayer == 0)
			{
				int EnemyDist = Heuristic(Pos, TileElem.Key);

				if (EnemyDist <= 2)
				{
					Score -= 15;
				}
			}
		}

		// bonus for being a sniper and moving to a high tile (better line of sight and more damage)
		if (Unit->IsA(ASniperUnit::StaticClass()))
		{
			Score += 5;
		}

		// chose the move with the best score
		if (Score > BestScore)
		{
			BestScore = Score;
			BestMove = Pos;
		}
	}

	///-------------------------------------
	/// MOVE THE UNIT TO THE BEST TILE CHOSEN
	///-------------------------------------
	if (BestMove != SelectedUnitGridPos)
	{
		GridMap[SelectedUnitGridPos].OccupyingUnit = nullptr;
		GridMap[BestMove].OccupyingUnit = Unit;

		FVector NewPos = GridMap[BestMove].WorldPosition + FVector(0, 0, 80);
		Unit->SetActorLocation(NewPos);
		// log the move
		LogMove(Unit, SelectedUnitGridPos, BestMove);

		SelectedUnitGridPos = BestMove;
		Unit->bHasMoved = true;

		UE_LOG(LogTemp, Warning, TEXT("AI MOVED"));
	}

	///--------------------------------------------
	// MOVE THEN ATTACK IF POSSIBLE (IA agressive)
	///---------------------------------------------
	for (auto& TileElem : GridMap)
	{
		if (TryAttack(TileElem.Key))
		{
			SelectedUnit = nullptr;
			SelectedUnitGridPos = FIntPoint(-1, -1);
			return;
		}
	}

	///----------------------------------------------------------------------------------------------
	/// END OF AI TURN, CHECK IF THE UNIT HAS ALREADY PLAYED THIS TURN TO AVOID DOUBLE END TURN CALLS
	///-----------------------------------------------------------------------------------------------
	EndUnitTurn(Unit);

	if (!Unit->bTurnCompleted)
	{
		EndUnitTurn(Unit);
	}
}
/*///////////////////////////////////////////////////////////////////////////////////////////////////
CHECK THE VICTORY CONDITION: IF A PLAYER CONTROLS 2 OR MORE TOURS FOR 2 CONSECUTIVE TURNS, THEY WIN
*///////////////////////////////////////////////////////////////////////////////////////////////////
void AGridManager::CheckVictoryCondition()
{
	int TowerCount[2] = { 0, 0 };

	// count how many towers each player controls
	for (const FTowerData& Tower : Towers)
	{
		if (Tower.State == ETowerState::Controlled)
		{
			if (Tower.OwnerPlayer == 0 || Tower.OwnerPlayer == 1)
			{
				TowerCount[Tower.OwnerPlayer]++;
			}
		}
	}

	// check if any player controls 2 or more towers, and update their consecutive control streak
	for (int Player = 0; Player < 2; Player++)
	{
		if (TowerCount[Player] >= 2)
		{
			ConsecutiveTowerControl[Player]++;
		}
		else
		{
			ConsecutiveTowerControl[Player] = 0;
		}
		
		FString PlayerName = (Player == 0) ? TEXT("PLAYER") : TEXT("AI");

		
		// log the current tower control status for each player
		if (ConsecutiveTowerControl[Player] >= 2)
		{
			FString Winner = (Player == 0) ? TEXT("PLAYER") : TEXT("AI");

			UE_LOG(LogTemp, Warning, TEXT("GAME OVER: %s WINS"), *Winner);

			bGameOver = true;
			WinnerPlayer = Player;
			UpdateUI();

			// if the game is over, disable ticking to stop all game logic
			SetActorTickEnabled(false);
		}
	}
}

/*///////////////////////////////////////////////////////////////////////////////////////
DISPLAY LOGGING: FUNCTIONS TO LOG UNIT MOVEMENTS AND ATTACKS IN A HUMAN-READABLE FORMAT,
*///////////////////////////////////////////////////////////////////////////////////////
FString GetUnitLetter(ABaseUnit* Unit)
{
	if (Unit->IsA(ASniperUnit::StaticClass())) return "S";
	if (Unit->IsA(ABrawlerUnit::StaticClass())) return "B";
	return "?";
}
/*/////////////////////////////////////////////////////////////////////////////
DISTINGUISH PLAYER AND AI IN LOGS BY USING THEIR NAMES INSTEAD OF JUST NUMBERS
*//////////////////////////////////////////////////////////////////////////////
FString GetPlayerTag(int Player)
{
	return (Player == 0) ? "HP" : "AI";
}

/*//////////////////////////////////////////////////////////////////////////
DISPLAY A LOG OF THE UNIT'S MOVEMENT FROM ONE TILE TO ANOTHER IN THE FORMAT:
*///////////////////////////////////////////////////////////////////////////
void AGridManager::LogMove(ABaseUnit* Unit, FIntPoint From, FIntPoint To)
{
	if (!GridMap.Contains(From) || !GridMap.Contains(To))
		return;

	FString Log = FString::Printf(
		TEXT("%s: %s %s -> %s"),
		*GetPlayerTag(Unit->OwnerPlayer),
		*GetUnitLetter(Unit),
		*GridMap[From].CoordName,
		*GridMap[To].CoordName
	);

	MoveHistory.Add(Log);

	UE_LOG(LogTemp, Warning, TEXT("%s"), *Log);
}

/*///////////////////////////////////////////////////////////////////////////////////////
DISPLAY A LOG OF THE UNIT'S ATTACK ON A TARGET TILE WITH THE DAMAGE DEALT IN THE FORMAT:
*/////////////////////////////////////////////////////////////////////////////////////////
void AGridManager::LogAttack(ABaseUnit* Unit, FIntPoint TargetPos, int Damage)
{  
	if (!GridMap.Contains(TargetPos))
		return;

	FString Log = FString::Printf(
		TEXT("%s: %s %s %d"),
		*GetPlayerTag(Unit->OwnerPlayer),
		*GetUnitLetter(Unit),
		*GridMap[TargetPos].CoordName,
		Damage
	);

	MoveHistory.Add(Log);

	UE_LOG(LogTemp, Warning, TEXT("%s"), *Log);
}

/*///////////////////////////////////////////////////////////////////
 UPDATES THE GAME INTERFACE ELEMENTS
  SYNCRONISES THE HUD WITH the current grid state and player turns.
*////////////////////////////////////////////////////////////////////
void AGridManager::UpdateUI()
{
	if (!WidgetInstance) return;

	UTextBlock* TurnText = Cast<UTextBlock>(
		WidgetInstance->GetWidgetFromName(TEXT("TurnText"))
	);

	UTextBlock* CoinText = Cast<UTextBlock>(
		WidgetInstance->GetWidgetFromName(TEXT("CoinText"))
	);

	UTextBlock* TowerText = Cast<UTextBlock>(
		WidgetInstance->GetWidgetFromName(TEXT("TowerText"))
	);

	UTextBlock* HPText = Cast<UTextBlock>(
		WidgetInstance->GetWidgetFromName(TEXT("HPText"))
	);

	UTextBlock* PhaseText = Cast<UTextBlock>(
		WidgetInstance->GetWidgetFromName(TEXT("PhaseText"))
	);

	UTextBlock* HistoryText = Cast<UTextBlock>(
		WidgetInstance->GetWidgetFromName(TEXT("HistoryText"))
	);

	///-------
	// TURN
	///-------
	if (TurnText)
	{
		///-----------
		//  GAME OVER
		///-----------
		if (bGameOver)
		{
			if (WinnerPlayer == 0)
			{
				TurnText->SetText(FText::FromString(TEXT(" GAME OVER: PLAYER WINS")));
				TurnText->SetColorAndOpacity(FSlateColor(FLinearColor(0.f, 1.f, 1.f)));
			}
			else
			{
				TurnText->SetText(FText::FromString(TEXT(" GAME OVER: AI WINS")));
				TurnText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
			}

			return; 
		}

		///-------------
		/// NORMAL TURN
		///--------------
		if (CurrentPlayer == 0)
		{
			TurnText->SetText(FText::FromString(TEXT("YOUR TURN")));
			TurnText->SetColorAndOpacity(FSlateColor(FLinearColor(0.f, 1.f, 1.f))); 
		}
		else
		{
			TurnText->SetText(FText::FromString(TEXT("AI TURN")));
			TurnText->SetColorAndOpacity(FSlateColor(FLinearColor::Black)); 
		}
	}

	///---------
	/// PHASE
	///----------
	if (PhaseText)
	{
		FString PhaseStr = (CurrentPhase == EGamePhase::Placement)
			? TEXT(" Placement Phase")
			: TEXT(" Battle Phase");

		PhaseText->SetText(FText::FromString(PhaseStr));
	}

	///---------------
	/// COIN TOSS
	///--------------
	if (CoinText)
	{
		if (FirstPlayer == 0)
		{
			CoinText->SetText(FText::FromString(TEXT(" Coin toss: HEADS -> Human starts")));
			CoinText->SetColorAndOpacity(FSlateColor(FLinearColor(0.f, 1.f, 1.f))); 
		}
		else
		{
			CoinText->SetText(FText::FromString(TEXT(" Coin toss: TAILS -> AI starts")));
			CoinText->SetColorAndOpacity(FSlateColor(FLinearColor::Black)); 
		}
	}

	///----------
	/// TOWERS
	///----------
	if (TowerText)
	{
		int TowerCount[2] = { 0, 0 };

		for (const FTowerData& Tower : Towers)
		{
			if (Tower.State == ETowerState::Controlled)
			{
				if (Tower.OwnerPlayer == 0 || Tower.OwnerPlayer == 1)
				{
					TowerCount[Tower.OwnerPlayer]++;
				}
			}
		}

		FString TowersStr = FString::Printf(
			TEXT(" Player towers: %d   |   AI towers: %d"),
			TowerCount[0],
			TowerCount[1]
		);

		TowerText->SetText(FText::FromString(TowersStr));
	}

	///---------------
	/// HP UNITS
	///---------------
	if (HPText)
	{
		FString HPString = TEXT("❤️ UNITS\n");

		for (TActorIterator<ABaseUnit> It(GetWorld()); It; ++It)
		{
			ABaseUnit* Unit = *It;
			if (!Unit) continue;

			FString Type = Unit->IsA(ASniperUnit::StaticClass()) ? "S" : "B";

			if (Unit->OwnerPlayer == 0)
			{
				HPString += FString::Printf(
					TEXT(" P %s → %d HP\n"),
					*Type,
					Unit->CurrentHP
				);
			}
			else
			{
				HPString += FString::Printf(
					TEXT(" AI %s → %d HP\n"),
					*Type,
					Unit->CurrentHP
				);
			}
		}

		HPText->SetText(FText::FromString(HPString));
	}

	///------------
	/// HISTORY
	///------------
	if (HistoryText)
	{
		FString HistoryStr = TEXT(" HISTORY\n");

		int MaxLines = 8;
		int Start = FMath::Max(0, MoveHistory.Num() - MaxLines);

		for (int i = Start; i < MoveHistory.Num(); i++)
		{
			HistoryStr += MoveHistory[i] + "\n";
		}

		HistoryText->SetText(FText::FromString(HistoryStr));
	}
}

/*//////////
* SET COLORS
*///////////
void AGridManager::ApplyTileColors()
{
	for (auto& Elem : GridMap)
	{
		const FTileData& Tile = Elem.Value;

		if (Tile.InstanceIndex < 0)
			continue;

		GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 0, Tile.BaseColor.R, false);
		GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 1, Tile.BaseColor.G, false);
		GridMeshVisual->SetCustomDataValue(Tile.InstanceIndex, 2, Tile.BaseColor.B, false);
	}

	GridMeshVisual->MarkRenderStateDirty();
}

/*

*/
void AGridManager::StartGame(int32 NewGridSize, float NewNoise)
{  
	// parameters receive from menu
	UE_LOG(LogTemp, Warning, TEXT(">>> NewGridSize = %d"), NewGridSize);
	UE_LOG(LogTemp, Warning, TEXT(">>> NewNoise = %f"), NewNoise);
	bGameStarted = true;
	
	GridSize = NewGridSize;
	NoiseScale = NewNoise;

	// remove startmenu
	if (IsValid(StartMenuInstance))
	{
		StartMenuInstance->RemoveFromParent();
		StartMenuInstance = nullptr;
	}


	APlayerController* PC = GetWorld()->GetFirstPlayerController();

	if (PC)
	{
		PC->bShowMouseCursor = true; // mouse

		FInputModeGameAndUI InputMode; 
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);

		PC->SetInputMode(InputMode);
	}

	// start the game
	Seed = FMath::RandRange(0, 1000000);
	FirstPlayer = FMath::RandRange(0, 1);
	CurrentPlayer = FirstPlayer;


	GenerateMap();
	ApplyTileColors();
	SpawnTowers();

	// create HUD
	if (WidgetClass)
	{
		WidgetInstance = CreateWidget<UUserWidget>(GetWorld(), WidgetClass);

		if (WidgetInstance)
		{
			WidgetInstance->AddToViewport();
		}
	}
	// set camera 
	
	for (TActorIterator<AStrategicoCameraPawn> It(GetWorld()); It; ++It)
	{
		if (PC)
		{
			PC->SetViewTarget(*It);

			float MapSize = GridSize * TileSpacing;

			FVector Center = GetActorLocation() + FVector(MapSize / 2, MapSize / 2, 0);

			//  position
			It->SetActorLocation(Center + FVector(0, 0, 800));
			It->SetActorRotation(FRotator(-90.f, 0.f, 0.f));

			if (It->Camera)
			{
				///--------------------
				/// SMART ZOOM 
				///---------------------

				float Margin = 1.1f;

				float TargetZoom = MapSize * Margin;

				// limits
				float MinZoom = 800.f;    
				float MaxZoom = 6000.f;   

				TargetZoom = FMath::Clamp(TargetZoom, MinZoom, MaxZoom);

				It->Camera->OrthoWidth = TargetZoom;
			}

		
		}

		break;
	}
	UpdateUI();
}


/*///////////////////////////////
RESET PARAMETERS. MEMOVE HUD...
*///////////////////////////////
void AGridManager::RestartGame()
{
	UE_LOG(LogTemp, Warning, TEXT("=== RESTART ==="));


	SetActorTickEnabled(true);

	bGameStarted = false;
	bGameOver = false;
	bIsAIPlaying = false;

	///--------------
	/// RESET FLOW
	///---------------
	CurrentPhase = EGamePhase::Placement;
	CurrentPlayer = 0;
	FirstPlayer = 0;

	UnitsPlaced = 0;

	UnitsPlayedThisTurn[0] = 0;
	UnitsPlayedThisTurn[1] = 0;

	ConsecutiveTowerControl[0] = 0;
	ConsecutiveTowerControl[1] = 0;

	MoveHistory.Empty();

	///--------------------
	/// RESET SELECTION
	///--------------------
	SelectedUnit = nullptr;
	SelectedUnitGridPos = FIntPoint(-1, -1);
	CurrentRangeTiles.Empty();

	///-----------
	/// CLEAN MAP
	///-----------
	FlushPersistentDebugLines(GetWorld());

	GridMeshVisual->ClearInstances();
	GridMap.Empty();
	Towers.Empty();
	TowerPositions.Empty();

	///-----------------
	/// DESTROY UNITS
	///-----------------
	for (TActorIterator<ABaseUnit> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			It->Destroy();
		}
	}

	///--------------
	/// REMOVE HUD
	///--------------
	if (WidgetInstance)
	{
		WidgetInstance->RemoveFromParent();
		WidgetInstance = nullptr;
	}

	///------------
	/// INPUT MENU
	///------------
	APlayerController* PC = GetWorld()->GetFirstPlayerController();

	if (PC)
	{
		PC->bShowMouseCursor = true;

		FInputModeUIOnly InputMode;
		PC->SetInputMode(InputMode);
	}

	///-----------------
	/// RECREATE MENU
	///-----------------
	if (StartMenuClass)
	{
		StartMenuInstance = CreateWidget<UWBP_StartMenu>(GetWorld(), StartMenuClass);

		if (StartMenuInstance)
		{
			StartMenuInstance->GridManagerRef2 = this;
			StartMenuInstance->AddToViewport();
		}
	}
}

/*///////////////////////////////////////////////////////////////////////////
//   PLAY NEXT AI UNIT : EXECUTES THE AUTOMATIC TACTICAL MOVE FOR THE AI TURN
*////////////////////////////////////////////////////////////////////////////
void AGridManager::PlayNextAIUnit()
{
	// max 2 units
	if (CurrentAIIndex >= AIUnitsToPlay.Num() || CurrentAIIndex >= 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("=== AI TURN END ==="));

		CurrentPlayer = 0;
		ResetUnitsForNewTurn();
		bIsAIPlaying = false;

		return;
	}

	ABaseUnit* Unit = AIUnitsToPlay[CurrentAIIndex];

	UE_LOG(LogTemp, Warning, TEXT("AI playing unit %d"), CurrentAIIndex);

	GetWorld()->GetTimerManager().SetTimer(
		AITimerHandle,
		[this, Unit]()
		{
			///---------------------
			/// SÉLECTION
			///---------------------

			SelectedUnit = Unit;

			bool bFound = false;

			for (auto& Elem : GridMap)
			{
				if (Elem.Value.OccupyingUnit == Unit)
				{
					SelectedUnitGridPos = Elem.Key;
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				SelectedUnit = nullptr;
				return;
			}

			// color
			SafeSetTileColor(GridMap[SelectedUnitGridPos], 1.f, 0.f, 1.f);

			///------------
			/// MOVE RANGE
			///------------
			ShowMovementRange();

			///-------------------------
			/// DELAY → ATTACK RANGE
			///-------------------------
			GetWorld()->GetTimerManager().SetTimer(
				AITimerHandle,
				[this, Unit]()
				{
					HideMovementRange();
					ShowAttackRange();

					// =========================
					// DELAY → ACTION
					// =========================
					GetWorld()->GetTimerManager().SetTimer(
						AITimerHandle,
						[this, Unit]()
						{
							// IA
							PlayAIUnit(Unit);

							// cleanup
							HideMovementRange();

							SelectedUnit = nullptr;
							SelectedUnitGridPos = FIntPoint(-1, -1);

							///----------
							// NEXT UNIT
							///----------
							CurrentAIIndex++;
							PlayNextAIUnit();

						},
						1.5f, // time before action
						false
					);

				},
				1.5f, // move range timing
				false
			);

		},
		0.5f, // small initial delay
		false
	);
}

/*/////////////////////////////////////////////////////////////////////////////
FIND BEST TARGET : EVALUATES THE GRID TO IDENTIFY THE OPTIMAL ATTACK POSITION
*//////////////////////////////////////////////////////////////////////////////
FIntPoint AGridManager::FindBestTarget(ABaseUnit* Unit)
{
	FIntPoint BestPos = SelectedUnitGridPos;

	int BestScore = -9999;

	///--------------------
	// priority to towers
	///--------------------
	for (const FTowerData& Tower : Towers)
	{
		
		if (Tower.OwnerPlayer == 1)
			continue;

		int Dist = Heuristic(SelectedUnitGridPos, Tower.Position);

		int Score = 100 - Dist * 5; // high priority

		// high bonus if near
		if (Dist <= 3)
		{
			Score += 50;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestPos = Tower.Position;
		}
	}

	///----------------------------
	/// ENNEMIEs (SECONDLY)
	///---------------------------
	for (auto& Elem : GridMap)
	{
		ABaseUnit* Other = Elem.Value.OccupyingUnit;

		if (!Other || Other->OwnerPlayer != 0)
			continue;

		int Dist = Heuristic(SelectedUnitGridPos, Elem.Key);

		int Score = 50 - Dist * 3;

		// BONUS if weak ennemy
		if (Other->CurrentHP < 10)
		{
			Score += 20;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestPos = Elem.Key;
		}
	}

	return BestPos;
}

/*/////////////////////////////////////////////////////////////
// AI TURN:FOR ANY UNIT, TRY ATTACK, MOVE TO THE NEAREST ENEMY
*//////////////////////////////////////////////////////////////

void AGridManager::PlayAITurn()
{
	UE_LOG(LogTemp, Warning, TEXT("=== AI TURN START ==="));

	AIUnitsToPlay.Empty();
	CurrentAIIndex = 0;

	// pick AI unit
	for (TActorIterator<ABaseUnit> It(GetWorld()); It; ++It)
	{
		if (It->OwnerPlayer == 1 && !It->bTurnCompleted)
		{
			AIUnitsToPlay.Add(*It);
		}
	}

	if (AIUnitsToPlay.Num() == 0)
	{
		CurrentPlayer = 0;
		ResetUnitsForNewTurn();
		bIsAIPlaying = false;
		return;
	}

	PlayNextAIUnit();
}