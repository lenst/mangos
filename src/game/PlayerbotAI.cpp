#include "Common.h"
#include "Database/DatabaseEnv.h"
//#include "DBCStores.h"
#include "World.h"
#include "SpellMgr.h"
#include "PlayerbotAI.h"
#include "PlayerbotPriestAI.h"
#include "PlayerbotMageAI.h"
#include "PlayerbotWarlockAI.h"
#include "PlayerbotWarriorAI.h"
#include "PlayerbotShamanAI.h"
#include "PlayerbotRogueAI.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "WorldPacket.h"
#include "Spell.h"
#include "Unit.h"
#include "SpellAuras.h"
#include "SharedDefines.h"

/*
 * Packets often compress the GUID (global unique identifier)
 * This function extracts the guid from the packet and decompresses it.
 * The first word (8 bits) in the packet represents how many words in the following packet(s) are part of
 * the guid and what weight they hold. I call it the mask. For example) if mask is 01001001,
 * there will be only 3 words. The first word is shifted to the left 0 times,
 * the second is shifted 3 times, and the third is shifted 6.
 */
uint64 extractGuid(WorldPacket& packet) {
    uint8 mask; packet >> mask;
    uint64 guid = 0;
    uint8 bit = 0;
    uint8 testMask = 1;
    while (true) {
        if (mask & testMask) {
            uint8 word; packet >> word;
            guid += (word << bit);
        }
        if (bit == 7) break;
        ++bit;
        testMask <<= 1;
    }
    return guid;
}

PlayerbotAI::PlayerbotAI(Player* const master, Player* const bot):
    m_master(master), m_bot(bot), m_ignoreAIUpdatesUntilTime(0),
    m_combatOrder(ORDERS_NONE), m_ScenarioType(SCENARIO_PVEEASY),
    m_TimeDoneEating(0), m_TimeDoneDrinking(0), m_CurrentlyCastingSpellId(0),
    m_IsFollowingMaster(true), m_spellIdCommand(0), m_targetGuidCommand(0),
    m_role(ROLE_MIXED), m_quiet(false),
    m_classAI(0)
{

    // get class specific ai
    switch(m_bot->getClass()) {
    case CLASS_PRIEST:
        m_classAI = (PlayerbotClassAI*)new PlayerbotPriestAI(master, m_bot, this);
        break;
    case CLASS_MAGE:
        m_classAI = (PlayerbotClassAI*)new PlayerbotMageAI(master, m_bot, this);
        break;
    case CLASS_WARLOCK:
        m_classAI = (PlayerbotClassAI*)new PlayerbotWarlockAI(master, m_bot, this);
        break;
    case CLASS_WARRIOR:
        m_classAI = (PlayerbotClassAI*)new PlayerbotWarriorAI(master, m_bot, this);
        break;
    case CLASS_SHAMAN:
        m_classAI = (PlayerbotClassAI*)new PlayerbotShamanAI(master, m_bot, this);
        break;
    case CLASS_ROGUE:
        m_classAI = (PlayerbotClassAI*)new PlayerbotRogueAI(master, m_bot, this);
        break;
    }
}

PlayerbotAI::~PlayerbotAI() {}

// finds spell ID for matching substring args
// in priority of full text match, spells not taking reagents, and highest rank
uint32 PlayerbotAI::getSpellId(const char* args) const {
    if(!*args)
        return 0;

    std::string namepart = args;
    std::wstring wnamepart;

    if(!Utf8toWStr(namepart,wnamepart))
        return 0;

    // converting string that we try to find to lower case
    wstrToLower(wnamepart);

    int loc = m_master->GetSession()->GetSessionDbcLocale();

    uint32 foundSpellId = 0;
    bool foundExactMatch = false;
    bool foundMatchUsesNoReagents = false;

    for(PlayerSpellMap::iterator itr = m_bot->GetSpellMap().begin(); itr != m_bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;

        if(itr->second->state == PLAYERSPELL_REMOVED || itr->second->disabled || IsPassiveSpell(spellId))
            continue;

        const SpellEntry* pSpellInfo = sSpellStore.LookupEntry(spellId);
        if (! pSpellInfo)
            continue;

        const std::string name = pSpellInfo->SpellName[loc];
        if(name.empty() || !Utf8FitTo(name, wnamepart))
            continue;

        bool isExactMatch = (name.length() == wnamepart.length()) ? true : false;
        bool usesNoReagents = (pSpellInfo->Reagent[0] <=  0) ? true : false;

        // if we already found a spell
        bool useThisSpell = true;
        if (foundSpellId > 0) {
            if (isExactMatch && ! foundExactMatch) {}
            else if (usesNoReagents && ! foundMatchUsesNoReagents) {}
            else if (spellId > foundSpellId) {}
            else
                useThisSpell = false;
        }
        if (useThisSpell) {
            foundSpellId = spellId;
            foundExactMatch = isExactMatch;
            foundMatchUsesNoReagents = usesNoReagents;
        }
    }

    return foundSpellId;
}

/*
 * Send a list of equipment that is in bot's inventor that is currently unequipped.
 * This is called when the master is inspecting the bot.
 */
void PlayerbotAI::SendNotEquipList(Player& player) {

    // find all unequipped items and put them in
    // a vector of dynamically created lists where the vector index is from 0-18
    // and the list contains Item* that can be equipped to that slot
    // Note: each dynamically created list in the vector must be deleted at end
    // so NO EARLY RETURNS!
    // see enum EquipmentSlots in Player.h to see what equipment slot each index in vector
    // is assigned to. (The first is EQUIPMENT_SLOT_HEAD=0, and last is EQUIPMENT_SLOT_TABARD=18)
    std::list<Item*>* equip[19];
    for (uint8 i=0; i < 19; ++i) equip[i] = NULL;

    // list out items in main backpack
    for (uint8 slot=INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++) {
    	Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    	if (! pItem) continue;

        uint16 dest;
        uint8 msg = m_bot->CanEquipItem(NULL_SLOT, dest, pItem, !pItem->IsBag() );
        if( msg != EQUIP_ERR_OK ) continue;

        // the dest looks like it includes the old loc in the 8 higher bits
        // so casting it to a uint8 strips them
        uint8 equipSlot = uint8(dest);
        if (!(equipSlot >= 0 && equipSlot < 19)) continue;

        // create a list if one doesn't already exist
        if (equip[equipSlot] == NULL)
            equip[equipSlot] = new std::list<Item*>;

        std::list<Item*>* itemListForEqSlot = equip[equipSlot];
        itemListForEqSlot->push_back(pItem);
    }

    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag) {
    	const Bag* const pBag = (Bag*) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
    	if (pBag) {
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot) {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (! pItem) continue;

                uint16 equipSlot;
                uint8 msg = m_bot->CanEquipItem(NULL_SLOT, equipSlot, pItem, !pItem->IsBag() );
                if( msg != EQUIP_ERR_OK ) continue;
                if (!(equipSlot >= 0 && equipSlot < 19)) continue;

                // create a list if one doesn't already exist
                if (equip[equipSlot] == NULL)
                    equip[equipSlot] = new std::list<Item*>;

                std::list<Item*>* itemListForEqSlot = equip[equipSlot];
                itemListForEqSlot->push_back(pItem);
            }
    	}
    }

    TellMaster("Here's all the items in my inventory that I can equip.");
    ChatHandler ch(m_master);

    const std::string descr[]  = {"head","neck","shoulders","body","chest","waist","legs","feet","wrists",
                                  "hands","finger1","finger2","trinket1","trinket2","back","mainhand","offhand","ranged","tabard"};

    // now send client all items that can be equipped by slot
    for (uint8 equipSlot=0; equipSlot< 19; ++equipSlot) {
        if (equip[equipSlot] == NULL) continue;
        std::list<Item*>* itemListForEqSlot = equip[equipSlot];
        std::ostringstream out;
        out << descr[equipSlot] << ": ";
        for (std::list<Item*>::iterator it = itemListForEqSlot->begin(); it != itemListForEqSlot->end(); ++it) {
            const ItemPrototype* const pItemProto = (*it)->GetProto();
            out << " |cffffffff|Hitem:" << pItemProto->ItemId << ":0:0:0:0:0:0:0"
                << "|h[" << pItemProto->Name1 << "]|h|r";
        }
        ch.SendSysMessage(out.str().c_str());

        delete itemListForEqSlot; // delete list of Item*
    }
}

