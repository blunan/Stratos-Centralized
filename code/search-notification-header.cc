#include "search-notification-header.h"

#include "ns3/address-utils.h"

#include "utilities.h"

TypeId SearchNotificationHeader::GetTypeId() {
	static TypeId typeId = TypeId("SearchNotificationHeader")
		.SetParent<Header>()
		.AddConstructor<SearchNotificationHeader>();
	return typeId;
}

TypeId SearchNotificationHeader::GetInstanceTypeId() const {
	return GetTypeId();
}

uint32_t SearchNotificationHeader::GetSerializedSize() const {
	int sum = 0;
	for(int i = 0; i < nOfferedServices; i++) {
		sum += offeredServicesSize[i];
	}
	return 14 + (nOfferedServices * 2) + sum;
}

void SearchNotificationHeader::Print(std::ostream &stream) const {
	stream << "Search notification sent from " << nodeAddress << " in (" << currentPosition.x << ", " << currentPosition.y << ") offering: ";
	for(std::list<std::string>::const_iterator i = offeredServices.begin(); i != offeredServices.end(); i++) {
		stream << *i << ", ";
	}
}

uint32_t SearchNotificationHeader::Deserialize(Buffer::Iterator start) {
	Buffer::Iterator i = start;
	ReadFrom(i, nodeAddress);
	currentPosition.x = i.ReadU32();
	currentPosition.y = i.ReadU32();
	nOfferedServices = i.ReadU16();
	offeredServicesSize = new int[nOfferedServices];
	for(int j = 0; j < nOfferedServices; j++) {
		offeredServicesSize[j] = i.ReadU16();
	}
	offeredServices.clear();
	for(int j = 0; j < nOfferedServices; j++) {
		char tmp[offeredServicesSize[j] + 1];
		for(int k = 0; k < offeredServicesSize[j]; k++) {
			tmp[k] = i.ReadU8();
		}
		tmp[offeredServicesSize[j]] = '\0';
		offeredServices.push_back(std::string(tmp));
	}
	uint32_t size = i.GetDistanceFrom(start);
	return size;
}

void SearchNotificationHeader::Serialize(Buffer::Iterator serializer) const {
	WriteTo(serializer, nodeAddress);
	serializer.WriteU32(currentPosition.x);
	serializer.WriteU32(currentPosition.y);
	serializer.WriteU16(nOfferedServices);
	for(int j = 0; j < nOfferedServices; j++) {
		serializer.WriteU16(offeredServicesSize[j]);
	}
	for(std::list<std::string>::const_iterator j = offeredServices.begin(); j != offeredServices.end(); j++) {
		std::string service = *j;
		for(int k = 0; k < (int) service.length(); k++) {
			serializer.WriteU8(service.at(k));
		}
	}
}

SearchNotificationHeader::SearchNotificationHeader() {
	nOfferedServices = 0;
	currentPosition.x = 0;
	currentPosition.y = 0;
	offeredServicesSize = NULL;
	nodeAddress = Ipv4Address::GetAny();
}

Ipv4Address SearchNotificationHeader::GetNodeAddress() {
	return nodeAddress;
}

POSITION SearchNotificationHeader::GetCurrentPosition() {
	return currentPosition;
}

std::list<std::string> SearchNotificationHeader::GetOfferedServices() {
	return offeredServices;
}

void SearchNotificationHeader::SetNodeAddress(Ipv4Address nodeAddress) {
	this->nodeAddress = nodeAddress;
}

void SearchNotificationHeader::SetCurrentPosition(POSITION currentPosition) {
	this->currentPosition = currentPosition;
}

void SearchNotificationHeader::SetOfferedServices(std::list<std::string> offeredServices) {
	this->offeredServices = offeredServices;
	nOfferedServices = offeredServices.size();
	offeredServicesSize = new int[nOfferedServices];
	int i = 0;
	for(std::list<std::string>::iterator j = offeredServices.begin(); j != offeredServices.end(); j++) {
		offeredServicesSize[i++] = (*j).length();
	}
}

std::ostream & operator<< (std::ostream & stream, SearchNotificationHeader const & notificationHeader) {
	notificationHeader.Print(stream);
	return stream;
}