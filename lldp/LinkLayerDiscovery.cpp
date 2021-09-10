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

#include "stdafx.h"
#include "LinkLayerDiscovery.h"


LinkLayerDiscovery::LinkLayerDiscovery(unsigned long long chassis)
	: Component(ComponentTypes::LINK_LAYER_DISCOVERY), chassisId(chassis)
{
cout << "LinkLayerDiscovery Constructor called." << endl;
SimLog::logFile << "LinkLayerDiscovery Constructor called." << hex << "  chassis 0x" << chassisId << endl;
}


LinkLayerDiscovery::~LinkLayerDiscovery()
{
	pLldpPorts.clear();
cout << "LinkLayerDiscovery Destructor called." << endl;
SimLog::logFile << "LinkLayerDiscovery Destructor called." << hex << "  chassis 0x" << chassisId << endl;
}

void LinkLayerDiscovery::reset()
{
	for (auto& pPort : pLldpPorts)              // For each Aggregation Port:
	{
		pPort->reset();

		if (SimLog::Debug > 12)
		{
			LldpPort& port = *pPort;
			SimLog::logFile << "Time " << SimLog::Time << ":  Resetting LLDP "
				<< hex << "  chassis 0x" << chassisId << "  port 0x" << pPort->portId
				<< dec << endl;
		}
		/**/
	}
	
}


void LinkLayerDiscovery::timerTick()
{
	if (!suspended)
	{
		for (auto& pPort : pLldpPorts)              // For each Aggregation Port:
		{
			pPort->timerTick();

			if ((SimLog::Debug > 12) && (SimLog::Time < 0))
			{
				LldpPort& port = *pPort;
				SimLog::logFile << "Time " << SimLog::Time << ":  LLDP timer tick"
					<< hex << "  chassis 0x" << chassisId << "  port 0x" << pPort->portId
					<< dec << endl;
			}
			/**/
		}

	}
}


