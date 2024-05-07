// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-
#include "octopus-routing.h"

#include "datapath/dgr-headers.h"
#include "datapath/octopus-headers.h"
#include "datapath/romam-tags.h"
#include "priority_manage/ddr-queue-disc.h"
#include "routing_algorithm/armed-spf-rie.h"
#include "utility/route-manager.h"

#include "ns3/boolean.h"
#include "ns3/fifo-queue-disc.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "ns3/ipv4-route.h"
#include "ns3/log.h"
#include "ns3/loopback-net-device.h"
#include "ns3/names.h"
#include "ns3/network-module.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/traffic-control-module.h"
#include "ns3/udp-header.h"
#include "ns3/udp-socket-factory.h"

#include <iomanip>
#include <random>
#include <string>
#include <vector>

#define OCTOPUS_PORT 666
#define OCTOPUS_BROAD_CAST "224.0.0.17"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OctopusRouting");

NS_OBJECT_ENSURE_REGISTERED(OctopusRouting);

TypeId
OctopusRouting::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::OctopusRouting")
                            .SetParent<RomamRouting>()
                            .SetGroupName("romam")
                            .AddConstructor<OctopusRouting>();
    return tid;
}

OctopusRouting::OctopusRouting()
    : m_armDatabase(),
      m_initialized(false)
{
    NS_LOG_FUNCTION(this);
    m_rand = CreateObject<UniformRandomVariable>();
}

OctopusRouting::~OctopusRouting()
{
    NS_LOG_FUNCTION(this);
}

Ptr<Ipv4Route>
OctopusRouting::RouteOutput(Ptr<Packet> p,
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
        return 0; // Let other routing protocols try to handle this
    }

    //
    // See if this is a unicast packet we have a route for.
    //
    NS_LOG_LOGIC("Looking up route");
    Ptr<Ipv4Route> rtentry = LookupRoute(header.GetDestination());
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
OctopusRouting::RouteInput(Ptr<const Packet> p,
                           const Ipv4Header& header,
                           Ptr<const NetDevice> idev,
                           const UnicastForwardCallback& ucb,
                           const MulticastForwardCallback& mcb,
                           const LocalDeliverCallback& lcb,
                           const ErrorCallback& ecb)
{
    NS_LOG_FUNCTION(this << p << header << header.GetSource() << header.GetDestination() << idev
                         << &lcb << &ecb);
    // Check if input device supports IP
    NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
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
    if (m_ipv4->IsForwarding(iif) == false)
    {
        NS_LOG_LOGIC("Forwarding disabled for this interface");
        ecb(p, header, Socket::ERROR_NOROUTETOHOST);
        return true;
    }
    // Next, try to find a route
    NS_LOG_LOGIC("Unicast destination- looking up global route");
    Ptr<Ipv4Route> rtentry = LookupRoute(header.GetDestination());
    if (rtentry)
    {
        uint32_t oif = rtentry->GetOutputDevice()->GetIfIndex();
        NS_LOG_LOGIC("Found unicast destination- calling unicast callback");
        ucb(rtentry, p, header);
        SendOneHopAck(header.GetDestination(), iif, oif);
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
OctopusRouting::NotifyInterfaceUp(uint32_t i)
{
    NS_LOG_FUNCTION(this << i);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_FUNCTION("Update routing table");
        RouteManager::DeleteRoutes();
        RouteManager::BuildLSDB();
        RouteManager::InitializeSPFRoutes();
    }
}

void
OctopusRouting::NotifyInterfaceDown(uint32_t i)
{
    NS_LOG_FUNCTION(this << i);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_FUNCTION("Update routing table");
        RouteManager::DeleteRoutes();
        RouteManager::BuildLSDB();
        RouteManager::InitializeSPFRoutes();
    }
}

void
OctopusRouting::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_FUNCTION("Update routing table");
        RouteManager::DeleteRoutes();
        RouteManager::BuildLSDB();
        RouteManager::InitializeSPFRoutes();
    }
}

void
OctopusRouting::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION(this << interface << address);
    if (m_respondToInterfaceEvents && Simulator::Now().GetSeconds() > 0) // avoid startup events
    {
        NS_LOG_FUNCTION("Update routing table");
        RouteManager::DeleteRoutes();
        RouteManager::BuildLSDB();
        RouteManager::InitializeSPFRoutes();
    }
}

