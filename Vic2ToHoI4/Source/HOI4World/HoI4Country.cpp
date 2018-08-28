/*Copyright (c) 2018 The Paradox Game Converters Project

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/



#include "HoI4Country.h"
#include "Advisor.h"
#include "HoI4World.h"
#include "Log.h"
#include "HoI4Faction.h"
#include "HoI4Focus.h"
#include "HoI4Leader.h"
#include "HoI4Localisation.h"
#include "Names.h"
#include "../Mappers/CountryMapping.h"
#include "../Mappers/GovernmentMapper.h"
#include "../Mappers/GraphicsMapper.h"
#include "../Mappers/V2Localisations.h"
#include "../V2World/Relations.h"
#include "../V2World/Party.h"
#include "../Mappers/ProvinceMapper.h"
#include "OSCompatibilityLayer.h"
#include <algorithm>
#include <fstream>



HoI4Country::HoI4Country(const string& _tag, const string& _commonCountryFile, const HoI4::World* _theWorld):
	theWorld(_theWorld),
	srcCountry(nullptr),
	filename(""),
	human(false),
	governmentIdeology("neutrality"),
	leaderIdeology("neutrality"),
	rulingParty(),
	parties(),
	ideologySupport(),
	lastElection(),
	sphereLeader(""),
	tag(_tag),
	provinces(),
	states(),
	capitalStateNum(0),
	capitalState(nullptr),
	commonCountryFile(_commonCountryFile),
	technologies(),
	researchBonuses(),
	relations(),
	color(),
	faction(nullptr),
	factionLeader(false),
	allies(),
	puppets(),
	puppetMaster(""),
	practicals(),
	leaders(),
	graphicalCulture("western_european_gfx"),
	graphicalCulture2d("western_european_2d"),
	majorNation(false),
	civilized(false),
	brigs(),
	convoys(0),
	provinceCount(0),
	armyStrength(0),
	militaryFactories(0.0),
	civilianFactories(0.0),
	dockyards(0.0),
	threat(0.0),
	mobilizationLaw("volunteer_only"),
	economicLaw("civilian_economy"),
	tradeLaw("export_focus"),
	greatPower(false),
	divisions(),
	ships(),
	planes(),
	navalLocation(0),
	equipmentStockpile(),
	nationalFocus(nullptr)
{
}


void HoI4Country::initFromV2Country(const Vic2::World& _srcWorld, const Vic2::Country* _srcCountry, const map<int, int>& stateMap, const map<int, HoI4::State*>& states, HoI4::namesMapper& theNames, const graphicsMapper& theGraphics, const CountryMapper& countryMap)
{
	srcCountry = _srcCountry;

	determineFilename();

	human = srcCountry->isHuman();
	color = srcCountry->getColor();
	civilized = srcCountry->isCivilized();
	threat = srcCountry->getBadBoy() / 10.0;
	auto possibleGraphicalCulture = theGraphics.getGraphicalCulture(srcCountry->getPrimaryCultureGroup());
	if (possibleGraphicalCulture)
	{
		graphicalCulture = *possibleGraphicalCulture;
	}
	else
	{
		graphicalCulture = "western_european_gfx";
	}
	auto possibleGraphicalCulture2d = theGraphics.get2dGraphicalCulture(srcCountry->getPrimaryCultureGroup());
	if (possibleGraphicalCulture2d)
	{
		graphicalCulture2d = *possibleGraphicalCulture2d;
	}
	else
	{
		graphicalCulture2d = "western_european_2d";
	}
	lastElection = srcCountry->getLastElection();
	initIdeas(theNames);

	stability = 0.3;
	warSupport = 0.3;

	if (srcCountry->getProvinces().size() > 0)
	{
		/*stability -= (srcCountry->getAverageMilitancy() / 10 / 1.5);
		if (stability < 0.15)
		{
			stability = 0.15;
		}
		LOG(LogLevel::Debug) << "stability," << tag << "," << stability;
		*/
		float warAttitude = srcCountry->getAverageIssueSupport("jingoism");
		warAttitude += srcCountry->getAverageIssueSupport("pro_military") / 2;
		warAttitude -= srcCountry->getAverageIssueSupport("anti_military") / 2;
		warAttitude -= srcCountry->getAverageIssueSupport("pacifism");
		warSupport += warAttitude * 0.00375;
		warSupport += (srcCountry->getRevanchism() / 5.0) - (srcCountry->getWarExhaustion() / 100.0 / 2.5);
	}

	convertLaws();
	//convertLeaders(portraitMap, landPersonalityMap, seaPersonalityMap, landBackgroundMap, seaBackgroundMap);
	convertRelations(countryMap);

	determineCapitalFromVic2(stateMap, states);
	if (isThisStateOwnedByUs(capitalState))
	{
		capitalState->setAsCapitalState();
	}

	majorNation = srcCountry->isGreatNation();
}


void HoI4Country::determineFilename()
{
	auto possibleFilename = Utils::GetFileFromTag("./blankMod/output/history/countries/", tag);
	if (!possibleFilename)
	{
		possibleFilename = Utils::GetFileFromTag(theConfiguration.getHoI4Path() + "/history/countries/", tag);
	}

	if (possibleFilename)
	{
		filename = *possibleFilename;
	}
	else
	{
		filename = tag + " - " + commonCountryFile;
	}
}


void HoI4Country::convertGovernment(const Vic2::World& sourceWorld, const governmentMapper& governmentMap)
{
	auto possibleRulingParty = srcCountry->getRulingParty(sourceWorld.getParties());
	if (!possibleRulingParty)
	{
		LOG(LogLevel::Error) << "Could not find the ruling party for " << srcCountry->getTag() << ". Most likely a mod was not included.";
		LOG(LogLevel::Error) << "Double-check your settings, and remember to include EU4 to Vic2 mods. See the FAQ for more information.";
		exit(-1);
	}
	else
	{
		rulingParty = *possibleRulingParty;
	}

	governmentIdeology = governmentMap.getIdeologyForCountry(srcCountry, rulingParty.getIdeology());
	leaderIdeology = governmentMap.getLeaderIdeologyForCountry(srcCountry, rulingParty.getIdeology());
	parties = srcCountry->getActiveParties(sourceWorld.getParties());
	for (auto party: parties)
	{
		string partyName = party.getName();
		string trimmedName = partyName.substr(4, partyName.size());
		HoI4Localisation::addPoliticalPartyLocalisation(partyName, tag + "_" + trimmedName + "_party");
	}
}


void HoI4Country::convertParties(const set<string>& majorIdeologies, const governmentMapper& governmentMap)
{
	for (auto HoI4Ideology: majorIdeologies)
	{
		for (auto party: parties)
		{
			if (governmentMap.getSupportedIdeology(governmentIdeology, party.getIdeology(), majorIdeologies) == HoI4Ideology)
			{
				HoI4Localisation::addPoliticalPartyLocalisation(party.getName(), tag + "_" + HoI4Ideology + "_party");
			}
		}
	}
	HoI4Localisation::addPoliticalPartyLocalisation(rulingParty.getName(), tag + "_" + governmentIdeology + "_party");
}


void HoI4Country::initIdeas(HoI4::namesMapper& theNames)
{
	HoI4Localisation::addIdeaLocalisation(tag + "_tank_manufacturer", theNames.takeCarCompanyName(srcCountry->getPrimaryCulture()));
	HoI4Localisation::addIdeaLocalisation(tag + "_motorized_equipment_manufacturer", theNames.takeCarCompanyName(srcCountry->getPrimaryCulture()));
	HoI4Localisation::addIdeaLocalisation(tag + "_infantry_equipment_manufacturer", theNames.takeWeaponCompanyName(srcCountry->getPrimaryCulture()));
	HoI4Localisation::addIdeaLocalisation(tag + "_artillery_manufacturer", theNames.takeWeaponCompanyName(srcCountry->getPrimaryCulture()));
	HoI4Localisation::addIdeaLocalisation(tag + "_light_aircraft_manufacturer", theNames.takeAircraftCompanyName(srcCountry->getPrimaryCulture()));
	HoI4Localisation::addIdeaLocalisation(tag + "_medium_aircraft_manufacturer", theNames.takeAircraftCompanyName(srcCountry->getPrimaryCulture()));
	HoI4Localisation::addIdeaLocalisation(tag + "_heavy_aircraft_manufacturer", theNames.takeAircraftCompanyName(srcCountry->getPrimaryCulture()));
	HoI4Localisation::addIdeaLocalisation(tag + "_naval_aircraft_manufacturer", theNames.takeAircraftCompanyName(srcCountry->getPrimaryCulture()));
	HoI4Localisation::addIdeaLocalisation(tag + "_naval_manufacturer", theNames.takeNavalCompanyName(srcCountry->getPrimaryCulture()));
	HoI4Localisation::addIdeaLocalisation(tag + "_industrial_concern", theNames.takeIndustryCompanyName(srcCountry->getPrimaryCulture()));
	HoI4Localisation::addIdeaLocalisation(tag + "_electronics_concern", theNames.takeElectronicCompanyName(srcCountry->getPrimaryCulture()));
}


