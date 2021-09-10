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

/**/
void LldpPort::LldpTxSM::reset(LldpPort& port)
{
	port.TxSmState = enterTxLldpInitialize(port);
}

/**/
void LldpPort::LldpTxSM::timerTick(LldpPort& port)
{
	if (port.LLDP_txShutdownWhile > 0) port.LLDP_txShutdownWhile--;
}
/**/

int LldpPort::LldpTxSM::run(LldpPort& port, bool singleStep)
{
	bool transitionTaken = false;
	int loop = 0;

	do
	{
		transitionTaken = stepTxSM(port);
		loop++;
	} while (!singleStep && transitionTaken && loop < 10);
	if (!transitionTaken) loop--;
	return (loop);
}


bool LldpPort::LldpTxSM::stepTxSM(LldpPort& port)
{
	TxSmStates nextTxSmState = TxSmStates::NO_STATE;
	bool transitionTaken = false;
	
	bool globalTransition = !(port.pIss && port.pIss->getOperational());               

	if (globalTransition && (port.TxSmState != TxSmStates::TX_LLDP_INITIALIZE))
		nextTxSmState = enterTxLldpInitialize(port);         // Global transition, but only take if will change something
	else switch (port.TxSmState)
	{
	case TxSmStates::TX_LLDP_INITIALIZE:
		if ((port.adminStatus == ENABLED_RX_TX) || (port.adminStatus == ENABLED_TX_ONLY)) nextTxSmState = enterTxIdle(port);
		break;
	case TxSmStates::TX_IDLE:
		if ((port.adminStatus == DISABLED) || (port.adminStatus == ENABLED_RX_ONLY)) nextTxSmState = enterTxShutdownFrame(port);
		if (port.txNow && port.txCredit > 0) nextTxSmState = enterTxInfoFrame(port);
		break;
	case TxSmStates::TX_SHUTDOWN_FRAME:
		if (port.LLDP_txShutdownWhile == 0) nextTxSmState = enterTxLldpInitialize(port);
		break;
	case TxSmStates::TX_INFO_FRAME:
		nextTxSmState = enterTxLldpInitialize(port);
		break;
	default:
		nextTxSmState = enterTxLldpInitialize(port);
		break;
	}
	if (nextTxSmState != TxSmStates::NO_STATE)
	{
		port.TxSmState = nextTxSmState;
		transitionTaken = true;
	}
	else {}   // no change to TxSmState (or anything else) 

	return (transitionTaken);
}


LldpPort::LldpTxSM::TxSmStates LldpPort::LldpTxSM::enterTxLldpInitialize(LldpPort& port)
{
	//TODO:  txInitializeLldp(port);

	port.LLDP_txShutdownWhile = 0;

	return (TxSmStates::TX_LLDP_INITIALIZE);
}

LldpPort::LldpTxSM::TxSmStates LldpPort::LldpTxSM::enterTxIdle(LldpPort& port)
{
	port.txTTL = port.msgTxInterval * port.msgTxHold + 1;
	if (port.txTTL > 65535) port.txTTL = 65535;

	return (TxSmStates::TX_IDLE);
}

LldpPort::LldpTxSM::TxSmStates LldpPort::LldpTxSM::enterTxShutdownFrame(LldpPort& port)
{
	mibConstrShutdownLldpdu(port);
	port.LLDP_txShutdownWhile = port.reInitDelay;

	return (TxSmStates::TX_SHUTDOWN_FRAME);
}

LldpPort::LldpTxSM::TxSmStates LldpPort::LldpTxSM::enterTxInfoFrame(LldpPort& port)
{
	mibConstrInfoLldpdu(port);
	port.txCredit--;                // only enter when txCredit > 0, so don't need to check
	port.txNow = false;

	return (enterTxIdle(port));
}

void LldpPort::LldpTxSM::mibConstrShutdownLldpdu(LldpPort& port)
{
//	if (transmitLldpdu(port, 0)) port.txCount++;	
	transmitLldpdu(port, 0);
}

