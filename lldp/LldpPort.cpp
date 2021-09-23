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

// const unsigned char defaultPortState = 0x43; 

MibEntry::MibEntry()
{
	rxTtl = 0;
	ttlTimer = 0;
	totalSize = 0;
}

MibEntry::~MibEntry()
{}

xpduMapEntry::xpduMapEntry()
{
	sizeXpduTlvs = 0;
	status = RxXpduStatus::CURRENT;
}

xpduMapEntry::~xpduMapEntry()
{}

manXpdu::manXpdu(unsigned char num, unsigned char rev, unsigned long check)
{
	xpduDesc.num = num;
	xpduDesc.rev = rev;
	xpduDesc.check = check;
	status = RxXpduStatus::CURRENT;
}

manXpdu::~manXpdu()
{}

LldpPort::LldpPort(unsigned long long chassis, unsigned long port, unsigned long long dstAddr)
	: chassisId(chassis), portId(port), lldpScopeAddress(dstAddr)
{

	pIss = nullptr;

	pRxLldpFrame = nullptr;
	maxSizeNborMIBs = 20000;    // arbitrary maximum size for storing all neighbor MIB info

	//   Initialize local MIB entry
	bool success = false;
	localMIB.chassisID = TLV(TLVtypes::CHASSIS_ID, 7);    // Create chassis ID TLV
	success = localMIB.chassisID.putChar(2, 4);           // use subtype 4 (MAC Address)
	success &= localMIB.chassisID.putAddr(3, chassisId);  //    and MAC Address from chassisId
	if (!success)
		SimLog::logFile << "Time " << SimLog::Time << ":   LldpTxSM failed to create ChassisID TLV" << endl;
	localMIB.portID = TLV(TLVtypes::PORT_ID, 5);       // Create port ID TLV
	success = localMIB.portID.putChar(2, 4);           // use subtype 5 (ifName)
	success &= localMIB.portID.putLong(3, portId);     //    and portID
	if (!success)
		SimLog::logFile << "Time " << SimLog::Time << ":   LldpTxSM failed to create PortID TLV" << endl;

//	localMIB.ttl = TlvTtl(msgTxInterval * msgTxHold);   // Create TTL TLV
	localMIB.ttlTimer = 0;                              // TTL timer not used on local MIB Entry

	localMIB.totalSize = 8 + localMIB.chassisID.getLength() + localMIB.portID.getLength();


	systemName = "Someone";               // System Name not yet assigned
	systemDescription = "Somewhere";      // System Description not yet assigned
	portDescription = "Something";        // Port Description not yet assigned

	/**/
	// Initialize local MIB entry manifest
	//   Need the Normal/Manifest LLDPDU and at least 3 XPDUs (with at least one TLV each) for testing
	/*
//	unique_ptr<manXpdu> pXpdu0 = make_unique<manXpdu>();   // xpdu0 is for Normal/Manifest TLV
	shared_ptr<manXpdu> pXpdu0 = make_shared<manXpdu>();   // xpdu0 is for Normal/Manifest TLV
	pXpdu0->xpduTlvs.push_back(TlvString(TLVtypes::SYSTEM_NAME, systemName));
	localMIB.pManXpdus.push_back(pXpdu0);
	localMIB.totalSize += (2 + systemName.length());

//	unique_ptr<manXpdu> pXpdu1 = make_unique<manXpdu>(1, 1, 0x0101);
	shared_ptr<manXpdu> pXpdu1 = make_shared<manXpdu>(1, 1, 0x0101);
	pXpdu1->xpduTlvs.push_back(TlvString(TLVtypes::SYSTEM_DESC, systemDescription));
	localMIB.pManXpdus.push_back(pXpdu1);
	localMIB.totalSize += (2 + systemDescription.length());

	//	unique_ptr<manXpdu> pXpdu2 = make_unique<manXpdu>(2, 1, 0x0201);
	shared_ptr<manXpdu> pXpdu2 = make_shared<manXpdu>(2, 1, 0x0201);
	pXpdu2->xpduTlvs.push_back(TlvString(TLVtypes::PORT_DESC, portDescription));
	localMIB.pManXpdus.push_back(pXpdu2);
	localMIB.totalSize += (2 + portDescription.length());

	//	unique_ptr<manXpdu> pXpdu3 = make_unique<manXpdu>(3, 1, 0x0301);
	shared_ptr<manXpdu> pXpdu3 = make_shared<manXpdu>(3, 1, 0x0301);
	//TODO: Using OUI = 00:00:01 for test purposes; subtype 0xaa
	string xpdu3str = "Somehow";
	TLV xpdu3Tlv = TlvOui(0x000001aa, 4 + (unsigned short)xpdu3str.length());   
	xpdu3Tlv.putString(6, xpdu3str);
	pXpdu3->xpduTlvs.push_back(xpdu3Tlv);
	localMIB.pManXpdus.push_back(pXpdu3);
	localMIB.totalSize += (6 + xpdu3str.length());
	/**/
	SimLog::logFile << "     creating local MIB xpdu map: " << hex;
	xpduMapEntry xpdu0;
	xpduDescriptor desc0(0, 1, 0);
	xpdu0.xpduDesc = desc0;
	xpdu0.pTlvs.push_back(make_shared<TlvString>(TLVtypes::SYSTEM_NAME, systemName));
	xpdu0.sizeXpduTlvs = xpdu0.pTlvs[0]->getLength();
	localMIB.xpduMap.insert(make_pair(xpdu0.xpduDesc.num, xpdu0));
	SimLog::logFile << "(num = " << (unsigned short)xpdu0.xpduDesc.num << " , mapSize = " << localMIB.xpduMap.size()
		<< " TLV type " << (unsigned short)localMIB.xpduMap.at(xpdu0.xpduDesc.num).pTlvs[0]->getType() << " ) ";

	xpduMapEntry xpdu1;
	xpduDescriptor desc1(1, 1, 0x0101);
	xpdu1.xpduDesc = desc1;
	xpdu1.pTlvs.push_back(make_shared<TlvString>(TLVtypes::SYSTEM_DESC, systemDescription));
	xpdu1.sizeXpduTlvs = xpdu1.pTlvs[0]->getLength();
	localMIB.xpduMap.insert(make_pair(xpdu1.xpduDesc.num, xpdu1));
	SimLog::logFile << "(num = " << (unsigned short)xpdu1.xpduDesc.num << " , mapSize = " << localMIB.xpduMap.size()
		<< " TLV type " << (unsigned short)localMIB.xpduMap.at(xpdu1.xpduDesc.num).pTlvs[0]->getType() << " ) ";

	xpduMapEntry xpdu2;
	xpduDescriptor desc2(2, 1, 0x0201);
	xpdu2.xpduDesc = desc2;
	xpdu2.pTlvs.push_back(make_shared<TlvString>(TLVtypes::PORT_DESC, portDescription));
	xpdu2.sizeXpduTlvs = xpdu1.pTlvs[0]->getLength();
	localMIB.xpduMap.insert(make_pair(xpdu2.xpduDesc.num, xpdu2));
	SimLog::logFile << "(num = " << (unsigned short)xpdu2.xpduDesc.num << " , mapSize = " << localMIB.xpduMap.size()
		<< " TLV type " << (unsigned short)localMIB.xpduMap.at(xpdu2.xpduDesc.num).pTlvs[0]->getType() << " ) ";

	xpduMapEntry xpdu3;
	xpduDescriptor desc3(3, 1, 0x0301);
	xpdu3.xpduDesc = desc3;
	string xpdu3str = "Somehow";
	xpdu3.pTlvs.push_back(make_shared<TlvOui>(0x000001aa, 4 + (unsigned short)xpdu3str.length()));
	xpdu3.pTlvs[0]->putString(6, xpdu3str);
	xpdu3.sizeXpduTlvs = xpdu1.pTlvs[0]->getLength();
	localMIB.xpduMap.insert(make_pair(xpdu3.xpduDesc.num, xpdu3));
	SimLog::logFile << "(num = " << (unsigned short)xpdu3.xpduDesc.num << " , mapSize = " << localMIB.xpduMap.size()
		<< " TLV type " << (unsigned short)localMIB.xpduMap.at(xpdu3.xpduDesc.num).pTlvs[0]->getType() << " ) ";
	SimLog::logFile << dec << endl;
	SimLog::logFile << "      local MIB xpduMap has " << localMIB.xpduMap.size() << " entries" << endl;

	/**/




	operational = false;                                       // Set by Receive State Machine based on ISS.Operational
	lldpV2Enabled = false;                                      //TODO:  what changes lldpV2Enabled?
	/**/

//	cout << "LldpPort Constructor called." << endl;
	SimLog::logFile << "LldpPort Constructor called." << hex << "  chassis 0x" << chassisId
		<< "  port 0x" << portId << dec << endl;
}


