/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ddr-router.h"

#include "../ddr-routing.h"

#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DDRRouter");

TypeId
DDRRouter::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::DDRRouter").SetParent<RomamRouter>().SetGroupName("romam");
    return tid;
}

DDRRouter::DDRRouter()
{
    NS_LOG_FUNCTION(this);
}

DDRRouter::~DDRRouter()
{
    NS_LOG_FUNCTION(this);
}

void
DDRRouter::SetRoutingProtocol(Ptr<RomamRouting> routing)
{
    NS_LOG_FUNCTION(this << routing);
    m_routingProtocol = DynamicCast<DDRRouting>(routing);
}

Ptr<RomamRouting>
DDRRouter::GetRoutingProtocol(void)
{
    NS_LOG_FUNCTION(this);
    return m_routingProtocol;
}

void
DDRRouter::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_routingProtocol = 0;
    RomamRouter::DoDispose();
}

} // namespace ns3
