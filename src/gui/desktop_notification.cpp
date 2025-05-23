#include "desktop_notification.h"

#ifdef _WIN32
#	include <shellapi.h>
#	include <winrt/Windows.UI.Notifications.h>
#	include <winrt/Windows.Data.Xml.Dom.h>
#	include <winrt/Windows.ApplicationModel.h>
using namespace winrt;
using namespace Windows::UI::Notifications;
using namespace Windows::Data::Xml::Dom;

// Implementation
static std::string g_app_name;
static desktop_notification::ClickCallback g_click_callback;

bool desktop_notification::initialize(const std::string& app_name) {
	g_app_name = app_name;
	return true;
}

bool desktop_notification::show(const std::string& title, const std::string& message, ClickCallback on_click) {
	g_click_callback = on_click;

	try {
		auto toast_xml = ToastNotificationManager::GetTemplateContent(ToastTemplateType::ToastText02);

		auto text_elements = toast_xml.GetElementsByTagName(L"text");
		text_elements.Item(0).AppendChild(toast_xml.CreateTextNode(winrt::to_hstring(title)));
		text_elements.Item(1).AppendChild(toast_xml.CreateTextNode(winrt::to_hstring(message)));

		auto toast = ToastNotification(toast_xml);

		if (on_click) {
			toast.Activated([](ToastNotification const&, auto const&) {
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
	// Nothing to cleanup for Windows toast notifications
}

#elif __linux__
#	include <libnotify/notify.h>
#	include <gtk/gtk.h>

static std::string g_app_name;
static desktop_notification::ClickCallback g_click_callback;

static void on_notification_closed(NotifyNotification* notification, gpointer user_data) {
	g_object_unref(G_OBJECT(notification));
}

static void on_notification_activated(NotifyNotification* notification, char* action, gpointer user_data) {
	if (g_click_callback) {
		g_click_callback();
	}
}

bool desktop_notification::initialize(const std::string& app_name) {
	g_app_name = app_name;
	return notify_init(app_name.c_str());
}

bool desktop_notification::show(const std::string& title, const std::string& message, ClickCallback on_click) {
	if (!notify_is_initted())
		return false;

	g_click_callback = on_click;

	NotifyNotification* notification = notify_notification_new(title.c_str(), message.c_str(), nullptr);
	if (!notification)
		return false;

	notify_notification_set_timeout(notification, 5000); // 5 seconds

	if (on_click) {
		notify_notification_add_action(
			notification, "default", "Open", NOTIFY_ACTION_CALLBACK(on_notification_activated), nullptr, nullptr
		);
	}

	g_signal_connect(notification, "closed", G_CALLBACK(on_notification_closed), nullptr);

	GError* error = nullptr;
	bool success = notify_notification_show(notification, &error);

	if (error) {
		g_error_free(error);
		g_object_unref(G_OBJECT(notification));
		return false;
	}

	return success;
}

bool desktop_notification::is_supported() {
	return notify_is_initted();
}

bool desktop_notification::has_permission() {
	return notify_is_initted();
}

void desktop_notification::cleanup() {
	if (notify_is_initted()) {
		notify_uninit();
	}
}

#endif
