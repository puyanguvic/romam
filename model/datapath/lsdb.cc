/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */


// #include "global-route-manager-impl.h"

// #include "candidate-queue.h"
// #include "global-router-interface.h"
// #include "ipv4-global-routing.h"
// #include "ipv4.h"

#include "ns3/assert.h"
#include "ns3/fatal-error.h"
#include "ns3/log.h"
// #include "ns3/node-list.h"
// #include "ns3/simulator.h"

// #include <algorithm>
// #include <iostream>
// #include <queue>
// #include <utility>
// #include <vector>

#include "lsdb.h"
namespace ns3
{
NS_LOG_COMPONENT_DEFINE("LinkStateDatabase");

namespace open_routing
{
/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param exit the exit node
 * \returns the reference to the output stream
 */
std::ostream&
operator<<(std::ostream& os, const SPFVertex::NodeExit_t& exit)
{
    os << "(" << exit.first << " ," << exit.second << ")";
    return os;
}

std::ostream&
operator<<(std::ostream& os, const SPFVertex::ListOfSPFVertex_t& vs)
{
    typedef SPFVertex::ListOfSPFVertex_t::const_iterator CIter_t;
    os << "{";
    for (CIter_t iter = vs.begin(); iter != vs.end();)
    {
        os << (*iter)->m_vertexId;
        if (++iter != vs.end())
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
// SPFVertex Implementation
//
// ---------------------------------------------------------------------------

SPFVertex::SPFVertex()
    : m_vertexType(VertexUnknown),
      m_vertexId("255.255.255.255"),
      m_lsa(nullptr),
      m_distanceFromRoot(SPF_INFINITY),
      m_rootOif(SPF_INFINITY),
      m_nextHop("0.0.0.0"),
      m_parents(),
      m_children(),
      m_vertexProcessed(false)
{
}

SPFVertex::SPFVertex(LSA* lsa)
    : m_vertexId(lsa->GetLinkStateId()),
      m_lsa(lsa),
      m_distanceFromRoot(SPF_INFINITY),
      m_rootOif(SPF_INFINITY),
      m_nextHop("0.0.0.0"),
      m_parents(),
      m_children(),
      m_vertexProcessed(false)
{
    if (lsa->GetLSType() == LSA::RouterLSA)
    {
        NS_LOG_LOGIC("Setting m_vertexType to VertexRouter");
        m_vertexType = SPFVertex::VertexRouter;
    }
    else if (lsa->GetLSType() == LSA::NetworkLSA)
    {
        NS_LOG_LOGIC("Setting m_vertexType to VertexNetwork");
        m_vertexType = SPFVertex::VertexNetwork;
    }
}

SPFVertex::~SPFVertex()
{
    NS_LOG_LOGIC("Children vertices - " << m_children);
    NS_LOG_LOGIC("Parent verteices - " << m_parents);

    // find this node from all its parents and remove the entry of this node
    // from all its parents
    for (ListOfSPFVertex_t::iterator piter = m_parents.begin(); piter != m_parents.end(); piter++)
    {
        // remove the current vertex from its parent's children list. Check
        // if the size of the list is reduced, or the child<->parent relation
        // is not bidirectional
        uint32_t orgCount = (*piter)->m_children.size();
        (*piter)->m_children.remove(this);
        uint32_t newCount = (*piter)->m_children.size();
        if (orgCount > newCount)
        {
            NS_ASSERT_MSG(orgCount > newCount,
                          "Unable to find the current vertex from its parents --- impossible!");
        }
    }

    // delete children
    while (!m_children.empty())
    {
        // pop out children one by one. Some children may disappear
        // when deleting some other children in the list. As a result,
        // it is necessary to use pop to walk through all children, instead
        // of using iterator.
        //
        // Note that m_children.pop_front () is not necessary as this
        // p is removed from the children list when p is deleted
        SPFVertex* p = m_children.front();
        // 'p' == 0, this child is already deleted by its other parent
        if (p == nullptr)
        {
            continue;
        }
        NS_LOG_LOGIC("Parent vertex-" << m_vertexId << " deleting its child vertex-"
                                      << p->GetVertexId());
        delete p;
        p = nullptr;
    }
    m_children.clear();
    // delete parents
    m_parents.clear();
    // delete root exit direction
    m_ecmpRootExits.clear();

    NS_LOG_LOGIC("Vertex-" << m_vertexId << " completed deleted");
}

void
SPFVertex::SetVertexType(SPFVertex::VertexType type)
{
    m_vertexType = type;
}

SPFVertex::VertexType
SPFVertex::GetVertexType() const
{
    return m_vertexType;
}

void
SPFVertex::SetVertexId(Ipv4Address id)
{
    m_vertexId = id;
}

Ipv4Address
SPFVertex::GetVertexId() const
{
    return m_vertexId;
}

void
SPFVertex::SetLSA(LSA* lsa)
{
    m_lsa = lsa;
}

LSA*
SPFVertex::GetLSA() const
{
    return m_lsa;
}

void
SPFVertex::SetDistanceFromRoot(uint32_t distance)
{
    m_distanceFromRoot = distance;
}

uint32_t
SPFVertex::GetDistanceFromRoot() const
{
    return m_distanceFromRoot;
}

void
SPFVertex::SetParent(SPFVertex* parent)
{
    // always maintain only one parent when using setter/getter methods
    m_parents.clear();
    m_parents.push_back(parent);
}

SPFVertex*
SPFVertex::GetParent(uint32_t i) const
{
    // If the index i is out-of-range, return 0 and do nothing
    if (m_parents.size() <= i)
    {
        NS_LOG_LOGIC("Index to SPFVertex's parent is out-of-range.");
        return nullptr;
    }
    ListOfSPFVertex_t::const_iterator iter = m_parents.begin();
    while (i-- > 0)
    {
        iter++;
    }
    return *iter;
}

void
SPFVertex::MergeParent(const SPFVertex* v)
{
    NS_LOG_LOGIC("Before merge, list of parents = " << m_parents);
    // combine the two lists first, and then remove any duplicated after
    m_parents.insert(m_parents.end(), v->m_parents.begin(), v->m_parents.end());
    // remove duplication
    m_parents.sort();
    m_parents.unique();
    NS_LOG_LOGIC("After merge, list of parents = " << m_parents);
}

void
SPFVertex::SetRootExitDirection(Ipv4Address nextHop, int32_t id)
{
    // always maintain only one root's exit
    m_ecmpRootExits.clear();
    m_ecmpRootExits.emplace_back(nextHop, id);
    // update the following in order to be backward compatitable with
    // GetNextHop and GetOutgoingInterface methods
    m_nextHop = nextHop;
    m_rootOif = id;
}

void
SPFVertex::SetRootExitDirection(SPFVertex::NodeExit_t exit)
{
    SetRootExitDirection(exit.first, exit.second);
}

SPFVertex::NodeExit_t
SPFVertex::GetRootExitDirection(uint32_t i) const
{
    typedef ListOfNodeExit_t::const_iterator CIter_t;

    NS_ASSERT_MSG(i < m_ecmpRootExits.size(),
                  "Index out-of-range when accessing SPFVertex::m_ecmpRootExits!");
    CIter_t iter = m_ecmpRootExits.begin();
    while (i-- > 0)
    {
        iter++;
    }

    return *iter;
}

SPFVertex::NodeExit_t
SPFVertex::GetRootExitDirection() const
{
    NS_ASSERT_MSG(m_ecmpRootExits.size() <= 1,
                  "Assumed there is at most one exit from the root to this vertex");
    return GetRootExitDirection(0);
}

void
SPFVertex::MergeRootExitDirections(const SPFVertex* vertex)
{
    // obtain the external list of exit directions
    //
    // Append the external list into 'this' and remove duplication afterward
    const ListOfNodeExit_t& extList = vertex->m_ecmpRootExits;
    m_ecmpRootExits.insert(m_ecmpRootExits.end(), extList.begin(), extList.end());
    m_ecmpRootExits.sort();
    m_ecmpRootExits.unique();
}

void
SPFVertex::InheritAllRootExitDirections(const SPFVertex* vertex)
{
    // discard all exit direction currently associated with this vertex,
    // and copy all the exit directions from the given vertex
    if (!m_ecmpRootExits.empty())
    {
        NS_LOG_WARN("x root exit directions in this vertex are going to be discarded");
    }
    m_ecmpRootExits.clear();
    m_ecmpRootExits.insert(m_ecmpRootExits.end(),
                           vertex->m_ecmpRootExits.begin(),
                           vertex->m_ecmpRootExits.end());
}

uint32_t
SPFVertex::GetNRootExitDirections() const
{
    return m_ecmpRootExits.size();
}

uint32_t
SPFVertex::GetNChildren() const
{
    return m_children.size();
}

SPFVertex*
SPFVertex::GetChild(uint32_t n) const
{
    uint32_t j = 0;

    for (ListOfSPFVertex_t::const_iterator i = m_children.begin(); i != m_children.end(); i++, j++)
    {
        if (j == n)
        {
            return *i;
        }
    }
    NS_ASSERT_MSG(false, "Index <n> out of range.");
    return nullptr;
}

uint32_t
SPFVertex::AddChild(SPFVertex* child)
{
    m_children.push_back(child);
    return m_children.size();
}

void
SPFVertex::SetVertexProcessed(bool value)
{
    m_vertexProcessed = value;
}

bool
SPFVertex::IsVertexProcessed() const
{
    return m_vertexProcessed;
}

void
SPFVertex::ClearVertexProcessed()
{
    for (uint32_t i = 0; i < this->GetNChildren(); i++)
    {
        this->GetChild(i)->ClearVertexProcessed();
    }
    this->SetVertexProcessed(false);
}

// ---------------------------------------------------------------------------
//
// LSDB Implementation
//
// ---------------------------------------------------------------------------

TypeId
LSDB::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::open_routing::LSDB").SetParent<Object>().SetGroupName("open_routing");
    return tid;
}

LSDB::LSDB()
    : m_database(),
      m_extdatabase()
{
}

LSDB::~LSDB()
{
    LSDBMap_t::iterator i;
    for (i = m_database.begin(); i != m_database.end(); i++)
    {
        NS_LOG_LOGIC("free LSA");
        LSA* temp = i->second;
        delete temp;
    }
    for (uint32_t j = 0; j < m_extdatabase.size(); j++)
    {
        NS_LOG_LOGIC("free ASexternalLSA");
        LSA* temp = m_extdatabase.at(j);
        delete temp;
    }
    NS_LOG_LOGIC("clear map");
    m_database.clear();
}

void
LSDB::Initialize()
{
    LSDBMap_t::iterator i;
    for (i = m_database.begin(); i != m_database.end(); i++)
    {
        LSA* temp = i->second;
        temp->SetStatus(LSA::LSA_SPF_NOT_EXPLORED);
    }
}

void
LSDB::Insert(Ipv4Address addr, LSA* lsa)
{
    if (lsa->GetLSType() == LSA::ASExternalLSAs)
    {
        m_extdatabase.push_back(lsa);
    }
    else
    {
        m_database.insert(LSDBPair_t(addr, lsa));
    }
}

LSA*
LSDB::GetExtLSA(uint32_t index) const
{
    return m_extdatabase.at(index);
}

uint32_t
LSDB::GetNumExtLSAs() const
{
    return m_extdatabase.size();
}

LSA*
LSDB::GetLSA(Ipv4Address addr) const
{
    //
    // Look up an LSA by its address.
    //
    LSDBMap_t::const_iterator i;
    for (i = m_database.begin(); i != m_database.end(); i++)
    {
        if (i->first == addr)
        {
            return i->second;
        }
    }
    return nullptr;
}

LSA*
LSDB::GetLSAByLinkData(Ipv4Address addr) const
{
    //
    // Look up an LSA by its address.
    //
    LSDBMap_t::const_iterator i;
    for (i = m_database.begin(); i != m_database.end(); i++)
    {
        LSA* temp = i->second;
        // Iterate among temp's Link Records
        for (uint32_t j = 0; j < temp->GetNLinkRecords(); j++)
        {
            LinkRecord* lr = temp->GetLinkRecord(j);
            if (lr->GetLinkType() == LinkRecord::TransitNetwork &&
                lr->GetLinkData() == addr)
            {
                return temp;
            }
        }
    }
    return nullptr;
}

void
LSDB::Print (std::ostream &os) const
{
    os << "Print LSDB" << std::endl;
}

}
}