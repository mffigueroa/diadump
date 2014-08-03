#include <memory>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinNT.h>
#include <stdexcept>
#include "Utility.h"
#include "PE.h"

using namespace std;

PE::PE(const wstring& peFilename)
{
	ifstream fp(peFilename.c_str(), ios::in | ios::binary);

	if(!fp) {
		throw runtime_error("Unable to open PE file");
	}

	fp.read(reinterpret_cast<char*>(&m_dosHeader), sizeof(IMAGE_DOS_HEADER));

	if(m_dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
		throw runtime_error("Invalid PE file. DOS Header signature invalid.");

	fp.seekg(m_dosHeader.e_lfanew, ios::beg);
	
	DWORD	dwPESignature;
	fp.read(reinterpret_cast<char*>(&dwPESignature), sizeof(DWORD));
	
	if(dwPESignature != IMAGE_NT_SIGNATURE)
		throw runtime_error("Invalid PE file. PE signature invalid.");

	fp.read(reinterpret_cast<char*>(&m_fileHeader), sizeof(IMAGE_FILE_HEADER));

	if(m_fileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 &&
		m_fileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
			throw runtime_error("Not an x86-compatible executable.");
	}

	m_bIs64Bit = m_fileHeader.Machine == IMAGE_FILE_MACHINE_AMD64;

	if(m_fileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64)) {
		fp.read(reinterpret_cast<char*>(&m_optionalHeader), sizeof(IMAGE_OPTIONAL_HEADER64));
	} else {
		IMAGE_OPTIONAL_HEADER32 tmpOptionalHeader32;
		fp.read(reinterpret_cast<char*>(&tmpOptionalHeader32), sizeof(IMAGE_OPTIONAL_HEADER32));

		// move over all of the optional header fields to the 64-bit version
		m_optionalHeader.Magic							= tmpOptionalHeader32.Magic;
		m_optionalHeader.MajorLinkerVersion				= tmpOptionalHeader32.MajorLinkerVersion;
		m_optionalHeader.MinorLinkerVersion				= tmpOptionalHeader32.MinorLinkerVersion;
		m_optionalHeader.SizeOfCode						= tmpOptionalHeader32.SizeOfCode;
		m_optionalHeader.SizeOfInitializedData			= tmpOptionalHeader32.SizeOfInitializedData;
		m_optionalHeader.SizeOfUninitializedData		= tmpOptionalHeader32.SizeOfUninitializedData;
		m_optionalHeader.AddressOfEntryPoint			= tmpOptionalHeader32.AddressOfEntryPoint;
		m_optionalHeader.BaseOfCode						= tmpOptionalHeader32.BaseOfCode;
		m_optionalHeader.ImageBase						= tmpOptionalHeader32.ImageBase;
		m_optionalHeader.SectionAlignment				= tmpOptionalHeader32.SectionAlignment;
		m_optionalHeader.FileAlignment					= tmpOptionalHeader32.FileAlignment;
		m_optionalHeader.MajorOperatingSystemVersion	= tmpOptionalHeader32.MajorOperatingSystemVersion;
		m_optionalHeader.MinorOperatingSystemVersion	= tmpOptionalHeader32.MinorOperatingSystemVersion;
		m_optionalHeader.MajorImageVersion				= tmpOptionalHeader32.MajorImageVersion;
		m_optionalHeader.MinorImageVersion				= tmpOptionalHeader32.MinorImageVersion;
		m_optionalHeader.MajorSubsystemVersion			= tmpOptionalHeader32.MajorSubsystemVersion;
		m_optionalHeader.MinorSubsystemVersion			= tmpOptionalHeader32.MinorSubsystemVersion;
		m_optionalHeader.Win32VersionValue				= tmpOptionalHeader32.Win32VersionValue;
		m_optionalHeader.SizeOfImage					= tmpOptionalHeader32.SizeOfImage;
		m_optionalHeader.SizeOfHeaders					= tmpOptionalHeader32.SizeOfHeaders;
		m_optionalHeader.CheckSum						= tmpOptionalHeader32.CheckSum;
		m_optionalHeader.Subsystem						= tmpOptionalHeader32.Subsystem;
		m_optionalHeader.DllCharacteristics				= tmpOptionalHeader32.DllCharacteristics;
		m_optionalHeader.SizeOfStackReserve				= tmpOptionalHeader32.SizeOfStackReserve;
		m_optionalHeader.SizeOfStackCommit				= tmpOptionalHeader32.SizeOfStackCommit;
		m_optionalHeader.SizeOfHeapReserve				= tmpOptionalHeader32.SizeOfHeapReserve;
		m_optionalHeader.SizeOfHeapCommit				= tmpOptionalHeader32.SizeOfHeapCommit;
		m_optionalHeader.LoaderFlags					= tmpOptionalHeader32.LoaderFlags;
		m_optionalHeader.NumberOfRvaAndSizes			= tmpOptionalHeader32.NumberOfRvaAndSizes;

		memcpy_s(reinterpret_cast<void*>(&m_optionalHeader.DataDirectory), sizeof(m_optionalHeader.DataDirectory),
				reinterpret_cast<void*>(&tmpOptionalHeader32.DataDirectory), sizeof(tmpOptionalHeader32.DataDirectory));
	}

	IMAGE_SECTION_HEADER peSectionHeader;

	for(WORD sectionNum = 0; sectionNum < m_fileHeader.NumberOfSections; ++sectionNum) {
		fp.read(reinterpret_cast<char*>(&peSectionHeader), sizeof(peSectionHeader));
		m_sections.push_back(PESection(peSectionHeader));
	}

	m_startOfSectionsOffset = fp.tellg();
	fp.seekg(0, ios::end);
	std::streamoff endOfSectionsOffset = fp.tellg();

	m_sectionsBuf = shared_ptr<unsigned char>(new unsigned char [endOfSectionsOffset - m_startOfSectionsOffset + 1], DeleteBuffer<unsigned char>);

	fp.seekg(m_startOfSectionsOffset, ios::beg);
	fp.read((char*)m_sectionsBuf.get(), endOfSectionsOffset - m_startOfSectionsOffset + 1);
}

unsigned long PE::getOffsetForRVA(const unsigned long long rva) const
{
	for(std::vector<PESection>::const_iterator i = m_sections.begin(), i_end = m_sections.end();
		i != i_end; ++i) {
			const IMAGE_SECTION_HEADER& sectHeader = i->getSectionHeader();

			if(sectHeader.VirtualAddress <= rva && rva <= sectHeader.VirtualAddress + sectHeader.Misc.VirtualSize) {
				if(sectHeader.PointerToRawData) {
					return sectHeader.PointerToRawData + (rva - sectHeader.VirtualAddress) - m_startOfSectionsOffset;
				} else {
					return 0;
				}
			}
	}

	return 0;
}

std::tr1::shared_ptr<const unsigned char> PE::getSectionsBuf() const
{
	return m_sectionsBuf;
}

std::tr1::shared_ptr<unsigned char> PE::getSectionsBuf()
{
	return m_sectionsBuf;
}

std::streamoff	PE::getStartOfSectionsOffset() const
{
	return m_startOfSectionsOffset;
}

bool PE::Is64Bit() const
{
	return m_bIs64Bit;
}

unsigned long PE::getEntryPoint() const
{
	return m_optionalHeader.AddressOfEntryPoint;
}

unsigned long long PE::getImageBase() const
{
	return m_optionalHeader.ImageBase;
}