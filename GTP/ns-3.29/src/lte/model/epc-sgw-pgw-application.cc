/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Jaume Nin <jnin@cttc.cat>
 *         Nicola Baldo <nbaldo@cttc.cat>
 */


#include "epc-sgw-pgw-application.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"
#include "ns3/ipv4.h"
#include "ns3/ipv6.h"
#include "ns3/ipv6-header.h"
#include "ns3/inet-socket-address.h"
#include "ns3/epc-gtpu-header.h"
#include "ns3/abort.h"
#include <queue>
#include <map>
#include "ns3/core-module.h"

#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EpcSgwPgwApplication");

/////////////////////////
// UeInfo
/////////////////////////


EpcSgwPgwApplication::UeInfo::UeInfo ()
{
  NS_LOG_FUNCTION (this);
}

void
EpcSgwPgwApplication::UeInfo::AddBearer (Ptr<EpcTft> tft, uint8_t bearerId, uint32_t teid)
{
  NS_LOG_FUNCTION (this << tft << teid);
  m_teidByBearerIdMap[bearerId] = teid;
  return m_tftClassifier.Add (tft, teid);
}

void
EpcSgwPgwApplication::UeInfo::RemoveBearer (uint8_t bearerId)
{
  NS_LOG_FUNCTION (this << bearerId);
  m_teidByBearerIdMap.erase (bearerId);
}

uint32_t
EpcSgwPgwApplication::UeInfo::Classify (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  // we hardcode DOWNLINK direction since the PGW is espected to
  // classify only downlink packets (uplink packets will go to the
  // internet without any classification). 
  return m_tftClassifier.Classify (p, EpcTft::DOWNLINK);
}

Ipv4Address 
EpcSgwPgwApplication::UeInfo::GetEnbAddr ()
{
  return m_enbAddr;
}

void
EpcSgwPgwApplication::UeInfo::SetEnbAddr (Ipv4Address enbAddr)
{
  m_enbAddr = enbAddr;
}

Ipv4Address 
EpcSgwPgwApplication::UeInfo::GetUeAddr ()
{
  return m_ueAddr;
}

void
EpcSgwPgwApplication::UeInfo::SetUeAddr (Ipv4Address ueAddr)
{
  m_ueAddr = ueAddr;
}

Ipv6Address 
EpcSgwPgwApplication::UeInfo::GetUeAddr6 ()
{
  return m_ueAddr6;
}

void
EpcSgwPgwApplication::UeInfo::SetUeAddr6 (Ipv6Address ueAddr)
{
  m_ueAddr6 = ueAddr;
}

/////////////////////////
// EpcSgwPgwApplication
/////////////////////////


TypeId
EpcSgwPgwApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EpcSgwPgwApplication")
    .SetParent<Object> ()
    .SetGroupName("Lte")
    .AddTraceSource ("RxFromTun",
                     "Receive data packets from internet in Tunnel net device",
                     MakeTraceSourceAccessor (&EpcSgwPgwApplication::m_rxTunPktTrace),
                     "ns3::EpcSgwPgwApplication::RxTracedCallback")
    .AddTraceSource ("RxFromS1u",
                     "Receive data packets from S1 U Socket",
                     MakeTraceSourceAccessor (&EpcSgwPgwApplication::m_rxS1uPktTrace),
                     "ns3::EpcSgwPgwApplication::RxTracedCallback")
    // .AddTraceSource ("TxToS1u",
    //                  "Transmit data packets from S1 U Socket",
    //                  MakeTraceSourceAccessor (&EpcSgwPgwApplication::m_txS1uPktTrace),
    //                  "ns3::EpcSgwPgwApplication::TxTracedCallback")    
    ;
  return tid;
}

