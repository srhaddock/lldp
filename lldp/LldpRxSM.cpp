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
#include "LldpPort.h"


void LldpPort::LldpRxSM::reset(LldpPort& port)
{
//	port.currentWhileTimer = 0;
	port.RxSmState = enterWaitOperational(port);
	port.pRxLldpFrame = nullptr;
}

/**/
void LldpPort::LldpRxSM::timerTick(LldpPort& dr)
{
//	if (dr.currentWhileTimer > 0) dr.currentWhileTimer--;
}
/**/

int  LldpPort::LldpRxSM::run(LldpPort& dr, bool singleStep)
{
	bool transitionTaken = false;
	int loop = 0;

	do
	{
		transitionTaken = step(dr);
		loop++;
	} while (!singleStep && transitionTaken && loop < 10);
	if (!transitionTaken) loop--;
	return (loop);
}

bool LldpPort::LldpRxSM::step(LldpPort& port)
{
	bool transitionTaken = false;
	RxSmStates nextRxSmState = NO_STATE;

	bool globalTransition = !port.rxInfoAge && !(port.pIss && port.pIss->getOperational());

	if (globalTransition && (port.RxSmState != RxSmStates::WAIT_OPERATIONAL))
		nextRxSmState = enterWaitOperational(port);         // Global transition, but only take if will change something
	else switch (port.RxSmState)
	{
	case RxSmStates::WAIT_OPERATIONAL:
		if (!globalTransition && port.rxInfoAge) nextRxSmState = enterDeleteAgedInfo(port);
		else if (!globalTransition) nextRxSmState = enterRxInitialize(port);
		break;
	case RxSmStates::RX_INITIALIZE:
		if ((port.adminStatus == ENABLED_RX_TX) || (port.adminStatus == ENABLED_RX_ONLY) || port.sentManifest)
			nextRxSmState = enterRxWaitFrame(port);
		break;
	case RxSmStates::RX_WAIT_FRAME:
		rxCheckTimers(port);
if (!port.sentManifest && (port.adminStatus == DISABLED) || (port.adminStatus == ENABLED_TX_ONLY))
nextRxSmState = enterRxInitialize(port);
else if (port.pRxLldpFrame)
nextRxSmState = enterRxFrame(port);
break;
// Don't have case statements for DELETE_AGED_INFO, RX_FRAME, 
//	  RX_EXTENDED, DELETE_INFO, UPDATE_INFO, REMOTE_CHANGES, or RX_XPDU_REQUEST 
//    because these states are executed and fall through to 
//    RX_INITIALIZE or WAIT_OPERATIONAL within a single call to stepLldpRxSM().
	default:
		nextRxSmState = enterWaitOperational(port);
		break;
	}

	if (nextRxSmState != RxSmStates::NO_STATE)
	{
		port.RxSmState = nextRxSmState;
		transitionTaken = true;
	}
	else {}   // no change to RxSmState (or anything else) 

	port.pRxLldpFrame = nullptr;      // Done with received frame, if any

	/**/
	return (transitionTaken);
}

LldpPort::LldpRxSM::RxSmStates LldpPort::LldpRxSM::enterWaitOperational(LldpPort& port)
{
	return (RxSmStates::WAIT_OPERATIONAL);
}

LldpPort::LldpRxSM::RxSmStates LldpPort::LldpRxSM::enterDeleteAgedInfo(LldpPort& port)
{
	//TODO:  delete any neighbors that timed out
	//TODO:  set somethingChangedRemote
	port.rxInfoAge = false;

	return (enterWaitOperational(port));
}

LldpPort::LldpRxSM::RxSmStates LldpPort::LldpRxSM::enterRxInitialize(LldpPort& port)
{
	//TODO::  rxInitializeLLDP(port);
	port.pRxLldpFrame = nullptr;

	return (RxSmStates::RX_INITIALIZE);
}

LldpPort::LldpRxSM::RxSmStates LldpPort::LldpRxSM::enterRxWaitFrame(LldpPort& port)
{
	port.rxInfoAge = false;

	return (RxSmStates::RX_WAIT_FRAME);
}

