
#include "PlayerbotDruidAI.h"

class PlayerbotAI;

PlayerbotDruidAI::PlayerbotDruidAI(Player* const master, Player* const bot,
                                   PlayerbotAI* const ai)
  : PlayerbotClassAI(master, bot, ai)
{
}

PlayerbotDruidAI::~PlayerbotDruidAI() {}


void PlayerbotDruidAI::DoNextCombatManeuver(Unit *pTarget){
	PlayerbotAI* ai = GetAI();
    ai->CastSpell("moonfire") ||
        ai->CastSpell("roots") ||
        ai->CastSpell("wrath");
}