void
EpcSgwPgwApplication::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_s1uSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  m_s1uSocket = 0;
  delete (m_s11SapSgw);
}

  
EpcSgwPgwApplication::EpcSgwPgwApplication (const Ptr<VirtualNetDevice> tunDevice, const Ptr<Socket> s1uSocket)
  : m_s1uSocket (s1uSocket),
    m_tunDevice (tunDevice),
    m_gtpuUdpPort (2152), // fixed by the standard
    m_teidCount (0),
    m_s11SapMme (0)
{
//*************************************************************************
  packet_in_queue = 0;
  total_payload = 0;
  total_size = 0;
  packet_count = 0;
//*************************************************************************
  NS_LOG_FUNCTION (this << tunDevice << s1uSocket);
  m_s1uSocket->SetRecvCallback (MakeCallback (&EpcSgwPgwApplication::RecvFromS1uSocket, this));
  m_s11SapSgw = new MemberEpcS11SapSgw<EpcSgwPgwApplication> (this);
//  Simulator::Schedule (MilliSeconds (800), PrintData, total_payload, total_size);
	
}

  
EpcSgwPgwApplication::~EpcSgwPgwApplication ()
{
  // std::cout<<"packet_count  :  "<<packet_count<<std::endl;
  // std::cout<<"total_payload :  "<<total_payload<<std::endl;
  // std::cout<<"total_size    :  "<<total_size<<std::endl;
  NS_LOG_FUNCTION (this);
}


bool
EpcSgwPgwApplication::RecvFromTunDevice (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << source << dest << packet << packet->GetSize ());
  m_rxTunPktTrace (packet->Copy ());
  Ptr<Packet> pCopy = packet->Copy ();

  uint8_t ipType;
  pCopy->CopyData (&ipType, 1);
  ipType = (ipType>>4) & 0x0f;

  // get IP address of UE
  // 直接加到队列
  if (ipType == 0x04)
    {	
      Ipv4Header ipv4Header;
      pCopy->RemoveHeader (ipv4Header);
      Ipv4Address ueAddr =  ipv4Header.GetDestination ();
      NS_LOG_LOGIC ("packet addressed to UE " << ueAddr);
      // find corresponding UeInfo address
      std::map<Ipv4Address, Ptr<UeInfo> >::iterator it = m_ueInfoByAddrMap.find (ueAddr);
      if (it == m_ueInfoByAddrMap.end ())
        {        
          NS_LOG_WARN ("unknown UE address " << ueAddr);
        }
      else
        {
          Ipv4Address enbAddr = it->second->GetEnbAddr ();      
          uint32_t teid = it->second->Classify (packet);   
          if (teid == 0)
            {
              NS_LOG_WARN ("no matching bearer for this packet");                   
            }
          else
            {
				SendToS1uSocket (packet, enbAddr, teid);
            }
        }
    }

  else if (ipType == 0x06)
    {
      Ipv6Header ipv6Header;
      pCopy->RemoveHeader (ipv6Header);
      Ipv6Address ueAddr =  ipv6Header.GetDestinationAddress ();
      NS_LOG_LOGIC ("packet addressed to UE " << ueAddr);
      // find corresponding UeInfo address
      std::map<Ipv6Address, Ptr<UeInfo> >::iterator it = m_ueInfoByAddrMap6.find (ueAddr);
      if (it == m_ueInfoByAddrMap6.end ())
        {        
          NS_LOG_WARN ("unknown UE address " << ueAddr);
        }
      else
        {
          Ipv4Address enbAddr = it->second->GetEnbAddr ();      
          uint32_t teid = it->second->Classify (packet);   
          if (teid == 0)
            {
              NS_LOG_WARN ("no matching bearer for this packet");                   
            }
          else
            {
              SendToS1uSocket (packet, enbAddr, teid);
            }
        }
    }
  else
    {
      NS_ABORT_MSG ("EpcSgwPgwApplication::RecvFromTunDevice - Unknown IP type...");
    }

  // there is no reason why we should notify the TUN
  // VirtualNetDevice that he failed to send the packet: if we receive
  // any bogus packet, it will just be silently discarded.
  const bool succeeded = true;
  return succeeded;
}

