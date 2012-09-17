/**
 * Copyright 2010 JogAmp Community. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY JogAmp Community ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL JogAmp Community OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of JogAmp Community.
 */
 
#include "NativewindowCommon.h"
#include "Xmisc.h"
#include "jogamp_nativewindow_x11_X11Lib.h"

// #define VERBOSE_ON 1

#ifdef VERBOSE_ON
    #define DBG_PRINT(args...) fprintf(stderr, args);
#else
    #define DBG_PRINT(args...)
#endif


/* Linux headers don't work properly */
#define __USE_GNU
#include <dlfcn.h>
#undef __USE_GNU

/* Current versions of Solaris don't expose the XF86 extensions,
   although with the recent transition to Xorg this will probably
   happen in an upcoming release */
#if !defined(__sun_obsolete) && !defined(_HPUX)
#include <X11/extensions/xf86vmode.h>
#else
/* Need to provide stubs for these */
Bool XF86VidModeGetGammaRampSize(
    Display *display,
    int screen,
    int* size)
{
  return False;
}

Bool XF86VidModeGetGammaRamp(
    Display *display,
    int screen,
    int size,
    unsigned short *red_array,
    unsigned short *green_array,
    unsigned short *blue_array) {
  return False;
}
Bool XF86VidModeSetGammaRamp(
    Display *display,
    int screen,
    int size,
    unsigned short *red_array,
    unsigned short *green_array,
    unsigned short *blue_array) {
  return False;
}
#endif /* defined(__sun_obsolete) || defined(_HPUX) */

/* HP-UX doesn't define RTLD_DEFAULT. */
#if defined(_HPUX) && !defined(RTLD_DEFAULT)
#define RTLD_DEFAULT NULL
#endif

static const char * const ClazzNameBuffers = "com/jogamp/common/nio/Buffers";
static const char * const ClazzNameBuffersStaticCstrName = "copyByteBuffer";
static const char * const ClazzNameBuffersStaticCstrSignature = "(Ljava/nio/ByteBuffer;)Ljava/nio/ByteBuffer;";
static const char * const ClazzNameByteBuffer = "java/nio/ByteBuffer";
static const char * const ClazzNamePoint = "javax/media/nativewindow/util/Point";
static const char * const ClazzAnyCstrName = "<init>";
static const char * const ClazzNamePointCstrSignature = "(II)V";
static jclass X11UtilClazz = NULL;
static jmethodID getCurrentThreadNameID = NULL;
static jmethodID dumpStackID = NULL;
static jclass clazzBuffers = NULL;
static jmethodID cstrBuffers = NULL;
static jclass clazzByteBuffer = NULL;
static jclass pointClz = NULL;
static jmethodID pointCstr = NULL;

