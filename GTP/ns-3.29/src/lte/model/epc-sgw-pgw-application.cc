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

#include "ns3/gtp-new-header.h"
#include "ns3/abort.h"
#include <queue>
#include <map>
#include "ns3/core-module.h"
#include <stdlib.h>
#include <ctime>
#include <random>
#define QSIZE 5
#define MAXQL 10 // Max queue length
#define MINQL 0
#define MAXWT 5 //Max wait time
#define MINWT 0
#define GtpuHeader GtpNewHeader
#include "ns3/udp-header.h"
#include "ns3/ipv4-header.h"
namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EpcSgwPgwApplication");

/////////////////////////
// UeInfo
/////////////////////////

//clock_t start,end;

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
/* 	.AddTraceSource ("RxFromTun2",
					 "Receive data packets from internet in Tunnel net device",
					 MakeTraceSourceAccessor (&EpcSgwPgwApplication::m_rxTunPktTrace2),
					 "ns3::EpcSgwPgwApplication::RxTracedCallback") */
    .AddTraceSource ("RxFromS1u",
                     "Receive data packets from S1 U Socket",
                     MakeTraceSourceAccessor (&EpcSgwPgwApplication::m_rxS1uPktTrace),
                     "ns3::EpcSgwPgwApplication::RxTracedCallback")
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
  for(int i = 0;i < 256;i++){
    ept[i] = 1;
  }
  std::cout<<"asda"<<std::endl;
//*************************************************************************
  NS_LOG_FUNCTION (this << tunDevice << s1uSocket);
  m_s1uSocket->SetRecvCallback (MakeCallback (&EpcSgwPgwApplication::RecvFromS1uSocket, this));
  m_s11SapSgw = new MemberEpcS11SapSgw<EpcSgwPgwApplication> (this);
}


EpcSgwPgwApplication::~EpcSgwPgwApplication ()
{
  NS_LOG_FUNCTION (this);
}


