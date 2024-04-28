/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "octopus-router.h"

#include "../octopus-routing.h"

#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OctopusRouter");

TypeId
OctopusRouter::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::OctopusRouter").SetParent<Object>().SetGroupName("romam");
    return tid;
}

OctopusRouter::OctopusRouter()
{
    NS_LOG_FUNCTION(this);
}

OctopusRouter::~OctopusRouter()
{
    NS_LOG_FUNCTION(this);
}

void
OctopusRouter::SetRoutingProtocol(Ptr<RomamRouting> routing)
{
    NS_LOG_FUNCTION(this << routing);
    m_routingProtocol = DynamicCast<OctopusRouting>(routing);
}

Ptr<RomamRouting>
OctopusRouter::GetRoutingProtocol(void)
{
    NS_LOG_FUNCTION(this);
    return m_routingProtocol;
}

void
OctopusRouter::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_routingProtocol = 0;
    RomamRouter::DoDispose();
}

} // namespace ns3
