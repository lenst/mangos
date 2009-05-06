#include "PlayerbotClassAI.h"
#include "Common.h"

PlayerbotClassAI::PlayerbotClassAI(Player* const master, Player* const bot, PlayerbotAI* const ai): m_master(master), m_bot(bot), m_ai(ai) {
    assert (ai);
    assert (bot);
}

PlayerbotClassAI::~PlayerbotClassAI() {}

void PlayerbotClassAI::DoNextCombatManeuver(Unit *) {}

void PlayerbotClassAI::DoNonCombatActions(){}

void PlayerbotClassAI::BuffPlayer(Player* target) {}
