#include "spf-route-info-entry.h"

#include "ns3/assert.h"
#include "ns3/log.h"

#define MAX_UINT32 0xffffffff

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ShortestPathForestRIE");

ShortestPathForestRIE::ShortestPathForestRIE()
{
    NS_LOG_FUNCTION(this);
}

ShortestPathForestRIE::~ShortestPathForestRIE()
{
    NS_LOG_FUNCTION(this);
}

ShortestPathForestRIE::ShortestPathForestRIE(const ShortestPathForestRIE& route)
    : m_dest(route.m_dest),
      m_destNetworkMask(route.m_destNetworkMask),
      m_gateway(route.m_gateway),
      m_interface(route.m_interface),
      m_nextIface(route.m_nextIface),
      m_distance(route.m_distance)
{
    NS_LOG_FUNCTION(this << route);
}

ShortestPathForestRIE::ShortestPathForestRIE(const ShortestPathForestRIE* route)
    : m_dest(route->m_dest),
      m_destNetworkMask(route->m_destNetworkMask),
      m_gateway(route->m_gateway),
      m_interface(route->m_interface),
      m_nextIface(route->m_nextIface),
      m_distance(route->m_distance)
{
    NS_LOG_FUNCTION(this << route);
}

ShortestPathForestRIE::ShortestPathForestRIE(Ipv4Address dest,
                                             Ipv4Address gateway,
                                             uint32_t interface)
    : m_dest(dest),
      m_destNetworkMask(Ipv4Mask::GetOnes()),
      m_gateway(gateway),
      m_interface(interface),
      m_nextIface(MAX_UINT32),
      m_distance(MAX_UINT32)
{
}

ShortestPathForestRIE::ShortestPathForestRIE(Ipv4Address dest, uint32_t interface)
    : m_dest(dest),
      m_destNetworkMask(Ipv4Mask::GetOnes()),
      m_gateway(Ipv4Address::GetZero()),
      m_interface(interface),
      m_nextIface(MAX_UINT32),
      m_distance(MAX_UINT32)
{
}

ShortestPathForestRIE::ShortestPathForestRIE(Ipv4Address network,
                                             Ipv4Mask networkMask,
                                             Ipv4Address gateway,
                                             uint32_t interface)
    : m_dest(network),
      m_destNetworkMask(networkMask),
      m_gateway(gateway),
      m_interface(interface),
      m_nextIface(MAX_UINT32),
      m_distance(MAX_UINT32)
{
    NS_LOG_FUNCTION(this << network << networkMask << gateway << interface);
}

ShortestPathForestRIE::ShortestPathForestRIE(Ipv4Address network,
                                             Ipv4Mask networkMask,
                                             uint32_t interface)
    : m_dest(network),
      m_destNetworkMask(networkMask),
      m_gateway(Ipv4Address::GetZero()),
      m_interface(interface),
      m_nextIface(MAX_UINT32),
      m_distance(MAX_UINT32)
{
    NS_LOG_FUNCTION(this << network << networkMask << interface);
}

ShortestPathForestRIE::ShortestPathForestRIE(Ipv4Address dest,
                                             Ipv4Address gateway,
                                             uint32_t interface,
                                             uint32_t nextIface,
                                             uint32_t distance)
    : m_dest(dest),
      m_destNetworkMask(Ipv4Mask::GetOnes()),
      m_gateway(gateway),
      m_interface(interface),
      m_nextIface(nextIface),
      m_distance(distance)
{
    // std::cout << "CreateNetworkRouteTo with distance" << distance << std::endl;
    NS_LOG_FUNCTION(this << dest << gateway << interface << distance);
}

bool
ShortestPathForestRIE::IsHost() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask == Ipv4Mask::GetOnes();
}

Ipv4Address
ShortestPathForestRIE::GetDest() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

bool
ShortestPathForestRIE::IsNetwork() const
{
    NS_LOG_FUNCTION(this);
    return !IsHost();
}

bool
ShortestPathForestRIE::IsDefault() const
{
    NS_LOG_FUNCTION(this);
    return m_dest == Ipv4Address::GetZero();
}

Ipv4Address
ShortestPathForestRIE::GetDestNetwork() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

Ipv4Mask
ShortestPathForestRIE::GetDestNetworkMask() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask;
}

bool
ShortestPathForestRIE::IsGateway() const
{
    NS_LOG_FUNCTION(this);
    return m_gateway != Ipv4Address::GetZero();
}

Ipv4Address
ShortestPathForestRIE::GetGateway() const
{
    NS_LOG_FUNCTION(this);
    return m_gateway;
}

uint32_t
ShortestPathForestRIE::GetInterface() const
{
    NS_LOG_FUNCTION(this);
    return m_interface;
}

uint32_t
ShortestPathForestRIE::GetNextIface() const
{
    NS_LOG_FUNCTION(this);
    return m_nextIface;
}

uint32_t
ShortestPathForestRIE::GetDistance() const
{
    NS_LOG_FUNCTION(this);
    return m_distance;
}

ShortestPathForestRIE
ShortestPathForestRIE::CreateHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(dest << nextHop << interface);
    return ShortestPathForestRIE(dest, nextHop, interface);
}

ShortestPathForestRIE
ShortestPathForestRIE::CreateHostRouteTo(Ipv4Address dest, uint32_t interface)
{
    NS_LOG_FUNCTION(dest << interface);
    return ShortestPathForestRIE(dest, interface);
}

ShortestPathForestRIE
ShortestPathForestRIE::CreateHostRouteTo(Ipv4Address dest,
                                         Ipv4Address nextHop,
                                         uint32_t interface,
                                         uint32_t nextIface,
                                         uint32_t distance)
{
    NS_LOG_FUNCTION(dest << nextHop << interface << nextIface << distance);
    return ShortestPathForestRIE(dest, nextHop, interface, nextIface, distance);
}

ShortestPathForestRIE
ShortestPathForestRIE::CreateNetworkRouteTo(Ipv4Address network,
                                            Ipv4Mask networkMask,
                                            Ipv4Address nextHop,
                                            uint32_t interface)
{
    NS_LOG_FUNCTION(network << networkMask << nextHop << interface);
    return ShortestPathForestRIE(network, networkMask, nextHop, interface);
}

ShortestPathForestRIE
ShortestPathForestRIE::CreateNetworkRouteTo(Ipv4Address network,
                                            Ipv4Mask networkMask,
                                            uint32_t interface)
{
    NS_LOG_FUNCTION(network << networkMask << interface);
    return ShortestPathForestRIE(network, networkMask, interface);
}

ShortestPathForestRIE
ShortestPathForestRIE::CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(nextHop << interface);
    return ShortestPathForestRIE(Ipv4Address::GetZero(), Ipv4Mask::GetZero(), nextHop, interface);
}

std::ostream&
operator<<(std::ostream& os, const ShortestPathForestRIE& route)
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
operator==(const ShortestPathForestRIE a, const ShortestPathForestRIE b)
{
    return (a.GetDest() == b.GetDest() && a.GetDestNetworkMask() == b.GetDestNetworkMask() &&
            a.GetGateway() == b.GetGateway() && a.GetInterface() == b.GetInterface());
}

} // namespace ns3
