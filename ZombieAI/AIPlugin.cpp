#include "stdafx.h"
#include "AIPlugin.h"

#include "AI/BehaviourTree/Blackboard.h"
#include "AI/BehaviourTree/BehaviorTree.h"
#include "AI/BehaviourTree/Behaviours.h"
#include "AI/SteeringBehaviours/CombinedSB_PipelineImpl.h"

#pragma region defines
//Define some stuff for our behavior tree to make it easier to write
#define SEL new BehaviorSelector({
#define SEQ new BehaviorSequence({
#define PSEQ new BehaviorPartialSequence({
#define ALWAYS new BehaviorAlwaysTrue({
#define RUNGOOD new BehaviorRunningIsGood({
#define ALL new BehaviorDoAll({
#define COND new BehaviorConditional({
#define ACTION new BehaviorAction({
#define ACTIONFAIL new BehaviorActionInverse({
#define END }),
#pragma endregion

//Current AI features:
//Discover houses
//Remember house locations
//Go into houses
//Check houses for items

//Current AI behavior point record:
//223 Level One

AIPlugin* AIPlugin::m_pInstance = nullptr;

AIPlugin* AIPlugin::GetInstance()
{
	if (!m_pInstance)
		m_pInstance = new AIPlugin();

	return m_pInstance;
}

AIPlugin::AIPlugin():IBehaviourPlugin(GameDebugParams(20,true,false, false,false,0.5f))
{
}

AIPlugin::~AIPlugin()
{
}