void 
EpcSgwPgwApplication::RecvFromS1uSocket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);  
  NS_ASSERT (socket == m_s1uSocket);
  Ptr<Packet> packet = socket->Recv ();
  GtpuHeader gtpu;
  packet->RemoveHeader (gtpu);
  uint32_t teid = gtpu.GetTeid ();

  SendToTunDevice (packet, teid);

  m_rxS1uPktTrace (packet->Copy ());
}

void 
EpcSgwPgwApplication::SendToTunDevice (Ptr<Packet> packet, uint32_t teid)
{
  NS_LOG_FUNCTION (this << packet << teid);
  NS_LOG_LOGIC (" packet size: " << packet->GetSize () << " bytes");

  uint8_t ipType;
  packet->CopyData (&ipType, 1);
  ipType = (ipType>>4) & 0x0f;

  if (ipType == 0x04)
    {
      m_tunDevice->Receive (packet, 0x0800, m_tunDevice->GetAddress (), m_tunDevice->GetAddress (), NetDevice::PACKET_HOST);
    }
  else if (ipType == 0x06)
    {
      m_tunDevice->Receive (packet, 0x86DD, m_tunDevice->GetAddress (), m_tunDevice->GetAddress (), NetDevice::PACKET_HOST);
    }
  else
    {
      NS_ABORT_MSG ("EpcSgwPgwApplication::SendToTunDevice - Unknown IP type...");
    }
}

void 
EpcSgwPgwApplication::SendToS1uSocket (Ptr<Packet> packet, Ipv4Address enbAddr, uint32_t teid)
{
  NS_LOG_FUNCTION (this << packet << enbAddr << teid);
  packet_count++;
  GtpuHeader gtpu;
  gtpu.SetTeid (teid);
  // From 3GPP TS 29.281 v10.0.0 Section 5.1
  // Length of the payload + the non obligatory GTP-U header
  total_payload += packet->GetSize() - 28;
  /* c++;
  if (c > 100000)
  {
    std::cout<<"total_payload          :      "<<total_payload<<std::endl;
    c = 0;
  } */
  gtpu.SetLength (packet->GetSize () + gtpu.GetSerializedSize () - 8);  
  packet->AddHeader (gtpu);
  // if (packet->GetSize() > 52)
  // {
  //     std::cout<<packet->GetSize()<<std::endl;
  // }
//  uint32_t flags = 0;
//  m_s1uSocket->SendTo (packet, flags, InetSocketAddress (enbAddr, m_gtpuUdpPort));
//  return;
//************************************************************************
  Map[enbAddr].push(packet);

  packet_in_queue+=1;
  // std::cout<<packet_in_queue<<std::endl;
  if(packet_in_queue >= 50){
	  //combination of the small GTP-Bag
	  Combine_And_Send(Map,m_s1uSocket,m_gtpuUdpPort,packet_in_queue, total_size);  
	  //std::cout<<"sending.......ok"<<std::endl;
	  //packet_in_queue = 0;
  }else{
//	  Simulator::Schedule (MilliSeconds (10000), Combine_And_Send,Map,m_s1uSocket,m_gtpuUdpPort,packet_in_queue, total_size);
	  
          //packet_in_queue = 0;
  }
}

void EpcSgwPgwApplication::PrintData(uint64_t total_payload, uint64_t total_size)
{
    std::cout<<"total payload:            "<<total_payload<<std::endl;
    std::cout<<"total size:               "<<total_size<<std::endl;
}

