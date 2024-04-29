/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "dgr-router.h"

#include "../dgr-routing.h"

#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DGRRouter");

TypeId
DGRRouter::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::DGRRouter").SetParent<RomamRouter>().SetGroupName("romam");
    return tid;
}

DGRRouter::DGRRouter()
{
    NS_LOG_FUNCTION(this);
}

DGRRouter::~DGRRouter()
{
    NS_LOG_FUNCTION(this);
}

void
DGRRouter::SetRoutingProtocol(Ptr<RomamRouting> routing)
{
    NS_LOG_FUNCTION(this << routing);
    m_routingProtocol = DynamicCast<DGRRouting>(routing);
}

Ptr<RomamRouting>
DGRRouter::GetRoutingProtocol(void)
{
    NS_LOG_FUNCTION(this);
    return m_routingProtocol;
}

void
DGRRouter::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_routingProtocol = 0;
    RomamRouter::DoDispose();
}

} // namespace ns3
