#include "ospf-header.h"
#include "ns3/log.h"


namespace ns3
{

// NS_LOG_COMPONENT_DEFINE("open_routing::OSPFHeader");

namespace open_routing
{

//------------------------------------------
//---------- OSPFHeader
//------------------------------------------

OSPFHeader::OSPFHeader() : m_version(2), // OSPF version 2 for IPv4
                           m_type(0),
                           m_length(0),
                           m_routerId(0),
                           m_areaId(0),
                           m_checksum(0),
                           m_AuType(0),
                           m_Authentication(0)
{

}

OSPFHeader::~OSPFHeader ()
{

}

void
OSPFHeader::SetType(uint8_t type)
{
    m_type = type;
}

uint8_t
OSPFHeader::GetType() const
{
    return m_type;
}

void
OSPFHeader::SetLength(uint16_t length)
{
    m_length = length;
}

uint16_t
OSPFHeader::GetLength() const
{
    return m_length;
}

void OSPFHeader::SetRouterID(uint32_t routerId)
{
    m_routerId = routerId;
}

uint32_t OSPFHeader::GetRouterID() const
{
    return m_routerId;
}

void OSPFHeader::SetAreaID(uint32_t areaId)
{
    m_areaId = areaId;
}

uint32_t OSPFHeader::GetAreaID() const
{
    return m_areaId;
}

void OSPFHeader::SetChecksum(uint16_t checksum)
{
    m_checksum = checksum;
}

uint16_t OSPFHeader::GetChecksum() const
{
    return m_checksum;
}

void OSPFHeader::SetAuType(uint16_t auType)
{
    m_AuType = auType;
}

uint16_t OSPFHeader::GetAuType() const
{
    return m_AuType;
}

void OSPFHeader::SetAuthentication(uint64_t authentication)
{
    m_Authentication = authentication;
}

uint64_t OSPFHeader::GetAuthentication() const
{
    return m_Authentication;
}

TypeId 
OSPFHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::open_routing::OSPFHeader")
        .SetParent<Header>()
        .SetGroupName("open_routing")
        .AddConstructor<OSPFHeader>();
    return tid;
}

TypeId
OSPFHeader::GetInstanceTypeId() const
{
    // NS_LOG_FUNCTION(this);
    return GetTypeId();
}

void
OSPFHeader::Print(std::ostream &os) const
{
    // NS_LOG_FUNCTION(this << &os);
    os << "OSPF Header: Type=" << (uint32_t)m_type
       << ", Length=" << m_length
       << ", RouterID=" << m_routerId
       << ", AreaID=" << m_areaId
       << ", Checksum=" << m_checksum
       << ", AuType=" << m_AuType
       << ", Authentication=" << m_Authentication << std::endl;
}

uint32_t
OSPFHeader::GetSerializedSize() const
{
    // Size of OSPF header in bytes
    return 24; // Adjust this based on actual header structure
}

void OSPFHeader::Serialize(Buffer::Iterator start) const
{
    // NS_LOG_FUNCTION(this << &start);
    start.WriteU8(m_version);
    start.WriteU8(m_type);
    start.WriteHtonU16(m_length);
    start.WriteHtonU32(m_routerId);
    start.WriteHtonU32(m_areaId);
    start.WriteHtonU16(m_checksum);
    start.WriteHtonU16(m_AuType);
    start.WriteHtonU64(m_Authentication);
}

uint32_t
OSPFHeader::Deserialize(Buffer::Iterator start)
{
    m_version = start.ReadU8();
    m_type = start.ReadU8();
    m_length = start.ReadNtohU16();
    m_routerId = start.ReadNtohU32();
    m_areaId = start.ReadNtohU32();
    m_checksum = start.ReadNtohU16();
    m_AuType = start.ReadNtohU16();
    m_Authentication = start.ReadNtohU64();

    return GetSerializedSize();
}

//------------------------------------------
//---------- HelloHeader
//------------------------------------------

HelloHeader::HelloHeader() 
    : m_networkMask(), // Initialize with default Ipv4Address
      m_helloInterval(0),
      m_options(0),
      m_routerPriority(0),
      m_routerDeadInterval(0),
      m_designatedRouter(), // Initialize with default Ipv4Address
      m_backupDesignatedRouter() // Initialize with default Ipv4Address
{

}

HelloHeader::~HelloHeader()
{

}

void
HelloHeader::SetNetworkMask(Ipv4Address mask)
{
    m_networkMask = mask;
}

Ipv4Address
HelloHeader::GetNetworkMask() const
{
    return m_networkMask;
}

void
HelloHeader::SetHelloInterval(uint16_t interval)
{
    m_helloInterval = interval;
}

uint16_t
HelloHeader::GetHelloInterval() const
{
    return m_helloInterval;
}

void
HelloHeader::SetOptions(uint8_t options)
{
    m_options = options;
}

uint8_t
HelloHeader::GetOptions() const
{
    return m_options;
}

void
HelloHeader::SetRouterPriority (uint8_t priority)
{
    m_routerPriority = priority;
}

uint8_t
HelloHeader::GetRouterPriority () const
{
    return m_routerPriority;
}

void
HelloHeader::SetRouterDeadInterval (uint32_t interval)
{
    m_helloInterval = interval;
}

uint32_t
HelloHeader::GetRouterDeadInterval () const
{
    return m_helloInterval;
}

void
HelloHeader::SetDesignatedRouter (Ipv4Address addr)
{
    m_designatedRouter = addr;
}

Ipv4Address
HelloHeader::GetDesignatedRouter () const
{
    return m_designatedRouter;
}

void
HelloHeader::SetBackupDesignatedRouter (Ipv4Address addr)
{
    m_backupDesignatedRouter = addr;
}

Ipv4Address
HelloHeader::GetBackupDesignatedRouter () const
{
    return m_backupDesignatedRouter;
}

void
HelloHeader::AddNeighbor(Ipv4Address addr)
{
    m_neighbors.push_back (addr);
}

void
HelloHeader::ClearNeighbors ()
{
    m_neighbors.clear ();
}

uint16_t
HelloHeader::GetNNeighbors () const
{
    return m_neighbors.size ();
}

TypeId
HelloHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::open_routing::HelloHeader")
        .SetParent<Header>()
        .SetGroupName("open_routing")
        .AddConstructor<HelloHeader>();
    return tid;
}

