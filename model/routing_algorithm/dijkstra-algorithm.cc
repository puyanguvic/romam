/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "dijkstra-algorithm.h"

#include "../romam-routing.h"
#include "../utility/ospf-router.h"
#include "../utility/romam-router.h"
#include "route-candidate-queue.h"

#include "ns3/assert.h"
#include "ns3/fatal-error.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/node-list.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>
#include <queue>
#include <utility>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DijkstraAlgorithm");

DijkstraAlgorithm::DijkstraAlgorithm()
    : m_spfroot(nullptr)
{
    NS_LOG_FUNCTION(this);
    m_lsdb = new LSDB();
}

DijkstraAlgorithm::~DijkstraAlgorithm()
{
    NS_LOG_FUNCTION(this);
    if (m_lsdb)
    {
        delete m_lsdb;
    }
}

void
DijkstraAlgorithm::DeleteRoutes()
{
    NS_LOG_FUNCTION(this);
    for (auto i = NodeList::Begin(); i != NodeList::End(); i++)
    {
        Ptr<Node> node = *i;
        Ptr<RomamRouter> router = node->GetObject<RomamRouter>();
        if (!router)
        {
            std::cout << "No find OSPF Router\n";
            continue;
        }
        Ptr<RomamRouting> gr = router->GetRoutingProtocol();
        uint32_t j = 0;
        uint32_t nRoutes = gr->GetNRoutes();
        NS_LOG_LOGIC("Deleting " << gr->GetNRoutes() << " routes from node " << node->GetId());
        // Each time we delete route 0, the route index shifts downward
        // We can delete all routes if we delete the route numbered 0
        // nRoutes times
        for (j = 0; j < nRoutes; j++)
        {
            NS_LOG_LOGIC("Deleting route " << j << " from node " << node->GetId());
            gr->RemoveRoute(0);
        }
        NS_LOG_LOGIC("Deleted " << j << " routes from node " << node->GetId());
    }
    if (m_lsdb)
    {
        NS_LOG_LOGIC("Deleting LSDB, creating new one");
        delete m_lsdb;
        m_lsdb = new LSDB();
    }
}

void
DijkstraAlgorithm::InsertLSDB(LSDB* lsdb)
{
    m_lsdb = lsdb;
    // lsdb->Print(std::cout);
}

void
DijkstraAlgorithm::InitializeRoutes()
{
    NS_LOG_FUNCTION(this);
    //
    // Walk the list of nodes in the system.
    //
    if (m_lsdb == nullptr)
    {
        NS_LOG_LOGIC("Empty LSDB, please insert LSDB.");
        return;
    }

    NS_LOG_INFO("About to start SPF calculation");
    for (auto i = NodeList::Begin(); i != NodeList::End(); i++)
    {
        Ptr<Node> node = *i;
        //
        // Look for the GlobalRouter interface that indicates that the node is
        // participating in routing.
        //
        Ptr<RomamRouter> rtr = node->GetObject<RomamRouter>();
        uint32_t systemId = Simulator::GetSystemId();
        // Ignore nodes that are not assigned to our systemId (distributed sim)
        if (node->GetSystemId() != systemId)
        {
            continue;
        }
        //
        // if the node has a global router interface, then run the global routing
        // algorithms.
        //
        if (rtr && rtr->GetNumLSAs())
        {
            SPFCalculate(rtr->GetRouterId());
        }
    }
    std::cout << "---Finished initialize routes with Dijkstra algorithm---\n";
    NS_LOG_INFO("Finished SPF calculation");
}

