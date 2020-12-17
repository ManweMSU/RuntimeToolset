#include <EngineRuntime.h>

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Codec;
using namespace Engine::IO::ConsoleControl;

bool Save(Image * image, const string & as, const string & codec, Console & console)
{
	try {
		console << L"Writing \"" << as << L"\"...";
		SafePointer<Stream> stream;
		try {
			stream = new FileStream(as, AccessReadWrite, CreateAlways);
		}
		catch (...) {
			console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
			console << TextColor(Console::ColorRed) << L"Can not write the file." << TextColorDefault() << LineFeed();
			throw;
		}
		try {
			EncodeImage(stream, image, codec);
		}
		catch (...) {
			console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
			console << TextColor(Console::ColorRed) << L"Can not encode the image." << TextColorDefault() << LineFeed();
			throw;
		}
		console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
	} catch (...) { return false; }
	return true;
}
int Main(void)
{
	Console console;

	SafePointer< Array<string> > args = GetCommandLine();

	console << ENGINE_VI_APPNAME << LineFeed();
	console << L"Copyright " << string(ENGINE_VI_COPYRIGHT).Replace(L'\xA9', L"(C)") << LineFeed();
	console << L"Version " << ENGINE_VI_APPVERSION << L", build " << ENGINE_VI_BUILD << LineFeed() << LineFeed();
	
	if (args->Length() < 2) {
		console << L"Command line syntax:" << LineFeed();
		console << L"  " << ENGINE_VI_APPSYSNAME << L" <input> <format> [:decompose] <output>" << LineFeed();
		console << L"Where" << LineFeed();
		console << L"  input      - file (or files written by file filter) to convert," << LineFeed();
		console << L"  format     - format (codec) to use to save images," << LineFeed();
		console << L"  :decompose - save images in separate files instead of multi-frame image file," << LineFeed();
		console << L"  output     - output file name or directory if :decompose specified." << LineFeed();
		console << L"Available image formats:" << LineFeed();
		console << L"  :bmp       - Windows Bitmap," << LineFeed();
		console << L"  :png       - Portable Network Graphics," << LineFeed();
		console << L"  :jpg       - Joint Photographic Image Format," << LineFeed();
		console << L"  :gif       - Graphics Interchange Format," << LineFeed();
		console << L"  :tif       - Tagged Image File Format," << LineFeed();
		console << L"  :ico       - Windows Icon," << LineFeed();
		console << L"  :cur       - Windows Cursor," << LineFeed();
		console << L"  :icns      - Apple Icon," << LineFeed();
		console << L"  :eiwv      - Engine Image Volume." << LineFeed();
		console << LineFeed();
	} else {
		string source;
		string output;
		string codec;
		bool decompose = false;
		for (int i = 1; i < args->Length(); i++) {
			if (args->ElementAt(i)[0] == L':') {
				if (string::CompareIgnoreCase(args->ElementAt(i), L":decompose") == 0) {
					decompose = true;
				} else if (string::CompareIgnoreCase(args->ElementAt(i), L":bmp") == 0) {
					codec = L"BMP";
				} else if (string::CompareIgnoreCase(args->ElementAt(i), L":png") == 0) {
					codec = L"PNG";
				} else if (string::CompareIgnoreCase(args->ElementAt(i), L":jpg") == 0) {
					codec = L"JPG";
				} else if (string::CompareIgnoreCase(args->ElementAt(i), L":gif") == 0) {
					codec = L"GIF";
				} else if (string::CompareIgnoreCase(args->ElementAt(i), L":tif") == 0) {
					codec = L"TIF";
				} else if (string::CompareIgnoreCase(args->ElementAt(i), L":ico") == 0) {
					codec = L"ICO";
				} else if (string::CompareIgnoreCase(args->ElementAt(i), L":cur") == 0) {
					codec = L"CUR";
				} else if (string::CompareIgnoreCase(args->ElementAt(i), L":icns") == 0) {
					codec = L"ICNS";
				} else if (string::CompareIgnoreCase(args->ElementAt(i), L":eiwv") == 0) {
					codec = L"EIWV";
				} else {
					console << TextColor(Console::ColorRed) << L"Invalid command line argument." << TextColorDefault() << LineFeed();
					return 1;
				}
			} else {
				if (!source.Length()) {
					source = args->ElementAt(i);
				} else if (!output.Length()) {
					output = args->ElementAt(i);
				} else {
					console << TextColor(Console::ColorRed) << L"Invalid command line argument." << TextColorDefault() << LineFeed();
					return 1;
				}
			}
		}
		if (!source.Length()) {
			console << TextColor(Console::ColorRed) << L"No input in command line." << TextColorDefault() << LineFeed();
			return 1;
		}
		if (!codec.Length()) {
			console << TextColor(Console::ColorRed) << L"No format in command line." << TextColorDefault() << LineFeed();
			return 1;
		}
		if (!output.Length()) {
			console << TextColor(Console::ColorRed) << L"No output in command line." << TextColorDefault() << LineFeed();
			return 1;
		}
		source = IO::ExpandPath(source);
		output = IO::ExpandPath(output);
		UI::Windows::InitializeCodecCollection();
		try {
			SafePointer<Image> image = new Image;
			SafePointer< Array<string> > files = IO::Search::GetFiles(source);
			if (!files->Length()) {
				console << TextColor(Console::ColorRed) << L"No files found." << TextColorDefault() << LineFeed();
				return 1;
			}
			for (int i = 0; i < files->Length(); i++) {
				string name = IO::Path::GetDirectory(source) + L"/" + files->ElementAt(i);
				console << L"Importing \"" + name + L"\"...";
				SafePointer<Stream> input;
				SafePointer<Image> decoded;
				try {
					input = new FileStream(name, AccessRead, OpenExisting);
				}
				catch (...) {
					console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
					console << TextColor(Console::ColorRed) << L"Can not access the file." << TextColorDefault() << LineFeed();
					return 1;
				}
				try {
					decoded = DecodeImage(input);
				}
				catch (...) {
					console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
					console << TextColor(Console::ColorRed) << L"Can not decode the file." << TextColorDefault() << LineFeed();
					return 1;
				}
				for (int j = 0; j < decoded->Frames.Length(); j++) image->Frames.Append(decoded->Frames.ElementAt(j));
				console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
			}
			if (decompose) {
				try { IO::CreateDirectory(output); } catch (...) {}
				for (int i = 0; i < image->Frames.Length(); i++) {
					SafePointer<Image> fake = new Image;
					fake->Frames.Append(image->Frames.ElementAt(i));
					if (!Save(fake, output + L"/frame_" + string(uint32(i), L"0123456789", 3) + L"." + codec.LowerCase(), codec, console)) return 1;
				}
			} else {
				if (!Save(image, output, codec, console)) return 1;
			}
		}
		catch (...) {
			console << TextColor(Console::ColorRed) << L"Internal error." << TextColorDefault() << LineFeed();
			return 1;
		}
	}
	return 0;
}