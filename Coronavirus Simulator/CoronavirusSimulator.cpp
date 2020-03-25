#include "plugin.h"
#include "CTheZones.h"
#include "CMessages.h"
#include "CPopCycle.h"
#include "CClock.h"
#include "CGangWars.h"
#include "CGeneral.h"
#include "CPopulation.h"
#include "CCarCtrl.h"
#include "CTimer.h"
#include "CPathFind.h"
#include "CStreaming.h"
#include "CCheat.h"
#include "TestCheat.h"
#include "CMenuManager.h"
#include "extensions/ScriptCommands.h"

using namespace plugin;
using namespace injector;

CZoneExtraInfo* ZoneExtraInfoArray = (CZoneExtraInfo*)0xBA1DF0;
char *popcyclePeds = (char*)0xC0E798;
float *carDensityMult = (float*)0x8A5B20;
void Cough(CPed *ped, bool random);
int GetInfectThisZone(CZoneExtraInfo* zoneExtraInfo);
void IncreaseInfectThisZone(CZoneExtraInfo* zoneExtraInfo, float increaseFactor);
bool IsZoneValidToInfect(CZone *zone, CZoneExtraInfo *zoneExtraInfo);
void DecreaseInfectThisZone(CZoneExtraInfo* zoneExtraInfo, float decreaseFactor);

class PedData {
public:
	bool infected;
	bool infectedJustNow;
	unsigned int lastCough;

	PedData(CPed *ped) {
		infected = false;
		infectedJustNow = false;
		lastCough = 0;
	}
};
PedExtendedData<PedData> pedData;

char buffer[100];
const int maxValue = 100;

void LoadSceneForPathNodes(const CVector& point)
{
	return ((void (__stdcall*)(float x, float y, float z)) 0x44DE00)(point.x, point.y, point.z);
}