//
// This method is derived from quagga ospf_spf_next ().  See RFC2328 Section
// 16.1 (2) for further details.
//
// We're passed a parameter <v> that is a vertex which is already in the SPF
// tree.  A vertex represents a router node.  We also get a reference to the
// SPF candidate queue, which is a priority queue containing the shortest paths
// to the networks we know about.
//
// We examine the links in v's LSA and update the list of candidates with any
// vertices not already on the list.  If a lower-cost path is found to a
// vertex already on the candidate list, store the new (lower) cost.
//
void
DijkstraAlgorithm::SPFNext(Vertex* v, RouteCandidateQueue& candidate)
{
    NS_LOG_FUNCTION(this << v << &candidate);

    Vertex* w = nullptr;
    LSA* w_lsa = nullptr;
    LinkRecord* l = nullptr;
    uint32_t distance = 0;
    uint32_t numRecordsInVertex = 0;
    //
    // V points to a Router-LSA or Network-LSA
    // Loop over the links in router LSA or attached routers in Network LSA
    //
    if (v->GetVertexType() == Vertex::VertexRouter)
    {
        numRecordsInVertex = v->GetLSA()->GetNLinkRecords();
    }
    if (v->GetVertexType() == Vertex::VertexNetwork)
    {
        numRecordsInVertex = v->GetLSA()->GetNAttachedRouters();
    }

    for (uint32_t i = 0; i < numRecordsInVertex; i++)
    {
        // Get w_lsa:  In case of V is Router-LSA
        if (v->GetVertexType() == Vertex::VertexRouter)
        {
            NS_LOG_LOGIC("Examining link " << i << " of " << v->GetVertexId() << "'s "
                                           << v->GetLSA()->GetNLinkRecords() << " link records");
            //
            // (a) If this is a link to a stub network, examine the next link in V's LSA.
            // Links to stub networks will be considered in the second stage of the
            // shortest path calculation.
            //
            l = v->GetLSA()->GetLinkRecord(i);
            NS_ASSERT(l != nullptr);
            if (l->GetLinkType() == LinkRecord::StubNetwork)
            {
                NS_LOG_LOGIC("Found a Stub record to " << l->GetLinkId());
                continue;
            }
            //
            // (b) Otherwise, W is a transit vertex (router or transit network).  Look up
            // the vertex W's LSA (router-LSA or network-LSA) in Area A's link state
            // database.
            //
            if (l->GetLinkType() == LinkRecord::PointToPoint)
            {
                //
                // Lookup the link state advertisement of the new link -- we call it <w> in
                // the link state database.
                //
                w_lsa = m_lsdb->GetLSA(l->GetLinkId());
                NS_ASSERT(w_lsa);
                NS_LOG_LOGIC("Found a P2P record from " << v->GetVertexId() << " to "
                                                        << w_lsa->GetLinkStateId());
            }
            else if (l->GetLinkType() == LinkRecord::TransitNetwork)
            {
                w_lsa = m_lsdb->GetLSA(l->GetLinkId());
                NS_ASSERT(w_lsa);
                NS_LOG_LOGIC("Found a Transit record from " << v->GetVertexId() << " to "
                                                            << w_lsa->GetLinkStateId());
            }
            else
            {
                NS_ASSERT_MSG(0, "illegal Link Type");
            }
        }
        // Get w_lsa:  In case of V is Network-LSA
        if (v->GetVertexType() == Vertex::VertexNetwork)
        {
            w_lsa = m_lsdb->GetLSAByLinkData(v->GetLSA()->GetAttachedRouter(i));
            if (!w_lsa)
            {
                continue;
            }
            NS_LOG_LOGIC("Found a Network LSA from " << v->GetVertexId() << " to "
                                                     << w_lsa->GetLinkStateId());
        }

        // Note:  w_lsa at this point may be either RouterLSA or NetworkLSA
        //
        // (c) If vertex W is already on the shortest-path tree, examine the next
        // link in the LSA.
        //
        // If the link is to a router that is already in the shortest path first tree
        // then we have it covered -- ignore it.
        //
        if (w_lsa->GetStatus() == LSA::LSA_SPF_IN_SPFTREE)
        {
            NS_LOG_LOGIC("Skipping ->  LSA " << w_lsa->GetLinkStateId() << " already in SPF tree");
            continue;
        }
        //
        // (d) Calculate the link state cost D of the resulting path from the root to
        // vertex W.  D is equal to the sum of the link state cost of the (already
        // calculated) shortest path to vertex V and the advertised cost of the link
        // between vertices V and W.
        //
        if (v->GetLSA()->GetLSType() == LSA::RouterLSA)
        {
            NS_ASSERT(l != nullptr);
            distance = v->GetDistanceFromRoot() + l->GetMetric();
        }
        else
        {
            distance = v->GetDistanceFromRoot();
        }

        NS_LOG_LOGIC("Considering w_lsa " << w_lsa->GetLinkStateId());

        // Is there already vertex w in candidate list?
        if (w_lsa->GetStatus() == LSA::LSA_SPF_NOT_EXPLORED)
        {
            // Calculate nexthop to w
            // We need to figure out how to actually get to the new router represented
            // by <w>.  This will (among other things) find the next hop address to send
            // packets destined for this network to, and also find the outbound interface
            // used to forward the packets.

            // prepare vertex w
            w = new Vertex(w_lsa);
            if (SPFNexthopCalculation(v, w, l, distance))
            {
                w_lsa->SetStatus(LSA::LSA_SPF_CANDIDATE);
                //
                // Push this new vertex onto the priority queue (ordered by distance from the
                // root node).
                //
                candidate.Push(w);
                NS_LOG_LOGIC("Pushing " << w->GetVertexId()
                                        << ", parent vertexId: " << v->GetVertexId()
                                        << ", distance: " << w->GetDistanceFromRoot());
            }
            else
            {
                NS_ASSERT_MSG(0,
                              "SPFNexthopCalculation never " << "return false, but it does now!");
            }
        }
        else if (w_lsa->GetStatus() == LSA::LSA_SPF_CANDIDATE)
        {
            //
            // We have already considered the link represented by <w>.  What wse have to
            // do now is to decide if this new router represents a route with a shorter
            // distance metric.
            //
            // So, locate the vertex in the candidate queue and take a look at the
            // distance.

            /* (quagga-0.98.6) W is already on the candidate list; call it cw.
             * Compare the previously calculated cost (cw->distance)
             * with the cost we just determined (w->distance) to see
             * if we've found a shorter path.
             */
            Vertex* cw;
            cw = candidate.Find(w_lsa->GetLinkStateId());
            if (cw->GetDistanceFromRoot() < distance)
            {
                //
                // This is not a shorter path, so don't do anything.
                //
                continue;
            }
            else if (cw->GetDistanceFromRoot() == distance)
            {
                //
                // This path is one with an equal cost.
                //
                NS_LOG_LOGIC("Equal cost multiple paths found.");

                // At this point, there are two instances 'w' and 'cw' of the
                // same vertex, the vertex that is currently being considered
                // for adding into the shortest path tree. 'w' is the instance
                // as seen from the root via vertex 'v', and 'cw' is the instance
                // as seen from the root via some other vertices other than 'v'.
                // These two instances are being merged in the following code.
                // In particular, the parent nodes, the next hops, and the root's
                // output interfaces of the two instances are being merged.
                //
                // Note that this is functionally equivalent to calling
                // ospf_nexthop_merge (cw->nexthop, w->nexthop) in quagga-0.98.6
                // (ospf_spf.c::859), although the detail implementation
                // is very different from quagga (blame ns3::DijkstraAlgorithm)

                // prepare vertex w
                w = new Vertex(w_lsa);
                SPFNexthopCalculation(v, w, l, distance);
                cw->MergeRootExitDirections(w);
                cw->MergeParent(w);
                // SPFVertexAddParent (w) is necessary as the destructor of
                // SPFVertex checks if the vertex and its parent is linked
                // bidirectionally
                SPFVertexAddParent(w);
                delete w;
            }
            else // cw->GetDistanceFromRoot () > w->GetDistanceFromRoot ()
            {
                //
                // this path represents a new, lower-cost path to <w> (the vertex we found in
                // the current link record of the link state advertisement of the current root
                // (vertex <v>)
                //
                // N.B. the nexthop_calculation is conditional, if it finds a valid nexthop
                // it will call spf_add_parents, which will flush the old parents
                //
                if (SPFNexthopCalculation(v, cw, l, distance))
                {
                    //
                    // If we've changed the cost to get to the vertex represented by <w>, we
                    // must reorder the priority queue keyed to that cost.
                    //
                    candidate.Reorder();
                }
            } // new lower cost path found
        } // end W is already on the candidate list
    } // end loop over the links in V's LSA
}

