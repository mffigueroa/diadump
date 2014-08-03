extern "C"
{
#include <xed-interface.h>
}

#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>

#include "Disassembler.h"

using namespace std;

xed_reg_enum_t	PDBRegToDisasReg(CV_HREG_e reg);

Disassembler::Disassembler(const wchar_t* exeFilename)
	: m_pe(exeFilename), m_pdb(exeFilename)
{
	m_functions = m_pdb.GetFunctions();

	xed_tables_init();

	// The state of the machine -- required for decoding
    if (m_pe.Is64Bit()) {
        m_machineMode = XED_MACHINE_MODE_LONG_64;
        m_stackAddrWidth = XED_ADDRESS_WIDTH_64b;
    }
    else {
        m_machineMode = XED_MACHINE_MODE_LEGACY_32;
        m_stackAddrWidth = XED_ADDRESS_WIDTH_32b;
    }
}

bool Disassembler::DisassembleFunctions()
{
	for(vector<Function>::const_iterator i = m_functions.begin(), i_end = m_functions.end();
		i != i_end; ++i) {

			DisassembledFunction currFunction;

			unsigned long functionOffset = m_pe.getOffsetForRVA(i->address);
			std::tr1::shared_ptr<const unsigned char> functionCode(m_pe.getSectionsBuf(), m_pe.getSectionsBuf().get() + functionOffset);

			for(size_t offset = 0; offset < i->length;) {
				DisassembledInstruction currInstr;

				xed_error_enum_t	xed_error;
				xed_decoded_inst_t	xedd;

				xed_decoded_inst_zero(&xedd);
				xed_decoded_inst_set_mode(&xedd, m_machineMode, m_stackAddrWidth);

				xed_error = xed_decode(&xedd, 
					XED_STATIC_CAST(const xed_uint8_t*, functionCode.get() + offset),
					15);
		
				if(xed_error == XED_ERROR_NONE) {
					xed_uint_t instrLen = xed_decoded_inst_get_length(&xedd);
					
					currInstr.instr = xedd;
					currInstr.offsetFromFunctionStart = offset;
					currInstr.bytes.resize(instrLen);
					memcpy_s(&currInstr.bytes[0], instrLen, functionCode.get() + offset, instrLen);
					currInstr.validInstruction = true;

					offset += instrLen;
				} else {
					wcout	<< L"Invalid instruction:" << endl
							<< i->compiland << endl
							<< i->name << endl
							<< "Offset: " << offset << endl
							<< "Bytes:";

					for(int byteNum = 0; byteNum < 15; ++byteNum)
						wcout << L" " << hex << nouppercase << setw(2) << setfill(L'0') << *static_cast<const unsigned char*>(functionCode.get() + offset + byteNum);

					wcout << endl << endl;

					xed_decoded_inst_zero(&currInstr.instr);
					currInstr.offsetFromFunctionStart = 0;
					currInstr.validInstruction = false;

					// try again at the next byte
					++offset;
				}

				currFunction.instructions.push_back(currInstr);
			}

			m_disassembledFunctions.push_back(currFunction);
	}

	return true;
}

