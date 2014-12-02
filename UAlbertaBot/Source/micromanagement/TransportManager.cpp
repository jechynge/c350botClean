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
		//conditions for grabbing possible cargo
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Reaver && !unit->isLoaded())
		{
			waitingReavers.push_back(unit);
		}
	}

	// For each transportUnit (shuttle)
	BOOST_FOREACH(BWAPI::Unit * transportUnit, transportUnits)
	{
		// If the shuttle is low on health and carrying a payload, jettison any cargo.
		if ((transportUnit->getHitPoints() + transportUnit->getShields()) < MIN_SAFE_HEALTH)
		{
			BOOST_FOREACH(BWAPI::Unit* reaver, transportUnit->getLoadedUnits())
			{
				transportUnit->unload(reaver);
			}

			// Attempt to GTFO to let shields recharge
			smartMove(transportUnit, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));

			// This shuttle isn't viable until its shields recharge. continue
			continue;
		}

		// and we're carrying any reavers
		if (transportUnit->getLoadedUnits().size() > 0)
		{
			// if we're in position
			if (transportUnit->getDistance(order.position) < order.radius)
			{
				// drop reavers so they can attack
				BOOST_FOREACH(BWAPI::Unit * reaver, transportUnit->getLoadedUnits())
				{
					transportUnit->unload(reaver);
				}
			}
			else
			{
				smartMove(transportUnit, order.position);
			}
		}
		else if (waitingReavers.empty())
		{
			smartMove(transportUnit, order.position);
		}
		else
		{
			BWAPI::Unit * closestReaver = closestCarryUnit(transportUnit, waitingReavers);

			transportUnit->load(closestReaver, true);
		}

		//// if we're in position
		//if (transportUnit->getDistance(order.position) < order.radius)
		//{
		//	// and we're carrying any reavers
		//	if (transportUnit->getLoadedUnits().size() > 0)
		//	{
		//		

		//		// unloadAll doesn't work properly, so drop everything explicitly
		//		BOOST_FOREACH(BWAPI::Unit* reaver, loadedUnits)
		//		{
		//			transportUnit->load(reaver, true);
		//		}
		//	}
		//	else
		//	{
		//		BWAPI::Unit * closestReaver = closestCarryUnit(transportUnit, waitingReavers);

		//		transportUnit->stop();

		//		transportUnit->load(closestReaver, true);
		//	}
		//}
		//else
		//{
		//	// if we're not in position and we've got a full house, move to target
		//	if (!transportUnit->getLoadedUnits().empty())
		//	{
		//		smartMove(transportUnit, order.position);
		//	}
		//	// if there are reavers waiting to be loaded, pick them up
		//	else if (waitingReavers.size() > 0)
		//	{
		//		BWAPI::Unit * closestReaver = closestCarryUnit(transportUnit, waitingReavers);

		//		transportUnit->stop();

		//		transportUnit->load(closestReaver, true);
		//	}
		//}
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