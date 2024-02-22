/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */


// #include "global-route-manager-impl.h"

// #include "candidate-queue.h"
// #include "global-router-interface.h"
// #include "ipv4-global-routing.h"
// #include "ipv4.h"

// #include "ns3/assert.h"
// #include "ns3/fatal-error.h"
// #include "ns3/log.h"
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
namespace open_routing
{

std::ostream&
operator<<(std::ostream& os, const Vertex::ListOfVertex_t& vs)
{
    os << "{";
    for (auto iter = vs.begin(); iter != vs.end();)
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

Vertex::Vertex()
    : m_vertexType(VertexUnknown),
      m_vertexId("255.255.255.255"),
      m_lsa(nullptr),
      m_distanceFromRoot(INFINITY_N),
      m_rootOif(INFINITY_N),
      m_nextHop("0.0.0.0"),
      m_parents(),
      m_children(),
      m_vertexProcessed(false)
{
    // NS_LOG_FUNCTION(this);
}

Vertex::Vertex(LSA* lsa)
    : m_vertexId(lsa->GetLinkStateId()),
      m_lsa(lsa),
      m_distanceFromRoot(INFINITY_N),
      m_rootOif(INFINITY_N),
      m_nextHop("0.0.0.0"),
      m_parents(),
      m_children(),
      m_vertexProcessed(false)
{
    // NS_LOG_FUNCTION(this << lsa);

    if (lsa->GetLSType() == LSA::RouterLSA)
    {
        NS_LOG_LOGIC("Setting m_vertexType to VertexRouter");
        m_vertexType = Vertex::VertexRouter;
    }
    else if (lsa->GetLSType() == LSA::NetworkLSA)
    {
        NS_LOG_LOGIC("Setting m_vertexType to VertexNetwork");
        m_vertexType = Vertex::VertexNetwork;
    }
}

Vertex::~Vertex()
{
    // NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Children vertices - " << m_children);
    NS_LOG_LOGIC("Parent verteices - " << m_parents);

    // find this node from all its parents and remove the entry of this node
    // from all its parents
    for (auto piter = m_parents.begin(); piter != m_parents.end(); piter++)
    {
        // remove the current vertex from its parent's children list. Check
        // if the size of the list is reduced, or the child<->parent relation
        // is not bidirectional
        uint32_t orgCount = (*piter)->m_children.size();
        (*piter)->m_children.remove(this);
        uint32_t newCount = (*piter)->m_children.size();
        if (orgCount > newCount)
        {
            std::cout << "ASSERT: Unable to find the current vertext from its parents --- impossible!" << std::endl;
            // NS_ASSERT_MSG(orgCount > newCount,
            //               "Unable to find the current vertex from its parents --- impossible!");
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
        Vertex* p = m_children.front();
        // 'p' == 0, this child is already deleted by its other parent
        if (p == nullptr)
        {
            continue;
        }
        // NS_LOG_LOGIC("Parent vertex-" << m_vertexId << " deleting its child vertex-"
        //                               << p->GetVertexId());
        delete p;
        p = nullptr;
    }
    m_children.clear();
    // delete parents
    m_parents.clear();
    // delete root exit direction
    m_ecmpRootExits.clear();

    // NS_LOG_LOGIC("Vertex-" << m_vertexId << " completed deleted");
}

Vertex::VertexType
Vertex::GetVertexType() const
{
    // NS_LOG_FUNCTION(this);
    return m_vertexType;
}

void
Vertex::SetVertexType(Vertex::VertexType type)
{
    // NS_LOG_FUNCTION(this << type);
    m_vertexType = type;
}


void
Vertex::SetVertexId(Ipv4Address id)
{
    // NS_LOG_FUNCTION(this << id);
    m_vertexId = id;
}

Ipv4Address
Vertex::GetVertexId() const
{
    // NS_LOG_FUNCTION(this);
    return m_vertexId;
}

LSA*
Vertex::GetLSA() const
{
    // NS_LOG_FUNCTION(this);
    return m_lsa;
}

void
Vertex::SetLSA(LSA* lsa)
{
    // NS_LOG_FUNCTION(this << lsa);
    m_lsa = lsa;
}

uint32_t
Vertex::GetDistanceFromRoot() const
{
    // NS_LOG_FUNCTION(this);
    return m_distanceFromRoot;
}

void
Vertex::SetDistanceFromRoot(uint32_t distance)
{
    // NS_LOG_FUNCTION(this << distance);
    m_distanceFromRoot = distance;
}

void
Vertex::SetRootExitDirection(Ipv4Address nextHop, int32_t id)
{
    // NS_LOG_FUNCTION(this << nextHop << id);

    // always maintain only one root's exit
    m_ecmpRootExits.clear();
    m_ecmpRootExits.emplace_back(nextHop, id);
    // update the following in order to be backward compatitable with
    // GetNextHop and GetOutgoingInterface methods
    m_nextHop = nextHop;
    m_rootOif = id;
}

void
Vertex::SetRootExitDirection(Vertex::NodeExit_t exit)
{
    // NS_LOG_FUNCTION(this << exit);
    SetRootExitDirection(exit.first, exit.second);
}

Vertex::NodeExit_t
Vertex::GetRootExitDirection(uint32_t i) const
{
    // NS_LOG_FUNCTION(this << i);

    // NS_ASSERT_MSG(i < m_ecmpRootExits.size(),
    //               "Index out-of-range when accessing SPFVertex::m_ecmpRootExits!");
    auto iter = m_ecmpRootExits.begin();
    while (i-- > 0)
    {
        iter++;
    }

    return *iter;
}

Vertex::NodeExit_t
Vertex::GetRootExitDirection() const
{
    // NS_LOG_FUNCTION(this);

    // NS_ASSERT_MSG(m_ecmpRootExits.size() <= 1,
    //               "Assumed there is at most one exit from the root to this vertex");
    return GetRootExitDirection(0);
}

void
Vertex::MergeRootExitDirections(const Vertex* vertex)
{
    // NS_LOG_FUNCTION(this << vertex);

    // obtain the external list of exit directions
    //
    // Append the external list into 'this' and remove duplication afterward
    const ListOfNodeExit_t& extList = vertex->m_ecmpRootExits;
    m_ecmpRootExits.insert(m_ecmpRootExits.end(), extList.begin(), extList.end());
    m_ecmpRootExits.sort();
    m_ecmpRootExits.unique();
}


void
Vertex::InheritAllRootExitDirections(const Vertex* vertex)
{
    // NS_LOG_FUNCTION(this << vertex);

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
Vertex::GetNRootExitDirections() const
{
    // NS_LOG_FUNCTION(this);
    return m_ecmpRootExits.size();
}

Vertex*
Vertex::GetParent(uint32_t i) const
{
    // NS_LOG_FUNCTION(this << i);

    // If the index i is out-of-range, return 0 and do nothing
    if (m_parents.size() <= i)
    {
        NS_LOG_LOGIC("Index to SPFVertex's parent is out-of-range.");
        return nullptr;
    }
    auto iter = m_parents.begin();
    while (i-- > 0)
    {
        iter++;
    }
    return *iter;
}


void
Vertex::SetParent(Vertex* parent)
{
    // NS_LOG_FUNCTION(this << parent);

    // always maintain only one parent when using setter/getter methods
    m_parents.clear();
    m_parents.push_back(parent);
}



void
Vertex::MergeParent(const Vertex* v)
{
    // NS_LOG_FUNCTION(this << v);

    // NS_LOG_LOGIC("Before merge, list of parents = " << m_parents);
    // combine the two lists first, and then remove any duplicated after
    m_parents.insert(m_parents.end(), v->m_parents.begin(), v->m_parents.end());
    // remove duplication
    m_parents.sort();
    m_parents.unique();
    // NS_LOG_LOGIC("After merge, list of parents = " << m_parents);
}





void
Vertex::SetParent(Vertex* parent)
{
    // NS_LOG_FUNCTION(this << parent);

    // always maintain only one parent when using setter/getter methods
    m_parents.clear();
    m_parents.push_back(parent);
}

Vertex*
Vertex::GetParent(uint32_t i) const
{
    // NS_LOG_FUNCTION(this << i);

    // If the index i is out-of-range, return 0 and do nothing
    if (m_parents.size() <= i)
    {
        NS_LOG_LOGIC("Index to SPFVertex's parent is out-of-range.");
        return nullptr;
    }
    auto iter = m_parents.begin();
    while (i-- > 0)
    {
        iter++;
    }
    return *iter;
}

uint32_t
Vertex::GetNChildren() const
{
    // NS_LOG_FUNCTION(this);
    return m_children.size();
}

Vertex*
Vertex::GetChild(uint32_t n) const
{
    // NS_LOG_FUNCTION(this << n);
    uint32_t j = 0;

    for (auto i = m_children.begin(); i != m_children.end(); i++, j++)
    {
        if (j == n)
        {
            return *i;
        }
    }
    // NS_ASSERT_MSG(false, "Index <n> out of range.");
    return nullptr;
}

uint32_t
Vertex::AddChild(Vertex* child)
{
    // NS_LOG_FUNCTION(this << child);
    m_children.push_back(child);
    return m_children.size();
}

void
Vertex::SetVertexProcessed(bool value)
{
    // NS_LOG_FUNCTION(this << value);
    m_vertexProcessed = value;
}

bool
Vertex::IsVertexProcessed() const
{
    // NS_LOG_FUNCTION(this);
    return m_vertexProcessed;
}

void
Vertex::ClearVertexProcessed()
{
    // NS_LOG_FUNCTION(this);
    for (uint32_t i = 0; i < this->GetNChildren(); i++)
    {
        this->GetChild(i)->ClearVertexProcessed();
    }
    this->SetVertexProcessed(false);
}

void
Vertex::SetVertexProcessed(bool value)
{
    // NS_LOG_FUNCTION(this << value);
    m_vertexProcessed = value;
}

bool
Vertex::IsVertexProcessed() const
{
    // NS_LOG_FUNCTION(this);
    return m_vertexProcessed;
}

void
Vertex::ClearVertexProcessed()
{
    // NS_LOG_FUNCTION(this);
    for (uint32_t i = 0; i < this->GetNChildren(); i++)
    {
        this->GetChild(i)->ClearVertexProcessed();
    }
    this->SetVertexProcessed(false);
}


}
}