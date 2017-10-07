#pragma once
#include "stdafx.h"
#include "Blackboard.h"
#include "BehaviorTree.h"
#include "AI/SteeringBehaviours/SteeringBehaviours.h"

#pragma region VARIABLES
//File-local variables, not exported as symbol when compiled

//Max values
static const int MaxHealth = 10;
static const int MaxEnergy = 20;

//Critical values
static const float CriticalHealthTreshold = 0.25f;
static const float CriticalEnergyTreshold = 0.2f;

//House searching wall offset
static const b2Vec2 HouseWallOffset = b2Vec2(5.f, 5.f);

//World offset
static const b2Vec2 WorldEdgeOffset = b2Vec2(25.f, 25.f);
#pragma endregion

#pragma region MATH FOR NORMALIZING
/*
 * MATH EQUATIONS USED IN NORMALIZING of health and stuff
 */

 //x lower = y higher (less health = more important)
inline float NormalizeLogarithmicInverse(const float val, const float e = 2)
{
	return pow(1 - val, e);
}
#pragma endregion

#pragma region CONDITIONS
/*
 * CONDITIONS
 */

#pragma region Core Stats
 //Core stats
inline bool IsHealthCritical(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	auto valid = pBlackboard->GetData("AgentInfo", agentInfo);

	if (!valid) return false;

	float hpNeed = NormalizeLogarithmicInverse(agentInfo.Health / MaxHealth);
	if (hpNeed >= 0.65)
	{
		printf("Health critical\n");
		return true;
	}

	return false;
}
inline bool IsEnergyCritical(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	auto valid = pBlackboard->GetData("AgentInfo", agentInfo);

	if (!valid) return false;

	float energyNeed = NormalizeLogarithmicInverse(agentInfo.Energy / MaxEnergy);
	if (energyNeed >= 0.7)
		return true;

	return false;
}

//Eat whenever we're missing at least enough for not a single energy from food to go wasted
//Clear inv as much as we can
inline bool NotMaxHealth(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	auto valid = pBlackboard->GetData("AgentInfo", agentInfo);

	if (!valid) return false;

	return (agentInfo.Health != MaxHealth);
}
inline bool NotMaxEnergy(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	auto valid = pBlackboard->GetData("AgentInfo", agentInfo);

	if (!valid) return false;

	return (agentInfo.Energy != MaxEnergy);
}
#pragma endregion

#pragma region Items
//
//Items
//
inline bool HasTargetItem(Blackboard* pBlackboard)
{
	//If the current targetitem isnt taken, it means we .. well... havent taken it yet
	//Grab it
	TargetItem item;
	auto valid = pBlackboard->GetData("TargetItem", item);

	if (!valid)
		return false;

	if (!item.m_Valid)
		return false;

	return !item.m_Taken;
}
#pragma endregion

#pragma region Houses
//
//Houses
//
inline bool HasTargetHouse(Blackboard* pBlackboard)
{
	//Init data to null
	House targetHouse;
	auto dataAvailable = pBlackboard->GetData("CurrentHouse", targetHouse);

	//Check if valid
	if (!dataAvailable || targetHouse.m_Checked)
		return false;

	return true;
}
inline bool InsideTargetHouse(Blackboard* pBlackboard)
{
	//Check if we're inside the house that we wanted to go into
	House targetHouse;
	AgentInfo agentInfo;
	auto dataAvailable = pBlackboard->GetData("CurrentHouse", targetHouse)
		&& pBlackboard->GetData("AgentInfo", agentInfo);

	//Check if valid
	if (!dataAvailable || targetHouse.m_Checked)
		return false;

	//Check if inside the house
	if (PointInRectangle(agentInfo.Position, targetHouse.m_HouseInfo.Center, targetHouse.m_HouseInfo.Size))
		return true;

	//Otherwise, we haven't reached the entrance yet so update our entrance position with the current position
	pBlackboard->ChangeData("HouseEntrance", agentInfo.Position);

	return false;
}
//Search optimizing
inline bool HouseBigEnough(Blackboard* pBlackboard)
{
	//Is the house big enough to warrant corner checking
	House currentHouse;
	AgentInfo agentInfo;
	auto dataAvailable = pBlackboard->GetData("CurrentHouse", currentHouse)
		&& pBlackboard->GetData("AgentInfo", agentInfo);

	if (!dataAvailable)
		return false;

	//Check size
	if ((currentHouse.m_HouseInfo.Size.x / 2.f) >= agentInfo.FOV_Range)
		return false;
	if ((currentHouse.m_HouseInfo.Size.y / 2.f) >= agentInfo.FOV_Range)
		return false;

	printf("[House] Performing full house sweep.\n");

	return true;
}
//Not found a house in x seconds
inline bool NoDiscoveryInTime(Blackboard* pBlackboard)
{
	return true;
}
#pragma endregion
#pragma endregion