//
// This method is derived from quagga ospf_nexthop_calculation() 16.1.1.
//
// Calculate nexthop from root through V (parent) to vertex W (destination)
// with given distance from root->W.
//
// As appropriate, set w's parent, distance, and nexthop information
//
// For now, this is greatly simplified from the quagga code
//
int
DijkstraAlgorithm::SPFNexthopCalculation(Vertex* v, Vertex* w, LinkRecord* l, uint32_t distance)
{
    NS_LOG_FUNCTION(this << v << w << l << distance);
    //
    // If w is a NetworkVertex, l should be null
    /*
      if (w->GetVertexType () == SPFVertex::VertexNetwork && l)
        {
            NS_ASSERT_MSG (0, "Error:  SPFNexthopCalculation parameter problem");
        }
    */

    //
    // The vertex m_spfroot is a distinguished vertex representing the node at
    // the root of the calculations.  That is, it is the node for which we are
    // calculating the routes.
    //
    // There are two distinct cases for calculating the next hop information.
    // First, if we're considering a hop from the root to an "adjacent" network
    // (one that is on the other side of a point-to-point link connected to the
    // root), then we need to store the information needed to forward down that
    // link.  The second case is if the network is not directly adjacent.  In that
    // case we need to use the forwarding information from the vertex on the path
    // to the destination that is directly adjacent [node 1] in both cases of the
    // diagram below.
    //
    // (1) [root] -> [point-to-point] -> [node 1]
    // (2) [root] -> [point-to-point] -> [node 1] -> [point-to-point] -> [node 2]
    //
    // We call the propagation of next hop information down vertices of a path
    // "inheriting" the next hop information.
    //
    // The point-to-point link information is only useful in this calculation when
    // we are examining the root node.
    //
    if (v == m_spfroot)
    {
        //
        // In this case <v> is the root node, which means it is the starting point
        // for the packets forwarded by that node.  This also means that the next hop
        // address of packets headed for some arbitrary off-network destination must
        // be the destination at the other end of one of the links off of the root
        // node if this root node is a router.  We then need to see if this node <w>
        // is a router.
        //
        if (w->GetVertexType() == Vertex::VertexRouter)
        {
            //
            // In the case of point-to-point links, the link data field (m_linkData) of a
            // Romam Router Link Record contains the local IP address.  If we look at the
            // link record describing the link from the perspecive of <w> (the remote
            // node from the viewpoint of <v>) back to the root node, we can discover the
            // IP address of the router to which <v> is adjacent.  This is a distinguished
            // address -- the next hop address to get from <v> to <w> and all networks
            // accessed through that path.
            //
            // SPFGetNextLink () is a little odd.  used in this way it is just going to
            // return the link record describing the link from <w> to <v>.  Think of it as
            // SPFGetLink.
            //
            NS_ASSERT(l);
            LinkRecord* linkRemote = nullptr;
            linkRemote = SPFGetNextLink(w, v, linkRemote);
            //
            // At this point, <l> is the Romam Router Link Record describing the point-
            // to point link from <v> to <w> from the perspective of <v>; and <linkRemote>
            // is the Romam Router Link Record describing that same link from the
            // perspective of <w> (back to <v>).  Now we can just copy the next hop
            // address from the m_linkData member variable.
            //
            // The next hop member variable we put in <w> has the sense "in order to get
            // from the root node to the host represented by vertex <w>, you have to send
            // the packet to the next hop address specified in w->m_nextHop.
            //
            Ipv4Address nextHop = linkRemote->GetLinkData();
            //
            // Now find the outgoing interface corresponding to the point to point link
            // from the perspective of <v> -- remember that <l> is the link "from"
            // <v> "to" <w>.
            //
            uint32_t outIf = FindOutgoingInterfaceId(l->GetLinkData());

            w->SetRootExitDirection(nextHop, outIf);
            w->SetDistanceFromRoot(distance);
            w->SetParent(v);
            NS_LOG_LOGIC("Next hop from " << v->GetVertexId() << " to " << w->GetVertexId()
                                          << " goes through next hop " << nextHop
                                          << " via outgoing interface " << outIf
                                          << " with distance " << distance);
        } // end W is a router vertes
        else
        {
            NS_ASSERT(w->GetVertexType() == Vertex::VertexNetwork);
            // W is a directly connected network; no next hop is required
            LSA* w_lsa = w->GetLSA();
            NS_ASSERT(w_lsa->GetLSType() == LSA::NetworkLSA);
            // Find outgoing interface ID for this network
            uint32_t outIf =
                FindOutgoingInterfaceId(w_lsa->GetLinkStateId(), w_lsa->GetNetworkLSANetworkMask());
            // Set the next hop to 0.0.0.0 meaning "not exist"
            Ipv4Address nextHop = Ipv4Address::GetZero();
            w->SetRootExitDirection(nextHop, outIf);
            w->SetDistanceFromRoot(distance);
            w->SetParent(v);
            NS_LOG_LOGIC("Next hop from " << v->GetVertexId() << " to network " << w->GetVertexId()
                                          << " via outgoing interface " << outIf
                                          << " with distance " << distance);
            return 1;
        }
    } // end v is the root
    else if (v->GetVertexType() == Vertex::VertexNetwork)
    {
        // See if any of v's parents are the root
        if (v->GetParent() == m_spfroot)
        {
            // 16.1.1 para 5. ...the parent vertex is a network that
            // directly connects the calculating router to the destination
            // router.  The list of next hops is then determined by
            // examining the destination's router-LSA...
            NS_ASSERT(w->GetVertexType() == Vertex::VertexRouter);
            LinkRecord* linkRemote = nullptr;
            while ((linkRemote = SPFGetNextLink(w, v, linkRemote)))
            {
                /* ...For each link in the router-LSA that points back to the
                 * parent network, the link's Link Data field provides the IP
                 * address of a next hop router.  The outgoing interface to
                 * use can then be derived from the next hop IP address (or
                 * it can be inherited from the parent network).
                 */
                Ipv4Address nextHop = linkRemote->GetLinkData();
                uint32_t outIf = v->GetRootExitDirection().second;
                w->SetRootExitDirection(nextHop, outIf);
                NS_LOG_LOGIC("Next hop from " << v->GetVertexId() << " to " << w->GetVertexId()
                                              << " goes through next hop " << nextHop
                                              << " via outgoing interface " << outIf);
            }
        }
        else
        {
            w->SetRootExitDirection(v->GetRootExitDirection());
        }
    }
    else
    {
        //
        // If we're calculating the next hop information from a node (v) that is
        // *not* the root, then we need to "inherit" the information needed to
        // forward the packet from the vertex closer to the root.  That is, we'll
        // still send packets to the next hop address of the router adjacent to the
        // root on the path toward <w>.
        //
        // Above, when we were considering the root node, we calculated the next hop
        // address and outgoing interface required to get off of the root network.
        // At this point, we are further away from the root network along one of the
        // (shortest) paths.  So the next hop and outgoing interface remain the same
        // (are inherited).
        //
        w->InheritAllRootExitDirections(v);
    }
    //
    // In all cases, we need valid values for the distance metric and a parent.
    //
    w->SetDistanceFromRoot(distance);
    w->SetParent(v);

    return 1;
}

//
// This method is derived from quagga ospf_get_next_link ()
//
// First search the Romam Router Link Records of vertex <v> for one
// representing a point-to point link to vertex <w>.
//
// What is done depends on prev_link.  Contrary to appearances, prev_link just
// acts as a flag here.  If prev_link is NULL, we return the first Global
// Router Link Record we find that describes a point-to-point link from <v>
// to <w>.  If prev_link is not NULL, we return a Romam Router Link Record
// representing a possible *second* link from <v> to <w>.
//
LinkRecord*
DijkstraAlgorithm::SPFGetNextLink(Vertex* v, Vertex* w, LinkRecord* prev_link)
{
    NS_LOG_FUNCTION(this << v << w << prev_link);

    bool skip = true;
    bool found_prev_link = false;
    LinkRecord* l;
    //
    // If prev_link is 0, we are really looking for the first link, not the next
    // link.
    //
    if (prev_link == nullptr)
    {
        skip = false;
        found_prev_link = true;
    }
    //
    // Iterate through the Romam Router Link Records advertised by the vertex
    // <v> looking for records representing the point-to-point links off of this
    // vertex.
    //
    for (uint32_t i = 0; i < v->GetLSA()->GetNLinkRecords(); ++i)
    {
        l = v->GetLSA()->GetLinkRecord(i);
        //
        // The link ID of a link record representing a point-to-point link is set to
        // the router ID of the neighboring router -- the router to which the link
        // connects from the perspective of <v> in this case.  The vertex ID is also
        // set to the router ID (using the link state advertisement of a router node).
        // We're just checking to see if the link <l> is actually the link from <v> to
        // <w>.
        //
        if (l->GetLinkId() == w->GetVertexId())
        {
            if (!found_prev_link)
            {
                NS_LOG_LOGIC("Skipping links before prev_link found");
                found_prev_link = true;
                continue;
            }

            NS_LOG_LOGIC("Found matching link l:  linkId = " << l->GetLinkId()
                                                             << " linkData = " << l->GetLinkData());
            //
            // If skip is false, don't (not too surprisingly) skip the link found -- it's
            // the one we're interested in.  That's either because we didn't pass in a
            // previous link, and we're interested in the first one, or because we've
            // skipped a previous link and moved forward to the next (which is then the
            // one we want).
            //
            if (!skip)
            {
                NS_LOG_LOGIC("Returning the found link");
                return l;
            }
            else
            {
                //
                // Skip is true and we've found a link from <v> to <w>.  We want the next one.
                // Setting skip to false gets us the next point-to-point Romam Router link
                // record in the LSA from <v>.
                //
                NS_LOG_LOGIC("Skipping the found link");
                skip = false;
                continue;
            }
        }
    }
    return nullptr;
}