void HoI4Country::convertLaws()
{
	// mobilization laws are based on the ruling party's war policy
	if (rulingParty.getWarPolicy() == "jingoism")
	{
		mobilizationLaw = "limited_conscription";
	}
	else if (rulingParty.getWarPolicy() == "pacifism")
	{
		mobilizationLaw = "disarmed_nation";
	}

	// some ideologies have non-default laws
	if (governmentIdeology == "fascism")
	{
		economicLaw = "partial_economic_mobilisation";
		tradeLaw = "limited_exports";
	}
	else if (governmentIdeology == "radical")
	{
		tradeLaw = "free_trade";
	}
}


/*void HoI4Country::convertLeaders(portraitMapping& portraitMap, personalityMap& landPersonalityMap, personalityMap& seaPersonalityMap, backgroundMap& landBackgroundMap, backgroundMap& seaBackgroundMap)
{
	vector<V2Leader*> srcLeaders = srcCountry->getLeaders();
	for (auto srcLeader : srcLeaders)
	{
		HoI4Leader newLeader(srcLeader, tag, landPersonalityMap, seaPersonalityMap, landBackgroundMap, seaBackgroundMap, portraitMap[graphicalCulture]);
		leaders.push_back(newLeader);
	}

	theConfiguration.setLeaderIDForNextCountry();
}*/


void HoI4Country::convertRelations(const CountryMapper& countryMap)
{
	auto srcRelations = srcCountry->getRelations();
	for (auto srcRelation: srcRelations)
	{
		auto HoI4Tag = countryMap.getHoI4Tag(srcRelation.second->getTag());
		if (HoI4Tag)
		{
			auto newRelation = new HoI4Relations(*HoI4Tag, srcRelation.second);
			relations.insert(make_pair(*HoI4Tag, newRelation));
		}
	}
}


void HoI4Country::determineCapitalFromVic2(const map<int, int>& provinceToStateIDMap, const map<int, HoI4::State*>& states)
{
	int oldCapital = srcCountry->getCapital();
	if (auto mapping = theProvinceMapper.getVic2ToHoI4ProvinceMapping(oldCapital))
	{
		auto capitalStateMapping = provinceToStateIDMap.find((*mapping)[0]);
		if (capitalStateMapping != provinceToStateIDMap.end() && isStateValidForCapital(capitalStateMapping->second, states))
		{
			capitalStateNum = capitalStateMapping->second;
			capitalState = states.find(capitalStateNum)->second;
			if (isThisStateOwnedByUs(states.find(capitalStateNum)->second))
			{
				setCapitalInCapitalState((*mapping)[0]);
			}
		}
		else
		{
			findBestCapital();
		}
	}
}


bool HoI4Country::isStateValidForCapital(int stateNum, const map<int, HoI4::State*>& states)
{
	auto state = states.find(stateNum)->second;
	return (isThisStateOwnedByUs(state) || isThisStateACoreWhileWeOwnNoStates(state));
}


bool HoI4Country::isThisStateOwnedByUs(const HoI4::State* state) const
{
	return ((state != nullptr) && (state->getOwner() == tag));
}


bool HoI4Country::isThisStateACoreWhileWeOwnNoStates(const HoI4::State* state) const
{
	for (auto core: state->getCores())
	{
		if (core == tag)
		{
			return true;
		}
	}

	return false;
}


void HoI4Country::setCapitalInCapitalState(int capitalProvince)
{
	if (capitalState != nullptr)
	{
		capitalState->setVPLocation(capitalProvince);
	}
}


void HoI4Country::findBestCapital()
{
	capitalStateNum = 0;
	LOG(LogLevel::Warning) << "Could not properly set capital for " << tag;
}


void HoI4Country::setGovernmentToExistingIdeology(const set<string>& majorIdeologies, const map<string, HoI4Ideology*>& ideologies, const governmentMapper& governmentMap)
{
	governmentIdeology = governmentMap.getExistingIdeologyForCountry(srcCountry, rulingParty.getIdeology(), majorIdeologies, ideologies);
	leaderIdeology = governmentMap.getExistingLeaderIdeologyForCountry(srcCountry, rulingParty.getIdeology(), majorIdeologies, ideologies);
}


void HoI4Country::convertIdeologySupport(const set<string>& majorIdeologies, const governmentMapper& governmentMap)
{
	for (auto upperHouseIdeology: srcCountry->getUpperHouseComposition())
	{
		string ideology = governmentMap.getSupportedIdeology(governmentIdeology, upperHouseIdeology.first, majorIdeologies);
		auto supportItr = ideologySupport.find(ideology);
		if (supportItr == ideologySupport.end())
		{
			ideologySupport.insert(make_pair(ideology, 0));
			supportItr = ideologySupport.find(ideology);
		}
		supportItr->second += static_cast<int>(upperHouseIdeology.second * 100);
	}

	int remainingSupport = 100;
	for (auto ideology: ideologySupport)
	{
		remainingSupport -= ideology.second;
	}
	auto supportItr = ideologySupport.find("neutrality");
	if (supportItr == ideologySupport.end())
	{
		ideologySupport.insert(make_pair("neutrality", 0));
		supportItr = ideologySupport.find("neutrality");
	}
	supportItr->second += remainingSupport;
}


void HoI4Country::convertNavy(const map<string, HoI4::UnitMap>& unitMap)
{
	for (auto army : srcCountry->getArmies())
	{
		for (auto regiment : army->getRegiments())
		{
			string type = regiment->getType();
			
			if (unitMap.count(type) > 0)
			{
				HoI4::UnitMap unitInfo = unitMap.at(type);

				if (unitInfo.getCategory() == "naval") {
					// Ships get mapped
					HoI4Ship newShip(regiment->getName(),unitInfo.getType(),unitInfo.getEquipment(), tag);
					ships.push_back(newShip);
				}
			}
			else
			{
				LOG(LogLevel::Warning) << "Unknown unit type: " << type;
			}
		}
	}

	for (auto state: states)
	{
		if (state.second->getOwner() == tag)
		{
			auto mainNavalLocation = state.second->getMainNavalLocation();
			if (mainNavalLocation)
			{
				// Mapped ships will be placed in a single large fleet
				navalLocation = *mainNavalLocation;
			}
		}
	}
}

void HoI4Country::convertConvoys(const map<string, HoI4::UnitMap>& unitMap)
{
	for (auto army : srcCountry->getArmies())
	{
		for (auto regiment : army->getRegiments())
		{
			string type = regiment->getType();

			if (unitMap.count(type) > 0)
			{
				HoI4::UnitMap unitInfo = unitMap.at(type);

				if (unitInfo.getCategory() == "convoy") {
					// Convoys get placed in national stockpile
					convoys = convoys + 1;
				}
			}
			else
			{
				LOG(LogLevel::Warning) << "Unknown unit type: " << type;
			}
		}
	}
}

void HoI4Country::convertAirforce(const map<string, HoI4::UnitMap>& unitMap)
{
        for (auto army : srcCountry->getArmies())
	{
		for (auto regiment : army->getRegiments())
		{
			string type = regiment->getType();

			if (unitMap.count(type) > 0)
			{
				HoI4::UnitMap unitInfo = unitMap.at(type);

				if (unitInfo.getCategory() == "air") {
					// Air units get placed in national stockpile
					equipmentStockpile[unitInfo.getEquipment()] = equipmentStockpile[unitInfo.getEquipment()] + unitInfo.getSize();
				}
			}
			else
			{
				LOG(LogLevel::Warning) << "Unknown unit type: " << type;
			}
		}
	}	
}

bool sufficientUnits(map<string, int>& units, map<string, string> subs,
                     map<string, int>& req)
{
        for (auto unit : req)
        {
                int available = units[unit.first];
                available += units[subs[unit.first]];
                if (available < unit.second)
                {
                        return false;
                }
        }
        return true;
}

