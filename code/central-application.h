#ifndef CENTRAL_APPLICATION_H
#define CENTRAL_APPLICATION_H

#include "ns3/internet-module.h"

#include <map>
#include <pthread.h>

#include "definitions.h"
#include "application-helper.h"
#include "search-error-header.h"
#include "position-application.h"
#include "ontology-application.h"
#include "search-request-header.h"
#include "search-response-header.h"
#include "search-notification-header.h"

using namespace ns3;

class CentralApplication : public Application {

	public:
		static TypeId GetTypeId();

		CentralApplication();
		~CentralApplication();

	protected:
		virtual void DoInitialize();
		virtual void DoDispose();

	private:
		virtual void StartApplication();
		virtual void StopApplication();

	private:
		pthread_mutex_t mutex;
		std::map<uint, POSITION> positions;
		std::map<uint, std::list<std::string> > services;

		Ptr<Socket> socket;
		Ptr<PositionApplication> positionManager;
		Ptr<OntologyApplication> ontologyManager;
		
		void ReceiveMessage(Ptr<Socket> socket);
		void SendUnicastMessage(Ptr<Packet> packet, uint destinationAddress);

		void ReceiveRequest(Ptr<Packet> packet);
		std::list<uint> FilterNodesByDistance(SearchRequestHeader request);
		uint GetBestNode(std::list<uint> nodes, SearchRequestHeader request);

		void ReceiveNotification(Ptr<Packet> packet);
		
		void SendError(SearchErrorHeader errorHeader);
		void CreateAndSendError(SearchRequestHeader request);
		SearchErrorHeader CreateError(SearchRequestHeader request);

		void SendResponse(SearchResponseHeader responseHeader);
		void CreateAndSendResponse(uint node, SearchRequestHeader request);
		SearchResponseHeader CreateResponse(uint node, SearchRequestHeader request);
};

class CentralHelper : public ApplicationHelper {

	public:
		CentralHelper();
};

#endif