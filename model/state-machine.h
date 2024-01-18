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
    /**
     * @brief The base state class declares methods that all Concrete State should
     * implement and also provides a backreference to the Context object, associated
     * with the State. This backreference can be used by States to transition the 
     * context to another State.
     */
    class Context;

    class State {
        public:
            virtual ~State();

            void Set_context(Context *context);
            virtual void Handle() = 0;

        protected:
            Context *m_context;
        };


    /**
     * @brief This Context defines the interface of interest to clients. It also 
     * maintains a reference to an instance of a State subclass, which represents
     * the current state of Context.
     */
    class Context
    {
    
    public:
        Context (State *state);
        
        ~Context();

        // The context allows changing the State Object at runtime.
        void TransitionTo (State *state);

        // The context delegates part of its behavior to the current State Object.
        void Request ();
    private:
        State *m_state;
    };
} // namespace open_routing
} // namespace ns3

#endif // STATE_MACHINE_H
