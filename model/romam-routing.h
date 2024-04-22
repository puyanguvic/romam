/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef ROMAM_ROUTING_H
#define ROMAM_ROUTING_H

#include "ns3/ipv4-header.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4.h"

#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"

#include "routing_algorithm/ipv4-route-info-entry.h"
#include "datapath/tsdb.h"
#include "datapath/dgr-headers.h"

#include <list>
#include <stdint.h>

namespace ns3
{

class Packet;
class NetDevice;
class Ipv4Interface;
class Ipv4Address;
class Ipv4Header;

class Ipv4RouteInfoEntry;
// class Ipv4MulticastRoutingTableEntry;
class Node;

class RomamRouting : public Ipv4RoutingProtocol
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief Construct an empty Romam routing protocol,
   *
   * The RomamRouting class supports host and network unicast routes.
   * This method initializes the lists containing these routes to empty.
   *
   * \see Ipv4GlobalRouting
   */
  RomamRouting ();
  ~RomamRouting () override;


  // These methods inherited from base class
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
    void AddHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface);
    /**
     * \brief Add a host route to the global routing table.
     *
     * \param dest The Ipv4Address destination for this route.
     * \param interface The network interface index used to send packets to the
     * destination.
     *
     * \see Ipv4Address
     */
    void AddHostRouteTo(Ipv4Address dest, uint32_t interface);

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
    void AddNetworkRouteTo(Ipv4Address network,
                           Ipv4Mask networkMask,
                           Ipv4Address nextHop,
                           uint32_t interface);

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
    void AddNetworkRouteTo(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface);

    /**
     * \brief Add an external route to the global routing table.
     *
     * \param network The Ipv4Address network for this route.
     * \param networkMask The Ipv4Mask to extract the network.
     * \param nextHop The next hop Ipv4Address
     * \param interface The network interface index used to send packets to the
     * destination.
     */
    void AddASExternalRouteTo(Ipv4Address network,
                              Ipv4Mask networkMask,
                              Ipv4Address nextHop,
                              uint32_t interface);

  /**
   * \brief Get the number of individual unicast routes that have been added
   * to the routing table.
   *
   * \warning The default route counts as one of the routes.
   * \returns the number of routes
   */
  uint32_t GetNRoutes (void) const;

  Ipv4RouteInfoEntry *GetRoute (uint32_t i) const;

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
  void RemoveRoute (uint32_t i);

  /**
   * Assign a fixed random variable stream number to the random variables
   * used by this model.  Return the number of streams (possibly zero) that
   * have been assigned.
   *
   * \param stream first stream index to use
   * \return the number of stream indices assigned by this model
   */
  int64_t AssignStreams (int64_t stream);

  // static bool CompareRouteCost(Ipv4DGRRoutingTableEntry* route1, Ipv4DGRRoutingTableEntry* route2);
    /**
   * \brief Lookup in the forwarding table for destination.
   * \param dest destination address
   * \param oif output interface if any (put 0 otherwise)
   * \return Ipv4Route to route the packet to reach dest address
   */
  Ptr<Ipv4Route> LookupRoute (Ipv4Address dest, Ptr<NetDevice> oif = 0);

  /**
   * Start protocol operation
  */
  void DoInitialize (void) override;
protected:
  /**
   * \brief Dispose this object
  */
  void DoDispose (void) override;


