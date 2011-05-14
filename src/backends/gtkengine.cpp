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

#include "backends/gtkengine.h"
#include "backends/rendering.h"
#include "backends/input.h"
#include "swf.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

using namespace lightspark;

void GtkEngine::destroy(GtkWidget *widget, void *user_data) {
    GtkEngine *th = (GtkEngine*)user_data;
    th->system->stopEngines();
}

void GtkEngine::bootstrap(SystemState *sys) {
    gtk_init(0, NULL);
    system=sys;

    X11Params *p=new X11Params();

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    p->container=window;

    RECT size=system->getFrameSize();
    p->width=size.Xmax/20;
    p->height=size.Ymax/20;

    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), this);
    gtk_window_set_title(GTK_WINDOW(window), "Lightspark");
    gtk_window_set_default_size(GTK_WINDOW(window), p->width, p->height);
    // gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    // We need the X window.
    gtk_widget_realize(window);
    gtk_widget_show(window);
    gtk_widget_map(window);

    GdkWindow *gdk = gtk_widget_get_window(window);

    p->display=GDK_WINDOW_XDISPLAY(gdk);
    p->window=GDK_WINDOW_XWINDOW(gdk);

    Visual *vis=gdk_x11_visual_get_xvisual(gdk_window_get_visual(gdk));

    p->visual=XVisualIDFromVisual(vis);

    x11Params=p;

	sys->getRenderThread()->start(this);
	sys->getInputThread()->start(this);

    gtk_main();
}

void GtkEngine::execute(SystemState *sys) {
}
