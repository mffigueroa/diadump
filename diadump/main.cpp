#include <iostream>
#include <fstream>
#include <stdlib.h>

#include "Disassembler.h"
#include "Utility.h"

using namespace std;

//
//	Program flow:
//
//	1. Load the given PE file.
//
//	2. In the PE structure, find the PDB filename.
//
//	3. Load the PDB file.
//
//	4. Start recursive descent disassembly at PE entry point.
//
//	5. At first instruction and any jmp or call instruction,
//		check if the target is some function we recognize.
//
//			a. If so, start outputting instruction to type mappings.
//			b. Otherwise, don't output anything as we won't have type information.
//				Although this may not necessarily be the case, we will make this assumption for now.

int wmain(int argc, wchar_t* argv[])
{
	if(argc < 2) {
		wcout << L"Usage: " << argv[0] << " exeFilename [outDumpFilename]" << endl;
		system("pause");
		return 1;
	}

	Disassembler disas(argv[1]);
	
	if(!disas.DisassembleFunctions())
	{
		wcout << L"Error: Unable to disassemble functions." << endl;
		system("pause");
	}

	wchar_t* outFilename;

	if(argc > 2) {
		outFilename = argv[2];
	} else {
		outFilename = L"exedump_out.txt";
	}
    
	wofstream outDump(outFilename, ios::out);

	const vector<Function>& functions = disas.GetFunctions();

	//wcout << endl << endl << endl << L"Disassembled functions (check output for disassembled instructions)" << endl << endl;

	for(vector<Function>::const_iterator i = functions.begin(), i_end = functions.end();
		i != i_end; ++i) {

			//wcout << endl << i->compiland << endl << i->name << endl << endl;
			disas.OutputFunctionDisassembly(i, outDump);
	}

	//wcout << endl << endl << endl;

	system("pause");
	return 0;
}