void LinkLayerDiscovery::run(bool singleStep)
{
	unsigned short nPorts = (unsigned short)pLldpPorts.size();
	unique_ptr<Frame> pTempFrame = nullptr;

	if (!suspended)       
	{
		/*
		for (auto& pDR : pDistRelays)              // Run each DR that is instantiated
		{
			if (pDR) pDR->run(singleStep);
		}
		/**/
		//TODO:  May want to reorder so do Dist Relay collect and RXSM and DistRelayMachine before Mux machine
		//       and Dist Relay Gw/Agg and TXSM and distribute after Mux and selection.
		//       Goal to have LACP respond to DR_SOLO - DR_PAIRED transitions before update home state and txDRCPDU

		// Receive data path
		for (unsigned short i = 0; i < nPorts; i++)              // For each LLDP Port:
		{

			if ((SimLog::Debug > 12) && (SimLog::Time < 0))
			{
				LldpPort& port = *pLldpPorts[i];
				SimLog::logFile << "Time " << SimLog::Time << ":  LLDP checking receive path"
					<< hex << "  chassis 0x" << chassisId << "  port 0x" << port.portId
					<< dec << endl;
			}
			/**/
			pTempFrame = move(pLldpPorts[i]->pIss->Indication());   // Get ingress frame, if available, from ISS
			if (pTempFrame)                                        // If there is an ingress frame
			{
				if (SimLog::Debug > 5)
				{
					/**/
					SimLog::logFile << "Time " << SimLog::Time << ":  LLDP in Device:Port " << hex << pLldpPorts[i]->chassisId
						<< ":" << pLldpPorts[i]->portId << " receiving frame ";
					pTempFrame->PrintFrameHeader();
					SimLog::logFile << dec << endl;
					/**/
				}
				if ((pTempFrame->MacDA == pLldpPorts[i]->lldpDestinationAddress) &&   // if frame has proper DA and contains an LLDPDU
					(pTempFrame->getNextEtherType() == LldpEthertype))
				{
					pLldpPorts[i]->pRxLldpFrame = move(pTempFrame);                   // then pass to LLDP RxSM
				}
				else                                                                  
				{
					pLldpPorts[i]->indications.push(move(pTempFrame));                // else pass it up the stack
				}
			}
		}

		// Check for administrative changes to Aggregator configuration
//		LinkAgg::LacpSelection::adminAggregatorUpdate(pAggPorts, pAggregators);

		// Run state machines
		int transitions = 0;
		for (unsigned short i = 0; i < nPorts; i++)              // For each LLDP Port:
		{

			if ((SimLog::Debug > 12) && (SimLog::Time < 0))
			{
				LldpPort& port = *pLldpPorts[i];
				SimLog::logFile << "Time " << SimLog::Time << ":  LLDP running receive machines"
					<< hex << "  chassis 0x" << chassisId << "  port 0x" << port.portId
					<< dec << endl;
			}
			/**/
//			transitions += AggPort::LacpRxSM::runRxSM(*pAggPorts[i], true);
//			transitions += AggPort::LacpMuxSM::runMuxSM(*pAggPorts[i], true);
			transitions += LldpPort::LldpRxSM::run(*pLldpPorts[i], true);
//			transitions += AggPort::LacpMuxSM::run(*pAggPorts[i], true);
		}
//		updateAggregatorStatus();
//		runCSDC();

		//TODO:  Can you aggregate ports with different LACP versions?  Do you need to test for it?  Does partner LACP version need to be in DRCPDU?
//		LinkAgg::LacpSelection::runSelection(pAggPorts, pAggregators);

		for (unsigned short i = 0; i < nPorts; i++)              // For each LLDP Port:
		{
			/*
//			transitions += AggPort::LacpPeriodicSM::runPeriodicSM(*pAggPorts[i], true);
			if (pAggPorts[i]->actorLacpVersion == 1)  // If actor is v1 then run Periodic state machine
				transitions += AggPort::LacpPeriodicSM::run(*pAggPorts[i], true);

			if ((transitions && (SimLog::Debug > 8)) || (SimLog::Debug > 12))
			{
				SimLog::logFile << "Time " << SimLog::Time << ":  Sys " << hex << pAggPorts[i]->get_aAggActorSystemID()
					<< ":  Port " << hex << pAggPorts[i]->get_aAggPortActorPort() << dec
					<< ": RxSM " << pAggPorts[i]->RxSmState << " MuxSM " << pAggPorts[i]->MuxSmState
					<< " PerSM " << pAggPorts[i]->PerSmState << " TxSM " << pAggPorts[i]->TxSmState
					<< " PerT " << pAggPorts[i]->periodicTimer << " RxT " << pAggPorts[i]->currentWhileTimer
					<< " WtrT " << pAggPorts[i]->waitToRestoreTimer
					<< " Up " << pAggPorts[i]->pIss->getOperational()
					<< " Sel " << pAggPorts[i]->portSelected
					<< " Dist " << (bool)(pAggPorts[i]->actorOperPortState.distributing)
					<< " NTT " << pAggPorts[i]->NTT
					<< endl;

			}
			/**/

			if ((SimLog::Debug > 12) && (SimLog::Time < 0))
			{
				LldpPort& port = *pLldpPorts[i];
				SimLog::logFile << "Time " << SimLog::Time << ":  LLDP running transmit machines"
					<< hex << "  chassis 0x" << chassisId << "  port 0x" << port.portId
					<< dec << endl;
			}
			/**/
//			transitions += AggPort::LacpTxSM::runTxSM(*pAggPorts[i], true);
			transitions += LldpPort::LldpTxTimerSM::run(*pLldpPorts[i], true);
			transitions += LldpPort::LldpTxSM::run(*pLldpPorts[i], true);
		}

		// Transmit data path
		
		for (unsigned short i = 0; i < nPorts; i++)              // For each LLDP Port:
		{

			if ((SimLog::Debug > 12) && (SimLog::Time < 0))
			{
				LldpPort& port = *pLldpPorts[i];
				SimLog::logFile << "Time " << SimLog::Time << ":  LLDP checking transmit path"
					<< hex << "  chassis 0x" << chassisId << "  port 0x" << port.portId
					<< dec << endl;
			}
			/**/
			if (pLldpPorts[i]->getEnabled() && !pLldpPorts[i]->requests.empty())  // If there is an egress frame at the LLDP Port ISS
			{
				pTempFrame = move(pLldpPorts[i]->requests.front());   // Get egress frame
				pLldpPorts[i]->requests.pop();
				if (pTempFrame)                                         // Shouldn't be necessary since already tested that there was a frame
				{
					pLldpPorts[i]->pIss->Request(move(pTempFrame));     //    Send frame down the stack
				}

			}
			/*
			if (pAggregators[i]->getEnabled() && !pAggregators[i]->requests.empty())  // If there is an egress frame at Aggregator
			{
				pTempFrame = move(pAggregators[i]->requests.front());   // Get egress frame
				pAggregators[i]->requests.pop();
				if (pTempFrame)                                         // Shouldn't be necessary since already tested that there was a frame
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << pAggregators[i]->actorAdminSystem.addrMid
							<< ":" << pAggregators[i]->aggregatorIdentifier << " transmitting frame ";
						pTempFrame->PrintFrameHeader();
						SimLog::logFile << dec << endl;
					}
					shared_ptr<AggPort> pEgressPort = distributeFrame(*pAggregators[i], *pTempFrame);   // get egress Aggregation Port
					if (pEgressPort)
					{
						pEgressPort->pIss->Request(move(pTempFrame));     //    Send frame through egress Aggregation Port
					}
				}
			}
			/**/
		}
		


	}
}