LldpPort::~LldpPort()
{
	pRxLldpFrame = nullptr;
	pIss = nullptr;

//	cout << "LldpPort Destructor called." << endl;
	SimLog::logFile << "LldpPort Destructor called." << hex << "  chassis 0x" << chassisId
		<< "  port 0x" << portId << dec << endl;
}

void LldpPort::setEnabled(bool val)
{
	enabled = val;
}

bool LldpPort::getOperational() const
{
	bool operational = (enabled && pIss && pIss->getOperational());
	return (operational);
}

unsigned long long LldpPort::getMacAddress() const                  // MAC-SA for all Aggregator clients
{
	if (pIss) return (pIss->getMacAddress());
	else return(0);
}


/**/
void LldpPort::reset()
{
	pRxLldpFrame = nullptr;
	adminStatus = ENABLED_RX_TX;

	LldpPort::LldpRxSM::reset(*this);
	LldpPort::LldpTxSM::reset(*this);
	LldpPort::LldpTxTimerSM::reset(*this);


}

void LldpPort::timerTick()
{
	LldpPort::LldpRxSM::timerTick(*this);
	LldpPort::LldpTxSM::timerTick(*this);
	LldpPort::LldpTxTimerSM::timerTick(*this);
}

void LldpPort::run(bool singleStep)
{
	LldpPort::LldpRxSM::run(*this, singleStep);
	LldpPort::LldpTxSM::run(*this, singleStep);
	LldpPort::LldpTxTimerSM::run(*this, singleStep);
}


