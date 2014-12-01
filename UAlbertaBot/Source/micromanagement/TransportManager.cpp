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
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Reaver && unit->isCompleted() && unit->exists() && !unit->isLoaded())
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
			transportUnit->unloadAll(false);

			// Attempt to GTFO to let shields recharge
			smartMove(transportUnit, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));

			// This shuttle isn't viable until its shields recharge. continue
			continue;
		}

		// if have reavers, or not building anymore reavers
		if (transportUnit->getLoadedUnits().size() == 2)
		{
			if (order.type == order.HarassWorkers)
			{
				// if we're not within range of our designated target
				if (transportUnit->getDistance(order.position) > order.radius)
				{
					smartMove(transportUnit, order.position);
				}
				else
				{
					// drop reavers so they can attack
					transportUnit->unloadAll(true);
				}
			}
		}
		// if there are reavers waiting to be loaded, pick them up
		else if (waitingReavers.size() > 0)
		{
			BWAPI::Unit * closestReaver = closestCarryUnit(transportUnit, waitingReavers);

			transportUnit->stop();

			transportUnit->load(closestReaver, true);
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

BWAPI::Position TransportManager::getDropSite(BWAPI::Unit * transportUnit)
{
	BWAPI::Unit * closestMinerals = NULL;
	double closestDist = 100000;
	BWTA::BaseLocation* enemyBase = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
	std::set<BWAPI::Unit*> minerals = enemyBase->getMinerals();

	BOOST_FOREACH(BWAPI::Unit * mineral, minerals)
	{
		if (!mineral->getPosition().isValid())
		{
			continue;
		}

		double dist = mineral->getDistance(transportUnit);

		if (!closestMinerals || (dist < closestDist))
		{
			closestMinerals = mineral;
			closestDist = dist;
		}
	}

	int xpos = -1, ypos = -1;

	if (closestMinerals)
	{
		int dx = closestMinerals->getPosition().x() - transportUnit->getPosition().x();

		if (dx < 0)
		{
			xpos = closestMinerals->getPosition().x() - 30;
		}
		else
		{
			xpos = closestMinerals->getPosition().x() + 30;
		}

		int dy = closestMinerals->getPosition().y() - transportUnit->getPosition().y();

		if (dx < 0)
		{
			ypos = closestMinerals->getPosition().y() - 30;
		}
		else
		{
			ypos = closestMinerals->getPosition().y() + 30;
		}
	}

	return BWAPI::Position(xpos, ypos);
}
