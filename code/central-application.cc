#include "central-application.h"
#include "position-application.h"
#include "ontology-application.h"

#include <limits>

#include "utilities.h"
#include "type-header.h"

NS_LOG_COMPONENT_DEFINE("CentralApplication");

NS_OBJECT_ENSURE_REGISTERED(CentralApplication);

TypeId CentralApplication::GetTypeId() {
	NS_LOG_FUNCTION_NOARGS();
	static TypeId typeId = TypeId("CentralApplication")
		.SetParent<Application>()
		.AddConstructor<CentralApplication>()
		.AddAttribute("nSchedule",
 						"Max number of nodes in a schedule.",
 						IntegerValue(3),
 						MakeIntegerAccessor(&CentralApplication::MAX_SCHEDULE_SIZE),
 						MakeIntegerChecker<int>());
	return typeId;
}

CentralApplication::CentralApplication() {
	NS_LOG_FUNCTION(this);
}

CentralApplication::~CentralApplication() {
	NS_LOG_FUNCTION(this);
}

void CentralApplication::DoInitialize() {
	NS_LOG_FUNCTION(this);
	pthread_mutex_init(&mutex, NULL);
	socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
	socket->SetAllowBroadcast(false);
	InetSocketAddress local = InetSocketAddress(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), SEARCH_PORT);
	socket->Bind(local);
	Application::DoInitialize();
}

void CentralApplication::DoDispose() {
	NS_LOG_FUNCTION(this);
	if(socket != NULL) {
		socket->Close();
	}
	pthread_mutex_destroy(&mutex);
	Application::DoDispose();
}

void CentralApplication::StartApplication() {
	NS_LOG_FUNCTION(this);
	socket->SetRecvCallback(MakeCallback(&CentralApplication::ReceiveMessage, this));
}

void CentralApplication::StopApplication() {
	NS_LOG_FUNCTION(this);
	if(socket != NULL) {
		socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
	}
}

void CentralApplication::ReceiveMessage(Ptr<Socket> socket) {
	NS_LOG_FUNCTION(this << socket);
	Ptr<Packet> packet = socket->Recv();
	TypeHeader typeHeader;
	packet->RemoveHeader(typeHeader);
	if(!typeHeader.IsValid()) {
		NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Received search message is invalid");
		return;
	}
	NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Processing search message");
	switch(typeHeader.GetType()) {
		case STRATOS_SEARCH_NOTIFICATION:
			ReceiveNotification(packet);
			break;
		case STRATOS_SEARCH_REQUEST:
			ReceiveRequest(packet);
			break;
		default:
			NS_LOG_WARN(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Serach message is unknown!");
			break;
	}
}

void CentralApplication::SendUnicastMessage(Ptr<Packet> packet, uint destinationAddress) {
	NS_LOG_FUNCTION(this << packet << destinationAddress);
	InetSocketAddress remote = InetSocketAddress(Ipv4Address(destinationAddress), SEARCH_PORT);
	socket->Connect(remote);
	socket->Send(packet);
}

void CentralApplication::ReceiveRequest(Ptr<Packet> packet) {
	NS_LOG_FUNCTION(this << packet);
	SearchRequestHeader requestHeader;
	packet->RemoveHeader(requestHeader);
	NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Received request: " << requestHeader);
	std::list<uint> nodes = FilterNodesByDistance(requestHeader);
	if(nodes.size() <= 0) {
		NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> There are no nodes in the area of interest, sending error");
		CreateAndSendError(requestHeader);
		return;
	}
	std::list<uint> scheduleNodes = GetScheduleNodes(nodes, requestHeader);
	if(!scheduleNodes.empty()) {
		CreateAndSendResponse(scheduleNodes, requestHeader);
	} else {
		CreateAndSendError(requestHeader);
	}
}

std::list<uint> CentralApplication::FilterNodesByDistance(SearchRequestHeader request) {
	NS_LOG_FUNCTION(this << request);
	std::list<uint> nodes;
	POSITION nodePosition;
	POSITION requestPosition = request.GetRequestPosition();
	double requestDistance = request.GetMaxDistanceAllowed();
	pthread_mutex_lock(&mutex);
	for(std::map<uint, POSITION>::iterator i = positions.begin(); i != positions.end(); i++) {
		if(i->first != request.GetRequestAddress().Get()) {
			nodePosition = i->second;
			double distance = PositionApplication::CalculateDistanceFromTo(nodePosition, requestPosition);
			if(distance <= requestDistance) {
				nodes.push_back(i->first);
			}
		}
	}
	pthread_mutex_unlock(&mutex);
	NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> There are " << nodes.size() << " nodes in the area of interest");
	return nodes;
}

std::list<uint> CentralApplication::GetScheduleNodes(std::list<uint> nodes, SearchRequestHeader request) {
	NS_LOG_FUNCTION(this << &nodes << request);
	std::list<uint> bestNodes;
	OFFERED_SERVICE bestOfferedService;
	std::list<std::string> offeredServices;
	std::string requestedService = request.GetRequestedService();
	while(!nodes.empty() && bestNodes.size() < MAX_SCHEDULE_SIZE) {
		std::list<uint>::iterator bestNode = nodes.begin();
		int minSemanticDistance = std::numeric_limits<int>::max();
		NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Searching best node to provide service " << requestedService);
		for(std::list<uint>::iterator i = nodes.begin(); i != nodes.end(); i++) {
			pthread_mutex_lock(&mutex);
			offeredServices = services[*i];
			pthread_mutex_unlock(&mutex);
			bestOfferedService = OntologyApplication::GetBestOfferedService(requestedService, offeredServices);
			if(bestOfferedService.semanticDistance < minSemanticDistance) {
				bestNode = i;
				minSemanticDistance = bestOfferedService.semanticDistance;
			}
		}
		NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Best node to provide service " << requestedService << " is " << *bestNode << " providing service " << bestOfferedService.service << " with semantic distance " << minSemanticDistance);
		bestNodes.push_back(*bestNode);
		nodes.erase(bestNode);
	}
	return bestNodes;
}

void CentralApplication::ReceiveNotification(Ptr<Packet> packet) {
	NS_LOG_FUNCTION(this << packet);
	SearchNotificationHeader notificationHeader;
	packet->RemoveHeader(notificationHeader);
	NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Received notification: " << notificationHeader);
	uint node = notificationHeader.GetNodeAddress().Get();
	pthread_mutex_lock(&mutex);
	services[node] = notificationHeader.GetOfferedServices();
	positions[node] = notificationHeader.GetCurrentPosition();
	pthread_mutex_unlock(&mutex);
}

void CentralApplication::SendError(SearchErrorHeader errorHeader) {
	NS_LOG_FUNCTION(this << errorHeader);
	Ptr<Packet> packet = Create<Packet>();
	packet->AddHeader(errorHeader);
	TypeHeader typeHeader(STRATOS_SEARCH_ERROR);
	packet->AddHeader(typeHeader);
	NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Schedule error to send");
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &CentralApplication::SendUnicastMessage, this, packet, errorHeader.GetRequestAddress().Get());
}