private:
  /// Set to true if packets are randomly routed among ECMP; set to false for using only one route consistently
  bool m_randomEcmpRouting;
  /// Set to true if this interface should respond to interface events by globallly recomputing routes 
  bool m_respondToInterfaceEvents;
  /// A uniform random number generator for randomly routing packets among ECMP 
  Ptr<UniformRandomVariable> m_rand;

  /// container of Ipv4RoutingTableEntry (routes to hosts)
  typedef std::list<Ipv4RouteInfoEntry *> HostRoutes;
  /// const iterator of container of Ipv4RoutingTableEntry (routes to hosts)
  typedef std::list<Ipv4RouteInfoEntry *>::const_iterator HostRoutesCI;
  /// iterator of container of Ipv4RoutingTableEntry (routes to hosts)
  typedef std::list<Ipv4RouteInfoEntry *>::iterator HostRoutesI;

  /// container of Ipv4RoutingTableEntry (routes to networks)
  typedef std::list<Ipv4RouteInfoEntry *> NetworkRoutes;
  /// const iterator of container of Ipv4RoutingTableEntry (routes to networks)
  typedef std::list<Ipv4RouteInfoEntry *>::const_iterator NetworkRoutesCI;
  /// iterator of container of Ipv4RoutingTableEntry (routes to networks)
  typedef std::list<Ipv4RouteInfoEntry *>::iterator NetworkRoutesI;

  /// container of Ipv4RoutingTableEntry (routes to external AS)
  typedef std::list<Ipv4RouteInfoEntry *> ASExternalRoutes;
  /// const iterator of container of Ipv4RoutingTableEntry (routes to external AS)
  typedef std::list<Ipv4RouteInfoEntry *>::const_iterator ASExternalRoutesCI;
  /// iterator of container of Ipv4RoutingTableEntry (routes to external AS)
  typedef std::list<Ipv4RouteInfoEntry *>::iterator ASExternalRoutesI;

  HostRoutes m_hostRoutes;             //!< Routes to hosts
  NetworkRoutes m_networkRoutes;       //!< Routes to networks
  ASExternalRoutes m_ASexternalRoutes; //!< External routes imported
  Ptr<Ipv4> m_ipv4; //!< associated IPv4 instance
  
  // RouteSelectMode_t m_routeSelectMode; //!< route select mode
  TSDB m_tsdb; //!< the Traffic State DataBase (TSDB) of the DGR Rout

  // use a socket list neighbors
  /// One socket per interface, each bound to that interface's address
  /// (reason: for Neighbor status sensing, we need to know on which interface
  /// the messages arrive)
  typedef std::map<Ptr<Socket>, uint32_t> SocketList;
  /// socket list type iterator
  typedef std::map<Ptr<Socket>, uint32_t>::iterator SocketListI;
  /// socket list type const iterator
  typedef std::map<Ptr<Socket>, uint32_t>::const_iterator SocketListCI;

  SocketList  m_unicastSocketList; //!< list of sockets for unicast messages (socket, interface index)
  Ptr<Socket> m_multicastRecvSocket; //!< multicast receive socket

  EventId m_nextUnsolicitedUpdate; //!< Next Unsolicited Update event
  EventId m_nextTriggeredUpdate; //!< Next Triggered Update event

  Time m_unsolicitedUpdate;             //!< Time between two Unsolicited Neighbor State Updates.

  std::set<uint32_t> m_interfaceExclusions; //!<Set of excluded interfaces
  
  /**
   * Receive an DGR message
   * 
   * \param socket the socket the packet was received from.
  */
  void Receive (Ptr<Socket> socket);

  /**
   * \brief Sending Neighbor Status Updates on all interfaces.
   * \param periodic true for periodic update, else triggered.
  */
  void DoSendNeighborStatusUpdate (bool periodic);

  // /**
  //  * \brief Send Neighbor Status Request on all interfaces
  // */
  // void SendNeighborStatusRequest ();

  /**
   * \brief Send Triggered Routing Updates on all interfaces.
  */
  void SendTriggeredNeighborStatusUpdate ();

  /**
   * \brief Send Unsolicited neighbor status information Updates on all interfaces.
  */
  void SendUnsolicitedUpdate ();

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
  void HandleResponses (DgrHeader hdr,
                       Ipv4Address senderAddress,
                       uint32_t incomingInterface,
                       uint8_t hopLimit);

  bool m_initialized; //!< flag to allow socket's late-creation.
};


} // namespace ns3

#endif /* ROMAM_ROUTING_H */
