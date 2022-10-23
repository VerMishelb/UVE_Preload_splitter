#include <algorithm>
#include <numeric>
#include "AtlasPack.h"
#include "IniPreload.h"
#include "Debug.h"

Atlas::Atlas() : pixel_padding_(0), use_power_of_two_(1), colour_padding_(2) {}

void Atlas::SetPadding(int _padding) {
	pixel_padding_ = _padding;
}

void Atlas::SetPowerOfTwo(bool pot) {
	use_power_of_two_ = pot;
}

void Atlas::SetColourPadding(int _margin) {
	colour_padding_ = _margin;
}

int Atlas::SaveAtlas(const std::string& path, const std::vector<AtlasEntry>& images, int frames_amount, int loop_mode, int preload_version, bool force_greyscale) {
	if (images.empty()) { return -1; }
	Targa image{};
	IniPreload preload{};

	int real_frames_amount = images.size();
	if (frames_amount == -1) {
		frames_amount = real_frames_amount;
	}

	preload.format_version = preload_version;
	preload.width = size_.x;
	preload.height = size_.y;
	preload.frames_amount = frames_amount;
	preload.file_format = IniPreload::FILE_FORMAT_TGA;

	TargaHeader tga_header = images[0].image.GetHeader();
	tga_header.w = size_.x;
	tga_header.h = size_.y;
	if (force_greyscale) {
		tga_header.image_descriptor = 8;
		tga_header.colour_depth = 8;
		tga_header.image_type = 3;//Uncompressed greyscale
	}
	image.SetHeader(tga_header);

	DEBUG_PRINTVAL(size_.x, "%i");
	DEBUG_PRINTVAL(size_.y, "%i");

	for (size_t i = 0; i < real_frames_amount; ++i) {
		DEBUG_PRINTVAL(i, "%i [blitting frame]");
		DEBUG_PRINTVAL(images[i].rect.x, "%i");
		DEBUG_PRINTVAL(images[i].rect.y, "%i");
		DEBUG_PRINTVAL(images[i].rect.w, "%i");
		DEBUG_PRINTVAL(images[i].rect.h, "%i");

		//Make fun of colour bleeding
		if (image.GetHeader().colour_depth == 32) {
			for (int i2 = colour_padding_; i2 > 0; --i2) {
				//Up
				image.BlitRegionTransparent(
					images[i].image.GetRegion(images[i].data_start.x, images[i].data_start.y, images[i].rect.w, images[i].rect.h, false),
					images[i].rect.x, images[i].rect.y - i2, images[i].rect.w, images[i].rect.h,
					preload.IsFlipped(), 127
				);
				//Down
				image.BlitRegionTransparent(
					images[i].image.GetRegion(images[i].data_start.x, images[i].data_start.y, images[i].rect.w, images[i].rect.h, false),
					images[i].rect.x, images[i].rect.y + i2, images[i].rect.w, images[i].rect.h,
					preload.IsFlipped(), 127
				);
				//Left
				image.BlitRegionTransparent(
					images[i].image.GetRegion(images[i].data_start.x, images[i].data_start.y, images[i].rect.w, images[i].rect.h, false),
					images[i].rect.x - i2, images[i].rect.y, images[i].rect.w, images[i].rect.h,
					preload.IsFlipped(), 127
				);
				//Right
				image.BlitRegionTransparent(
					images[i].image.GetRegion(images[i].data_start.x, images[i].data_start.y, images[i].rect.w, images[i].rect.h, false),
					images[i].rect.x + i2, images[i].rect.y, images[i].rect.w, images[i].rect.h,
					preload.IsFlipped(), 127
				);
			}
		}

		//Draw an actual image
		image.BlitRegionTransparent(
			images[i].image.GetRegion(images[i].data_start.x, images[i].data_start.y, images[i].rect.w, images[i].rect.h, false),
			images[i].rect.x, images[i].rect.y, images[i].rect.w, images[i].rect.h,
			preload.IsFlipped()
		);

		PreloadFrameData frame{ 0 };
		frame.x = images[i].rect.x;
		frame.y = images[i].rect.y;
		frame.w = images[i].rect.w;
		frame.h = images[i].rect.h;
		frame.xo = images[i].offset.x;
		frame.yo = images[i].offset.y;
		preload.AddEntry(frame);
	}

	if (loop_mode == PACKFLAG_REPEAT_REVERSE && real_frames_amount < 2) {
		loop_mode = PACKFLAG_REPEAT_LAST_FRAME;
	}
	DEBUG_PRINTVAL(loop_mode, "%i");
	bool reverse = true;
	int index{ real_frames_amount-1 };//for PACKFLAG_REPEAT_REVERSE
	for (int i = real_frames_amount; i < frames_amount; ++i) {
		switch (loop_mode)
		{
		case PACKFLAG_REPEAT_LAST_FRAME:
			index = real_frames_amount - 1;
			break;
		case PACKFLAG_REPEAT_LOOP:
			index = i % real_frames_amount;
			break;
		case PACKFLAG_REPEAT_REVERSE:
			index += (reverse) ? -1 : 1;
			if (index == 0 || index+1 == real_frames_amount) {
				reverse = !reverse;
			}
			break;
		default:
			break;
		}

		preload.AddEntry(preload.GetEntry(index));
	}
	printf_s("Saving the atlas to %s\n", path.c_str());
	image.Save(path);
	preload.Save(path + ".ini.preload");
	return 0;
}

