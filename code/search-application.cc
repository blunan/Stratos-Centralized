#include "search-application.h"

#include "utilities.h"
#include "definitions.h"
#include "type-header.h"

NS_LOG_COMPONENT_DEFINE("SearchApplication");

NS_OBJECT_ENSURE_REGISTERED(SearchApplication);

TypeId SearchApplication::GetTypeId() {
	NS_LOG_FUNCTION_NOARGS();
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
	NS_LOG_FUNCTION(this);
}

SearchApplication::~SearchApplication() {
	NS_LOG_FUNCTION(this);
}

void SearchApplication::DoInitialize() {
	NS_LOG_FUNCTION(this);
	response = false;
	serviceManager = DynamicCast<ServiceApplication>(GetNode()->GetApplication(3));
	resultsManager = DynamicCast<ResultsApplication>(GetNode()->GetApplication(4));
	ontologyManager = DynamicCast<OntologyApplication>(GetNode()->GetApplication(0));
	positionManager = DynamicCast<PositionApplication>(GetNode()->GetApplication(1));
	scheduleManager = DynamicCast<ScheduleApplication>(GetNode()->GetApplication(5));
	socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
	socket->SetAllowBroadcast(false);
	localAddress = GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
	InetSocketAddress local = InetSocketAddress(localAddress, SEARCH_PORT);
	socket->Bind(local);
	Application::DoInitialize();
}

void SearchApplication::DoDispose() {
	NS_LOG_FUNCTION(this);
	if(socket != NULL) {
		socket->Close();
	}
	Application::DoDispose();
}

void SearchApplication::StartApplication() {
	NS_LOG_FUNCTION(this);
	socket->SetRecvCallback(MakeCallback(&SearchApplication::ReceiveMessage, this));
	Simulator::Schedule(Seconds(Utilities::Random(0, HELLO_TIME)), &SearchApplication::CreateAndSendNotification, this);
}

void SearchApplication::StopApplication() {
	NS_LOG_FUNCTION(this);
	if(socket != NULL) {
		socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
	}
}

SearchResponseHeader SearchApplication::SelectBestResponse(std::list<SearchResponseHeader> responses) {
	NS_LOG_FUNCTION(&responses);
	SearchResponseHeader response;
	SearchResponseHeader bestResponse = responses.front();
	responses.pop_front();
	std::list<SearchResponseHeader>::iterator i;
	NS_LOG_DEBUG("Best response is: " << bestResponse);
	for(i = responses.begin(); i != responses.end(); i++) {
		response = *i;
		if(response.GetOfferedService().semanticDistance < bestResponse.GetOfferedService().semanticDistance) {
			bestResponse = response;
			NS_LOG_DEBUG("New best response selected by semantic distance: " << bestResponse);
		} else if(response.GetOfferedService().semanticDistance == bestResponse.GetOfferedService().semanticDistance) {
			if(response.GetResponseAddress() < bestResponse.GetResponseAddress()) {
				bestResponse = response;
				NS_LOG_DEBUG("New best response selected by address: " << bestResponse);
			}
		}
	}
	return bestResponse;
}

void SearchApplication::CreateAndSendRequest() {
	NS_LOG_FUNCTION(this);
	SearchRequestHeader request = CreateRequest();
	SendRequest(request);
	NS_LOG_DEBUG(localAddress << " -> Initialize results values for request: " << request);
	resultsManager->Activate();
	resultsManager->SetRequestTime(request.GetRequestTimestamp());
	resultsManager->SetRequestService(request.GetRequestedService());
	resultsManager->SetRequestPosition(request.GetRequestPosition());
	resultsManager->SetRequestDistance(request.GetMaxDistanceAllowed());
}

void SearchApplication::ReceiveMessage(Ptr<Socket> socket) {
	NS_LOG_FUNCTION(this << socket);
	Ptr<Packet> packet = socket->Recv();
	TypeHeader typeHeader;
	packet->RemoveHeader(typeHeader);
	if(!typeHeader.IsValid()) {
		NS_LOG_DEBUG(localAddress << " -> Received search message is invalid");
		return;
	}
	NS_LOG_DEBUG(localAddress << " -> Processing search message");
	switch(typeHeader.GetType()) {
		case STRATOS_SEARCH_ERROR:
			ReceiveError(packet);
			break;
		case STRATOS_SEARCH_RESPONSE:
			ReceiveResponse(packet);
			break;
		default:
			NS_LOG_WARN(localAddress << " -> Serach message is unknown!");
			break;
	}
}

void SearchApplication::SendUnicastMessage(Ptr<Packet> packet, uint destinationAddress) {
	NS_LOG_FUNCTION(this << packet << destinationAddress);
	InetSocketAddress remote = InetSocketAddress(Ipv4Address(destinationAddress), SEARCH_PORT);
	socket->Connect(remote);
	socket->Send(packet);
}

SearchRequestHeader SearchApplication::CreateRequest() {
	NS_LOG_FUNCTION(this);
	SearchRequestHeader request;
	request.SetRequestAddress(localAddress);
	request.SetRequestTimestamp(Utilities::GetCurrentRawDateTime());
	request.SetRequestPosition(positionManager->GetCurrentPosition());
	request.SetRequestedService(OntologyApplication::GetRandomService());
	request.SetMaxDistanceAllowed(Utilities::Random(MIN_REQUEST_DISTANCE, MAX_REQUEST_DISTANCE));
	NS_LOG_DEBUG(localAddress << " -> Request created: " << request);
	return request;
}

