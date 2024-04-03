/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/abort.h"
#include "ns3/channel.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/ipv4.h"
#include "ns3/bridge-net-device.h"
#include "ns3/loopback-net-device.h"

#include "../ipv4-dgr-routing.h"

#include "lsa.h"
#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DGRRoutingLinkRecord");

// ---------------------------------------------------------------------------
//
// DGRRoutingLinkRecord Implementation
//
// ---------------------------------------------------------------------------

DGRRoutingLinkRecord::DGRRoutingLinkRecord ()
  :
    m_linkId ("0.0.0.0"),
    m_linkData ("0.0.0.0"),
    m_linkType (Unknown),
    m_metric (0)
{
  NS_LOG_FUNCTION (this);
}

DGRRoutingLinkRecord::DGRRoutingLinkRecord (
  LinkType    linkType, 
  Ipv4Address linkId, 
  Ipv4Address linkData, 
  uint16_t    metric)
  :
    m_linkId (linkId),
    m_linkData (linkData),
    m_linkType (linkType),
    m_metric (metric)
{
  NS_LOG_FUNCTION (this << linkType << linkId << linkData << metric);
}

DGRRoutingLinkRecord::~DGRRoutingLinkRecord ()
{
  NS_LOG_FUNCTION (this);
}

Ipv4Address
DGRRoutingLinkRecord::GetLinkId (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkId;
}

void
DGRRoutingLinkRecord::SetLinkId (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_linkId = addr;
}

Ipv4Address
DGRRoutingLinkRecord::GetLinkData (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkData;
}

void
DGRRoutingLinkRecord::SetLinkData (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_linkData = addr;
}

DGRRoutingLinkRecord::LinkType
DGRRoutingLinkRecord::GetLinkType (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkType;
}

void
DGRRoutingLinkRecord::SetLinkType (
  DGRRoutingLinkRecord::LinkType linkType)
{
  NS_LOG_FUNCTION (this << linkType);
  m_linkType = linkType;
}

uint16_t
DGRRoutingLinkRecord::GetMetric (void) const
{
  NS_LOG_FUNCTION (this);
  return m_metric;
}

void
DGRRoutingLinkRecord::SetMetric (uint16_t metric)
{
  NS_LOG_FUNCTION (this << metric);
  m_metric = metric;
}

// ---------------------------------------------------------------------------
//
// DGRRoutingLSA Implementation
//
// ---------------------------------------------------------------------------

DGRRoutingLSA::DGRRoutingLSA()
  : 
    m_lsType (DGRRoutingLSA::Unknown),
    m_linkStateId ("0.0.0.0"),
    m_advertisingRtr ("0.0.0.0"),
    m_linkRecords (),
    m_networkLSANetworkMask ("0.0.0.0"),
    m_attachedRouters (),
    m_status (DGRRoutingLSA::LSA_SPF_NOT_EXPLORED),
    m_node_id (0)
{
  NS_LOG_FUNCTION (this);
}

DGRRoutingLSA::DGRRoutingLSA (
  DGRRoutingLSA::SPFStatus status,
  Ipv4Address linkStateId, 
  Ipv4Address advertisingRtr)
  :
    m_lsType (DGRRoutingLSA::Unknown),
    m_linkStateId (linkStateId),
    m_advertisingRtr (advertisingRtr),
    m_linkRecords (),
    m_networkLSANetworkMask ("0.0.0.0"),
    m_attachedRouters (),
    m_status (status),
    m_node_id (0)
{
  NS_LOG_FUNCTION (this << status << linkStateId << advertisingRtr);
}

DGRRoutingLSA::DGRRoutingLSA (DGRRoutingLSA& lsa)
  : m_lsType (lsa.m_lsType), m_linkStateId (lsa.m_linkStateId),
    m_advertisingRtr (lsa.m_advertisingRtr),
    m_networkLSANetworkMask (lsa.m_networkLSANetworkMask),
    m_status (lsa.m_status),
    m_node_id (lsa.m_node_id)
{
  NS_LOG_FUNCTION (this << &lsa);
  NS_ASSERT_MSG (IsEmpty (),
                 "DGRRoutingLSA::DGRRoutingLSA (): Non-empty LSA in constructor");
  CopyLinkRecords (lsa);
}

DGRRoutingLSA&
DGRRoutingLSA::operator= (const DGRRoutingLSA& lsa)
{
  NS_LOG_FUNCTION (this << &lsa);
  m_lsType = lsa.m_lsType;
  m_linkStateId = lsa.m_linkStateId;
  m_advertisingRtr = lsa.m_advertisingRtr;
  m_networkLSANetworkMask = lsa.m_networkLSANetworkMask, 
  m_status = lsa.m_status;
  m_node_id = lsa.m_node_id;

  ClearLinkRecords ();
  CopyLinkRecords (lsa);
  return *this;
}

