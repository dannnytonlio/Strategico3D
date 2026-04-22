// copyright 2026 by danny tonlio - all rights reserved
// StrategicoGameMode.cpp - Game mode class for Strategico, managing game rules and state


#include "StrategicoGameMode.h"
#include "StrategicoCameraPawn.h"
#include "Engine/Engine.h"
#include "GridManager.h"

AStrategicoGameMode::AStrategicoGameMode()
{
	// Set this game mode to be the default pawn class to our camera pawn, which will be used for the player controller
	DefaultPawnClass = AStrategicoCameraPawn::StaticClass();

	
}