/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "dgr-queue-disc.h"

#include "../datapath/romam-tags.h"

#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"

#define FAST_LANE 0
#define SLOW_LANE 1
#define NORMAL_LANE 2

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DGRQueueDisc");

TypeId
DGRQueueDisc::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::DGRQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("Romam")
            .AddConstructor<DGRQueueDisc>()
            .AddAttribute("MaxSize",
                          "The maximum number of packets accepted by this queue disc.",
                          QueueSizeValue(QueueSize("1085p")),
                          MakeQueueSizeAccessor(&QueueDisc::SetMaxSize, &QueueDisc::GetMaxSize),
                          MakeQueueSizeChecker());
    return tid;
}

DGRQueueDisc::DGRQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::MULTIPLE_QUEUES, QueueSizeUnit::PACKETS)
{
    NS_LOG_FUNCTION(this);
}

DGRQueueDisc::~DGRQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

bool
DGRQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);
    uint32_t lane = EnqueueClassify(item);
    // if (lane == 1) std::cout << "lane: " << lane << std::endl;
    // std::cout << "get current size : " << GetInternalQueue(lane)->GetCurrentSize ().GetValue() <<
    // std::endl;
    if (lane == 0 && GetInternalQueue(lane)->GetCurrentSize().GetValue() >= LinesSize[lane])
    {
        // std::cout << "fast lane full" << std::endl;
        lane += 1; // when fast congestion, the arrival packe will be enqueue to slow lane
    }
    bool retval = GetInternalQueue(lane)->Enqueue(item);
    if (!retval)
    {
        NS_LOG_WARN("Packet enqueue failed. Check the size of the internal queues");
    }

    NS_LOG_LOGIC("Number packets lane " << lane << ": " << GetInternalQueue(lane)->GetNPackets());

    return retval;
}

Ptr<QueueDiscItem>
DGRQueueDisc::DoDequeue(void)
{
    NS_LOG_FUNCTION(this);

    Ptr<QueueDiscItem> item;

    for (uint32_t i = 0; i < GetNInternalQueues(); i++)
    {
        item = GetInternalQueue(i)->Dequeue();
        if (item != nullptr)
        {
            NS_LOG_LOGIC("Popped from band " << i << ": " << item);
            NS_LOG_LOGIC("Number packets band " << i << ": " << GetInternalQueue(i)->GetNPackets());
            // std::cout << item->GetSize () << std::endl;
            return item;
        }
    }

    NS_LOG_LOGIC("Queue empty");
    return item;
}

Ptr<const QueueDiscItem>
DGRQueueDisc::DoPeek(void)
{
    NS_LOG_FUNCTION(this);

    Ptr<const QueueDiscItem> item;

    for (uint32_t i = 0; i < GetNInternalQueues(); i++)
    {
        item = GetInternalQueue(i)->Peek();
        if (item != nullptr)
        {
            NS_LOG_LOGIC("Peeked from band " << i << ": " << item);
            NS_LOG_LOGIC("Number packets band " << i << ": " << GetInternalQueue(i)->GetNPackets());
            return item;
        }
    }

    NS_LOG_LOGIC("Queue empty");
    return item;
}

bool
DGRQueueDisc::CheckConfig(void)
{
    NS_LOG_FUNCTION(this);
    if (GetNQueueDiscClasses() > 0)
    {
        NS_LOG_ERROR("DGRQueueDisc cannot have classes");
        return false;
    }

    if (GetNPacketFilters() != 0)
    {
        NS_LOG_ERROR("DGRQueueDisc needs no packet filter");
        return false;
    }

    if (GetNInternalQueues() == 0)
    {
        // create 3 DropTail queues with GetLimit() packets each
        ObjectFactory factory;
        factory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>");
        factory.Set("MaxSize", QueueSizeValue(GetMaxSize()));
        AddInternalQueue(factory.Create<InternalQueue>());
        AddInternalQueue(factory.Create<InternalQueue>());
        AddInternalQueue(factory.Create<InternalQueue>());
        GetInternalQueue(0)->SetMaxSize(QueueSize("17p"));
        GetInternalQueue(1)->SetMaxSize(QueueSize("68p"));
        GetInternalQueue(2)->SetMaxSize(QueueSize("1000p"));
    }

    if (GetNInternalQueues() != 3)
    {
        NS_LOG_ERROR("DGRQueueDisc needs 3 internal queues");
        return false;
    }

    if (GetInternalQueue(0)->GetMaxSize().GetUnit() != QueueSizeUnit::PACKETS ||
        GetInternalQueue(1)->GetMaxSize().GetUnit() != QueueSizeUnit::PACKETS ||
        GetInternalQueue(2)->GetMaxSize().GetUnit() != QueueSizeUnit::PACKETS)
    {
        NS_LOG_ERROR("PfifoFastQueueDisc needs 3 internal queues operating in packet mode");
        return false;
    }
    return true;
}

void
DGRQueueDisc::InitializeParams(void)
{
    NS_LOG_FUNCTION(this);
}

uint32_t
DGRQueueDisc::EnqueueClassify(Ptr<QueueDiscItem> item)
{
    PriorityTag priorityTag;
    if (item->GetPacket()->PeekPacketTag(priorityTag))
    {
        uint32_t priority = priorityTag.GetPriority();
        switch (priority)
        {
        case 0x00:
            return FAST_LANE;
        case 0x01:
            return SLOW_LANE;
        default:
            return NORMAL_LANE;
        }
    }
    else
    {
        return NORMAL_LANE;
    }
}
} // namespace ns3