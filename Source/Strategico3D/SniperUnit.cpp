// copyright 2026 by danny tonlio - all rights reserved
// SniperUnit.cpp - Implementation of the Sniper unit, a long-range unit with low HP and high damage

#include "SniperUnit.h"

ASniperUnit::ASniperUnit()
{
	// set default values for the Sniper unit

	Health = 20;

	MoveRange = 4;

	AttackRange = 10;

	MinDamage = 4;

	MaxDamage = 8;
}