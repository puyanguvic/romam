/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/node-list.h"
#include "ns3/point-to-point-module.h"
#include "ns3/romam-module.h"
#include "ns3/topology-read-module.h"
#include "ns3/traffic-control-module.h"

#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <stdlib.h>
#include <string>

using namespace ns3;

std::string expName = "DDR";
NS_LOG_COMPONENT_DEFINE(expName);

int
main(int argc, char* argv[])
{
    std::string topo("abilene");
    std::string format("Inet");
    uint32_t budget(20000); // in microsecond
    uint32_t sink = 10;
    uint32_t sender = 0;
    uint16_t udpPort = 9;
    uint32_t nPacket = 10000;
    uint32_t packetSize = 1400; // bytes

    // Set up command line parameters used to control the experiment
    CommandLine cmd(__FILE__);
    cmd.AddValue("format", "Format to use for data input [Orbis|Inet|Rocketfuel].", format);
    cmd.AddValue("topo", "topology", topo);
    cmd.AddValue("budget", "budget", budget);
    cmd.AddValue("sender", "Node # of sender", sender);
    cmd.AddValue("sink", "Node # of sink", sink);

    cmd.Parse(argc, argv);

    // ------------- Read topology data-------------------
    std::string input("contrib/romam/topo/Inet_" + topo + "_topo.txt");
    TopologyReaderHelper topoHelp;
    topoHelp.SetFileName(input);
    topoHelp.SetFileType(format);
    Ptr<TopologyReader> inFile = topoHelp.GetTopologyReader();
    NodeContainer nodes;
    if (inFile)
    {
        nodes = inFile->Read();
    }
    if (inFile->LinksSize() == 0)
    {
        NS_LOG_ERROR("Problems reading the topology file. Failing.");
        return -1;
    }

    // -------- Create nodes and network stacks ---------------
    NS_LOG_INFO("creating internet stack");
    DDRHelper ddr;
    Ipv4ListRoutingHelper list;
    list.Add(ddr, 10);
    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    NS_LOG_INFO("creating ipv4 addresses");
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.252");

    int totlinks = inFile->LinksSize();

    NS_LOG_INFO("creating node containers");
    NodeContainer* nc = new NodeContainer[totlinks];
    NetDeviceContainer* ndc = new NetDeviceContainer[totlinks];
    PointToPointHelper p2p;
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::DDRQueueDisc");

    NS_LOG_INFO("creating ipv4 interfaces");
    Ipv4InterfaceContainer* ipic = new Ipv4InterfaceContainer[totlinks];
    // std::cout << "totlinks number: " << totlinks << std::endl;
    TopologyReader::ConstLinksIterator iter;
    int i = 0;
    for (iter = inFile->LinksBegin(); iter != inFile->LinksEnd(); iter++, i++)
    {
        nc[i] = NodeContainer(iter->GetFromNode(), iter->GetToNode());
        std::string delay = iter->GetAttribute("Weight");
        std::stringstream ss;
        ss << delay;
        uint16_t metric; //!< metric in milliseconds
        ss >> metric;
        p2p.SetChannelAttribute("Delay", StringValue(delay + "ms"));
        p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
        ndc[i] = p2p.Install(nc[i]);
        tch.Install(ndc[i]);
        ipic[i] = address.Assign(ndc[i]);
        ipic[i].SetMetric(0, metric);
        ipic[i].SetMetric(1, metric);
        address.NewNetwork();
    }

    DDRHelper::PopulateRoutingTables();

    // -------------------- UDP traffic -----------------
    Ptr<Node> udpSinkNode = nodes.Get(sink);
    Ptr<Ipv4> ipv4UdpSink = udpSinkNode->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddrUdpSink = ipv4UdpSink->GetAddress(1, 0);
    Ipv4Address ipv4AddrUdpSink = iaddrUdpSink.GetLocal();

    RomamSinkHelper sinkHelper("ns3::UdpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(sink));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(31.0));

    // udp sender
    Ptr<Socket> udpSocket = Socket::CreateSocket(nodes.Get(sender), UdpSocketFactory::GetTypeId());
    Ptr<RomamUdpApplication> app = CreateObject<RomamUdpApplication>();
    app->Setup(udpSocket,
               InetSocketAddress(ipv4AddrUdpSink, udpPort),
               packetSize,
               nPacket,
               DataRate("50Mbps"),
               budget,
               true);
    nodes.Get(sender)->AddApplication(app);
    app->SetStartTime(Seconds(0.0));
    app->SetStopTime(Seconds(30.0));

    // // --------------- Net Anim ---------------------
    // AnimationInterface anim(topo + expName + ".xml");
    // std::ifstream topoNetanim(input);
    // std::istringstream buffer;
    // std::string line;
    // getline(topoNetanim, line);
    // for (uint32_t i = 0; i < nodes.GetN(); i++)
    // {
    //     getline(topoNetanim, line);
    //     buffer.clear();
    //     buffer.str(line);
    //     int no;
    //     double x, y;
    //     buffer >> no;
    //     buffer >> x;
    //     buffer >> y;
    //     anim.SetConstantPosition(nodes.Get(no), x * 10, y * 10);
    // }

    // // ------------------------------------------------------------
    // // -- Print routing table
    // // ---------------------------------------------
    // DDRHelper r;
    // Ptr<OutputStreamWrapper> routingStream =
    //     Create<OutputStreamWrapper>(topo + expName + ".routes", std::ios::out);
    // r.PrintRoutingTableAllAt(Seconds(0), routingStream);

    // -------- Run the simulation --------------------------
    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();

    delete[] ipic;
    delete[] ndc;
    delete[] nc;

    NS_LOG_INFO("Done.");
    return 0;
}