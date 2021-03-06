#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <stdint.h>

#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"


#include "ns3/mp-internet-stack-helper.h"
#include "ns3/mp-tcp-packet-sink.h"
#include "ns3/mp-tcp-l4-protocol.h"
#include "ns3/mp-tcp-socket-base.h"
#include "ns3/mp-tcp-packet-sink-helper.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/point-to-point-net-device.h"


#include "ns3/csma-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FirstMultipathToplogy");

	static const uint32_t totalTxBytes = 10000000;
static const uint32_t sendBufSize  = 14000; //2000000;
static const uint32_t recvBufSize  = 2000; //2000000;
static uint32_t currentTxBytes     = 0;
static const double simDuration    = 1.05;

Ptr<Node> client;
Ptr<Node> server;


// Perform series of 1040 byte writes 
static const uint32_t writeSize = sendBufSize;
uint8_t data[totalTxBytes];
Ptr<MpTcpSocketBase> lSocket    = 0;

void StartFlow (Ptr<MpTcpSocketBase>, Ipv4Address, uint16_t);
void WriteUntilBufferFull (Ptr<Socket>, unsigned int);
void connectionSucceeded(Ptr<Socket>);
void connectionFailed(Ptr<Socket>);

void HandlePeerClose (Ptr<Socket>);
void HandlePeerError (Ptr<Socket>);
void CloseConnection (Ptr<Socket>);

int connect(Address &addr);
void variateDelay(PointToPointHelper P2Plink);

static void
CwndTracer (double oldval, double newval)
{
  NS_LOG_INFO ("Moving cwnd from " << oldval << " to " << newval);
}

int main(int argc, char *argv[])
{
	//LogComponentEnable("MpTcpPacketSink",LOG_LEVEL_ALL);
	LogComponentEnable("MpTcpSocketBase", LOG_LEVEL_ALL);
	//LogComponentEnable("MpTcpHeader",LOG_LEVEL_ALL);
	int sf = 2;
	std::string phyMode ("OfdmRate54Mbps");
	double rss = -80;  // -dBm
	 CongestionCtrl_t cc = Fully_Coupled;
	PacketReorder_t  pr = D_SACK;


	 NodeContainer nodes;
     nodes.Create(2);
     client = nodes.Get(0);
     server = nodes.Get(1);
	
     InternetStackHelper stack;
     stack.SetTcp ("ns3::MpTcpL4Protocol");
     stack.Install (nodes);

	vector<Ipv4InterfaceContainer> ipv4Ints;
	//Installing interfaces and giving addresses
    for(int i=0; i < sf; i++)
			{
				WifiHelper wifi;
				wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
				YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
				wifiPhy.Set ("RxGain", DoubleValue (0) ); 
				wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 
				YansWifiChannelHelper wifiChannel ;
				wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
				wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue(rss));
				wifiPhy.SetChannel (wifiChannel.Create ());

			    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
			    wifi.SetRemoteStationManager ("ns3::ArfWifiManager");
				  // Set it to adhoc mode
				  wifiMac.SetType ("ns3::AdhocWifiMac");
				 
				 //if (i==1){
					   //wifiPhy.Set("ChannelNumber",UintegerValue(6)); 
					////   wifiPhy.Set("Frequency",UintegerValue(5000)); 
				 //}
				  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);
				 Ptr<WifiNetDevice> w = DynamicCast<WifiNetDevice> (devices.Get(0));
				//Ptr<YansWifiPhy> yw = DynamicCast<WifiPhy> (w->GetPhy());

				std::cout<<"Channel: "<<w->GetPhy()->GetChannelNumber()<<endl;
				std::cout<<"Starting Frequency: "<<w->GetPhy()->GetFrequency()<<endl;

				//std::cout<<"Frequency: "<<yw->GetChannelFrequencyMhz()<<endl;
				    // Attribution of the IP addresses
				std::stringstream netAddr;
				netAddr << "10.1." << (i+1) << ".0";
				string str = netAddr.str();

				Ipv4AddressHelper ipv4addr;
				ipv4addr.SetBase(str.c_str(), "255.255.255.0");
				Ipv4InterfaceContainer interface = ipv4addr.Assign(devices);
				ipv4Ints.insert(ipv4Ints.end(), interface);

    }
    // Checking addresses
    for( int k=0;k<2; k++)
    {
		for(int j=1;j<3;j++)
		{
      Ptr<Ipv4> ipv40 = nodes.Get(k)->GetObject<Ipv4> (); // Get Ipv4 instance of the node
		Ipv4Address addr = ipv40->GetAddress (j, 0).GetLocal ();
		std::cout<<"node: "<<k <<"  addr "<< j<<" " <<addr<<endl;
		}
	}
