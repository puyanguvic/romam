#include "spf.h"
#include "ns3/ipv4-address.h"

namespace ns3
{

void SPFCalculate(Ipv4Address root)
{
    // NS_LOG_FUNCTION(this << root);

    SPFVertex* v;
    //
    // Initialize the Link State Database.
    //
    m_lsdb->Initialize();
    //
    // The candidate queue is a priority queue of SPFVertex objects, with the top
    // of the queue being the closest vertex in terms of distance from the root
    // of the tree.  Initially, this queue is empty.
    //
    CandidateQueue candidate;
    NS_ASSERT(candidate.Size() == 0);
    //
    // Initialize the shortest-path tree to only contain the router doing the
    // calculation.  Each router (and corresponding network) is a vertex in the
    // shortest path first (SPF) tree.
    //
    v = new SPFVertex(m_lsdb->GetLSA(root));
    //
    // This vertex is the root of the SPF tree and it is distance 0 from the root.
    // We also mark this vertex as being in the SPF tree.
    //
    m_spfroot = v;
    v->SetDistanceFromRoot(0);
    v->GetLSA()->SetStatus(GlobalRoutingLSA::LSA_SPF_IN_SPFTREE);
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
        // We examine the Global Router Link Records in the Link State
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
        // of the routers found in the Global Router Link Records and added tehm to
        // the candidate list.
        //
        NS_LOG_LOGIC(candidate);
        v = candidate.Pop();
        NS_LOG_LOGIC("Popped vertex " << v->GetVertexId());
        //
        // Update the status field of the vertex to indicate that it is in the SPF
        // tree.
        //
        v->GetLSA()->SetStatus(GlobalRoutingLSA::LSA_SPF_IN_SPFTREE);
        //
        // The current vertex has a parent pointer.  By calling this rather oddly
        // named method (blame quagga) we add the current vertex to the list of
        // children 
        // We're going to pop of a pointer to every vertex in the tree except the
        // root in order of distance from the root.  For each of the vertices, we call
        // SPFIntraAddRouter ().  Down in SPFIntraAddRouter, we look at all of the
        // point-to-point Global Router Link Records (the links to nodes adjacent to
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
        if (v->GetVertexType() == SPFVertex::VertexRouter)
        {
            SPFIntraAddRouter(v);
        }
        else if (v->GetVertexType() == SPFVertex::VertexNetwork)
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
        GlobalRoutingLSA* extlsa = m_lsdb->GetExtLSA(i);
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

}