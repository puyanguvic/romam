/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulation-singleton.h"
#include "dgr-route-manager.h"
#include "route-manager-impl.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DGRRouteManager");

// ---------------------------------------------------------------------------
//
// DGRRoutingManager Implementation
//
// ---------------------------------------------------------------------------

void
DGRRouteManager::DeleteDGRRoutes ()
{
  NS_LOG_FUNCTION_NOARGS ();
  SimulationSingleton<DGRRouteManagerImpl>::Get ()->
  DeleteDGRRoutes ();
}

void
DGRRouteManager::BuildDGRRoutingDatabase (void) 
{
  NS_LOG_FUNCTION_NOARGS ();
  SimulationSingleton<DGRRouteManagerImpl>::Get ()->
  BuildDGRRoutingDatabase ();
}

void
DGRRouteManager::InitializeRoutes (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  SimulationSingleton<DGRRouteManagerImpl>::Get ()->
  InitializeRoutes ();
}

uint32_t
DGRRouteManager::AllocateRouterId (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  static uint32_t routerId = 0;
  return routerId++;
}


} // namespace ns3