//	std::cout<<nodes.Get(0)->GetDevice(1)->GetChannel()->GetChannelFrequencyMhz() <<endl;
	//std::cout<<nodes.Get(0)->GetDevice(2)->GetChannel()<<endl;
	//Mobility
	 MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); //optional
  positionAlloc->Add (Vector (5.0, 0.0, 0.0)); //optional
  mobility.SetPositionAllocator (positionAlloc); //optional
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel"); // optional

  mobility.Install (nodes); 
  
	 // Install applications: two CBR streams each saturating the channel 
	 
	  uint32_t servPort = 5000;
	  
	
	std::cout<<ipv4Ints[0].GetAddress (1)<<endl;
	std::cout<<client->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
	MpTcpPacketSinkHelper help("ns3::TcpSocketFactory",(InetSocketAddress (ipv4Ints[0].GetAddress (1), servPort)), pr);
	ApplicationContainer sapp=help.Install(server);
	ApplicationContainer Apps;
    Apps.Add(sapp);
    lSocket = new MpTcpSocketBase (client);
   
    lSocket->SetCongestionCtrlAlgo (cc);

    lSocket->SetDataDistribAlgo (Round_Robin);

    lSocket->SetPacketReorderAlgo (pr);

    lSocket->Bind ();

    // Trace changes to the congestion window
    Config::ConnectWithoutContext ("/NodeList/0/$ns3::MpTcpSocketBase/subflows/0/CongestionWindow", MakeCallback (&CwndTracer));

    Simulator::ScheduleNow (&StartFlow, lSocket, ipv4Ints[0].GetAddress (1), servPort);


    //Simulator::Stop (Seconds(simDuration + 1000.0));
Simulator::Stop (Seconds(simDuration));
    Simulator::Run ();
    Simulator::Destroy();
	
}

void StartFlow(Ptr<MpTcpSocketBase> localSocket, Ipv4Address servAddress, uint16_t servPort)
{
  NS_LOG_LOGIC("Starting flow at time " <<  Simulator::Now ().GetSeconds ());
 
  //localSocket->Connect (InetSocketAddress (servAddress, servPort));//connect
  lSocket->SetMaxSubFlowNumber(5);
  lSocket->SetMinSubFlowNumber(3);
  lSocket->SetSourceAddress(Ipv4Address("10.1.1.1"));
  lSocket->allocateSendingBuffer(sendBufSize);
  lSocket->allocateRecvingBuffer(recvBufSize);
  // the following buffer is uesed by the received to hold out of sequence data
  lSocket->SetunOrdBufMaxSize(50);


  int connectionState = lSocket->Connect( servAddress, servPort);
//NS_LOG_LOGIC("mpTopology:: connection request sent");
  // tell the tcp implementation to call WriteUntilBufferFull again
  // if we blocked and new tx buffer space becomes available

  if(connectionState == 0)
  {
      lSocket->SetConnectCallback  (MakeCallback (&connectionSucceeded), MakeCallback (&connectionFailed));
      lSocket->SetDataSentCallback (MakeCallback (&WriteUntilBufferFull));
      lSocket->SetCloseCallbacks   (MakeCallback (&HandlePeerClose), MakeCallback(&HandlePeerError));
      lSocket->GetSubflow(0)->StartTracing ("CongestionWindow");
  }else
  {
      //localSocket->NotifyConnectionFailed();
      NS_LOG_LOGIC("mpTopology:: connection failed");
  }
  //WriteUntilBufferFull (localSocket);
}

