/*
https://github.com/Nickswoboda/atlas-packer was used as a reference, though highly modified.
*/

#ifndef AtlasPack_h_
#define AtlasPack_h_

#include <vector>
#include "Targa.h"
#include <string>

#define MAX_ATLAS_SIZE 8128// 8192

enum PackFlags {
	PACKFLAG_REPEAT_LOOP,
	PACKFLAG_REPEAT_REVERSE,
	PACKFLAG_REPEAT_LAST_FRAME
};

struct Vector2 {
	int
		x{0},
		y{0};
};

struct Vector2f {
	float
		x{ 0 },
		y{ 0 };
};

struct Rect {
	int
		x{0},
		y{0},
		w{0},
		h{0};
};
struct AtlasEntry {
	Targa image{};
	Rect rect{0,0,0,0};
	Vector2f offset{0,0};
	Vector2 data_start{ 0,0 };
};

class Atlas {
public:
	Atlas();
	int CreateAtlas(std::vector<AtlasEntry>& images);
	void GetSizes(const std::vector<AtlasEntry>& images, std::vector<Vector2>& sizes);
	std::vector<int> GetSortedIndices(const std::vector<AtlasEntry>& images);
	bool PackAtlas(std::vector<AtlasEntry>& images, Vector2 size,
		const std::vector<int>& sorted_ids);
	void SetPadding(unsigned int _padding);
	void SetPowerOfTwo(bool pot);
	int SaveAtlas(const std::string& path, const std::vector<AtlasEntry>& images, int frames_amount, int loop_mode, int preload_version);

	Vector2 size_{ 1,1 };
	std::vector<Vector2> sizes_{};
	std::vector<int> sorted_ids_{};
	unsigned int images_area{ 0 };
	int pixel_padding_;

private:
	bool IntersectsRect(const Rect& new_rect, const Rect& free_rect);
	void PushSplitRects(const Rect& new_rect, const Rect free_rect);
	bool EnclosedInRect(const Rect& a, const Rect& b);

	std::vector<Rect> free_rects_{};
	bool use_power_of_two_;
};

#endif // !AtlasPack_h_
