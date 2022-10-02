#ifndef _Targa_h_
#define _Targa_h_

#include <vector>
#include <string>

//For TGA
struct TargaHeader {
	unsigned short
		x{0},
		y{0},
		w{0},
		h{0};
	unsigned char
		image_type{0},
		colour_depth{0},
		image_descriptor{0};
};

struct PixelData {
	unsigned char a{0}, r{ 0 }, g{ 0 }, b{ 0 };
	bool operator!=(const PixelData& other) const;
	bool operator==(const PixelData& other) const;
};

class Targa {
public:
	Targa();
	~Targa();
	int Open(const std::string& path);
	int Save(const std::string& path);
	void SetHeader(const TargaHeader& header);
	TargaHeader GetHeader() const;
	void SetPixel(int x, int y, const PixelData& px, bool bottom_to_top = true);
	PixelData GetPixel(int x, int y, bool bottom_to_top = true) const;
	//The resulting region is always upside down for consistence
	std::vector<PixelData> GetRegion(int x, int y, int w, int h, bool bottom_to_top = true) const;
	void BlitRegion(const std::vector<PixelData>& _data, int x, int y, int w, int h, bool bottom_to_top = true);
	bool PixelIsTransparent(const PixelData& px, bool check_alpha_only = true);

	std::vector<unsigned char> data{};

	unsigned short
		x{0},
		y{0},
		w{0},
		h{0};
	unsigned char
		image_type{0},
		colour_depth{0},
		image_descriptor{0};
};

#endif