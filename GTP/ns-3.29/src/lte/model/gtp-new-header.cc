#include "gtp-new-header.h"
#include "ns3/log.h"
#include "ns3/packet.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("GtpNewHeader");

/********************************************************
 *        GTP-New Header
 ********************************************************/

NS_OBJECT_ENSURE_REGISTERED (GtpNewHeader);

TypeId
GtpNewHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::GtpNewHeader")
    .SetParent<Header> ()
    .SetGroupName("Lte")
    .AddConstructor<GtpNewHeader> ();
  return tid;
}
GtpNewHeader::GtpNewHeader ()
  : m_version (3),
    m_protocolType (true),
    m_extensionHeaderFlag (false),
    m_sequenceNumberFlag (true),
    m_nPduNumberFlag (true),
    m_teidFlag(true),
    m_messageType (255),
    m_length (0),
    m_teid (0),
    m_teidHashValue(0),
    m_sequenceNumber (0),
    m_nPduNumber (0),
    m_nextExtensionType (0)

{

}

GtpNewHeader::~GtpNewHeader ()
{
}

TypeId
GtpNewHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
GtpNewHeader::GetSerializedSize (void) const
{
  if (m_version == 3 && m_teidFlag) return 12;
  else if (m_version == 3 && !m_teidFlag) return 8;
  else if (m_version == 4 && m_teidFlag) return 11;
  else return 7;
}
void
GtpNewHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  uint8_t firstByte = m_version << 5 | m_protocolType << 4 | m_extensionHeaderFlag << 3;
  firstByte |= m_sequenceNumberFlag << 2 | m_nPduNumberFlag << 1 | m_teidFlag;
  i.WriteU8 (firstByte);
  i.WriteU8 (m_length);
  if (m_version == 3) i.WriteU8 (m_messageType);
  if (m_teidFlag) i.WriteHtonU32 (m_teid);
  i.WriteU8 (m_teidHashValue);
  i.WriteHtonU16 (m_sequenceNumber);
  i.WriteU8 (m_nPduNumber);
  i.WriteU8 (m_nextExtensionType);

}
uint32_t
GtpNewHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  uint8_t firstByte = i.ReadU8 ();
  m_version = firstByte >> 5 & 0x7;
  m_protocolType = firstByte >> 4 & 0x1;
  m_extensionHeaderFlag = firstByte >> 3 & 0x1;
  m_sequenceNumberFlag  = firstByte >> 2 & 0x1;
  m_nPduNumberFlag = firstByte >> 1 & 0x1;
  m_teidFlag = firstByte & 0x1;

  m_length = i.ReadU8 ();
  if (m_version == 3) m_messageType = i.ReadU8 ();
  if (m_teidFlag) m_teid = i.ReadNtohU32 ();
  m_teidHashValue = i.ReadU8 ();
  m_sequenceNumber = i.ReadNtohU16 ();
  m_nPduNumber = i.ReadU8 ();
  m_nextExtensionType = i.ReadU8 ();
  return GetSerializedSize ();
}
void
GtpNewHeader::Print (std::ostream &os) const
{
  os << " version=" << (uint32_t) m_version << " [";
  if (m_protocolType)
    {
      os << " PT ";
    }
  if (m_extensionHeaderFlag)
    {
      os << " E ";
    }
  if (m_sequenceNumberFlag)
    {
      os << " S ";
    }
  if (m_nPduNumberFlag)
    {
      os << " PN ";
    }
  if (m_teidFlag)
    {
      os << " T ";
    }
  os << "], messageType=" << (uint32_t) m_messageType << ", length=" << (uint32_t) m_length;
  os << ", teid=" << (uint32_t) m_teid << ", sequenceNumber=" << (uint32_t) m_sequenceNumber;
  os << ", nPduNumber=" << (uint32_t) m_nPduNumber << ", nextExtensionType=" << (uint32_t) m_nextExtensionType;
}

bool
GtpNewHeader::GetExtensionHeaderFlag () const
{
  return m_extensionHeaderFlag;
}

