/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "romam-udp-application.h"

#include "../datapath/romam-tags.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/timestamp-tag.h"

#include <iostream>

#define MAX_UINT_32 0xffffffff

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RomamUdpApplication");

NS_OBJECT_ENSURE_REGISTERED(RomamUdpApplication);

RomamUdpApplication::RomamUdpApplication()
    : m_socket(nullptr),
      m_peer(),
      m_packetSize(0),
      m_nPackets(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetSent(0),
      m_budget(MAX_UINT_32),
      m_flag(false),
      m_vbr(false),
      m_priority(false)
{
}

RomamUdpApplication::~RomamUdpApplication()
{
    m_socket = nullptr;
}

TypeId
RomamUdpApplication::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RomamUdpApplication")
                            .SetParent<Application>()
                            .SetGroupName("Romam")
                            .AddConstructor<RomamUdpApplication>()
        // .AddAttribute ("Variable_bitrate",
        //                "Enable the VBR",
        //                BooleanValue (false),
        //                MakeBooleanAccessor (&RomamUdpApplication::m_vbr),
        //                MakeBooleanChecker ())
        ;
    return tid;
}

void
RomamUdpApplication::Setup(Ptr<Socket> socket,
                           Address sinkAddress,
                           uint32_t packetSize,
                           uint32_t nPackets,
                           DataRate dataRate,
                           uint32_t budget,
                           bool flag)
{
    m_socket = socket;
    m_peer = sinkAddress;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
    m_budget = budget;
    m_flag = flag;
}

void
RomamUdpApplication::Setup(Ptr<Socket> socket,
                           Address sinkAddress,
                           uint32_t packetSize,
                           uint32_t nPackets,
                           DataRate dataRate,
                           bool flag)
{
    m_socket = socket;
    m_peer = sinkAddress;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
    m_flag = flag;
}

void
RomamUdpApplication::SetPriority(bool priority)
{
    m_priority = priority;
}

void
RomamUdpApplication::StartApplication(void)
{
    m_running = true;
    m_packetSent = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
}

void
RomamUdpApplication::StopApplication(void)
{
    m_running = false;
    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }
    if (m_socket)
    {
        m_socket->Close();
    }
}

void
RomamUdpApplication::SendPacket()
{
    TimestampTag txTimeTag;
    FlagTag flagTag;
    BudgetTag budgetTag;

    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    if (m_priority)
    {
        PriorityTag priorityTag;
        priorityTag.SetPriority(true);
        packet->AddPacketTag(priorityTag);
    }
    Time txTime = Simulator::Now();
    if (m_budget != MAX_UINT_32)
    {
        budgetTag.SetBudget(m_budget);
        packet->AddPacketTag(budgetTag);
    }
    flagTag.SetFlag(m_flag);
    txTimeTag.SetTimestamp(txTime);
    packet->AddPacketTag(txTimeTag);
    packet->AddPacketTag(flagTag);
    // std::cout << "Send a packet\n";
    m_socket->Send(packet);
    if (++m_packetSent < m_nPackets)
    {
        ScheduleTx();
    }
}

void
RomamUdpApplication::ScheduleTx()
{
    if (m_running)
    {
        if (m_vbr)
        {
            Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
            double rate = static_cast<double>(rand->GetInteger(1, 100)) / 100;
            Time tNext(
                Seconds(rate * m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
            m_sendEvent = Simulator::Schedule(tNext, &RomamUdpApplication::SendPacket, this);
        }
        else
        {
            Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
            m_sendEvent = Simulator::Schedule(tNext, &RomamUdpApplication::SendPacket, this);
        }
    }
}

void
RomamUdpApplication::ChangeRate(DataRate newDataRate)
{
    m_dataRate = newDataRate;
}

} // namespace ns3