
#include "PlayerbotWarlockAI.h"

class PlayerbotAI;
PlayerbotWarlockAI::PlayerbotWarlockAI(Player* const master, Player* const bot, PlayerbotAI* const ai): PlayerbotClassAI(master, bot, ai) {

    SHADOW_BOLT = ai->getSpellId("shadow bolt");
    IMMOLATE = ai->getSpellId("immolate");
    INCINERATE = ai->getSpellId("incinerate");
    SEARING_PAIN = ai->getSpellId("searing pain");
    CONFLAGRATE = ai->getSpellId("conflagrate");
    SOUL_FIRE = ai->getSpellId("soul fire");
    SHADOWFURY = ai->getSpellId("shadowfury");
    CORRUPTION = ai->getSpellId("corruption"); //affliction
    DARK_PACT = ai->getSpellId("dark pact");
    CURSE_OF_WEAKNESS = ai->getSpellId("curse of weakness"); //CURSE
    CHAOS_BOLT = ai->getSpellId("chaos bolt");
    CURSE_OF_AGONY = ai->getSpellId("curse of agony"); //CURSE
    DEMON_SKIN = ai->getSpellId("demon skin"); //DEMONOLOGY
    DEMON_ARMOR = ai->getSpellId("demon armor");//DEMONOLOGY
    FEL_ARMOR = ai->getSpellId("fel armor");//DEMONOLOGY
    SHADOW_WARD = ai->getSpellId("shadow ward");//DEMONOLOGY
    SOULSHATTER = ai->getSpellId("soulshatter"); //DEMONOLOGY
    DRAIN_SOUL = ai->getSpellId("drain soul"); //affliction
    CURSE_OF_RECKLESSNESS = ai->getSpellId("curse of recklessness"); //CURSE
    DRAIN_LIFE = ai->getSpellId("drain life"); //affliction
    CURSE_OF_TONGUES = ai->getSpellId("curse of tongues"); //CURSE
    SIPHON_LIFE = ai->getSpellId("siphon life"); //affliction
    UNSTABLE_AFFLICTION = ai->getSpellId("unstable affliction"); //affliction
    HAUNT = ai->getSpellId("haunt"); //affliction
    ATROCITY = ai->getSpellId("atrocity"); //affliction
    SEED_OF_CORRUPTION = ai->getSpellId("seed of corruption"); //affliction
    SUMMON_IMP = ai->getSpellId("summon imp"); //DEMONOLOGY
    SUMMON_VOIDWALKER = ai->getSpellId("summon voidwalker"); //DEMONOLOGY
    SUMMON_SUCCUBUS = ai->getSpellId("summon succubus"); //DEMONOLOGY
    SUMMON_FELHUNTER = ai->getSpellId("summon fellhunter"); //DEMONOLOGY
    SUMMON_FELGUARD = ai->getSpellId("summon fellguard"); //DEMONOLOGY
}
PlayerbotWarlockAI::~PlayerbotWarlockAI() {}


bool PlayerbotWarlockAI::CastDot(uint32 spellId, Unit *pTarget, int min_mana, int castTime)
{
    PlayerbotAI* ai = GetAI();
    if (spellId > 0 && ai->GetManaPercent() >= min_mana) {
        if (!ai->HasAura(spellId, *pTarget)) {
            ai->CastSpell(spellId, *pTarget);
            if (castTime != 2)
                ai->SetIgnoreUpdateTime(castTime);
            return true;
        }
    }
    return false;
}

void PlayerbotWarlockAI::DoNextCombatManeuver(Unit *pTarget){
    PlayerbotAI* ai = GetAI();
    if (!ai) return;
    switch (ai->GetScenarioType()) {
    case SCENARIO_DUEL:

        if (SHADOW_BOLT > 0) {
            ai->CastSpell(SHADOW_BOLT);
        }
        return;
    }

    // ------- Non Duel combat ----------

    ai->Follow(*GetMaster()); // dont want to melee mob

    // Damage Spells

    Player *m_bot = GetPlayerBot();
    if( !m_bot->HasInArc(M_PI, pTarget)) {
        m_bot->SetInFront(pTarget);
    }

    bool dotted = CastDot(CURSE_OF_AGONY, pTarget, 14, 1)
        || CastDot(CORRUPTION, pTarget, 19, 1)
        || CastDot(SIPHON_LIFE, pTarget, 22, 1)
        || CastDot(UNSTABLE_AFFLICTION, pTarget, 20, 2)
        || CastDot(HAUNT, pTarget, 20, 2);
    
    if (!dotted) {
        do {
            if (DARK_PACT > 0 && ai->GetManaPercent() < 15) {
                ai->CastSpell(DARK_PACT, *pTarget);
                SAY("Casting Dark Pact");
                break;
            }
            if (SHADOW_BOLT > 0 && LastSpellDestruction < 1 && ai->GetManaPercent() >= 23) {
                ai->CastSpell(SHADOW_BOLT, *pTarget);
                SAY("casting shadow bolt");
                SpellSequence = SPELL_CURSES;
                (LastSpellDestruction = LastSpellDestruction + 1);
                ai->SetIgnoreUpdateTime(3);
                break;
            }
            else if (IMMOLATE > 0 && LastSpellDestruction < 2 && ai->GetManaPercent() >= 23) {
                ai->CastSpell(IMMOLATE, *pTarget);
                SAY("casting immolate");
                SpellSequence = SPELL_CURSES;
                (LastSpellDestruction = LastSpellDestruction + 1);
                break;
            }
            else if (INCINERATE > 0 && LastSpellDestruction < 3 && ai->GetManaPercent() >= 19) {
                ai->CastSpell(INCINERATE, *pTarget);
                SAY("casting incinerate");
                SpellSequence = SPELL_CURSES;
                (LastSpellDestruction = LastSpellDestruction + 1);
                break;
            }
            LastSpellDestruction = 0;
        } while(0);
    } 

} // end DoNextCombatManeuver

void PlayerbotWarlockAI::DoNonCombatActions(){
    Player * bot = GetPlayerBot();
    if (!bot) return;

    SpellSequence = SPELL_CURSES;

    // buff myself  DEMON_SKIN, DEMON_ARMOR, SHADOW_WARD, FEL_ARMOR

    if (FEL_ARMOR > 0) {
        (!bot->HasAura(FEL_ARMOR, 0) && GetAI()->CastSpell (FEL_ARMOR, *bot));
    }
    else if (DEMON_ARMOR > 0) {
        (!bot->HasAura(DEMON_ARMOR, 0) && GetAI()->CastSpell (DEMON_ARMOR, *bot));
    }
    else if (DEMON_SKIN > 0) {
        (!bot->HasAura(DEMON_SKIN, 0) && GetAI()->CastSpell (DEMON_SKIN, *bot));
    }

    PlayerbotClassAI::DoNonCombatActions();
} // end DoNonCombatActions

//void PlayerbotWarlockAI::BuffPlayer(Player* target) {
//	GetAI()->CastSpell(ARCANE_INTELLECT, *target);
//}
