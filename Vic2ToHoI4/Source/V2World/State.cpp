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


#include <unordered_map>
#include <string>

#include "State.h"
#include "Building.h"
#include "Country.h"
#include "Province.h"
#include "StateDefinitions.h"
#include "World.h"
#include "Log.h"
#include "ParserHelpers.h"



Vic2::State::State(std::istream& theStream, const std::string& ownerTag):
	owner(ownerTag)
{
	registerKeyword(std::regex("provinces"), [this](const std::string& unused, std::istream& theStream){
		commonItems::intList provinceList(theStream);
		for (auto province: provinceList.getInts())
		{
			provinceNums.insert(province);
		}
	});
	registerKeyword(std::regex("state_buildings"), [this](const std::string& unused, std::istream& theStream){
		Building theBuilding(theStream);
		factoryLevel += theBuilding.getLevel();
	});
	registerKeyword(std::regex("[A-Za-z0-9_]+"), commonItems::ignoreItem);

	parseStream(theStream);
	setID();
        rgos.resize(kNumResources);
}


Vic2::State::State(std::set<std::pair<int, Vic2::Province*>> theProvinces)
{
	for (auto province: theProvinces)
	{
		provinceNums.insert(province.first);
		provinces.insert(province.second);
	}
	setID();
	determineIfPartialState();
}

#pragma optimize("",off)
void Vic2::State::setID()
{
	auto foundStateID = theStateDefinitions.getStateID(*provinceNums.begin());
	if (foundStateID)
	{
		stateID = *foundStateID;
	}
	else
	{
		LOG(LogLevel::Warning) << "Could not find the state for Vic2 province " << *provinceNums.begin() << ".";
	}
}


void Vic2::State::setCapital()
{
	capitalProvince = theStateDefinitions.getCapitalProvince(stateID);
}
#pragma optimize("",on)

void Vic2::State::determineEmployedWorkers()
{
	workerStruct workers = countEmployedWorkers();
	workers = limitWorkersByFactoryLevels(workers);
	employedWorkers = determineEmployedWorkersScore(workers);
}


Vic2::workerStruct Vic2::State::countEmployedWorkers() const
{
	workerStruct workers;

	for (auto province: provinces)
	{
		workers.craftsmen += province->getPopulation("craftsmen");
		workers.clerks += province->getPopulation("clerks");
		workers.artisans += province->getPopulation("artisans");
		workers.capitalists += province->getLiteracyWeightedPopulation("capitalists");
	}

	return workers;
}


Vic2::workerStruct Vic2::State::limitWorkersByFactoryLevels(const workerStruct& workers) const
{
	workerStruct newWorkers;
	if ((workers.craftsmen + workers.clerks) > (factoryLevel * 10000))
	{
		newWorkers.craftsmen = static_cast<int>((factoryLevel * 10000.0f) / (workers.craftsmen + workers.clerks) * workers.craftsmen);
		newWorkers.clerks = static_cast<int>((factoryLevel * 10000.0f) / (workers.craftsmen + workers.clerks) * workers.clerks);
		newWorkers.artisans = workers.artisans;
	}
	else
	{
		newWorkers = workers;
	}

	return newWorkers;
}


int Vic2::State::determineEmployedWorkersScore(const workerStruct& workers) const
{
	int employedWorkerScore = workers.craftsmen + (workers.clerks * 2) + static_cast<int>(workers.artisans * 0.5) + (workers.capitalists * 2);
	if (ownerHasNoCores())
	{
		employedWorkerScore /= 2;
	}

	return employedWorkerScore;
}


bool Vic2::State::ownerHasNoCores() const
{
	for (auto province: provinces)
	{
		for (auto country: province->getCores())
		{
			if (country->getTag() == owner)
			{
				return false;
			}
		}
	}

	return true;
}


void Vic2::State::determineIfPartialState()
{
	if (provinces.size() > 0)
	{
		for (auto expectedProvince: theStateDefinitions.getAllProvinces(*provinceNums.begin()))
		{
			if (provinceNums.count(expectedProvince) == 0)
			{
				partialState = true;
				break;
			}
		}
	}
}


int Vic2::State::getPopulation() const
{
	int population = 0;
	for (auto province: provinces)
	{
		population += province->getPopulation();
	}

	return population;
}


int Vic2::State::getAverageRailLevel() const
{
	int totalRailLevel = 0;
	for (auto province: provinces)
	{
		totalRailLevel += province->getRailLevel();
	}

	if (provinces.size() > 0)
	{
		return (totalRailLevel / provinces.size());
	}
	else
	{
		return 0;
	}
}

void Vic2::State::consolidateRgo(Vic2::State* other, int res)
{
        rgos[res] += other->rgos[res];
        other->rgos[res] = 0;
}


void Vic2::State::setRgo()
{
        double weights[kNumResources] = {0, 0, 0, 0, 0, 0};
        static const double kAdjust[kNumResources] = {1.0, 1.0, 1.0,
                                                      1.0, 1.0, 1.0};
        static const std::unordered_map<std::string, std::unordered_map<int, double>> kWeights = {
          {"iron", {{kSteel, 5.0}}},
          {"coal", {{kSteel, 2.5}, {kOil, 1.5}}},
          {"oil", {{kOil, 5.0}}},
          {"timber", {{kSteel, 1.0}}},
          {"tropical_wood", {{kOil, 1.0}, {kAluminium, 0.6}}},
          {"grain", {{kOil, 0.3}}},
          {"fish", {{kOil, 0.3}}},
          {"rubber", {{kRubber, 5.0}}},
          {"silk", {{kRubber, 1.0}}},
          {"tea", {{kRubber, 1.0}}},
          {"precious_metal", {{kAluminium, 5.0}}},
          {"opium", {{kAluminium, 1.0}}},
          {"wool", {{kAluminium, 1.4}}},
          {"dye", {{kTungsten, 15.0}}},
          {"cotton", {{kTungsten, 1.5}}},
          {"coffee", {{kTungsten, 0.75}}},
          {"tobacco", {{kTungsten, 0.75}}},
          {"sulphur", {{kChromium, 5.0}}},
          {"fruit", {{kChromium, 0.6}}},
          {"cattle", {{kChromium, 0.6}}},
        };
	for (auto province: provinces)
	{
                double population = province->getTotalPopulation() * (1.0 / 250000);
                if (kWeights.find(province->getRgo()) == kWeights.end()) {
                        LOG(LogLevel::Warning)
                            << "Province " << province->getIdentifier()
                            << " has unweighted RGO type "
                            << province->getRgo();
                        continue;
                }
                for (const auto& weight : kWeights.at(province->getRgo()))
                {
                        weights[weight.first] += population * weight.second;
                }
                LOG(LogLevel::Info)
                    << "Province " << province->getIdentifier() << " rgo "
                    << province->getRgo() << " population " << population
                    << " aluminium " << weights[kAluminium];
        }

        for (int i = 0; i < kNumResources; ++i)
        {
                rgos[i] = weights[i] * kAdjust[i];
        }
}
