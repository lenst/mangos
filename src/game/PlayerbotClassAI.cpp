#include "PlayerbotClassAI.h"
#include "Common.h"

PlayerbotClassAI::PlayerbotClassAI(Player* const master, Player* const bot, PlayerbotAI* const ai): m_master(master), m_bot(bot), m_ai(ai) {
    assert (ai);
    assert (bot);
}

PlayerbotClassAI::~PlayerbotClassAI() {}

void PlayerbotClassAI::DoNextCombatManeuver(Unit *) {}

void PlayerbotClassAI::DoNonCombatActions()
{
    if ( (m_bot->getPowerType() == POWER_MANA
          && GetAI()->GetManaPercent() < 25)
         || GetAI()->GetHealthPercent() < 25)
    {
        GetAI()->Feast();
    }
}

void PlayerbotClassAI::BuffPlayer(Player* target) {}

bool PlayerbotClassAI::HandleCommand(const std::string& text, Player& fromPlayer) {
    return false;
}
    
void PlayerbotClassAI::TextemotBonk(ChatHandler &ch) {};


// Group Buff Utilitiy
bool PlayerbotClassAI::BuffGroup(uint32 spellId, bool manaUserOnly) {
	Player* bot = GetPlayerBot();
	PlayerbotAI* ai = GetAI();
    Group *g = bot->GetGroup();
    if (!g) return false;
    for (GroupReference *itr = g->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player *pPlayer = itr->getSource();
        if ((!manaUserOnly || pPlayer->getPowerType() == POWER_MANA)
            && !pPlayer->HasAura(spellId, 0)
            && ai->CastSpell(spellId, *pPlayer))
            return true;
    }
    return false;
}
