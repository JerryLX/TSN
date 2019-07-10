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
 * Author: Jaume Nin <jaume.nin@cttc.cat>
 */

#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
#include <ns3/flow-monitor.h>
#include <ns3/flow-monitor-helper.h>
#include "ns3/bulk-send-application.h"

#include <iostream>
#include <fstream>
//#include "ns3/gtk-config-store.h"

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeB,
 * attaches one UE per eNodeB starts a flow for each UE to  and from a remote host.
 * It also  starts yet another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("EpcFirstExample");

int total_size = 0;
int total_payload = 0;
int packet_num_merged = 0;
int packet_num_split = 0;
int packet_num_sgw = 0;
int packet_num_sgw2 = 0;

double time_sgw = 0;
double time_sgw2 = 0;

void RxFromS1uTracer(Ptr<Packet> p)
{
  std::ofstream outfile;
  outfile.open("merged_packet.txt", std::ios::app);
  outfile << Simulator::Now().GetSeconds() << "  idx: " << packet_num_merged << "  size: " << p->GetSize() + 28<< std::endl;
  outfile.close();
  total_size += p->GetSize() + 28;
  packet_num_merged++;
}

void RxFromS1uTracer_split(Ptr<Packet> p)
{
  std::ofstream outfile;
  outfile.open("split_packet.txt", std::ios::app);
  outfile << Simulator::Now().GetSeconds() << "  idx: " << packet_num_split << "  size: " << p->GetSize() << std::endl;
  outfile.close();
  total_payload += p->GetSize();
  packet_num_split++;
}

void TxSGWS1uTracer(Ptr<Packet> p)
{
  std::ofstream outfile;
  outfile.open("sgw_packet.txt", std::ios::app);
  time_sgw += double(Simulator::Now().GetSeconds());
  outfile << Simulator::Now().GetSeconds() << "  idx: " << packet_num_sgw << "  size: " << p->GetSize() << std::endl;
  outfile.close();
  packet_num_sgw++;
}

/* void TxSGWS1uTracer2(Ptr<Packet> p)
{
  std::ofstream outfile;
  outfile.open("sgw_packet2.txt", std::ios::app);
  outfile << Simulator::Now().GetSeconds() << "  idx: " << packet_num_sgw << "  size: " << p->GetSize()<<std::endl;
  outfile.close();
  packet_num_sgw2++;
}
 */

double compute_time_cost2() 
{
    std::ifstream infile;  
	infile.open("tmp.txt", std::ios::in);
	double time = 0;
	uint16_t merge_num = 0;
	while (infile >> time >> merge_num)
	{
		time_sgw2 += time * merge_num;
		packet_num_sgw2 += merge_num;
	}
	infile.close();
	return ((time_sgw2/packet_num_sgw2)-(time_sgw/packet_num_sgw));
}
 
 
void PrintResult()
{
  std::cout<<"packet_count  :  "<<packet_num_split * 6 << "M" << std::endl;
  std::cout<<"Total Payload :  "<<total_payload * 6<< "MBytes " <<std::endl;
  std::cout<<"Total Size    :  "<<total_size * 6 << "MBytes " <<std::endl;
  std::cout<<"Bandwidth Usage: "<<double(total_payload)/double(total_size)<<std::endl;
  // std::cout<<"Average Time Cost: "<<compute_time_cost2()<<std::endl;
}


int
main (int argc, char *argv[])
{

  uint16_t numberOfNodes = 1;
  double simTime = 5;
  //double distance = 60.0;
  double distance = 0;
  double interPacketInterval = 100;
  bool useCa = false;

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("numberOfNodes", "Number of eNodeBs + UE pairs", numberOfNodes);
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("distance", "Distance between eNBs [m]", distance);
  cmd.AddValue("interPacketInterval", "Inter packet interval [ms])", interPacketInterval);
  cmd.AddValue("useCa", "Whether to use carrier aggregation.", useCa);
  cmd.Parse(argc, argv);

  if (useCa)
   {
     Config::SetDefault ("ns3::LteHelper::UseCa", BooleanValue (useCa));
     Config::SetDefault ("ns3::LteHelper::NumberOfComponentCarriers", UintegerValue (2));
     Config::SetDefault ("ns3::LteHelper::EnbComponentCarrierManager", StringValue ("ns3::RrComponentCarrierManager"));
   }

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper>  epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  // parse again so you can override default values from the command line
  cmd.Parse(argc, argv);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

   // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("10Mb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
 //// Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create(numberOfNodes);
  ueNodes.Create(numberOfNodes);

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint16_t i = 0; i < numberOfNodes; i++)
    {
      positionAlloc->Add (Vector(distance * i, 0, 0));
    }
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Attach one UE per eNodeB
  for (uint16_t i = 0; i < numberOfNodes; i++)
      {
        lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
        // side effect: the default EPS bearer will be activated
      }


  // Install and start applications on UEs and remote host
  uint16_t dlPort = 1234;
  uint16_t ulPort = 2000;
  uint16_t otherPort = 3000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;


  
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      ++ulPort;
      ++otherPort;
      PacketSinkHelper dlPacketSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
    serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get(u)));
	
//	  OnOffHelper source ("ns3::TcpSocketFactory",
  //                       InetSocketAddress(ueIpIface.GetAddress(u), dlPort));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  //    source.SetAttribute ("MaxBytes", UintegerValue (1000000000));
//	  source.SetAttribute ("DataRate", DataRateValue (DataRate ("10Mb/s")));
	//  source.SetAttribute ("PacketSize", UintegerValue (36));
	  //source.SetAttribute ("OnTime", RandomVariableValue (ConstantVariable (1)));
      //source.SetAttribute ("OffTime", RandomVariableValue (ConstantVariable (0)));

      //ApplicationContainer sourceApps = source.Install (remoteHost);
	
	

       UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
      dlClient.SetAttribute ("Interval", TimeValue (MicroSeconds(interPacketInterval)));
      dlClient.SetAttribute ("MaxPackets", UintegerValue(1000000000));
      dlClient.SetAttribute ("PacketSize", UintegerValue(36)); 
	  	  clientApps.Add (dlClient.Install (remoteHost));
	  


/*        UdpClientHelper dlClient2 (ueIpIface.GetAddress (u), dlPort);
      dlClient2.SetAttribute ("Interval", TimeValue (MicroSeconds(interPacketInterval*4)));
      dlClient2.SetAttribute ("MaxPackets", UintegerValue(1000000000));
      dlClient2.SetAttribute ("PacketSize", UintegerValue(228));
      clientApps.Add (dlClient2.Install (remoteHost));  */
      


    }
  serverApps.Start (MilliSeconds (1));
  clientApps.Start (MilliSeconds (1));

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/RxFromS1u", MakeCallback(&RxFromS1uTracer));
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/RxFromS1u_split", MakeCallback(&RxFromS1uTracer_split));

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/RxFromTun", MakeCallback(&TxSGWS1uTracer));
  // Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/RxFromTun2", MakeCallback(&TxSGWS1uTracer2));

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();
  
  lteHelper->EnableTraces();
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  /*GtkConfigStore config;
  config.ConfigureAttributes();*/

  Simulator::Destroy();

  PrintResult();
  flowMonitor->SerializeToXmlFile("tsn.xml", true, true);
  return 0;

}

