#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridManager.generated.h"


UENUM(BlueprintType)
enum class ETowerState : uint8
{
    Neutral,    
    Controlled, 
    Contested   
};

/** * FTowerData: Core data structure for managing tower logic and persistence
 */
USTRUCT(BlueprintType)
struct FTowerData
{
    GENERATED_BODY()

    UPROPERTY()
    FIntPoint Position; // Grid coordinates of the tower

    UPROPERTY()
    ETowerState State = ETowerState::Neutral; // Current occupancy state

    UPROPERTY()
    int32 OwnerPlayer = -1; // ID of the player controlling the tower (-1 = None)
};

/*//////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS (Optimizes compilation time)
//////////////////////////////////////////////////////////////////////////////*/
class UInstancedStaticMeshComponent;
class ABaseUnit;
class UUserWidget;
class UWBP_StartMenu;

// FTileData: Information for each individual cell in the tactical grid

USTRUCT(BlueprintType)
struct FTileData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString CoordName; // Chess-style name 

    UPROPERTY(BlueprintReadOnly)
    int32 Elevation = 0; // Terrain height (used for movement cost calculations)

    UPROPERTY(BlueprintReadOnly)
    bool bIsWalkable = true; // Flag to determine if units can enter this tile

    UPROPERTY(BlueprintReadOnly)
    FVector WorldPosition = FVector::ZeroVector; // 3D coordinates in the game world

    int32 InstanceIndex = -1; // Technical index for the Instanced Static Mesh

    FLinearColor BaseColor; // The original color assigned during grid generation

    UPROPERTY()
    ABaseUnit* OccupyingUnit = nullptr; // Reference to the unit currently on this tile
};

/** * EGamePhase: Manages the high-level flow of the match
 */
UENUM()
enum class EGamePhase : uint8
{
    Placement, 
    Gameplay   
};

/** * AGridManager: The central engine of Strategico3D.
 * Handles grid generation, AI logic, turn management, and game rules.
 */
UCLASS(Blueprintable)
class STRATEGICO3D_API AGridManager : public AActor
{
    GENERATED_BODY()

public:

    // Default Constructor 
    AGridManager();

    // Frame-by-frame update loop 
    virtual void Tick(float DeltaTime) override;

 
    UFUNCTION(BlueprintCallable)
    void StartGame(int32 NewGridSize, float NewNoise);

    UFUNCTION(BlueprintCallable)
    void RestartGame();

protected:
   
    virtual void BeginPlay() override;

    // Visual component managing thousands of grid tiles efficiently 
    UPROPERTY(VisibleAnywhere)
    UInstancedStaticMeshComponent* GridMeshVisual;

    //Grid configuration parameters 
    UPROPERTY(EditAnywhere)
    int32 GridSize = 25;

    UPROPERTY(EditAnywhere)
    float TileSpacing = 100.f;

    UPROPERTY(EditAnywhere)
    float NoiseScale = 0.05f; // Frequency for Perlin Noise elevation

    UPROPERTY(EditAnywhere)
    int32 Seed = 123; // Random seed for reproducible map generation

    //The main data container: Map linking coordinates to tile properties
    UPROPERTY()
    TMap<FIntPoint, FTileData> GridMap;

    /* Array of static tower locations for persistence during UI flushes */
    UPROPERTY()
    TArray<FIntPoint> TowerPositions;

   
    EGamePhase CurrentPhase = EGamePhase::Placement;
    int CurrentPlayer = 0;
    int UnitsPlaced = 0;
    int MaxUnitsPerPlayer = 2;
    int FirstPlayer = 0; // Stores the result of the initial "Coin Toss"

 
    ABaseUnit* SelectedUnit = nullptr;
    int UnitsPlayedThisTurn[2] = { 0, 0 }; // Counters for Player (0) and AI (1)

    void EndUnitTurn(ABaseUnit* Unit);

    //Selection tracking in grid space 
    FIntPoint SelectedUnitGridPos = FIntPoint(-1, -1);

    /*/////----------------------
    // CORE CORE LOGIC FUNCTIONS
    *///--------------------------

    void GenerateMap(); 
    void SpawnTowers(); 

    //Updates tile visuals without breaking instance integrity 
    void SafeSetTileColor(const FTileData& Tile, float R, float G, float B);
    void RedrawTowers();

    // Input handling 
    void HandleMouseClick();
    void TryPlaceUnit(FIntPoint Pos);
    void SelectUnit(FIntPoint Pos);

    // Tactical Range Visuals 
    void ShowMovementRange();
    void ShowAttackRange();
    void HideMovementRange();

    //Combat and Navigation 
    bool TryAttack(FIntPoint TargetPos);
    void UpdateTowers(); 
    FIntPoint FindNearestWalkableTile(FIntPoint TargetPos);
    FString GetColLetter(int32 Index); 

   
    void ResetUnitsForNewTurn();

    // AI Logic Suite 
    void AIPlaceUnit(); 
    void PlayAITurn(); 
    void PlayAIUnit(ABaseUnit* Unit); 
    FIntPoint FindBestTarget(ABaseUnit* Unit); 
    bool bIsAIPlaying = false; 

    int ConsecutiveTowerControl[2] = { 0, 0 }; 
    void CheckVictoryCondition();

    void LogMove(ABaseUnit* Unit, FIntPoint From, FIntPoint To);
    void LogAttack(ABaseUnit* Unit, FIntPoint TargetPos, int Damage);

    // UI Integration
    void UpdateUI(); 
    UPROPERTY(EditAnywhere)
    TSubclassOf<UUserWidget> WidgetClass; // Class reference for the HUD
    UUserWidget* WidgetInstance;

    UPROPERTY()
    TArray<FString> MoveHistory; 

    void ApplyTileColors(); 

    bool bGameOver = false;
    int WinnerPlayer = -1;

    //Start Menu Management
    UPROPERTY(EditAnywhere)
    TSubclassOf<UWBP_StartMenu> StartMenuClass;
    UWBP_StartMenu* StartMenuInstance;
    bool bGameStarted = false;

    // AI Sequencing (Delayed actions for better UX)
    TArray<ABaseUnit*> AIUnitsToPlay;
    int32 CurrentAIIndex = 0;
    FTimerHandle AITimerHandle;
    void PlayNextAIUnit();

    // Advanced Pathfinding Calculations (Dijkstra/A*)
    TMap<FIntPoint, int> GetReachableTiles(ABaseUnit* Unit);

    TArray<FIntPoint> CurrentRangeTiles;
    TArray<FTowerData> Towers;

    // A* Search Algorithm node structure
    struct FPathNode
    {
        FIntPoint Position;
        int GCost; // Movement cost from start
        int HCost; // Estimated cost to target (Heuristic)
        int FCost() const { return GCost + HCost; } // Total weight
    };

    int Heuristic(FIntPoint A, FIntPoint B); 
    TArray<FIntPoint> FindPathAStar(FIntPoint Start, FIntPoint Goal); // Optimal pathfinding
};