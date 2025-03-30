#include "common/updates.h"
#include <boost/process.hpp>
#include <cstddef>

using json = nlohmann::json;
namespace bp = boost::process;

namespace {
	bool is_version_newer(const std::string& current, const std::string& latest) {
		std::vector<int> current_parts;
		std::vector<int> latest_parts;
		std::regex re("\\d+");

		auto current_begin = std::sregex_iterator(current.begin(), current.end(), re);
		auto current_end = std::sregex_iterator();
		for (std::sregex_iterator i = current_begin; i != current_end; ++i) {
			current_parts.push_back(std::stoi((*i).str()));
		}

		auto latest_begin = std::sregex_iterator(latest.begin(), latest.end(), re);
		auto latest_end = std::sregex_iterator();
		for (std::sregex_iterator i = latest_begin; i != latest_end; ++i) {
			latest_parts.push_back(std::stoi((*i).str()));
		}

		for (size_t i = 0; i < std::min(current_parts.size(), latest_parts.size()); ++i) {
			if (current_parts[i] < latest_parts[i])
				return true;
			if (current_parts[i] > latest_parts[i])
				return false;
		}

		return latest_parts.size() > current_parts.size();
	}
}

updates::UpdateCheckRes updates::is_latest_version(bool include_beta) {
	std::string url = include_beta ? "https://api.github.com/repos/f0e/blur/releases"
	                               : "https://api.github.com/repos/f0e/blur/releases/latest";

	auto response = cpr::Get(cpr::Url{ url });

	if (response.status_code != 200) {
		u::log("Update check failed with status {}", response.status_code);
		return { .success = false };
	}

	try {
		std::string latest_tag;

		if (include_beta) {
			json releases = json::parse(response.text);

			if (releases.empty() || !releases.is_array()) {
				u::log("Update check failed: No releases found");
				return { .success = false };
			}

			// most recent release (first in the array)
			latest_tag = releases[0]["tag_name"];
		}
		else {
			json release = json::parse(response.text);

			if (release.contains("tag_name")) {
				latest_tag = release["tag_name"];
			}
			else {
				u::log("Update check failed: Release information not found");
				return { .success = false };
			}
		}

		// remove 'v' prefix if it exists
		std::string latest_version_number = latest_tag;
		if (!latest_tag.empty() && latest_tag[0] == 'v') {
			latest_version_number = latest_tag.substr(1);
		}

		bool is_latest = !is_version_newer(BLUR_VERSION, latest_version_number);

		return {
			.success = true,
			.is_latest = is_latest,
			.latest_tag = latest_tag,
			.latest_tag_url = "https://github.com/f0e/blur/releases/" + latest_tag,
		};
	}
	catch (const std::exception& e) {
		u::log("Failed to parse latest release JSON: {}", e.what());
		return {
			.success = false,
		};
	}
}

bool updates::update_to_tag(
	const std::string& tag,
	const std::optional<std::function<void(const std::string&, const std::string&)>>& message_callback
) {
	try {
		u::log("Beginning update to tag: {}", tag);

		// Create download URL and temp file path
		std::string download_url =
			"https://github.com/f0e/blur/releases/download/" + tag + "/blur-Windows-Installer-x64.exe";
		std::filesystem::path installer_path = std::filesystem::temp_directory_path() / "blur-installer.exe";

		// Open file for writing
		std::ofstream installer_file(installer_path.string(), std::ios::binary);
		if (!installer_file.is_open()) {
			u::log("Failed to create installer file at {}", installer_path.string());
			return false;
		}

		// Setup download with progress tracking
		size_t total_bytes = 0;
		size_t downloaded_bytes = 0;
		float last_reported_progress = 0.0f;

		// Get file size first
		auto head_response = cpr::Head(cpr::Url{ download_url });
		if (head_response.status_code == 200 && head_response.header.contains("Content-Length")) {
			total_bytes = std::stoul(head_response.header["Content-Length"]);
			u::log("Total download size: {} bytes", total_bytes);
		}
		else {
			u::log("Unable to determine download size");
			total_bytes = static_cast<size_t>(80 * 1024 * 1024); // Fallback size 80mb
		}

		// Setup download session
		cpr::Session session;
		session.SetUrl(cpr::Url{ download_url });
		if (message_callback) {
			session.SetWriteCallback(cpr::WriteCallback([&](const std::string_view& data, intptr_t userdata) -> bool {
				downloaded_bytes += data.size();
				installer_file.write(data.data(), data.size());

				float progress = static_cast<float>(downloaded_bytes) / static_cast<float>(total_bytes);
				if (progress - last_reported_progress >= 0.01f) {
					(*message_callback)(std::format("Downloading update: {:.1f}%", progress * 100.f), {});
					last_reported_progress = progress;
				}

				return true;
			}));
		}

		// Execute download
		auto response = session.Get();
		installer_file.close();

		if (response.status_code != 200) {
			u::log("Download failed with status code: {}", response.status_code);
			return false;
		}

		// Complete progress
		if (message_callback)
			(*message_callback)("Update download complete", {});

		u::log("Download complete, launching installer");

		// Launch installer and exit
		bp::spawn(installer_path.string());
		std::exit(0);
		return true;
	}
	catch (const std::exception& e) {
		u::log("Update failed with exception: {}", e.what());
		return false;
	}
}

bool updates::update_to_latest(
	bool include_beta,
	const std::optional<std::function<void(const std::string&, const std::string&)>>& message_callback
) {
	auto check_result = is_latest_version(include_beta);
	if (!check_result.success || check_result.is_latest) {
		return false;
	}

	return update_to_tag(check_result.latest_tag, message_callback);
}
