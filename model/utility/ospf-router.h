/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_ROUTER_H
#define OSPF_ROUTER_H

#include "romam-router.h"

// #include "ns3/bridge-net-device.h"
// #include "ns3/channel.h"
// #include "ns3/ipv4-address.h"
// #include "ns3/net-device-container.h"
// #include "ns3/node.h"
// #include "ns3/object.h"
// #include "ns3/ptr.h"

#include <list>
#include <stdint.h>

namespace ns3
{
class OSPFinRomamRouting;

/**
 * @brief An interface aggregated to a node to provide global routing info
 *
 * An interface aggregated to a node that provides global routing information
 * to a global route manager.  The presence of the interface indicates that
 * the node is a router.  The interface is the mechanism by which the router
 * advertises its connections to neighboring routers.  We're basically
 * allowing the route manager to query for link state advertisements.
 */
class OSPFRouter : public RomamRouter
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    /**
     * @brief Create a Global Router class
     */
    OSPFRouter();

    /**
     * \brief Set the specific Global Routing Protocol to be used
     * \param routing the routing protocol
     */
    void SetRoutingProtocol(Ptr<OSPFinRomamRouting> routing);

    /**
     * \brief Get the specific Global Routing Protocol used
     * \returns the routing protocol
     */
    Ptr<OSPFinRomamRouting> GetRoutingProtocol(void);

  private:
    ~OSPFRouter() override;

    Ptr<OSPFinRomamRouting> m_routingProtocol; //!< the Ipv4GlobalRouting in use

    // inherited from Object
    void DoDispose(void) override;

    /**
     * @brief Global Router copy construction is disallowed.
     * @param sr object to copy from.
     */
    OSPFRouter(OSPFRouter& sr);

    /**
     * @brief Global Router assignment operator is disallowed.
     * @param sr object to copy from.
     * @returns The object copied.
     */
    OSPFRouter& operator=(OSPFRouter& sr);
};

} // namespace ns3

#endif /* ROMAM_ROUTER_H */
