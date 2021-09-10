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
#include "Lldpdu.h"

TLV::TLV(unsigned char type, unsigned short length)
{
	if ((type > 0) && (type < 128) && (length < 512))   // If valid type and length
	{
		v.resize(length + 2, 0);                        // Fix size of TLV
		v[0] = (type << 1) + (length >> 8);             // Init first two bytes with type and length
		v[1] = (unsigned char)(length & 0xFF);
	}
	else
	{
		v.push_back(0);                                 // Else create Terminator TLV
		v.push_back(0);
	}

//	cout << "Constructor for TLV with type " << (unsigned short)type << "  and length " << length
//		<< hex << " : v[0] = 0x" << (unsigned short)v[0] 
//		<< "  v[1] = 0x" << (unsigned short)v[1] << dec << endl;
//	SimLog::logFile << "Constructor for TLV with type " << (unsigned short)type << "  and length " << length
//		<< hex << " : v[0] = 0x" << (unsigned short)v[0]
//		<< "  v[1] = 0x" << (unsigned short)v[1] << dec << endl;
}

TLV::TLV(const TLV& copySource)  // Copy constructor
{
	v = copySource.v;
	
//	cout << "***** TLV Copy Constructor executed ***** (" << SimLog::Time << ")" << endl;
//	SimLog::logFile << "***** TLV Copy Constructor executed for type " << (unsigned short)getType() 
//		<<  " ***** (" << SimLog::Time << ")" << endl;
}

TLV::~TLV()
{
//	cout << "Destructor for TLV of type " << getType() << endl;
//	SimLog::logFile << "Destructor for TLV of type " << getType() << endl;
}

unsigned short TLV::getType() 
{
	return (v[0] >> 1);
}

unsigned short TLV::getLength() 
{
	unsigned short len = (((v[0] & 0x01) << 8) + v[1]);
	return (len);
}

unsigned char TLV::getChar(unsigned short offset) 
{
	unsigned char output = 0;
	if ((offset + 1) <= (unsigned short)v.size())                // If valid offset
		output = v[offset];
	return (output);                                             //    return data, else return 0
}

unsigned short TLV::getShort(unsigned short offset)
{
	unsigned short output = 0;
	if ((offset + 2) <= (unsigned short)v.size())                // If valid offset
		output = ((v[offset] << 8) + v[offset + 1]); 
	return (output);                                             //    return data, else return 0
}

unsigned long TLV::getLong(unsigned short offset)
{
	unsigned short output = 0;
	if ((offset + 4) <= (unsigned short)v.size())                // If valid offset
	{
		output = v[offset];
		for (int i = 1; i < 4; i++)
			output = (output << 8) + v[offset + i];
	}
	return (output);                                             //    return data, else return 0
}

unsigned long long TLV::getAddr(unsigned short offset)
{
	unsigned short output = 0;
	if ((offset + 6) <= (unsigned short)v.size())                // If valid offset
	{
		output = v[offset];
		for (int i = 1; i < 6; i++)
			output = (output << 8) + v[offset + i];
	}
	return (output);                                             //    return data, else return 0
}

unsigned long long TLV::getLongLong(unsigned short offset)
{
	unsigned short output = 0;
	if ((offset + 8) <= (unsigned short)v.size())                // If valid offset
	{
		output = v[offset];
		for (int i = 1; i < 8; i++)
			output = (output << 8) + v[offset + i];
	}
	return (output);                                             //    return data, else return 0
}

std::string TLV::getString(unsigned short offset, unsigned short length)
{
	std::string output = "";
	if ((offset + length) <= (unsigned short)v.size())                // If valid offset
	{
		for (int i = offset; i < (offset + length); i++)
			output += v[i];
	}
	return(output);
}

bool TLV::putChar(unsigned short offset, unsigned char input)
{
	bool success = false;
	if ((offset > 1) && ((offset + 2) <= (unsigned short)v.size()))   // If valid offset
	{
		v[offset] = input;                                            //    store data
		success = true;
	}
	return(success);
}

bool TLV::putShort(unsigned short offset, unsigned short input)
{
	bool success = false;
	if ((offset > 1) && ((offset + 2) <= (unsigned short)v.size()))   // If valid offset
	{
		v[offset + 1] = input & 0xFF;                                 //    store data
		v[offset] = (input >> 8);
		success = true;
	}
	return(success);
}

bool TLV::putLong(unsigned short offset, unsigned long input)
{
	bool success = false;
	if ((offset > 1) && ((offset + 4) <= (unsigned short)v.size()))   // If valid offset
	{
		unsigned long data = input;
//		SimLog::logFile << "    putLong input = 0x" << hex << input << " with chars ";
		for (int i = 0; i < 4; i++)                                   //    store data
		{
//			char temp = data & 0xFF;
//			SimLog::logFile << (unsigned short)temp << " ";
			v[offset + 3 - i] = data & 0xFF;
			data = data >> 8;
		}
//		SimLog::logFile << dec << endl;
		success = true;
	}
	return(success);
}

bool TLV::putAddr(unsigned short offset, unsigned long long input)
{
	bool success = false;
	if ((offset > 1) && ((offset + 6) <= (unsigned short)v.size()))   // If valid offset
	{
		unsigned long long data = input;
		for (int i = 0; i < 6; i++)                                   //    store data
		{
			v[offset + 5 - i] = data & 0xFF;
			data = data >> 8;
		}
		success = true;
	}
	return(success);
}

bool TLV::putLongLong(unsigned short offset, unsigned long long input)
{
	bool success = false;
	if ((offset > 1) && ((offset + 8) <= (unsigned short)v.size()))   // If valid offset
	{
		unsigned long long data = input;
		for (int i = 0; i < 8; i++)                                   //    store data
		{
			v[offset + 7 - i] = data & 0xFF;
			data = data >> 8;
		}
		success = true;
	}
	return(success);
}

