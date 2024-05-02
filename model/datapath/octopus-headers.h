/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OCTOPUS_HEADERS_H
#define OCTOPUS_HEADERS_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/packet.h"

#include <list>

namespace ns3
{

/**
 * \ingroup dgr
 * \brief dgr header
 */
class OctopusHeader : public Header
{
  public:
    OctopusHeader();

    /**
     * \brief Get the type ID
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * \brief Return the instance type identifier.
     * \return the object TypeId
     */
    TypeId GetInstanceTypeId() const override;

    void Print(std::ostream& os) const override;

    /**
     * \brief Get the serialized size of the packet
     * \return size
     */
    uint32_t GetSerializedSize() const override;

    /**
     * \brief Serialize the packet.
     * \param start Buffer iterator
     */
    void Serialize(Buffer::Iterator start) const override;

    /**
     * \brief Deserialize the packet
     * \param start Buffer iterator
     * \return size of the packet
     */
    uint32_t Deserialize(Buffer::Iterator start) override;

    enum Command_e
    {
        ACK = 0x1,
        REQUEST = 0x2
    };

    Command_e GetCommand() const;
    void SetCommand(Command_e command);
    Ipv4Address GetDestination() const;
    void SetDestination(Ipv4Address destination);
    double GetReward() const;
    void SetReward(double reward);

  private:
    uint8_t m_command;
    Ipv4Address m_destination;
    double m_reward; //!< command type
};

/**
 * \brief Stream insertion operator
 *
 * \param os the reference to the output stream
 * \param h the DGR header
 * \returns the reference to the output stream
 */
std::ostream& operator<<(std::ostream& os, const OctopusHeader& h);

} // namespace ns3

#endif /* OCTOPUS_HEADERS_H */
