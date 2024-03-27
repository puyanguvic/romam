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

namespace ns3 {
namespace open_routing {

// State base class Implementation

State::~State() {
}

void
State::Set_context(Context *context) {
    this->m_context = context;
}

// Context base class Implementation

Context::Context(State *state) : m_state(nullptr) {
    this->TransitionTo(state);
}

Context::~Context() {
    delete m_state;  // Ensure proper deletion of the current state
}

void
Context::TransitionTo(State *state) {
    if (state == nullptr) {
        std::cerr << "Error: Trying to transition to a null state." << std::endl;
        return;
    }

    std::cout << "Context: Transition to " << typeid(*state).name() << ".\n";
    if (this->m_state != nullptr) {
        delete this->m_state;
    }
    this->m_state = state;
    this->m_state->Set_context(this);
}

void
Context::Request() {
    if (this->m_state != nullptr) {
        this->m_state->Handle();
    } else {
        std::cerr << "Error: No current state to handle request." << std::endl;
    }
}

} // namespace open_routing
} // namespace ns3
