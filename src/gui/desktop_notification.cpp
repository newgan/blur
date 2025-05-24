#include "desktop_notification.h"

#ifdef _WIN32
#	include "common/blur.h"
#	include "sdl.h"
#	include <shellapi.h>
#	include <winrt/Windows.Foundation.h>
#	include <winrt/Windows.UI.Notifications.h>
#	include <winrt/Windows.Data.Xml.Dom.h>
#	include <winrt/Windows.ApplicationModel.h>
using namespace winrt;
using namespace Windows::UI::Notifications;
using namespace Windows::Data::Xml::Dom;

static std::string g_app_name;
static desktop_notification::ClickCallback g_click_callback;
static bool g_initialised = false;

bool desktop_notification::initialise(const std::string& app_name) {
	if (g_initialised)
		return true;

	// winrt::init_apartment(); // don't need it?

	g_app_name = app_name;
	g_initialised = true;
	return true;
}

bool desktop_notification::show(const std::string& title, const std::string& message, ClickCallback on_click) {
	// don't show notification if window is focused
	if (SDL_GetWindowFlags(sdl::window) & SDL_WINDOW_INPUT_FOCUS)
		return false;

	if (!g_initialised)
		initialise(APPLICATION_NAME);

	g_click_callback = on_click;

	try {
		auto toast_xml = ToastNotificationManager::GetTemplateContent(ToastTemplateType::ToastText02);

		auto text_elements = toast_xml.GetElementsByTagName(L"text");
		text_elements.Item(0).AppendChild(toast_xml.CreateTextNode(winrt::to_hstring(title)));
		text_elements.Item(1).AppendChild(toast_xml.CreateTextNode(winrt::to_hstring(message)));

		// // ToastImageAndText02
		// auto image_elements = toast_xml.GetElementsByTagName(L"image");
		// auto image_element = image_elements.Item(0).as<Windows::Data::Xml::Dom::XmlElement>();
		// image_element.SetAttribute(
		// 	L"src", winrt::hstring(L"file:///[something]/blur.ico")
		// );
		// image_element.SetAttribute(L"alt", winrt::hstring(L"Blur icon"));

		auto toast = ToastNotification(toast_xml);

		if (on_click) {
			toast.Activated([](ToastNotification const&, auto const&) {
				SDL_RaiseWindow(sdl::window); // probably won't work.

				if (g_click_callback)
					g_click_callback();
			});
		}

		ToastNotificationManager::CreateToastNotifier(winrt::to_hstring(g_app_name)).Show(toast);
		return true;
	}
	catch (...) {
		return false;
	}
}

bool desktop_notification::is_supported() {
	return true;
}

bool desktop_notification::has_permission() {
	return true;
}

void desktop_notification::cleanup() {
	if (!g_initialised)
		return;

	g_initialised = false;
}

#elif __linux__
#	include "common/blur.h"
#	include <libnotify/notify.h>

namespace {
	bool g_initialized = false;
	std::string g_app_name;
	std::unordered_map<NotifyNotification*, ClickCallback> g_click_callbacks;
	std::mutex g_callbacks_mutex;

	// Callback function for notification actions
	void on_notification_action(NotifyNotification* notification, char* action, gpointer user_data) {
		std::lock_guard<std::mutex> lock(g_callbacks_mutex);
		auto it = g_click_callbacks.find(notification);
		if (it != g_click_callbacks.end() && it->second) {
			it->second();
		}
	}

	// Callback for when notification is closed
	void on_notification_closed(NotifyNotification* notification, gpointer user_data) {
		std::lock_guard<std::mutex> lock(g_callbacks_mutex);
		g_click_callbacks.erase(notification);
	}
}

bool desktop_notification::initialise(const std::string& app_name) {
	if (g_initialized) {
		return true;
	}

	if (!notify_init(app_name.c_str())) {
		return false;
	}

	g_initialized = true;
	g_app_name = app_name;
	return true;
}

bool desktop_notification::show(const std::string& title, const std::string& message, ClickCallback on_click) {
	// Auto-initialize if not already done
	if (!g_initialized) {
		if (!initialise("Desktop Notification")) {
			return false;
		}
	}

	// Create the notification
	NotifyNotification* notification = notify_notification_new(
		title.c_str(),
		message.c_str(),
		nullptr // icon - use default
	);

	if (!notification) {
		return false;
	}

	// Set up click callback if provided
	if (on_click) {
		std::lock_guard<std::mutex> lock(g_callbacks_mutex);
		g_click_callbacks[notification] = on_click;

		// Add a default action (this is triggered when the notification body is clicked)
		notify_notification_add_action(
			notification, "default", "Click", (NotifyActionCallback)on_notification_action, nullptr, nullptr
		);

		// Set up close callback to clean up our callback storage
		g_signal_connect(notification, "closed", G_CALLBACK(on_notification_closed), nullptr);
	}

	// Set timeout (in milliseconds, -1 for default, 0 for no timeout)
	notify_notification_set_timeout(notification, NOTIFY_EXPIRES_DEFAULT);

	// Show the notification
	GError* error = nullptr;
	gboolean result = notify_notification_show(notification, &error);

	if (!result) {
		if (error) {
			g_error_free(error);
		}

		// Clean up on failure
		if (on_click) {
			std::lock_guard<std::mutex> lock(g_callbacks_mutex);
			g_click_callbacks.erase(notification);
		}
		g_object_unref(G_OBJECT(notification));
		return false;
	}

	// Don't unref immediately if we have callbacks, as they need the notification object
	if (!on_click) {
		g_object_unref(G_OBJECT(notification));
	}

	return true;
}

bool desktop_notification::is_supported() {
	// libnotify is available on most Linux desktop environments
	// We can check if we can initialize it
	if (g_initialized) {
		return true;
	}

	// Try a quick initialization test
	if (notify_init("test")) {
		if (!g_initialized) {
			notify_uninit();
		}
		return true;
	}
	return false;
}

bool desktop_notification::has_permission() {
	// On Linux, notification permissions are typically handled at the system level
	// If libnotify can initialize, we generally have permission
	return is_supported();
}

void desktop_notification::cleanup() {
	if (g_initialized) {
		// Clear all pending callbacks
		{
			std::lock_guard<std::mutex> lock(g_callbacks_mutex);
			g_click_callbacks.clear();
		}

		notify_uninit();
		g_initialized = false;
		g_app_name.clear();
	}
}

#endif