//
void EpcSgwPgwApplication::Combine_And_Send(std::map<Ipv4Address, std::queue<Ptr<Packet>>> & Map,Ptr<Socket> m_s1uSocket,uint16_t m_gtpuUdpPort,int & p_i_q, uint64_t &total_size){
    // std::cout<<"C&C"<<std::endl; 
    auto it = Map.begin();
    Ipv4Header ipv4;
    UdpHeader udp;
    while(it != Map.end()) {
        std::queue<Ptr<Packet>> packet_queue = it -> second;
        Ptr<Packet> packet;
        if(!packet_queue.empty()){
            packet = packet_queue.front();
            packet_queue.pop();
        }
 //  std::cout<<"packet_size_start"<< packet->GetSize()<<std::endl; 
        //std::cout<<packet->GetSize()<<std::endl;
        while(!packet_queue.empty()){
       //     std::cout<<"merge"<<std::endl;
            Ptr<Packet> packet_n = packet_queue.front();
     //       std::cout<<"packet before size : " << packet->GetSize()<<std::endl;
            packet->AddAtEnd(packet_n);
        //    std::cout<<"after add     : " << packet->GetSize()<<std::endl;
            packet_queue.pop();
        }
        if (packet->GetSize() > 52)
        {
          
         //   std::cout<<"send_total length:  " << packet->GetSize()<<std::endl;  
        }
        packet->AddHeader(udp);
        packet->AddHeader(ipv4);
       // std::cout<<packet->GetSize()<<std::endl;  
        total_size += packet->GetSize();
   //     std::cout<<"total_size:    "<<total_size<<std::endl;
        // m_txS1uPktTrace (packet->Copy ());
        m_s1uSocket->SendTo (packet, 0, InetSocketAddress (it->first, m_gtpuUdpPort));
        //std::cout<<"sending.......ok"<<std::endl;
        it++;
    }
        Map.clear();
        p_i_q = 0;
}
//****************************************************

void 
EpcSgwPgwApplication::SetS11SapMme (EpcS11SapMme * s)
{
  m_s11SapMme = s;
}

EpcS11SapSgw* 
EpcSgwPgwApplication::GetS11SapSgw ()
{
  return m_s11SapSgw;
}

void 
EpcSgwPgwApplication::AddEnb (uint16_t cellId, Ipv4Address enbAddr, Ipv4Address sgwAddr)
{
  NS_LOG_FUNCTION (this << cellId << enbAddr << sgwAddr);
  EnbInfo enbInfo;
  enbInfo.enbAddr = enbAddr;
  enbInfo.sgwAddr = sgwAddr;
  m_enbInfoByCellId[cellId] = enbInfo;
}

void 
EpcSgwPgwApplication::AddUe (uint64_t imsi)
{
  NS_LOG_FUNCTION (this << imsi);
  Ptr<UeInfo> ueInfo = Create<UeInfo> ();
  m_ueInfoByImsiMap[imsi] = ueInfo;
}

void 
EpcSgwPgwApplication::SetUeAddress (uint64_t imsi, Ipv4Address ueAddr)
{
  NS_LOG_FUNCTION (this << imsi << ueAddr);
  std::map<uint64_t, Ptr<UeInfo> >::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi); 
  m_ueInfoByAddrMap[ueAddr] = ueit->second;
  ueit->second->SetUeAddr (ueAddr);
}

void 
EpcSgwPgwApplication::SetUeAddress6 (uint64_t imsi, Ipv6Address ueAddr)
{
  NS_LOG_FUNCTION (this << imsi << ueAddr);
  std::map<uint64_t, Ptr<UeInfo> >::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi); 
  m_ueInfoByAddrMap6[ueAddr] = ueit->second;
  ueit->second->SetUeAddr6 (ueAddr);
}

