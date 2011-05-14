/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2011  Alessandro Pignotti (a.pignotti@sssup.it)
    Copyright (C) 2010-2011  Timon Van Overveldt (timonvo@gmail.com)

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

#include "engine.h"

#include "swf.h"
#include "backends/rendering.h"
#include "backends/input.h"
#include "threading.h"
#include <iostream>
#include <gdk/gdkkeysyms.h>

using namespace std;
using namespace lightspark;

void* Engine::renderWorker(RenderThread* th)
{
    SystemState* sys=system;
    rt=th;

	X11Params* p=x11Params;
	SemaphoreLighter lighter(th->initialized);

	th->windowWidth=p->width;
	th->windowHeight=p->height;
	
	Display* d=XOpenDisplay(NULL);

	int a,b;
	Bool glx_present=glXQueryVersion(d,&a,&b);
	if(!glx_present)
	{
		LOG(LOG_ERROR,_("glX not present"));
		return NULL;
	}
	int attrib[10]={GLX_BUFFER_SIZE,24,GLX_DOUBLEBUFFER,True,None};
	GLXFBConfig* fb=glXChooseFBConfig(d, 0, attrib, &a);
	if(!fb)
	{
		attrib[2]=None;
		fb=glXChooseFBConfig(d, 0, attrib, &a);
		LOG(LOG_ERROR,_("Falling back to no double buffering"));
	}
	if(!fb)
	{
		LOG(LOG_ERROR,_("Could not find any GLX configuration"));
		::abort();
	}
	int i;
	for(i=0;i<a;i++)
	{
		int id;
		glXGetFBConfigAttrib(d,fb[i],GLX_VISUAL_ID,&id);
		if(id==(int)p->visual)
			break;
	}
	if(i==a)
	{
		//No suitable id found
		LOG(LOG_ERROR,_("No suitable graphics configuration available"));
		return NULL;
	}
	th->mFBConfig=fb[i];
	cout << "Chosen config " << hex << fb[i] << dec << endl;
	XFree(fb);

	th->mContext = glXCreateNewContext(d,th->mFBConfig,GLX_RGBA_TYPE ,NULL,1);
	GLXWindow glxWin=p->window;
	glXMakeCurrent(d, glxWin,th->mContext);
	if(!glXIsDirect(d,th->mContext))
		cout << "Indirect!!" << endl;

	th->commonGLInit(th->windowWidth, th->windowHeight);
	th->commonGLResize();
	lighter.light();
	
	ThreadProfile* profile=sys->allocateProfiler(RGB(200,0,0));
	profile->setTag("Render");
	FTTextureFont font(th->fontPath.c_str());
	if(font.Error())
	{
		LOG(LOG_ERROR,_("Unable to load serif font"));
		throw RunTimeException("Unable to load font");
	}
	
	font.FaceSize(12);

	glEnable(GL_TEXTURE_2D);
	try
	{
		Chronometer chronometer;
		while(1)
		{
			sem_wait(&th->event);
			if(th->m_sys->isShuttingDown())
				break;
			chronometer.checkpoint();
			
			if(th->resizeNeeded)
			{
				if(th->windowWidth!=th->newWidth ||
					th->windowHeight!=th->newHeight)
				{
					th->windowWidth=th->newWidth;
					th->windowHeight=th->newHeight;
					LOG(LOG_ERROR,_("Window resize not supported in plugin"));
				}
				th->newWidth=0;
				th->newHeight=0;
				th->resizeNeeded=false;
				th->commonGLResize();
				profile->accountTime(chronometer.checkpoint());
				continue;
			}

			if(th->newTextureNeeded)
				th->handleNewTexture();

			if(th->prevUploadJob)
				th->finalizeUpload();

			if(th->uploadNeeded)
			{
				th->handleUpload();
				profile->accountTime(chronometer.checkpoint());
				continue;
			}

			if(th->m_sys->isOnError())
			{
				glLoadIdentity();
				glScalef(1.0f/th->scaleX,-1.0f/th->scaleY,1);
				glTranslatef(-th->offsetX,(th->windowHeight-th->offsetY)*(-1.0f),0);
				glUseProgram(0);
				glActiveTexture(GL_TEXTURE1);
				glDisable(GL_TEXTURE_2D);
				glActiveTexture(GL_TEXTURE0);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glDrawBuffer(GL_BACK);

				glClearColor(0,0,0,1);
				glClear(GL_COLOR_BUFFER_BIT);
				glColor3f(0.8,0.8,0.8);
					    
				font.Render("We're sorry, Lightspark encountered a yet unsupported Flash file",
						-1,FTPoint(0,th->windowHeight/2+20));

				stringstream errorMsg;
				errorMsg << "SWF file: " << th->m_sys->getOrigin().getParsedURL();
				font.Render(errorMsg.str().c_str(),
						-1,FTPoint(0,th->windowHeight/2));
					    
				errorMsg.str("");
				errorMsg << "Cause: " << th->m_sys->errorCause;
				font.Render(errorMsg.str().c_str(),
						-1,FTPoint(0,th->windowHeight/2-20));

				font.Render("Press C to copy this error to clipboard",
						-1,FTPoint(0,th->windowHeight/2-40));
				
				glFlush();
				glXSwapBuffers(d,glxWin);
			}
			else
			{
				glXSwapBuffers(d,glxWin);
				th->coreRendering(font);
				//Call glFlush to offload work on the GPU
				glFlush();
			}
			profile->accountTime(chronometer.checkpoint());
			th->renderNeeded=false;
		}
	}
	catch(LightsparkException& e)
	{
		LOG(LOG_ERROR,_("Exception in RenderThread ") << e.what());
		sys->setError(e.cause);
	}
	glDisable(GL_TEXTURE_2D);
	th->commonGLDeinit();
	glXMakeCurrent(d,None,NULL);
	glXDestroyContext(d,th->mContext);
	XCloseDisplay(d);
	return NULL;
}