LldpPort::LldpRxSM::RxTypes LldpPort::LldpRxSM::rxProcessFrame(LldpPort& port)
{
	//TODO: rxProcessFrame 
	RxTypes rxType = RxTypes::INVALID;
	Lldpdu& rxLldpdu = (Lldpdu&)(port.pRxLldpFrame->getNextSdu());
	int nborIndex = 0;

	for (auto& tlv : rxLldpdu.tlvs)
	{
		SimLog::logFile << "    Received TLV type = " << (unsigned short)tlv.getType();
		SimLog::logFile << " and length = " << tlv.getLength() << " : ";
		tlv.printBytes();
		if (tlv.getType() == TLVtypes::SYSTEM_NAME)
		{
			SimLog::logFile << "  : ";
			tlv.printString(2);
		}
		SimLog::logFile << endl;
	}


	if (rxLldpdu.tlvs.size() > 2)      // If received LLDPDU has at least 3 TLVs
	{
		if ((rxLldpdu.tlvs[0].getType() == TLVtypes::CHASSIS_ID) &&  // If first 2 TLVs are Chassis ID and Port ID
			(rxLldpdu.tlvs[0].getLength() >= 2) &&                   //    with valid lengths
			(rxLldpdu.tlvs[0].getLength() <= 256) &&
			(rxLldpdu.tlvs[1].getType() == TLVtypes::PORT_ID) &&
			(rxLldpdu.tlvs[1].getLength() >= 2) &&
			(rxLldpdu.tlvs[1].getLength() <= 256))
		{
			if ((rxLldpdu.tlvs[2].getType() == TLVtypes::TTL) &&           // If third TLV is TTL
				(rxLldpdu.tlvs[2].getLength() >= 2))                       //    with valid length
			{
				if ((static_cast<TlvTtl&>(rxLldpdu.tlvs[2])).getTtl())     //    If TTL value is > 0
					rxType = RxTypes::NORMAL;                              //        then Normal LLDPDU
				else
					rxType = RxTypes::SHUTDOWN;                            //    else Shutdown LLDPDU
			}
			if ((rxLldpdu.tlvs[2].getType() == TLVtypes::XID) &&           // If third TLV is Extension Identifier
				(rxLldpdu.tlvs[2].getLength() >= 8) &&                     //    with valid length
				(port.lldpV2Enabled) &&                                    //    and LLDPv2 supported
				(port.lldpScopeAddress == rxLldpdu.tlvs[2].getAddr(2)) &&  //    and matching scope address
				(findNborIndex(port, rxLldpdu.tlvs) < rxLldpdu.tlvs.size()))  //    and from a known neighbor
				// Later will verify Scope and that the chassis ID and Port ID match a known neighbor
			{
				rxType = RxTypes::XPDU;                                    //    then Extension LLDPDU
			}
			if ((rxLldpdu.tlvs[2].getType() == TLVtypes::XREQ) &&          // If third TLV is Extension Request
				(rxLldpdu.tlvs[2].getLength() >= 14) &&                    //    with valid length
				(port.lldpV2Enabled) &&                                    //    and LLDPv2 supported
				(port.lldpScopeAddress == rxLldpdu.tlvs[2].getAddr(8)) &&  //    and matching scope address
				(rxLldpdu.tlvs[0] == port.localMIB.chassisID) &&           //    and Chassis ID and Port ID match 
				(rxLldpdu.tlvs[1] == port.localMIB.portID))                //        this LLDP agent
				// Later will verify that the chassis ID and Port ID match this LLDP agent
				//TODO:  check returnAddress != 0 ?
			{
				rxType = RxTypes::XREQ;                                    //    then Extension Request LLDPDU
			}
		}
	}

	//TODO: There are some TLV validation rules that result in the entire LLDPDU, not just the TLV, being discarded.
	//    Should these validations be done here?
	if ((rxType != RxTypes::SHUTDOWN) && (rxType != RxTypes::XREQ))
	{
		for (size_t i = 3; i < rxLldpdu.tlvs.size(); i++)           // Walk through TLVs
		{
			unsigned short type = rxLldpdu.tlvs[i].getType();    // Check TLV type
			if (((type == TLVtypes::CHASSIS_ID) ||               // If find out of place
				(type == TLVtypes::PORT_ID) ||                  //    Chassis ID, Port ID, or TTL TLV
				(type == TLVtypes::TTL)) ||
				((port.lldpV2Enabled) &&                         //    or LLDPv2 enabled and find out of place
				 ((type == TLVtypes::XID) ||                     //    XID or XREQ TLV
				  (type == TLVtypes::XREQ) ||
				  ((type == TLVtypes::MANIFEST) && (rxType != RxTypes::NORMAL)))))  // or find 2nd Manifest TLV
			{
				rxType = RxTypes::INVALID;                       // Mark frame as invalid
			}
			else if (type == TLVtypes::END)                      // If termination TLV
			{
				i = rxLldpdu.tlvs.size();                        //    then don't processs any more TLVs
				//TODO::  If were real frame (char string) rather than vector of TLVs would have to check to make sure
				//     sum of TLV lengths not longer than the frame.
			}
			else if (port.lldpV2Enabled && (type == TLVtypes::MANIFEST) && (rxType == RxTypes::NORMAL))  // If Normal LLDPDU has a manifest TLV
			{
				rxType = RxTypes::MANIFEST;                      //    then change LLDPDU type to Manifest
			}
			//TODO:  For all recognized TLV types: check min length and other TLV-specific constraints
			//TODO:  So far this only does checks that would invalidate LLDPDU.  Still need to do checks that invalidate individual TLVs.
		}
	}

	if (rxType == RxTypes::INVALID)          // if type is invalid
	{
		SimLog::logFile << "Time " << SimLog::Time << ":  Received INVALID LLDPDU !!!!!!!!!!!!!!!" << endl;
		//TODO:  increment error counters
	}
	else if ((rxType != XREQ) && (port.adminStatus == adminStatusVals::ENABLED_TX_ONLY))
	{     // if LLDP agent is transmit only, then discard all valid LLDPDUs except XREQ without incrementing error counters
		  //TODO: this currenlty doesn't match standard because won't increment counters for poorly formatted LLDPDUs
		  //     since it doesn't necessarily even implement the rx machine
		rxType = RxTypes::INVALID;  // change type to invalid so LLDPDU will be discarded
	}
	/*
	SimLog::logFile << "Received LLDPDU of type " << (unsigned short)rxType << endl;
	bool matchlocal = (rxLldpdu.tlvs[0] == port.localMIB.chassisID);
	bool matchremote = (rxLldpdu.tlvs[0] == rxLldpdu.tlvs[0]);
	SimLog::logFile << "   Match local Chassis ID is ";
	if (matchlocal) SimLog::logFile << "true ";
	else SimLog::logFile << "false ";
	SimLog::logFile << " and match remote Chassis ID is ";
	if (matchremote) SimLog::logFile << "true ";
	else SimLog::logFile << "false ";
	SimLog::logFile << endl;
	/**/

	return(rxType);
}

