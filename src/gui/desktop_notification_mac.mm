#include "desktop_notification.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

static desktop_notification::ClickCallback g_click_callback;
static bool g_has_permission = false;

@interface NotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation NotificationDelegate

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
	didReceiveNotificationResponse:(UNNotificationResponse*)response
			 withCompletionHandler:(void (^)(void))completionHandler {
	if (g_click_callback) {
		g_click_callback();
	}
	completionHandler();
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
	   willPresentNotification:(UNNotification*)notification
		 withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler {
	completionHandler(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionSound);
}

@end

static NotificationDelegate* g_delegate = nil;

namespace desktop_notification {

bool initialize(const std::string& app_name) {
	@autoreleasepool {
		g_delegate = [[NotificationDelegate alloc] init];

		UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
		center.delegate = g_delegate;

		[center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings* settings) {
		  if (settings.authorizationStatus == UNAuthorizationStatusAuthorized) {
			  g_has_permission = true;
		  }
		  else {
			  [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
									completionHandler:^(BOOL granted, NSError* error) { g_has_permission = granted; }];
		  }
		}];
	}

	return true;
}

bool show(const std::string& title, const std::string& message, ClickCallback on_click) {
	if (!g_has_permission) {
		return false;
	}

	g_click_callback = on_click;

	@autoreleasepool {
		UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
		content.title = [NSString stringWithUTF8String:title.c_str()];
		content.body = [NSString stringWithUTF8String:message.c_str()];
		content.sound = [UNNotificationSound defaultSound];

		UNNotificationRequest* request = [UNNotificationRequest requestWithIdentifier:[[NSUUID UUID] UUIDString]
																			  content:content
																			  trigger:nil];

		[[UNUserNotificationCenter currentNotificationCenter] addNotificationRequest:request withCompletionHandler:nil];
	}

	return true;
}

bool is_supported() {
	return true;
}

bool has_permission() {
	return g_has_permission;
}

void cleanup() {
	g_delegate = nil;
}

} // namespace desktop_notification
