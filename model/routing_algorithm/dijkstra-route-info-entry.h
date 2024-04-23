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
class DijkstraRTE : public RouteInfoEntry
{
  public:
    /**
     * \brief This constructor does nothing
     */
    DijkstraRTE();

    /**
     * \brief destructor.
     */
    ~DijkstraRTE() override;

    /**
     * \brief Copy Constructor
     * \param route The route to copy
     */
    DijkstraRTE(const DijkstraRTE& route);

    /**
     * \brief Copy Constructor
     * \param route The route to copy
     */
    DijkstraRTE(const DijkstraRTE* route);

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
     * \return An DijkstraRTE object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static DijkstraRTE CreateHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface);

    /**
     * \return An DijkstraRTE object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param interface Outgoing interface
     */
    static DijkstraRTE CreateHostRouteTo(Ipv4Address dest, uint32_t interface);

    /**
     * \return An DijkstraRTE object corresponding to the input parameters.
     * \param network Ipv4Address of the destination network
     * \param networkMask Ipv4Mask of the destination network mask
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static DijkstraRTE CreateNetworkRouteTo(Ipv4Address network,
                                            Ipv4Mask networkMask,
                                            Ipv4Address nextHop,
                                            uint32_t interface);

    /**
     * \return An DijkstraRTE object corresponding to the input parameters.
     * \param network Ipv4Address of the destination network
     * \param networkMask Ipv4Mask of the destination network mask
     * \param interface Outgoing interface
     */
    static DijkstraRTE CreateNetworkRouteTo(Ipv4Address network,
                                            Ipv4Mask networkMask,
                                            uint32_t interface);

    /**
     * \return An DijkstraRTE object corresponding to the input
     * parameters.  This route is distinguished; it will match any
     * destination for which a more specific route does not exist.
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static DijkstraRTE CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface);

  private:
    /**
     * \brief Constructor.
     * \param network network address
     * \param mask network mask
     * \param gateway the gateway
     * \param interface the interface index
     */
    DijkstraRTE(Ipv4Address network, Ipv4Mask mask, Ipv4Address gateway, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param mask network mask
     * \param interface the interface index
     */
    DijkstraRTE(Ipv4Address dest, Ipv4Mask mask, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param gateway the gateway
     * \param interface the interface index
     */
    DijkstraRTE(Ipv4Address dest, Ipv4Address gateway, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param interface the interface index
     */
    DijkstraRTE(Ipv4Address dest, uint32_t interface);

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
std::ostream& operator<<(std::ostream& os, const DijkstraRTE& route);

/**
 * \brief Equality operator.
 *
 * \param a lhs
 * \param b rhs
 * \returns true if operands are equal, false otherwise
 */
bool operator==(const DijkstraRTE a, const DijkstraRTE b);

} // namespace ns3

#endif /* Dijkstra_ROUTING_TABLE_ENTRY_H */