void
OctopusRouting::SetIpv4(Ptr<Ipv4> ipv4)
{
    NS_LOG_FUNCTION(this << ipv4);
    NS_ASSERT(!m_ipv4 && ipv4);
    m_ipv4 = ipv4;
}

// Formatted like output of "route -n" command
void
OctopusRouting::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
    NS_LOG_FUNCTION(this << stream);
    std::ostream* os = stream->GetStream();
    // Copy the current ostream state
    std::ios oldState(nullptr);
    oldState.copyfmt(*os);

    // *os << "Node: " << m_ipv4->GetObject<Node>()->GetId() << ", Time: " << Now().As(unit)
    //     << ", Local time: " << m_ipv4->GetObject<Node>()->GetLocalTime().As(unit)
    //     << ", OctopusRouting table" << std::endl;

    if (GetNRoutes() > 0)
    {
        *os << "  Destination     Gateway    Flags   Metric  Iface   NextIface   Loss   Pulls"
            << std::endl;
        for (uint32_t j = 0; j < GetNRoutes(); j++)
        {
            std::ostringstream dest, gw, mask, flags, metric;
            ArmedSpfRIE route = GetRoute(j);
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
            *os << std::setw(18) << route.GetCumulativeLoss();
            *os << std::setw(8) << route.GetNumPulls();
            *os << std::endl;
        }
    }
    *os << std::endl;
    (*os).copyfmt(oldState);
}

void
OctopusRouting::AddHostRouteTo(Ipv4Address dest, Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(this << dest << nextHop << interface);
    ArmedSpfRIE* route = new ArmedSpfRIE();
    *route = ArmedSpfRIE::CreateHostRouteTo(dest, nextHop, interface);
    m_hostRoutes.push_back(route);
}

void
OctopusRouting::AddHostRouteTo(Ipv4Address dest, uint32_t interface)
{
    NS_LOG_FUNCTION(this << dest << interface);
    ArmedSpfRIE* route = new ArmedSpfRIE();
    *route = ArmedSpfRIE::CreateHostRouteTo(dest, interface);
    m_hostRoutes.push_back(route);
}

void
OctopusRouting::AddHostRouteTo(Ipv4Address dest,
                               Ipv4Address nextHop,
                               uint32_t interface,
                               uint32_t nextInterface,
                               uint32_t distance)
{
    NS_LOG_FUNCTION(this << dest << nextHop << interface << nextInterface << distance);
    ArmedSpfRIE* route = new ArmedSpfRIE();
    *route = ArmedSpfRIE::CreateHostRouteTo(dest, nextHop, interface, nextInterface, distance);
    m_hostRoutes.push_back(route);
}

void
OctopusRouting::AddNetworkRouteTo(Ipv4Address network,
                                  Ipv4Mask networkMask,
                                  Ipv4Address nextHop,
                                  uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << nextHop << interface);
    ArmedSpfRIE* route = new ArmedSpfRIE();
    *route = ArmedSpfRIE::CreateNetworkRouteTo(network, networkMask, nextHop, interface);
    m_networkRoutes.push_back(route);
}

void
OctopusRouting::AddNetworkRouteTo(Ipv4Address network, Ipv4Mask networkMask, uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << interface);
    ArmedSpfRIE* route = new ArmedSpfRIE();
    *route = ArmedSpfRIE::CreateNetworkRouteTo(network, networkMask, interface);
    m_networkRoutes.push_back(route);
}

void
OctopusRouting::AddASExternalRouteTo(Ipv4Address network,
                                     Ipv4Mask networkMask,
                                     Ipv4Address nextHop,
                                     uint32_t interface)
{
    NS_LOG_FUNCTION(this << network << networkMask << nextHop << interface);
    ArmedSpfRIE* route = new ArmedSpfRIE();
    *route = ArmedSpfRIE::CreateNetworkRouteTo(network, networkMask, nextHop, interface);
    m_ASexternalRoutes.push_back(route);
}

uint32_t
OctopusRouting::GetNRoutes(void) const
{
    NS_LOG_FUNCTION(this);
    uint32_t n = 0;
    n += m_hostRoutes.size();
    n += m_networkRoutes.size();
    n += m_ASexternalRoutes.size();
    return n;
}

