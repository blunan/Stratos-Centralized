#ifndef SERVICE_APPLICATION_H
#define SERVICE_APPLICATION_H

#include "ns3/internet-module.h"

#include "application-helper.h"
#include "results-application.h"
#include "service-error-header.h"
#include "ontology-application.h"
#include "service-request-response-header.h"

using namespace ns3;

class ScheduleApplication;

class ServiceApplication : public Application {

	public:
		static TypeId GetTypeId();

		ServiceApplication();
		~ServiceApplication();

	protected:
		virtual void DoInitialize();
		virtual void DoDispose();

	private:
		virtual void StartApplication();
		virtual void StopApplication();

	public:
		int NUMBER_OF_PACKETS_TO_SEND;
		void CreateAndSendRequest(Ipv4Address destinationAddress, std::string service, int packets);

	private:
		Ptr<Socket> socket;
		Ipv4Address localAddress;
		Ptr<ResultsApplication> resultsManager;
		Ptr<OntologyApplication> ontologyManager;
		Ptr<ScheduleApplication> scheduleManager;
		std::map<std::pair<uint, std::string>, Flag> status;
		std::map<std::pair<uint, std::string>, int> packets;
		std::map<std::pair<uint, std::string>, int> maxPackets;
		std::map<std::pair<uint, std::string>, EventId> timers;
		std::map<std::pair<uint, std::string>, EventId> resends;

		void ReceiveMessage(Ptr<Socket> socket);
		void CancelService(std::pair<uint, std::string> key);
		void SendUnicastMessage(Ptr<Packet> packet, uint destinationAddress);
		std::pair<uint, std::string> GetSenderKey(ServiceErrorHeader errorHeader);
		std::pair<uint, std::string> GetSenderKey(ServiceRequestResponseHeader requestResponse);
		std::pair<uint, std::string> GetDestinationKey(ServiceRequestResponseHeader requestResponse);
		void Retry(Ptr<Packet> packet, int nTry, std::pair<uint, std::string> key, uint destinationAddress);

		void ReceiveRequest(Ptr<Packet> packet);
		void SendRequest(ServiceRequestResponseHeader requestHeader);
		void CreateAndSendRequest(ServiceRequestResponseHeader response, Flag flag);
		ServiceRequestResponseHeader CreateRequest(ServiceRequestResponseHeader response, Flag flag);
		ServiceRequestResponseHeader CreateRequest(Ipv4Address destinationAddress, std::string service);

		void ReceiveError(Ptr<Packet> packet);
		void SendError(ServiceErrorHeader errorHeader);
		void CreateAndSendError(ServiceRequestResponseHeader requestResponse);
		ServiceErrorHeader CreateError(ServiceRequestResponseHeader requestResponse);

		void ReceiveResponse(Ptr<Packet> packet);
		void SendResponse(ServiceRequestResponseHeader responseHeader);
		void CreateAndSendResponse(ServiceRequestResponseHeader request, Flag flag);
		ServiceRequestResponseHeader CreateResponse(ServiceRequestResponseHeader request, Flag flag);
};

class ServiceHelper : public ApplicationHelper {

	public:
		ServiceHelper();
};

#endif