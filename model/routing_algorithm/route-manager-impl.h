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
 *
 * Authors:  Craig Dowell (craigdo@ee.washington.edu)
 *           Tom Henderson (tomhend@u.washington.edu)
 */

#ifndef DGR_ROUTE_MANAGER_IMPL_H
#define DGR_ROUTE_MANAGER_IMPL_H

#include <stdint.h>
#include <list>
#include <queue>
#include <map>
#include <vector>
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"

#include "../utility/romam-router.h"
#include "../datapath/lsdb.h"
// #include "../datapath/dgr-router-interface.h"
#include "dgr-candidate-queue.h"
// #include "dgr-router-interface.h"
// #include "dgr-candidate-queue.h"

namespace ns3 {

class DgrCandidateQueue;
class Ipv4DGRRouting;

/**
 * @brief A global router implementation.
 *
 * This singleton object can query interface each node in the system
 * for a DGRRouter interface.  For those nodes, it fetches one or
 * more Link State Advertisements and stores them in a local database.
 * Then, it can compute shortest paths on a per-node basis to all routers, 
 * and finally configure each of the node's forwarding tables.
 *
 * The design is guided by OSPFv2 \RFC{2328} section 16.1.1 and quagga ospfd.
 */
class DGRRouteManagerImpl
{
public:
  DGRRouteManagerImpl ();
  virtual ~DGRRouteManagerImpl ();
/**
 * @brief Delete all static routes on all nodes that have a
 * DGRRouterInterface
 *
 * \todo  separate manually assigned static routes from static routes that
 * the global routing code injects, and only delete the latter
 */
  virtual void DeleteDGRRoutes ();

/**
 * @brief Build the routing database by gathering Link State Advertisements
 * from each node exporting a DGRRouter interface.
 */
  virtual void BuildDGRRoutingDatabase ();

/**
 * @brief Compute routes using a Dijkstra SPF computation and populate
 * per-node forwarding tables
 */
  virtual void InitializeRoutes ();

/**
 * @brief Debugging routine; allow client code to supply a pre-built LSDB
 */
  void DebugUseLsdb (DGRRouteManagerLSDB*);

/**
 * @brief Debugging routine; call the core SPF from the unit tests
 * @param root the root node to start calculations
 */
  void DebugSPFCalculate (Ipv4Address root);

private:
/**
 * @brief DGRRouteManagerImpl copy construction is disallowed.
 * There's no  need for it and a compiler provided shallow copy would be 
 * wrong.
 *
 * @param srmi object to copy from
 */
  DGRRouteManagerImpl (DGRRouteManagerImpl& srmi);

/**
 * @brief Global Route Manager Implementation assignment operator is
 * disallowed.  There's no  need for it and a compiler provided shallow copy
 * would be hopelessly wrong.
 *
 * @param srmi object to copy from
 * @returns the copied object
 */
  DGRRouteManagerImpl& operator= (DGRRouteManagerImpl& srmi);

  DGRVertex* m_spfroot; //!< the root node
  DGRRouteManagerLSDB* m_lsdb; //!< the Link State DataBase (LSDB) of the Global Route Manager

  /**
   * \brief Test if a node is a stub, from an OSPF sense.
   *
   * If there is only one link of type 1 or 2, then a default route
   * can safely be added to the next-hop router and SPF does not need
   * to be run
   *
   * \param root the root node
   * \returns true if the node is a stub
   */
  bool CheckForStubNode (Ipv4Address root);

  /**
   * \brief Calculate the shortest path first (SPF) tree
   *
   * Equivalent to quagga ospf_spf_calculate
   * \param root the root node
   */
  void SPFCalculate (Ipv4Address root, Ipv4Address initroot, DGRRoutingLinkRecord *l, uint32_t Iface);

  /**
   * \brief Process Stub nodes
   *
   * Processing logic from RFC 2328, page 166 and quagga ospf_spf_process_stubs ()
   * stub link records will exist for point-to-point interfaces and for
   * broadcast interfaces for which no neighboring router can be found
   *
   * \param v vertex to be processed
   */
  void SPFProcessStubs (DGRVertex* v);

  /**
   * \brief Process Autonomous Systems (AS) External LSA
   *
   * \param v vertex to be processed
   * \param extlsa external LSA
   */
  void ProcessASExternals (DGRVertex* v, DGRRoutingLSA* extlsa);

