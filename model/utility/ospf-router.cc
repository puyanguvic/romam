/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ospf-router.h"

#include "../ospf-routing.h"

#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OSPFRouter");

TypeId
OSPFRouter::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::OSPFRouter").SetParent<Object>().SetGroupName("romam");
    return tid;
}

OSPFRouter::OSPFRouter()
{
    NS_LOG_FUNCTION(this);
}

OSPFRouter::~OSPFRouter()
{
    NS_LOG_FUNCTION(this);
}

void
OSPFRouter::SetRoutingProtocol(Ptr<Ipv4RoutingProtocol> routing)
{
    NS_LOG_FUNCTION(this << routing);
    m_routingProtocol = DynamicCast<OSPFRouting>(routing);
}

Ptr<Ipv4RoutingProtocol>
OSPFRouter::GetRoutingProtocol(void)
{
    NS_LOG_FUNCTION(this);
    return m_routingProtocol;
}

void
OSPFRouter::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_routingProtocol = 0;
    RomamRouter::DoDispose();
}

} // namespace ns3
