/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2011  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#ifndef TOPLEVEL_VECTOR_H
#define TOPLEVEL_VECTOR_H

#include "asobject.h"

namespace lightspark
{

template<class T> class TemplatedClass;
class Vector: public ASObject
{
	Class_base* vec_type;
	bool fixed;
	std::vector<ASObject*> vec;
public:
	Vector() : vec_type(NULL) {}
	static void sinit(Class_base* c);
	static void buildTraits(ASObject* o) {};
	static ASObject* generator(TemplatedClass<Vector>* o_class, ASObject* const* args, const unsigned int argslen);

	void setTypes(const std::vector<Class_base*>& types);

	//Overloads
	tiny_string toString(bool debugMsg=false);
	_NR<ASObject> getVariableByMultiname(const multiname& name, GET_VARIABLE_OPTION opt);

	//TODO: do we need to implement generator?
	ASFUNCTION(_constructor);
	ASFUNCTION(_applytype);

	ASFUNCTION(push);
};



}
#endif