void AIPlugin::Start()
{	
	//Get the world info
	auto worldInfo = WORLD_GetInfo();
	auto agentInfo = AGENT_GetInfo();

#pragma region StartSteering
	//Create our steeringbehaviours and whatnot
	m_pSeekBehaviour = new SteeringBehaviours::Seek();
	m_pLookAroundBehaviour = new SteeringBehaviours::LookAround();
	m_pFallbackBehaviour = new SteeringBehaviours::Wander();
	m_pFallbackBehaviour->SetWanderRadius(5.f);
	m_pArriveBehaviour = new SteeringBehaviours::Arrive();
	m_pArriveBehaviour->SetSlowRadius(agentInfo.GrabRange);

	SteeringBehaviours::ISteeringBehaviour* pCurrBehaviour = nullptr;

	//Pipeline
	m_pDecomposer = new CombinedSB::NavMeshDecomposer();
	m_pActuator = new CombinedSB::BasicActuator(m_pLookAroundBehaviour);
	m_pConstraint = new CombinedSB::AvoidEnemyConstraint(m_VecEnemies, m_VecHouses);
	m_pTargeter = new CombinedSB::FixedGoalTargeter();
	m_pSteeringPipeline = new CombinedSB::SteeringPipeline();
	m_pSteeringPipeline->SetActuator(m_pActuator);
	m_pSteeringPipeline->SetDecomposers({ m_pDecomposer });
	m_pSteeringPipeline->SetFallBack(m_pFallbackBehaviour);
	m_pSteeringPipeline->SetConstraints({ m_pConstraint });
	m_pSteeringPipeline->SetTargeters({ m_pTargeter });
#pragma endregion

#pragma region StartBlackboard
	//Blackboard
	m_pBlackboard = new Blackboard();

	m_pBlackboard->AddData("Plugin", this);
	
	//Steering behaviours
	m_pBlackboard->AddData("WanderBehaviour", m_pFallbackBehaviour);
	m_pBlackboard->AddData("SeekBehaviour", m_pSeekBehaviour);
	m_pBlackboard->AddData("LookAroundBehaviour", m_pLookAroundBehaviour);
	m_pBlackboard->AddData("ArriveBehaviour", m_pArriveBehaviour);
	m_pBlackboard->AddData("CurrentBehaviour", pCurrBehaviour);
	m_pBlackboard->AddData("Target", b2Vec2_zero);

	//World info and agent info
	m_pBlackboard->AddData("WorldInfo", worldInfo);
	m_pBlackboard->AddData("AgentInfo", AgentInfo());

	//Discovery
	m_pBlackboard->AddData("LastDiscovery", 0.f);

	//Houses
	m_pBlackboard->AddData("HouseLocations", vector<House>{});
	m_pBlackboard->AddData("CurrentHouse", House());
	m_pBlackboard->AddData("HouseEntrance", b2Vec2_zero);

	//Items
	m_pBlackboard->AddData("Items", vector<EntityInfo>{});
	m_pBlackboard->AddData("TargetItem", TargetItem());

	//Enemies
	m_pBlackboard->AddData("Enemies", vector<EntityInfo>{});
#pragma endregion

#pragma region StartBehaviourTree
	//Make the behaviourtree
	m_pBehaviourTree = new BehaviorTree(m_pBlackboard,
	{
		SEQ
			//Always assume it's safe to stop sprinting, this will be changed in the same frame if it isn't and thus be fine
			ALWAYS
				ACTION(StopSprinting) END 
			END
 
			SEL
				//Use items if we need them, check our stats
				#pragma region UseHealthAndFoodIfCritical
				ALL
					//Health if we need it
					SEQ
						COND(IsHealthCritical) END
						ACTIONFAIL(UseAnyHealthKit) END
						//We're in trouble now, sprint!
						ACTION(StartSprinting) END
					END

					//Energy if we need it
					SEQ
						COND(IsEnergyCritical) END
						ACTIONFAIL(UseAnyFood) END
						//We're in trouble now, sprint!
						ACTION(StartSprinting) END
					END
				END
				#pragma endregion

				//Otherwise, use the best medkit or food that doesn't waste any of it
				#pragma region UseBestAvailableFoodOrHealth
				SEQ
					ALWAYS
						COND(NotMaxHealth) END
						ACTION(UseBestHealthKit) END
					END
					ALWAYS
						COND (NotMaxEnergy) END
						ACTION(UseBestFood) END
					END
				END
				#pragma endregion
			END

			SEL
				SEL
					SEL
						//Picking up items
						#pragma region Picking up items
						SEQ
							SEL
								//Always look for items when we're moving, even if we're going to a house
								//If we happen to walk by something, it might be useful and we may need it
								PSEQ
									COND(HasTargetItem) END
									ACTION(PickupItem) END
								END
							
								//See if there were any items we spotted before
								ACTION(SpotNewItem) END
							END

							//Set item as target
							ACTION(SetItemAsTarget) END
							//Go to target
							ACTION(GoToTarget) END
						END
						#pragma endregion

						//House-checking
						#pragma region Going into houses and searching them
						SEL
							//If we have a target house	
							SEQ
								COND(HasTargetHouse) END
								SEL
									//If we were checking one, keep checking
									PSEQ
										COND(InsideTargetHouse) END //If we're inside the target house
										SEQ
											//Go to the target
											ACTION(LookAroundGoToTarget) END

											//Sprint for faster searching
											ACTION(StartSprinting) END

											//Keep checking the house
											PSEQ
												//Check center
												ACTION(CheckHouseCenter) END
												//If the house is big enough, check corners
												//COND(HouseBigEnough) END
												ACTION(CheckTopLeftCorner) END
												ACTION(CheckTopRightCorner) END
												ACTION(CheckBottomRightCorner) END
												ACTION(CheckBottomLeftCorner) END
											END
										END
										ACTION(LeaveHouse) END //Get out of this house
										ACTION(MarkHouseChecked) END //Mark the house checked
									END

									//Otherwise, go to our target house
									ACTION(SetHouseAsTarget) END
								END

								//Go to our target
								ACTION(GoToTarget) END
							END

							//Otherwise, find one
							ACTION(SetTargetHouse) END
						END
						#pragma endregion
					END

					//World searching
					SEQ
						RUNGOOD
							PSEQ
								ACTION(CheckWorldTopLeft) END
								ACTION(CheckWorldTopRight) END
								ACTION(CheckWorldBottomLeft) END
								ACTION(CheckWorldBottomRight) END
								ACTION(ResetHouses) END
							END
						END

						ACTION(LookAroundGoToTarget) END
					END
				END

				//If we haven't found anything in the last x seconds
				//Go back to an old house in the hopes something respawned there

				//Wander if all else fails
				ACTION(WanderAround) END
			END
		END
	});
#pragma endregion
}