//
// Used to test if a node is a stub, from an OSPF sense.
// If there is only one link of type 1 or 2, then a default route
// can safely be added to the next-hop router and SPF does not need
// to be run
//
bool
DijkstraAlgorithm::CheckForStubNode(Ipv4Address root)
{
    NS_LOG_FUNCTION(this << root);
    LSA* rlsa = m_lsdb->GetLSA(root);
    Ipv4Address myRouterId = rlsa->GetLinkStateId();
    int transits = 0;
    LinkRecord* transitLink = nullptr;
    for (uint32_t i = 0; i < rlsa->GetNLinkRecords(); i++)
    {
        LinkRecord* l = rlsa->GetLinkRecord(i);
        if (l->GetLinkType() == LinkRecord::TransitNetwork)
        {
            transits++;
            transitLink = l;
        }
        else if (l->GetLinkType() == LinkRecord::PointToPoint)
        {
            transits++;
            transitLink = l;
        }
    }
    if (transits == 0)
    {
        // This router is not connected to any router.  Probably, global
        // routing should not be called for this node, but we can just raise
        // a warning here and return true.
        NS_LOG_WARN("all nodes should have at least one transit link:" << root);
        return true;
    }
    if (transits == 1)
    {
        if (transitLink->GetLinkType() == LinkRecord::TransitNetwork)
        {
            // Install default route to next hop router
            // What is the next hop?  We need to check all neighbors on the link.
            // If there is a single router that has two transit links, then
            // that is the default next hop.  If there are more than one
            // routers on link with multiple transit links, return false.
            // Not yet implemented, so simply return false
            NS_LOG_LOGIC("TBD: Would have inserted default for transit");
            return false;
        }
        else if (transitLink->GetLinkType() == LinkRecord::PointToPoint)
        {
            // Install default route to next hop
            // The link record LinkID is the router ID of the peer.
            // The Link Data is the local IP interface address
            LSA* w_lsa = m_lsdb->GetLSA(transitLink->GetLinkId());
            uint32_t nLinkRecords = w_lsa->GetNLinkRecords();
            for (uint32_t j = 0; j < nLinkRecords; ++j)
            {
                //
                // We are only concerned about point-to-point links
                //
                LinkRecord* lr = w_lsa->GetLinkRecord(j);
                if (lr->GetLinkType() != LinkRecord::PointToPoint)
                {
                    continue;
                }
                // Find the link record that corresponds to our routerId
                if (lr->GetLinkId() == myRouterId)
                {
                    // Next hop is stored in the LinkID field of lr
                    Ptr<RomamRouter> router = rlsa->GetNode()->GetObject<RomamRouter>();
                    NS_ASSERT(router);
                    Ptr<RomamRouting> gr = router->GetRoutingProtocol();
                    NS_ASSERT(gr);
                    gr->AddNetworkRouteTo(Ipv4Address("0.0.0.0"),
                                          Ipv4Mask("0.0.0.0"),
                                          lr->GetLinkData(),
                                          FindOutgoingInterfaceId(transitLink->GetLinkData()));
                    NS_LOG_LOGIC("Inserting default route for node "
                                 << myRouterId << " to next hop " << lr->GetLinkData()
                                 << " via interface "
                                 << FindOutgoingInterfaceId(transitLink->GetLinkData()));
                    return true;
                }
            }
        }
    }
    return false;
}

// quagga ospf_spf_calculate
void
DijkstraAlgorithm::SPFCalculate(Ipv4Address root)
{
    NS_LOG_FUNCTION(this << root);
    Vertex* v;
    //
    // Initialize the Link State Database.
    //
    m_lsdb->Initialize();
    //
    // The candidate queue is a priority queue of SPFVertex objects, with the top
    // of the queue being the closest vertex in terms of distance from the root
    // of the tree.  Initially, this queue is empty.
    //
    RouteCandidateQueue candidate;
    NS_ASSERT(candidate.Size() == 0);
    //
    // Initialize the shortest-path tree to only contain the router doing the
    // calculation.  Each router (and corresponding network) is a vertex in the
    // shortest path first (SPF) tree.
    //
    v = new Vertex(m_lsdb->GetLSA(root));
    //
    // This vertex is the root of the SPF tree and it is distance 0 from the root.
    // We also mark this vertex as being in the SPF tree.
    //
    m_spfroot = v;
    v->SetDistanceFromRoot(0);
    v->GetLSA()->SetStatus(LSA::LSA_SPF_IN_SPFTREE);
    NS_LOG_LOGIC("Starting SPFCalculate for node " << root);

    //
    // Optimize SPF calculation, for ns-3.
    // We do not need to calculate SPF for every node in the network if this
    // node has only one interface through which another router can be
    // reached.  Instead, short-circuit this computation and just install
    // a default route in the CheckForStubNode() method.
    //
    if (NodeList::GetNNodes() > 0 && CheckForStubNode(root))
    {
        NS_LOG_LOGIC("SPFCalculate truncated for stub node " << root);
        delete m_spfroot;
        return;
    }

    for (;;)
    {
        //
        // The operations we need to do are given in the OSPF RFC which we reference
        // as we go along.
        //
        // RFC2328 16.1. (2).
        //
        // We examine the Romam Router Link Records in the Link State
        // Advertisements of the current vertex.  If there are any point-to-point
        // links to unexplored adjacent vertices we add them to the tree and update
        // the distance and next hop information on how to get there.  We also add
        // the new vertices to the candidate queue (the priority queue ordered by
        // shortest path).  If the new vertices represent shorter paths, we use them
        // and update the path cost.
        //
        SPFNext(v, candidate);
        //
        // RFC2328 16.1. (3).
        //
        // If at this step the candidate list is empty, the shortest-path tree (of
        // transit vertices) has been completely built and this stage of the
        // procedure terminates.
        //
        if (candidate.Size() == 0)
        {
            break;
        }
        //
        // Choose the vertex belonging to the candidate list that is closest to the
        // root, and add it to the shortest-path tree (removing it from the candidate
        // list in the process).
        //
        // Recall that in the previous step, we created SPFVertex structures for each
        // of the routers found in the Romam Router Link Records and added tehm to
        // the candidate list.
        //
        NS_LOG_LOGIC(candidate);
        v = candidate.Pop();
        NS_LOG_LOGIC("Popped vertex " << v->GetVertexId());
        //
        // Update the status field of the vertex to indicate that it is in the SPF
        // tree.
        //
        v->GetLSA()->SetStatus(LSA::LSA_SPF_IN_SPFTREE);
        //
        // The current vertex has a parent pointer.  By calling this rather oddly
        // named method (blame quagga) we add the current vertex to the list of
        // children of that parent vertex.  In the next hop calculation called during
        // SPFNext, the parent pointer was set but the vertex has been orphaned up
        // to now.
        //
        SPFVertexAddParent(v);
        //
        // Note that when there is a choice of vertices closest to the root, network
        // vertices must be chosen before router vertices in order to necessarily
        // find all equal-cost paths.
        //
        // RFC2328 16.1. (4).
        //
        // This is the method that actually adds the routes.  It'll walk the list
        // of nodes in the system, looking for the node corresponding to the router
        // ID of the root of the tree -- that is the router we're building the routes
        // for.  It looks for the Ipv4 interface of that node and remembers it.  So
        // we are only actually adding routes to that one node at the root of the SPF
        // tree.
        //
        // We're going to pop of a pointer to every vertex in the tree except the
        // root in order of distance from the root.  For each of the vertices, we call
        // SPFIntraAddRouter ().  Down in SPFIntraAddRouter, we look at all of the
        // point-to-point Romam Router Link Records (the links to nodes adjacent to
        // the node represented by the vertex).  We add a route to the IP address
        // specified by the m_linkData field of each of those link records.  This will
        // be the *local* IP address associated with the interface attached to the
        // link.  We use the outbound interface and next hop information present in
        // the vertex <v> which have possibly been inherited from the root.
        //
        // To summarize, we're going to look at the node represented by <v> and loop
        // through its point-to-point links, adding a *host* route to the local IP
        // address (at the <v> side) for each of those links.
        //
        if (v->GetVertexType() == Vertex::VertexRouter)
        {
            SPFIntraAddRouter(v);
        }
        else if (v->GetVertexType() == Vertex::VertexNetwork)
        {
            SPFIntraAddTransit(v);
        }
        else
        {
            NS_ASSERT_MSG(0, "illegal SPFVertex type");
        }
        //
        // RFC2328 16.1. (5).
        //
        // Iterate the algorithm by returning to Step 2 until there are no more
        // candidate vertices.

    } // end for loop

    // Second stage of SPF calculation procedure
    SPFProcessStubs(m_spfroot);
    for (uint32_t i = 0; i < m_lsdb->GetNumExtLSAs(); i++)
    {
        m_spfroot->ClearVertexProcessed();
        LSA* extlsa = m_lsdb->GetExtLSA(i);
        NS_LOG_LOGIC("Processing External LSA with id " << extlsa->GetLinkStateId());
        ProcessASExternals(m_spfroot, extlsa);
    }

    //
    // We're all done setting the routing information for the node at the root of
    // the SPF tree.  Delete all of the vertices and corresponding resources.  Go
    // possibly do it again for the next router.
    //
    delete m_spfroot;
    m_spfroot = nullptr;
}

