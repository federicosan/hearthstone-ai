#pragma once

#include "game-engine/stages/helper.h"
#include "game-engine/cards/common.h"

namespace GameEngine
{
	inline bool StageHelper::PlayerDrawCard(Board & board)
	{
		if (board.player.hand.HasCardToDraw()) {
			StageHelper::Fatigue(board, SLOT_PLAYER_SIDE);
			return StageHelper::CheckHeroMinionDead(board);
		}

		if (board.player.hand.GetCount() < 10) {
			board.player.hand.DrawOneCardToHand();
		}
		else {
			// hand can have maximum of 10 cards
			board.player.hand.DrawOneCardAndDiscard();
			// TODO: distroy card (trigger deathrattle?)
		}

		return false;
	}

	inline bool StageHelper::OpponentDrawCard(Board & board)
	{
		// TODO: implement
		throw std::runtime_error("not implemented");
	}

	inline void StageHelper::GetBoardMoves(
		Board const & board, SlotIndex side, Player const& player, NextMoveGetter & next_moves, bool & all_cards_determined)
	{
		bool const minions_full = !player.minions.IsFull();

		// if all the hand cards are determined,
		// then all identical boards have the same next moves
		// --> is_deterministic is true
		// if some of the hand cards are not yet determined
		// then some different boards might considered as the identical boards in MCTS tree
		// and those different boards might produce different set of next moves
		// --> is_deterministic is false
		all_cards_determined = true;

		for (Hand::Locator hand_idx = 0; hand_idx < board.player.hand.GetCount(); ++hand_idx)
		{
			bool hand_card_determined = false;
			const Card &playing_card = board.player.hand.GetCard(hand_idx, hand_card_determined);
			if (!hand_card_determined) all_cards_determined = false;

			switch (playing_card.type) {
			case Card::TYPE_MINION:
				if (!minions_full) continue;
				GetBoardMoves_PlayMinion(board, side, player, hand_idx, playing_card, next_moves);
				break;

			case Card::TYPE_WEAPON:
				GetBoardMoves_EquipWeapon(board, side, player, hand_idx, playing_card, next_moves);
				break;

			default:
				break;
			}
		}

		// the choices to attack by hero/minion
		GetBoardMoves_Attack(board, side, player, next_moves);

		// the choice to end turn
		Move move_end_turn;
		move_end_turn.action = Move::ACTION_END_TURN;
		next_moves.AddItem(std::move(move_end_turn));
	}

	inline void StageHelper::GetBoardMoves_PlayMinion(
		Board const& board, SlotIndex side, Player const& player, Hand::Locator hand_card, Card const& playing_card,
		NextMoveGetter &next_move_getter)
	{
		if (player.stat.crystal.GetCurrent() < playing_card.cost) return;

		SlotIndexBitmap required_targets;
		bool meet_requirements;
		if (Cards::CardCallbackManager::GetRequiredTargets(playing_card.id, board, side, required_targets, meet_requirements) &&
			meet_requirements == false)
		{
			return;
		}

#ifdef CHOOSE_WHERE_TO_PUT_MINION
		for (int i = 0; i <= player.minions.GetMinionCount(); ++i)
		{
			SlotIndex idx = SlotIndexHelper::GetMinionIndex(side, i);
			next_move_getter.AddItem(NextMoveGetter::ItemPlayerPlayMinion(hand_card, idx, required_targets)); // TODO: unify for both player and opponent
		}
#else
		next_move_getter.AddItem(NextMoveGetter::ItemPlayerPlayMinion(
			hand_card, SlotIndexHelper::GetPlayerMinionIndex(board.player.minions.GetMinionCount()), required_targets));
#endif
	}

	inline void StageHelper::GetBoardMoves_EquipWeapon(
		Board const& board, SlotIndex side, Player const& player, Hand::Locator hand_card, Card const& playing_card,
		NextMoveGetter &next_move_getter)
	{
		if (player.stat.crystal.GetCurrent() < playing_card.cost) return;

		SlotIndexBitmap required_targets;
		bool meet_requirements;
		if (Cards::CardCallbackManager::GetRequiredTargets(playing_card.id, board, SLOT_PLAYER_SIDE, required_targets, meet_requirements) &&
			meet_requirements == false)
		{
			return;
		}

		next_move_getter.AddItem(NextMoveGetter::ItemPlayerEquipWeapon(hand_card, required_targets)); // TODO: unify for both player and opponent
	}

