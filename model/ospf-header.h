#ifndef OSPF_HEADER_H
#define OSPF_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"


namespace ns3
{
namespace open_routing
{

/**
 * \ingroup open_routing
 *
 * \brief OSPF packet header
 * 
 * OSPF packets are classified to five types that have the same packet header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Version #   |       Type    |         Packet length         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Router ID                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           Area ID                             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |           Checksum            |             AuType            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Authentication                          |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
class OSPFHeader : public Header
{
  public:
     /**
     * \brief Construct a null OSPF header
     * Initializes a new OSPF header with default values for each field.
     */
    OSPFHeader();

    ~OSPFHeader () override;

    /**
     * \brief Set the type of OSPF message
     * 
     * @param type The OSPF message type to be set.
     */
    void SetType(uint8_t type);

    /**
     * \brief Get the type of OSPF message
     * 
     * @return uint8_t The current OSPF message type.
     */
    uint8_t GetType() const;

    /**
     * \brief Set the length of the OSPF message
     * 
     * @param length The length of the OSPF message to be set.
     */
    void SetLength(uint16_t length);

    /**
     * \brief Get the length of the OSPF message
     * 
     * @return uint16_t The current length of the OSPF message.
     */
    uint16_t GetLength() const;

    /**
     * \brief Set the Router ID
     * 
     * @param routerId The Router ID to be set.
     */
    void SetRouterID(uint32_t routerId);

    /**
     * \brief Get the Router ID
     * 
     * @return uint32_t The current Router ID.
     */
    uint32_t GetRouterID() const;

    /**
     * \brief Set the Area ID
     * 
     * @param areaId The Area ID to be set.
     */
    void SetAreaID(uint32_t areaId);

    /**
     * \brief Get the Area ID
     * 
     * @return uint32_t The current Area ID.
     */
    uint32_t GetAreaID() const;

    /**
     * \brief Set the checksum for error checking
     * 
     * @param checksum The checksum value to be set.
     */
    void SetChecksum(uint16_t checksum);

    /**
     * \brief Get the checksum
     * 
     * @return uint16_t The current checksum value.
     */
    uint16_t GetChecksum() const;

    /**
     * \brief Set the Authentication type
     * 
     * @param auType The Authentication type to be set.
     */
    void SetAuType(uint16_t auType);

    /**
     * \brief Get the Authentication type
     * 
     * @return uint16_t The current Authentication type.
     */
    uint16_t GetAuType() const;

    /**
     * \brief Set the Authentication data
     * 
     * @param authentication The 64-bit Authentication data to be set.
     */
    void SetAuthentication(uint64_t authentication);

    /**
     * \brief Get the Authentication data
     * 
     * @return uint64_t The current 64-bit Authentication data.
     */
    uint64_t GetAuthentication() const;

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

private:
    uint8_t m_version;          //!< OSPF version number, which is 2 for OSPFv2
    uint8_t m_type;             //!< OSPF packet type from 1 to 5, corresponding to hello, DD, LSR, LSU, and LSAck, respectively
    uint16_t m_length;          //!< Total length of the OSPF packet in bytes, including the header
    uint32_t m_routerId;        //!< ID of the advertising router
    uint32_t m_areaId;          //!< ID of the area where the advertising router resides
    uint16_t m_checksum;        //!< Checksum of the message
    // Authentication type, ranging from 0 to 2, corresponding to non-authentication,
    // simple (plaintext) authentication, and MD5 authentication, respectively
    uint16_t m_AuType;          
    // Information determined by authentication type. It is not defined for authentication type 0.
    // It is defined as passward information for authentication type 1, and defined as Key ID, MD5
    // authentication data length, and sequence number for authentication type 2.
    uint64_t m_Authentication;  
};


/**
 * \ingroup open_routing
 *
 * \brief Hello Packet header for OSPF
 * 
 * A router sends hello packets periodically to find and maintain neighbr
 * relationships, and to elect the DR or BDR, including information about
 * values of timers, DR, BDR, and neighbors that are already known.
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Network Mask                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         HelloInterval         |    Options    |    Rtr Pri    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     RouterDeadInterval                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Designted Router                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                   Backup Designated Router                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Neighbor                             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
class HelloHeader : public Header 
{
public:
  HelloHeader ();
  ~HelloHeader ();

  // ns-3 type declaration
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  // Override Header methods
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual uint32_t GetSerializedSize () const;
  virtual void Print (std::ostream &os) const;

  // Setters
  void SetNetworkMask (Ipv4Address mask);
  void SetHelloInterval (uint16_t interval);
  void SetOptions (uint8_t options);
  void SetRouterPriority (uint8_t priority);
  void SetRouterDeadInterval (uint32_t interval);
  void SetDesignatedRouter (Ipv4Address address);
  void SetBackupDesignatedRouter (Ipv4Address address);
  // Add methods to handle the Neighbor field

  // Getters
  Ipv4Address GetNetworkMask () const;
  uint16_t GetHelloInterval () const;
  uint8_t GetOptions () const;
  uint8_t GetRouterPriority () const;
  uint32_t GetRouterDeadInterval () const;
  Ipv4Address GetDesignatedRouter () const;
  Ipv4Address GetBackupDesignatedRouter () const;
  // Add methods to retrieve the Neighbor field

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
  static TypeId GetTypeId();
  TypeId GetInstanceTypeId() const override;
  void Print(std::ostream& os) const override;
  uint32_t GetSerializedSize() const override;
  void Serialize(Buffer::Iterator start) const override;uint32_t Deserialize(Buffer::Iterator start) override;

private:
  // Network mask associated with the router's sending interface. If
  // two routers have different network masks, they cannot become neighbors.
  Ipv4Address m_networkMask;      
  
  // Interval for sending hello packets. If two routers have different intervals,
  // they cannot become neighbors.
  uint16_t m_helloInterval;

  uint8_t m_options;
  
  // Rtr Pri, Router priority. A value of 0 means the router cannot become the DR or BDR.
  uint8_t m_routerPriority;
  
  // Time before declaring a silent router down. If two routers have different
  // dead intervals, they cannot become neighbors.
  uint32_t m_routerDeadInterval;
  
  // IP address of DR
  Ipv4Address m_designatedRouter;
  
  // IP address of BDR
  Ipv4Address m_backupDesignatedRouter;

  typedef std::list<Ipv4Address> ListOfNeighbors_t;
  // Neighbor, Router ID of the neighbor router.
  ListOfNeighbors_t m_neighbors;
};

/**
 * \ingroup open_routing
 *
 * \brief Database Descriptor (DBD) header for OSPF
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Interface MTU         |    Options    |0|0|0|0|0|I|M|MS
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     DD sequence number                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           LSA Header                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           LSA Header                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 */

class DBDHeader : public Header{
public:
    // Setters and Getters for each field
    void SetInterfaceMTU(uint16_t mtu) { interfaceMTU = mtu; }
    uint16_t GetInterfaceMTU() const { return interfaceMTU; }