#pragma region ACTIONS
/*
 * ACTIONS
 */

#pragma region Core Stats
 //
 //Core stats (health and energy)
 //
inline BehaviorState UseAnyHealthKit(Blackboard* pBlackboard)
{
	ExamPlugin* plugin;
	bool valid = pBlackboard->GetData("Plugin", plugin);

	if (!valid)
		return Failure;

	//Find the food in our inventory
	for (auto i = 0; i < plugin->INVENTORY_GetCapacity(); ++i)
	{
		ItemInfo item;
		if (plugin->INVENTORY_GetItem(i, item))
		{
			if (item.Type == HEALTH)
			{
				plugin->INVENTORY_UseItem(i);
				plugin->INVENTORY_RemoveItem(i);
				printf("[Item] Used an emergency healthpack.\n");
				return Success;
			}
		}
	}

	return Failure;
}
inline BehaviorState UseAnyFood(Blackboard* pBlackboard)
{
	ExamPlugin* plugin;
	bool valid = pBlackboard->GetData("Plugin", plugin);

	if (!valid)
		return Failure;

	//Find the food in our inventory
	for (auto i = 0; i < plugin->INVENTORY_GetCapacity(); ++i)
	{
		ItemInfo item;
		if (plugin->INVENTORY_GetItem(i, item))
		{
			if (item.Type == FOOD)
			{
				plugin->INVENTORY_UseItem(i);
				plugin->INVENTORY_RemoveItem(i);
				printf("[Item] Ate some emergency food.\n");
				return Success;
			}
		}
	}

	return Failure;
}
inline BehaviorState UseBestHealthKit(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	ExamPlugin* plugin;
	bool valid = pBlackboard->GetData("Plugin", plugin)
		&& pBlackboard->GetData("AgentInfo", agentInfo);

	if (!valid)
		return Failure;

	//Find the healthpack in our inventory
	//Use the one that gets us closest to the max hp but not over
	int bestSlot = -1;
	int bestAmount = -10;

	for (auto i = 0; i < plugin->INVENTORY_GetCapacity(); ++i)
	{
		ItemInfo item;
		if (plugin->INVENTORY_GetItem(i, item))
		{
			if (item.Type == HEALTH)
			{
				int amount = 0;
				if (plugin->ITEM_GetMetadata(item, "health", amount))
				{
					int left = (MaxHealth - agentInfo.Health - amount);
					if (left >= bestAmount && left >= 0)
					{
						bestSlot = i;
						bestAmount = left;
					}
				}
			}
		}
	}

	if (bestSlot != -1)
	{
		plugin->INVENTORY_UseItem(bestSlot);
		plugin->INVENTORY_RemoveItem(bestSlot);
		printf("[Item] Used a healthkit.\n");
		return Success;
	}

	return Failure;
}
inline BehaviorState UseBestFood(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	ExamPlugin* plugin;
	bool valid = pBlackboard->GetData("Plugin", plugin)
		&& pBlackboard->GetData("AgentInfo", agentInfo);

	if (!valid)
		return Failure;

	//Find the food in our inventory
	//Use the one that gets us closest to the max hp but not over
	int bestSlot = -1;
	int bestAmount = -20;

	for (auto i = 0; i < plugin->INVENTORY_GetCapacity(); ++i)
	{
		ItemInfo item;
		if (plugin->INVENTORY_GetItem(i, item))
		{
			if (item.Type == FOOD)
			{
				int amount = 0;
				if (plugin->ITEM_GetMetadata(item, "energy", amount))
				{
					int left = (MaxEnergy - agentInfo.Energy - amount);
					if (left >= bestAmount && left >= 0)
					{
						bestSlot = i;
						bestAmount = left;
					}
				}
			}
		}
	}

	if (bestSlot != -1)
	{
		plugin->INVENTORY_UseItem(bestSlot);
		plugin->INVENTORY_RemoveItem(bestSlot);
		printf("[Item] Ate some food.\n");
		return Success;
	}

	return Failure;
}
#pragma endregion