	inline void StageHelper::GetBoardMoves_Attack(Board const & board, SlotIndex side, Player const & player, NextMoveGetter & next_move_getter)
	{
		SlotIndexBitmap attacker;
		SlotIndexBitmap attacked;

		attacker = SlotIndexHelper::GetTargets(SlotIndexHelper::TARGET_TYPE_PLAYER_ATTACKABLE, board);

		if (!attacker.None()) {
			attacked = SlotIndexHelper::GetTargets(SlotIndexHelper::TARGET_TYPE_OPPONENT_CAN_BE_ATTACKED, board);

			NextMoveGetter::ItemAttack player_attack_move(std::move(attacker), std::move(attacked));
			next_move_getter.AddItem(std::move(player_attack_move));
		}
	}

	inline void StageHelper::DealDamage(GameEngine::Board & board, SlotIndex taker_idx, int damage, bool poisonous)
	{
		return StageHelper::DealDamage(board.object_manager.GetObject(taker_idx), damage, poisonous);
	}

	inline void StageHelper::DealDamage(GameEngine::BoardObjects::BoardObject taker, int damage, bool poisonous)
	{
		taker->TakeDamage(damage, poisonous);
	}

	inline SlotIndex StageHelper::GetTargetForForgetfulAttack(GameEngine::Board & board, SlotIndex origin_attacked)
	{
		const bool player_side = SlotIndexHelper::IsPlayerSide(origin_attacked);

		int possible_targets = 1 - 1; // +1 for the hero, -1 for the original attack target
		if (player_side) possible_targets += board.player.minions.GetMinionCount();
		else possible_targets += board.object_manager.opponent_minions.GetMinionCount();

		if (possible_targets == 0)
		{
			// no other target available --> attack original target
			return origin_attacked;
		}

		if (board.random_generator.GetRandom(2) == 0) {
			// forgetful effect does not triggered
			return origin_attacked;
		}

		int r = board.random_generator.GetRandom(possible_targets);

		if (player_side) {
			if (origin_attacked != SLOT_PLAYER_HERO) {
				// now we have a chance to attack the hero, take care of it first
				if (r == possible_targets - 1) {
					return SLOT_PLAYER_HERO;
				}
			}
			// now we can only hit the minions
			return SlotIndexHelper::GetPlayerMinionIndex(r);
		}
		else {
			if (origin_attacked != SLOT_OPPONENT_HERO) {
				// now we have a chance to attack the hero, take care of it first
				if (r == possible_targets - 1) {
					return SLOT_OPPONENT_HERO;
				}
			}
			// now we can only hit the minions
			return SlotIndexHelper::GetOpponentMinionIndex(r);
		}
	}

	inline void StageHelper::HandleAttack(GameEngine::Board & board, GameEngine::SlotIndex attacker_idx, GameEngine::SlotIndex attacked_idx)
	{
		// TODO: trigger secrets

		auto attacker = board.object_manager.GetObject(attacker_idx);

		for (int forgetful_count = attacker->GetForgetfulCount(); forgetful_count > 0; --forgetful_count) {
			attacked_idx = StageHelper::GetTargetForForgetfulAttack(board, attacked_idx);
		}

		auto attacked = board.object_manager.GetObject(attacked_idx);
		StageHelper::DealDamage(attacked, attacker->GetAttack(), attacker->IsPoisonous());

		if (attacked.IsMinion()) {
			// If minion, deal damage equal to the attacked's attack
			StageHelper::DealDamage(attacker, attacked->GetAttack(), attacked->IsPoisonous());
		}

		attacker->AttackedOnce();

		if (attacker.IsMinion() && attacker->IsFreezeAttacker()) attacked->SetFreezed();
		if (attacked.IsMinion() && attacked->IsFreezeAttacker()) attacker->SetFreezed();
	}