void 
EpcSgwPgwApplication::DoCreateSessionRequest (EpcS11SapSgw::CreateSessionRequestMessage req)
{
  NS_LOG_FUNCTION (this << req.imsi);
  std::map<uint64_t, Ptr<UeInfo> >::iterator ueit = m_ueInfoByImsiMap.find (req.imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << req.imsi); 
  uint16_t cellId = req.uli.gci;
  std::map<uint16_t, EnbInfo>::iterator enbit = m_enbInfoByCellId.find (cellId);
  NS_ASSERT_MSG (enbit != m_enbInfoByCellId.end (), "unknown CellId " << cellId); 
  Ipv4Address enbAddr = enbit->second.enbAddr;
  ueit->second->SetEnbAddr (enbAddr);

  EpcS11SapMme::CreateSessionResponseMessage res;
  res.teid = req.imsi; // trick to avoid the need for allocating TEIDs on the S11 interface

  for (std::list<EpcS11SapSgw::BearerContextToBeCreated>::iterator bit = req.bearerContextsToBeCreated.begin ();
       bit != req.bearerContextsToBeCreated.end ();
       ++bit)
    {
      // simple sanity check. If you ever need more than 4M teids
      // throughout your simulation, you'll need to implement a smarter teid
      // management algorithm. 
      NS_ABORT_IF (m_teidCount == 0xFFFFFFFF);
      uint32_t teid = ++m_teidCount;  
      ueit->second->AddBearer (bit->tft, bit->epsBearerId, teid);

      EpcS11SapMme::BearerContextCreated bearerContext;
      bearerContext.sgwFteid.teid = teid;
      bearerContext.sgwFteid.address = enbit->second.sgwAddr;
      bearerContext.epsBearerId =  bit->epsBearerId; 
      bearerContext.bearerLevelQos = bit->bearerLevelQos; 
      bearerContext.tft = bit->tft;
      res.bearerContextsCreated.push_back (bearerContext);
    }
  m_s11SapMme->CreateSessionResponse (res);
  
}

void 
EpcSgwPgwApplication::DoModifyBearerRequest (EpcS11SapSgw::ModifyBearerRequestMessage req)
{
  NS_LOG_FUNCTION (this << req.teid);
  uint64_t imsi = req.teid; // trick to avoid the need for allocating TEIDs on the S11 interface
  std::map<uint64_t, Ptr<UeInfo> >::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi); 
  uint16_t cellId = req.uli.gci;
  std::map<uint16_t, EnbInfo>::iterator enbit = m_enbInfoByCellId.find (cellId);
  NS_ASSERT_MSG (enbit != m_enbInfoByCellId.end (), "unknown CellId " << cellId); 
  Ipv4Address enbAddr = enbit->second.enbAddr;
  ueit->second->SetEnbAddr (enbAddr);
  // no actual bearer modification: for now we just support the minimum needed for path switch request (handover)
  EpcS11SapMme::ModifyBearerResponseMessage res;
  res.teid = imsi; // trick to avoid the need for allocating TEIDs on the S11 interface
  res.cause = EpcS11SapMme::ModifyBearerResponseMessage::REQUEST_ACCEPTED;
  m_s11SapMme->ModifyBearerResponse (res);
}
 
void
EpcSgwPgwApplication::DoDeleteBearerCommand (EpcS11SapSgw::DeleteBearerCommandMessage req)
{
  NS_LOG_FUNCTION (this << req.teid);
  uint64_t imsi = req.teid; // trick to avoid the need for allocating TEIDs on the S11 interface
  std::map<uint64_t, Ptr<UeInfo> >::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi);

  EpcS11SapMme::DeleteBearerRequestMessage res;
  res.teid = imsi;

  for (std::list<EpcS11SapSgw::BearerContextToBeRemoved>::iterator bit = req.bearerContextsToBeRemoved.begin ();
       bit != req.bearerContextsToBeRemoved.end ();
       ++bit)
    {
      EpcS11SapMme::BearerContextRemoved bearerContext;
      bearerContext.epsBearerId =  bit->epsBearerId;
      res.bearerContextsRemoved.push_back (bearerContext);
    }
  //schedules Delete Bearer Request towards MME
  m_s11SapMme->DeleteBearerRequest (res);
}

void
EpcSgwPgwApplication::DoDeleteBearerResponse (EpcS11SapSgw::DeleteBearerResponseMessage req)
{
  NS_LOG_FUNCTION (this << req.teid);
  uint64_t imsi = req.teid; // trick to avoid the need for allocating TEIDs on the S11 interface
  std::map<uint64_t, Ptr<UeInfo> >::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi);

  for (std::list<EpcS11SapSgw::BearerContextRemovedSgwPgw>::iterator bit = req.bearerContextsRemoved.begin ();
       bit != req.bearerContextsRemoved.end ();
       ++bit)
    {
      //Function to remove de-activated bearer contexts from S-Gw and P-Gw side
      ueit->second->RemoveBearer (bit->epsBearerId);
    }
}

}  // namespace ns3
