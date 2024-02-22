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

#ifndef OSPF_FSM_H
#define OSPF_FSM_H

#include <iostream>
#include <map>
#include <functional>
#include <utility>
#include "ns3/open-routing-module.h"

namespace ns3
{
namespace open_routing
{

/**
 * @brief This is the first OSPF neighbor state. It means that no information (hello)
 * has been received from this neighbor, but hello packets can still be sent to the neighbor
 * in this state.
 * In the full adjacent neighbor state, if a router does not receive hello packet from
 * a neighbor within the RouterDeadInterval time (RouterDeadInterval = 4 * HelloInterval by default)
 * or if the manually configured neighbor is removed from the configuration, then the neighbor state
 * changes from Full to Down.
 */
class  Down : public State {
    public:
    void Handle () override;

};

/**
 * @brief This state is only valid for manually configured neighbor in an NBMA environment.
 * In Attempt state, the router sends unicast hello packets every poll interval to the neighbor,
 * from which hellos have not been received within the dead interval.
 */
class Attempt : public State {
    void Handle () override;
};

/**
 * @brief This state specifies that the router has received a hello packet from its neighbor,
 * but the received router ID was not included in the hello packet. When a router receives a 
 * hello packet from neighbor, it must list the sender router ID in its hello packet as an 
 * acknowledgment that it received a valid hello packet.
 * 
 */
class Init : public State {
    public:
    void Handle () override;
};

/**
 * @brief This state designates that bi-directional communication has been established between
 * two routers. Bi-directional means that each router sees the hello packet from the other router.
 * This state is attained when the router receiving the hello packet sees its own Router ID within 
 * the received hello packet neighbor field. At this state, a router decides whether to become 
 * adjacent with this neighbor. On broadcast media and non-broadcast multi-access networks, a 
 * router becomes full only with the designated router (DR) and the backup designated router (BDR);
 * it stays in the 2-way state with all other neighbors. On Point-to-point and Point-to-multipoint
 *  networks, a router becomes full with all connected routers.
 * 
 * At the end of this stage, the DR and BDR for broadcast and non-broadcast multi-access networks 
 * are elected. For more information on the DR election process, refer to DR Election.
 */
class TwoWay : public State {
    public:
    void Handle () override;
};

/**
 * @brief Once the DR and BDR are elected, the actual process of the exchange link state information 
 * can start between the routers and their DR and BDR.
 * 
 * In this state, the routers and their DR and BDR establish a primary-secondary relationship and choose
 * the initial sequence number for adjacency formation. The router with the higher router ID becomes the 
 * primary and starts the exchange, and as such, is the only router that can increment the sequence number. 
 * You would logically conclude that the DR/BDR with the highest router ID is the primary for this process. 
 * The DR/BDR election could be because of a higher priority configured on the router instead of highest 
 * router ID. Thus, it is possible that a DR plays a secondary role. Also, that primary/secondary election 
 * is on a per-neighbor basis.
 * 
 */
class Exstart : public State {
    public:
    void Handle () override;
};

/**
 * @brief In the exchange state, OSPF routers exchange database descriptor (DBD) packets. Database 
 * descriptors contain link-state advertisement (LSA) headers only and describe the contents of the 
 * entire link-state database. Each DBD packet has a sequence number which can be incremented only 
 * by primary which is explicitly acknowledged by secondary. Routers also send link-state request 
 * packets and link-state update packets (which contain the entire LSA) in this state. The contents 
 * of the DBD received are compared to the information contained in the routers link-state database 
 * to check if new or more current link-state information is available with the neighbor.
 * 
 */
class Exchange : public State {
    public:
    void Handle () override;
};

/**
 * @brief In this state, the actual exchange of link state information occurs. Based on the information
 *  provided by the DBDs, routers send link-state request packets. The neighbor then provides the 
 * requested link-state information in link-state update packets. During the adjacency, if a router 
 * receives an outdated or lost LSA, it sends a link-state request packet for that LSA. All link-state 
 * update packets are acknowledged.
 * 
 */
class Loading : public State {
    public:
    void Handle () override;
};

/**
 * @brief In this state, routers are fully adjacent with each other. All the router and network LSAs 
 * are exchanged and the routers' databases are fully synchronized.
 * 
 * Full is the normal state for an OSPF router. If a router is stuck in another state, it is an 
 * indication that there are problems when the adjacencies are formed. The only exception to this
 * is the 2-way state, which is normal in a broadcast network. Routers achieve the FULL state with 
 * their DR and BDR in NBMA/broadcast media and FULL state with every neighbor in the residual media 
 * such as point-to-point and point-to-multipoint.
 * 
 */
class Full : public State {
    public:
    void Handle () override;
};

} // namespace open_routing
} // namespace ns3

#endif // STATE_MACHINE_H