bool
EpcSgwPgwApplication::RecvFromTunDevice (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << source << dest << packet << packet->GetSize ());
  // m_rxTunPktTrace (packet->Copy ());
  double time_sgw = 0;
  time_sgw += double(Simulator::Now().GetSeconds());
  std::cout<<"time_sgw  " << time_sgw<<std::endl;
  
  Ptr<Packet> pCopy = packet->Copy ();

  uint8_t ipType;
  pCopy->CopyData (&ipType, 1);
  ipType = (ipType>>4) & 0x0f;

  // get IP address of UE
  // ֱ?Ӽӵ?????
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
//**********************************************************************************************************
void
EpcSgwPgwApplication::SendToS1uSocket (Ptr<Packet> packet, Ipv4Address enbAddr, uint32_t teid)
{
  NS_LOG_FUNCTION (this << packet << enbAddr << teid);
  m_rxTunPktTrace (packet->Copy ());
  GtpNewHeader gtpu;
  gtpu.SetTeid (teid);
  uint8_t msgtp = rand() % 4 + 1;//4 kind of msg types Random here
  gtpu.SetMessageType(msgtp);
  //Calculate teidhash

  bool newt = 1;
  int tmp = find_insert_teid(teid,newt);//Find teid hash in the hash table
  uint8_t teid_hs;
  if(tmp == -1){// hash table full
    tmp = find_insert_teid(teid,newt);
    teid_hs = tmp;
    gtpu.SetTeidFlag(true);
  }else{
    if(newt == 1){//teid_hash not in the hash table
        teid_hs = tmp;
        gtpu.SetTeidFlag(true);
    }else{//teid_hash in hash table
        gtpu.SetTeidFlag(false);
        teid_hs = tmp;
    }
  }
  gtpu.SetTeidHashValue(teid_hs);
  //std::cout <<"f"<<gtpu.GetTeidFlag()<<"Teid"<< gtpu.GetTeid()<< gtpu.GetTeidHashValue() << "---hs" << std::endl;
  gtpu.SetLength (packet->GetSize ());
  //std::cout << gtpu.GetLength() << " " << packet->GetSize () << std::endl;
  packet->AddHeader (gtpu);
  if(Map.find(enbAddr) == Map.end()){//enBAddr not in the map,new a map item.
    Map[enbAddr] = QueueManager(enbAddr);
  }
  QueueManager &qm = Map[enbAddr];
  PC_Queue &t_q = qm.Que_map[msgtp];
//  t_q.pq.push(packet);
  if(!gtpu.GetTeidFlag())
       t_q.pq.push(packet);
  else {
	     UdpHeader udp;
   packet->AddHeader(udp);
   Ipv4Header ipv4;
   packet->AddHeader(ipv4);
     m_s1uSocket->SendTo (packet, 0, InetSocketAddress (enbAddr, m_gtpuUdpPort));
     return;
  }
  //std::cout<<"TEID:"<<gtpu.GetTeid()<<"TEIDHS:"<<gtpu.GetTeidHashValue()<<"Version:"<<gtpu.GetVersion()<<std::endl;
  if(t_q.pq.size() >= t_q.max_len){
    //std::cout<<"queue full and send ... ..."<<t_q.pq.size()<<std::endl;

    Combine_And_Send2(t_q,enbAddr, m_s1uSocket , m_gtpuUdpPort , 1);
    //Combine_And_Send(Map,m_s1uSocket,m_gtpuUdpPort,packet_in_queue);
  }else{
  //  std::cout<<"time up and send ... ..."<<std::endl;
    Simulator::Schedule(MilliSeconds (t_q.wait_time),Combine_And_Send2,t_q,enbAddr,m_s1uSocket,m_gtpuUdpPort,0);
  }
}
void EpcSgwPgwApplication::Combine_And_Send2(PC_Queue & pcq,Ipv4Address ip,Ptr<Socket> m_s1uSocket,uint16_t m_gtpuUdpPort,int mode){
   std::queue<Ptr<Packet>> &packet_queue = pcq.pq;
   //std::cout<< packet->GetSize() <<std::endl;

   uint16_t merge_num = 0;
   Ptr<Packet> packet;
   // start = clock();
   if(!packet_queue.empty()){
        packet = packet_queue.front();
        packet_queue.pop();
		merge_num++;
   }
  std::cout<<packet->GetSize()<<std::endl;
   while(!packet_queue.empty()){//Set the last packet GTPV-4(no msg type)
        Ptr<Packet> packet_n = packet_queue.front();
        GtpuHeader gtpu;
        packet_n->RemoveHeader (gtpu);
        gtpu.SetVersion(4);
        packet_n->AddHeader(gtpu);
        packet->AddAtEnd(packet_n);
        packet_queue.pop();
		merge_num++;
   }
  // std::cout<<packet->GetSize()<<std::endl;

   if(mode == 1){// modify parameters when queue full more than 3
        pcq.cnt_timeout = 0;
        pcq.cnt_queue_full++;
        if(pcq.cnt_queue_full >= 3){
            //std::cout<<"Modify parameters(3 continuous queue full): queue_length:"<<pcq.max_len<<" ,wait_time:"<<pcq.wait_time<<std::endl;
            pcq.cnt_queue_full = 0;
            pcq.max_len += pcq.a;
            pcq.wait_time -= pcq.b;
            if(pcq.max_len > MAXQL){
                pcq.max_len = MAXQL;
            }
            if(pcq.wait_time < MINWT){
                pcq.wait_time = MINWT;
            }
            //std::cout<<"After_modify: queue_length"<<pcq.max_len<<" ,wait_time:"<<pcq.wait_time<<std::endl;
        }
   }else{// modify parameters when time out more than 3
       pcq.cnt_queue_full = 0;
       pcq.cnt_timeout ++;
       if(pcq.cnt_timeout >= 3){
          //std::cout<<"Modify parameters(3 continuous time out): queue_length:"<<pcq.max_len<<" ,wait_time:"<<pcq.wait_time<<std::endl;
          pcq.cnt_timeout = 0;
          pcq.wait_time += pcq.b;
          pcq.max_len -= pcq.a;
          if(pcq.wait_time > MAXWT){
               pcq.wait_time = MAXWT;
          }
          if(pcq.max_len < MINQL){
               pcq.max_len = MINQL;
          }
          //std::cout<<"Modify parameters: queue_length:"<<pcq.max_len<<" ,wait_time:"<<pcq.wait_time<<std::endl;
       }
   }
   //printf("%d\n", packet->GetSize());
   //  printf("send   %d\n", packet->GetSize());
   //end = clock();
   //double endtime = (double)(end-start)/CLOCKS_PER_SEC;
   //std::cout<<"Total time: "<<endtime*1000<<"ms"<<std::endl;
   // m_rxTunPktTrace2 (packet->Copy ());
  // srand(time(NULL));
  // if ((double)rand()/RAND_MAX < 0.02)
   //	  return;

   record_merged_packet(packet->Copy(), merge_num);
   //printf("%d\n", packet->GetSize());
   UdpHeader udp;
   packet->AddHeader(udp);
   Ipv4Header ipv4;
   packet->AddHeader(ipv4);
   //printf("send   %d\n", packet->GetSize());


   m_s1uSocket->SendTo (packet, 0, InetSocketAddress (ip, m_gtpuUdpPort));
      packet->RemoveHeader(ipv4);
	     packet->RemoveHeader(udp);
}
//*****************************************************************************

void EpcSgwPgwApplication:: record_merged_packet(Ptr<Packet> p , uint16_t merge_num)
{
  std::ofstream outfile;
  outfile.open("sgw_packet2.txt", std::ios::app);
  outfile << Simulator::Now().GetSeconds() << "  size: " << p->GetSize()<< " merge_num: " << merge_num << std::endl;
  outfile.close();

  outfile.open("tmp.txt", std::ios::app);
  outfile << Simulator::Now().GetSeconds() << " "<< merge_num << std::endl;
  outfile.close();
}


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