#pragma region Items
//
//Items
//
inline BehaviorState SpotNewItem(Blackboard* pBlackboard)
{
	vector<EntityInfo> items;
	auto valid = pBlackboard->GetData("Items", items);

	if (!valid)
		return Failure;

	int vecSize = items.size();
	if (vecSize <= 0)
		return Failure;

	//Get the last item
	TargetItem targetItem;
	targetItem.m_EntityInfo = items[vecSize - 1];
	targetItem.m_Valid = true;

	//Change targetitem
	pBlackboard->ChangeData("TargetItem", targetItem);

	printf("[Item] Moving to pick up a new item.\n");
	return Success;
}
inline BehaviorState SetItemAsTarget(Blackboard* pBlackboard)
{
	b2Vec2 target;
	TargetItem item;

	auto valid = pBlackboard->GetData("TargetItem", item);

	if (!valid || item.m_Taken)
		return Failure;

	printf("[Item] Moving to item\n");

	target = item.m_EntityInfo.Position;
	pBlackboard->ChangeData("Target", target);

	return Success;
}
inline BehaviorState PickupItem(Blackboard* pBlackboard)
{
	TargetItem item;
	AgentInfo agentInfo;
	auto valid = pBlackboard->GetData("TargetItem", item)
		&& pBlackboard->GetData("AgentInfo", agentInfo);

	if (!valid || item.m_Taken || !item.m_Valid)
		return Failure;

	//Move to pick up the item
	if (abs(agentInfo.Position - item.m_EntityInfo.Position).LengthSquared() < agentInfo.GrabRange * agentInfo.GrabRange)
	{
		ExamPlugin* plugin;
		pBlackboard->GetData("Plugin", plugin);

		//Grab the item info
		ItemInfo itemInfo;
		bool validItem = plugin->ITEM_Grab(item.m_EntityInfo, itemInfo);

		if (validItem)
		{
			bool pickedUp = false;

			int freeSlot = -1;
			ItemInfo slotItem;

#pragma region EmptySlotCheck
			//If we have an empty slot, put it in there
			//Save last spot for discarding useless item to force them to respawn (and potentially become useful)
			for (int slot = 0; slot < plugin->INVENTORY_GetCapacity() - 1; ++slot)
			{
				if (!plugin->INVENTORY_GetItem(slot, slotItem))
				{
					freeSlot = slot;
					break;
				}
				else
				{
					//Replace garbage and guns in our inventory, we dont care for those
					if (slotItem.Type == GARBAGE || slotItem.Type == PISTOL)
					{
						printf("[Item] Replacing useless item in inventory with something else.\n");
						plugin->INVENTORY_RemoveItem(slot);
						freeSlot = slot;
						break;
					}
				}
			}
#pragma endregion

			//If there was a free slot, put the new item in there
			if (freeSlot != -1)
			{
				if (plugin->INVENTORY_AddItem(freeSlot, itemInfo))
					pickedUp = true;
				if (itemInfo.Type == GARBAGE || itemInfo.Type == PISTOL)
				{
					plugin->INVENTORY_RemoveItem(freeSlot);
					printf("[Item] New item was useless, removed it.\n");
				}
			}
			//Otherwise, start weighing what item will be more useful
			else
			{
				//Check if there's an item we don't need
				//If our health is full, we will discard a heathkit in exchange for food
				//If our health isn't full and the item is health, force use some health
				//If our food isn't full but we have some food and the item itself is food, force eat food
				//If its a gun or trash, pick it up an discard it to force the item to respawn

				switch (itemInfo.Type)
				{
				case PISTOL:
					plugin->INVENTORY_AddItem(plugin->INVENTORY_GetCapacity(), itemInfo);
					plugin->INVENTORY_RemoveItem(plugin->INVENTORY_GetCapacity());
					printf("[Item] Found a pistol and discarded it.\n");
					break;
				case FOOD:
					printf("[Item] Found food.\n");
					if (UseAnyFood(pBlackboard) == Success)
					{
						PickupItem(pBlackboard);
					}
					else
					{
						if (UseAnyHealthKit(pBlackboard) == Success)
						{
							PickupItem(pBlackboard);
						}
						else
						{
							plugin->INVENTORY_AddItem(plugin->INVENTORY_GetCapacity(), itemInfo);
							plugin->INVENTORY_RemoveItem(plugin->INVENTORY_GetCapacity());
							printf("[Item] Discarded excess food.\n");
						}
					}
					break;
				case HEALTH:
					printf("[Item] Found health.\n");
					if (UseAnyHealthKit(pBlackboard) == Success)
					{
						PickupItem(pBlackboard);
					}
					else
					{
						plugin->INVENTORY_AddItem(plugin->INVENTORY_GetCapacity(), itemInfo);
						plugin->INVENTORY_RemoveItem(plugin->INVENTORY_GetCapacity());
						printf("[Item] Discarded excess healthkit.\n");
					}
					break;
				case GARBAGE:
					plugin->INVENTORY_AddItem(plugin->INVENTORY_GetCapacity(), itemInfo);
					plugin->INVENTORY_RemoveItem(plugin->INVENTORY_GetCapacity());
					printf("[Item] Found junk and discarded it.\n");
					break;
				default:
					break;
				}
			}

			//Set the current TargetItem to picked up if we did
			item.m_Taken = pickedUp;
			pBlackboard->ChangeData("TargetItem", item);

			//Remove it from our backlog of items to go for
			if (pickedUp)
			{
				vector<EntityInfo> itemsLog;
				if (pBlackboard->GetData("Items", itemsLog))
				{
					for (auto it = itemsLog.begin(); it != itemsLog.end(); ++it)
					{
						if (it->Position == item.m_EntityInfo.Position)
						{
							printf("[Item] Cleared item from backlog.\n");
							itemsLog.erase(itemsLog.end() - 1);
							pBlackboard->ChangeData("Items", itemsLog);
							break;
						}
					}

					//Couldn't find it, something broke, reset the itemslog to prevent getting stuck
					itemsLog.clear();
					pBlackboard->ChangeData("Items", itemsLog);
				}
			}

			printf("[Item] Picked up an item.\n");
			return Success;
		}

		//It failed, so its an FOV problem
		//Ignore the item for now to avoid getting stuck
		item.m_Taken = true;
		pBlackboard->ChangeData("TargetItem", item);

		vector<EntityInfo> itemsLog;
		if (pBlackboard->GetData("Items", itemsLog))
		{
			printf("[Item] Cleared item from backlog to avoid getting stuck.\n");
			if (itemsLog.size() > 0) itemsLog.erase(itemsLog.end() - 1);
			pBlackboard->ChangeData("Items", itemsLog);
		}

		return Failure;
	}
	else
	{
		//Set target to go to the item
		b2Vec2 target = item.m_EntityInfo.Position;
		pBlackboard->ChangeData("Target", target);
	}

	return Running;
}
#pragma endregion

