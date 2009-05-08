#ifndef _PLAYERBOTCLASSAI_H
#define _PLAYERBOTCLASSAI_H

#include "Common.h"
#include "World.h"
#include "SpellMgr.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "WorldPacket.h"
#include "Unit.h"
#include "SharedDefines.h"
#include "Chat.h"
#include "PlayerbotAI.h"

class Player;
class PlayerbotAI;

class MANGOS_DLL_SPEC PlayerbotClassAI  {
 public:
  PlayerbotClassAI(Player* const master, Player* const bot, PlayerbotAI* const ai);
  virtual ~PlayerbotClassAI();

  // all combat actions go here
  virtual void DoNextCombatManeuver(Unit*);

  // all non combat actions go here, ex buffs, heals, rezzes
  virtual void DoNonCombatActions();

  // buff a specific player, usually a real PC who is not in group
  virtual void BuffPlayer(Player* target);

  // from a whisper or from the party channel, return true if handled
  virtual bool HandleCommand(const std::string& text, Player& fromPlayer);

  // display class specific info in response to a /bonk
  virtual void TextemotBonk(ChatHandler &ch);

  // Utilities
  Player* GetMaster () {return m_master;}
  Player* GetPlayerBot() {return m_bot;}
  PlayerbotAI* GetAI (){return m_ai;};
  bool BuffGroup(uint32 spellId, bool manaUserOnly = false);

 private:
  Player* m_master;
  Player* m_bot;
  PlayerbotAI* m_ai;
};

#define SAY(m) if (!GetAI()->GetQuiet()) {GetAI()->TellMaster((m));}

#endif
