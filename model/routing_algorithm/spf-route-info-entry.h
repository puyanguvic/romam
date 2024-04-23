/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef SPF_ROUTING_TABLE_ENTRY_H
#define SPF_ROUTING_TABLE_ENTRY_H

#include "route-info-entry.h"

#include "ns3/ipv4-address.h"

#include <list>
#include <ostream>
#include <vector>

namespace ns3
{

class ShortestPathForestRIE : public RouteInfoEntry
{
  public:
    /**
     * \brief This constructor does nothing
     */
    ShortestPathForestRIE();

    /**
     * \brief destructor.
     */
    ~ShortestPathForestRIE() override;

    /**
     * \brief Copy Constructor
     * \param route The route to copy
     */
    ShortestPathForestRIE(const ShortestPathForestRIE& route);

    /**
     * \brief Copy Constructor
     * \param route The route to copy
     */
    ShortestPathForestRIE(const ShortestPathForestRIE* route);

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
     * \brief Get the output interface at next hop
     *
     * \return index of the interface
     */
    uint32_t GetNextIface() const;

    /**
     * @brief Get the Distance to the destination
     *
     * @return the distance value
     */
    uint32_t GetDistance() const;

    /**
     * \return An ShortestPathForestRIE object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    ShortestPathForestRIE CreateHostRouteTo(Ipv4Address dest,
                                            Ipv4Address nextHop,
                                            uint32_t interface);

    /**
     * \return An ShortestPathForestRIE object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param interface Outgoing interface
     */
    static ShortestPathForestRIE CreateHostRouteTo(Ipv4Address dest, uint32_t interface);

    /**
     * \return An Ipv4RoutingTableEntry object corresponding to the input parameters.
     * \param dest Ipv4Address of the destination
     * \param nextHop the Ipv4Address the nextHop
     * \param interface Outgoing interface
     * \param  nextInterface Outgoing interface in next hop
     * \param distance The distance between root and destination
     */
    static ShortestPathForestRIE CreateHostRouteTo(Ipv4Address dest,
                                                   Ipv4Address nextHop,
                                                   uint32_t interface,
                                                   uint32_t nextIface,
                                                   uint32_t distance);

    /**
     * \return An ShortestPathForestRIE object corresponding to the input parameters.
     * \param network Ipv4Address of the destination network
     * \param networkMask Ipv4Mask of the destination network mask
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static ShortestPathForestRIE CreateNetworkRouteTo(Ipv4Address network,
                                                      Ipv4Mask networkMask,
                                                      Ipv4Address nextHop,
                                                      uint32_t interface);

    /**
     * \return An ShortestPathForestRIE object corresponding to the input parameters.
     * \param network Ipv4Address of the destination network
     * \param networkMask Ipv4Mask of the destination network mask
     * \param interface Outgoing interface
     */
    static ShortestPathForestRIE CreateNetworkRouteTo(Ipv4Address network,
                                                      Ipv4Mask networkMask,
                                                      uint32_t interface);

    /**
     * \return An ShortestPathForestRIE object corresponding to the input
     * parameters.  This route is distinguished; it will match any
     * destination for which a more specific route does not exist.
     * \param nextHop Ipv4Address of the next hop
     * \param interface Outgoing interface
     */
    static ShortestPathForestRIE CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface);

  private:
    /**
     * \brief Constructor.
     * \param network network address
     * \param mask network mask
     * \param gateway the gateway
     * \param interface the interface index
     */
    ShortestPathForestRIE(Ipv4Address network,
                          Ipv4Mask mask,
                          Ipv4Address gateway,
                          uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param mask network mask
     * \param interface the interface index
     */
    ShortestPathForestRIE(Ipv4Address dest, Ipv4Mask mask, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param gateway the gateway
     * \param interface the interface index
     */
    ShortestPathForestRIE(Ipv4Address dest, Ipv4Address gateway, uint32_t interface);
    /**
     * \brief Constructor.
     * \param dest destination address
     * \param interface the interface index
     */
    ShortestPathForestRIE(Ipv4Address dest, uint32_t interface);

    /**
     * \brief Constructor.
     * \param dest destination address
     * \param gateway gateway address
     * \param interface the interface index
     * \param nextInterface the interface index in next hop
     * \param distance the distance between root and destination
     */
    ShortestPathForestRIE(Ipv4Address dest,
                          Ipv4Address gateway,
                          uint32_t interface,
                          uint32_t nextIface,
                          uint32_t distance);

    Ipv4Address m_dest;         //!< destination address
    Ipv4Mask m_destNetworkMask; //!< destination network mask
    Ipv4Address m_gateway;      //!< gateway
    uint32_t m_interface;       //!< output interface
    uint32_t m_nextIface;       //!< output interface in next hop
    uint32_t m_distance;        //!< the distance from current node to the destination
};

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param route the Ipv4 routing table entry
 * \returns the reference to the output stream
 */
std::ostream& operator<<(std::ostream& os, const ShortestPathForestRIE& route);

/**
 * \brief Equality operator.
 *
 * \param a lhs
 * \param b rhs
 * \returns true if operands are equal, false otherwise
 */
bool operator==(const ShortestPathForestRIE a, const ShortestPathForestRIE b);

} // namespace ns3

#endif /* SPF_ROUTING_TABLE_ENTRY_H */