LldpPort::LldpRxSM::RxSmStates LldpPort::LldpRxSM::enterRxFrame(LldpPort& port)
{
	Lldpdu& rxLldpdu = (Lldpdu&)(port.pRxLldpFrame->getNextSdu());

	RxTypes rxType = rxProcessFrame(port);
	switch (rxType)
	{
	//TODO: verify if multiple commands within a case-break sequence need to be enclosed in {}
	case RxTypes::SHUTDOWN:
		rxDeleteInfo(port);
		break;
	case RxTypes::XREQ:
		rxXREQ(port, rxLldpdu);
		break;
	case RxTypes::NORMAL:
		rxNormal(port, rxLldpdu);
//		rxUpdateInfo(port);
		break;
	case RxTypes::MANIFEST:
		xRxManifest(port, rxLldpdu);
//		if (xRxManifest(port, rxLldpdu)) rxUpdateInfo(port);
		break;
	case RxTypes::XPDU:
		xRxXPDU(port, rxLldpdu);
//		if (xRxXPDU(port, rxLldpdu)) rxUpdateInfo(port);
		break;
	case RxTypes::INVALID:
	default:
		break;
	}

	return (enterRxWaitFrame(port));
}


/*
bool LldpPort::LldpRxSM::runRxExtended(LldpPort& port)  // returns true if manifest is complete
{
	if (port.rxType == MANIFEST)
		return(enterRxManifest(port));
	else if (port.rxType == XPDU)
		return(enterRxXPDU(port));
	else 
		return(false);
}
/**/

void LldpPort::LldpRxSM::rxCheckTimers(LldpPort& port)
{
	//TODO: check timers for all neighbors
	//TODO:     handle XREQ timeout, calling generateXREQ()
	//TODO:     handle RXTTL timeout, calling rxDeleteInfo()

}

