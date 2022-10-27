#include "Targa.h"

#include <fstream>
/*
The bitsperpixel specifies the size of each colour value.
When 24 or 32 the normal conventions apply.
For 16 bits each colour component is stored as 5 bits
and the remaining bit is a binary alpha value.
The colour components are converted into single byte components
by simply shifting each component up by 3 bits (multiply by 8).
*/
Targa::Targa() :
	x{ 0 }, y{ 0 }, w{ 0 }, h{ 0 },
	image_type{ 0 }, colour_depth{ 0 }, image_descriptor{ 0 }
{}

Targa::~Targa() {}

int Targa::Open(const std::string& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return 0;
	}
	file.seekg(2);
	image_type = file.get();
	file.seekg(8);
	file.read(reinterpret_cast<char*>(&x), 2);
	file.read(reinterpret_cast<char*>(&y), 2);
	file.read(reinterpret_cast<char*>(&w), 2);
	file.read(reinterpret_cast<char*>(&h), 2);
	colour_depth = file.get();
	image_descriptor = file.get();
	data.resize(w*h*(colour_depth/8));/*!!!*/
	file.read(reinterpret_cast<char*>(data.data()), data.size());
	file.close();
	return 1;
}

int Targa::Save(const std::string& path) {
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		return 0;
	}

	/* Header */
	file.put(0); file.put(0);
	file.put(image_type);
	file.put(0); file.put(0); file.put(0); file.put(0); file.put(0);
	file.write(reinterpret_cast<char*>(&x), 2);
	file.write(reinterpret_cast<char*>(&y), 2);
	file.write(reinterpret_cast<char*>(&w), 2);
	file.write(reinterpret_cast<char*>(&h), 2);
	file.put(colour_depth);
	file.put(image_descriptor);
	file.write(reinterpret_cast<char*>(data.data()), data.size());
	file.close();
	return 1;
}

void Targa::SetHeader(const TargaHeader& header) {
	x = header.x;
	y = header.y;
	w = header.w;
	h = header.h;
	image_type = header.image_type;
	colour_depth = header.colour_depth;
	image_descriptor = header.image_descriptor;
	
	data.resize(w * h * (colour_depth >> 3));
}

void Targa::SetPixel(int x, int y, const PixelData& px, bool bottom_to_top) {
	//int px_d = x + x*y;
	int px_d = (bottom_to_top) ? x + w * y : x + w * (h-y-1);
	if (px_d * colour_depth >> 3 > data.size()) {
		printf_s("Error. Tried to put a pixel to\n%dx%d\nwhen the image is\n%dx%d.\n", x, y, w, h);
	}
	switch (colour_depth) {
		case 32:	
			data[px_d * 4] 	= px.b;
			data[px_d * 4 + 1] = px.g;
			data[px_d * 4 + 2] = px.r;
			data[px_d * 4 + 3] = px.a;
			break;
		case 24:
			data[px_d * 3] 	= px.b;
			data[px_d * 3 + 1] = px.g;
			data[px_d * 3 + 2] = px.r;
			break;
		case 8:
			data[px_d] = px.r;
			break;
		default:
			break;
	};
}

PixelData Targa::GetPixel(int x, int y, bool bottom_to_top) const {
	//int px_d = x + x*y;// Created a cool looking noise bug
	int px_d = (bottom_to_top) ? x + w * y : x + w * (h-y-1);
	PixelData px{0};
	switch (colour_depth) {
		case 32:
			px.b = data[px_d * 4];
			px.g = data[px_d * 4 + 1];
			px.r = data[px_d * 4 + 2];
			px.a = data[px_d * 4 + 3];
			break;
		case 24:
			px.b = data[px_d * 3];
			px.g = data[px_d * 3 + 1];
			px.r = data[px_d * 3 + 2];
			break;
		case 8:
			px.r = data[px_d];
			break;
		default:
			break;
	};
	return px;
}

std::vector<PixelData> Targa::GetRegion(int x, int y, int w, int h, bool bottom_to_top) const {
	size_t res_size = std::abs(w*h);
	std::vector<PixelData> res{res_size};
	int
		x_from = x,
		x_to = x_from + w,//+1
		y_from = bottom_to_top ? y : this->h - y - 1,
		y_to = bottom_to_top ? y_from + h: y_from - h;//+-1

	for (int _y = y_from; _y != y_to; (bottom_to_top ? ++_y : --_y)) {
		for (int _x = x_from; _x < x_to; ++_x) {
			res[(_x - x_from) + std::abs(_y - y_from) * w] = GetPixel(_x, _y, true);
		}
	}
	return res;
}

void Targa::BlitRegion(const std::vector<PixelData>& _data, int x, int y, int w, int h, bool bottom_to_top) {
	int
		x_from = x,
		x_to = x_from + w,//+1
		y_from = bottom_to_top ? y : this->h - y - 1,
		y_to = bottom_to_top ? y_from + h : y_from - h;//+-1

	for (int _y = y_from; _y != y_to; (bottom_to_top ? ++_y : --_y)) {
		for (int _x = x_from; _x < x_to; ++_x) {
			SetPixel(_x, _y, _data[(_x - x_from) + std::abs(_y - y_from) * w], true);
		}
	}
}

void Targa::BlitRegionTransparent(const std::vector<PixelData>& _data, int x, int y, int w, int h, bool bottom_to_top, uint8_t a_) {
	int
		x_from = x,
		x_to = x_from + w,//+1
		y_from = bottom_to_top ? y : this->h - y - 1,
		y_to = bottom_to_top ? y_from + h : y_from - h;//+-1

	for (int _y = y_from; _y != y_to; (bottom_to_top ? ++_y : --_y)) {
		for (int _x = x_from; _x < x_to; ++_x) {
			if (!PixelIsTransparent(_data[(_x - x_from) + std::abs(_y - y_from) * w]))
			SetPixel(_x, _y, 
				{ 
					(a_ == 255) ? _data[(_x - x_from) + std::abs(_y - y_from) * w].a : a_,
					_data[(_x - x_from) + std::abs(_y - y_from) * w].r,
					_data[(_x - x_from) + std::abs(_y - y_from) * w].g,
					_data[(_x - x_from) + std::abs(_y - y_from) * w].b,
				}, true);
		}
	}
}

bool Targa::PixelIsTransparent(const PixelData& px, bool alpha_only) {
	switch (colour_depth) {
	case 32:
		if (alpha_only) return (!px.a);
		else return (px == PixelData{ 0,0,0,0 });
		break;
	case 24:
		return false;
		break;
	case 8:
		return (!px.r);
		break;
	default:
		return false;
		break;
	}
}

TargaHeader Targa::GetHeader() const {
	TargaHeader header;
	header.x = x;
	header.y = y;
	header.w = w;
	header.h = h;
	header.image_type = image_type;
	header.colour_depth = colour_depth;
	header.image_descriptor = image_descriptor;
	return header;
}

bool PixelData::operator!=(const PixelData& other) const {
	return (!(*this == other));
}

bool PixelData::operator==(const PixelData& other) const {
	return ( r == other.r && g == other.g && b == other.b && a == other.a );
}