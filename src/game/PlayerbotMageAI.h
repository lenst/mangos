#ifndef _PlayerbotMageAI_H
#define _PlayerbotMageAI_H

#include "PlayerbotClassAI.h"

enum {
	SPELL_FROST,
	SPELL_FIRE,
	SPELL_ARCANE
};


enum MageStrategyType {
  MAGESTRATEGY_MIXED,
  MAGESTRATEGY_FROST
};

//class Player;

class MANGOS_DLL_SPEC PlayerbotMageAI : PlayerbotClassAI {
    public:
        PlayerbotMageAI(Player* const master, Player* const bot, PlayerbotAI* const ai);
        virtual ~PlayerbotMageAI();

		// all combat actions go here
		void DoNextCombatManeuver(Unit*);

		// all non combat actions go here, ex buffs, heals, rezzes
		void DoNonCombatActions();

		// buff a specific player, usually a real PC who is not in group
		void BuffPlayer(Player *target);

        // from a whisper or from the party channel, return true if handled
        bool HandleCommand(const std::string& text, Player& fromPlayer);

        // display class specific info in response to a /bonk
        void TextemotBonk(ChatHandler &ch);

	private:
        void DoNextFrost(Unit*);

        MageStrategyType m_strat;

		// arcane
		uint32 ARCANE_MISSILES, ARCANE_EXPLOSION, COUNTERSPELL, SLOW, ARCANE_BARRAGE, ARCANE_BLAST, MIRROR_IMAGE;

		// fire
		uint32 FIREBALL, FIREBLAST, IMPACT, FLAMESTRIKE, SCORCH, PYROBLAST, BLAST_WAVE, COMBUSTION, DRAGONS_BREATH, LIVING_BOMB, FROSTFIRE_BOLT;

		// FROST
		uint32 CHILLED, FROSTBITE, DEEP_FREEZE, FROSTBOLT, FROST_NOVA, BLIZZARD, ICY_VEINS, CONE_OF_COLD, ICE_BARRIER, SUMMON_WATER_ELEMENTAL, ICE_LANCE;

		// buffs
		uint32 FROST_ARMOR, ICE_ARMOR, MAGE_ARMOR, ARCANE_INTELLECT, ARCANE_BRILLIANCE, DALARAN_INTELLECT, DALARAN_BRILLIANCE, MANA_SHIELD;

		uint32 SpellSequence, LastSpellArcane, LastSpellFire, LastSpellFrost, CONJURE_WATER, CONJURE_FOOD;
};

#endif