void HoI4Country::convertArmyDivisions(const map<string, HoI4::UnitMap>& unitMap, const vector<HoI4::DivisionTemplateType>& divisionTemplates)
{
	if (capitalState == nullptr)
	{
		return;
	}

	map<string, int> BattalionsAndCompanies;
        map<string, string> substitutes;
        substitutes["artillery"] = "artillery_brigade";
        map<string, int> vicRegiments;

	for (auto army : srcCountry->getArmies())
	{
		for (auto regiment : army->getRegiments())
		{
                        if (regiment->isMobilised())
                        {
                                continue;
                        }
                        string type = regiment->getType();
                        vicRegiments[type]++;

			if (unitMap.count(type) > 0)
			{
				HoI4::UnitMap unitInfo = unitMap.at(type);

				if (unitInfo.getCategory() == "land") {
					// Calculate how many Battalions and Companies are available after mapping Vic2 armies
					BattalionsAndCompanies[unitInfo.getType()] += unitInfo.getSize();
                                }
                        }
			else
			{
				LOG(LogLevel::Warning) << "Unknown unit type: " << type;
			}
		}
	}
        LOG(LogLevel::Info) << srcCountry->getIdentifier() << " has Vic regiments:";
        for (const auto& vr : vicRegiments) {
                LOG(LogLevel::Info) << "  " << vr.first << " = " << vr.second;
        }
        LOG(LogLevel::Info) << srcCountry->getIdentifier() << " has HoI components:";
        for (const auto& bc : BattalionsAndCompanies) {
                LOG(LogLevel::Info) << "  " << bc.first << " = " << bc.second;
        }

        map<string, int> createdDivs;
        int totalDivCount = 0;
	for (auto& divTemplate: divisionTemplates)
	{
		// For each template determine the Battalion and Company requirements.
		int divisionCounter = 1;

		map<string, int> templateRequirements;
		for (auto regiment : divTemplate.getRegiments())
		{
			templateRequirements[regiment.getType()] = templateRequirements[regiment.getType()] + 1;
		}
		for (auto regiment : divTemplate.getSupportRegiments())
		{
			templateRequirements[regiment.getType()] = templateRequirements[regiment.getType()] + 1;
		}

                // Create new divisions as long as sufficient units exist,
                // otherwise move on to next template
                while (sufficientUnits(BattalionsAndCompanies, substitutes,
                                       templateRequirements))
                {
                        HoI4::DivisionType newDivision(
                            to_string(divisionCounter) + ". " +
                                divTemplate.getName(),
                            divTemplate.getName(),
                            capitalState->getVPLocation());

                        for (auto& unit : templateRequirements)
                        {
                                for (int i = 0; i < unit.second; ++i)
                                {
                                        if (BattalionsAndCompanies[unit.first] > 0)
                                        {
                                                BattalionsAndCompanies[unit.first]--;
                                        }
                                        else
                                        {
                                                BattalionsAndCompanies[substitutes[unit.first]]--;
                                        }
                                }
                        }
                        bool dropDivision = true;
                        if (totalDivCount < 25)
                        {
                                dropDivision = false;
                        }
                        else if (totalDivCount < 100)
                        {
                                dropDivision = (totalDivCount % 3 != 0);
                        }
                        else
                        {
                                dropDivision = (totalDivCount % 5 != 0);
                        }
                        if (!dropDivision)
                        {
                                divisionCounter++;
                                divisions.push_back(newDivision);
                                createdDivs[divTemplate.getName()]++;
                        }
                        totalDivCount++;
                }
        }
        LOG(LogLevel::Info)
            << srcCountry->getIdentifier() << " gets divisions:";
        for (const auto& cd : createdDivs)
        {
                LOG(LogLevel::Info) << "  " << cd.first << " = " << cd.second;
        }
        LOG(LogLevel::Info)
            << srcCountry->getIdentifier() << " has leftover components:";
        for (const auto& bc : BattalionsAndCompanies)
        {
                if (bc.second == 0)
                {
                        continue;
                }
                LOG(LogLevel::Info) << "  " << bc.first << " = " << bc.second;
        }

        /*
	// get the total number of source brigades and the number of source brigades per location
	int infantryBrigades = 0;
	int artilleryBrigades = 0;
	int supportBrigades = 0;
	int tankBrigades = 0;
	int cavalryBrigades = 0;
	int cavalrySupportBrigades = 0;
	int mountainBrigades = 0;
	const double adjustment = 0.1 * theConfiguration.getForceMultiplier();

	map<int, double> locations;
	int totalRegiments = 0;
	for (auto army : srcCountry->getArmies())
	{
		// get the number of source brigades per location
		int HoI4location = 0;
		if (auto provMapping = theProvinceMapper.getVic2ToHoI4ProvinceMapping(army->getLocation()))
		{
			for (auto HoI4ProvNum: *provMapping)
			{
				if (HoI4ProvNum != 0 && provinces.find(HoI4ProvNum) != provinces.end())
				{
					HoI4location = HoI4ProvNum;
				}
			}
		}

		// no weight for locations we don't own
		totalRegiments += army->getRegiments().size();
		if (provinces.find(HoI4location) != provinces.end()) {
			locations[HoI4location] += army->getRegiments().size();
		}

		// get the total number of source brigades
		for (auto regiment : army->getRegiments())
		{
			string type = regiment->getType();
			if (type == "artillery")
			{
				infantryBrigades += 2;
				artilleryBrigades++;
			}
			else if (type == "cavalry")
			{
				cavalryBrigades += 3;
			}
			else if (type == "cuirassier" || type == "dragoon" || type == "hussar")
			{
				cavalryBrigades += 3;
				cavalrySupportBrigades++;
			}
			else if (type == "engineer")
			{
				supportBrigades += 3;
			}
			else if (type == "guard")
			{
				mountainBrigades += 2;
			}
			else if (type == "infantry")
			{
				infantryBrigades += 3;
			}
			else if (type == "irregular")
			{
				infantryBrigades += 1;
			}
			else if (type == "tank")
			{
				tankBrigades++;
			}
		}
	}
	double InfWep = 0;
	if (technologies.find("infantry_weapons1") == technologies.end())
	{
		InfWep = 0.3;
	}
	if (technologies.find("infantry_weapons1") != technologies.end())
	{
		InfWep = 0.5;
	}
	armyStrength = 0;
	armyStrength = static_cast<long>((100 * InfWep*infantryBrigades) + (supportBrigades * 175) + (artilleryBrigades * 126) + (tankBrigades * 1135) + (cavalryBrigades * 120 * InfWep) + (mountainBrigades * 140 * InfWep));
	// calculate the number of brigades in different types of divisions
	int infantryPerDivision = 0;
	if (infantryBrigades <= 45)
	{
		infantryPerDivision = 3;
	}
	else if (infantryBrigades <= 90)
	{
		infantryPerDivision = 6;
	}
	else
	{
		infantryPerDivision = 9;
	}

	int tanksPerDivision = 0;
	if (tankBrigades <= 5)
	{
		tanksPerDivision = 1;
	}
	else if (tankBrigades <= 10)
	{
		tanksPerDivision = 2;
	}
	else
	{
		tanksPerDivision = 3;
	}

	int cavalryPerDivision = 0;
	if (cavalryBrigades <= 9)
	{
		cavalryPerDivision = 1;
	}
	else if (cavalryBrigades <= 18)
	{
		cavalryPerDivision = 2;
	}
	else
	{
		cavalryPerDivision = 3;
	}

	int mountainPerDivision = 0;
	if (mountainBrigades <= 9)
	{
		mountainPerDivision = 1;
	}
	else if (mountainBrigades <= 18)
	{
		mountainPerDivision = 2;
	}
	else
	{
		mountainPerDivision = 3;
	}

	int advancedIndex = -1;
	int mediumIndex = -1;
	int basicIndex = -1;
	int tankIndex = -1;
	bool mediumSupport = false;
	// create division templates
	if (tankBrigades > 0)
	{
		HoI4DivisionTemplateType newDivisionTemplate("Tank Division");
		for (int i = 0; i < tanksPerDivision; i++)
		{
			HoI4RegimentType lightArmorRegimentOne("light_armor", 0, i);
			newDivisionTemplate.addRegiment(lightArmorRegimentOne);
			HoI4RegimentType lightArmorRegimentTwo("light_armor", 1, i);
			newDivisionTemplate.addRegiment(lightArmorRegimentTwo);
			HoI4RegimentType motorizedrRegiment("motorized", 2, i);
			newDivisionTemplate.addRegiment(motorizedrRegiment);
		}
		tankIndex = divisionTemplates.size();
		divisionTemplates.push_back(newDivisionTemplate);
	}
	if (cavalryBrigades > 0)
	{
		HoI4DivisionTemplateType newDivisionTemplate("Cavalry Division");
		for (int i = 0; i < cavalryPerDivision; i++)
		{
			HoI4RegimentType cavalryRegimentOne("cavalry", 0, i);
			newDivisionTemplate.addRegiment(cavalryRegimentOne);
			HoI4RegimentType cavalryRegimentTwo("cavalry", 1, i);
			newDivisionTemplate.addRegiment(cavalryRegimentTwo);
			HoI4RegimentType cavalryRegimentThree("cavalry", 2, i);
			newDivisionTemplate.addRegiment(cavalryRegimentThree);
		}
		divisionTemplates.push_back(newDivisionTemplate);
	}
	if (mountainBrigades > 0)
	{
		HoI4DivisionTemplateType newDivisionTemplate("Mountaineers");
		for (int i = 0; i < cavalryPerDivision; i++)
		{
			HoI4RegimentType mountainRegimentOne("mountaineers", 0, i);
			newDivisionTemplate.addRegiment(mountainRegimentOne);
			HoI4RegimentType mountainRegimentTwo("mountaineers", 1, i);
			newDivisionTemplate.addRegiment(mountainRegimentTwo);
			HoI4RegimentType mountainRegimentThree("mountaineers", 2, i);
			newDivisionTemplate.addRegiment(mountainRegimentThree);
		}
		divisionTemplates.push_back(newDivisionTemplate);
	}
	if ((artilleryBrigades > 0) || (supportBrigades > 0))
	{
		if (3 * artilleryBrigades > infantryPerDivision * supportBrigades)
		{
			//there are more brigades with artillery than with support, meddiv will have only art
			HoI4DivisionTemplateType newDivisionTemplate("Support Infantry Division");
			for (int i = 0; i < (infantryPerDivision / 3); i++)
			{
				HoI4RegimentType infantryRegiment("infantry", 0, i);
				newDivisionTemplate.addRegiment(infantryRegiment);
			}
			for (int i = 0; i < (infantryPerDivision / 3); i++)
			{
				HoI4RegimentType infantryRegimentOne("infantry", i, 0);
				newDivisionTemplate.addRegiment(infantryRegimentOne);
				HoI4RegimentType infantryRegimentTwo("infantry", i, 1);
				newDivisionTemplate.addRegiment(infantryRegimentTwo);
				HoI4RegimentType infantryRegimentThree("infantry", i, 2);
				newDivisionTemplate.addRegiment(infantryRegimentThree);
				HoI4RegimentType artilleryRegiment("artillery_brigade", i, 3);
				newDivisionTemplate.addRegiment(artilleryRegiment);
			}
			mediumIndex = divisionTemplates.size();
			divisionTemplates.push_back(newDivisionTemplate);

			if (supportBrigades > 0)
			{
				//have both support brigades and artillery, have superdiv
				HoI4DivisionTemplateType newDivisionTemplate("Advance Infantry Division");
				for (int i = 0; i < (infantryPerDivision / 3); i++)
				{
					HoI4RegimentType infantryRegiment("infantry", 0, i);
					newDivisionTemplate.addRegiment(infantryRegiment);
				}
				for (int i = 0; i < (infantryPerDivision / 3); i++)
				{
					HoI4RegimentType infantryRegimentOne("infantry", i, 0);
					newDivisionTemplate.addRegiment(infantryRegimentOne);
					HoI4RegimentType infantryRegimentTwo("infantry", i, 1);
					newDivisionTemplate.addRegiment(infantryRegimentTwo);
					HoI4RegimentType infantryRegimentThree("infantry", i, 2);
					newDivisionTemplate.addRegiment(infantryRegimentThree);
					HoI4RegimentType artilleryRegiment("artillery_brigade", i, 3);
					newDivisionTemplate.addRegiment(artilleryRegiment);
				}
				HoI4RegimentType engineerRegiment("engineer", 0, 0);
				newDivisionTemplate.addSupportRegiment(engineerRegiment);
				HoI4RegimentType reconRegiment("recon", 0, 1);
				newDivisionTemplate.addSupportRegiment(reconRegiment);

				advancedIndex = divisionTemplates.size();
				divisionTemplates.push_back(newDivisionTemplate);
			}
		}
		else
		{
			//there are more brigades with support then artillery, meddiv will have only support
			HoI4DivisionTemplateType newDivisionTemplate("Support Infantry Division");
			for (int i = 0; i < (infantryPerDivision / 3); i++)
			{
				HoI4RegimentType infantryRegiment("infantry", 0, i);
				newDivisionTemplate.addRegiment(infantryRegiment);
			}
			for (int i = 0; i < (infantryPerDivision / 3); i++)
			{
				HoI4RegimentType infantryRegimentOne("infantry", i, 0);
				newDivisionTemplate.addRegiment(infantryRegimentOne);
				HoI4RegimentType infantryRegimentTwo("infantry", i, 1);
				newDivisionTemplate.addRegiment(infantryRegimentTwo);
				HoI4RegimentType infantryRegimentThree("infantry", i, 2);
				newDivisionTemplate.addRegiment(infantryRegimentThree);
			}
			HoI4RegimentType engineerRegiment("engineer", 0, 0);
			newDivisionTemplate.addSupportRegiment(engineerRegiment);
			HoI4RegimentType reconRegiment("recon", 0, 1);
			newDivisionTemplate.addSupportRegiment(reconRegiment);
			mediumSupport = true;
			mediumIndex = divisionTemplates.size();
			divisionTemplates.push_back(newDivisionTemplate);

			if (artilleryBrigades != 0)
			{
				//have both supportbrigs and artillery, have superdiv
				HoI4DivisionTemplateType newDivisionTemplate("Advance Infantry Division");
				for (int i = 0; i < (infantryPerDivision / 3); i++)
				{
					HoI4RegimentType infantryRegiment("infantry", 0, i);
					newDivisionTemplate.addRegiment(infantryRegiment);
				}
				for (int i = 0; i < (infantryPerDivision / 3); i++)
				{
					HoI4RegimentType infantryRegimentOne("infantry", i, 0);
					newDivisionTemplate.addRegiment(infantryRegimentOne);
					HoI4RegimentType infantryRegimentTwo("infantry", i, 1);
					newDivisionTemplate.addRegiment(infantryRegimentTwo);
					HoI4RegimentType infantryRegimentThree("infantry", i, 2);
					newDivisionTemplate.addRegiment(infantryRegimentThree);
					HoI4RegimentType artilleryRegiment("artillery_brigade", i, 3);
					newDivisionTemplate.addRegiment(artilleryRegiment);
				}
				HoI4RegimentType engineerRegiment("engineer", 0, 0);
				newDivisionTemplate.addSupportRegiment(engineerRegiment);
				HoI4RegimentType reconRegiment("recon", 0, 1);
				newDivisionTemplate.addSupportRegiment(reconRegiment);
				advancedIndex = divisionTemplates.size();
				divisionTemplates.push_back(newDivisionTemplate);
			}
		}
	}

	// basic infantry division
	HoI4DivisionTemplateType newDivisionTemplate("Basic Infantry Division");
	for (int i = 0; i < (infantryPerDivision / 3); i++)
	{
		HoI4RegimentType infantryRegiment("infantry", 0, i);
		newDivisionTemplate.addRegiment(infantryRegiment);
	}
	for (int i = 0; i < (infantryPerDivision / 3); i++)
	{
		HoI4RegimentType infantryRegimentOne("infantry", i, 0);
		newDivisionTemplate.addRegiment(infantryRegimentOne);
		HoI4RegimentType infantryRegimentTwo("infantry", i, 1);
		newDivisionTemplate.addRegiment(infantryRegimentTwo);
		HoI4RegimentType infantryRegimentThree("infantry", i, 2);
		newDivisionTemplate.addRegiment(infantryRegimentThree);
	}
	basicIndex = divisionTemplates.size();
	divisionTemplates.push_back(newDivisionTemplate);

	// calculate number of units per location
	double totalWeight = 0;
	if (0 == locations.size())
	{
		locations[capital] = totalRegiments;
	}
	for (auto const location : locations)
	{
		totalWeight += location.second;
	}
	int numberOfDivisions = infantryBrigades / infantryPerDivision;
	for (auto& location : locations)
	{
		if (totalWeight > 0)
		{
			// Use ceiling here to avoid losing units to, eg, numberOfDivisions = 12,
			// totalWeight = 13. This can happen in the presence of aircraft.
			location.second = ceil(location.second * adjustment * numberOfDivisions / totalWeight);
		}
	}

	// place units
	int numCav = 1;
	int numTank = 1;
	int numMountain = 1;
	int numAdvanced = 1;
	int numMedium = 1;
	int numBasic = 1;

	infantryBrigades = static_cast<int>(0.5 + adjustment * infantryBrigades);
	artilleryBrigades = static_cast<int>(0.5 + adjustment * artilleryBrigades);
	supportBrigades = static_cast<int>(0.5 + adjustment * supportBrigades);
	tankBrigades = static_cast<int>(0.5 + adjustment * tankBrigades);
	cavalryBrigades = static_cast<int>(0.5 + adjustment * cavalryBrigades);
	cavalrySupportBrigades = static_cast<int>(0.5 + adjustment * cavalrySupportBrigades);
	mountainBrigades = static_cast<int>(0.5 + adjustment * mountainBrigades);

	for (auto const location : locations)
	{
		int unitsInProv = 0;
		while (unitsInProv < location.second)
		{
			if (infantryBrigades >= infantryPerDivision)
			{
				int infLocation = 0;
				if (location.first != 0)
				{
					infLocation = location.first;
				}
				else if (capital != 0)
				{
					infLocation = capital;
				}
				else
				{
					LOG(LogLevel::Warning) << "When converting units for " << tag << ", one of the locations for unit placement was undefined!";
					break;
				}
				if (tankBrigades > 0)
				{
					HoI4DivisionType newDivision(to_string(numTank++) + ". Tank Division", "Tank Division", infLocation);
					divisions.push_back(newDivision);
					tankBrigades -= tanksPerDivision;
				}
				if (cavalryBrigades > 0)
				{
					HoI4DivisionType newDivision(to_string(numCav++) + ". Cavalry Division", "Cavalry Division", infLocation);
					divisions.push_back(newDivision);
					cavalryBrigades -= cavalryPerDivision;
				}
				if (mountainBrigades > 0)
				{
					HoI4DivisionType newDivision(to_string(numMountain++) + ". Mountaineers", "Mountaineers", infLocation);
					divisions.push_back(newDivision);
					mountainBrigades -= mountainPerDivision;
				}

				if ((3 * artilleryBrigades >= infantryPerDivision) && (supportBrigades >= 1))
				{
					// Super Placement
					HoI4DivisionType newDivision(to_string(numAdvanced++) + ". Advance Infantry Division", "Advance Infantry Division", infLocation);
					divisions.push_back(newDivision);
					artilleryBrigades -= (infantryPerDivision / 3);
					supportBrigades--;
				}
				else if ((3 * artilleryBrigades >= infantryPerDivision) || (supportBrigades >= 1))
				{
					// Med Placement
					HoI4DivisionType newDivision(to_string(numMedium++) + ". Support Infantry Division", "Support Infantry Division", infLocation);
					divisions.push_back(newDivision);
					artilleryBrigades -= (infantryPerDivision / 3);
					supportBrigades--;
				}
				else
				{
					// Bad Placement
					HoI4DivisionType newDivision(to_string(numBasic++) + ". Basic Infantry Division", "Basic Infantry Division", infLocation);
					divisions.push_back(newDivision);
				}
				infantryBrigades -= infantryPerDivision;
				unitsInProv++;
			}
			else
				break;
		}
	}
	if (artilleryBrigades >= numAdvanced && advancedIndex != -1)
	{
		HoI4RegimentType artilleryRegiment("artillery", 0, 2);
		divisionTemplates[advancedIndex].addSupportRegiment(artilleryRegiment);
		artilleryBrigades -= numAdvanced;
	}
	if (artilleryBrigades >= numMedium && mediumIndex != -1)
	{
		HoI4RegimentType artilleryRegiment("artillery", 0, mediumSupport ? 2 : 0);
		divisionTemplates[mediumIndex].addSupportRegiment(artilleryRegiment);
		artilleryBrigades -= numMedium;
	}
	if (artilleryBrigades >= numBasic && basicIndex != -1)
	{
		HoI4RegimentType artilleryRegiment("artillery", 0, 0);
		divisionTemplates[basicIndex].addSupportRegiment(artilleryRegiment);
		artilleryBrigades -= numBasic;
	}
	if (artilleryBrigades >= numTank && tankIndex != -1)
	{
		HoI4RegimentType artilleryRegiment("artillery", 0, 0);
		divisionTemplates[tankIndex].addSupportRegiment(artilleryRegiment);
		artilleryBrigades -= numTank;
	}*/
}