#pragma region House behaviour and code
void AIPlugin::CheckNewHouses(const vector<HouseInfo>& vecHouseInfo)
{
	//Check if any new houses in here
	if (vecHouseInfo.size() <= 0) return;

	vector<House> houseLocations;
	m_pBlackboard->GetData("HouseLocations", houseLocations);

	//Go through every detected house
	int index = 0;
	for (auto houseit = vecHouseInfo.begin(); houseit != vecHouseInfo.end(); ++houseit)
	{
		bool alreadyExists = false;

		//Check if we already know about this house
		for (auto knownit = houseLocations.begin(); knownit != houseLocations.end(); ++knownit)
		{
			if (houseit->Center == knownit->m_HouseInfo.Center)
			{
				alreadyExists = true;
				break;
			};
		}

		//If we go this far, the house is new, push it to the vector
		if (!alreadyExists)
		{
			houseLocations.push_back(House(vecHouseInfo[index], false));
			printf("[HOUSE INFO] Adding a new house to vec of house locations.\n");
		}

		++index;
	}

	//Update the blackboard
	m_pBlackboard->ChangeData("HouseLocations", houseLocations);
}

#ifdef _DEBUG
void AIPlugin::DrawKnownHouses()
{
	AgentInfo agentInfo;
	vector<House> houseLocations;
	m_pBlackboard->GetData("AgentInfo", agentInfo);
	m_pBlackboard->GetData("HouseLocations", houseLocations);

	for (auto it = houseLocations.begin(); it != houseLocations.end(); ++it)
	{
		//Draw the center
		DEBUG_DrawPoint(it->m_HouseInfo.Center, 2, b2Color(1, 1, 0, 1));
		//Draw a line from here to the player
		DEBUG_DrawSegment(agentInfo.Position, it->m_HouseInfo.Center, b2Color(1, 0.78, 0.8, 1));
	}
}
#endif
#pragma endregion

#pragma region Entity checking
void AIPlugin::CheckForEntities(const vector<EntityInfo>& vecEntityInfo)
{
	if (vecEntityInfo.size() <= 0) return;

	//Get the items we know the location of
	vector<EntityInfo> items;
	m_pBlackboard->GetData("Items", items);

	m_VecEnemies.clear();

	//Loop through the entities, add any new items ones
	for (auto it : vecEntityInfo)
	{
		bool alreadyThere = false;

		switch (it.Type)
		{
		case ITEM:
			if (items.size() == 0)
			{
				printf("[Item] Encountered new item.\n");
				items.push_back(it);
				continue;
			}

			//Only push back new items
			for (auto jt : items)
			{
				if (jt.Position == it.Position)
					alreadyThere = true;
			}
			if (!alreadyThere)
			{
				printf("[Item] Encountered new item.\n");
				items.push_back(it);
			}
			break;
		case ENEMY:
			//Replace enemies because they move anyway
			m_VecEnemies.push_back(it.Position);
			break;
		}
	}

	//Change the data in the blackboard
	m_pBlackboard->ChangeData("Items", items);
	//m_pBlackboard->ChangeData("Enemies", m_VecEnemies);
}
#pragma endregion