static void _initClazzAccess(JNIEnv *env) {
    jclass c;

    if(!NativewindowCommon_init(env)) return;

    getCurrentThreadNameID = (*env)->GetStaticMethodID(env, X11UtilClazz, "getCurrentThreadName", "()Ljava/lang/String;");
    if(NULL==getCurrentThreadNameID) {
        NativewindowCommon_FatalError(env, "FatalError Java_jogamp_nativewindow_x11_X11Lib: can't get method getCurrentThreadName");
    }
    dumpStackID = (*env)->GetStaticMethodID(env, X11UtilClazz, "dumpStack", "()V");
    if(NULL==dumpStackID) {
        NativewindowCommon_FatalError(env, "FatalError Java_jogamp_nativewindow_x11_X11Lib: can't get method dumpStack");
    }

    c = (*env)->FindClass(env, ClazzNameBuffers);
    if(NULL==c) {
        NativewindowCommon_FatalError(env, "FatalError Java_jogamp_nativewindow_x11_X11Lib: can't find %s", ClazzNameBuffers);
    }
    clazzBuffers = (jclass)(*env)->NewGlobalRef(env, c);
    (*env)->DeleteLocalRef(env, c);
    if(NULL==clazzBuffers) {
        NativewindowCommon_FatalError(env, "FatalError Java_jogamp_nativewindow_x11_X11Lib: can't use %s", ClazzNameBuffers);
    }
    c = (*env)->FindClass(env, ClazzNameByteBuffer);
    if(NULL==c) {
        NativewindowCommon_FatalError(env, "FatalError Java_jogamp_nativewindow_x11_X11Lib: can't find %s", ClazzNameByteBuffer);
    }
    clazzByteBuffer = (jclass)(*env)->NewGlobalRef(env, c);
    (*env)->DeleteLocalRef(env, c);
    if(NULL==c) {
        NativewindowCommon_FatalError(env, "FatalError Java_jogamp_nativewindow_x11_X11Lib: can't use %s", ClazzNameByteBuffer);
    }

    cstrBuffers = (*env)->GetStaticMethodID(env, clazzBuffers, 
                            ClazzNameBuffersStaticCstrName, ClazzNameBuffersStaticCstrSignature);
    if(NULL==cstrBuffers) {
        NativewindowCommon_FatalError(env, "FatalError Java_jogamp_nativewindow_x11_X11Lib: can't create %s.%s %s",
            ClazzNameBuffers, ClazzNameBuffersStaticCstrName, ClazzNameBuffersStaticCstrSignature);
    }

    c = (*env)->FindClass(env, ClazzNamePoint);
    if(NULL==c) {
        NativewindowCommon_FatalError(env, "FatalError Java_jogamp_nativewindow_x11_X11Lib: can't find %s", ClazzNamePoint);
    }
    pointClz = (jclass)(*env)->NewGlobalRef(env, c);
    (*env)->DeleteLocalRef(env, c);
    if(NULL==pointClz) {
        NativewindowCommon_FatalError(env, "FatalError Java_jogamp_nativewindow_x11_X11Lib: can't use %s", ClazzNamePoint);
    }
    pointCstr = (*env)->GetMethodID(env, pointClz, ClazzAnyCstrName, ClazzNamePointCstrSignature);
    if(NULL==pointCstr) {
        NativewindowCommon_FatalError(env, "FatalError Java_jogamp_nativewindow_x11_X11Lib: can't fetch %s.%s %s",
            ClazzNamePoint, ClazzAnyCstrName, ClazzNamePointCstrSignature);
    }
}

static JavaVM *jvmHandle = NULL;
static int jvmVersion = 0;

static void setupJVMVars(JNIEnv * env) {
    if(0 != (*env)->GetJavaVM(env, &jvmHandle)) {
        jvmHandle = NULL;
    }
    jvmVersion = (*env)->GetVersion(env);
}

static XErrorHandler origErrorHandler = NULL ;
static int errorHandlerQuiet = 0 ;
static int errorHandlerDebug = 0 ;
static int errorHandlerThrowException = 0;

static int x11ErrorHandler(Display *dpy, XErrorEvent *e)
{
    if(!errorHandlerQuiet) {
        const char * errnoStr = strerror(errno);
        char threadName[80];
        char errCodeStr[80];
        char reqCodeStr[80];

        int shallBeDetached = 0;
        JNIEnv *jniEnv = NativewindowCommon_GetJNIEnv(jvmHandle, jvmVersion, &shallBeDetached);

        (void) NativewindowCommon_GetStaticStringMethod(jniEnv, X11UtilClazz, getCurrentThreadNameID, threadName, sizeof(threadName), "n/a");
        snprintf(errCodeStr, sizeof(errCodeStr), "%d", e->request_code);
        XGetErrorDatabaseText(dpy, "XRequest", errCodeStr, "Unknown", reqCodeStr, sizeof(reqCodeStr));
        XGetErrorText(dpy, e->error_code, errCodeStr, sizeof(errCodeStr));

        fprintf(stderr, "Info: Nativewindow X11 Error (Thread: %s): %d - %s, dpy %p, id %x, # %d: %d:%d %s\n",
            threadName, e->error_code, errCodeStr, e->display, (int)e->resourceid, (int)e->serial,
            (int)e->request_code, (int)e->minor_code, reqCodeStr);

        if( errorHandlerDebug ) {
            (*jniEnv)->CallStaticVoidMethod(jniEnv, X11UtilClazz, dumpStackID);
        }

        if(errorHandlerThrowException) {
            if(NULL != jniEnv) {
                NativewindowCommon_throwNewRuntimeException(jniEnv, "Nativewindow X11 Error (Thread: %s): %d - %s, dpy %p, id %x, # %d: %d:%d %s\n",
                                                            threadName, e->error_code, errCodeStr, e->display, (int)e->resourceid, (int)e->serial,
                                                            (int)e->request_code, (int)e->minor_code, reqCodeStr);
            } else {
                fprintf(stderr, "Nativewindow X11 Error: null JNIEnv");
                #if 0
                    if(NULL!=origErrorHandler) {
                        origErrorHandler(dpy, e);
                    }
                #endif
            }
        }
        fflush(stderr);

        if (NULL != jniEnv && shallBeDetached) {
            (*jvmHandle)->DetachCurrentThread(jvmHandle);
        }
    }

    return 0;
}

