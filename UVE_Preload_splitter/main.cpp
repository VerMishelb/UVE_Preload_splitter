#include <iostream>
#include <vector>
#include "IniPreload.h"
#include "Targa.h"
#include "AtlasPack.h"
#include "Debug.h"

/* Don't put 0 in the beginning. */
#define VERSION 2'00'01'04
#define VERSION_STR "2.0.1.4"
#define ERRMSG_NOT_ENOUGH_ARGS(x) "Incorrect " x " usage: not enough arguments.\nRun without parameters to see usage examples.\n"
#define ERRMSG_FILE(x) "An error occurred when trying to read/write " << x << ".\n"
#define HALF(x) ((float)(x)/2.f+1.f)

#define CHKSET(x, y) {if ( (x) != (y) ) x = y;}

#define SCREWUP printf_s("Something went wrong at  %s:%d", __func__, __LINE__)

/*
? Protection from repetitive entries (not sure if it's necessary, slows arguments parsing significantly in theory but shouldn't harm)
*/

/*
CHANGE LOG (primary, major, minor, patch)
TODO
- Add proper errors output with (at least) line and function
  where the error occurred.
- Proper exe name arguments system.
- Rework pack entries to support them independantly

2.0.1.4
- Fixed alpha channel check for tranparent blitting.

2.0.1.3
- Added a colour bleeding padding option. See "--colour-padding".
- Added --greyscale option to convert 32 bit frames to 8 bit atlas.

2.0.1.2
- Fixed an issue where free rect reference became outdated after pushing back at least one new rect.
- Changed one particular C-style cast to C++ style cast.
- Removed useless code comments.

2.0.1.1
- Added middle point display at export output because I'm tired of debugging this through visual studio.
- --dbg-middle and --dbg-frame flags. See usage.
- Fixed R and B channels being swapped (didn't affect anything due to both read and write functions being broken, fixing each other).
- Fixed size calculation for export mode that would ruin your work when exporting some edge case stuff.
- Float offsets support to fix CIU being a [[Thirty percent off!]]
- "Preload" -> "Preload type" typo fix.

2.0.1.0
- Fixed errors not triggering the error message in ParseArgs.
- Fixed the last file being ignored under specific circumstances.
- Help is now displayed when you double click the app.
- Fixed every even file being ignored with -ps -c flag combination.
- --centered export flag for keeping the true center at the center of resulting frames.
- Enhanced export frame size calculations (hopefully without breaking anything this time).
- Sprite sheet files are renamed to "_sheet" and include single frame width & height in the name.
- Fixed horizontal sprite sheets being generated backwards.
- Fixed a ton of uninitialised variables that did not cause issues but potentially could.
- Added pack mode for 8, 24 and 32 bit TGAs. Read help screen for more info.

2.0.0.2
- Y offset is now calculated correctly

2.0.0.1
- Preload version now affects the way frames are defined. CIU (float) preloads go from bottom to top, CI (int) go from top to bottom.
- Images no longer flipped using the descriptor bit to make the result more predictable.
- -f is now togglable (can negate itself if met twice in the same command) and turned on by default.

2.0.0.0
- Initial release
*/

enum EntryFlags {
	ENTRYFLAG_EXPORT,
	ENTRYFLAG_CONVERT_FLOAT,
	ENTRYFLAG_CONVERT_INT,
	ENTRYFLAG_PACK_INT,
	ENTRYFLAG_PACK_FLOAT
};

enum ExportFlags {
	EXPORTFLAG_SPRSHEET_NONE,
	EXPORTFLAG_SPRSHEET_V,
	EXPORTFLAG_SPRSHEET_H
};

struct Entry {
	int flag;
	std::string tga, preload;
};

int gCntErr = 0, gCntOk = 0;
int gDefaultFlag = EntryFlags::ENTRYFLAG_EXPORT;
int gExportOptions = ExportFlags::EXPORTFLAG_SPRSHEET_NONE;
int gPackLoop = PackFlags::PACKFLAG_REPEAT_LAST_FRAME;
int gPackFrames = -1;
int gPackPadding = 0;
int gPackColourBleedingPadding = 2;
bool gPackAlphaTrimmingOnly = true;
bool gPackPowerOfTwo = true;
bool gPackGreyscale = false;
bool gSearchForEntries = false;
bool gFlipExportedFrames = true;
bool gExportCentered = false;
bool gKeepWindow = false;
bool gDebugSizesMiddle = false;
bool gDebugSizesFrame = false;

