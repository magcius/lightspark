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

#ifndef _FLASH_NET_H
#define _FLASH_NET_H

#include "compat.h"
#include "asobject.h"
#include "flash/net/NetConnection.h"
#include "flash/net/NetStream.h"
#include "flash/net/ObjectEncoding.h"

namespace lightspark
{

class URLRequest: public ASObject
{
private:
	tiny_string url;
	_NR<ASObject> data;
	enum METHOD { GET=0, POST };
	METHOD method;
public:
	URLRequest();
	void finalize();
	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);
	ASFUNCTION(_constructor);
	ASFUNCTION(_getURL);
	ASFUNCTION(_setURL);
	ASFUNCTION(_getMethod);
	ASFUNCTION(_setMethod);
	ASFUNCTION(_setData);
	ASFUNCTION(_getData);
	URLInfo getRequestURL() const;
	void getPostData(std::vector<uint8_t>& data) const;
};

class URLRequestMethod: public ASObject
{
public:
	static void sinit(Class_base*);
};

class URLVariables: public ASObject
{
private:
	void decode(const tiny_string& s);
	tiny_string toString_priv();
public:
	URLVariables(){}
	URLVariables(const tiny_string& s);
	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);
	ASFUNCTION(_constructor);
	ASFUNCTION(decode);
	ASFUNCTION(_toString);
	tiny_string toString();
};

class URLLoaderDataFormat: public ASObject
{
public:
	static void sinit(Class_base*);
};

class SharedObject: public EventDispatcher
{
public:
	static void sinit(Class_base*);
};

class URLLoader: public EventDispatcher, public IThreadJob
{
private:
	tiny_string dataFormat;
	URLInfo url;
	_NR<ASObject> data;
	Spinlock downloaderLock;
	Downloader* downloader;
	std::vector<uint8_t> postData;
	void execute();
	void threadAbort();
	void jobFence();
public:
	URLLoader();
	void finalize();
	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);
	ASFUNCTION(_constructor);
	ASFUNCTION(load);
	ASFUNCTION(_getDataFormat);
	ASFUNCTION(_getData);
	ASFUNCTION(_setDataFormat);
};

ASObject* sendToURL(ASObject* obj,ASObject* const* args, const unsigned int argslen);

};

#endif
