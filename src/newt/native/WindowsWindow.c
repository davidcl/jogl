/*
 * Copyright (c) 2008 Sun Microsystems, Inc. All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * - Redistribution of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 * - Redistribution in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of Sun Microsystems, Inc. or the names of
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind. ALL
 * EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN
 * MICROSYSTEMS, INC. ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE FOR
 * ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR
 * DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES. IN NO EVENT WILL SUN OR
 * ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA, OR FOR
 * DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE
 * DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF LIABILITY,
 * ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE, EVEN IF
 * SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * 
 */

#include <Windows.h>
#include <Windowsx.h>
#include <tchar.h>
#include <stdlib.h>
// NOTE: it looks like SHFullScreen and/or aygshell.dll is not available on the APX 2500 any more
// #ifdef UNDER_CE
// #include "aygshell.h"
// #endif

/* This typedef is apparently needed for Microsoft compilers before VC8,
   and on Windows CE */
#if (_MSC_VER < 1400) || defined(UNDER_CE)
#ifdef _WIN64
typedef long long intptr_t;
#else
typedef int intptr_t;
#endif
#endif

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL                   0x020A
#endif //WM_MOUSEWHEEL

#ifndef WHEEL_DELTA
#define WHEEL_DELTA                     120
#endif //WHEEL_DELTA

#ifndef WHEEL_PAGESCROLL
#define WHEEL_PAGESCROLL                (UINT_MAX)
#endif //WHEEL_PAGESCROLL

#ifndef GET_WHEEL_DELTA_WPARAM  // defined for (_WIN32_WINNT >= 0x0500)
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif

#include "com_sun_javafx_newt_windows_WindowsWindow.h"

#include "EventListener.h"
#include "MouseEvent.h"
#include "InputEvent.h"
#include "KeyEvent.h"

static jmethodID sizeChangedID = NULL;
static jmethodID positionChangedID = NULL;
static jmethodID focusChangedID = NULL;
static jmethodID windowDestroyNotifyID = NULL;
static jmethodID windowDestroyedID = NULL;
static jmethodID sendMouseEventID = NULL;
static jmethodID sendKeyEventID = NULL;

// This is set by DispatchMessages, below, and cleared when it exits
static JNIEnv* env = NULL;

// Really need to factor this out in to a separate run-time file
static jchar* GetNullTerminatedStringChars(JNIEnv* env, jstring str)
{
    jchar* strChars = NULL;
    strChars = calloc((*env)->GetStringLength(env, str) + 1, sizeof(jchar));
    if (strChars != NULL) {
        (*env)->GetStringRegion(env, str, 0, (*env)->GetStringLength(env, str), strChars);
    }
    return strChars;
}

static jint GetModifiers() {
    jint modifiers = 0;
    if (HIBYTE(GetKeyState(VK_CONTROL)) != 0) {
        modifiers |= EVENT_CTRL_MASK;
    }
    if (HIBYTE(GetKeyState(VK_SHIFT)) != 0) {
        modifiers |= EVENT_SHIFT_MASK;
    }
    if (HIBYTE(GetKeyState(VK_MENU)) != 0) {
        modifiers |= EVENT_ALT_MASK;
    }
    if (HIBYTE(GetKeyState(VK_LBUTTON)) != 0) {
        modifiers |= EVENT_BUTTON1_MASK;
    }
    if (HIBYTE(GetKeyState(VK_MBUTTON)) != 0) {
        modifiers |= EVENT_BUTTON2_MASK;
    }
    if (HIBYTE(GetKeyState(VK_RBUTTON)) != 0) {
        modifiers |= EVENT_BUTTON3_MASK;
    }

    return modifiers;
}

static int WmChar(JNIEnv *env, jobject window, UINT character, UINT repCnt,
                  UINT flags, BOOL system)
{
    // The Alt modifier is reported in the 29th bit of the lParam,
    // i.e., it is the 13th bit of `flags' (which is HIWORD(lParam)).
    BOOL alt_is_down = (flags & (1<<13)) != 0;
    if (system && alt_is_down) {
        if (character == VK_SPACE) {
            return 1;
        }
    }
    (*env)->CallVoidMethod(env, window, sendKeyEventID,
                           (jint) EVENT_KEY_TYPED,
                           GetModifiers(),
                           (jint) -1,
                           (jchar) character);
    return 1;
}

