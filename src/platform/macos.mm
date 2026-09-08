#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

namespace solace::platform
{
void make_window_transparent(void* native_window)
{
    if (!native_window)
        return;

    NSWindow* window = (__bridge NSWindow*)native_window;
    window.opaque = NO;
    window.backgroundColor = [NSColor clearColor];

    CALayer* layer = window.contentView.layer;
    if ([layer isKindOfClass:[CAMetalLayer class]])
        ((CAMetalLayer*)layer).opaque = NO;
}
}
