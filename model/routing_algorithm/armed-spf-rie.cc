#include "armed-spf-rie.h"

#include "../datapath/arm-value-db.h"

#include "ns3/assert.h"
#include "ns3/log.h"

#include <cmath>

#define MAX_UINT32 0xffffffff

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ArmedSpfRIE");

ArmedSpfRIE::ArmedSpfRIE()
{
    NS_LOG_FUNCTION(this);
}

ArmedSpfRIE::~ArmedSpfRIE()
{
    NS_LOG_FUNCTION(this);
}

ArmedSpfRIE::ArmedSpfRIE(const ArmedSpfRIE& route)
    : m_dest(route.m_dest),
      m_destNetworkMask(route.m_destNetworkMask),
      m_gateway(route.m_gateway),
      m_interface(route.m_interface),
      m_nextIface(route.m_nextIface),
      m_distance(route.m_distance),
      m_cumulative_loss(route.m_cumulative_loss),
      m_num_pulls(route.m_num_pulls)
{
    NS_LOG_FUNCTION(this << route);
}

ArmedSpfRIE::ArmedSpfRIE(const ArmedSpfRIE* route)
    : m_dest(route->m_dest),
      m_destNetworkMask(route->m_destNetworkMask),
      m_gateway(route->m_gateway),
      m_interface(route->m_interface),
      m_nextIface(route->m_nextIface),
      m_distance(route->m_distance),
      m_cumulative_loss(route->m_cumulative_loss),
      m_num_pulls(route->m_num_pulls)
{
    NS_LOG_FUNCTION(this << route);
}

ArmedSpfRIE::ArmedSpfRIE(Ipv4Address dest, Ipv4Address gateway, uint32_t interface)
    : m_dest(dest),
      m_destNetworkMask(Ipv4Mask::GetOnes()),
      m_gateway(gateway),
      m_interface(interface),
      m_nextIface(MAX_UINT32),
      m_distance(MAX_UINT32),
      m_cumulative_loss(0.0),
      m_num_pulls(0)
{
    NS_LOG_FUNCTION(this);
}

ArmedSpfRIE::ArmedSpfRIE(Ipv4Address dest, uint32_t interface)
    : m_dest(dest),
      m_destNetworkMask(Ipv4Mask::GetOnes()),
      m_gateway(Ipv4Address::GetZero()),
      m_interface(interface),
      m_nextIface(MAX_UINT32),
      m_distance(MAX_UINT32),
      m_cumulative_loss(0.0),
      m_num_pulls(0)
{
    NS_LOG_FUNCTION(this);
}

ArmedSpfRIE::ArmedSpfRIE(Ipv4Address network,
                         Ipv4Mask networkMask,
                         Ipv4Address gateway,
                         uint32_t interface)
    : m_dest(network),
      m_destNetworkMask(networkMask),
      m_gateway(gateway),
      m_interface(interface),
      m_nextIface(MAX_UINT32),
      m_distance(MAX_UINT32),
      m_cumulative_loss(0.0),
      m_num_pulls(0)
{
    NS_LOG_FUNCTION(this << network << networkMask << gateway << interface);
}

ArmedSpfRIE::ArmedSpfRIE(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
    : m_dest(network),
      m_destNetworkMask(networkMask),
      m_gateway(Ipv4Address::GetZero()),
      m_interface(interface),
      m_nextIface(MAX_UINT32),
      m_distance(MAX_UINT32),
      m_cumulative_loss(0.0),
      m_num_pulls(0)
{
    NS_LOG_FUNCTION(this << network << networkMask << interface);
}

ArmedSpfRIE::ArmedSpfRIE(Ipv4Address dest,
                         Ipv4Address gateway,
                         uint32_t interface,
                         uint32_t nextIface,
                         uint32_t distance)
    : m_dest(dest),
      m_destNetworkMask(Ipv4Mask::GetOnes()),
      m_gateway(gateway),
      m_interface(interface),
      m_nextIface(nextIface),
      m_distance(distance),
      m_cumulative_loss(0.0),
      m_num_pulls(0)
{
    NS_LOG_FUNCTION(this << dest << gateway << interface << distance);
    m_cumulative_loss = 1 - exp(-(double)distance);
}

bool
ArmedSpfRIE::IsHost() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask == Ipv4Mask::GetOnes();
}