bool Disassembler::OutputFunctionDisassembly(vector<Function>::const_iterator funcIter, wostream& out) const
{
	unsigned long long funcAddr = m_pe.getImageBase() + funcIter->address;

	vector<DisassembledFunction>::const_iterator disasFuncIter = m_disassembledFunctions.begin() + (funcIter - m_functions.begin());

	out << endl
		<< funcIter->compiland << endl
		<< funcIter->name << endl
		<< L"0x" << hex << uppercase << setw(16) << setfill(L'0') << funcAddr << L" - "
		<< L"0x" << hex << uppercase << setw(16) << setfill(L'0') << funcAddr + funcIter->length - 1 << endl
		<< endl;

	string instrDumpStr;
	instrDumpStr.resize(256);

	for(vector<DisassembledInstruction>::const_iterator i = disasFuncIter->instructions.begin(), i_end = disasFuncIter->instructions.end();
		i != i_end; ++i) {

			unsigned long long instrAddr = funcAddr + i->offsetFromFunctionStart;
			out << L"0x" << hex << uppercase << setw(16) << setfill(L'0') << right << instrAddr << L" ";

			xed_decoded_inst_dump_intel_format(&i->instr, &instrDumpStr[0], 255, instrAddr);

			wstring tmpWideStr(instrDumpStr.begin(), instrDumpStr.begin() + strlen(instrDumpStr.c_str()));
			out << setfill(L' ') << setw(40) << left << tmpWideStr;

			wstringstream bytes;

			for(size_t byteNum = 0, byteNum_end = i->bytes.length(); byteNum < byteNum_end; ++byteNum)
						bytes << L" " << hex << nouppercase << setw(2) << setfill(L'0') << right << static_cast<const unsigned char>(i->bytes[byteNum]);

			out << setw(45) << bytes.str();

			// if we hit a 'ret' instruction, don't read further
			if(xed_decoded_inst_get_category(&i->instr) == XED_CATEGORY_RET) {
				out << endl;
				break;
			}

			PrintOperands(*i, *funcIter, out);

			out << endl;
	}

	return true;
}

const vector<Function>& Disassembler::GetFunctions() const
{
	return m_functions;
}

const vector<DisassembledFunction>& Disassembler::GetDisassembledFunctions() const
{
	return m_disassembledFunctions;
}

void Disassembler::PrintOperands(const DisassembledInstruction& instr, const Function& func, std::wostream& out) const
{
	const xed_inst_t* xi = xed_decoded_inst_inst(&instr.instr);
    const xed_operand_values_t* operandValues = xed_decoded_inst_operands_const(&instr.instr);
    size_t numOperands = xed_inst_noperands(xi);

	for(size_t opNum = 0; opNum < numOperands; ++opNum) {
		const xed_operand_t*	op = xed_inst_operand(xi, opNum);
		xed_operand_type_enum_t opType = xed_operand_type(op);
		xed_operand_enum_t		opName = xed_operand_name(op);

		if(opType == XED_OPERAND_TYPE_REG || XED_OPERAND_TYPE_NT_LOOKUP_FN) {
			xed_reg_enum_t reg = xed_decoded_inst_get_reg(&instr.instr, opName);

			for(vector<Variable>::const_iterator var = func.localVariables.begin(), var_end = func.localVariables.end();
				var != var_end; ++var) {

					if(var->location == ValueInRegister && PDBRegToDisasReg(var->eRegister) == reg) {
						out << " " << xed_reg_enum_t2str(reg) << " = " << var->name << " ";
						break;
					}
			}
		} /*else if(opType == XED_OPERAND_TYPE_IMM || opType == XED_OPERAND_TYPE_IMM_CONST) {
			xed_uint32_t opValue = xed_operand_imm(op);

			for(vector<Variable>::const_iterator var = func.localVariables.begin(), var_end = func.localVariables.end();
				var != var_end; ++var) {

					if(var->location == Constant && var->value == opValue) {
						break;
					}
			}
		}*/
	}

	size_t memops = xed_decoded_inst_number_of_memory_operands(&instr.instr);

	for(size_t i = 0; i < memops; ++i) {
		// for now, not handling this case as it involves
		// paying attention to data flow
		if(	xed_decoded_inst_get_index_reg(&instr.instr,i) != XED_REG_INVALID ||
			!xed_decoded_inst_get_memory_displacement_width(&instr.instr,i))
				continue;

		xed_reg_enum_t baseReg = xed_decoded_inst_get_base_reg(&instr.instr,i);
		long long displacement = xed_decoded_inst_get_memory_displacement(&instr.instr,i);

		bool foundVar = false;
		
		for(vector<Variable>::const_iterator var = func.localVariables.begin(), var_end = func.localVariables.end();
			var != var_end; ++var) {

				if(var->location == RegisterRelative && PDBRegToDisasReg(var->eRegister) == baseReg && var->offset == displacement) {
					foundVar = true;
					out << " " << xed_reg_enum_t2str(baseReg);

					if(displacement >= 0) {
						out << " + " << hex << nouppercase << "0x" << displacement << " = " << var->name << " ";
					} else {
						out << " - " << hex << nouppercase << "0x" << -displacement << " = " << var->name << " ";
					}

					break;
				}
		}

		if(foundVar)
			break;

		for(vector<Variable>::const_iterator var = func.parameters.begin(), var_end = func.parameters.end();
			var != var_end; ++var) {

				if(var->location == RegisterRelative && PDBRegToDisasReg(var->eRegister) == baseReg && var->offset == displacement) {
					out << " " << xed_reg_enum_t2str(baseReg);

					if(displacement >= 0) {
						out << " + " << hex << nouppercase << "0x" << displacement << " = " << var->name << " ";
					} else {
						out << " - " << hex << nouppercase << "0x" << -displacement << " = " << var->name << " ";
					}

					foundVar = true;
					break;
				}
		}

		if(foundVar)
			break;
	}


	;
}

