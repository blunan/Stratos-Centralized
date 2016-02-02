#include "search-application.h"

#include "utilities.h"
#include "definitions.h"
#include "type-header.h"

NS_OBJECT_ENSURE_REGISTERED(SearchApplication);

TypeId SearchApplication::GetTypeId() {
	static TypeId typeId = TypeId("SearchApplication")
		.SetParent<Application>()
		.AddConstructor<SearchApplication>()
		.AddAttribute("centralServerAddress",
						"Address of the central server.",
						UintegerValue(Ipv4Address("255.255.255.255").Get()),
						MakeUintegerAccessor(&SearchApplication::centralServerAddress),
						MakeUintegerChecker<uint>());
	return typeId;
}

SearchApplication::SearchApplication() {
}

SearchApplication::~SearchApplication() {
}

void SearchApplication::DoInitialize() {
	response = false;
	serviceManager = DynamicCast<ServiceApplication>(GetNode()->GetApplication(3));
	resultsManager = DynamicCast<ResultsApplication>(GetNode()->GetApplication(4));
	ontologyManager = DynamicCast<OntologyApplication>(GetNode()->GetApplication(0));
	positionManager = DynamicCast<PositionApplication>(GetNode()->GetApplication(1));
	localAddress = GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
	socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
	socket->SetAllowBroadcast(false);
	InetSocketAddress local = InetSocketAddress(localAddress, SEARCH_PORT);
	socket->Bind(local);
	Application::DoInitialize();
}

void SearchApplication::DoDispose() {
	if(socket != NULL) {
		socket->Close();
	}
	Application::DoDispose();
}

void SearchApplication::StartApplication() {
	socket->SetRecvCallback(MakeCallback(&SearchApplication::ReceiveMessage, this));
	Simulator::Schedule(Seconds(Utilities::Random(0, HELLO_TIME)), &SearchApplication::CreateAndSendNotification, this);
}

void SearchApplication::StopApplication() {
	if(socket != NULL) {
		socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
	}
}

void SearchApplication::CreateAndSendRequest() {
	SearchRequestHeader request = CreateRequest();
	SendRequest(request);
	resultsManager->Activate();
	resultsManager->SetRequestTime(request.GetRequestTimestamp());
	resultsManager->SetRequestPosition(request.GetRequestPosition());
	resultsManager->SetRequestService(request.GetRequestedService());
	resultsManager->SetRequestDistance(request.GetMaxDistanceAllowed());
}

void SearchApplication::ReceiveMessage(Ptr<Socket> socket) {
	Ptr<Packet> packet = socket->Recv();
	TypeHeader typeHeader;
	packet->RemoveHeader(typeHeader);
	if(!typeHeader.IsValid()) {
		return;
	}
	switch(typeHeader.GetType()) {
		case STRATOS_SEARCH_ERROR:
			ReceiveError(packet);
			break;
		case STRATOS_SEARCH_RESPONSE:
			ReceiveResponse(packet);
			break;
		default:
			break;
	}
}

void SearchApplication::SendUnicastMessage(Ptr<Packet> packet, uint destinationAddress) {
	InetSocketAddress remote = InetSocketAddress(Ipv4Address(destinationAddress), SEARCH_PORT);
	socket->Connect(remote);
	socket->Send(packet);
}

SearchRequestHeader SearchApplication::CreateRequest() {
	SearchRequestHeader request;
	request.SetRequestAddress(localAddress);
	request.SetRequestTimestamp(Utilities::GetCurrentRawDateTime());
	request.SetRequestPosition(positionManager->GetCurrentPosition());
	request.SetRequestedService(OntologyApplication::GetRandomService());
	request.SetMaxDistanceAllowed(Utilities::Random(MIN_REQUEST_DISTANCE, MAX_REQUEST_DISTANCE));
	return request;
}

