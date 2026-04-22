// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WBP_GameHUD.generated.h"

class AGridManager;

UCLASS(Blueprintable)
class STRATEGICO3D_API UWBP_GameHUD : public UUserWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true"))
	AGridManager* GridManagerRef;
};