PixelData DebugColourMiddleAbs = { 255, 255, 0, 0 };//RGBA
PixelData DebugColourMiddle = { 255, 255, 255, 0 };
PixelData DebugColourOffset = { 255, 0, 0, 255 };
PixelData DebugColourFrame = { 255, 127, 127, 127 };

void PrintHelp() {
	printf_s(
		"Coded by VerMishelb\n"
		"Format researched by Emerald\n\n"

		"USAGE:\n"
		"Drag and drop one or more files to this application. The following is supported (# - supported by exe name args):\n\n"

		"No arguments, just files - equivalent to \"-e -pa\"\n\n"

		"# -pa, --pairs-auto [TGA|PRELOAD...] - App will make an entry if the pair is found [e.g. \"my_file.tga.ini.preload\" looks for \"my_file.tga\", and the opposite way].\n\n"/* gSearchForEntries is responsible for this. */

		"-p, --pair [TGA, PRELOAD] - Treat the next TWO files as the parts of the next entry (override the search). The order must not be changed.\n\n"

		"# -ps, --pairs - All files after this flag will be treated in pairs, as they would with -p.\n\n"

		"# -k, --keep - Keep the window after execution. ONLY -k WORKS FOR EXE NAME ARGS!!! (currently)\n\n"

		"CONVERSION\n"
		"# -c, --convert-to [float|int] - All files after this flag will be converted to specified version.\n\n"

		"EXPORTING\n"
		"# -e, --export - All files after this flag will have their frames extracted.\n\n"

		"# --sprite-sheet [v|h|none] - Save exported frames to the same file, either as a vertical or as a horizontal sprite sheet. Apply with \"none\" to disable.\n\n"

		"# -f, --flip - Flip exported frames vertically. Useful for CIU.\n\n"

		"# --centered - If used, frames won't be created with minimal space taken but perfectly centered.\n"
		"\tUse this if you plan to reimport the frames back using packing options.\n"
		"\tIt's better not to use this for GIFs due to space wastage.\n\n"

		"--dbg-middle, --dbg-frame - Put a RED pixel to the absolute middle, GREEN pixel at the middle + offset and GREY pixel frame around the image.\n\n"

		"PACKING\n"
		"# --pack [int|float] - All files after this flag will be combined into an atlas + preload pair.\n\n"

		"--frames [number] - Put a desired amount of frames instead of automatic detection. 0 for auto.\n\n"

		"# --loop [cycle|reverse|last] - If the amount of files specified for packing is bigger than files given, fill the rest like this:\n"
		"\tcycle - Go back to the first frame and start the next loop iteration\n"
		"\treverse - Go backwards from that point.\n"
		"\tlast - Keep using the last frame given (stop the animation).\n"
		"\tDefault: last\n\n"

		"--padding [number] - Separate frames in the atlas by [number] transparent pixels to avoid colour bleeding. 0 by default (uses --colour-margin instead). If <0, sets to 0.\n\n"

		"--power-of-two [1|0] - If enabled, the resulting atlas has 2^x dimensions. On by default. WARNING! Disabling this will result in a HUGE time increase, it's recommended not to use this option unless it's crucial.\n\n"

		"--use-alpha-trimming [1|0] - If enabled, pixels that are fully transparent but still contain colour data will not be considered. On by default.\n\n"

		"--colour-padding [number] - Add [number] fully transparent but coloured pixels around each frame to avoid colour bleeding. The default value is 2.\n\n"

		"--greyscale - If the input images sequence is saved as TrueColor 32 bpp images (e.g. how Paint.NET always saves), then the images will be converted to grayscale on the fly USING THE RED CHANNEL. Toggleable, off by default.\n\n"

		"Tip: If the executable name has brackets, some arguments can be stated here to be applied automatically.\n"
		"EXAMPLES (PS.exe is this executable):\n"
		"PS.exe -pa file1.tga file2.tga.ini.preload\n"
		"\tOpens file1.tga and file1.tga.preload, extracts all frames applying all offsets, then does the same for file2.tga.\n"
		"PS.exe file.tga file.tga.ini.preload [DEFAULT USAGE]\n"
		"\tExtract frames from file.tga.ini.preload using file.tga as the source file.\n"
		"PS.exe -pa a.tga b.tga -p C.tga C.tga.ini.preload d.tga\n"
		"\tSearch pairs for a, b and d automatically, use C.tga and C...preload as a pair.\n"
		"PS.exe -pa a.tga b.tga -ps C.tga C.preload D.tga D.preload\n"
		"\tSearch pairs for a, b, explicit pairs for C and D\n"
		"PS.exe -pa -c int A.tga\n"
		"\tLook for A.tga.ini.preload and re-save it as CI episodes preload with integer offsets.\n"
		"\nPS(-pa -c float).exe DragAndDroppedFiles.preload\n"
		"\tApply automatic completion for entries to all dropped files and convert all preloads, found from respective tga or directly dropped, to float offsets.\n"
		"PS(-pa -f).exe Files.tga\n"
		"\tAutomatic entries completion, extract frames and flip them vertically.\n\n"
	);
}

