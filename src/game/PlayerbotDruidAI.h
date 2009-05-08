#ifndef _PlayerbotDruidAI_H
#define _PlayerbotDruidAI_H

#include "PlayerbotClassAI.h"


class MANGOS_DLL_SPEC PlayerbotDruidAI : PlayerbotClassAI {
    public:
        PlayerbotDruidAI(Player* const master, Player* const bot, PlayerbotAI* const ai);
        virtual ~PlayerbotDruidAI();

		// all combat actions go here
		void DoNextCombatManeuver(Unit*);

		// all non combat actions go here, ex buffs, heals, rezzes
		void DoNonCombatActions();

		// buff a specific player, usually a real PC who is not in group
		//void BuffPlayer(Player *target);

        // from a whisper or from the party channel, return true if handled
        //bool HandleCommand(const std::string& text, Player& fromPlayer);
        
	private:

        // resto
        uint32 MARK_OF_THE_WILD;

};

#endif
        