void PlayerbotAI::HandleMasterOutgoingPacket(const WorldPacket& packet, WorldSession& masterSession) {
/*
  const char* oc = LookupOpcodeName(packet.GetOpcode());

  std::ostringstream out;
  out << "out: " << oc;
  sLog.outError(out.str().c_str());
*/
}

void PlayerbotAI::HandleMasterIncomingPacket(const WorldPacket& packet, WorldSession& masterSession) {
    switch(packet.GetOpcode()) {

        // If master inspects one of his bots, give the master useful info in chat window
        // such as inventory that can be equipped
    case CMSG_INSPECT: {
        WorldPacket p(packet);
        p.rpos(0); // reset reader
        uint64 guid; p >> guid;
        Player* const bot = masterSession.GetPlayerBot(guid);
        if (! bot) return;
        bot->GetPlayerbotAI()->SendNotEquipList(*bot);
    }

        // handle emotes from the master
        //case CMSG_EMOTE:
    case CMSG_TEXT_EMOTE: {
        WorldPacket p(packet);
        p.rpos(0); // reset reader
        uint32 emoteNum;
        p >> emoteNum;

        /*
          std::ostringstream out;
          out << "emote is: " << emoteNum;
          ChatHandler ch(masterSession.GetPlayer());
          ch.SendSysMessage(out.str().c_str());
        */
        switch(emoteNum) {

        case TEXTEMOTE_BOW: {
            // Buff anyone who bows before me. Useful for players not in bot's group
            // How do I get correct target???
            //Player* const pPlayer = masterSession.GetPlayerBot(masterSession.GetPlayer()->GetSelection());
            //if (pPlayer->GetPlayerbotAI()->GetClassAI())
            //	pPlayer->GetPlayerbotAI()->GetClassAI()->BuffPlayer(pPlayer);
            return;
        }

        case TEXTEMOTE_BONK: {
            Player* const pPlayer = masterSession.GetPlayerBot(masterSession.GetPlayer()->GetSelection());
            if (! pPlayer || ! pPlayer->GetPlayerbotAI()) return;
            PlayerbotAI* const pBot = pPlayer->GetPlayerbotAI();

            ChatHandler ch(masterSession.GetPlayer());
            { 	std::ostringstream out;
                out << "time(0): " << time(0)
                    << " m_ignoreAIUpdatesUntilTime: " << pBot->m_ignoreAIUpdatesUntilTime;
                ch.SendSysMessage(out.str().c_str());
            }
            { 	std::ostringstream out;
                out << "m_TimeDoneEating: " << pBot->m_TimeDoneEating
                    << " m_TimeDoneDrinking: " << pBot->m_TimeDoneDrinking;
                ch.SendSysMessage(out.str().c_str());
            }
            { 	std::ostringstream out;
                out << "m_CurrentlyCastingSpellId: " << pBot->m_CurrentlyCastingSpellId;
                ch.SendSysMessage(out.str().c_str());
            }
            { 	std::ostringstream out;
                out << "m_IsFollowingMaster: " << pBot->m_IsFollowingMaster;
                ch.SendSysMessage(out.str().c_str());
            }
            { 	std::ostringstream out;
                out << "IsBeingTeleported() " << pBot->m_bot->IsBeingTeleported();
                ch.SendSysMessage(out.str().c_str());
            }
            { 	std::ostringstream out;
                bool tradeActive = (pBot->m_bot->GetTrader()) ? true : false;
                out << "tradeActive: " << tradeActive;
                ch.SendSysMessage(out.str().c_str());
            }
            { 	std::ostringstream out;
                out << "IsCharmed() " << pBot->m_bot->isCharmed();
                ch.SendSysMessage(out.str().c_str());
            }
            return;
        }

        case TEXTEMOTE_EAT:
        case TEXTEMOTE_DRINK: {
            for (PlayerBotMap::const_iterator it = masterSession.GetPlayerBotsBegin(); it != masterSession.GetPlayerBotsEnd(); ++it) {
                Player* const bot = it->second;
                bot->GetPlayerbotAI()->Feast();
            }
            return;
        }

            // emote to stay
        case TEXTEMOTE_STAND: {
            Player* const bot = masterSession.GetPlayerBot(masterSession.GetPlayer()->GetSelection());
            if (bot) bot->GetPlayerbotAI()->Stay();
            else {
                for (PlayerBotMap::const_iterator it = masterSession.GetPlayerBotsBegin(); it != masterSession.GetPlayerBotsEnd(); ++it) {
                    Player* const bot = it->second;
                    bot->GetPlayerbotAI()->Stay();
                }
            }
            return;
        }

            // 324 is the followme emote (not defined in enum)
            // if master has bot selected then only bot follows, else all bots follow
        case 324:
        case TEXTEMOTE_WAVE: {
            Player* const bot = masterSession.GetPlayerBot(masterSession.GetPlayer()->GetSelection());
            if (bot) bot->GetPlayerbotAI()->Follow(* masterSession.GetPlayer());
            else {
                for (PlayerBotMap::const_iterator it = masterSession.GetPlayerBotsBegin(); it != masterSession.GetPlayerBotsEnd(); ++it) {
                    Player* const bot = it->second;
                    bot->GetPlayerbotAI()->Follow(* masterSession.GetPlayer());
                }
            }
            return;
        }
        }
    }
/*

  default: {
  const char* oc = LookupOpcodeName(packet.GetOpcode());
  ChatHandler ch(masterSession.GetPlayer());
  ch.SendSysMessage(oc);

  std::ostringstream out;
  out << "in: " << oc;
  sLog.outError(out.str().c_str());
  }
*/



    }
}

