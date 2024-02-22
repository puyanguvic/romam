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

#include "ospf-FSM.h"

namespace ns3
{

namespace open_routing
{

// ---------------------------------------------------------------------------
//
// State base class Implementation
//
// ---------------------------------------------------------------------------
void
Down::Handle ()
{
    // Check for received Hello Packets
    if (/* condition to check for received Hello packets */) {
        // If a Hello packet is received, transition to the next state (e.g., Init or Attempt state)
        TransitionToNextState();
    } else {
        // If no Hello packet is received, continue to send Hello packets
        SendHelloPackets();

        // Check the RouterDeadInterval
        if (/* condition to check if the RouterDeadInterval has expired */) {
            // If the RouterDeadInterval has expired, take necessary action
            // This might include logging the event, alerting the system, or other actions
            HandleRouterDeadIntervalExpiration();
        }
    }
}

void
Attempt::Handle () 
{
    // Send a unicast Hello packet to the neighbor.
    // This is specifically for NBMA networks where multicast is not effective.
    SendUnicastHello();

    // Check for Hello packet response within the dead interval.
    if (IsDeadIntervalExpired()) {
        // If there's no Hello response within the dead interval,
        // transition back to the Down state.
        TransitionToDownState();
    } else if (IsHelloReceived()) {
        // If a Hello packet is received from the neighbor,
        // transition to the Init state.
        TransitionToInitState();
    }
}



void
Init::Handle ()
{
    // Check for own Router ID in received Hello packet
    
    // Transition to TwoWay state if bidirectional communication is established
}

void
TwoWay::Handle ()
{
    // Decide on forming full adjacency
    // Handle DR and BDR election process
    // Transition to Exstart state with DR/BDR or other routers on P2P networks
}

void
Exstart::Handle ()
{
    // Initialize sequence numbers

    // Determine primary-secondary relationship

    // Transition to Exchange state once initial sequence number is accepted

}

void
Exchange::Handle ()
{
    // Exchange DBD packets

    // Send and manage Link-State Request packets

    // Transition to Loading state once all DBDs are exchanged

}

void
Loading::Handle ()
{
    // Exchange LSAs based on requests

    // Acknowledge received LSAs

    // Transition to Full state once all requested LSAs are received

}

void
Full::Handle ()
{
    // Perform routine maintenance if necessary

    // Handle any retransmissions due to link changes

    // Transition to Down state upon link failure or neighbor loss

}
}
}