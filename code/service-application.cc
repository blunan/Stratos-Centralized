#include "service-application.h"

#include "utilities.h"
#include "definitions.h"
#include "type-header.h"

NS_LOG_COMPONENT_DEFINE("ServiceApplication");

NS_OBJECT_ENSURE_REGISTERED(ServiceApplication);

TypeId ServiceApplication::GetTypeId() {
	NS_LOG_FUNCTION_NOARGS();
	static TypeId typeId = TypeId("ServiceApplication")
		.SetParent<Application>()
		.AddConstructor<ServiceApplication>()
		.AddAttribute("nPackets",
						"Number of service packets to send.",
						IntegerValue(10),
						MakeIntegerAccessor(&ServiceApplication::NUMBER_OF_PACKETS_TO_SEND),
						MakeIntegerChecker<int>());
	return typeId;
}

ServiceApplication::ServiceApplication() {
	NS_LOG_FUNCTION(this);
}

ServiceApplication::~ServiceApplication() {
	NS_LOG_FUNCTION(this);
}

void ServiceApplication::DoInitialize() {
	NS_LOG_FUNCTION(this);
	resultsManager = DynamicCast<ResultsApplication>(GetNode()->GetApplication(4));
	ontologyManager = DynamicCast<OntologyApplication>(GetNode()->GetApplication(0));
	localAddress = GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
	socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
	socket->SetAllowBroadcast(false);
	InetSocketAddress local = InetSocketAddress(localAddress, SERVICE_PORT);
	socket->Bind(local);
	Application::DoInitialize();
}

void ServiceApplication::DoDispose() {
	NS_LOG_FUNCTION(this);
	if(socket != NULL) {
		socket->Close();
	}
	Application::DoDispose();
}

void ServiceApplication::StartApplication() {
	NS_LOG_FUNCTION(this);
	socket->SetRecvCallback(MakeCallback(&ServiceApplication::ReceiveMessage, this));
}

void ServiceApplication::StopApplication() {
	NS_LOG_FUNCTION(this);
	if(socket != NULL) {
		socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
	}
}

void ServiceApplication::CreateAndSendRequest(Ipv4Address destinationAddress, std::string service, int requestPackets) {
	NS_LOG_FUNCTION(this << destinationAddress << service << requestPackets);
	ServiceRequestResponseHeader request = CreateRequest(destinationAddress, service);
	SendRequest(request);
	std::pair<uint, std::string> key = GetDestinationKey(request);
	maxPackets[key] = requestPackets;
	status[key] = STRATOS_START_SERVICE;
	NS_LOG_DEBUG(localAddress << " -> Service for " << destinationAddress << " requesting " << requestPackets << " packets is in state " << STRATOS_START_SERVICE);
}

void ServiceApplication::ReceiveMessage(Ptr<Socket> socket) {
	NS_LOG_FUNCTION(this << socket);
	Ptr<Packet> packet = socket->Recv();
	TypeHeader typeHeader;
	packet->RemoveHeader(typeHeader);
	if(!typeHeader.IsValid()) {
		NS_LOG_DEBUG(localAddress << " -> Received service message is invalid");
		return;
	}
	NS_LOG_DEBUG(localAddress << " -> Processing service message");
	switch(typeHeader.GetType()) {
		case STRATOS_SERVICE_ERROR:
			ReceiveError(packet);
			break;
		case STRATOS_SERVICE_REQUEST:
			ReceiveRequest(packet);
			break;
		case STRATOS_SERVICE_RESPONSE:
			ReceiveResponse(packet);
			break;
		default:
			NS_LOG_WARN(localAddress << " -> Service message is unknown!");
			break;
	}
}

void ServiceApplication::SetCallback(Callback<void> continueScheduleCallback) {
	this->continueScheduleCallback = continueScheduleCallback;
}