void
DGRRoutingLSA::CopyLinkRecords (const DGRRoutingLSA& lsa)
{
  NS_LOG_FUNCTION (this << &lsa);
  for (ListOfLinkRecords_t::const_iterator i = lsa.m_linkRecords.begin ();
       i != lsa.m_linkRecords.end (); 
       i++)
    {
      DGRRoutingLinkRecord *pSrc = *i;
      DGRRoutingLinkRecord *pDst = new DGRRoutingLinkRecord;

      pDst->SetLinkType (pSrc->GetLinkType ());
      pDst->SetLinkId (pSrc->GetLinkId ());
      pDst->SetLinkData (pSrc->GetLinkData ());
      pDst->SetMetric (pSrc->GetMetric ());

      m_linkRecords.push_back (pDst);
      pDst = 0;
    }

  m_attachedRouters = lsa.m_attachedRouters;
}

DGRRoutingLSA::~DGRRoutingLSA()
{
  NS_LOG_FUNCTION (this);
  ClearLinkRecords ();
}

void
DGRRoutingLSA::ClearLinkRecords (void)
{
  NS_LOG_FUNCTION (this);
  for ( ListOfLinkRecords_t::iterator i = m_linkRecords.begin ();
        i != m_linkRecords.end (); 
        i++)
    {
      NS_LOG_LOGIC ("Free link record");

      DGRRoutingLinkRecord *p = *i;
      delete p;
      p = 0;

      *i = 0;
    }
  NS_LOG_LOGIC ("Clear list");
  m_linkRecords.clear ();
}

uint32_t
DGRRoutingLSA::AddLinkRecord (DGRRoutingLinkRecord* lr)
{
  NS_LOG_FUNCTION (this << lr);
  m_linkRecords.push_back (lr);
  return m_linkRecords.size ();
}

uint32_t
DGRRoutingLSA::GetNLinkRecords (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkRecords.size ();
}

DGRRoutingLinkRecord *
DGRRoutingLSA::GetLinkRecord (uint32_t n) const
{
  NS_LOG_FUNCTION (this << n);
  uint32_t j = 0;
  for ( ListOfLinkRecords_t::const_iterator i = m_linkRecords.begin ();
        i != m_linkRecords.end (); 
        i++, j++)
    {
      if (j == n) 
        {
          return *i;
        }
    }
  NS_ASSERT_MSG (false, "DGRRoutingLSA::GetLinkRecord (): invalid index");
  return 0;
}

bool
DGRRoutingLSA::IsEmpty (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkRecords.size () == 0;
}

DGRRoutingLSA::LSType
DGRRoutingLSA::GetLSType (void) const
{
  NS_LOG_FUNCTION (this);
  return m_lsType;
}

void
DGRRoutingLSA::SetLSType (DGRRoutingLSA::LSType typ) 
{
  NS_LOG_FUNCTION (this << typ);
  m_lsType = typ;
}

Ipv4Address
DGRRoutingLSA::GetLinkStateId (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkStateId;
}

void
DGRRoutingLSA::SetLinkStateId (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_linkStateId = addr;
}

Ipv4Address
DGRRoutingLSA::GetAdvertisingRouter (void) const
{
  NS_LOG_FUNCTION (this);
  return m_advertisingRtr;
}

void
DGRRoutingLSA::SetAdvertisingRouter (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_advertisingRtr = addr;
}

void
DGRRoutingLSA::SetNetworkLSANetworkMask (Ipv4Mask mask)
{
  NS_LOG_FUNCTION (this << mask);
  m_networkLSANetworkMask = mask;
}

Ipv4Mask
DGRRoutingLSA::GetNetworkLSANetworkMask (void) const
{
  NS_LOG_FUNCTION (this);
  return m_networkLSANetworkMask;
}

DGRRoutingLSA::SPFStatus
DGRRoutingLSA::GetStatus (void) const
{
  NS_LOG_FUNCTION (this);
  return m_status;
}

uint32_t
DGRRoutingLSA::AddAttachedRouter (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_attachedRouters.push_back (addr);
  return m_attachedRouters.size ();
}

uint32_t
DGRRoutingLSA::GetNAttachedRouters (void) const
{
  NS_LOG_FUNCTION (this);
  return m_attachedRouters.size (); 
}

Ipv4Address
DGRRoutingLSA::GetAttachedRouter (uint32_t n) const
{
  NS_LOG_FUNCTION (this << n);
  uint32_t j = 0;
  for ( ListOfAttachedRouters_t::const_iterator i = m_attachedRouters.begin ();
        i != m_attachedRouters.end (); 
        i++, j++)
    {
      if (j == n) 
        {
          return *i;
        }
    }
  NS_ASSERT_MSG (false, "DGRRoutingLSA::GetAttachedRouter (): invalid index");
  return Ipv4Address ("0.0.0.0");
}

