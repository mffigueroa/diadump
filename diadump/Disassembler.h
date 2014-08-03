#ifndef __DISASSEMBLER_H__
#define __DISASSEMBLER_H__

extern "C"
{
	#include <xed-interface.h>
}

#include <map>
#include <string>
#include <vector>

#include "PE.h"
#include "PDB.h"

typedef struct
{
	size_t				offsetFromFunctionStart;
	std::string			bytes;
	xed_decoded_inst_t	instr;
	bool				validInstruction;
} DisassembledInstruction;

typedef struct
{
	std::vector<DisassembledInstruction>	instructions;
} DisassembledFunction;

class Disassembler
{
public:
	Disassembler(const wchar_t* exeFilename);

	bool										DisassembleFunctions();
	bool										OutputFunctionDisassembly(std::vector<Function>::const_iterator funcIter, std::wostream& out) const;
	const std::vector<Function>&				GetFunctions() const;
	const std::vector<DisassembledFunction>&	GetDisassembledFunctions() const;
	void										PrintOperands(const DisassembledInstruction& instr, const Function& func, std::wostream& out) const;

private:
	PE										m_pe;
	PDB										m_pdb;

	xed_machine_mode_enum_t					m_machineMode;
    xed_address_width_enum_t				m_stackAddrWidth;

	std::vector<Function>					m_functions;
	std::vector<DisassembledFunction>		m_disassembledFunctions;
};

#endif