/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ipv4-route-info-entry.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#define MAX_UINT32 0xffffffff

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Ipv4RouteInfoEntry");

/*****************************************************
 *     Network Ipv4RoutingTableEntry
 *****************************************************/

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry ()
{
  NS_LOG_FUNCTION (this);
}

/**
 * \brief change m_distance (MAX_UINT32) to m_distance (route.m_distance)
*/
Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (Ipv4RouteInfoEntry const &route)
  : m_dest (route.m_dest),
    m_destNetworkMask (route.m_destNetworkMask),
    m_gateway (route.m_gateway),
    m_interface (route.m_interface),
    m_nextInterface (route.m_nextInterface),
    m_distance (route.m_distance)

{
  NS_LOG_FUNCTION (this << route);
}

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (Ipv4RouteInfoEntry const *route)
  : m_dest (route->m_dest),
    m_destNetworkMask (route->m_destNetworkMask),
    m_gateway (route->m_gateway),
    m_interface (route->m_interface),
    m_nextInterface (route->m_nextInterface),
    m_distance (route->m_distance)
{
  NS_LOG_FUNCTION (this << route);
}

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (Ipv4Address dest,
                                              Ipv4Address gateway,
                                              uint32_t interface)
  : m_dest (dest),
    m_destNetworkMask (Ipv4Mask::GetOnes ()),
    m_gateway (gateway),
    m_interface (interface),
    m_nextInterface (MAX_UINT32),
    m_distance (MAX_UINT32)
{
}

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (Ipv4Address dest,
                                              uint32_t interface)
  : m_dest (dest),
    m_destNetworkMask (Ipv4Mask::GetOnes ()),
    m_gateway (Ipv4Address::GetZero ()),
    m_interface (interface),
    m_nextInterface (MAX_UINT32),
    m_distance (MAX_UINT32)
{
}

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (Ipv4Address network,
                                              Ipv4Mask networkMask,
                                              Ipv4Address gateway,
                                              uint32_t interface)
  : m_dest (network),
    m_destNetworkMask (networkMask),
    m_gateway (gateway),
    m_interface (interface),
    m_nextInterface (MAX_UINT32),
    m_distance (MAX_UINT32)
{
  NS_LOG_FUNCTION (this << network << networkMask << gateway << interface);
}

Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (Ipv4Address network,
                                              Ipv4Mask networkMask,
                                              uint32_t interface)
  : m_dest (network),
    m_destNetworkMask (networkMask),
    m_gateway (Ipv4Address::GetZero ()),
    m_interface (interface),
    m_nextInterface (MAX_UINT32),
    m_distance (MAX_UINT32)
{
  NS_LOG_FUNCTION (this << network << networkMask << interface);
}

  /**
   * \author Pu Yang
   * \brief Constructor.
   * \param dest destination address
   * \param interface the interface index
   * \param distance the distance between root and dest
   */
Ipv4RouteInfoEntry::Ipv4RouteInfoEntry (Ipv4Address dest,
                                              Ipv4Address gateway,
                                              uint32_t interface,
                                              uint32_t nextInterface,
                                              uint32_t distance)
  : m_dest (dest),
    m_destNetworkMask (Ipv4Mask::GetOnes ()),
    m_gateway (gateway),
    m_interface (interface),
    m_nextInterface (nextInterface),
    m_distance (distance)
{
    // std::cout << "CreateNetworkRouteTo with distance" << distance << std::endl;
    NS_LOG_FUNCTION (this << dest << gateway << interface << distance);
}

bool
Ipv4RouteInfoEntry::IsHost (void) const
{
  NS_LOG_FUNCTION (this);
  if (m_destNetworkMask == Ipv4Mask::GetOnes ())
    {
      return true;
    }
  else
    {
      return false;
    }
}
Ipv4Address
Ipv4RouteInfoEntry::GetDest (void) const
{
  NS_LOG_FUNCTION (this);
  return m_dest;
}
bool
Ipv4RouteInfoEntry::IsNetwork (void) const
{
  NS_LOG_FUNCTION (this);
  return !IsHost ();
}
bool
Ipv4RouteInfoEntry::IsDefault (void) const
{
  NS_LOG_FUNCTION (this);
  if (m_dest == Ipv4Address::GetZero ())
    {
      return true;
    }
  else
    {
      return false;
    }
}
Ipv4Address
Ipv4RouteInfoEntry::GetDestNetwork (void) const
{
  NS_LOG_FUNCTION (this);
  return m_dest;
}
Ipv4Mask
Ipv4RouteInfoEntry::GetDestNetworkMask (void) const
{
  NS_LOG_FUNCTION (this);
  return m_destNetworkMask;
}
bool
Ipv4RouteInfoEntry::IsGateway (void) const
{
  NS_LOG_FUNCTION (this);
  if (m_gateway == Ipv4Address::GetZero ())
    {
      return false;
    }
  else
    {
      return true;
    }
}
Ipv4Address
Ipv4RouteInfoEntry::GetGateway (void) const
{
  NS_LOG_FUNCTION (this);
  return m_gateway;
}

