#include "Common.h"
#include "TransportManager.h"

TransportManager::TransportManager()  { }

void TransportManager::executeMicro(const UnitVector & targets)
{
	const UnitVector & transportUnits = getUnits();

	if (transportUnits.empty()) 
	{
		return;
	}

	UnitVector waitingReavers;

	// find local reaver units
	BOOST_FOREACH(BWAPI::Unit * unit, BWAPI::Broodwar->self()->getUnits())
	{
		// only consider reavers that aren't in a transport
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Reaver && !unit->isLoaded())
		{
			waitingReavers.push_back(unit);
		}
	}

	BWAPI::Position enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy())->getPosition();

	BOOST_FOREACH(BWAPI::Unit * transportUnit, transportUnits)
	{
		// if the shuttle is low on health and carrying a payload, jettison any cargo.
		if ((transportUnit->getHitPoints() + transportUnit->getShields()) < MIN_SAFE_HEALTH)
		{
			BOOST_FOREACH(BWAPI::Unit* reaver, transportUnit->getLoadedUnits())
			{
				transportUnit->unload(reaver);
			}

			// attempt to GTFO to let shields recharge
			smartMove(transportUnit, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));

			// this shuttle isn't viable until its shields recharge
			continue;
		}

		// if we're carrying any reavers
		if (transportUnit->getLoadedUnits().size() > 0)
		{
			// and if we're in position
			if (transportUnit->getDistance(order.position) < order.radius)
			{
				// drop reavers so they can attack
				BOOST_FOREACH(BWAPI::Unit * reaver, transportUnit->getLoadedUnits())
				{
					transportUnit->unload(reaver);
				}
			}
			// otherwise, get in position
			else
			{
				smartMove(transportUnit, order.position);
			}
		}
		// if there are no free reavers, go back to home base
		else if (waitingReavers.empty())
		{
			smartMove(transportUnit, order.position);
		}
		// otherwise, if there are reavers around, find the nearest one
		else
		{
			BWAPI::Unit * closestReaver = closestCarryUnit(transportUnit, waitingReavers);

			// pick it up as long as it's weapon is cooling down (it's fired its weapon) or it's not in the enemy base
			if (closestReaver->getGroundWeaponCooldown() || closestReaver->getDistance(enemyBaseLocation) > 400)
			{
				transportUnit->load(closestReaver, true);
			}
		}
	}
}

BWAPI::Unit * TransportManager::closestCarryUnit(BWAPI::Unit * transportUnit, UnitVector & rangedUnits)
{
	BWAPI::Unit * closestGround = NULL;
	double closestDist = 100000;

	BOOST_FOREACH(BWAPI::Unit * unit, rangedUnits)
	{
		double dist = unit->getDistance(transportUnit);

		if (!closestGround || (dist < closestDist))
		{
			closestGround = unit;
			closestDist = dist;
		}
	}

	return closestGround;
}