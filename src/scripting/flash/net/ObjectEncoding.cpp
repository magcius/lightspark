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

#include "abc.h"
#include "scripting/flash/net/ObjectEncoding.h"

using namespace std;
using namespace lightspark;

SET_NAMESPACE("flash.net");

REGISTER_CLASS_NAME(ObjectEncoding);

void ObjectEncoding::sinit(Class_base* c)
{
	c->setSuper(Class<ASObject>::getRef());
	c->setVariableByQName("AMF0","",abstract_i(AMF0),DECLARED_TRAIT);
	c->setVariableByQName("AMF3","",abstract_i(AMF3),DECLARED_TRAIT);
	c->setVariableByQName("DEFAULT","",abstract_i(DEFAULT),DECLARED_TRAIT);
};
