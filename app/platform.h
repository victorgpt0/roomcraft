#pragma once

#include <string>

// The few things the app does differently in a web browser (Emscripten).
// On the desktop these do nothing.
//
// In the browser the app has a virtual file system instead of a disk:
//   /layouts     the example layouts, packed into the page at build time
//   /my_layouts  the user's saved layouts, kept in the browser's IndexedDB
//                so they are still there next visit (see web/shell.html)
// Screenshots and GIFs are written to the virtual file system and then
// handed to the browser as a download.

namespace app::platform {

// True when running in a web browser.
constexpr bool kWeb =
#ifdef __EMSCRIPTEN__
    true;
#else
    false;
#endif

// Browser: download the file at `path` (from the virtual file system) to the
// user's Downloads folder. Desktop: nothing (the file is already on disk).
void offerDownload(const std::string& path);

// Browser: the size of the browser window's page area in CSS pixels, which
// the app fills. Returns false on the desktop.
bool pageSize(int& width, int& height);

// Browser: copy /my_layouts into IndexedDB so saved layouts survive a page
// reload. Desktop: nothing.
void persistSavedLayouts();

} // namespace app::platform