void NativewindowCommon_x11ErrorHandlerEnable(JNIEnv * env, Display *dpy, int onoff, int quiet, int sync) {
    errorHandlerQuiet = quiet;
    if(onoff) {
        if(NULL==origErrorHandler) {
            setupJVMVars(env);
            origErrorHandler = XSetErrorHandler(x11ErrorHandler);
            if(sync && NULL!=dpy) {
                XSync(dpy, False);
            }
        }
    } else {
        if(NULL!=origErrorHandler) {
            if(sync && NULL!=dpy) {
                XSync(dpy, False);
            }
            XSetErrorHandler(origErrorHandler);
            origErrorHandler = NULL;
        }
    }
}

static XIOErrorHandler origIOErrorHandler = NULL;

static int x11IOErrorHandler(Display *dpy)
{
    const char * dpyName = XDisplayName(NULL);
    const char * errnoStr = strerror(errno);
    char threadName[80];
    int shallBeDetached = 0;
    JNIEnv *jniEnv = NativewindowCommon_GetJNIEnv(jvmHandle, jvmVersion, &shallBeDetached);

    (void) NativewindowCommon_GetStaticStringMethod(jniEnv, X11UtilClazz, getCurrentThreadNameID, threadName, sizeof(threadName), "n/a");

    fprintf(stderr, "Nativewindow X11 IOError (Thread %s): Display %p (%s): %s\n", threadName, dpy, dpyName, errnoStr);
    (*jniEnv)->CallStaticVoidMethod(jniEnv, X11UtilClazz, dumpStackID);

    if (NULL != jniEnv) {
        NativewindowCommon_FatalError(jniEnv, "Nativewindow X11 IOError (Thread %s): Display %p (%s): %s", threadName, dpy, dpyName, errnoStr);

        if (shallBeDetached) {
            (*jvmHandle)->DetachCurrentThread(jvmHandle);
        }
    }
    if(NULL!=origIOErrorHandler) {
        origIOErrorHandler(dpy);
    }
    return 0;
}

static void x11IOErrorHandlerEnable(int onoff, JNIEnv * env) {
    if(onoff) {
        if(NULL==origIOErrorHandler) {
            setupJVMVars(env);
            origIOErrorHandler = XSetIOErrorHandler(x11IOErrorHandler);
        }
    } else {
        XSetIOErrorHandler(origIOErrorHandler);
        origIOErrorHandler = NULL;
    }
}

static int _initialized=0;

JNIEXPORT jboolean JNICALL 
Java_jogamp_nativewindow_x11_X11Util_initialize0(JNIEnv *env, jclass clazz, jboolean debug) {
    if(0==_initialized) {
        if(debug) {
            errorHandlerDebug = 1;
        }
        X11UtilClazz = (jclass)(*env)->NewGlobalRef(env, clazz);

        _initClazzAccess(env);
        x11IOErrorHandlerEnable(1, env);
        _initialized=1;
        if(JNI_TRUE == debug) {
            fprintf(stderr, "Info: NativeWindow native init passed\n");
        }
    }
    return JNI_TRUE;
}

JNIEXPORT void JNICALL 
Java_jogamp_nativewindow_x11_X11Util_shutdown0(JNIEnv *env, jclass _unused) {
    x11IOErrorHandlerEnable(0, env);
}

