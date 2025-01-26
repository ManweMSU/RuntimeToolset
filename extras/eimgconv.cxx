#include <EngineRuntime.h>

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Codec;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;

bool Save(Image * image, const string & as, const string & codec, Console & console, bool silent)
{
	try {
		if (!silent) console << L"Writing \"" << as << L"\"...";
		SafePointer<Stream> stream;
		try {
			stream = new FileStream(as, AccessReadWrite, CreateAlways);
		}
		catch (...) {
			if (!silent) console << TextColor(ConsoleColor::Red) << L"Failed." << TextColorDefault() << LineFeed();
			if (!silent) console << TextColor(ConsoleColor::Red) << L"Can not write the file." << TextColorDefault() << LineFeed();
			throw;
		}
		try {
			EncodeImage(stream, image, codec);
		}
		catch (...) {
			if (!silent) console << TextColor(ConsoleColor::Red) << L"Failed." << TextColorDefault() << LineFeed();
			if (!silent) console << TextColor(ConsoleColor::Red) << L"Can not encode the image." << TextColorDefault() << LineFeed();
			throw;
		}
		if (!silent) console << TextColor(ConsoleColor::Green) << L"Succeed." << TextColorDefault() << LineFeed();
	} catch (...) { return false; }
	return true;
}

struct {
	bool silent = false;
	bool nologo = false;
	bool decompose = false;
	Array<string> input = Array<string>(0x20);
	string output;
	string codec;
} state;

bool ParseCommandLine(Console & console)
{
	SafePointer< Array<string> > args = GetCommandLine();
	for (int i = 1; i < args->Length(); i++) {
		auto & a = args->ElementAt(i);
		if (a[0] == L'-' || a[0] == L':') {
			for (int j = 1; j < a.Length(); j++) {
				auto o = a[j];
				if (o == L'N') {
					state.nologo = true;
				} else if (o == L'S') {
					state.silent = true;
				} else if (o == L'd') {
					state.decompose = true;
				} else if (o == L'f') {
					if (state.codec.Length()) {
						console << TextColor(ConsoleColor::Red) << L"Format redefinition." << TextColorDefault() << LineFeed();
						return false;
					}
					if (i < args->Length() - 1) {
						i++;
						if (string::CompareIgnoreCase(args->ElementAt(i), L"bmp") == 0) {
							state.codec = ImageFormatDIB;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"png") == 0) {
							state.codec = ImageFormatPNG;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"jpg") == 0) {
							state.codec = ImageFormatJPEG;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"jpeg") == 0) {
							state.codec = ImageFormatJPEG;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"gif") == 0) {
							state.codec = ImageFormatGIF;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"tif") == 0) {
							state.codec = ImageFormatTIFF;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"tiff") == 0) {
							state.codec = ImageFormatTIFF;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"ico") == 0) {
							state.codec = ImageFormatWindowsIcon;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"cur") == 0) {
							state.codec = ImageFormatWindowsCursor;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"icns") == 0) {
							state.codec = ImageFormatAppleIcon;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"eiwv") == 0) {
							state.codec = ImageFormatEngine;
						} else {
							console << TextColor(ConsoleColor::Red) << L"Unknown image format name." << TextColorDefault() << LineFeed();
							return false;
						}
					} else {
						console << TextColor(ConsoleColor::Red) << L"Not enough command line arguments." << TextColorDefault() << LineFeed();
						return false;
					}
				} else if (o == L'o') {
					if (state.output.Length()) {
						console << TextColor(ConsoleColor::Red) << L"Output path redefinition." << TextColorDefault() << LineFeed();
						return false;
					}
					if (i < args->Length() - 1) {
						i++;
						state.output = args->ElementAt(i);
					} else {
						console << TextColor(ConsoleColor::Red) << L"Not enough command line arguments." << TextColorDefault() << LineFeed();
						return false;
					}
				} else {
					console << TextColor(ConsoleColor::Red) << L"Invalid command line option." << TextColorDefault() << LineFeed();
					return false;
				}
			}
		} else state.input << args->ElementAt(i);
	}
	return true;
}
string ExtensionForCodec(const string & codec)
{
	if (codec == ImageFormatDIB) return L"bmp";
	else if (codec == ImageFormatPNG) return L"png";
	else if (codec == ImageFormatJPEG) return L"jpg";
	else if (codec == ImageFormatGIF) return L"gif";
	else if (codec == ImageFormatTIFF) return L"tif";
	else if (codec == ImageFormatDDS) return L"dds";
	else if (codec == ImageFormatHEIF) return L"heif";
	else if (codec == ImageFormatWindowsIcon) return L"ico";
	else if (codec == ImageFormatWindowsCursor) return L"cur";
	else if (codec == ImageFormatAppleIcon) return L"icns";
	else if (codec == ImageFormatEngine) return L"eiwv";
	else return L"";
}

