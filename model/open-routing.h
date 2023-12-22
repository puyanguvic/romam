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

#ifndef OPEN_ROUTING_H
#define OPEN_ROUTING_H

#include "ns3/ipv4-routing-protocol.h"

#include "ipv4-header.h"
#include "ipv4-routing-protocol.h"
#include "ipv4.h"

#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"

#include <list>
#include <stdint.h>
#include <utility>

// Add a doxygen group for this module.
// If you have more than one file, this should be in only one of them.
/**
 * \defgroup open-routing Description of the open-routing
 */

namespace ns3
{

class Packet;
class NetDevice;
class Ipv4Interface;
class Ipv4Address;
class Ipv4Header;
class Ipv4RoutingTableEntry;
class Ipv4MulticastRoutingTableEntry;
class Node;

namespace open_routing
{

// Each class should be documented using Doxygen,
// and have an \ingroup open-routing directive

class OpenRouting : public Ipv4RoutingProtocol
{
public:
    /**
     * \brief The interface Id associated with this class.
     * \return type identifier
     */
    static TypeId GetTypeId();
    OpenRouting(/* args */);
    ~OpenRouting() override;

    Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p,
                               const Ipv4Header& header,
                               Ptr<NetDevice> oif,
                               Socket::SocketErrno& sockerr) override;

    bool RouteInput(Ptr<const Packet> p,
                    const Ipv4Header& header,
                    Ptr<const NetDevice> idev,
                    const UnicastForwardCallback& ucb,
                    const MulticastForwardCallback& mcb,
                    const LocalDeliverCallback& lcb,
                    const ErrorCallback& ecb) override;

    void NotifyInterfaceUp(uint32_t interface) override;
    void NotifyInterfaceDown(uint32_t interface) override;
    void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
    void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
    void SetIpv4(Ptr<Ipv4> ipv4) override;
    void PrintRoutingTable(Ptr<OutputStreamWrapper> stream,
                           Time::Unit unit = Time::S) const override;


    /**
     * @brief build up socket to support unicast and multicast
     * 
     */
    
private:
    /**
     * \brief Ipv4 reference.
     */
    Ptr<Ipv4> m_ipv4;

    /**
     * \brief database
     * 
     */

    /**
     * \brief routing table
     * 
     */

    /**
     * \brief state machine 
     * 
     */
};


    
} // namespace open_routing
} // namespace ns3

#endif /* OPEN_ROUTING_H */