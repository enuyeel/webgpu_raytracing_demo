#include "window.h"

namespace window
{
  struct HandleImpl {} dummy;
}

#include <emscripten.h> //emscripten_set_main_loop()

window::Handle window::create()
{
  return &dummy;
}

void window::show(Handle hwnd)
{

}

void window::destroyWindow(Handle hwnd)
{

}

void window::loop(Redraw func)
{
  emscripten_set_main_loop(func, 0, false);
}
