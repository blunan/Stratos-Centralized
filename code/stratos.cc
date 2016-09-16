#include "stratos.h"

#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

#include "utilities.h"
#include "definitions.h"
#include "search-application.h"
#include "central-application.h"
#include "service-application.h"
#include "results-application.h"
#include "ontology-application.h"
#include "position-application.h"
#include "schedule-application.h"

NS_LOG_COMPONENT_DEFINE("Stratos");

Stratos::Stratos(int argc, char *argv[]) {
	NS_LOG_FUNCTION(this);
	MAX_SCHEDULE_SIZE = 3; // 1, 2, 3*, 4, 5
	NUMBER_OF_MOBILE_NODES = 50; //0, 25, 50*, 100
	NUMBER_OF_REQUESTER_NODES = 4; //1, 2, 4*, 8, 16, 24, 32
	NUMBER_OF_PACKETS_TO_SEND = 20; //10, 20*, 40, 60
	NUMBER_OF_SERVICES_OFFERED = 2; //1, 2*, 4, 8

	NS_LOG_INFO("Parsing argument values if any");
	CommandLine cmd;
	cmd.AddValue("nSchedule", "Max number of nodes in a schedule.", MAX_SCHEDULE_SIZE);
	cmd.AddValue("nMobile", "Number of mobile nodes.", NUMBER_OF_MOBILE_NODES);
	cmd.AddValue("nRequesters", "Number of requester nodes.", NUMBER_OF_REQUESTER_NODES);
	cmd.AddValue("nPackets", "Number of service packets to send.", NUMBER_OF_PACKETS_TO_SEND);
	cmd.AddValue("nServices", "Number of services offered by a node.", NUMBER_OF_SERVICES_OFFERED);
	cmd.Parse(argc, argv);
	NS_LOG_INFO("Max schedule size = " << MAX_SCHEDULE_SIZE);
	NS_LOG_INFO("Number of mobile nodes = " << NUMBER_OF_MOBILE_NODES);
	NS_LOG_INFO("Number of requester nodes = " << NUMBER_OF_REQUESTER_NODES);
	NS_LOG_INFO("Number of service packets to send = " << NUMBER_OF_PACKETS_TO_SEND);
	NS_LOG_INFO("Number of services offered by a node = " << NUMBER_OF_SERVICES_OFFERED);

	SeedManager::SetSeed(time(NULL));
	NS_LOG_INFO("Random seed seted to current time");
}

void Stratos::Run() {
	NS_LOG_FUNCTION(this);
	std::map<int, int> nodos;
	for(; nodos.size() < (uint) NUMBER_OF_REQUESTER_NODES;) {
		int nodo = Utilities::Random(1, TOTAL_NUMBER_OF_NODES - 1);
		nodos[nodo] = nodo;
	}
	Ptr<SearchApplication> searchApp;
	Ptr<ResultsApplication> resultsApp;
	Ptr<ResultsApplication> requesterResultsApp;
	for(std::map<int, int>::iterator i = nodos.begin(); i != nodos.end(); i++) {
		double requestTime = Utilities::Random(2, MAX_REQUEST_TIME);
		searchApp = DynamicCast<SearchApplication>(wifiNodes.Get(i->first)->GetApplication(2));
		requesterResultsApp = DynamicCast<ResultsApplication>(wifiNodes.Get(i->first)->GetApplication(4));
		Simulator::Schedule(Seconds(requestTime), &SearchApplication::CreateAndSendRequest, searchApp);
		for(int j = 1; j < TOTAL_NUMBER_OF_NODES; j++) {
			resultsApp = DynamicCast<ResultsApplication>(wifiNodes.Get(j)->GetApplication(4));
			Simulator::Schedule(Seconds(requestTime), &ResultsApplication::EvaluateNode, resultsApp, requesterResultsApp);
		}
	}
	Ptr<FlowMonitor> flowMonitor;
	FlowMonitorHelper flowHelper;
	flowMonitor = flowHelper.InstallAll();
	Simulator::Stop(Seconds(TOTAL_SIMULATION_TIME));
	Simulator::Run();
	double bytes = 0;
	std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
	for(std::map<FlowId, FlowMonitor::FlowStats>::iterator i = stats.begin(); i != stats.end(); i++) {
		bytes += i->second.txBytes;
	}
	std::cout << bytes << std::endl;
	Simulator::Destroy();
}

void Stratos::CreateNodes() {
	NS_LOG_FUNCTION(this);
	CreateMobileNodes();
	CreateStaticNodes();
	wifiNodes.Add(mobileNodes);
	wifiNodes.Add(staticNodes);
}

void Stratos::CreateDevices() {
	NS_LOG_FUNCTION(this);
	YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
	wifiPhy.SetChannel(wifiChannel.Create());
	NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
	wifiMac.SetType("ns3::AdhocWifiMac");
	WifiHelper wifi = WifiHelper::Default();
	wifiDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes);
}