ArmedSpfRIE*
OctopusRouting::GetRoute(uint32_t index) const
{
    NS_LOG_FUNCTION(this << index);
    if (index < m_hostRoutes.size())
    {
        uint32_t tmp = 0;
        HostRoutes::const_iterator i = m_hostRoutes.begin();
        for (; i != m_hostRoutes.end(); i++)
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
        NetworkRoutes::const_iterator j = m_networkRoutes.begin();
        for (; j != m_networkRoutes.end(); j++)
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
    ASExternalRoutes::const_iterator k = m_ASexternalRoutes.begin();
    for (; k != m_ASexternalRoutes.end(); k++)
    {
        if (tmp == index)
        {
            return *k;
        }
        tmp++;
    }
    NS_ASSERT(false);
    // quiet compiler.
    return 0;
}

void
OctopusRouting::RemoveRoute(uint32_t index)
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
OctopusRouting::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_rand->SetStream(stream);
    return 1;
}

Ptr<Ipv4Route>
OctopusRouting::LookupRoute(Ipv4Address dest, Ptr<NetDevice> oif)
{
    NS_LOG_FUNCTION(this << dest << oif);
    NS_LOG_LOGIC("Looking for route for destination " << dest);
    Ptr<Ipv4Route> rtentry = nullptr;
    // store all available routes that bring packets to their destination
    typedef std::vector<ArmedSpfRIE*> RouteVec_t;
    RouteVec_t allRoutes;

    NS_LOG_LOGIC("Number of m_hostRoutes = " << m_hostRoutes.size());
    for (auto i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
    {
        NS_ASSERT((*i)->IsHost());
        if ((*i)->GetDest() == dest)
        {
            if (oif)
            {
                if (oif != m_ipv4->GetNetDevice((*i)->GetInterface()))
                {
                    NS_LOG_LOGIC("Not on requested interface, skipping");
                    continue;
                }
            }
            (*i)->PullArm();
            allRoutes.push_back(*i);
            NS_LOG_LOGIC(allRoutes.size() << "Found global host route" << *i);
        }
    }

    if (!allRoutes.empty()) // if route(s) is found
    {
        int nRoutes = (int)allRoutes.size();
        double p[nRoutes];
        int ref = 0;
        double p_total = 0.0;
        double chances = nRoutes * log(nRoutes);
        for (auto i = allRoutes.begin(); i != allRoutes.end(); i++)
        {
            // Get the number of pulls
            uint32_t nPulls = (*i)->GetNumPulls();
            double loss = (*i)->GetCumulativeLoss();
            double eta = sqrt(chances / (double)nPulls);
            p[ref] = exp(-eta * loss);
            p_total += p[ref];
            ref += 1;
        }
        // norm the probabilities
        p[0] = p[0] / p_total;
        for (int j = 1; j < nRoutes; j++)
        {
            p[j] = p[j] / p_total + p[j - 1];
        }
        double random = m_rand->GetValue(0, 1);
        int j = 0;
        while (p[j] < random)
            j++;
        if (j >= nRoutes)
        {
            j = nRoutes - 1;
        }
        uint32_t selectIndex = j;
        ArmedSpfRIE* route = allRoutes.at(selectIndex);
        // create a Ipv4Route object from the selected routing table entry
        rtentry = Create<Ipv4Route>();
        rtentry->SetDestination(route->GetDest());
        /// \todo handle multi-address case
        rtentry->SetSource(m_ipv4->GetAddress(route->GetInterface(), 0).GetLocal());
        rtentry->SetGateway(route->GetGateway());
        uint32_t interfaceIdx = route->GetInterface();
        rtentry->SetOutputDevice(m_ipv4->GetNetDevice(interfaceIdx));
        return rtentry;
    }
    else
    {
        return nullptr;
    }
}

void
OctopusRouting::InitializeSocketList()
{
    // Initialize the sockets for every netdevice
    for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++)
    {
        Ptr<LoopbackNetDevice> check = DynamicCast<LoopbackNetDevice>(m_ipv4->GetNetDevice(i));
        if (check)
        {
            continue;
        }

        bool activeInterface = false;
        if (m_interfaceExclusions.find(i) == m_interfaceExclusions.end())
        {
            activeInterface = true;
            m_ipv4->SetForwarding(i, true);
        }

        for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); j++)
        {
            Ipv4InterfaceAddress address = m_ipv4->GetAddress(i, j);
            NS_LOG_LOGIC("For interface: " << i << "the " << j << "st Address is " << address);

            if (address.GetScope() != Ipv4InterfaceAddress::HOST && activeInterface == true)
            {
                NS_LOG_LOGIC("Octopus: add socket to " << address.GetLocal());
                TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
                Ptr<Node> theNode = m_ipv4->GetObject<Node>();
                Ptr<Socket> socket = Socket::CreateSocket(theNode, tid);

                InetSocketAddress local = InetSocketAddress(address.GetLocal(), OCTOPUS_PORT);
                socket->BindToNetDevice(m_ipv4->GetNetDevice(i));
                int ret = socket->Bind(local);
                NS_ASSERT_MSG(ret == 0, "Bind unsuccessful");

                socket->SetRecvCallback(MakeCallback(&OctopusRouting::Receive, this));
                socket->SetIpRecvTtl(true);
                socket->SetRecvPktInfo(true);

                m_unicastSocketList[socket] = i;
            }
        }
    }

    if (!m_multicastRecvSocket)
    {
        NS_LOG_LOGIC("DGR: adding receiving socket");
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        Ptr<Node> theNode = m_ipv4->GetObject<Node>();
        m_multicastRecvSocket = Socket::CreateSocket(theNode, tid);
        InetSocketAddress local = InetSocketAddress(OCTOPUS_BROAD_CAST, OCTOPUS_PORT);
        m_multicastRecvSocket->Bind(local);
        m_multicastRecvSocket->SetRecvCallback(MakeCallback(&OctopusRouting::Receive, this));
        m_multicastRecvSocket->SetIpRecvTtl(true);
        m_multicastRecvSocket->SetRecvPktInfo(true);
    }
}

