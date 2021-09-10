/*
Copyright 2021 Stephen Haddock Consulting, LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#include "LldpPort.h"



class LinkLayerDiscovery : public Component
{
public:
	LinkLayerDiscovery(unsigned long long chassis);
	~LinkLayerDiscovery();
	LinkLayerDiscovery(LinkLayerDiscovery& copySource) = delete;             // Disable copy constructor
	LinkLayerDiscovery& operator= (const LinkLayerDiscovery&) = delete;      // Disable assignment operator

	unsigned long long chassisId;

	std::vector<shared_ptr<LldpPort>> pLldpPorts;

	void reset();
	void timerTick();
	void run(bool singleStep);

/*
//	bool LinkAgg::configDistRelay(unsigned short distRelayIndex, unsigned short numAggPorts, unsigned short numIrp, 
//		sysId drniAggId, unsigned short defaultDrniKey, unsigned short firstLinkNum);
	bool configDistRelay(unsigned short distRelayIndex, unsigned short numAggPorts, unsigned short numIrp,
		sysId drniAggId, unsigned short defaultDrniKey, unsigned short firstLinkNum);

	static unsigned short frameConvID(LagAlgorithms algorithm, Frame& thisFrame);  // const??
	static unsigned short macAddrHash(Frame& thisFrame);
/**/

private:
	/*
	void resetCSDC();
	void runCSDC();
	void updatePartnerDistributionAlgorithm(AggPort& port);
	void updateMask(Aggregator& agg);
	void updateActorDistributionAlgorithm(Aggregator& thisAgg);
	void compareDistributionAlgorithms(Aggregator& thisAgg);
	void updateActiveLinks(Aggregator& thisAgg);
	void updateConversationPortVector(Aggregator& thisAgg);
	void updateConversationMasks(Aggregator& thisAgg);
	void updateAggregatorOperational(Aggregator& thisAgg);
	void debugUpdateMask(Aggregator& thisAgg, std::string routine);


	bool collectFrame(AggPort& thisPort, Frame& thisFrame);  // const??
//	shared_ptr<AggPort> LinkAgg::distributeFrame(Aggregator& thisAgg, Frame& thisFrame);
	shared_ptr<AggPort> distributeFrame(Aggregator& thisAgg, Frame& thisFrame);

//	static class LacpSelection
	class LacpSelection
	{
	public:
//		static void resetSelection(AggPort& port);
		static void runSelection(std::vector<shared_ptr<AggPort>>& pAggPorts, std::vector<shared_ptr<Aggregator>>& pAggregators);
		static void adminAggregatorUpdate(std::vector<shared_ptr<AggPort>>& pAggPorts, std::vector<shared_ptr<Aggregator>>& pAggregators);

	private:
		static int findMatchingAggregator(int thisPort, std::vector<shared_ptr<AggPort>>& pAggPorts);
		static bool matchAggregatorLagid(AggPort& port, Aggregator& agg);
		static bool allowableAggregator(AggPort& port, Aggregator& agg);
		static void clearAggregator(Aggregator& agg, std::vector<shared_ptr<AggPort>>& pAggPorts);
		static int findAvailableAggregator(AggPort& thisPort, std::vector<shared_ptr<AggPort>>& pAggPorts, std::vector<shared_ptr<Aggregator>>& pAggregators);
		static bool activeAggregator(Aggregator& agg, std::vector<shared_ptr<AggPort>>& pAggPorts);
	};
	/**/
};

union addr12
{
	unsigned long long all;
	struct
	{
		unsigned long long low : 12;
		unsigned long long loMid : 12;
		unsigned long long hiMid : 12;
		unsigned long long high : 12;
	};
};



