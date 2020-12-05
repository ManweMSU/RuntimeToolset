#include <EngineRuntime.h>

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Storage;
using namespace Engine::IO::ConsoleControl;

string root;
string test_root;
SafePointer<Registry> config;
Array<string> arch_list = Array<string>(0x10);

string ExpandPath(const string & path, const string & relative_to)
{
	if (path[0] == L'/' || (path[0] && path[1] == L':')) return IO::ExpandPath(path);
	else return IO::ExpandPath(relative_to + L"/" + path);
}
bool LoadConfigurations(Console & console)
{
	root = IO::Path::GetDirectory(IO::GetExecutablePath());
	console.Write(L"Loading common configuration files...");
	try {
		try {
			FileStream stream(root + L"/ertaconf.ecs", AccessRead, OpenExisting);
			config = LoadRegistry(&stream);
			if (!config) throw Exception();
		} catch (...) {
			FileStream stream(root + L"/ertaconf.ini", AccessRead, OpenExisting);
			config = CompileTextRegistry(&stream);
			if (!config) throw Exception();
		}
	} catch (...) {
		console << TextColor(Console::ColorRed) << L"Failed to load the target set." << TextColorDefault() << LineFeed();
		return false;
	}
	SafePointer<RegistryNode> archs = config->OpenNode(L"AvailableArchitectures");
	if (archs) for (auto & v : archs->GetValues()) if (archs->GetValueBoolean(v)) arch_list.Append(v);
	test_root = ExpandPath(config->GetValueString(L"Test"), root);
	console.WriteLine(L"Done.");
	return true;
}
void ProduceFrame(int width, Codec::Image * image)
{
	int border = width / 16;
	SafePointer<Codec::Frame> frame = new Codec::Frame(width, width, -1, Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Normal, Codec::ScanOrigin::TopDown);
	for (int y = 0; y < width; y++) for (int x = 0; x < width; x++) {
		uint32 pixel = 0x4444FF00;
		if (y < border || y > width - border - 1 || x < border || x > width - border - 1) pixel = 0xCCFF4400;
		frame->SetPixel(x, y, pixel);
	}
	image->Frames.Append(frame);
}
void CreateIcon(Console & console)
{
	auto file = IO::ExpandPath(test_root + L"/test.eiwv");
	console.Write(FormatString(L"Creating icon at: \"%0\"..", file));
	SafePointer<Codec::Image> image = new Codec::Image;
	ProduceFrame(16, image);
	ProduceFrame(24, image);
	ProduceFrame(32, image);
	ProduceFrame(48, image);
	ProduceFrame(64, image);
	ProduceFrame(128, image);
	ProduceFrame(256, image);
	ProduceFrame(512, image);
	ProduceFrame(1024, image);
	FileStream image_stream(file, AccessReadWrite, CreateAlways);
	Codec::EncodeImage(&image_stream, image, L"EIWV");
	console.WriteLine(L"Done.");
}
bool BuildTest(const string & arch, bool debug)
{
	Array<string> args(0x10);
	args << IO::ExpandPath(test_root + L"/test.ertproj");
	if (debug) args << L":Nad"; else args << L":Nar";
	args << arch;
	SafePointer<Process> builder = CreateCommandProcess(L"ertbuild", &args);
	if (builder) {
		builder->Wait();
		if (builder->GetExitCode()) return false;
		return true;
	} else return false;
}

int Main(void)
{
	Console console;
	try {
		UI::Windows::InitializeCodecCollection();
		if (!LoadConfigurations(console)) return 1;
		CreateIcon(console);
		int try_count = 0;
		int success_count = 0;
		for (auto & a : arch_list) {
			if (BuildTest(a, false)) success_count++;
			if (BuildTest(a, true)) success_count++;
			try_count += 2;
		}
		console << FormatString(L"Build command was invoked for %0 configurations.", try_count) << LineFeed();
		console << FormatString(L"%0 of them succeeded (%1%%).", try_count, success_count * 100 / try_count) << LineFeed();
	} catch (...) {
		console << TextColor(Console::ColorRed) << L"Internal error." << TextColorDefault() << LineFeed();
		return 1;
	}
	return 0;
}