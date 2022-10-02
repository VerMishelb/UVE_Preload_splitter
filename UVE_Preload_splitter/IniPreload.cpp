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
	std::ifstream file(f_path, std::ios::binary);
	if (!file) {
		return 0;
	}
	file.read(reinterpret_cast<char*>(&format_version), 4);
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

int IniPreload::SetVersion(const int& version) {
	format_version = version;
	return 1;
}

int IniPreload::Save(const std::string& f_path) {
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
			int ixo = frames[i].xo, iyo = frames[i].yo;
			file.write(reinterpret_cast<char*>(&ixo), 4);
			file.write(reinterpret_cast<char*>(&iyo), 4);
		}
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