void CallPause() {
	if (gKeepWindow) {
		std::cout << "Press Enter to continue.\n";
		int c = 0;
		c = getchar();
	}
}

int ParseArgs(std::vector<Entry>& files, int& argc, char**& argv) {
	if (argc <= 1 || argc >= 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
		gKeepWindow = true;
		PrintHelp();
		CallPause();
		return 1;
	}
	else {
		std::string exe_name_args = "";
		std::string exe_name =
			std::string(argv[0]).substr(std::string(argv[0]).find_last_of("\\/") + 1);
		if (exe_name.find('(') != std::string::npos && exe_name.find(')') > exe_name.find('(')) {
			exe_name_args = exe_name.substr(exe_name.find('(') + 1, exe_name.rfind(')'));
		}
		else if (argc <= 2) {
			PrintHelp();
			return 1;
		}
		/* A bootleg replacement for proper exe name arguments system.
		Should be reworked later. */
		if (exe_name_args.find("-e") != std::string::npos) {
			gDefaultFlag = ENTRYFLAG_EXPORT;
		}
		if (exe_name_args.find("-c int") != std::string::npos) {
			gDefaultFlag = ENTRYFLAG_CONVERT_INT;
		}
		if (exe_name_args.find("-c float") != std::string::npos) {
			gDefaultFlag = ENTRYFLAG_CONVERT_FLOAT;
		}
		if (exe_name_args.find("-pa") != std::string::npos)
			gSearchForEntries = true;
		if (exe_name_args.find("-ps") != std::string::npos)
			gSearchForEntries = false;
		if (exe_name_args.find("-f") != std::string::npos)
			gFlipExportedFrames = !gFlipExportedFrames;
		if (exe_name_args.find("-k") != std::string::npos)
			gKeepWindow = true;
		if (exe_name_args.find("--sprite-sheet h") != std::string::npos)
			gExportOptions = EXPORTFLAG_SPRSHEET_H;
		if (exe_name_args.find("--sprite-sheet v") != std::string::npos)
			gExportOptions = EXPORTFLAG_SPRSHEET_V;
		if (exe_name_args.find("--sprite-sheet none") != std::string::npos)
			gExportOptions = EXPORTFLAG_SPRSHEET_NONE;
		if (exe_name_args.find("--pack int") != std::string::npos) {
			gDefaultFlag = ENTRYFLAG_PACK_INT;
		}
		if (exe_name_args.find("--pack float") != std::string::npos) {
			gDefaultFlag = ENTRYFLAG_PACK_FLOAT;
		}
		if (exe_name_args.find("--centered") != std::string::npos) {
			gExportCentered = !gExportCentered;
		}
		if (exe_name_args.find("--loop cycle")) {
			gPackLoop = PackFlags::PACKFLAG_REPEAT_LOOP;
		}
		if (exe_name_args.find("--loop reverse")) {
			gPackLoop = PackFlags::PACKFLAG_REPEAT_REVERSE;
		}
		if (exe_name_args.find("--loop last")) {
			gPackLoop = PackFlags::PACKFLAG_REPEAT_LAST_FRAME;
		}

		for (int i = 1; i < argc; ++i) {
			if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--pair")) {
				if (i + 2 >= argc) {
					printf_s(ERRMSG_NOT_ENOUGH_ARGS("-p"));
					return 0;
				}
				Entry entry{ 0 };
				entry.tga = argv[++i];
				entry.preload = argv[++i];
				entry.flag = gDefaultFlag;
				files.push_back(entry);
				continue;
			}
			else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--convert")) {
				if (i + 1 >= argc) {
					printf_s(ERRMSG_NOT_ENOUGH_ARGS("-c"));
					return 0;
				}
				if (!strcmp(argv[++i], "int"))
					gDefaultFlag = ENTRYFLAG_CONVERT_INT;
				if (!strcmp(argv[i], "float"))
					gDefaultFlag = ENTRYFLAG_CONVERT_FLOAT;
				continue;
			}
			else if (!strcmp(argv[i], "-e") || !strcmp(argv[i], "--export")) {
				gDefaultFlag = ENTRYFLAG_EXPORT;
				continue;
			}
			else if (!strcmp(argv[i], "-pa") || !strcmp(argv[i], "--pairs-auto")) {
				gSearchForEntries = true;
				continue;
			}
			else if (!strcmp(argv[i], "-ps") || !strcmp(argv[i], "--pairs")) {
				gSearchForEntries = false;
				continue;
			}
			else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--flip")) {
				gFlipExportedFrames = !gFlipExportedFrames;
				continue;
			}
			else if (!strcmp(argv[i], "-k") || !strcmp(argv[i], "--keep")) {
				gKeepWindow = true;
				continue;
			}
			else if (!strcmp(argv[i], "--sprite-sheet")) {
				if (i + 1 >= argc) {
					printf_s(ERRMSG_NOT_ENOUGH_ARGS("--sprite-sheet"));
					return 0;
				}
				if (!strcmp(argv[++i], "v"))
					gExportOptions = EXPORTFLAG_SPRSHEET_V;
				if (!strcmp(argv[i], "h"))
					gExportOptions = EXPORTFLAG_SPRSHEET_H;
				if (!strcmp(argv[i], "none"))
					gExportOptions = EXPORTFLAG_SPRSHEET_NONE;
				continue;
			}
			else if (!strcmp(argv[i], "--pack")) {
				if (i + 1 >= argc) {
					printf_s(ERRMSG_NOT_ENOUGH_ARGS("--pack"));
					return 0;
				}
				if (!strcmp(argv[++i], "int"))
					gDefaultFlag = ENTRYFLAG_PACK_INT;
				if (!strcmp(argv[i], "float"))
					gDefaultFlag = ENTRYFLAG_PACK_FLOAT;
				continue;
			}
			else if (!strcmp(argv[i], "--centered")) {
				gExportCentered = !gExportCentered;
			}
			else if (!strcmp(argv[i], "--loop")) {
				if (i + 1 >= argc) {
					printf_s(ERRMSG_NOT_ENOUGH_ARGS("--loop"));
					return 0;
				}
				if (!strcmp(argv[++i], "cycle")) {
					gPackLoop = PackFlags::PACKFLAG_REPEAT_LOOP;
				}
				if (!strcmp(argv[i], "last")) {
					gPackLoop = PackFlags::PACKFLAG_REPEAT_LAST_FRAME;
				}
				if (!strcmp(argv[i], "reverse")) {
					gPackLoop = PackFlags::PACKFLAG_REPEAT_REVERSE;
				}
			}
			else if (!strcmp(argv[i], "--frames")) {
				if (i + 1 >= argc) {
					printf_s(ERRMSG_NOT_ENOUGH_ARGS("--frames"));
					return 0;
				}
				int value = std::strtol(argv[++i], nullptr, 10);
				gPackFrames = value;
			}
			else if (!strcmp(argv[i], "--padding")) {
				if (i + 1 >= argc) {
					printf_s(ERRMSG_NOT_ENOUGH_ARGS("--padding"));
					return 0;
				}
				int value = std::strtol(argv[++i], nullptr, 10);
				if (value < 0) { value = 0; }
				gPackPadding = value;
			}
			else if (!strcmp(argv[i], "--power-of-two")) {
				if (i + 1 >= argc) {
					printf_s(ERRMSG_NOT_ENOUGH_ARGS("--power-of-two"));
					return 0;
				}
				int value = std::strtol(argv[++i], nullptr, 10);
				gPackPowerOfTwo = value;
			}
			else if (!strcmp(argv[i], "--use-alpha-trimming")) {
				if (i + 1 >= argc) {
					printf_s(ERRMSG_NOT_ENOUGH_ARGS("--use-alpha-trimming"));
					return 0;
				}
				int value = std::strtol(argv[++i], nullptr, 10);
				gPackAlphaTrimmingOnly = value;
			}
			else if (!strcmp(argv[i], "--dbg-frame")) {
				gDebugSizesFrame = true;
			}
			else if (!strcmp(argv[i], "--dbg-middle")) {
				gDebugSizesMiddle = true;
			}
			else if (!strcmp(argv[i], "--colour-padding") || !strcmp(argv[i], "--color-padding")) {
				if (i + 1 >= argc) {
					printf_s(ERRMSG_NOT_ENOUGH_ARGS("--colour-padding"));
					return 0;
				}
				int value = std::strtol(argv[++i], nullptr, 10);
				gPackColourBleedingPadding = value;
			}
			else if (!strcmp(argv[i], "--greyscale") || !strcmp(argv[i], "--grayscale")) {
				gPackGreyscale = !gPackGreyscale;
			}

			else {
				Entry entry{ 0 };
				if (gSearchForEntries) {
					std::string path = argv[i];
					if (path.rfind('.') != std::string::npos && path.substr(path.rfind('.')) == ".preload") {
						entry.preload = path;
						entry.tga = path.substr(0, path.rfind(".") - 4);//X.tga.ini.preload
					}
					else {
						entry.tga = path;
						entry.preload = path + ".ini.preload";
					}
				}
				else {
					switch (gDefaultFlag) {
					case ENTRYFLAG_EXPORT:
						if (i + 1 >= argc) { 
							printf_s("An entry without a correct file pair was found. Did you not select the second file or missed an argument parameter?\n");
							continue; 
						}
						entry.tga = argv[i];
						entry.preload = argv[++i];
						break;
					case ENTRYFLAG_CONVERT_INT:
					case ENTRYFLAG_CONVERT_FLOAT:
						entry.preload = argv[i];
						break;
					case ENTRYFLAG_PACK_INT:
					case ENTRYFLAG_PACK_FLOAT:
						entry.tga = argv[i];
						break;
					default:
						continue;
					}
				}
				entry.flag = gDefaultFlag;
				printf_s("@ %s\n~ %s\n", entry.tga.c_str(), entry.preload.c_str());
				files.push_back(entry);
			}
		}
		return 1;
	}
}

