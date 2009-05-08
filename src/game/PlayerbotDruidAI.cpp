
#include "PlayerbotDruidAI.h"

class PlayerbotAI;

PlayerbotDruidAI::PlayerbotDruidAI(Player* const master, Player* const bot,
                                   PlayerbotAI* const ai)
  : PlayerbotClassAI(master, bot, ai)
{
    MARK_OF_THE_WILD = ai->getSpellId("mark of the wild");
}

PlayerbotDruidAI::~PlayerbotDruidAI() {}


void PlayerbotDruidAI::DoNextCombatManeuver(Unit *pTarget){
	PlayerbotAI* ai = GetAI();
    ai->CastSpell("moonfire") ||
        ai->CastSpell("roots") ||
        ai->CastSpell("wrath");
}

void PlayerbotDruidAI::DoNonCombatActions() {
    BuffGroup(MARK_OF_THE_WILD);

    PlayerbotClassAI::DoNonCombatActions();
}