// handle outgoing packets the server would send to the client
void PlayerbotAI::HandleBotOutgoingPacket(const WorldPacket& packet) {
    switch(packet.GetOpcode()) {
    case MSG_MOVE_TELEPORT_ACK: {
        if (!m_bot->IsBeingTeleportedNear())
            return;

        m_bot->SetSemaphoreTeleportNear(false);

        uint32 old_zone = m_bot->GetZoneId();

        WorldLocation const& dest = m_master->GetTeleportDest();

        m_bot->SetPosition(dest.x, dest.y, dest.z, dest.o, true);

        uint32 newzone, newarea;

        m_bot->GetZoneAndAreaId(newzone, newarea);
        m_bot->UpdateZone(newzone, newarea);

        // new zone
        if(old_zone != newzone)
        {
            // honorless target
            if(m_bot->pvpInfo.inHostileArea)
                m_bot->CastSpell(m_bot, 2479, true);
        }

        // resummon pet
        m_bot->ResummonPetTemporaryUnSummonedIfAny();
        return;
    }
    case SMSG_DUEL_WINNER: {
        m_bot->HandleEmoteCommand(EMOTE_ONESHOT_APPLAUD);
        return;
    }
    case SMSG_DUEL_COMPLETE: {
        m_ignoreAIUpdatesUntilTime = time(0) + 4;
        m_combatOrder = ORDERS_NONE;
        m_ScenarioType = SCENARIO_PVEEASY;
        m_bot->GetMotionMaster()->Clear(true);
        return;
    }
    case SMSG_DUEL_OUTOFBOUNDS: {
        m_bot->HandleEmoteCommand(EMOTE_ONESHOT_CHICKEN);
        return;
    }
    case SMSG_DUEL_REQUESTED: {
        m_ignoreAIUpdatesUntilTime = 0;
        WorldPacket p(packet);
        uint64 flagGuid; p >> flagGuid;
        uint64 playerGuid; p >> playerGuid;
        Player* const pPlayer = ObjectAccessor::FindPlayer(playerGuid);
        if (canObeyCommandFrom(*pPlayer)) {
            m_bot->GetMotionMaster()->Clear(true);
            WorldPacket* const packet = new WorldPacket(CMSG_DUEL_ACCEPTED, 8);
            *packet << flagGuid;
            m_bot->GetSession()->QueuePacket(packet); // queue the packet to get around race condition

            // follow target in casting range
            float angle = (float) (M_PI - (M_PI / urand(1,50))); // generates random float between 0 number less then 3.14
            float dist = (float) urand(4,10);
            m_bot->GetMotionMaster()->Clear(true);
            m_bot->GetMotionMaster()->MoveFollow(pPlayer, dist, angle);

            m_bot->SetSelection(playerGuid);
            m_ignoreAIUpdatesUntilTime = time(0) + 4;
            m_combatOrder = ORDERS_KILL;
            m_ScenarioType = SCENARIO_DUEL;
        }
        return;
    }

    case SMSG_INVENTORY_CHANGE_FAILURE: {
        TellMaster("I can't use that.");
        return;
    }
    case SMSG_SPELL_FAILURE: {
        WorldPacket p(packet);
        uint64 casterGuid = extractGuid(p);
        if (casterGuid != m_bot->GetGUID()) return;
        uint32 spellId; p >> spellId;
        if (m_CurrentlyCastingSpellId == spellId) {
            m_ignoreAIUpdatesUntilTime = time(0) + 1;
            m_CurrentlyCastingSpellId = 0;
        }
        return;
    }

        // if a change in speed was detected for the master
        // make sure we have the same mount status
    case SMSG_FORCE_RUN_SPEED_CHANGE: {
        WorldPacket p(packet);
        uint64 guid = extractGuid(p);
        if (guid != m_master->GetGUID()) return;
        if (m_master->IsMounted() && ! m_bot->IsMounted()) {
            Item* const pItem = m_bot->GetPlayerbotAI()->FindMount(300);
            if (pItem) m_bot->GetPlayerbotAI()->UseItem(*pItem);
        } else if (! m_master->IsMounted() && m_bot->IsMounted()) {
            WorldPacket emptyPacket;
            m_bot->GetSession()->HandleDismountOpcode(emptyPacket);
        }
    }

        // handle flying acknowledgement
    case SMSG_MOVE_SET_CAN_FLY: {
        WorldPacket p(packet);
        uint64 guid = extractGuid(p);
        if (guid != m_bot->GetGUID()) return;
        m_bot->AddUnitMovementFlag(MOVEMENTFLAG_FLYING2);
        //m_bot->SetSpeed(MOVE_RUN, m_master->GetSpeed(MOVE_FLIGHT) +0.1f, true);
        return;
    }

        // handle dismount flying acknowledgement
    case SMSG_MOVE_UNSET_CAN_FLY: {
        WorldPacket p(packet);
        uint64 guid = extractGuid(p);
        if (guid != m_bot->GetGUID()) return;
        m_bot->RemoveUnitMovementFlag(MOVEMENTFLAG_FLYING2);
        //m_bot->SetSpeed(MOVE_RUN,m_master->GetSpeedRate(MOVE_RUN),true);
        return;
    }

        // If the leader role was given to the bot automatically give it to the master
        // if the master is in the group, otherwise leave group
    case SMSG_GROUP_SET_LEADER: {
        WorldPacket p(packet);
        std::string name; p >> name;
        if (m_bot->GetGroup() && name == m_bot->GetName()) {
            if (m_bot->GetGroup()->IsMember(m_master->GetGUID())) {
                p.resize(8);
                p << m_master->GetGUID();
                m_bot->GetSession()->HandleGroupSetLeaderOpcode(p);
            } else {
                p.clear(); // not really needed
                m_bot->GetSession()->HandleGroupLeaveOpcode(p); // packet not used
            }
        }
        return;
    }

        // If the master leaves the group, then the bot leaves too
    case SMSG_PARTY_COMMAND_RESULT: {
        WorldPacket p(packet);
        uint32 operation; p >> operation;
        std::string member; p >> member;
        uint32 result; p >> result;
        p.clear();
        if (operation == PARTY_OP_LEAVE) {
            if (member == m_master->GetName())
                m_bot->GetSession()->HandleGroupLeaveOpcode(p); // packet not used
        }
        return;
    }

        // Handle Group invites (auto accept if master is in group, otherwise decline & send message
    case SMSG_GROUP_INVITE: {
        if (m_bot->GetGroupInvite()) {
            const Group* const grp = m_bot->GetGroupInvite();
            if (! grp) return;
            Player* const inviter = objmgr.GetPlayer(grp->GetLeaderGUID());
            if (! inviter) return;
            WorldPacket p;
            if (! canObeyCommandFrom(*inviter)) {
                std::string buf = "I can't accept your invite unless you first invite my master ";
                buf += m_master->GetName();
                buf += ".";
                SendWhisper(buf, *inviter);
                m_bot->GetSession()->HandleGroupDeclineOpcode(p); // packet not used
            } else
                m_bot->GetSession()->HandleGroupAcceptOpcode(p);  // packet not used
        }
        return;
    }

        // Handle when another player opens the trade window with the bot
        // also sends list of tradable items bot can trade if bot is allowed to obey commands from
    case SMSG_TRADE_STATUS: {
        if (m_bot->GetTrader() == NULL) break;
        WorldPacket p(packet);
        uint32 status; p >> status;
        p.clear();

        //4 == TRADE_STATUS_TRADE_ACCEPT
        if (status == 4) {
            m_bot->GetSession()->HandleAcceptTradeOpcode(p); // packet not used
        }

        //1 == TRADE_STATUS_BEGIN_TRADE
        else if (status == 1) {
            m_bot->GetSession()->HandleBeginTradeOpcode(p); // packet not used

            if (! canObeyCommandFrom(*(m_bot->GetTrader()))) {
                SendWhisper("I'm not allowed to trade you any of my items, but you are free to give me money or items.", *(m_bot->GetTrader()));
                return;
            }

            // list out items available for trade
            std::ostringstream out;

            // list out items in main backpack
            for (uint8 slot=INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++) {
                const Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                if (pItem && pItem->CanBeTraded())
                {
                    const ItemPrototype* const pItemProto = pItem->GetProto();
                    std::string name = pItemProto->Name1;

                    out << " |cffffffff|Hitem:" << pItemProto->ItemId << ":0:0:0:0:0:0:0" << "|h[" << name << "]|h|r";
                    if (pItem->GetCount() > 1)
                        out << "x" << pItem->GetCount() << ' ';
                }
            }
            // list out items in other removable backpacks
            for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag) {
                const Bag* const pBag = (Bag*) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
                if (pBag) {
                    for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot) {
                        const Item* const pItem = m_bot->GetItemByPos(bag, slot);
                        if (pItem && pItem->CanBeTraded())
                        {
                            const ItemPrototype* const pItemProto = pItem->GetProto();
                            const std::string name = pItemProto->Name1;

                            // item link format: http://www.wowwiki.com/ItemString
                            // itemId, enchantId, jewelId1, jewelId2, jewelId3, jewelId4, suffixId, uniqueId
                            out << " |cffffffff|Hitem:" << pItemProto->ItemId << ":0:0:0:0:0:0:0" << "|h[" << name << "]|h|r";
                            if (pItem->GetCount() > 1)
                                out << "x" << pItem->GetCount() << ' ';
                        }
                    }
                }
            }

            // calculate how much money bot has
            uint32 copper = m_bot->GetMoney();
            uint32 gold = uint32(copper / 10000);
            copper -= (gold * 10000);
            uint32 silver = uint32(copper / 100);
            copper -= (silver * 100);

            // send bot the message
            std::ostringstream whisper;
            whisper << "I have |cff00ff00" << gold
                    << "|r|cfffffc00g|r|cff00ff00" << silver
                    << "|r|cffcdcdcds|r|cff00ff00" << copper
                    << "|r|cffffd333c|r" << " and the following items:";
            SendWhisper(whisper.str().c_str(), *(m_bot->GetTrader()));
            ChatHandler ch(m_bot->GetTrader());
            ch.SendSysMessage(out.str().c_str());
        }
        return;
    }

    case SMSG_SPELL_GO: {
        WorldPacket p(packet);
        uint64 castItemGuid = extractGuid(p);
        uint64 casterGuid = extractGuid(p);
        if (casterGuid != m_bot->GetGUID()) return;

        uint32 spellId; p >> spellId;
        uint16 castFlags; p >> castFlags;
        uint32 msTime; p >> msTime;
        uint8 numHit; p >> numHit;

        if (m_CurrentlyCastingSpellId == spellId) {

            Spell* const pSpell = m_bot->FindCurrentSpellBySpellId(spellId);
            if (! pSpell) return;

            if (pSpell->IsChannelActive() || pSpell->IsAutoRepeat())
                m_ignoreAIUpdatesUntilTime = time(0) + (GetSpellDuration(pSpell->m_spellInfo) / 1000) + 1;
            else if (pSpell->IsAutoRepeat())
                m_ignoreAIUpdatesUntilTime = time(0) + 6;
            else {
                m_ignoreAIUpdatesUntilTime = time(0) + 1;
                m_CurrentlyCastingSpellId = 0;
            }
        }

        return;
    }

        /*
          case SMSG_MONSTER_MOVE:
          case SMSG_UPDATE_WORLD_STATE:
          case SMSG_COMPRESSED_UPDATE_OBJECT:
          case MSG_MOVE_SET_FACING:
          case MSG_MOVE_STOP:
          case MSG_MOVE_HEARTBEAT:
          case MSG_MOVE_STOP_STRAFE:
          case MSG_MOVE_START_STRAFE_LEFT:
          case SMSG_UPDATE_OBJECT:
          case MSG_MOVE_START_FORWARD:
          return;

          default:
          const char* oc = LookupOpcodeName(packet.GetOpcode());
          TellMaster(oc);
          sLog.outError("opcode: %s", oc);
        */

    }
}

