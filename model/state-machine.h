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

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <iostream>
#include <map>
#include <functional>
#include <utility>

namespace ns3
{
namespace open_routing
{

template <typename StateType, typename EventType>
class StateMachine {
public:
    using ActionFunction = std::function<void()>;

    StateMachine();
    virtual ~StateMachine();

    void addTransition(const StateType& fromState, const EventType& event,
                       const StateType& toState, ActionFunction action);

    void handleEvent(const EventType& event);

    StateType getCurrentState() const;

protected:
    StateType currentState;
    std::map<std::pair<StateType, EventType>, StateType> transitions;
    std::map<std::pair<StateType, EventType>, ActionFunction> actions;
};

} // namespace open_routing
} // namespace ns3

#endif // STATE_MACHINE_H







// // Define the states for the routing protocol
// enum class RouterState {
//     Idle,
//     Configuring,
//     Active,
//     Error
// };

// // Define the events for the routing protocol
// enum class RouterEvent {
//     Start,
//     Configure,
//     Activate,
//     Deactivate,
//     ErrorDetected
// };



// // Define the states for the routing protocol
// enum class RouterState {
//     Idle,
//     Configuring,
//     Active,
//     Error
// };

// // Define the events for the routing protocol
// enum class RouterEvent {
//     Start,
//     Configure,
//     Activate,
//     Deactivate,
//     ErrorDetected
// };



// // Example usage
// int main() {
//     // Instantiate the RoutingProtocol with an initial state
//     RoutingProtocol<RouterState, RouterEvent> router(RouterState::Idle);

//     // Add transitions based on the routing protocol's logic
//     router.addTransition(RouterState::Idle, RouterEvent::Start, RouterState::Configuring, [](){
//         std::cout << "Transitioning to Configuring state..." << std::endl;
//     });

//     router.addTransition(RouterState::Configuring, RouterEvent::Configure, RouterState::Active, [](){
//         std::cout << "Transitioning to Active state..." << std::endl;
//     });

//     router.addTransition(RouterState::Active, RouterEvent::Deactivate, RouterState::Idle, [](){
//         std::cout << "Transitioning to Idle state..." << std::endl;
//     });

//     router.addTransition(RouterState::Active, RouterEvent::ErrorDetected, RouterState::Error, [](){
//         std::cout << "Transitioning to Error state..." << std::endl;
//     });

//     // Process events to trigger state transitions
//     router.processEvent(RouterEvent::Start);
//     router.processEvent(RouterEvent::Configure);
//     router.processEvent(RouterEvent::Deactivate);

//     // Get the current state of the routing protocol
//     std::cout << "Current State: " << static_cast<int>(router.getCurrentState()) << std::endl;

//     return 0;
// }
