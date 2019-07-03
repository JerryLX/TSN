/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef MULTI_QUEUE_H
#define MULTI_QUEUE_H

#include <vector>
#include "ns3/drop-tail-queue.h"
#include "ns3/vlan-header.h"

namespace ns3 {
/**
 * \ingroup multi-queue
 * 
 * \brief A Multilevel queue for packets.
 * 
 * Multi level queue used by TSN scheduling algorithm. 
 * Different queues represent as different priority.
 */

class MultiQueue
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId.
   */
  static TypeId GetTypeId (void);

  /**
   * \brief MultiQueue Constructor
   * 
   * Create a multi-queue.
   * Optional parameter: nQueue, number of queues.
   */
  MultiQueue();
  MultiQueue(uint8_t nQueue);
  ~MultiQueue ();

  /**
   * \brief push a packet into the queue.
   */
  virtual bool Enqueue (Ptr<Packet> packet);
  virtual Ptr<Packet> Dequeue (void);
  virtual bool IsEmpty (void);

protected:
  virtual int ChooseQueueForEnqueue (Ptr<Packet> packet);
  virtual int ChooseQueueForDequeue (void);
  bool DoEnqueue (Ptr<Packet> packet, int qid);
  Ptr<Packet> DoDequeue (int qid);
private:
  uint8_t m_queueAmount; //Number of queues, "uint8_t" is the type of vlan header priority.
  std::vector<DropTailQueue<Packet>> m_multiQueue;  //Container of queues
};

TypeId MultiQueue::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MultiQueue")
    .SetParent<Queue<Packet>>()
    .SetGroupName("Network")
  ;
  return tid;
}

MultiQueue::MultiQueue()
{
  m_queueAmount = 8;
  for(uint8_t i = 0; i < m_queueAmount; i++)
    m_multiQueue.push_back(DropTailQueue<Packet>());
}

MultiQueue::MultiQueue(uint8_t nQueue)
{
  m_queueAmount = nQueue;
  for(uint8_t i = 0; i < m_queueAmount; i++)
    m_multiQueue.push_back(DropTailQueue<Packet>());
}

MultiQueue::~MultiQueue ()
{
}

bool
MultiQueue::Enqueue (Ptr<Packet> packet)
{

  int enqueId = ChooseQueueForEnqueue(packet);
  return DoEnqueue(packet, enqueId);
}

Ptr<Packet>
MultiQueue::Dequeue (void)
{
  
  int dequeId = ChooseQueueForDequeue();
  if (dequeId >= 0)
  {
    Ptr<Packet> packet = DoDequeue(dequeId);
    return packet;
  }
  else return nullptr;
}

bool 
MultiQueue::IsEmpty ()
{
  for(uint8_t i = 0; i < m_queueAmount; i++)
    if(!m_multiQueue[i].IsEmpty())
      return false;
  return true;
}

int
MultiQueue::ChooseQueueForEnqueue (Ptr<Packet> packet)
{
  VlanHeader packetHeader;
  packet->PeekHeader(packetHeader);
  return(packetHeader.GetPriority() / (8 / m_queueAmount));
}

int
MultiQueue::ChooseQueueForDequeue (void)
{
  for(uint8_t i = m_queueAmount - 1; i >= 0; i--)
    if(!m_multiQueue[i].IsEmpty())
      return i;
  return -1;
}

bool
MultiQueue::DoEnqueue (Ptr<Packet> packet, int qid)
{
  return m_multiQueue[qid].Enqueue(packet);
}

Ptr<Packet>
MultiQueue::DoDequeue (int qid)
{
  Ptr<Packet> packet = m_multiQueue[qid].Dequeue();
  return packet;
}

} // namespace ns3

#endif /* MULTI_QUEUE_H */