/**/
/*
*   802.1AX Standard managed objects access routines
*/

void LldpPort::updateManifest(TLVtypes tlvChanged)
{
	//TODO:  This currently assumes all TLVs are in a known position in a known xpdu
	//TODO:  Should calculate check values
	//TODO:  Problem with just updating totalSize is that if it is ever incorrect it will stay that way
	switch (tlvChanged)
	{
	/*
	case SYSTEM_NAME:
		// update(replace) first TLV in Normal/Manifest LLDPDU
		localMIB.pManXpdus[0]->xpduDesc.rev ++;
		localMIB.pManXpdus[0]->xpduDesc.check ++;  // should be calculated
		localMIB.totalSize += (systemName.length() - localMIB.pManXpdus[0]->xpduTlvs[0].getLength());
		localMIB.pManXpdus[0]->xpduTlvs[0] = (TlvString(TLVtypes::SYSTEM_NAME, systemName));
		break;
	case SYSTEM_DESC:
		// update(replace) first TLV in first XPDU
		localMIB.pManXpdus[1]->xpduDesc.rev++;
		localMIB.pManXpdus[1]->xpduDesc.check++;  // should be calculated
		localMIB.totalSize += (systemDescription.length() - localMIB.pManXpdus[1]->xpduTlvs[0].getLength());
		localMIB.pManXpdus[1]->xpduTlvs[0] = (TlvString(TLVtypes::SYSTEM_DESC, systemDescription));
		break;
	case PORT_DESC:
		// update(replace) first TLV in second XPDU
		localMIB.pManXpdus[2]->xpduDesc.rev++;
		localMIB.pManXpdus[2]->xpduDesc.check++;  // should be calculated
		localMIB.totalSize += (portDescription.length() - localMIB.pManXpdus[2]->xpduTlvs[0].getLength());
		localMIB.pManXpdus[2]->xpduTlvs[0] = (TlvString(TLVtypes::PORT_DESC, portDescription));
		break;
	/**/
	case SYSTEM_NAME:
		// update(replace) first TLV in Normal/Manifest LLDPDU
		localMIB.xpduMap.at(0).xpduDesc.rev++;
		localMIB.xpduMap.at(0).xpduDesc.check++;  // should be calculated
		localMIB.totalSize += (systemName.length() - localMIB.xpduMap.at(0).pTlvs[0]->getLength());
		localMIB.xpduMap.at(0).pTlvs[0] = make_shared<TLV>(TlvString(TLVtypes::SYSTEM_NAME, systemName));
		break;
	case SYSTEM_DESC:
		// update(replace) first TLV in first XPDU
		localMIB.xpduMap.at(1).xpduDesc.rev++;
		localMIB.xpduMap.at(1).xpduDesc.check++;  // should be calculated
		localMIB.totalSize += (systemDescription.length() - localMIB.xpduMap.at(1).pTlvs[0]->getLength());
		localMIB.xpduMap.at(1).pTlvs[0] = make_shared<TLV>(TlvString(TLVtypes::SYSTEM_DESC, systemDescription));
		break;
	case PORT_DESC:
		// update(replace) first TLV in second XPDU
		localMIB.xpduMap.at(2).xpduDesc.rev++;
		localMIB.xpduMap.at(2).xpduDesc.check++;  // should be calculated
		localMIB.totalSize += (portDescription.length() - localMIB.xpduMap.at(2).pTlvs[0]->getLength());
		localMIB.xpduMap.at(2).pTlvs[0] = make_shared<TLV>(TlvString(TLVtypes::PORT_DESC, portDescription));
		break;
	}
	localChange = true;
}

string LldpPort::get_systemName()
{
	return (systemName);
}

void LldpPort::set_systemName(string input)
{
	systemName = input;
	updateManifest(TLVtypes::SYSTEM_NAME);
}

string LldpPort::get_systemDescription()
{
	return (systemDescription);
}

void LldpPort::set_systemDescription(string input)
{
	systemDescription = input;
	updateManifest(TLVtypes::SYSTEM_DESC);
}

string LldpPort::get_portDescription()
{
	return (portDescription);
}

void LldpPort::set_portDescription(string input)
{
	portDescription = input;
	updateManifest(TLVtypes::PORT_DESC);
}

bool LldpPort::get_lldpV2Enabled()
{
	return (lldpV2Enabled);
}

void LldpPort::set_lldpV2Enabled(bool enable)  // if no constraint checking then might as well make public variable
{
	lldpV2Enabled = enable;
}

/**/

void LldpPort::test_removeNbor()
{
	if (nborMIBs.size() > 0)       // if there is a neighbor MIB entry
		nborMIBs.pop_back();       //     remove the neighbor at the end of the list
}


