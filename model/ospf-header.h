#ifndef OSPF_HEADER_H
#define OSPF_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"


namespace ns3
{
namespace open_routing
{

class LSAHeader;
class LSA;

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
  HelloHeader (); //!< Construct a null Hello Header
  ~HelloHeader () override;

  void SetNetworkMask (Ipv4Address mask); //!< Set the Network Mask for the interface
  Ipv4Address GetNetworkMask () const;    //!< Get the Network Mask of the interface

  void SetHelloInterval (uint16_t interval); //!< Set the Hello Interval
  uint16_t GetHelloInterval () const;        //!< Get the Hello Interval

  void SetOptions (uint8_t options);     //!< Set OSPF options
  uint8_t GetOptions () const;           //!< Get OSPF options

  void SetRouterPriority (uint8_t priority); //!< Set the Router Priority
  uint8_t GetRouterPriority () const;        //!< Get the Router Priority

  void SetRouterDeadInterval (uint32_t interval); //!< Set Router Dead Interval
  uint32_t GetRouterDeadInterval () const;        //!< Get Router Dead Interval

  void SetDesignatedRouter (Ipv4Address address); //!< Set the Designated Router address
  Ipv4Address GetDesignatedRouter () const;       //!< Get the Designated Router address

  void SetBackupDesignatedRouter (Ipv4Address address); //!< Set the Backup Designated Router address
  Ipv4Address GetBackupDesignatedRouter () const;       //!< Get the Backup Designated Router address

  void AddNeighbor(Ipv4Address addr);              //!< Add a neighbor router
  void ClearNeighbors ();                          //!< Clear the list of neighbor routers
  uint16_t GetNNeighbors () const;                 //!< Get the number of neighbor routers
  Ipv4Address GetNeighbor(uint16_t n) const;       //!< Get a specific neighbor router by index

  static TypeId GetTypeId();
  TypeId GetInstanceTypeId() const override;
  void Print(std::ostream& os) const override;
  uint32_t GetSerializedSize() const override;
  void Serialize(Buffer::Iterator start) const override;
  uint32_t Deserialize(Buffer::Iterator start) override;

private:
  Ipv4Address m_networkMask;              //!< Network mask of the router's interface
  uint16_t m_helloInterval;               //!< Interval for sending Hello packets
  uint8_t m_options;                      //!< OSPF options for the Hello packet
  uint8_t m_routerPriority;               //!< Priority of the router in the OSPF network
  uint32_t m_routerDeadInterval;          //!< Time before declaring a silent router down
  Ipv4Address m_designatedRouter;         //!< IP address of the Designated Router
  Ipv4Address m_backupDesignatedRouter;   //!< IP address of the Backup Designated Router
  std::list<Ipv4Address> m_neighbors;     //!< List of neighbor routers
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
class DDHeader : public Header{
public:
    DDHeader ();
    ~DDHeader ();
    
    void SetInterfaceMTU(uint16_t mtu);
    uint16_t GetInterfaceMTU() const;

    void SetOptions(uint8_t opts);
    uint8_t GetOptions() const;

    void SetFlags(uint8_t flgs);
    uint8_t GetFlags() const;

    void SetDDSequenceNumber(uint32_t seqNum);
    uint32_t GetDDSequenceNumber() const;

    void AddLSAHeader(LSAHeader lsaHeader);
    void ClearLSAHeaders ();
    uint16_t GetNLSAHeaders () const;
    LSAHeader GetLSAHeader(uint16_t n) const;

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
    uint16_t interfaceMTU;      // Interface Maximum Transmission Unit
    uint8_t options;            // OSPF-specific options
    uint8_t flags;              // Flags (Init, More, Master/Slave)
    uint32_t ddSequenceNumber;  // Database Descriptor sequence number
    typedef std::list<LSAHeader> ListOfLSAHeaders_t;
    // Neighbor, Router ID of the neighbor router.
    ListOfLSAHeaders_t m_LSAHeaders;
};


/**
 * \ingroup open_routing
 *
 * \brief Link State Request (LSR) header for OSPF
 * 
 * After exchanging DD packets, two routers know which LSAs of the peer
 * are missing from the local LSDB. Then, they send (link state request)
 * LSR packets to request the missing LSAs. An LSR packet contains the 
 * brief of the missing LSAs.
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
class LSRHeader : public Header{
public:
    LSRHeader ();
    ~LSRHeader ();
    
    void SetlsType(uint32_t lsType);
    uint32_t GetlsType() const;
    
    void SetLinkStateId (uint32_t linkStateId);
    uint32_t GetLinkStateId () const;

    void SetAdvertisingRouter (Ipv4Address ipv4);
    Ipv4Address GetAdvertisingRouter () const;

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
    uint32_t m_lsType; //!< Type of the LSA to be requested. Type 1 for example indicate the Router LSA
    uint32_t m_linkStateId; //!< Determined by LSA type.
    Ipv4Address m_advertisingRouter; //!< ID of the router that sent the LSA.
};

/**
 * \ingroup open_routing
 *
 * \brief Link State Update (LSU) header for OSPF
 * 
 * LSU (Link State Update) packets are used to send the requested LSAs
 * to the peer. Each packet carries a collection of LSAs.
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
class LSU : public Header{
public:
    LSU ();
    ~LSU ();

    void SetNLSAs (uint32_t num);
    uint32_t GetNLSAs () const;

    void AddLSA(LSA lsa);
    void ClearLSAs ();
    LSAHeader GetLSA(uint16_t n) const;

private:
    uint32_t m_nLSAs; //!< Number of LSAs
    typedef std::list<LSA> ListOfLSAs_t;
    ListOfLSAs_t m_LSAs; //!< List of LSAs
};

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
class LSAck : public Header {
public:
    LSAck ();
    ~LSAck ();

    uint32_t GetNLSAHeaders () const;
    void AddLSAHeader(LSAHeader lsaHeader);
    void ClearLSAHeaders ();
    LSAHeader GetLSAHeader(uint16_t n) const;

private:
    typedef std::list<LSAHeader> ListOfLSAHeaders_t;
    ListOfLSAHeaders_t m_LSAHeaders; //!< List of LSAs
};

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
class LSAHeader : public Header {
public:
    LSAHeader ();
    ~LSAHeader ();

private:
    uint16_t m_lsAge;
    uint8_t m_options;
    uint8_t m_lsType;
    uint32_t m_linkStateId;
    uint32_t m_advertisingRouter;
    uint32_t m_lsSequenceNumber;
    uint16_t m_lsChecksum;
    uint16_t m_length;
};

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
class RouterLSA : public Header {
public:
    RouterLSA ();
    ~RouterLSA ();

private:
    // uinfinished bits
    uint32_t m_linkId;
    uint32_t m_linkData;
    uint8_t m_type;
    uint8_t m_tos;
    uint16_t m_metrix;
};

/**
 * \ingroup open_routing
 *
 * \brief Network LSAs format
 * 
 * The Link state ID of a network LSA is the interface address of the DR
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
class NetworkLSA : public Header {
public:

private:
    Ipv4Address m_networkMask;
    Ipv4Address m_attachedRouter;
};


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
