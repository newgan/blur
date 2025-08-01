#pragma once

namespace tasks {
	void run(const std::vector<std::string>& arguments);

	void add_files(const std::vector<std::wstring>& path_strs);
	void add_sample_video(const std::wstring& path_str);
	void process_pending_files();
}
