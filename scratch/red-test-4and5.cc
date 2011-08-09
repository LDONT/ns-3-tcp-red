/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Authors: Marcos Talau <talau@users.sourceforge.net>
 *          Duy Nguyen <duy@soe.ucsc.edu>
 *
 */

/**
 * These validation tests are detailed in http://icir.org/floyd/papers/redsims.ps
 */

/** Network topology
 *
 *    10Mb/s, 2ms                            10Mb/s, 4ms
 * n0--------------|                    |---------------n4
 *                 |   1.5Mbps/s, 20ms  |
 *                 n2------------------n3
 *    10Mb/s, 3ms  |                    |    10Mb/s, 5ms
 * n1--------------|                    |---------------n5
 *
 *
 */

#include "ns3/core-module.h"
#include "ns3/simulator-module.h"
#include "ns3/node-module.h"
#include "ns3/helper-module.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/red-queue.h"
#include "ns3/uinteger.h"

#define FILE_PLOT_QUEUE "/tmp/red-queue.plotme"
#define FILE_PLOT_QUEUE_AVG "/tmp/red-queue_avg.plotme"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RedEx1");

uint32_t checkTimes;
double avgQueueSize;

void
CheckQueueSize (Ptr<Queue> queue)
{
  uint32_t qSize = StaticCast<RedQueue> (queue)->GetQueueSize ();

  avgQueueSize += qSize;
  checkTimes++;

  // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queue);

  std::ofstream fPlotQueue (FILE_PLOT_QUEUE, std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue.close();

  std::ofstream fPlotQueueAvg (FILE_PLOT_QUEUE_AVG, std::ios::out|std::ios::app);
  fPlotQueueAvg << Simulator::Now ().GetSeconds () << " " << avgQueueSize / checkTimes << std::endl;
  fPlotQueueAvg.close();
}

int
main (int argc, char *argv[])
{
  //  LogComponentEnable ("RedEx1", LOG_LEVEL_INFO);
  //  LogComponentEnable ("RedQueue", LOG_LEVEL_ALL);
  //   LogComponentEnable ("TcpNewReno", LOG_LEVEL_INFO);

  bool flowMonitor = true;
  bool m_writeResults = true;
  uint32_t redTest = 0;

  // The times
  double global_start_time;
  double global_stop_time;
  double sink_start_time;
  double sink_stop_time;
  double client_start_time;
  double client_stop_time;

  std::string redLinkDataRate = "1.5Mbps";
  std::string redLinkDelay = "20ms";

  global_start_time = 0.0;
  global_stop_time = 11; 
  sink_start_time = global_start_time;
  sink_stop_time = global_stop_time + 3.0;
  client_start_time = sink_start_time + 0.2;
  client_stop_time = global_stop_time - 2.0;

  // Configuration and command line parameter parsing
  CommandLine cmd;
  cmd.AddValue ("testnumber", "Run test 1 or 3", redTest);
  cmd.Parse (argc, argv);

  if ((redTest == 0) || ((redTest != 4) && (redTest != 5)))
    {
      std::cout << "Please, use arg --testnumber=4/5" << std::endl;
      exit(1);
    }

  NS_LOG_INFO ("Create nodes");
  NodeContainer c;
  c.Create (6);
  Names::Add ( "N0", c.Get (0));
  Names::Add ( "N1", c.Get (1));
  Names::Add ( "N2", c.Get (2));
  Names::Add ( "N3", c.Get (3));
  Names::Add ( "N4", c.Get (4));
  Names::Add ( "N5", c.Get (5));
  NodeContainer n0n2 = NodeContainer (c.Get (0), c.Get (2));
  NodeContainer n1n2 = NodeContainer (c.Get (1), c.Get (2));
  NodeContainer n2n3 = NodeContainer (c.Get (2), c.Get (3));
  NodeContainer n3n4 = NodeContainer (c.Get (3), c.Get (4));
  NodeContainer n3n5 = NodeContainer (c.Get (3), c.Get (5));

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpReno"));
  // 42 = headers size
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1000 - 42));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

  uint32_t meanPktSize = 500;

  // RED params
  Config::SetDefault ("ns3::RedQueue::MeanPktSize", UintegerValue (meanPktSize));
  Config::SetDefault ("ns3::RedQueue::Wait", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueue::Gentle", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueue::m_qW", DoubleValue (0.002));
  Config::SetDefault ("ns3::RedQueue::LinkBandwidth", StringValue(redLinkDataRate));
  Config::SetDefault ("ns3::RedQueue::LinkDelay", StringValue(redLinkDelay));
  if (redTest == 4) // packet mode
    {
      Config::SetDefault ("ns3::RedQueue::Mode", StringValue("Packets"));
      Config::SetDefault ("ns3::RedQueue::m_minTh", DoubleValue (5));
      Config::SetDefault ("ns3::RedQueue::m_maxTh", DoubleValue (15));
      Config::SetDefault ("ns3::RedQueue::QueueLimit", UintegerValue (25));
    }
  else // test 5, byte mode
    {
      Config::SetDefault ("ns3::RedQueue::Mode", StringValue("Bytes"));
      Config::SetDefault ("ns3::RedQueue::m_ns1Compat", BooleanValue (true));
      Config::SetDefault ("ns3::RedQueue::m_minTh", DoubleValue (5 * meanPktSize));
      Config::SetDefault ("ns3::RedQueue::m_maxTh", DoubleValue (15 * meanPktSize));
      Config::SetDefault ("ns3::RedQueue::QueueLimit", UintegerValue (25 * meanPktSize));
    }

  // fix the TCP window size
  uint16_t wnd = 15000;
  GlobalValue::Bind ("GlobalFixedTcpWindowSize", IntegerValue (wnd));

  InternetStackHelper internet;
  internet.Install (c);

  NS_LOG_INFO ("Create channels");
  PointToPointHelper p2p;

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer devn0n2 = p2p.Install (n0n2);

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("3ms"));
  NetDeviceContainer devn1n2 = p2p.Install (n1n2);

  p2p.SetQueue("ns3::RedQueue"); // yeah, only backbone link have special queue
  p2p.SetDeviceAttribute ("DataRate", StringValue (redLinkDataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (redLinkDelay));
  NetDeviceContainer devn2n3 = p2p.Install (n2n3);

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("4ms"));
  NetDeviceContainer devn3n4 = p2p.Install (n3n4);

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer devn3n5 = p2p.Install (n3n5);

  NS_LOG_INFO ("Assign IP Addresses");

  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = ipv4.Assign (devn0n2);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (devn1n2);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = ipv4.Assign (devn2n3);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i4 = ipv4.Assign (devn3n4);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i5 = ipv4.Assign (devn3n5);

  ///> set up the routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (redTest == 5) // byte mode
    {
      // like in ns2 test, r2 -> r1, have a queue in packet mode
      Ptr<PointToPointNetDevice> nd = StaticCast<PointToPointNetDevice> (devn2n3.Get (1));
      Ptr<Queue> queue = nd->GetQueue ();

      StaticCast<RedQueue> (queue)->SetMode (RedQueue::PACKETS);
      StaticCast<RedQueue> (queue)->SetTh (5, 15);
      StaticCast<RedQueue> (queue)->SetQueueLimit (25);
    }

  // SINKs
  // #1
  uint16_t port1 = 50001;
  Address sinkLocalAddress1(InetSocketAddress (Ipv4Address::GetAny (), port1));
  PacketSinkHelper sinkHelper1 ("ns3::TcpSocketFactory", sinkLocalAddress1);
  ApplicationContainer sinkApp1 = sinkHelper1.Install (n3n4.Get(1));
  sinkApp1.Start (Seconds (sink_start_time));
  sinkApp1.Stop (Seconds (sink_stop_time));
  // #2
  uint16_t port2 = 50002;
  Address sinkLocalAddress2(InetSocketAddress (Ipv4Address::GetAny (), port2));
  PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", sinkLocalAddress2);
  ApplicationContainer sinkApp2 = sinkHelper2.Install (n3n5.Get(1));
  sinkApp2.Start (Seconds (sink_start_time));
  sinkApp2.Stop (Seconds (sink_stop_time));
  // #3
  uint16_t port3 = 50003;
  Address sinkLocalAddress3(InetSocketAddress (Ipv4Address::GetAny (), port3));
  PacketSinkHelper sinkHelper3 ("ns3::TcpSocketFactory", sinkLocalAddress3);
  ApplicationContainer sinkApp3 = sinkHelper3.Install (n0n2.Get(0));
  sinkApp3.Start (Seconds (sink_start_time));
  sinkApp3.Stop (Seconds (sink_stop_time));
  // #4
  uint16_t port4 = 50004;
  Address sinkLocalAddress4(InetSocketAddress (Ipv4Address::GetAny (), port4));
  PacketSinkHelper sinkHelper4 ("ns3::TcpSocketFactory", sinkLocalAddress4);
  ApplicationContainer sinkApp4 = sinkHelper4.Install (n1n2.Get(0));
  sinkApp4.Start (Seconds (sink_start_time));
  sinkApp4.Stop (Seconds (sink_stop_time));

  // Connection #1
  // Create the OnOff applications to send TCP to the server
  // onoffhelper is a client that send data to TCP destination
  OnOffHelper clientHelper1 ("ns3::TcpSocketFactory", Address ());
  clientHelper1.SetAttribute 
      ("OnTime", RandomVariableValue (ConstantVariable (1)));
  clientHelper1.SetAttribute 
      ("OffTime", RandomVariableValue (ConstantVariable (0)));
  clientHelper1.SetAttribute 
      ("DataRate", DataRateValue (DataRate ("10Mb/s")));
  clientHelper1.SetAttribute 
      ("PacketSize", UintegerValue (1000));

  ApplicationContainer clientApps1;
  AddressValue remoteAddress1
    (InetSocketAddress (i3i4.GetAddress (1), port1));
  clientHelper1.SetAttribute ("Remote", remoteAddress1);
  clientApps1.Add(clientHelper1.Install (n0n2.Get(0)));
  clientApps1.Start (Seconds (client_start_time));
  clientApps1.Stop (Seconds (client_stop_time));


  // Connection #2
  OnOffHelper clientHelper2 ("ns3::TcpSocketFactory", Address ());
  clientHelper2.SetAttribute 
      ("OnTime", RandomVariableValue (ConstantVariable (1)));
  clientHelper2.SetAttribute 
      ("OffTime", RandomVariableValue (ConstantVariable (0)));
  clientHelper2.SetAttribute 
      ("DataRate", DataRateValue (DataRate ("10Mb/s")));
  clientHelper2.SetAttribute 
      ("PacketSize", UintegerValue (1000));

  ApplicationContainer clientApps2;
  AddressValue remoteAddress2
    (InetSocketAddress (i3i5.GetAddress (1), port2));
  clientHelper2.SetAttribute ("Remote", remoteAddress2);
  clientApps2.Add(clientHelper2.Install (n1n2.Get(0)));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (client_stop_time));

  // Connection #3
  OnOffHelper clientHelper3 ("ns3::TcpSocketFactory", Address ());
  clientHelper3.SetAttribute 
      ("OnTime", RandomVariableValue (ConstantVariable (1)));
  clientHelper3.SetAttribute 
      ("OffTime", RandomVariableValue (ConstantVariable (0)));
  clientHelper3.SetAttribute 
      ("DataRate", DataRateValue (DataRate ("10Mb/s")));
  clientHelper3.SetAttribute 
      ("PacketSize", UintegerValue (1000));

  ApplicationContainer clientApps3;
  AddressValue remoteAddress3
    (InetSocketAddress (i0i2.GetAddress (0), port3));
  clientHelper3.SetAttribute ("Remote", remoteAddress3);
  clientApps3.Add(clientHelper3.Install (n3n4.Get(1)));
  clientApps3.Start (Seconds (3.5));
  clientApps3.Stop (Seconds (client_stop_time));


  // Connection #4
  OnOffHelper clientHelper4 ("ns3::TcpSocketFactory", Address ());
  clientHelper4.SetAttribute 
      ("OnTime", RandomVariableValue (ConstantVariable (1)));
  clientHelper4.SetAttribute 
      ("OffTime", RandomVariableValue (ConstantVariable (0)));
  clientHelper4.SetAttribute 
      ("DataRate", DataRateValue (DataRate ("40b/s")));
  clientHelper4.SetAttribute 
      ("PacketSize", UintegerValue (5 * 8)); // telnet

  ApplicationContainer clientApps4;
  AddressValue remoteAddress4
    (InetSocketAddress (i1i2.GetAddress (0), port4));
  clientHelper4.SetAttribute ("Remote", remoteAddress4);
  clientApps4.Add(clientHelper4.Install (n3n5.Get(1)));
  clientApps4.Start (Seconds (1.0));
  clientApps4.Stop (Seconds (client_stop_time));


  if (m_writeResults)
    {
      PointToPointHelper ptp;
      ptp.EnablePcapAll ("red");
    }

  Ptr<FlowMonitor> flowmon;

  if (flowMonitor)
    {
      FlowMonitorHelper flowmonHelper;
      flowmon = flowmonHelper.InstallAll ();
    }

  remove(FILE_PLOT_QUEUE);
  remove(FILE_PLOT_QUEUE_AVG);
  Ptr<PointToPointNetDevice> nd = StaticCast<PointToPointNetDevice> (devn2n3.Get (0));
  Ptr<Queue> queue = nd->GetQueue ();
  Simulator::ScheduleNow (&CheckQueueSize, queue);

  Simulator::Stop (Seconds (sink_stop_time));
  Simulator::Run ();

  if (flowMonitor)
    {
      flowmon->SerializeToXmlFile ("red.flowmon", false, false);
    }

  Simulator::Destroy ();

  return 0;
}
