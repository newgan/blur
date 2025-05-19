#pragma once

struct GlobalAppSettings {
	bool check_updates = true;
	bool check_beta = false;

	bool operator==(const GlobalAppSettings& other) const = default;

	[[nodiscard]] nlohmann::json to_json() const;
};

namespace config_app {
	const std::string APP_CONFIG_FILENAME = "blur.cfg";

	// inline const std::vector<std::string> CHECK_UPDATES_OPTIONS = { "off", "on", "beta" };

	void create(const std::filesystem::path& filepath, const GlobalAppSettings& current_settings = GlobalAppSettings());
	GlobalAppSettings parse(const std::filesystem::path& config_filepath);
	std::filesystem::path get_app_config_path();
	GlobalAppSettings get_app_config();
}