void LldpPort::LldpRxSM::rxNormal(LldpPort& port, Lldpdu& rxLldpdu)
{
	bool nborChanged = false;
	std::vector<TLV>& rxTlvs = rxLldpdu.tlvs;
	unsigned int index = findNborIndex(port, rxTlvs);
	if (index == port.nborMIBs.size())                             // if didn't find nbor
	{
		unsigned long size = 0;                                    // then find size of new neighbor
		for (auto& tlv : rxTlvs)
			size += tlv.getLength();
		if (roomForNewNeighbor(port, size))                        //    and if have room
		{
			createNeighbor(port, rxTlvs, true);                    //    then create new neighbor MIB entry (with tlvs) at index value
			port.nborMIBs[index].ttlTimer = port.nborMIBs[index].rxTtl;     //       and restart timer
			nborChanged = true; 
		}
	}
	else
	{
		map<unsigned char, xpduMapEntry>& nborMap = *(port.nborMIBs[index].pXpduMap);

		if (nborMap.size() != 1)           // if previously received Manifest and XPDU LLDPDUs
		{
			xpduMapEntry normalLldpduMapEntry = nborMap.at(0);   // save old map entry for Normal LLDPDU
			nborMap.clear();                                     // clear out old xpdu entries
			nborMap.insert(make_pair(0, normalLldpduMapEntry));  // restore old map entry for Normal LLDPDU
		}
		bool tlvsMatch = compareTlvs( nborMap.at(0).pTlvs, rxTlvs);
		if (!tlvsMatch)                                                            // if new TLVs don't match old
		{
			nborMap.at(0).pTlvs.clear();                                           //    then clear old TLVs      
			for (unsigned int i = 0; i < (rxTlvs.size() - 3); i++)                 //    and for each new TLV
				nborMap.at(0).pTlvs[i] = make_shared<TLV>(rxTlvs[i + 3]);          //        copy TLV and put pointer in map
			nborChanged = true;                                                    //    Note that something changed 
		}
		port.nborMIBs[index].rxTtl = (static_cast<TlvTtl&>(rxTlvs[2])).getTtl();       //       Save received TTL value
		port.nborMIBs[index].ttlTimer = port.nborMIBs[index].rxTtl;                    //       and restart timer
	}
	
	SimLog::logFile << "    After rxNormal size of nbors is " << port.nborMIBs.size();
	if (port.nborMIBs.size() > 0)
	{
		SimLog::logFile << " and entry " << (unsigned short)port.nborMIBs[0].pXpduMap->begin()->first;
		SimLog::logFile << " has " << port.nborMIBs[0].pXpduMap->begin()->second.pTlvs.size() << " TLVs ";
	}
	SimLog::logFile << " with change = " << nborChanged << endl;

	//TODO: init neighbor specific timers

}

