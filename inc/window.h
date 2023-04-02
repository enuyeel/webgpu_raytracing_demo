namespace window {

  //opaque Window handle
  typedef struct HandleImpl* Handle;

  typedef void (*Redraw)();

  Handle create();

  void show(Handle hwnd);

  void destroyWindow(Handle hwnd);

  void loop(Redraw func);

}