#ifndef __TYPE_H__
#define __TYPE_H__

#include <vector>
#include <memory>
#include <atlbase.h>
#include <cvconst.h>

struct IDiaSymbol;

enum TypeModifier
{
	Pointer,
	Array,
	None
};

class Type
{
public:
	Type(CComPtr<IDiaSymbol> sym);

	bool											GetBasicType(enum BasicType& r_basicType);
	const std::vector<std::tr1::shared_ptr<Type>>&	GetSubtypes();
	bool											IsKnownType();

private:
	bool									m_bIsBasicType;
	bool									m_bKnownType;
	enum BasicType							m_basicType;
	TypeModifier							m_modifier;
	std::vector<std::tr1::shared_ptr<Type>>	m_subtypes;
};

#endif