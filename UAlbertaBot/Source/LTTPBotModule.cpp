/* 
 +----------------------------------------------------------------------+
 | LTTPBotModule - a derivative of UAlbertaBot                          |
 +----------------------------------------------------------------------+
 | University of Alberta - CMPUT 350 StarCraft Competition              |
 +----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------+
 | Author: David Churchill <dave.churchill@gmail.com>                   |
 +----------------------------------------------------------------------+
 | Modifications: Team Late To The Party                                |
 +----------------------------------------------------------------------+
*/

#include "Common.h"
#include "LTTPBotModule.h"


BWAPI::AIModule * __NewAIModule()
{
	return new LTTPBotModule();
}

LTTPBotModule::LTTPBotModule()  {}
LTTPBotModule::~LTTPBotModule() {}

void LTTPBotModule::onStart()
{
	BWAPI::Broodwar->setLocalSpeed(0);
	//BWAPI::Broodwar->setFrameSkip(240);

    SparCraft::init();

	BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
	//BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);

    Options::BotModes::SetBotMode(Options::BotModes::AIIDE_TOURNAMENT);
	Options::Modules::checkOptions();
	
    if (Options::Modules::USING_GAMECOMMANDER)
	{
		BWTA::readMap();
		BWTA::analyze();
	}
	
	if (Options::Modules::USING_MICRO_SEARCH)
	{
		SparCraft::init();
		
		//micro.onStart();
	}

	if (Options::Modules::USING_BUILD_LEARNER)
	{
		BuildOrderSearch::getStarcraftDataInstance().init(BWAPI::Broodwar->self()->getRace());
		SparCraft::Hash::initHash();
	}
}

void LTTPBotModule::onEnd(bool isWinner)
{
	if (Options::Modules::USING_GAMECOMMANDER)
	{
		StrategyManager::Instance().onEnd(isWinner);

		std::stringstream result;
		std::string win = isWinner ? "win" : "lose";

		double sum = 0;
		BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
			{
				sum += sqrt((double)(unit->getHitPoints() + unit->getShields()));
			}
		}
		BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->enemy()->getUnits())
		{
			if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
			{
				sum -= sqrt((double)(unit->getHitPoints() + unit->getShields()));
			}
		}

		//result << "Game against " << BWAPI::Broodwar->enemy()->getName() << " " << win << " with strategy " << StrategyManager::Instance().getCurrentStrategy() << "\n";

		result << sum << " " << BWAPI::Broodwar->getFrameCount() << "\n";

		Logger::Instance().log(result.str());

		ProductionManager::Instance().onGameEnd();
	}	
}

void LTTPBotModule::onFrame()
{
	if (Options::Modules::USING_UNIT_COMMAND_MGR)
	{
		UnitCommandManager::Instance().update();
	}

	if (Options::Modules::USING_GAMECOMMANDER) 
	{ 
		gameCommander.update(); 
	}
	
	if (Options::Modules::USING_ENHANCED_INTERFACE)
	{
		eui.update();
	}

	if (Options::Modules::USING_MICRO_SEARCH)
	{
		//micro.update();
	}


	if (Options::Modules::USING_REPLAY_VISUALIZER)
	{
		BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->getAllUnits())
		{
			BWAPI::Broodwar->drawTextMap(unit->getPosition().x(), unit->getPosition().y(), "   %d", unit->getPlayer()->getID());

			if (unit->isSelected())
			{
				BWAPI::Broodwar->drawCircleMap(unit->getPosition().x(), unit->getPosition().y(), 1000, BWAPI::Colors::Red);
			}
		}
	}
}

void LTTPBotModule::onUnitDestroy(BWAPI::Unit * unit)
{
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitDestroy(unit); }
	if (Options::Modules::USING_ENHANCED_INTERFACE) { eui.onUnitDestroy(unit); }
}

void LTTPBotModule::onUnitMorph(BWAPI::Unit * unit)
{
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitMorph(unit); }
}

void LTTPBotModule::onSendText(std::string text)
{ 
	BWAPI::Broodwar->sendText(text.c_str());

	if (Options::Modules::USING_REPLAY_VISUALIZER && (text.compare("sim") == 0))
	{
		BWAPI::Unit * selected = NULL;
		BOOST_FOREACH (BWAPI::Unit * unit, BWAPI::Broodwar->getAllUnits())
		{
			if (unit->isSelected())
			{
				selected = unit;
				break;
			}
		}

		if (selected)
		{
			#ifdef USING_VISUALIZATION_LIBRARIES
				//ReplayVisualizer rv;
				//rv.launchSimulation(selected->getPosition(), 1000);
			#endif
		}
	}

	if (Options::Modules::USING_BUILD_ORDER_DEMO)
	{

		std::stringstream type;
		std::stringstream numUnitType;
		int numUnits = 0;

		int i=0;
		for (i=0; i<text.length(); ++i)
		{
			if (text[i] == ' ')
			{
				i++;
				break;
			}

			type << text[i];
		}

		for (; i<text.length(); ++i)
		{
			numUnitType << text[i];
		}

		numUnits = atoi(numUnitType.str().c_str());

	
		BWAPI::UnitType t = BWAPI::UnitTypes::getUnitType(type.str());

		BWAPI::Broodwar->printf("Searching for %d of %s", numUnits, t.getName().c_str());

		MetaPairVector goal;
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Probe, 8));
		goal.push_back(MetaPair(BWAPI::UnitTypes::Protoss_Gateway, 2));
		goal.push_back(MetaPair(t, numUnits));

		ProductionManager::Instance().setSearchGoal(goal);
	}
}

void LTTPBotModule::onUnitCreate(BWAPI::Unit * unit)
{ 
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitCreate(unit); }
}

void LTTPBotModule::onUnitShow(BWAPI::Unit * unit)
{ 
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitShow(unit); }
}

void LTTPBotModule::onUnitHide(BWAPI::Unit * unit)
{ 
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitHide(unit); }
}

void LTTPBotModule::onUnitRenegade(BWAPI::Unit * unit)
{ 
	if (Options::Modules::USING_GAMECOMMANDER) { gameCommander.onUnitRenegade(unit); }
}
