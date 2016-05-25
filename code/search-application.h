#ifndef SEARCH_APPLICATION_H
#define SEARCH_APPLICATION_H

#include "ns3/internet-module.h"

#include <map>

#include "application-helper.h"
#include "service-application.h"
#include "search-error-header.h"
#include "results-application.h"
#include "position-application.h"
#include "ontology-application.h"
#include "schedule-application.h"
#include "search-request-header.h"
#include "search-response-header.h"
#include "search-notification-header.h"

using namespace ns3;

class SearchApplication : public Application {

	public:
		static TypeId GetTypeId();

		SearchApplication();
		~SearchApplication();

	protected:
		virtual void DoInitialize();
		virtual void DoDispose();

	private:
		virtual void StartApplication();
		virtual void StopApplication();

	public:
		void CreateAndSendRequest();

	private:
		std::map<std::pair<uint, double>, EventId> timers;

		bool response;
		Ptr<Socket> socket;
		Ipv4Address localAddress;
		uint centralServerAddress;
		Ptr<ResultsApplication> resultsManager;
		Ptr<ServiceApplication> serviceManager;
		Ptr<PositionApplication> positionManager;
		Ptr<OntologyApplication> ontologyManager;
		Ptr<ScheduleApplication> scheduleManager;

		void ReceiveMessage(Ptr<Socket> socket);
		void SendUnicastMessage(Ptr<Packet> packet, uint destinationAddress);

		SearchRequestHeader CreateRequest();
		void SendRequest(SearchRequestHeader requestHeader);
		std::pair<uint, double> GetRequestKey(SearchRequestHeader request);
		void RetryRequest(Ptr<Packet> packet, int nTry, std::pair<uint, double> key);

		void ReceiveError(Ptr<Packet> packet);
		std::pair<uint, double> GetRequestKey(SearchErrorHeader error);

		void ReceiveResponse(Ptr<Packet> packet);
		std::pair<uint, double> GetRequestKey(SearchResponseHeader response);

		void CreateAndSendNotification();
		SearchNotificationHeader CreateNotification();
		void SendNotification(SearchNotificationHeader notificationHeader);
};

class SearchHelper : public ApplicationHelper {

	public:
		SearchHelper();
};

#endif