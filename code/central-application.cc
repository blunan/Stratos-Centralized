#include "central-application.h"

#include <limits>

#include "utilities.h"
#include "type-header.h"

NS_LOG_COMPONENT_DEFINE("CentralApplication");

NS_OBJECT_ENSURE_REGISTERED(CentralApplication);

TypeId CentralApplication::GetTypeId() {
	NS_LOG_FUNCTION_NOARGS();
	static TypeId typeId = TypeId("CentralApplication")
		.SetParent<Application>()
		.AddConstructor<CentralApplication>();
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
	ontologyManager = DynamicCast<OntologyApplication>(GetNode()->GetApplication(0));
	positionManager = DynamicCast<PositionApplication>(GetNode()->GetApplication(1));
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
		NS_LOG_DEBUG(typeHeader);
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
		CreateAndSendError(requestHeader);
		return;
	}
	uint bestNode = GetBestNode(nodes, requestHeader);
	CreateAndSendResponse(bestNode, requestHeader);
}

std::list<uint> CentralApplication::FilterNodesByDistance(SearchRequestHeader request) {
	std::list<uint> nodes;
	POSITION nodePosition;
	POSITION requestPosition = request.GetRequestPosition();
	double requestDistance = request.GetMaxDistanceAllowed();
	pthread_mutex_lock(&mutex);
	for(std::map<uint, POSITION>::iterator i = positions.begin(); i != positions.end(); i++) {
		nodePosition = i->second;
		double distance = positionManager->CalculateDistanceFromTo(nodePosition, requestPosition);
		if(distance <= requestDistance) {
			nodes.push_back(i->first);
		}
	}
	pthread_mutex_unlock(&mutex);
	return nodes;
}

uint CentralApplication::GetBestNode(std::list<uint> nodes, SearchRequestHeader request) {
	uint bestNode = 0;
	OFFERED_SERVICE bestOfferedService;
	std::list<std::string> offeredServices;
	int minSemanticDistance = std::numeric_limits<int>::max();
	std::string requestedService = request.GetRequestedService();
	pthread_mutex_lock(&mutex);
	for(std::list<uint>::iterator i = nodes.begin(); i != nodes.end(); i++) {
		offeredServices = services[*i];
		bestOfferedService = ontologyManager->GetBestOfferedService(requestedService, offeredServices);
		if(bestOfferedService.semanticDistance < minSemanticDistance) {
			bestNode = *i;
			minSemanticDistance = bestOfferedService.semanticDistance;
		}
	}
	pthread_mutex_unlock(&mutex);
	return bestNode;
}

void CentralApplication::ReceiveNotification(Ptr<Packet> packet) {
	SearchNotificationHeader notificationHeader;
	packet->RemoveHeader(notificationHeader);
	uint node = notificationHeader.GetNodeAddress().Get();
	pthread_mutex_lock(&mutex);
	services[node] = notificationHeader.GetOfferedServices();
	positions[node] = notificationHeader.GetCurrentPosition();
	pthread_mutex_unlock(&mutex);
	//std::cout << notificationHeader << std::endl;
}

void CentralApplication::SendError(SearchErrorHeader errorHeader) {
	Ptr<Packet> packet = Create<Packet>();
	packet->AddHeader(errorHeader);
	TypeHeader typeHeader(STRATOS_SEARCH_ERROR);
	packet->AddHeader(typeHeader);
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &CentralApplication::SendUnicastMessage, this, packet, errorHeader.GetRequestAddress().Get());
}

void CentralApplication::CreateAndSendError(SearchRequestHeader request) {
	SendError(CreateError(request));
}

SearchErrorHeader CentralApplication::CreateError(SearchRequestHeader request) {
	SearchErrorHeader error;
	error.SetRequestAddress(request.GetRequestAddress());
	error.SetRequestTimestamp(request.GetRequestTimestamp());
	return error;
}

void CentralApplication::SendResponse(SearchResponseHeader responseHeader) {
	Ptr<Packet> packet = Create<Packet>();
	packet->AddHeader(responseHeader);
	TypeHeader typeHeader(STRATOS_SEARCH_RESPONSE);
	packet->AddHeader(typeHeader);
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &CentralApplication::SendUnicastMessage, this, packet, responseHeader.GetRequestAddress().Get());
}

void CentralApplication::CreateAndSendResponse(uint node, SearchRequestHeader request) {
	SendResponse(CreateResponse(node, request));
}

SearchResponseHeader CentralApplication::CreateResponse(uint node, SearchRequestHeader request) {
	POSITION requester = request.GetRequestPosition();
	pthread_mutex_lock(&mutex);
	POSITION me = positions[node];
	pthread_mutex_unlock(&mutex);
	double distance = positionManager->CalculateDistanceFromTo(requester, me);
	SearchResponseHeader response;
	response.SetDistance(distance);
	response.SetResponseAddress(Ipv4Address(node));
	//response.SetHopDistance(request.GetCurrentHops());
	response.SetRequestAddress(request.GetRequestAddress());
	response.SetRequestTimestamp(request.GetRequestTimestamp());
	pthread_mutex_lock(&mutex);
	response.SetOfferedService(ontologyManager->GetBestOfferedService(request.GetRequestedService(), services[node]));
	pthread_mutex_unlock(&mutex);
	return response;
}

CentralHelper::CentralHelper() {
	objectFactory.SetTypeId("CentralApplication");
}