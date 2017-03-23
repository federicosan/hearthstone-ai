#pragma once

#include "Cards/AuraHelper.h"
#include "Cards/FlagAuraHelper.h"
#include "Cards/EventRegister.h"
#include "Cards/BattlecryHelper.h"
#include "Cards/MinionCardUtils.h"
#include "Cards/CardAttributes.h"

namespace Cards
{
	template <typename T>
	class GeneralCardBase: public state::Cards::CardData
	{
	public:
		template <typename... Types>
		auto Aura() { return AuraHelper<T, Types...>(*this); }

		template <typename... Types>
		auto PlayerFlagAura() { return PlayerFlagAuraHelper<T, Types...>(*this); }

		template <typename... Types>
		auto Enrage() { return EnrageHelper<T, Types...>(*this); }

		template <typename LifeTime, typename SelfPolicy, typename EventType, typename EventHandler = T>
		using RegisteredEventType = OneEventRegisterHelper<LifeTime, SelfPolicy, EventType, EventHandler>;

		template <typename EventType, typename EventHandler = T>
		using RegisteredManagedEventType = ManagedOneEventRegisterHelper<EventType, EventHandler>;

		template <typename... RegisteredEvents>
		auto RegisterEvents() {
			return EventsRegisterHelper<RegisteredEvents...>::Process((state::Cards::CardData&)*this);
		}

		template <typename LifeTime, typename SelfPolicy, typename EventType, typename EventHandler = T>
		auto RegisterEvent() {
			return RegisterEvents<RegisteredEventType<LifeTime, SelfPolicy, EventType, EventHandler>>();
		}

		template <typename EventType, typename EventHandler = T>
		auto RegisterEvent() {
			return RegisterEvents<RegisteredManagedEventType<EventType, EventHandler>>();
		}


		template <typename Context>
		static void SummonToRight(Context && context, Cards::CardId card_id)
		{
			int pos = context.card_.GetZonePosition() + 1;
			return SummonInternal(std::forward<Context>(context), card_id, context.card_.GetPlayerIdentifier(), pos);
		}

		template <typename Context>
		static void SummonToLeft(Context && context, Cards::CardId card_id)
		{
			int pos = context.card_.GetZonePosition();
			return SummonInternal(std::forward<Context>(context), card_id, context.card_.GetPlayerIdentifier(), pos);
		}

		template <typename Context>
		static void SummonToEnemy(Context && context, Cards::CardId card_id)
		{
			state::PlayerIdentifier player = context.card_.GetPlayerIdentifier().Opposite();
			int pos = (int)context.state_.GetBoard().Get(player).minions_.Size();
			return SummonInternal(std::forward<Context>(context), card_id, player, pos);
		}

		template <typename Context>
		static void SummonToAllyByCopy(Context&& context, state::Cards::Card const& card)
		{
			state::PlayerIdentifier player = context.card_.GetPlayerIdentifier();
			int pos = (int)context.state_.GetBoard().Get(player).minions_.Size();
			return SummonInternalByCopy(std::forward<Context>(context), card, player, pos);
		}

	private:
		template <typename Context>
		static void SummonInternal(Context&& context, Cards::CardId card_id, state::PlayerIdentifier player, int pos)
		{
			if (context.state_.GetBoard().Get(player).minions_.Full()) return;

			FlowControl::Manipulate(context.state_, context.flow_context_).Board()
				.SummonMinionById(card_id, player, pos);
		}
		template <typename Context>
		static void SummonInternalByCopy(Context&& context, state::Cards::Card const& card, state::PlayerIdentifier player, int pos)
		{
			if (context.state_.GetBoard().Get(player).minions_.Full()) return;

			FlowControl::Manipulate(context.state_, context.flow_context_).Board()
				.SummonMinionByCopy(card, player, pos);
		}
	};
}