int Atlas::CreateAtlas(std::vector<AtlasEntry>& images) {
	images_area = 0;
	GetSizes(images, sizes_);
	sorted_ids_ = GetSortedIndices(images);

	while (!PackAtlas(images, size_, sorted_ids_)) {
		if (!use_power_of_two_) {
			++size_.x;
			//Don't put back into the heap if it will be larger than the maximum width of MAX_ATLAS_SIZE
			if (!(size_.x > MAX_ATLAS_SIZE)) {
				sizes_.push_back(size_);
				std::push_heap(sizes_.begin(), sizes_.end(), [](Vector2 a, Vector2 b) { return a.x * a.y > b.x * b.y; });
			}
		}

		if (sizes_.empty()) { printf_s("sizes_ is empty (should not be)\n"); return -1; }

		//Pop the next smallest area
		std::pop_heap(sizes_.begin(), sizes_.end(), [](Vector2 a, Vector2 b) { return a.x * a.y > b.x * b.y; });
		size_ = sizes_.back();
		sizes_.pop_back();
#ifdef DEBUG_ENABLE
		for (int i = 0; i < sizes_.size(); ++i) {
			printf_s("sizes[%i] = {%i, %i} (CreateAtlas)\n", 
				i,
				sizes_[i].x,
				sizes_[i].y);
		}
		printf_s("Popped size: {%i, %i}\n", size_.x, size_.y);
#endif
	}

	sizes_.clear();

	return images.size();
}