void ServiceApplication::CancelService(std::pair<uint, std::string> key) {
	NS_LOG_FUNCTION(this << &key);
	status[key] = STRATOS_SERVICE_STOPPED;
	NS_LOG_DEBUG(localAddress << " -> Service for " << key.first << " is in state " << STRATOS_SERVICE_STOPPED);
	if(continueScheduleCallback.IsNull()) {
		NS_LOG_ERROR(localAddress << " -> Schedule Callback must not be null!");
		return;
	}
	continueScheduleCallback();
}

void ServiceApplication::SendUnicastMessage(Ptr<Packet> packet, uint destinationAddress) {
	NS_LOG_FUNCTION(this << packet << destinationAddress);
	InetSocketAddress remote = InetSocketAddress(Ipv4Address(destinationAddress), SERVICE_PORT);
	socket->Connect(remote);
	socket->Send(packet);
}

std::pair<uint, std::string> ServiceApplication::GetSenderKey(ServiceErrorHeader errorHeader) {
	NS_LOG_FUNCTION(this << errorHeader);
	std::string service = errorHeader.GetService();
	uint senderAddress = errorHeader.GetSenderAddress().Get();
	NS_LOG_DEBUG(localAddress << " -> Sender key from error is [" << senderAddress << ", " << service << "] " << errorHeader);
	return std::make_pair(senderAddress, service);
}

std::pair<uint, std::string> ServiceApplication::GetSenderKey(ServiceRequestResponseHeader requestResponse) {
	NS_LOG_FUNCTION(this << requestResponse);
	std::string service = requestResponse.GetService();
	uint senderAddress = requestResponse.GetSenderAddress().Get();
	NS_LOG_DEBUG(localAddress << " -> Sender key is [" << senderAddress << ", " << service << "] " << requestResponse);
	return std::make_pair(senderAddress, service);
}

std::pair<uint, std::string> ServiceApplication::GetDestinationKey(ServiceRequestResponseHeader requestResponse) {
	NS_LOG_FUNCTION(this << requestResponse);
	std::string service = requestResponse.GetService();
	uint destinationAddress = requestResponse.GetDestinationAddress().Get();
	NS_LOG_DEBUG(localAddress << " -> Destination key is [" << destinationAddress << ", " << service << "] " << requestResponse);
	return std::make_pair(destinationAddress, service);
}

void ServiceApplication::Retry(Ptr<Packet> packet, int nTry, std::pair<uint, std::string> key, uint destinationAddress)  {
	NS_LOG_FUNCTION(this << packet << nTry << &key << destinationAddress);
	if (nTry <= MAX_TRIES) {
		NS_LOG_DEBUG(localAddress << " -> Retrying request (" << nTry << ")");
		Simulator::Schedule(Seconds(Utilities::GetJitter()), &ServiceApplication::SendUnicastMessage, this, packet, destinationAddress);
		NS_LOG_DEBUG(localAddress << " -> Schedule next retry");
		resends[key] = Simulator::Schedule(Seconds(MAX_RESPONSE_WAIT_TIME + Utilities::GetJitter()), &ServiceApplication::Retry, this, packet, ++nTry, key, destinationAddress);
	}
}

