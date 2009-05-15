
#include "PlayerbotRogueAI.h"

class PlayerbotAI;
PlayerbotRogueAI::PlayerbotRogueAI(Player* const master, Player* const bot, PlayerbotAI* const ai): PlayerbotClassAI(master, bot, ai) {

	SINISTER_STRIKE = ai->getSpellId("sinister strike"); //COMBAT
	BACKSTAB = ai->getSpellId("backstab");
	GOUGE = ai->getSpellId("gouge");
	EVASION = ai->getSpellId("evasion");
	SPRINT = ai->getSpellId("sprint");
	KICK = ai->getSpellId("kick");
	FEINT = ai->getSpellId("feint");
	SHIV = ai->getSpellId("shiv");
	FAN_OF_KNIVES = ai->getSpellId("fan of knives");

	SHADOWSTEP = ai->getSpellId("shadowstep"); //SUBTLETY
	VANISH = ai->getSpellId("vanish");
	HEMORRHAGE = ai->getSpellId("hemorrhage");
	BLIND = ai->getSpellId("blind");
	SHADOW_DANCE = ai->getSpellId("shadow dance");
	CLOAK_OF_SHADOWS = ai->getSpellId("cloak of shadows");
	TRICK_TRADE = ai->getSpellId("trick of the trade");

	EVISCERATE = ai->getSpellId("eviscerate"); //ASSASSINATION
	SLICE_DICE = ai->getSpellId("slice and dice");
	GARROTE = ai->getSpellId("garrote");
	EXPOSE_ARMOR = ai->getSpellId("expose armor");
	AMBUSH = ai->getSpellId("ambush");
	RAPTURE = ai->getSpellId("rapture");
	DISMANTLE = ai->getSpellId("dismantle");
	CHEAP_SHOT = ai->getSpellId("cheap shot");
	KIDNEY_SHOT = ai->getSpellId("kidney shot");
	MUTILATE = ai->getSpellId("mutilate");
	ENVENOM = ai->getSpellId("envenom");
	DEADLY_THROW = ai->getSpellId("deadly throw");

}

PlayerbotRogueAI::~PlayerbotRogueAI() {}

