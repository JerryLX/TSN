#include <iomanip>
#include <iostream>
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "vlan-header.h"
#include "address-utils.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("VlanHeader");

NS_OBJECT_ENSURE_REGISTERED (VlanHeader);

VlanHeader::VlanHeader ()
  : m_lengthType (0),
    m_tpid(0x8100),
    m_fragCount(0)
{
  NS_LOG_FUNCTION (this);
}

void
VlanHeader::SetLengthType (uint16_t lengthType)
{
  NS_LOG_FUNCTION (this << lengthType);
  m_lengthType = lengthType;
}
uint16_t
VlanHeader::GetLengthType (void) const
{
  NS_LOG_FUNCTION (this);
  return m_lengthType;
}

void
VlanHeader::SetPreamble (uint64_t preamble)
{
  NS_LOG_FUNCTION (this << preamble);
  m_preamble = preamble;
}
uint64_t
VlanHeader::GetPreamble (void) const
{
  NS_LOG_FUNCTION (this);
  return m_preamble;
}

void
VlanHeader::SetSfd (uint8_t sfd)
{
  NS_LOG_FUNCTION (this << sfd);
  m_sfd = sfd;
}
uint8_t
VlanHeader::GetSfd (void) const
{
  NS_LOG_FUNCTION (this);
  return m_sfd;
}

void
VlanHeader::SetSource (Mac48Address source)
{
  NS_LOG_FUNCTION (this << source);
  m_src = source;
}
Mac48Address
VlanHeader::GetSource (void) const
{
  NS_LOG_FUNCTION (this);
  return m_src;
}

void 
VlanHeader::SetDestination (Mac48Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  m_dst = dst;
}
Mac48Address
VlanHeader::GetDestination (void) const
{
  NS_LOG_FUNCTION (this);
  return m_dst;
}

void
VlanHeader::SetFragCount (uint8_t fragCount)
{
  NS_LOG_FUNCTION (this << fragCount);
  m_fragCount = fragCount;
}
uint8_t
VlanHeader::GetFragCount (void) const
{
  NS_LOG_FUNCTION (this);
  return m_fragCount;
}

void
VlanHeader::SetPriority (uint8_t priority)
{
  NS_LOG_FUNCTION (this << priority);
  m_priority = priority;
}
uint8_t
VlanHeader::GetPriority (void) const
{
  NS_LOG_FUNCTION (this);
  return m_priority;
}

void
VlanHeader::SetCfi (bool cfi)
{
  NS_LOG_FUNCTION (this << cfi);
  m_cfi = cfi;
}
bool
VlanHeader::GetCfi (void) const
{
  NS_LOG_FUNCTION (this);
  return m_cfi;
}

void
VlanHeader::SetVid (uint16_t vid)
{
  NS_LOG_FUNCTION (this << vid);
  m_vid = vid;
}
uint16_t
VlanHeader::GetVid (void) const
{
  NS_LOG_FUNCTION (this);
  return m_vid;
}


void
VlanHeader::SetFlowId (uint32_t flowId)
{
  NS_LOG_FUNCTION (this << flowId);
  m_flowId = flowId;
}
uint32_t
VlanHeader::GetFlowId (void) const
{
  NS_LOG_FUNCTION (this);
  return m_flowId;
}

uint32_t 
VlanHeader::GetHeaderSize (void) const
{
  NS_LOG_FUNCTION (this);
  return GetSerializedSize ();
}


TypeId 
VlanHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::VlanHeader")
    .SetParent<Header> ()
    .SetGroupName("Network")
    .AddConstructor<VlanHeader> ()
  ;
  return tid;
}
TypeId 
VlanHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
void 
VlanHeader::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);

  os << "preamble/sfd=" << m_preamble << "," << m_sfd << ",";

  os << " length/type=0x" << std::hex << m_lengthType << std::dec
     << ", source=" << m_src
     << ", destination=" << m_dst
     << ", priority=" << m_priority;
}
uint32_t 
VlanHeader::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  if (m_fragCount == 0)
    return PREAMBLE_SIZE + LENGTH_SIZE + 2*MAC_ADDR_SIZE;
  else return PREAMBLE_SIZE;
}

void
VlanHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  
  if (m_fragCount == 0) {
    m_preamble = (m_preamble << 8) ^ m_sfd;
  } else {
    m_preamble = (((m_preamble << 8) ^ m_sfd) << 8) ^ m_fragCount;
  }
  i.WriteU64(m_preamble);
  if (m_fragCount == 0) {
    WriteTo(i, m_dst);
    WriteTo(i, m_src);
    i.WriteHtonU16 (m_lengthType);
  }
}
uint32_t
VlanHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;

  m_preamble = i.ReadU64 ();
  
  uint8_t temp = (m_preamble >> 8) & (0xff);
  if (temp == 0b10101010) {
    m_sfd = m_preamble & 0xff;
    m_preamble = m_preamble >> 8;
    ReadFrom (i, m_dst);
    ReadFrom (i, m_src);
    m_lengthType = i.ReadNtohU16 ();
  }else {
    m_fragCount = m_preamble & 0xff;
    m_sfd = (m_preamble >> 8) & 0xff;
    m_preamble = m_preamble >> 16;
  }
  return GetSerializedSize ();
}

} // namespace ns3