uint8 PlayerbotAI::GetHealthPercent(const Unit& target) const {
    return (static_cast<float>(target.GetHealth()) / target.GetMaxHealth()) * 100;
}
uint8 PlayerbotAI::GetHealthPercent() const {
    return GetHealthPercent(*m_bot);
}
uint8 PlayerbotAI::GetManaPercent(const Unit& target) const {
    return (static_cast<float>(target.GetPower(POWER_MANA)) / target.GetMaxPower(POWER_MANA)) * 100;
}
uint8 PlayerbotAI::GetManaPercent() const {
    return GetManaPercent(*m_bot);
}
uint8 PlayerbotAI::GetBaseManaPercent(const Unit& target) const {
    if (target.GetPower(POWER_MANA) >= target.GetCreateMana()) {
        return (100);
    }
    else {
	return (static_cast<float>(target.GetPower(POWER_MANA)) / target.GetMaxPower(POWER_MANA)) * 100;
    }
}
uint8 PlayerbotAI::GetBaseManaPercent() const {
    return GetBaseManaPercent(*m_bot);
}
uint8 PlayerbotAI::GetRageAmount(const Unit& target) const {
    return (static_cast<float>(target.GetPower(POWER_RAGE)));
}
uint8 PlayerbotAI::GetRageAmount() const {
    return GetRageAmount(*m_bot);
}
uint8 PlayerbotAI::GetEnergyAmount(const Unit& target) const {
    return (static_cast<float>(target.GetPower(POWER_ENERGY)));
}
uint8 PlayerbotAI::GetEnergyAmount() const {
    return GetEnergyAmount(*m_bot);
}

typedef std::pair<uint32, uint8> spellEffectPair;
typedef std::multimap< spellEffectPair, Aura*> AuraMap;

bool PlayerbotAI::HasAura(uint32 spellId, const Unit& player) const {
    for (AuraMap::const_iterator iter = player.GetAuras().begin(); iter != player.GetAuras().end(); ++iter) {
        if (iter->second->GetId() == spellId)
            return true;
    }
    return false;
}
bool PlayerbotAI::HasAura(const char* spellName) const {
    return HasAura(spellName, *m_bot);
}
bool PlayerbotAI::HasAura(const char* spellName, const Unit& player) const {
    uint32 spellId = getSpellId(spellName);
    return (spellId) ? HasAura(spellId, player) : false;
}

// looks through all items / spells that bot could have to get a mount
Item* PlayerbotAI::FindMount(uint32 matchingRidingSkill) const {
    // list out items in main backpack

    Item* partialMatch = NULL;

    for (uint8 slot=INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++) {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem) {
            const ItemPrototype* const pItemProto = pItem->GetProto();
            if (! pItemProto || ! m_bot->CanUseItem(pItemProto) ||
                pItemProto->RequiredSkill != SKILL_RIDING) continue;
            if (pItemProto->RequiredSkillRank == matchingRidingSkill) return pItem;
            else if (! partialMatch ||
                     (partialMatch && partialMatch->GetProto()->RequiredSkillRank <
                      pItemProto->RequiredSkillRank))
                partialMatch = pItem;
        }
    }

    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag) {
        const Bag* const pBag = (Bag*) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag) {
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot) {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (pItem) {
                    const ItemPrototype* const pItemProto = pItem->GetProto();
                    if (! pItemProto || ! m_bot->CanUseItem(pItemProto) ||
                        pItemProto->RequiredSkill != SKILL_RIDING) continue;
                    if (pItemProto->RequiredSkillRank == matchingRidingSkill) return pItem;
                    else if (! partialMatch ||
                             (partialMatch && partialMatch->GetProto()->RequiredSkillRank <
                              pItemProto->RequiredSkillRank))
                        partialMatch = pItem;
                }
            }
        }
    }
    return partialMatch;
}