void LldpPort::LldpTxSM::mibConstrInfoLldpdu(LldpPort& port)
{
//	if (transmitLldpdu(port, port.txTTL)) port.txCount++;
	transmitLldpdu(port, port.txTTL);
}

/**/

bool LldpPort::LldpTxSM::transmitLldpdu(LldpPort& port, int TTL)
{
	bool success = false;

    if (port.pIss && port.pIss->getOperational())  // Transmit frame only if MAC won't immediately discard
	{
		unsigned long long mySA = port.pIss->getMacAddress();
		shared_ptr<Lldpdu> pMyLldpdu = std::make_shared<Lldpdu>();
		prepareLldpdu(port, *pMyLldpdu, TTL);
		unique_ptr<Frame> myFrame = make_unique<Frame>(port.lldpDestinationAddress, mySA, (shared_ptr<Sdu>)pMyLldpdu);
		port.pIss->Request(move(myFrame));
		success = true;

		if ((SimLog::Debug > 4))
		{
			// SimLog::logFile << "Time " << SimLog::Time << ":  Transmit LACPDU" 
			SimLog::logFile << "Time " << SimLog::Time << ":  Transmit LLDPDU from" 
				<< hex << " chassis " << port.chassisId << "  port " << port.portId
				<< "  with DA " << port.lldpDestinationAddress << " and SA " << port.pIss->getMacAddress() 
				<< dec << endl;
		}

	} 
	else 
	{
		if ((SimLog::Debug > 9))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":  Can't transmit LLDPDU from down port " << hex << port.pIss->getMacAddress() << dec << endl;
		}

	}

	return (success);
}


