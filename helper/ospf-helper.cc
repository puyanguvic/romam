/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-helper.h"

#include "ns3/ipv4-list-routing.h"
#include "ns3/log.h"
#include "ns3/romam-module.h"
#include "ns3/traffic-control-layer.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OSPFHelper");

OSPFHelper::OSPFHelper()
{
}

OSPFHelper::OSPFHelper(const OSPFHelper& o)
{
}

OSPFHelper*
OSPFHelper::Copy(void) const
{
    return new OSPFHelper(*this);
}

Ptr<Ipv4RoutingProtocol>
OSPFHelper::Create(Ptr<Node> node) const
{
    NS_LOG_LOGIC("Adding DGRRouter interface to node " << node->GetId());
    // install DGRv2 Queue to netdevices

    // install DGR router to node.
    Ptr<OSPFRouter> router = CreateObject<OSPFRouter>();
    node->AggregateObject(router);

    NS_LOG_LOGIC("Adding RomamRouting Protocol to node " << node->GetId());
    Ptr<OSPFRouting> routing = CreateObject<OSPFRouting>();
    router->SetRoutingProtocol(routing);

    return routing;
}

void
OSPFHelper::PopulateRoutingTables(void)
{
    std::clock_t t;
    t = clock();
    RouteManager::BuildLSDB();
    RouteManager::InitializeDijkstraRoutes();
    t = clock() - t;
    uint32_t time_init_ms = 1000000.0 * t / CLOCKS_PER_SEC;
    std::cout << "CPU time used for Romam Routing Protocol Init: " << time_init_ms << " ms\n";
}

void
OSPFHelper::RecomputeRoutingTables(void)
{
    RouteManager::DeleteRoutes();
    RouteManager::BuildLSDB();
    RouteManager::InitializeDijkstraRoutes();
}

} // namespace ns3