Item* PlayerbotAI::FindFood() const {
    // list out items in main backpack
    for (uint8 slot=INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++) {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem) {
            const ItemPrototype* const pItemProto = pItem->GetProto();
            if (! pItemProto || ! m_bot->CanUseItem(pItemProto)) continue;
            if(pItemProto->Class==ITEM_CLASS_CONSUMABLE &&
               pItemProto->SubClass==ITEM_SUBCLASS_FOOD) {

            	// if is FOOD
            	if (pItemProto->Spells[0].SpellCategory == SPELL_CATEGORY_FOOD)
                    return pItem;
            }
        }
    }
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag) {
        const Bag* const pBag = (Bag*) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag) {
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot) {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (pItem) {
                    const ItemPrototype* const pItemProto = pItem->GetProto();
                    if (! pItemProto || ! m_bot->CanUseItem(pItemProto)) continue;
                    if(pItemProto->Class==ITEM_CLASS_CONSUMABLE &&
                       pItemProto->SubClass==ITEM_SUBCLASS_FOOD) {
                        // if is FOOD
                        if (pItemProto->Spells[0].SpellCategory == SPELL_CATEGORY_FOOD)
                            return pItem;
                    }
                }
            }
        }
    }
    return NULL;
}

Item* PlayerbotAI::FindDrink() const {
    // list out items in main backpack
    for (uint8 slot=INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++) {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem) {
            const ItemPrototype* const pItemProto = pItem->GetProto();
            if (! pItemProto || ! m_bot->CanUseItem(pItemProto)) continue;
            if(pItemProto->Class==ITEM_CLASS_CONSUMABLE &&
               pItemProto->SubClass==ITEM_SUBCLASS_FOOD) {
                if (pItemProto->Spells[0].SpellCategory == SPELL_CATEGORY_DRINK)
                    return pItem;
            }
        }
    }
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag) {
        const Bag* const pBag = (Bag*) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag) {
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot) {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (pItem) {
                    const ItemPrototype* const pItemProto = pItem->GetProto();
                    if (! pItemProto || ! m_bot->CanUseItem(pItemProto)) continue;
                    if(pItemProto->Class==ITEM_CLASS_CONSUMABLE &&
                       pItemProto->SubClass==ITEM_SUBCLASS_FOOD) {
                        // if is WATER
                        if (pItemProto->Spells[0].SpellCategory == SPELL_CATEGORY_DRINK)
                            return pItem;
                    }
                }
            }
        }
    }
    return NULL;
}

Item* PlayerbotAI::FindBandage() const {
    // list out items in main backpack
    for (uint8 slot=INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; slot++) {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (pItem) {
            const ItemPrototype* const pItemProto = pItem->GetProto();
            if (! pItemProto || ! m_bot->CanUseItem(pItemProto)) continue;
            if(pItemProto->Class==ITEM_CLASS_CONSUMABLE &&
               pItemProto->SubClass==ITEM_SUBCLASS_BANDAGE) {
            	return pItem;
            }
        }
    }
    // list out items in other removable backpacks
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag) {
        const Bag* const pBag = (Bag*) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag) {
            for (uint8 slot = 0; slot < pBag->GetBagSize(); ++slot) {
                Item* const pItem = m_bot->GetItemByPos(bag, slot);
                if (pItem) {
                    const ItemPrototype* const pItemProto = pItem->GetProto();
                    if (! pItemProto || ! m_bot->CanUseItem(pItemProto)) continue;
                    if(pItemProto->Class==ITEM_CLASS_CONSUMABLE &&
                       pItemProto->SubClass==ITEM_SUBCLASS_BANDAGE) {
                        return pItem;
                    }
                }
            }
        }
    }
    return NULL;
}

void PlayerbotAI::InterruptCurrentCastingSpell() {
    TellMaster("I'm interrupting my current spell!");
    WorldPacket* const packet = new WorldPacket(CMSG_CANCEL_CAST,4);
    *packet << m_CurrentlyCastingSpellId;
    m_CurrentlyCastingSpellId = 0;
    m_bot->GetSession()->QueuePacket(packet);
}

void PlayerbotAI::Feast() {
    // stand up if we are done feasting
    if (!(m_bot->GetHealth() < m_bot->GetMaxHealth() ||
          (m_bot->getPowerType() == POWER_MANA &&
           m_bot->GetPower(POWER_MANA) < m_bot->GetMaxPower(POWER_MANA)))) {
        m_bot->SetStandState(PLAYER_STATE_NONE);
        return;
    }

    // wait 3 seconds before checking if we need to drink more or eat more
    time_t currentTime = time(0);
    //m_ignoreAIUpdatesUntilTime = currentTime + 3;

    // should we drink another
    if (m_bot->getPowerType() == POWER_MANA && currentTime > m_TimeDoneDrinking
        && ((static_cast<float>(m_bot->GetPower(POWER_MANA)) / m_bot->GetMaxPower(POWER_MANA)) < 0.8)) {
        Item* pItem = FindDrink();
        m_TimeDoneDrinking = currentTime + 30;
        if (pItem != NULL) {
            TellMaster("drinking now...");
            UseItem(*pItem);
            return;
        }
        else
            TellMaster("I need water.");
    }

    // should we eat another
    if (currentTime > m_TimeDoneEating && ((static_cast<float>(m_bot->GetHealth()) / m_bot->GetMaxHealth()) < 0.8)) {
        Item* pItem = FindFood();
        if (pItem != NULL) {
            TellMaster("eating now...");
            UseItem(*pItem);
        }
        else
            TellMaster("I need food.");
        m_TimeDoneEating = currentTime + 30;
    }

    // if we are no longer eating or drinking
    // because we are out of items or we are above 80% in both stats
    if (currentTime > m_TimeDoneEating && currentTime > m_TimeDoneDrinking) {
        TellMaster("done feasting!");
        m_bot->SetStandState(PLAYER_STATE_NONE);
    }
}

// intelligently sets a reasonable combat order for this bot
// based on its class / level / etc
void PlayerbotAI::GetCombatOrders() {
    Unit* thingToAttack = m_master->getAttackerForHelper();
    if (! thingToAttack) return;

    // if thing to attack is in a duel, then ignore and don't call updateAI for 6 seconds
    // this method never gets called when the bot is in a duel and this code
    // prevents bot from helping
    if (thingToAttack->GetTypeId() == TYPEID_PLAYER && dynamic_cast<Player*>(thingToAttack)->duel) {
        m_ignoreAIUpdatesUntilTime = time(0) + 6;
        return;
    }

    m_bot->SetSelection(thingToAttack->GetGUID());
    m_ignoreAIUpdatesUntilTime = time(0) + 1;
    m_combatOrder = ORDERS_KILL;

    if (m_bot->getStandState() != PLAYER_STATE_NONE)
        m_bot->SetStandState(PLAYER_STATE_NONE);

    m_bot->Attack(thingToAttack, true);

    m_bot->GetMotionMaster()->Clear(true);

    // follow target in casting range - I commented out the priest & mage classes because of strange behavior - feel free to experiment
    switch(m_bot->getClass()) {
    case CLASS_PRIEST:
        break;
    case CLASS_MAGE:
        break;
    case CLASS_SHAMAN:
        break;
    case CLASS_WARLOCK:
        break;
    case CLASS_DRUID: {
        float angle = (float) (M_PI - (M_PI / urand(1,50))); // generates random float between 0 number less then 3.14
        float dist = (float) urand(4,10);
        m_bot->GetMotionMaster()->Clear(true);
        m_bot->GetMotionMaster()->MoveFollow(thingToAttack, dist, angle);
        break;
    }
    default:
        m_bot->GetMotionMaster()->MoveChase(thingToAttack);
    }

    return;
}

