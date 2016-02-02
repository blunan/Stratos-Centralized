#include "ontology-application.h"

#include "ns3/core-module.h"

#include <limits>

#include "utilities.h"

NS_OBJECT_ENSURE_REGISTERED(OntologyApplication);

const std::string OntologyApplication::SERVICES[] = {"0", "00", "000", "0000", "00000", "00001", "0001", "0002", "00020", "00021", "00022", "0003", "00030", "00031", "001", "0010", "00100", "0011", "00110", "00111", "01", "010", "0100", "01000", "0101", "01010", "01011", "01012", "01013", "011", "0110", "02", "020", "021", "022", "023"};

const int OntologyApplication::TOTAL_NUMBER_OF_SERVICES = 35;

TypeId OntologyApplication::GetTypeId() {
	static TypeId typeId = TypeId("OntologyApplication")
		.SetParent<Application>()
		.AddConstructor<OntologyApplication>()
		.AddAttribute("nServices",
						"Number of services offered by a node.",
						IntegerValue(2),
						MakeIntegerAccessor(&OntologyApplication::NUMBER_OF_SERVICES_OFFERED),
						MakeIntegerChecker<int>());
	return typeId;
}

OntologyApplication::OntologyApplication() {
}

OntologyApplication::~OntologyApplication() {
}

void OntologyApplication::DoInitialize() {
	bool found;
	std::string service;
	std::list<std::string>::iterator j;
	for(int i = 0; i < NUMBER_OF_SERVICES_OFFERED; i++) {
		found = false;
		service = GetRandomService();
		for(j = offeredServices.begin(); j != offeredServices.end(); j++) {
			if(service.compare(*j) == 0) {
				found = true;
				break;
			}
		}
		if(found) {
			i--;
		} else {
			offeredServices.push_back(service);
		}
	}
	Application::DoInitialize();
}

void OntologyApplication::DoDispose() {
	offeredServices.clear();
	Application::DoDispose();
}

void OntologyApplication::StartApplication() {
}

void OntologyApplication::StopApplication() {
}

std::string OntologyApplication::GetRandomService() {
	return SERVICES[(int) Utilities::Random(1, TOTAL_NUMBER_OF_SERVICES)];
}

std::string OntologyApplication::GetCommonPrefix(std::string c1, std::string c2) {
	int minLength = c1.length() > c2.length() ? c2.length() : c1.length();
	std::string commonPrefix;
	for(int i = 0; i < minLength; i++) {
		if(c1.at(i) == c2.at(i)) {
			commonPrefix.push_back(c1.at(i));
		} else {
			break;
		}
	}
	return commonPrefix;
}

int OntologyApplication::SemanticDistance(std::string requiredService, std::string offeredService) {
	if(offeredService.compare(requiredService) == 0) {
		return 0;
	}
	std::string commonPrefix = GetCommonPrefix(requiredService, offeredService);
	if(offeredService.compare(commonPrefix) == 0) {
		return 0;
	}
	int distanceFromOfferedToCommon = offeredService.length() - commonPrefix.length();
	int distanceFromRequiredToCommon = requiredService.length() - commonPrefix.length();
	return distanceFromOfferedToCommon > distanceFromRequiredToCommon ? distanceFromOfferedToCommon : distanceFromRequiredToCommon;
}

std::list<std::string> OntologyApplication::GetOfferedServices() {
	return offeredServices;
}

OFFERED_SERVICE OntologyApplication::GetBestOfferedService(std::string requiredService) {
	std::string service;
	int semanticDistance;
	std::string bestOfferedService;
	int minSemanticDistance = std::numeric_limits<int>::max();
	std::list<std::string>::iterator i;
	for(i = offeredServices.begin(); i != offeredServices.end(); i++) {
		service = *i;
		semanticDistance = SemanticDistance(requiredService, service);
		if(semanticDistance < minSemanticDistance) {
			bestOfferedService = service;
			minSemanticDistance = semanticDistance;
		}
	}
	OFFERED_SERVICE result;
	result.service = bestOfferedService;
	result.semanticDistance = minSemanticDistance;
	return result;
}

OFFERED_SERVICE OntologyApplication::GetBestOfferedService(std::string requiredService, std::list<std::string> offeredServices) {
	std::string service;
	int semanticDistance;
	std::string bestOfferedService;
	int minSemanticDistance = std::numeric_limits<int>::max();
	std::list<std::string>::iterator i;
	for(i = offeredServices.begin(); i != offeredServices.end(); i++) {
		service = *i;
		semanticDistance = SemanticDistance(requiredService, service);
		if(semanticDistance < minSemanticDistance) {
			bestOfferedService = service;
			minSemanticDistance = semanticDistance;
		}
	}
	OFFERED_SERVICE result;
	result.service = bestOfferedService;
	result.semanticDistance = minSemanticDistance;
	return result;
}

OntologyHelper::OntologyHelper() {
	objectFactory.SetTypeId("OntologyApplication");
}