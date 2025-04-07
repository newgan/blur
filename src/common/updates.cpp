#include "common/updates.h"
#include <boost/process.hpp>
#include <cstddef>

using json = nlohmann::json;
namespace bp = boost::process;

const std::string WINDOWS_INSTALLER_NAME = "blur-Windows-Installer-x64.exe";
const std::string MACOS_INSTALLER_NAME = "blur-macOS-Release-arm64.dmg";

namespace {
	// NOLINTBEGIN ai code idc
	bool is_version_newer(const std::string& current, const std::string& latest) {
		// Parse version strings into components
		std::vector<std::string> current_components;
		std::vector<std::string> latest_components;

		// Split by dots and dashes
		std::regex component_splitter("[\\.-]");

		// Split current version into components
		std::string current_clean = current;
		if (!current_clean.empty() && current_clean[0] == 'v') {
			current_clean = current_clean.substr(1);
		}

		std::sregex_token_iterator current_iter(current_clean.begin(), current_clean.end(), component_splitter, -1);
		std::sregex_token_iterator end;
		for (; current_iter != end; ++current_iter) {
			current_components.push_back(*current_iter);
		}

		// Split latest version into components
		std::string latest_clean = latest;
		if (!latest_clean.empty() && latest_clean[0] == 'v') {
			latest_clean = latest_clean.substr(1);
		}

		std::sregex_token_iterator latest_iter(latest_clean.begin(), latest_clean.end(), component_splitter, -1);
		for (; latest_iter != end; ++latest_iter) {
			latest_components.push_back(*latest_iter);
		}

		// Compare numeric parts first (major.minor.patch)
		size_t i = 0;
		while (i < std::min(current_components.size(), latest_components.size())) {
			// If both components are numeric, compare as integers
			if (std::regex_match(current_components[i], std::regex("^\\d+$")) &&
			    std::regex_match(latest_components[i], std::regex("^\\d+$")))
			{
				int current_val = std::stoi(current_components[i]);
				int latest_val = std::stoi(latest_components[i]);

				if (current_val < latest_val)
					return true;
				if (current_val > latest_val)
					return false;
			}
			// Handle preview/beta/alpha components
			else if (i > 0) { // Only consider text components after version numbers
				// If current is a release but latest is a preview, current is newer
				bool current_is_preview = current_components[i].find("preview") != std::string::npos ||
				                          current_components[i].find("beta") != std::string::npos ||
				                          current_components[i].find("alpha") != std::string::npos;

				bool latest_is_preview = latest_components[i].find("preview") != std::string::npos ||
				                         latest_components[i].find("beta") != std::string::npos ||
				                         latest_components[i].find("alpha") != std::string::npos;

				// If one is preview and the other isn't
				if (current_is_preview != latest_is_preview) {
					return current_is_preview; // Current is older if it's a preview
				}

				// If both are preview versions
				if (current_is_preview && latest_is_preview) {
					// Extract preview numbers for comparison
					std::regex preview_num("\\d+");
					std::smatch current_match, latest_match;

					if (std::regex_search(current_components[i], current_match, preview_num) &&
					    std::regex_search(latest_components[i], latest_match, preview_num))
					{
						int current_preview_num = std::stoi(current_match[0]);
						int latest_preview_num = std::stoi(latest_match[0]);

						if (current_preview_num < latest_preview_num)
							return true;
						if (current_preview_num > latest_preview_num)
							return false;
					}
					// If no numbers found, compare lexicographically
					else {
						if (current_components[i] < latest_components[i])
							return true;
						if (current_components[i] > latest_components[i])
							return false;
					}
				}
				// If neither are previews, compare lexicographically
				else {
					if (current_components[i] < latest_components[i])
						return true;
					if (current_components[i] > latest_components[i])
						return false;
				}
			}

			i++;
		}

		// If we've compared all components of one version but the other has more,
		// the one with more components is newer, unless it's a preview
		if (current_components.size() != latest_components.size()) {
			if (current_components.size() < latest_components.size()) {
				// Check if the extra component in latest is a preview indicator
				if (i < latest_components.size() && (latest_components[i].find("preview") != std::string::npos ||
				                                     latest_components[i].find("beta") != std::string::npos ||
				                                     latest_components[i].find("alpha") != std::string::npos))
				{
					return false; // Current is a stable release, so it's newer
				}
				return true; // Latest has more components and isn't preview, so it's newer
			}
			// Check if the extra component in current is a preview indicator
			if (i < current_components.size() && (current_components[i].find("preview") != std::string::npos ||
			                                      current_components[i].find("beta") != std::string::npos ||
			                                      current_components[i].find("alpha") != std::string::npos))
			{
				return true; // Current is a preview, so it's older
			}
			return false; // Current has more components and isn't preview, so it's newer
		}

		// Versions are equal
		return false;
	}

	// NOLINTEND
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

			// get most recent release (needs to have an installer, might make a release without one temporarily - don't
			// want anyone updating until i have)
			for (const auto& release : releases) {
				if (!latest_tag.empty())
					break;

				for (const auto& asset : release["assets"]) {
#if defined(_WIN32)
					if (asset["name"] == WINDOWS_INSTALLER_NAME) {
#elif defined(__linux__)
					// todo when there's an installer
					{
#elif defined(__APPLE__)
					if (asset["name"] == MACOS_INSTALLER_NAME) {
#endif
						latest_tag = release["tag_name"];
						break;
					}
				}
			}
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
	const std::string& tag, const std::optional<std::function<void(const std::string&)>>& progress_callback
) {
	try {
		u::log("Beginning update to tag: {}", tag);

		std::string installer_filename;
		std::filesystem::path installer_path;

#ifdef _WIN32
		installer_filename = WINDOWS_INSTALLER_NAME;
		installer_path = std::filesystem::temp_directory_path() / installer_filename;
#elif defined(__APPLE__)
		installer_filename = MACOS_INSTALLER_NAME;
		installer_path = std::filesystem::temp_directory_path() / installer_filename;
#else
		u::log("Unsupported platform for automatic updates");
		return false;
#endif

		// Create download URL
		std::string download_url = "https://github.com/f0e/blur/releases/download/" + tag + "/" + installer_filename;

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
		if (progress_callback) {
			session.SetWriteCallback(cpr::WriteCallback([&](const std::string_view& data, intptr_t userdata) -> bool {
				downloaded_bytes += data.size();
				installer_file.write(data.data(), data.size());

				float progress = static_cast<float>(downloaded_bytes) / static_cast<float>(total_bytes);
				if (progress - last_reported_progress >= 0.01f) {
					(*progress_callback)(std::format("Downloading update: {:.1f}%", progress * 100.f));
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
		if (progress_callback)
			(*progress_callback)("Update download complete");

		u::log("Download complete, launching installer");

#ifdef _WIN32
		bp::spawn(installer_path.string());
#elif defined(__APPLE__)
		bp::spawn("/usr/bin/open", installer_path.string());
#endif

		return true;
	}
	catch (const std::exception& e) {
		u::log("Update failed with exception: {}", e.what());
		return false;
	}
}

bool updates::update_to_latest(
	bool include_beta, const std::optional<std::function<void(const std::string&)>>& progress_callback
) {
	auto check_result = is_latest_version(include_beta);
	if (!check_result.success || check_result.is_latest) {
		return false;
	}

	return update_to_tag(check_result.latest_tag, progress_callback);
}