void ServiceApplication::ReceiveRequest(Ptr<Packet> packet) {
	NS_LOG_FUNCTION(this << packet);
	ServiceRequestResponseHeader requestHeader;
	packet->RemoveHeader(requestHeader);
	NS_LOG_DEBUG(localAddress << " -> Received request " << requestHeader);
	if(!ontologyManager->DoIProvideService(requestHeader.GetService())) {
		NS_LOG_DEBUG(localAddress << " -> Request for a service not provided " << requestHeader.GetService() << ", sending error");
		CreateAndSendError(requestHeader);
		return;
	}
	Flag flag;
	std::pair<uint, std::string> requester = GetSenderKey(requestHeader);
	Simulator::Cancel(timers[requester]);
	Simulator::Cancel(resends[requester]);
	Flag currentStatus = status[requester];
	NS_LOG_DEBUG(localAddress << " -> Service for [" << requester.first << ", " << requester.second << "] is in state " << currentStatus);
	NS_LOG_DEBUG(localAddress << " -> Request [" << requester.first << ", " << requester.second << "] has flag " << requestHeader.GetFlag());
	switch(requestHeader.GetFlag()) {
		case STRATOS_START_SERVICE:
			if(currentStatus == STRATOS_NULL) {
				flag = STRATOS_SERVICE_STARTED;
				status[requester] = STRATOS_DO_SERVICE;
				NS_LOG_DEBUG(localAddress << " -> Service for [" << requester.first << ", " << requester.second << "] changes to state " << STRATOS_DO_SERVICE);
				CreateAndSendResponse(requestHeader, flag);
			} else {
				NS_LOG_DEBUG(localAddress << " -> Request [" << requester.first << ", " << requester.second << "] out of sync, sending error");
				CreateAndSendError(requestHeader);
			}
		break;
		case STRATOS_DO_SERVICE:
			if(currentStatus == STRATOS_DO_SERVICE) {
				if(packets[requester] < NUMBER_OF_PACKETS_TO_SEND) {
					flag = STRATOS_DO_SERVICE;
					packets[requester] += 1;
					NS_LOG_DEBUG(localAddress << " -> Sending data packet to request [" << requester.first << ", " << requester.second << "]");
				} else {
					flag = STRATOS_SERVICE_STOPPED;
					status[requester] = STRATOS_SERVICE_STOPPED;
					NS_LOG_DEBUG(localAddress << " -> No data left for request [" << requester.first << ", " << requester.second << "]");
					NS_LOG_DEBUG(localAddress << " -> Service for [" << requester.first << ", " << requester.second << "] changes to state " << STRATOS_SERVICE_STOPPED);
				}
				CreateAndSendResponse(requestHeader, flag);
			} else {
				NS_LOG_DEBUG(localAddress << " -> Request [" << requester.first << ", " << requester.second << "] out of sync, sending error");
				CreateAndSendError(requestHeader);
			}
		break;
		case STRATOS_STOP_SERVICE:
			flag = STRATOS_SERVICE_STOPPED;
			status[requester] = STRATOS_SERVICE_STOPPED;
			NS_LOG_DEBUG(localAddress << " -> Service for [" << requester.first << ", " << requester.second << "] changes to state " << STRATOS_SERVICE_STOPPED);
			CreateAndSendResponse(requestHeader, flag);
		break;
		default:
			NS_LOG_WARN(localAddress << " -> Request [" << requester.first << ", " << requester.second << "] has unknown flag " << requestHeader.GetFlag());
	}
}

void ServiceApplication::SendRequest(ServiceRequestResponseHeader requestHeader) {
	NS_LOG_FUNCTION(this << requestHeader);
	std::pair<uint, std::string> key = GetDestinationKey(requestHeader);
	Ptr<Packet> packet = Create<Packet>(PACKET_LENGTH);
	packet->AddHeader(requestHeader);
	TypeHeader typeHeader(STRATOS_SERVICE_REQUEST);
	packet->AddHeader(typeHeader);
	NS_LOG_DEBUG(localAddress << " -> Schedule request to send");
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &ServiceApplication::SendUnicastMessage, this, packet, requestHeader.GetDestinationAddress().Get());
	NS_LOG_DEBUG(localAddress << " -> Schedule next retry");
	resends[GetDestinationKey(requestHeader)] = Simulator::Schedule(Seconds(MAX_RESPONSE_WAIT_TIME), &ServiceApplication::Retry, this, packet, 1, GetDestinationKey(requestHeader), requestHeader.GetDestinationAddress().Get());
	NS_LOG_DEBUG(localAddress << " -> Setting up cancel timer");
	timers[key] = Simulator::Schedule(Seconds(MAX_RESPONSE_WAIT_TIME * (MAX_TRIES + 1)), &ServiceApplication::CancelService, this, key);
}

void ServiceApplication::CreateAndSendRequest(ServiceRequestResponseHeader response, Flag flag) {
	NS_LOG_FUNCTION(this << response << flag);
	SendRequest(CreateRequest(response, flag));
}

