/*
 * Copyright (c) 2024 Pu Yang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Pu Yang  <puyang@uvic.ca>
 */

#include "state-machine.h"

namespace ns3
{

// NS_LOG_COMPONENT_DEFINE("StateMachine");

namespace open_routing
{

template <typename StateType, typename EventType>
StateMachine<StateType, EventType>::StateMachine() {}

template <typename StateType, typename EventType>
StateMachine<StateType, EventType>::~StateMachine() {}

template <typename StateType, typename EventType>
void StateMachine<StateType, EventType>::addTransition(const StateType& fromState, const EventType& event,
                                                       const StateType& toState, ActionFunction action) {
    transitions[{fromState, event}] = toState;
    actions[{fromState, event}] = action;
}

template <typename StateType, typename EventType>
void StateMachine<StateType, EventType>::handleEvent(const EventType& event) {
    auto transitionKey = std::make_pair(currentState, event);
    auto transitionIt = transitions.find(transitionKey);

    if (transitionIt != transitions.end()) {
        auto actionIt = actions.find(transitionKey);
        if (actionIt != actions.end()) {
            actionIt->second();
        }

        currentState = transitionIt->second;
    }
}

template <typename StateType, typename EventType>
StateType StateMachine<StateType, EventType>::getCurrentState() const {
    return currentState;
}

}
}