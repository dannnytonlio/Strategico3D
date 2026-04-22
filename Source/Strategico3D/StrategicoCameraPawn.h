// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"   
#include "Camera/CameraComponent.h"

#include "StrategicoCameraPawn.generated.h"

class UCameraComponent;

UCLASS()
class STRATEGICO3D_API AStrategicoCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AStrategicoCameraPawn();
	// camera
	UPROPERTY(VisibleAnywhere)
	USpringArmComponent* SpringArm;
	// camera component
	UPROPERTY()
	UCameraComponent* Camera;
protected:
	virtual void BeginPlay() override;

private:

	UPROPERTY()
	USceneComponent* Root;
	

};