void Stratos::InstallInternetStack() {
	NS_LOG_FUNCTION(this);
	InternetStackHelper internetStack;
	AodvHelper aodv;
	internetStack.SetRoutingHelper(aodv);
	internetStack.Install(wifiNodes);
	Ipv4AddressHelper addressHelper;
	addressHelper.SetBase("10.0.0.0", "255.0.0.0");
	addressHelper.Assign(wifiDevices);
}

void Stratos::InstallApplications() {
	NS_LOG_FUNCTION(this);
	Ptr<Node> centralNode = wifiNodes.Get(0);
	NodeContainer nodes;
	for(int i = 1; i < TOTAL_NUMBER_OF_NODES; i++) {
		nodes.Add(wifiNodes.Get(i));
	}
	ApplicationContainer applications;
	OntologyHelper ontology;
	ontology.SetAttribute("nServices", IntegerValue(NUMBER_OF_SERVICES_OFFERED));
	applications.Add(ontology.Install(wifiNodes));
	PositionHelper position;
	applications.Add(position.Install(wifiNodes));
	SearchHelper search;
	search.SetAttribute("centralServerAddress", UintegerValue(centralNode->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal().Get()));
	applications.Add(search.Install(nodes));
	ServiceHelper service;
	service.SetAttribute("nPackets", IntegerValue(NUMBER_OF_PACKETS_TO_SEND));
	applications.Add(service.Install(nodes));
	ResultsHelper results;
	applications.Add(results.Install(nodes));
	CentralHelper central;
	central.SetAttribute("nSchedule", IntegerValue(MAX_SCHEDULE_SIZE));
	applications.Add(central.Install(centralNode));
	ScheduleHelper schedule;
	schedule.SetAttribute("nSchedule", IntegerValue(MAX_SCHEDULE_SIZE));
	applications.Add(schedule.Install(nodes));
	applications.Start(Seconds(1));
	applications.Stop(Seconds(TOTAL_SIMULATION_TIME - 1));
}

void Stratos::CreateMobileNodes() {
	NS_LOG_FUNCTION(this);
	int nMobileNodes;
	if(NUMBER_OF_MOBILE_NODES == TOTAL_NUMBER_OF_NODES) {
		nMobileNodes = TOTAL_NUMBER_OF_NODES - 1;
	} else {
		nMobileNodes = NUMBER_OF_MOBILE_NODES;
	}
	NS_LOG_DEBUG("Creating " << nMobileNodes << " mobile nodes");
	mobileNodes.Create(nMobileNodes);
	for(int i = 0; i < nMobileNodes; ++i) {
		std::ostringstream name;
		name << "Mobile Node - " << i;
		Names::Add(name.str(), mobileNodes.Get(i));
	}
	MobilityHelper mobility;
	Ptr<PositionAllocator> positionAllocator = GetPositionAllocator();
	mobility.SetPositionAllocator(positionAllocator);
	mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
							"PositionAllocator", PointerValue(positionAllocator),
							"Speed", StringValue("ns3::UniformRandomVariable[Min=1|Max=4]"), //[1m/s, 4m/s]
							"Pause", StringValue("ns3::ConstantRandomVariable[Constant=40]")); //40s
	mobility.Install(mobileNodes);
}

void Stratos::CreateStaticNodes() {
	NS_LOG_FUNCTION(this);
	int nStaticNodes;
	if(NUMBER_OF_MOBILE_NODES == TOTAL_NUMBER_OF_NODES) {
		nStaticNodes = 1;
	} else {
		nStaticNodes = TOTAL_NUMBER_OF_NODES - NUMBER_OF_MOBILE_NODES;
	}
	NS_LOG_DEBUG("Creating " << nStaticNodes << " static nodes");
	staticNodes.Create(nStaticNodes);
	for(int i = 0; i < nStaticNodes; ++i) {
		std::ostringstream name;
		name << "Static Node - " << i;
		Names::Add(name.str(), staticNodes.Get(i));
	}
	MobilityHelper mobility;
	Ptr<PositionAllocator> positionAllocator = GetPositionAllocator();
	mobility.SetPositionAllocator(positionAllocator);
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobility.Install(staticNodes);
}

Ptr<PositionAllocator> Stratos::GetPositionAllocator() {
	NS_LOG_FUNCTION(this);
	Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable>();
	random->SetAttribute("Min", DoubleValue(0));
	random->SetAttribute("Max", DoubleValue(MAX_DISTANCE));
	Ptr<RandomRectanglePositionAllocator> positionAllocator = CreateObject<RandomRectanglePositionAllocator>();
	positionAllocator->SetX(random);
	positionAllocator->SetY(random);
	return positionAllocator;
}