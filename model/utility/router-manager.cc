/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulation-singleton.h"
#include "router-manager.h"
#include "../routing_algorithm/dijkstra's-algorithm.h"
#include "../datapath/global-lsdb-manager.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RouterManager");

uint32_t
RouterManager::AllocateRouterId (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  static uint32_t routerId = 0;
  return routerId++;
}

void
RouterManager::DeleteRoutes ()
{
  NS_LOG_FUNCTION_NOARGS ();
  SimulationSingleton<DijkstraAlgorithm>::Get ()->
  DeleteRoutes ();
}

void
RouterManager::BuildLSDB (void) 
{
  NS_LOG_FUNCTION_NOARGS ();
  SimulationSingleton<GlobalLSDBManager>::Get ()->
  BuildLinkStateDatabase ();
}

void
RouterManager::InitializeRoutes (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  LSDB* lsdb = SimulationSingleton<GlobalLSDBManager>::Get ()->GetLSDB ();
  DijkstraAlgorithm* dijkstra = new DijkstraAlgorithm ();
  dijkstra->InitializeRoutes (lsdb);
}

} // namespace ns3