JNIEXPORT void JNICALL 
Java_jogamp_nativewindow_x11_X11Util_setX11ErrorHandler0(JNIEnv *env, jclass _unused, jboolean onoff, jboolean quiet) {
    NativewindowCommon_x11ErrorHandlerEnable(env, NULL, onoff ? 1 : 0, quiet ? 1 : 0, 0 /* no dpy, no sync */);
}

/*   Java->C glue code:
 *   Java package: jogamp.nativewindow.x11.X11Lib
 *    Java method: XVisualInfo XGetVisualInfo(long arg0, long arg1, XVisualInfo arg2, java.nio.IntBuffer arg3)
 *     C function: XVisualInfo *  XGetVisualInfo(Display * , long, XVisualInfo * , int * );
 */
JNIEXPORT jobject JNICALL
Java_jogamp_nativewindow_x11_X11Lib_XGetVisualInfo1__JJLjava_nio_ByteBuffer_2Ljava_lang_Object_2I(JNIEnv *env, jclass _unused, jlong arg0, jlong arg1, jobject arg2, jobject arg3, jint arg3_byte_offset) {
  XVisualInfo * _ptr2 = NULL;
  int * _ptr3 = NULL;
  XVisualInfo *  _res;
  int count;
  jobject jbyteSource;
  jobject jbyteCopy;
    if(0==arg0) {
        NativewindowCommon_FatalError(env, "invalid display connection..");
    }
    if (arg2 != NULL) {
        _ptr2 = (XVisualInfo *) (((char*) (*env)->GetDirectBufferAddress(env, arg2)) + 0);
    }
  if (arg3 != NULL) {
    _ptr3 = (int *) (((char*) (*env)->GetPrimitiveArrayCritical(env, arg3, NULL)) + arg3_byte_offset);
  }
  NativewindowCommon_x11ErrorHandlerEnable(env, (Display *) (intptr_t) arg0, 1, 0, 0);
  _res = XGetVisualInfo((Display *) (intptr_t) arg0, (long) arg1, (XVisualInfo *) _ptr2, (int *) _ptr3);
  NativewindowCommon_x11ErrorHandlerEnable(env, (Display *) (intptr_t) arg0, 0, 0, 0);
  count = _ptr3[0];
  if (arg3 != NULL) {
    (*env)->ReleasePrimitiveArrayCritical(env, arg3, _ptr3, 0);
  }
  if (_res == NULL) return NULL;

  jbyteSource = (*env)->NewDirectByteBuffer(env, _res, count * sizeof(XVisualInfo));
  jbyteCopy   = (*env)->CallStaticObjectMethod(env, clazzBuffers, cstrBuffers, jbyteSource);

  XFree(_res);

  return jbyteCopy;
}

JNIEXPORT jint JNICALL 
Java_jogamp_nativewindow_x11_X11Lib_GetVisualIDFromWindow(JNIEnv *env, jclass _unused, jlong display, jlong window) {
    Display * dpy = (Display *)(intptr_t)display;
    Window      w = (Window) window;
    XWindowAttributes xwa;
    jlong r = 0; // undefinded

    if(NULL==dpy) {
        NativewindowCommon_throwNewRuntimeException(env, "invalid display connection..");
        return;
    }

    NativewindowCommon_x11ErrorHandlerEnable(env, dpy, 1, 0, 1);
    memset(&xwa, 0, sizeof(XWindowAttributes));
    XGetWindowAttributes(dpy, w, &xwa);
    if(NULL != xwa.visual) {
        r = (jint) XVisualIDFromVisual( xwa.visual );
    } else {
        r = 0;
    }
    NativewindowCommon_x11ErrorHandlerEnable(env, dpy, 0, 0, 1);

    return r;
}


JNIEXPORT jint JNICALL 
Java_jogamp_nativewindow_x11_X11Lib_DefaultVisualID(JNIEnv *env, jclass _unused, jlong display, jint screen) {
  jlong r;
    if(0==display) {
        NativewindowCommon_FatalError(env, "invalid display connection..");
    }
  NativewindowCommon_x11ErrorHandlerEnable(env, (Display *) (intptr_t) display, 1, 0, 0);
  r = (jint) XVisualIDFromVisual( DefaultVisual( (Display*) (intptr_t) display, screen ) );
  NativewindowCommon_x11ErrorHandlerEnable(env, (Display *) (intptr_t) display, 0, 0, 0);
  return r;
}

