#pragma once

namespace config_base {
	std::map<std::string, std::string> read_config_map(const std::filesystem::path& config_filepath);

	template<typename T>
	void extract_config_value(const std::map<std::string, std::string>& config, const std::string& var, T& out) {
		if (!config.contains(var)) {
			DEBUG_LOG("config missing variable '{}'", var);
			return;
		}

		try {
			std::stringstream ss(config.at(var));
			ss.exceptions(std::ios::failbit); // enable exceptions
			ss >> std::boolalpha >> out;      // boolalpha: enable true/false bool parsing
		}
		catch (const std::exception&) {
			DEBUG_LOG("failed to parse config variable '{}' (value: {})", var, config.at(var));
			return;
		}
	}

	inline void extract_config_string(
		const std::map<std::string, std::string>& config, const std::string& var, std::string& out
	) {
		if (!config.contains(var)) {
			DEBUG_LOG("config missing variable '{}'", var);
			return;
		}

		out = config.at(var);
	}

	template<typename ConfigType>
	ConfigType load_config(
		const std::filesystem::path& config_path,
		void (*create_func)(const std::filesystem::path&, const ConfigType&),
		ConfigType (*parse_func)(const std::filesystem::path&)
	) {
		bool config_exists = std::filesystem::exists(config_path);

		if (!config_exists) {
			create_func(config_path, ConfigType());

			if (blur.verbose)
				u::log(L"Configuration file not found, default config generated at {}", config_path.wstring());
		}

		return parse_func(config_path);
	}
}
