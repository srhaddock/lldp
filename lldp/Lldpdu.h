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

// #include <iostream>
// #include <string>
#include <vector>
#include "Mac.h"

// using namespace std;

union lldpduTlvTypeLength
{
	unsigned short typeLength;
	struct
	{
		unsigned short length : 9;        // LSB  // 9 bit length field 
		unsigned short type : 7;          // MSB  // 7 bit type field
	};
};

enum TLVtypes { END, CHASSIS_ID, PORT_ID, TTL, PORT_DESC, SYSTEM_NAME, SYSTEM_DESC,
	SYSTEM_CAPABILITIES, MGMT_ADDR, MANIFEST, XREQ, XID, ORG_SPECIFIC=127 };

class TLV
{
protected:
	std::vector<unsigned char> v;

public:
	TLV(unsigned char type = 0, unsigned short length = 0);
	TLV(const TLV& copySource);      // copy constructor
	~TLV();

	unsigned short getType();
	unsigned short getLength();
	unsigned char getChar(unsigned short offset);
	unsigned short getShort(unsigned short offset);
	unsigned long getLong(unsigned short offset);
	unsigned long long getAddr(unsigned short offset);
	unsigned long long getLongLong(unsigned short offset);
	std::string getString(unsigned short offset, unsigned short length);
	bool putChar(unsigned short offset, unsigned char input);
	bool putShort(unsigned short offset, unsigned short input);
	bool putLong(unsigned short offset, unsigned long input);
	bool putAddr(unsigned short offset, unsigned long long input);
	bool putLongLong(unsigned short offset, unsigned long long input);
	bool putString(unsigned short offset, std::string input);

	void printBytes(unsigned short offset = 0, unsigned short length = 0);
	void printString(unsigned short offset = 0, unsigned short length = 0);

};

class TlvTtl : public TLV
{
public:
	TlvTtl(unsigned short ttl);
	~TlvTtl();

	unsigned short getTtl();
	bool putTtl(unsigned short input);
};

class TlvOui : public TLV
{
public:
	TlvOui(unsigned long OUItype, unsigned short length);
	~TlvOui();

	unsigned long getOuiType();
};

class TlvString : public TLV
{
public:
	TlvString(unsigned char type, std::string inputStr);
	~TlvString();
};

/**/
class xpduDescriptor
{
public:
	unsigned char num;
	unsigned char rev;
	unsigned long check;
};

class tlvManifest : public TLV
{
public:
	tlvManifest(unsigned short length);
	~tlvManifest();

	unsigned long long getReturnAddr();
	unsigned long getTotalSize();
	unsigned char getNumXpdus();
	xpduDescriptor getXpduDescriptor(unsigned short position);
	bool putReturnAddr(unsigned long long returnAddr);
	bool puttotalSize(unsigned long size);
	bool putNumXpdus(unsigned char numXpdus);
	bool putXpduDescriptor(unsigned short position, xpduDescriptor desc);
};


/*
class tlvREQ : public TLV
{
public:
	tlvREQ();
	~tlvREQ();

	unsigned long long returnAddr;
	unsigned long long scopeAddr;
	unsigned char rsvd;
	unsigned char numXpdus;
	std::vector<xpduDescriptor> xpduDescriptors;
};

class tlvXID : public TLV
{
public:
	tlvXID();
	~tlvXID();

	unsigned long long scopeAddr;
	unsigned char xpduNum;
	unsigned char xpduRev;
};

class tlvBasicString : public TLV
{
public:
	tlvBasicString(TLVtypes TLVtype);
	~tlvBasicString();

	std::string tlvString;
};
/**/

class Lldpdu : public Sdu
{
public:
	Lldpdu();
	~Lldpdu();
	
	static const Lldpdu& getLldpdu(Frame& LldpFrame);      // Returns a constant reference to the LLDPDU

	/**/
	// For initial testing just pass Chassis ID and Port ID outside of any TLV structure
	unsigned long long chassisId;
	unsigned long portId;
	unsigned short rxTTL;
	/**/

	std::vector<TLV> tlvs;
};

