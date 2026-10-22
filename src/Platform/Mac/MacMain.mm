#import <Cocoa/Cocoa.h>

#include "Platform/Mac/MacHost.h"

#include <cstdio>

int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;

    @autoreleasepool {
        CloverPic::Platform::Mac::MacHost host;
        if (!host.Initialize()) {
            std::fprintf(stderr, "[mac] failed to create the CloverPic windows\n");
            return 1;
        }
        return host.Run();
    }
}