void LldpPort::LldpRxSM::xRxManifest(LldpPort& port, Lldpdu& rxLldpdu) // returns true if manifest is complete
{
	bool manifestComplete = false;

	bool nborChanged = false;
	std::vector<TLV>& rxTlvs = rxLldpdu.tlvs;
	unsigned int manifestTlvIndex = 0;
	for (unsigned int i = 3; ((i < rxTlvs.size()) && (manifestTlvIndex == 0)); i++) // Find manifest TLV
		if (rxTlvs[i].getType() == TLVtypes::MANIFEST)
			manifestTlvIndex = i;
	if (manifestTlvIndex == 0)
	{
		SimLog::logFile << "(" << SimLog::Time << ") in xRxManifest but cannot find manifest TLV" << endl;
	}
	else
	{
		tlvManifest& rxManTLV = static_cast<tlvManifest&>(rxTlvs[manifestTlvIndex]);   // If got here, there must be manifest TLV
		unsigned int index = findNborIndex(port, rxTlvs);
		if (index == port.nborMIBs.size())                             // if didn't find nbor
		{
			unsigned long size = rxManTLV.getTotalSize();              // then find size of new neighbor
			if (roomForNewNeighbor(port, size))                        //    and if have room
			{
				createNeighbor(port, rxTlvs, false);                        //    then create new neighbor MIB entry without tlvs
//				nborChanged = true;  // Don't signal neighbor changed until move new info to current XPDU map
			}
			else
			{
				// TODO:  If no room then discard frame (and increment counters?)
			}
		}
		if (index < port.nborMIBs.size())   // unless discarded frame because no room, will now have index to neighbor MIB entry
		{
			MibEntry& nbor = port.nborMIBs[index];
			nbor.nborAddr = rxManTLV.getReturnAddr();                          // Save return address for XREQs

			SimLog::logFile << "               Saving return address from manifest TLV: " << hex << nbor.nborAddr << dec << endl;

			bool tlvsMatch = compareTlvs(nbor.pXpduMap->at(0).pTlvs, rxTlvs);  // Compare current TLVs for XPDU0, including Manifest TLV, to new LLDPDU
			if (tlvsMatch)                                                     // If they match then no change to neighbor
			{
				nbor.rxTtl = (static_cast<TlvTtl&>(rxTlvs[2])).getTtl();       //       so just save received TTL value
				nbor.ttlTimer = nbor.rxTtl;                                    //       and restart timer
				nbor.pNewXpduMap = nullptr;                                    //       and discard any partially completed manifest
			}
			else                  // If new TLVs are different then need to create a new XPDU Map from the received Manifest TLV
			// TODO: Could optimize(?) my comparing received Manifest TLV to current.  If the same then only XPDU0 tlvs changed so could update
			//          nbor without creating entire new XPDU map just to discover all other XPDUs are current.
			// TODO: Could optimize case where received Manifest LLDPDU exactly matches partially completed Manifest LLDPDU.  If the same then 
			//          only have to change status requested and retried to update, but not create entire new map.
			// TODO: Somewhere handle case of Manifest TLV with no XPDU Descriptors
			// TODO:  handle case where just enabled LLDPV2 so have manifest TLV in XPDU 0 but no other manifest entries
			{
				shared_ptr<map<unsigned char, xpduMapEntry>> pManXpduMap = make_shared<map<unsigned char, xpduMapEntry>>(); 
				xpduMapEntry newNborMapEntry;                        // Create xpdu map entry for Normal LLDPDU
				newNborMapEntry.sizeXpduTlvs = 6 + nbor.chassisID.getLength() + nbor.portID.getLength(); // in this entry include first 3 tlv lengths
				//TODO:  Think the cumulative size of TLVs is really only needed on localMIB (for transmitting a manifest)
				newNborMapEntry.xpduDesc = xpduDescriptor(0, 0, 0);
				if (rxTlvs.size() > 3)                // If have more than first three mandatory TLVs to copy (which better be true since have manifest TLV
					for (size_t i = 3; i < rxTlvs.size(); i++)
					{
						newNborMapEntry.sizeXpduTlvs += (2 + rxTlvs[i].getLength());   // update cumulative size of tlvs
						newNborMapEntry.pTlvs.push_back(make_shared<TLV>(rxTlvs[i]));  // copy TLVs and put pointers in map
					}
				pManXpduMap->insert(make_pair(0, newNborMapEntry));  // Put xpdu map entry in map with key=0

				for (unsigned int i = 0; i < rxManTLV.getNumXpdus(); i++)              // For each xpdu descriptor in manifest TLV
				{ 
					newNborMapEntry.sizeXpduTlvs = 0;                                  //     Create a xpdu map entry with zero size
					newNborMapEntry.xpduDesc = rxManTLV.getXpduDescriptor(i);          //     Copy the xpdu descriptor from the manifest TLV
					newNborMapEntry.status = RxXpduStatus::UPDATE;                     //     Assume need to request XPDU in Extension LLDPDU
					newNborMapEntry.pTlvs.clear();                                     //     No TLVs yet
					auto oldXpdu = nbor.pXpduMap->find(newNborMapEntry.xpduDesc.num);  //     Search current XPDU map for this XPDU number
					if ((oldXpdu != nbor.pXpduMap->end()) && 
						(oldXpdu->second.xpduDesc == newNborMapEntry.xpduDesc))        //     If found and descriptors match
					{
						newNborMapEntry.pTlvs = oldXpdu->second.pTlvs;                 //          then copy pointers to tlvs in current XPDU map to new map
						newNborMapEntry.sizeXpduTlvs = oldXpdu->second.sizeXpduTlvs;   //          and copy size
						newNborMapEntry.status = RxXpduStatus::CURRENT;                //          and set status to current so don't request Extension LLDPDU
					}
					else if (nbor.pNewXpduMap)                                         //     If no current XPDU, but have partially completed manifest
					{
						oldXpdu = nbor.pNewXpduMap->find(newNborMapEntry.xpduDesc.num);    //     Search partial XPDU map for this XPDU number
						if ((oldXpdu != nbor.pNewXpduMap->end()) &&
							(oldXpdu->second.xpduDesc == newNborMapEntry.xpduDesc) &&
							(oldXpdu->second.status == RxXpduStatus::NEW))                 //     If found and descriptors match and have new tlvs
						{
							newNborMapEntry.pTlvs = oldXpdu->second.pTlvs;                 //          then copy partial XPDU map tlv pointers to new map entry
							newNborMapEntry.sizeXpduTlvs = oldXpdu->second.sizeXpduTlvs;   //          and copy size
							newNborMapEntry.status = RxXpduStatus::NEW;                    //          and set status to new so don't request Extension LLDPDU
						}
					}
					pManXpduMap->insert(make_pair(newNborMapEntry.xpduDesc.num, newNborMapEntry));  //     Put xpdu map entry in map with key = xpdu number
				}
				nbor.pNewXpduMap = pManXpduMap;          // Store pointer to new XPDU map (overwriting pointer to any partially completed manifest

				manifestComplete = xRxCheckManifest(port, nbor);
			}
		}

		SimLog::logFile << "    After xRxManifest size of nbors is " << port.nborMIBs.size();
		if ((port.nborMIBs.size() > 0) && (port.nborMIBs[0].pNewXpduMap))
		{
			SimLog::logFile << " and first nbor has XPDUs: ";
			for (auto& mapEntry : (*port.nborMIBs[0].pNewXpduMap))
			{
				SimLog::logFile << " xpdu " << (unsigned short)mapEntry.first;
				SimLog::logFile << " has " << mapEntry.second.pTlvs.size() << " TLVs w/ status ";
				SimLog::logFile << " status " << mapEntry.second.status << "; ";
			}
		}
		SimLog::logFile << " manifestComplete = " << manifestComplete << endl;

		//TODO: init neighbor specific timers

	}

	// Store new neighbor MIB entry if have received all XPDUs
	if (manifestComplete) rxUpdateInfo(port);

//	return(manifestComplete);
}

