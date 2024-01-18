#include "ospf-header.h"
#include "ns3/log.h"


namespace ns3
{

NS_LOG_COMPONENT_DEFINE("OSPFHeader");

namespace open_routing
{

// OSPFHeader::OSPFHeader() : m_version(2), // OSPF version 2 for IPv4
//                            m_type(0),
//                            m_length(0),
//                            m_routerId(0),
//                            m_areaId(0),
//                            m_checksum(0),
//                            m_AuType(0),
//                            m_Authentication(0)
// {
//     // Default Constructor Body (if necessary)
// }

OSPFHeader::OSPFHeader ()
{

}

OSPFHeader::~OSPFHeader ()
{

}

// void
// OSPFHeader::SetType(uint8_t type)
// {
//     m_type = type;
// }

// uint8_t
// OSPFHeader::GetType() const
// {
//     return m_type;
// }

// void
// OSPFHeader::SetLength(uint16_t length)
// {
//     m_length = length;
// }

// uint16_t
// OSPFHeader::GetLength() const
// {
//     return m_length;
// }

// void OSPFHeader::SetRouterID(uint32_t routerId)
// {
//     m_routerId = routerId;
// }

// uint32_t OSPFHeader::GetRouterID() const
// {
//     return m_routerId;
// }

// void OSPFHeader::SetAreaID(uint32_t areaId)
// {
//     m_areaId = areaId;
// }

// uint32_t OSPFHeader::GetAreaID() const
// {
//     return m_areaId;
// }

// void OSPFHeader::SetChecksum(uint16_t checksum)
// {
//     m_checksum = checksum;
// }

// uint16_t OSPFHeader::GetChecksum() const
// {
//     return m_checksum;
// }

// void OSPFHeader::SetAuType(uint16_t auType)
// {
//     m_AuType = auType;
// }

// uint16_t OSPFHeader::GetAuType() const
// {
//     return m_AuType;
// }

// void OSPFHeader::SetAuthentication(uint64_t authentication)
// {
//     m_Authentication = authentication;
// }

// uint64_t OSPFHeader::GetAuthentication() const
// {
//     return m_Authentication;
// }

// TypeId 
// OSPFHeader::GetTypeId()
// {
//     static TypeId tid = TypeId("ns3::open_routing::OSPFHeader")
//         .SetParent<Header>()
//         .SetGroupName("open_routing")
//         .AddConstructor<OSPFHeader>();
//     return tid;
// }

// TypeId
// OSPFHeader::GetInstanceTypeId() const
// {
//     // NS_LOG_FUNCTION(this);
//     return GetTypeId();
// }

// void
// OSPFHeader::Print(std::ostream &os) const
// {
//     // NS_LOG_FUNCTION(this << &os);
//     os << "OSPF Header: Type=" << (uint32_t)m_type
//        << ", Length=" << m_length
//        << ", RouterID=" << m_routerId
//        << ", AreaID=" << m_areaId
//        << ", Checksum=" << m_checksum
//        << ", AuType=" << m_AuType
//        << ", Authentication=" << m_Authentication << std::endl;
// }

// uint32_t
// OSPFHeader::GetSerializedSize() const
// {
//     // Size of OSPF header in bytes
//     return 24; // Adjust this based on actual header structure
// }

// void OSPFHeader::Serialize(Buffer::Iterator start) const
// {
//     // NS_LOG_FUNCTION(this << &start);
//     start.WriteU8(m_version);
//     start.WriteU8(m_type);
//     start.WriteHtonU16(m_length);
//     start.WriteHtonU32(m_routerId);
//     start.WriteHtonU32(m_areaId);
//     start.WriteHtonU16(m_checksum);
//     start.WriteHtonU16(m_AuType);
//     start.WriteHtonU64(m_Authentication);
// }

// uint32_t
// OSPFHeader::Deserialize(Buffer::Iterator start)
// {
//     m_version = start.ReadU8();
//     m_type = start.ReadU8();
//     m_length = start.ReadNtohU16();
//     m_routerId = start.ReadNtohU32();
//     m_areaId = start.ReadNtohU32();
//     m_checksum = start.ReadNtohU16();
//     m_AuType = start.ReadNtohU16();
//     m_Authentication = start.ReadNtohU64();

//     return GetSerializedSize();
// }

} // namespace open_routing
} // namespace ns3