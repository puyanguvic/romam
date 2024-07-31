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

#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <stdlib.h>
#include <string>

using namespace ns3;

std::string expName = "Octopus";
NS_LOG_COMPONENT_DEFINE(expName);

int
main(int argc, char* argv[])
{
    std::string topo("abilene");
    std::string format("Inet");

    // Set up command line parameters used to control the experiment
    CommandLine cmd(__FILE__);
    cmd.AddValue("format", "Format to use for data input [Orbis|Inet|Rocketfuel].", format);
    cmd.AddValue("topo", "topology", topo);

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
    OctopusHelper oct;
    Ipv4ListRoutingHelper list;
    list.Add(oct, 10);
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

    OctopusHelper::PopulateRoutingTables();

    // ------------------------------------------------------------
    // -- Print routing table
    // ---------------------------------------------
    OctopusHelper r;
    Ptr<OutputStreamWrapper> routingStream =
        Create<OutputStreamWrapper>(topo + expName + ".routes", std::ios::out);
    r.PrintRoutingTableAllAt(Seconds(0), routingStream);

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