static LRESULT CALLBACK wndProc(HWND wnd, UINT message,
                                WPARAM wParam, LPARAM lParam)
{
    RECT rc;
    int useDefWindowProc = 0;
    jobject window = NULL;
    BOOL isKeyDown = FALSE;

#if defined(UNDER_CE) || _MSC_VER <= 1200
    window = (jobject) GetWindowLong(wnd, GWL_USERDATA);
#else
    window = (jobject) GetWindowLongPtr(wnd, GWLP_USERDATA);
#endif
    if (window == NULL || env == NULL) {
        // Shouldn't happen
        return DefWindowProc(wnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_CLOSE:
        (*env)->CallVoidMethod(env, window, windowDestroyNotifyID);
        // Called by Window.java: DestroyWindow(wnd);
        break;

    case WM_DESTROY:
        (*env)->CallVoidMethod(env, window, windowDestroyedID);
        break;

    case WM_SYSCHAR:
        useDefWindowProc = WmChar(env, window, wParam,
                                  LOWORD(lParam), HIWORD(lParam), FALSE);
        break;

    case WM_CHAR:
        useDefWindowProc = WmChar(env, window, wParam,
                                  LOWORD(lParam), HIWORD(lParam), TRUE);
        break;
        
    case WM_KEYDOWN:
        (*env)->CallVoidMethod(env, window, sendKeyEventID, (jint) EVENT_KEY_PRESSED,
                               GetModifiers(), (jint) wParam, (jchar) -1);
        useDefWindowProc = 1;
        break;

    case WM_KEYUP:
        (*env)->CallVoidMethod(env, window, sendKeyEventID, (jint) EVENT_KEY_RELEASED,
                               GetModifiers(), (jint) wParam, (jchar) -1);
        useDefWindowProc = 1;
        break;

    case WM_SIZE:
        GetClientRect(wnd, &rc);
        (*env)->CallVoidMethod(env, window, sizeChangedID, (jint) rc.right, (jint) rc.bottom);
        break;

    case WM_LBUTTONDOWN:
        (*env)->CallVoidMethod(env, window, sendMouseEventID,
                               (jint) EVENT_MOUSE_PRESSED,
                               GetModifiers(),
                               (jint) LOWORD(lParam), (jint) HIWORD(lParam),
                               (jint) 1, (jint) 0);
        useDefWindowProc = 1;
        break;

    case WM_LBUTTONUP:
        (*env)->CallVoidMethod(env, window, sendMouseEventID,
                               (jint) EVENT_MOUSE_RELEASED,
                               GetModifiers(),
                               (jint) LOWORD(lParam), (jint) HIWORD(lParam),
                               (jint) 1, (jint) 0);
        useDefWindowProc = 1;
        break;

    case WM_MBUTTONDOWN:
        (*env)->CallVoidMethod(env, window, sendMouseEventID,
                               (jint) EVENT_MOUSE_PRESSED,
                               GetModifiers(),
                               (jint) LOWORD(lParam), (jint) HIWORD(lParam),
                               (jint) 2, (jint) 0);
        useDefWindowProc = 1;
        break;

    case WM_MBUTTONUP:
        (*env)->CallVoidMethod(env, window, sendMouseEventID,
                               (jint) EVENT_MOUSE_RELEASED,
                               GetModifiers(),
                               (jint) LOWORD(lParam), (jint) HIWORD(lParam),
                               (jint) 2, (jint) 0);
        useDefWindowProc = 1;
        break;

    case WM_RBUTTONDOWN:
        (*env)->CallVoidMethod(env, window, sendMouseEventID,
                               (jint) EVENT_MOUSE_PRESSED,
                               GetModifiers(),
                               (jint) LOWORD(lParam), (jint) HIWORD(lParam),
                               (jint) 3, (jint) 0);
        useDefWindowProc = 1;
        break;

    case WM_RBUTTONUP:
        (*env)->CallVoidMethod(env, window, sendMouseEventID,
                               (jint) EVENT_MOUSE_RELEASED,
                               GetModifiers(),
                               (jint) LOWORD(lParam), (jint) HIWORD(lParam),
                               (jint) 3,  (jint) 0);
        useDefWindowProc = 1;
        break;

    case WM_MOUSEMOVE:
        (*env)->CallVoidMethod(env, window, sendMouseEventID,
                               (jint) EVENT_MOUSE_MOVED,
                               GetModifiers(),
                               (jint) LOWORD(lParam), (jint) HIWORD(lParam),
                               (jint) 0,  (jint) 0);
        useDefWindowProc = 1;
        break;

    case WM_MOUSEWHEEL: {
        // need to convert the coordinates to component-relative
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        POINT eventPt;
        eventPt.x = x;
        eventPt.y = y;
        ScreenToClient(wnd, &eventPt);
        (*env)->CallVoidMethod(env, window, sendMouseEventID,
                               (jint) EVENT_MOUSE_WHEEL_MOVED,
                               GetModifiers(),
                               (jint) eventPt.x, (jint) eventPt.y,
                               (jint) 0,  (jint) GET_WHEEL_DELTA_WPARAM(wParam));
        useDefWindowProc = 1;
        break;
    }

    case WM_SETFOCUS:
        (*env)->CallVoidMethod(env, window, focusChangedID,
                               (jlong)wParam, JNI_TRUE);
        useDefWindowProc = 1;
        break;

    case WM_KILLFOCUS:
        (*env)->CallVoidMethod(env, window, focusChangedID,
                               (jlong)wParam, JNI_FALSE);
        useDefWindowProc = 1;
        break;

    case WM_MOVE:
        (*env)->CallVoidMethod(env, window, positionChangedID,
                               (jint)LOWORD(lParam), (jint)HIWORD(lParam));
        useDefWindowProc = 1;
        break;

    case WM_ERASEBKGND:
        // ignore erase background
        useDefWindowProc = 0;
        break;


    // FIXME: generate EVENT_MOUSE_ENTERED, EVENT_MOUSE_EXITED
    default:
        useDefWindowProc = 1;
    }

    if (useDefWindowProc)
        return DefWindowProc(wnd, message, wParam, lParam);
    return 0;
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    initIDs
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_initIDs
  (JNIEnv *env, jclass clazz)
{
    sizeChangedID = (*env)->GetMethodID(env, clazz, "sizeChanged", "(II)V");
    positionChangedID = (*env)->GetMethodID(env, clazz, "positionChanged", "(II)V");
    focusChangedID = (*env)->GetMethodID(env, clazz, "focusChanged", "(JZ)V");
    windowDestroyNotifyID    = (*env)->GetMethodID(env, clazz, "windowDestroyNotify",    "()V");
    windowDestroyedID = (*env)->GetMethodID(env, clazz, "windowDestroyed", "()V");
    sendMouseEventID = (*env)->GetMethodID(env, clazz, "sendMouseEvent", "(IIIIII)V");
    sendKeyEventID = (*env)->GetMethodID(env, clazz, "sendKeyEvent", "(IIIC)V");
    if (sizeChangedID == NULL ||
        positionChangedID == NULL ||
        focusChangedID == NULL ||
        windowDestroyNotifyID == NULL ||
        windowDestroyedID == NULL ||
        sendMouseEventID == NULL ||
        sendKeyEventID == NULL) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    LoadLibraryW
 * Signature: (Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_LoadLibraryW
  (JNIEnv *env, jclass clazz, jstring dllName)
{
    jchar* _dllName = GetNullTerminatedStringChars(env, dllName);
    HMODULE lib = LoadLibraryW(_dllName);
    free(_dllName);
    return (jlong) (intptr_t) lib;
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    RegisterWindowClass
 * Signature: (Ljava/lang/String;J)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_RegisterWindowClass
  (JNIEnv *env, jclass clazz, jstring appName, jlong hInstance)
{
    WNDCLASS* wc;
#ifndef UNICODE
    const char* _appName = NULL;
#endif

    wc = calloc(1, sizeof(WNDCLASS));
    /* register class */
    wc->style = CS_HREDRAW | CS_VREDRAW;
    wc->lpfnWndProc = (WNDPROC)wndProc;
    wc->cbClsExtra = 0;
    wc->cbWndExtra = 0;
    /* This cast is legal because the HMODULE for a DLL is the same as
       its HINSTANCE -- see MSDN docs for DllMain */
    wc->hInstance = (HINSTANCE) hInstance;
    wc->hIcon = NULL;
    wc->hCursor = LoadCursor(NULL, MAKEINTRESOURCE(IDC_ARROW));
    wc->hbrBackground = GetStockObject(BLACK_BRUSH);
    wc->lpszMenuName = NULL;
#ifdef UNICODE
    wc->lpszClassName = GetNullTerminatedStringChars(env, appName);
#else
    _appName = (*env)->GetStringUTFChars(env, appName, NULL);
    wc->lpszClassName = strdup(_appName);
    (*env)->ReleaseStringUTFChars(env, appName, _appName);
#endif
    if (!RegisterClass(wc)) {
        free((void *)wc->lpszClassName);
        free(wc);
        return 0;
    }
    return (jlong) (intptr_t) wc;
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    CreateWindow
 * Signature: (Ljava/lang/String;JJZIIII)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_CreateWindow
  (JNIEnv *env, jobject obj, jstring windowClassName, jlong hInstance, jlong visualID,
        jboolean bIsUndecorated,
        jint jx, jint jy, jint defaultWidth, jint defaultHeight)
{
    const TCHAR* wndClassName = NULL;
    DWORD windowStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE;
    int x=(int)jx, y=(int)jy;
    int width=(int)defaultWidth, height=(int)defaultHeight;
    HWND window = NULL;

#ifdef UNICODE
    wndClassName = GetNullTerminatedStringChars(env, windowClassName);
#else
    wndClassName = (*env)->GetStringUTFChars(env, windowClassName, NULL);
#endif

    // FIXME: until we have valid values coming down from the
    // application code, use some default values
#ifdef UNDER_CE
    // Prefer to not have any surprises in the initial window sizing or placement
    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
    x = y = 0;
#else
    x = CW_USEDEFAULT;
    y = 0;
    if (bIsUndecorated) {
        windowStyle |= WS_POPUP | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    } else {
        windowStyle |= WS_OVERLAPPEDWINDOW;
    }
#endif

    (void) visualID; // FIXME: use the visualID ..

    window = CreateWindow(wndClassName, wndClassName, windowStyle,
                          x, y, width, height,
                          NULL, NULL,
                          (HINSTANCE) hInstance,
                          NULL);
#ifdef UNICODE
    free((void*) wndClassName);
#else
    (*env)->ReleaseStringUTFChars(env, windowClassName, wndClassName);
#endif
    
    if (window != NULL) {
#if defined(UNDER_CE) || _MSC_VER <= 1200
        SetWindowLong(window, GWL_USERDATA, (intptr_t) (*env)->NewGlobalRef(env, obj));
#else
        SetWindowLongPtr(window, GWLP_USERDATA, (intptr_t) (*env)->NewGlobalRef(env, obj));
#endif
        ShowWindow(window, SW_SHOWNORMAL);
    }
    return (jlong) (intptr_t) window;
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    DestroyWindow
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_DestroyWindow
  (JNIEnv *env, jobject obj, jlong window)
{
    DestroyWindow((HWND) window);
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    GetDC
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_GetDC
  (JNIEnv *env, jobject obj, jlong window)
{
    return (jlong) GetDC((HWND) window);
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    ReleaseDC
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_ReleaseDC
  (JNIEnv *env, jobject obj, jlong window, jlong dc)
{
    ReleaseDC((HWND) window, (HDC) dc);
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    setVisible0
 * Signature: (JZ)V
 */
JNIEXPORT void JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_setVisible0
  (JNIEnv *_env, jclass clazz, jlong window, jboolean visible)
{
    HWND hWnd = (HWND) (intptr_t) window;
    if (visible) {
        ShowWindow(hWnd, SW_SHOW);
    } else {
        ShowWindow(hWnd, SW_HIDE);
    }
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    DispatchMessages
 * Signature: (JI)V
 */
JNIEXPORT void JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_DispatchMessages
  (JNIEnv *_env, jclass clazz, jlong window, jint eventMask)
{
    int i = 0;
    MSG msg;
    BOOL gotOne;

    env = _env;

    if(eventMask<0) {
        eventMask *= -1;
        /* FIXME: re-select input mask 
            long xevent_mask_key = 0;
            long xevent_mask_ptr = 0;
            long xevent_mask_win = 0;
            if( 0 != ( eventMask & EVENT_MOUSE ) ) {
                xevent_mask_ptr |= ButtonPressMask|ButtonReleaseMask|PointerMotionMask;
            }
            if( 0 != ( eventMask & EVENT_KEY ) ) {
                xevent_mask_key |= KeyPressMask|KeyReleaseMask;
            }
            if( 0 != ( eventMask & EVENT_WINDOW ) ) {
                xevent_mask_win |= ExposureMask;
            }

            XSelectInput(dpy, w, xevent_mask_win|xevent_mask_key|xevent_mask_ptr);
        */
    }

    // Periodically take a break
    do {
        gotOne = PeekMessage(&msg, (HWND) window, 0, 0, PM_REMOVE);
        if (gotOne) {
            ++i;
            switch (msg.message) {
                case WM_CLOSE:
                case WM_DESTROY:
                case WM_SIZE:
                    if( ! ( eventMask & EVENT_WINDOW ) ) {
                        continue;
                    }
                    break;

                case WM_CHAR:
                case WM_KEYDOWN:
                case WM_KEYUP:
                    if( ! ( eventMask & EVENT_KEY ) ) {
                        continue;
                    }
                    break;

                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                case WM_MOUSEMOVE:
                    if( ! ( eventMask & EVENT_MOUSE ) ) {
                        continue;
                    }
                    break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    } while (gotOne && i < 100);

    env = NULL;
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    setSize0
 * Signature: (JII)V
 */
JNIEXPORT void JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_setSize0
  (JNIEnv *env, jobject obj, jlong window, jint width, jint height)
{
    RECT r;
    HWND w = (HWND) window;
    GetWindowRect(w, &r);
    MoveWindow(w, r.left, r.top, width, height, TRUE);
    (*env)->CallVoidMethod(env, obj, sizeChangedID, (jint) width, (jint) height);
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    setFullScreen0
 * Signature: (JZ)Z
 */
JNIEXPORT jboolean JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_setFullScreen0
  (JNIEnv *env, jobject obj, jlong window, jboolean fullscreen)
{
#ifdef UNDER_CE
    int screenWidth;
    int screenHeight;
    HWND win = (HWND) window;

    if (fullscreen) {
        screenWidth  = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);
        // NOTE: looks like SHFullScreen and/or aygshell.dll is not available on the APX 2500 any more
        // First, hide all of the shell parts
        // SHFullScreen(win,
        //              SHFS_HIDETASKBAR | SHFS_HIDESIPBUTTON | SHFS_HIDESTARTICON);
        MoveWindow(win, 0, 0, screenWidth, screenHeight, TRUE);
        (*env)->CallVoidMethod(env, obj, sizeChangedID, (jint) screenWidth, (jint) screenHeight);
    } else {
        RECT rc;
        int width, height;

        // First, show all of the shell parts
        // SHFullScreen(win,
        //              SHFS_SHOWTASKBAR | SHFS_SHOWSIPBUTTON | SHFS_SHOWSTARTICON);
        // Now resize the window to the size of the work area
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, FALSE);
        width = rc.right - rc.left;
        height = rc.bottom - rc.top;
        MoveWindow(win, rc.left, rc.top, width, height, TRUE);
        (*env)->CallVoidMethod(env, obj, sizeChangedID, (jint) width, (jint) height);
    }
    return JNI_TRUE;
#else
    /* For the time being, full-screen not supported on the desktop */
    return JNI_FALSE;
#endif
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    setPosition
 * Signature: (JII)V
 */
JNIEXPORT void JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_setPosition
  (JNIEnv *env, jclass clazz, jlong window, jint x, jint y)
{
    UINT flags = SWP_NOACTIVATE | SWP_NOZORDER;
    HWND hwnd = (HWND)window;
    RECT r;

    GetWindowRect(hwnd, &r);
    SetWindowPos(hwnd, 0, x, y, (r.right-r.left), (r.bottom-r.top), flags);
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    setTitle
 * Signature: (JLjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_setTitle
  (JNIEnv *env, jclass clazz, jlong window, jstring title)
{
    HWND hwnd = (HWND)window;
    if (title != NULL) {
        jchar *titleString = GetNullTerminatedStringChars(env, title);
        if (titleString != NULL) {
            SetWindowTextW(hwnd, titleString);
            free(titleString);
        }
    }
}

/*
 * Class:     com_sun_javafx_newt_windows_WindowsWindow
 * Method:    requestFocus
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_sun_javafx_newt_windows_WindowsWindow_requestFocus
  (JNIEnv *env, jclass clazz, jlong window)
{
    HWND hwnd = (HWND)window;

    if (IsWindow(hwnd)) {
        SetFocus(hwnd);
    }
}