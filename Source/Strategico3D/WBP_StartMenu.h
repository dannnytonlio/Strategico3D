#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WBP_StartMenu.generated.h"

class AGridManager;

UCLASS()
class STRATEGICO3D_API UWBP_StartMenu : public UUserWidget
{
    GENERATED_BODY()

public:

    UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true))
    class AGridManager* GridManagerRef2;
};