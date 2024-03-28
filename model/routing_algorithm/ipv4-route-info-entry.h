#ifndef IPV4_ROUTE_INFO_ENTRY_H
#define IPV4_ROUTE_INFO_ENTRY_H

#include "ns3/ipv4-address.h"
#include "ns3/route-info-entry.h"

#include <list>
#include <ostream>
#include <vector>

namespace ns3
{

/**
 * \ingroup romam
 *
 * A record of an IPv4 route information entry for routing protocols in romam.
 * This is not a reference counted object.
 */
class Ipv4RouteInfoEntry : public RouteInfoEntry
{
  public:
    /**
     * \brief This constructor does nothing
     */
    Ipv4RouteInfoEntry();
    /**
     * \brief Copy Constructor
     * \param route The route to copy
     */
    Ipv4RouteInfoEntry(const Ipv4RouteInfoEntry& route);
    /**
     * \brief Copy Constructor
     * \param route The route to copy
     */
    Ipv4RouteInfoEntry(const Ipv4RouteInfoEntry* route);
    /**
     * \return True if this route is a host route (mask of all ones); false otherwise
     */
    bool IsHost() const;
    /**
     * \return True if this route is not a host route (mask is not all ones); false otherwise
     *
     * This method is implemented as !IsHost ().
     */
    bool IsNetwork() const;
    /**
     * \return True if this route is a default route; false otherwise
     */
    bool IsDefault() const;
    /**
     * \return True if this route is a gateway route; false otherwise
     */
    bool IsGateway() const;
    /**
     * \return address of the gateway stored in this entry
     */
    Ipv4Address GetGateway() const;
    /**
     * \return The IPv4 address of the destination of this route
     */
    Ipv4Address GetDest() const;
    /**
     * \return The IPv4 network number of the destination of this route
     */
    Ipv4Address GetDestNetwork() const;
    /**
     * \return The IPv4 network mask of the destination of this route
     */
    Ipv4Mask GetDestNetworkMask() const;
    /**
     * \return The Ipv4 interface number used for sending outgoing packets
     */
    uint32_t GetInterface() const;
    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static Ipv4RouteInfoEntry CreateHostRouteTo(Ipv4Address dest,
                                                   Ipv4Address nextHop,
                                                   uint32_t interface);
    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param interface Outgoing interface
     */
    static Ipv4RouteInfoEntry CreateHostRouteTo(Ipv4Address dest, uint32_t interface);
    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input parameters.
     * \param network Ipv4Address of the destination network
     * \param networkMask Ipv4Mask of the destination network mask
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static Ipv4RouteInfoEntry CreateNetworkRouteTo(Ipv4Address network,
                                                      Ipv4Mask networkMask,
                                                      Ipv4Address nextHop,
                                                      uint32_t interface);
    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input parameters.
     * \param network Ipv4Address of the destination network
     * \param networkMask Ipv4Mask of the destination network mask
     * \param interface Outgoing interface
     */
    static Ipv4RouteInfoEntry CreateNetworkRouteTo(Ipv4Address network,
                                                      Ipv4Mask networkMask,
                                                      uint32_t interface);
    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input
     * parameters.  This route is distinguished; it will match any
     * destination for which a more specific route does not exist.
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static Ipv4RouteInfoEntry CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface);

  private:
//     /**
//      * \brief Constructor.
//      * \param network network address
//      * \param mask network mask
//      * \param gateway the gateway
//      * \param interface the interface index
//      */
//     Ipv4RoutingTableEntry(Ipv4Address network,
//                           Ipv4Mask mask,
//                           Ipv4Address gateway,
//                           uint32_t interface);
//     /**
//      * \brief Constructor.
//      * \param dest destination address
//      * \param mask network mask
//      * \param interface the interface index
//      */
//     Ipv4RoutingTableEntry(Ipv4Address dest, Ipv4Mask mask, uint32_t interface);
//     /**
//      * \brief Constructor.
//      * \param dest destination address
//      * \param gateway the gateway
//      * \param interface the interface index
//      */
//     Ipv4RoutingTableEntry(Ipv4Address dest, Ipv4Address gateway, uint32_t interface);
//     /**
//      * \brief Constructor.
//      * \param dest destination address
//      * \param interface the interface index
//      */
//     Ipv4RoutingTableEntry(Ipv4Address dest, uint32_t interface);

    Ipv4Address m_dest;         //!< destination address
    Ipv4Mask m_destNetworkMask; //!< destination network mask
    Ipv4Address m_gateway;      //!< gateway
    uint32_t m_interface;       //!< output interface
};

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param route the Ipv4 routing table entry
 * \returns the reference to the output stream
 */
std::ostream& operator<<(std::ostream& os, const Ipv4RouteInfoEntry& route);

/**
 * \brief Equality operator.
 *
 * \param a lhs
 * \param b rhs
 * \returns true if operands are equal, false otherwise
 */
bool operator==(const Ipv4RouteInfoEntry a, const Ipv4RouteInfoEntry b);

} // namespace ns3

#endif /* IPV4_ROUTING_TABLE_ENTRY_H */
