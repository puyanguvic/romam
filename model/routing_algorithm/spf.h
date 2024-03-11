// spf.h

#ifndef SPF_H
#define SPF_H


#include "ns3/ipv4-address.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/open-routing-module.h"

#include <list>
#include <map>
#include <queue>
#include <stdint.h>
#include <vector>

namespace ns3
{

/**
  * \brief Calculate the shortest path first (SPF) tree
  *
  * Equivalent to quagga ospf_spf_calculate
  * \param root the root node
  */
  void SPFCalculate(Ipv4Address root, GlobalRouteManagerLSDB* lsdb);


}
#endif // SPF_H
