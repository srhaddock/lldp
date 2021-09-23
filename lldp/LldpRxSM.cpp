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
		generateXREQ(port);
		break;
	case RxTypes::NORMAL:
		rxNormal(port, rxLldpdu);
		rxUpdateInfo(port);
		break;
	case RxTypes::MANIFEST:
		if (xRxManifest(port)) rxUpdateInfo(port);
		break;
	case RxTypes::XPDU:
		if (xRxXPDU(port)) rxUpdateInfo(port);
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
			createNeighbor(port, rxTlvs);                             //    then create new neighbor MIB entry
//			port.nborMIBs[index].pManXpdus[0]->xpduTlvs = rxTlvs;     //    and copy the new TLVs
			nborChanged = true; 
		}
	}
	else
	{
		/*
		if (port.nborMIBs[index].pManXpdus.size() != 1)          // if more than one LLDPDU previously received
		{
			port.nborMIBs[index].pManXpdus.clear();     // Delete any old XPDU info
			port.nborMIBs[index].pManXpdus.push_back(make_shared<manXpdu>(0, 0, 0));  // Add a pointer to "xpdu 0" with no tlvs
			nborChanged = true;
		}
		bool tlvsMatch = port.nborMIBs[index].pManXpdus[0]->xpduTlvs.size() == rxTlvs.size();
		if (tlvsMatch)                                                // see if new TLVs match old
			for (unsigned int i = 0; i < rxTlvs.size(); i++)
				tlvsMatch &= port.nborMIBs[index].pManXpdus[0]->xpduTlvs[i] == rxTlvs[i];
		if (!tlvsMatch)                                               // if new TLVs don't match old
		{
			port.nborMIBs[index].pManXpdus[0]->xpduTlvs = rxTlvs;     //    then copy the new TLVs
			nborChanged = true;       // Note this will signal something changed even if only TTL changed.
		}
		/**/
		if (port.nborMIBs[index].xpduMap.size() != 1)           // if more than one LLDPDU previously recieved
		{
			xpduMapEntry normalLldpduMapEntry = port.nborMIBs[index].xpduMap.at(0);   // save old map entry for Normal LLDPDU
			port.nborMIBs[index].xpduMap.clear();                                     // clear out old xpdu entries
			port.nborMIBs[index].xpduMap.insert(make_pair(0, normalLldpduMapEntry));  // restore old map entry for Normal LLDPDU
		}
		bool tlvsMatch = port.nborMIBs[index].xpduMap.at(0).pTlvs.size() == (rxTlvs.size() - 3);
		if (tlvsMatch && (rxTlvs.size() > 3))                                                // see if new TLVs match old
			for (unsigned int i = 0; i < (rxTlvs.size() - 3); i++)
				tlvsMatch &= *(port.nborMIBs[index].xpduMap.at(0).pTlvs[i]) == rxTlvs[i + 3];
		if (!tlvsMatch && (rxTlvs.size() > 3))                                               // if new TLVs don't match old
		{
			port.nborMIBs[index].xpduMap.at(0).pTlvs.clear();                                //    then clear old TLVs      
			for (unsigned int i = 0; i < (rxTlvs.size() - 3); i++)                           //    and for each new TLV
				port.nborMIBs[index].xpduMap.at(0).pTlvs[i] = make_shared<TLV>(rxTlvs[i + 3]); //  copy TLV and put pointer in map
			nborChanged = true;       // Note this will signal something changed even if only TTL changed.
		}
	}
	
	SimLog::logFile << "    After rxNormal size of nbors is " << port.nborMIBs.size();
	if (port.nborMIBs.size() > 0)
	{
		/*
		SimLog::logFile << " and entry " << index;
		if ((port.nborMIBs[0].pManXpdus.size() > 0) && port.nborMIBs[0].pManXpdus[0])
		{
			SimLog::logFile << " has " << port.nborMIBs[0].pManXpdus[0]->xpduTlvs.size() << " TLVs ";
		}
		/**/
		SimLog::logFile << " and entry " << (unsigned short)port.nborMIBs[0].xpduMap.begin()->first;
		SimLog::logFile << " has " << port.nborMIBs[0].xpduMap.begin()->second.pTlvs.size() << " TLVs ";
	}
	SimLog::logFile << " with change = " << nborChanged << endl;

	//TODO: clear out any residual XPDU info if have previously received manifest
	//TODO: init neighbor specific timers

}

bool LldpPort::LldpRxSM::xRxManifest(LldpPort& port) // returns true if manifest is complete
{
	//TODO: process manifest
	//TODO: init neighbor specific timers

	return(xRxCheckManifest(port));
}

bool LldpPort::LldpRxSM::xRxXPDU(LldpPort& port)  // returns true if manifest is complete
{
	bool xReqComplete = false;
	//TODO: process XPDU  setting xReqComplete as appropriate

	if (xReqComplete)          // If all XPDUs for this XREQ received, then check manifest
		return(xRxCheckManifest(port));  
	else
		return(false);         // Manifest is not complete if not all XPDUs received
}

bool LldpPort::LldpRxSM::xRxCheckManifest(LldpPort& port)  // returns true if manifest is complete
{
	bool manifestComplete = false;
	//TODO: check manifest setting manifestComplete as appropriate

	if (!manifestComplete) generateXREQ(port);

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

	return (index);
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

 void LldpPort::LldpRxSM::createNeighbor(LldpPort& port, std::vector<TLV>& tlvs)
 {
	 MibEntry newNbor;                   // Create a MIB entry for new neighbor
	 newNbor.chassisID = tlvs[0];        // Fill in Chassis ID, Port ID, and TTL
	 newNbor.portID = tlvs[1];
	 newNbor.rxTtl = (static_cast<TlvTtl&>(tlvs[2])).getTtl();

	 xpduMapEntry newNborMapEntry;        // Create xpdu map entry for Normal LLDPDU
	 newNborMapEntry.xpduDesc = xpduDescriptor(0, 0, 0);
	 if (tlvs.size() > 3)                // If have more than first three mandatory TLVs
		 for (size_t i = 3; i < tlvs.size(); i++)
		 {
			 newNborMapEntry.sizeXpduTlvs += (2 + tlvs[i].getLength());   // update cumulative size of tlvs
			 newNborMapEntry.pTlvs.push_back(make_shared<TLV>(tlvs[i]));  // copy TLVs and put pointers in map
		 }
	 newNbor.xpduMap.insert(make_pair(0, newNborMapEntry));  // Put xpdu map entry in map with key=0

//	 newNbor.ttl = tlvs[2];
//	 newNbor.pManXpdus.push_back(make_shared<manXpdu>(0, 0, 0));  // Add a pointer to "xpdu 0" with no tlvs

	 port.nborMIBs.push_back(newNbor);   // Add MIB entry to list of neighbors
 }





