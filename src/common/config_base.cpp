#include "config_base.h"

std::map<std::string, std::string> config_base::read_config_map(const std::filesystem::path& config_filepath) {
	std::map<std::string, std::string> config = {};

	// retrieve all of the variables in the config file
	std::ifstream input(config_filepath);
	std::string line;
	while (std::getline(input, line)) {
		// get key & value
		auto pos = line.find(':');
		if (pos == std::string::npos) // not a variable
			continue;

		std::string key = line.substr(0, pos);
		std::string value = line.substr(pos + 1);

		// trim whitespace
		key = u::trim(key);
		if (key == "")
			continue;

		value = u::trim(value);

		if (key != "custom ffmpeg filters") {
			// remove all spaces in values (it breaks stringstream string parsing, this is a dumb workaround) todo:
			// better solution
			std::erase(value, ' ');
		}

		config[key] = value;
	}

	return config;
}