void
OctopusRouting::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    m_initialized = true;
    RomamRouting::DoInitialize();
}

void
OctopusRouting::DoDispose(void)
{
    NS_LOG_FUNCTION(this);
    // TODO: Realise memorys
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

void
OctopusRouting::Receive(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Address sender;
    Ptr<Packet> packet = socket->RecvFrom(sender);
    InetSocketAddress senderAddr = InetSocketAddress::ConvertFrom(sender);
    NS_LOG_INFO("Received " << *packet << " from " << senderAddr.GetIpv4() << ":"
                            << senderAddr.GetPort());
    Ipv4Address senderAddress = senderAddr.GetIpv4();
    // uint32_t senderPort = senderAddr.GetPort ();

    if (socket == m_multicastRecvSocket)
    {
        NS_LOG_LOGIC("Received a packet from the multicast socket");
    }
    else
    {
        NS_LOG_LOGIC("Received a packet from one of the unicast sockets");
    }

    Ipv4PacketInfoTag interfaceInfo;
    if (!packet->RemovePacketTag(interfaceInfo))
    {
        NS_ABORT_MSG("No incoming interface on This message, aborting,");
    }
    uint32_t incomingIf = interfaceInfo.GetRecvIf();
    Ptr<Node> node = m_ipv4->GetObject<Node>();
    Ptr<NetDevice> dev = node->GetDevice(incomingIf);
    // uint32_t ipInterfaceIndex = m_ipv4->GetInterfaceForDevice(dev);

    int32_t interfaceForAddress = m_ipv4->GetInterfaceForAddress(senderAddress);
    if (interfaceForAddress != -1)
    {
        NS_LOG_LOGIC("Ignoring a packet sent by myself.");
        return;
    }

    OctopusHeader hdr;
    packet->RemoveHeader(hdr);
    if (hdr.GetCommand() == OctopusHeader::ACK)
    {
        NS_LOG_LOGIC("Update the cumulative loss with" << hdr.GetDestination() << ", "
                                                       << hdr.GetReward());
        Ipv4Address dest = hdr.GetDestination();
        double reward = hdr.GetReward();
        HandleUpdate(dest, incomingIf, reward);
    }
    else
    {
        // Leave for future use
        NS_LOG_LOGIC("Ignoring message with unknown command: " << int(hdr.GetCommand()));
    }
}

void
OctopusRouting::HandleUpdate(Ipv4Address dest, uint32_t interface, double reward)
{
    NS_LOG_FUNCTION(this << dest << interface << reward);

    NS_LOG_LOGIC("Looking for route for destination " << dest);

    // store all available routes that bring packets to their destination
    typedef std::vector<ArmedSpfRIE*> RouteVec_t;
    RouteVec_t allRoutes;

    NS_LOG_LOGIC("Number of m_hostRoutes = " << m_hostRoutes.size());
    for (auto i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
    {
        NS_ASSERT((*i)->IsHost());
        if ((*i)->GetDest() == dest)
        {
            allRoutes.push_back(*i);
            NS_LOG_LOGIC(allRoutes.size() << "Found global host route" << *i);
        }
    }

    if (!allRoutes.empty()) // if route(s) is found
    {
        int nRoutes = (int)allRoutes.size();
        double p[nRoutes];
        int ref = 0;
        double p_total = 0.0;
        double chances = nRoutes * log(nRoutes);
        ArmedSpfRIE* route;
        int route_ref = 0;
        for (auto i = allRoutes.begin(); i != allRoutes.end(); i++)
        {
            // record the right route we are finding
            if ((*i)->GetInterface() == interface)
            {
                route = *i;
                route_ref = ref;
            }
            // Get the number of pulls
            uint32_t nPulls = (*i)->GetNumPulls();
            double loss = (*i)->GetCumulativeLoss();
            double eta = sqrt(chances / (double)nPulls);
            p[ref] = exp(-eta * loss);
            p_total += p[ref];
            ref += 1;
        }
        // norm the probabilities
        for (int j = 0; j < nRoutes; j++)
        {
            p[j] = p[j] / p_total;
        }
        // update arm's cumulative loss
        // check the queueing delay of current node.
        Ptr<NetDevice> odev = m_ipv4->GetNetDevice(interface);
        Ptr<QueueDisc> disc =
            m_ipv4->GetObject<Node>()->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(
                odev);
        uint32_t length = disc->GetNBytes();
        double delay = length / 100.0; // delay in milliseconds
        reward += delay;
        double delta = (1 - exp(-(route->GetDistance() + reward))) / p[route_ref];
        route->UpdateArm(delta);
    }

    // Find the route and upate arm
    NS_LOG_FUNCTION(this << dest << interface << reward);
    NS_LOG_LOGIC("Looking for this route");
    for (auto i = m_hostRoutes.begin(); i != m_hostRoutes.end(); i++)
    {
        NS_ASSERT((*i)->IsHost());
        if ((*i)->GetDest() == dest)
        {
            if ((*i)->GetInterface() == interface)
            {
                (*i)->UpdateArm(reward);
                return;
            }
        }
    }
}

void
OctopusRouting::SendOneHopAck(Ipv4Address dest, uint32_t iif, uint32_t oif)
{
    NS_LOG_FUNCTION(this);
    auto iter = m_unicastSocketList.begin();
    for (; iter != m_unicastSocketList.end(); iter++)
    {
        if (iter->second == iif)
        {
            break;
        }
    }

    if (iter != m_unicastSocketList.end())
    {
        Ptr<NetDevice> odev = m_ipv4->GetNetDevice(oif);
        Ptr<QueueDisc> disc =
            m_ipv4->GetObject<Node>()->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(
                odev);
        uint32_t length = disc->GetNBytes();
        double delay = length / 100.0; // delay in milliseconds

        Ptr<Packet> p = Create<Packet>();
        SocketIpTtlTag ttlTag;
        p->RemovePacketTag(ttlTag);
        ttlTag.SetTtl(1);
        p->AddPacketTag(ttlTag);
        OctopusHeader hdr;
        hdr.SetCommand(OctopusHeader::ACK);
        hdr.SetDestination(dest);
        hdr.SetReward(delay);
        p->AddHeader(hdr);
        iter->first->SendTo(p, 0, InetSocketAddress(OCTOPUS_BROAD_CAST, OCTOPUS_PORT));
    }
}
} // namespace ns3