void
DijkstraAlgorithm::ProcessASExternals(Vertex* v, LSA* extlsa)
{
    NS_LOG_FUNCTION(this << v << extlsa);
    NS_LOG_LOGIC("Processing external for destination "
                 << extlsa->GetLinkStateId() << ", for router " << v->GetVertexId()
                 << ", advertised by " << extlsa->GetAdvertisingRouter());
    if (v->GetVertexType() == Vertex::VertexRouter)
    {
        LSA* rlsa = v->GetLSA();
        NS_LOG_LOGIC("Processing router LSA with id " << rlsa->GetLinkStateId());
        if ((rlsa->GetLinkStateId()) == (extlsa->GetAdvertisingRouter()))
        {
            NS_LOG_LOGIC("Found advertising router to destination");
            SPFAddASExternal(extlsa, v);
        }
    }
    for (uint32_t i = 0; i < v->GetNChildren(); i++)
    {
        if (!v->GetChild(i)->IsVertexProcessed())
        {
            NS_LOG_LOGIC("Vertex's child " << i << " not yet processed, processing...");
            ProcessASExternals(v->GetChild(i), extlsa);
            v->GetChild(i)->SetVertexProcessed(true);
        }
    }
}

//
// Adding external routes to routing table - modeled after
// SPFAddIntraAddStub()
//

void
DijkstraAlgorithm::SPFAddASExternal(LSA* extlsa, Vertex* v)
{
    NS_LOG_FUNCTION(this << extlsa << v);

    NS_ASSERT_MSG(m_spfroot, "DijkstraAlgorithm::SPFAddASExternal (): Root pointer not set");
    // Two cases to consider: We are advertising the external ourselves
    // => No need to add anything
    // OR find best path to the advertising router
    if (v->GetVertexId() == m_spfroot->GetVertexId())
    {
        NS_LOG_LOGIC("External is on local host: " << v->GetVertexId() << "; returning");
        return;
    }
    NS_LOG_LOGIC("External is on remote host: " << extlsa->GetAdvertisingRouter()
                                                << "; installing");

    Ipv4Address routerId = m_spfroot->GetVertexId();

    NS_LOG_LOGIC("Vertex ID = " << routerId);
    //
    // We need to walk the list of nodes looking for the one that has the router
    // ID corresponding to the root vertex.  This is the one we're going to write
    // the routing information to.
    //
    for (auto i = NodeList::Begin(); i != NodeList::End(); i++)
    {
        Ptr<Node> node = *i;
        //
        // The router ID is accessible through the RomamRouter interface, so we need
        // to QI for that interface.  If there's no RomamRouter interface, the node
        // in question cannot be the router we want, so we continue.
        //
        Ptr<RomamRouter> rtr = node->GetObject<RomamRouter>();

        if (!rtr)
        {
            NS_LOG_LOGIC("No RomamRouter interface on node " << node->GetId());
            continue;
        }
        //
        // If the router ID of the current node is equal to the router ID of the
        // root of the SPF tree, then this node is the one for which we need to
        // write the routing tables.
        //
        NS_LOG_LOGIC("Considering router " << rtr->GetRouterId());

        if (rtr->GetRouterId() == routerId)
        {
            NS_LOG_LOGIC("Setting routes for node " << node->GetId());
            //
            // Routing information is updated using the Ipv4 interface.  We need to QI
            // for that interface.  If the node is acting as an IP version 4 router, it
            // should absolutely have an Ipv4 interface.
            //
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_ASSERT_MSG(ipv4,
                          "DijkstraAlgorithm::SPFIntraAddRouter (): "
                          "QI for <Ipv4> interface failed");
            //
            // Get the Romam Router Link State Advertisement from the vertex we're
            // adding the routes to.  The LSA will have a number of attached Romam Router
            // Link Records corresponding to links off of that vertex / node.  We're going
            // to be interested in the records corresponding to point-to-point links.
            //
            NS_ASSERT_MSG(v->GetLSA(),
                          "DijkstraAlgorithm::SPFIntraAddRouter (): "
                          "Expected valid LSA in SPFVertex* v");
            Ipv4Mask tempmask = extlsa->GetNetworkLSANetworkMask();
            Ipv4Address tempip = extlsa->GetLinkStateId();
            tempip = tempip.CombineMask(tempmask);

            //
            // Here's why we did all of that work.  We're going to add a host route to the
            // host address found in the m_linkData field of the point-to-point link
            // record.  In the case of a point-to-point link, this is the local IP address
            // of the node connected to the link.  Each of these point-to-point links
            // will correspond to a local interface that has an IP address to which
            // the node at the root of the SPF tree can send packets.  The vertex <v>
            // (corresponding to the node that has these links and interfaces) has
            // an m_nextHop address precalculated for us that is the address to which the
            // root node should send packets to be forwarded to these IP addresses.
            // Similarly, the vertex <v> has an m_rootOif (outbound interface index) to
            // which the packets should be send for forwarding.
            //
            Ptr<RomamRouter> router = node->GetObject<RomamRouter>();
            if (!router)
            {
                continue;
            }
            Ptr<RomamRouting> gr = router->GetRoutingProtocol();
            NS_ASSERT(gr);
            // walk through all next-hop-IPs and out-going-interfaces for reaching
            // the stub network gateway 'v' from the root node
            for (uint32_t i = 0; i < v->GetNRootExitDirections(); i++)
            {
                Vertex::NodeExit_t exit = v->GetRootExitDirection(i);
                Ipv4Address nextHop = exit.first;
                int32_t outIf = exit.second;
                if (outIf >= 0)
                {
                    gr->AddASExternalRouteTo(tempip, tempmask, nextHop, outIf);
                    NS_LOG_LOGIC("(Route " << i << ") Node " << node->GetId()
                                           << " add external network route to " << tempip
                                           << " using next hop " << nextHop << " via interface "
                                           << outIf);
                }
                else
                {
                    NS_LOG_LOGIC("(Route " << i << ") Node " << node->GetId()
                                           << " NOT able to add network route to " << tempip
                                           << " using next hop " << nextHop
                                           << " since outgoing interface id is negative");
                }
            }
            return;
        } // if
    } // for
}