#pragma region Houses
//
//Houses
//
inline BehaviorState SetTargetHouse(Blackboard* pBlackboard)
{
	House targetHouse;

	//Get a new target house
	vector<House> houseLocations;
	auto dataAvailable = pBlackboard->GetData("HouseLocations", houseLocations);

	if (!dataAvailable)
		return Failure;

	int vecSize = houseLocations.size();
	if (vecSize <= 0)
		return Failure;

	//Check houses
	for (int i = vecSize - 1; i >= 0; --i)
	{
		if (!houseLocations[i].m_Checked)
		{
			pBlackboard->ChangeData("CurrentHouse", houseLocations[i]);
			return Success;
		}
	}

	return Failure;
}
inline BehaviorState SetHouseAsTarget(Blackboard* pBlackboard)
{
	//Init data to null
	House targetHouse;

	//Get the required data
	auto dataAvailable = pBlackboard->GetData("CurrentHouse", targetHouse);

	if (!dataAvailable)
		return Failure;
	if (targetHouse.m_Checked)
		return Failure;

	//Set the house center as the new target
	pBlackboard->ChangeData("Target", targetHouse.m_HouseInfo.Center);
	return Success;
}
//Searching for items
inline BehaviorState CheckHouseCenter(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	House currentHouse;
	auto dataAvailable = pBlackboard->GetData("AgentInfo", agentInfo)
		&& pBlackboard->GetData("CurrentHouse", currentHouse);

	if (!dataAvailable)
		return Failure;

	//If the agent is near the center, success!
	if (abs(currentHouse.m_HouseInfo.Center - agentInfo.Position).LengthSquared() <= 0.1f)
	{
		printf("[HOUSE] Center checked.\n");
		return Success;
	}

	//Else set the target
	pBlackboard->ChangeData("Target", currentHouse.m_HouseInfo.Center);
	return Running;
}
inline BehaviorState CheckTopLeftCorner(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	House currentHouse;
	auto dataAvailable = pBlackboard->GetData("AgentInfo", agentInfo)
		&& pBlackboard->GetData("CurrentHouse", currentHouse);

	if (!dataAvailable)
		return Failure;

	auto wallDistance = currentHouse.m_HouseInfo.Size / 2.f;
	auto corner = currentHouse.m_HouseInfo.Center - b2Vec2(wallDistance.x, -wallDistance.y) + b2Vec2(HouseWallOffset.x, -HouseWallOffset.y);

	//If the agent is near the topleft corner, success!
	if (abs(corner - agentInfo.Position).LengthSquared() <= agentInfo.FOV_Range * agentInfo.FOV_Range)
	{
		printf("[HOUSE] Top-left corner checked.\n");
		return Success;
	}

	//Else set the target
	pBlackboard->ChangeData("Target", corner);

	return Running;
}
inline BehaviorState CheckTopRightCorner(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	House currentHouse;
	auto dataAvailable = pBlackboard->GetData("AgentInfo", agentInfo)
		&& pBlackboard->GetData("CurrentHouse", currentHouse);

	if (!dataAvailable)
		return Failure;

	auto wallDistance = currentHouse.m_HouseInfo.Size / 2.f;
	auto corner = currentHouse.m_HouseInfo.Center + b2Vec2(wallDistance.x, wallDistance.y) - HouseWallOffset;

	//If the agent is near the topleft corner, success!
	if (abs(corner - agentInfo.Position).LengthSquared() <= agentInfo.FOV_Range * agentInfo.FOV_Range)
	{
		printf("[HOUSE] Top-right corner checked.\n");
		return Success;
	}

	//Else set the target
	pBlackboard->ChangeData("Target", corner);
	return Running;
}
inline BehaviorState CheckBottomRightCorner(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	House currentHouse;
	auto dataAvailable = pBlackboard->GetData("AgentInfo", agentInfo)
		&& pBlackboard->GetData("CurrentHouse", currentHouse);

	if (!dataAvailable)
		return Failure;

	auto wallDistance = currentHouse.m_HouseInfo.Size / 2.f;
	auto corner = currentHouse.m_HouseInfo.Center + b2Vec2(wallDistance.x, -wallDistance.y) + b2Vec2(-HouseWallOffset.x, HouseWallOffset.y);

	//If the agent is near the topleft corner, success!
	if (abs(corner - agentInfo.Position).LengthSquared() <= agentInfo.FOV_Range * agentInfo.FOV_Range)
	{
		printf("[HOUSE] Bottom-right corner checked.\n");
		return Success;
	}

	//Else set the target
	pBlackboard->ChangeData("Target", corner);
	return Running;
}
inline BehaviorState CheckBottomLeftCorner(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	House currentHouse;
	auto dataAvailable = pBlackboard->GetData("AgentInfo", agentInfo)
		&& pBlackboard->GetData("CurrentHouse", currentHouse);

	if (!dataAvailable)
		return Failure;

	auto wallDistance = currentHouse.m_HouseInfo.Size / 2.f;
	auto corner = currentHouse.m_HouseInfo.Center - b2Vec2(wallDistance.x, wallDistance.y) + HouseWallOffset;

	//If the agent is near the topleft corner, success!
	if (abs(corner - agentInfo.Position).LengthSquared() <= agentInfo.FOV_Range * agentInfo.FOV_Range)
	{
		printf("[HOUSE] Bottom-left corner checked.\n");
		return Success;
	}

	//Else set the target
	pBlackboard->ChangeData("Target", corner);
	return Running;
}
//Done checking
inline BehaviorState MarkHouseChecked(Blackboard* pBlackboard)
{
	//Init data to null
	House targetHouse;
	//Get the required data
	auto dataAvailable = pBlackboard->GetData("CurrentHouse", targetHouse);

	if (dataAvailable && !targetHouse.m_Checked)
	{
		targetHouse.m_Checked = true;
		vector<House> houseLocations;
		auto dataAvailable = pBlackboard->GetData("HouseLocations", houseLocations);

		if (dataAvailable)
		{
			size_t index = 0;

			//Find this house in our vector of houses
			for (auto it = houseLocations.begin(); it != houseLocations.end(); ++it)
			{
				if (targetHouse.m_HouseInfo.Center == it->m_HouseInfo.Center)
				{
					houseLocations[index].m_Checked = true;
					printf("[HOUSE] Current house marked as checked.\n");
					pBlackboard->ChangeData("CurrentHouse", targetHouse);
					pBlackboard->ChangeData("HouseLocations", houseLocations);
					return Success;
				}

				++index;
			}
		}
	}

	return Failure;
}
inline BehaviorState LeaveHouse(Blackboard* pBlackboard)
{
	//Leave the house back the way we came in
	AgentInfo agentInfo;
	b2Vec2 entryPoint;
	House currentHouse;
	auto dataAvailable = pBlackboard->GetData("HouseEntrance", entryPoint)
		&& pBlackboard->GetData("AgentInfo", agentInfo)
		&& pBlackboard->GetData("CurrentHouse", currentHouse);

	if (!dataAvailable)
		return Failure;

	b2Vec2 offset = b2Vec2_zero;

	//Is the entrance on the left or on the right?
	if (entryPoint.x > currentHouse.m_HouseInfo.Center.x)
		offset.x = 15;
	else
		offset.x = -15;

	//Is the entrance above or below
	if (entryPoint.y > currentHouse.m_HouseInfo.Center.y)
		offset.y = 15;
	else
		offset.y = -15;

	//If the agent is near the exit
	//Alternatively, if the actor is sufficiently out of the house
	if (abs((entryPoint + offset) - agentInfo.Position).LengthSquared() <= 5.f || abs(currentHouse.m_HouseInfo.Center - agentInfo.Position).LengthSquared() > ((currentHouse.m_HouseInfo.Size - currentHouse.m_HouseInfo.Center).LengthSquared() + 500.f))
	{
		printf("[HOUSE] Exited house.\n");
		return Success;
	}

	printf("Leaving house\n");

	//Set the target to the outside area
	pBlackboard->ChangeData("Target", entryPoint + offset);
	return Running;
}
#pragma endregion

