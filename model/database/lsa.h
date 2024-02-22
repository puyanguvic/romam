/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef LSA_H
#define LSA_H

#include <stdint.h>
#include <list>
#include <queue>
#include <map>
#include <vector>
#include "ns3/ipv4-address.h"
#include "ns3/node.h"

namespace ns3
{
namespace open_routing
{

/**
 *
 * @brief A single link record for a link state advertisement.
 *
 * The LinkRecord is modeled after the OSPF link record field of
 * a Link State Advertisement.  Right now we will only see two types of link
 * records corresponding to a stub network and a point-to-point link (channel).
 */
class LinkRecord
{
  public:
    friend class LSA; //!< Friend class.

    /**
     * @enum LinkType
     * @brief Enumeration of the possible types of Global Routing Link Records.
     *
     * These values are defined in the OSPF spec.  We currently only use
     * PointToPoint and StubNetwork types.
     */
    enum LinkType
    {
        Unknown = 0,    /**< Uninitialized Link Record */
        PointToPoint,   /**< Record representing a point to point channel */
        TransitNetwork, /**< Unused -- for future OSPF compatibility  */
        StubNetwork,    /**< Record represents a leaf node network */
        VirtualLink     /**< Unused -- for future OSPF compatibility  */
    };

    /**
     * @brief Construct an empty ("uninitialized") Global Routing Link Record.
     *
     * The Link ID and Link Data Ipv4 addresses are set to "0.0.0.0";
     * The Link Type is set to Unknown;
     * The metric is set to 0.
     */
    LinkRecord();

    /**
     * Construct an initialized Global Routing Link Record.
     *
     * @param linkType The type of link record to construct.
     * @param linkId The link ID for the record.
     * @param linkData The link data field for the record.
     * @param metric The metric field for the record.
     * @see LinkType
     * @see SetLinkId
     * @see SetLinkData
     */
    LinkRecord(LinkType linkType,
                            Ipv4Address linkId,
                            Ipv4Address linkData,
                            uint16_t metric);

    /**
     * @brief Destroy a Global Routing Link Record.
     *
     * Currently does nothing.  Here as a placeholder only.
     */
    ~LinkRecord();

    /**
     * Get the Link ID field of the Global Routing Link Record.
     *
     * For an OSPF type 1 link (PointToPoint) the Link ID will be the Router ID
     * of the neighboring router.
     *
     * For an OSPF type 3 link (StubNetwork), the Link ID will be the adjacent
     * neighbor's IP address
     *
     * @returns The Ipv4Address corresponding to the Link ID field of the record.
     */
    Ipv4Address GetLinkId() const;

    /**
     * @brief Set the Link ID field of the Global Routing Link Record.
     *
     * For an OSPF type 1 link (PointToPoint) the Link ID must be the Router ID
     * of the neighboring router.
     *
     * For an OSPF type 3 link (StubNetwork), the Link ID must be the adjacent
     * neighbor's IP address
     *
     * @param addr An Ipv4Address to store in the Link ID field of the record.
     */
    void SetLinkId(Ipv4Address addr);

    /**
     * @brief Get the Link Data field of the Global Routing Link Record.
     *
     * For an OSPF type 1 link (PointToPoint) the Link Data will be the IP
     * address of the node of the local side of the link.
     *
     * For an OSPF type 3 link (StubNetwork), the Link Data will be the
     * network mask
     *
     * @returns The Ipv4Address corresponding to the Link Data field of the record.
     */
    Ipv4Address GetLinkData() const;

    /**
     * @brief Set the Link Data field of the Global Routing Link Record.
     *
     * For an OSPF type 1 link (PointToPoint) the Link Data must be the IP
     * address of the node of the local side of the link.
     *
     * For an OSPF type 3 link (StubNetwork), the Link Data must be set to the
     * network mask
     *
     * @param addr An Ipv4Address to store in the Link Data field of the record.
     */
    void SetLinkData(Ipv4Address addr);

    /**
     * @brief Get the Link Type field of the Global Routing Link Record.
     *
     * The Link Type describes the kind of link a given record represents.  The
     * values are defined by OSPF.
     *
     * @see LinkType
     * @returns The LinkType of the current Global Routing Link Record.
     */
    LinkType GetLinkType() const;

