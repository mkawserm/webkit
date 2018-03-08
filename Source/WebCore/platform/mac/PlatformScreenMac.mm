/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PlatformScreen.h"

#if PLATFORM(MAC)

#import "FloatRect.h"
#import "FrameView.h"
#import "HostWindow.h"
#import "ScreenProperties.h"
#import <ColorSync/ColorSync.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>

extern "C" {
bool CGDisplayUsesInvertedPolarity(void);
bool CGDisplayUsesForceToGray(void);
}

namespace WebCore {

// These functions scale between screen and page coordinates because JavaScript/DOM operations
// assume that the screen and the page share the same coordinate system.

static PlatformDisplayID displayID(NSScreen *screen)
{
    return [[[screen deviceDescription] objectForKey:@"NSScreenNumber"] intValue];
}

static PlatformDisplayID displayID(Widget* widget)
{
    if (!widget)
        return 0;

    auto* view = widget->root();
    if (!view)
        return 0;

    auto* hostWindow = view->hostWindow();
    if (!hostWindow)
        return 0;

    return hostWindow->displayID();
}

// Screen containing the menubar.
static NSScreen *firstScreen()
{
    NSArray *screens = [NSScreen screens];
    if (![screens count])
        return nil;
    return [screens objectAtIndex:0];
}

static NSWindow *window(Widget* widget)
{
    if (!widget)
        return nil;
    return widget->platformWidget().window;
}

static NSScreen *screen(Widget* widget)
{
    // If the widget is in a window, use that, otherwise use the display ID from the host window.
    // First case is for when the NSWindow is in the same process, second case for when it's not.
    if (auto screenFromWindow = window(widget).screen)
        return screenFromWindow;
    return screen(displayID(widget));
}

bool screenIsMonochrome(Widget*)
{
    // This is a system-wide accessibility setting, same on all screens.
    return CGDisplayUsesForceToGray();
}

bool screenHasInvertedColors()
{
    // This is a system-wide accessibility setting, same on all screens.
    return CGDisplayUsesInvertedPolarity();
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
void getScreenProperties(HashMap<PlatformDisplayID, ScreenProperties>& screenProperties)
{
    for (NSScreen *screen in [NSScreen screens]) {
        FloatRect screenAvailableRect = [screen visibleFrame];
        screenAvailableRect.setY(NSMaxY([screen frame]) - (screenAvailableRect.y() + screenAvailableRect.height())); // flip
        FloatRect screenRect = [screen frame];
        int screenDepth = NSBitsPerPixelFromDepth(screen.depth);
        int screenDepthPerComponent = NSBitsPerSampleFromDepth(screen.depth);
        screenProperties.set(WebCore::displayID(screen), ScreenProperties { screenAvailableRect, screenRect, screenDepth, screenDepthPerComponent });
    }
}

static HashMap<PlatformDisplayID, ScreenProperties>& screenProperties()
{
    static NeverDestroyed<HashMap<PlatformDisplayID, ScreenProperties>> screenProperties;
    return screenProperties;
}

void setScreenProperties(const HashMap<PlatformDisplayID, ScreenProperties>& properties)
{
    screenProperties() = properties;
}
    
static ScreenProperties getScreenProperties(Widget* widget)
{
    auto displayIDForWidget = displayID(widget);
    if (displayIDForWidget && screenProperties().contains(displayIDForWidget))
        return screenProperties().get(displayIDForWidget);
    // Return property of the first screen if the screen is not found in the map.
    auto iter = screenProperties().begin();
    return screenProperties().get(iter->key);
}
#endif

int screenDepth(Widget* widget)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    if (!screenProperties().isEmpty()) {
        ASSERT(getScreenProperties(widget).screenDepth);
        return getScreenProperties(widget).screenDepth;
    }
#endif
    return NSBitsPerPixelFromDepth(screen(widget).depth);
}

int screenDepthPerComponent(Widget* widget)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    if (!screenProperties().isEmpty()) {
        ASSERT(getScreenProperties(widget).screenDepthPerComponent);
        return getScreenProperties(widget).screenDepthPerComponent;
    }
#endif
    return NSBitsPerSampleFromDepth(screen(widget).depth);
}

FloatRect screenRect(Widget* widget)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    if (!screenProperties().isEmpty())
        return getScreenProperties(widget).screenRect;
#endif
    return toUserSpace([screen(widget) frame], window(widget));
}

FloatRect screenAvailableRect(Widget* widget)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    if (!screenProperties().isEmpty()) {
        return getScreenProperties(widget).screenAvailableRect;
    }
#endif
    return toUserSpace([screen(widget) visibleFrame], window(widget));
}

NSScreen *screen(NSWindow *window)
{
    return [window screen] ?: firstScreen();
}

NSScreen *screen(PlatformDisplayID displayID)
{
    for (NSScreen *screen in [NSScreen screens]) {
        if (WebCore::displayID(screen) == displayID)
            return screen;
    }
    return firstScreen();
}

CGColorSpaceRef screenColorSpace(Widget* widget)
{
    return screen(widget).colorSpace.CGColorSpace;
}

bool screenSupportsExtendedColor(Widget* widget)
{
    if (!widget)
        return false;

    return [screen(widget) canRepresentDisplayGamut:NSDisplayGamutP3];
}

FloatRect toUserSpace(const NSRect& rect, NSWindow *destination)
{
    FloatRect userRect = rect;
    userRect.setY(NSMaxY([screen(destination) frame]) - (userRect.y() + userRect.height())); // flip
    return userRect;
}

NSRect toDeviceSpace(const FloatRect& rect, NSWindow *source)
{
    FloatRect deviceRect = rect;
    deviceRect.setY(NSMaxY([screen(source) frame]) - (deviceRect.y() + deviceRect.height())); // flip
    return deviceRect;
}

NSPoint flipScreenPoint(const NSPoint& screenPoint, NSScreen *screen)
{
    NSPoint flippedPoint = screenPoint;
    flippedPoint.y = NSMaxY([screen frame]) - flippedPoint.y;
    return flippedPoint;
}

} // namespace WebCore

#endif // PLATFORM(MAC)
