#include "search-schedule-header.h"

#include "utilities.h"

TypeId SearchScheduleHeader::GetTypeId() {
	static TypeId typeId = TypeId("SearchScheduleHeader")
		.SetParent<Header>()
		.AddConstructor<SearchScheduleHeader>();
	return typeId;
}

TypeId SearchScheduleHeader::GetInstanceTypeId() const {
	return GetTypeId();
}

uint32_t SearchScheduleHeader::GetSerializedSize() const {
	return 10 + serializedScheduleSize;
}

void SearchScheduleHeader::Print(std::ostream &stream) const {
	stream << "Search schedule response to " << requestAddress << " at " << requestTimestamp << " with " << schedule.size() << " nodes in schedule";
}

uint32_t SearchScheduleHeader::Deserialize(Buffer::Iterator start) {
	Buffer::Iterator i = start;
	ReadFrom(i, requestAddress);
	requestTimestamp = i.ReadU32();
	int scheduleSize = i.ReadU16();
	for(int j = 0; j < scheduleSize; j++) {
		SearchResponseHeader response;
		serializedScheduleSize += response.Deserialize(i);
		schedule.push_back(response);
	}
	uint32_t size = i.GetDistanceFrom(start);
	return size;
}

void SearchScheduleHeader::Serialize(Buffer::Iterator serializer) const {
	WriteTo(serializer, requestAddress);
	serializer.WriteU32(requestTimestamp);
	serializer.WriteU16(schedule.size());
	for(std::list<SearchResponseHeader>::iterator i = list.begin(); i != list.end(); i++) {
		(*i).Serialize(serializer);
	}
}

SearchScheduleHeader::SearchScheduleHeader() {
	schedule.clear();
	serializedScheduleSize = 0;
	requestAddress = Ipv4Address::GetAny();
	requestTimestamp = Utilities::GetCurrentRawDateTime();
}

double SearchScheduleHeader::GetRequestTimestamp() {
	return requestTimestamp;
}

Ipv4Address SearchScheduleHeader::GetRequestAddress() {
	return requestAddress;
}

std::list<SearchResponseHeader> SearchScheduleHeader::GetSchedule() {
	return schedule;
}

void SearchScheduleHeader::SetRequestTimestamp(double requestTimestamp) {
	this->requestTimestamp = requestTimestamp;
}

void SearchScheduleHeader::SetRequestAddress(Ipv4Address requestAddress) {
	this->requestAddress = requestAddress;
}

void SearchScheduleHeader::SetResponseAddress(std::list<SearchResponseHeader> schedule) {
	serializedScheduleSize = 0;
	this->schedule = schedule;
	for(std::list<SearchResponseHeader>::iterator i = list.begin(); i != list.end(); i++) {
		serializedScheduleSize += (*i).GetSerializedSize();
	}
}

std::ostream & operator<< (std::ostream & stream, SearchScheduleHeader const & scheduleHeader) {
	scheduleHeader.Print(stream);
	return stream;
}