/*
bool LinkAgg::configDistRelay(unsigned short distRelayIndex, unsigned short numAggPorts, unsigned short numIrp, 
	sysId adminDrniId, unsigned short adminDrniKey, unsigned short firstLinkNumber)
{
	cout << "  Configuring Distributed Relay Aggregator  . . .";
	SimLog::logFile << "  Configuring Distributed Relay Aggregator  . . .";

	shared_ptr<DistributedRelay> pDR = pDistRelays[distRelayIndex];
	bool success = (((distRelayIndex + numAggPorts + numIrp) <= (unsigned short)pDistRelays.size()) &&
		(numAggPorts > 1) && (numIrp > 1) && pDR);   // make sure there are enough ports to avoid illegal vector index 
	if (success)
	{
		DistributedRelay& DR = *pDistRelays[distRelayIndex];

		pDR->pAggregator = pAggregators[distRelayIndex];
		pDR->pIss = pAggregators[distRelayIndex + numAggPorts];

		unsigned short aggKey = pAggregators[distRelayIndex]->get_aAggActorAdminKey();
		unsigned short irpKey = aggKey + numAggPorts;    // Put IRP Port # in Key to make unique in DRNI System

		std::array<unsigned char, 16> nullDigest;
		nullDigest.fill(0);

		for (int px = distRelayIndex; px < (distRelayIndex + numAggPorts + numIrp); px++)
		{
			if (px == distRelayIndex)                         // config DRNI Aggregator
			{
//				pAggregators[distRelayIndex]->set_aAggActorSystemPriority(drniAggId.pri);
//				pAggregators[distRelayIndex]->set_aAggActorSystemID(drniAggId.addr);
//				pAggregators[distRelayIndex]->set_aAggActorAdminKey(aggKey);
				pAggregators[distRelayIndex]->operDrniId = adminDrniId;
				pAggregators[distRelayIndex]->operDrniKey = adminDrniKey;

				pAggregators[distRelayIndex]->set_convLinkMap(Aggregator::convLinkMaps::EIGHT_LINK_SPREAD);
				pAggregators[distRelayIndex]->set_actorAdminConvLinkDigest(convLinkMapEightLinkSpreadDigest);
				pAggregators[distRelayIndex]->set_aAggPortAlgorithm(LagAlgorithms::C_VID);
//				pAggregators[distRelayIndex]->set_aAggConvServiceDigest(nullDigest);
			}
			else if (px == (distRelayIndex + numAggPorts))     // config IRP Aggregator
			{
				pAggregators[distRelayIndex + numAggPorts]->set_aAggActorAdminKey(irpKey);
			}
			else                                              // disable unused Aggregators
			{
				pAggregators[px]->setEnabled(false);
				pAggregators[px]->set_aAggActorAdminKey(unusedAggregatorKey);
			}

			if (px < (distRelayIndex + numAggPorts))          // config DRNI Aggregation Ports
			{
				pAggPorts[px]->actorAdminSystem = pAggregators[distRelayIndex]->actorAdminSystem;
				pAggPorts[px]->actorAdminPortKey = pAggregators[distRelayIndex]->actorAdminAggregatorKey;

				pAggPorts[px]->set_aAggPortLinkNumber(firstLinkNumber + px - distRelayIndex);;  // Sequentially number DRNI links
			}
			else                                              // config Intra-Relay Aggregation Ports
			{
				pAggPorts[px]->set_aAggPortActorAdminKey(irpKey);
				pAggPorts[px]->actorAdminSystem = pAggregators[distRelayIndex + numAggPorts]->actorAdminSystem;
			}
		}

		cout << "  successfully completed." << endl;
		SimLog::logFile << "  successfully completed." << endl;
	}
	else
	{
		cout << "  unsuccessful." << endl;
		SimLog::logFile << "  unsuccessful." << endl;
	}
	
	return(success);
}
/**/