bool TLV::putString(unsigned short offset, std::string input)
{
	bool success = false;
	if ((offset > 1) && ((offset + input.length()) <= (unsigned short)v.size()))   // If valid offset
	{
		int i = offset;
		for (auto  character : input)                                   //    store data
		{
			v[i] = character;
			i++;
		}
		success = true;
	}
	return(success);
}

void TLV::printBytes(unsigned short offset, unsigned short length)
{
	unsigned short count = length;
	if (count == 0) count = (unsigned short)v.size() - offset;
	if ((offset + count) <= (unsigned short)v.size())
	{
		SimLog::logFile << hex;
	//	for (auto entry : v)
		for (int i = offset; i < offset + count; i++)
		{
			SimLog::logFile << (unsigned short)v[i] << "  ";
		//	SimLog::logFile << (unsigned short)entry << "  ";
		}
		SimLog::logFile << dec;
	}
}

void TLV::printString(unsigned short offset, unsigned short length)
{
	unsigned short count = length;
	if (count == 0) count = (unsigned short)v.size() - offset;
	if ((offset + count) <= (unsigned short)v.size())
	{
//		for (auto entry : v) 
		for (int i = offset; i < offset + count; i++)
		{
			SimLog::logFile << v[i];
		//	SimLog::logFile << entry;
		}
	}
}




TlvTtl::TlvTtl(unsigned short ttl)
	: TLV(TLVtypes::TTL, 2) 
{
	putShort(2, ttl);
}

TlvTtl::~TlvTtl() 
{}

unsigned short TlvTtl::getTtl() 
{
	return (getShort(2));
}

bool TlvTtl::putTtl(unsigned short input) 
{
	return(putShort(2, input));
}

TlvOui::TlvOui(unsigned long OUItype, unsigned short length)
	: TLV(TLVtypes::ORG_SPECIFIC, length)
{
	putLong(2, OUItype);
	if (putLong(2, OUItype))
		cout << " Created tlv with type " << (unsigned short)getType() << " and OUI type "
		<< hex << getLong(2) << " and length " << getLength() << dec << endl;
	else
		cout << " Can't put oui type " << hex << OUItype << "in TLV of length " << dec << length << endl;
}

TlvOui::~TlvOui()
{}

unsigned long TlvOui::getOuiType() {
	return (getLong(2));
}

TlvString::TlvString(unsigned char type,  std::string inputStr)
	: TLV(type, (unsigned short)inputStr.length() + 2)
{
	putString(2, inputStr);
}

TlvString::~TlvString()
{}



tlvManifest::tlvManifest(unsigned short length)
	: TLV(TLVtypes::MANIFEST, length)
{}

tlvManifest::~tlvManifest()
{}

unsigned long long tlvManifest::getReturnAddr()
{
	return(getAddr(2));
}

unsigned long tlvManifest::getTotalSize()
{
	unsigned long size = getChar(8);
	size = (size << 8) + getChar(9);
	size = (size << 16) + getChar(10);
	return(size);
}

unsigned char tlvManifest::getNumXpdus()
{
	return(getChar(11));
}

xpduDescriptor tlvManifest::getXpduDescriptor(unsigned short position)
{
	unsigned short index = 12 + (position * 4);
	xpduDescriptor desc;
	desc.num = getChar(index);
	desc.rev = getChar(index + 1);
	desc.check = getLong(index + 2);
	return(desc);
}

bool tlvManifest::putReturnAddr(unsigned long long returnAddr)
{
	return(putAddr(2, returnAddr));
}

bool tlvManifest::puttotalSize(unsigned long size)
{
	bool success = size < 0x01000000;
	if (success)
	{
		success &= putChar(8, ((size >> 16) & 0xFF));
		success &= putChar(8, ((size >> 8) & 0xFF));
		success &= putChar(8, (size & 0xFF));
	}
	return (success);
}


bool tlvManifest::putNumXpdus(unsigned char numXpdus)
{
	return(putChar(11, numXpdus));

}

bool tlvManifest::putXpduDescriptor(unsigned short position, xpduDescriptor desc)
{
	unsigned short index = 12 + (position * 4);
	bool success = putChar(index, desc.num);
	success &= putChar(index + 1, desc.rev);
	success &= putLong(index + 2, desc.check);
	return(success);

}




/*

tlvManifest::tlvManifest()
	: TLV(TLVtypes::MANIFEST)
{

}

tlvManifest::~tlvManifest()
{
}

tlvREQ::tlvREQ()
	: TLV(TLVtypes::XREQ)
{

}

tlvREQ::~tlvREQ()
{
}

tlvXID::tlvXID()
	: TLV(TLVtypes::XID)
{

}

tlvXID::~tlvXID()
{
}

tlvOuiString::tlvOuiString(unsigned long OUItype)
	: TLV(TLVtypes::ORG_SPECIFIC)
{
	ouiType = OUItype;
}

tlvOuiString::~tlvOuiString()
{
}

tlvBasicString::tlvBasicString(TLVtypes TLVtype)
	: TLV(TLVtype)
{

}

tlvBasicString::~tlvBasicString()
{
}
/**/


Lldpdu::Lldpdu()
	: Sdu(LldpEthertype)
{
}


Lldpdu::~Lldpdu()
{
}


const Lldpdu& Lldpdu::getLldpdu(Frame& LldpFrame)        // Returns a constant reference to the Lldpdu
{
	// Will generate an error if getNextEtherType and getNextSubType were not validated before calling 

	return((const Lldpdu&)LldpFrame.getNextSdu());
}
