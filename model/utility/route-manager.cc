/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "route-manager.h"

#include "../routing_algorithm/dijkstra-algorithm.h"
#include "../routing_algorithm/spf-algorithm.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/simulation-singleton.h"
// #include "../routing_algorithm/spf-algorithm.h"
#include "../datapath/global-lsdb-manager.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RouteManager");

uint32_t
RouteManager::AllocateRouterId(void)
{
    NS_LOG_FUNCTION_NOARGS();
    static uint32_t routerId = 0;
    return routerId++;
}

void
RouteManager::DeleteRoutes()
{
    NS_LOG_FUNCTION_NOARGS();
    SimulationSingleton<DijkstraAlgorithm>::Get()->DeleteRoutes();
}

void
RouteManager::BuildLSDB(void)
{
    NS_LOG_FUNCTION_NOARGS();
    SimulationSingleton<GlobalLSDBManager>::Get()->BuildLinkStateDatabase();
}

void
RouteManager::InitializeDijkstraRoutes(void)
{
    NS_LOG_FUNCTION_NOARGS();
    LSDB* lsdb = SimulationSingleton<GlobalLSDBManager>::Get()->GetLSDB();
    // lsdb->Print(std::cout);
    SPFAlgorithm* spf = new SPFAlgorithm();
    spf->InsertLSDB(lsdb);
    spf->InitializeRoutes();
}

void
RouteManager::InitializeSPFRoutes(void)
{
    NS_LOG_FUNCTION_NOARGS();
    LSDB* lsdb = SimulationSingleton<GlobalLSDBManager>::Get()->GetLSDB();
    SPFAlgorithm* spf = new SPFAlgorithm();
    spf->InsertLSDB(lsdb);
    spf->InitializeRoutes();
}

} // namespace ns3
