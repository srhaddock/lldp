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
#include "Mac.h"
#include "Lldpdu.h"

using namespace std;

class Lacpdu;


enum RxXpduStatus { CURRENT, NEW, UPDATE, REQUESTED, RETRIED };

class manXpdu
{
public:
	manXpdu(unsigned char num = 0, unsigned char rev = 0, unsigned long check = 0);
	~manXpdu();

	xpduDescriptor xpduDesc;
	RxXpduStatus status;
	std::vector<TLV> xpduTlvs;
};

class MibEntry
{
public:
	MibEntry();
	~MibEntry();

	TLV chassisID;
	TLV portID;
	TLV ttl; 
	unsigned short ttlTimer;
	int totalSize;
	std::vector<unique_ptr<manXpdu>> pManXpdus;
};

class LldpPort : public IssQ
{
	friend class LinkLayerDiscovery;

public:
	LldpPort(unsigned long long chassis, unsigned long port, unsigned long long dstAddr);
	~LldpPort();
	LldpPort(LldpPort& copySource) = delete;             // Disable copy constructor
	LldpPort& operator= (const LldpPort&) = delete;      // Disable assignment operator

	virtual void setEnabled(bool val);
	bool getOperational() const;
	virtual unsigned long long getMacAddress() const;

	shared_ptr<Iss> pIss;

	void reset();
	void timerTick();
	void run(bool singleStep);


 private:
	unsigned long long chassisId;
	unsigned long portId;
	unsigned long long lldpDestinationAddress;
	bool operational;                   // maybe don't have local variable, just pass through from supporting service
	bool lldpV2Enabled;

	unique_ptr<Frame> pRxLldpFrame;

	std::string systemName;
	std::string systemDescription;
	std::string portDescription;

	enum adminStatusVals { DISABLED, ENABLED_TX_ONLY, ENABLED_RX_ONLY, ENABLED_RX_TX };
	adminStatusVals adminStatus;    

	int msgTxInterval = 10;            // range 1 - 3600; default 30;
	int msgTxHold = 3;                 // range 1 - 100;  default 4;
	int msgFastTx = 1;                 // range 1 - 3600; default 1;
	int txFastInit = 4;                // range 1 - 8;    default 4;
	int reInitDelay = 2;               //                 default 2;
	int txCreditMax = 5;               // range 1 - 10;   default 5;

	MibEntry localMIB;
	std::vector<MibEntry> nborMIBs;
	

public:
	/*
	*   802.1AX Standard managed objects access routines
	*/
	void updateManifest(TLVtypes tlvChanged);
	
	string get_systemName();
	void set_systemName(string input);
	string get_systemDescription();
	void set_systemDescription(string input);
	string get_portDescription();
	void set_portDescription(string input);

	/**/
private:

//	static class LldpRxSM
	class LldpRxSM
	{
	public:

		enum RxSmStates { NO_STATE, WAIT_OPERATIONAL, RX_INITIALIZE, DELETE_AGED_INFO, RX_WAIT_FRAME, 
			RX_FRAME, RX_EXTENDED, DELETE_INFO, UPDATE_INFO, REMOTE_CHANGES, RX_XPDU_REQUEST };
		enum RxTypes { INVALID, NORMAL, SHUTDOWN, MANIFEST, XPDU, XREQ };

		static void reset(LldpPort& port);
		static void timerTick(LldpPort& port);
		static int run(LldpPort& port, bool singleStep);

	private:
		static bool step(LldpPort& port);
		
		static RxSmStates enterWaitOperational(LldpPort& port);
		static RxSmStates enterRxInitialize(LldpPort& port);
		static RxSmStates enterDeleteAgedInfo(LldpPort& port);
		static RxSmStates enterRxWaitFrame(LldpPort& port);
		static RxSmStates enterRxFrame(LldpPort& port);
//		static RxSmStates enterRxExtended(LldpPort& port);
//		static RxSmStates enterUpdateInfo(LldpPort& port);
		static RxSmStates enterRemoteChanges(LldpPort& port);
		static RxSmStates enterRxXpduRequest(LldpPort& port, const Lldpdu& rxLldpdu);

		static void rxCheckTimers(LldpPort& port);
		static RxTypes rxProcessFrame(LldpPort& port, const Lldpdu& rxLldpdu);
		static void rxNormal(LldpPort& port);
		static void rxDeleteInfo(LldpPort& port);
		static void rxUpdateInfo(LldpPort& port);

		static void createNeighbor(LldpPort& port);
	//	static bool runRxExtended(LldpPort& port);
		static bool xRxManifest(LldpPort& port);
		static bool xRxXPDU(LldpPort& port);
		static bool xRxCheckManifest(LldpPort& port);
		static void generateXREQ(LldpPort& port);
	/**/
	};

	LldpRxSM::RxSmStates RxSmState;
//	LldpRxSM::RxTypes rxType; // This can be a local variable in received state machine
	bool rxInfoAge;
	bool anyTTLExpired;
//	bool rcvFrame;          // Not used:  instead use pRxLldpFrame != nullptr
//	bool rxChanges;         // This can be a local variable in received state machine
	bool sentManifest;
//	bool manifestComplete;  // This can be a local variable in received state machine
	int rxTTL;







//	static class LldpTxSM
	class LldpTxSM
	{
	public:

		enum TxSmStates { NO_STATE, TX_LLDP_INITIALIZE, TX_IDLE, TX_SHUTDOWN_FRAME, TX_INFO_FRAME };

		static void reset(LldpPort& port);
		static void timerTick(LldpPort& port);
		static int run(LldpPort& port, bool singleStep);

	private:

		static bool stepTxSM(LldpPort& port);
		static TxSmStates enterTxLldpInitialize(LldpPort& port);
		static TxSmStates enterTxIdle(LldpPort& port);
		static TxSmStates enterTxShutdownFrame(LldpPort& port);
		static TxSmStates enterTxInfoFrame(LldpPort& port);

		static void mibConstrInfoLldpdu(LldpPort& port);
		static void mibConstrShutdownLldpdu(LldpPort& port);
		static bool transmitLldpdu(LldpPort& port, int TTL);
		static void prepareLldpdu(LldpPort& port, Lldpdu& myLldpdu, unsigned short TTL);
		/**/
	};

	LldpTxSM::TxSmStates TxSmState;
	int LLDP_txShutdownWhile;
	int txTTL;
	int LACP_txWhen;
	bool txOpportunity;
	int txCredit;



	//	static class LldpTxTimerSM
	class LldpTxTimerSM
	{
	public:

		enum TxTimerSmStates { NO_STATE, TX_TIMER_INITIALIZE, TX_TIMER_IDLE, TX_TIMER_EXPIRES, TX_FAST_START, SIGNAL_TX };

		static void reset(LldpPort& port);
		static void timerTick(LldpPort& port);
		static int run(LldpPort& port, bool singleStep);

	private:

		static bool stepTxSM(LldpPort& port);
		static TxTimerSmStates enterTxTimerInitialize(LldpPort& port);
		static TxTimerSmStates enterTxTimerIdle(LldpPort& port);
		static TxTimerSmStates enterTxTimerExpires(LldpPort& port);
		static TxTimerSmStates enterTxFastStart(LldpPort& port);
		static TxTimerSmStates enterSignalTx(LldpPort& port);

	};

	LldpTxTimerSM::TxTimerSmStates TxTimerSmState;
	bool localChange;
	bool newNeighbor;       
	int LLDP_txTTR;
	int txFast;
	bool txNow;


};

