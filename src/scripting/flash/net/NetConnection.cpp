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
#include "scripting/flash/net/NetConnection.h"

using namespace std;
using namespace lightspark;

SET_NAMESPACE("flash.net");

REGISTER_CLASS_NAME(NetConnection);

NetConnection::NetConnection():_connected(false)
{
}

void NetConnection::sinit(Class_base* c)
{
	//assert(c->constructor==NULL);
	c->setConstructor(Class<IFunction>::getFunction(_constructor));
	c->setSuper(Class<EventDispatcher>::getRef());
	c->setDeclaredMethodByQName("connect","",Class<IFunction>::getFunction(connect),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("connected","",Class<IFunction>::getFunction(_getConnected),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("defaultObjectEncoding","",Class<IFunction>::getFunction(_getDefaultObjectEncoding),GETTER_METHOD,false);
	c->setDeclaredMethodByQName("defaultObjectEncoding","",Class<IFunction>::getFunction(_setDefaultObjectEncoding),SETTER_METHOD,false);
	sys->staticNetConnectionDefaultObjectEncoding = ObjectEncoding::DEFAULT;
	c->setDeclaredMethodByQName("objectEncoding","",Class<IFunction>::getFunction(_getObjectEncoding),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("objectEncoding","",Class<IFunction>::getFunction(_setObjectEncoding),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("protocol","",Class<IFunction>::getFunction(_getProtocol),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("uri","",Class<IFunction>::getFunction(_getURI),GETTER_METHOD,true);
}

void NetConnection::buildTraits(ASObject* o)
{
}

ASFUNCTIONBODY(NetConnection, _constructor)
{
	NetConnection* th=Class<NetConnection>::cast(obj);
	th->objectEncoding = sys->staticNetConnectionDefaultObjectEncoding;
	return NULL;
}
ASFUNCTIONBODY(NetConnection,connect)
{
	NetConnection* th=Class<NetConnection>::cast(obj);
	//This takes 1 required parameter and an unspecified number of optional parameters
	assert_and_throw(argslen>0);

	//This seems strange:
	//LOCAL_WITH_FILE may not use connect(), even if it tries to connect to a local file.
	//I'm following the specification to the letter. Testing showed
	//that the official player allows connect(null) in localWithFile.
	if(args[0]->getObjectType() != T_NULL
	&& sys->securityManager->evaluateSandbox(SecurityManager::LOCAL_WITH_FILE))
		throw Class<SecurityError>::getInstanceS("SecurityError: NetConnection::connect "
				"from LOCAL_WITH_FILE sandbox");
	//Null argument means local file or web server, the spec only mentions NULL, but youtube uses UNDEFINED, so supporting that too.
	if(args[0]->getObjectType()==T_NULL || args[0]->getObjectType()==T_UNDEFINED)
	{
		th->_connected = false;
	}
	//String argument means Flash Remoting/Flash Media Server
	else
	{
		th->_connected = false;
		ASString* command = static_cast<ASString*>(args[0]);
		th->uri = URLInfo(command->toString());

		if(sys->securityManager->evaluatePoliciesURL(th->uri, true) != SecurityManager::ALLOWED)
		{
			//TODO: find correct way of handling this case
			throw Class<SecurityError>::getInstanceS("SecurityError: connection to domain not allowed by securityManager");
		}
		
		if(!(th->uri.getProtocol() == "rtmp" ||
		     th->uri.getProtocol() == "rtmpe" ||
		     th->uri.getProtocol() == "rtmps"))
		{
			throw UnsupportedException("NetConnection::connect: only RTMP is supported");
		}

		// We actually create the connection later in
		// NetStream::play(). For now, we support only
		// streaming, not remoting (NetConnection.call() is
		// not implemented).
	}

	//When the URI is undefined the connect is successful (tested on Adobe player)
	th->incRef();
	getVm()->addEvent(_MR(th), _MR(Class<NetStatusEvent>::getInstanceS("status", "NetConnection.Connect.Success")));
	return NULL;
}

ASFUNCTIONBODY(NetConnection,_getConnected)
{
	NetConnection* th=Class<NetConnection>::cast(obj);
	return abstract_b(th->_connected);
}

ASFUNCTIONBODY(NetConnection,_getDefaultObjectEncoding)
{
	return abstract_i(sys->staticNetConnectionDefaultObjectEncoding);
}

ASFUNCTIONBODY(NetConnection,_setDefaultObjectEncoding)
{
	assert_and_throw(argslen == 1);
	int32_t value = args[0]->toInt();
	if(value == 0)
		sys->staticNetConnectionDefaultObjectEncoding = ObjectEncoding::AMF0;
	else if(value == 3)
		sys->staticNetConnectionDefaultObjectEncoding = ObjectEncoding::AMF3;
	else
		throw RunTimeException("Invalid object encoding");
	return NULL;
}

ASFUNCTIONBODY(NetConnection,_getObjectEncoding)
{
	NetConnection* th=Class<NetConnection>::cast(obj);
	return abstract_i(th->objectEncoding);
}

ASFUNCTIONBODY(NetConnection,_setObjectEncoding)
{
	NetConnection* th=Class<NetConnection>::cast(obj);
	assert_and_throw(argslen == 1);
	if(th->_connected)
	{
		throw Class<ReferenceError>::getInstanceS("set NetConnection.objectEncoding after connect");
	}
	int32_t value = args[0]->toInt();
	if(value == 0)
		th->objectEncoding = ObjectEncoding::AMF0; 
	else
		th->objectEncoding = ObjectEncoding::AMF3; 
	return NULL;
}

ASFUNCTIONBODY(NetConnection,_getProtocol)
{
	NetConnection* th=Class<NetConnection>::cast(obj);
	if(th->_connected)
		return Class<ASString>::getInstanceS(th->protocol);
	else
		throw Class<ArgumentError>::getInstanceS("get NetConnection.protocol before connect");
}

ASFUNCTIONBODY(NetConnection,_getURI)
{
	NetConnection* th=Class<NetConnection>::cast(obj);
	if(th->_connected && th->uri.isValid())
		return Class<ASString>::getInstanceS(th->uri.getURL());
	else
	{
		//Reference says the return should be undefined. The right thing is "null" as a string
		return Class<ASString>::getInstanceS("null");
	}
}