ServiceRequestResponseHeader ServiceApplication::CreateRequest(ServiceRequestResponseHeader response, Flag flag) {
	NS_LOG_FUNCTION(this << response << flag);
	ServiceRequestResponseHeader request;
	request.SetFlag(flag);
	request.SetSenderAddress(localAddress);
	request.SetService(response.GetService());
	request.SetDestinationAddress(response.GetSenderAddress());
	NS_LOG_DEBUG(localAddress << " -> Request created: " << request);
	return request;
}

ServiceRequestResponseHeader ServiceApplication::CreateRequest(Ipv4Address destinationAddress, std::string service) {
	NS_LOG_FUNCTION(this << destinationAddress << service);
	ServiceRequestResponseHeader request;
	request.SetService(service);
	request.SetFlag(STRATOS_START_SERVICE);
	request.SetSenderAddress(localAddress);
	request.SetDestinationAddress(destinationAddress);
	NS_LOG_DEBUG(localAddress << " -> Request created: " << request);
	return request;
}

void ServiceApplication::ReceiveError(Ptr<Packet> packet) {
	NS_LOG_FUNCTION(this << packet);
	ServiceErrorHeader errorHeader;
	packet->RemoveHeader(errorHeader);
	NS_LOG_DEBUG(localAddress << " -> Error received: " << errorHeader);
	std::pair<uint, std::string> key = GetSenderKey(errorHeader);
	NS_LOG_DEBUG(localAddress << " -> Cancelling service [" << key.first << ", " << key.second << "]");
	CancelService(key);
}

void ServiceApplication::SendError(ServiceErrorHeader errorHeader) {
	NS_LOG_FUNCTION(this << errorHeader);
	Ptr<Packet> packet = Create<Packet>();
	packet->AddHeader(errorHeader);
	TypeHeader typeHeader(STRATOS_SERVICE_ERROR);
	packet->AddHeader(typeHeader);
	NS_LOG_DEBUG(localAddress << " -> Schedule error to send");
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &ServiceApplication::SendUnicastMessage, this, packet, errorHeader.GetDestinationAddress().Get());
}

void ServiceApplication::CreateAndSendError(ServiceRequestResponseHeader requestResponse) {
	NS_LOG_FUNCTION(this << requestResponse);
	SendError(CreateError(requestResponse));
}

ServiceErrorHeader ServiceApplication::CreateError(ServiceRequestResponseHeader requestResponse) {
	NS_LOG_FUNCTION(this << requestResponse);
	ServiceErrorHeader error;
	error.SetService(requestResponse.GetService());
	error.SetSenderAddress(requestResponse.GetDestinationAddress());
	error.SetDestinationAddress(requestResponse.GetSenderAddress());
	NS_LOG_DEBUG(localAddress << " -> Error created: " << error);
	return error;
}