/*   Java->C glue code:
 *   Java package: jogamp.nativewindow.x11.X11Lib
 *    Java method: void XLockDisplay(long display)
 *     C function: void XLockDisplay(Display *  display);
 */
JNIEXPORT void JNICALL 
Java_jogamp_nativewindow_x11_X11Lib_XLockDisplay__J(JNIEnv *env, jclass _unused, jlong display) {
  if(0==display) {
      NativewindowCommon_FatalError(env, "invalid display connection..");
  }
  XLockDisplay((Display *) (intptr_t) display);
}

/*   Java->C glue code:
 *   Java package: jogamp.nativewindow.x11.X11Lib
 *    Java method: void XUnlockDisplay(long display)
 *     C function: void XUnlockDisplay(Display *  display);
 */
JNIEXPORT void JNICALL 
Java_jogamp_nativewindow_x11_X11Lib_XUnlockDisplay__J(JNIEnv *env, jclass _unused, jlong display) {
  if(0==display) {
      NativewindowCommon_FatalError(env, "invalid display connection..");
  }
  XUnlockDisplay((Display *) (intptr_t) display);
}

/*   Java->C glue code:
 *   Java package: jogamp.nativewindow.x11.X11Lib
 *    Java method: int XCloseDisplay(long display)
 *     C function: int XCloseDisplay(Display *  display);
 */
JNIEXPORT jint JNICALL 
Java_jogamp_nativewindow_x11_X11Lib_XCloseDisplay__J(JNIEnv *env, jclass _unused, jlong display) {
  int _res;
  if(0==display) {
      NativewindowCommon_FatalError(env, "invalid display connection..");
  }
  NativewindowCommon_x11ErrorHandlerEnable(env, NULL, 1, 0, 0);
  _res = XCloseDisplay((Display *) (intptr_t) display);
  NativewindowCommon_x11ErrorHandlerEnable(env, NULL, 0, 0, 0);
  return _res;
}

/*
 * Class:     jogamp_nativewindow_x11_X11Lib
 * Method:    CreateDummyWindow
 * Signature: (JIIII)J
 */
JNIEXPORT jlong JNICALL Java_jogamp_nativewindow_x11_X11Lib_CreateDummyWindow
  (JNIEnv *env, jclass unused, jlong display, jint screen_index, jint visualID, jint width, jint height)
{
    Display * dpy  = (Display *)(intptr_t)display;
    int       scrn_idx = (int)screen_index;
    Window  windowParent = 0;
    Window  window = 0;

    XVisualInfo visualTemplate;
    XVisualInfo *pVisualQuery = NULL;
    Visual *visual = NULL;
    int depth;

    XSetWindowAttributes xswa;
    unsigned long attrMask;
    int n;

    Screen* scrn;

    if(NULL==dpy) {
        NativewindowCommon_FatalError(env, "invalid display connection..");
        return 0;
    }

    if(visualID<0) {
        NativewindowCommon_throwNewRuntimeException(env, "invalid VisualID ..");
        return 0;
    }

    NativewindowCommon_x11ErrorHandlerEnable(env, dpy, 1, 0, 0);

    scrn = ScreenOfDisplay(dpy, scrn_idx);

    // try given VisualID on screen
    memset(&visualTemplate, 0, sizeof(XVisualInfo));
    visualTemplate.screen = scrn_idx;
    visualTemplate.visualid = (VisualID)visualID;
    pVisualQuery = XGetVisualInfo(dpy, VisualIDMask|VisualScreenMask, &visualTemplate,&n);
    if(pVisualQuery!=NULL) {
        visual   = pVisualQuery->visual;
        depth    = pVisualQuery->depth;
        visualID = (jint)pVisualQuery->visualid;
        XFree(pVisualQuery);
        pVisualQuery=NULL;
    }
    DBG_PRINT( "X11: [CreateWindow] trying given (dpy %p, screen %d, visualID: %d, parent %p) found: %p\n", dpy, scrn_idx, (int)visualID, windowParent, visual);

    if (visual==NULL)
    { 
        NativewindowCommon_x11ErrorHandlerEnable(env, dpy, 0, 0, 1);
        NativewindowCommon_throwNewRuntimeException(env, "could not query Visual by given VisualID, bail out!");
        return 0;
    } 

    if(pVisualQuery!=NULL) {
        XFree(pVisualQuery);
        pVisualQuery=NULL;
    }

    if(0==windowParent) {
        windowParent = XRootWindowOfScreen(scrn);
    }

    attrMask  = ( CWBackingStore | CWBackingPlanes | CWBackingPixel | CWBackPixmap | 
                  CWBorderPixel | CWColormap | CWOverrideRedirect ) ;

    memset(&xswa, 0, sizeof(xswa));
    xswa.override_redirect = False; // use the window manager, always
    xswa.border_pixel = 0;
    xswa.background_pixmap = None;
    xswa.backing_store=NotUseful; /* NotUseful, WhenMapped, Always */
    xswa.backing_planes=0;        /* planes to be preserved if possible */
    xswa.backing_pixel=0;         /* value to use in restoring planes */

    xswa.colormap = XCreateColormap(dpy,
                                    XRootWindow(dpy, scrn_idx),
                                    visual,
                                    AllocNone);

    window = XCreateWindow(dpy,
                           windowParent,
                           0, 0,
                           width, height,
                           0, // border width
                           depth,
                           InputOutput,
                           visual,
                           attrMask,
                           &xswa);
    XSync(dpy, False);

    XSelectInput(dpy, window, 0); // no events

    NativewindowCommon_x11ErrorHandlerEnable(env, dpy, 0, 0, 1);

    DBG_PRINT( "X11: [CreateWindow] created window %p on display %p\n", window, dpy);

    return (jlong) window;
}