void HoI4Country::addState(HoI4::State* _state)
{
	states.insert(make_pair(_state->getID(), _state));

	if (capitalStateNum == 0)
	{
		capitalState = _state;
		capitalStateNum = _state->getID();
		_state->setAsCapitalState();
	}

	for (const auto province: _state->getProvinces())
	{
		addProvince(province);
		provinceCount++;
	}
}


void HoI4Country::addProvince(int _province)
{
	provinces.insert(_province);
}


optional<const HoI4Relations*> HoI4Country::getRelations(string withWhom) const
{
	map<string, HoI4Relations*>::const_iterator i = relations.find(withWhom);
	if (i != relations.end())
	{
		return i->second;
	}
	else
	{
		return {};
	}
}


void HoI4Country::setTechnology(const string& tech, int level)
{
	map<string, int>::iterator techEntry = technologies.find(tech);
	if ((techEntry == technologies.end()) || (techEntry->second < level))
	{
		technologies[tech] = level;
	}
}


void HoI4Country::setResearchBonus(const string& tech, int bonus)
{
	map<string, int>::iterator researchBonusEntry = researchBonuses.find(tech);
	if ((researchBonusEntry == researchBonuses.end()) || (researchBonusEntry->second < bonus))
	{
		researchBonuses[tech] = bonus;
	}
}