class CoronavirusSimulator {
public:
	CoronavirusSimulator() {

		static bool someoneIsInfected;
		static unsigned int curIndex;
		static bool getFirstTime = true;
		static int lastHour = 0;
		static int lastDay = 0;
		static float increaseFactor = 1.0f;
		static float lastIncreaseLoop = 1.0f;
		static float increaseLoop;
		static float loopMult = 1.0f;
		static float percentMult = 0.7f;
		static float travelChance = 1.4f;
		static float outerMaxDistance = 500.0f;
		static float innerDistance = -50.0f;
		static float decreaseFactor = 1.0f;
		static float chanceToDecrease = 0.0f;
		static float chanceToDecreaseFactor = 1.75f;
		static float chanceToDecreaseMax = 30.0f;
		static bool dayChanged = false;

		// Infect zone during ped despawn
		Events::pedDtorEvent += [](CPed *ped) {
			if (pedData.Get(ped).infectedJustNow) {
				CZone *curZone = CTheZones::FindSmallestZoneForPosition(ped->GetPosition(), false);
				CZoneExtraInfo *curZoneExtraInfo = &ZoneExtraInfoArray[curZone->m_nZoneExtraIndexInfo];
				if (IsZoneValidToInfect(curZone, curZoneExtraInfo)) {
					// Only if this zone is 0, otherwise it will increase too much during peds despawn
					if (GetInfectThisZone(curZoneExtraInfo) == 0) {
						IncreaseInfectThisZone(curZoneExtraInfo, 1);
					}
				}
			}
		};

		Events::pedRenderEvent.before += [](CPed *ped) {
			if (ped->m_nPedType == 0) {
				if (CPad::GetPad(0)->GetDisplayVitalStats(ped) && CPad::GetPad(0)->ConversationNoJustDown()) {
					Cough(ped, false);
				}
			}
			else {
				if (pedData.Get(ped).lastCough == 0) {
					// Infect chance during spawn based on zone infection density
					CZone *curZone = CTheZones::FindSmallestZoneForPosition(ped->GetPosition(), false);
					CZoneExtraInfo *curZoneExtraInfo = &ZoneExtraInfoArray[curZone->m_nZoneExtraIndexInfo];

					int infectDensity = GetInfectThisZone(curZoneExtraInfo);
					if (infectDensity > 0) {
						if (CGeneral::GetRandomNumberInRange(0, maxValue) < infectDensity) {
							pedData.Get(ped).infected = true;
						}
					}
					pedData.Get(ped).lastCough = CTimer::m_snTimeInMilliseconds;
				}
				if (pedData.Get(ped).infected) {
					Cough(ped, true);
				}
			}
			//pedData.Get(ped).lastCough = CTimer::m_snTimeInMilliseconds;
		};


		Events::gameProcessEvent.after += [] {

			if (getFirstTime)
			{
				for (int i = 0; i < CTheZones::TotalNumberOfInfoZones; ++i) {
					CZoneExtraInfo* zoneExtraInfo = &ZoneExtraInfoArray[CTheZones::ZoneInfoArray[i].m_nZoneExtraIndexInfo];
					for (int gangId = 0; gangId < 9; ++gangId)
					{
						zoneExtraInfo->m_nGangDensity[gangId] = 0;
					}
				}
				CGangWars::bGangWarsActive = true;
				CTheZones::FillZonesWithGangColours(false);

				lastHour = CClock::ms_nGameClockHours;
				lastDay = CClock::ms_nGameClockDays;
				getFirstTime = false;

				return;
			}

			if (TestCheat("QUAR0")) { CPopulation::PedDensityMultiplier = 1.0f; *carDensityMult = 1.0f; CMessages::AddMessageJumpQ("Quarantine Level 0", 2000, 0, 0); }
			if (TestCheat("QUAR1")) { CPopulation::PedDensityMultiplier = 0.7f; *carDensityMult = 0.8f; CMessages::AddMessageJumpQ("Quarantine Level 1", 2000, 0, 0); }
			if (TestCheat("QUAR2")) { CPopulation::PedDensityMultiplier = 0.5f; *carDensityMult = 0.6f; CMessages::AddMessageJumpQ("Quarantine Level 2", 2000, 0, 0); }
			if (TestCheat("QUAR3")) { CPopulation::PedDensityMultiplier = 0.3f; *carDensityMult = 0.4f; CMessages::AddMessageJumpQ("Quarantine Level 3", 2000, 0, 0); }
			if (TestCheat("QUAR4")) { CPopulation::PedDensityMultiplier = 0.15f; *carDensityMult = 0.2f; CMessages::AddMessageJumpQ("Quarantine Level 4", 2000, 0, 0); }
			if (TestCheat("QUAR5")) { CPopulation::PedDensityMultiplier = 0.05f; *carDensityMult = 0.1f; CMessages::AddMessageJumpQ("Quarantine Level 5", 2000, 0, 0); }
			if (TestCheat("QUAR6")) { CPopulation::PedDensityMultiplier = 0.025f; *carDensityMult = 0.05f; CMessages::AddMessageJumpQ("Quarantine Level 6", 2000, 0, 0); }
			if (TestCheat("QUAR7")) { CPopulation::PedDensityMultiplier = 0.0f; *carDensityMult = 0.0f; CMessages::AddMessageJumpQ("Quarantine Total", 2000, 0, 0); }

			increaseLoop = 0.0f;

			int hour = CClock::ms_nGameClockHours;
			int day = CClock::ms_nGameClockDays;

			int hourDiff = hour - lastHour;
			int dayDiff = day - lastDay;

			if (hourDiff != 0 || dayDiff > 0)
			{
				dayChanged = (dayDiff > 0);
				if (hourDiff <= 0) hourDiff += (24 * dayDiff);
				if (hourDiff > 0)
				{
					curIndex = 0;

					increaseLoop = hourDiff * loopMult;
					lastIncreaseLoop = increaseLoop;
				}
			}

			/*CPlayerPed *playerPed = FindPlayerPed(0);
			CZone *curZone = CTheZones::FindSmallestZoneForPosition(playerPed->GetPosition(), false);
			CZoneExtraInfo* zoneExtraInfo = &ZoneExtraInfoArray[curZone->m_nZoneExtraIndexInfo];

			int popZoneType = zoneExtraInfo->m_nFlags & 0x1F;
			sprintf(buffer, "%i", popZoneType);
			CMessages::AddMessageJumpQ(buffer, 1, 0, 0);

			if (IsZoneValidToInfect(curZone, zoneExtraInfo))
			{
				sprintf(buffer, "ok");
				CMessages::AddMessageJumpQ(buffer, 1, 0, 0);
			}*/

			lastHour = hour;
			lastDay = day;

			if (curIndex > 0) increaseLoop = lastIncreaseLoop; // continuing last

			if ((increaseLoop > 0.0f && !CCheat::m_aCheatsActive[74])) { // GHOSTTOWN

				bool updateColors = false;
				if (curIndex == 0) someoneIsInfected = false; // reset

				while (increaseLoop > 0.0f) {
					int i;
					for (i = curIndex; i < CTheZones::TotalNumberOfInfoZones; ++i) {

						if (i == (unsigned int)(CTheZones::TotalNumberOfInfoZones * 0.25f)) break;
						if (i == (unsigned int)(CTheZones::TotalNumberOfInfoZones * 0.50f)) break;
						if (i == (unsigned int)(CTheZones::TotalNumberOfInfoZones * 0.75f)) break;

						CZone *zone = &CTheZones::ZoneInfoArray[i];
						CZoneExtraInfo* zoneExtraInfo = &ZoneExtraInfoArray[zone->m_nZoneExtraIndexInfo];

						// Only infect if someone is infected here (dur!)
						int numInfected = GetInfectThisZone(zoneExtraInfo);
						while (numInfected > 0)
						{
							someoneIsInfected = true;

							// Get inner and outer distance, considering car density multiplier
							if (*carDensityMult > 2.0f) *carDensityMult = 2.0f;
							float outerDistance = outerMaxDistance * *carDensityMult;
							if (outerDistance < 0.0f) outerDistance = 0.0f;
							float distance = CGeneral::GetRandomNumberInRange(innerDistance, outerDistance);

							// Get random coord including distance limit from zone borders
							CVector pos;
							bool isTravel = false;

							if (CGeneral::GetRandomNumberInRange(0.0f, 100.0f) < (*carDensityMult * travelChance)) {
								isTravel = true;
								pos.x = CGeneral::GetRandomNumberInRange(-3000.0f, 3000.0f);
								pos.y = CGeneral::GetRandomNumberInRange(-3000.0f, 3000.0f);
							}
							else {
								pos.x = CGeneral::GetRandomNumberInRange(zone->m_fX1 - distance, zone->m_fX2 + distance);
								pos.y = CGeneral::GetRandomNumberInRange(zone->m_fY1 - distance, zone->m_fY2 + distance);
							}
							pos.z = 0.0f;

							// Get zone there, can be the same as original one
							CZone *curZone = CTheZones::FindSmallestZoneForPosition(pos, false);
							if (curZone && abs(curZone->m_fX1 - curZone->m_fX2) < 1000.0f) {
								CZoneExtraInfo* curZoneExtraInfo = &ZoneExtraInfoArray[curZone->m_nZoneExtraIndexInfo];

								// Only update if not completely infected
								if (GetInfectThisZone(curZoneExtraInfo) < maxValue)
								{

									// Randomize the chance to infect new people based on num of peds from popcycle, and ped density multiplier
									int popZoneType = curZoneExtraInfo->m_nFlags & 0x1F;
									int numPeds = popcyclePeds[20 * (CPopCycle::m_nCurrentTimeOfWeek + 2 * CPopCycle::m_nCurrentTimeIndex) + popZoneType];
									numPeds *= exp(CPopulation::PedDensityMultiplier * 0.5f) / 1.7f;

									if (isTravel) numPeds *= 2; // increase chances if it's just a travel

									if (CGeneral::GetRandomNumberInRange(0.0f, 100.0f) < (float)(numPeds * percentMult)) {
										if (IsZoneValidToInfect(curZone, curZoneExtraInfo)) {
											IncreaseInfectThisZone(curZoneExtraInfo, increaseFactor);
											updateColors = true;
										}
									}
								}
							}
							
							// We can also decrease it...
							if (CGeneral::GetRandomNumberInRange(0.0f, 100.0f) < chanceToDecrease) {
								DecreaseInfectThisZone(zoneExtraInfo, decreaseFactor);
								updateColors = true;
							}

							numInfected -= 10;
						}
					}

					if (i >= CTheZones::TotalNumberOfInfoZones) {
						curIndex = 0;

						// Increase the chance to decrease the infection is someone is infected, vice versa
						if (dayChanged)
						{
							if (someoneIsInfected) {
								//sprintf(buffer, "increasing %f", chanceToDecrease);
								//CMessages::AddMessageJumpQ(buffer, 500, 0, 0);

								chanceToDecrease += chanceToDecreaseFactor;
								if (chanceToDecrease > chanceToDecreaseMax) chanceToDecrease = chanceToDecreaseMax;
							}
							else {
								//sprintf(buffer, "DEcreasing %f", chanceToDecrease);
								//CMessages::AddMessageJumpQ(buffer, 500, 0, 0);

								chanceToDecrease -= (chanceToDecreaseFactor * 6.0f);
								if (chanceToDecrease < 0.0f) chanceToDecrease = 0.0f;
								updateColors = true;
							}
							dayChanged = false;
						}
					}
					else {
						curIndex = i + 1;
					}
					increaseLoop -= 1.0f;
				}

				if (updateColors) CTheZones::FillZonesWithGangColours(false);
			}
		};
		 
		Events::reInitGameEvent += [] {
			getFirstTime = true;
		};

    }


} coronavirusSimulator;

