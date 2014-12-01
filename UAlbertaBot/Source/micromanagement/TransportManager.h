#pragma once;

#include <Common.h>
#include "MicroManager.h"

class MicroManager;

class TransportManager : public MicroManager
{
	const static int MIN_SAFE_HEALTH = 50;

	BWAPI::Unit * closestCarryUnit(BWAPI::Unit * transportUnit, UnitVector & rangedUnits);

public:

	TransportManager();
	~TransportManager() {}

	void executeMicro(const UnitVector & targets);
};