void Atlas::GetSizes(const std::vector<AtlasEntry>& images, std::vector<Vector2>& sizes) {
	int total_padding = 0;
	if (images[0].image.colour_depth == 32) {
		total_padding = pixel_padding_ + colour_padding_ * 2;
	}
	else {
		total_padding = pixel_padding_ + colour_padding_;
	}
	sizes.clear();
	for (int i = 0; i < images.size(); ++i) {
		images_area += 
			(images[i].rect.w + total_padding) *
			(images[i].rect.h + total_padding);
	}

	//1-512, 508*2, 508*3...
	unsigned int
		min_w = 0,
		min_h = 0,
		max_h = 0;
	int no_power_of_two_add_px = 8;

	for (int i = 0; i < images.size(); ++i) {
		if (images[i].rect.w + total_padding * 2 > min_w) {
			min_w = images[i].rect.w + total_padding * 2;
		}
		max_h += images[i].rect.h + total_padding * 2;
		if (images[i].rect.h + total_padding * 2 > min_h) {
			min_h = images[i].rect.h + total_padding * 2;
		}
	}

	//Round this up to avoid size calculations issue
	if (use_power_of_two_) {
		if (max_h <= 512){
			--max_h;
			max_h |= max_h >> 1;
			max_h |= max_h >> 2;
			max_h |= max_h >> 4;
			max_h |= max_h >> 8;
			max_h |= max_h >> 16;
			++max_h; // next power of 2
		}
		else {
			while (max_h % 508) { ++max_h; }
		}
	}
	else {
		while (max_h % no_power_of_two_add_px) { ++max_h; }
	}

	if (max_h > MAX_ATLAS_SIZE) { max_h = MAX_ATLAS_SIZE; }

	unsigned int no_power_of_two_start_width = images_area / max_h;
	
	DEBUG_PRINTVAL(use_power_of_two_, "%i");

	for (unsigned int h = ((use_power_of_two_) ? 1 : min_h); h <= max_h;) {
		unsigned int w = (use_power_of_two_) ? 1 : no_power_of_two_start_width;
		while (w * h < images_area || w < min_w) {
			use_power_of_two_ ?
				((w < 512) ? w <<= 1 :
					(w == 512) ? w = 1016 : w += 508) :
				(w += no_power_of_two_add_px);//*= 2 but fast
		}
		if (w <= MAX_ATLAS_SIZE && h >= min_h) {
			DEBUG_PRINTVAL(w, "%i [GetSizes():197]");
			DEBUG_PRINTVAL(h, "%i");
			DEBUG_PRINTVAL(min_h, "%i");
			DEBUG_PRINTVAL(min_w, "%i");
			DEBUG_PRINTVAL(w * h, "%i");
			DEBUG_PRINTVAL(images_area, "%i");
			Vector2 temp_vec{ static_cast<int>(w), static_cast<int>(h) };
			sizes.push_back(temp_vec);
		}
		use_power_of_two_ ?
			((h < 512) ? h <<= 1 :
				(h == 512) ? h = 1016 : h += 508) :
			(h += no_power_of_two_add_px);
	}
	if (sizes.empty()) { return; }

	//Get the smallest one
	std::make_heap(sizes.begin(), sizes.end(), [](Vector2 a, Vector2 b) {
		return a.x * a.y > b.x * b.y;
		});
	std::pop_heap(sizes.begin(), sizes.end(), [](Vector2 a, Vector2 b) {
		return a.x * a.y > b.x * b.y;
		});

	//std::sort(sizes.begin(), sizes.end(), [](Vector2 a, Vector2 b) {
	//	return a.x * a.y > b.x * b.y;
	//	});

	size_ = sizes.back();
	sizes.pop_back();
#ifdef DEBUG_ENABLE
	for (int i = 0; i < sizes.size(); ++i) {
		printf_s("sizes[%i] = {%i, %i} (%i)\n", i, sizes[i].x, sizes[i].y, sizes[i].x * sizes[i].y);
	}
	printf_s("Popped size: {%i, %i}\n", size_.x, size_.y);
#endif // DEBUG_ENABLE
}

std::vector<int> Atlas::GetSortedIndices(const std::vector<AtlasEntry>& images) {
	std::vector<int> sorted_ids(images.size());
	/*
	Assigns to every element in the range [first,last) successive
	values of 0, as if incremented with ++0 after each element is written.
	*/
	std::iota(sorted_ids.begin(), sorted_ids.end(), 0);
	std::sort(sorted_ids.begin(), sorted_ids.end(),
		[&images](int i, int j) { return images[i].rect.h > images[j].image.h; }
	);
	return sorted_ids;
}

