#include "Common.h"
#include "CombatCommander.h"

CombatCommander::CombatCommander() 
	: attacking(false)
	, foundEnemy(false)
	, attackSent(false) 
{
	
}

bool CombatCommander::squadUpdateFrame()
{
	return BWAPI::Broodwar->getFrameCount() % 24 == 0;
}

void CombatCommander::update(std::set<BWAPI::Unit *> unitsToAssign)
{
	if(squadUpdateFrame())
	{
		// clear all squad data
		squadData.clearSquadData();

		// give back combat workers to worker manager
		WorkerManager::Instance().finishedWithCombatWorkers();
        
		// Assign defense and attack squads
		assignDropSquads(unitsToAssign);
        assignScoutDefenseSquads();
		assignDefenseSquads(unitsToAssign);
		assignAttackSquads(unitsToAssign);
		assignIdleSquads(unitsToAssign);
	}

	squadData.update();
}

void CombatCommander::assignDropSquads(std::set<BWAPI::Unit *> & unitsToAssign)
{
	if (unitsToAssign.empty()) { return; }

	UnitVector dropUnits, reavers;

	BWAPI::Unit * shuttle = NULL, * closestReaver = NULL, * roboticsBay = NULL;

	BWAPI::Position enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy())->getPosition();
	BWAPI::Position ourBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getPosition();

	// remove the reavers and shuttles from the set so they don't get double-assigned.
	// They're special snowflakes with a specific purpose
	BOOST_FOREACH(BWAPI::Unit* unit, unitsToAssign)
	{
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Reaver)
		{
			if (unit->getScarabCount() < 5 && !unit->isTraining())
			{
				unit->train(BWAPI::UnitTypes::Protoss_Scarab);
				unit->train(BWAPI::UnitTypes::Protoss_Scarab);
			}

			reavers.push_back(unit);
		}

		if (unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle)
		{
			shuttle = unit;
		}
	}

	int distanceToShuttle = 100000;

	// if we have a shuttle
	if (shuttle)
	{
		// assign the shuttle to our drop unit squad
		dropUnits.push_back(shuttle);
		unitsToAssign.erase(shuttle);

		// find the reaver closest to our shuttle
		BOOST_FOREACH(BWAPI::Unit * reaver, reavers)
		{
			int dist = shuttle->getDistance(reaver);

			if (!closestReaver || dist < distanceToShuttle)
			{
				closestReaver = reaver;
			}
		}

		// if there is one, assign it to this squad
		if (closestReaver)
		{
			unitsToAssign.erase(closestReaver);
			dropUnits.push_back(closestReaver);
		}

		// if we only have a shuttle, have it hang out at the base and wait for a reaver
		if (dropUnits.size() == 1)
		{
			BWAPI::Position orderTarget = ourBaseLocation;
			int orderRadius = 100;

			squadData.addSquad(Squad(dropUnits, SquadOrder(SquadOrder::HarassWorkers, orderTarget, orderRadius, "More Reavers!")));
		}
		// if we have a reaver, then set our target to the enemy base
		else if (dropUnits.size() > 1)
		{
			BWAPI::Position orderTarget = enemyBaseLocation;
			int orderRadius = 100;

			// if we're close to the enemy base, refine the position to their worker lines instead
			if (shuttle->getDistance(orderTarget) < orderRadius)
			{
				orderTarget = getDropSite(shuttle);
				orderRadius = 30;
			}

			squadData.addSquad(Squad(dropUnits, SquadOrder(SquadOrder::HarassWorkers, orderTarget, orderRadius, "Harass Workers!")));
		}
	}
}

void CombatCommander::assignIdleSquads(std::set<BWAPI::Unit *> & unitsToAssign)
{
	if (unitsToAssign.empty()) { return; }

	UnitVector combatUnits(unitsToAssign.begin(), unitsToAssign.end());
	unitsToAssign.clear();

	squadData.addSquad(Squad(combatUnits, SquadOrder(SquadOrder::Defend, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), 1000, "Defend Idle")));
}

