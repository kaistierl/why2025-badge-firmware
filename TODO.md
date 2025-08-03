# Stuff that should be fixed

* `window_framebuffer_free` doesn't "move" the other framebuffer
* events are only delivered once per frame, and then only one input. 
* clean frames don't get re-blitted but due to lack of occlusion culling this causes graphical glitches
* We include the `select()` system call, but it only kind of works.
* esp-idf assumes it owns FreeRTOS TLS slot 0
* It seems that LWIP allocates in the task context, but then frees in the LWIP task context. This causes a heap corruption because the free is attempted with a different dlmalloc heap. Work around by not using spiram for this for now.
* There is a data race in thread/process creation and a process getting killed while the message is still in flight towards Zeus. The message never gets destroyed, and possibly the thread will leak.
* Writing to and reading from flash should be handled by a kernel task
* There are some sequencing problems in the wifi connect/disconnect code