void HoI4Country::calculateIndustry()
{
	for (auto state : states)
	{
		civilianFactories += state.second->getCivFactories();
		militaryFactories += state.second->getMilFactories();
		dockyards += state.second->getDockyards();
	}
}


void HoI4Country::reportIndustry(ofstream& out)
{
	if (states.size() > 0)
	{
		out << tag << ',';
		out << militaryFactories << ',';
		out << civilianFactories << ',';
		out << dockyards << ',';
		out << militaryFactories + civilianFactories + dockyards << '\n';
	}
}


void HoI4Country::addVPsToCapital(int VPs)
{
	if (capitalState != nullptr)
	{
		capitalState->addVictoryPointValue(VPs);
	}
}


void HoI4Country::addGenericFocusTree(const set<string>& majorIdeologies)
{
	if (!nationalFocus)
	{
		HoI4FocusTree genericNationalFocus(*this);
		genericNationalFocus.addGenericFocusTree(majorIdeologies);
		nationalFocus = genericNationalFocus.makeCustomizedCopy(*this);
	}
}


void HoI4Country::adjustResearchFocuses()
{
	if (greatPower && (nationalFocus != nullptr))
	{
		nationalFocus->removeFocus("extra_tech_slot_2" + tag);
	}
}


double HoI4Country::getStrengthOverTime(double years) const
{
	return getMilitaryStrength() + getEconomicStrength(years);
}


double HoI4Country::getMilitaryStrength() const
{
	return armyStrength;
}


double HoI4Country::getEconomicStrength(double years) const
{
	double militarySectorStrength = militaryFactories * 3 * 365 * years;
	double civilianSectorStrength = civilianFactories * 0.469 * 0.5 * 3 * 365 * 0.5 * years * years; /*.469 is milfac per year, .5 since half are used by consumer goods*/

	return militarySectorStrength + civilianSectorStrength;
}


void HoI4Country::outputToCommonCountriesFile(ofstream& countriesFile) const
{
	countriesFile << tag.c_str() << " = \"countries/" << Utils::normalizeUTF8Path(commonCountryFile) << "\"\n";
}


void HoI4Country::outputColors(ofstream& out) const
{
	out << tag << " = {\n";
	out << "\tcolor = rgb { " << color << " }\n";
	out << "\tcolor_ui = rgb { " << color << " }";
	out << "}\n";
}


void HoI4Country::outputToNamesFiles(ofstream& namesFile, const HoI4::namesMapper& theNames) const
{
	auto femaleSurnames = theNames.getFemaleSurnames(srcCountry->getPrimaryCulture());

	namesFile << tag << " = {\n";

	namesFile << "\tmale = {\n";
	namesFile << "\t\tnames = {\n";
	outputNamesSet(namesFile, theNames.getMaleNames(srcCountry->getPrimaryCulture()), "\t\t\t");
	namesFile << "\t\t}\n";
	if (femaleSurnames->size() > 0)
	{
		namesFile << "\t\tsurnames = {\n";
		outputNamesSet(namesFile, theNames.getSurnames(srcCountry->getPrimaryCulture()), "\t\t");
		namesFile << "\t\t}\n";
	}
	namesFile << "\t}\n";

	namesFile << "\tfemale = {\n";
	namesFile << "\t\tnames = {\n";
	outputNamesSet(namesFile, theNames.getFemaleNames(srcCountry->getPrimaryCulture()), "\t\t\t");
	namesFile << "\t\t}\n";
	if (femaleSurnames->size() > 0)
	{
		namesFile << "\t\tsurnames = {\n";
		outputNamesSet(namesFile, femaleSurnames, "\t\t");
		namesFile << "\t}\n";
	}
	namesFile << "\t}\n";

	if (femaleSurnames->size() == 0)
	{
		namesFile << "\tsurnames = {\n";
		outputNamesSet(namesFile, theNames.getSurnames(srcCountry->getPrimaryCulture()), "\t\t");
		namesFile << "\t}\n";
	}

	namesFile << "\tcallsigns = {\n";
	outputNamesSet(namesFile, theNames.getCallsigns(srcCountry->getPrimaryCulture()), "\t\t");
	namesFile << "\t}\n";

	namesFile << "}\n";
}