void CombatCommander::assignAttackSquads(std::set<BWAPI::Unit *> & unitsToAssign)
{
	if (unitsToAssign.empty()) { return; }

	bool workersDefending = false;
	BOOST_FOREACH (BWAPI::Unit * unit, unitsToAssign)
	{
		if (unit->getType().isWorker())
		{
			workersDefending = true;
		}
	}

	// do we have workers in combat
	bool attackEnemy = !unitsToAssign.empty() && !workersDefending && StrategyManager::Instance().doAttack(unitsToAssign);

	// if we are attacking, what area are we attacking?
	if (attackEnemy) 
	{	
		assignAttackRegion(unitsToAssign);				// attack occupied enemy region
		assignAttackKnownBuildings(unitsToAssign);		// attack known enemy buildings
		assignAttackVisibleUnits(unitsToAssign);			// attack visible enemy units
		assignAttackExplore(unitsToAssign);				// attack and explore for unknown units
	} 
}

BWTA::Region * CombatCommander::getClosestEnemyRegion()
{
	BWTA::Region * closestEnemyRegion = NULL;
	double closestDistance = 100000;

	// for each region that our opponent occupies
	BOOST_FOREACH (BWTA::Region * region, InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->enemy()))
	{
		double distance = region->getCenter().getDistance(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));

		if (!closestEnemyRegion || distance < closestDistance)
		{
			closestDistance = distance;
			closestEnemyRegion = region;
		}
	}

	return closestEnemyRegion;
}

void CombatCommander::assignScoutDefenseSquads() 
{
	// for each of our occupied regions
	BOOST_FOREACH(BWTA::Region * myRegion, InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->self()))
	{
		BWAPI::Position regionCenter = myRegion->getCenter();
		if (!regionCenter.isValid())
		{
			continue;
		}

		// all of the enemy units in this region
		std::set<BWAPI::Unit *> enemyUnitsInRegion;
		BOOST_FOREACH (BWAPI::Unit * enemyUnit, BWAPI::Broodwar->enemy()->getUnits())
		{			
			if (BWTA::getRegion(BWAPI::TilePosition(enemyUnit->getPosition())) == myRegion)
			{
				enemyUnitsInRegion.insert(enemyUnit);
			}
		}

        // special case: figure out if the only attacker is a worker, the enemy is scouting
        if (enemyUnitsInRegion.size() == 1 && (*enemyUnitsInRegion.begin())->getType().isWorker())
        {
            // the enemy worker that is attacking us
            BWAPI::Unit * enemyWorker       = *enemyUnitsInRegion.begin();

            // get our worker unit that is mining that is closest to it
            BWAPI::Unit * workerDefender    = WorkerManager::Instance().getClosestMineralWorkerTo(enemyWorker);

            // grab it from the worker manager
            WorkerManager::Instance().setCombatWorker(workerDefender);
            
            // put it into a unit vector
            UnitVector workerDefenseForce;
            workerDefenseForce.push_back(workerDefender);

            // make a squad using the worker to defend
            squadData.addSquad(Squad(workerDefenseForce, SquadOrder(SquadOrder::Defend, regionCenter, 1000, "Get That Scout!")));
			return;
        }
	}
}

