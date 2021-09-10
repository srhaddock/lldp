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

LldpPort::LldpRxSM::RxTypes LldpPort::LldpRxSM::rxProcessFrame(LldpPort& port, const Lldpdu& rxLldpdu)
{
	//TODO: rxProcessFrame
	return(RxTypes::INVALID);
}

LldpPort::LldpRxSM::RxSmStates LldpPort::LldpRxSM::enterRxFrame(LldpPort& port)
{
	Lldpdu& rxLldpdu = (Lldpdu&)(port.pRxLldpFrame->getNextSdu());

	RxTypes rxType = rxProcessFrame(port, rxLldpdu);
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
		rxNormal(port);
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

void LldpPort::LldpRxSM::rxNormal(LldpPort& port)
{
	//TODO: clear out any residual XPDU info if have previously receive manifest
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

/**/