// Processing logic from RFC 2328, page 166 and quagga ospf_spf_process_stubs ()
// stub link records will exist for point-to-point interfaces and for
// broadcast interfaces for which no neighboring router can be found
void
DijkstraAlgorithm::SPFProcessStubs(Vertex* v)
{
    NS_LOG_FUNCTION(this << v);
    NS_LOG_LOGIC("Processing stubs for " << v->GetVertexId());
    if (v->GetVertexType() == Vertex::VertexRouter)
    {
        LSA* rlsa = v->GetLSA();
        NS_LOG_LOGIC("Processing router LSA with id " << rlsa->GetLinkStateId());
        for (uint32_t i = 0; i < rlsa->GetNLinkRecords(); i++)
        {
            NS_LOG_LOGIC("Examining link " << i << " of " << v->GetVertexId() << "'s "
                                           << v->GetLSA()->GetNLinkRecords() << " link records");
            LinkRecord* l = v->GetLSA()->GetLinkRecord(i);
            if (l->GetLinkType() == LinkRecord::StubNetwork)
            {
                NS_LOG_LOGIC("Found a Stub record to " << l->GetLinkId());
                SPFIntraAddStub(l, v);
                continue;
            }
        }
    }
    for (uint32_t i = 0; i < v->GetNChildren(); i++)
    {
        if (!v->GetChild(i)->IsVertexProcessed())
        {
            SPFProcessStubs(v->GetChild(i));
            v->GetChild(i)->SetVertexProcessed(true);
        }
    }
}

// RFC2328 16.1. second stage.
void
DijkstraAlgorithm::SPFIntraAddStub(LinkRecord* l, Vertex* v)
{
    NS_LOG_FUNCTION(this << l << v);

    NS_ASSERT_MSG(m_spfroot, "DijkstraAlgorithm::SPFIntraAddStub (): Root pointer not set");

    // XXX simplified logic for the moment.  There are two cases to consider:
    // 1) the stub network is on this router; do nothing for now
    //    (already handled above)
    // 2) the stub network is on a remote router, so I should use the
    // same next hop that I use to get to vertex v
    if (v->GetVertexId() == m_spfroot->GetVertexId())
    {
        NS_LOG_LOGIC("Stub is on local host: " << v->GetVertexId() << "; returning");
        return;
    }
    NS_LOG_LOGIC("Stub is on remote host: " << v->GetVertexId() << "; installing");
    //
    // The root of the Shortest Path First tree is the router to which we are
    // going to write the actual routing table entries.  The vertex corresponding
    // to this router has a vertex ID which is the router ID of that node.  We're
    // going to use this ID to discover which node it is that we're actually going
    // to update.
    //
    Ipv4Address routerId = m_spfroot->GetVertexId();

    NS_LOG_LOGIC("Vertex ID = " << routerId);
    //
    // We need to walk the list of nodes looking for the one that has the router
    // ID corresponding to the root vertex.  This is the one we're going to write
    // the routing information to.
    //
    for (auto i = NodeList::Begin(); i != NodeList::End(); i++)
    {
        Ptr<Node> node = *i;
        //
        // The router ID is accessible through the RomamRouter interface, so we need
        // to QI for that interface.  If there's no RomamRouter interface, the node
        // in question cannot be the router we want, so we continue.
        //
        Ptr<RomamRouter> rtr = node->GetObject<RomamRouter>();

        if (!rtr)
        {
            NS_LOG_LOGIC("No RomamRouter interface on node " << node->GetId());
            continue;
        }
        //
        // If the router ID of the current node is equal to the router ID of the
        // root of the SPF tree, then this node is the one for which we need to
        // write the routing tables.
        //
        NS_LOG_LOGIC("Considering router " << rtr->GetRouterId());

        if (rtr->GetRouterId() == routerId)
        {
            NS_LOG_LOGIC("Setting routes for node " << node->GetId());
            //
            // Routing information is updated using the Ipv4 interface.  We need to QI
            // for that interface.  If the node is acting as an IP version 4 router, it
            // should absolutely have an Ipv4 interface.
            //
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_ASSERT_MSG(ipv4,
                          "DijkstraAlgorithm::SPFIntraAddRouter (): "
                          "QI for <Ipv4> interface failed");
            //
            // Get the Romam Router Link State Advertisement from the vertex we're
            // adding the routes to.  The LSA will have a number of attached Romam Router
            // Link Records corresponding to links off of that vertex / node.  We're going
            // to be interested in the records corresponding to point-to-point links.
            //
            NS_ASSERT_MSG(v->GetLSA(),
                          "DijkstraAlgorithm::SPFIntraAddRouter (): "
                          "Expected valid LSA in Vertex* v");
            Ipv4Mask tempmask(l->GetLinkData().Get());
            Ipv4Address tempip = l->GetLinkId();
            tempip = tempip.CombineMask(tempmask);
            //
            // Here's why we did all of that work.  We're going to add a host route to the
            // host address found in the m_linkData field of the point-to-point link
            // record.  In the case of a point-to-point link, this is the local IP address
            // of the node connected to the link.  Each of these point-to-point links
            // will correspond to a local interface that has an IP address to which
            // the node at the root of the SPF tree can send packets.  The vertex <v>
            // (corresponding to the node that has these links and interfaces) has
            // an m_nextHop address precalculated for us that is the address to which the
            // root node should send packets to be forwarded to these IP addresses.
            // Similarly, the vertex <v> has an m_rootOif (outbound interface index) to
            // which the packets should be send for forwarding.
            //

            Ptr<RomamRouter> router = node->GetObject<RomamRouter>();
            if (!router)
            {
                continue;
            }
            Ptr<RomamRouting> gr = router->GetRoutingProtocol();
            NS_ASSERT(gr);
            // walk through all next-hop-IPs and out-going-interfaces for reaching
            // the stub network gateway 'v' from the root node
            for (uint32_t i = 0; i < v->GetNRootExitDirections(); i++)
            {
                Vertex::NodeExit_t exit = v->GetRootExitDirection(i);
                Ipv4Address nextHop = exit.first;
                int32_t outIf = exit.second;
                if (outIf >= 0)
                {
                    gr->AddNetworkRouteTo(tempip, tempmask, nextHop, outIf);
                    NS_LOG_LOGIC("(Route " << i << ") Node " << node->GetId()
                                           << " add network route to " << tempip
                                           << " using next hop " << nextHop << " via interface "
                                           << outIf);
                }
                else
                {
                    NS_LOG_LOGIC("(Route " << i << ") Node " << node->GetId()
                                           << " NOT able to add network route to " << tempip
                                           << " using next hop " << nextHop
                                           << " since outgoing interface id is negative");
                }
            }
            return;
        } // if
    } // for
}