void PlayerbotAI::DoNextCombatManeuver() {
    Unit* const pTarget = ObjectAccessor::GetUnit(*m_bot, m_bot->GetSelection());

    // if current order doesn't make sense anymore
    // clear our orders so we can get orders in next update
    if (!pTarget || pTarget->isDead() || !pTarget->IsInWorld() || !m_bot->IsHostileTo(pTarget)) {
        m_combatOrder = ORDERS_NONE;
        m_bot->SetSelection(0);
        m_bot->GetMotionMaster()->Clear(true);
        return;
    }

    if (GetClassAI()) {
        (GetClassAI())->DoNextCombatManeuver(pTarget);
        return;
    }

    switch(m_bot->getClass()) {
    case CLASS_DRUID:
        CastSpell("moonfire") || CastSpell("roots") ||  CastSpell("wrath");
        break;
    }
}

// this is where the AI should go
//GetRandomContactPoint
//GetPower, GetMaxPower
// HasSpellCooldown
// IsAffectedBySpellmod
// isMoving
// hasUnitState(FLAG) FLAG like: UNIT_STAT_ROOT, UNIT_STAT_CONFUSED, UNIT_STAT_STUNNED
// hasAuraType

void PlayerbotAI::UpdateAI(const uint32 p_time) {
    time_t currentTime = time(0);
    if (currentTime < m_ignoreAIUpdatesUntilTime || m_bot->IsBeingTeleported() || m_bot->GetTrader()) return;

    // default updates occur every two seconds
    m_ignoreAIUpdatesUntilTime = currentTime + 2;

    // if we are casting a spell then interrupt it
    // make sure any actions that cast a spell set a proper m_ignoreAIUpdatesUntilTime!
    Spell* const pSpell = GetCurrentSpell();
    if (pSpell && !(pSpell->IsChannelActive() || pSpell->IsAutoRepeat()))
        InterruptCurrentCastingSpell();

    // direct cast command from master
    else if (m_spellIdCommand != 0) {
    	Unit* pTarget = ObjectAccessor::GetUnit(*m_bot, m_targetGuidCommand);
    	if (pTarget != NULL)
            CastSpell(m_spellIdCommand, *pTarget);
    	m_spellIdCommand = 0;
    	m_targetGuidCommand = 0;
    }

    // handle combat
    else if (m_combatOrder != ORDERS_NONE)
        DoNextCombatManeuver();

    // if master is in combat and bot is not, automatically assist master
    // NOTE: combat orders are also set via incoming packets to bot or outgoing packets from master
//	else if (m_master->isInCombat() && ! m_bot->isInCombat())
    else if (m_master->isInCombat())
        GetCombatOrders();

    // are we sitting, if so feast if possible
    else if (m_bot->getStandState() == PLAYER_STATE_SIT)
        Feast();

    // if commanded to follow master and not already following master then follow master
    else if (!m_bot->isInCombat() && m_IsFollowingMaster &&
             m_bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE)
        Follow(* m_master);

    // do class specific non combat actions
    else if (GetClassAI()) {
        (GetClassAI())->DoNonCombatActions();
    }
}

Spell* PlayerbotAI::GetCurrentSpell() const {
    if (m_CurrentlyCastingSpellId == 0) return NULL;
    Spell* const pSpell = m_bot->FindCurrentSpellBySpellId(m_CurrentlyCastingSpellId);
    return pSpell;
}

void PlayerbotAI::TellMaster(const std::string& text) {
    SendWhisper(text, *m_master);
}

void PlayerbotAI::SendWhisper(const std::string& text, Player& player) {
    WorldPacket data(SMSG_MESSAGECHAT, 200);
    m_bot->BuildPlayerChat(&data, CHAT_MSG_REPLY, text, LANG_UNIVERSAL);
    player.GetSession()->SendPacket(&data);
}

bool PlayerbotAI::canObeyCommandFrom(const Player& player) const {
    return player.GetSession()->GetAccountId() == m_master->GetSession()->GetAccountId();
}

bool PlayerbotAI::CastSpell(const char* args) {
    uint32 spellId = getSpellId(args);
    return (spellId) ? CastSpell(spellId) : false;
}

bool PlayerbotAI::CastSpell(uint32 spellId, Unit& target) {
    uint64 oldSel = m_bot->GetSelection();
    m_bot->SetSelection(target.GetGUID());
    bool rv = CastSpell(spellId);
    m_bot->SetSelection(oldSel);
    return rv;
}

bool PlayerbotAI::CastSpell(uint32 spellId) {

    // see Creature.cpp 1738 for reference
    // don't allow bot to cast damage spells on friends
    const SpellEntry* const pSpellInfo = sSpellStore.LookupEntry(spellId);
    if (! pSpellInfo) {
    	TellMaster("missing spell entry in CastSpell.");
    	return false;
    }

    // set target
    uint64 targetGUID = m_bot->GetSelection();
    Unit* pTarget = ObjectAccessor::GetUnit(*m_bot, m_bot->GetSelection());
    if (IsPositiveSpell(spellId)) {
    	if (pTarget && ! m_bot->IsFriendlyTo(pTarget))
            pTarget = m_bot;
    }
    else {
    	if (pTarget && m_bot->IsFriendlyTo(pTarget))
            return false;

    	// search for Creature::reachWithSpellAttack to also see some examples of spell distance usage
    	if (! m_bot->isInFront(pTarget, 10)) {
            m_bot->SetInFront(pTarget);
            WorldPacket data;
            m_bot->BuildHeartBeatMsg(&data);
            m_bot->SendMessageToSet(&data,true);
    	}
    }

    if (HasAura(spellId, *pTarget)) return false;

    m_bot->CastSpell(pTarget,pSpellInfo,false);

    Spell* const pSpell = m_bot->FindCurrentSpellBySpellId(spellId);
    if (! pSpell) return false;

    m_CurrentlyCastingSpellId = spellId;
    // m_ignoreAIUpdatesUntilTime = time(0) + pSpell->GetCastTime() + 3;
    m_ignoreAIUpdatesUntilTime = time(0) + 6;

    // if this caused the caster to move (blink) update the position
    // I think this is normally done on the client
    // this should be done on spell success
    /*
      if (name == "Blink") {
      float x,y,z;
      m_bot->GetPosition(x,y,z);
      m_bot->GetNearPoint(m_bot, x, y, z, 1, 5, 0);
      m_bot->Relocate(x,y,z);
      WorldPacket data;
      m_bot->BuildHeartBeatMsg(&data);
      m_bot->SendMessageToSet(&data,true);
      }
    */

    return true;
}

