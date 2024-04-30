/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_ROUTING_H
#define OSPF_ROUTING_H

#include "datapath/tsdb.h"
#include "romam-routing.h"

#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
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
class DijkstraRIE;
class Node;

class OSPFRouting : public RomamRouting
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);
    /**
     * \brief Construct an empty Romam routing protocol,
     *
     * The RomamRouting class supports host and network unicast routes.
     * This method initializes the lists containing these routes to empty.
     *
     * \see Ipv4GlobalRouting
     */
    OSPFRouting();
    ~OSPFRouting() override;

    // These methods inherited from Ipv4RoutingProtocol class
    Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p,
                               const Ipv4Header& header,
                               Ptr<NetDevice> oif,
                               Socket::SocketErrno& sockerr) override;
    bool RouteInput(Ptr<const Packet> p,
                    const Ipv4Header& header,
                    Ptr<const NetDevice> idev,
                    const UnicastForwardCallback& ucb,
                    const MulticastForwardCallback& mcb,
                    const LocalDeliverCallback& lcb,
                    const ErrorCallback& ecb) override;
    void NotifyInterfaceUp(uint32_t interface) override;
    void NotifyInterfaceDown(uint32_t interface) override;
    void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
    void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
    void SetIpv4(Ptr<Ipv4> ipv4) override;
    void PrintRoutingTable(Ptr<OutputStreamWrapper> stream,
                           Time::Unit unit = Time::S) const override;

    // These methods inherited from Objective class
    void DoInitialize(void) override;

    // These methods inherited from RomamRouting class
    void AddHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface) override;
    void AddHostRouteTo(Ipv4Address dest, uint32_t interface) override;
    void AddHostRouteTo(Ipv4Address dest,
                        Ipv4Address nextHop,
                        uint32_t interface,
                        uint32_t nextIface,
                        uint32_t distance) override;
    void AddNetworkRouteTo(Ipv4Address network,
                           Ipv4Mask networkMask,
                           Ipv4Address nextHop,
                           uint32_t interface) override;
    void AddNetworkRouteTo(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface) override;
    void AddASExternalRouteTo(Ipv4Address network,
                              Ipv4Mask networkMask,
                              Ipv4Address nextHop,
                              uint32_t interface) override;
    uint32_t GetNRoutes(void) const override;
    void RemoveRoute(uint32_t i) override;

    /**
     * Assign a fixed random variable stream number to the random variables
     * used by this model.  Return the number of streams (possibly zero) that
     * have been assigned.
     *
     * \param stream first stream index to use
     * \return the number of stream indices assigned by this model
     */
    int64_t AssignStreams(int64_t stream);

    DijkstraRIE* GetRoute(uint32_t i) const;

  protected:
    // These methods inherited from Objective class
    void DoDispose(void) override;

  private:
    /// Set to true if packets are randomly routed among ECMP; set to false for using only one route
    /// consistently
    bool m_randomEcmpRouting;
    /// Set to true if this interface should respond to interface events by globallly recomputing
    /// routes
    bool m_respondToInterfaceEvents;
    /// A uniform random number generator for randomly routing packets among ECMP
    Ptr<UniformRandomVariable> m_rand;

    /// container of Ipv4RoutingTableEntry (routes to hosts)
    typedef std::list<DijkstraRIE*> HostRoutes;
    /// const iterator of container of Ipv4RoutingTableEntry (routes to hosts)
    typedef std::list<DijkstraRIE*>::const_iterator HostRoutesCI;
    /// iterator of container of Ipv4RoutingTableEntry (routes to hosts)
    typedef std::list<DijkstraRIE*>::iterator HostRoutesI;

    /// container of Ipv4RoutingTableEntry (routes to networks)
    typedef std::list<DijkstraRIE*> NetworkRoutes;
    /// const iterator of container of Ipv4RoutingTableEntry (routes to networks)
    typedef std::list<DijkstraRIE*>::const_iterator NetworkRoutesCI;
    /// iterator of container of Ipv4RoutingTableEntry (routes to networks)
    typedef std::list<DijkstraRIE*>::iterator NetworkRoutesI;

    /// container of Ipv4RoutingTableEntry (routes to external AS)
    typedef std::list<DijkstraRIE*> ASExternalRoutes;
    /// const iterator of container of Ipv4RoutingTableEntry (routes to external AS)
    typedef std::list<DijkstraRIE*>::const_iterator ASExternalRoutesCI;
    /// iterator of container of Ipv4RoutingTableEntry (routes to external AS)
    typedef std::list<DijkstraRIE*>::iterator ASExternalRoutesI;

    /**
     * \brief Lookup in the route infomation base (RIB) for destination.
     * \param dest destination address
     * \param oif output interface if any (put 0 otherwise)
     * \return Ipv4Route to route the packet to reach dest address
     */
    Ptr<Ipv4Route> LookupRoute(Ipv4Address dest, Ptr<NetDevice> oif = 0) const;

    HostRoutes m_hostRoutes;             //!< Routes to hosts
    NetworkRoutes m_networkRoutes;       //!< Routes to networks
    ASExternalRoutes m_ASexternalRoutes; //!< External routes imported
    Ptr<Ipv4> m_ipv4;                    //!< associated IPv4 instance
};

} // namespace ns3

#endif /* OSPF_ROUTING_H */
