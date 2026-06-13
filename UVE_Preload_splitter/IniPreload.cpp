#include "IniPreload.h"
#include <fstream>


IniPreload::IniPreload() :
	format_version{ 0 },
	width{ 0 },
	height{ 0 },
	frames_amount{ 0 },
	file_format{ 0 }
{}

IniPreload::~IniPreload() {}

int IniPreload::Open(const std::string& f_path) {
	// todo: add a routine for opening text versions!
	std::ifstream file(f_path, std::ios::binary);
	if (!file) {
		return 0;
	}
	file.read(reinterpret_cast<char*>(&format_version), 4);
	if (format_version == 0x696B535B) { // "[Ski"
		file.close();
		return OpenIni(f_path);
	}
	file.read(reinterpret_cast<char*>(&width), 4);
	file.read(reinterpret_cast<char*>(&height), 4);
	file.read(reinterpret_cast<char*>(&file_format), 4);
	file.read(reinterpret_cast<char*>(&frames_amount), 4);

	for (int i = 0; i < frames_amount && !file.eof(); ++i) {
		PreloadFrameData frame{ 0 };
		file.read(reinterpret_cast<char*>(&frame.x), 4);
		file.read(reinterpret_cast<char*>(&frame.y), 4);
		file.read(reinterpret_cast<char*>(&frame.w), 4);
		file.read(reinterpret_cast<char*>(&frame.h), 4);

		int temp{ 0 };
		switch (format_version) {
		case VERSION_FLOAT:
			file.read(reinterpret_cast<char*>(&frame.xo), 4);
			file.read(reinterpret_cast<char*>(&frame.yo), 4);
			break;
		case VERSION_INT:
			file.read(reinterpret_cast<char*>(&temp), 4);
			frame.xo = static_cast<float>(temp);
			file.read(reinterpret_cast<char*>(&temp), 4);
			frame.yo = static_cast<float>(temp);
			break;
		default:
			return 0;
			break;
		}
		frames.push_back(frame);
	}
	file.close();
	return 1;
}

int IniPreload::OpenIni(const std::string& f_path) {
	std::ifstream file(f_path);
	if (!file) {
		return 0;
	}

	frames_amount = 0;
	file_format = VERSION_INI;
	int frame_last = -1;
	width = 0;
	height = 0;

	while (!file.eof()) {
		PreloadFrameData frame{ 0 };
		std::string str{};
		std::getline(file, str);
		
		// [Skin 0]
		// 0+6   ^
		// 7-0+6 = 1 (length)
		if (str.find("[Skin ") != std::string::npos && str.find(']') != std::string::npos) {
			str = str.substr(str.find("[Skin ") + 6, str.find(']') - str.find("[Skin ") + 6);
			int frame_current = stoi(str);
			if (frame_current == frame_last + 1) {
				frames_amount++;
				frame_last = frame_current;
			}
			else {
				std::printf("Frames in wrong order: %d came after %d.\n", frame_current, frame_last);
				return 0;
			}

			struct {
				bool left = false;
				bool top = false;
				bool width = false;
				bool height = false;
				bool origin_adjust_x = false;
				bool origin_adjust_y = false;
				bool Complete() { return left && top && width && height; }
				void PrintMissing() { 
					if (!left) std::printf("left is missing.\n");
					if (!top) std::printf("top is missing.\n");
					if (!width) std::printf("width is missing.\n");
					if (!height) std::printf("height is missing.\n");
				}
			} found_entries;


			while (!file.eof()) {
				auto cursor_pos = file.tellg();
				std::getline(file, str);
				if (str.find("left=") != std::string::npos) {
					// left=1
					str = str.substr(str.find('=', str.find("left=")) + 1);
					frame.x = static_cast<float>(stoi(str));
					found_entries.left = true;
				}
				else if (str.find("top=") != std::string::npos) {
					// left=1
					str = str.substr(str.find('=', str.find("top=")) + 1);
					frame.y = static_cast<float>(stoi(str));
					found_entries.top = true;
				}
				else if (str.find("width=") != std::string::npos) {
					// left=1
					str = str.substr(str.find('=', str.find("width=")) + 1);
					frame.w = static_cast<float>(stoi(str));
					found_entries.width = true;
				}
				else if (str.find("height=") != std::string::npos) {
					// left=1
					str = str.substr(str.find('=', str.find("height=")) + 1);
					frame.h = static_cast<float>(stoi(str));
					found_entries.height = true;
				}
				else if (str.find("origin_adjust_x=") != std::string::npos) {
					// left=1
					str = str.substr(str.find('=', str.find("origin_adjust_x=")) + 1);
					frame.xo = static_cast<float>(stoi(str));
					found_entries.origin_adjust_x = true;
				}
				else if (str.find("origin_adjust_y=") != std::string::npos) {
					// left=1
					str = str.substr(str.find('=', str.find("origin_adjust_y=")) + 1);
					frame.yo = static_cast<float>(stoi(str));
					found_entries.origin_adjust_y = true;
				}
				else if (str.find("[Skin ") != std::string::npos && str.find(']') != std::string::npos) {
					file.seekg(cursor_pos);
					break;
				}
				else { // empty line or a ; comment, or something else entirely which we are too afraid to read.
					break;
				}
			}
			if (!found_entries.Complete()) {
				std::printf("Incomplete entry №%d! Reading aborted.\n", frame_current);
				found_entries.PrintMissing();
				return 0;
			}
		}
		else { // unknown or empty line
			continue;
		}

		width = std::max(width, frame.x + frame.w);
		height = std::max(height, frame.y + frame.h);

		frames.push_back(frame);
	}
	file.clear();
	file.close();
	return 1;
}