void
DGRRoutingLSA::SetStatus (DGRRoutingLSA::SPFStatus status)
{
  NS_LOG_FUNCTION (this << status);
  m_status = status;
}

Ptr<Node>
DGRRoutingLSA::GetNode (void) const
{
  NS_LOG_FUNCTION (this);
  return NodeList::GetNode (m_node_id);
}

void
DGRRoutingLSA::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << node);
  m_node_id = node->GetId ();
}

void
DGRRoutingLSA::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << std::endl;
  os << "========== DGR Routing LSA ==========" << std::endl;
  os << "m_lsType = " << m_lsType;
  if (m_lsType == DGRRoutingLSA::RouterLSA) 
    {
      os << " (DGRRoutingLSA::RouterLSA)";
    }
  else if (m_lsType == DGRRoutingLSA::NetworkLSA) 
    {
      os << " (DGRRoutingLSA::NetworkLSA)";
    }
  else if (m_lsType == DGRRoutingLSA::ASExternalLSAs)
    {
      os << " (DGRRoutingLSA::ASExternalLSA)";
    }
  else
    {
      os << "(Unknown LSType)";
    }
  os << std::endl;

  os << "m_linkStateId = " << m_linkStateId << " (Router ID)" << std::endl;
  os << "m_advertisingRtr = " << m_advertisingRtr << " (Router ID)" << std::endl;

  if (m_lsType == DGRRoutingLSA::RouterLSA) 
    {
      for ( ListOfLinkRecords_t::const_iterator i = m_linkRecords.begin ();
            i != m_linkRecords.end (); 
            i++)
        {
          DGRRoutingLinkRecord *p = *i;

          os << "---------- RouterLSA Link Record ----------" << std::endl;
          os << "m_linkType = " << p->m_linkType;
          if (p->m_linkType == DGRRoutingLinkRecord::PointToPoint)
            {
              os << " (DGRRoutingLinkRecord::PointToPoint)" << std::endl;
              os << "m_linkId = " << p->m_linkId << std::endl;
              os << "m_linkData = " << p->m_linkData << std::endl;
              os << "m_metric = " << p->m_metric << std::endl;
            }
          else if (p->m_linkType == DGRRoutingLinkRecord::TransitNetwork)
            {
              os << " (DGRRoutingLinkRecord::TransitNetwork)" << std::endl;
              os << "m_linkId = " << p->m_linkId << " (Designated router for network)" << std::endl;
              os << "m_linkData = " << p->m_linkData << " (This router's IP address)" << std::endl;
              os << "m_metric = " << p->m_metric << std::endl;
            }
          else if (p->m_linkType == DGRRoutingLinkRecord::StubNetwork)
            {
              os << " (DGRRoutingLinkRecord::StubNetwork)" << std::endl;
              os << "m_linkId = " << p->m_linkId << " (Network number of attached network)" << std::endl;
              os << "m_linkData = " << p->m_linkData << " (Network mask of attached network)" << std::endl;
              os << "m_metric = " << p->m_metric << std::endl;
            }
          else
            {
              os << " (Unknown LinkType)" << std::endl;
              os << "m_linkId = " << p->m_linkId << std::endl;
              os << "m_linkData = " << p->m_linkData << std::endl;
              os << "m_metric = " << p->m_metric << std::endl;
            }
          os << "---------- End RouterLSA Link Record ----------" << std::endl;
        }
    }
  else if (m_lsType == DGRRoutingLSA::NetworkLSA) 
    {
      os << "---------- NetworkLSA Link Record ----------" << std::endl;
      os << "m_networkLSANetworkMask = " << m_networkLSANetworkMask << std::endl;
      for ( ListOfAttachedRouters_t::const_iterator i = m_attachedRouters.begin (); i != m_attachedRouters.end (); i++)
        {
          Ipv4Address p = *i;
          os << "attachedRouter = " << p << std::endl;
        }
      os << "---------- End NetworkLSA Link Record ----------" << std::endl;
    }
  else if (m_lsType == DGRRoutingLSA::ASExternalLSAs)
    {
      os << "---------- ASExternalLSA Link Record --------" << std::endl;
      os << "m_linkStateId = " << m_linkStateId << std::endl;
      os << "m_networkLSANetworkMask = " << m_networkLSANetworkMask << std::endl;
    }
  else 
    {
      NS_ASSERT_MSG (0, "Illegal LSA LSType: " << m_lsType);
    }
  os << "========== End Global Routing LSA ==========" << std::endl;
}

std::ostream& operator<< (std::ostream& os, DGRRoutingLSA& lsa)
{
  lsa.Print (os);
  return os;
}

} // namespace ns3