PluginOutput AIPlugin::Update(float dt)
{
	//Output
	PluginOutput output = {};

	auto agentInfo = AGENT_GetInfo(); //Contains all Agent Parameters, retrieved by copy!
	auto vecEntities = FOV_GetEntities(); //Contains all entities

#pragma region DrawDebugStuff
	//Draw debug stuff
	DEBUG_DrawCircle(agentInfo.Position, agentInfo.GrabRange, { 0,0,1 }); //DEBUG_... > Debug helpers (disabled during release build)
	DEBUG_DrawSolidCircle(m_Target, 0.3f, { 0.f,0.f }, { 1.f,0.f,0.f });
#pragma endregion

#pragma region UpdateHouses
	//Fetch visible houses and add to list of known houses
	auto houses = FOV_GetHouses();
	CheckNewHouses(houses);

	//Draw in debug
	#ifdef _DEBUG 
		//DrawKnownHouses(); 
	#endif 
#pragma endregion

#pragma region UpdateItems
	CheckForEntities(vecEntities);
#pragma endregion

#pragma region UpdateBlackboard
	//Update the blackboard
	m_pBlackboard->ChangeData("AgentInfo", agentInfo);
#pragma endregion

#pragma region UpdateBehaviourTree
	//Update the behavior tree
	m_pBehaviourTree->Update();
#pragma endregion

#pragma region UpdateSteering
	//Get current behaviour
	SteeringBehaviours::ISteeringBehaviour* pBehaviour;
	m_pBlackboard->GetData("CurrentBehaviour", pBehaviour);

	//Tie target to navmesh
	b2Vec2 target = b2Vec2_zero;
	m_pBlackboard->GetData("Target", target);

	//target = NAVMESH_GetClosestPathPoint(target);
	//pBehaviour->SetTarget(target);

	//Calc with pipeline
	m_pActuator->SetBehaviour(pBehaviour);
	m_pTargeter->GetGoalRef() = target;
	output = m_pSteeringPipeline->CalculateSteering(dt, agentInfo);
	
	//Fallback behaviour is wandering, just in case
	/*if (pBehaviour)
		output = pBehaviour->CalculateSteering(dt, agentInfo);
	else
		output = m_pFallbackBehaviour->CalculateSteering(dt, agentInfo);*/
#pragma endregion

#pragma region UpdateFPS
	m_FPS = (size_t)1.f / dt;
#pragma endregion

#pragma region DrawDebugTarget
	//Draw target
	DEBUG_DrawPoint(target, 10, b2Color(0.f, 1.f, 0.f, 1.f));
	DEBUG_DrawCircle(target, 2, b2Color(0.f, 1.f, 0.f, 1.f));
#pragma endregion

	return output;
}

//Extend the UI [ImGui call only!]
void AIPlugin::ExtendUI_ImGui()
{
	ImGui::Text("Selected Slot: %i", m_SelectedInventorySlot);
	ImGui::Text("FPS: %i", m_FPS);
}

void AIPlugin::End()
{
	//Delete behaviortree, which will delete the rootaction and the blackboard
	//Blackboard will delete all the present pointers, so it serves as a cleaner
	if (m_pBehaviourTree) delete m_pBehaviourTree;

	//Delete steering pipeline	
	if (m_pSteeringPipeline) delete m_pSteeringPipeline;
	if (m_pDecomposer) delete m_pDecomposer;
	if (m_pTargeter) delete m_pTargeter;
	if (m_pConstraint) delete m_pConstraint;
	if (m_pActuator) delete m_pActuator;

	//Delete fallback if it still exists (it might have been removed in the behaviortree, it might not have)
	if (m_pFallbackBehaviour) delete m_pFallbackBehaviour;
	//Same for seek
	if (m_pSeekBehaviour) delete m_pSeekBehaviour;
	//Same for arrive
	if (m_pArriveBehaviour) delete m_pArriveBehaviour;
	//Same for lookaround
	if (m_pLookAroundBehaviour) delete m_pLookAroundBehaviour;
}

void AIPlugin::ProcessEvents(const SDL_Event& e)
{
	//Empty
}
