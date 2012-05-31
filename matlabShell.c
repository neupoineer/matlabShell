/* */
/*
 *	matlabShell.c
 *
 *	This is a simple program call MATLAB matlab.el
 *
 *    Copyright (c) 1998 by Robert A. Morris
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * If you did not receive a copy of the GPL you can get it at
 *     //http://www.fsf.org
 * 
 * This program is a matlab shell which will run under WindowsNT, and possibly
 * any OS, and can be invoked either from from matlab.el or a native
 * command shell.
 *
 * See the usage notes at the end of this file, or invoke it with
 * -h argument for brief usage message

 *      02dec98 version 1.0 ram@cs.umb.edu
 *         -remove echo; instead require 
 *              matlab-shell-process-echoes nil
 *              in matlab-shell-mode-hook
 *         -works with matlab.el ver 2.2
 *      01nov98 version 0.91 ram@cs.umb.edu
 *      Bugs fixed:
 *        "exit" command processing should be case sensitive
 *        input not echoed properly
 *        line spacing different from Matlab standard window
 *
 *        - Matlab "exit" command must be all lower case, so
 *          replace isExitCommand() with simple strncmp()
 *        - echo input because something is erasing it on NT. Maybe comint.el?
 *        - make line spacing look like Matlab native shell
 *      Known deficiencies in 0.91:
 *      1. Matlab standard command window pops up when starting
 *      2. Matlab window doesn't exit if *matlab* buffer is killed without
 *       sending exit command to matlab from matlabShell
 *
 *	01nov98 version 0.9 ram@cs.umb.edu
 *	Known deficencies in 0.9
 *	1. should be quiet production mode and verbose C debugging modes
 *      2. Matlab Debug is untested, probably doesn't work
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <Windows.h>
#include "engine.h"

void printErr() {
  DWORD err = GetLastError();
  WCHAR buffer[1024];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, LANG_SYSTEM_DEFAULT, buffer, 1024, NULL);
  wprintf(buffer);
}

#define MAXLEN 1024;		/* default */
int main(int argc, char **argv)
{
  char version[]="MatlabShell 1.0. 02Dec1998.\nCopyright 1998 Robert A. Morris. \nThis is Free Software licensed under the GNU Public License.";
	Engine *ep;
	int inputMax; /* buffer size */
	char *inbuf;

	int outputMax; /*buffer size */
	char *fromEngine; 

	int noArgs;
	int len, pos;

	int debug = 0;
	int retval; /* for debug */
	
	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	DWORD dwRead;
	DWORD dwRes;

	/* matlab.el always invokes the shell command with two
	   arguments, the second of which is NULL unless the lisp
	   variable matlab-shell-command-switches is set, in which
	   case the string value of that variable is passed as the
	   argv[1] to the shell program. In that case we have to parse
	   this string. In the standalone case, we may see 1, 2, or 3
	   args, the first of which is always the program path
	*/

	printf("%s\n", version);
	noArgs = (argc==1) || (argv[1]==0 || argv[1][0] == 0);

	if ( (!noArgs) && (argv[1][0] == '?' || !strcmp(argv[1], "-h"))) {
	  printf("usage: %s <inbufSize> <outbufSize>", argv[0]);
	  exit(0);
	}

	/* Start the MATLAB engine */
	if (!(ep = engOpen(NULL))) {
	  printf("Can not start engine\n");
	  exit(-1);
	}

	inputMax = MAXLEN;
	outputMax = 0;
	
	/* if there are args they might be:
	   1. one string from matlab.el with either 1 or 2 numbers
	   2. one or two strings from a command shell invocation
	*/
	if ( !noArgs ){ 
	  inputMax = atoi(argv[1]);
	  if (argc>2 && argv[2]) /* doesn't happen under matlab.el */
	    outputMax = atoi(argv[2]);
	  else { /*matlab.el passes args as a single string */
	    len = strlen(argv[1]);
	    pos = strcspn(argv[1], " \t\n"); /* scan to white space */
	    if (debug) printf("argv[1]=%s len=%d pos=%d\n", argv[1], len, pos);
	    argv[1][pos]=0; /* split */
	    inputMax = atoi(argv[1]);
	    if (pos < len) /* there was stuff left */
	      outputMax = atoi(1+pos+argv[1]);
	  }
	}
	if (!outputMax)		/* nobody set it */
	  outputMax = 8*inputMax;

	inbuf = malloc(inputMax+2); /* room for newline and \0 */
	outputMax = inputMax*8;
	fromEngine = malloc(outputMax +2);
	engOutputBuffer(ep, fromEngine, outputMax);

	/* Vista+ only :( Any easy way to distinguish pipe from console??? */
	/* if (debug) { */
	/*   if (GetFileInformationByHandleEx(hStdIn, FileNameInfo, inbuf, inputMax)) { */
	/*     wprintf(((PFILE_NAME_INFO)inbuf)->FileName); */
	/*   } */
	/* } */

	printf(">> "); fflush(stdout);
	len = 0;
	while (1) {
	  do {
	    /* accumulate more input if available */
	    if ( !ReadFile(hStdIn, inbuf + len, inputMax - len, &dwRead, NULL)) {
	      printErr();
	    } else {
	      len += dwRead;
	      if (debug) {
		printf("Got %d bytes\n", dwRead);
	      }
	    }
	    printf(">> "); fflush(stdout); /* this makes matlab-mode happy */
	    Sleep(250);
	    if (!PeekNamedPipe(hStdIn, NULL, 0, NULL, &dwRead, NULL)) {
	      printErr();
	      dwRes = WAIT_TIMEOUT;
	    } else {
	      dwRes = dwRead > 0 ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
	    }
	    if (debug) {
	      printf("WAIT_OBJECT_0 == dwRes  = %d\n", WAIT_OBJECT_0 == dwRes);
	    }
	  } while (WAIT_OBJECT_0 == dwRes);

	  if (!len) continue;
	    
	    /* On NT, something erases input and I don't know what. It
	     might be the way comint.el is passing input to this
	     process. If this is platform dependent then other platforms
	     may see doubled input  */

	  //	  printf("%s",inbuf);   fflush(stdout);	/* re-echo input */
	  
	  /* it would be good to test retval, but on NT it seems
	     to return non-zero whether the engine is
	     running or not, contrary to Matlab doc.
	     In fact, I can't figure out how to know whether the
	     engine is running, so special case "exit"
	  */
	  
	  inbuf[len-1] = 0;
	  retval = engEvalString(ep, inbuf);
	  if (debug) {
	    printf("retval=%x\n", retval);
	    fflush(stdout);
	  }				/* swap around to make ob-comint happy with eoe */
	  if (retval) {
	    printf("exiting\n"); fflush(stdout);
	    break;
	  }
	  if (fromEngine[0] == 0 ){ /*the command didn't return anything */
	    if(debug)
	      printf("\ncmd returned nothing");
	  }
	  else {
	    char fmt[10];
	    char *next = fromEngine;
	    const char pattern[] = "\nans =\n\n";
	    char *pch;
	    do {		/* answer by answer to make ob-octave happy */
	      pch = strstr(next+1, pattern);
	      if (pch > next) {
		sprintf(fmt, "%%.%ds>> ", pch - next);
		printf(fmt, next);
		next = pch;
	      }
	    } while (pch != NULL);
	    printf("%s>> ", next); fflush(stdout);
	    fromEngine[0] = 0; /* clear buffer, else confusing */
	  }
	  len = 0;
        }
        return 0;
}
