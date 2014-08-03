#ifndef __PE_H__
#define __PE_H__

#include <memory>
#include <string>
#include <vector>

#include "PESection.h"

class PE
{
public:
	PE(const std::wstring& peFilename);

	unsigned long								getEntryPoint() const;
	unsigned long								getOffsetForRVA(const unsigned long long rva) const;
	std::streamoff								getStartOfSectionsOffset() const;

	std::tr1::shared_ptr<const unsigned char>	getSectionsBuf() const;
	std::tr1::shared_ptr<unsigned char>			getSectionsBuf();
	
	unsigned long long							getImageBase() const;
	bool										Is64Bit() const;

private:
	std::vector<std::wstring>			m_dllImports;
	std::vector<PESection>				m_sections;

	std::tr1::shared_ptr<unsigned char>	m_sectionsBuf;
	std::streamoff						m_startOfSectionsOffset;

	IMAGE_DOS_HEADER					m_dosHeader;
	IMAGE_FILE_HEADER					m_fileHeader;
	IMAGE_OPTIONAL_HEADER64				m_optionalHeader;

	bool								m_bIs64Bit;
};

#endif