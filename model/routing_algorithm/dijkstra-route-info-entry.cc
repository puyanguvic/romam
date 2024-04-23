#include "dijkstra-route-info-entry.h"

#include "ns3/assert.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DijkstraRTE");

DijkstraRTE::DijkstraRTE()
{
    NS_LOG_FUNCTION(this);
}

DijkstraRTE::~DijkstraRTE()
{
    NS_LOG_FUNCTION(this);
}

DijkstraRTE::DijkstraRTE(const DijkstraRTE& route)
    : m_dest(route.m_dest),
      m_destNetworkMask(route.m_destNetworkMask),
      m_gateway(route.m_gateway),
      m_interface(route.m_interface)
{
    NS_LOG_FUNCTION(this << route);
}

DijkstraRTE::DijkstraRTE(const DijkstraRTE* route)
    : m_dest(route->m_dest),
      m_destNetworkMask(route->m_destNetworkMask),
      m_gateway(route->m_gateway),
      m_interface(route->m_interface)
{
    NS_LOG_FUNCTION(this << route);
}

DijkstraRTE::DijkstraRTE(Ipv4Address dest, Ipv4Address gateway, uint32_t interface)
    : m_dest(dest),
      m_destNetworkMask(Ipv4Mask::GetOnes()),
      m_gateway(gateway),
      m_interface(interface)
{
}

DijkstraRTE::DijkstraRTE(Ipv4Address dest, uint32_t interface)
    : m_dest(dest),
      m_destNetworkMask(Ipv4Mask::GetOnes()),
      m_gateway(Ipv4Address::GetZero()),
      m_interface(interface)
{
}

DijkstraRTE::DijkstraRTE(Ipv4Address network,
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

DijkstraRTE::DijkstraRTE(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
    : m_dest(network),
      m_destNetworkMask(networkMask),
      m_gateway(Ipv4Address::GetZero()),
      m_interface(interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << interface);
}

bool
DijkstraRTE::IsHost() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask == Ipv4Mask::GetOnes();
}

Ipv4Address
DijkstraRTE::GetDest() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

bool
DijkstraRTE::IsNetwork() const
{
    NS_LOG_FUNCTION(this);
    return !IsHost();
}

bool
DijkstraRTE::IsDefault() const
{
    NS_LOG_FUNCTION(this);
    return m_dest == Ipv4Address::GetZero();
}

Ipv4Address
DijkstraRTE::GetDestNetwork() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

Ipv4Mask
DijkstraRTE::GetDestNetworkMask() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask;
}

bool
DijkstraRTE::IsGateway() const
{
    NS_LOG_FUNCTION(this);
    return m_gateway != Ipv4Address::GetZero();
}

Ipv4Address
DijkstraRTE::GetGateway() const
{
    NS_LOG_FUNCTION(this);
    return m_gateway;
}

uint32_t
DijkstraRTE::GetInterface() const
{
    NS_LOG_FUNCTION(this);
    return m_interface;
}

DijkstraRTE
DijkstraRTE::CreateHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(dest << nextHop << interface);
    return DijkstraRTE(dest, nextHop, interface);
}

DijkstraRTE
DijkstraRTE::CreateHostRouteTo(Ipv4Address dest, uint32_t interface)
{
    NS_LOG_FUNCTION(dest << interface);
    return DijkstraRTE(dest, interface);
}

DijkstraRTE
DijkstraRTE::CreateNetworkRouteTo(Ipv4Address network,
                                  Ipv4Mask networkMask,
                                  Ipv4Address nextHop,
                                  uint32_t interface)
{
    NS_LOG_FUNCTION(network << networkMask << nextHop << interface);
    return DijkstraRTE(network, networkMask, nextHop, interface);
}

DijkstraRTE
DijkstraRTE::CreateNetworkRouteTo(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
{
    NS_LOG_FUNCTION(network << networkMask << interface);
    return DijkstraRTE(network, networkMask, interface);
}

DijkstraRTE
DijkstraRTE::CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(nextHop << interface);
    return DijkstraRTE(Ipv4Address::GetZero(), Ipv4Mask::GetZero(), nextHop, interface);
}

std::ostream&
operator<<(std::ostream& os, const DijkstraRTE& route)
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
operator==(const DijkstraRTE a, const DijkstraRTE b)
{
    return (a.GetDest() == b.GetDest() && a.GetDestNetworkMask() == b.GetDestNetworkMask() &&
            a.GetGateway() == b.GetGateway() && a.GetInterface() == b.GetInterface());
}

} // namespace ns3