PreloadFrameData CalculateTotalFrameSize(IniPreload& preload) {
	PreloadFrameData sizes{ 0 };
	int	wLeft = 0,
		wRight = 0,
		hTop = 0,
		hBottom = 0;

	for (size_t i = 0; i < preload.frames.size(); ++i) {
		int w_half = HALF(preload.frames[i].w);//Middle pixel of the useful area+1 (3)

		/* width */
		if (preload.frames[i].xo >= 0) {
			if (wLeft < w_half - 1) {
				wLeft = w_half - 1;
			}
			if (wRight < w_half - 1 + preload.frames[i].xo - ((preload.frames[i].w % 2) ? 0 : 1)) {
				wRight = w_half - 1 + preload.frames[i].xo - ((preload.frames[i].w % 2) ? 0 : 1);
			}
		}
		else {
			if (wLeft < w_half - 1 - preload.frames[i].xo) {
				wLeft = w_half - 1 - preload.frames[i].xo;
			}
			if (wRight < w_half - 1 - ((preload.frames[i].w % 2) ? 0 : 1)) {
				wRight = w_half - 1 - ((preload.frames[i].w % 2) ? 0 : 1);
			}
		}

		int h_half = HALF(preload.frames[i].h);
		if (preload.frames[i].yo >= 0) {
			if (hTop < h_half - 1) {
				hTop = h_half - 1 ;
			}
			if (hBottom < h_half - 1 + preload.frames[i].yo - ((preload.frames[i].h % 2) ? 0 : 1)) {
				hBottom = h_half - 1 + preload.frames[i].yo - ((preload.frames[i].h % 2) ? 0 : 1);
			}
		}
		else {
			if (hTop < h_half - 1 - preload.frames[i].yo ) {
				hTop = h_half - 1 - preload.frames[i].yo ;
			}
			if (hBottom < h_half - 1 - ((preload.frames[i].h % 2) ? 0 : 1)) {
				hBottom = h_half - 1 - ((preload.frames[i].h % 2) ? 0 : 1);
			}
		}
	}

	sizes.x = (gExportCentered) ? std::max(wLeft, wRight) : wLeft;//Middle point (2)
	sizes.y = (gExportCentered) ? std::max(hTop, hBottom) : hTop;

	sizes.w = (gExportCentered) ? std::max(wLeft, wRight) * 2 + 1 : wLeft + wRight + 1;
	sizes.h = (gExportCentered) ? std::max(hTop, hBottom) * 2 + 1 : hTop + hBottom + 1;
	DEBUG_PRINTVAL(hTop, "%d");
	DEBUG_PRINTVAL(hBottom, "%d");
	//DEBUG_PAUSE;
	return sizes;
}


