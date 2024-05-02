/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef ROMAM_ROUTING_H
#define ROMAM_ROUTING_H

#include "datapath/dgr-headers.h"
#include "datapath/tsdb.h"

#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"

#include <list>
#include <stdint.h>

namespace ns3
{

class Packet;
class NetDevice;
class Ipv4Interface;
class Ipv4Address;
class Ipv4Header;

class RouteInfoEntry;
class Node;

class RomamRouting : public Ipv4RoutingProtocol
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);
    ~RomamRouting() override;

    /**
     * \brief Add a host route to the global routing table.
     *
     * \param dest The Ipv4Address destination for this route.
     * \param nextHop The Ipv4Address of the next hop in the route.
     * \param interface The network interface index used to send packets to the
     * destination.
     *
     * \see Ipv4Address
     */
    virtual void AddHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface) = 0;

    /**
     * \brief Add a host route to the global routing table.
     *
     * \param dest The Ipv4Address destination for this route.
     * \param interface The network interface index used to send packets to the
     * destination.
     *
     * \see Ipv4Address
     */
    virtual void AddHostRouteTo(Ipv4Address dest, uint32_t interface) = 0;

    /**
     * \brief Add a host route to the global routing table with the distance
     * between root and destination
     * \param dest The Ipv4Address destination for this route.
     * \param nextHop The next hop Ipv4Address
     * \param interface The network interface index used to send packets to the
     *  destination
     * \param distance The distance between root and destination
     */
    virtual void AddHostRouteTo(Ipv4Address dest,
                                Ipv4Address nextHop,
                                uint32_t interface,
                                uint32_t nextIface,
                                uint32_t distance) = 0;

    /**
     * \brief Add a network route to the global routing table.
     *
     * \param network The Ipv4Address network for this route.
     * \param networkMask The Ipv4Mask to extract the network.
     * \param nextHop The next hop in the route to the destination network.
     * \param interface The network interface index used to send packets to the
     * destination.
     *
     * \see Ipv4Address
     */
    virtual void AddNetworkRouteTo(Ipv4Address network,
                                   Ipv4Mask networkMask,
                                   Ipv4Address nextHop,
                                   uint32_t interface) = 0;

    /**
     * \brief Add a network route to the global routing table.
     *
     * \param network The Ipv4Address network for this route.
     * \param networkMask The Ipv4Mask to extract the network.
     * \param interface The network interface index used to send packets to the
     * destination.
     *
     * \see Ipv4Address
     */
    virtual void AddNetworkRouteTo(Ipv4Address network,
                                   Ipv4Mask networkMask,
                                   uint32_t interface) = 0;

    /**
     * \brief Add an external route to the routing table.
     *
     * \param network The Ipv4Address network for this route.
     * \param networkMask The Ipv4Mask to extract the network.
     * \param nextHop The next hop Ipv4Address
     * \param interface The network interface index used to send packets to the
     * destination.
     */
    virtual void AddASExternalRouteTo(Ipv4Address network,
                                      Ipv4Mask networkMask,
                                      Ipv4Address nextHop,
                                      uint32_t interface) = 0;

    /**
     * \brief Get the number of individual unicast routes that have been added
     * to the routing table.
     *
     * \warning The default route counts as one of the routes.
     * \returns the number of routes
     */
    virtual uint32_t GetNRoutes(void) const = 0;

    /**
     * \brief Remove a route from the global unicast routing table.
     *
     * Externally, the unicast global routing table appears simply as a table with
     * n entries.  The one subtlety of note is that if a default route has been set
     * it will appear as the zeroth entry in the table.  This means that if the
     * default route has been set, calling RemoveRoute (0) will remove the
     * default route.
     *
     * \param i The index (into the routing table) of the route to remove.  If
     * the default route has been set, it will occupy index zero.
     *
     * \see Ipv4RoutingTableEntry
     * \see Ipv4GlobalRouting::GetRoute
     * \see Ipv4GlobalRouting::AddRoute
     */
    virtual void RemoveRoute(uint32_t i) = 0;

    // protected:
    //   /**
    //    * \brief Dispose this object
    //    */
    //   virtual void DoDispose() override;

    //   virtual void DoInitialize() override;
};

} // namespace ns3

#endif /* ROMAM_ROUTING_H */
