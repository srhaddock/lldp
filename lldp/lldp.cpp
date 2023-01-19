/*
Copyright 2023 Stephen Haddock Consulting, LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/// lldp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "stdafx.h"
#include "Device.h"
#include "Mac.h"
#include "Frame.h"
#include "LinkLayerDiscovery.h"

using namespace std;


int main()
{
    std::cout << "Hello World!\n";
	SimLog::logFile << endl;
	SimLog::Debug = 15; // 6 9
	SimLog::Time = 0;

	unsigned short testUint = 3;
	testUint = testUint - 5;
	SimLog::logFile << "Testing a negative result to unsigned short arithmetic:  5 - 3 = " << testUint << endl;

//	void send8Frames(EndStn& source);
	void basicLldpTest(std::vector<unique_ptr<Device>> & Devices);


	//
	//  Build some Devices
	//
	/**/
	std::vector<unique_ptr<Device>> Devices;   // Pointer to each device stored in a vector

	int brgCnt = 3;
	int brgMacCnt = 8;
	int endStnCnt = 3;
	int endStnMacCnt = 4;

	cout << "   Building Devices:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "   Building Devices:  " << endl << endl;

//	for (int dev = 0; dev < brgCnt + endStnCnt; dev++)
	for (int dev = 0; dev < brgCnt; dev++)              // Just build bridges
		{
		unique_ptr<Device> thisDev = nullptr;
		if (dev < brgCnt)  // Build Bridges first
		{
			thisDev = make_unique<Device>(brgMacCnt);     // Make a device with brgMacCnt MACs
		//	thisDev->createBridge(CVlanEthertype);        // Add a C-VLAN bridge component with a bridge port for each MAC
		//TODO:  This kludge gives each bridge a unique name
			switch (dev)
			{
			case 0: thisDev->createBridge(CVlanEthertype, "Larry", "is in New York"); break;
			case 1: thisDev->createBridge(CVlanEthertype, "Curly", "is in Boston"); break; 
			case 2: thisDev->createBridge(CVlanEthertype, "  Moe", "is in Denver"); break;
			default: thisDev->createBridge(CVlanEthertype, "Anonymous", "is not to be found"); break;
			}
		}
//TODO:  End stations are broken
		else               //    then build End Stations
		{
			thisDev = make_unique<Device>(endStnMacCnt);  // Make a device with endStnMacCnt MACs
			thisDev->createEndStation();                  // Add an end station component
		}
		Devices.push_back(move(thisDev));                       // Put Device in vector of Devices
	}

	//
	//  Run the simulation
	//
	cout << endl << endl << "   Running Simulation:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "   Running Simulation (with Debug level " << SimLog::Debug << "):  " << endl << endl;

	SimLog::Time = 0;

	//
	//  Select Link Layer Discovery tests to run
	//

    basicLldpTest(Devices);

	//
	// Clean up devices.
	//

	cout << endl << "    Cleaning up devices:" << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "    Cleaning up devices:" << endl << endl;

	Devices.clear();

	cout << endl << "*** End of program ***" << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "*** End of program ***" << endl << endl;

	return 0;
}


void basicLldpTest(std::vector<unique_ptr<Device>>& Devices)
{
	int start = SimLog::Time;

	cout << endl << endl << "   Basic LLDP Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   Basic LLDP Tests:  " << endl << endl;

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}

	LinkLayerDiscovery& dev0Lldp = (LinkLayerDiscovery&)*(Devices[0]->pComponents[1]);  // alias to LLDP shim of bridge b00
//  LinkAgg& dev0Lag = (LinkAgg&)*(Devices[0]->pComponents[1]);  // alias to LinkAgg shim of bridge b00
//	dev0Lldp.pLldpPorts[0]->set_aAggPortWTRTime(30);                  // temp: set WTR timer on bridge:port b00:100

	for (int i = 0; i < 1000; i++)
	{
		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;

		//  Make or break connections

		if (SimLog::Time == start + 10)
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[1]->pMacs[0]), 5);   // Connect two Bridges
		// Link 1 comes up with AggPort b00:100 on Aggregator b00:200 and AggPort b01:100 on Aggregator b01:200.

		if ((SimLog::Time == start + 33) || (SimLog::Time == start + 35))     // remove neighbor MIB info on dev 0 port 0
		{
			LinkLayerDiscovery& LLDP = (LinkLayerDiscovery&)*(Devices[0]->pComponents[1]);  // assumes LLDP shim is component after bridge
			LLDP.pLldpPorts[0]->test_removeNbor();
		}

		if (SimLog::Time == start + 50)      // set lldpV2Enabled in all ports on all three bridges
		{
			for (unsigned int i = 0; i < 3; i++)
			{
				LinkLayerDiscovery& LLDP = (LinkLayerDiscovery&)*(Devices[i]->pComponents[1]);  // assumes LLDP shim is component after bridge
				for (unsigned int j = 0; j < LLDP.pLldpPorts.size(); j++)
				{
					LldpPort& port = *LLDP.pLldpPorts[j];
					port.set_lldpV2Enabled(true);
					port.test_removeNbor();
				}
			}
		}

		if (SimLog::Time == start + 300)
			Mac::Disconnect((Devices[0]->pMacs[0]));                           // Take down first link
		// Link 1 goes down and conversations immediately re-allocated to other links.
		// AggPorts b00:102 and b00:103 remain up on Aggregator b00:200

		if (SimLog::Time == start + 990)
		{
//			dev0Lag.pAggPorts[0]->set_aAggPortWTRTime(0);                  // temp: restore default WTR timer on bridge:port b00:100
			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}
		}


		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);   // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit
		if ((SimLog::Debug > 0)  && (SimLog::Time < 0))
			SimLog::logFile << endl <<  "   About to transmit from MACs  " << endl;

		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}

		if ((SimLog::Debug > 0) && (SimLog::Time < 0))
			SimLog::logFile  << "   Done transmiting from MACs  " << endl;


		SimLog::Time++;
	}
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
