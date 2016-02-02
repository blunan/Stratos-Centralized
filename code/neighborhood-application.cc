#include "neighborhood-application.h"

#include "utilities.h"
#include "type-header.h"

NS_OBJECT_ENSURE_REGISTERED(NeighborhoodApplication);

TypeId NeighborhoodApplication::GetTypeId() {
	static TypeId typeId = TypeId("NeighborhoodApplication")
		.SetParent<Application>()
		.AddConstructor<NeighborhoodApplication>();
	return typeId;
}

NeighborhoodApplication::NeighborhoodApplication() {
}

void NeighborhoodApplication::DoInitialize() {
	neighborhood.clear();
	pthread_mutex_init(&mutex, NULL);
	socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
	InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), HELLO_PORT);
	socket->Bind(local);
	InetSocketAddress remote = InetSocketAddress(Ipv4Address::GetBroadcast(), HELLO_PORT);
	socket->SetAllowBroadcast(true);
	socket->Connect(remote);
	Application::DoInitialize();
}

NeighborhoodApplication::~NeighborhoodApplication() {
}

void NeighborhoodApplication::DoDispose() {
	neighborhood.clear();
	if(socket != NULL) {
		socket->Close();
	}
	pthread_mutex_destroy(&mutex);
	Application::DoDispose();
}

void NeighborhoodApplication::StartApplication() {
	ScheduleNextUpdate();
	socket->SetRecvCallback(MakeCallback(&NeighborhoodApplication::ReceiveHelloMessage, this));
	Simulator::Schedule(Seconds(Utilities::GetJitter()), &NeighborhoodApplication::ScheduleNextHelloMessage, this);
}

void NeighborhoodApplication::StopApplication() {
	Simulator::Cancel(sendHelloMessage);
	Simulator::Cancel(updateNeighborhood);
	if(socket != NULL) {
		socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
	}
}

void NeighborhoodApplication::SendHelloMessage() {
	Ptr<Packet> packet = Create<Packet>();
	TypeHeader typeHeader(STRATOS_HELLO);
	//resultsManager->addBytesControl(typeHeader.GetSerializedSize());
	packet->AddHeader(typeHeader);
	socket->Send(packet);
	ScheduleNextHelloMessage();
}

void NeighborhoodApplication::UpdateNeighborhood() {
	double seconds;
	NEIGHBOR neighbor;
	std::list<NEIGHBOR>::iterator i;
	double now = Utilities::GetCurrentRawDateTime();
	pthread_mutex_lock(&mutex);
	for(i = neighborhood.begin(); i != neighborhood.end(); i++) {
		neighbor = *i;
		seconds = Utilities::GetSecondsElapsedSinceUntil(neighbor.lastSeen, now);
		if(seconds >= MAX_TIMES_NOT_SEEN * HELLO_TIME) {
			neighborhood.erase(i--);
		}
	}
	pthread_mutex_unlock(&mutex);
	ScheduleNextUpdate();
}

void NeighborhoodApplication::ScheduleNextUpdate() {
	updateNeighborhood = Simulator::Schedule(Seconds(HELLO_TIME), &NeighborhoodApplication::UpdateNeighborhood, this);
}

void NeighborhoodApplication::ScheduleNextHelloMessage() {
	sendHelloMessage = Simulator::Schedule(Seconds(Utilities::GetJitter() + HELLO_TIME), &NeighborhoodApplication::SendHelloMessage, this);
}

void NeighborhoodApplication::ReceiveHelloMessage(Ptr<Socket> socket) {
	Address sourceAddress;
	Ptr<Packet> packet = socket->RecvFrom(sourceAddress);
	TypeHeader typeHeader;
	packet->RemoveHeader(typeHeader);
	if(!typeHeader.IsValid() || typeHeader.GetType() != STRATOS_HELLO) {
		return;
	}
	InetSocketAddress inetSourceAddress = InetSocketAddress::ConvertFrom(sourceAddress);
	Ipv4Address sender = inetSourceAddress.GetIpv4();
	AddUpdateNeighborhood(sender.Get(), Utilities::GetCurrentRawDateTime());
}

void NeighborhoodApplication::AddUpdateNeighborhood(uint address, double now) {
	pthread_mutex_lock(&mutex);
	std::list<NEIGHBOR>::iterator i;
	for(i = neighborhood.begin(); i != neighborhood.end(); i++) {
		if((*i).address == address) {
			neighborhood.erase(i);
			break;
		}
	}
	NEIGHBOR neighbor;
	neighbor.lastSeen = now;
	neighbor.address = address;
	neighborhood.push_back(neighbor);
	pthread_mutex_unlock(&mutex);
}

std::list<uint> NeighborhoodApplication::GetNeighborhood() {
	std::list<uint> neighborhood;
	std::list<NEIGHBOR>::iterator i;
	pthread_mutex_lock(&mutex);
	for(i = this->neighborhood.begin(); i != this->neighborhood.end(); i++) {
		neighborhood.push_back((*i).address);
	}
	pthread_mutex_unlock(&mutex);
	return neighborhood;
}

NeighborhoodHelper::NeighborhoodHelper() {
	objectFactory.SetTypeId("NeighborhoodApplication");
}