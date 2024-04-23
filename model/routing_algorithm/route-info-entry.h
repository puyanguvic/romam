/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef ROUTE_INFO_ENTRY_H
#define ROUTE_INFO_ENTRY_H

#include "ns3/ipv4-address.h"

#include <list>
#include <ostream>
#include <vector>

namespace ns3
{
/**
 * @brief This class is the entry of RoutInfoBase (RIB)
 *
 */
class RouteInfoEntry
{
  public:
    /**
     * \brief virtual destructor.
     */
    virtual ~RouteInfoEntry();

    /**
     * \return True if this route is a host route (mask of all ones); false otherwise
     */
    virtual bool IsHost() const = 0;

    /**
     * \return True if this route is not a host route (mask is not all ones); false otherwise
     *
     * This method is implemented as !IsHost ().
     */
    virtual bool IsNetwork() const = 0;

    /**
     * \return True if this route is a default route; false otherwise
     */
    virtual bool IsDefault() const = 0;

    /**
     * \return True if this route is a gateway route; false otherwise
     */
    virtual bool IsGateway() const = 0;

    /**
     * \return address of the gateway stored in this entry
     */
    virtual Ipv4Address GetGateway() const = 0;

    /**
     * \return The IPv4 address of the destination of this route
     */
    virtual Ipv4Address GetDest() const = 0;

    /**
     * \return The IPv4 network number of the destination of this route
     */
    virtual Ipv4Address GetDestNetwork() const = 0;

    /**
     * \return The IPv4 network mask of the destination of this route
     */
    virtual Ipv4Mask GetDestNetworkMask() const = 0;
    /**
     * \return The Ipv4 interface number used for sending outgoing packets
     */
    virtual uint32_t GetInterface() const = 0;
};
} // namespace ns3

#endif /* ROUTE_INFO_ENTRY_H */