void HoI4Country::outputUnitType(ofstream& unitNamesFile, const string& sourceUnitType, const string& destUnitType, const string& defaultName) const
{
	unitNamesFile << "\t" << destUnitType << " = {\n";
	unitNamesFile << "\t\tprefix = \"\"\n";
	unitNamesFile << "\t\tgeneric = { \""<< defaultName <<"\" }\n";
	unitNamesFile << "\t\tunique = {\n";

	unitNamesFile << "\t\t\t";
	for (string shipName : srcCountry->getShipNames(sourceUnitType))
	{
		unitNamesFile << "\"" << shipName << "\" ";
	}
	unitNamesFile << "\n";

	unitNamesFile << "\t\t}\n";
	unitNamesFile << "\t}\n";
	unitNamesFile << "\n";
}

void HoI4Country::outputToUnitNamesFiles(ofstream& unitNamesFile) const
{
	unitNamesFile << tag << " = {\n";

	outputUnitType(unitNamesFile, "frigate", "submarine", "Submarine");
	outputUnitType(unitNamesFile, "monitor", "carrier", "Carrier");

	outputUnitType(unitNamesFile, "dreadnought", "battleship", "Battleship");
	outputUnitType(unitNamesFile, "ironclad", "battle_cruiser", "Battlecruiser");
	outputUnitType(unitNamesFile, "manowar", "heavy_cruiser", "Heavy Cruiser");

	outputUnitType(unitNamesFile, "cruiser", "destroyer", "Destroyer");
	outputUnitType(unitNamesFile, "commerce_raider", "light_cruiser", "Light Cruiser");

	unitNamesFile << "}\n\n";
}

void HoI4Country::outputNamesSet(ofstream& namesFile, const optional<vector<string>>& names, const string& tabs) const
{
	if (names)
	{
		namesFile << tabs;

		for (unsigned int i = 0; i < names->size(); i++)
		{
			namesFile << '\"' << (*names)[i] << '\"';
			if ((i + 1) == names->size())
			{
				continue;
			}
			else if (((i + 1) % 10) == 0)
			{
				namesFile << "\n";
				namesFile << tabs;
			}
			else
			{
				namesFile << " ";
			}
		}

		namesFile << '\n';
	}
}


void HoI4Country::output(const set<const HoI4::Advisor*, HoI4::advisorCompare>& ideologicalMinisters, const vector<HoI4::DivisionTemplateType>& divisionTemplates, HoI4::namesMapper& theNames, graphicsMapper& theGraphics) const
{
	if (capitalStateNum != 0)
	{
		outputHistory(theNames, theGraphics);
		outputOOB(divisionTemplates);
		outputCommonCountryFile();
		outputAdvisorIdeas(ideologicalMinisters);

		if (nationalFocus != nullptr)
		{
			nationalFocus->output("output/" + theConfiguration.getOutputName() + "/common/national_focus/" + srcCountry->getTag() + "_NF.txt");
		}
	}
}


void HoI4Country::outputHistory(HoI4::namesMapper& theNames, graphicsMapper& theGraphics) const
{
	ofstream output("output/" + theConfiguration.getOutputName() + "/history/countries/" + Utils::normalizeUTF8Path(filename));
	if (!output.is_open())
	{
		Log(LogLevel::Error) << "Could not open output/" << theConfiguration.getOutputName() << "/history/countries/" << Utils::normalizeUTF8Path(filename);
		exit(-1);
	}
	output << "\xEF\xBB\xBF";    // add the BOM to make HoI4 happy

	outputCapital(output);
	outputResearchSlots(output);
	outputThreat(output);
	outputOOBLine(output);
	outputTechnology(output);
	outputResearchBonuses(output);
	outputConvoys(output);
	outputEquipmentStockpile(output);
	outputPuppets(output);
	outputPolitics(output);
	outputRelations(output);
	outputFactions(output);
	outputIdeas(output);
	outputStability(output);
	outputWarSupport(output);
	outputCountryLeader(output, theNames, theGraphics);

	output.close();
}


void HoI4Country::outputCapital(ofstream& output) const
{
	if (capitalStateNum > 0)
	{
		output << "capital = " << capitalStateNum << '\n';
	}
	else if (states.size() > 0)
	{
		output << "capital = " << states.begin()->first << '\n';
	}
	else
	{
		output << "capital = 1\n";
	}
}


void HoI4Country::outputResearchSlots(ofstream& output) const
{
	if (greatPower)
	{
		output << "set_research_slots = 4\n";
	}
	else if (civilized)
	{
		output << "set_research_slots = 3\n";
	}
	else
	{
		output << "set_research_slots = 2\n";
	}
}


void HoI4Country::outputThreat(ofstream& output) const
{
	if (threat != 0.0)
	{
		output << "add_named_threat = { threat = "<< threat << " name = infamy }\n";
	}
	output << "\n";
}


void HoI4Country::outputOOBLine(ofstream& output) const
{
	output << "oob = \"" << tag << "_OOB\"\n";
	output << "\n";
}


void HoI4Country::outputTechnology(ofstream& output) const
{
	output << "# Starting tech\n";
	output << "set_technology = {\n";
	for (auto tech : technologies)
	{
		output << tech.first << " = 1\n";
	}
	output << "}\n";
	output << "\n";
}

void HoI4Country::outputResearchBonuses(ofstream& output) const
{
	output << "# Research Bonuses\n";
	for (auto researchBonus : researchBonuses)
	{
		output << "add_tech_bonus = { bonus = 0." << float(researchBonus.second) << " uses = 1 category = " << researchBonus.first << " name = doctrine_bonus }\n";
	}
}


void HoI4Country::outputConvoys(ofstream& output) const
{
	output << "set_convoys = " << convoys << '\n';
	output << "\n";
}


void HoI4Country::outputEquipmentStockpile(ofstream& output) const
{
	for (auto eqp: equipmentStockpile)
	{
		output << "add_equipment_to_stockpile = { type = " << eqp.first << " amount = " << eqp.second << " producer = " << tag << " }\n";
	}	
	output << "\n";
}


void HoI4Country::outputPuppets(ofstream& output) const
{
	if (puppets.size() > 0)
	{
		output << "# DIPLOMACY\n";
		output << "if = {\n";
		output << "    limit = {\n";
		output << "        has_dlc = \"Together for Victory\"\n";
		output << "    }\n";
		for (auto puppet : puppets)
		{
			if (governmentIdeology == "fascism")
			{
				output << "    set_autonomy = {\n";
				output << "        target = " << puppet << "\n";
				output << "        autonomous_state = autonomy_integrated_puppet\n";
				output << "    }\n";
			}
			else
			{
				output << "    set_autonomy = {\n";
				output << "        target = " << puppet << "\n";
				output << "        autonomous_state = autonomy_dominion\n";
				output << "        freedom_level = 0.4\n";
				output << "    }\n";
			}
		}
		output << "    else = {\n";
		for (auto puppet : puppets)
		{
			if (governmentIdeology == "fascism")
			{
				output << "        set_autonomy = {\n";
				output << "            target = " << puppet << "\n";
				output << "            autonomous_state = autonomy_puppet\n";
				output << "        }\n";
			}
			else
			{
				output << "        puppet = " << puppet << "\n";
			}
		}
		output << "    }\n";
		output << "}\n";
		output << "\n";

		output << "if = {\n";
		output << "    limit = {has_dlc = \"Together for Victory\" }\n";
		output << "\n";
		output << "    add_to_tech_sharing_group = " << tag <<"_research\n";
		output << "}\n\n";
	}

	if (puppetMaster != "")
	{
		output << "if = {\n";
		output << "    limit = {has_dlc = \"Together for Victory\" }\n";
		output << "\n";
		output << "    add_to_tech_sharing_group = " << puppetMaster << "_research\n";
		output << "}\n\n";
	}
}


void HoI4Country::outputPolitics(ofstream& output) const
{
	output << "set_politics = {\n";
	output << "\n";
	output << "    parties = {\n";
	for (auto ideology: ideologySupport)
	{
		output << "        " << ideology.first << " = {\n";
		output << "            popularity = " << ideology.second << "\n";
		output << "        }\n";
	}
	output << "    }\n";
	output << "    \n";

	output << "    ruling_party = " << governmentIdeology << "\n";
	output << "    last_election = \"" << lastElection << "\"\n";
	output << "    election_frequency = 48\n";
	if (areElectionsAllowed())
	{
		output << "    elections_allowed = yes\n";
	}
	else
	{
		output << "    elections_allowed = no\n";
	}
	output << "}\n";
}


