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

#include "lsa.h"
#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LinkStateAdvertisment");

// ---------------------------------------------------------------------------
//
// LinkRecord Implementation
//
// ---------------------------------------------------------------------------

LinkRecord::LinkRecord ()
  :
    m_linkId ("0.0.0.0"),
    m_linkData ("0.0.0.0"),
    m_linkType (Unknown),
    m_metric (0)
{
  NS_LOG_FUNCTION (this);
}

LinkRecord::LinkRecord (
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

LinkRecord::~LinkRecord ()
{
  NS_LOG_FUNCTION (this);
}

Ipv4Address
LinkRecord::GetLinkId (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkId;
}

void
LinkRecord::SetLinkId (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_linkId = addr;
}

Ipv4Address
LinkRecord::GetLinkData (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkData;
}

void
LinkRecord::SetLinkData (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_linkData = addr;
}

LinkRecord::LinkType
LinkRecord::GetLinkType (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkType;
}

void
LinkRecord::SetLinkType (
  LinkRecord::LinkType linkType)
{
  NS_LOG_FUNCTION (this << linkType);
  m_linkType = linkType;
}

uint16_t
LinkRecord::GetMetric (void) const
{
  NS_LOG_FUNCTION (this);
  return m_metric;
}

void
LinkRecord::SetMetric (uint16_t metric)
{
  NS_LOG_FUNCTION (this << metric);
  m_metric = metric;
}

// ---------------------------------------------------------------------------
//
// LSA Implementation
//
// ---------------------------------------------------------------------------

LSA::LSA()
  : 
    m_lsType (LSA::Unknown),
    m_linkStateId ("0.0.0.0"),
    m_advertisingRtr ("0.0.0.0"),
    m_linkRecords (),
    m_networkLSANetworkMask ("0.0.0.0"),
    m_attachedRouters (),
    m_status (LSA::LSA_SPF_NOT_EXPLORED),
    m_node_id (0)
{
  NS_LOG_FUNCTION (this);
}

LSA::LSA (
  LSA::SPFStatus status,
  Ipv4Address linkStateId, 
  Ipv4Address advertisingRtr)
  :
    m_lsType (LSA::Unknown),
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

LSA::LSA (LSA& lsa)
  : m_lsType (lsa.m_lsType), m_linkStateId (lsa.m_linkStateId),
    m_advertisingRtr (lsa.m_advertisingRtr),
    m_networkLSANetworkMask (lsa.m_networkLSANetworkMask),
    m_status (lsa.m_status),
    m_node_id (lsa.m_node_id)
{
  NS_LOG_FUNCTION (this << &lsa);
  NS_ASSERT_MSG (IsEmpty (),
                 "LSA::LSA (): Non-empty LSA in constructor");
  CopyLinkRecords (lsa);
}

LSA&
LSA::operator= (const LSA& lsa)
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
LSA::CopyLinkRecords (const LSA& lsa)
{
  NS_LOG_FUNCTION (this << &lsa);
  for (ListOfLinkRecords_t::const_iterator i = lsa.m_linkRecords.begin ();
       i != lsa.m_linkRecords.end (); 
       i++)
    {
      LinkRecord *pSrc = *i;
      LinkRecord *pDst = new LinkRecord;

      pDst->SetLinkType (pSrc->GetLinkType ());
      pDst->SetLinkId (pSrc->GetLinkId ());
      pDst->SetLinkData (pSrc->GetLinkData ());
      pDst->SetMetric (pSrc->GetMetric ());

      m_linkRecords.push_back (pDst);
      pDst = 0;
    }

  m_attachedRouters = lsa.m_attachedRouters;
}

LSA::~LSA()
{
  NS_LOG_FUNCTION (this);
  ClearLinkRecords ();
}

void
LSA::ClearLinkRecords (void)
{
  NS_LOG_FUNCTION (this);
  for ( ListOfLinkRecords_t::iterator i = m_linkRecords.begin ();
        i != m_linkRecords.end (); 
        i++)
    {
      NS_LOG_LOGIC ("Free link record");

      LinkRecord *p = *i;
      delete p;
      p = 0;

      *i = 0;
    }
  NS_LOG_LOGIC ("Clear list");
  m_linkRecords.clear ();
}

uint32_t
LSA::AddLinkRecord (LinkRecord* lr)
{
  NS_LOG_FUNCTION (this << lr);
  m_linkRecords.push_back (lr);
  return m_linkRecords.size ();
}

uint32_t
LSA::GetNLinkRecords (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkRecords.size ();
}

LinkRecord *
LSA::GetLinkRecord (uint32_t n) const
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
  NS_ASSERT_MSG (false, "LSA::GetLinkRecord (): invalid index");
  return 0;
}

bool
LSA::IsEmpty (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkRecords.size () == 0;
}

