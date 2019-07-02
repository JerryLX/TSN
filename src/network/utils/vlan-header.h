#ifndef VLAN_HEADER_H
#define VLAN_HEADER_H

#include "ns3/header.h"
#include <string>
#include "ns3/mac48-address.h"

namespace ns3 {

class VlanHeader : public Header 
{
public:
  
  VlanHeader ();
  
  void SetLengthType (uint16_t size);

  uint16_t GetLengthType (void) const;
  
  void SetSource (Mac48Address source);

  Mac48Address GetSource (void) const;
  
  void SetDestination (Mac48Address destination);

  Mac48Address GetDestination (void) const;
  
  void SetPreamble (uint64_t preamble);

  uint64_t GetPreamble (void) const;
  
  void SetSfd (uint8_t sfd);

  uint8_t GetSfd (void) const;
  
  void SetFragCount (uint8_t fragCount);

  uint8_t GetFragCount (void) const;
  
  void SetPriority (uint8_t priority);

  uint8_t GetPriority (void) const;
  
  void SetCfi (bool cfi);

  bool GetCfi (void) const;
    
  void SetVid (uint16_t vid);

  uint16_t GetVid (void) const;
  
  void SetFlowId (uint32_t flowId);
  
  uint32_t GetFlowId (void) const;
  
  /**
   * \return The size of the header
   */
  uint32_t GetHeaderSize () const;

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);

private:
  
  static const int PREAMBLE_SIZE = 8; //!< size of the preamble_sfd header field
  static const int LENGTH_SIZE = 2;   //!< size of the length_type header field
  static const int MAC_ADDR_SIZE = 6; //!< size of src/dest addr header fields

  
  mutable uint64_t m_preamble;      //前导码字段,实际7字节
  uint8_t m_sfd;           //SFD字段
  uint8_t m_fragCount;     //帧抢占中分片计数字段
  uint16_t m_lengthType;      //Length or type of the packet
  Mac48Address m_src;        // Source address
  Mac48Address m_dst;   // Destination address
  uint16_t m_tpid;      //固定数值0x8100
  uint8_t m_priority;   //优先级字段，实际3位
  bool m_cfi;           //是否可丢弃标记
  uint16_t m_vid;       //虚拟局域网序号，实际12位
  uint16_t m_tci;      //前面三个字段组成，一共16位
  
  uint32_t m_flowId;    //帧所属流的序号
};

} // namespace ns3


#endif /* ETHERNET_HEADER_H */
