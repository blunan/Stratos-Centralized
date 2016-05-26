#ifndef SEARCH_NOTIFICATION_HEADER_H
#define SEARCH_NOTIFICATION_HEADER_H

#include "ns3/header.h"
#include "ns3/internet-module.h"

#include "definitions.h"

using namespace ns3;

class SearchNotificationHeader : public Header {

	public:
		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;
		virtual uint32_t GetSerializedSize() const;
		virtual void Print(std::ostream &stream) const;
		virtual uint32_t Deserialize(Buffer::Iterator start);
		virtual void Serialize(Buffer::Iterator serializer) const;

	private:
		int nOfferedServices;
		int *offeredServicesSize;

		Ipv4Address nodeAddress;
		POSITION currentPosition;
		std::list<std::string> offeredServices;

	public:
		SearchNotificationHeader();

		Ipv4Address GetNodeAddress();
		POSITION GetCurrentPosition();
		std::list<std::string> GetOfferedServices();

		void SetNodeAddress(Ipv4Address nodeAddress);
		void SetCurrentPosition(POSITION currentPosition);
		void SetOfferedServices(std::list<std::string> offeredServices);
};
std::ostream & operator<< (std::ostream & stream, SearchNotificationHeader const & notificationHeader);

#endif