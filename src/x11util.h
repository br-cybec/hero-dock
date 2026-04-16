#pragma once
#include <cstdint>
// X11 utility functions - pure C interface, no Qt, no X11 macros in this header

void x11_set_dock_type(unsigned long winId);
void x11_set_strut(unsigned long winId, int screenX, int screenY, int screenW, int screenH,
                   int position, int thickness);
void x11_clear_strut(unsigned long winId);
void x11_activate_window(unsigned long winId);
void x11_close_window(unsigned long winId);

struct X11WindowInfo {
    unsigned long wid;
    char wmClass[256];
    char wmName[256];
    bool valid;
};

// Returns number of windows filled into out[] (max maxOut)
int  x11_get_client_list(X11WindowInfo* out, int maxOut);
unsigned long x11_get_active_window();
bool x11_get_window_title(unsigned long wid, char* out, int maxLen);