void SearchApplication::SendRequest(SearchRequestHeader requestHeader) {
	Ptr<Packet> packet = Create<Packet>();
	packet->AddHeader(requestHeader);
	TypeHeader typeHeader(STRATOS_SEARCH_REQUEST);
	packet->AddHeader(typeHeader);
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &SearchApplication::SendUnicastMessage, this, packet, centralServerAddress);
	timers[GetRequestKey(requestHeader)] = Simulator::Schedule(Seconds(MAX_RESPONSE_WAIT_TIME + Utilities::GetJitter()), &SearchApplication::RetryRequest, this, packet, 1, GetRequestKey(requestHeader));
	//std::cout << "SendRequest :: " << requestHeader << std::endl;
}

std::pair<uint, double> SearchApplication::GetRequestKey(SearchRequestHeader request) {
	uint address = request.GetRequestAddress().Get();
	double timestamp = request.GetRequestTimestamp();
	return std::make_pair(address, timestamp);
}

void SearchApplication::ReceiveError(Ptr<Packet> packet) {
	SearchErrorHeader errorHeader;
	packet->RemoveHeader(errorHeader);
	Simulator::Cancel(timers[GetRequestKey(errorHeader)]);
	//std::cout << Now().GetMilliSeconds() << " -> Search response to " << localAddress << " no responses for request" << std::endl;
}

std::pair<uint, double> SearchApplication::GetRequestKey(SearchErrorHeader error) {
	uint address = error.GetRequestAddress().Get();
	double timestamp = error.GetRequestTimestamp();
	return std::make_pair(address, timestamp);
}

void SearchApplication::RetryRequest(Ptr<Packet> packet, int nTry, std::pair<uint, double> key)  {
	if (nTry <= MAX_TRIES) {
		//std::cout << "SearchApplication::Try " << nTry << std::endl;
		Simulator::Schedule(Seconds(Utilities::GetJitter()), &SearchApplication::SendUnicastMessage, this, packet, centralServerAddress);
		timers[key] = Simulator::Schedule(Seconds(MAX_RESPONSE_WAIT_TIME + Utilities::GetJitter()), &SearchApplication::RetryRequest, this, packet, ++nTry, key);
	}
}

void SearchApplication::ReceiveResponse(Ptr<Packet> packet) {
	SearchResponseHeader responseHeader;
	packet->RemoveHeader(responseHeader);
	Simulator::Cancel(timers[GetRequestKey(responseHeader)]);
	if(!response) {
		response = true;
		//std::cout << "ReceiveResponse :: " << responseHeader << std::endl;
		serviceManager->CreateAndSendRequest(responseHeader.GetResponseAddress(), responseHeader.GetOfferedService().service);
		resultsManager->SetResponseSemanticDistance(responseHeader.GetOfferedService().semanticDistance);
	}
}

std::pair<uint, double> SearchApplication::GetRequestKey(SearchResponseHeader response) {
	uint address = response.GetRequestAddress().Get();
	double timestamp = response.GetRequestTimestamp();
	return std::make_pair(address, timestamp);
}

void SearchApplication::CreateAndSendNotification() {
	SendNotification(CreateNotification());
}

SearchNotificationHeader SearchApplication::CreateNotification() {
	SearchNotificationHeader notification;
	notification.SetNodeAddress(localAddress);
	notification.SetCurrentPosition(positionManager->GetCurrentPosition());
	notification.SetOfferedServices(ontologyManager->GetOfferedServices());
	return notification;
}

void SearchApplication::SendNotification(SearchNotificationHeader notificationHeader) {
	Ptr<Packet> packet = Create<Packet>();
	packet->AddHeader(notificationHeader);
	TypeHeader typeHeader(STRATOS_SEARCH_NOTIFICATION);
	packet->AddHeader(typeHeader);
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &SearchApplication::SendUnicastMessage, this, packet, centralServerAddress);
	Simulator::Schedule(Seconds(HELLO_TIME + Utilities::Random(0, HELLO_TIME)), &SearchApplication::SendNotification, this, notificationHeader);
	//std::cout << notificationHeader << std::endl;
}

SearchHelper::SearchHelper() {
	objectFactory.SetTypeId("SearchApplication");
}