uint32_t
Ipv4RouteInfoEntry::GetInterface (void) const
{
  NS_LOG_FUNCTION (this);
  return m_interface;
}

uint32_t
Ipv4RouteInfoEntry::GetNextInterface (void) const
{
  NS_LOG_FUNCTION (this);
  return m_nextInterface;
}

uint32_t
Ipv4RouteInfoEntry::GetDistance (void) const
{
  NS_LOG_FUNCTION (this);
  return m_distance;
}

Ipv4RouteInfoEntry 
Ipv4RouteInfoEntry::CreateHostRouteTo (Ipv4Address dest, 
                                          Ipv4Address nextHop,
                                          uint32_t interface)
{
  NS_LOG_FUNCTION (dest << nextHop << interface);
  return Ipv4RouteInfoEntry (dest, nextHop, interface);
}
Ipv4RouteInfoEntry 
Ipv4RouteInfoEntry::CreateHostRouteTo (Ipv4Address dest,
                                          uint32_t interface)
{
  NS_LOG_FUNCTION (dest << interface);
  return Ipv4RouteInfoEntry (dest, interface);
}

/**
 * \author Pu Yang
 * \brief Create a host route with the distance value  
*/
Ipv4RouteInfoEntry
Ipv4RouteInfoEntry::CreateHostRouteTo (Ipv4Address dest, 
                                          Ipv4Address nextHop,
                                          uint32_t interface,
                                          uint32_t nextInterface,
                                          uint32_t distance)
{
  NS_LOG_FUNCTION (dest << nextHop << interface << nextInterface << distance);
  // std::cout << "CreateNetworkRouteTo with distance" << std::endl;
  return Ipv4RouteInfoEntry (dest, nextHop, interface, nextInterface, distance);
}

Ipv4RouteInfoEntry 
Ipv4RouteInfoEntry::CreateNetworkRouteTo (Ipv4Address network, 
                                             Ipv4Mask networkMask,
                                             Ipv4Address nextHop,
                                             uint32_t interface)
{
  NS_LOG_FUNCTION (network << networkMask << nextHop << interface);
  return Ipv4RouteInfoEntry (network, networkMask, 
                                nextHop, interface);
}
Ipv4RouteInfoEntry 
Ipv4RouteInfoEntry::CreateNetworkRouteTo (Ipv4Address network, 
                                             Ipv4Mask networkMask,
                                             uint32_t interface)
{
  NS_LOG_FUNCTION (network << networkMask << interface);
  return Ipv4RouteInfoEntry (network, networkMask, 
                                interface);
}
Ipv4RouteInfoEntry 
Ipv4RouteInfoEntry::CreateDefaultRoute (Ipv4Address nextHop, 
                                           uint32_t interface)
{
  NS_LOG_FUNCTION (nextHop << interface);
  return Ipv4RouteInfoEntry (Ipv4Address::GetZero (), Ipv4Mask::GetZero (), nextHop, interface);
}


std::ostream& operator<< (std::ostream& os, Ipv4RouteInfoEntry const& route)
{
  if (route.IsDefault ())
    {
      NS_ASSERT (route.IsGateway ());
      os << "default out=" << route.GetInterface () << ", next hop=" << route.GetGateway ();
    }
  else if (route.IsHost ())
    {
      if (route.IsGateway ())
        {
          os << "host="<< route.GetDest () << 
          ", out=" << route.GetInterface () <<
          ", next hop=" << route.GetGateway ();
        }
      else
        {
          os << "host="<< route.GetDest () << 
          ", out=" << route.GetInterface ();
        }
    }
  else if (route.IsNetwork ()) 
    {
      if (route.IsGateway ())
        {
          os << "network=" << route.GetDestNetwork () <<
          ", mask=" << route.GetDestNetworkMask () <<
          ",out=" << route.GetInterface () <<
          ", next hop=" << route.GetGateway ();
        }
      else
        {
          os << "network=" << route.GetDestNetwork () <<
          ", mask=" << route.GetDestNetworkMask () <<
          ",out=" << route.GetInterface ();
        }
    }
  else
    {
      NS_ASSERT (false);
    }
  return os;
}

bool operator== (const Ipv4RouteInfoEntry a, const Ipv4RouteInfoEntry b)
{
  return (a.GetDest () == b.GetDest () && 
          a.GetDestNetworkMask () == b.GetDestNetworkMask () &&
          a.GetGateway () == b.GetGateway () &&
          a.GetInterface () == b.GetInterface ());
}

} // namespace ns3
