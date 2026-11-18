// RoomCraft - Checkpoint 3 interactive prototype.
//
// Usage:
//   roomcraft_app                      open the editor
//   roomcraft_app --capture <folder>   run a scripted tour, save screenshots
//                                      and a GIF to <folder>, then exit
//   roomcraft_app --size <w> <h>       window size in pixels

#include "app.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    app::Options options;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--capture") == 0 && i + 1 < argc) {
            options.captureDirectory = argv[++i];
        } else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
            options.width = std::atoi(argv[++i]);
            options.height = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("usage: %s [--size <width> <height>] [--capture <folder>]\n", argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "unknown argument: %s (try --help)\n", argv[i]);
            return 2;
        }
    }
    app::App application(options);
    return application.run();
}
