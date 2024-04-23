#include "romam-routing.h"

#include "ns3/boolean.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include <iomanip>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RomamRouting");

NS_OBJECT_ENSURE_REGISTERED(RomamRouting);

TypeId
RomamRouting::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RomamRouting").SetParent<Object>().SetGroupName("Romam");
    return tid;
}

RomamRouting::~RomamRouting()
{
    NS_LOG_FUNCTION(this);
}

} // namespace ns3