void connectionSucceeded (Ptr<Socket> localSocket)
{
    NS_LOG_FUNCTION_NOARGS();
    NS_LOG_INFO("mpTopology: Connection requeste succeed");
    Simulator::Schedule (Seconds (1.0), &WriteUntilBufferFull, lSocket, 0);
    Simulator::Schedule (Seconds (simDuration), &CloseConnection, lSocket);
    //Ptr<MpTcpSocketImpl> lSock = localSocket;
    // advertise local addresses
    //lSocket->InitiateSubflows();
    //WriteUntilBufferFull(lSocket, 0);
}

void connectionFailed (Ptr<Socket> localSocket)
{
    NS_LOG_FUNCTION_NOARGS();
    NS_LOG_INFO("mpTopology: Connection requeste failure");
    lSocket->Close();
}

void HandlePeerClose (Ptr<Socket> localSocket)
{
    NS_LOG_FUNCTION_NOARGS();
    NS_LOG_INFO("mpTopology: Connection closed by peer");
    lSocket->Close();
}

void HandlePeerError (Ptr<Socket> localSocket)
{
    NS_LOG_FUNCTION_NOARGS();
    NS_LOG_INFO("mpTopology: Connection closed by peer error");
    lSocket->Close();
}

void CloseConnection (Ptr<Socket> localSocket)
{
    lSocket->Close();
    NS_LOG_LOGIC("mpTopology:: currentTxBytes = " << currentTxBytes);
    NS_LOG_LOGIC("mpTopology:: totalTxBytes   = " << totalTxBytes);
    NS_LOG_LOGIC("mpTopology:: connection to remote host has been closed");
}

void variateDelay (Ptr<Node> node)
{
    //NS_LOG_INFO ("variateDelay");

    Ptr<Ipv4L3Protocol> ipv4 = node->GetObject<Ipv4L3Protocol> ();
    TimeValue delay;
    for(uint32_t i = 0; i < ipv4->GetNInterfaces(); i++)
    {
        //Ptr<NetDevice> device = m_node->GetDevice(i);
        Ptr<Ipv4Interface> interface = ipv4->GetInterface(i);
        Ipv4InterfaceAddress interfaceAddr = interface->GetAddress (0);
        // do not consider loopback addresses
        if(interfaceAddr.GetLocal() == Ipv4Address::GetLoopback())
        {
            // loopback interface has identifier equal to zero
            continue;
        }

        Ptr<NetDevice> netDev =  interface->GetDevice();
        Ptr<Channel> P2Plink  =  netDev->GetChannel();
        P2Plink->GetAttribute(string("Delay"), delay);
        double oldDelay = delay.Get().GetSeconds();
        //NS_LOG_INFO ("variateDelay -> old delay == " << oldDelay);
        std::stringstream strDelay;
        double newDelay = (rand() % 100) * 0.001;
        double err = newDelay - oldDelay;
        strDelay << (0.95 * oldDelay + 0.05 * err) << "s";
        P2Plink->SetAttribute(string("Delay"), StringValue(strDelay.str()));
        P2Plink->GetAttribute(string("Delay"), delay);
        //NS_LOG_INFO ("variateDelay -> new delay == " << delay.Get().GetSeconds());
    }
}

void WriteUntilBufferFull (Ptr<Socket> localSocket, unsigned int txSpace)
{
    NS_LOG_FUNCTION_NOARGS();

    //uint32_t txSpace = localSocket->GetTxAvailable ();
  while (currentTxBytes < totalTxBytes && lSocket->GetTxAvailable () > 0)
  {
      uint32_t left    = totalTxBytes - currentTxBytes;
      uint32_t toWrite = std::min(writeSize, lSocket->GetTxAvailable ());
      toWrite = std::min( toWrite , left );
//NS_LOG_LOGIC("mpTopology:: data already sent ("<< currentTxBytes <<") data buffered ("<< toWrite << ") to be sent subsequentlly");
      int amountBuffered = lSocket->FillBuffer (&data[currentTxBytes], toWrite);
      currentTxBytes += amountBuffered;

    //  variateDelay(client);
/*
      variateDelay(sndP2Plink);

      variateDelay(trdP2Plink);
*/

      lSocket->SendBufferedData();
      // std::cout<<"sending "<<toWrite<< " bytes"<<endl;
  }


}

