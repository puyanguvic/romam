/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <utility>
#include <vector>
#include <queue>
#include <algorithm>
#include <iostream>
#include "ns3/assert.h"
#include "ns3/fatal-error.h"
#include "ns3/log.h"
#include "ns3/node-list.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-list-routing.h"

#include "lsdb.h"
#include <ctime>
#include <chrono>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DGRRouteManagerImpl");

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param exit the exit node
 * \returns the reference to the output stream
 */
std::ostream& 
operator<< (std::ostream& os, const DGRVertex::NodeExit_t& exit)
{
  os << "(" << exit.first << " ," << exit.second << ")";
  return os;
}

std::ostream& 
operator<< (std::ostream& os, const DGRVertex::ListOfDGRVertex_t& vs)
{
  typedef DGRVertex::ListOfDGRVertex_t::const_iterator CIter_t;
  os << "{";
  for (CIter_t iter = vs.begin (); iter != vs.end ();)
    {
      os << (*iter)->m_vertexId;
      if (++iter != vs.end ()) 
        {
          os << ", ";
        }
      else 
        { 
          break;
        }
    }
  os << "}";
  return os;
}

// ---------------------------------------------------------------------------
//
// DGRVertex Implementation
//
// ---------------------------------------------------------------------------

DGRVertex::DGRVertex () : 
  m_vertexType (VertexUnknown), 
  m_vertexId ("255.255.255.255"), 
  m_lsa (0),
  m_distanceFromRoot (DISTINFINITY), 
  m_rootOif (DISTINFINITY),
  m_nextHop ("0.0.0.0"),
  m_parents (),
  m_children (),
  m_vertexProcessed (false)
{
  NS_LOG_FUNCTION (this);
}

DGRVertex::DGRVertex (DGRRoutingLSA* lsa) : 
  m_vertexId (lsa->GetLinkStateId ()),
  m_lsa (lsa),
  m_distanceFromRoot (DISTINFINITY), 
  m_rootOif (DISTINFINITY),
  m_nextHop ("0.0.0.0"),
  m_parents (),
  m_children (),
  m_vertexProcessed (false)
{
  NS_LOG_FUNCTION (this << lsa);

  if (lsa->GetLSType () == DGRRoutingLSA::RouterLSA) 
    {
      NS_LOG_LOGIC ("Setting m_vertexType to VertexRouter");
      m_vertexType = DGRVertex::VertexRouter;
    }
  else if (lsa->GetLSType () == DGRRoutingLSA::NetworkLSA) 
    { 
      NS_LOG_LOGIC ("Setting m_vertexType to VertexNetwork");
      m_vertexType = DGRVertex::VertexNetwork;
    }
}

DGRVertex::~DGRVertex ()
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("Children vertices - " << m_children);
  NS_LOG_LOGIC ("Parent verteices - " << m_parents);

  // find this node from all its parents and remove the entry of this node
  // from all its parents
  for (ListOfDGRVertex_t::iterator piter = m_parents.begin (); 
       piter != m_parents.end ();
       piter++)
    {
      // remove the current vertex from its parent's children list. Check
      // if the size of the list is reduced, or the child<->parent relation
      // is not bidirectional
      uint32_t orgCount = (*piter)->m_children.size ();
      (*piter)->m_children.remove (this);
      uint32_t newCount = (*piter)->m_children.size ();
      if (orgCount > newCount)
        {
          NS_ASSERT_MSG (orgCount > newCount, "Unable to find the current vertex from its parents --- impossible!");
        }
    }

  // delete children
  while (m_children.size () > 0)
    {
      // pop out children one by one. Some children may disappear 
      // when deleting some other children in the list. As a result,
      // it is necessary to use pop to walk through all children, instead
      // of using iterator.
      //
      // Note that m_children.pop_front () is not necessary as this
      // p is removed from the children list when p is deleted
      DGRVertex* p = m_children.front ();
      // 'p' == 0, this child is already deleted by its other parent
      if (p == 0) continue;
      NS_LOG_LOGIC ("Parent vertex-" << m_vertexId << " deleting its child vertex-" << p->GetVertexId ());
      delete p;
      p = 0;
    }
  m_children.clear ();
  // delete parents
  m_parents.clear ();
  // delete root exit direction
  m_ecmpRootExits.clear ();

  NS_LOG_LOGIC ("Vertex-" << m_vertexId << " completed deleted");
}

void
DGRVertex::SetVertexType (DGRVertex::VertexType type)
{
  NS_LOG_FUNCTION (this << type);
  m_vertexType = type;
}

DGRVertex::VertexType
DGRVertex::GetVertexType (void) const
{
  NS_LOG_FUNCTION (this);
  return m_vertexType;
}

void
DGRVertex::SetVertexId (Ipv4Address id)
{
  NS_LOG_FUNCTION (this << id);
  m_vertexId = id;
}

