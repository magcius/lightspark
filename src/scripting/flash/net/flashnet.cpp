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

#include <map>
#include "abc.h"
#include "flashnet.h"
#include "class.h"
#include "flash/system/flashsystem.h"
#include "compat.h"
#include "backends/audio.h"
#include "backends/builtindecoder.h"
#include "backends/rendering.h"
#include "backends/security.h"
#include "argconv.h"

using namespace std;
using namespace lightspark;

SET_NAMESPACE("flash.net");

REGISTER_CLASS_NAME(URLLoader);
REGISTER_CLASS_NAME(URLLoaderDataFormat);
REGISTER_CLASS_NAME(URLRequest);
REGISTER_CLASS_NAME(URLRequestMethod);
REGISTER_CLASS_NAME(URLVariables);
REGISTER_CLASS_NAME(SharedObject);
REGISTER_CLASS_NAME(NetConnection);
REGISTER_CLASS_NAME(NetStream);

URLRequest::URLRequest():method(GET)
{
}

void URLRequest::sinit(Class_base* c)
{
	c->setConstructor(Class<IFunction>::getFunction(_constructor));
	c->setSuper(Class<ASObject>::getRef());
	c->setDeclaredMethodByQName("url","",Class<IFunction>::getFunction(_setURL),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("url","",Class<IFunction>::getFunction(_getURL),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("method","",Class<IFunction>::getFunction(_setMethod),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("method","",Class<IFunction>::getFunction(_getMethod),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("data","",Class<IFunction>::getFunction(_setData),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("data","",Class<IFunction>::getFunction(_getData),GETTER_METHOD,true);
}

void URLRequest::buildTraits(ASObject* o)
{
}

URLInfo URLRequest::getRequestURL() const
{
	URLInfo ret=sys->getOrigin().goToURL(url);
	if(method!=GET)
		return ret;

	if(data.isNull())
		return ret;

	if(data->getClass()==Class<ByteArray>::getClass())
		throw RunTimeException("ByteArray data not supported in URLRequest");
	else
	{
		tiny_string newURL = ret.getParsedURL();
		if(ret.getQuery() == "")
			newURL += "?";
		else
			newURL += "&amp;";
		newURL += data->toString();
		ret=ret.goToURL(newURL);
	}
	return ret;
}

void URLRequest::getPostData(vector<uint8_t>& outData) const
{
	if(method!=POST)
		return;

	if(data.isNull())
		return;

	if(data->getClass()==Class<ByteArray>::getClass())
		throw RunTimeException("ByteArray not support in URLRequest");
	else if(data->getClass()==Class<URLVariables>::getClass())
	{
		//Prepend the Content-Type header
		tiny_string strData="Content-type: application/x-www-form-urlencoded\r\nContent-length: ";
		const tiny_string& tmpStr=data->toString();
		char buf[20];
		snprintf(buf,20,"%u\r\n\r\n",tmpStr.len());
		strData+=buf;
		strData+=tmpStr;
		outData.insert(outData.end(),strData.raw_buf(),strData.raw_buf()+strData.len());
	}
	else
	{
		const tiny_string& strData=data->toString();
		outData.insert(outData.end(),strData.raw_buf(),strData.raw_buf()+strData.len());
	}
}

void URLRequest::finalize()
{
	ASObject::finalize();
	data.reset();
}

ASFUNCTIONBODY(URLRequest,_constructor)
{
	URLRequest* th=static_cast<URLRequest*>(obj);
	if(argslen==1 && args[0]->getObjectType()==T_STRING)
	{
		th->url=args[0]->toString();
	}
	return NULL;
}

ASFUNCTIONBODY(URLRequest,_setURL)
{
	URLRequest* th=static_cast<URLRequest*>(obj);
	assert_and_throw(argslen==1);
	th->url=args[0]->toString();
	return NULL;
}

ASFUNCTIONBODY(URLRequest,_getURL)
{
	URLRequest* th=static_cast<URLRequest*>(obj);
	return Class<ASString>::getInstanceS(th->url);
}

ASFUNCTIONBODY(URLRequest,_setMethod)
{
	URLRequest* th=static_cast<URLRequest*>(obj);
	assert_and_throw(argslen==1);
	const tiny_string& tmp=args[0]->toString();
	if(tmp=="GET")
		th->method=GET;
	else if(tmp=="POST")
		th->method=POST;
	else
		throw UnsupportedException("Unsupported method in URLLoader");
	return NULL;
}

ASFUNCTIONBODY(URLRequest,_getMethod)
{
	URLRequest* th=static_cast<URLRequest*>(obj);
	switch(th->method)
	{
		case GET:
			return Class<ASString>::getInstanceS("GET");
		case POST:
			return Class<ASString>::getInstanceS("POST");
	}
	return NULL;
}

ASFUNCTIONBODY(URLRequest,_getData)
{
	URLRequest* th=static_cast<URLRequest*>(obj);
	if(th->data.isNull())
		return new Undefined;

	th->data->incRef();
	return th->data.getPtr();
}

ASFUNCTIONBODY(URLRequest,_setData)
{
	URLRequest* th=static_cast<URLRequest*>(obj);
	assert_and_throw(argslen==1);

	args[0]->incRef();
	th->data=_MR(args[0]);

	return NULL;
}

void URLRequestMethod::sinit(Class_base* c)
{
	c->setSuper(Class<ASObject>::getRef());
	c->setVariableByQName("GET","",Class<ASString>::getInstanceS("GET"),DECLARED_TRAIT);
	c->setVariableByQName("POST","",Class<ASString>::getInstanceS("POST"),DECLARED_TRAIT);
}

URLLoader::URLLoader():dataFormat("text"),data(NULL),downloader(NULL)
{
}

void URLLoader::finalize()
{
	EventDispatcher::finalize();
	data.reset();
}

void URLLoader::sinit(Class_base* c)
{
	c->setConstructor(Class<IFunction>::getFunction(_constructor));
	c->setSuper(Class<EventDispatcher>::getRef());
	c->setDeclaredMethodByQName("dataFormat","",Class<IFunction>::getFunction(_getDataFormat),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("data","",Class<IFunction>::getFunction(_getData),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("dataFormat","",Class<IFunction>::getFunction(_setDataFormat),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("load","",Class<IFunction>::getFunction(load),NORMAL_METHOD,true);
}

void URLLoader::buildTraits(ASObject* o)
{
}

ASFUNCTIONBODY(URLLoader,_constructor)
{
	EventDispatcher::_constructor(obj,NULL,0);
	if(argslen==1 && args[0]->getClass() == Class<URLRequest>::getClass())
	{
		//URLRequest* urlRequest=Class<URLRequest>::dyncast(args[0]);
		load(obj, args, argslen);
	}
	return NULL;
}

ASFUNCTIONBODY(URLLoader,load)
{
	URLLoader* th=static_cast<URLLoader*>(obj);
	ASObject* arg=args[0];
	URLRequest* urlRequest=Class<URLRequest>::dyncast(arg);
	assert_and_throw(urlRequest);

	assert_and_throw(th->downloader==NULL);
	th->url=urlRequest->getRequestURL();

	if(!th->url.isValid())
	{
		//Notify an error during loading
		th->incRef();
		sys->currentVm->addEvent(_MR(th),_MR(Class<IOErrorEvent>::getInstanceS()));
		return NULL;
	}

	//TODO: support the right events (like SecurityErrorEvent)
	//URLLoader ALWAYS checks for policy files, in contrast to NetStream.play().
	SecurityManager::EVALUATIONRESULT evaluationResult = 
		sys->securityManager->evaluateURLStatic(th->url, ~(SecurityManager::LOCAL_WITH_FILE),
			SecurityManager::LOCAL_WITH_FILE | SecurityManager::LOCAL_TRUSTED, true);
	//Network sandboxes can't access local files (this should be a SecurityErrorEvent)
	if(evaluationResult == SecurityManager::NA_REMOTE_SANDBOX)
		throw Class<SecurityError>::getInstanceS("SecurityError: URLLoader::load: "
				"connect to network");
	//Local-with-filesystem sandbox can't access network
	else if(evaluationResult == SecurityManager::NA_LOCAL_SANDBOX)
		throw Class<SecurityError>::getInstanceS("SecurityError: URLLoader::load: "
				"connect to local file");
	else if(evaluationResult == SecurityManager::NA_PORT)
		throw Class<SecurityError>::getInstanceS("SecurityError: URLLoader::load: "
				"connect to restricted port");
	else if(evaluationResult == SecurityManager::NA_RESTRICT_LOCAL_DIRECTORY)
		throw Class<SecurityError>::getInstanceS("SecurityError: URLLoader::load: "
				"not allowed to navigate up for local files");

	//TODO: should we disallow accessing local files in a directory above 
	//the current one like we do with NetStream.play?

	urlRequest->getPostData(th->postData);

	//To be decreffed in jobFence
	th->incRef();
	sys->addJob(th);
	return NULL;
}

void URLLoader::jobFence()
{
	decRef();
}

void URLLoader::execute()
{
	//TODO: support httpStatus, progress, open events

	//Check for URL policies and send SecurityErrorEvent if needed
	SecurityManager::EVALUATIONRESULT evaluationResult = sys->securityManager->evaluatePoliciesURL(url, true);
	if(evaluationResult == SecurityManager::NA_CROSSDOMAIN_POLICY)
	{
		this->incRef();
		getVm()->addEvent(_MR(this),_MR(Class<SecurityErrorEvent>::getInstanceS("SecurityError: URLLoader::load: "
					"connection to domain not allowed by securityManager")));
		return;
	}

	{
		SpinlockLocker l(downloaderLock);
		//All the checks passed, create the downloader
		if(postData.empty())
		{
			//This is a GET request
			//Don't cache our downloaded files
			downloader=sys->downloadManager->download(url, false, NULL);
		}
		else
		{
			downloader=sys->downloadManager->downloadWithData(url, postData, NULL);
			//Clean up the postData for the next load
			postData.clear();
		}
	}

	if(!downloader->hasFailed())
	{
		downloader->waitForTermination();
		//HACK: the downloader may have been cleared in the mean time
		assert(downloader);
		if(!downloader->hasFailed())
		{
			istream s(downloader);
			uint8_t* buf=new uint8_t[downloader->getLength()];
			//TODO: avoid this useless copy
			s.read((char*)buf,downloader->getLength());
			//TODO: test binary data format
			if(dataFormat=="binary")
			{
				_R<ByteArray> byteArray=_MR(Class<ByteArray>::getInstanceS());
				byteArray->acquireBuffer(buf,downloader->getLength());
				data=byteArray;
				//The buffers must not be deleted, it's now handled by the ByteArray instance
			}
			else if(dataFormat=="text")
			{
				data=_MR(Class<ASString>::getInstanceS((char*)buf,downloader->getLength()));
				delete[] buf;
			}
			else if(dataFormat=="variables")
			{
				data=_MR(Class<URLVariables>::getInstanceS((char*)buf));
				delete[] buf;
			}
			//Send a complete event for this object
			this->incRef();
			sys->currentVm->addEvent(_MR(this),_MR(Class<Event>::getInstanceS("complete")));
		}
		else
		{
			//Notify an error during loading
			this->incRef();
			sys->currentVm->addEvent(_MR(this),_MR(Class<IOErrorEvent>::getInstanceS()));
		}
	}
	else
	{
		//Notify an error during loading
		this->incRef();
		sys->currentVm->addEvent(_MR(this),_MR(Class<IOErrorEvent>::getInstanceS()));
	}

	{
		//Acquire the lock to ensure consistency in threadAbort
		SpinlockLocker l(downloaderLock);
		sys->downloadManager->destroy(downloader);
		downloader = NULL;
	}
}

void URLLoader::threadAbort()
{
	SpinlockLocker l(downloaderLock);
	if(downloader != NULL)
		downloader->stop();
}

ASFUNCTIONBODY(URLLoader,_getDataFormat)
{
	URLLoader* th=static_cast<URLLoader*>(obj);
	return Class<ASString>::getInstanceS(th->dataFormat);
}

ASFUNCTIONBODY(URLLoader,_getData)
{
	URLLoader* th=static_cast<URLLoader*>(obj);
	if(th->data.isNull())
		return new Undefined;
	
	th->data->incRef();
	return th->data.getPtr();
}

ASFUNCTIONBODY(URLLoader,_setDataFormat)
{
	URLLoader* th=static_cast<URLLoader*>(obj);
	assert_and_throw(args[0]);
	th->dataFormat=args[0]->toString();
	return NULL;
}

void URLLoaderDataFormat::sinit(Class_base* c)
{
	c->setSuper(Class<ASObject>::getRef());
	c->setVariableByQName("VARIABLES","",Class<ASString>::getInstanceS("variables"),DECLARED_TRAIT);
	c->setVariableByQName("TEXT","",Class<ASString>::getInstanceS("text"),DECLARED_TRAIT);
	c->setVariableByQName("BINARY","",Class<ASString>::getInstanceS("binary"),DECLARED_TRAIT);
}

void SharedObject::sinit(Class_base* c)
{
	c->setSuper(Class<EventDispatcher>::getRef());
};

void URLVariables::decode(const tiny_string& s)
{
	const char* nameStart=NULL;
	const char* nameEnd=NULL;
	const char* valueStart=NULL;
	const char* valueEnd=NULL;
	const char* cur=s.raw_buf();
	while(1)
	{
		if(nameStart==NULL)
			nameStart=cur;
		if(*cur == '=')
		{
			if(nameStart==NULL || valueStart!=NULL) //Skip this
			{
				nameStart=NULL;
				nameEnd=NULL;
				valueStart=NULL;
				valueEnd=NULL;
				cur++;
				continue;
			}
			nameEnd=cur;
			valueStart=cur+1;
		}
		else if(*cur == '&' || *cur==0)
		{
			if(nameStart==NULL || nameEnd==NULL || valueStart==NULL || valueEnd!=NULL)
			{
				nameStart=NULL;
				nameEnd=NULL;
				valueStart=NULL;
				valueEnd=NULL;
				cur++;
				continue;
			}
			valueEnd=cur;
			char* name=g_uri_unescape_segment(nameStart,nameEnd,NULL);
			char* value=g_uri_unescape_segment(valueStart,valueEnd,NULL);
			nameStart=NULL;
			nameEnd=NULL;
			valueStart=NULL;
			valueEnd=NULL;
			//Check if the variable already exists
			multiname propName;
			propName.name_type=multiname::NAME_STRING;
			propName.name_s=name;
			propName.ns.push_back(nsNameAndKind("",NAMESPACE));
			_NR<ASObject> curValue=getVariableByMultiname(propName);
			if(!curValue.isNull())
			{
				//If the variable already exists we have to create an Array of values
				Array* arr=NULL;
				if(curValue->getObjectType()!=T_ARRAY)
				{
					arr=Class<Array>::getInstanceS();
					curValue->incRef();
					arr->push(curValue.getPtr());
					setVariableByMultiname(propName,arr);
				}
				else
					arr=Class<Array>::cast(curValue.getPtr());

				arr->push(Class<ASString>::getInstanceS(value));
			}
			else
				setVariableByMultiname(propName,Class<ASString>::getInstanceS(value));

			g_free(name);
			g_free(value);
			if(*cur==0)
				break;
		}
		cur++;
	}
}

void URLVariables::sinit(Class_base* c)
{
	c->setConstructor(Class<IFunction>::getFunction(_constructor));
	c->setSuper(Class<ASObject>::getRef());
	c->setDeclaredMethodByQName("decode","",Class<IFunction>::getFunction(decode),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("toString","",Class<IFunction>::getFunction(_toString),DYNAMIC_TRAIT);
}

void URLVariables::buildTraits(ASObject* o)
{
}

URLVariables::URLVariables(const tiny_string& s)
{
	decode(s);
}

ASFUNCTIONBODY(URLVariables,decode)
{
	URLVariables* th=Class<URLVariables>::cast(obj);
	assert_and_throw(argslen==1);
	th->decode(args[0]->toString());
	return NULL;
}

ASFUNCTIONBODY(URLVariables,_toString)
{
	URLVariables* th=Class<URLVariables>::cast(obj);
	assert_and_throw(argslen==0);
	return Class<ASString>::getInstanceS(th->toString_priv());
}

ASFUNCTIONBODY(URLVariables,_constructor)
{
	URLVariables* th=Class<URLVariables>::cast(obj);
	assert_and_throw(argslen<=1);
	if(argslen==1)
		th->decode(args[0]->toString());
	return NULL;
}

tiny_string URLVariables::toString_priv()
{
	int size=numVariables();
	tiny_string tmp;
	for(int i=0;i<size;i++)
	{
		const tiny_string& name=getNameAt(i);
		//TODO: check if the allow_unicode flag should be true or false in g_uri_escape_string

		ASObject* val=getValueAt(i);
		if(val->getObjectType()==T_ARRAY)
		{
			//Print using multiple properties
			//Ex. ["foo","bar"] -> prop1=foo&prop1=bar
			Array* arr=Class<Array>::cast(val);
			for(int32_t j=0;j<arr->size();j++)
			{
				//Escape the name
				char* escapedName=g_uri_escape_string(name.raw_buf(),NULL, false);
				tmp+=escapedName;
				g_free(escapedName);
				tmp+="=";

				//Escape the value
				const tiny_string& value=arr->at(j)->toString();
				char* escapedValue=g_uri_escape_string(value.raw_buf(),NULL, false);
				tmp+=escapedValue;
				g_free(escapedValue);

				if(j!=arr->size()-1)
					tmp+="&";
			}
		}
		else
		{
			//Escape the name
			char* escapedName=g_uri_escape_string(name.raw_buf(),NULL, false);
			tmp+=escapedName;
			g_free(escapedName);
			tmp+="=";

			//Escape the value
			const tiny_string& value=val->toString();
			char* escapedValue=g_uri_escape_string(value.raw_buf(),NULL, false);
			tmp+=escapedValue;
			g_free(escapedValue);
		}
		if(i!=size-1)
			tmp+="&";
	}
	return tmp;
}

tiny_string URLVariables::toString()
{
	assert_and_throw(implEnable);
	return toString_priv();
}

ASFUNCTIONBODY(lightspark,sendToURL)
{
	assert_and_throw(argslen == 1);
	ASObject* arg=args[0];
	URLRequest* urlRequest=Class<URLRequest>::dyncast(arg);
	assert_and_throw(urlRequest);

	URLInfo url=urlRequest->getRequestURL();

	if(!url.isValid())
		return NULL;

	//TODO: support the right events (like SecurityErrorEvent)
	//URLLoader ALWAYS checks for policy files, in contrast to NetStream.play().
	SecurityManager::EVALUATIONRESULT evaluationResult = 
		sys->securityManager->evaluateURLStatic(url, ~(SecurityManager::LOCAL_WITH_FILE),
			SecurityManager::LOCAL_WITH_FILE | SecurityManager::LOCAL_TRUSTED,
			true);
	//Network sandboxes can't access local files (this should be a SecurityErrorEvent)
	if(evaluationResult == SecurityManager::NA_REMOTE_SANDBOX)
		throw Class<SecurityError>::getInstanceS("SecurityError: sendToURL: "
				"connect to network");
	//Local-with-filesystem sandbox can't access network
	else if(evaluationResult == SecurityManager::NA_LOCAL_SANDBOX)
		throw Class<SecurityError>::getInstanceS("SecurityError: sendToURL: "
				"connect to local file");
	else if(evaluationResult == SecurityManager::NA_PORT)
		throw Class<SecurityError>::getInstanceS("SecurityError: sendToURL: "
				"connect to restricted port");
	else if(evaluationResult == SecurityManager::NA_RESTRICT_LOCAL_DIRECTORY)
		throw Class<SecurityError>::getInstanceS("SecurityError: sendToURL: "
				"not allowed to navigate up for local files");

	//Also check cross domain policies. TODO: this should be async as it could block if invoked from ExternalInterface
	evaluationResult = sys->securityManager->evaluatePoliciesURL(url, true);
	if(evaluationResult == SecurityManager::NA_CROSSDOMAIN_POLICY)
	{
		//TODO: find correct way of handling this case (SecurityErrorEvent in this case)
		throw Class<SecurityError>::getInstanceS("SecurityError: sendToURL: "
				"connection to domain not allowed by securityManager");
	}

	//TODO: should we disallow accessing local files in a directory above 
	//the current one like we do with NetStream.play?

	vector<uint8_t> postData;
	urlRequest->getPostData(postData);
	assert_and_throw(postData.empty());

	//Don't cache our downloaded files
	Downloader* downloader=sys->downloadManager->download(url, false, NULL);
	//TODO: make the download asynchronous instead of waiting for an unused response
	downloader->waitForTermination();
	sys->downloadManager->destroy(downloader);
	return NULL;
}
