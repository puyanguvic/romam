#ifndef OSPF_HEADER_H
#define OSPF_HEADER_H

#include "ns3/header.h"

namespace ns3
{
namespace open_routing
{
/**
 * \ingroup OSPF
 *
 * \brief Packet header for OSPF
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

//     /**
//      * \brief Set the type of OSPF message
//      * 
//      * @param type The OSPF message type to be set.
//      */
//     void SetType(uint8_t type);

//     /**
//      * \brief Get the type of OSPF message
//      * 
//      * @return uint8_t The current OSPF message type.
//      */
//     uint8_t GetType() const;

//     /**
//      * \brief Set the length of the OSPF message
//      * 
//      * @param length The length of the OSPF message to be set.
//      */
//     void SetLength(uint16_t length);

//     /**
//      * \brief Get the length of the OSPF message
//      * 
//      * @return uint16_t The current length of the OSPF message.
//      */
//     uint16_t GetLength() const;

//     /**
//      * \brief Set the Router ID
//      * 
//      * @param routerId The Router ID to be set.
//      */
//     void SetRouterID(uint32_t routerId);

//     /**
//      * \brief Get the Router ID
//      * 
//      * @return uint32_t The current Router ID.
//      */
//     uint32_t GetRouterID() const;

//     /**
//      * \brief Set the Area ID
//      * 
//      * @param areaId The Area ID to be set.
//      */
//     void SetAreaID(uint32_t areaId);

//     /**
//      * \brief Get the Area ID
//      * 
//      * @return uint32_t The current Area ID.
//      */
//     uint32_t GetAreaID() const;

//     /**
//      * \brief Set the checksum for error checking
//      * 
//      * @param checksum The checksum value to be set.
//      */
//     void SetChecksum(uint16_t checksum);

//     /**
//      * \brief Get the checksum
//      * 
//      * @return uint16_t The current checksum value.
//      */
//     uint16_t GetChecksum() const;

//     /**
//      * \brief Set the Authentication type
//      * 
//      * @param auType The Authentication type to be set.
//      */
//     void SetAuType(uint16_t auType);

//     /**
//      * \brief Get the Authentication type
//      * 
//      * @return uint16_t The current Authentication type.
//      */
//     uint16_t GetAuType() const;

//     /**
//      * \brief Set the Authentication data
//      * 
//      * @param authentication The 64-bit Authentication data to be set.
//      */
//     void SetAuthentication(uint64_t authentication);

//     /**
//      * \brief Get the Authentication data
//      * 
//      * @return uint64_t The current 64-bit Authentication data.
//      */
//     uint64_t GetAuthentication() const;

//     /**
//      * \brief Get the type ID.
//      * \return the object TypeId
//      */
//     static TypeId GetTypeId();
//     TypeId GetInstanceTypeId() const override;
//     void Print(std::ostream& os) const override;
//     uint32_t GetSerializedSize() const override;
//     void Serialize(Buffer::Iterator start) const override;
//     uint32_t Deserialize(Buffer::Iterator start) override;

// private:
//     uint8_t m_version;          //!< OSPF version
//     uint8_t m_type;             //!< OSPF message type
//     uint16_t m_length;          //!< Length of the OSPF message
//     uint32_t m_routerId;        //!< Router ID
//     uint32_t m_areaId;          //!< Area ID
//     uint16_t m_checksum;        //!< Checksum for error-checking
//     uint16_t m_AuType;          //!< Authentication type
//     uint64_t m_Authentication;  //!< Authentication data
};

} // namespace open_routing
} // namespace ns3

#endif /* OSPF_HEADER_H */
