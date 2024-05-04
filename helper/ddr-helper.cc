/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ddr-helper.h"

#include "ns3/ipv4-list-routing.h"
#include "ns3/log.h"
#include "ns3/romam-module.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/traffic-control-layer.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DDRRoutingHelper");

DDRHelper::DDRHelper()
{
}

DDRHelper::DDRHelper(const DDRHelper& o)
{
}

DDRHelper*
DDRHelper::Copy(void) const
{
    return new DDRHelper(*this);
}

Ptr<Ipv4RoutingProtocol>
DDRHelper::Create(Ptr<Node> node) const
{
    NS_LOG_LOGIC("Adding DDRRouter interface to node " << node->GetId());
    // install DDR router to node.
    Ptr<DDRRouter> router = CreateObject<DDRRouter>();
    node->AggregateObject(router);

    // // install DDRQueueDisc to netdevices
    // QueueDiscContainer container = Install(node);

    NS_LOG_LOGIC("Adding DDRRouting Protocol to node " << node->GetId());
    Ptr<DDRRouting> routing = CreateObject<DDRRouting>();
    router->SetRoutingProtocol(routing);
    return routing;
}

void
DDRHelper::PopulateRoutingTables(void)
{
    std::clock_t t;
    t = clock();
    RouteManager::BuildLSDB();
    std::cout << "---Finished build up LSDB---\n";
    RouteManager::InitializeSPFRoutes();
    // Initialize Sockets
    for (auto i = NodeList::Begin(); i != NodeList::End(); i++)
    {
        Ptr<Node> node = *i;
        Ptr<DDRRouter> router = node->GetObject<DDRRouter>();
        if (!router)
        {
            continue;
        }
        Ptr<RomamRouting> routing = router->GetRoutingProtocol();
        Ptr<DDRRouting> octopus = DynamicCast<DDRRouting>(routing);
        octopus->InitializeSocketList();
    }

    t = clock() - t;
    uint32_t time_init_ms = 1000.0 * t / CLOCKS_PER_SEC;

    std::cout << "CPU time used for DDR Init: " << time_init_ms << " ms\n";
}

void
DDRHelper::RecomputeRoutingTables(void)
{
    RouteManager::DeleteRoutes();
    RouteManager::BuildLSDB();
    RouteManager::InitializeSPFRoutes();
}

QueueDiscContainer
DDRHelper::Install(Ptr<Node> node) const
{
    NetDeviceContainer container;
    for (uint32_t i = 0; i < node->GetNDevices(); i++)
    {
        container.Add(node->GetDevice(i));
    }
    return Install(container);
}

QueueDiscContainer
DDRHelper::Install(NetDeviceContainer c) const
{
    QueueDiscContainer container;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        container.Add(Install(*i));
    }
    return container;
}

QueueDiscContainer
DDRHelper::Install(Ptr<NetDevice> d) const
{
    QueueDiscContainer container;
    // A TrafficControlLayer object is aggregated by the InternetStackHelper, but check
    // anyway because a queue disc has no effect without a TrafficControlLayer object
    Ptr<TrafficControlLayer> tc = d->GetNode()->GetObject<TrafficControlLayer>();
    std::cout << "install queue disc\n";
    if (tc == nullptr)
    {
        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::DDRQueueDisc");
        container = tch.Install(d);
    }
    // else
    // {
    //     // NS_ASSERT(tc);
    //     // Generate the DDRQeueuDisc Object
    //     std::cout << "Install DDRQueueDisc\n";
    //     NS_LOG_LOGIC("Install DDRQueueDisc to routers");
    //     ObjectFactory queueFactory;
    //     queueFactory.SetTypeId("DDRQueueDisc");
    //     Ptr<DDRQueueDisc> qdisc = queueFactory.Create<DDRQueueDisc>();
    //     tc->SetRootQueueDiscOnDevice(d, qdisc);
    //     container.Add(qdisc);
    // }

    return container;
}

} // namespace ns3