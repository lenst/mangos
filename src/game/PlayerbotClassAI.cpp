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
    