Ipv4Address
DGRVertex::GetVertexId (void) const
{
  NS_LOG_FUNCTION (this);
  return m_vertexId;
}

void
DGRVertex::SetLSA (DGRRoutingLSA* lsa)
{
  NS_LOG_FUNCTION (this << lsa);
  m_lsa = lsa;
}

DGRRoutingLSA*
DGRVertex::GetLSA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_lsa;
}

void
DGRVertex::SetDistanceFromRoot (uint32_t distance)
{
  NS_LOG_FUNCTION (this << distance);
  m_distanceFromRoot = distance;
}

uint32_t
DGRVertex::GetDistanceFromRoot (void) const
{
  NS_LOG_FUNCTION (this);
  return m_distanceFromRoot;
}

void
DGRVertex::SetParent (DGRVertex* parent)
{
  NS_LOG_FUNCTION (this << parent);

  // always maintain only one parent when using setter/getter methods
  m_parents.clear ();
  m_parents.push_back (parent);
}

DGRVertex*
DGRVertex::GetParent (uint32_t i) const
{
  NS_LOG_FUNCTION (this << i);

  // If the index i is out-of-range, return 0 and do nothing
  if (m_parents.size () <= i)
    {
      NS_LOG_LOGIC ("Index to DGRVertex's parent is out-of-range.");
      return 0;
    }
  ListOfDGRVertex_t::const_iterator iter = m_parents.begin ();
  while (i-- > 0) 
    {
      iter++;
    }
  return *iter;
}

void 
DGRVertex::MergeParent (const DGRVertex* v)
{
  NS_LOG_FUNCTION (this << v);

  NS_LOG_LOGIC ("Before merge, list of parents = " << m_parents);
  // combine the two lists first, and then remove any duplicated after
  m_parents.insert (m_parents.end (), 
                    v->m_parents.begin (), v->m_parents.end ());
  // remove duplication
  m_parents.sort ();
  m_parents.unique ();
  NS_LOG_LOGIC ("After merge, list of parents = " << m_parents);
}

void 
DGRVertex::SetRootExitDirection (Ipv4Address nextHop, int32_t id)
{
  NS_LOG_FUNCTION (this << nextHop << id);

  // always maintain only one root's exit
  m_ecmpRootExits.clear ();
  m_ecmpRootExits.push_back (NodeExit_t (nextHop, id));
  // update the following in order to be backward compatitable with
  // GetNextHop and GetOutgoingInterface methods
  m_nextHop = nextHop;
  m_rootOif = id;
}

void 
DGRVertex::SetRootExitDirection (DGRVertex::NodeExit_t exit)
{
  NS_LOG_FUNCTION (this << exit);
  SetRootExitDirection (exit.first, exit.second);
}

DGRVertex::NodeExit_t
DGRVertex::GetRootExitDirection (uint32_t i) const
{
  NS_LOG_FUNCTION (this << i);
  typedef ListOfNodeExit_t::const_iterator CIter_t;

  NS_ASSERT_MSG (i < m_ecmpRootExits.size (), "Index out-of-range when accessing DGRVertex::m_ecmpRootExits!");
  CIter_t iter = m_ecmpRootExits.begin ();
  while (i-- > 0) { iter++; }

  return *iter;
}

DGRVertex::NodeExit_t 
DGRVertex::GetRootExitDirection () const
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (m_ecmpRootExits.size () <= 1, "Assumed there is at most one exit from the root to this vertex");
  return GetRootExitDirection (0);
}

void 
DGRVertex::MergeRootExitDirections (const DGRVertex* vertex)
{
  NS_LOG_FUNCTION (this << vertex);

  // obtain the external list of exit directions
  //
  // Append the external list into 'this' and remove duplication afterward
  const ListOfNodeExit_t& extList = vertex->m_ecmpRootExits;
  m_ecmpRootExits.insert (m_ecmpRootExits.end (), 
                          extList.begin (), extList.end ());
  m_ecmpRootExits.sort ();
  m_ecmpRootExits.unique ();
}

void 
DGRVertex::InheritAllRootExitDirections (const DGRVertex* vertex)
{
  NS_LOG_FUNCTION (this << vertex);

  // discard all exit direction currently associated with this vertex,
  // and copy all the exit directions from the given vertex
  if (m_ecmpRootExits.size () > 0)
    {
      NS_LOG_WARN ("x root exit directions in this vertex are going to be discarded");
    }
  m_ecmpRootExits.clear ();
  m_ecmpRootExits.insert (m_ecmpRootExits.end (), 
                          vertex->m_ecmpRootExits.begin (), vertex->m_ecmpRootExits.end ());
}

uint32_t 
DGRVertex::GetNRootExitDirections () const
{
  NS_LOG_FUNCTION (this);
  return m_ecmpRootExits.size ();
}

