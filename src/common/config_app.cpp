#include "config_app.h"
#include "config_base.h"

void config_app::create(const std::filesystem::path& filepath, const GlobalAppSettings& current_settings) {
	std::ofstream output(filepath);

	output << "[blur v" << BLUR_VERSION << "]" << "\n";

	output << "\n";
	output << "- updates" << "\n";
	output << "check for updates: " << (current_settings.check_updates ? "true" : "false") << "\n";
	output << "include beta updates: " << (current_settings.check_beta ? "true" : "false") << "\n";
}

GlobalAppSettings config_app::parse(const std::filesystem::path& config_filepath) {
	auto config_map = config_base::read_config_map(config_filepath);

	GlobalAppSettings settings;

	config_base::extract_config_value(config_map, "check for updates", settings.check_updates);
	config_base::extract_config_value(config_map, "include beta updates", settings.check_beta);

	// recreate the config file using the parsed values (keeps nice formatting)
	create(config_filepath, settings);

	return settings;
}

std::filesystem::path config_app::get_app_config_path() {
	return blur.settings_path / APP_CONFIG_FILENAME;
}

GlobalAppSettings config_app::get_app_config() {
	return config_base::load_config<GlobalAppSettings>(get_app_config_path(), create, parse);
}

nlohmann::json GlobalAppSettings::to_json() const {
	nlohmann::json j;
	j["check_updates"] = this->check_updates;
	j["check_beta"] = this->check_beta;
	return j;
}