bool IsZoneValidToInfect(CZone *zone, CZoneExtraInfo *zoneExtraInfo) {

	// We can trust in some popcycle groups
	int popZoneType = zoneExtraInfo->m_nFlags & 0x1F;
	if (popZoneType == 12 || popZoneType == 7 && popZoneType == 0 && popZoneType == 8) return true;

	CVector centerPos;

	centerPos.x = ((zone->m_fX1 + zone->m_fX2) / 2.0f);
	centerPos.y = ((zone->m_fY1 + zone->m_fY2) / 2.0f);
	centerPos.z = 0.0f;

	//02C0: get_closest_char_node 141@ 142@ 143@ store_to 137@ 138@ 139@
	CVector pathPos;

	if (DistanceBetweenPoints(centerPos, FindPlayerPed(0)->GetPosition()) > 1000.0f) {
		LoadSceneForPathNodes(centerPos);
		CStreaming::LoadAllRequestedModels(0);
	}

	Command<Commands::GET_CLOSEST_CHAR_NODE>(centerPos.x, centerPos.y, centerPos.z, &pathPos.x, &pathPos.y, &pathPos.z);

	if (centerPos.x == 0.0f) return false;

	if (DistanceBetweenPoints(centerPos, pathPos) > 200.0f) return false;
	
	if (CTheZones::PointLiesWithinZone(&centerPos, zone)) return true;
	return false;
}

