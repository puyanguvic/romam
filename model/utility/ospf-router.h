/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OSPF_ROUTER_H
#define OSPF_ROUTER_H

#include "romam-router.h"

#include <list>
#include <stdint.h>

namespace ns3
{
class OSPFRouting;

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
    // Inherate from RomamRouter
    void SetRoutingProtocol(Ptr<RomamRouting> routing) override;
    virtual Ptr<RomamRouting> GetRoutingProtocol() override;

  private:
    ~OSPFRouter() override;

    Ptr<OSPFRouting> m_routingProtocol; //!< the Ipv4GlobalRouting in use

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