void LldpPort::LldpTxSM::prepareLldpdu(LldpPort& port, Lldpdu& myLldpdu, unsigned short TTL)
{
	myLldpdu.TimeStamp = SimLog::Time;
	/**/
	myLldpdu.chassisId = port.chassisId;
	myLldpdu.portId = port.portId;
	myLldpdu.rxTTL = TTL;
	/**/

	myLldpdu.tlvs.push_back(port.localMIB.chassisID);     // Copy Chassis ID TLV
	myLldpdu.tlvs.push_back(port.localMIB.portID);        // Copy Port ID TLV
//	myLldpdu.tlvs.push_back(port.localMIB.ttl);           // Copy TTL TLV
	myLldpdu.tlvs.push_back(TlvTtl(TTL));                 // Create TTL TLV on tlvs vector

	SimLog::logFile << "1st TLV: ";
	myLldpdu.tlvs[0].printBytes();
	SimLog::logFile << endl << "2nd TLV: ";
	myLldpdu.tlvs[1].printBytes();
	SimLog::logFile << endl << "3rd TLV: ";
	myLldpdu.tlvs[2].printBytes();
	SimLog::logFile << endl ;

	if (TTL > 0)               // if not shutdown then add TLVs (else done)
	{
		/*
		bool success = false;
		TLV chassisTLV(TLVtypes::CHASSIS_ID, 7);           // Create chassis ID TLV
		success = chassisTLV.putChar(2, 4);                // use subtype 4 (MAC Address)
		success &= chassisTLV.putAddr(3, port.chassisId);  //    and MAC Address from chassisId
		if (!success)
			SimLog::logFile << "Time " << SimLog::Time << ":   LldpTxSM failed to create ChassisID TLV" << endl;
		myLldpdu.tlvs.push_back(chassisTLV);               // Copy to tlvs vector

		myLldpdu.tlvs.push_back(TLV(TLVtypes::PORT_ID, 5));       // Create port ID TLV on tlvs vector
		success = myLldpdu.tlvs[1].putChar(2, 4);                 // use subtype 5 (ifName)
		success &= myLldpdu.tlvs[1].putLong(3, port.portId);      //    and portID
		if (!success)
			SimLog::logFile << "Time " << SimLog::Time << ":   LldpTxSM failed to create PortID TLV" << endl;

		myLldpdu.tlvs.push_back(TlvTtl(TTL));             // Create TTL TLV on tlvs vector
		/**/

	//	myLldpdu.tlvs.push_back(TlvString(TLVtypes::SYSTEM_NAME, port.systemName));
		/**/
		if (!port.localMIB.pManXpdus.empty() && port.localMIB.pManXpdus[0])
				for (auto tlv : port.localMIB.pManXpdus[0]->xpduTlvs)
				{
					SimLog::logFile << "local MIB entry has an xpdu in first entry of pManXpdus ";
					TLV& tlv3 = (port.localMIB.pManXpdus[0]->xpduTlvs[0]);
					SimLog::logFile << "with type = " << (unsigned short)tlv3.getType() << endl;
					myLldpdu.tlvs.push_back(tlv3);
				}
		else
			SimLog::logFile << "local MIB entry has invalid first pManXpdu" << endl;
		/**/
		
		SimLog::logFile << "4th TLV: ";
		myLldpdu.tlvs[3].printBytes();
		SimLog::logFile << "  : ";
		myLldpdu.tlvs[3].printString(2);
		SimLog::logFile << endl;




	}

	/*
	myLacpdu.actorSystem = port.actorOperSystem;
	myLacpdu.actorKey = port.actorOperPortKey;
	myLacpdu.actorPort = port.actorPort;
	myLacpdu.actorState = port.actorOperPortState;

	myLacpdu.partnerSystem.id = port.partnerOperSystem.id;
	myLacpdu.partnerKey = port.partnerOperKey;
	myLacpdu.partnerSystem = port.partnerOperSystem;
	myLacpdu.partnerPort.id = port.partnerOperPort.id;
	myLacpdu.partnerState.state = port.partnerOperPortState.state;

	myLacpdu.collectorMaxDelay = port.collectorMaxDelay;  

	/*
	if (port.actorLacpVersion >= 2)        // if v2 send portAlgorithm etc from current or previous aggregator
	{
		myLacpdu.portAlgorithmTlv = true;
		myLacpdu.portConversationIdDigestTlv = true;
		//TODO:  should I create a per-AggPort boolean that controls whether Service Mapping TLV included?
		myLacpdu.portConvServiceDigestTlv = true;
		myLacpdu.linkNumber = port.LinkNumber;

		myLacpdu.actorPortAlgorithm = port.actorOperPortAlgorithm;
		myLacpdu.actorConvLinkDigest = port.actorOperConvLinkDigest;
		if (myLacpdu.portConvServiceDigestTlv)
			myLacpdu.actorConvServiceDigest = port.actorOperConvServiceDigest;

		myLacpdu.portConversationMaskTlvs = false;
		/*
		//  Ax-2014 only
		myLacpdu.portConversationMaskTlvs = port.longLacpduXmit && port.enableLongLacpduXmit && (port.partnerLacpVersion >= 2);
		// Test enableLongLacpduXmit and partnerLacpVersion even though already tested to set longLacpduXmit in case they change
		//   while longLacpduXmit is true.
		if (myLacpdu.portConversationMaskTlvs)
		{
			myLacpdu.actorPortConversationMaskState.actorPartnerSync = port.actorPartnerSync;
			myLacpdu.actorPortConversationMaskState.DWC = port.actorDWC;
			myLacpdu.actorPortConversationMaskState.PSI = false;      //TODO: Not relevant ?
			myLacpdu.actorPortConversationMask = port.portOperConversationMask;
		}
		/*
	}
	else
	{
		myLacpdu.portAlgorithmTlv = false;
		myLacpdu.portConversationIdDigestTlv = false;
		myLacpdu.portConvServiceDigestTlv = false;
		myLacpdu.portConversationMaskTlvs = false;
	}
	/**/
}
/**/

void LldpPort::LldpTxTimerSM::reset(LldpPort& port)
{
	port.TxTimerSmState = enterTxTimerInitialize(port);
}

/**/
void LldpPort::LldpTxTimerSM::timerTick(LldpPort& port)
{
	if (port.txCredit < port.txCreditMax) port.txCredit++;
	if (port.LLDP_txTTR > 0) port.LLDP_txTTR--;
}
/**/

int LldpPort::LldpTxTimerSM::run(LldpPort& port, bool singleStep)
{
	bool transitionTaken = false;
	int loop = 0;

	do
	{
		transitionTaken = stepTxSM(port);
		loop++;
	} while (!singleStep && transitionTaken && loop < 10);
	if (!transitionTaken) loop--;
	return (loop);
}