/**********
           
		    Conversation-Sensitive Collection and Distribution (CSCD) Routines

/*

void LinkAgg::resetCSDC()
{
	for (auto pAgg : pAggregators)            // May want to wrap this in a while loop and repeat until no flags set
	{
		Aggregator& agg = *pAgg;

		agg.partnerPortAlgorithm = LagAlgorithms::UNSPECIFIED;
		agg.partnerConvLinkDigest.fill(0);
		agg.partnerConvServiceDigest.fill(0);

		agg.differentPortAlgorithms = false;
		agg.differentConvServiceDigests = false;
		agg.differentConvLinkDigests = false;

		agg.operDiscardWrongConversation = (agg.adminDiscardWrongConversation == adminValues::FORCE_TRUE);

		agg.activeLagLinks.clear();
		agg.conversationLinkVector.fill(0);
		agg.conversationPortVector.fill(0);

		agg.changeActorDistAlg = false;
		agg.changeConvLinkMap = false;
		agg.changeDistAlg = false;
		agg.changeLinkState = false;
		agg.changeAggregationLinks = false;
		agg.changeCSDC = false;
	}
	//TODO: verify that some per-port machine resets all conversation masks, link number, local copies of dwc.
}

void LinkAgg::runCSDC()
{
	for (auto pPort : pAggPorts)           // Walk through all Aggregation Ports
	{
		updatePartnerDistributionAlgorithm(*pPort);
	}
	for (auto pAgg : pAggregators)            // May want to wrap this in a while loop and repeat until no flags set
	{
		updateMask(*pAgg);
	}
}

void LinkAgg::updateMask(Aggregator& agg)
{
	int loop = 0;
	do
	{
		if (agg.changeActorDistAlg)
		{
			debugUpdateMask(agg, "updateActorDistributionAlgorithm");
			agg.changeActorDistAlg = false;
			updateActorDistributionAlgorithm(agg);
			agg.updateDistRelayAggState = true;
		}
		else if (agg.changeDistAlg)
		{
			debugUpdateMask(agg, "compareDistributionAlgorithms");
			agg.changeDistAlg = false;
			compareDistributionAlgorithms(agg);
			agg.updateDistRelayAggState = true;
		}
		else if (agg.changeLinkState)
		{
			debugUpdateMask(agg, "updateActiveLinks");
			agg.changeLinkState = false;
			updateActiveLinks(agg);
		}
		else if (agg.changeAggregationLinks)
		{
			debugUpdateMask(agg, "updateConversationPortVector");
			agg.changeAggregationLinks = false;
			updateConversationPortVector(agg);
			agg.updateDistRelayAggState = true;
		}
		else if (agg.changeCSDC)
		{
			debugUpdateMask(agg, "updateConversationMasks");
			agg.changeCSDC = false;
			updateConversationMasks(agg);
		}
		else if (agg.changeDistributing)
		{
			debugUpdateMask(agg, "updateAggregatorOperational");
			agg.changeDistributing = false;
			updateAggregatorOperational(agg);
		}

		loop++;
	} while (loop < 10);

}

void LinkAgg::debugUpdateMask(Aggregator& thisAgg, std::string routine)
{
	if (SimLog::Debug > 7)
	{
		SimLog::logFile << "Time " << SimLog::Time
			<< ":   Device:Aggregator " << hex << thisAgg.actorAdminSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
			<< " entering updateMask = " << routine
			<< dec << endl;
	}
}

void LinkAgg::updatePartnerDistributionAlgorithm(AggPort& port)
{
	Aggregator& agg = *(pAggregators[port.actorPortAggregatorIndex]);

	agg.changeDistributing |= port.changeActorDistributing;
	port.changeActorDistributing = false;

	agg.changeLinkState |= port.changePortLinkState;
	port.changePortLinkState = false;

	//  If the partner distribution algorithm parameters have changed for any port that is COLLECTING on an Aggregator,
	//     update the Aggregator's distribution algorithm parameters.
	//     Assumes that the portOper... distribution algorithm parameters are null for any port that expects the 
	//     Aggregator to use the default values (including port that is DEFAULTED, is version 1 (?), 
	//     or did not receive the proper V2 TLVs from the partner.

	if ((SimLog::Debug > 7) && port.changePartnerOperDistAlg)
	{
		SimLog::logFile << "Time " << SimLog::Time
			<< ":   Device:Aggregator " << hex << ":  *** Port " << port.actorAdminSystem.addrMid << ":" << port.actorPort.num
			<< " entering updateMask = updatePartnerDistributionAlgorithm"
			<< dec << endl;
	}

	if (port.actorOperPortState.collecting && (port.portSelected == AggPort::selectedVals::SELECTED))
		// Need to test partner.sync and/or portOperational as well?
	{
		if ((port.changePartnerOperDistAlg) &&
			(port.partnerOperPortAlgorithm == LagAlgorithms::NONE))
			// partnerOperPortAlgorithm will only be NONE if no LACPDUv2 exchange with partner, so use default values.
			//TODO:  so why would it ever be NONE?  Why wouldn't it still have initialized value of UNSPECIFIED?
		{
			agg.partnerPortAlgorithm = LagAlgorithms::UNSPECIFIED;
			agg.partnerConvLinkDigest.fill(0);
			agg.partnerConvServiceDigest.fill(0);
			agg.changeDistAlg = true;
			// Could optimize further processing by only setting change flag if new values differ from old values.
			if ((SimLog::Debug > 3))
			{
				SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorAdminSystem.addrMid << ":" << port.actorPort.num
					<< " setting default partner algorithm on Aggregator " << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
			}
		}
		else if (port.changePartnerOperDistAlg)
		{
			agg.partnerPortAlgorithm = port.partnerOperPortAlgorithm;
			agg.partnerConvLinkDigest = port.partnerOperConvLinkDigest;
			agg.partnerConvServiceDigest = port.partnerOperConvServiceDigest;
			agg.changeDistAlg = true;
			// Could optimize further processing by only setting change flag if new values differ from old values.
			if ((SimLog::Debug > 3))
			{
				SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorAdminSystem.addrMid << ":" << port.actorPort.num
					<< " setting partner's algorithm on Aggregator " << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
			}
		}
	}
	port.changePartnerOperDistAlg = false;

}

void LinkAgg::updateActorDistributionAlgorithm(Aggregator& thisAgg)
{
	for (auto lagPortIndex : thisAgg.lagPorts)      //   walk list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		port.actorOperPortAlgorithm = thisAgg.actorPortAlgorithm;
		port.actorOperConvLinkDigest = thisAgg.actorConvLinkDigest;
		port.actorOperConvServiceDigest = thisAgg.actorConvServiceDigest;
		if (port.actorOperPortState.sync)   //     If the AggPort is attached to Aggregator
		{
			port.NTT = true;                //         then communicate change to partner
			if (SimLog::Debug > 6)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
					<< ":" << port.actorPort.num << " NTT: Change actor dist algorithm on Link " << dec << port.LinkNumber
					<< hex << "  Device:Aggregator " << thisAgg.actorAdminSystem.addrMid << ":" << port.actorPortAggregatorIdentifier
					<< dec << endl;
			}
		}
		if (port.actorOperPortState.collecting && (port.portSelected == AggPort::selectedVals::SELECTED))
		{
			thisAgg.changeDistAlg |= true;
			thisAgg.changeAggregationLinks |= thisAgg.changeConvLinkMap;
		}
		thisAgg.changeConvLinkMap = false;
	}
}

void LinkAgg::compareDistributionAlgorithms(Aggregator& thisAgg)
{
	bool oldDifferPortConvDigest = thisAgg.differentConvLinkDigests;
	bool oldDifferentDistAlg = (thisAgg.differentPortAlgorithms ||
		                        thisAgg.differentConvLinkDigests ||
	                         	thisAgg.differentConvServiceDigests);	

	thisAgg.differentPortAlgorithms = ((thisAgg.actorPortAlgorithm != thisAgg.partnerPortAlgorithm) ||
		(thisAgg.actorPortAlgorithm == LagAlgorithms::UNSPECIFIED) ||
		(thisAgg.partnerPortAlgorithm == LagAlgorithms::UNSPECIFIED));  // actually, testing partner unspecified is redundant
//  temporarily overwriting without check for UNSPECIFIED, since this is the easiest way to make actor and partner match by default
//	thisAgg.differentPortAlgorithms = (thisAgg.actorPortAlgorithm != thisAgg.partnerPortAlgorithm);

	thisAgg.differentConvLinkDigests = (thisAgg.actorConvLinkDigest != thisAgg.partnerConvLinkDigest);
	thisAgg.differentConvServiceDigests = (thisAgg.actorConvServiceDigest != thisAgg.partnerConvServiceDigest);
	bool differentDistAlg = (thisAgg.differentPortAlgorithms || 
		                     thisAgg.differentConvLinkDigests || 
							 thisAgg.differentConvServiceDigests);

	bool partnerMightWin = (thisAgg.actorOperSystem.id >= thisAgg.partnerSystem.id);
	// can't compare Port IDs  here, so set flag if partner System ID <= actor System ID
	thisAgg.changeLinkState |= ((oldDifferentDistAlg != differentDistAlg) && partnerMightWin);
	// standard (currently) says to use differConvLinkDigests, but no harm is using the 'OR' of all differXxx

	bool oldDWC = thisAgg.operDiscardWrongConversation;
	thisAgg.operDiscardWrongConversation = ((thisAgg.adminDiscardWrongConversation == adminValues::FORCE_TRUE) ||
		((thisAgg.adminDiscardWrongConversation == adminValues::AUTO) && !differentDistAlg));
	thisAgg.changeCSDC |= ((oldDWC != thisAgg.operDiscardWrongConversation) && !thisAgg.activeLagLinks.empty());


	//TODO:  report to management if any "differentXXX" flags are set
	//TODO:  Set differentConvLinkDigests if actor and partner Link Numbers don't match 
	//         (are they guaranteed to be stable by the time actor.sync and partner.sync?)
	//         (will changes to actor or partner set the change distribution algorithms flag?)
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time
			<< ":   Device:Aggregator " << hex << thisAgg.actorAdminSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
			<< " differ-Alg/CDigest/SDigest = " << thisAgg.differentPortAlgorithms << "/"
			<< thisAgg.differentConvLinkDigests << "/" << thisAgg.differentConvServiceDigests
			<< "  adminDWC = " << thisAgg.adminDiscardWrongConversation << "  DWC = " << thisAgg.operDiscardWrongConversation
			<< dec << endl;
	}

}

void LinkAgg::updateActiveLinks(Aggregator& thisAgg)
{
	std::list<unsigned short> newActiveLinkList;  // Build a new list of the link numbers active on the LAG
	for (auto lagPortIndex : thisAgg.lagPorts)      //   from list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		bool differentDistAlg = (thisAgg.differentPortAlgorithms ||
			thisAgg.differentConvLinkDigests ||
			thisAgg.differentConvServiceDigests);

		bool partnerWins = ((thisAgg.actorOperSystem.id > thisAgg.partnerSystem.id) ||
			                ((thisAgg.actorOperSystem.id == thisAgg.partnerSystem.id) && (port.actorPort.id > port.partnerOperPort.id)));
			                // Compare Port IDs as well as System IDs, so comparison works with loopback

		if (port.actorOperPortState.collecting && (port.portSelected == AggPort::selectedVals::SELECTED))
		{
			unsigned short oldLinkNumber = port.LinkNumber;
			if (!differentDistAlg && partnerWins)
				port.LinkNumber = port.partnerLinkNumber;
			else
				port.LinkNumber = port.adminLinkNumber;

			newActiveLinkList.push_back(port.LinkNumber);      // Add its Link Number to the new active link list  

			if (oldLinkNumber != port.LinkNumber)
			{
				port.NTT = true;
				if (SimLog::Debug > 6)
				{
					SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
						<< ":" << port.actorPort.num << " NTT: Link Number changing to  " << dec << port.LinkNumber << endl;
				}
			}
		}
	}

	newActiveLinkList.sort();                       // Put list of link numbers in ascending order 
	if (thisAgg.activeLagLinks != newActiveLinkList)  // If the active links have changed
	{
		thisAgg.activeLagLinks = newActiveLinkList;   // Save the new list of active link numbers
		thisAgg.changeAggregationLinks |= true;
		//TODO:  For DRNI have to set a flag to let DRF know that list of active ports may have changed.
	}
}

void LinkAgg::updateConversationPortVector(Aggregator& thisAgg)
{
	if (thisAgg.activeLagLinks.empty())         // If there are no links active on the LAG 
	{
		thisAgg.conversationLinkVector.fill(0);          // Clear all Conversation ID to Link associations; 
		thisAgg.conversationPortVector.fill(0);          // Clear all Conversation ID to Port associations; 
		// Don't need to update conversation masks because disableCollectingDistributing will clear masks when links go inactive
	}
	else                                        // Otherwise there are active links on the LAG
	{
		thisAgg.updateConversationLinkVector(thisAgg.activeLagLinks, thisAgg.conversationLinkVector);           // Create new Conversation ID to Link associations

		std::array<unsigned short, 4096> newConvPortVector;  // Create temporary new CID to Port vector
		newConvPortVector.fill(0);                           // Clear new Conversation ID to Port associations; 
		for (auto lagPortIndex : thisAgg.lagPorts)
		{
			AggPort& port = *(pAggPorts[lagPortIndex]);

			if (port.actorOperPortState.collecting && (port.portSelected == AggPort::selectedVals::SELECTED)
				&& (port.LinkNumber > 0))
			{
				for (unsigned short cid = 0; cid < 4096; cid++)
				{
					if (thisAgg.conversationLinkVector[cid] == port.LinkNumber)
					{
						newConvPortVector[cid] = port.actorPort.num;
					}
				}
			}
		}

		if (thisAgg.conversationPortVector != newConvPortVector)
		{
			thisAgg.conversationPortVector = newConvPortVector;
			thisAgg.changeCSDC = true;
		}
	}

	if (SimLog::Debug > 4)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorAdminSystem.addrMid
			<< ":" << thisAgg.aggregatorIdentifier << dec << " ConvLinkMap = ";
		for (int k = 0; k < 16; k++) SimLog::logFile << "  " << thisAgg.conversationLinkVector[k];
		SimLog::logFile << endl;
	}
}

void LinkAgg::updateConversationMasks(Aggregator& thisAgg)
{
	for (auto lagPortIndex : thisAgg.lagPorts)      //   from list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		port.actorDWC = thisAgg.operDiscardWrongConversation;  // Update port's copy of DWC

		for (int convID = 0; convID < 4096; convID++)   //    for all conversation ID values.
		{
			bool passConvID = (port.actorOperPortState.collecting && (port.portSelected == AggPort::selectedVals::SELECTED) &&
				(thisAgg.conversationPortVector[convID] == port.actorPort.num) && (port.actorPort.num > 0));
				// Determine if link is active and the conversation ID maps to this port number

			port.portOperConversationMask[convID] = passConvID;       // If so then distribute this conversation ID. 
		}

		if (SimLog::Debug > 4)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
				<< ":" << port.actorPort.num << " Link " << dec << port.LinkNumber << "   ConvMask = ";
			for (int k = 0; k < 16; k++) SimLog::logFile << "  " << port.portOperConversationMask[k];
			SimLog::logFile << endl;
		}

		// Turn off distribution and collection if convID is moving between links.
		port.distributionConversationMask = port.distributionConversationMask & port.portOperConversationMask;  
		port.collectionConversationMask = port.collectionConversationMask & port.portOperConversationMask;      
	}

	//  Before just cleared distribution/collection masks for Conversation IDs where the port's link number did not match the conversationLinkVector.
	//     Now want to set the distribution/collection mask for Conversation IDs where the port's link number does match.
	//     This assures "break-before-make" behavior so no Conversation ID bit is ever set in the collection mask for more than one port.
	for (auto lagPortIndex : thisAgg.lagPorts)      //   from list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		port.distributionConversationMask = port.portOperConversationMask;         // Set distribution mask to the new value

		//TODO:  I think I need more qualification here.  Don't want to collectionConversationMask.set() if not collecting (and SELECTED and partner.sync?)
		if (thisAgg.operDiscardWrongConversation)                                  // If enforcing Conversation-sensitive Collection
			port.collectionConversationMask = port.portOperConversationMask;       //    then make collection mask the same as the distribution mask
		else
			port.collectionConversationMask.set();                                 // Otherwise make collection mask true for all conversation IDs


	}
}

void LinkAgg::updateAggregatorOperational(Aggregator& thisAgg)
{
	bool AggregatorUp = false;

	for (auto lagPortIndex : thisAgg.lagPorts)      //   from list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		AggregatorUp |= port.actorOperPortState.distributing;
	}

	if (!AggregatorUp && thisAgg.operational)                    // if Aggregator going down
	{
		thisAgg.operational = false;                // Set Aggregator ISS to not operational
		//TODO:  flush aggregator request queue?

		cout << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorAdminSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
			<< " is DOWN" << dec << endl;
		if (SimLog::Time > 0)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorAdminSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
				<< " is DOWN" << dec << endl;
		}
	}
	if (AggregatorUp && !thisAgg.operational)                    // if Aggregator going down
	{
		thisAgg.operational = true;                 // Set Aggregator ISS is operational

		cout << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorAdminSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
			<< " is UP with Link numbers:  " << dec;
		for (auto link : thisAgg.activeLagLinks)
			cout << link << "  ";
		cout << dec << endl;
		if (SimLog::Time > 0)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorAdminSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
				<< " is UP with Link numbers:  " << dec;
			for (auto link : thisAgg.activeLagLinks)
				SimLog::logFile << link << "  ";
			SimLog::logFile << dec << endl;
		}
	}
}

/*
//void LinkAgg::updateConversationLinkVector(Aggregator& thisAgg, std::list<unsigned short>& links, std::array<unsigned short, 4096>& convLinkVector)
void Aggregator::updateConversationLinkVector(std::list<unsigned short>& links, std::array<unsigned short, 4096>& convLinkVector)
{
	switch (selectedconvLinkMap)
	{
	case Aggregator::convLinkMaps::ADMIN_TABLE:
		linkMap_AdminTable(links, convLinkVector);
		break;
	case Aggregator::convLinkMaps::ACTIVE_STANDBY:
		linkMap_ActiveStandby(links, convLinkVector);
		break;
	case Aggregator::convLinkMaps::EVEN_ODD:
		linkMap_EvenOdd(links, convLinkVector);
		break;
	case Aggregator::convLinkMaps::EIGHT_LINK_SPREAD:
		linkMap_EightLinkSpread(links, convLinkVector);
		break;
	}
}

//void LinkAgg::linkMap_EightLinkSpread(Aggregator& thisAgg, std::list<unsigned short>& links, std::array<unsigned short, 4096>& convLinkVector)
void Aggregator::linkMap_EightLinkSpread(std::list<unsigned short>& links, std::array<unsigned short, 4096>& convLinkVector)
{
	const int nLinks = 8;  // Must be a power of two
	const int nRows = 8;   // Must be a power of two
	const unsigned short convLinkPriorityList[nRows][nLinks] =
	{
		{ 0, 3, 6, 5, 1, 2, 7, 4 },
		{ 1, 2, 7, 4, 0, 3, 6, 5 },
		{ 2, 5, 0, 7, 3, 4, 1, 6 },
		{ 3, 4, 1, 6, 2, 5, 0, 7 },
		{ 4, 7, 2, 1, 5, 6, 3, 0 },
		{ 5, 6, 3, 0, 4, 7, 2, 1 },
		{ 6, 1, 4, 3, 7, 0, 5, 2 },
		{ 7, 0, 5, 2, 6, 1, 4, 3 }
	};

	std::array<unsigned short, nLinks> activeLinkNumbers = { 0 };

	// First, put the Aggregator's activeLagLinks list to an array, nlinks long, where the position in the array
	//   is determined by the active Link Number modulo nLinks.  If multiple active Link Numbers map to the same
	//   position in the array, the lowest of those Link Numbers will be stored in the array.  If no active Link
	//   Numbers map to a position in the array then the value at that position will be 0.
	for (auto& link : links)
	{
		unsigned short baseLink = link % nLinks;
		if ((activeLinkNumbers[baseLink] == 0) || (link < activeLinkNumbers[baseLink]))
			activeLinkNumbers[baseLink] = link;
	}


	// Then, create the first 8 entries in the conversation-to-link map from the first 8 rows of the Link Priority List table
	for (int row = 0; row < nRows; row++)
	{
		convLinkVector[row] = 0;                                       // Link Number 0 is default if no Link Numbers in row are active
		for (int j = 0; j < nLinks; j++)                                            // Search a row of the table
		{                                                                           //   for the first Link Number that is active.
			if ((convLinkPriorityList[row][j] < activeLinkNumbers.size()) && activeLinkNumbers[convLinkPriorityList[row][j]])
			{
				convLinkVector[row] = activeLinkNumbers[convLinkPriorityList[row][j]];    //   Copy that Link Number into map
				break;                                                              //       and quit the for loop.
			}
		}
	}

	// Finally, repeat first 8 entries through the rest of the conversation-to-link map
	for (int row = nRows; row < 4096; row++)
	{
		convLinkVector[row] = convLinkVector[row % nRows];
	}
}
/*

//void LinkAgg::linkMap_EvenOdd(Aggregator& thisAgg, std::list<unsigned short>& links, std::array<unsigned short, 4096>& convLinkVector)
void Aggregator::linkMap_EvenOdd(std::list<unsigned short>& links, std::array<unsigned short, 4096>& convLinkVector)
{
	if (links.empty())              // If there are no active links
	{
		convLinkVector.fill(0);         //    then the conversationLinkVector is all zero
	} 
	else                                             // Otherwise ...
	{
		unsigned short evenLink = links.front();    // Even Conversation IDs go to lowest LinkNumber.
		unsigned short oddLink = links.back();      // Odd Conversation IDs go to highest LinkNumber.  (Other links are standby)
		for (int j = 0; j < 4096; j += 2)
		{
			convLinkVector[j] = evenLink;
			convLinkVector[j + 1] = oddLink;
		}
	}
}

//void LinkAgg::linkMap_ActiveStandby(Aggregator& thisAgg, std::list<unsigned short>& links, std::array<unsigned short, 4096>& convLinkVector)
void Aggregator::linkMap_ActiveStandby(std::list<unsigned short>& links, std::array<unsigned short, 4096>& convLinkVector)
{
	if (links.empty())              // If there are no active links
	{
		convLinkVector.fill(0);         //    then the conversationLinkVector is all zero
	}
	else                                             // Otherwise ...
	{
		// Active link will be the lowest LinkNumber.  All others will be Standby.
		unsigned short activeLink = links.front();
		convLinkVector.fill(activeLink);
	}
}

//void LinkAgg::linkMap_AdminTable(Aggregator& thisAgg, std::list<unsigned short>& links, std::array<unsigned short, 4096>& convLinkVector)
void Aggregator::linkMap_AdminTable(std::list<unsigned short>& links, std::array<unsigned short, 4096>& convLinkVector)
{
	convLinkVector.fill(0);                           // Start with  conversationLinkVector is all zero
	for (const auto& entry : adminConvLinkMap)         // For each entry in the administered adminConvLinkMap table (std::map)
	{
		unsigned short convID = entry.first;                       //    Get the conversation ID.
		for (const auto& link : entry.second)                      //    Walk the prioritized list of desired links for that conversation ID.
		{
			if (isInList(link, links))            //    If the desired link is active on the Aggregator
			{
				convLinkVector[convID] = link;        //        then put that link number in the conversationLinkVector
				break;
			}
		}
	}
}

// Is there really no standard library routine to test if a value is in a list?
//bool LinkAgg::isInList(unsigned short val, const std::list<unsigned short>& thisList)
bool Aggregator::isInList(unsigned short val, const std::list<unsigned short>& thisList)
{
	bool inList = false;
	for (auto& listVal : thisList)
	{
		if (listVal == val)
		{
			inList = true;
			break;
		}
	}
	return (inList);
}
/*


bool LinkAgg::collectFrame(AggPort& thisPort, Frame& thisFrame)  // const??
{
	bool collect = (thisPort.actorOperPortState.distributing);   // always return false if port not distributing (collecting ??)
	unsigned short convID = frameConvID(thisPort.actorOperPortAlgorithm, thisFrame);

	if (collect && thisPort.actorDWC)                            // if collecting but Discard Wrong Conversation is set
	{
		collect = thisPort.collectionConversationMask[convID];   // then verify this Conversation ID can be received through this AggPort
	}
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << thisPort.actorAdminSystem.addrMid
			<< ":" << thisPort.actorPort.num << " Link " << dec << thisPort.LinkNumber << "  DWC " << thisPort.actorDWC << hex;
		if (collect)
		{
			SimLog::logFile << " collecting ConvID " << convID << dec << endl;
		}
		else
		{
			SimLog::logFile << " discarding ingress ConvID " << convID << dec << endl;
		}
	}

	return (collect);
}


shared_ptr<AggPort> LinkAgg::distributeFrame(Aggregator& thisAgg, Frame& thisFrame)
{
	shared_ptr<AggPort> pEgressPort = nullptr;
	unsigned short convID = frameConvID(thisAgg.actorPortAlgorithm, thisFrame);

	// Checking conversation mask of each port on lag is inefficient for this implementation, but ensures will  
	//   not send a frame during a transient when the conversation ID of the frame is moving between ports. 
	//   In this simulation will never be attempting to transmit during such a transient, 
	//   but if distribution was handled by a different thread (or in hardware) it could happen.
	// It would also be possible to create an array of port IDs (or port pointers) per Conversation ID 
	//   that was set to null when updating conversation masks and set to appropriate port when all masks updated.
	//   This would be more efficient for this simulation but not done since not as described in standard.
	for (auto lagPortIndex : thisAgg.lagPorts)
	{
		if (pAggPorts[lagPortIndex]->portOperConversationMask[convID])    // will be false if port not distributing or inappropriate port for this convID
		{
			pEgressPort = pAggPorts[lagPortIndex];
		}
	}
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorAdminSystem.addrMid
			<< ":" << thisAgg.aggregatorIdentifier;

		if (pEgressPort)
		{
			SimLog::logFile << " distributing ConvID " << convID 
				<< "  to Port " << pEgressPort->actorAdminSystem.addrMid << ":" << pEgressPort->actorPort.num
				<< " Link " << dec << pEgressPort->LinkNumber << dec << endl;
		}
		else
		{
			SimLog::logFile << " discarding egress ConvID " << convID << dec << endl;
		}
	}

	return (pEgressPort);
}


unsigned short LinkAgg::frameConvID(LagAlgorithms algorithm, Frame& thisFrame)  // const??
{
	unsigned short convID = 0;

	switch (algorithm)
	{
	case LagAlgorithms::C_VID:
		if (thisFrame.getNextEtherType() == CVlanEthertype)
		{
//			convID = ((VlanTag&)(thisFrame.getNextSdu())).Vtag.id;
			convID = (VlanTag::getVlanTag(thisFrame)).Vtag.id;
			
		}
		break;
	case LagAlgorithms::S_VID:
		if (thisFrame.getNextEtherType() == SVlanEthertype)
		{
//			convID = ((VlanTag&)(thisFrame.getNextSdu())).Vtag.id;
			convID = (VlanTag::getVlanTag(thisFrame)).Vtag.id;
		}
		break;
	default:  // includes 	LagAlgorithms::UNSPECIFIED
		convID = macAddrHash(thisFrame);
		break;
	}
	return (convID);
}

/*
 *  macAddrHash calculates 12-bit 1's complement sum of the DA and SA fields of the input frame
/*
unsigned short LinkAgg::macAddrHash(Frame& thisFrame)
{
	unsigned long long hash = 0;
	addr12 tempAddr;                         // addr12 allows treating a 48-bit MAC address as four 12-bit values

	tempAddr.all = thisFrame.MacDA;          // Calculate sum of 12-bit chunks of DA and SA fields.
	hash += tempAddr.high;
	hash += tempAddr.hiMid;
	hash += tempAddr.loMid;
	hash += tempAddr.low;
	tempAddr.all = thisFrame.MacSA;
	hash += tempAddr.high;
	hash += tempAddr.hiMid;
	hash += tempAddr.loMid;
	hash += tempAddr.low;
	hash = (hash & 0x0fff) + (hash >> 12);   // Roll overflow back into sum
	if (hash > 0x0fff) hash++;
	hash = (~hash) & 0x0fff;                 // Take 1's complement


	return ((unsigned short)hash);
}
/**/


