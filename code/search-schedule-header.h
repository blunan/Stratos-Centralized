#ifndef SEARCH_SCHEDULE_HEADER_H
#define SEARCH_SCHEDULE_HEADER_H

#include "ns3/header.h"
#include "ns3/internet-module.h"

#include "definitions.h"
#include "search-response-header.h"

using namespace ns3;

class SearchScheduleHeader : public Header {

	public:
		static TypeId GetTypeId();
		virtual TypeId GetInstanceTypeId() const;
		virtual uint32_t GetSerializedSize() const;
		virtual void Print(std::ostream &stream) const;
		virtual uint32_t Deserialize(Buffer::Iterator start);
		virtual void Serialize(Buffer::Iterator serializer) const;

	private:
		int serializedScheduleSize;

		double requestTimestamp;
		Ipv4Address requestAddress;
		std::list<SearchResponseHeader> schedule;

	public:
		SearchScheduleHeader();

		double GetRequestTimestamp();
		Ipv4Address GetRequestAddress();
		std::list<SearchResponseHeader> GetSchedule();

		void SetRequestTimestamp(double requestTimestamp);
		void SetRequestAddress(Ipv4Address requestAddress);
		void SetSchedule(std::list<SearchResponseHeader> schedule);
};
std::ostream & operator<< (std::ostream & stream, SearchScheduleHeader const & scheduleHeader);

#endif