  /**
   * \brief Examine the links in v's LSA and update the list of candidates with any
   *        vertices not already on the list
   *
   * \internal
   *
   * This method is derived from quagga ospf_spf_next ().  See RFC2328 Section
   * 16.1 (2) for further details.
   *
   * We're passed a parameter \a v that is a vertex which is already in the SPF
   * tree.  A vertex represents a router node.  We also get a reference to the
   * SPF candidate queue, which is a priority queue containing the shortest paths
   * to the networks we know about.
   *
   * We examine the links in v's LSA and update the list of candidates with any
   * vertices not already on the list.  If a lower-cost path is found to a
   * vertex already on the candidate list, store the new (lower) cost.
   *
   * \param v the vertex
   * \param candidate the SPF candidate queue
   */
  void SPFNext (DGRVertex* v, DGRCandidateQueue& candidate);

  /**
   * \brief Calculate nexthop from root through V (parent) to vertex W (destination)
   *        with given distance from root->W.
   *
   * This method is derived from quagga ospf_nexthop_calculation() 16.1.1.
   * For now, this is greatly simplified from the quagga code
   *
   * \param v the parent
   * \param w the destination
   * \param l the link record
   * \param distance the target distance
   * \returns 1 on success
   */
  int SPFNexthopCalculation (DGRVertex* v, DGRVertex* w, 
                             DGRRoutingLinkRecord* l, uint32_t distance);

  /**
   * \brief Adds a vertex to the list of children *in* each of its parents
   *
   * Derived from quagga ospf_vertex_add_parents ()
   *
   * This is a somewhat oddly named method (blame quagga).  Although you might
   * expect it to add a parent *to* something, it actually adds a vertex
   * to the list of children *in* each of its parents.
   *
   * Given a pointer to a vertex, it links back to the vertex's parent that it
   * already has set and adds itself to that vertex's list of children.
   *
   * \param v the vertex
   */
  void DGRVertexAddParent (DGRVertex* v);

  /**
   * \brief Search for a link between two vertices.
   *
   * This method is derived from quagga ospf_get_next_link ()
   *
   * First search the Global Router Link Records of vertex \a v for one
   * representing a point-to point link to vertex \a w.
   *
   * What is done depends on prev_link.  Contrary to appearances, prev_link just
   * acts as a flag here.  If prev_link is NULL, we return the first Global
   * Router Link Record we find that describes a point-to-point link from \a v
   * to \a w.  If prev_link is not NULL, we return a Global Router Link Record
   * representing a possible *second* link from \a v to \a w.
   *
   * \param v first vertex
   * \param w second vertex
   * \param prev_link the previous link in the list
   * \returns the link's record
   */
  DGRRoutingLinkRecord* SPFGetNextLink (DGRVertex* v, DGRVertex* w, 
                                           DGRRoutingLinkRecord* prev_link);

  /**
   * \brief Add a host route to the routing tables
   *
   *
   * This method is derived from quagga ospf_intra_add_router ()
   *
   * This is where we are actually going to add the host routes to the routing
   * tables of the individual nodes.
   *
   * The vertex passed as a parameter has just been added to the SPF tree.
   * This vertex must have a valid m_root_oid, corresponding to the outgoing
   * interface on the root router of the tree that is the first hop on the path
   * to the vertex.  The vertex must also have a next hop address, corresponding
   * to the next hop on the path to the vertex.  The vertex has an m_lsa field
   * that has some number of link records.  For each point to point link record,
   * the m_linkData is the local IP address of the link.  This corresponds to
   * a destination IP address, reachable from the root, to which we add a host
   * route.
   *
   * \param v the vertex
   *
   */
  void SPFIntraAddRouter (DGRVertex* v, DGRVertex* v_init, Ipv4Address nextHop,  uint32_t Iface);

  /**
   * \brief Add a transit to the routing tables
   *
   * \param v the vertex
   */
  void SPFIntraAddTransit (DGRVertex* v);

  /**
   * \brief Add a stub to the routing tables
   *
   * \param l the global routing link record
   * \param v the vertex
   */
  void SPFIntraAddStub (DGRRoutingLinkRecord *l, DGRVertex* v);

  /**
   * \brief Add an external route to the routing tables
   *
   * \param extlsa the external LSA
   * \param v the vertex
   */
  void SPFAddASExternal (DGRRoutingLSA *extlsa, DGRVertex *v);

  /**
   * \brief Return the interface number corresponding to a given IP address and mask
   *
   * This is a wrapper around GetInterfaceForPrefix(), but we first
   * have to find the right node pointer to pass to that function.
   * If no such interface is found, return -1 (note:  unit test framework
   * for routing assumes -1 to be a legal return value)
   *
   * \param a the target IP address
   * \param amask the target subnet mask
   * \return the outgoing interface number
   */
  int32_t FindOutgoingInterfaceId (Ipv4Address a, 
                                   Ipv4Mask amask = Ipv4Mask ("255.255.255.255"));
};

} // namespace ns3

#endif /* GLOBAL_ROUTE_MANAGER_IMPL_H */