xed_reg_enum_t	PDBRegToDisasReg(CV_HREG_e reg)
{
	if(reg == CV_AMD64_BPL) {
		return XED_REG_BPL;
	} if(reg == CV_AMD64_CR8) {
		return XED_REG_CR8;
	} if(reg == CV_AMD64_DIL) {
		return XED_REG_DIL;
	} if(reg == CV_AMD64_DR10) {
		return XED_REG_DR10;
	} if(reg == CV_AMD64_DR11) {
		return XED_REG_DR11;
	} if(reg == CV_AMD64_DR12) {
		return XED_REG_DR12;
	} if(reg == CV_AMD64_DR13) {
		return XED_REG_DR13;
	} if(reg == CV_AMD64_DR14) {
		return XED_REG_DR14;
	} if(reg == CV_AMD64_DR15) {
		return XED_REG_DR15;
	} if(reg == CV_AMD64_DR8) {
		return XED_REG_DR8;
	} if(reg == CV_AMD64_DR9) {
		return XED_REG_DR9;
	} if(reg == CV_AMD64_EFLAGS) {
		return XED_REG_EFLAGS;
	} if(reg == CV_AMD64_FLAGS) {
		return XED_REG_RFLAGS;
	} if(reg == CV_AMD64_R10) {
		return XED_REG_R10;
	} if(reg == CV_AMD64_R10B) {
		return XED_REG_R10B;
	} if(reg == CV_AMD64_R10D) {
		return XED_REG_R10D;
	} if(reg == CV_AMD64_R10W) {
		return XED_REG_R10W;
	} if(reg == CV_AMD64_R11) {
		return XED_REG_R11;
	} if(reg == CV_AMD64_R11B) {
		return XED_REG_R11B;
	} if(reg == CV_AMD64_R11D) {
		return XED_REG_R11D;
	} if(reg == CV_AMD64_R11W) {
		return XED_REG_R11W;
	} if(reg == CV_AMD64_R12) {
		return XED_REG_R12;
	} if(reg == CV_AMD64_R12B) {
		return XED_REG_R12B;
	} if(reg == CV_AMD64_R12D) {
		return XED_REG_R12D;
	} if(reg == CV_AMD64_R12W) {
		return XED_REG_R12W;
	} if(reg == CV_AMD64_R13) {
		return XED_REG_R13;
	} if(reg == CV_AMD64_R13B) {
		return XED_REG_R13B;
	} if(reg == CV_AMD64_R13D) {
		return XED_REG_R13D;
	} if(reg == CV_AMD64_R13W) {
		return XED_REG_R13W;
	} if(reg == CV_AMD64_R14) {
		return XED_REG_R14;
	} if(reg == CV_AMD64_R14B) {
		return XED_REG_R14B;
	} if(reg == CV_AMD64_R14D) {
		return XED_REG_R14D;
	} if(reg == CV_AMD64_R14W) {
		return XED_REG_R14W;
	} if(reg == CV_AMD64_R15) {
		return XED_REG_R15;
	} if(reg == CV_AMD64_R15B) {
		return XED_REG_R15B;
	} if(reg == CV_AMD64_R15D) {
		return XED_REG_R15D;
	} if(reg == CV_AMD64_R15W) {
		return XED_REG_R15W;
	} if(reg == CV_AMD64_R8) {
		return XED_REG_R8;
	} if(reg == CV_AMD64_R8B) {
		return XED_REG_R8B;
	} if(reg == CV_AMD64_R8D) {
		return XED_REG_R8D;
	} if(reg == CV_AMD64_R8W) {
		return XED_REG_R8W;
	} if(reg == CV_AMD64_R9) {
		return XED_REG_R9;
	} if(reg == CV_AMD64_R9B) {
		return XED_REG_R9B;
	} if(reg == CV_AMD64_R9D) {
		return XED_REG_R9D;
	} if(reg == CV_AMD64_R9W) {
		return XED_REG_R9W;
	} if(reg == CV_AMD64_RAX) {
		return XED_REG_RAX;
	} if(reg == CV_AMD64_RBP) {
		return XED_REG_RBP;
	} if(reg == CV_AMD64_RBX) {
		return XED_REG_RBX;
	} if(reg == CV_AMD64_RCX) {
		return XED_REG_RCX;
	} if(reg == CV_AMD64_RDI) {
		return XED_REG_RDI;
	} if(reg == CV_AMD64_RDX) {
		return XED_REG_RDX;
	} if(reg == CV_AMD64_RIP) {
		return XED_REG_RIP;
	} if(reg == CV_AMD64_RSI) {
		return XED_REG_RSI;
	} if(reg == CV_AMD64_RSP) {
		return XED_REG_RSP;
	} if(reg == CV_AMD64_SIL) {
		return XED_REG_SIL;
	} if(reg == CV_AMD64_SPL) {
		return XED_REG_SPL;
	} if(reg == CV_AMD64_XMM14) {
		return XED_REG_XMM14;
	} if(reg == CV_AMD64_XMM15) {
		return XED_REG_XMM15;
	} if(reg == CV_AMD64_XMM8) {
		return XED_REG_XMM8;
	} if(reg == CV_AMD64_XMM9) {
		return XED_REG_XMM9;
	} if(reg == CV_AMD64_YMM10) {
		return XED_REG_YMM10;
	} if(reg == CV_AMD64_YMM11) {
		return XED_REG_YMM11;
	} if(reg == CV_AMD64_YMM12) {
		return XED_REG_YMM12;
	} if(reg == CV_AMD64_YMM13) {
		return XED_REG_YMM13;
	} if(reg == CV_AMD64_YMM14) {
		return XED_REG_YMM14;
	} if(reg == CV_AMD64_YMM15) {
		return XED_REG_YMM15;
	} if(reg == CV_AMD64_YMM8) {
		return XED_REG_YMM8;
	} if(reg == CV_AMD64_YMM9) {
		return XED_REG_YMM9;
	} if(reg == CV_REG_AH) {
		return XED_REG_AH;
	} if(reg == CV_REG_AL) {
		return XED_REG_AL;
	} if(reg == CV_REG_AX) {
		return XED_REG_AX;
	} if(reg == CV_REG_BH) {
		return XED_REG_BH;
	} if(reg == CV_REG_BL) {
		return XED_REG_BL;
	} if(reg == CV_REG_BP) {
		return XED_REG_BP;
	} if(reg == CV_REG_BX) {
		return XED_REG_BX;
	} if(reg == CV_REG_CH) {
		return XED_REG_CH;
	} if(reg == CV_REG_CL) {
		return XED_REG_CL;
	} if(reg == CV_REG_CR0) {
		return XED_REG_CR0;
	} if(reg == CV_REG_CR1) {
		return XED_REG_CR1;
	} if(reg == CV_REG_CR2) {
		return XED_REG_CR2;
	} if(reg == CV_REG_CR3) {
		return XED_REG_CR3;
	} if(reg == CV_REG_CR4) {
		return XED_REG_CR4;
	} if(reg == CV_REG_CS) {
		return XED_REG_CS;
	} if(reg == CV_REG_CX) {
		return XED_REG_CX;
	} if(reg == CV_REG_DH) {
		return XED_REG_DH;
	} if(reg == CV_REG_DI) {
		return XED_REG_DI;
	} if(reg == CV_REG_DL) {
		return XED_REG_DL;
	} if(reg == CV_REG_DR0) {
		return XED_REG_DR0;
	} if(reg == CV_REG_DR1) {
		return XED_REG_DR1;
	} if(reg == CV_REG_DR2) {
		return XED_REG_DR2;
	} if(reg == CV_REG_DR3) {
		return XED_REG_DR3;
	} if(reg == CV_REG_DR4) {
		return XED_REG_DR4;
	} if(reg == CV_REG_DR5) {
		return XED_REG_DR5;
	} if(reg == CV_REG_DR6) {
		return XED_REG_DR6;
	} if(reg == CV_REG_DR7) {
		return XED_REG_DR7;
	} if(reg == CV_REG_DS) {
		return XED_REG_DS;
	} if(reg == CV_REG_DX) {
		return XED_REG_DX;
	} if(reg == CV_REG_EAX) {
		return XED_REG_EAX;
	} if(reg == CV_REG_EBP) {
		return XED_REG_EBP;
	} if(reg == CV_REG_EBX) {
		return XED_REG_EBX;
	} if(reg == CV_REG_ECX) {
		return XED_REG_ECX;
	} if(reg == CV_REG_EDI) {
		return XED_REG_EDI;
	} if(reg == CV_REG_EDX) {
		return XED_REG_EDX;
	} if(reg == CV_REG_EFLAGS) {
		return XED_REG_EFLAGS;
	} if(reg == CV_REG_EIP) {
		return XED_REG_EIP;
	} if(reg == CV_REG_ES) {
		return XED_REG_ES;
	} if(reg == CV_REG_ESI) {
		return XED_REG_ESI;
	} if(reg == CV_REG_ESP) {
		return XED_REG_ESP;
	} if(reg == CV_REG_FLAGS) {
		return XED_REG_FLAGS;
	} if(reg == CV_REG_FS) {
		return XED_REG_FS;
	} if(reg == CV_REG_GDTR) {
		return XED_REG_GDTR;
	} if(reg == CV_REG_GS) {
		return XED_REG_GS;
	} if(reg == CV_REG_IDTR) {
		return XED_REG_IDTR;
	} if(reg == CV_REG_IP) {
		return XED_REG_IP;
	} if(reg == CV_REG_LDTR) {
		return XED_REG_LDTR;
	} if(reg == CV_REG_MXCSR) {
		return XED_REG_MXCSR;
	} if(reg == CV_REG_SI) {
		return XED_REG_SI;
	} if(reg == CV_REG_SP) {
		return XED_REG_SP;
	} if(reg == CV_REG_SS) {
		return XED_REG_SS;
	} if(reg == CV_REG_ST0) {
		return XED_REG_ST0;
	} if(reg == CV_REG_ST1) {
		return XED_REG_ST1;
	} if(reg == CV_REG_ST2) {
		return XED_REG_ST2;
	} if(reg == CV_REG_ST3) {
		return XED_REG_ST3;
	} if(reg == CV_REG_ST4) {
		return XED_REG_ST4;
	} if(reg == CV_REG_ST5) {
		return XED_REG_ST5;
	} if(reg == CV_REG_ST6) {
		return XED_REG_ST6;
	} if(reg == CV_REG_ST7) {
		return XED_REG_ST7;
	} if(reg == CV_REG_TR) {
		return XED_REG_TR;
	} if(reg == CV_REG_XMM0) {
		return XED_REG_XMM0;
	} if(reg == CV_REG_XMM1) {
		return XED_REG_XMM1;
	} if(reg == CV_REG_XMM10) {
		return XED_REG_XMM10;
	} if(reg == CV_REG_XMM11) {
		return XED_REG_XMM11;
	} if(reg == CV_REG_XMM12) {
		return XED_REG_XMM12;
	} if(reg == CV_REG_XMM13) {
		return XED_REG_XMM13;
	} if(reg == CV_REG_XMM2) {
		return XED_REG_XMM2;
	} if(reg == CV_REG_XMM3) {
		return XED_REG_XMM3;
	} if(reg == CV_REG_XMM4) {
		return XED_REG_XMM4;
	} if(reg == CV_REG_XMM5) {
		return XED_REG_XMM5;
	} if(reg == CV_REG_XMM6) {
		return XED_REG_XMM6;
	} if(reg == CV_REG_XMM7) {
		return XED_REG_XMM7;
	} if(reg == CV_REG_YMM0) {
		return XED_REG_YMM0;
	} if(reg == CV_REG_YMM1) {
		return XED_REG_YMM1;
	} if(reg == CV_REG_YMM2) {
		return XED_REG_YMM2;
	} if(reg == CV_REG_YMM3) {
		return XED_REG_YMM3;
	} if(reg == CV_REG_YMM4) {
		return XED_REG_YMM4;
	} if(reg == CV_REG_YMM5) {
		return XED_REG_YMM5;
	} if(reg == CV_REG_YMM6) {
		return XED_REG_YMM6;
	} if(reg == CV_REG_YMM7) {
		return XED_REG_YMM7;
	} 
	
	return XED_REG_INVALID;

	/* UNMAPPED CV_HREG_e's
	*
	*		XED_REG_CR5
	*		XED_REG_CR6
	*		XED_REG_CR7
	*		XED_REG_CR9
	*		XED_REG_CR10
	*		XED_REG_CR11
	*		XED_REG_CR12
	*		XED_REG_CR13
	*		XED_REG_CR14
	*		XED_REG_CR15
	*		XED_REG_ERROR
	*		XED_REG_MMX0
	*		XED_REG_MMX1
	*		XED_REG_MMX2
	*		XED_REG_MMX3
	*		XED_REG_MMX4
	*		XED_REG_MMX5
	*		XED_REG_MMX6
	*		XED_REG_MMX7
	*		XED_REG_STACKPUSH
	*		XED_REG_STACKPOP
	*		XED_REG_TSC
	*		XED_REG_TSCAUX
	*		XED_REG_MSRS
	*		XED_REG_FSBASE
	*		XED_REG_GSBASE
	*		XED_REG_X87CONTROL
	*		XED_REG_X87STATUS
	*		XED_REG_X87TAG
	*		XED_REG_X87PUSH
	*		XED_REG_X87POP
	*		XED_REG_X87POP2
	*		XED_REG_X87OPCODE
	*		XED_REG_X87LASTCS
	*		XED_REG_X87LASTIP
	*		XED_REG_X87LASTDS
	*		XED_REG_X87LASTDP
	*		XED_REG_TMP0
	*		XED_REG_TMP1
	*		XED_REG_TMP2
	*		XED_REG_TMP3
	*		XED_REG_TMP4
	*		XED_REG_TMP5
	*		XED_REG_TMP6
	*		XED_REG_TMP7
	*		XED_REG_TMP8
	*		XED_REG_TMP9
	*		XED_REG_TMP10
	*		XED_REG_TMP11
	*		XED_REG_TMP12
	*		XED_REG_TMP13
	*		XED_REG_TMP14
	*		XED_REG_TMP15
	*		XED_REG_XCR0
	*		
	*
	*/
}