	inline void StageHelper::RemoveMinionsIfDead(Board & board, SlotIndex side)
	{
		std::list<std::function<void()>> death_triggers;

		while (true) { // loop until no deathrattle are triggered
			death_triggers.clear();

			// mark as pending death
			for (auto it = board.object_manager.GetMinionIteratorAtBeginOfSide(side); !it.IsEnd(); it.GoToNext())
			{
				if (!it->GetMinion().pending_removal && it->GetHP() > 0) continue;

				it.GetMinions().MarkPendingRemoval(it);

				for (auto const& trigger : it->GetAndClearOnDeathTriggers()) {
					std::function<void(BoardObjects::MinionIterator&)> functor(trigger.func);
					death_triggers.push_back(std::bind(functor, it));
				}
			}

			// trigger deathrattles
			for (auto const& death_trigger : death_triggers) {
				death_trigger();
			}

			// actually remove dead minions
			for (auto it = board.object_manager.GetMinionIteratorAtBeginOfSide(side); !it.IsEnd();)
			{
				if (!it->GetMinion().pending_removal) {
					it.GoToNext();
					continue;
				}

				// remove all effects (including auras)
				it->ClearEnchantments();
				it->ClearAuras();

				it.GetMinions().EraseAndGoToNext(it);
			}

			if (death_triggers.empty()) break;
		}
	}

	inline bool StageHelper::CheckHeroMinionDead(Board & board)
	{
		if (board.object_manager.GetObject(SLOT_PLAYER_HERO)->GetHP() <= 0) {
			board.stage = STAGE_LOSS;
			return true;
		}

		if (board.object_manager.GetObject(SLOT_OPPONENT_HERO)->GetHP() <= 0) {
			board.stage = STAGE_WIN;
			return true;
		}

		RemoveMinionsIfDead(board, SLOT_PLAYER_SIDE);
		RemoveMinionsIfDead(board, SLOT_OPPONENT_SIDE);

		return false; // state not changed
	}

	// return true if stage changed
	inline bool StageHelper::PlayMinion(Board & board, Card const & card, SlotIndex playing_side, PlayMinionData const & data)
	{
		Cards::CardCallbackManager::BattleCry(card.id, board, playing_side, data);
		if (StageHelper::CheckHeroMinionDead(board)) return true;

		auto it = board.object_manager.GetMinionIterator(data.put_location);

		StageHelper::SummonMinion(card, it);

		return false;
	}

	// return true if stage changed
	inline bool StageHelper::SummonMinion(Card const & card, BoardObjects::MinionIterator & it)
	{
		BoardObjects::MinionData summoning_minion;
		summoning_minion.Summon(card);

		if (it.GetMinions().IsFull()) return false;

		// add minion
		auto & summoned_minion = it.GetMinions().InsertBefore(it, std::move(summoning_minion));

		Cards::CardCallbackManager::AfterSummoned(card.id, summoned_minion);

		return true;
	}

	inline bool StageHelper::EquipWeapon(Board & board, Card const & card, SlotIndex playing_side, Move::EquipWeaponData const & data)
	{
		auto & playing_hero = board.object_manager.GetHeroBySide(playing_side);

		playing_hero.DestroyWeapon();

		Cards::CardCallbackManager::Weapon_BattleCry(card.id, board, playing_side, data);
		if (StageHelper::CheckHeroMinionDead(board)) return true;

		playing_hero.EquipWeapon(card);
		Cards::CardCallbackManager::Weapon_AfterEquipped(card.id, playing_hero);
		if (StageHelper::CheckHeroMinionDead(board)) return true;

		return false;
	}

	inline void StageHelper::Fatigue(GameEngine::Board & board, SlotIndex side)
	{
		if (SlotIndexHelper::IsPlayerSide(side)) {
			++board.player.stat.fatigue_damage;
			StageHelper::DealDamage(board.object_manager.GetObject(SLOT_PLAYER_HERO), board.player.stat.fatigue_damage, false);
		}
		else {
			++board.opponent_stat.fatigue_damage;
			StageHelper::DealDamage(board.object_manager.GetObject(SLOT_OPPONENT_HERO), board.opponent_stat.fatigue_damage, false);
		}
	}
}