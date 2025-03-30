#pragma once

struct GlobalAppSettings {
	std::string auto_updates = "off";
	bool offline = false;

	bool operator==(const GlobalAppSettings& other) const {
		return auto_updates == other.auto_updates && offline == other.offline;
	}

	[[nodiscard]] nlohmann::json to_json() const;
};

namespace config_app {
	const std::string APP_CONFIG_FILENAME = "blur.cfg";

	inline const std::vector<std::string> AUTO_UPDATE_OPTIONS = { "off", "on", "beta" };

	void create(const std::filesystem::path& filepath, const GlobalAppSettings& current_settings = GlobalAppSettings());
	GlobalAppSettings parse(const std::filesystem::path& config_filepath);
	std::filesystem::path get_app_config_path();
	GlobalAppSettings get_app_config();
}
