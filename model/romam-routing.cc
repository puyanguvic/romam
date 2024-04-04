//
// Copyright (c) 2008 University of Washington
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "romam-routing.h"

#include "ns3/global-route-manager.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-table-entry.h"


#include <iomanip>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RomamRouting");

NS_OBJECT_ENSURE_REGISTERED(RomamRouting);

TypeId
RomamRouting::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::RomamRouting")
            .SetParent<Object>()
            .SetGroupName("Romam");
    return tid;
}

RomamRouting::RomamRouting()
    : m_respondToInterfaceEvents(false)
{
    NS_LOG_FUNCTION(this);

    m_rand = CreateObject<UniformRandomVariable>();
}

RomamRouting::~RomamRouting()
{
    NS_LOG_FUNCTION(this);
}

void
RomamRouting::AddHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(this << dest << nextHop << interface);
}

void
RomamRouting::AddHostRouteTo(Ipv4Address dest, uint32_t interface)
{
    NS_LOG_FUNCTION(this << dest << interface);
}

void
RomamRouting::AddNetworkRouteTo(Ipv4Address network,
                                     Ipv4Mask networkMask,
                                     Ipv4Address nextHop,
                                     uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << nextHop << interface);
}

void
RomamRouting::AddNetworkRouteTo(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << interface);
}

void
RomamRouting::AddASExternalRouteTo(Ipv4Address network,
                                        Ipv4Mask networkMask,
                                        Ipv4Address nextHop,
                                        uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << nextHop << interface);
}

Ptr<Ipv4Route>
RomamRouting::LookupGlobal(Ipv4Address dest, Ptr<NetDevice> oif)
{
    NS_LOG_FUNCTION(this << dest << oif);
    NS_LOG_LOGIC("Looking for route for destination " << dest);
    Ptr<Ipv4Route> rtentry = nullptr;
    return rtentry;
}

uint32_t
RomamRouting::GetNRoutes() const
{
    NS_LOG_FUNCTION(this);
    uint32_t n = 0;
    n += m_hostRoutes.size();
    n += m_networkRoutes.size();
    n += m_ASexternalRoutes.size();
    return n;
}

Ipv4RoutingTableEntry*
RomamRouting::GetRoute(uint32_t index) const
{
    NS_LOG_FUNCTION(this << index);
    return nullptr;
}

void
RomamRouting::RemoveRoute(uint32_t index)
{
    NS_LOG_FUNCTION(this << index);
}

int64_t
RomamRouting::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_rand->SetStream(stream);
    return 1;
}

void
RomamRouting::DoDispose()
{
    NS_LOG_FUNCTION(this);
    for (auto i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i = m_hostRoutes.erase(i))
    {
        delete (*i);
    }
    for (auto j = m_networkRoutes.begin(); j != m_networkRoutes.end(); j = m_networkRoutes.erase(j))
    {
        delete (*j);
    }
    for (auto l = m_ASexternalRoutes.begin(); l != m_ASexternalRoutes.end();
         l = m_ASexternalRoutes.erase(l))
    {
        delete (*l);
    }

    Ipv4RoutingProtocol::DoDispose();
}

// Formatted like output of "route -n" command
void
RomamRouting::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
    NS_LOG_FUNCTION(this << stream);
    std::ostream* os = stream->GetStream();
    // Copy the current ostream state
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    *os << std::resetiosflags(std::ios::adjustfield) << std::setiosflags(std::ios::left);

    *os << "Node: " << m_ipv4->GetObject<Node>()->GetId() << ", Time: " << Now().As(unit)
        << ", Local time: " << m_ipv4->GetObject<Node>()->GetLocalTime().As(unit)
        << ", RomamRouting table" << std::endl;
}

Ptr<Ipv4Route>
RomamRouting::RouteOutput(Ptr<Packet> p,
                               const Ipv4Header& header,
                               Ptr<NetDevice> oif,
                               Socket::SocketErrno& sockerr)
{
    NS_LOG_FUNCTION(this << p << &header << oif << &sockerr);
    Ptr<Ipv4Route> rtentry = nullptr;
    return rtentry;
}

bool
RomamRouting::RouteInput(Ptr<const Packet> p,
                              const Ipv4Header& header,
                              Ptr<const NetDevice> idev,
                              const UnicastForwardCallback& ucb,
                              const MulticastForwardCallback& mcb,
                              const LocalDeliverCallback& lcb,
                              const ErrorCallback& ecb)
{
    NS_LOG_FUNCTION(this << p << header << header.GetSource() << header.GetDestination() << idev
                         << &lcb << &ecb);
    // Check if input device supports IP
    NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
    return false;
}

void
RomamRouting::NotifyInterfaceUp(uint32_t i)
{
    NS_LOG_FUNCTION(this << i);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        // GlobalRouteManager::DeleteGlobalRoutes();
        // GlobalRouteManager::BuildGlobalRoutingDatabase();
        // GlobalRouteManager::InitializeRoutes();
    }
}

void
RomamRouting::NotifyInterfaceDown(uint32_t i)
{
    NS_LOG_FUNCTION(this << i);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        // GlobalRouteManager::DeleteGlobalRoutes();
        // GlobalRouteManager::BuildGlobalRoutingDatabase();
        // GlobalRouteManager::InitializeRoutes();
    }
}

void
RomamRouting::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        // GlobalRouteManager::DeleteGlobalRoutes();
        // GlobalRouteManager::BuildGlobalRoutingDatabase();
        // GlobalRouteManager::InitializeRoutes();
    }
}

void
RomamRouting::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        // GlobalRouteManager::DeleteGlobalRoutes();
        // GlobalRouteManager::BuildGlobalRoutingDatabase();
        // GlobalRouteManager::InitializeRoutes();
    }
}

void
RomamRouting::SetIpv4(Ptr<Ipv4> ipv4)
{
    NS_LOG_FUNCTION(this << ipv4);
    NS_ASSERT(!m_ipv4 && ipv4);
    m_ipv4 = ipv4;
}

} // namespace ns3
