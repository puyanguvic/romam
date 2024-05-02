/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef ARM_VALUE_DB_H
#define ARM_VALUE_DB_H

#define STATESIZE 10
#include "database.h"

#include "ns3/core-module.h"

#include <map>
#include <utility>

namespace ns3
{

class ArmValue
{
  public:
    ArmValue();
    ~ArmValue();
    double GetCumulativeLoss() const;
    uint32_t GetNumPulls() const;
    void UpdateArm(double reward);
    void Print(std::ostream& os) const;

  private:
    double m_cumulative;  //!< cumulative loss
    uint32_t m_num_pulls; //!< number of pulls
};

class NeighborArms
{
  public:
    NeighborArms();
    ~NeighborArms();

    ArmValue* GetArmValue(uint32_t nIface) const;
    uint32_t GetNumArmValuePairs() const;
    void UpdateArm(uint32_t nIface, double reward);
    void Print(std::ostream& os) const;

  private:
    typedef std::map<uint32_t, ArmValue*> NAMap_t;   //!< status, statistic*/
    typedef std::pair<uint32_t, ArmValue*> NAPair_t; //!< pair of <interface, StatusUnit>
    NAMap_t m_arms;
};

/**
 * \brief The DGR neighbor status database
 *
 * Each node in DGR maintains a neighbor status data base.
 */
class ArmValueDB : public Database
{
  public:
    /**
     * @brief Construct an empty Neighbor Status Database.
     *
     * The database map composing the Neighbor Status Database in initilaized in
     * this constructor.
     */
    ArmValueDB();

    /**
     * \brief Destroy an empty Neighbor Status Database.
     *
     * Before distroy the database, resources should be release by clear ()
     */
    ~ArmValueDB();

    // Delete copy constructor and assignment operator to avodi misuse.
    ArmValueDB(const ArmValueDB&) = delete; // Disallow copying
    ArmValueDB& operator=(const ArmValueDB&) = delete;

    /**
     * \brief Get the NeighborStatusEntry of a Interface
     *
     * \param iface The interface number
     * \return NeighborStatusEntry*
     */
    NeighborArms* GetNeighborArms(uint32_t iface) const;

    ArmValue* GetArmValue(uint32_t iface, uint32_t nIface) const;
    void UpdateArm(uint32_t iface, uint32_t nIface, double reward);

    /**
     * \brief Print the database
     *
     */
    void Print(std::ostream& os) const override;

  private:
    typedef std::map<uint32_t, NeighborArms*> ArmValueDBMap_t;
    typedef std::pair<uint32_t, NeighborArms*> ArmValueDBPair_t;
    ArmValueDBMap_t m_database; //!< database of <interface, NeighborArms>
};

} // namespace ns3

#endif /* ARM_VALUE_DB_H */