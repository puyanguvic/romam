/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "arm-value-db.h"

#include "ns3/log.h"

namespace ns3
{

ValueUnit::ValueUnit()
    : m_cumulative(0.0),
      m_num_pulls(0)
{
}

ValueUnit::~ValueUnit()
{
}

double
ValueUnit::GetCumulativeLoss() const
{
    return m_cumulative;
}

uint32_t
ValueUnit::GetNumPulls() const
{
    return m_num_pulls;
}

void
ValueUnit::UpdateArm(double reward)
{
    // Todo: Update values;
    m_num_pulls += 1;
    m_cumulative += reward;
}

void
ValueUnit::Print(std::ostream& os) const
{
    os << "Cumulative loss = " << GetCumulativeLoss() << ", Number of Pulls = " << GetNumPulls()
       << std::endl;
}

//----------------------------------------------------------------------
//-- NeighborArms
//------------------------------------------------------
NeighborArms::NeighborArms()
    : m_arms()
{
}

NeighborArms::~NeighborArms()
{
    for (auto it = m_arms.begin(); it != m_arms.end(); it++)
    {
        NS_LOG_LOGIC("Free database entries");
        ValueUnit* temp = it->second;
        delete temp;
    }
    m_arms.clear();
}

ValueUnit*
NeighborArms::GetValueUnit(uint32_t nIface) const
{
    NAMap_t::const_iterator ci = m_arms.find(nIface);
    if (ci != m_arms.end())
    {
        return ci->second;
    }
    return nullptr;
}

uint32_t
NeighborArms::GetNumArmValuePairs() const
{
    return m_arms.size();
}

void
NeighborArms::UpdateArm(uint32_t nIface, double reward)
{
    auto it = m_arms.find(nIface);
    if (it == m_arms.end())
    {
        ValueUnit* vu = new ValueUnit();
        m_arms.insert(NAPair_t(nIface, vu));
        vu->UpdateArm(reward);
    }
    it->second->UpdateArm(reward);
}

void
NeighborArms::Print(std::ostream& os) const
{
    os << "Next_Iface    ValueUnit" << std::endl;
    NAMap_t::const_iterator ci;
    for (ci = m_arms.begin(); ci != m_arms.end(); ci++)
    {
        os << ci->first << "    ";
        ci->second->Print(os);
    }
}

//----------------------------------------------------------------------
//-- ArmValueDB
//------------------------------------------------------
// NS_OBJECT_ENSURE_REGISTERED(ArmValueDB);

ArmValueDB::ArmValueDB()
    : m_database()
{
    // NS_LOG_FUNCTION(this);
}

ArmValueDB::~ArmValueDB()
{
    // NS_LOG_LOGIC ("Clear map");
    for (auto it = m_database.begin(); it != m_database.end(); it++)
    {
        NS_LOG_LOGIC("Free database entries");
        NeighborArms* temp = it->second;
        delete temp;
    }

    m_database.clear();
}

NeighborArms*
ArmValueDB::GetNeighborArms(uint32_t iface) const
{
    // NS_LOG_FUNCTION (this << iface);
    ArmValueDBMap_t::const_iterator ci = m_database.find(iface);
    if (ci != m_database.end())
    {
        return ci->second;
    }
    return nullptr;
}

ValueUnit*
ArmValueDB::GetValueUnit(uint32_t iface, uint32_t nIface) const
{
    // NS_LOG_FUNCTION (this << iface);
    //
    // Look up a NSE by it's interface.
    //
    ArmValueDBMap_t::const_iterator ci = m_database.find(iface);
    if (ci != m_database.end())
    {
        return ci->second->GetValueUnit(nIface);
    }
    return nullptr;
}

void
ArmValueDB::UpdateArm(uint32_t iface, uint32_t nIface, double reward)
{
    auto it = m_database.find(iface);
    if (it != m_database.end())
    {
        it->second->UpdateArm(nIface, reward);
    }
    else
    {
        NeighborArms* neighbor = new NeighborArms();
        m_database.insert(ArmValueDBPair_t(iface, neighbor));
        neighbor->UpdateArm(nIface, reward);
    }
}

void
ArmValueDB::Print(std::ostream& os) const
{
    ArmValueDBMap_t::const_iterator ci;
    for (ci = m_database.begin(); ci != m_database.end(); ci++)
    {
        os << "Interface = " << ci->first << std::endl;
        ci->second->Print(os);
    }
}

} // namespace ns3