void SearchApplication::SendRequest(SearchRequestHeader requestHeader) {
	NS_LOG_FUNCTION(this << requestHeader);
	Ptr<Packet> packet = Create<Packet>();
	packet->AddHeader(requestHeader);
	TypeHeader typeHeader(STRATOS_SEARCH_REQUEST);
	packet->AddHeader(typeHeader);
	NS_LOG_DEBUG(localAddress << " -> Schedule request to send");
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &SearchApplication::SendUnicastMessage, this, packet, centralServerAddress);
	NS_LOG_DEBUG(localAddress << " -> Schedule request to retry");
	timers[GetRequestKey(requestHeader)] = Simulator::Schedule(Seconds(MAX_RESPONSE_WAIT_TIME + Utilities::GetJitter()), &SearchApplication::RetryRequest, this, packet, 1, GetRequestKey(requestHeader));
}

std::pair<uint, double> SearchApplication::GetRequestKey(SearchRequestHeader request) {
	NS_LOG_FUNCTION(this << request);
	uint address = request.GetRequestAddress().Get();
	double timestamp = request.GetRequestTimestamp();
	NS_LOG_DEBUG(localAddress << " -> Key from request is [" << address << ", " << timestamp << "] " << request);
	return std::make_pair(address, timestamp);
}

void SearchApplication::ReceiveError(Ptr<Packet> packet) {
	NS_LOG_FUNCTION(this << packet);
	SearchErrorHeader errorHeader;
	packet->RemoveHeader(errorHeader);
	Simulator::Cancel(timers[GetRequestKey(errorHeader)]);
	NS_LOG_DEBUG(localAddress << " -> There is no response for request: " << errorHeader);
}

std::pair<uint, double> SearchApplication::GetRequestKey(SearchErrorHeader error) {
	NS_LOG_FUNCTION(this << error);
	uint address = error.GetRequestAddress().Get();
	double timestamp = error.GetRequestTimestamp();
	NS_LOG_DEBUG(localAddress << " -> Key from error is [" << address << ", " << timestamp << "] " << error);
	return std::make_pair(address, timestamp);
}

void SearchApplication::RetryRequest(Ptr<Packet> packet, int nTry, std::pair<uint, double> key)  {
	NS_LOG_FUNCTION(this << packet << nTry << &key);
	if (nTry <= MAX_TRIES) {
		NS_LOG_DEBUG(localAddress << " -> Retrying request (" << nTry << ")");
		Simulator::Schedule(Seconds(Utilities::GetJitter()), &SearchApplication::SendUnicastMessage, this, packet, centralServerAddress);
		NS_LOG_DEBUG(localAddress << " -> Schedule next retry");
		timers[key] = Simulator::Schedule(Seconds(MAX_RESPONSE_WAIT_TIME + Utilities::GetJitter()), &SearchApplication::RetryRequest, this, packet, ++nTry, key);
	}
}

void SearchApplication::ReceiveResponse(Ptr<Packet> packet) {
	NS_LOG_FUNCTION(this << packet);
	SearchScheduleHeader scheduleHeader;
	packet->RemoveHeader(scheduleHeader);
	NS_LOG_DEBUG(localAddress << " -> Received response: " << scheduleHeader);
	Simulator::Cancel(timers[GetRequestKey(scheduleHeader)]);
	if(!response) {
		response = true;
		NS_LOG_DEBUG(localAddress << " -> Starting service for request");
		std::list<SearchResponseHeader> responses = scheduleHeader.GetSchedule();
		scheduleManager->CreateAndExecuteSchedule(responses);
	}
}

std::pair<uint, double> SearchApplication::GetRequestKey(SearchScheduleHeader response) {
	NS_LOG_FUNCTION(this << response);
	uint address = response.GetRequestAddress().Get();
	double timestamp = response.GetRequestTimestamp();
	NS_LOG_DEBUG(localAddress << " -> Key from response is [" << address << ", " << timestamp << "] " << response);
	return std::make_pair(address, timestamp);
}

void SearchApplication::CreateAndSendNotification() {
	NS_LOG_FUNCTION(this);
	SendNotification(CreateNotification());
}

SearchNotificationHeader SearchApplication::CreateNotification() {
	//NS_LOG_FUNCTION(this);
	SearchNotificationHeader notification;
	notification.SetNodeAddress(localAddress);
	notification.SetCurrentPosition(positionManager->GetCurrentPosition());
	notification.SetOfferedServices(ontologyManager->GetOfferedServices());
	//NS_LOG_DEBUG(localAddress << " -> Notification created: " << notification);
	return notification;
}

void SearchApplication::SendNotification(SearchNotificationHeader notificationHeader) {
	//NS_LOG_FUNCTION(this << notificationHeader);
	Ptr<Packet> packet = Create<Packet>();
	packet->AddHeader(notificationHeader);
	TypeHeader typeHeader(STRATOS_SEARCH_NOTIFICATION);
	packet->AddHeader(typeHeader);
	//NS_LOG_DEBUG(localAddress << " -> Schedule notification to send");
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &SearchApplication::SendUnicastMessage, this, packet, centralServerAddress);
	//NS_LOG_DEBUG(localAddress << " -> Schedule next notification");
	Simulator::Schedule(Seconds(HELLO_TIME + Utilities::Random(0, HELLO_TIME)), &SearchApplication::SendNotification, this, notificationHeader);
}

SearchHelper::SearchHelper() {
	NS_LOG_FUNCTION(this);
	objectFactory.SetTypeId("SearchApplication");
}