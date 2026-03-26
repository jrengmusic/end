/**
 * @file Notifications.mm
 * @brief macOS desktop notification via UNUserNotificationCenter.
 */

#include "Notifications.h"

#if JUCE_MAC

#import <UserNotifications/UserNotifications.h>

// ---------------------------------------------------------------------------
// Delegate that allows notifications to appear while END is frontmost.
// Without this, macOS silently suppresses notifications from the active app.
// ---------------------------------------------------------------------------

@interface EndNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation EndNotificationDelegate

- (void) userNotificationCenter:(UNUserNotificationCenter*) center
        willPresentNotification:(UNNotification*) notification
          withCompletionHandler:(void (^)(UNNotificationPresentationOptions)) completionHandler
{
    juce::ignoreUnused (center, notification);
    completionHandler (UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionSound);
}

@end

// ---------------------------------------------------------------------------

namespace Terminal
{
namespace Notifications
{

static void ensureDelegate()
{
    static EndNotificationDelegate* delegate { nil };

    if (delegate == nil)
    {
        delegate = [[EndNotificationDelegate alloc] init];
        UNUserNotificationCenter* center { [UNUserNotificationCenter currentNotificationCenter] };
        center.delegate = delegate;

        [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
                              completionHandler:^(BOOL, NSError*) {}];
    }
}

void show (const juce::String& title, const juce::String& body)
{
    ensureDelegate();

    UNMutableNotificationContent* content { [[UNMutableNotificationContent alloc] init] };
    content.body = [NSString stringWithUTF8String: body.toUTF8()];

    if (title.isNotEmpty())
    {
        content.title = [NSString stringWithUTF8String: title.toUTF8()];
    }

    content.sound = [UNNotificationSound defaultSound];

    NSString* identifier { [NSString stringWithFormat:@"end-%f", [[NSDate date] timeIntervalSince1970]] };

    UNNotificationRequest* request { [UNNotificationRequest requestWithIdentifier:identifier
                                                                          content:content
                                                                          trigger:nil] };

    [[UNUserNotificationCenter currentNotificationCenter] addNotificationRequest:request
                                                           withCompletionHandler:nil];
}

} // namespace Notifications
} // namespace Terminal

#endif