// ChatHandler already implements some useful commands the master can call on bots
// These commands are protected inside the ChatHandler class so this class provides access to the commands
// we'd like to call on our bots
class PlayerbotChatHandler : protected ChatHandler {
public:
    explicit PlayerbotChatHandler(Player* pMasterPlayer) : ChatHandler(pMasterPlayer) {}

    bool revive(const Player& botPlayer) {
        return HandleReviveCommand(botPlayer.GetName());
    }
    bool teleport(const Player& botPlayer) {
        return HandleNamegoCommand(botPlayer.GetName());
    }
    void sysmessage(const char *str) {
        SendSysMessage(str);
    }
};

// extracts all item ids in format below
// I decided to roll my own extractor rather then use the one in ChatHandler
// because this one works on a const string, and it handles multiple links
// |color|linkType:key:something1:...:somethingN|h[name]|h|r
void PlayerbotAI::extractItemIds(const std::string& text, std::list<uint32>& itemIds) const {
    uint8 pos = 0;
    while (true) {
        int i = text.find("Hitem:", pos);
        if (i == -1) break;
        pos = i + 6;
        int endPos = text.find(':', pos);
        if (endPos == -1) break;
        std::string idC = text.substr(pos, endPos - pos);
        uint32 id = atol(idC.c_str());
        pos = endPos;
        if (id) itemIds.push_back(id);
    }
}

// extracts currency in #g#s#c format
uint32 PlayerbotAI::extractMoney(const std::string& text) const {
    // if user specified money in ##g##s##c format
    std::string acum = "";
    uint32 copper = 0;
    for (uint8 i=0; i < text.length(); i++) {
        if (text[i] == 'g') {
            copper += (atol(acum.c_str()) * 100 * 100);
            acum = "";
        } else if (text[i] == 'c') {
            copper += atol(acum.c_str());
            acum = "";
        } else if (text[i] == 's') {
            copper += (atol(acum.c_str()) * 100);
            acum = "";
        } else if (text[i] == ' ') {
            break;
        } else if (text[i] >= 48 && text[i] <= 57) {
            acum += text[i];
        } else {
            copper = 0;
            break;
        }
    }
    return copper;
}

// finds items in inventory and adds Item* to foundItemList
// also removes found item IDs from itemIdSearchList when found
void PlayerbotAI::findItemsInInv(std::list<uint32>& itemIdSearchList, std::list<Item*>& foundItemList) const {

    // look for items in main bag
    for (uint8 slot=INVENTORY_SLOT_ITEM_START; itemIdSearchList.size() > 0 &&
             slot < INVENTORY_SLOT_ITEM_END; ++slot) {
        Item* const pItem = m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!pItem) continue;
        for (std::list<uint32>::iterator it = itemIdSearchList.begin(); it != itemIdSearchList.end(); ++it) {
            if (pItem->GetProto()->ItemId != *it) continue;
            foundItemList.push_back(pItem);
            itemIdSearchList.erase(it);
            break;
        }
    }

    // for all for items in other bags
    for (uint8 bag = INVENTORY_SLOT_BAG_START; itemIdSearchList.size() > 0 &&
             bag < INVENTORY_SLOT_BAG_END; ++bag) {
        Bag* const pBag = (Bag*) m_bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (! pBag) continue;
        for (uint8 slot = 0; itemIdSearchList.size() > 0 &&
                 slot < pBag->GetBagSize(); ++slot) {
            Item* const pItem = m_bot->GetItemByPos(bag, slot);
            if (!pItem) continue;
            for (std::list<uint32>::iterator it = itemIdSearchList.begin(); it != itemIdSearchList.end(); ++it) {
                if (pItem->GetProto()->ItemId != *it) continue;
                foundItemList.push_back(pItem);
                itemIdSearchList.erase(it);
                break;
            }
        }
    }
}

// submits packet to use an item
void PlayerbotAI::UseItem(Item& item) {
    uint8 bagIndex = item.GetBagSlot();
    uint8 slot = item.GetSlot();
    uint8 cast_count = 1;
    uint32 spellid = 0;		// only used in combat
    uint64 item_guid = item.GetGUID();
    uint32 glyphIndex = 0;	// ??
    uint8 unk_flags = 0;	// not 0x02

    // create target data
    // note other targets are possible but not supported at the moment
    // see SpellCastTargets::read in Spell.cpp to see other options
    // for setting target

    uint32 target = TARGET_FLAG_SELF;

    WorldPacket* const packet = new WorldPacket(CMSG_USE_ITEM, 1+1+1+4+8+4+1);
    *packet << bagIndex << slot << cast_count << spellid << item_guid << glyphIndex << unk_flags << target;
    m_bot->GetSession()->QueuePacket(packet); // queue the packet to get around race condition

    // certain items cause player to sit (food,drink)
    // tell bot to stop following if this is the case
    // (doesn't work since we queued the packet!)
    // maybe its not needed???
    //if (! m_bot->IsStandState())
    //	m_bot->GetMotionMaster()->Clear();
}

// submits packet to use an item
void PlayerbotAI::EquipItem(Item& item) {
    uint8 bagIndex = item.GetBagSlot();
    uint8 slot = item.GetSlot();

    WorldPacket* const packet = new WorldPacket(CMSG_AUTOEQUIP_ITEM,2);
    *packet << bagIndex << slot;
    m_bot->GetSession()->QueuePacket(packet);
}

// submits packet to trade an item (trade window must already be open)
bool PlayerbotAI::TradeItem(const Item& item) {
    if (! m_bot->GetTrader() || item.IsInTrade() || ! item.CanBeTraded())
        return false;
    for (uint8 i=0; i < TRADE_SLOT_TRADED_COUNT; ++i) {
        if (m_bot->GetItemPosByTradeSlot(i) == NULL_SLOT) {
            WorldPacket* const packet = new WorldPacket(CMSG_SET_TRADE_ITEM, 3);
            *packet << (uint8) i << (uint8) item.GetBagSlot() << (uint8) item.GetSlot();
            m_bot->GetSession()->QueuePacket(packet);
            return true;
        }
    }
    return false;
}

// submits packet to trade copper (trade window must be open)
bool PlayerbotAI::TradeCopper(uint32 copper) {
    if (copper > 0) {
        WorldPacket* const packet = new WorldPacket(CMSG_SET_TRADE_GOLD, 4);
        *packet << copper;
        m_bot->GetSession()->QueuePacket(packet);
        return true;
    }
    return false;
}

void PlayerbotAI::Stay() {
    m_IsFollowingMaster = false;
    m_bot->GetMotionMaster()->Clear(true);
    m_bot->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
}