LSA::LSType
LSA::GetLSType (void) const
{
  NS_LOG_FUNCTION (this);
  return m_lsType;
}

void
LSA::SetLSType (LSA::LSType typ) 
{
  NS_LOG_FUNCTION (this << typ);
  m_lsType = typ;
}

Ipv4Address
LSA::GetLinkStateId (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkStateId;
}

void
LSA::SetLinkStateId (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_linkStateId = addr;
}

Ipv4Address
LSA::GetAdvertisingRouter (void) const
{
  NS_LOG_FUNCTION (this);
  return m_advertisingRtr;
}

void
LSA::SetAdvertisingRouter (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_advertisingRtr = addr;
}

void
LSA::SetNetworkLSANetworkMask (Ipv4Mask mask)
{
  NS_LOG_FUNCTION (this << mask);
  m_networkLSANetworkMask = mask;
}

Ipv4Mask
LSA::GetNetworkLSANetworkMask (void) const
{
  NS_LOG_FUNCTION (this);
  return m_networkLSANetworkMask;
}

LSA::SPFStatus
LSA::GetStatus (void) const
{
  NS_LOG_FUNCTION (this);
  return m_status;
}

uint32_t
LSA::AddAttachedRouter (Ipv4Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_attachedRouters.push_back (addr);
  return m_attachedRouters.size ();
}

uint32_t
LSA::GetNAttachedRouters (void) const
{
  NS_LOG_FUNCTION (this);
  return m_attachedRouters.size (); 
}

Ipv4Address
LSA::GetAttachedRouter (uint32_t n) const
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
  NS_ASSERT_MSG (false, "LSA::GetAttachedRouter (): invalid index");
  return Ipv4Address ("0.0.0.0");
}

void
LSA::SetStatus (LSA::SPFStatus status)
{
  NS_LOG_FUNCTION (this << status);
  m_status = status;
}

Ptr<Node>
LSA::GetNode (void) const
{
  NS_LOG_FUNCTION (this);
  return NodeList::GetNode (m_node_id);
}

void
LSA::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this << node);
  m_node_id = node->GetId ();
}

void
LSA::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << std::endl;
  os << "========== Link State Advertisement ==========" << std::endl;
  os << "m_lsType = " << m_lsType;
  if (m_lsType == LSA::RouterLSA) 
    {
      os << " (LSA::RouterLSA)";
    }
  else if (m_lsType == LSA::NetworkLSA) 
    {
      os << " (LSA::NetworkLSA)";
    }
  else if (m_lsType == LSA::ASExternalLSAs)
    {
      os << " (LSA::ASExternalLSA)";
    }
  else
    {
      os << "(Unknown LSType)";
    }
  os << std::endl;

  os << "m_linkStateId = " << m_linkStateId << " (Router ID)" << std::endl;
  os << "m_advertisingRtr = " << m_advertisingRtr << " (Router ID)" << std::endl;

  if (m_lsType == LSA::RouterLSA) 
    {
      for ( ListOfLinkRecords_t::const_iterator i = m_linkRecords.begin ();
            i != m_linkRecords.end (); 
            i++)
        {
          LinkRecord *p = *i;

          os << "---------- RouterLSA Link Record ----------" << std::endl;
          os << "m_linkType = " << p->m_linkType;
          if (p->m_linkType == LinkRecord::PointToPoint)
            {
              os << " (LinkRecord::PointToPoint)" << std::endl;
              os << "m_linkId = " << p->m_linkId << std::endl;
              os << "m_linkData = " << p->m_linkData << std::endl;
              os << "m_metric = " << p->m_metric << std::endl;
            }
          else if (p->m_linkType == LinkRecord::TransitNetwork)
            {
              os << " (LinkRecord::TransitNetwork)" << std::endl;
              os << "m_linkId = " << p->m_linkId << " (Designated router for network)" << std::endl;
              os << "m_linkData = " << p->m_linkData << " (This router's IP address)" << std::endl;
              os << "m_metric = " << p->m_metric << std::endl;
            }
          else if (p->m_linkType == LinkRecord::StubNetwork)
            {
              os << " (LinkRecord::StubNetwork)" << std::endl;
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
  else if (m_lsType == LSA::NetworkLSA) 
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
  else if (m_lsType == LSA::ASExternalLSAs)
    {
      os << "---------- ASExternalLSA Link Record --------" << std::endl;
      os << "m_linkStateId = " << m_linkStateId << std::endl;
      os << "m_networkLSANetworkMask = " << m_networkLSANetworkMask << std::endl;
    }
  else 
    {
      NS_ASSERT_MSG (0, "Illegal LSA LSType: " << m_lsType);
    }
  os << "========== End Link State Advertisement ==========" << std::endl;
}

std::ostream& operator<< (std::ostream& os, LSA& lsa)
{
  lsa.Print (os);
  return os;
}

} // namespace ns3
