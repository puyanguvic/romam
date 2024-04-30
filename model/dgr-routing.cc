// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-

#include "dgr-routing.h"

#include "datapath/romam-tags.h"
#include "priority_manage/dgr-queue-disc.h"
#include "routing_algorithm/spf-route-info-entry.h"
#include "utility/route-manager.h"

#include "ns3/boolean.h"
#include "ns3/ipv4-route.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/simulator.h"
#include "ns3/timestamp-tag.h"
#include "ns3/traffic-control-module.h"

#include <iomanip>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DGRRouting");

NS_OBJECT_ENSURE_REGISTERED(DGRRouting);

TypeId
DGRRouting::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DGRRouting")
            .SetParent<RomamRouting>()
            .SetGroupName("Romam")
            .AddAttribute("RandomEcmpRouting",
                          "Set to true if packets are randomly routed among ECMP; set to false for "
                          "using only one route consistently",
                          BooleanValue(false),
                          MakeBooleanAccessor(&DGRRouting::m_randomEcmpRouting),
                          MakeBooleanChecker())
            .AddAttribute("RespondToInterfaceEvents",
                          "Set to true if you want to dynamically recompute the global routes upon "
                          "Interface notification events (up/down, or add/remove address)",
                          BooleanValue(false),
                          MakeBooleanAccessor(&DGRRouting::m_respondToInterfaceEvents),
                          MakeBooleanChecker());
    return tid;
}

DGRRouting::DGRRouting()
    : m_randomEcmpRouting(false),
      m_respondToInterfaceEvents(false)
{
    NS_LOG_FUNCTION(this);
    m_rand = CreateObject<UniformRandomVariable>();
}

DGRRouting::~DGRRouting()
{
    NS_LOG_FUNCTION(this);
}

Ptr<Ipv4Route>
DGRRouting::RouteOutput(Ptr<Packet> p,
                        const Ipv4Header& header,
                        Ptr<NetDevice> oif,
                        Socket::SocketErrno& sockerr)
{
    NS_LOG_FUNCTION(this << p << &header << oif << &sockerr);
    //
    // First, see if this is a multicast packet we have a route for.  If we
    // have a route, then send the packet down each of the specified interfaces.
    //
    if (header.GetDestination().IsMulticast())
    {
        NS_LOG_LOGIC("Multicast destination-- returning false");
        return nullptr; // Let other routing protocols try to handle this
    }
    //
    // See if this is a unicast packet we have a route for.
    //
    NS_LOG_LOGIC("Unicast destination- looking up");
    Ptr<Ipv4Route> rtentry;
    BudgetTag budgetTag;
    if (p != nullptr && p->GetSize() != 0 && p->PeekPacketTag(budgetTag) &&
        budgetTag.GetBudget() != 0)
    {
        rtentry = LookupDGRRoute(header.GetDestination(), p);
    }
    else
    {
        rtentry = LookupShortestRoute(header.GetDestination(), oif);
    }
    if (rtentry)
    {
        sockerr = Socket::ERROR_NOTERROR;
    }
    else
    {
        sockerr = Socket::ERROR_NOROUTETOHOST;
    }
    return rtentry;
}

