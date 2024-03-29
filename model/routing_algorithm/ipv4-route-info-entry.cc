#include "ipv4-route-info-entry.h"

#include "ns3/assert.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE ("Ipv4RouteInfoEntry");


Ipv4RouteInfoEntry::Ipv4RouteInfoEntry ()
{
    NS_LOG_FUNCTION (this);
}

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (const Ipv4RouteInfoEntry& route)
    : m_dest(route.m_dest),
      m_destNetworkMask (route.m_destNetworkMask),
      m_gateway (route.m_gateway),
      m_cost (route.m_cost),
      m_interface (route.m_interface)

{
    NS_LOG_FUNCTION (this << route);
}

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (const Ipv4RouteInfoEntry* route)
    : m_dest (route->m_dest),
      m_destNetworkMask (route->m_destNetworkMask),
      m_gateway (route->m_gateway),
      m_cost (route->m_cost),
      m_interface (route->m_interface)   
{
    NS_LOG_FUNCTION (this << route);
}

bool
Ipv4RouteInfoEntry::IsHost () const
{
    NS_LOG_FUNCTION (this);
    return m_destNetworkMask == Ipv4Mask::GetOnes ();
}

bool
Ipv4RouteInfoEntry::IsNetwork () const
{
    NS_LOG_FUNCTION (this);
    return !IsHost ();
}

bool
Ipv4RouteInfoEntry::IsDefault () const
{
    NS_LOG_FUNCTION (this);
    return m_dest == Ipv4Address::GetZero ();
}

bool
Ipv4RouteInfoEntry::IsGateway () const
{
    NS_LOG_FUNCTION (this);
    return m_gateway != Ipv4Address::GetZero ();
}

Ipv4Address
Ipv4RouteInfoEntry::GetGateway () const
{
    NS_LOG_FUNCTION (this);
    return m_gateway;
}

Ipv4Address
Ipv4RouteInfoEntry::GetDest () const
{
    NS_LOG_FUNCTION (this);
    return m_dest;
}

Ipv4Address
Ipv4RouteInfoEntry::GetDestNetwork () const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

Ipv4Mask
Ipv4RouteInfoEntry::GetDestNetworkMask () const
{
    NS_LOG_FUNCTION (this);
    return m_destNetworkMask;
}

uint32_t
Ipv4RouteInfoEntry::GetCost () const
{
    NS_LOG_FUNCTION (this);
    return m_cost;
}

uint32_t
Ipv4RouteInfoEntry::GetInterface () const
{
    NS_LOG_FUNCTION (this);
    return m_interface;
}

Ipv4RouteInfoEntry
Ipv4RouteInfoEntry::CreateHostRouteTo(Ipv4Address dest,
                                      Ipv4Address nextHop,
                                      uint32_t interface)
{
    NS_LOG_FUNCTION (dest << nextHop << interface);
    return Ipv4RouteInfoEntry (dest, nextHop, interface);
}
                                      
Ipv4RouteInfoEntry
Ipv4RouteInfoEntry::CreateHostRouteTo(Ipv4Address dest, uint32_t interface)
{
    NS_LOG_FUNCTION (dest << interface);
    return Ipv4RouteInfoEntry (dest, interface);
}

Ipv4RouteInfoEntry
Ipv4RouteInfoEntry::CreateNetworkRouteTo(Ipv4Address network,
                                         Ipv4Mask networkMask,
                                         Ipv4Address nextHop,
                                         uint32_t interface)
{
    NS_LOG_FUNCTION (network << networkMask << nextHop << interface);
    return Ipv4RouteInfoEntry (network, networkMask, nextHop, interface);
}

Ipv4RouteInfoEntry
Ipv4RouteInfoEntry::CreateNetworkRouteTo(Ipv4Address network,
                                         Ipv4Mask networkMask,
                                         uint32_t interface)
{
    NS_LOG_FUNCTION (network << networkMask << interface);
    return Ipv4RouteInfoEntry (network, networkMask, interface);
}

Ipv4RouteInfoEntry
Ipv4RouteInfoEntry::CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION (nextHop << interface);
    return Ipv4RouteInfoEntry (Ipv4Address::GetZero (), Ipv4Mask::GetZero (), nextHop, interface);
}

void
Ipv4RouteInfoEntry::Print (std::ostream &os) const
{
    if (this->IsDefault())
    {
        NS_ASSERT(this->IsGateway());
        os << "default out=" << this->GetInterface() << ", next hop=" << this->GetGateway();
    }
    else if (this->IsHost())
    {
        if (this->IsGateway())
        {
            os << "host=" << this->GetDest() << ", out=" << this->GetInterface()
               << ", next hop=" << this->GetGateway();
        }
        else
        {
            os << "host=" << this->GetDest() << ", out=" << this->GetInterface();
        }
    }
    else if (this->IsNetwork())
    {
        if (this->IsGateway())
        {
            os << "network=" << this->GetDestNetwork() << ", mask=" << this->GetDestNetworkMask()
               << ",out=" << this->GetInterface() << ", next hop=" << this->GetGateway();
        }
        else
        {
            os << "network=" << this->GetDestNetwork() << ", mask=" << this->GetDestNetworkMask()
               << ",out=" << this->GetInterface();
        }
    }
    else
    {
        NS_ASSERT(false);
    }
}

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (Ipv4Address network,
                                        Ipv4Mask mask,
                                        Ipv4Address gateway,
                                        uint32_t interface)
    : m_dest (network),
      m_destNetworkMask (mask),
      m_gateway (gateway),
      m_cost (INFINITY),
      m_interface (interface)
{
    NS_LOG_FUNCTION (this << network << mask << gateway << interface);
}


Ipv4RouteInfoEntry::Ipv4RouteInfoEntry(Ipv4Address dest, Ipv4Mask mask, uint32_t interface)
    : m_dest (dest),
      m_destNetworkMask (mask),
      m_gateway (Ipv4Address::GetZero()),
      m_cost (INFINITY),
      m_interface (interface)
{
    NS_LOG_FUNCTION (this << dest << mask << interface);
}

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (Ipv4Address dest, Ipv4Address gateway, uint32_t interface)
    : m_dest (dest),
      m_destNetworkMask (Ipv4Mask::GetOnes()),
      m_gateway (gateway),
      m_cost (INFINITY),
      m_interface (interface)
{
    NS_LOG_FUNCTION (this << dest << gateway << interface);
}

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry(Ipv4Address dest, uint32_t interface)
    : m_dest (dest),
      m_destNetworkMask (Ipv4Mask::GetOnes()),
      m_gateway (Ipv4Address::GetZero()),
      m_cost (INFINITY),
      m_interface (interface)
{
    NS_LOG_FUNCTION (this << dest << interface);
}

std::ostream&
operator<<(std::ostream& os, const Ipv4RouteInfoEntry& route)
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
operator==(const Ipv4RouteInfoEntry a, const Ipv4RouteInfoEntry b)
{
    return (a.GetDest() == b.GetDest() && a.GetDestNetworkMask() == b.GetDestNetworkMask() &&
            a.GetGateway() == b.GetGateway() && a.GetInterface() == b.GetInterface());
}


} // namespace ns3