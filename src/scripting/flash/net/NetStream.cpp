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
#include "scripting/flash/net/NetStream.h"

using namespace std;
using namespace lightspark;

SET_NAMESPACE("flash.net");

REGISTER_CLASS_NAME(NetStream);

NetStream::NetStream():frameRate(0),tickStarted(false),connection(NULL),downloader(NULL),
	videoDecoder(NULL),audioDecoder(NULL),audioStream(NULL),streamTime(0),paused(false),
	closed(true),client(NullRef),checkPolicyFile(false),rawAccessAllowed(false),
	oldVolume(-1.0)
{
	sem_init(&mutex,0,1);
}

void NetStream::finalize()
{
	EventDispatcher::finalize();
	connection.reset();
	client.reset();
}

NetStream::~NetStream()
{
	assert(!executing);
	if(tickStarted)
		sys->removeJob(this);
	delete videoDecoder; 
	delete audioDecoder; 
	sem_destroy(&mutex);
}

void NetStream::sinit(Class_base* c)
{
	c->setConstructor(Class<IFunction>::getFunction(_constructor));
	c->setSuper(Class<EventDispatcher>::getRef());
	c->setVariableByQName("CONNECT_TO_FMS","",Class<ASString>::getInstanceS("connectToFMS"),DECLARED_TRAIT);
	c->setVariableByQName("DIRECT_CONNECTIONS","",Class<ASString>::getInstanceS("directConnections"),DECLARED_TRAIT);
	c->setDeclaredMethodByQName("play","",Class<IFunction>::getFunction(play),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("resume","",Class<IFunction>::getFunction(resume),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("pause","",Class<IFunction>::getFunction(pause),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("togglePause","",Class<IFunction>::getFunction(togglePause),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("close","",Class<IFunction>::getFunction(close),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("seek","",Class<IFunction>::getFunction(seek),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("bytesLoaded","",Class<IFunction>::getFunction(_getBytesLoaded),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("bytesTotal","",Class<IFunction>::getFunction(_getBytesTotal),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("time","",Class<IFunction>::getFunction(_getTime),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("currentFPS","",Class<IFunction>::getFunction(_getCurrentFPS),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("client","",Class<IFunction>::getFunction(_getClient),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("client","",Class<IFunction>::getFunction(_setClient),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("checkPolicyFile","",Class<IFunction>::getFunction(_getCheckPolicyFile),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("checkPolicyFile","",Class<IFunction>::getFunction(_setCheckPolicyFile),SETTER_METHOD,true);
	REGISTER_GETTER_SETTER(c,soundTransform);
}

void NetStream::buildTraits(ASObject* o)
{
}

ASFUNCTIONBODY_GETTER_SETTER(NetStream,soundTransform);

ASFUNCTIONBODY(NetStream,_getClient)
{
	NetStream* th=Class<NetStream>::cast(obj);
	if(th->client.isNull())
		return new Undefined();

	th->client->incRef();
	return th->client.getPtr();
}

ASFUNCTIONBODY(NetStream,_setClient)
{
	assert_and_throw(argslen == 1);
	if(args[0]->getObjectType() == T_NULL)
		throw Class<TypeError>::getInstanceS();

	NetStream* th=Class<NetStream>::cast(obj);

	args[0]->incRef();
	th->client = _MR(args[0]);
	return NULL;
}

ASFUNCTIONBODY(NetStream,_getCheckPolicyFile)
{
	NetStream* th=Class<NetStream>::cast(obj);

	return abstract_b(th->checkPolicyFile);
}

ASFUNCTIONBODY(NetStream,_setCheckPolicyFile)
{
	assert_and_throw(argslen == 1);
	
	NetStream* th=Class<NetStream>::cast(obj);

	th->checkPolicyFile = Boolean_concrete(args[0]);
	return NULL;
}

ASFUNCTIONBODY(NetStream,_constructor)
{
	obj->incRef();
	_R<NetStream> th=_MR(Class<NetStream>::cast(obj));

	LOG(LOG_CALLS,_("NetStream constructor"));
	assert_and_throw(argslen>=1 && argslen <=2);
	assert_and_throw(args[0]->getClass()==Class<NetConnection>::getClass());

	args[0]->incRef();
	_R<NetConnection> netConnection = _MR(Class<NetConnection>::cast(args[0]));
	if(argslen == 2)
	{
		if(args[1]->getObjectType() == T_STRING)
		{
			tiny_string value = Class<ASString>::cast(args[1])->toString();
			if(value == "directConnections")
				th->peerID = DIRECT_CONNECTIONS;
			else
				th->peerID = CONNECT_TO_FMS;
		}
		else if(args[1]->getObjectType() == T_NULL)
			th->peerID = CONNECT_TO_FMS;
		else
			throw Class<ArgumentError>::getInstanceS("NetStream constructor: peerID");
	}

	th->client = th;
	th->connection=netConnection;

	return NULL;
}

ASFUNCTIONBODY(NetStream,play)
{
	NetStream* th=Class<NetStream>::cast(obj);

	//Make sure the stream is restarted properly
	if(th->closed)
		th->closed = false;
	else
		return NULL;

	//Reset the paused states
	th->paused = false;
//	th->audioPaused = false;
	assert(!th->connection.isNull());
	
	if(th->connection->uri.isValid())
	{
		//We should connect to FMS
		assert_and_throw(argslen>=1 && argslen<=4);
		//Args: name, start, len, reset
		th->url=th->connection->uri;
		th->url.setStream(args[0]->toString());
	}
	else
	{
		//HTTP download
		assert_and_throw(argslen>=1);
		//args[0] is the url
		//what is the meaning of the other arguments
		th->url = sys->getOrigin().goToURL(args[0]->toString());

		SecurityManager::EVALUATIONRESULT evaluationResult = 
			sys->securityManager->evaluateURLStatic(th->url, ~(SecurityManager::LOCAL_WITH_FILE),
				SecurityManager::LOCAL_WITH_FILE | SecurityManager::LOCAL_TRUSTED,
				true); //Check for navigating up in local directories (not allowed)
		if(evaluationResult == SecurityManager::NA_REMOTE_SANDBOX)
			throw Class<SecurityError>::getInstanceS("SecurityError: NetStream::play: "
					"connect to network");
		//Local-with-filesystem sandbox can't access network
		else if(evaluationResult == SecurityManager::NA_LOCAL_SANDBOX)
			throw Class<SecurityError>::getInstanceS("SecurityError: NetStream::play: "
					"connect to local file");
		else if(evaluationResult == SecurityManager::NA_PORT)
			throw Class<SecurityError>::getInstanceS("SecurityError: NetStream::play: "
					"connect to restricted port");
		else if(evaluationResult == SecurityManager::NA_RESTRICT_LOCAL_DIRECTORY)
			throw Class<SecurityError>::getInstanceS("SecurityError: NetStream::play: "
					"not allowed to navigate up for local files");
	}

	assert_and_throw(th->downloader==NULL);
	
	if(!th->url.isValid())
	{
		//Notify an error during loading
		th->incRef();
		sys->currentVm->addEvent(_MR(th),_MR(Class<IOErrorEvent>::getInstanceS()));
	}
	else //The URL is valid so we can start the download and add ourself as a job
	{
		//Cache our downloaded files
		th->downloader=sys->downloadManager->download(th->url, true, NULL);
		th->streamTime=0;
		//To be decreffed in jobFence
		th->incRef();
		sys->addJob(th);
	}
	return NULL;
}

void NetStream::jobFence()
{
	decRef();
}

ASFUNCTIONBODY(NetStream,resume)
{
	NetStream* th=Class<NetStream>::cast(obj);
	if(th->paused)
	{
		th->paused = false;
		sem_wait(&th->mutex);
		if(th->audioStream)
			sys->audioManager->resumeStreamPlugin(th->audioStream);
		sem_post(&th->mutex);
		th->incRef();
		getVm()->addEvent(_MR(th), _MR(Class<NetStatusEvent>::getInstanceS("status", "NetStream.Unpause.Notify")));
	}
	return NULL;
}

ASFUNCTIONBODY(NetStream,pause)
{
	NetStream* th=Class<NetStream>::cast(obj);
	if(!th->paused)
	{
		th->paused = true;
		sem_wait(&th->mutex);
		if(th->audioStream)
			sys->audioManager->pauseStreamPlugin(th->audioStream);
		sem_post(&th->mutex);
		th->incRef();
		getVm()->addEvent(_MR(th),_MR(Class<NetStatusEvent>::getInstanceS("status", "NetStream.Pause.Notify")));
	}
	return NULL;
}

ASFUNCTIONBODY(NetStream,togglePause)
{
	NetStream* th=Class<NetStream>::cast(obj);
	if(th->paused)
		th->resume(obj, NULL, 0);
	else
		th->pause(obj, NULL, 0);
	return NULL;
}

ASFUNCTIONBODY(NetStream,close)
{
	NetStream* th=Class<NetStream>::cast(obj);
	//TODO: set the time property to 0
	
	//Everything is stopped in threadAbort
	if(!th->closed)
	{
		th->threadAbort();
		th->incRef();
		getVm()->addEvent(_MR(th), _MR(Class<NetStatusEvent>::getInstanceS("status", "NetStream.Play.Stop")));
	}
	LOG(LOG_CALLS, _("NetStream::close called"));
	return NULL;
}

ASFUNCTIONBODY(NetStream,seek)
{
	//NetStream* th=Class<NetStream>::cast(obj);
	assert_and_throw(argslen == 1);
	return NULL;
}

//Tick is called from the timer thread, this happens only if a decoder is available
void NetStream::tick()
{
	//Check if the stream is paused
	if(audioStream && audioStream->isValid())
	{
		//TODO: use soundTransform->pan
		if(soundTransform != NULL && soundTransform->volume != oldVolume)
		{
			audioStream->setVolume(soundTransform->volume);
			oldVolume = soundTransform->volume;
		}
	}
	if(paused)
		return;
	//Advance video and audio to current time, follow the audio stream time
	//No mutex needed, ticking can happen only when stream is completely ready
	if(audioStream && sys->audioManager->isTimingAvailablePlugin())
	{
		assert(audioDecoder);
		streamTime=audioStream->getPlayedTime()+audioDecoder->initialTime;
	}
	else
	{
		streamTime+=1000/frameRate;
		audioDecoder->skipAll();
	}
	videoDecoder->skipUntil(streamTime);
	//The next line ensures that the downloader will not be destroyed before the upload jobs are fenced
	videoDecoder->waitForFencing();
	sys->getRenderThread()->addUploadJob(videoDecoder);
}

bool NetStream::isReady() const
{
	if(videoDecoder==NULL || audioDecoder==NULL)
		return false;

	bool ret=videoDecoder->isValid() && audioDecoder->isValid();
	return ret;
}

bool NetStream::lockIfReady()
{
	sem_wait(&mutex);
	bool ret=isReady();
	if(!ret) //If the data is not valid so not release the lock to keep the condition
		sem_post(&mutex);
	return ret;
}

void NetStream::unlock()
{
	sem_post(&mutex);
}

void NetStream::execute()
{
	//checkPolicyFile only applies to per-pixel access, loading and playing is always allowed.
	//So there is no need to disallow playing if policy files disallow it.
	//We do need to check if per-pixel access is allowed.
	SecurityManager::EVALUATIONRESULT evaluationResult = sys->securityManager->evaluatePoliciesURL(url, true);
	if(evaluationResult == SecurityManager::NA_CROSSDOMAIN_POLICY)
		rawAccessAllowed = true;

	if(downloader->hasFailed())
	{
		this->incRef();
		sys->currentVm->addEvent(_MR(this),_MR(Class<IOErrorEvent>::getInstanceS()));
		sys->downloadManager->destroy(downloader);
		return;
	}

	//The downloader hasn't failed yet at this point

	istream s(downloader);
	s.exceptions ( istream::eofbit | istream::failbit | istream::badbit );

	ThreadProfile* profile=sys->allocateProfiler(RGB(0,0,200));
	profile->setTag("NetStream");
	bool waitForFlush=true;
	StreamDecoder* streamDecoder=NULL;
	//We need to catch possible EOF and other error condition in the non reliable stream
	try
	{
		Chronometer chronometer;
		streamDecoder=new FFMpegStreamDecoder(s);
		if(!streamDecoder->isValid())
			threadAbort();

		bool done=false;
		while(!done)
		{
			//Check if threadAbort has been called, if so, stop this loop
			if(closed)
				done = true;
			bool decodingSuccess=streamDecoder->decodeNextFrame();
			if(decodingSuccess==false)
				done = true;

			if(videoDecoder==NULL && streamDecoder->videoDecoder)
			{
				videoDecoder=streamDecoder->videoDecoder;
				this->incRef();
				getVm()->addEvent(_MR(this),
						_MR(Class<NetStatusEvent>::getInstanceS("status", "NetStream.Play.Start")));
				this->incRef();
				getVm()->addEvent(_MR(this),
						_MR(Class<NetStatusEvent>::getInstanceS("status", "NetStream.Buffer.Full")));
			}

			if(audioDecoder==NULL && streamDecoder->audioDecoder)
				audioDecoder=streamDecoder->audioDecoder;

			if(audioStream==NULL && audioDecoder && audioDecoder->isValid() && sys->audioManager->pluginLoaded())
				audioStream=sys->audioManager->createStreamPlugin(audioDecoder);

			if(audioStream && audioStream->paused() && !paused)
			{
				//The audio stream is paused but should not!
				//As we have new data fill the stream
				audioStream->fill();
			}

			if(!tickStarted && isReady())
			{
				multiname onMetaDataName;
				onMetaDataName.name_type=multiname::NAME_STRING;
				onMetaDataName.name_s="onMetaData";
				onMetaDataName.ns.push_back(nsNameAndKind("",NAMESPACE));
				_NR<ASObject> callback = client->getVariableByMultiname(onMetaDataName);
				if(!callback.isNull() && callback->getObjectType() == T_FUNCTION)
				{
					ASObject* callbackArgs[1];
					ASObject* metadata = Class<ASObject>::getInstanceS();
					double d;
					uint32_t i;
					if(streamDecoder->getMetadataDouble("width",d))
						metadata->setVariableByQName("width", "",abstract_d(d),DYNAMIC_TRAIT);
					else
						metadata->setVariableByQName("width", "", abstract_d(getVideoWidth()),DYNAMIC_TRAIT);
					if(streamDecoder->getMetadataDouble("height",d))
						metadata->setVariableByQName("height", "",abstract_d(d),DYNAMIC_TRAIT);
					else
						metadata->setVariableByQName("height", "", abstract_d(getVideoHeight()),DYNAMIC_TRAIT);
					if(streamDecoder->getMetadataDouble("framerate",d))
						metadata->setVariableByQName("framerate", "",abstract_d(d),DYNAMIC_TRAIT);
					if(streamDecoder->getMetadataDouble("duration",d))
						metadata->setVariableByQName("duration", "",abstract_d(d),DYNAMIC_TRAIT);
					if(streamDecoder->getMetadataInteger("canseekontime",i))
						metadata->setVariableByQName("canSeekToEnd", "",abstract_b(i == 1),DYNAMIC_TRAIT);
					if(streamDecoder->getMetadataDouble("audiodatarate",d))
						metadata->setVariableByQName("audiodatarate", "",abstract_d(d),DYNAMIC_TRAIT);
					if(streamDecoder->getMetadataDouble("videodatarate",d))
						metadata->setVariableByQName("videodatarate", "",abstract_d(d),DYNAMIC_TRAIT);

					//TODO: missing: audiocodecid (Number), cuePoints (Object[]),
					//videocodecid (Number), custommetadata's
					client->incRef();
					metadata->incRef();
					callbackArgs[0] = metadata;
					callback->incRef();
					_R<FunctionEvent> event(new FunctionEvent(_MR(static_cast<IFunction*>(callback.getPtr())),
							_MR(client), callbackArgs, 1));
					getVm()->addEvent(NullRef,event);
				}

				tickStarted=true;
				if(frameRate==0)
				{
					assert(videoDecoder->frameRate);
					frameRate=videoDecoder->frameRate;
				}
				sys->addTick(1000/frameRate,this);
				//Also ask for a render rate equal to the video one (capped at 24)
				float localRenderRate=dmin(frameRate,24);
				sys->setRenderRate(localRenderRate);
			}
			profile->accountTime(chronometer.checkpoint());
			if(aborting)
				throw JobTerminationException();
		}

	}
	catch(LightsparkException& e)
	{
		LOG(LOG_ERROR, "Exception in NetStream " << e.cause);
		threadAbort();
		waitForFlush=false;
	}
	catch(JobTerminationException& e)
	{
		waitForFlush=false;
	}
	catch(exception& e)
	{
		LOG(LOG_ERROR, _("Exception in reading: ")<<e.what());
	}
	if(waitForFlush)
	{
		//Put the decoders in the flushing state and wait for the complete consumption of contents
		if(audioDecoder)
			audioDecoder->setFlushing();
		if(videoDecoder)
			videoDecoder->setFlushing();
		
		if(audioDecoder)
			audioDecoder->waitFlushed();
		if(videoDecoder)
			videoDecoder->waitFlushed();

		this->incRef();
		getVm()->addEvent(_MR(this), _MR(Class<NetStatusEvent>::getInstanceS("status", "NetStream.Play.Stop")));
		this->incRef();
		getVm()->addEvent(_MR(this), _MR(Class<NetStatusEvent>::getInstanceS("status", "NetStream.Buffer.Flush")));
	}
	//Before deleting stops ticking, removeJobs also spin waits for termination
	sys->removeJob(this);
	tickStarted=false;
	sem_wait(&mutex);
	//Change the state to invalid to avoid locking
	videoDecoder=NULL;
	audioDecoder=NULL;
	//Clean up everything for a possible re-run
	sys->downloadManager->destroy(downloader);
	//This transition is critical, so the mutex is needed
	downloader=NULL;
	if(audioStream)
		sys->audioManager->freeStreamPlugin(audioStream);
	audioStream=NULL;
	sem_post(&mutex);
	delete streamDecoder;
}

void NetStream::threadAbort()
{
	//This will stop the rendering loop
	closed = true;

	sem_wait(&mutex);
	if(downloader)
		downloader->stop();

	//Clear everything we have in buffers, discard all frames
	if(videoDecoder)
	{
		videoDecoder->setFlushing();
		videoDecoder->skipAll();
	}
	if(audioDecoder)
	{
		//Clear everything we have in buffers, discard all frames
		audioDecoder->setFlushing();
		audioDecoder->skipAll();
	}
	sem_post(&mutex);
}

ASFUNCTIONBODY(NetStream,_getBytesLoaded)
{
	NetStream* th=Class<NetStream>::cast(obj);
	if(th->isReady())
		return abstract_i(th->getReceivedLength());
	else
		return abstract_i(0);
}

ASFUNCTIONBODY(NetStream,_getBytesTotal)
{
	NetStream* th=Class<NetStream>::cast(obj);
	if(th->isReady())
		return abstract_i(th->getTotalLength());
	else
		return abstract_d(0);
}

ASFUNCTIONBODY(NetStream,_getTime)
{
	NetStream* th=Class<NetStream>::cast(obj);
	if(th->isReady())
		return abstract_d(th->getStreamTime()/1000.);
	else
		return abstract_d(0);
}

ASFUNCTIONBODY(NetStream,_getCurrentFPS)
{
	//TODO: provide real FPS (what really is displayed)
	NetStream* th=Class<NetStream>::cast(obj);
	if(th->isReady() && !th->paused)
		return abstract_d(th->getFrameRate());
	else
		return abstract_d(0);
}

uint32_t NetStream::getVideoWidth() const
{
	assert(isReady());
	return videoDecoder->getWidth();
}

uint32_t NetStream::getVideoHeight() const
{
	assert(isReady());
	return videoDecoder->getHeight();
}

double NetStream::getFrameRate()
{
	assert(isReady());
	return frameRate;
}

const TextureChunk& NetStream::getTexture() const
{
	assert(isReady());
	return videoDecoder->getTexture();
}

uint32_t NetStream::getStreamTime()
{
	assert(isReady());
	return streamTime;
}

uint32_t NetStream::getReceivedLength()
{
	assert(isReady());
	return downloader->getReceivedLength();
}

uint32_t NetStream::getTotalLength()
{
	assert(isReady());
	return downloader->getLength();
}