TypeId
HelloHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
HelloHeader::Print(std::ostream &os) const
{
    os << "Hello Header: Network Mask=" << m_networkMask << "\n"
       << ", Hello Interval=" << m_helloInterval
       << ", Options=" << (uint32_t)m_options
       << ", Router Priority=" << (uint32_t)m_routerPriority << "\n"
       << ", Dead Interval=" << m_routerDeadInterval <<"\n"
       << ", Designated Router=" << m_designatedRouter << "\n"
       << ", Backup Designated Router=" << m_backupDesignatedRouter << "\n"
       << "The neighbor's addresses" << std::endl;

    for (std::list<Ipv4Address>::const_iterator iter = m_neighbors.begin ();
         iter != m_neighbors.end ();
         iter ++)
        {
           iter->Print (os);
           os << ", ";
        }

}

uint32_t
HelloHeader::GetSerializedSize() const
{
    // The size of HelloHeader is 20 bytes + with Neighbor IpAddresses size
    return 20 + 4 * m_neighbors.size ();
}

void
HelloHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteHtonU32 (m_networkMask.Get());
    i.WriteHtolsbU16 (m_helloInterval);
    i.WriteU8 (m_options);
    i.WriteU8 (m_routerPriority);
    i.WriteHtonU32 (m_routerDeadInterval);
    i.WriteHtonU32 (m_designatedRouter.Get ());
    i.WriteHtonU32 (m_backupDesignatedRouter.Get ());

    for (std::list<Ipv4Address>::const_iterator iter = m_neighbors.begin ();
         iter != m_neighbors.end ();
         iter ++)
        {
           i.WriteHtonU32 (iter->Get ());
        }
}

uint32_t
HelloHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    
    m_networkMask = Ipv4Address(i.ReadNtohU32());
    m_helloInterval = i.ReadNtohU16();
    m_options = i.ReadU8();
    m_routerPriority = i.ReadU8();
    m_routerDeadInterval = i.ReadNtohU32();
    m_designatedRouter = Ipv4Address(i.ReadNtohU32());
    m_backupDesignatedRouter = Ipv4Address(i.ReadNtohU32());

    // Clear existing neighbors
    m_neighbors.clear();

    // Calculate the number of neighbors from the remaining size
    uint32_t remainingSize = i.GetRemainingSize (); // 20 bytes for the fixed part
    uint32_t neighborCount = remainingSize / 4;
    for (uint32_t j = 0; j < neighborCount; ++j)
    {
        Ipv4Address neighborAddress(i.ReadNtohU32());
        m_neighbors.push_back(neighborAddress);
    }

    return GetSerializedSize();
}


} // namespace open_routing
} // namespace ns3