    /**
     * @brief Set the Link Type field of the Global Routing Link Record.
     *
     * The Link Type describes the kind of link a given record represents.  The
     * values are defined by OSPF.
     *
     * @see LinkType
     * @param linkType The new LinkType for the current Global Routing Link Record.
     */
    void SetLinkType(LinkType linkType);

    /**
     * @brief Get the Metric Data field of the Global Routing Link Record.
     *
     * The metric is an abstract cost associated with forwarding a packet across
     * a link.  A sum of metrics must have a well-defined meaning.  That is, you
     * shouldn't use bandwidth as a metric (how does the sum of the bandwidth of
     * two hops relate to the cost of sending a packet); rather you should use
     * something like delay.
     *
     * @returns The metric field of the Global Routing Link Record.
     */
    uint16_t GetMetric() const;

    /**
     * @brief Set the Metric Data field of the Global Routing Link Record.
     *
     * The metric is an abstract cost associated with forwarding a packet across
     * a link.  A sum of metrics must have a well-defined meaning.  That is, you
     * shouldn't use bandwidth as a metric (how does the sum of the bandwidth of
     * two hops relate to the cost of sending a packet); rather you should use
     * something like delay.
     *
     * @param metric The new metric for the current Global Routing Link Record.
     */
    void SetMetric(uint16_t metric);

  private:
    /**
     * m_linkId and m_linkData are defined by OSPF to have different meanings
     * depending on the type of link a given link records represents.  They work
     * together.
     *
     * For Type 1 link (PointToPoint), set m_linkId to Router ID of
     * neighboring router.
     *
     * For Type 3 link (Stub), set m_linkId to neighbor's IP address
     */
    Ipv4Address m_linkId;

    /**
     * m_linkId and m_linkData are defined by OSPF to have different meanings
     * depending on the type of link a given link records represents.  They work
     * together.
     *
     * For Type 1 link (PointToPoint), set m_linkData to local IP address
     *
     * For Type 3 link (Stub), set m_linkData to mask
     */
    Ipv4Address m_linkData; // for links to RouterLSA,

    /**
     * The type of the Global Routing Link Record.  Defined in the OSPF spec.
     * We currently only use PointToPoint and StubNetwork types.
     */
    LinkType m_linkType;

    /**
     * The metric for a given link.
     *
     * A metric is abstract cost associated with forwarding a packet across a
     * link.  A sum of metrics must have a well-defined meaning.  That is, you
     * shouldn't use bandwidth as a metric (how does the sum of the bandwidth
     * of two hops relate to the cost of sending a packet); rather you should
     * use something like delay.
     */
    uint16_t m_metric;
};


/**
 * @brief a Link State Advertisement (LSA) for a router, used in global
 * routing.
 *
 * Roughly equivalent to a global incarnation of the OSPF link state header
 * combined with a list of Link Records.  Since it's global, there's
 * no need for age or sequence number.  See \RFC{2328}, Appendix A.
 */
class LSA
{
  public:
    /**
     * @enum LSType
     * @brief corresponds to LS type field of \RFC{2328} OSPF LSA header
     */
    enum LSType
    {
        Unknown = 0, /**< Uninitialized Type */
        RouterLSA,
        NetworkLSA,
        SummaryLSA,
        SummaryLSA_ASBR,
        ASExternalLSAs
    };

    /**
     * @enum SPFStatus
     * @brief Enumeration of the possible values of the status flag in the Routing
     * Link State Advertisements.
     */
    enum SPFStatus
    {
        LSA_SPF_NOT_EXPLORED = 0, /**< New vertex not yet considered */
        LSA_SPF_CANDIDATE,        /**< Vertex is in the SPF candidate queue */
        LSA_SPF_IN_SPFTREE        /**< Vertex is in the SPF tree */
    };

    /**
     * @brief Create a blank Link State Advertisement.
     *
     * On completion Ipv4Address variables initialized to 0.0.0.0 and the
     * list of Link State Records is empty.
     */
    LSA();

    /**
     * @brief Create an initialized Link State Advertisement.
     *
     * On completion the list of Link State Records is empty.
     *
     * @param status The status to of the new LSA.
     * @param linkStateId The Ipv4Address for the link state ID field.
     * @param advertisingRtr The Ipv4Address for the advertising router field.
     */
    LSA(SPFStatus status, Ipv4Address linkStateId, Ipv4Address advertisingRtr);