//
// Return the interface number corresponding to a given IP address and mask
// This is a wrapper around GetInterfaceForPrefix(), but we first
// have to find the right node pointer to pass to that function.
// If no such interface is found, return -1 (note:  unit test framework
// for routing assumes -1 to be a legal return value)
//
int32_t
DijkstraAlgorithm::FindOutgoingInterfaceId(Ipv4Address a, Ipv4Mask amask)
{
    NS_LOG_FUNCTION(this << a << amask);
    //
    // We have an IP address <a> and a vertex ID of the root of the SPF tree.
    // The question is what interface index does this address correspond to.
    // The answer is a little complicated since we have to find a pointer to
    // the node corresponding to the vertex ID, find the Ipv4 interface on that
    // node in order to iterate the interfaces and find the one corresponding to
    // the address in question.
    //
    Ipv4Address routerId = m_spfroot->GetVertexId();
    //
    // Walk the list of nodes in the system looking for the one corresponding to
    // the node at the root of the SPF tree.  This is the node for which we are
    // building the routing table.
    //
    for (auto i = NodeList::Begin(); i != NodeList::End(); i++)
    {
        Ptr<Node> node = *i;

        Ptr<RomamRouter> rtr = node->GetObject<RomamRouter>();
        //
        // If the node doesn't have a RomamRouter interface it can't be the one
        // we're interested in.
        //
        if (!rtr)
        {
            continue;
        }

        if (rtr->GetRouterId() == routerId)
        {
            //
            // This is the node we're building the routing table for.  We're going to need
            // the Ipv4 interface to look for the ipv4 interface index.  Since this node
            // is participating in routing IP version 4 packets, it certainly must have
            // an Ipv4 interface.
            //
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_ASSERT_MSG(ipv4,
                          "DijkstraAlgorithm::FindOutgoingInterfaceId (): "
                          "GetObject for <Ipv4> interface failed");
            //
            // Look through the interfaces on this node for one that has the IP address
            // we're looking for.  If we find one, return the corresponding interface
            // index, or -1 if not found.
            //
            int32_t interface = ipv4->GetInterfaceForPrefix(a, amask);

#if 0
          if (interface < 0)
            {
              NS_FATAL_ERROR ("DijkstraAlgorithm::FindOutgoingInterfaceId(): "
                              "Expected an interface associated with address a:" << a);
            }
#endif
            return interface;
        }
    }
    //
    // Couldn't find it.
    //
    NS_LOG_LOGIC("FindOutgoingInterfaceId():Can't find root node " << routerId);
    return -1;
}

//
// This method is derived from quagga ospf_intra_add_router ()
//
// This is where we are actually going to add the host routes to the routing
// tables of the individual nodes.
//
// The vertex passed as a parameter has just been added to the SPF tree.
// This vertex must have a valid m_root_oid, corresponding to the outgoing
// interface on the root router of the tree that is the first hop on the path
// to the vertex.  The vertex must also have a next hop address, corresponding
// to the next hop on the path to the vertex.  The vertex has an m_lsa field
// that has some number of link records.  For each point to point link record,
// the m_linkData is the local IP address of the link.  This corresponds to
// a destination IP address, reachable from the root, to which we add a host
// route.
//
void
DijkstraAlgorithm::SPFIntraAddRouter(Vertex* v)
{
    NS_LOG_FUNCTION(this << v);

    NS_ASSERT_MSG(m_spfroot, "DijkstraAlgorithm::SPFIntraAddRouter (): Root pointer not set");
    //
    // The root of the Shortest Path First tree is the router to which we are
    // going to write the actual routing table entries.  The vertex corresponding
    // to this router has a vertex ID which is the router ID of that node.  We're
    // going to use this ID to discover which node it is that we're actually going
    // to update.
    //
    Ipv4Address routerId = m_spfroot->GetVertexId();

    NS_LOG_LOGIC("Vertex ID = " << routerId);
    //
    // We need to walk the list of nodes looking for the one that has the router
    // ID corresponding to the root vertex.  This is the one we're going to write
    // the routing information to.
    //
    for (auto i = NodeList::Begin(); i != NodeList::End(); i++)
    {
        Ptr<Node> node = *i;
        //
        // The router ID is accessible through the RomamRouter interface, so we need
        // to GetObject for that interface.  If there's no RomamRouter interface,
        // the node in question cannot be the router we want, so we continue.
        //
        Ptr<RomamRouter> rtr = node->GetObject<RomamRouter>();

        if (!rtr)
        {
            NS_LOG_LOGIC("No RomamRouter interface on node " << node->GetId());
            continue;
        }
        //
        // If the router ID of the current node is equal to the router ID of the
        // root of the SPF tree, then this node is the one for which we need to
        // write the routing tables.
        //
        NS_LOG_LOGIC("Considering router " << rtr->GetRouterId());

        if (rtr->GetRouterId() == routerId)
        {
            NS_LOG_LOGIC("Setting routes for node " << node->GetId());
            //
            // Routing information is updated using the Ipv4 interface.  We need to
            // GetObject for that interface.  If the node is acting as an IP version 4
            // router, it should absolutely have an Ipv4 interface.
            //
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_ASSERT_MSG(ipv4,
                          "DijkstraAlgorithm::SPFIntraAddRouter (): "
                          "GetObject for <Ipv4> interface failed");
            //
            // Get the Romam Router Link State Advertisement from the vertex we're
            // adding the routes to.  The LSA will have a number of attached Romam Router
            // Link Records corresponding to links off of that vertex / node.  We're going
            // to be interested in the records corresponding to point-to-point links.
            //
            LSA* lsa = v->GetLSA();
            NS_ASSERT_MSG(lsa,
                          "DijkstraAlgorithm::SPFIntraAddRouter (): "
                          "Expected valid LSA in Vertex* v");

            uint32_t nLinkRecords = lsa->GetNLinkRecords();
            //
            // Iterate through the link records on the vertex to which we're going to add
            // routes.  To make sure we're being clear, we're going to add routing table
            // entries to the tables on the node corresping to the root of the SPF tree.
            // These entries will have routes to the IP addresses we find from looking at
            // the local side of the point-to-point links found on the node described by
            // the vertex <v>.
            //
            NS_LOG_LOGIC(" Node " << node->GetId() << " found " << nLinkRecords
                                  << " link records in LSA " << lsa << "with LinkStateId "
                                  << lsa->GetLinkStateId());
            for (uint32_t j = 0; j < nLinkRecords; ++j)
            {
                //
                // We are only concerned about point-to-point links
                //
                LinkRecord* lr = lsa->GetLinkRecord(j);
                if (lr->GetLinkType() != LinkRecord::PointToPoint)
                {
                    continue;
                }
                //
                // Here's why we did all of that work.  We're going to add a host route to the
                // host address found in the m_linkData field of the point-to-point link
                // record.  In the case of a point-to-point link, this is the local IP address
                // of the node connected to the link.  Each of these point-to-point links
                // will correspond to a local interface that has an IP address to which
                // the node at the root of the SPF tree can send packets.  The vertex <v>
                // (corresponding to the node that has these links and interfaces) has
                // an m_nextHop address precalculated for us that is the address to which the
                // root node should send packets to be forwarded to these IP addresses.
                // Similarly, the vertex <v> has an m_rootOif (outbound interface index) to
                // which the packets should be send for forwarding.
                //
                Ptr<RomamRouter> router = node->GetObject<RomamRouter>();
                if (!router)
                {
                    continue;
                }
                Ptr<RomamRouting> gr = router->GetRoutingProtocol();
                NS_ASSERT(gr);
                // walk through all available exit directions due to ECMP,
                // and add host route for each of the exit direction toward
                // the vertex 'v'
                for (uint32_t i = 0; i < v->GetNRootExitDirections(); i++)
                {
                    Vertex::NodeExit_t exit = v->GetRootExitDirection(i);
                    Ipv4Address nextHop = exit.first;
                    int32_t outIf = exit.second;
                    if (outIf >= 0)
                    {
                        gr->AddHostRouteTo(lr->GetLinkData(), nextHop, outIf);
                        NS_LOG_LOGIC("(Route " << i << ") Node " << node->GetId()
                                               << " adding host route to " << lr->GetLinkData()
                                               << " using next hop " << nextHop
                                               << " and outgoing interface " << outIf);
                    }
                    else
                    {
                        NS_LOG_LOGIC("(Route " << i << ") Node " << node->GetId()
                                               << " NOT able to add host route to "
                                               << lr->GetLinkData() << " using next hop " << nextHop
                                               << " since outgoing interface id is negative "
                                               << outIf);
                    }
                } // for all routes from the root the vertex 'v'
            }
            //
            // Done adding the routes for the selected node.
            //
            return;
        }
    }
}

