#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-table-entry.h"

#include "romam-routing.h"

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

Ptr<Ipv4Route>
RomamRouting::RouteOutput(Ptr<Packet> p,
                          const Ipv4Header& header,
                          Ptr<NetDevice> oif,
                          Socket::SocketErrno& sockerr)
{
    NS_LOG_FUNCTION(this << p << &header << oif << &sockerr);
    Ptr<Ipv4Route> rtentry = nullptr;
    // TODO: Query routing cache for an existing route, for an outbound packet
    //
    // This lookup is used by transport protocols. It does not cause any packet
    // to be forwarded, and is synchoronous. Can be used for mulicast or unicast.
    // The Linux equivalent is ip_route_output ()
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
    NS_LOG_FUNCTION(this << p << header << header.GetSource() 
                         << header.GetDestination() << idev
                         << &lcb << &ecb);
    NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
    // TODO: This lookup is used in the forwarding process.  The packet is
    // handed over to the Ipv4RoutingProtocol, and will get forwarded onward
    // by one of the callbacks.  The Linux equivalent is ip_route_input().
    // There are four valid outcomes, and a matching callbacks to handle each.
    return false;
}

void
RomamRouting::NotifyInterfaceUp(uint32_t i)
{
    NS_LOG_FUNCTION(this << i);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        // TODO: Protocols are expected to implement this method to be 
        // notified of the state change of an interface in a node.
    }
}

void
RomamRouting::NotifyInterfaceDown(uint32_t i)
{
    NS_LOG_FUNCTION(this << i);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        // TODO: Protocols are expected to implement this method to be 
        // notified of the state change of an interface in a node.
    }
}

void
RomamRouting::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        // TODO: Protocols are expected to implement this method to be 
        // notified of the state change of an interface in a node.
    }
}

void
RomamRouting::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        // TODO: Protocols are expected to implement this method to be 
        // notified of the state change of an interface in a node.
    }
}

void
RomamRouting::SetIpv4(Ptr<Ipv4> ipv4)
{
    NS_LOG_FUNCTION(this << ipv4);
    NS_ASSERT(!m_ipv4 && ipv4);
    m_ipv4 = ipv4;
}

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
    // TODO: Print the Routing Table entries
}

void
RomamRouting::AddHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(this << dest << nextHop << interface);
    auto route = new Ipv4RouteInfoEntry();
    *route = Ipv4RouteInfoEntry::CreateHostRouteTo(dest, nextHop, interface);
    m_hostRoutes.push_back(route);
}

void
RomamRouting::AddHostRouteTo(Ipv4Address dest, uint32_t interface)
{
    NS_LOG_FUNCTION(this << dest << interface);
    auto route = new Ipv4RouteInfoEntry();
    *route = Ipv4RouteInfoEntry::CreateHostRouteTo(dest, interface);
    m_hostRoutes.push_back(route);
}

void
RomamRouting::AddNetworkRouteTo(Ipv4Address network,
                                     Ipv4Mask networkMask,
                                     Ipv4Address nextHop,
                                     uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << nextHop << interface);
    auto route = new Ipv4RouteInfoEntry();
    *route = Ipv4RouteInfoEntry::CreateNetworkRouteTo(network, networkMask, nextHop, interface);
    m_networkRoutes.push_back(route);
}

void
RomamRouting::AddNetworkRouteTo(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << interface);
    auto route = new Ipv4RouteInfoEntry();
    *route = Ipv4RouteInfoEntry::CreateNetworkRouteTo(network, networkMask, interface);
    m_networkRoutes.push_back(route);
}

void
RomamRouting::AddASExternalRouteTo(Ipv4Address network,
                                   Ipv4Mask networkMask,
                                   Ipv4Address nextHop,
                                   uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << nextHop << interface);
    auto route = new Ipv4RouteInfoEntry();
    *route = Ipv4RouteInfoEntry::CreateNetworkRouteTo(network, networkMask, nextHop, interface);
    m_ASexternalRoutes.push_back(route);
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

Ipv4RouteInfoEntry*
RomamRouting::GetRoute(uint32_t index) const
{
    NS_LOG_FUNCTION(this << index);
    if (index < m_hostRoutes.size())
    {
        uint32_t tmp = 0;
        for (HostRoutesCI i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
        {
            if (tmp == index)
            {
                return *i;
            }
            tmp++;
        }
    }
    NS_ASSERT(false);
    // quiet compiler.
    return nullptr;
}

void
RomamRouting::RemoveRoute(uint32_t index)
{
    NS_LOG_FUNCTION(this << index);
    if (index < m_hostRoutes.size())
    {
        uint32_t tmp = 0;
        for (HostRoutesI i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
        {
            if (tmp == index)
            {
                NS_LOG_LOGIC("Removing route " << index << "; size = " << m_hostRoutes.size());
                delete *i;
                m_hostRoutes.erase(i);
                NS_LOG_LOGIC("Done removing host route "
                             << index << "; host route remaining size = " << m_hostRoutes.size());
                return;
            }
            tmp++;
        }
    }
    NS_ASSERT (false);
}

int64_t
RomamRouting::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_rand->SetStream(stream);
    return 1;
}

// Ptr<Ipv4Route>
// RomamRouting::LookupRoute (Ipv4Address dest,
//                            Ptr<NetDevice> oif = 0)
// {
//     NS_LOG_FUNCTION (this << dest << oif);
//     Ptr<Ipv4Route> rtentry = nullptr;
//     // Todo : Find the route to destination
//     return rtentry;
// }

Ptr<Ipv4Route>
RomamRouting::LookupRoute (Ipv4Address dest, Ptr<NetDevice> oif)
{
    NS_LOG_FUNCTION (this << dest << oif);
    NS_LOG_LOGIC ("Looking for route for destination " << dest);
    // TODO: Look up route
    Ptr<Ipv4Route> rtentry = 0;
    return rtentry;
}


void
RomamRouting::DoInitialize (void)
{
    NS_LOG_FUNCTION (this);
    // Initialize the routing protocol
    Ipv4RoutingProtocol::DoInitialize ();
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

} // namespace ns3
