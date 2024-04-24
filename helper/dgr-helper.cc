/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "dgr-helper.h"

#include "ns3/ipv4-list-routing.h"
#include "ns3/log.h"
#include "ns3/traffic-control-layer.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DGRRoutingHelper");

DGRHelper::DGRHelper()
{
}

DGRHelper::DGRHelper(const DGRHelper& o)
{
}

DGRHelper*
DGRHelper::Copy(void) const
{
    return new DGRHelper(*this);
}

Ptr<Ipv4RoutingProtocol>
DGRHelper::Create(Ptr<Node> node) const
{
    NS_LOG_LOGIC("Adding DGRRouter interface to node " << node->GetId());
    // install DGRv2 Queue to netdevices

    // install DGR router to node.
    Ptr<DGRRouter> router = CreateObject<DGRRouter>();
    node->AggregateObject(router);

    NS_LOG_LOGIC("Adding DGRRouting Protocol to node " << node->GetId());
    Ptr<DGRRouting> routing = CreateObject<DGRRouting>();
    router->SetRoutingProtocol(routing);

    return routing;
}

void
DGRHelper::PopulateRoutingTables(void)
{
    std::clock_t t;
    t = clock();
    RouteManager::BuildLSDB();
    RouteManager::InitializeSPFRoutes();

    t = clock() - t;
    uint32_t time_init_ms = 1000000.0 * t / CLOCKS_PER_SEC;
    std::cout << "CPU time used for DGR Init: " << time_init_ms << " ms\n";
}

void
DGRHelper::RecomputeRoutingTables(void)
{
    RouteManager::DeleteRoutes();
    RouteManager::BuildLSDB();
    RouteManager::InitializeSPFRoutes();
}

QueueDiscContainer
DGRHelper::Install(Ptr<Node> node)
{
    NetDeviceContainer container;
    for (uint32_t i = 0; i < node->GetNDevices(); i++)
    {
        container.Add(node->GetDevice(i));
    }
    return Install(container);
}

QueueDiscContainer
DGRHelper::Install(NetDeviceContainer c)
{
    QueueDiscContainer container;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        container.Add(Install(*i));
    }
    return container;
}

QueueDiscContainer
DGRHelper::Install(Ptr<NetDevice> d)
{
    QueueDiscContainer container;
    // A TrafficControlLayer object is aggregated by the InternetStackHelper, but check
    // anyway because a queue disc has no effect without a TrafficControlLayer object
    Ptr<TrafficControlLayer> tc = d->GetNode()->GetObject<TrafficControlLayer>();
    NS_ASSERT(tc);

    // Generate the DGRv2Qeueu Object
    ObjectFactory queueFactory;
    queueFactory.SetTypeId("ns3::DGRv2QueueDisc");
    Ptr<DGRv2QueueDisc> qdisc = queueFactory.Create<DGRv2QueueDisc>();
    tc->SetRootQueueDiscOnDevice(d, qdisc);
    container.Add(qdisc);
    return container;
}

} // namespace ns3