void LldpPort::LldpRxSM::xRxXPDU(LldpPort& port, Lldpdu& rxLldpdu)  // returns true if manifest is complete
{
	bool unexpectedXPDU = true;
	bool manifestComplete = false;

	std::vector<TLV>& rxTlvs = rxLldpdu.tlvs;

	unsigned int index = findNborIndex(port, rxTlvs);
	if ((index != port.nborMIBs.size()) && (port.nborMIBs[index].pNewXpduMap != nullptr)) // if found nbor and nbor has a new XPDU map
	{
		MibEntry& nbor = port.nborMIBs[index];
		map<unsigned char, xpduMapEntry>& newXpduMap = *(nbor.pNewXpduMap);
		tlvXID& rxXidTLV = static_cast<tlvXID&>(rxTlvs[2]);   // If got here, there must be XID TLV
		xpduDescriptor rxDesc = rxXidTLV.getXpduDescriptor();

		SimLog::logFile << "    Trying to find nbor new map entry for XPDU number " << (unsigned short)rxDesc.num << " rev " << (unsigned short)rxDesc.rev ;

		auto mapEntry = newXpduMap.find(rxDesc.num);                    //     Search new XPDU map for this XPDU number
	
		if (mapEntry != newXpduMap.end())
			SimLog::logFile << " success" << endl;
		else
			SimLog::logFile << " WITHOUT SUCCESS !!" << endl;

		if ((mapEntry != newXpduMap.end()) &&                       //     If found
			((mapEntry->second.xpduDesc).rev == rxDesc.rev) &&            //        and revision matches
			(port.lldpScopeAddress == rxXidTLV.getScopeAddr())         //        and scope address match
			//TODO:  also need to calculate checksum and validate
			//TODO:  check that rxDesc.num > 0
			)
		{
			mapEntry->second.sizeXpduTlvs = 0;    // Clear out old TLVs
			mapEntry->second.pTlvs.clear();
			if (rxTlvs.size() > 3)                // If have more than first three mandatory TLVs to copy 
				for (size_t i = 3; i < rxTlvs.size(); i++)
				{
					mapEntry->second.sizeXpduTlvs += (2 + rxTlvs[i].getLength());   // update cumulative size of tlvs
					mapEntry->second.pTlvs.push_back(make_shared<TLV>(rxTlvs[i]));  // copy TLVs and put pointers in map
				}
			mapEntry->second.status = RxXpduStatus::NEW;                //          and set status
			unexpectedXPDU = false;

			manifestComplete = xRxCheckManifest(port, nbor);
		}

	}

	SimLog::logFile << "    After xRxXPDU for nbor " << index;
	if ((port.nborMIBs.size() > 0) && (port.nborMIBs[index].pNewXpduMap))
	{
		SimLog::logFile << " and nbor has XPDUs: ";
		for (auto& mapEntry : (*port.nborMIBs[index].pNewXpduMap))
		{
			SimLog::logFile << " xpdu " << (unsigned short)mapEntry.first;
			SimLog::logFile << " has " << mapEntry.second.pTlvs.size() << " TLVs w/ status ";
			SimLog::logFile << mapEntry.second.status << ";";
		}
	}
	SimLog::logFile << " with unexpected XPDU = " << unexpectedXPDU << endl;

	// Store new neighbor MIB entry if have received all XPDUs
	if (manifestComplete) rxUpdateInfo(port);

//	return(manifestComplete);
}