void CombatCommander::assignDefenseSquads(std::set<BWAPI::Unit *> & unitsToAssign) 
{
	if (unitsToAssign.empty()) { return; }

	// for each of our occupied regions
	BOOST_FOREACH(BWTA::Region * myRegion, InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->self()))
	{
		BWAPI::Position regionCenter = myRegion->getCenter();
		if (!regionCenter.isValid())
		{
			continue;
		}

		// start off assuming all enemy units in region are just workers
		int numDefendersPerEnemyUnit = 1;

		// all of the enemy units in this region
		std::set<BWAPI::Unit *> enemyUnitsInRegion;
		BOOST_FOREACH (BWAPI::Unit * enemyUnit, BWAPI::Broodwar->enemy()->getUnits())
		{			
			if (BWTA::getRegion(BWAPI::TilePosition(enemyUnit->getPosition())) == myRegion)
			{
				enemyUnitsInRegion.insert(enemyUnit);

				// if the enemy isn't a worker, increase the amount of defenders for it
				if (!enemyUnit->getType().isWorker())
				{
					numDefendersPerEnemyUnit = 3;
				}
			}
		}

		// figure out how many units we need on defense
		const int numFlyingNeeded = numDefendersPerEnemyUnit * InformationManager::Instance().numEnemyFlyingUnitsInRegion(myRegion);
		const int numGroundNeeded = numDefendersPerEnemyUnit * InformationManager::Instance().numEnemyUnitsInRegion(myRegion);

		if(numGroundNeeded > 0 || numFlyingNeeded > 0)
		{
			// our defenders
			std::set<BWAPI::Unit *> flyingDefenders;
			std::set<BWAPI::Unit *> groundDefenders;

			BOOST_FOREACH (BWAPI::Unit * unit, unitsToAssign)
			{
				if (unit->getType().airWeapon() != BWAPI::WeaponTypes::None)
				{
					flyingDefenders.insert(unit);
				}
				else if (unit->getType().groundWeapon() != BWAPI::WeaponTypes::None)
				{
					groundDefenders.insert(unit);
				}
			}

			// the defense force we want to send
			UnitVector defenseForce;

			// get flying defenders
			for (int i=0; i<numFlyingNeeded && !flyingDefenders.empty(); ++i)
			{
				BWAPI::Unit * flyingDefender = findClosestDefender(enemyUnitsInRegion, flyingDefenders);
				defenseForce.push_back(flyingDefender);
				unitsToAssign.erase(flyingDefender);
				flyingDefenders.erase(flyingDefender);
			}

			// get ground defenders
			for (int i=0; i<numGroundNeeded && !groundDefenders.empty(); ++i)
			{
				BWAPI::Unit * groundDefender = findClosestDefender(enemyUnitsInRegion, groundDefenders);

				if (groundDefender->getType().isWorker())
				{
					WorkerManager::Instance().setCombatWorker(groundDefender);
				}

				defenseForce.push_back(groundDefender);
				unitsToAssign.erase(groundDefender);
				groundDefenders.erase(groundDefender);
			}

			// if we need a defense force, make the squad and give the order
			if (!defenseForce.empty()) 
			{
				squadData.addSquad(Squad(defenseForce, SquadOrder(SquadOrder::Defend, regionCenter, 1000, "Defend Region")));
				return;
			}
		}
	}
}

void CombatCommander::assignAttackRegion(std::set<BWAPI::Unit *> & unitsToAssign) 
{
	if (unitsToAssign.empty()) { return; }

	BWTA::Region * enemyRegion = getClosestEnemyRegion();

	if (enemyRegion && enemyRegion->getCenter().isValid()) 
	{
		UnitVector oppUnitsInArea, ourUnitsInArea;
		MapGrid::Instance().GetUnits(oppUnitsInArea, enemyRegion->getCenter(), 800, false, true);
		MapGrid::Instance().GetUnits(ourUnitsInArea, enemyRegion->getCenter(), 200, true, false);

		if (!oppUnitsInArea.empty())
		{
			UnitVector combatUnits(unitsToAssign.begin(), unitsToAssign.end());
			unitsToAssign.clear();

			squadData.addSquad(Squad(combatUnits, SquadOrder(SquadOrder::Attack, enemyRegion->getCenter(), 1000, "Attack Region")));
		}
	}
}

void CombatCommander::assignAttackVisibleUnits(std::set<BWAPI::Unit *> & unitsToAssign) 
{
	if (unitsToAssign.empty()) { return; }

	BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->isVisible())
		{
			UnitVector combatUnits(unitsToAssign.begin(), unitsToAssign.end());
			unitsToAssign.clear();

			squadData.addSquad(Squad(combatUnits, SquadOrder(SquadOrder::Attack, unit->getPosition(), 1000, "Attack Visible")));

			return;
		}
	}
}

