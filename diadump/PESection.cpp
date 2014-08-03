#include "PESection.h"

PESection::PESection()
{
	memset(reinterpret_cast<void*>(&m_sectionHeader), 0, sizeof(m_sectionHeader));
}

PESection::PESection(const IMAGE_SECTION_HEADER& sectionHeader)
{
	memcpy_s(reinterpret_cast<void*>(&m_sectionHeader), sizeof(m_sectionHeader),
		reinterpret_cast<const void*>(&sectionHeader), sizeof(sectionHeader));
}

const IMAGE_SECTION_HEADER& PESection::getSectionHeader() const
{
	return m_sectionHeader;
}