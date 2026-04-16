// x11util.cpp - X11 calls isolated here, never include Qt headers in this file
#include "x11util.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <cstring>
#include <cstdio>

// Get display via dlsym at runtime to avoid linking issues
#include <dlfcn.h>

static Display* getDisplay() {
    // Try XOpenDisplay as fallback
    static Display* dpy = nullptr;
    if (!dpy) dpy = XOpenDisplay(nullptr);
    return dpy;
}

void x11_set_dock_type(unsigned long winId) {
    Display* dpy = getDisplay();
    if (!dpy) return;

    Atom wmType  = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom wmDock  = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(dpy, winId, wmType, XA_ATOM, 32, PropModeReplace,
                    (unsigned char*)&wmDock, 1);

    Atom wmState       = XInternAtom(dpy, "_NET_WM_STATE",              False);
    Atom wmAbove       = XInternAtom(dpy, "_NET_WM_STATE_ABOVE",        False);
    Atom wmSkipTask    = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom wmSkipPager   = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER",   False);
    Atom wmSticky      = XInternAtom(dpy, "_NET_WM_STATE_STICKY",       False);
    Atom estados[] = { wmAbove, wmSkipTask, wmSkipPager, wmSticky };
    XChangeProperty(dpy, winId, wmState, XA_ATOM, 32, PropModeReplace,
                    (unsigned char*)estados, 4);
    XFlush(dpy);
}

void x11_set_strut(unsigned long winId,
                   int screenX, int screenY, int screenW, int screenH,
                   int position, int thickness) {
    Display* dpy = getDisplay();
    if (!dpy) return;

    long strut[12] = {0};
    switch (position) {
    case 0: // Abajo
        strut[3] = thickness; strut[10] = screenX; strut[11] = screenX+screenW-1; break;
    case 1: // Arriba
        strut[2] = thickness; strut[8]  = screenX; strut[9]  = screenX+screenW-1; break;
    case 2: // Izquierda
        strut[0] = thickness; strut[4]  = screenY; strut[5]  = screenY+screenH-1; break;
    case 3: // Derecha
        strut[1] = thickness; strut[6]  = screenY; strut[7]  = screenY+screenH-1; break;
    }
    Atom pStrut = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    Atom sStrut = XInternAtom(dpy, "_NET_WM_STRUT",         False);
    XChangeProperty(dpy, winId, pStrut, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)strut, 12);
    XChangeProperty(dpy, winId, sStrut, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)strut, 4);
    XFlush(dpy);
}

void x11_clear_strut(unsigned long winId) {
    Display* dpy = getDisplay();
    if (!dpy) return;
    long strut[12] = {0};
    Atom pStrut = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);
    XChangeProperty(dpy, winId, pStrut, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)strut, 12);
    XFlush(dpy);
}

void x11_activate_window(unsigned long winId) {
    Display* dpy = getDisplay();
    if (!dpy) return;
    Atom active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    XEvent ev = {};
    ev.xclient.type         = ClientMessage;
    ev.xclient.window       = winId;
    ev.xclient.message_type = active;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = 2;
    ev.xclient.data.l[1]    = CurrentTime;
    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    XFlush(dpy);
}

void x11_close_window(unsigned long winId) {
    Display* dpy = getDisplay();
    if (!dpy) return;
    Atom closeAtom = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
    XEvent ev = {};
    ev.xclient.type         = ClientMessage;
    ev.xclient.window       = winId;
    ev.xclient.message_type = closeAtom;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = CurrentTime;
    ev.xclient.data.l[1]    = DefaultRootWindow(dpy);
    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    XFlush(dpy);
}

int x11_get_client_list(X11WindowInfo* out, int maxOut) {
    Display* dpy = getDisplay();
    if (!dpy) return 0;

    Atom clientList = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    Atom actualType; int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), clientList,
                           0, 1024, False, XA_WINDOW,
                           &actualType, &actualFormat, &nItems, &bytesAfter,
                           &data) != Success || !data) return 0;

    Window* windows = (Window*)data;
    int count = 0;

    for (unsigned long i = 0; i < nItems && count < maxOut; i++) {
        Window w = windows[i];
        XClassHint hint;
        if (!XGetClassHint(dpy, w, &hint)) continue;

        out[count].wid = (unsigned long)w;
        strncpy(out[count].wmClass, hint.res_class ? hint.res_class : "", 255);
        strncpy(out[count].wmName,  hint.res_name  ? hint.res_name  : "", 255);
        out[count].wmClass[255] = 0;
        out[count].wmName[255]  = 0;
        out[count].valid = true;
        XFree(hint.res_class);
        XFree(hint.res_name);
        count++;
    }
    XFree(data);
    return count;
}

unsigned long x11_get_active_window() {
    Display* dpy = getDisplay();
    if (!dpy) return 0;

    Atom activeAtom = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    Atom aType; int aFmt;
    unsigned long aN, aB;
    unsigned char* aData = nullptr;
    unsigned long result = 0;

    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), activeAtom,
                           0, 1, False, XA_WINDOW,
                           &aType, &aFmt, &aN, &aB, &aData) == Success && aData) {
        result = *((Window*)aData);
        XFree(aData);
    }
    return result;
}

bool x11_get_window_title(unsigned long wid, char* out, int maxLen) {
    Display* dpy = getDisplay();
    if (!dpy || !out || maxLen <= 0) return false;

    Atom netName = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8    = XInternAtom(dpy, "UTF8_STRING",  False);
    Atom aType; int aFmt;
    unsigned long aN, aB;
    unsigned char* nd = nullptr;

    if (XGetWindowProperty(dpy, (Window)wid, netName, 0, 256, False, utf8,
                           &aType, &aFmt, &aN, &aB, &nd) == Success && nd) {
        strncpy(out, (char*)nd, maxLen-1);
        out[maxLen-1] = 0;
        XFree(nd);
        return true;
    }
    out[0] = 0;
    return false;
}
