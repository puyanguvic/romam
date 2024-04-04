/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulation-singleton.h"
#include "router-manager.h"

// #include "route-manager-impl.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RouterManager");

// ---------------------------------------------------------------------------
//
// DGRRoutingManager Implementation
//
// ---------------------------------------------------------------------------

void
RouterManager::DeleteDGRRoutes ()
{
  NS_LOG_FUNCTION_NOARGS ();
  // SimulationSingleton<RouterManagerImpl>::Get ()->
  // DeleteDGRRoutes ();
}

void
RouterManager::BuildDGRRoutingDatabase (void) 
{
  NS_LOG_FUNCTION_NOARGS ();
  // SimulationSingleton<RouterManagerImpl>::Get ()->
  // BuildDGRRoutingDatabase ();
}

void
RouterManager::InitializeRoutes (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  // SimulationSingleton<RouterManagerImpl>::Get ()->
  // InitializeRoutes ();
}

uint32_t
RouterManager::AllocateRouterId (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  static uint32_t routerId = 0;
  return routerId++;
}


} // namespace ns3