uint32_t 
DGRVertex::GetNChildren (void) const
{
  NS_LOG_FUNCTION (this);
  return m_children.size ();
}

DGRVertex*
DGRVertex::GetChild (uint32_t n) const
{
  NS_LOG_FUNCTION (this << n);
  uint32_t j = 0;

  for ( ListOfDGRVertex_t::const_iterator i = m_children.begin ();
        i != m_children.end ();
        i++, j++)
    {
      if (j == n)
        {
          return *i;
        }
    }
  NS_ASSERT_MSG (false, "Index <n> out of range.");
  return 0;
}

uint32_t
DGRVertex::AddChild (DGRVertex* child)
{
  NS_LOG_FUNCTION (this << child);
  m_children.push_back (child);
  return m_children.size ();
}

void 
DGRVertex::SetVertexProcessed (bool value)
{
  NS_LOG_FUNCTION (this << value);
  m_vertexProcessed = value;
}

bool 
DGRVertex::IsVertexProcessed (void) const
{
  NS_LOG_FUNCTION (this);
  return m_vertexProcessed;
}

void
DGRVertex::ClearVertexProcessed (void)
{
  NS_LOG_FUNCTION (this);
  for (uint32_t i = 0; i < this->GetNChildren (); i++)
    {
      this->GetChild (i)->ClearVertexProcessed ();
    }
  this->SetVertexProcessed (false);
}

// ---------------------------------------------------------------------------
//
// DGRRouteManagerLSDB Implementation
//
// ---------------------------------------------------------------------------

DGRRouteManagerLSDB::DGRRouteManagerLSDB ()
  :
    m_database (),
    m_extdatabase ()
{
  NS_LOG_FUNCTION (this);
}

DGRRouteManagerLSDB::~DGRRouteManagerLSDB ()
{
  NS_LOG_FUNCTION (this);
  LSDBMap_t::iterator i;
  for (i= m_database.begin (); i!= m_database.end (); i++)
    {
      NS_LOG_LOGIC ("free LSA");
      DGRRoutingLSA* temp = i->second;
      delete temp;
    }
  for (uint32_t j = 0; j < m_extdatabase.size (); j++)
    {
      NS_LOG_LOGIC ("free ASexternalLSA");
      DGRRoutingLSA* temp = m_extdatabase.at (j);
      delete temp;
    }
  NS_LOG_LOGIC ("clear map");
  m_database.clear ();
}

void
DGRRouteManagerLSDB::Initialize ()
{
  NS_LOG_FUNCTION (this);
  LSDBMap_t::iterator i;
  for (i= m_database.begin (); i!= m_database.end (); i++)
    {
      DGRRoutingLSA* temp = i->second;
      temp->SetStatus (DGRRoutingLSA::LSA_SPF_NOT_EXPLORED);
    }
}

void
DGRRouteManagerLSDB::Insert (Ipv4Address addr, DGRRoutingLSA* lsa)
{
  NS_LOG_FUNCTION (this << addr << lsa);
  if (lsa->GetLSType () == DGRRoutingLSA::ASExternalLSAs) 
    {
      m_extdatabase.push_back (lsa);
    } 
  else
    {
      m_database.insert (LSDBPair_t (addr, lsa));
    }
}

DGRRoutingLSA*
DGRRouteManagerLSDB::GetExtLSA (uint32_t index) const
{
  NS_LOG_FUNCTION (this << index);
  return m_extdatabase.at (index);
}

uint32_t
DGRRouteManagerLSDB::GetNumExtLSAs () const
{
  NS_LOG_FUNCTION (this);
  return m_extdatabase.size ();
}

DGRRoutingLSA*
DGRRouteManagerLSDB::GetLSA (Ipv4Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
//
// Look up an LSA by its address.
//
  LSDBMap_t::const_iterator i;
  for (i= m_database.begin (); i!= m_database.end (); i++)
    {
      if (i->first == addr)
        {
          return i->second;
        }
    }
  return 0;
}

DGRRoutingLSA*
DGRRouteManagerLSDB::GetLSAByLinkData (Ipv4Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
//
// Look up an LSA by its address.
//
  LSDBMap_t::const_iterator i;
  for (i= m_database.begin (); i!= m_database.end (); i++)
    {
      DGRRoutingLSA* temp = i->second;
// Iterate among temp's Link Records
      for (uint32_t j = 0; j < temp->GetNLinkRecords (); j++)
        {
          DGRRoutingLinkRecord *lr = temp->GetLinkRecord (j);
          if ( lr->GetLinkType () == DGRRoutingLinkRecord::TransitNetwork &&
               lr->GetLinkData () == addr)
            {
              return temp;
            }
        }
    }
  return 0;
}

} // namespace ns3
