/* Copyright 2026 TheSuperHackers. SPDX-License-Identifier: GPL-3.0-or-later */
#include "PreRTS.h"
#include "Common/DeveloperHarness.h"
#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/RandomValue.h"
#include "Common/Recorder.h"
#include "Common/MessageStream.h"
#include "Common/FramePacer.h"
#include "GameNetwork/GameInfo.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameClient/MapUtil.h"
#include "GameClient/GameClient.h"
#include "GameClient/Display.h"
#include "GameClient/View.h"
#include <cstring>
namespace DeveloperTools
{
namespace
{
bool requested=false,preset=false,presetDone=false,revealed=false;
enum class QuickState { Waiting, Loading, Ready, Failed };
QuickState state=QuickState::Waiting;
const char* status="DEV: manual skirmish; quick game not requested";
constexpr unsigned Capacity=384,Seed=0x4b1001;
const char* Map="Maps\\Twilight Flame\\Twilight Flame.map";
struct Unit { ObjectID id;bool enemy; };
std::array<Unit,Capacity> spawned{};
unsigned count=0,lastFrame=0;
Player* human()
{
    auto* p=ThePlayerList?ThePlayerList->getLocalPlayer():nullptr;
    return p && p->getPlayerIndex()>0 && p->isPlayerActive() && p->getPlayerType()==PLAYER_HUMAN?p:nullptr;
}
Player* opponent()
{
    // Skirmish slot mapping, never enumeration of active computer players (neutral is computer too).
    auto* local=human();auto* p=ThePlayerList?ThePlayerList->getPlayerFromSlotIndex(1):nullptr;
    if(!local || !p || !validDevOwners(local->getPlayerIndex(),local->isPlayerActive(),local->getPlayerType()==PLAYER_HUMAN,
        p->getPlayerIndex(),p->isPlayerActive(),p->getPlayerType()==PLAYER_COMPUTER))return nullptr;
    return local->getRelationship(p->getDefaultTeam())==ENEMIES?p:nullptr;
}
void reject(const char* reason){state=QuickState::Failed;status=reason;OutputDebugStringA(reason);}
bool startQuickGame()
{
    if(TheNetwork || (TheRecorder && TheRecorder->isPlaybackMode()) ||
       (TheGameLogic->isInGame() && !TheGameLogic->isInShellGame()))return false;
    if(!TheMapCache || !ThePlayerTemplateStore || !TheMessageStream || !TheWritableGlobalData || !TheFramePacer)return false;
    const auto* map=TheMapCache->findMap(Map);int faction=-1;
    for(int i=0;i<ThePlayerTemplateStore->getPlayerTemplateCount();++i)
        if(ThePlayerTemplateStore->getNthPlayerTemplate(i)->getName()==AsciiString("FactionChina")){faction=i;break;}
    if(!map || !map->m_isMultiplayer || !map->m_doesExist || map->m_numPlayers<2 || faction<0)return false;
    if(TheGameLogic->isInShellGame())TheGameLogic->clearGameData(FALSE);
    // Same lazy singleton ownership as the Skirmish menu; engine owns shutdown.
    if(!TheSkirmishGameInfo)TheSkirmishGameInfo=NEW SkirmishGameInfo;
    auto* game=TheSkirmishGameInfo;game->init();game->clearSlotList();game->setLocalIP(0);game->enterGame();
    for(int i=0;i<2;++i)
    {
        GameSlot slot;slot.setState(i?SLOT_EASY_AI:SLOT_PLAYER,UnicodeString(i?L"Developer opponent":L"Developer"));
        slot.setIP(i?1:0);slot.setColor(i);slot.setPlayerTemplate(faction);slot.setStartPos(i);
        slot.setTeamNumber(i);slot.setAccept();game->setSlot(i,slot);
    }
    Money cash;cash.setStartingCash(10000);game->setStartingCash(cash);
    game->setMap(Map);game->setMapCRC(map->m_CRC);game->setMapSize(map->m_filesize);game->setSeed(Seed);game->setUseStats(FALSE);game->startGame(0);
    TheWritableGlobalData->m_mapName=Map;TheWritableGlobalData->m_framesPerSecondLimit=120;TheFramePacer->setFramesPerSecondLimit(120);
    InitRandom(Seed); // Normal new-game RNG initialization; only explicit developer quick game.
    auto* msg=TheMessageStream->appendMessage(GameMessage::MSG_NEW_GAME);
    msg->appendIntegerArgument(GAME_SKIRMISH);msg->appendIntegerArgument(DIFFICULTY_NORMAL);
    msg->appendIntegerArgument(0);msg->appendIntegerArgument(120);
    state=QuickState::Loading;status="DEV: loading normal offline skirmish";return true;
}
Coord3D anchor(bool enemy)
{
    Region3D bounds;TheTerrainLogic->getExtent(&bounds);Coord3D p;
    p.x=bounds.lo.x+(bounds.hi.x-bounds.lo.x)*0.22f;
    p.y=bounds.lo.y+(bounds.hi.y-bounds.lo.y)*(enemy?0.65f:0.22f);
    p.z=TheTerrainLogic->getGroundHeight(p.x,p.y);return p;
}
void spawnGroup(unsigned amount,bool enemy)
{
    Player* owner=enemy?opponent():human();
    if(!owner || !owner->getDefaultTeam() || !TheThingFactory || !TheAI || !TheAI->pathfinder() || amount>Capacity-count)
        {status="DEV: spawn refused (ownership/services/capacity)";return;}
    const auto* tank=TheThingFactory->findTemplate("ChinaTankBattleMaster",FALSE);
    const auto* infantry=TheThingFactory->findTemplate("ChinaInfantryRedguard",FALSE);
    if(!tank || !infantry){status="DEV: China unit templates unavailable";return;}
    Region3D bounds;TheTerrainLogic->getExtent(&bounds);const auto origin=anchor(enemy);
    unsigned accepted=0;
    // Bounded placement, using ordinary footprint/destination services. No benchmark tolerance/retry assertions.
    for(unsigned candidate=0;candidate<512 && accepted<amount;++candidate)
    {
        Coord3D pos=origin;pos.x+=(candidate%16)*48.0f;pos.y+=(candidate/16)*48.0f;
        if(pos.x>=bounds.hi.x-48 || pos.y>=bounds.hi.y-48)continue;
        pos.z=TheTerrainLogic->getGroundHeight(pos.x,pos.y);
        auto* obj=TheThingFactory->newObject(accepted%2?tank:infantry,owner->getDefaultTeam());
        obj->setOrientation(0);obj->setPosition(&pos);auto* ai=obj->getAIUpdateInterface();
        if(!ai || obj->getControllingPlayer()!=owner || TheTerrainLogic->getLayerForDestination(&pos)!=LAYER_GROUND ||
           !TheAI->pathfinder()->validMovementPosition(obj->getCrusherLevel()>0,LAYER_GROUND,ai->getLocomotorSet(),&pos))
            {TheGameLogic->destroyObject(obj);continue;}
        spawned[count++]={obj->getID(),enemy};++accepted;
    }
    if(!enemy && TheTacticalView)TheTacticalView->userLookAt(&origin);
    status=accepted==amount?"DEV: owned army spawned (human slot0 / enemy slot1 in Quick Game)":"DEV: partial spawn; terrain/capacity limited";
}
void moveUnits(bool battle)
{
    unsigned ordered=0;
    for(unsigned i=0;i<count;++i)
    {
        auto* obj=TheGameLogic->findObjectByID(spawned[i].id);auto* owner=spawned[i].enemy?opponent():human();
        if(!obj || !owner || obj->getControllingPlayer()!=owner || obj->isEffectivelyDead())continue;
        auto* ai=obj->getAIUpdateInterface();if(!ai)continue;
        Coord3D goal=anchor(!spawned[i].enemy);goal.x+=(i%12)*48.0f;goal.y+=(i/12%8)*48.0f;
        goal.z=TheTerrainLogic->getGroundHeight(goal.x,goal.y);
        if(battle)ai->aiAttackMoveToPosition(&goal,NO_MAX_SHOTS_LIMIT,CMD_FROM_SCRIPT);
        else ai->aiMoveToPosition(&goal,CMD_FROM_SCRIPT);
        ++ordered;
    }
    status=ordered?"DEV: normal orders issued; interactive, no auto-exit":"DEV: no surviving owned units to order";
}
}
void requestQuickGame(){requested=true;}
bool quickGameEnabled(){return requested && devModeEnabled() && !scenarioEnabled();}
bool configureDevPreset(const char* value){preset=value && std::strcmp(value,"battle")==0;return preset;}
const char* interactiveStatus(){return status;}
bool interactiveOffline()
{
    return devModeEnabled() && !scenarioEnabled() && TheGameLogic && TheGameLogic->getGameMode()==GAME_SKIRMISH &&
        !TheNetwork && (!TheRecorder || !TheRecorder->isPlaybackMode()) &&
        !TheGameLogic->isLoadingMap() && !TheGameLogic->isLoadingSave() && !TheGameLogic->isClearingGameData() &&
        TheGameInfo && TheGameInfo==TheSkirmishGameInfo && human();
}
void beginInteractiveOuter()
{
    if(!devModeEnabled() || scenarioEnabled())return;
    if(!interactiveOffline()){count=0;revealed=false;lastFrame=0;}
    if(!quickGameEnabled() || state!=QuickState::Waiting)return;
    if(!TheGameClient || !TheDisplay || !TheGameLogic)return;
    if(!readyForScenarioStart(TheGameClient->isStartupIntroComplete(),TheDisplay->isMoviePlaying(),
        TheGameLogic->isLoadingMap(),TheGameLogic->isLoadingSave(),TheGameLogic->isClearingGameData()))return;
    if(!startQuickGame())reject("DEV: quick game refused; map/faction/offline initialization unavailable");
}
void interactiveTick(unsigned frame,unsigned actions)
{
    if(!interactiveOffline())return;
    if(frame<lastFrame){count=0;revealed=false;}lastFrame=frame;
    if(quickGameEnabled() && state==QuickState::Loading)
    {
        if(ThePlayerList->getPlayerFromSlotIndex(0)!=human() || !opponent())
            {reject("DEV: quick game ownership validation failed");return;}
        state=QuickState::Ready;status="DEV: READY human slot0 / enemy AI slot1; interactive, no auto-exit";
    }
    if(quickGameEnabled() && state!=QuickState::Ready)return;
    auto has=[actions](Action a){return (actions&(1u<<static_cast<unsigned>(a)))!=0;};
    auto* local=human();
    if(has(Action::Money))local->getMoney()->deposit(100000,FALSE,FALSE);
    if(has(Action::InstantBuild))
    {
#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
        local->toggleInstantBuild();
#else
        status="DEV: instant build deferred in Release; use spawn actions";
#endif
    }
    if(has(Action::Reveal) && ThePartitionManager)
    {
        if(revealed)ThePartitionManager->shroudMapForPlayer(local->getPlayerIndex());
        else ThePartitionManager->revealMapForPlayer(local->getPlayerIndex());
        revealed=!revealed;
    }
    if(has(Action::Group))spawnGroup(12,false);
    if(has(Action::LargeGroup))spawnGroup(48,false);
    if(has(Action::Army))spawnGroup(96,false);
    if(has(Action::OpposingArmy))spawnGroup(96,true);
    if(has(Action::Move))moveUnits(false);
    const bool autoBattle=quickGameEnabled() && preset && !presetDone;
    if(has(Action::Battle) || autoBattle)
    {
        presetDone=true;
        if(!count){spawnGroup(96,false);spawnGroup(96,true);}
        moveUnits(true);
    }
}
void shutdownInteractive()
{
    requested=preset=presetDone=revealed=false;state=QuickState::Waiting;count=lastFrame=0;
    status="DEV: manual skirmish; quick game not requested";
}
}