#pragma region MapWandering
inline BehaviorState CheckWorldTopLeft(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	WorldInfo worldInfo;
	auto dataAvailable = pBlackboard->GetData("AgentInfo", agentInfo)
		&& pBlackboard->GetData("WorldInfo", worldInfo);

	if (!dataAvailable)
		return Failure;

	auto corner = worldInfo.Center + b2Vec2(-worldInfo.Dimensions.x / 2.f, worldInfo.Dimensions.y / 2.f) + b2Vec2(WorldEdgeOffset.x, -WorldEdgeOffset.y);

	//If the agent is near the topleft corner, success!
	if (abs(corner - agentInfo.Position).LengthSquared() <= 10.f)
	{
		printf("[WORLD] Topleft checked.\n");
		return Success;
	}

	printf("Checking world\n");

	//Else set the target
	pBlackboard->ChangeData("Target", corner);
	return Running;
}
inline BehaviorState CheckWorldTopRight(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	WorldInfo worldInfo;
	auto dataAvailable = pBlackboard->GetData("AgentInfo", agentInfo)
		&& pBlackboard->GetData("WorldInfo", worldInfo);

	if (!dataAvailable)
		return Failure;

	auto corner = worldInfo.Center + b2Vec2(worldInfo.Dimensions.x / 2.f, worldInfo.Dimensions.y / 2.f) + b2Vec2(-WorldEdgeOffset.x, -WorldEdgeOffset.y);

	//If the agent is near the topright corner, success!
	if (abs(corner - agentInfo.Position).LengthSquared() <= 10.f)
	{
		printf("[WORLD] Toprightchecked.\n");
		return Success;
	}

	//Else set the target
	pBlackboard->ChangeData("Target", corner);
	return Running;
}
inline BehaviorState CheckWorldBottomRight(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	WorldInfo worldInfo;
	auto dataAvailable = pBlackboard->GetData("AgentInfo", agentInfo)
		&& pBlackboard->GetData("WorldInfo", worldInfo);

	if (!dataAvailable)
		return Failure;

	auto corner = worldInfo.Center + b2Vec2(worldInfo.Dimensions.x / 2.f, -worldInfo.Dimensions.y / 2.f) + b2Vec2(-WorldEdgeOffset.x, WorldEdgeOffset.y);

	//If the agent is near the bottomright corner, success!
	if (abs(corner - agentInfo.Position).LengthSquared() <= 10.f)
	{
		printf("[WORLD] Bottomright checked.\n");
		return Success;
	}

	//Else set the target
	pBlackboard->ChangeData("Target", corner);
	return Running;
}
inline BehaviorState CheckWorldBottomLeft(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	WorldInfo worldInfo;
	auto dataAvailable = pBlackboard->GetData("AgentInfo", agentInfo)
		&& pBlackboard->GetData("WorldInfo", worldInfo);

	if (!dataAvailable)
		return Failure;

	auto corner = worldInfo.Center - b2Vec2(worldInfo.Dimensions.x / 2.f, worldInfo.Dimensions.y / 2.f) + b2Vec2(WorldEdgeOffset.x, WorldEdgeOffset.y);

	//If the agent is near the bottomleft corner, success!
	if (abs(corner - agentInfo.Position).LengthSquared() <= 10.f)
	{
		printf("[WORLD] Bottomleft checked.\n");
		return Success;
	}

	//Else set the target
	pBlackboard->ChangeData("Target", corner);
	return Running;
}
inline BehaviorState ResetHouses(Blackboard* pBlackboard)
{
	pBlackboard->ChangeData("HouseLocations", vector<House>{});
	return Success;
}
#pragma endregion

