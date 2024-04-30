#ifndef DDR_QUEUE_DISC_H
#define DDR_QUEUE_DISC_H

#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/queue-disc.h"
#include "ns3/random-variable-stream.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/traced-value.h"

namespace ns3
{

/**
 * \ingroup traffic-control
 *
 * \brief A TBF packet queue disc
 */
class DDRQueueDisc : public QueueDisc
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * \brief DDRQueueDisc Constructor
     *
     * Create a queue disc
     */
    DDRQueueDisc();

    /**
     * \brief Destructor
     *
     * Destructor
     */
    ~DDRQueueDisc() override;

    // Reasons for dropping packets
    static constexpr const char* LIMIT_EXCEEDED_DROP =
        "Queue disc limit exceeded"; //!< Packet dropped due to queue disc limit exceeded

    uint32_t GetQueueStatus();
    uint32_t GetQueueDelay();

  protected:
    /**
     * \brief Dispose of the object
     */
    void DoDispose() override;

  private:
    uint32_t m_fastWeight;
    uint32_t m_normalWeight;
    uint32_t m_currentFastWeight;
    uint32_t m_currentNormalWeight;

    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    Ptr<const QueueDiscItem> DoPeek() override;
    bool CheckConfig() override;
    void InitializeParams() override;

    uint32_t EnqueueClassify(Ptr<QueueDiscItem> item);
};

} // namespace ns3

#endif /* DDR_QUEUE_DISC_H */
