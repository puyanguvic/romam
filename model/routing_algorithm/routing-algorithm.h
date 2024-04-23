/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef ROUTING_ALGORITHM_H
#define ROUTING_ALGORITHM_H

namespace ns3
{

class LSDB;

class RoutingAlgorithm
{
  public:
    virtual ~RoutingAlgorithm();
    /**
     * @brief Delete all static routes on all nodes that have a
     * Romam Router Interface
     *
     * \todo  separate manually assigned static routes from static routes that
     * the global routing code injects, and only delete the latter
     */
    virtual void DeleteRoutes() = 0;

    /**
     * @brief Compute routes using a dijkstra SPF computation and
     * populate per-node forwarding tables
     */
    virtual void InitializeRoutes() = 0;
};

} // namespace ns3

#endif /* ROUTING_ALGORITHM_H */
