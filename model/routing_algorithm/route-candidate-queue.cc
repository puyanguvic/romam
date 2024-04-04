/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 */

#include <algorithm>
#include <iostream>
#include "ns3/log.h"
#include "ns3/assert.h"
#include "route-candidate-queue.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RouteCandidateQueue");

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param t the Vertex type
 * \returns the reference to the output stream
 */
std::ostream&
operator<< (std::ostream& os, const Vertex::VertexType& t)
{
  switch (t)
    {
    case Vertex::VertexRouter:  os << "router"; break;
    case Vertex::VertexNetwork: os << "network"; break;
    default:                       os << "unknown"; break;
    };
  return os;
}

std::ostream& 
operator<< (std::ostream& os, const RouteCandidateQueue& q)
{
  typedef RouteCandidateQueue::DGRCandidateList_t List_t;
  typedef List_t::const_iterator CIter_t;
  const RouteCandidateQueue::DGRCandidateList_t& list = q.m_candidates;

  os << "*** CandidateQueue Begin (<id, distance, LSA-type>) ***" << std::endl;
  for (CIter_t iter = list.begin (); iter != list.end (); iter++)
    {
      os << "<" 
      << (*iter)->GetVertexId () << ", "
      << (*iter)->GetDistanceFromRoot () << ", "
      << (*iter)->GetVertexType () << ">" << std::endl;
    }
  os << "*** CandidateQueue End ***";
  return os;
}

RouteCandidateQueue::RouteCandidateQueue()
  : m_candidates ()
{
  NS_LOG_FUNCTION (this);
}

RouteCandidateQueue::~RouteCandidateQueue()
{
  NS_LOG_FUNCTION (this);
  Clear ();
}

void
RouteCandidateQueue::Clear (void)
{
  NS_LOG_FUNCTION (this);
  while (!m_candidates.empty ())
    {
      Vertex *p = Pop ();
      delete p;
      p = 0;
    }
}

void
RouteCandidateQueue::Push (Vertex *vNew)
{
  NS_LOG_FUNCTION (this << vNew);

  DGRCandidateList_t::iterator i = std::upper_bound (
      m_candidates.begin (), m_candidates.end (), vNew,
      &RouteCandidateQueue::CompareVertex
      );
  m_candidates.insert (i, vNew);
}

Vertex *
RouteCandidateQueue::Pop (void)
{
  NS_LOG_FUNCTION (this);
  if (m_candidates.empty ())
    {
      return 0;
    }

  Vertex *v = m_candidates.front ();
  m_candidates.pop_front ();
  return v;
}

Vertex *
RouteCandidateQueue::Top (void) const
{
  NS_LOG_FUNCTION (this);
  if (m_candidates.empty ())
    {
      return 0;
    }

  return m_candidates.front ();
}

bool
RouteCandidateQueue::Empty (void) const
{
  NS_LOG_FUNCTION (this);
  return m_candidates.empty ();
}

uint32_t
RouteCandidateQueue::Size (void) const
{
  NS_LOG_FUNCTION (this);
  return m_candidates.size ();
}

Vertex *
RouteCandidateQueue::Find (const Ipv4Address addr) const
{
  NS_LOG_FUNCTION (this);
  DGRCandidateList_t::const_iterator i = m_candidates.begin ();

  for (; i != m_candidates.end (); i++)
    {
      Vertex *v = *i;
      if (v->GetVertexId () == addr)
        {
          return v;
        }
    }

  return 0;
}

void
RouteCandidateQueue::Reorder (void)
{
  NS_LOG_FUNCTION (this);

  m_candidates.sort (&RouteCandidateQueue::CompareVertex);
  NS_LOG_LOGIC ("After reordering the CandidateQueue");
  NS_LOG_LOGIC (*this);
}

/*
 * In this implementation, Vertex follows the ordering where
 * a vertex is ranked first if its GetDistanceFromRoot () is smaller;
 * In case of a tie, NetworkLSA is always ranked before RouterLSA.
 *
 * This ordering is necessary for implementing ECMP
 */
bool 
RouteCandidateQueue::CompareVertex (const Vertex* v1, const Vertex* v2)
{
  NS_LOG_FUNCTION (&v1 << &v2);

  bool result = false;
  if (v1->GetDistanceFromRoot () < v2->GetDistanceFromRoot ())
    {
      result = true;
    }
  else if (v1->GetDistanceFromRoot () == v2->GetDistanceFromRoot ())
    {
      if (v1->GetVertexType () == Vertex::VertexNetwork 
          && v2->GetVertexType () == Vertex::VertexRouter)
        {
          result = true;
        }
    }
  return result;
}

} // namespace ns3
