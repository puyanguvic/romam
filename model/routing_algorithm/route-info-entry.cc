#include "route-info-entry.h"

#include "ns3/assert.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE ("RouteInfoEntry");

NS_OBJECT_ENSURE_REGISTERED (RouteInfoEntry);

TypeId
RouteInfoEntry::GetTypeId()
{
    static TypeId tid = 
        TypeId("ns3::RouteInfoEntry").SetParent<Object>().SetGroupName("Romam");
    return tid;
}

} // namespace ns3