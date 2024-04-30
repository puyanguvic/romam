#include "ddr-queue-disc.h"

#include "../datapath/romam-tags.h"

#include "ns3/attribute.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/object-factory.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#define DELAY_SENSITIVE 0
#define BEST_EFFORT 1

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DDRQueueDisc");

NS_OBJECT_ENSURE_REGISTERED(DDRQueueDisc);

TypeId
DDRQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DDRQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<DDRQueueDisc>()
            .AddAttribute("MaxSize",
                          "The maximum number of packets accepted by this queue disc.",
                          QueueSizeValue(QueueSize("3MB")),
                          MakeQueueSizeAccessor(&QueueDisc::SetMaxSize, &QueueDisc::GetMaxSize),
                          MakeQueueSizeChecker());

    return tid;
}

DDRQueueDisc::DDRQueueDisc()
    : QueueDisc(QueueDiscSizePolicy::MULTIPLE_QUEUES, QueueSizeUnit::BYTES)
{
    NS_LOG_FUNCTION(this);
}

DDRQueueDisc::~DDRQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

uint32_t
DDRQueueDisc::GetQueueStatus()
{
    uint32_t currentSize = GetInternalQueue(0)->GetCurrentSize().GetValue();
    uint32_t maxSize = GetInternalQueue(0)->GetMaxSize().GetValue();
    return currentSize * 10 / maxSize;
}

uint32_t
DDRQueueDisc::GetQueueDelay()
{
    // in microsecond
    uint32_t currentSize = GetInternalQueue(0)->GetCurrentSize().GetValue();
    uint32_t maxSize = GetInternalQueue(0)->GetMaxSize().GetValue();
    return currentSize * 10 * 2000 / maxSize;
}

void
DDRQueueDisc::DoDispose()
{
    NS_LOG_FUNCTION(this);
    QueueDisc::DoDispose();
}

bool
DDRQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);
    uint32_t band = EnqueueClassify(item);
    // if (band == 0)
    //   {
    //     std::cout << "fast lane" << std::endl;
    //   }
    // uint32_t itemSize = item->GetSize ();
    // uint32_t currentQueueSize = GetInternalQueue (band)->GetCurrentSize ().GetValue ();
    // uint32_t maxQueueSize = GetInternalQueue (band)->GetMaxSize ().GetValue ();
    // if (currentQueueSize + itemSize > maxQueueSize)
    //   {
    //     NS_LOG_LOGIC ("Queue disc limit exceeded -- drop packet");
    //     DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
    //   }
    bool retval = GetInternalQueue(band)->Enqueue(item);
    if (!retval)
    {
        NS_LOG_WARN("Packet enqueue failed. Check the size of the internal queues");
    }

    NS_LOG_LOGIC("Band current size " << band << ": " << GetInternalQueue(band)->GetCurrentSize());
    return retval;
}

Ptr<QueueDiscItem>
DDRQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    Ptr<QueueDiscItem> item;

    for (uint32_t i = 0; i < GetNInternalQueues(); i++)
    {
        item = GetInternalQueue(i)->Dequeue();
        if (item)
        {
            // if (i == 0) std::cout << "Popped from band" << i << std::endl;
            NS_LOG_LOGIC("Popped from band " << i << ": " << item);
            NS_LOG_LOGIC("Number packets band " << i << ": " << GetInternalQueue(i)->GetNPackets());
            return item;
        }
    }

    NS_LOG_LOGIC("Queue empty");
    return item;
}

Ptr<const QueueDiscItem>
DDRQueueDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);

    Ptr<const QueueDiscItem> item;

    for (uint32_t i = 0; i < GetNInternalQueues(); i++)
    {
        item = GetInternalQueue(i)->Peek();
        if (item)
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
DDRQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);
    if (GetNQueueDiscClasses() > 0)
    {
        NS_LOG_ERROR("DDRQueueDisc cannot have classes");
        return false;
    }

    if (GetNPacketFilters() != 0)
    {
        NS_LOG_ERROR("DDRQueueDisc needs no packet filter");
        return false;
    }

    if (GetNInternalQueues() == 0)
    {
        // create 2 DropTail queues with GetLimit() packets each
        // std::cout << "Create 2 DropTail Queue" << std::endl;
        ObjectFactory factory;
        factory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>");
        factory.Set("MaxSize", QueueSizeValue(QueueSize("250KB")));
        AddInternalQueue(factory.Create<InternalQueue>());
        factory.Set("MaxSize", QueueSizeValue(QueueSize("2500KB")));
        AddInternalQueue(factory.Create<InternalQueue>());

        // GetInternalQueue (0)->GetMaxSize ();
        // GetInternalQueue (0)->GetMaxSize ();
    }

    if (GetNInternalQueues() != 2)
    {
        NS_LOG_ERROR("DGRVirtualQueueDisc needs 2 internal queues");
        return false;
    }

    if (GetInternalQueue(0)->GetMaxSize().GetUnit() != QueueSizeUnit::BYTES ||
        GetInternalQueue(1)->GetMaxSize().GetUnit() != QueueSizeUnit::BYTES)
    {
        NS_LOG_ERROR("DDRQueueDisc needs 2 internal queues operating in BYTES mode");
        return false;
    }
    return true;
}

void
DDRQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);
    m_fastWeight = 10;
    m_normalWeight = 1;
    m_currentFastWeight = m_fastWeight;
    m_currentNormalWeight = m_normalWeight;
}

uint32_t
DDRQueueDisc::EnqueueClassify(Ptr<QueueDiscItem> item)
{
    PriorityTag priorityTag;
    if (item->GetPacket()->PeekPacketTag(priorityTag))
    {
        return DELAY_SENSITIVE;
    }
    else
    {
        return BEST_EFFORT;
    }
}

} // namespace ns3
