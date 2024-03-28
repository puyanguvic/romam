

/**
 * \ingroup globalrouting
 *
 * @brief A single link record for a link state advertisement.
 *
 * The GlobalRoutingLinkRecord is modeled after the OSPF link record field of
 * a Link State Advertisement.  Right now we will only see two types of link
 * records corresponding to a stub network and a point-to-point link (channel).
 */
class GlobalRoutingLinkRecord
{
  public:
    friend class GlobalRoutingLSA; //!< Friend class.

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
    GlobalRoutingLinkRecord();

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
    GlobalRoutingLinkRecord(LinkType linkType,
                            Ipv4Address linkId,
                            Ipv4Address linkData,
                            uint16_t metric);

    /**
     * @brief Destroy a Global Routing Link Record.
     *
     * Currently does nothing.  Here as a placeholder only.
     */
    ~GlobalRoutingLinkRecord();

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
