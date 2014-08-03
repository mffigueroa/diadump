#ifndef __PDB_H__
#define __PDB_H__

#include <memory>
#include <vector>
#include <string>

#include "Type.h"

enum VariableLocation
{
	RegisterRelative,
	MemberVariable,
	ValueInRegister,
	InBitfield,
	StaticRVA,
	StaticSectionOffset,
	Constant,
	UnknownLocation
};

typedef struct
{
	VariableLocation			location;
	long long					offset;
	unsigned long				section;
	unsigned long long			szSize;
	//VARIANT*					value;
	CV_HREG_e					eRegister;
	std::tr1::shared_ptr<Type>	type;
	std::wstring				name;
} Variable;

typedef struct
{
	std::wstring			compiland;
	std::wstring			name;

	unsigned long long		length;
	DWORD					address;

	std::vector<Variable>	parameters;
	std::vector<Variable>	localVariables;
} Function;

class PDB
{
public:
	PDB(const wchar_t* exeFilename);

	bool							FindFunction(unsigned long long address, Function& func);
	bool							FindFunction(unsigned long long address, const Function& func) const;
	const std::vector<Function>&	GetFunctions() const;

private:
	std::vector<Function>			m_functions;
};

#endif