//This is a GTK event handler and the gdk lock is already acquired
gboolean Engine::inputWorker(GtkWidget *widget, GdkEvent *event, InputThread* th)
{
	//Set sys to this SystemState
	sys=th->m_sys;
	gboolean ret=FALSE;
	switch(event->type)
	{
		case GDK_KEY_PRESS:
		{
			//cout << "key press" << endl;
			switch(event->key.keyval)
			{
				case GDK_p:
					th->m_sys->showProfilingData=!th->m_sys->showProfilingData;
					break;
				case GDK_m:
					if (!th->m_sys->audioManager->pluginLoaded())
						break;
					th->m_sys->audioManager->toggleMuteAll();
					if(th->m_sys->audioManager->allMuted())
						LOG(LOG_NO_INFO, "All sounds muted");
					else
						LOG(LOG_NO_INFO, "All sounds unmuted");
					break;
				case GDK_c:
					if(th->m_sys->hasError())
					{
						GtkClipboard *clipboard;
						clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
						gtk_clipboard_set_text(clipboard, th->m_sys->getErrorCause().c_str(),
								th->m_sys->getErrorCause().size());
						clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
						gtk_clipboard_set_text(clipboard, th->m_sys->getErrorCause().c_str(),
								th->m_sys->getErrorCause().size());
						LOG(LOG_NO_INFO, "Copied error to clipboard");
					}
					else
						LOG(LOG_NO_INFO, "No error to be coppied to clipboard");
					break;
				default:
					break;
			}
			ret=TRUE;
			break;
		}
		case GDK_EXPOSE:
		{
			//Signal the renderThread
			th->m_sys->getRenderThread()->draw(false);
			ret=TRUE;
			break;
		}
		case GDK_BUTTON_PRESS:
		{
			//Grab focus, to receive keypresses
			gtk_widget_grab_focus(widget);
			th->handleMouseDown(event->button.x,event->button.y);
			ret=TRUE;
			break;
		}
		case GDK_BUTTON_RELEASE:
		{
			th->handleMouseUp(event->button.x,event->button.y);
			ret=TRUE;
			break;
		}
		case GDK_MOTION_NOTIFY:
		{
			th->handleMouseMove(event->motion.x,event->motion.y);
			ret=TRUE;
			break;
		}
		default:
//#ifdef EXPENSIVE_DEBUG
//			cout << "GDKTYPE " << event->type << endl;
//#endif
			break;
	}
	return ret;
}