void CombatCommander::assignAttackKnownBuildings(std::set<BWAPI::Unit *> & unitsToAssign) 
{
	if (unitsToAssign.empty()) { return; }

	FOR_EACH_UIMAP_CONST (iter, InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
	{
		const UnitInfo ui(iter->second);
		if(ui.type.isBuilding())
		{
			UnitVector combatUnits(unitsToAssign.begin(), unitsToAssign.end());
			unitsToAssign.clear();

			squadData.addSquad(Squad(combatUnits, SquadOrder(SquadOrder::Attack, ui.lastPosition, 1000, "Attack Known")));
			return;	
		}
	}
}

void CombatCommander::assignAttackExplore(std::set<BWAPI::Unit *> & unitsToAssign) 
{
	if (unitsToAssign.empty()) { return; }

	UnitVector combatUnits(unitsToAssign.begin(), unitsToAssign.end());
	unitsToAssign.clear();

	squadData.addSquad(Squad(combatUnits, SquadOrder(SquadOrder::Attack, MapGrid::Instance().getLeastExplored(), 1000, "Attack Explore")));
}

BWAPI::Unit* CombatCommander::findClosestDefender(std::set<BWAPI::Unit *> & enemyUnitsInRegion, const std::set<BWAPI::Unit *> & units) 
{
	BWAPI::Unit * closestUnit = NULL;
	double minDistance = 1000000;

	BOOST_FOREACH (BWAPI::Unit * enemyUnit, enemyUnitsInRegion) 
	{
		BOOST_FOREACH (BWAPI::Unit * unit, units)
		{
			double dist = unit->getDistance(enemyUnit);
			if (!closestUnit || dist < minDistance) 
			{
				closestUnit = unit;
				minDistance = dist;
			}
		}
	}

	return closestUnit;
}

BWAPI::Position CombatCommander::getDefendLocation()
{
	return BWTA::getRegion(BWTA::getStartLocation(BWAPI::Broodwar->self())->getTilePosition())->getCenter();
}

void CombatCommander::drawSquadInformation(int x, int y)
{
	squadData.drawSquadInformation(x, y);
}

BWAPI::Position CombatCommander::getDropSite(BWAPI::Unit * transportUnit)
{
	BWAPI::Unit * closestMinerals = NULL;
	double closestDist = 100000;
	BWTA::BaseLocation* enemyBase = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
	std::set<BWAPI::Unit*> minerals = enemyBase->getMinerals();

	// see if we can find the enemy's mineral location, and get the one closest to their base
	BOOST_FOREACH(BWAPI::Unit * mineral, minerals)
	{
		if (!mineral->getPosition().isValid())
		{
			continue;
		}

		double dist = mineral->getDistance(enemyBase->getPosition());

		if (!closestMinerals || (dist < closestDist))
		{
			closestMinerals = mineral;
			closestDist = dist;
		}
	}

	int xpos = enemyBase->getPosition().x(), ypos = enemyBase->getPosition().y(), diff = 120;

	// attempt to drop between their base and the mineral lines
	if (closestMinerals)
	{
		// determine where the mineral line is in relation to the base
		int dx = closestMinerals->getPosition().x() - enemyBase->getPosition().x();
		int dy = closestMinerals->getPosition().y() - enemyBase->getPosition().y();

		if (abs(dx) > abs(dy))
		{
			// west
			if (dx < 0)
			{
				xpos = enemyBase->getPosition().x() - diff;
			}
			// east
			else
			{
				xpos = enemyBase->getPosition().x() + diff;
			}
		}
		else
		{
			// north
			if (dy < 0)
			{
				ypos = enemyBase->getPosition().y() - diff;
			}
			// south
			else
			{
				ypos = enemyBase->getPosition().y() + diff;
			}
		}
	}

	return BWAPI::Position(xpos, ypos);
}