#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "BaseUnit.generated.h"

UCLASS()
class STRATEGICO3D_API ABaseUnit : public AActor
{
	GENERATED_BODY()

public:
	ABaseUnit();
	void ResetTurn();
	void Respawn();

protected:
	virtual void BeginPlay() override;

public:

	//Mesh
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* UnitMesh;

	// HP max 
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int Health;

	// HP 
	UPROPERTY(VisibleAnywhere)
	int CurrentHP;

	// Attack 
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int AttackRange;


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int MinDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int MaxDamage;


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int MoveRange;

	UPROPERTY()
	FIntPoint InitialGridPosition;
	// color
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor UnitColor;

	UPROPERTY()
	int OwnerPlayer;


	UPROPERTY()
	bool bHasMoved = false;

	UPROPERTY()
	bool bHasAttacked = false;

	UPROPERTY()
	bool bTurnCompleted = false;


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int MaxHP;

	void UpdateColor();
	void SetTeamColor(int Player);
private:

	UMaterialInstanceDynamic* DynamicMaterial;
   // initial position for respawn
	FVector SpawnLocation;
};