bool LldpPort::LldpRxSM::xRxCheckManifest(LldpPort& port, MibEntry& nbor)  // returns true if manifest is complete
{
	bool manifestComplete = true;
	bool xReqComplete = true;
	xpduDescriptor first;
	xpduDescriptor second;
	unsigned char xpduCount = 0;

	for (auto& mapEntry : *nbor.pNewXpduMap)
	{   // Search in order.  Important that request in order, so any with status Requested or Retried are before Update
		RxXpduStatus xpduStatus = mapEntry.second.status;
		if ((xpduStatus == RxXpduStatus::REQUESTED) || (xpduStatus == RxXpduStatus::RETRIED))
		{
			xReqComplete = false;
			manifestComplete = false;
			break;
		}
		if (xpduStatus == RxXpduStatus::UPDATE)
		{
			manifestComplete = false;
			if (xpduCount < 2)
			{
				xpduCount++;
				mapEntry.second.status = RxXpduStatus::REQUESTED;
				if (xpduCount == 1)
				{
					first = mapEntry.second.xpduDesc;
				}
				else
				{
					second = mapEntry.second.xpduDesc;
					break;
				}
			}
		}
	}

	// Transmit XREQ if no outstanding requests, and more XPDUs to be updated, and attached to a MAC
	if (xReqComplete && !manifestComplete && port.pIss && nbor.nborAddr)  
	{
		tlvREQ newReq(port.pIss->getMacAddress(), port.lldpScopeAddress, xpduCount);
		newReq.putXpduDescriptor(0, first);
		if (xpduCount > 1)
			newReq.putXpduDescriptor(1, second);

		shared_ptr<Lldpdu> pMyLldpdu = std::make_shared<Lldpdu>();      // Create LLDPDU
		pMyLldpdu->tlvs.push_back(nbor.chassisID);                      // Add Chassis ID of nbor sourcing info
		pMyLldpdu->tlvs.push_back(nbor.portID);                         // Add Port ID
		pMyLldpdu->tlvs.push_back(newReq);                              // Copy XREQ TLV to LLDPDU
		unique_ptr<Frame> myFrame = make_unique<Frame>(nbor.nborAddr, port.pIss->getMacAddress(), (shared_ptr<Sdu>)pMyLldpdu);
		port.pIss->Request(move(myFrame));                              // Transmit frame

		unsigned short requestTime = (nbor.rxTtl + 63) >> 5;            // Round up rxTTL/32, and add 1 (in case next timer tick comes immediately)
		if (nbor.ttlTimer > requestTime)
		{
			nbor.restoreTime = nbor.ttlTimer - requestTime;
			nbor.ttlTimer = requestTime;
		}
		else
			nbor.restoreTime = 0;

		SimLog::logFile << "Time " << SimLog::Time << ": xRxCheckManifest transmitting XREQ to " << hex << nbor.nborAddr << dec << " for " << (unsigned short)newReq.getNumXpdus();
		SimLog::logFile << ": xpdu num " << (unsigned short)newReq.getXpduDescriptor(0).num << " rev " << (unsigned short)newReq.getXpduDescriptor(0).rev;
		if (xpduCount > 1)
			SimLog::logFile << " and xpdu num " << (unsigned short)newReq.getXpduDescriptor(1).num << " rev " << (unsigned short)newReq.getXpduDescriptor(1).rev;
		SimLog::logFile << " time limit " << requestTime << endl;
	}

	return (manifestComplete);
}

void LldpPort::LldpRxSM::rxUpdateInfo(LldpPort& port) 
{
	//TODO: update remote LLDP MIB entry for this neighbor
	//TODO: set something changed remote

}

void LldpPort::LldpRxSM::rxDeleteInfo(LldpPort& port)
{
	//TODO: delete remote LLDP MIB entry for this neighbor
	//TODO: set something changed remote

}

/*
void LldpPort::LldpRxSM::generateXREQ(LldpPort& port)
{
	//TODO:  generateXREQ
}

/*
 bool LldpPort::LldpRxSM::findNeighbor(LldpPort& port, std::vector<TLV>& tlvs, int index)
{
	 bool foundNbor = false;
	 for (index = 0; !foundNbor && index < port.nborMIBs.size(); index++)
	 {
		 foundNbor = ((port.nborMIBs[index].chassisID == tlvs[0]) &&
					  (port.nborMIBs[index].portID == tlvs[1]));
	 }
	 if (foundNbor) index--;

	 SimLog::logFile << "LLDPDU is from a known neighbor is ";
	 if (foundNbor) SimLog::logFile << "true ";
	 else SimLog::logFile << "false ";
	 SimLog::logFile << " and index is " << index << endl;

	 return (foundNbor);
}
 /**/

unsigned int LldpPort::LldpRxSM::findNborIndex(LldpPort& port, std::vector<TLV>& tlvs)
{
	unsigned int index = 0;
	bool foundNbor = false;
	for (index = 0; !foundNbor && index < port.nborMIBs.size(); index++)
	{
		foundNbor = ((port.nborMIBs[index].chassisID == tlvs[0]) &&
			(port.nborMIBs[index].portID == tlvs[1]));
	}
	if (foundNbor) index--;

	SimLog::logFile << "    LLDPDU is from a known neighbor is ";
	if (foundNbor) SimLog::logFile << "true ";
	else SimLog::logFile << "false ";
	SimLog::logFile << " and index is " << index << endl;

	return (index);        // index will equal nborMIBs.size if no matching neighbor found
}
/**/

 bool LldpPort::LldpRxSM::roomForNewNeighbor(LldpPort& port, unsigned long newNborSize)
 {
	 unsigned long long currentSize = 0;
	 for (auto& entry : port.nborMIBs)
		 currentSize += entry.totalSize;
	 return (currentSize + newNborSize < port.maxSizeNborMIBs);
 }