int GetInfectThisZone(CZoneExtraInfo* zoneExtraInfo) {
	return zoneExtraInfo->m_nGangDensity[9];
}

void IncreaseInfectThisZone(CZoneExtraInfo* zoneExtraInfo, float increaseFactor) {
	zoneExtraInfo->m_nGangDensity[9] += (int)increaseFactor;
	if (zoneExtraInfo->m_nGangDensity[9] > maxValue) zoneExtraInfo->m_nGangDensity[9] = maxValue;
}

void DecreaseInfectThisZone(CZoneExtraInfo* zoneExtraInfo, float decreaseFactor) {
	zoneExtraInfo->m_nGangDensity[9] -= (int)decreaseFactor;
	if (zoneExtraInfo->m_nGangDensity[9] < 0) zoneExtraInfo->m_nGangDensity[9] = 0;
}

void Cough(CPed *ped, bool random) {
	if ((CTimer::m_snTimeInMilliseconds - pedData.Get(ped).lastCough) > 3000)
	{
		if (random)
		{
			if (CGeneral::GetRandomNumberInRange(0, 10) > 7) return;
		}
		Command<Commands::SET_CHAR_SAY_CONTEXT_IMPORTANT>(ped, 340);
		pedData.Get(ped).lastCough = CTimer::m_snTimeInMilliseconds;
		if (!ped->m_nPedFlags.bInVehicle) {
			for (int i = 0; i < 16; ++i)
			{
				CPed *closePed = (CPed*)ped->m_pIntelligence->m_pedScanner.m_apEntities[i];
				if (closePed && !closePed->m_nPedFlags.bInVehicle && !closePed->IsPlayer() && !closePed->m_nPedFlags.bFadeOut && !pedData.Get(closePed).infected && !pedData.Get(closePed).infectedJustNow) {
					if (DistanceBetweenPoints(closePed->GetPosition(), ped->GetPosition()) < 5.0f)
					{
						pedData.Get(closePed).infectedJustNow = true;
						break;
					}
				}
				closePed = nullptr;
			}
		}
	}
}
