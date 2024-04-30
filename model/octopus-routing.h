// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-

#ifndef OCTOPUS_ROUTING_H
#define OCTOPUS_ROUTING_H

#include "romam-routing.h"

#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"

#include <list>
#include <map>
#include <stdint.h>

namespace ns3
{

class Packet;
class NetDevice;
class Ipv4Interface;
class Ipv4Address;
class Ipv4Header;
class Node;
class ShortestPathForestRIE;
class ArmValueDB;

typedef enum
{
    NONE,
    KSHORT,
    DGR,
    DDR
} RouteSelectMode_t;

class OctopusRouting : public RomamRouting
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);
    /**
     * \brief Construct an empty Ipv4GlobalRouting routing protocol,
     *
     * The Ipv4GlobalRouting class supports host and network unicast routes.
     * This method initializes the lists containing these routes to empty.
     *
     * \see Ipv4GlobalRouting
     */
    OctopusRouting();
    ~OctopusRouting() override;

    // These methods inherited from Ipv4Routing Protocol class
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
    void DoInitialize() override;

    // These methods inherited from RomamRouting class
    void AddHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface) override;
    void AddHostRouteTo(Ipv4Address dest, uint32_t interface) override;
    void AddHostRouteTo(Ipv4Address dest,
                        Ipv4Address nextHop,
                        uint32_t interface,
                        uint32_t nextInterface,
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

    ShortestPathForestRIE* GetRoute(uint32_t i) const;

  protected:
    /**
     * \brief Dispose this object
     */
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
    typedef std::list<ShortestPathForestRIE*> HostRoutes;
    /// const iterator of container of Ipv4RoutingTableEntry (routes to hosts)
    typedef std::list<ShortestPathForestRIE*>::const_iterator HostRoutesCI;
    /// iterator of container of Ipv4RoutingTableEntry (routes to hosts)
    typedef std::list<ShortestPathForestRIE*>::iterator HostRoutesI;

    /// container of Ipv4RoutingTableEntry (routes to networks)
    typedef std::list<ShortestPathForestRIE*> NetworkRoutes;
    /// const iterator of container of Ipv4RoutingTableEntry (routes to networks)
    typedef std::list<ShortestPathForestRIE*>::const_iterator NetworkRoutesCI;
    /// iterator of container of Ipv4RoutingTableEntry (routes to networks)
    typedef std::list<ShortestPathForestRIE*>::iterator NetworkRoutesI;

    /// container of RoutingTableEntry (routes to external AS)
    typedef std::list<ShortestPathForestRIE*> ASExternalRoutes;
    /// const iterator of container of Ipv4RoutingTableEntry (routes to external AS)
    typedef std::list<ShortestPathForestRIE*>::const_iterator ASExternalRoutesCI;
    /// iterator of container of Ipv4RoutingTableEntry (routes to external AS)
    typedef std::list<ShortestPathForestRIE*>::iterator ASExternalRoutesI;

    /**
     * \brief Lookup in the forwarding table for destination.
     * \param dest destination address
     * \param oif output interface if any (put 0 otherwise)
     * \return Ipv4Route to route the packet to reach dest address
     */
    Ptr<Ipv4Route> LookupECMPRoute(Ipv4Address dest, Ptr<NetDevice> oif = 0);
    Ptr<Ipv4Route> LookupKShortRoute(Ipv4Address dest,
                                     Ptr<Packet> p,
                                     Ptr<const NetDevice> idev = 0);
    Ptr<Ipv4Route> LookupDGRRoute(Ipv4Address dest, Ptr<Packet> p, Ptr<const NetDevice> idev = 0);
    Ptr<Ipv4Route> LookupDDRRoute(Ipv4Address dest, Ptr<Packet> p, Ptr<const NetDevice> idev = 0);

    HostRoutes m_hostRoutes;             //!< Routes to hosts
    NetworkRoutes m_networkRoutes;       //!< Routes to networks
    ASExternalRoutes m_ASexternalRoutes; //!< External routes imported
    Ptr<Ipv4> m_ipv4;                    //!< associated IPv4 instance

    RouteSelectMode_t m_routeSelectMode; //!< route select mode
    ArmValueDB m_armDB;                  //!< the Neighbor State DataBase (NSDB) of the DGR Rout

    // use a socket list neighbors
    /// One socket per interface, each bound to that interface's address
    /// (reason: for Neighbor status sensing, we need to know on which interface
    /// the messages arrive)
    typedef std::map<Ptr<Socket>, uint32_t> SocketList;
    /// socket list type iterator
    typedef std::map<Ptr<Socket>, uint32_t>::iterator SocketListI;
    /// socket list type const iterator
    typedef std::map<Ptr<Socket>, uint32_t>::const_iterator SocketListCI;

    SocketList
        m_unicastSocketList; //!< list of sockets for unicast messages (socket, interface index)
    Ptr<Socket> m_multicastRecvSocket; //!< multicast receive socket

    EventId m_nextUnsolicitedUpdate; //!< Next Unsolicited Update event
    EventId m_nextTriggeredUpdate;   //!< Next Triggered Update event

    Time m_unsolicitedUpdate; //!< Time between two Unsolicited Neighbor State Updates.

    // Time m_startupDelay;            //!< Random delay before protocol startup
    // Time m_minTriggeredUpdateDelay; //!< Min cooldown delay after a Triggered Update.
    // Time m_maxTriggeredUpdateDelay; //!< Max cooldown delay after a Triggered Update.
    // Time m_unsolicitedUpdate;       //!< time between two Unsolicited Routing Updates.
    // Time m_timeoutDelay;            //!< Delay before invalidating a status

    std::set<uint32_t> m_interfaceExclusions; //!< Set of excluded interfaces

    /**
     * Receive an DGR message
     *
     * \param socket the socket the packet was received from.
     */
    void Receive(Ptr<Socket> socket);

    /**
     * \brief Sending Neighbor Status Updates on all interfaces.
     * \param periodic true for periodic update, else triggered.
     */
    void DoSendNeighborStatusUpdate(bool periodic);

    // /**
    //  * \brief Send Neighbor Status Request on all interfaces
    // */
    // void SendNeighborStatusRequest ();

    /**
     * \brief Send Triggered Routing Updates on all interfaces.
     */
    void SendTriggeredNeighborStatusUpdate();

    /**
     * \brief Send Unsolicited neighbor status information Updates on all interfaces.
     */
    void SendUnsolicitedUpdate();

    // /**
    //  * \brief Handle DGR requests.
    //  *
    //  * \param hdr message header (Including NSEs)
    //  * \param senderAddress sender address
    //  * \param senderPort sender port
    //  * \param incomingInterface incoming interface
    //  * \param hopLimit packet's hop limit
    // */
    // void HandleRequests (DgrHeader hdr,
    //                     Ipv4Address senderAddress,
    //                     uint16_t senderPort,
    //                     uint32_t incomingInterface,
    //                     uint8_t hopLimit);

    /**
     * \brief Handle DGR responses.
     *
     * \param hdr message header (including NSEs)
     * \param senderAddress sender address
     * \param incomingInterface incoming interface
     * \param hopLimit packet's hop limit
     */
    void HandleResponses(DgrHeader hdr,
                         Ipv4Address senderAddress,
                         uint32_t incomingInterface,
                         uint8_t hopLimit);

    // Ptr<OutputStreamWrapper> m_outStream = Create<OutputStreamWrapper>
    // ("queueStatusErr.txt", std::ios::out);

    bool m_initialized; //!< flag to allow socket's late-creation.
};

} // namespace ns3

#endif /* OCTOPUS_ROUTING_H */