bool Atlas::PackAtlas(std::vector<AtlasEntry>& images, Vector2 size,
	const std::vector<int>& sorted_ids)
{
	//Start with the whole atlas being available
	DEBUG_PRINTVAL(size.x, "%i (PackAtlas)");
	DEBUG_PRINTVAL(size.y, "%i");

	int total_padding = pixel_padding_ + colour_padding_;
	
	free_rects_.clear();
	free_rects_.push_back({
		total_padding,
		total_padding,
		size.x - total_padding * 2,
		size.y - total_padding * 2
		});

	for (int image = 0; image < images.size(); ++image) {

		if (free_rects_.empty()) {
			return false;
		}

		int current_index = sorted_ids[image];
		DEBUG_PRINTVAL(current_index, "%i");

		int best_short_side_fit = MAX_ATLAS_SIZE;
		int best_fit_index = 0;
		for (int i = 0; i < free_rects_.size(); ++i) {
			int leftover_width = free_rects_[i].w - images[current_index].rect.w;
			int leftover_height = free_rects_[i].h - images[current_index].rect.h;
			DEBUG_PRINTVAL(free_rects_[i].w, "%i");
			DEBUG_PRINTVAL(free_rects_[i].h, "%i");
			DEBUG_PRINTVAL(images[current_index].rect.w, "%i");
			DEBUG_PRINTVAL(images[current_index].rect.h, "%i");

			DEBUG_PRINTVAL(leftover_width, "%i");
			DEBUG_PRINTVAL(leftover_height, "%i");
			int shortest_side = std::min(leftover_width, leftover_height);

			//If the shortest side < 0, then the image did not fit into the free rect (AKA the shortest leftover heuristic)
			if (shortest_side >= 0 && shortest_side < best_short_side_fit) {
				best_short_side_fit = shortest_side;
				best_fit_index = i;
			}
		}

		//No rects found
		DEBUG_PRINTVAL(best_short_side_fit, "%i");
		if (best_short_side_fit == MAX_ATLAS_SIZE) {
			return false;
		}

		images[current_index].rect.x = free_rects_[best_fit_index].x;
		images[current_index].rect.y = free_rects_[best_fit_index].y;
//		DEBUG_PRINTVAL(best_fit_index, "%i");
//		DEBUG_PRINTVAL(free_rects_[best_fit_index].y, "%i");

		//Used to not waste time going over the new split rects that are added
		int num_rects_left = free_rects_.size();
		for (int i = 0; i < num_rects_left; ++i) {
			if (IntersectsRect(images[current_index].rect, free_rects_[i])) {
				//Split intersected free rects into at most 4 new smaller rects
				PushSplitRects(images[current_index].rect, free_rects_[i]);

				free_rects_.erase(free_rects_.begin() + i);
				--i;
				--num_rects_left;
			}
		}

		//Remove any free rects that are completely enclosed within another
		for (int i = 0; i < free_rects_.size(); ++i) {
			for (int j = i + 1; j < free_rects_.size(); ++j) {
				//if j is enclosed in k, remove j
				if (EnclosedInRect(free_rects_[i], free_rects_[j])) {
					free_rects_.erase(free_rects_.begin() + i);
					--i;
					break;
				}
				//vice versa
				else if (EnclosedInRect(free_rects_[j], free_rects_[i])) {
					free_rects_.erase(free_rects_.begin() + j);
					--j;
				}

			}
		}
	}

	return true;
}

bool Atlas::IntersectsRect(const Rect& new_rect, const Rect& free_rect) {
	//Separating axis theorem
	if (new_rect.x >= free_rect.x + free_rect.w || new_rect.x + new_rect.w <= free_rect.x ||
		new_rect.y >= free_rect.y + free_rect.h || new_rect.y + new_rect.h <= free_rect.y) {
		return false;
	}

	return true;
}

//Updates the free rects after pushing another image to an existing one.
void Atlas::PushSplitRects(const Rect& new_rect, const Rect free_rect) {
	int total_padding = pixel_padding_ + colour_padding_*2;
	//Top
	if (new_rect.y > free_rect.y) {
		Rect temp = free_rect;
		temp.h = new_rect.y - free_rect.y - total_padding;
		free_rects_.push_back(temp);
	}

	//Bottom
	if (free_rect.y + free_rect.h > new_rect.y + new_rect.h) {
		Rect temp = free_rect;
		temp.y = new_rect.y + new_rect.h + total_padding;
		temp.h = free_rect.y + free_rect.h - (new_rect.y + new_rect.h) - total_padding;
		free_rects_.push_back(temp);
	}

	//Left
	if (new_rect.x > free_rect.x) {
		Rect temp = free_rect;
		temp.w = new_rect.x - free_rect.x - total_padding;
		free_rects_.push_back(temp);
	}

	//Right
	if (free_rect.x + free_rect.w > new_rect.x + new_rect.w) {
		Rect temp = free_rect;
		temp.x = new_rect.x + new_rect.w + total_padding;
		temp.w = free_rect.x + free_rect.w - (new_rect.x + new_rect.w) - total_padding;
		free_rects_.push_back(temp);
	}
}

bool Atlas::EnclosedInRect(const Rect& a, const Rect& b) {
	return (a.y >= b.y && a.x >= b.x &&
		a.x + a.w <= b.x + b.w && a.y + a.h <= b.y + b.h);
}