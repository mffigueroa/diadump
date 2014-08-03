#include <memory>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atlbase.h>
#include <dia2.h>
#include "Utility.h"
#include "Type.h"

using namespace std::tr1;

Type::Type(CComPtr<IDiaSymbol> datumType)
: m_subtypes(0), m_modifier(None)
{
	m_bKnownType = true;

	HRESULT hr;
	
	enum SymTagEnum datumTypeSymTag;
	hr = datumType->get_symTag((DWORD*)&datumTypeSymTag);
	
	if(FAILED(hr))
	{
		m_bKnownType = false;
		return;
	}

	if(datumTypeSymTag == SymTagData) {
		m_bKnownType = false;
		return;
	}
	
	m_bIsBasicType = true;

	CComPtr<IDiaSymbol> finalType = datumType;

	if(datumTypeSymTag == SymTagPointerType)
	{
		finalType.Release();
		hr = datumType->get_type(&finalType);
		
		if(FAILED(hr))
		{
			m_bKnownType = false;
			return;			
		}

		hr = finalType->get_symTag((DWORD*)&datumTypeSymTag);
		
		if(FAILED(hr))
		{
			m_bKnownType = false;
			return;
		}

		m_modifier = Pointer;
	}
	else if(datumTypeSymTag == SymTagArrayType)
	{
		finalType.Release();
		hr = datumType->get_type(&finalType);

		if(FAILED(hr))
		{
			m_bKnownType = false;
			return;
		}

		hr = finalType->get_symTag((DWORD*)&datumTypeSymTag);

		if(FAILED(hr))
		{
			m_bKnownType = false;
			return;
		}

		m_modifier = Array;
	}

	if(datumTypeSymTag == SymTagBaseType)
	{
		hr = finalType->get_baseType((DWORD*)&m_basicType);

		if(FAILED(hr))
		{
			m_bKnownType = false;
			return;
		}
	}
	else if(datumTypeSymTag == SymTagUDT)
	{
		m_bIsBasicType = false;

		CComPtr<IDiaEnumSymbols>	enumChildren;
		CComPtr<IDiaSymbol>			currChild;
		DWORD						numSymbolsFetched;

		hr = finalType->findChildren(SymTagData, NULL, NULL, &enumChildren);

		if(FAILED(hr))
		{
			m_bKnownType = false;
			return;
		}

		if(FAILED(hr))
		{
			m_bKnownType = false;
			return;
		}

		for(HRESULT moreData = enumChildren->Next(1, &currChild, &numSymbolsFetched);
			moreData == S_OK;
			moreData = enumChildren->Next(1, &currChild, &numSymbolsFetched))
		{
			CComPtr<IDiaSymbol> datumType;
			hr = currChild->get_type(&datumType);

			if(FAILED(hr)) {
				m_subtypes.push_back( shared_ptr<Type>(new Type(datumType)) );
			} else {
				// will return SymTagData on get_symTag, thus
				// filling this Type with a type of m_bKnownType = false
				m_subtypes.push_back( shared_ptr<Type>(new Type(currChild)) );
			}
			currChild.Release();
		}
	}
}

bool Type::GetBasicType(enum BasicType& r_basicType)
{
	if(m_bIsBasicType)
	{
		r_basicType = m_basicType;
		return true;
	}

	return false;
}

const std::vector<std::tr1::shared_ptr<Type>>&	Type::GetSubtypes()
{
	return m_subtypes;
}

bool Type::IsKnownType()
{
	return m_bKnownType;
}