Ipv4Address
ArmedSpfRIE::GetDest() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

bool
ArmedSpfRIE::IsNetwork() const
{
    NS_LOG_FUNCTION(this);
    return !IsHost();
}

bool
ArmedSpfRIE::IsDefault() const
{
    NS_LOG_FUNCTION(this);
    return m_dest == Ipv4Address::GetZero();
}

Ipv4Address
ArmedSpfRIE::GetDestNetwork() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

Ipv4Mask
ArmedSpfRIE::GetDestNetworkMask() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask;
}

bool
ArmedSpfRIE::IsGateway() const
{
    NS_LOG_FUNCTION(this);
    return m_gateway != Ipv4Address::GetZero();
}

Ipv4Address
ArmedSpfRIE::GetGateway() const
{
    NS_LOG_FUNCTION(this);
    return m_gateway;
}

uint32_t
ArmedSpfRIE::GetInterface() const
{
    NS_LOG_FUNCTION(this);
    return m_interface;
}

uint32_t
ArmedSpfRIE::GetNextIface() const
{
    NS_LOG_FUNCTION(this);
    return m_nextIface;
}

uint32_t
ArmedSpfRIE::GetDistance() const
{
    NS_LOG_FUNCTION(this);
    return m_distance;
}

double
ArmedSpfRIE::GetCumulativeLoss() const
{
    return m_cumulative_loss;
}

uint32_t
ArmedSpfRIE::GetNumPulls() const
{
    return m_num_pulls;
}

void
ArmedSpfRIE::UpdateArm(double reward)
{
    m_cumulative_loss += reward;
}

void
ArmedSpfRIE::PullArm()
{
    m_num_pulls += 1;
}

ArmedSpfRIE
ArmedSpfRIE::CreateHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(dest << nextHop << interface);
    return ArmedSpfRIE(dest, nextHop, interface);
}

ArmedSpfRIE
ArmedSpfRIE::CreateHostRouteTo(Ipv4Address dest, uint32_t interface)
{
    NS_LOG_FUNCTION(dest << interface);
    return ArmedSpfRIE(dest, interface);
}

ArmedSpfRIE
ArmedSpfRIE::CreateHostRouteTo(Ipv4Address dest,
                               Ipv4Address nextHop,
                               uint32_t interface,
                               uint32_t nextIface,
                               uint32_t distance)
{
    NS_LOG_FUNCTION(dest << nextHop << interface << nextIface << distance);
    return ArmedSpfRIE(dest, nextHop, interface, nextIface, distance);
}

ArmedSpfRIE
ArmedSpfRIE::CreateNetworkRouteTo(Ipv4Address network,
                                  Ipv4Mask networkMask,
                                  Ipv4Address nextHop,
                                  uint32_t interface)
{
    NS_LOG_FUNCTION(network << networkMask << nextHop << interface);
    return ArmedSpfRIE(network, networkMask, nextHop, interface);
}

ArmedSpfRIE
ArmedSpfRIE::CreateNetworkRouteTo(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
{
    NS_LOG_FUNCTION(network << networkMask << interface);
    return ArmedSpfRIE(network, networkMask, interface);
}

ArmedSpfRIE
ArmedSpfRIE::CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(nextHop << interface);
    return ArmedSpfRIE(Ipv4Address::GetZero(), Ipv4Mask::GetZero(), nextHop, interface);
}

std::ostream&
operator<<(std::ostream& os, const ArmedSpfRIE& route)
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
operator==(const ArmedSpfRIE a, const ArmedSpfRIE b)
{
    return (a.GetDest() == b.GetDest() && a.GetDestNetworkMask() == b.GetDestNetworkMask() &&
            a.GetGateway() == b.GetGateway() && a.GetInterface() == b.GetInterface());
}

} // namespace ns3
