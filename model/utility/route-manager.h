/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef ROUTE_MANAGER_H
#define ROUTE_MANAGER_H

#include "ns3/core-module.h"

namespace ns3
{

/**
 * \ingroup Romam Routing Framework
 *
 * @brief router manager
 */
class RouteManager
{
  public:
    /**
     * @brief Allocate a 32-bit router ID from monotonically increasing counter.
     * @returns A new new RouterId.
     */
    static uint32_t AllocateRouterId();

    /**
     * @brief Delete all static routes on all nodes that have a
     * RomamRouterInterface
     *
     */
    static void DeleteRoutes();

    /**
     * @brief Build the Link State Database (LSDB) by gathering Link State Advertisements
     * from each node exporting a DGRRouter interface.
     */
    static void BuildLSDB();

    /**
     * @brief Compute routes using a Dijkstra algorithm computation and populate
     * per-node forwarding tables
     */
    static void InitializeDijkstraRoutes();

    /**
     * @brief Compute routes using a Shortest path forest algorithm computation and populate
     * per-node forwarding tables
     */
    static void InitializeSPFRoutes();

  private:
    /**
     * @brief Global Route Manager copy construction is disallowed.  There's no
     * need for it and a compiler provided shallow copy would be wrong.
     *
     * @param srm object to copy from
     */
    RouteManager(RouteManager& srm);

    /**
     * @brief Global Router copy assignment operator is disallowed.  There's no
     * need for it and a compiler provided shallow copy would be wrong.
     *
     * @param srm object to copy from
     * @returns the copied object
     */
    RouteManager& operator=(RouteManager& srm);
};

} // namespace ns3

#endif /* ROUTE_MANAGER_H */