bool
DGRRouting::RouteInput(Ptr<const Packet> p,
                       const Ipv4Header& header,
                       Ptr<const NetDevice> idev,
                       const UnicastForwardCallback& ucb,
                       const MulticastForwardCallback& mcb,
                       const LocalDeliverCallback& lcb,
                       const ErrorCallback& ecb)
{
    NS_LOG_FUNCTION(this << p << header << header.GetSource() << header.GetDestination() << idev
                         << &lcb << &ecb);
    NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
    Ptr<Packet> p_copy = p->Copy();

    // Check if input device supports IP
    uint32_t iif = m_ipv4->GetInterfaceForDevice(idev);

    if (m_ipv4->IsDestinationAddress(header.GetDestination(), iif))
    {
        if (!lcb.IsNull())
        {
            NS_LOG_LOGIC("Local delivery to " << header.GetDestination());
            lcb(p, header, iif);
            return true;
        }
        else
        {
            // The local delivery callback is null.  This may be a multicast
            // or broadcast packet, so return false so that another
            // multicast routing protocol can handle it.  It should be possible
            // to extend this to explicitly check whether it is a unicast
            // packet, and invoke the error callback if so
            return false;
        }
    }

    // Check if input device supports IP forwarding
    if (!m_ipv4->IsForwarding(iif))
    {
        NS_LOG_LOGIC("Forwarding disabled for this interface");
        ecb(p, header, Socket::ERROR_NOROUTETOHOST);
        return true;
    }

    // Next, try to find a route
    NS_LOG_LOGIC("Unicast destination- looking up global route");
    Ptr<Ipv4Route> rtentry;
    BudgetTag budgetTag;
    if (p->PeekPacketTag(budgetTag) && budgetTag.GetBudget() != 0)
    {
        rtentry = LookupDGRRoute(header.GetDestination(), p_copy, idev);
    }
    else
    {
        rtentry = LookupShortestRoute(header.GetDestination());
    }

    if (rtentry)
    {
        const Ptr<Packet> p_c = p_copy->Copy();
        NS_LOG_LOGIC("Found unicast destination- calling unicast callback");
        ucb(rtentry, p_c, header);
        return true;
    }
    else
    {
        NS_LOG_LOGIC("Did not find unicast destination- returning false");
        return false; // Let other routing protocols try to handle this
                      // route request.
    }
}

void
DGRRouting::NotifyInterfaceUp(uint32_t i)
{
    NS_LOG_FUNCTION(this << i);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        RouteManager::DeleteRoutes();
        RouteManager::BuildLSDB();
        RouteManager::InitializeDijkstraRoutes();
    }
}

void
DGRRouting::NotifyInterfaceDown(uint32_t i)
{
    NS_LOG_FUNCTION(this << i);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        RouteManager::DeleteRoutes();
        RouteManager::BuildLSDB();
        RouteManager::InitializeDijkstraRoutes();
    }
}

void
DGRRouting::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        RouteManager::DeleteRoutes();
        RouteManager::BuildLSDB();
        RouteManager::InitializeDijkstraRoutes();
    }
}

void
DGRRouting::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_LOGIC("update routing table");
        RouteManager::DeleteRoutes();
        RouteManager::BuildLSDB();
        RouteManager::InitializeSPFRoutes();
    }
}

void
DGRRouting::SetIpv4(Ptr<Ipv4> ipv4)
{
    NS_LOG_FUNCTION(this << ipv4);
    NS_ASSERT(!m_ipv4 && ipv4);
    m_ipv4 = ipv4;
}

void
DGRRouting::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
    NS_LOG_FUNCTION(this << stream);
    std::ostream* os = stream->GetStream();
    // Copy the current ostream state
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    *os << std::resetiosflags(std::ios::adjustfield) << std::setiosflags(std::ios::left);

    *os << "Node: " << m_ipv4->GetObject<Node>()->GetId() << ", Time: " << Now().As(unit)
        << ", Local time: " << m_ipv4->GetObject<Node>()->GetLocalTime().As(unit)
        << ", DGRRouting table" << std::endl;

    if (GetNRoutes() > 0)
    {
        *os << "  Destination     Gateway    Flags   Metric  Iface   NextIface" << std::endl;
        for (uint32_t j = 0; j < GetNRoutes(); j++)
        {
            std::ostringstream dest;
            std::ostringstream gw;
            std::ostringstream mask;
            std::ostringstream flags;
            std::ostringstream metric;
            ShortestPathForestRIE route = GetRoute(j);
            dest << route.GetDest();
            *os << std::setw(13) << dest.str();
            gw << route.GetGateway();
            *os << std::setw(13) << gw.str();
            flags << "U";
            if (route.IsHost())
            {
                flags << "H";
            }
            else if (route.IsGateway())
            {
                flags << "G";
            }
            *os << std::setiosflags(std::ios::left) << std::setw(6) << flags.str();
            metric << route.GetDistance();
            if (route.GetDistance() == 0xffffffff)
            {
                *os << std::setw(9) << "-";
            }
            else
            {
                *os << std::setw(9) << metric.str();
            }

            if (Names::FindName(m_ipv4->GetNetDevice(route.GetInterface())) != "")
            {
                *os << Names::FindName(m_ipv4->GetNetDevice(route.GetInterface()));
            }
            else
            {
                *os << std::setw(7) << route.GetInterface();
                if (route.GetNextIface() != 0xffffffff)
                {
                    *os << std::setw(8) << route.GetNextIface();
                }
                else
                {
                    *os << std::setw(8) << "-";
                }
            }
            *os << std::endl;
        }
    }
    *os << std::endl;
    (*os).copyfmt(oldState);
}