void ServiceApplication::ReceiveResponse(Ptr<Packet> packet) {
	NS_LOG_FUNCTION(this << packet);
	ServiceRequestResponseHeader responseHeader;
	packet->RemoveHeader(responseHeader);
	NS_LOG_DEBUG(localAddress << " -> Received response " << responseHeader);
	Flag flag;
	std::pair<uint, std::string> responser = GetSenderKey(responseHeader);
	Simulator::Cancel(timers[responser]);
	Simulator::Cancel(resends[responser]);
	Flag currentStatus = status[responser];
	NS_LOG_DEBUG(localAddress << " -> Service for [" << responser.first << ", " << responser.second << "] is in state " << currentStatus);
	NS_LOG_DEBUG(localAddress << " -> Response [" << responser.first << ", " << responser.second << "] has flag " << responseHeader.GetFlag());
	switch(responseHeader.GetFlag()) {
		case STRATOS_SERVICE_STARTED:
			if(currentStatus == STRATOS_START_SERVICE) {
				flag = STRATOS_DO_SERVICE;
				status[responser] = STRATOS_DO_SERVICE;
				NS_LOG_DEBUG(localAddress << " -> Service for [" << responser.first << ", " << responser.second << "] changes to state " << STRATOS_DO_SERVICE);
				CreateAndSendRequest(responseHeader, flag);
			} else {
				NS_LOG_DEBUG(localAddress << " -> Response [" << responser.first << ", " << responser.second << "] out of sync, sending error");
				CreateAndSendError(responseHeader);
			}
		break;
		case STRATOS_DO_SERVICE:
			if(currentStatus == STRATOS_DO_SERVICE) {
				if((packets[responser] + 1) <= maxPackets[responser]) {
					flag = STRATOS_DO_SERVICE;
					packets[responser] += 1;
					resultsManager->AddPacket(Now().GetMilliSeconds());
					NS_LOG_DEBUG(localAddress << " -> Received data packet from [" << responser.first << ", " << responser.second << "]");
				}
				if(packets[responser] >= maxPackets[responser]) {
					flag = STRATOS_STOP_SERVICE;
					status[responser] = STRATOS_STOP_SERVICE;
					NS_LOG_DEBUG(localAddress << " -> All data received from [" << responser.first << ", " << responser.second << "]");
					NS_LOG_DEBUG(localAddress << " -> Service for [" << responser.first << ", " << responser.second << "] changes to state " << STRATOS_STOP_SERVICE);
				}
				CreateAndSendRequest(responseHeader, flag);
			} else {
				NS_LOG_DEBUG(localAddress << " -> Response [" << responser.first << ", " << responser.second << "] out of sync, sending error");
				CreateAndSendError(responseHeader);
			}
		break;
		case STRATOS_SERVICE_STOPPED:
			status[responser] = STRATOS_SERVICE_STOPPED;
			NS_LOG_DEBUG(localAddress << " -> Service for [" << responser.first << ", " << responser.second << "] changes to state " << STRATOS_SERVICE_STOPPED);
			CancelService(responser);
		break;
		default:
			NS_LOG_WARN(localAddress << " -> Request [" << responser.first << ", " << responser.second << "] has unknown flag " << responseHeader.GetFlag());
	}
}

void ServiceApplication::SendResponse(ServiceRequestResponseHeader responseHeader) {
	NS_LOG_FUNCTION(this << responseHeader);
	std::pair<uint, std::string> key = GetDestinationKey(responseHeader);
	Ptr<Packet> packet = Create<Packet>(PACKET_LENGTH);
	packet->AddHeader(responseHeader);
	TypeHeader typeHeader(STRATOS_SERVICE_RESPONSE);
	packet->AddHeader(typeHeader);
	NS_LOG_DEBUG(localAddress << " -> Schedule response to send");
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &ServiceApplication::SendUnicastMessage, this, packet, responseHeader.GetDestinationAddress().Get());
	NS_LOG_DEBUG(localAddress << " -> Schedule next retry");
	resends[key] = Simulator::Schedule(Seconds(MAX_RESPONSE_WAIT_TIME), &ServiceApplication::Retry, this, packet, 1, key, responseHeader.GetDestinationAddress().Get());
	NS_LOG_DEBUG(localAddress << " -> Setting up cancel timer");
	timers[key] = Simulator::Schedule(Seconds(MAX_RESPONSE_WAIT_TIME * (MAX_TRIES + 1)), &ServiceApplication::CancelService, this, key);
}

void ServiceApplication::CreateAndSendResponse(ServiceRequestResponseHeader request, Flag flag) {
	NS_LOG_FUNCTION(this << request << flag);
	SendResponse(CreateResponse(request, flag));
}

ServiceRequestResponseHeader ServiceApplication::CreateResponse(ServiceRequestResponseHeader request, Flag flag) {
	NS_LOG_FUNCTION(this << request << flag);
	ServiceRequestResponseHeader response;
	response.SetFlag(flag);
	response.SetSenderAddress(localAddress);
	response.SetService(request.GetService());
	response.SetDestinationAddress(request.GetSenderAddress());
	NS_LOG_DEBUG(localAddress << " -> Response created: " << response);
	return response;
}

ServiceHelper::ServiceHelper() {
	NS_LOG_FUNCTION(this);
	objectFactory.SetTypeId("ServiceApplication");
}