/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "octopus-helper.h"

#include "ns3/ipv4-list-routing.h"
#include "ns3/log.h"
#include "ns3/romam-module.h"
#include "ns3/traffic-control-layer.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OctopusHelper");

OctopusHelper::OctopusHelper()
{
}

OctopusHelper::OctopusHelper(const OctopusHelper& o)
{
}

OctopusHelper*
OctopusHelper::Copy(void) const
{
    return new OctopusHelper(*this);
}

Ptr<Ipv4RoutingProtocol>
OctopusHelper::Create(Ptr<Node> node) const
{
    NS_LOG_LOGIC("Adding Octopus Router interface to node " << node->GetId());
    // install DGRv2 Queue to netdevices

    // install DGR router to node.
    Ptr<OctopusRouter> router = CreateObject<OctopusRouter>();
    node->AggregateObject(router);

    NS_LOG_LOGIC("Adding RomamRouting Protocol to node " << node->GetId());
    Ptr<OctopusRouting> routing = CreateObject<OctopusRouting>();
    router->SetRoutingProtocol(routing);

    return routing;
}

void
OctopusHelper::PopulateRoutingTables(void)
{
    std::clock_t t;
    t = clock();
    RouteManager::BuildLSDB();
    RouteManager::InitializeSPFRoutes();
    // Initialize Sockets
    for (auto i = NodeList::Begin(); i != NodeList::End(); i++)
    {
        Ptr<Node> node = *i;
        Ptr<OctopusRouter> router = node->GetObject<OctopusRouter>();
        if (!router)
        {
            continue;
        }
        Ptr<RomamRouting> routing = router->GetRoutingProtocol();
        Ptr<OctopusRouting> octopus = DynamicCast<OctopusRouting>(routing);
        octopus->InitializeSocketList();
    }

    t = clock() - t;
    uint32_t time_init_ms = 1000.0 * t / CLOCKS_PER_SEC;
    std::cout << "CPU time used for Romam Routing Protocol Init: " << time_init_ms << " ms\n";
}

void
OctopusHelper::RecomputeRoutingTables(void)
{
    RouteManager::DeleteRoutes();
    RouteManager::BuildLSDB();
    RouteManager::InitializeSPFRoutes();
}

} // namespace ns3
