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
*/// lldp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "stdafx.h"
#include "Device.h"
#include "Mac.h"
#include "Frame.h"

using namespace std;


int main()
{
    std::cout << "Hello World!\n";
	SimLog::logFile << endl;
	SimLog::Debug = 8; // 6 9

//	void send8Frames(EndStn& source);


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

	for (int dev = 0; dev < brgCnt + endStnCnt; dev++)
	{
		unique_ptr<Device> thisDev = nullptr;
		if (dev < brgCnt)  // Build Bridges first
		{
			thisDev = make_unique<Device>(brgMacCnt);     // Make a device with brgMacCnt MACs
			thisDev->createBridge(CVlanEthertype);        // Add a C-VLAN bridge component with a bridge port for each MAC
		}
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
	cout << endl << "   Running Simulation:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "   Running Simulation (with Debug level " << SimLog::Debug << "):  " << endl << endl;

	SimLog::Time = 0;

	//
	//  Select Link Aggregation tests to run
	//

    //	basicLagTest(Devices);


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
