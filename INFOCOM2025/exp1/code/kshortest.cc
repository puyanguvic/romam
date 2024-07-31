/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/node-list.h"
#include "ns3/point-to-point-module.h"
#include "ns3/romam-module.h"
#include "ns3/topology-read-module.h"
#include "ns3/traffic-control-module.h"

#include <ctime>
#include <fstream>
#include <list>
#include <sstream>
#include <string>

using namespace ns3;

std::string expName = "kshortest";
NS_LOG_COMPONENT_DEFINE(expName);

int
main(int argc, char* argv[])
{
    std::string topo("abilene");
    std::string format("Inet");
    uint32_t budget = 30000; // 30ms
    uint32_t sink = 10;
    uint32_t sender = 0;
    uint16_t udpPort = 9;
    uint32_t nPacket = 1000;
    uint32_t packetSize = 1400; // bytes

    uint32_t routeSelectMode = 1;

    // Set up command line parameters used to control the experiment
    CommandLine cmd(__FILE__);
    cmd.AddValue("format", "Format to use for data input [Orbis|Inet|Rocketfuel].", format);
    cmd.AddValue("topo", "topology", topo);
    cmd.AddValue("budget", "budget", budget);
    cmd.AddValue("sender", "Node # of sender", sender);
    cmd.AddValue("sink", "Node # of sink", sink);

    cmd.AddValue("EcmpMode", "EcmpMode: (0 none, 1 KShort, 2 DGR, 3 DDR)", routeSelectMode);

    cmd.Parse(argc, argv);

    switch (routeSelectMode)
    {
    case 0:
        break;
    case 1:
        Config::SetDefault("ns3::DDRRouting::RouteSelectMode", StringValue("KSHORT"));
        break;
    case 2:
        Config::SetDefault("ns3::DDRRouting::RouteSelectMode", StringValue("DGR"));
        break;
    case 3:
        Config::SetDefault("ns3::DDRRouting::RouteSelectMode", StringValue("DDR"));
        break;
    default:
        break;
    }

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
    InternetStackHelper stack;
    DDRHelper ddr;
    Ipv4ListRoutingHelper list;
    list.Add(ddr, 10);
    stack.SetRoutingHelper(list);
    stack.Install(nodes);

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
        address.NewNetwork();
    }

    DDRHelper::PopulateRoutingTables();

    // -------- Run the simulation --------------------------
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    delete[] ipic;
    delete[] ndc;
    delete[] nc;

    NS_LOG_INFO("Done.");
    return 0;
}