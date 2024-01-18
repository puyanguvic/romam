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
    // Send Hello packets

    // Set timer to wait for replies

    // Transition to Attempt state if in NBMA network

    // Transition to Init state upon receiving a Hello packet
}

void
Attempt::Handle () 
{
    // Send unicast Hello packet in NBMA networks

    // Transition to Down state if no reply within the dead interval

    // Transition to Init State upon receiveing a Hello packet
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