void PlayerbotRogueAI::DoNextCombatManeuver(Unit *pTarget){
	PlayerbotAI* ai = GetAI();
	if (!ai) return;
	switch (ai->GetScenarioType()) {
		case SCENARIO_DUEL:

			if (SINISTER_STRIKE > 0) {
				ai->CastSpell(SINISTER_STRIKE);
			}
			return;
	}

	// ------- Non Duel combat ----------

	//ai->Follow(*GetMaster()); // dont want to melee mob

	// Damage Attacks

  Player *m_bot = GetPlayerBot();
	if( !m_bot->HasInArc(M_PI, pTarget)) {
	    m_bot->SetInFront(pTarget);
	}
	switch (SpellSequence) {

	  case SPELL_COMBAT:
			   if (SINISTER_STRIKE > 0 && LastSpellCombat < 1 && ai->GetEnergyAmount() >= 45) {

						ai->CastSpell(SINISTER_STRIKE, *pTarget);
						SpellSequence = SPELL_SUBTLETY;
						(LastSpellCombat = LastSpellCombat +1);
				break;

			}
			  else	if (BACKSTAB > 0 && LastSpellCombat < 2 && ai->GetEnergyAmount() >=45) {

						ai->CastSpell(BACKSTAB, *pTarget);
						SpellSequence = SPELL_SUBTLETY;
						(LastSpellCombat = LastSpellCombat +1);
						break;
				  }
			  else if (GOUGE > 0 && LastSpellCombat < 3 && ai->GetEnergyAmount() >=45) {

						ai->CastSpell(GOUGE, *pTarget);
						SpellSequence = SPELL_SUBTLETY;
						(LastSpellCombat = LastSpellCombat +1);
						break;
				  }
				else if (EVASION > 0 && LastSpellCombat < 5 && ai->GetEnergyAmount() >=0) {

						ai->CastSpell(EVASION);
						SpellSequence = SPELL_SUBTLETY;
						(LastSpellCombat = LastSpellCombat +1);
						break;
				  }
				else if (SPRINT > 0 && LastSpellCombat < 6 && ai->GetEnergyAmount() >=0) {

						ai->CastSpell(SPRINT);
						SpellSequence = SPELL_SUBTLETY;
						(LastSpellCombat = LastSpellCombat +1);
						break;
				  }
				else if (KICK > 0 && LastSpellCombat < 7 && ai->GetEnergyAmount() >=25) {

						ai->CastSpell(KICK);
						SpellSequence = SPELL_SUBTLETY;
						(LastSpellCombat = LastSpellCombat +1);
						break;
				  }
				else if (FEINT > 0 && LastSpellCombat < 8 && ai->GetEnergyAmount() >= 20) {

						ai->CastSpell(FEINT, *pTarget);
						SpellSequence = SPELL_SUBTLETY;
						(LastSpellCombat = LastSpellCombat +1);
						break;
				  }
				else if (SHIV > 0 && LastSpellCombat < 9 && ai->GetEnergyAmount() >= 20) {

						ai->CastSpell(SHIV, *pTarget);
						SpellSequence = SPELL_SUBTLETY;
						(LastSpellCombat = LastSpellCombat +1);
						break;
				  }
				else if (FAN_OF_KNIVES > 0 && LastSpellCombat < 10 && ai->GetEnergyAmount() >= 50) {

						ai->CastSpell(FAN_OF_KNIVES);
						SpellSequence = SPELL_SUBTLETY;
						(LastSpellCombat = LastSpellCombat +1);
						break;
				  }


					LastSpellCombat = 0;
					//SpellSequence = SPELL_SUBTLETY;
				//break;
				
			
		  
		case SPELL_SUBTLETY:
			if (SHADOWSTEP > 0 && LastSpellSubtlety < 1 && ai->GetEnergyAmount() >= 10) {
						ai->CastSpell(SHADOWSTEP, *pTarget);
						SpellSequence = SPELL_ASSASSINATION;
						(LastSpellSubtlety = LastSpellSubtlety +1);
				break;
				  }

				else if (VANISH > 0 && LastSpellSubtlety < 2 && ai->GetEnergyAmount() >= 0) {
						ai->CastSpell(VANISH);
						SpellSequence = SPELL_ASSASSINATION;
						(LastSpellSubtlety = LastSpellSubtlety +1);
						break;
				  }

				else if (HEMORRHAGE > 0 && LastSpellSubtlety < 4 && ai->GetEnergyAmount() >= 35) {
						ai->CastSpell(HEMORRHAGE, *pTarget);
						SpellSequence = SPELL_ASSASSINATION;
						(LastSpellSubtlety = LastSpellSubtlety +1);
						break;
				  }
				else if (BLIND > 0 && LastSpellSubtlety < 5 && ai->GetEnergyAmount() >= 30) {
						ai->CastSpell(BLIND, *pTarget);
						SpellSequence = SPELL_ASSASSINATION;
						(LastSpellSubtlety = LastSpellSubtlety +1);
						break;
				  }
				else if (SHADOW_DANCE > 0 && LastSpellSubtlety < 6 && ai->GetEnergyAmount() >= 0) {
						ai->CastSpell(SHADOW_DANCE);
						SpellSequence = SPELL_ASSASSINATION;
						(LastSpellSubtlety = LastSpellSubtlety +1);
						break;
				  }
				else if (CLOAK_OF_SHADOWS > 0 && LastSpellSubtlety < 7 && ai->GetEnergyAmount() >= 0) {
						ai->CastSpell(CLOAK_OF_SHADOWS);
						SpellSequence = SPELL_ASSASSINATION;
						(LastSpellSubtlety = LastSpellSubtlety +1);
						break;
				  }

				else if (TRICK_TRADE > 0 && LastSpellSubtlety < 8 && ai->GetEnergyAmount() >= 15) {
						ai->CastSpell(TRICK_TRADE, *(GetMaster()));
						SpellSequence = SPELL_ASSASSINATION;
						(LastSpellSubtlety = LastSpellSubtlety +1);
						break;
				  }




				LastSpellSubtlety = 0;
				//SpellSequence = SPELL_ASSASSINATION;
				//break;



		  case SPELL_ASSASSINATION:
			if (EVISCERATE > 0 && LastSpellAssassination < 1 && ai->GetEnergyAmount() >= 35) {
				ai->CastSpell(EVISCERATE, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}

			else if (SLICE_DICE > 0 && LastSpellAssassination < 2 && ai->GetEnergyAmount() >= 25) {
				ai->CastSpell(SLICE_DICE, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}
			else if (GARROTE > 0 && LastSpellAssassination < 3 && ai->GetEnergyAmount() >= 50) {
				ai->CastSpell(GARROTE, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}
			else if (EXPOSE_ARMOR > 0 && LastSpellAssassination < 4 && ai->GetEnergyAmount() >= 25) {
				ai->CastSpell(EXPOSE_ARMOR, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}
			else if (AMBUSH > 0 && LastSpellAssassination < 5 && ai->GetEnergyAmount() >= 60) {
				ai->CastSpell(AMBUSH, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}
			else if (RAPTURE > 0 && LastSpellAssassination < 6 && ai->GetEnergyAmount() >= 25) {
				ai->CastSpell(RAPTURE, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}
			else if (DISMANTLE > 0 && LastSpellAssassination < 7 && ai->GetEnergyAmount() >= 25) {
				ai->CastSpell(DISMANTLE);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}
			else if (CHEAP_SHOT > 0 && LastSpellAssassination < 8 && ai->GetEnergyAmount() >= 60) {
				ai->CastSpell(CHEAP_SHOT, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}
			else if (KIDNEY_SHOT > 0 && LastSpellAssassination < 8 && ai->GetEnergyAmount() >= 25) {
				ai->CastSpell(KIDNEY_SHOT, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}
			else if (MUTILATE > 0 && LastSpellAssassination < 8 && ai->GetEnergyAmount() >= 60) {
				ai->CastSpell(MUTILATE, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}
			else if (ENVENOM > 0 && LastSpellAssassination < 8 && ai->GetEnergyAmount() >= 35) {
				ai->CastSpell(ENVENOM, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}
			else if (DEADLY_THROW > 0 && LastSpellAssassination < 8 && ai->GetEnergyAmount() >= 35) {
				ai->CastSpell(DEADLY_THROW, *pTarget);
				SpellSequence = SPELL_COMBAT;
				(LastSpellAssassination = LastSpellAssassination + 1);
				break;
			}

			else {
				LastSpellAssassination = 0;
				SpellSequence = SPELL_COMBAT;
			}
	}

} // end DoNextCombatManeuver

void PlayerbotRogueAI::DoNonCombatActions(){
	Player * m_bot = GetPlayerBot();
	if (!m_bot) {
		return;
	}

	SpellSequence = SPELL_COMBAT;

	// hp check
	if (m_bot->getStandState() != PLAYER_STATE_NONE)
		m_bot->SetStandState(PLAYER_STATE_NONE);

	Item* pItem = GetAI()->FindFood();

	if (pItem != NULL && GetAI()->GetHealthPercent() < 15) {
		GetAI()->TellMaster("I could use some food.");
		GetAI()->UseItem(*pItem);
		GetAI()->SetIgnoreUpdateTime(30);
		return;
	}


  /*
	if (BATTLE_SHOUT > 0 && !m_bot->HasAura(BATTLE_SHOUT, 0)) {
		if (BLOODRAGE > 0) {
			GetAI()->CastSpell (BLOODRAGE, *m_bot);
		}
	}

	if (BATTLE_SHOUT > 0 && !m_bot->HasAura(BATTLE_SHOUT, 0)) {
		GetAI()->CastSpell (BATTLE_SHOUT, *m_bot);
	}

*/
} // end DoNonCombatActions

void PlayerbotRogueAI::BuffPlayer(Player* target) {

}
