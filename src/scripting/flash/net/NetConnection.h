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

#ifndef _FLASH_NET_NET_CONNECTION_H
#define _FLASH_NET_NET_CONNECTION_H

#include "compat.h"
#include "asobject.h"

namespace lightspark {

class NetStream;

class NetConnection: public EventDispatcher
{
friend class NetStream;
private:
	//Indicates whether the application is connected to a server through a persistent RMTP connection/HTTP server with Flash Remoting
	bool _connected;
	//The connection is to a flash media server
	ObjectEncoding::ENCODING objectEncoding;
	tiny_string protocol;
	URLInfo uri;
public:
	NetConnection();
	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);
	ASFUNCTION(_constructor);
	ASFUNCTION(connect);
	ASFUNCTION(_getConnected);
	ASFUNCTION(_getDefaultObjectEncoding);
	ASFUNCTION(_setDefaultObjectEncoding);
	ASFUNCTION(_getObjectEncoding);
	ASFUNCTION(_setObjectEncoding);
	ASFUNCTION(_getProtocol);
	ASFUNCTION(_getURI);
};

}

#endif
