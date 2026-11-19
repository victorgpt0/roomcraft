#include "platform.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// clang-format off
EM_JS(void, roomcraft_download, (const char* path), {
    const name = UTF8ToString(path);
    const bytes = FS.readFile(name);
    const type = name.endsWith('.gif') ? 'image/gif'
               : name.endsWith('.bmp') ? 'image/bmp'
               : 'application/octet-stream';
    const url = URL.createObjectURL(new Blob([bytes], {type}));
    const link = document.createElement('a');
    link.href = url;
    link.download = name.split('/').pop();
    document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
});

EM_JS(int, roomcraft_page_width, (), { return window.innerWidth; });
EM_JS(int, roomcraft_page_height, (), { return window.innerHeight; });

EM_JS(void, roomcraft_persist, (), {
    FS.syncfs(false, (error) => {
        if (error) console.warn('RoomCraft: could not store saved layouts', error);
    });
});
// clang-format on
#endif

namespace app::platform {

void offerDownload(const std::string& path) {
#ifdef __EMSCRIPTEN__
    roomcraft_download(path.c_str());
#else
    (void)path;
#endif
}

bool pageSize(int& width, int& height) {
#ifdef __EMSCRIPTEN__
    width = roomcraft_page_width();
    height = roomcraft_page_height();
    return width > 0 && height > 0;
#else
    (void)width;
    (void)height;
    return false;
#endif
}

void persistSavedLayouts() {
#ifdef __EMSCRIPTEN__
    roomcraft_persist();
#endif
}

} // namespace app::platform
