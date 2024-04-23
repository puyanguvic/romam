#include "dijkstra-route-info-entry.h"

#include "ns3/assert.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DijkstraRIE");

DijkstraRIE::DijkstraRIE()
{
    NS_LOG_FUNCTION(this);
}

DijkstraRIE::~DijkstraRIE()
{
    NS_LOG_FUNCTION(this);
}

DijkstraRIE::DijkstraRIE(const DijkstraRIE& route)
    : m_dest(route.m_dest),
      m_destNetworkMask(route.m_destNetworkMask),
      m_gateway(route.m_gateway),
      m_interface(route.m_interface)
{
    NS_LOG_FUNCTION(this << route);
}

DijkstraRIE::DijkstraRIE(const DijkstraRIE* route)
    : m_dest(route->m_dest),
      m_destNetworkMask(route->m_destNetworkMask),
      m_gateway(route->m_gateway),
      m_interface(route->m_interface)
{
    NS_LOG_FUNCTION(this << route);
}

DijkstraRIE::DijkstraRIE(Ipv4Address dest, Ipv4Address gateway, uint32_t interface)
    : m_dest(dest),
      m_destNetworkMask(Ipv4Mask::GetOnes()),
      m_gateway(gateway),
      m_interface(interface)
{
}

DijkstraRIE::DijkstraRIE(Ipv4Address dest, uint32_t interface)
    : m_dest(dest),
      m_destNetworkMask(Ipv4Mask::GetOnes()),
      m_gateway(Ipv4Address::GetZero()),
      m_interface(interface)
{
}

DijkstraRIE::DijkstraRIE(Ipv4Address network,
                         Ipv4Mask networkMask,
                         Ipv4Address gateway,
                         uint32_t interface)
    : m_dest(network),
      m_destNetworkMask(networkMask),
      m_gateway(gateway),
      m_interface(interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << gateway << interface);
}

DijkstraRIE::DijkstraRIE(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
    : m_dest(network),
      m_destNetworkMask(networkMask),
      m_gateway(Ipv4Address::GetZero()),
      m_interface(interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << interface);
}

bool
DijkstraRIE::IsHost() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask == Ipv4Mask::GetOnes();
}

Ipv4Address
DijkstraRIE::GetDest() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

bool
DijkstraRIE::IsNetwork() const
{
    NS_LOG_FUNCTION(this);
    return !IsHost();
}

bool
DijkstraRIE::IsDefault() const
{
    NS_LOG_FUNCTION(this);
    return m_dest == Ipv4Address::GetZero();
}

Ipv4Address
DijkstraRIE::GetDestNetwork() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

Ipv4Mask
DijkstraRIE::GetDestNetworkMask() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask;
}

bool
DijkstraRIE::IsGateway() const
{
    NS_LOG_FUNCTION(this);
    return m_gateway != Ipv4Address::GetZero();
}

Ipv4Address
DijkstraRIE::GetGateway() const
{
    NS_LOG_FUNCTION(this);
    return m_gateway;
}

uint32_t
DijkstraRIE::GetInterface() const
{
    NS_LOG_FUNCTION(this);
    return m_interface;
}

DijkstraRIE
DijkstraRIE::CreateHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(dest << nextHop << interface);
    return DijkstraRIE(dest, nextHop, interface);
}

DijkstraRIE
DijkstraRIE::CreateHostRouteTo(Ipv4Address dest, uint32_t interface)
{
    NS_LOG_FUNCTION(dest << interface);
    return DijkstraRIE(dest, interface);
}

DijkstraRIE
DijkstraRIE::CreateNetworkRouteTo(Ipv4Address network,
                                  Ipv4Mask networkMask,
                                  Ipv4Address nextHop,
                                  uint32_t interface)
{
    NS_LOG_FUNCTION(network << networkMask << nextHop << interface);
    return DijkstraRIE(network, networkMask, nextHop, interface);
}

DijkstraRIE
DijkstraRIE::CreateNetworkRouteTo(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
{
    NS_LOG_FUNCTION(network << networkMask << interface);
    return DijkstraRIE(network, networkMask, interface);
}

DijkstraRIE
DijkstraRIE::CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(nextHop << interface);
    return DijkstraRIE(Ipv4Address::GetZero(), Ipv4Mask::GetZero(), nextHop, interface);
}

std::ostream&
operator<<(std::ostream& os, const DijkstraRIE& route)
{
    if (route.IsDefault())
    {
        NS_ASSERT(route.IsGateway());
        os << "default out=" << route.GetInterface() << ", next hop=" << route.GetGateway();
    }
    else if (route.IsHost())
    {
        if (route.IsGateway())
        {
            os << "host=" << route.GetDest() << ", out=" << route.GetInterface()
               << ", next hop=" << route.GetGateway();
        }
        else
        {
            os << "host=" << route.GetDest() << ", out=" << route.GetInterface();
        }
    }
    else if (route.IsNetwork())
    {
        if (route.IsGateway())
        {
            os << "network=" << route.GetDestNetwork() << ", mask=" << route.GetDestNetworkMask()
               << ",out=" << route.GetInterface() << ", next hop=" << route.GetGateway();
        }
        else
        {
            os << "network=" << route.GetDestNetwork() << ", mask=" << route.GetDestNetworkMask()
               << ",out=" << route.GetInterface();
        }
    }
    else
    {
        NS_ASSERT(false);
    }
    return os;
}

bool
operator==(const DijkstraRIE a, const DijkstraRIE b)
{
    return (a.GetDest() == b.GetDest() && a.GetDestNetworkMask() == b.GetDestNetworkMask() &&
            a.GetGateway() == b.GetGateway() && a.GetInterface() == b.GetInterface());
}

} // namespace ns3
