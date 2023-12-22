
#include "ns3/ipv4-address.h"

/**
 * @brief a Link State Advertisement (LSA) for a router, used in global
 * routing.
 *
 * Roughly equivalent to a global incarnation of the OSPF link state header
 * combined with a list of Link Records.  Since it's global, there's
 * no need for age or sequence number.  See \RFC{2328}, Appendix A.
 */
class GlobalRoutingLSA
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
     * @brief Create a blank Global Routing Link State Advertisement.
     *
     * On completion Ipv4Address variables initialized to 0.0.0.0 and the
     * list of Link State Records is empty.
     */
    GlobalRoutingLSA();

    /**
     * @brief Create an initialized Global Routing Link State Advertisement.
     *
     * On completion the list of Link State Records is empty.
     *
     * @param status The status to of the new LSA.
     * @param linkStateId The Ipv4Address for the link state ID field.
     * @param advertisingRtr The Ipv4Address for the advertising router field.
     */
    GlobalRoutingLSA(SPFStatus status, Ipv4Address linkStateId, Ipv4Address advertisingRtr);

    /**
     * @brief Copy constructor for a Global Routing Link State Advertisement.
     *
     * Takes a piece of memory and constructs a semantically identical copy of
     * the given LSA.
     *
     * @param lsa The existing LSA to be used as the source.
     */
    GlobalRoutingLSA(GlobalRoutingLSA& lsa);

    /**
     * @brief Destroy an existing Global Routing Link State Advertisement.
     *
     * Any Global Routing Link Records present in the list are freed.
     */
    ~GlobalRoutingLSA();

    /**
     * @brief Assignment operator for a Global Routing Link State Advertisement.
     *
     * Takes an existing Global Routing Link State Advertisement and overwrites
     * it to make a semantically identical copy of a given prototype LSA.
     *
     * If there are any Global Routing Link Records present in the existing
     * LSA, they are freed before the assignment happens.
     *
     * @param lsa The existing LSA to be used as the source.
     * @returns Reference to the overwritten LSA.
     */
    GlobalRoutingLSA& operator=(const GlobalRoutingLSA& lsa);

    /**
     * @brief Copy any Global Routing Link Records in a given Global Routing Link
     * State Advertisement to the current LSA.
     *
     * Existing Link Records are not deleted -- this is a concatenation of Link
     * Records.
     *
     * @see ClearLinkRecords ()
     * @param lsa The LSA to copy the Link Records from.
     */
    void CopyLinkRecords(const GlobalRoutingLSA& lsa);

    /**
     * @brief Add a given Global Routing Link Record to the LSA.
     *
     * @param lr The Global Routing Link Record to be added.
     * @returns The number of link records in the list.
     */
    uint32_t AddLinkRecord(GlobalRoutingLinkRecord* lr);

    /**
     * @brief Return the number of Global Routing Link Records in the LSA.
     *
     * @returns The number of link records in the list.
     */
    uint32_t GetNLinkRecords() const;

    /**
     * @brief Return a pointer to the specified Global Routing Link Record.
     *
     * @param n The LSA number desired.
     * @returns The number of link records in the list.
     */
    GlobalRoutingLinkRecord* GetLinkRecord(uint32_t n) const;

    /**
     * @brief Release all of the Global Routing Link Records present in the Global
     * Routing Link State Advertisement and make the list of link records empty.
     */
    void ClearLinkRecords();

    /**
     * @brief Check to see if the list of Global Routing Link Records present in the
     * Global Routing Link State Advertisement is empty.
     *
     * @returns True if the list is empty, false otherwise.
     */
    bool IsEmpty() const;

    /**
     * @brief Print the contents of the Global Routing Link State Advertisement and
     * any Global Routing Link Records present in the list.  Quite verbose.
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
    typedef std::list<GlobalRoutingLinkRecord*> ListOfLinkRecords_t;

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
