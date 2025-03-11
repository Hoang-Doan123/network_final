#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <chrono>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdHocWifiHighContentionEvaluation");

int main (int argc, char *argv[])
{
  auto overallStart = std::chrono::high_resolution_clock::now();

  // Create output files for results
  std::ofstream throughputFile("throughput-vs-nodes-high-contention.csv");
  std::ofstream delayFile("delay-vs-nodes-high-contention.csv");
  std::ofstream lossFile("packetloss-vs-nodes-high-contention.csv");

  throughputFile << "Nodes,AvgThroughput(Kbps)" << std::endl;
  delayFile << "Nodes,AvgDelay(ms)" << std::endl;
  lossFile << "Nodes,AvgPacketLossRatio" << std::endl;

  // Loop through different node counts (scenarios)
  for (uint32_t nNodes = 2; nNodes <= 30; nNodes += 2)
  {
    auto scenarioStart = std::chrono::high_resolution_clock::now();

    NS_LOG_INFO ("Evaluating performance with " << nNodes << " nodes");

    NodeContainer wifiNodes;
    wifiNodes.Create (nNodes);
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211g);

    // Disable RTS/CTS by setting the threshold higher than the packet size
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("ErpOfdmRate54Mbps"),
                                  "ControlMode", StringValue ("ErpOfdmRate24Mbps"),
                                  "RtsCtsThreshold", UintegerValue (65535));

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
                                    "Exponent", DoubleValue (3.0),
                                    "ReferenceDistance", DoubleValue (1.0),
                                    "ReferenceLoss", DoubleValue (46.6777));

    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, wifiNodes);

    // Configure mobility by placing nodes in a grid pattern
    int gridSize = ceil(sqrt(nNodes));
    double distance = 5.0;

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (distance),
                                   "DeltaY", DoubleValue (distance),
                                   "GridWidth", UintegerValue (gridSize),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiNodes);

    for (uint32_t i = 0; i < wifiNodes.GetN (); i++)
    {
      Ptr<MobilityModel> mob = wifiNodes.Get(i)->GetObject<MobilityModel>();
      Vector pos = mob->GetPosition();
      std::cout << "Node " << i << " position: x=" << pos.x << ", y=" << pos.y << std::endl;
    }

    InternetStackHelper internet;
    internet.Install (wifiNodes);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    uint16_t port = 5000;

    ApplicationContainer sinkApps;
    ApplicationContainer sourceApps;

    // Set up sinks (receivers) on all nodes
    for (uint32_t i = 0; i < nNodes; i++)
    {
      PacketSinkHelper sink ("ns3::UdpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), port));
      sinkApps.Add (sink.Install (wifiNodes.Get (i)));
    }

    // Set up sources, each node sends to all other nodes to create maximum contention
    for (uint32_t i = 0; i < nNodes; i++)
    {
      for (uint32_t j = 0; j < nNodes; j++)
      {
        if (i == j) continue;
        OnOffHelper source ("ns3::UdpSocketFactory",
                            InetSocketAddress (interfaces.GetAddress (j), port));
        source.SetAttribute ("PacketSize", UintegerValue (1472));
        source.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
        source.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"));
        source.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
        sourceApps.Add (source.Install (wifiNodes.Get (i)));
      }
    }

    sinkApps.Start (Seconds (1.0));
    sourceApps.Start (Seconds (2.0));
    sinkApps.Stop (Seconds (30.0));
    sourceApps.Stop (Seconds (29.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (35.0));
    Simulator::Run ();

    // Performance metrics
    double totalThroughput = 0.0;
    double totalDelay = 0.0;
    double totalLossRatio = 0.0;
    int validFlows = 0;
    int totalFlows = 0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      totalFlows++;
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      if (iter->second.txPackets > 0)
      {
        double lossRatio = (iter->second.txPackets - iter->second.rxPackets) / static_cast<double>(iter->second.txPackets);
        totalLossRatio += lossRatio;
        if (iter->second.rxPackets > 0)
        {
          double throughput = iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024;
          totalThroughput += throughput;

          double delay = iter->second.delaySum.GetSeconds() / iter->second.rxPackets * 1000;
          totalDelay += delay;

          validFlows++;

          // Print flow statistics for some flows to avoid too much output
          if (iter->first % (nNodes) == 0)
          {
            std::cout << "Nodes = " << nNodes << ", Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
            std::cout << "  Throughput: " << throughput << " Kbps\n";
            std::cout << "  Mean Delay: " << delay << " ms\n";
            std::cout << "  Packet Loss Ratio: " << lossRatio << "\n\n";
          }
        }
      }
    }

    // Calculate and write average metrics to output files
    if (validFlows > 0)
    {
      double avgThroughput = totalThroughput / validFlows;
      double avgDelay = totalDelay / validFlows;
      double avgLossRatio = totalLossRatio / totalFlows;

      throughputFile << nNodes << "," << avgThroughput << std::endl;
      delayFile << nNodes << "," << avgDelay << std::endl;
      lossFile << nNodes << "," << avgLossRatio << std::endl;

      std::cout << "=== SUMMARY FOR " << nNodes << " NODES ===\n";
      std::cout << "Total Flows: " << totalFlows << "\n";
      std::cout << "Valid Flows (with rx packets): " << validFlows << "\n";
      std::cout << "Average Throughput: " << avgThroughput << " Kbps\n";
      std::cout << "Average Delay: " << avgDelay << " ms\n";
      std::cout << "Average Packet Loss Ratio: " << avgLossRatio << "\n\n";
    }
    else
    {
      throughputFile << nNodes << ",0" << std::endl;
      delayFile << nNodes << ",0" << std::endl;
      lossFile << nNodes << ",1" << std::endl;

      std::cout << "=== SUMMARY FOR " << nNodes << " NODES ===\n";
      std::cout << "No valid flows detected\n\n";
    }

    // Record the end time for the current scenario and print the real execution time
    auto scenarioEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> scenarioDuration = scenarioEnd - scenarioStart;
    std::cout << "Real execution time for scenario with " << nNodes << " nodes: " << scenarioDuration.count() << " seconds" << std::endl;

    Simulator::Destroy ();
  }

  throughputFile.close();
  delayFile.close();
  lossFile.close();

  // Record the overall real execution time for all scenarios
  auto overallEnd = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> overallDuration = overallEnd - overallStart;
  std::cout << "Total real execution time for all scenarios: "
            << overallDuration.count() << " seconds" << std::endl;

  return 0;
}