/*
 * Class:     jogamp_nativewindow_x11_X11Lib
 * Method:    DestroyDummyWindow
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_jogamp_nativewindow_x11_X11Lib_DestroyDummyWindow
  (JNIEnv *env, jclass unused, jlong display, jlong window)
{
    Display * dpy = (Display *)(intptr_t)display;
    Window      w = (Window) window;

    if(NULL==dpy) {
        NativewindowCommon_throwNewRuntimeException(env, "invalid display connection..");
        return;
    }

    NativewindowCommon_x11ErrorHandlerEnable(env, dpy, 1, 0, 0);
    XUnmapWindow(dpy, w);
    XSync(dpy, False);
    XDestroyWindow(dpy, w);
    NativewindowCommon_x11ErrorHandlerEnable(env, dpy, 0, 0, 1);
}

/*
 * Class:     jogamp_nativewindow_x11_X11Lib
 * Method:    GetRelativeLocation
 * Signature: (JIJJII)Ljavax/media/nativewindow/util/Point;
 */
JNIEXPORT jobject JNICALL Java_jogamp_nativewindow_x11_X11Lib_GetRelativeLocation0
  (JNIEnv *env, jclass unused, jlong jdisplay, jint screen_index, jlong jsrc_win, jlong jdest_win, jint src_x, jint src_y)
{
    Display * dpy = (Display *) (intptr_t) jdisplay;
    Screen * scrn = ScreenOfDisplay(dpy, (int)screen_index);
    Window root = XRootWindowOfScreen(scrn);
    Window src_win = (Window)jsrc_win;
    Window dest_win = (Window)jdest_win;
    int dest_x=-1;
    int dest_y=-1;
    Window child;
    Bool res;

    if( 0 == jdest_win ) { dest_win = root; }
    if( 0 == jsrc_win ) { src_win = root; }

    NativewindowCommon_x11ErrorHandlerEnable(env, dpy, 1, 0, 0);

    res = XTranslateCoordinates(dpy, src_win, dest_win, src_x, src_y, &dest_x, &dest_y, &child);

    NativewindowCommon_x11ErrorHandlerEnable(env, dpy, 0, 0, 0);

    DBG_PRINT( "X11: GetRelativeLocation0: %p %d/%d -> %p %d/%d - ok: %d\n",
        (void*)src_win, src_x, src_y, (void*)dest_win, dest_x, dest_y, (int)res);

    return (*env)->NewObject(env, pointClz, pointCstr, (jint)dest_x, (jint)dest_y);
}

