#include <EngineRuntime.h>

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Storage;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;

struct {
	bool nologo = false;
	bool print_where = false;
	bool print_version = false;
	bool silent = false;
	Array<string> packages = Array<string>(0x10);
	string root;
} state;

int ParseCommandLine(Console & console)
{
	SafePointer< Array<string> > args = GetCommandLine();
	int i = 1;
	while (i < args->Length()) {
		auto & cmd = args->ElementAt(i);
		if (cmd[0] == L':' || cmd[0] == L'-') {
			i++;
			for (int j = 1; j < cmd.Length(); j++) {
				auto arg = cmd[j];
				if (arg == L'N') {
					state.nologo = true;
				} else if (arg == L'S') {
					state.silent = true;
				} else if (arg == L'V') {
					state.print_version = true;
				} else if (arg == L'W') {
					state.print_where = true;
				} else if (arg == L'i') {
					if (i < args->Length()) {
						state.packages << IO::ExpandPath(args->ElementAt(i));
						i++;
					} else {
						console << TextColor(ConsoleColor::Yellow) << L"Invalid command line: argument expected." << TextColorDefault() << LineFeed();
						return 1;
					}
				} else {
					console << TextColor(ConsoleColor::Yellow) << FormatString(L"Command line argument \"%0\" is invalid.", string(arg, 1)) << TextColorDefault() << LineFeed();
					return 1;
				}
			}
		} else {
			console << TextColor(ConsoleColor::Yellow) << FormatString(L"Invalid command line argument \"%0\".", cmd) << TextColorDefault() << LineFeed();
			return 1;
		}
	}
	return 0;
}
int InstallPackage(Console & console, const string & package)
{
	string os, arch;
	#ifdef ENGINE_WINDOWS
	os = L"Windows";
	#endif
	#ifdef ENGINE_MACOSX
	os = L"MacOSX";
	#endif
	if (ApplicationPlatform == Platform::X86) arch = L"X86";
	else if (ApplicationPlatform == Platform::X64) arch = L"X64";
	else if (ApplicationPlatform == Platform::ARM) arch = L"ARM";
	else if (ApplicationPlatform == Platform::ARM64) arch = L"ARM64";
	if (!state.silent) console << L"Installing package \"" << IO::Path::GetFileName(package) << L"\"." << LineFeed();
	try {
		SafePointer<Stream> package_stream = new FileStream(package, AccessRead, OpenExisting);
		SafePointer<Archive> package_archive = OpenArchive(package_stream);
		if (!package_archive) {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Invalid archive file format." << TextColorDefault() << LineFeed();
			throw Exception();
		}
		for (ArchiveFile file = 1; file <= package_archive->GetFileCount(); file++) {
			auto name = package_archive->GetFileName(file);
			auto file_os = package_archive->GetFileAttribute(file, L"System");
			auto file_arch = package_archive->GetFileAttribute(file, L"Arch");
			auto folder = package_archive->GetFileAttribute(file, L"Folder");
			if (!file_os.Length()) file_os = L"*";
			if (!file_arch.Length()) file_arch = L"*";
			if (name.Length() && Syntax::MatchFilePattern(os, file_os) && Syntax::MatchFilePattern(arch, file_arch)) {
				if (!state.silent) console << L"Extracting file \"" << TextColor(ConsoleColor::Cyan) << name << TextColorDefault() << L"\"...";
				try {
					auto full_name = IO::ExpandPath(state.root + L"/" + name);
					if (string::CompareIgnoreCase(folder, L"yes") == 0) {
						IO::CreateDirectoryTree(full_name);
					} else {
						IO::CreateDirectoryTree(IO::Path::GetDirectory(full_name));
						SafePointer<Stream> source = package_archive->QueryFileStream(file);
						SafePointer<FileStream> dest = new FileStream(full_name, AccessReadWrite, CreateAlways);
						source->CopyTo(dest);
					}
				} catch (...) {
					if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Failed." << TextColorDefault() << LineFeed();
					throw;
				}
				if (!state.silent) console << TextColor(ConsoleColor::Green) << L"Succeed." << TextColorDefault() << LineFeed();
			}
		}
	} catch (IO::FileAccessException & e) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << FormatString(L"File access error (error code %0).", string(e.code, HexadecimalBase, 8)) << TextColorDefault() << LineFeed();
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Failed to install the package." << TextColorDefault() << LineFeed();
		return 1;
	} catch (...) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Failed to install the package." << TextColorDefault() << LineFeed();
		return 1;
	}
	if (!state.silent) console << TextColor(ConsoleColor::Green) << L"Package installed successfully." << TextColorDefault() << LineFeed();
	return 0;
}

int Main(void)
{
	Console console;
	try {
		int error = ParseCommandLine(console);
		if (error) return error;
		if (!state.nologo && !state.silent) {
			console << ENGINE_VI_APPNAME << LineFeed();
			console << L"Copyright " << string(ENGINE_VI_COPYRIGHT).Replace(L'\xA9', L"(C)") << LineFeed();
			console << L"Version " << ENGINE_VI_APPVERSION << L", build " << ENGINE_VI_BUILD << LineFeed() << LineFeed();
		}
		state.root = IO::Path::GetDirectory(IO::GetExecutablePath());
		if (state.print_where && !state.silent) {
			console << state.root << LineFeed();
		}
		if (state.print_version) {
			SafePointer<Registry> info;
			try {
				FileStream stream(state.root + L"/ertbndl.ecs", AccessRead, OpenExisting);
				info = LoadRegistry(&stream);
				if (!info) throw Exception();
			} catch (...) {
				if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Failed to load the bundle configuration." << TextColorDefault() << LineFeed();
				return 1;
			}
			if (!state.silent) {
				console << info->GetValueString(L"Name") << LineFeed();
				console << info->GetValueInteger(L"VersionMajor") << L"." << info->GetValueInteger(L"VersionMinor") << LineFeed();
				console << info->GetValueTime(L"Stamp").ToLocal().ToString() << LineFeed();
			}
		}
		for (auto & p : state.packages) {
			error = InstallPackage(console, p);
			if (error) return error;
		}
		if (!state.print_version && !state.print_where && !state.packages.Length() && !state.silent) {
			console << L"Command line syntax:" << LineFeed();
			console << L"  " << ENGINE_VI_APPSYSNAME << L" :NVWi" << LineFeed();
			console << L"You can optionally use the next options:" << LineFeed();
			console << L"  :N - use no logo mode - don't print application logo," << LineFeed();
			console << L"  :S - use silent mode - supress any output," << LineFeed();
			console << L"  :V - print current Runtime version and release date," << LineFeed();
			console << L"  :W - print the full path of the Runtime toolset," << LineFeed();
			console << L"  :i - install a package, package file is the next argument." << LineFeed();
			console << LineFeed();
		}
	} catch (Exception & e) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << FormatString(L"Manager failed: %0.", e.ToString()) << TextColorDefault() << LineFeed();
		return 1;
	} catch (...) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Manager failed: Unknown exception." << TextColorDefault() << LineFeed();
		return 1;
	}
	return 0;
}