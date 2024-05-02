/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "octopus-headers.h"

#include "ns3/log.h"

namespace ns3
{

OctopusHeader::OctopusHeader()
    : m_reward(0.0)
{
}

TypeId
OctopusHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::OctopusHeader")
                            .SetParent<Header>()
                            .SetGroupName("romam")
                            .AddConstructor<OctopusHeader>();
    return tid;
}

TypeId
OctopusHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
OctopusHeader::Print(std::ostream& os) const
{
    os << "destination: " << m_destination.Get() << ", reward: " << m_reward;
}

uint32_t
OctopusHeader::GetSerializedSize() const
{
    return 13;
}

void
OctopusHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(uint8_t(m_command));
    i.WriteHtonU32(m_destination.Get());
    i.WriteHtonU64(m_reward);
}

uint32_t
OctopusHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_command = i.ReadU8();
    m_destination.Set(i.ReadNtohU32());
    m_reward = i.ReadNtohU64();
    return GetSerializedSize();
}

OctopusHeader::Command_e
OctopusHeader::GetCommand() const
{
    return Command_e(m_command);
}

void
OctopusHeader::SetCommand(Command_e command)
{
    m_command = command;
}

Ipv4Address
OctopusHeader::GetDestination() const
{
    return m_destination;
}

void
OctopusHeader::SetDestination(Ipv4Address destination)
{
    m_destination = destination;
}

double
OctopusHeader::GetReward() const
{
    return m_reward;
}

void
OctopusHeader::SetReward(double reward)
{
    m_reward = reward;
}

std::ostream&
operator<<(std::ostream& os, const OctopusHeader& h)
{
    h.Print(os);
    return os;
}

} // namespace ns3
