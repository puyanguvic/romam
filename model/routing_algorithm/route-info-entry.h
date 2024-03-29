#ifndef ROUTE_INFO_ENTRY_H
#define ROUTE_INFO_ENTRY_H

#include "ns3/ipv4-address.h"
#include "ns3/object.h"

#include <list>
#include <ostream>
#include <vector>

namespace ns3
{

/**
 * \ingroup Romam
 *
 * A record of an route information entry for Romam routing protocols
 */
class RouteInfoEntry
{
  public:
    virtual void Print (std::ostream &os) const = 0; 
    
    /**
     * \return An RouteInfoEntry object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    // virtual RouteInfoEntry CreateHostRouteTo(Ipv4Address dest,
    //                                         Ipv4Address nextHop,
    //                                         uint32_t interface) = 0;

    /**
     * \return An RouteInfoEntry object corresponding to the input
     * parameters.  This route is distinguished; it will match any
     * destination for which a more specific route does not exist.
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    // virtual RouteInfoEntry CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface) = 0;
};

} // namespace ns3

#endif /* IPV4_ROUTING_TABLE_ENTRY_H */
