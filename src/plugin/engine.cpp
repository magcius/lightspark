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

#include "plugin.h"

void GtkPlugEngine::delayedCreation(GtkPlugEngine* th)
{
	X11Params* p=th->x11Params;
    SystemState* sys=th->system;
	//Create a plug in the XEmbed window
	p->container=gtk_plug_new((GdkNativeWindow)p->window);
	//Realize the widget now, as we need the X window
	gtk_widget_realize(p->container);
	//Show it now
	gtk_widget_show(p->container);
	gtk_widget_map(p->container);
	p->window=GDK_WINDOW_XWINDOW(p->container->window);
	XSync(p->display, False);
	//The lock is needed to avoid thread creation/destruction races
	Locker l(sys->mutex);
	if(sys->shutdown)
		return;
	sys->renderThread->start(th);
	sys->inputThread->start(th);
	//If the render rate is known start the render ticks
	if(th->renderRate)
		th->startRenderTicks();
}

void GtkPluginEngine::delayedStopping(GtkPlugEngine* th)
{
	//This is called from the plugin, also kill the stream
    th->pluginInst->mainDownloader->stop();
    th->system->stopEngines();
}

#ifndef GNASH_PATH
#error No GNASH_PATH defined
#endif

bool GtkPlugEngine::tryGnash(SystemState* sys) {
	Locker l(sys->mutex);

    //Check if the gnash standalone executable is available
	ifstream f(GNASH_PATH);
    bool gnashExists=false;
	if(f) gnashExists=true;
	f.close();

    if(!gnashExists)
        return false;

    if(sys->dumpedSWFPath.len()==0) //The path is not known yet
    {
        sys->waitingForDump=true;
        l.unlock();
        sys->fileDumpAvailable.wait();
        if(shutdown)
            return false;
        l.lock();
    }
    LOG(LOG_NO_INFO,_("Trying to invoke gnash!"));
    //Dump the cookies to a temporary file
    strcpy(cookiesFileName,"/tmp/lightsparkcookiesXXXXXX");
    int file=mkstemp(cookiesFileName);
    if(file!=-1)
    {
        std::string data("Set-Cookie: " + rawCookies);
        size_t res;
        size_t written = 0;
        // Keep writing until everything we wanted to write actually got written
        do
        {
            res = write(file, data.c_str()+written, data.size()-written);
            if(res < 0)
            {
                LOG(LOG_ERROR, _("Error during writing of cookie file for Gnash"));
                break;
            }
            written += res;
        } while(written < data.size());
        close(file);
        setenv("GNASH_COOKIES_IN", cookiesFileName, 1);
    }
    else
        cookiesFileName[0]=0;
    sigset_t oldset;
    sigset_t set;
    sigfillset(&set);
    //Blocks all signal to avoid terminating while forking with the browser signal handlers on
    pthread_sigmask(SIG_SETMASK, &set, &oldset);

    // This will be used to pipe the SWF's data to Gnash's stdin
    int gnashStdin[2];
    pipe(gnashStdin);

    childPid=fork();
    if(childPid==-1)
    {
        //Restore handlers
        pthread_sigmask(SIG_SETMASK, &oldset, NULL);
        LOG(LOG_ERROR,_("Child process creation failed, lightspark continues"));
        childPid=0;
    }
    else if(childPid==0) //Child process scope
    {
        // Close write end of Gnash's stdin pipe, we will only read
        close(gnashStdin[1]);
        // Point stdin to the read end of Gnash's stdin pipe
        dup2(gnashStdin[0], fileno(stdin));
        // Close the read end of the pipe
        close(gnashStdin[0]);

        //Allocate some buffers to store gnash arguments
        char bufXid[32];
        char bufWidth[32];
        char bufHeight[32];
        snprintf(bufXid,32,"%lu",x11Params.window);
        snprintf(bufWidth,32,"%u",x11Params.width);
        snprintf(bufHeight,32,"%u",x11Params.height);
        string params("FlashVars=");
        params+=rawParameters;
        char *const args[] =
			{
				strdup("gnash"), //argv[0]
				strdup("-x"), //Xid
				bufXid,
				strdup("-j"), //Width
				bufWidth,
				strdup("-k"), //Height
				bufHeight,
				strdup("-u"), //SWF url
				strdup(sys->origin.getParsedURL().raw_buf()),
				strdup("-P"), //SWF parameters
				strdup(params.c_str()),
				strdup("-vv"),
				strdup("-"),
				NULL
			};

        // Print out an informative message about how we are invoking Gnash
        {
            int i = 1;
            std::string argsStr = "";
            while(args[i] != NULL)
            {
                argsStr += " ";
                argsStr += args[i];
                i++;
            }
            cerr << "Invoking '" << GNASH_PATH << argsStr << " < " << sys->dumpedSWFPath.raw_buf() << "'" << endl;
        }

        //Avoid calling browser signal handler during the short time between enabling signals and execve
        sigaction(SIGTERM, NULL, NULL);
        //Restore handlers
        pthread_sigmask(SIG_SETMASK, &oldset, NULL);
        execve(GNASH_PATH, args, environ);
        //If we are here execve failed, print an error and die
        cerr << _("Execve failed, content will not be rendered") << endl;
        exit(0);
    }
    else //Parent process scope
    {
        // Pass the SWF's data to Gnash
        {
            // Close read end of stdin pipe, we will only write to it.
            close(gnashStdin[0]);
            // Open the SWF file
            std::ifstream swfStream(dumpedSWFPath.raw_buf(), ios::binary);
            // Read the SWF file and write it to Gnash's stdin
            char data[1024];
            std::streamsize written, ret;
            bool stop = false;
            while(!swfStream.eof() && !swfStream.fail() && !stop)
            {
                swfStream.read(data, 1024);
                // Keep writing until everything we wanted to write actually got written
                written = 0;
                do
                {
                    ret = write(gnashStdin[1], data+written, swfStream.gcount()-written);
                    if(ret < 0)
                    {
                        LOG(LOG_ERROR, _("Error during writing of SWF file to Gnash"));
                        stop = true;
                        break;
                    }
                    written += ret;
                } while(written < swfStream.gcount());
            }
            // Close the write end of Gnash's stdin, signalling EOF to Gnash.
            close(gnashStdin[1]);
            // Close the SWF file
            swfStream.close();
        }

        //Restore handlers
        pthread_sigmask(SIG_SETMASK, &oldset, NULL);
        //Engines should not be started, stop everything
        l.unlock();
        //We cannot stop the engines now, as this is inside a ThreadPool job
        NSN_PluginThreadAsyncCall(pluginInst->mInstance, delayedStopping, this);
        return true;
    }
    return false;
}

void GtkPlugEngine::execute(SystemState *sys) {
    if (tryGnash(sys))
        return;

    system=sys;
    NSN_PluginThreadAsyncCall(pluginInst->mInstance, delayedCreation, this);
}
