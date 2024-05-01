/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "octopus-headers.h"

#include "ns3/log.h"

namespace ns3
{

OctopusHeader::OctopusHeader()
    : m_interface(0),
      m_delay(0.0)
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
    os << "delay: " << m_delay << ", interface: " << m_interface;
}

uint32_t
OctopusHeader::GetSerializedSize() const
{
    return 12;
}

void
OctopusHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteHtolsbU32(m_interface);
    i.WriteHtolsbU64(m_delay);
}

uint32_t
OctopusHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_interface = i.ReadU32();
    m_delay = i.ReadU64();
    return GetSerializedSize();
}

uint32_t
OctopusHeader::GetInterface() const
{
    return m_interface;
}

void
OctopusHeader::SetInterface(uint32_t interface)
{
    m_interface = interface;
}

double
OctopusHeader::GetDelay() const
{
    return m_delay;
}

void
OctopusHeader::SetDelay(double delay)
{
    m_delay = delay;
}

std::ostream&
operator<<(std::ostream& os, const OctopusHeader& h)
{
    h.Print(os);
    return os;
}

} // namespace ns3