bool PlayerbotAI::Follow(Player& player) {
    if (m_master->IsBeingTeleported()) return false;
    m_IsFollowingMaster = true;

    if (! m_bot->IsStandState())
        m_bot->SetStandState(PLAYER_STATE_NONE);

    if (! m_bot->isInCombat()) {
        // if bot is dead and master is alive, revive bot
        if (m_master->isAlive() && ! m_bot->isAlive()) {
            m_ignoreAIUpdatesUntilTime = time(0) + 6;
            PlayerbotChatHandler ch(m_master);
            if (! ch.revive(*m_bot)) {
                ch.sysmessage(".. could not be revived ..");
                return false;
            }
        }

        // if bot has strayed too far from the master, teleport bot
        if (m_bot->GetMapId() != player.GetMapId() ||
            m_bot->GetZoneId() != player.GetZoneId() ||
            (abs(abs(m_bot->GetPositionX()) - abs(player.GetPositionX())) > 50) ||
            (abs(abs(m_bot->GetPositionY()) - abs(player.GetPositionY())) > 50) ||
            (abs(abs(m_bot->GetPositionZ()) - abs(player.GetPositionZ())) > 50)) {
            m_ignoreAIUpdatesUntilTime = time(0) + 6;
            PlayerbotChatHandler ch(m_master);
            if (! ch.teleport(*m_bot)) {
                ch.sysmessage(".. could not be teleported ..");
                return false;
            }
        }
    }

    if (m_bot->isAlive()) {
        float angle = (float) (M_PI - (M_PI / urand(1,50))); // generates random float between 0 number less then 3.14
        float dist = (float) urand(1,9);
        m_bot->GetMotionMaster()->Clear(true);
        m_bot->GetMotionMaster()->MoveFollow(&player, dist, angle);
        return true;
    }
    return false;
}

// handle commands sent through chat channels
void PlayerbotAI::HandleCommand(const std::string& text, Player& fromPlayer) {
    // ignore any messages from Addons
    if (text.find("X-Perl")!=std::wstring::npos) return;
    if (text.find("HealBot")!=std::wstring::npos) return;
    if (text.find("LOOT_OPENED")!=std::wstring::npos) return;
    if (text.find("CTRA")!=std::wstring::npos) return;
    if (text.find("Crb\t")!=std::wstring::npos) return;

    // if message is not from a player in the masters account auto reply and ignore
    if (! canObeyCommandFrom(fromPlayer)) {
        std::string msg = "I can't talk to you. Please speak to my master ";
        msg += m_master->GetName();
        SendWhisper(msg, fromPlayer);
        m_bot->HandleEmoteCommand(EMOTE_ONESHOT_NO);
    }

    // if in the middle of a trade, and player asks for an item/money
    else if (m_bot->GetTrader() && m_bot->GetTrader()->GetGUID() == fromPlayer.GetGUID()) {
        uint32 copper = extractMoney(text);
        if (copper > 0) TradeCopper(copper);

        std::list<uint32> itemIds;
        extractItemIds(text, itemIds);
        if(itemIds.size() == 0)
            SendWhisper("Show me what item you want by shift clicking the item in the chat window.", fromPlayer);
        else {
            std::list<Item*> itemList;
            findItemsInInv(itemIds, itemList);
            for (std::list<Item*>::iterator it = itemList.begin(); it != itemList.end(); ++it)
                TradeItem(**it);
        }
    }

    else if (text == "quiet")
        m_quiet = true;
    else if (text == "speak")
        m_quiet = false;
    
    else if (text == "mixed")
        m_role = ROLE_MIXED;
    else if (text == "dps")
        m_role = ROLE_DPS;
    else if (text == "healer")
        m_role = ROLE_HEALER;

    else if (text == "follow" || text == "come")
        Follow(*m_master);

    else if (text == "stay" || text == "stop")
        Stay();

    else if (text == "attack") {
        uint64 attackOnGuid = fromPlayer.GetSelection();
        if (attackOnGuid) {
            Unit* thingToAttack = ObjectAccessor::GetUnit(*m_bot, attackOnGuid);
            if (!m_bot->IsFriendlyTo(thingToAttack) && m_bot->IsWithinLOSInMap(thingToAttack)) {
                m_bot->GetMotionMaster()->Clear(true);
                m_combatOrder = ORDERS_KILL;
                m_bot->SetSelection(thingToAttack->GetGUID());
                m_bot->Attack(thingToAttack, true);
                m_bot->GetMotionMaster()->MoveChase(thingToAttack);
                m_ignoreAIUpdatesUntilTime = time(0) + 6;
            }
        }
        else {
            TellMaster("No target is selected.");
            m_bot->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
        }
    }

    // handle cast command
    else if (text.size() > 2 && text.substr(0,2) == "c " ||
             text.size() > 5 && text.substr(0,5) == "cast ") {
        std::string spellStr = text.substr(text.find(" ") + 1);
        uint32 spellId = (uint32)atol(spellStr.c_str());

        // try and get spell ID by name
        if (spellId == 0)
            spellId = getSpellId(spellStr.c_str());

        uint64 castOnGuid = fromPlayer.GetSelection();
        if (spellId != 0 && castOnGuid != 0) {
            m_spellIdCommand = spellId;
            m_targetGuidCommand = castOnGuid;
        }
    }

    // use items
    else if (text.size() > 2 && text.substr(0,2) == "u " ||
             text.size() > 4 && text.substr(0,4) == "use ") {
        std::list<uint32> itemIds;
        std::list<Item*> itemList;
        extractItemIds(text, itemIds);
        findItemsInInv(itemIds, itemList);
        for (std::list<Item*>::iterator it = itemList.begin(); it != itemList.end(); ++it)
            UseItem(**it);
    }

    // equip items
    else if (text.size() > 2 && text.substr(0,2) == "e " ||
             text.size() > 6 && text.substr(0,6) == "equip ") {
        std::list<uint32> itemIds;
        std::list<Item*> itemList;
        extractItemIds(text, itemIds);
        findItemsInInv(itemIds, itemList);
        for (std::list<Item*>::iterator it = itemList.begin(); it != itemList.end(); ++it)
            EquipItem(**it);
    }

    else if (text == "spells") {

        int loc = m_master->GetSession()->GetSessionDbcLocale();

        std::ostringstream posOut;
        std::ostringstream negOut;

        const std::string ignoreList = ",Opening,Closing,Stuck,Remove Insignia,Opening - No Text,Grovel,Duel,Honorless Target,";
        std::string alreadySeenList = ",";

        for(PlayerSpellMap::iterator itr = m_bot->GetSpellMap().begin(); itr != m_bot->GetSpellMap().end(); ++itr) {
            const uint32 spellId = itr->first;

            if(itr->second->state == PLAYERSPELL_REMOVED || itr->second->disabled || IsPassiveSpell(spellId))
                continue;

            const SpellEntry* const pSpellInfo = sSpellStore.LookupEntry(spellId);
            if (! pSpellInfo)
                continue;

            //|| name.find("Teleport") != -1

            std::string comp = ",";
            comp.append(pSpellInfo->SpellName[loc]);
            comp.append(",");

            if (!(ignoreList.find(comp) == std::string::npos &&
                  alreadySeenList.find(comp) == std::string::npos))
                continue;

            alreadySeenList += pSpellInfo->SpellName[loc];
            alreadySeenList += ",";

            if (IsPositiveSpell(spellId))
                posOut << " |cffffffff|Hspell:" << spellId << "|h[" << pSpellInfo->SpellName[loc] << "]|h|r";
            else
                negOut << " |cffffffff|Hspell:" << spellId << "|h[" << pSpellInfo->SpellName[loc] << "]|h|r";
        }

        ChatHandler ch(&fromPlayer);
        SendWhisper("here's my non-attack spells:", fromPlayer);
        ch.SendSysMessage(posOut.str().c_str());
        SendWhisper("and here's my attack spells:", fromPlayer);
        ch.SendSysMessage(negOut.str().c_str());
    }

    else {
        sLog.outString ("Bot got unk msg: %s", text.c_str());

        std::string msg = "What? follow, stay, (c)ast <spellname>, spells, (e)quip, (u)se.";
        SendWhisper(msg, fromPlayer);
        m_bot->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
    }
}