void CentralApplication::CreateAndSendError(SearchRequestHeader request) {
	NS_LOG_FUNCTION(this << request);
	SendError(CreateError(request));
}

SearchErrorHeader CentralApplication::CreateError(SearchRequestHeader request) {
	NS_LOG_FUNCTION(this << request);
	SearchErrorHeader error;
	error.SetRequestAddress(request.GetRequestAddress());
	error.SetRequestTimestamp(request.GetRequestTimestamp());
	NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Error created: " << error);
	return error;
}

void CentralApplication::SendResponse(SearchScheduleHeader scheduleHeader) {
	NS_LOG_FUNCTION(this << scheduleHeader);
	Ptr<Packet> packet = Create<Packet>();
	packet->AddHeader(scheduleHeader);
	TypeHeader typeHeader(STRATOS_SEARCH_RESPONSE);
	packet->AddHeader(typeHeader);
	NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Schedule response to send");
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &CentralApplication::SendUnicastMessage, this, packet, scheduleHeader.GetRequestAddress().Get());
}

SearchResponseHeader CentralApplication::CreateResponse(uint node, SearchRequestHeader request) {
	NS_LOG_FUNCTION(this << node << request);
	pthread_mutex_lock(&mutex);
	POSITION nodePosition = positions[node];
	std::list<std::string> nodeServices = services[node];
	pthread_mutex_unlock(&mutex);
	POSITION requesterPosition = request.GetRequestPosition();
	SearchResponseHeader response;
	response.SetResponseAddress(Ipv4Address(node));
	response.SetRequestAddress(request.GetRequestAddress());
	response.SetRequestTimestamp(request.GetRequestTimestamp());
	response.SetDistance(PositionApplication::CalculateDistanceFromTo(requesterPosition, nodePosition));
	response.SetOfferedService(OntologyApplication::GetBestOfferedService(request.GetRequestedService(), nodeServices));
	NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Response created: " << response);
	return response;
}

void CentralApplication::CreateAndSendResponse(std::list<uint> scheduleNodes, SearchRequestHeader request) {
	NS_LOG_FUNCTION(this << &scheduleNodes << request);
	SendResponse(CreateResponse(scheduleNodes, request));
}

SearchScheduleHeader CentralApplication::CreateResponse(std::list<uint> scheduleNodes, SearchRequestHeader request) {
	NS_LOG_FUNCTION(this << &scheduleNodes << request);
	SearchScheduleHeader scheduleResponse;
	scheduleResponse.SetRequestAddress(request.GetRequestAddress());
	scheduleResponse.SetRequestTimestamp(request.GetRequestTimestamp());
	std::list<SearchResponseHeader> schedule;
	for(std::list<uint>::iterator i = scheduleNodes.begin(); i != scheduleNodes.end(); i++) {
		schedule.push_back(CreateResponse((*i), request));
	}
	scheduleResponse.SetSchedule(schedule);
	NS_LOG_DEBUG(GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() << " -> Schedule response created: " << scheduleResponse);
	return scheduleResponse;
}

CentralHelper::CentralHelper() {
	NS_LOG_FUNCTION(this);
	objectFactory.SetTypeId("CentralApplication");
}