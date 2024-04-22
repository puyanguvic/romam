/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef ROUTER_MANAGER_H
#define ROUTER_MANAGER_H

#include "ns3/core-module.h"

namespace ns3 {

/**
 * \ingroup Romam Routing Framework
 *
 * @brief router manager
 */
class RouterManager
{
public:
/**
 * @brief Allocate a 32-bit router ID from monotonically increasing counter.
 * @returns A new new RouterId.
 */
  static uint32_t AllocateRouterId ();

/**
 * @brief Delete all static routes on all nodes that have a 
 * DGRRouterInterface
 *
 */
  static void DeleteRoutes ();

/**
 * @brief Build the Link State Database (LSDB) by gathering Link State Advertisements
 * from each node exporting a DGRRouter interface.
 */
  static void BuildLSDB ();

/**
 * @brief Compute routes using a Dijkstra SPF computation and populate
 * per-node forwarding tables
 */
  static void InitializeRoutes ();

private:
/**
 * @brief Global Route Manager copy construction is disallowed.  There's no 
 * need for it and a compiler provided shallow copy would be wrong.
 *
 * @param srm object to copy from
 */
  RouterManager (RouterManager& srm);

/**
 * @brief Global Router copy assignment operator is disallowed.  There's no 
 * need for it and a compiler provided shallow copy would be wrong.
 *
 * @param srm object to copy from
 * @returns the copied object
 */
  RouterManager& operator= (RouterManager& srm);
};

} // namespace ns3

#endif /* ROUTER_MANAGER_H */
