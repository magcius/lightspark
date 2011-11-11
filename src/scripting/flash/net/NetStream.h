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

#ifndef _FLASH_NET_NET_STREAM_H
#define _FLASH_NET_NET_STREAM_H

#include "asobject.h"
#include "compat.h"
#include "thread_pool.h"
#include "timer.h"

#include "backends/netutils.h"
#include "backends/decoder.h"
#include "backends/interfaces/audio/IAudioPlugin.h"

#include "scripting/flash/events/flashevents.h"
#include "scripting/flash/net/NetConnection.h"

namespace lightspark
{

class SoundTransform;

class NetStream: public EventDispatcher, public IThreadJob, public ITickJob
{
private:
	URLInfo url;
	double frameRate;
	bool tickStarted;
	//The NetConnection used by this NetStream
	_NR<NetConnection> connection;
	Downloader* downloader;
	VideoDecoder* videoDecoder;
	AudioDecoder* audioDecoder;
	AudioStream *audioStream;
	uint32_t streamTime;
	sem_t mutex;
	//IThreadJob interface for long jobs
	void execute();
	void threadAbort();
	void jobFence();
	//ITickJob interface to frame advance
	void tick();
	bool isReady() const;

	//Indicates whether the NetStream is paused
	bool paused;
	//Indicates whether the NetStream has been closed/threadAborted. This is reset at every play() call.
	//We initialize this value to true, so we can check that play() hasn't been called without being closed first.
	bool closed;

	enum CONNECTION_TYPE { CONNECT_TO_FMS=0, DIRECT_CONNECTIONS };
	CONNECTION_TYPE peerID;

	_NR<ASObject> client;
	bool checkPolicyFile;
	bool rawAccessAllowed;
	number_t oldVolume;
	ASPROPERTY_GETTER_SETTER(NullableRef<SoundTransform>,soundTransform);
public:
	NetStream();
	~NetStream();
	void finalize();
	static void sinit(Class_base*);
	static void buildTraits(ASObject* o);
	ASFUNCTION(_constructor);
	ASFUNCTION(play);
	ASFUNCTION(resume);
	ASFUNCTION(pause);
	ASFUNCTION(togglePause);
	ASFUNCTION(close);
	ASFUNCTION(seek);
	ASFUNCTION(_getBytesLoaded);
	ASFUNCTION(_getBytesTotal);
	ASFUNCTION(_getTime);
	ASFUNCTION(_getCurrentFPS);
	ASFUNCTION(_getClient);
	ASFUNCTION(_setClient);
	ASFUNCTION(_getCheckPolicyFile);
	ASFUNCTION(_setCheckPolicyFile);

	//Interface for video
	/**
	  	Get the frame width

		@pre lock on the object should be acquired and object should be ready
		@return the frame width
	*/
	uint32_t getVideoWidth() const;
	/**
	  	Get the frame height

		@pre lock on the object should be acquired and object should be ready
		@return the frame height
	*/
	uint32_t getVideoHeight() const;
	/**
	  	Get the frame rate

		@pre lock on the object should be acquired and object should be ready
		@return the frame rate
	*/
	double getFrameRate();
	/**
	  	Get the texture containing the current video Frame
		@pre lock on the object should be acquired and object should be ready
		@return a TextureChunk ready to be blitted
	*/
	const TextureChunk& getTexture() const;
	/**
	  	Get the stream time

		@pre lock on the object should be acquired and object should be ready
		@return the stream time
	*/
	uint32_t getStreamTime();
	/**
	  	Get the length of loaded data

		@pre lock on the object should be acquired and object should be ready
		@return the length of loaded data
	*/
	uint32_t getReceivedLength();
	/**
	  	Get the length of loaded data

		@pre lock on the object should be acquired and object should be ready
		@return the total length of the data
	*/
	uint32_t getTotalLength();
	/**
	  	Acquire the mutex to guarantee validity of data

		@return true if the lock has been acquired
	*/
	bool lockIfReady();
	/**
	  	Release the lock

		@pre the object should be locked
	*/
	void unlock();
};

}
#endif