int main(int argc, char** argv) {
	std::cout << "Preload splitter v" VERSION_STR " by VerMishelb (" __DATE__ ")\n";

	std::vector<Entry> entries;
	if (!ParseArgs(entries, argc, argv)) {
		std::cerr << "An error occurred when parsing arguments.\n";
		return 1;
	}

	Atlas atlas;
	atlas.SetPadding(gPackPadding);
	atlas.SetColourPadding(gPackColourBleedingPadding);
	atlas.SetPowerOfTwo(gPackPowerOfTwo);
	std::vector<AtlasEntry> atlas_entries{};

	for (size_t i = 0; i < entries.size(); ++i) {
		std::cout <<
			"\nEntry " << i <<
			"\nTGA: " << entries[i].tga <<
			"\nPreload: " << entries[i].preload <<
			"\nTask: ";

		/* switch here doesn't work because of variables initialisation */
		if (entries[i].flag == ENTRYFLAG_EXPORT) {
			printf_s("Export frames.\n");
			IniPreload preload{};
			if (!preload.Open(entries[i].preload)) {
				std::cerr << ERRMSG_FILE(entries[i].preload.c_str());
				++gCntErr;
				break;
			}
			preload.PrintFrames();
			PreloadFrameData sizes{ 0 };
			sizes = CalculateTotalFrameSize(preload);
			printf_s("New dimensions: %dx%d\nAbsolute middle: (%d, %d)\n", sizes.w, sizes.h, sizes.x, sizes.y);
			Targa tga{};
			if (!tga.Open(entries[i].tga)) {
				std::cerr << ERRMSG_FILE(entries[i].tga.c_str());
				++gCntErr;
				break;
			}

			TargaHeader new_header = tga.GetHeader();
			new_header.w = (gExportOptions == EXPORTFLAG_SPRSHEET_H) ? sizes.w * preload.frames_amount : sizes.w;
			new_header.h = (gExportOptions == EXPORTFLAG_SPRSHEET_V) ? sizes.h * preload.frames_amount : sizes.h;
			/* 6th bit flips the image vertically. It's not handy for regular TGAs but saves quite a lot of time with animations. */
			//if (gFlipExportedFrames && gExportOptions == EXPORTFLAG_SPRSHEET_NONE) {
			//	new_header.image_descriptor ^= 0x20;/* This ^ is xor */
			//}
			if (gExportOptions == EXPORTFLAG_SPRSHEET_NONE) {
				for (int j = 0; j < preload.frames_amount; ++j) {
					Targa tga_out{};
					tga_out.SetHeader(new_header);
					char name_part[5] = { 0 };
					sprintf_s(name_part, "%04d", j);
					std::string new_name = (
						entries[i].tga.substr(0, entries[i].tga.rfind('.'))
						+ '_' + name_part + ".tga");
					PreloadFrameData fr{ 0 };
					fr = preload.frames[j];



					for (int y = 0; y < fr.h; ++y) {
						for (int x = 0; x < fr.w; ++x) {
							//printf_s("putting %dx%d\n", sizes.x - (HALF(fr.w) - 1) + (int)fr.xo + x, sizes.y - (HALF(fr.h) - 1) + ((gFlipExportedFrames) ? -(int)fr.yo : (int)fr.yo) + y);

							tga_out.SetPixel(
								sizes.x - (HALF(fr.w)-1) + (int)fr.xo + x,
								//69 - 86/2 - 27 = 69 - 43 - 27 = -1
								//86/2
								//hTop = h_half - 1 - preload.frames[i].yo - ((preload.frames[i].h % 2) ? 0 : 1)
								//69 = 43 - (-27) 
								sizes.y - (HALF(fr.h)-1) + (int)fr.yo + y,
								tga.GetPixel(fr.x + x,
									fr.y + y,
									(preload.format_version == preload.VERSION_FLOAT)),
								!gFlipExportedFrames
							);
						}
					}

					//Debug middle and middle with offset
					if (gDebugSizesMiddle) {
						//Middle point
						tga_out.SetPixel(sizes.x, sizes.y, DebugColourMiddleAbs, !gFlipExportedFrames);
						//Middle point with an offset
						tga_out.SetPixel(sizes.x + (int)fr.xo, sizes.y + (int)fr.yo, DebugColourOffset, !gFlipExportedFrames);
					}
					if (gDebugSizesFrame) {
						//Frame top and bottom
						for (int x = 1; x < fr.w; ++x) {
							tga_out.SetPixel(
								sizes.x - (HALF(fr.w)-1) + (int)fr.xo + x,
								sizes.y - (HALF(fr.h) - 1) + (int)fr.yo,
								DebugColourFrame, !gFlipExportedFrames);
							tga_out.SetPixel(
								sizes.x - (HALF(fr.w) - 1) + (int)fr.xo + x,
								sizes.y - (HALF(fr.h) - 1) + (int)fr.yo + fr.h - 1,
								DebugColourFrame, !gFlipExportedFrames);
						}
						//Frame sides
						for (int y = 0; y < fr.h; ++y) {
							tga_out.SetPixel(
								sizes.x - (HALF(fr.w) - 1) + (int)fr.xo,
								sizes.y - (HALF(fr.h) - 1) + (int)fr.yo + y,
								DebugColourFrame, !gFlipExportedFrames);
							tga_out.SetPixel(
								sizes.x - (HALF(fr.w) - 1) + (int)fr.xo + fr.w - 1,
								sizes.y - (HALF(fr.h) - 1) + (int)fr.yo + y,
								DebugColourFrame, !gFlipExportedFrames);
						}
					}
					printf_s("Saving %s\n", new_name.c_str());
					if (!tga_out.Save(new_name)) {
						std::cerr << ERRMSG_FILE(new_name);
						++gCntErr;
						break;
					}
				}
			}
			else if (gExportOptions == EXPORTFLAG_SPRSHEET_V || gExportOptions == EXPORTFLAG_SPRSHEET_H) {
				Targa tga_out{};
				tga_out.SetHeader(new_header);
				char name_part[16] = { 0 };
				sprintf_s(name_part, "%dx%d", sizes.w, sizes.h);
				std::string new_name = (
					entries[i].tga.substr(0, entries[i].tga.rfind('.'))
					+ "_sheet" + name_part + ".tga");

				for (int j = 0; j < preload.frames_amount; ++j) {
					PreloadFrameData fr{ 0 };
					fr = preload.frames[j];
					for (int y = 0; y < fr.h; ++y) {
						for (int x = 0; x < fr.w; ++x) {
							tga_out.SetPixel(
								sizes.x - (HALF(fr.w) - 1) + (int)fr.xo + x +
								((gExportOptions == EXPORTFLAG_SPRSHEET_H) ? (j) * sizes.w : 0),
								sizes.y - (HALF(fr.h) - 1) + (int)fr.yo + y +
								((gExportOptions == EXPORTFLAG_SPRSHEET_V) ? (j) * sizes.h : 0),

								tga.GetPixel(fr.x + x,
									fr.y + y,
									(preload.format_version == preload.VERSION_FLOAT)),
								!gFlipExportedFrames
							);
						}
					}
				}
				
				printf_s("Saving %s\n", new_name.c_str());
				if (!tga_out.Save(new_name)) {
					std::cerr << ERRMSG_FILE(new_name);
					++gCntErr;
					break;
				}
			}
			++gCntOk;
		}



		else if (entries[i].flag == ENTRYFLAG_CONVERT_FLOAT || entries[i].flag == ENTRYFLAG_CONVERT_INT) {
			printf_s("Convert to ");
			if (entries[i].flag == ENTRYFLAG_CONVERT_INT)
				printf_s("int.\n");
			else
				printf_s("float.\n");
			IniPreload preload{};
			if (!preload.Open(entries[i].preload)) {
				std::cerr << ERRMSG_FILE(entries[i].preload);
				++gCntErr;
				break;
			}
			int targetVersion =
				(entries[i].flag == ENTRYFLAG_CONVERT_INT) ?
				IniPreload::VERSION_INT : IniPreload::VERSION_FLOAT;
			if (preload.format_version == targetVersion) {
				printf_s("This file does not require conversion.\n");
				++gCntOk;
			}
			else {
				preload.SetVersion(targetVersion);
				if (!preload.Save(entries[i].preload)) {
					std::cerr << "Could not save the file.\n";
					++gCntErr;
				}
				else {
					++gCntOk;
				}
			}
		}



		else if (entries[i].flag == ENTRYFLAG_PACK_INT || entries[i].flag == ENTRYFLAG_PACK_FLOAT) {
			printf_s("Pack.\nPreload type: ");
			if (entries[i].flag == ENTRYFLAG_PACK_INT) {
				printf_s("int\n");
			}
			else {
				printf_s("float\n");
			}

			//Open the image
			AtlasEntry atl_entry{};
			if (!atl_entry.image.Open(entries[i].tga)) {
				SCREWUP;
				return 1;
			}

			//Calculate frame rect and offset
			int 
				l_bound = atl_entry.image.w,//Left empty bound width
				t_bound = atl_entry.image.h;//Top empty bound width
			Vector2f middle_point = { HALF(atl_entry.image.w)-1, HALF(atl_entry.image.h)-1 };

			DEBUG_PRINTVAL(middle_point.x, "%.2f");
			DEBUG_PRINTVAL(middle_point.y, "%.2f");
			int x_useful_min = INT_MAX, x_useful_max = INT_MIN;
			int y_useful_min = INT_MAX, y_useful_max = INT_MIN;

			bool first_transparent_segment_y = true;
			for (int jy = 0; jy < atl_entry.image.h; ++jy) {
				bool empty_line = true;
				for (int jx = 0; jx < atl_entry.image.w; ++jx) {
					//If pixel has something
					if (!atl_entry.image.PixelIsTransparent(atl_entry.image.GetPixel(jx, jy, false))) {
						if (x_useful_min > jx) { x_useful_min = jx; }
						if (x_useful_max < jx) { x_useful_max = jx; }
						if (empty_line) { empty_line = false; }
						if (l_bound > jx) { l_bound = jx; }
					}
				}
				//Line has something
				if (!empty_line) {
					if (y_useful_min > jy) {
						y_useful_min = jy;
					}
					if (y_useful_max < jy) {
						y_useful_max = jy;
					}
					if (t_bound > jy) {
						t_bound = jy;
					}
				}
			}
			//DEBUG_PRINTVAL(t_bound, "%d");
			//DEBUG_PRINTVAL(l_bound, "%d");

			//Offsets!
			if (x_useful_min == INT_MAX) {
				atl_entry.rect.w = 1;
				atl_entry.rect.h = 1;
				atl_entry.data_start.x = 0;
				atl_entry.data_start.y = 0;
				atl_entry.offset.x = 0.f;
				atl_entry.offset.y = 0.f;
			}
			else {
				atl_entry.rect.w = x_useful_max - x_useful_min + 1;// +gPackColourBleedingPadding * 2;
				atl_entry.rect.h = y_useful_max - y_useful_min + 1;// +gPackColourBleedingPadding * 2;
				atl_entry.data_start.x = l_bound;// +gPackColourBleedingPadding;
				atl_entry.data_start.y = t_bound;// +gPackColourBleedingPadding;
				atl_entry.offset.x = (HALF(atl_entry.rect.w) - 1 + l_bound) - middle_point.x;// +gPackColourBleedingPadding;
				atl_entry.offset.y = (HALF(atl_entry.rect.h) - 1 + t_bound) - middle_point.y;// +gPackColourBleedingPadding;
			}

			printf_s("Atlas entry\nWxH: %dx%d\nOffsets: %.2f, %.2f\nColour margin: %d\nMargin: %d\n",
				atl_entry.rect.w,
				atl_entry.rect.h,
				atl_entry.offset.x,
				atl_entry.offset.y,
				gPackColourBleedingPadding,
				gPackPadding
				);
			DEBUG_PRINTVAL(atl_entry.data_start.x, "%d");
			DEBUG_PRINTVAL(atl_entry.data_start.y, "%d")
			//DEBUG_PAUSE_EXIT;

			atlas_entries.push_back(atl_entry);
		}
		else {
			printf_s("Unknown entry %d flag (%d)\n", i, entries[i].flag);
			++gCntErr;
		}
	}

	if (!atlas_entries.empty()) {
		if (atlas.CreateAtlas(atlas_entries) == -1) {
			printf_s("Could not create an image atlas.\n");
			++gCntErr;
		}
		else {
			int preload_version = 0;
			if (entries[0].flag == ENTRYFLAG_PACK_FLOAT)
				preload_version = IniPreload::VERSION_FLOAT;
			else if (entries[0].flag == ENTRYFLAG_PACK_INT)
				preload_version = IniPreload::VERSION_INT;
			char new_path[_MAX_PATH] = { 0 };
			sprintf_s(
				new_path, "%satl_%s",
				entries[0].tga.substr(0, entries[0].tga.find_last_of("\\/") + 1).c_str(),
				entries[0].tga.substr(entries[0].tga.find_last_of("\\/") + 1).c_str()
			);
			if (atlas.SaveAtlas(new_path, atlas_entries, gPackFrames, gPackLoop, preload_version, gPackGreyscale) != -1) {
				++gCntOk;
			}
			else {
				++gCntErr;
			}

		}
	}

	printf_s(
		"Done working.\n\tSuccess: %d\n\tErrors: %d\n\tTotal: %d\nPlease feed Slob God or he (it?) will starve.\n",
		gCntOk, gCntErr, gCntErr + gCntOk
	);

	CallPause();

	return 0;
}