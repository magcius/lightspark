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

#ifndef BACKENDS_ENGINE_H
#define BACKENDS_ENGINE_H

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "swf.h"

namespace lightspark
{

struct X11Params
{
	Display* display;
	GtkWidget* container;
	VisualID visual;
	Window window;
	int width;
	int height;
};

class Engine {
public:
    gboolean inputWorker(GtkWidget*, GdkEvent*, InputThread*);
    void* renderWorker(RenderThread*);
    virtual void execute(SystemState*);
    X11Params* x11Params;
    SystemState* system;

    virtual ~Engine() {
        delete x11Params;
    }
};

};
#endif
