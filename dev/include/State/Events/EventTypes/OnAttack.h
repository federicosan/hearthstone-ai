#pragma once

#include <tuple>
#include <string>
#include <iostream>
#include <functional>
#include "state/Types.h"

namespace state
{
	namespace Cards
	{
		class Card;
	}

	namespace Events
	{
		namespace EventTypes
		{
			class OnAttack
			{
			public:
				typedef std::function<void(HandlersContainerController &, State &, CardRef &, CardRef &)> FunctorType;
				typedef std::tuple<State &, CardRef &, CardRef &> ArgsTuple;

				template <typename T> OnAttack(T&& functor) : functor_(functor) {}

				void Handle(HandlersContainerController &controller, State & state_, CardRef & attacker_, CardRef & defender_)
				{
					functor_(controller, state_, attacker_, defender_);
				}

			private:
				FunctorType functor_;
			};
		}
	}
}