void
DijkstraAlgorithm::SPFIntraAddTransit(Vertex* v)
{
    NS_LOG_FUNCTION(this << v);

    NS_ASSERT_MSG(m_spfroot, "DijkstraAlgorithm::SPFIntraAddTransit (): Root pointer not set");
    //
    // The root of the Shortest Path First tree is the router to which we are
    // going to write the actual routing table entries.  The vertex corresponding
    // to this router has a vertex ID which is the router ID of that node.  We're
    // going to use this ID to discover which node it is that we're actually going
    // to update.
    //
    Ipv4Address routerId = m_spfroot->GetVertexId();

    NS_LOG_LOGIC("Vertex ID = " << routerId);
    //
    // We need to walk the list of nodes looking for the one that has the router
    // ID corresponding to the root vertex.  This is the one we're going to write
    // the routing information to.
    //
    for (auto i = NodeList::Begin(); i != NodeList::End(); i++)
    {
        Ptr<Node> node = *i;
        //
        // The router ID is accessible through the RomamRouter interface, so we need
        // to GetObject for that interface.  If there's no RomamRouter interface,
        // the node in question cannot be the router we want, so we continue.
        //
        Ptr<RomamRouter> rtr = node->GetObject<RomamRouter>();

        if (!rtr)
        {
            NS_LOG_LOGIC("No RomamRouter interface on node " << node->GetId());
            continue;
        }
        //
        // If the router ID of the current node is equal to the router ID of the
        // root of the SPF tree, then this node is the one for which we need to
        // write the routing tables.
        //
        NS_LOG_LOGIC("Considering router " << rtr->GetRouterId());

        if (rtr->GetRouterId() == routerId)
        {
            NS_LOG_LOGIC("setting routes for node " << node->GetId());
            //
            // Routing information is updated using the Ipv4 interface.  We need to
            // GetObject for that interface.  If the node is acting as an IP version 4
            // router, it should absolutely have an Ipv4 interface.
            //
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_ASSERT_MSG(ipv4,
                          "DijkstraAlgorithm::SPFIntraAddTransit (): "
                          "GetObject for <Ipv4> interface failed");
            //
            // Get the Romam Router Link State Advertisement from the vertex we're
            // adding the routes to.  The LSA will have a number of attached Romam Router
            // Link Records corresponding to links off of that vertex / node.  We're going
            // to be interested in the records corresponding to point-to-point links.
            //
            LSA* lsa = v->GetLSA();
            NS_ASSERT_MSG(lsa,
                          "DijkstraAlgorithm::SPFIntraAddTransit (): "
                          "Expected valid LSA in SPFVertex* v");
            Ipv4Mask tempmask = lsa->GetNetworkLSANetworkMask();
            Ipv4Address tempip = lsa->GetLinkStateId();
            tempip = tempip.CombineMask(tempmask);
            Ptr<RomamRouter> router = node->GetObject<RomamRouter>();
            if (!router)
            {
                continue;
            }
            Ptr<RomamRouting> gr = router->GetRoutingProtocol();
            NS_ASSERT(gr);
            // walk through all available exit directions due to ECMP,
            // and add host route for each of the exit direction toward
            // the vertex 'v'
            for (uint32_t i = 0; i < v->GetNRootExitDirections(); i++)
            {
                Vertex::NodeExit_t exit = v->GetRootExitDirection(i);
                Ipv4Address nextHop = exit.first;
                int32_t outIf = exit.second;

                if (outIf >= 0)
                {
                    gr->AddNetworkRouteTo(tempip, tempmask, nextHop, outIf);
                    NS_LOG_LOGIC("(Route " << i << ") Node " << node->GetId()
                                           << " add network route to " << tempip
                                           << " using next hop " << nextHop << " via interface "
                                           << outIf);
                }
                else
                {
                    NS_LOG_LOGIC("(Route " << i << ") Node " << node->GetId()
                                           << " NOT able to add network route to " << tempip
                                           << " using next hop " << nextHop
                                           << " since outgoing interface id is negative " << outIf);
                }
            }
        }
    }
}

// Derived from quagga ospf_vertex_add_parents ()
//
// This is a somewhat oddly named method (blame quagga).  Although you might
// expect it to add a parent *to* something, it actually adds a vertex
// to the list of children *in* each of its parents.
//
// Given a pointer to a vertex, it links back to the vertex's parent that it
// already has set and adds itself to that vertex's list of children.
//
void
DijkstraAlgorithm::SPFVertexAddParent(Vertex* v)
{
    NS_LOG_FUNCTION(this << v);

    for (uint32_t i = 0;;)
    {
        Vertex* parent;
        // check if all parents of vertex v
        if ((parent = v->GetParent(i++)) == nullptr)
        {
            break;
        }
        parent->AddChild(v);
    }
}

} // namespace ns3