int IniPreload::SetVersion(const int& version) {
	format_version = version;
	return 1;
}


int IniPreload::Save(const std::string& f_path) {
	switch (format_version) {
	case VERSION_INT:
	case VERSION_FLOAT:
		return (SaveIniPreload(f_path));
	case VERSION_INI:
		return (SaveIni(f_path));
	default:
		return 0;
	}
}

int IniPreload::SaveIniPreload(const std::string& f_path) {
	std::ofstream file(f_path, std::ios::trunc | std::ios::binary);
	if (!file) {
		return 0;
	}

	file.write(reinterpret_cast<char*>(&format_version), 4);
	file.write(reinterpret_cast<char*>(&width), 4);
	file.write(reinterpret_cast<char*>(&height), 4);
	file.write(reinterpret_cast<char*>(&file_format), 4);
	file.write(reinterpret_cast<char*>(&frames_amount), 4);
	for (int i = 0; i < frames_amount; ++i) {
		file.write(reinterpret_cast<char*>(&frames[i].x), 4);
		file.write(reinterpret_cast<char*>(&frames[i].y), 4);
		file.write(reinterpret_cast<char*>(&frames[i].w), 4);
		file.write(reinterpret_cast<char*>(&frames[i].h), 4);
		if (format_version == VERSION_FLOAT) {
			file.write(reinterpret_cast<char*>(&frames[i].xo), 4);
			file.write(reinterpret_cast<char*>(&frames[i].yo), 4);
		}
		else if (format_version == VERSION_INT) {
			int ixo = static_cast<int>(std::floor(frames[i].xo + 0.5f)), iyo = static_cast<int>(std::floor(frames[i].yo + 0.5f));
			file.write(reinterpret_cast<char*>(&ixo), 4);
			file.write(reinterpret_cast<char*>(&iyo), 4);
		}
	}
	file.close();
	return 1;
}

int IniPreload::SaveIni(const std::string& f_path) {
	std::ofstream file(f_path, std::ios::trunc);
	if (!file) { return 0; }
	for (int i = 0; i < frames_amount; ++i) {
		file << "[Skin " << i << "]\n";
		file << "left=" << frames[i].x << '\n';
		file << "top=" << frames[i].y << '\n';
		file << "width=" << frames[i].w << '\n';
		file << "height=" << frames[i].h << '\n';
		int ixo = static_cast<int>(std::floor(frames[i].xo + 0.5f));
		int iyo = static_cast<int>(std::floor(frames[i].yo + 0.5f));
		if (ixo) { file << "origin_adjust_x=" << ixo << '\n'; } // NOTE: Can there be only one of these or do they always come in pair?
		if (iyo) { file << "origin_adjust_y=" << iyo << "\n"; }
		if (i != frames_amount - 1) { file << "\n"; }
	}
	file.close();
	return 1;
}


void IniPreload::PrintFrames() {
	if (frames.size() == 0) {
		printf_s("No frames to print.\n");
		return;
	}
	for (size_t i = 0; i < frames.size(); ++i) {
		printf_s(
			"Frame %d [%d]\n"
			"\tx, y = %d, %d\n"
			"\tw, h = %dx%d\n"
			"\txo, yo = %f, %f\n",
			i + 1, i,
			frames[i].x,
			frames[i].y,
			frames[i].w,
			frames[i].h,
			frames[i].xo,
			frames[i].yo
		);
	}
}

PreloadFrameData IniPreload::GetEntry(int index) const {
	return frames[index];
}

void IniPreload::AddEntry(PreloadFrameData frame) {
	frames.push_back(frame);
}

bool IniPreload::IsFlipped() {
	return (format_version == VERSION_FLOAT);
}