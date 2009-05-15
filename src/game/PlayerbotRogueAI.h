
#ifndef _PlayerbotRogueAI_H
#define _PlayerbotRogueAI_H

#include "PlayerbotClassAI.h"

enum {
	SPELL_COMBAT,
	SPELL_SUBTLETY,
	SPELL_ASSASSINATION
};


//class Player;

class MANGOS_DLL_SPEC PlayerbotRogueAI : PlayerbotClassAI {
    public:
        PlayerbotRogueAI(Player* const master, Player* const bot, PlayerbotAI* const ai);
        virtual ~PlayerbotRogueAI();

		// all combat actions go here
		void DoNextCombatManeuver(Unit*);

		// all non combat actions go here, ex buffs, heals, rezzes
		void DoNonCombatActions();

		// buff a specific player, usually a real PC who is not in group
		void BuffPlayer(Player *target);

	private:



		// COMBAT
		uint32 SINISTER_STRIKE, BACKSTAB, GOUGE, EVASION, SPRINT, KICK, FEINT, SHIV, FAN_OF_KNIVES;

		// SUBTLETY
		uint32 SHADOWSTEP, VANISH, HEMORRHAGE, BLIND, SHADOW_DANCE, CLOAK_OF_SHADOWS, TRICK_TRADE;

		// ASSASSINATION
		uint32 EVISCERATE, SLICE_DICE, GARROTE, EXPOSE_ARMOR, AMBUSH, RAPTURE, DISMANTLE, CHEAP_SHOT, KIDNEY_SHOT, MUTILATE, ENVENOM, DEADLY_THROW;


		uint32 SpellSequence, LastSpellCombat, LastSpellSubtlety, LastSpellAssassination;
		
		
};

#endif
