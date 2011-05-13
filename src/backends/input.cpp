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

#include "scripting/abc.h"
#include "input.h"
#include "swf.h"
#include "rendering.h"
#include "compat.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

using namespace lightspark;
using namespace std;

InputThread::InputThread(SystemState* s):m_sys(s),terminated(false),
                                         mutexListeners("Input listeners"),mutexDragged("Input dragged"),curDragged(NULL),lastMouseDownTarget(NULL)
{
	LOG(LOG_NO_INFO,_("Creating input thread"));
}

static gboolean
event_callback(GtkWidget *widget, GdkEvent *event, void *user_data)
{
	InputThread *th = (InputThread *)user_data;
	return th->engine->inputWorker(widget, event, th);
}

void InputThread::start(Engine *e)
{
	engine=e;
	GtkWidget* container=e->x11Params->container;
	gtk_widget_set_can_focus(container,True);
	gtk_widget_add_events(container,GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
	                      GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK | GDK_EXPOSURE_MASK | GDK_VISIBILITY_NOTIFY_MASK |
	                      GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_FOCUS_CHANGE_MASK);
	g_signal_connect(G_OBJECT(container), "event", G_CALLBACK(event_callback), this);
}

InputThread::~InputThread()
{
	wait();
}

void InputThread::wait()
{
	if(terminated)
		return;
	terminated=true;
}

void InputThread::handleMouseDown(uint32_t x, uint32_t y)
{
	Locker locker(mutexListeners);
	_NR<InteractiveObject>selected = NullRef;
	try
	{
		selected=m_sys->hitTest(NullRef,x,y);
	}
	catch(LightsparkException& e)
	{
		LOG(LOG_ERROR,_("Error in input handling ") << e.cause);
		m_sys->setError(e.cause);
		return;
	}
	assert(maskStack.empty());
	if(selected==NULL)
		return;
	assert_and_throw(selected->getPrototype()->isSubClass(Class<InteractiveObject>::getClass()));
	lastMouseDownTarget=selected;
	//Add event to the event queue
	m_sys->currentVm->addEvent(selected,_MR(Class<MouseEvent>::getInstanceS("mouseDown",true)));
}

void InputThread::handleMouseUp(uint32_t x, uint32_t y)
{
	Locker locker(mutexListeners);
	_NR<InteractiveObject> selected = NullRef;
	try
	{
		selected=m_sys->hitTest(NullRef,x,y);
	}
	catch(LightsparkException& e)
	{
		LOG(LOG_ERROR,_("Error in input handling ") << e.cause);
		m_sys->setError(e.cause);
		return;
	}
	assert(maskStack.empty());
	if(selected==NULL)
		return;
	assert_and_throw(selected->getPrototype()->isSubClass(Class<InteractiveObject>::getClass()));
	//Add event to the event queue
	m_sys->currentVm->addEvent(selected,_MR(Class<MouseEvent>::getInstanceS("mouseUp",true)));
	if(lastMouseDownTarget==selected)
	{
		//Also send the click event
		m_sys->currentVm->addEvent(selected,_MR(Class<MouseEvent>::getInstanceS("click",true)));
		lastMouseDownTarget=NullRef;
	}
}

void InputThread::handleMouseMove(uint32_t x, uint32_t y)
{
	SpinlockLocker locker(inputDataSpinlock);
	mouseX = x;
	mouseY = y;
}

void InputThread::addListener(InteractiveObject* ob)
{
	Locker locker(mutexListeners);
	assert(ob);

#ifndef NDEBUG
	vector<InteractiveObject*>::const_iterator it=find(listeners.begin(),listeners.end(),ob);
	//Object is already register, should not happen
	assert_and_throw(it==listeners.end());
#endif
	
	//Register the listener
	listeners.push_back(ob);
}

void InputThread::removeListener(InteractiveObject* ob)
{
	Locker locker(mutexListeners);

	vector<InteractiveObject*>::iterator it=find(listeners.begin(),listeners.end(),ob);
	if(it==listeners.end()) //Listener not found
		return;
	
	//Unregister the listener
	listeners.erase(it);
}

void InputThread::enableDrag(Sprite* s, const lightspark::RECT& limit)
{
	Locker locker(mutexDragged);
	if(s==curDragged)
		return;
	
	if(curDragged) //Stop dragging the previous sprite
		curDragged->decRef();
	
	assert(s);
	//We need to avoid that the object is destroyed
	s->incRef();
	
	curDragged=s;
	dragLimit=limit;
}

void InputThread::disableDrag()
{
	Locker locker(mutexDragged);
	if(curDragged)
	{
		curDragged->decRef();
		curDragged=NULL;
	}
}

bool InputThread::isMasked(number_t x, number_t y) const
{
	for(uint32_t i=0;i<maskStack.size();i++)
	{
		number_t localX, localY;
		maskStack[i].m.multiply2D(x,y,localX,localY);
		if(!maskStack[i].d->isOpaque(localX, localY))
			return false;
	}

	return true;
}