bool LldpPort::LldpTxTimerSM::stepTxSM(LldpPort& port)
{
	TxTimerSmStates nextTxTimerSmState = TxTimerSmStates::NO_STATE;
	bool transitionTaken = false;

	bool globalTransition = ( !(port.pIss && port.pIss->getOperational())
		|| (port.adminStatus == DISABLED) || (port.adminStatus == ENABLED_RX_ONLY));

	if (globalTransition && (port.TxTimerSmState != TxTimerSmStates::TX_TIMER_INITIALIZE))
		nextTxTimerSmState = enterTxTimerInitialize(port);         // Global transition, but only take if will change something
	else switch (port.TxTimerSmState)
	{
	case TxTimerSmStates::TX_TIMER_INITIALIZE:
		if ((port.adminStatus == ENABLED_RX_TX) || (port.adminStatus == ENABLED_TX_ONLY)) nextTxTimerSmState = enterTxTimerIdle(port);
		break;
	case TxTimerSmStates::TX_TIMER_IDLE:
		if (port.newNeighbor) nextTxTimerSmState = enterTxFastStart(port);                  // Standard doesn't specify precedence of these transitions
		else if (port.LLDP_txTTR == 0) nextTxTimerSmState = enterTxTimerExpires(port);
		else if (port.localChange) nextTxTimerSmState = enterSignalTx(port);                // Put newNeighbor first so don't get 2 LLDPDUs if localChange true
		break;
	case TxTimerSmStates::TX_FAST_START:
		nextTxTimerSmState = enterTxTimerExpires(port);
		break;
	case TxTimerSmStates::TX_TIMER_EXPIRES:
		nextTxTimerSmState = enterSignalTx(port);
		break;
	case TxTimerSmStates::SIGNAL_TX:
		nextTxTimerSmState = enterTxTimerIdle(port);
		break;
	default:
		nextTxTimerSmState = enterTxTimerInitialize(port);
		break;
	}
	if (nextTxTimerSmState != TxTimerSmStates::NO_STATE)
	{
		port.TxTimerSmState = nextTxTimerSmState;
		transitionTaken = true;
	}
	else {}   // no change to TxSmState (or anything else) 

	return (transitionTaken);
}


LldpPort::LldpTxTimerSM::TxTimerSmStates LldpPort::LldpTxTimerSM::enterTxTimerInitialize(LldpPort& port)
{
//  port.txTick = false;            // txTick is handled by timerTick() routine
	port.txNow = false;
	port.localChange = false;
	port.LLDP_txTTR = 0;
	port.txFast = 0;
	port.newNeighbor = false;
	port.txCredit = port.txCreditMax;

	return (TxTimerSmStates::TX_TIMER_INITIALIZE);
}

LldpPort::LldpTxTimerSM::TxTimerSmStates LldpPort::LldpTxTimerSM::enterTxTimerIdle(LldpPort& port)
{
	return (TxTimerSmStates::TX_TIMER_IDLE);
}

LldpPort::LldpTxTimerSM::TxTimerSmStates LldpPort::LldpTxTimerSM::enterTxFastStart(LldpPort& port)
{
	port.newNeighbor = false;
	if (port.txFast == 0) port.txFast = port.txFastInit;

	return (enterTxTimerExpires(port));
}

LldpPort::LldpTxTimerSM::TxTimerSmStates LldpPort::LldpTxTimerSM::enterTxTimerExpires(LldpPort& port)
{
	if (port.txFast > 0) port.txFast--;

	return (enterSignalTx(port));
}

LldpPort::LldpTxTimerSM::TxTimerSmStates LldpPort::LldpTxTimerSM::enterSignalTx(LldpPort& port)
{
	port.txNow = true;
	port.localChange = false;
	if (port.txFast > 0) port.LLDP_txTTR = port.msgFastTx;
	else port.LLDP_txTTR = port.msgTxInterval;

	return (enterTxTimerIdle(port));
}
/**/