void HoI4Country::outputRelations(ofstream& output) const
{
	for (auto relation: relations)
	{
		if (relation.first != tag)
		{
			output << "add_opinion_modifier = { target = " << relation.first << " modifier = ";
			int relationsValue = relation.second->getRelations();
			if (relationsValue < 0)
			{
				output << "negative_";
			}
			else
			{
				output << "positive_";
			}
			output << abs(relationsValue) << " }\n";
		}
	}
	output << "\n";
}


bool HoI4Country::areElectionsAllowed(void) const
{
	if (
			(governmentIdeology == "democratic") ||
			(
				(governmentIdeology == "neutrality") &&
				(
					(leaderIdeology == "conservatism_neutral") ||
					(leaderIdeology == "liberalism_neutral") ||
					(leaderIdeology == "socialism_neutral")
				)
			)
		)
	{
		return true;
	}
	else
	{
		return false;
	}
}


void HoI4Country::outputFactions(ofstream& output) const
{
	if ((faction != nullptr) && (faction->getLeader()->getTag() == tag))
	{
		string allianceName;
		auto possibleLeaderName = getSourceCountry()->getName("english");
		if (possibleLeaderName)
		{
			allianceName = "Alliance of " + *possibleLeaderName;
		}
		else
		{
			LOG(LogLevel::Warning) << "Could not name alliance";
			allianceName = "faction";
		}
		output << "create_faction = \"" + allianceName + "\"\n";
		for (auto factionMember : faction->getMembers())
		{
			output << "add_to_faction = " + factionMember->getTag() + "\n";
		}
	}

	output << '\n';
}


void HoI4Country::outputIdeas(ofstream& output) const
{
	output << "add_ideas = {\n";
	if (majorNation)
	{
		output << "\tgreat_power\n";
	}
	if (!civilized)
	{
		output << "\tuncivilized\n";
	}

	output << "\t" << mobilizationLaw << "\n";
	output << "\t" << economicLaw << "\n";
	output << "\t" << tradeLaw << "\n";
	output << "\tculture_" << srcCountry->getPrimaryCulture() << "\n";

	output << "}\n";
}


void HoI4Country::outputStability(ofstream& output) const
{
	if (states.size() > 0)
	{
		output << "set_stability = 0." << int(stability * 100) << "\n";
	}
}


void HoI4Country::outputWarSupport(ofstream& output) const
{
	if (states.size() > 0)
	{
		output << "set_war_support = 0." << int(warSupport * 100) << "\n";
	}
}


void HoI4Country::outputCountryLeader(ofstream& output, HoI4::namesMapper& theNames, graphicsMapper& theGraphics) const
{
	optional<string> firstName = theNames.getMaleName(srcCountry->getPrimaryCulture());
	optional<string> surname = theNames.getSurname(srcCountry->getPrimaryCulture());
	string portrait = theGraphics.getLeaderPortrait(srcCountry->getPrimaryCultureGroup(), governmentIdeology);

	if (firstName && surname)
	{
		string upperFirstName = *firstName;
		std::transform(upperFirstName.begin(), upperFirstName.end(), upperFirstName.begin(), ::toupper);
		string upperSurname = *surname;
		std::transform(upperSurname.begin(), upperSurname.end(), upperSurname.begin(), ::toupper);
		output << "create_country_leader = {\n";
		output << "    name = \"" << *firstName << " " << *surname << "\"\n";
		output << "    desc = \"POLITICS_" << upperFirstName << "_" << upperSurname << "_DESC\"\n";
		output << "    picture = \"" << portrait << "\"\n";
		output << "    expire = \"1965.1.1\"\n";
		output << "    ideology = " << leaderIdeology << "\n";
		output << "    traits = {\n";
		output << "    }\n";
		output << "}\n";
	}
	else
	{
		LOG(LogLevel::Warning) << "Could not set leader for " + tag + ", as there were no names.";
	}
}


void HoI4Country::outputOOB(const vector<HoI4::DivisionTemplateType>& divisionTemplates) const
{
	ofstream output("output/" + theConfiguration.getOutputName() + "/history/units/" + tag + "_OOB.txt");
	if (!output.is_open())
	{
		Log(LogLevel::Error) << "Could not open output/" << theConfiguration.getOutputName() << "/history/units/" << tag << "_OOB.txt";
		exit(-1);
	}
	output << "\xEF\xBB\xBF";	// add the BOM to make HoI4 happy

										/*for (auto armyItr: armies)
										{
										if (armyItr->getProductionQueue())
										{
										armyItr->outputIntoProductionQueue(output, tag);
										}
										else
										{
										armyItr->output(output);
										}
										}*/
	output << "start_equipment_factor = 0\n";
	for (auto& divisionTemplate : divisionTemplates)
	{
		output << divisionTemplate;
		output << "\n";
	}
	output << "### No BHU air forces ###\n";
	output << "instant_effect = {\n";
	if (technologies.find("infantry_weapons1") != technologies.end())
	{
		output << "\tadd_equipment_production = {\n";
		output << "\t\tequipment = {\n";
		output << "\t\t\ttype = infantry_equipment_1\n";
		output << "\t\t\tcreator = \"" << tag << "\"\n";
		output << "\t\t}\n";
		output << "\t\trequested_factories = 5\n";
		output << "\t\tprogress = 0.88\n";
		output << "\t\tefficiency = 100\n";
		output << "\t}\n";
	}
	else
	{
		output << "\tadd_equipment_production = {\n";
		output << "\t\tequipment = {\n";
		output << "\t\t\ttype = infantry_equipment_0\n";
		output << "\t\t\tcreator = \"" << tag << "\"\n";
		output << "\t\t}\n";
		output << "\t\trequested_factories = 5\n";
		output << "\t\tprogress = 0.88\n";
		output << "\t\tefficiency = 100\n";
		output << "\t}\n";
	}
	if (technologies.find("gw_artillery") != technologies.end())
	{
		output << "\tadd_equipment_production = {\n";
		output << "\t\tequipment = {\n";
		output << "\t\t\ttype = artillery_equipment_1\n";
		output << "\t\t\tcreator = \"" << tag << "\"\n";
		output << "\t\t}\n";
		output << "\t\trequested_factories = 2\n";
		output << "\t\tprogress = 0.88\n";
		output << "\t\tefficiency = 100\n";
		output << "\t}\n";
	}
	if (technologies.find("fighter1") != technologies.end())
	{
		output << "\tadd_equipment_production = {\n";
		output << "\t\tequipment = {\n";
		output << "\t\t\ttype = fighter_equipment_1\n";
		output << "\t\t\tcreator = \"" << tag << "\"\n";
		output << "\t\t}\n";
		output << "\t\trequested_factories = 5\n";
		output << "\t\tprogress = 0.88\n";
		output << "\t\tefficiency = 100\n";
		output << "\t}\n";
	}
	else if (technologies.find("early_fighter") != technologies.end())
	{
		output << "\tadd_equipment_production = {\n";
		output << "\t\tequipment = {\n";
		output << "\t\t\ttype = fighter_equipment_0\n";
		output << "\t\t\tcreator = \"" << tag << "\"\n";
		output << "\t\t}\n";
		output << "\t\trequested_factories = 5\n";
		output << "\t\tprogress = 0.88\n";
		output << "\t\tefficiency = 100\n";
		output << "\t}\n";
	}
	if (technologies.find("basic_destroyer") != technologies.end())
	{
		output << "\tadd_equipment_production = {\n";
		output << "\t\tequipment = {\n";
		output << "\t\t\ttype = destroyer_2\n";
		output << "\t\t\tcreator = \"" << tag << "\"\n";
		output << "\t\t}\n";
		output << "\t\trequested_factories = 3\n";
		output << "\t\tprogress = 0.25\n";
		output << "\t\tamount = 10\n";
		output << "\t}\n";
	}
	else if (technologies.find("early_destroyer") != technologies.end())
	{
		output << "\tadd_equipment_production = {\n";
		output << "\t\tequipment = {\n";
		output << "\t\t\ttype = destroyer_1\n";
		output << "\t\t\tcreator = \"" << tag << "\"\n";
		output << "\t\t}\n";
		output << "\t\trequested_factories = 3\n";
		output << "\t\tprogress = 0.25\n";
		output << "\t\tamount = 10\n";
		output << "\t}\n";
	}
	if (technologies.find("basic_battleship") != technologies.end())
	{
		output << "\tadd_equipment_production = {\n";
		output << "\t\tequipment = {\n";
		output << "\t\t\ttype = battleship_2\n";
		output << "\t\t\tcreator = \"" << tag << "\"\n";
		output << "\t\t}\n";
		output << "\t\trequested_factories = 8\n";
		output << "\t\tprogress = 0.25\n";
		output << "\t\tamount = 3\n";
		output << "\t}\n";
	}
	else if (technologies.find("early_battleship") != technologies.end())
	{
		output << "\tadd_equipment_production = {\n";
		output << "\t\tequipment = {\n";
		output << "\t\t\ttype = battleship_1\n";
		output << "\t\t\tcreator = \"" << tag << "\"\n";
		output << "\t\t}\n";
		output << "\t\trequested_factories = 8\n";
		output << "\t\tprogress = 0.25\n";
		output << "\t\tamount = 3\n";
		output << "\t}\n";
	}
	output << "\tadd_equipment_production = {\n";
	output << "\t\tequipment = {\n";
	output << "\t\t\ttype = convoy_1\n";
	output << "\t\t\tcreator = \"" << tag << "\"\n";
	output << "\t\t}\n";
	output << "\t\trequested_factories = 10\n";
	output << "\t\tprogress = 0.50\n";
	output << "\t\tamount = 100\n";
	output << "\t}\n";
	output << "}\n";
	output << "units = {\n";
	for (auto& division : divisions)
	{
		output << division;
	}
	if (ships.size() > 0)
	{
		output << "\tnavy = {" << endl;
		output << "\t\tname = \"Grand Fleet\"" << endl;
		output << "\t\tlocation = " << navalLocation << endl;
		for (auto& ship : ships)
		{
			output << ship;
		}
		output << "\t}" << endl;
	}
	output << "}\n";
	if (planes.size() > 0)
	{
		output << "air_wings = {\n";
		output << "\t" << capitalStateNum << " = {\n";
		for (auto& plane: planes)
		{
			output << plane;
		}
		output << "\t}\n";
		output << "}\n";
	}
	output.close();
}