uint8_t
GtpNewHeader::GetLength () const
{
  return m_length;
}

uint8_t
GtpNewHeader::GetMessageType () const
{
  return m_messageType;
}

uint8_t
GtpNewHeader::GetNPduNumber () const
{
  return m_nPduNumber;
}

bool
GtpNewHeader::GetNPduNumberFlag () const
{
  return m_nPduNumberFlag;
}

uint8_t
GtpNewHeader::GetNextExtensionType () const
{
  return m_nextExtensionType;
}

bool
GtpNewHeader::GetProtocolType () const
{
  return m_protocolType;
}

uint16_t
GtpNewHeader::GetSequenceNumber () const
{
  return m_sequenceNumber;
}

bool
GtpNewHeader::GetSequenceNumberFlag () const
{
  return m_sequenceNumberFlag;
}

uint8_t
GtpNewHeader::GetTeidHashValue () const
{
    return m_teidHashValue;
}

bool
GtpNewHeader::GetTeidFlag() const
{
  return m_teidFlag;
}

uint32_t
GtpNewHeader::GetTeid () const
{
  return m_teid;
}

uint8_t
GtpNewHeader::GetVersion () const
{
  return m_version;
}

void
GtpNewHeader::SetExtensionHeaderFlag (bool m_extensionHeaderFlag)
{
  this->m_extensionHeaderFlag = m_extensionHeaderFlag;
}

void
GtpNewHeader::SetLength (uint8_t m_length)
{
  this->m_length = m_length;
}

void
GtpNewHeader::SetMessageType (uint8_t m_messageType)
{
  this->m_messageType = m_messageType;
}

void
GtpNewHeader::SetNPduNumber (uint8_t m_nPduNumber)
{
  this->m_nPduNumber = m_nPduNumber;
}

void
GtpNewHeader::SetNPduNumberFlag (bool m_nPduNumberFlag)
{
  this->m_nPduNumberFlag = m_nPduNumberFlag;
}

void
GtpNewHeader::SetTeidHashValue (uint8_t m_teidHashValue) {
  this->m_teidHashValue = m_teidHashValue;
}

void
GtpNewHeader::SetNextExtensionType (uint8_t m_nextExtensionType)
{
  this->m_nextExtensionType = m_nextExtensionType;
}

void
GtpNewHeader::SetProtocolType (bool m_protocolType)
{
  this->m_protocolType = m_protocolType;
}

void
GtpNewHeader::SetTeidFlag (bool m_teidFlag)
{
  this->m_teidFlag = m_teidFlag;
}

void
GtpNewHeader::SetSequenceNumber (uint16_t m_sequenceNumber)
{
  this->m_sequenceNumber = m_sequenceNumber;
}

void
GtpNewHeader::SetSequenceNumberFlag (bool m_sequenceNumberFlag)
{
  this->m_sequenceNumberFlag = m_sequenceNumberFlag;
}

void
GtpNewHeader::SetTeid (uint32_t m_teid)
{
  this->m_teid = m_teid;
}

void
GtpNewHeader::SetVersion (uint8_t m_version)
{
  // m_version is a uint3_t
  this->m_version = m_version & 0x7;
}

bool
GtpNewHeader::operator == (const GtpNewHeader& b) const
{
  if (m_version == b.m_version
      && m_protocolType == b.m_protocolType
      && m_extensionHeaderFlag == b.m_extensionHeaderFlag
      && m_sequenceNumberFlag == b.m_sequenceNumberFlag
      && m_nPduNumberFlag == b.m_nPduNumberFlag
      && m_messageType == b.m_messageType
      && m_length == b.m_length
      && m_teid == b.m_teid
      && m_sequenceNumber == b.m_sequenceNumber
      && m_nPduNumber == b.m_nPduNumber
      && m_nextExtensionType == b.m_nextExtensionType
      && m_teidFlag == b.m_teidFlag
      && m_teidHashValue == b.m_teidHashValue
      )
    {
      return true;
    }
  return false;
}

} // namespace ns3
