#ifndef __PESECTION_H__
#define __PESECTION_H__

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinNT.h>

class PESection
{
public:
	PESection();
	explicit PESection(const IMAGE_SECTION_HEADER& sectionHeader);

	const IMAGE_SECTION_HEADER& getSectionHeader() const;

private:
	IMAGE_SECTION_HEADER m_sectionHeader;
};

#endif