void
DGRRouting::AddHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(this << dest << nextHop << interface);
    auto route = new ShortestPathForestRIE();
    *route = ShortestPathForestRIE::CreateHostRouteTo(dest, nextHop, interface);
    m_hostRoutes.push_back(route);
}

void
DGRRouting::AddHostRouteTo(Ipv4Address dest, uint32_t interface)
{
    NS_LOG_FUNCTION(this << dest << interface);
    auto route = new ShortestPathForestRIE();
    *route = ShortestPathForestRIE::CreateHostRouteTo(dest, interface);
    m_hostRoutes.push_back(route);
}

void
DGRRouting::AddHostRouteTo(Ipv4Address dest,
                           Ipv4Address nextHop,
                           uint32_t interface,
                           uint32_t nextIface,
                           uint32_t distance)
{
    NS_LOG_FUNCTION(this << dest << interface << nextIface << distance);
    auto route = new ShortestPathForestRIE();
    *route =
        ShortestPathForestRIE::CreateHostRouteTo(dest, nextHop, interface, nextIface, distance);
    m_hostRoutes.push_back(route);
}

void
DGRRouting::AddNetworkRouteTo(Ipv4Address network,
                              Ipv4Mask networkMask,
                              Ipv4Address nextHop,
                              uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << nextHop << interface);
    auto route = new ShortestPathForestRIE();
    *route = ShortestPathForestRIE::CreateNetworkRouteTo(network, networkMask, nextHop, interface);
    m_networkRoutes.push_back(route);
}

void
DGRRouting::AddNetworkRouteTo(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << interface);
    auto route = new ShortestPathForestRIE();
    *route = ShortestPathForestRIE::CreateNetworkRouteTo(network, networkMask, interface);
    m_networkRoutes.push_back(route);
}

void
DGRRouting::AddASExternalRouteTo(Ipv4Address network,
                                 Ipv4Mask networkMask,
                                 Ipv4Address nextHop,
                                 uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << nextHop << interface);
    auto route = new ShortestPathForestRIE();
    *route = ShortestPathForestRIE::CreateNetworkRouteTo(network, networkMask, nextHop, interface);
    m_ASexternalRoutes.push_back(route);
}

uint32_t
DGRRouting::GetNRoutes() const
{
    NS_LOG_FUNCTION(this);
    uint32_t n = 0;
    n += m_hostRoutes.size();
    n += m_networkRoutes.size();
    n += m_ASexternalRoutes.size();
    return n;
}

