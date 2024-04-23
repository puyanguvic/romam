/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef Dijkstra_ROUTING_TABLE_ENTRY_H
#define Dijkstra_ROUTING_TABLE_ENTRY_H

#include "route-info-entry.h"

#include "ns3/ipv4-address.h"

#include <list>
#include <ostream>
#include <vector>

namespace ns3
{
/**
 * \ingroup ipv4Routing
 *
 * A record of an IPv4 routing table entry for Ipv4GlobalRouting and
 * Ipv4StaticRouting.  This is not a reference counted object.
 */
class DijkstraRIE : public RouteInfoEntry
{
  public:
    /**
     * \brief This constructor does nothing
     */
    DijkstraRIE();

    /**
     * \brief destructor.
     */
    ~DijkstraRIE() override;

    /**
     * \brief Copy Constructor
     * \param route The route to copy
     */
    DijkstraRIE(const DijkstraRIE& route);

    /**
     * \brief Copy Constructor
     * \param route The route to copy
     */
    DijkstraRIE(const DijkstraRIE* route);

    // These methods inherited from base class
    bool IsHost() const override;
    bool IsNetwork() const override;
    bool IsDefault() const override;
    bool IsGateway() const override;
    Ipv4Address GetGateway() const override;
    Ipv4Address GetDest() const override;
    Ipv4Address GetDestNetwork() const override;
    Ipv4Mask GetDestNetworkMask() const override;
    uint32_t GetInterface() const override;

    /**
     * \return An DijkstraRIE object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static DijkstraRIE CreateHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface);

    /**
     * \return An DijkstraRIE object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param interface Outgoing interface
     */
    static DijkstraRIE CreateHostRouteTo(Ipv4Address dest, uint32_t interface);

    /**
     * \return An DijkstraRIE object corresponding to the input parameters.
     * \param network Ipv4Address of the destination network
     * \param networkMask Ipv4Mask of the destination network mask
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static DijkstraRIE CreateNetworkRouteTo(Ipv4Address network,
                                            Ipv4Mask networkMask,
                                            Ipv4Address nextHop,
                                            uint32_t interface);

    /**
     * \return An DijkstraRIE object corresponding to the input parameters.
     * \param network Ipv4Address of the destination network
     * \param networkMask Ipv4Mask of the destination network mask
     * \param interface Outgoing interface
     */
    static DijkstraRIE CreateNetworkRouteTo(Ipv4Address network,
                                            Ipv4Mask networkMask,
                                            uint32_t interface);

    /**
     * \return An DijkstraRIE object corresponding to the input
     * parameters.  This route is distinguished; it will match any
     * destination for which a more specific route does not exist.
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static DijkstraRIE CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface);

  private:
    /**
     * \brief Constructor.
     * \param network network address
     * \param mask network mask
     * \param gateway the gateway
     * \param interface the interface index
     */
    DijkstraRIE(Ipv4Address network, Ipv4Mask mask, Ipv4Address gateway, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param mask network mask
     * \param interface the interface index
     */
    DijkstraRIE(Ipv4Address dest, Ipv4Mask mask, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param gateway the gateway
     * \param interface the interface index
     */
    DijkstraRIE(Ipv4Address dest, Ipv4Address gateway, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param interface the interface index
     */
    DijkstraRIE(Ipv4Address dest, uint32_t interface);

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
std::ostream& operator<<(std::ostream& os, const DijkstraRIE& route);

/**
 * \brief Equality operator.
 *
 * \param a lhs
 * \param b rhs
 * \returns true if operands are equal, false otherwise
 */
bool operator==(const DijkstraRIE a, const DijkstraRIE b);

} // namespace ns3

#endif /* Dijkstra_ROUTING_TABLE_ENTRY_H */