    void SetOptions(uint8_t opts) { options = opts; }
    uint8_t GetOptions() const { return options; }

    void SetFlags(uint8_t flgs) { flags = flgs; }
    uint8_t GetFlags() const { return flags; }

    void SetDDSequenceNumber(uint32_t seqNum) { ddSequenceNumber = seqNum; }
    uint32_t GetDDSequenceNumber() const { return ddSequenceNumber; }

    // Methods to handle LSA Headers (e.g., adding, retrieving)
    // ...

private:
    uint16_t interfaceMTU;      // Interface Maximum Transmission Unit
    uint8_t options;            // OSPF-specific options
    uint8_t flags;              // Flags (Init, More, Master/Slave)
    uint32_t ddSequenceNumber;  // Database Descriptor sequence number
    // Container for LSA Headers
    // std::vector<LSAHeader> lsaHeaders;
};


/**
 * \ingroup open_routing
 *
 * \brief Link State Request (LSR) header for OSPF
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          LS type                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Link state ID                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Advertising router                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                             ...                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 */

/**
 * \ingroup open_routing
 *
 * \brief Link State Update (LSU) header for OSPF
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Number of LSAs                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              LSA                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              LSA                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 */

/**
 * \ingroup open_routing
 *
 * \brief Link State Acknowledgment (LSAck) packet are used to 
 * acknowledge received LSU packets. An LSAck packet carries  the
 * headers of LSAs to be acknowledged
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           LSA header                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           LSA header                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 */

/**
 * \ingroup open_routing
 *
 * \brief LSA header format
 * All Link State Advertisements (LSAs) have the same header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |        LS age                 |  Options      |   LS type     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Link State ID                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Advertising router                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       LS sequence number                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      LS checksum              |             Length            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/**
 * \ingroup open_routing
 *
 * \brief Router LSAs format
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |    0    |V|E|B|       0       |             # Links           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                            Link ID                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           Link data                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      Type     |     #TOS      |             Metrix            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */


/**
 * \ingroup open_routing
 *
 * \brief Network LSAs format
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Network Mask                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Attached router                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/**
 * \ingroup open_routing
 *
 * \brief Summary LSAs format
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Network Mask                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Attached router                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/**
 * \ingroup open_routing
 *
 * \brief AS external LSA format
 * 
 * An AS external LSA is originated by an ASBR, and describes routing
 * information to a destination outside the AS.
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Network Mask                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |E|     TOS     |                 TOS  Metrix                   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Forwarding address                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      External route tag                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */


/**
 * \ingroup open_routing
 *
 * \brief NSSA external LSA format
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Network Mask                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |E|     TOS     |                 TOS  Metrix                   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Forwarding address                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      External route tag                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 */

} // namespace open_routing
} // namespace ns3

#endif /* OSPF_HEADER_H */