    /**
     * @brief Copy constructor for a Link State Advertisement.
     *
     * Takes a piece of memory and constructs a semantically identical copy of
     * the given LSA.
     *
     * @param lsa The existing LSA to be used as the source.
     */
    LSA(LSA& lsa);

    /**
     * @brief Destroy an existing Link State Advertisement.
     *
     * Any Link Records present in the list are freed.
     */
    ~LSA();

    /**
     * @brief Assignment operator for a Link State Advertisement.
     *
     * Takes an existing Link State Advertisement and overwrites
     * it to make a semantically identical copy of a given prototype LSA.
     *
     * If there are any Link Records present in the existing
     * LSA, they are freed before the assignment happens.
     *
     * @param lsa The existing LSA to be used as the source.
     * @returns Reference to the overwritten LSA.
     */
    LSA& operator=(const LSA& lsa);

    /**
     * @brief Copy any Link Records in a given Link
     * State Advertisement to the current LSA.
     *
     * Existing Link Records are not deleted -- this is a concatenation of Link
     * Records.
     *
     * @see ClearLinkRecords ()
     * @param lsa The LSA to copy the Link Records from.
     */
    void CopyLinkRecords(const LSA& lsa);

    /**
     * @brief Add a given Link Record to the LSA.
     *
     * @param lr The Link Record to be added.
     * @returns The number of link records in the list.
     */
    uint32_t AddLinkRecord(LinkRecord* lr);

    /**
     * @brief Return the number of Link Records in the LSA.
     *
     * @returns The number of link records in the list.
     */
    uint32_t GetNLinkRecords() const;

    /**
     * @brief Return a pointer to the specified Link Record.
     *
     * @param n The LSA number desired.
     * @returns The number of link records in the list.
     */
    LinkRecord* GetLinkRecord(uint32_t n) const;

    /**
     * @brief Release all of the Global Routing Link Records present in the Global
     * Routing Link State Advertisement and make the list of link records empty.
     */
    void ClearLinkRecords();

    /**
     * @brief Check to see if the list of Link Records present in the
     * Link State Advertisement is empty.
     *
     * @returns True if the list is empty, false otherwise.
     */
    bool IsEmpty() const;

    /**
     * @brief Print the contents of the Link State Advertisement and
     * any Link Records present in the list.  Quite verbose.
     * @param os the output stream
     */
    void Print(std::ostream& os) const;

    /**
     * @brief Return the LSType field of the LSA
     * @returns The LS Type.
     */
    LSType GetLSType() const;
    /**
     * @brief Set the LS type field of the LSA
     * @param typ the LS Type.
     */
    void SetLSType(LSType typ);

    /**
     * @brief Get the Link State ID as defined by the OSPF spec.  We always set it
     * to the router ID of the router making the advertisement.
     *
     * @see RoutingEnvironment::AllocateRouterId ()
     * @see GlobalRouting::GetRouterId ()
     * @returns The Ipv4Address stored as the link state ID.
     */
    Ipv4Address GetLinkStateId() const;

    /**
     * @brief Set the Link State ID is defined by the OSPF spec.  We always set it
     * to the router ID of the router making the advertisement.
     * @param addr IPv4 address which will act as ID
     * @see RoutingEnvironment::AllocateRouterId ()
     * @see GlobalRouting::GetRouterId ()
     */
    void SetLinkStateId(Ipv4Address addr);

    /**
     * @brief Get the Advertising Router as defined by the OSPF spec.  We always
     * set it to the router ID of the router making the advertisement.
     *
     * @see RoutingEnvironment::AllocateRouterId ()
     * @see GlobalRouting::GetRouterId ()
     * @returns The Ipv4Address stored as the advertising router.
     */
    Ipv4Address GetAdvertisingRouter() const;

    /**
     * @brief Set the Advertising Router as defined by the OSPF spec.  We always
     * set it to the router ID of the router making the advertisement.
     *
     * @param rtr ID of the router making advertisement
     * @see RoutingEnvironment::AllocateRouterId ()
     * @see GlobalRouting::GetRouterId ()
     */
    void SetAdvertisingRouter(Ipv4Address rtr);

