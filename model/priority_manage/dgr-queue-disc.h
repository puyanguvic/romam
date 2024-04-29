#ifndef TEST_QUEUE_DISC_H
#define TEST_QUEUE_DISC_H

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
class DGRQueueDisc : public QueueDisc
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * \brief DGRQueueDisc Constructor
     *
     * Create a queue disc
     */
    DGRQueueDisc();

    /**
     * \brief Destructor
     *
     * Destructor
     */
    ~DGRQueueDisc() override;

    // Reasons for dropping packets
    static constexpr const char* LIMIT_EXCEEDED_DROP =
        "Queue disc limit exceeded"; //!< Packet dropped due to queue disc limit exceeded

  protected:
    /**
     * \brief Dispose of the object
     */
    void DoDispose() override;

  private:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;
    bool CheckConfig() override;
    void InitializeParams() override;
    Ptr<const QueueDiscItem> DoPeek();
    uint32_t EnqueueClassify(Ptr<QueueDiscItem> item);

    uint32_t LinesSize[3] = {17, 28, 1000};
};

} // namespace ns3

#endif /* TEST_QUEUE_DISC_H */