/**/

 void LldpPort::LldpRxSM::createNeighbor(LldpPort& port, std::vector<TLV>& tlvs, bool copyTlvs)
 {
	 MibEntry newNbor;                   // Create a MIB entry for new neighbor
	 newNbor.chassisID = tlvs[0];        // Fill in Chassis ID, Port ID, and TTL
	 newNbor.portID = tlvs[1];
	 newNbor.rxTtl = (static_cast<TlvTtl&>(tlvs[2])).getTtl();
	 newNbor.ttlTimer = 0;               // Initialize timer to zero, in case just received Manifest LLDPDU

	 xpduMapEntry newNborMapEntry;        // Create xpdu map entry for Normal LLDPDU
	 newNborMapEntry.sizeXpduTlvs = 6+ newNbor.chassisID.getLength() + newNbor.portID.getLength(); // in this entry include first 3 tlv lengths
	 //TODO:  Think the cumulative size of TLVs is really only needed on localMIB (for transmitting a manifest)
	 newNborMapEntry.xpduDesc = xpduDescriptor(0, 0, 0);
	 if (copyTlvs && (tlvs.size() > 3))                // If have more than first three mandatory TLVs to copy
		 for (size_t i = 3; i < tlvs.size(); i++)
		 {
			 newNborMapEntry.sizeXpduTlvs += (2 + tlvs[i].getLength());   // update cumulative size of tlvs
			 newNborMapEntry.pTlvs.push_back(make_shared<TLV>(tlvs[i]));  // copy TLVs and put pointers in map
		 }
	 newNbor.pXpduMap->insert(make_pair(0, newNborMapEntry));  // Put xpdu map entry in map with key=0

	 port.nborMIBs.push_back(newNbor);   // Add MIB entry to list of neighbors
 }

 bool LldpPort::LldpRxSM::compareTlvs(vector<shared_ptr<TLV>>& pTlvs, vector<TLV>& rxTlvs)
 {
	 bool tlvsMatch = pTlvs.size() == (rxTlvs.size() - 3);            // potential match if have the same number of TLVs
	 if (tlvsMatch && (rxTlvs.size() > 3))                            // see if new TLVs match old
		 for (unsigned int i = 0; i < (rxTlvs.size() - 3); i++)
			 tlvsMatch &= *(pTlvs[i]) == rxTlvs[i + 3];

	 return (tlvsMatch);  // Note returns true if no TLVs as well as if all TLVs are the same
 }

 void LldpPort::LldpRxSM::rxXREQ(LldpPort& port, Lldpdu& rxLldpdu)
 {
	 // Think have already done all necessary validation.  Just send XPDU

	 tlvREQ& rxXreqTLV = static_cast<tlvREQ&>(rxLldpdu.tlvs[2]);   // If got here, there must be XREQ TLV
	 unsigned short numReq = rxXreqTLV.getNumXpdus();
	 SimLog::logFile << "Time " << SimLog::Time << ":        Receiving XREQ for " << numReq << " XPDUs" << endl;
	 for (unsigned short i = 0; i < numReq; i++)
	 {
		 xpduDescriptor desc = rxXreqTLV.getXpduDescriptor(i);     // Get the requested XPDU descriptor
		 auto xpdu = port.localMIB.pXpduMap->find(desc.num);       // Search local MIB for that XPDU number
		 if ((xpdu != port.localMIB.pXpduMap->end()) &&            //     If found
			 (xpdu->second.xpduDesc == desc) && port.pIss)         //        and descriptor matches and attached to sublayer
		 {
			 shared_ptr<Lldpdu> pMyLldpdu = std::make_shared<Lldpdu>();                 // Create LLDPDU
			 pMyLldpdu->tlvs.push_back(port.localMIB.chassisID);                        // Add Chassis ID 
			 pMyLldpdu->tlvs.push_back(port.localMIB.portID);                           // Add Port ID
			 pMyLldpdu->tlvs.push_back(tlvXID(port.lldpScopeAddress, desc));            // Create XID TLV
			 for (auto& pInfoTLV : xpdu->second.pTlvs)                                // append information TLVs
				 pMyLldpdu->tlvs.push_back(*pInfoTLV);
			 unique_ptr<Frame> myFrame = make_unique<Frame>(rxXreqTLV.getReturnAddr(),  // Wrap it in a frame
				 port.pIss->getMacAddress(), (shared_ptr<Sdu>)pMyLldpdu);
			 port.pIss->Request(move(myFrame));                                         // Transmit the XPDU

			 SimLog::logFile << "             Sending XPDU frame for num " << (unsigned short)desc.num << " rev " << (unsigned short)desc.rev << endl;
			 tlvXID& newXid = static_cast<tlvXID&>(pMyLldpdu->tlvs[2]);
		 }
	 }
 }