    /**
     * @brief For a Network LSA, set the Network Mask field that precedes
     * the list of attached routers.
     * @param mask the Network Mask field.
     */
    void SetNetworkLSANetworkMask(Ipv4Mask mask);

    /**
     * @brief For a Network LSA, get the Network Mask field that precedes
     * the list of attached routers.
     *
     * @returns the NetworkLSANetworkMask
     */
    Ipv4Mask GetNetworkLSANetworkMask() const;

    /**
     * @brief Add an attached router to the list in the NetworkLSA
     *
     * @param addr The Ipv4Address of the interface on the network link
     * @returns The number of addresses in the list.
     */
    uint32_t AddAttachedRouter(Ipv4Address addr);

    /**
     * @brief Return the number of attached routers listed in the NetworkLSA
     *
     * @returns The number of attached routers.
     */
    uint32_t GetNAttachedRouters() const;

    /**
     * @brief Return an Ipv4Address corresponding to the specified attached router
     *
     * @param n The attached router number desired (number in the list).
     * @returns The Ipv4Address of the requested router
     */
    Ipv4Address GetAttachedRouter(uint32_t n) const;

    /**
     * @brief Get the SPF status of the advertisement.
     *
     * @see SPFStatus
     * @returns The SPFStatus of the LSA.
     */
    SPFStatus GetStatus() const;

    /**
     * @brief Set the SPF status of the advertisement
     * @param status SPF status to set
     * @see SPFStatus
     */
    void SetStatus(SPFStatus status);

    /**
     * @brief Get the Node pointer of the node that originated this LSA
     * @returns Node pointer
     */
    Ptr<Node> GetNode() const;

    /**
     * @brief Set the Node pointer of the node that originated this LSA
     * @param node Node pointer
     */
    void SetNode(Ptr<Node> node);

  private:
    /**
     * The type of the LSA.  Each LSA type has a separate advertisement
     * format.
     */
    LSType m_lsType;
    /**
     * The Link State ID is defined by the OSPF spec.  We always set it to the
     * router ID of the router making the advertisement.
     *
     * @see RoutingEnvironment::AllocateRouterId ()
     * @see GlobalRouting::GetRouterId ()
     */
    Ipv4Address m_linkStateId;

    /**
     * The Advertising Router is defined by the OSPF spec.  We always set it to
     * the router ID of the router making the advertisement.
     *
     * @see RoutingEnvironment::AllocateRouterId ()
     * @see GlobalRouting::GetRouterId ()
     */
    Ipv4Address m_advertisingRtr;

    /**
     * A convenience typedef to avoid too much writers cramp.
     */
    typedef std::list<LinkRecord*> ListOfLinkRecords_t;

    /**
     * Each Link State Advertisement contains a number of Link Records that
     * describe the kinds of links that are attached to a given node.  We
     * consider PointToPoint and StubNetwork links.
     *
     * m_linkRecords is an STL list container to hold the Link Records that have
     * been discovered and prepared for the advertisement.
     *
     * @see GlobalRouting::DiscoverLSAs ()
     */
    ListOfLinkRecords_t m_linkRecords;

    /**
     * Each Network LSA contains the network mask of the attached network
     */
    Ipv4Mask m_networkLSANetworkMask;

    /**
     * A convenience typedef to avoid too much writers cramp.
     */
    typedef std::list<Ipv4Address> ListOfAttachedRouters_t;

    /**
     * Each Network LSA contains a list of attached routers
     *
     * m_attachedRouters is an STL list container to hold the addresses that have
     * been discovered and prepared for the advertisement.
     *
     * @see GlobalRouting::DiscoverLSAs ()
     */
    ListOfAttachedRouters_t m_attachedRouters;

    /**
     * This is a tristate flag used internally in the SPF computation to mark
     * if an SPFVertex (a data structure representing a vertex in the SPF tree
     * -- a router) is new, is a candidate for a shortest path, or is in its
     * proper position in the tree.
     */
    SPFStatus m_status;
    uint32_t m_node_id; //!< node ID
};

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param lsa the LSA
 * \returns the reference to the output stream
 */
std::ostream& operator<<(std::ostream& os, LSA& lsa);

}

} // namespace
#endif /* LSA_H */
