#include <atlbase.h>
#include <string>
#include <dia2.h>
#include <stdexcept>
#include "PDB.h"

using namespace std;
using namespace std::tr1;

PDB::PDB(const wchar_t* exeFilename)
{
	DWORD	tmpDwordValue;
	LONG	tmpLongValue;

	HRESULT hr = CoInitialize(NULL);

	if(hr != S_OK) {
		throw runtime_error("Unable to initialize COM interface.");
	}

	CComPtr<IDiaDataSource> dataSrc;

	hr = CoCreateInstance(CLSID_DiaSource,
					NULL,
					CLSCTX_INPROC_SERVER,
					__uuidof(IDiaDataSource),
					(void**)&dataSrc);
	
	if(hr != S_OK) {
		throw runtime_error("Unable to create IDiaDataSource interface.");
	}

	hr = dataSrc->loadDataForExe(exeFilename, NULL, NULL);

	if(hr != S_OK) {
		throw runtime_error("Unable to load PDB for given EXE.");
	}

	CComPtr<IDiaSession> session;

	hr = dataSrc->openSession(&session);

	if(hr != S_OK) {
		throw runtime_error("Unable to create IDiaSession.");
	}

	CComPtr<IDiaSymbol> globalScope;

	hr = session->get_globalScope(&globalScope);

	if(hr != S_OK) {
		throw runtime_error("Unable to get PDB's global scope.");
	}

	CComPtr<IDiaEnumSymbols> compilands;

	hr = globalScope->findChildren(SymTagCompiland, NULL, NULL, &compilands);

	if(hr != S_OK) {
		throw runtime_error("Unable to find PDB's compilands.");
	}

	CComPtr<IDiaSymbol> currCompiland;
	DWORD numSymbolsFetched;

	for(HRESULT moreChildren = compilands->Next(1, &currCompiland, &numSymbolsFetched);
		moreChildren == S_OK; moreChildren = compilands->Next(1, &currCompiland, &numSymbolsFetched))	{

			BSTR pName;
			hr = currCompiland->get_name(&pName);

			wstring compilandName;

			if(hr == S_OK) {
				compilandName = pName;
				SysFreeString(pName);
			} else {
				compilandName = L"UnnamedCompiland";
			}

			CComPtr<IDiaEnumSymbols>	functions;
			CComPtr<IDiaSymbol>			currFunction;

			hr = currCompiland->findChildren(SymTagFunction, NULL, NULL, &functions);

			if(hr != S_OK) {
				currCompiland.Release();
				continue;
			}

			for(HRESULT moreFuncs = functions->Next(1, &currFunction, &numSymbolsFetched);
				moreFuncs == S_OK; moreFuncs = functions->Next(1, &currFunction, &numSymbolsFetched)) {

					Function stCurrFunction;

					stCurrFunction.compiland = compilandName;

					hr = currFunction->get_name(&pName);

					if(hr == S_OK) {
						stCurrFunction.name = pName;
						SysFreeString(pName);
					} else {
						stCurrFunction.name = L"UnnamedFunction";
					}					

					hr = currFunction->get_relativeVirtualAddress(&stCurrFunction.address);

					// if we can't get the function's address,
					// then we can't use it.
					// Of course its possible it has a different
					// location type stored for it, which
					// can be looked into later. But one way or
					// another we need an address.
					if(hr != S_OK) {
						currFunction.Release();
						continue;
					}

					hr = currFunction->get_length(&stCurrFunction.length);

					if(hr != S_OK) {
						currFunction.Release();
						continue;
					}

					CComPtr<IDiaEnumSymbols>	funcData;
					CComPtr<IDiaSymbol>			currFuncDatum;

					hr = currFunction->findChildren(SymTagData, NULL, NULL, &funcData);

					if(hr != S_OK) {
						currFunction.Release();
						continue;
					}

					for(HRESULT moreData = funcData->Next(1, &currFuncDatum, &numSymbolsFetched);
						moreData == S_OK; moreData = funcData->Next(1, &currFuncDatum, &numSymbolsFetched)) {

							Variable currVar;

							currVar.location = UnknownLocation;
							currVar.offset = 0;
							currVar.section = 0;
							currVar.szSize = 0;
							currVar.eRegister = CV_REG_NONE;

							hr = currFuncDatum->get_name(&pName);

							if(hr == S_OK && pName && *pName) {
								currVar.name = pName;
								SysFreeString(pName);
							} else {
								currVar.name = L"NoName";
							}

							CComPtr<IDiaSymbol> datumType;
							hr = currFuncDatum->get_type(&datumType);
				
							if(hr == S_OK)
								currVar.type = shared_ptr<Type>(new Type(datumType));
							else
								currVar.type = 0;

							enum LocationType locType;
							hr = currFuncDatum->get_locationType(&tmpDwordValue);

							if(hr == S_OK) { 
								locType = *((LocationType*)&tmpDwordValue);
							} else {
								locType = LocIsNull;
							}

							currVar.location = UnknownLocation;

							if(locType == LocIsRegRel) {
								if(	currFuncDatum->get_registerId(&tmpDwordValue) == S_OK &&
									currFuncDatum->get_offset(&tmpLongValue) == S_OK ) {

										currVar.location = RegisterRelative;
										currVar.eRegister = static_cast<CV_HREG_e>(tmpDwordValue);
										currVar.offset = static_cast<long long>(tmpLongValue);

								}
							} else if(locType == LocIsStatic) {
								if(			currFuncDatum->get_relativeVirtualAddress(&tmpDwordValue) == S_OK) {

												currVar.offset = static_cast<long long>(tmpDwordValue);
												currVar.location = StaticRVA;

								} else if(	currFuncDatum->get_addressOffset(&tmpDwordValue) == S_OK &&
											currFuncDatum->get_addressSection(&currVar.section) == S_OK) {

												currVar.offset = static_cast<long long>(tmpDwordValue);
												currVar.location = StaticSectionOffset;

								}
							} else if(locType == LocIsThisRel) {

								if(currFuncDatum->get_offset(&tmpLongValue) == S_OK) {
									currVar.offset = static_cast<long long>(tmpLongValue);
									currVar.location = MemberVariable;
								}
							} else if(locType == LocIsEnregistered) {

								if(currFuncDatum->get_registerId(&tmpDwordValue) == S_OK) {
									currVar.location = ValueInRegister;
									currVar.eRegister = static_cast<CV_HREG_e>(tmpDwordValue);
								}
							} else if(locType == LocIsBitField) {

								if(	currFuncDatum->get_offset(&tmpLongValue)			== S_OK &&
									currFuncDatum->get_bitPosition(&currVar.section)	== S_OK &&
									currFuncDatum->get_length(&currVar.szSize)			== S_OK ) {

										currVar.location = InBitfield;
										currVar.offset = static_cast<long long>(tmpLongValue);
										
								}
							}

							if(currVar.location == UnknownLocation) {
								continue;
							}

							enum DataKind funcDatumKind;
							hr = currFuncDatum->get_dataKind(&tmpDwordValue);
							funcDatumKind = static_cast<DataKind>(tmpDwordValue);

							if(hr == S_OK) {

								if(funcDatumKind == DataIsParam) {	
									
									stCurrFunction.parameters.push_back(currVar);

								} else if(	funcDatumKind == DataIsLocal ||
											funcDatumKind == DataIsStaticLocal	) {

									stCurrFunction.localVariables.push_back(currVar);

								} else {
									continue;
								}
							} else {
								continue;
							}

							currFuncDatum.Release();
					}

					m_functions.push_back(stCurrFunction);
					currFunction.Release();
			}

			functions.Release();
			currCompiland.Release();
	}

	compilands.Release();
	globalScope.Release();
	session.Release();
	dataSrc.Release();
}

bool PDB::FindFunction(unsigned long long address, Function &func)
{
	for(vector<Function>::const_iterator i = m_functions.begin(), i_end = m_functions.end();
		i != i_end; ++i) {
			
			if(i->address <= address && address <= i->address + i->length) {
				func = *i;
				return true;
			}
	}

	return false;
}

const std::vector<Function>& PDB::GetFunctions() const
{
	return m_functions;
}