ShortestPathForestRIE*
DGRRouting::GetRoute(uint32_t index) const
{
    NS_LOG_FUNCTION(this << index);
    if (index < m_hostRoutes.size())
    {
        uint32_t tmp = 0;
        for (auto i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
        {
            if (tmp == index)
            {
                return *i;
            }
            tmp++;
        }
    }
    index -= m_hostRoutes.size();
    uint32_t tmp = 0;
    if (index < m_networkRoutes.size())
    {
        for (auto j = m_networkRoutes.begin(); j != m_networkRoutes.end(); j++)
        {
            if (tmp == index)
            {
                return *j;
            }
            tmp++;
        }
    }
    index -= m_networkRoutes.size();
    tmp = 0;
    for (auto k = m_ASexternalRoutes.begin(); k != m_ASexternalRoutes.end(); k++)
    {
        if (tmp == index)
        {
            return *k;
        }
        tmp++;
    }
    NS_ASSERT(false);
    // quiet compiler.
    return nullptr;
}

void
DGRRouting::RemoveRoute(uint32_t index)
{
    NS_LOG_FUNCTION(this << index);
    if (index < m_hostRoutes.size())
    {
        uint32_t tmp = 0;
        for (auto i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
        {
            if (tmp == index)
            {
                NS_LOG_LOGIC("Removing route " << index << "; size = " << m_hostRoutes.size());
                delete *i;
                m_hostRoutes.erase(i);
                NS_LOG_LOGIC("Done removing host route "
                             << index << "; host route remaining size = " << m_hostRoutes.size());
                return;
            }
            tmp++;
        }
    }
    index -= m_hostRoutes.size();
    uint32_t tmp = 0;
    for (auto j = m_networkRoutes.begin(); j != m_networkRoutes.end(); j++)
    {
        if (tmp == index)
        {
            NS_LOG_LOGIC("Removing route " << index << "; size = " << m_networkRoutes.size());
            delete *j;
            m_networkRoutes.erase(j);
            NS_LOG_LOGIC("Done removing network route "
                         << index << "; network route remaining size = " << m_networkRoutes.size());
            return;
        }
        tmp++;
    }
    index -= m_networkRoutes.size();
    tmp = 0;
    for (auto k = m_ASexternalRoutes.begin(); k != m_ASexternalRoutes.end(); k++)
    {
        if (tmp == index)
        {
            NS_LOG_LOGIC("Removing route " << index << "; size = " << m_ASexternalRoutes.size());
            delete *k;
            m_ASexternalRoutes.erase(k);
            NS_LOG_LOGIC("Done removing network route "
                         << index << "; network route remaining size = " << m_networkRoutes.size());
            return;
        }
        tmp++;
    }
    NS_ASSERT(false);
}

int64_t
DGRRouting::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_rand->SetStream(stream);
    return 1;
}

Ptr<Ipv4Route>
DGRRouting::LookupShortestRoute(Ipv4Address dest, Ptr<NetDevice> oif)
{
    NS_LOG_FUNCTION(this << dest << oif);
    NS_LOG_LOGIC("Looking for route for destination " << dest);
    Ptr<Ipv4Route> rtentry = 0;
    // store all available routes that bring packets to their destination
    typedef std::vector<ShortestPathForestRIE*> RouteVec_t;
    RouteVec_t allRoutes;

    NS_LOG_LOGIC("Number of m_hostRoutes = " << m_hostRoutes.size());
    for (HostRoutesCI i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
    {
        NS_ASSERT((*i)->IsHost());
        if ((*i)->GetDest() == dest)
        {
            if (oif != nullptr)
            {
                if (oif != m_ipv4->GetNetDevice((*i)->GetInterface()))
                {
                    NS_LOG_LOGIC("Not on requested interface, skipping");
                    continue;
                }
            }
            allRoutes.push_back(*i);
            NS_LOG_LOGIC(allRoutes.size() << "Found DGR host route" << *i);
        }
    }
    if (allRoutes.size() > 0) // if route(s) is found
    {
        uint32_t routRef = 0;
        uint32_t shortestDist = allRoutes.at(0)->GetDistance();
        for (uint32_t i = 0; i < allRoutes.size(); i++)
        {
            if (allRoutes.at(i)->GetDistance() < shortestDist)
            {
                routRef = i;
            }
        }
        ShortestPathForestRIE* route = allRoutes.at(routRef);

        // create a Ipv4Route object from the selected routing table entry
        rtentry = Create<Ipv4Route>();
        rtentry->SetDestination(route->GetDest());
        rtentry->SetSource(m_ipv4->GetAddress(route->GetInterface(), 0).GetLocal());
        rtentry->SetGateway(route->GetGateway());
        uint32_t interfaceIdx = route->GetInterface();
        rtentry->SetOutputDevice(m_ipv4->GetNetDevice(interfaceIdx));
        return rtentry;
    }
    else
    {
        return 0;
    }
}

Ptr<Ipv4Route>
DGRRouting::LookupDGRRoute(Ipv4Address dest, Ptr<Packet> p, Ptr<const NetDevice> idev)
{
    PriorityTag prioTag;
    BudgetTag bgtTag;
    TimestampTag timeTag;
    p->PeekPacketTag(prioTag);
    p->PeekPacketTag(bgtTag);
    p->PeekPacketTag(timeTag);
    DistTag distTag;
    uint32_t dist = UINT32_MAX;
    dist -= 1;
    if (p->PeekPacketTag(distTag))
        dist = distTag.GetDistance();
    // budget in microseconds
    uint32_t bgt;
    if (bgtTag.GetBudget() + timeTag.GetTimestamp().GetMicroSeconds() <
        Simulator::Now().GetMicroSeconds())
    {
        bgt = 0;
    }
    else
        bgt = (bgtTag.GetBudget() + timeTag.GetTimestamp().GetMicroSeconds() -
               Simulator::Now().GetMicroSeconds()) /
              100;
    /**
     * Lookup a Route to forward the DGR packets.
     */
    NS_LOG_FUNCTION(this << dest << idev);
    NS_LOG_LOGIC("Looking for route for destination " << dest);
    Ptr<Ipv4Route> rtentry = 0;
    // store all available routes that bring packets to their destination
    typedef std::vector<ShortestPathForestRIE*> RouteVec_t;
    // typedef std::vector<ShortestPathForestRIE *>::const_iterator RouteVecCI_t;
    RouteVec_t allRoutes;

    NS_LOG_LOGIC("Number of m_hostRoutes = " << m_hostRoutes.size());
    for (auto i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
    {
        NS_ASSERT((*i)->IsHost());
        if ((*i)->GetDest() == dest)
        {
            if (idev != nullptr)
            {
                if (idev == m_ipv4->GetNetDevice((*i)->GetInterface()))
                {
                    NS_LOG_LOGIC("Not on requested interface, skipping");
                    continue;
                }
            }

            // if interface is down, continue
            if (!m_ipv4->IsUp((*i)->GetInterface()))
                continue;

            // get the local queue delay in microseconds
            Ptr<NetDevice> dev_loc = m_ipv4->GetNetDevice((*i)->GetInterface());
            Ptr<QueueDisc> disc = m_ipv4->GetObject<Node>()
                                      ->GetObject<TrafficControlLayer>()
                                      ->GetRootQueueDiscOnDevice(dev_loc);
            Ptr<DGRQueueDisc> dgr_q = DynamicCast<DGRQueueDisc>(disc);

            // Get the Slow lane length
            uint32_t queue_len = dgr_q->GetInternalQueue(1)->GetCurrentSize().GetValue();
            uint32_t queue_max = dgr_q->GetInternalQueue(1)->GetMaxSize().GetValue();
            if (queue_len >= queue_max * 0.75)
            {
                NS_LOG_LOGIC("Congestion happened, skipping");
                continue;
            }

            // get the next hop slow queue infomation
            if ((*i)->GetNextIface() != 0xffffffff)
            {
                Ptr<Channel> channel = dev_loc->GetChannel();
                PointToPointChannel* p2pchannel =
                    dynamic_cast<PointToPointChannel*>(PeekPointer(channel));
                if (p2pchannel != 0)
                {
                    // Get the remote netdevice
                    Ptr<NetDevice> dev_rmt = p2pchannel->GetDevice(0);
                    if (dev_rmt == dev_loc)
                    {
                        dev_rmt = p2pchannel->GetDevice(1);
                    }
                    Ptr<Node> node_rmt = dev_rmt->GetNode();
                    Ptr<QueueDisc> disc_rmt =
                        node_rmt->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(
                            dev_rmt);
                    Ptr<DGRQueueDisc> dgr_q_rmt = DynamicCast<DGRQueueDisc>(disc_rmt);
                    uint32_t remot_queue_len =
                        dgr_q_rmt->GetInternalQueue(1)->GetCurrentSize().GetValue();
                    uint32_t remot_queue_max =
                        dgr_q_rmt->GetInternalQueue(1)->GetMaxSize().GetValue();
                    uint32_t remot_slow_len =
                        dgr_q_rmt->GetInternalQueue(2)->GetCurrentSize().GetValue();
                    uint32_t remot_slow_max =
                        dgr_q_rmt->GetInternalQueue(2)->GetMaxSize().GetValue();
                    if (remot_queue_len >= remot_queue_max * 0.75 ||
                        remot_slow_len >= remot_slow_max * 0.75)
                    {
                        NS_LOG_LOGIC("Congestion over 75\% in next hop, skipping");
                        continue;
                    }
                }
            }
        }
        if ((*i)->GetDistance() > bgt || (*i)->GetDistance() > dist)
        {
            NS_LOG_LOGIC("Too far to the destination, skipping");
            continue;
        }
        allRoutes.push_back(*i);
        NS_LOG_LOGIC(allRoutes.size()
                     << "Found DGR host route" << *i << " with Cost: " << (*i)->GetDistance());
    }

    if (allRoutes.size() > 0) // if route(s) is found
    {
        // random select
        uint32_t selectIndex = m_rand->GetInteger(0, allRoutes.size() - 1);

        // // optimal
        // uint32_t selectIndex = 0;
        // uint32_t min = UINT32_MAX;
        // for (uint32_t i = 0; i < allRoutes.size (); i++)
        // {
        //   if (allRoutes.at (i)->GetDistance () < min)
        //   {
        //     min = allRoutes.at (i)->GetDistance ();
        //     selectIndex = i;
        //   }
        //  }

        // // worst
        // uint32_t selectIndex = 0;
        // uint32_t max = 0;
        // for (uint32_t i = 0; i < allRoutes.size (); i++)
        // {
        //   if (allRoutes.at (i)->GetDistance () > max)
        //   {
        //     max = allRoutes.at (i)->GetDistance ();
        //     selectIndex = i;
        //   }
        // }

        // // back pressure selection
        // uint32_t selectIndex = 0;
        // double minPressure = 1.0;
        // uint32_t k = 0;
        // for (RouteVecCI_t i = allRoutes.begin ();
        //      i != allRoutes.end ();
        //      i++, k ++)
        //   {
        //     // get the output device
        //     Ptr <NetDevice> dev = m_ipv4->GetNetDevice ((*i)->GetInterface ());
        //     // get the nexthop queue infomation
        //     Ptr<Channel> channel = dev->GetChannel ();
        //     PointToPointChannel *p2pchannel = dynamic_cast <PointToPointChannel *> (PeekPointer
        //     (channel)); if (p2pchannel != 0)
        //       {
        //         Ptr<Node> node = dev->GetNode ();
        //         Ptr<PointToPointNetDevice> d_dev = p2pchannel->GetDestination (0) == dev ?
        //         p2pchannel->GetDestination (1) : p2pchannel->GetDestination (0); Ptr<Node> d_node
        //         = d_dev->GetNode (); if ((*i)->GetNextInterface () != 0xffffffff)
        //           {
        //             Ptr<NetDevice> next_dev = d_node->GetDevice ((*i)->GetNextInterface ());
        //             // std::cout << "next node: " << d_node->GetId () << "next interface: " <<
        //             (*i)->GetNextInterface () << std::endl; Ptr<QueueDisc> next_disc =
        //             d_node->GetObject<TrafficControlLayer> ()->GetRootQueueDiscOnDevice
        //             (next_dev); Ptr<DGRVirtualQueueDisc> next_dvq = DynamicCast
        //             <DGRVirtualQueueDisc> (next_disc); uint32_t next_dvq_length =
        //             next_dvq->GetInternalQueue (1)->GetCurrentSize ().GetValue (); uint32_t
        //             next_dvq_max = next_dvq->GetInternalQueue (1)->GetMaxSize ().GetValue ();
        //             uint32_t next_dvq_slow_length = next_dvq->GetInternalQueue
        //             (2)->GetCurrentSize ().GetValue ();
        //             // uint32_t next_dvq_slow_max = next_dvq->GetInternalQueue (2)-> GetMaxSize
        //             ().GetValue (); double pressure1 = next_dvq_length*1.0/next_dvq_max; double
        //             pressure2 = next_dvq_slow_length*1.0/155;
        //             // if (next_dvq_slow_length != 0) std::cout << "slow lane current: " <<
        //             next_dvq_slow_length  << "slow_max: " << next_dvq_slow_max << std::endl; if
        //             (pressure1 < pressure2) pressure1 = pressure2;
        //             // if (pressure1 > 0.2) std::cout << pressure1 << std::endl;
        //             if (pressure1 < minPressure)
        //               {
        //                 minPressure = pressure1;
        //                 selectIndex = k;
        //               }
        //           }
        //       }
        //   }

        // // back pressure + random selection
        // uint32_t selectIndex = 0;
        // double minPressure = 1.0;
        // uint32_t k = 0;
        // for (RouteVecCI_t i = allRoutes.begin ();
        //      i != allRoutes.end ();
        //      i++, k ++)
        //   {
        //     // get the output device
        //     Ptr <NetDevice> dev = m_ipv4->GetNetDevice ((*i)->GetInterface ());
        //     // get the nexthop queue infomation
        //     Ptr<Channel> channel = dev->GetChannel ();
        //     PointToPointChannel *p2pchannel = dynamic_cast <PointToPointChannel *> (PeekPointer
        //     (channel)); if (p2pchannel != 0)
        //       {
        //         Ptr<Node> node = dev->GetNode ();
        //         Ptr<PointToPointNetDevice> d_dev = p2pchannel->GetDestination (0) == dev ?
        //         p2pchannel->GetDestination (1) : p2pchannel->GetDestination (0); Ptr<Node> d_node
        //         = d_dev->GetNode (); if ((*i)->GetNextInterface () != 0xffffffff)
        //           {
        //             Ptr<NetDevice> next_dev = d_node->GetDevice ((*i)->GetNextInterface ());
        //             // std::cout << "next node: " << d_node->GetId () << "next interface: " <<
        //             (*i)->GetNextInterface () << std::endl; Ptr<QueueDisc> next_disc =
        //             d_node->GetObject<TrafficControlLayer> ()->GetRootQueueDiscOnDevice
        //             (next_dev); Ptr<DGRVirtualQueueDisc> next_dvq = DynamicCast
        //             <DGRVirtualQueueDisc> (next_disc); uint32_t next_dvq_length =
        //             next_dvq->GetInternalQueue (1)->GetCurrentSize ().GetValue (); uint32_t
        //             next_dvq_max = next_dvq->GetInternalQueue (1)->GetMaxSize ().GetValue ();
        //             uint32_t next_dvq_slow_length = next_dvq->GetInternalQueue
        //             (2)->GetCurrentSize ().GetValue ();
        //             // uint32_t next_dvq_slow_max = next_dvq->GetInternalQueue (2)-> GetMaxSize
        //             ().GetValue (); double pressure1 = next_dvq_length*1.0/next_dvq_max; double
        //             pressure2 = next_dvq_slow_length*1.0/155;
        //             // if (next_dvq_slow_length != 0) std::cout << "slow lane current: " <<
        //             next_dvq_slow_length  << "slow_max: " << next_dvq_slow_max << std::endl; if
        //             (pressure1 < pressure2) pressure1 = pressure2;
        //             // if (pressure1 > 0.2) std::cout << pressure1 << std::endl;
        //             if (pressure1 < minPressure)
        //               {
        //                 minPressure = pressure1;
        //                 selectIndex = k;
        //               }
        //           }
        //       }
        //   }

        ShortestPathForestRIE* route = allRoutes.at(selectIndex);
        rtentry = Create<Ipv4Route>();
        rtentry->SetDestination(route->GetDest());
        /// \todo handle multi-address case
        rtentry->SetSource(m_ipv4->GetAddress(route->GetInterface(), 0).GetLocal());
        rtentry->SetGateway(route->GetGateway());
        uint32_t interfaceIdx = route->GetInterface();
        rtentry->SetOutputDevice(m_ipv4->GetNetDevice(interfaceIdx));
        // if (route->GetDistance () > 30000) std::cout << "budget: " << bgt << " distance: " <<
        // route->GetDistance () << std::endl;
        if (bgt - route->GetDistance() <= 20)
        {
            prioTag.SetPriority(0);
        }
        else
        {
            prioTag.SetPriority(1);
        }
        distTag.SetDistance(route->GetDistance());
        p->ReplacePacketTag(prioTag);

        p->ReplacePacketTag(distTag);
        return rtentry;
    }
    else
    {
        return 0;
    }
}

void
DGRRouting::DoInitialize(void)
{
    NS_LOG_FUNCTION(this);
    Ipv4RoutingProtocol::DoInitialize();
}

void
DGRRouting::DoDispose()
{
    NS_LOG_FUNCTION(this);
    for (auto i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i = m_hostRoutes.erase(i))
    {
        delete (*i);
    }
    for (auto j = m_networkRoutes.begin(); j != m_networkRoutes.end(); j = m_networkRoutes.erase(j))
    {
        delete (*j);
    }
    for (auto l = m_ASexternalRoutes.begin(); l != m_ASexternalRoutes.end();
         l = m_ASexternalRoutes.erase(l))
    {
        delete (*l);
    }

    Ipv4RoutingProtocol::DoDispose();
}

} // namespace ns3