int Main(void)
{
	Console console;
	if (!ParseCommandLine(console)) return 2;
	if (!state.nologo && !state.silent) {
		console << ENGINE_VI_APPNAME << LineFeed();
		console << L"Copyright " << string(ENGINE_VI_COPYRIGHT).Replace(L'\xA9', L"(C)") << LineFeed();
		console << L"Version " << ENGINE_VI_APPVERSION << L", build " << ENGINE_VI_BUILD << LineFeed() << LineFeed();
	}
	if (!state.input.Length()) {
		if (!state.silent) {
			console << L"Command line syntax:" << LineFeed();
			console << L"  " << ENGINE_VI_APPSYSNAME << L" <input> :NSdfo" << LineFeed();
			console << L"Where" << LineFeed();
			console << L"  input - files (or file filters) to convert," << LineFeed();
			console << L"  :N    - use no logo mode - don't print application logo," << LineFeed();
			console << L"  :S    - use silent mode - supress any output," << LineFeed();
			console << L"  :d    - save images as separate files instead of multi-frame image file," << LineFeed();
			console << L"  :f    - format (codec) used to save images, defaults to the input one," << LineFeed();
			console << L"            one of the following values must be passed as the next argument:" << LineFeed();
			console << L"            bmp  - Windows Bitmap," << LineFeed();
			console << L"            png  - Portable Network Graphics," << LineFeed();
			console << L"            jpeg - Joint Photographic Image Format," << LineFeed();
			console << L"            gif  - Graphics Interchange Format," << LineFeed();
			console << L"            tiff - Tagged Image File Format," << LineFeed();
			console << L"            ico  - Windows Icon," << LineFeed();
			console << L"            cur  - Windows Cursor," << LineFeed();
			console << L"            icns - Apple Icon," << LineFeed();
			console << L"            eiwv - Engine Image Volume," << LineFeed();
			console << L"  :o    - overrides the name of the output file or directory." << LineFeed();
			console << LineFeed();
		}
	} else {
		Codec::InitializeDefaultCodecs();
		try {
			SafePointer<Image> image = new Image;
			for (auto & s : state.input) {
				auto source = IO::ExpandPath(s);
				SafePointer< Array<string> > files = IO::Search::GetFiles(source);
				if (!files->Length() && !state.silent) console << TextColor(ConsoleColor::Yellow) << L"No files found." << TextColorDefault() << LineFeed();
				for (int i = 0; i < files->Length(); i++) {
					string codec;
					string name = IO::ExpandPath(IO::Path::GetDirectory(source) + L"/" + files->ElementAt(i));
					if (!state.silent) console << L"Importing \"" + name + L"\"...";
					SafePointer<Stream> input;
					SafePointer<Image> decoded;
					try { input = new FileStream(name, AccessRead, OpenExisting); }
					catch (...) {
						if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Failed." << TextColorDefault() << LineFeed();
						if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Can not access the file." << TextColorDefault() << LineFeed();
						return 1;
					}
					try { decoded = DecodeImage(input, &codec, 0); if (!decoded) throw Exception(); }
					catch (...) {
						if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Failed." << TextColorDefault() << LineFeed();
						if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Can not decode the file." << TextColorDefault() << LineFeed();
						return 1;
					}
					if (!state.codec.Length()) state.codec = codec;
					if (!state.output.Length()) {
						auto suffix = state.decompose ? L"" : L"." + ExtensionForCodec(state.codec);
						auto serial = 0;
						state.output = IO::ExpandPath(IO::Path::GetDirectory(source) + L"/" + IO::Path::GetFileNameWithoutExtension(name));
						while (true) {
							string out;
							if (serial) out = state.output + L"." + string(uint(serial), DecimalBase, 3) + suffix;
							else out = state.output + suffix;
							try { IO::GetFileType(out); serial++; } catch (...) { state.output = out; break; }
						}
					}
					for (int j = 0; j < decoded->Frames.Length(); j++) image->Frames.Append(decoded->Frames.ElementAt(j));
					if (!state.silent) console << TextColor(ConsoleColor::Green) << L"Succeed." << TextColorDefault() << LineFeed();
				}
			}
			if (!image->Frames.Length()) {
				if (!state.silent) console << TextColor(ConsoleColor::Red) << L"No frames loaded. Exiting." << TextColorDefault() << LineFeed();
				return 1;
			}
			if (state.decompose) {
				auto suffix = L"." + ExtensionForCodec(state.codec);
				CreateDirectoryTree(state.output);
				for (int i = 0; i < image->Frames.Length(); i++) {
					SafePointer<Image> fake = new Image;
					fake->Frames.Append(image->Frames.ElementAt(i));
					if (!Save(fake, state.output + L"/frame_" + string(uint32(i + 1), DecimalBase, 3) + suffix, state.codec, console, state.silent)) return 1;
				}
			} else if (!Save(image, state.output, state.codec, console, state.silent)) return 1;
		}
		catch (...) {
			console << TextColor(ConsoleColor::Red) << L"Internal error." << TextColorDefault() << LineFeed();
			return 1;
		}
	}
	return 0;
}