void HoI4Country::outputCommonCountryFile() const
{
	ofstream output("output/" + theConfiguration.getOutputName() + "/common/countries/" + Utils::normalizeUTF8Path(commonCountryFile));
	if (!output.is_open())
	{
		Log(LogLevel::Error) << "Could not open " << "output/" << theConfiguration.getOutputName() << "/common/countries/" << Utils::normalizeUTF8Path(commonCountryFile);
		exit(-1);
	}

	if ((graphicalCulture != "") && (graphicalCulture2d != ""))
	{
		output << "graphical_culture = " << graphicalCulture << "\n";
		output << "graphical_culture_2d = " << graphicalCulture2d << "\n";
	}
	output << "color = { " << color << " }\n";

	output.close();
}


void HoI4Country::outputAdvisorIdeas(const set<const HoI4::Advisor*, HoI4::advisorCompare>& ideologicalAdvisors) const
{
	ofstream ideasFile("output/" + theConfiguration.getOutputName() + "/common/ideas/" + tag + ".txt");
	if (!ideasFile.is_open())
	{
		LOG(LogLevel::Error) << "Could not open output/" << theConfiguration.getOutputName() << "/common/ideas/" << tag << ".txt";
		exit(-1);
	}

	ideasFile << "ideas = {\n";
	ideasFile << "\tpolitical_advisor = {\n";
	for (auto ideologicalAdvisor: ideologicalAdvisors)
	{
		ideologicalAdvisor->output(ideasFile, tag);
	}
	ideasFile << "\t}\n";

	ideasFile << "\ttank_manufacturer = { \n";
	ideasFile << "\t\tdesigner = yes\n";
	ideasFile << "\t\t\n";
	ideasFile << "\t\t" << tag << "_tank_manufacturer = {\n";
	ideasFile << "\t\t\tpicture = generic_tank_manufacturer_1\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\tarmor = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { tank_manufacturer }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t}\n";
	ideasFile << "\tnaval_manufacturer = { \n";
	ideasFile << "\t\tdesigner = yes\n";
	ideasFile << "\t\t" << tag << "_naval_manufacturer = {\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tpicture = generic_naval_manufacturer_1\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\tnaval_equipment = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { naval_manufacturer }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t}\n";
	ideasFile << "\taircraft_manufacturer = { \n";
	ideasFile << "\t\tdesigner = yes\n";
	ideasFile << "\t\t" << tag << "_light_aircraft_manufacturer = {\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tpicture = generic_air_manufacturer_1\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\tair_equipment = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { light_aircraft_manufacturer }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t\t\n";
	ideasFile << "\t\t" << tag << "_medium_aircraft_manufacturer = {\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tpicture = generic_air_manufacturer_3\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\tair_equipment = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { medium_aircraft_manufacturer }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t\t\n";
	ideasFile << "\t\t" << tag << "_heavy_aircraft_manufacturer = {\n";
	ideasFile << "\t\t\tpicture = generic_air_manufacturer_2\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\tair_equipment = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { heavy_aircraft_manufacturer }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t\t\n";
	ideasFile << "\t\t" << tag << "_naval_aircraft_manufacturer = {\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tpicture = generic_naval_manufacturer_2\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\tair_equipment = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { naval_aircraft_manufacturer }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t}\n";
	ideasFile << "\tindustrial_concern = {\n";
	ideasFile << "\t\t" << tag << "_industrial_concern = {\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tpicture = generic_industrial_concern_1\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\tindustry = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { industrial_concern }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t\t" << tag << "_electronics_concern = {\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tpicture = generic_electronics_concern_1\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\telectronics = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { electronics_concern }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t}\n";
	ideasFile << "\tmateriel_manufacturer = {\n";
	ideasFile << "\t\tdesigner = yes\n";
	ideasFile << "\t\t" << tag << "_motorized_equipment_manufacturer = {\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tpicture = generic_motorized_equipment_manufacturer_3\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\tmotorized_equipment = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { motorized_equipment_manufacturer }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t\t\n";
	ideasFile << "\t\t" << tag << "_infantry_equipment_manufacturer = {\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tpicture = generic_infantry_equipment_manufacturer_2\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\tinfantry_weapons = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { infantry_equipment_manufacturer }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t\t\n";
	ideasFile << "\t\t" << tag << "_artillery_manufacturer = {\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tpicture = generic_artillery_manufacturer_2\n";
	ideasFile << "\t\t\tallowed = {\n";
	ideasFile << "\t\t\t\toriginal_tag = " << tag << "\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tcost = 150\n";
	ideasFile << "\t\t\tremoval_cost = 10\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\tresearch_bonus = {\n";
	ideasFile << "\t\t\t\tartillery = 0.10\n";
	ideasFile << "\t\t\t}\n";
	ideasFile << "\t\t\t\n";
	ideasFile << "\t\t\ttraits = { artillery_manufacturer }\n";
	ideasFile << "\t\t}\n";
	ideasFile << "\t}\n";

	ideasFile << "}\n";
}


void HoI4Country::outputIdeaGraphics(ofstream& ideasFile, graphicsMapper& graphics) const
{

	ideasFile << "\tspriteType = {\n";
	ideasFile << "\t\tname = \"GFX_idea_" << tag << "_communist_advisor\"\n";
	ideasFile << "\t\ttexturefile = \"" << graphics.getIdeologyMinisterPortrait(srcCountry->getPrimaryCultureGroup(), "communism") << "\"\n";
	ideasFile << "\t}\n";


	ideasFile << "\tspriteType = {\n";
	ideasFile << "\t\tname = \"GFX_idea_" << tag << "_democratic_advisor\"\n";
	ideasFile << "\t\ttexturefile = \"" << graphics.getIdeologyMinisterPortrait(srcCountry->getPrimaryCultureGroup(), "democratic") << "\"\n";
	ideasFile << "\t}\n";


	ideasFile << "\tspriteType = {\n";
	ideasFile << "\t\tname = \"GFX_idea_" << tag << "_neutrality_advisor\"\n";
	ideasFile << "\t\ttexturefile = \"" << graphics.getIdeologyMinisterPortrait(srcCountry->getPrimaryCultureGroup(), "neutrality") << "\"\n";
	ideasFile << "\t}\n";
	
	ideasFile << "\tspriteType = {\n";
	ideasFile << "\t\tname = \"GFX_idea_" << tag << "_absolutist_advisor\"\n";
	ideasFile << "\t\ttexturefile = \"" << graphics.getIdeologyMinisterPortrait(srcCountry->getPrimaryCultureGroup(), "absolutist") << "\"\n";
	ideasFile << "\t}\n";


	ideasFile << "\tspriteType = {\n";
	ideasFile << "\t\tname = \"GFX_idea_" << tag << "_radical_advisor\"\n";
	ideasFile << "\t\ttexturefile = \"" << graphics.getIdeologyMinisterPortrait(srcCountry->getPrimaryCultureGroup(), "radical") << "\"\n";
	ideasFile << "\t}\n";


	ideasFile << "\tspriteType = {\n";
	ideasFile << "\t\tname = \"GFX_idea_" << tag << "_fascist_advisor\"\n";
	ideasFile << "\t\ttexturefile = \"" << graphics.getIdeologyMinisterPortrait(srcCountry->getPrimaryCultureGroup(), "fascism") << "\"\n";
	ideasFile << "\t}\n";
}
