#ifndef _IniPreload_h_
#define _IniPreload_h_

#include <string>
#include <vector>

struct PreloadFrameData {
	int x{0},
		y{0},
		w{0},
		h{0};
	float
		xo{0},
		yo{0};
};

class IniPreload {
public:
	IniPreload();
	~IniPreload();
	int Open(const std::string& f_path);
	int Save(const std::string& f_path);
	int SetVersion(const int& version);
	void PrintFrames();
	PreloadFrameData GetEntry(int index) const;
	void AddEntry(PreloadFrameData frame);
	bool IsFlipped();

	static const unsigned int
		FILE_FORMAT_JPEG{ 0xCCCCCC00 },
		FILE_FORMAT_TGA{ 0xCCCCCC01 },
		VERSION_INT{1},
		VERSION_FLOAT{2}
	;

	std::vector<PreloadFrameData> frames;

	/* Format version 1 has int xo, yo. 2 has float xo, yo. */
	int format_version{0},
		width{0},
		height{0},
		frames_amount{0};
	unsigned int file_format{0};

};

#endif