#pragma region Sprinting
BehaviorState StartSprinting(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	auto valid = pBlackboard->GetData("AgentInfo", agentInfo);

	if (!valid)
		return Failure;

	agentInfo.RunMode = true;
	pBlackboard->ChangeData("AgentInfo", agentInfo);

	return Success;
}
BehaviorState StopSprinting(Blackboard* pBlackboard)
{
	AgentInfo agentInfo;
	auto valid = pBlackboard->GetData("AgentInfo", agentInfo);

	if (!valid)
		return Failure;

	agentInfo.RunMode = false;
	pBlackboard->ChangeData("AgentInfo", agentInfo);

	return Success;
}
#pragma endregion

#pragma region GeneralBehaviour
//
//General
//
inline BehaviorState WanderAround(Blackboard* pBlackboard)
{
	//Init data to null
	SteeringBehaviours::Wander* pWanderBehaviour = nullptr;
	SteeringBehaviours::ISteeringBehaviour* pCurrentBehaviour = nullptr;

	//Get the required data
	auto dataAvailable = pBlackboard->GetData("CurrentBehaviour", pCurrentBehaviour)
		&& pBlackboard->GetData("WanderBehaviour", pWanderBehaviour);

	if (!dataAvailable)
		return Failure;
	if (!pWanderBehaviour)
		return Failure;

	//Perform action
	if (pCurrentBehaviour != pWanderBehaviour)
	{
		printf("[STEERING CHANGE] Setting behaviour to Wander.\n");
		pCurrentBehaviour = pWanderBehaviour;
		pBlackboard->ChangeData("CurrentBehaviour", pCurrentBehaviour);
	}

	return Success;
}
inline BehaviorState LookAroundGoToTarget(Blackboard* pBlackboard)
{
	//Init data to null
	SteeringBehaviours::LookAround* pLookAroundBehaviour = nullptr;
	SteeringBehaviours::ISteeringBehaviour* pCurrentBehaviour = nullptr;
	b2Vec2 target = b2Vec2_zero;

	//Get the required data
	auto dataAvailable = pBlackboard->GetData("CurrentBehaviour", pCurrentBehaviour)
		&& pBlackboard->GetData("LookAroundBehaviour", pLookAroundBehaviour)
		&& pBlackboard->GetData("Target", target);

	if (!dataAvailable)
		return Failure;
	if (!pLookAroundBehaviour)
		return Failure;

	pLookAroundBehaviour->SetTarget(target);

	//Perform action
	if (pCurrentBehaviour != pLookAroundBehaviour)
	{
		printf("[STEERING CHANGE] Setting behaviour to LookAround.\n");
		pCurrentBehaviour = pLookAroundBehaviour;
		pBlackboard->ChangeData("CurrentBehaviour", pCurrentBehaviour);
	}

	return Success;
}
inline BehaviorState GoToTarget(Blackboard* pBlackboard)
{
	//Init data to null
	SteeringBehaviours::Seek* pSeekBehaviour = nullptr;
	SteeringBehaviours::ISteeringBehaviour* pCurrentBehaviour = nullptr;
	b2Vec2 target = b2Vec2_zero;

	//Get the required data
	auto dataAvailable = pBlackboard->GetData("CurrentBehaviour", pCurrentBehaviour)
		&& pBlackboard->GetData("SeekBehaviour", pSeekBehaviour)
		&& pBlackboard->GetData("Target", target);

	if (!dataAvailable)
		return Failure;
	if (!pSeekBehaviour)
		return Failure;

	pSeekBehaviour->SetTarget(target);

	//Perform action
	if (pCurrentBehaviour != pSeekBehaviour)
	{
		printf("[STEERING CHANGE] Setting behaviour to Seek.\n");
		pCurrentBehaviour = pSeekBehaviour;
		pBlackboard->ChangeData("CurrentBehaviour", pCurrentBehaviour);
	}

	return Success;
}
inline BehaviorState ArriveAtTarget(Blackboard* pBlackboard)
{
	//Init data to null
	SteeringBehaviours::Arrive* pArriveBehaviour = nullptr;
	SteeringBehaviours::ISteeringBehaviour* pCurrentBehaviour = nullptr;
	b2Vec2 target = b2Vec2_zero;

	//Get the required data
	auto dataAvailable = pBlackboard->GetData("CurrentBehaviour", pCurrentBehaviour)
		&& pBlackboard->GetData("ArriveBehaviour", pArriveBehaviour)
		&& pBlackboard->GetData("Target", target);

	if (!dataAvailable || !pArriveBehaviour)
		return Failure;

	pArriveBehaviour->SetTarget(target);

	//Perform action
	if (pCurrentBehaviour != pArriveBehaviour)
	{
		printf("[STEERING CHANGE] Setting behaviour to Arrive.\n");
		pCurrentBehaviour = pArriveBehaviour;
		pBlackboard->ChangeData("CurrentBehaviour", pCurrentBehaviour);
	}

	return Success;
}
#pragma endregion
#pragma endregion
