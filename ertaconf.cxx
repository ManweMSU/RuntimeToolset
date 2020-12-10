#include <EngineRuntime.h>

#ifdef ENGINE_WINDOWS
#include <PlatformSpecific/WindowsRegistry.h>
#endif

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Storage;
using namespace Engine::IO::ConsoleControl;

string AllocateIndex(void)
{
	static int h = 0;
	static int l = 0;
	widechar word[3];
	word[0] = L'A' + h;
	word[1] = L'A' + l;
	word[2] = 0;
	l++; if (l >= 26) { h++; l = 0; }
	return string(word);
}

class TargetSetConfig
{
public:
	string target_arch, target_os, target_conf, target_subs;
	string bootstrapper, source_filter;
	string extension_executable;
	string extension_object;
	string path_object;
	string path_runtime;
	Array<string> defines = Array<string>(0x10);
	struct {
		string command;
		string argument_define;
		string argument_include;
		string argument_output;
		Array<string> arguments = Array<string>(0x20);
	} compiler;
	struct {
		string command;
		string argument_output;
		Array<string> arguments = Array<string>(0x20);
	} linker;
	struct {
		string command;
		string redefine_output;
		string redefine_link;
		string icon_codec;
		string icon_extension;
		Array<int> icon_sizes = Array<int>(0x10);
		bool is_library = false;
		bool is_windows = false;
		bool is_macosx = false;
		struct {
			string command;
			string argument_output;
			Array<string> arguments = Array<string>(0x20);
		} compiler;
	} resource;

	TargetSetConfig(const string & _arch, const string & _os, const string & _conf, const string & _subs) :
		target_arch(_arch), target_os(_os), target_conf(_conf), target_subs(_subs) {}
	void Serialize(RegistryNode * to)
	{
		string name = target_os;
		if (target_subs.Length()) name += L"-" + target_subs;
		if (target_arch.Length()) name += L"-" + target_arch;
		if (target_conf.Length()) name += L"-" + target_conf;
		to->CreateNode(name);
		SafePointer<RegistryNode> node = to->OpenNode(name);
		if (bootstrapper.Length()) {
			node->CreateValue(L"Bootstrapper", RegistryValueType::String);
			node->SetValue(L"Bootstrapper", bootstrapper);
		}
		if (source_filter.Length()) {
			node->CreateValue(L"CompileFilter", RegistryValueType::String);
			node->SetValue(L"CompileFilter", source_filter);
		}
		if (extension_executable.Length()) {
			node->CreateValue(L"ExecutableExtension", RegistryValueType::String);
			node->SetValue(L"ExecutableExtension", extension_executable);
		}
		if (extension_object.Length()) {
			node->CreateValue(L"ObjectExtension", RegistryValueType::String);
			node->SetValue(L"ObjectExtension", extension_object);
		}
		if (path_object.Length()) {
			node->CreateValue(L"ObjectPath", RegistryValueType::String);
			node->SetValue(L"ObjectPath", path_object);
		}
		if (path_runtime.Length()) {
			node->CreateValue(L"RuntimePath", RegistryValueType::String);
			node->SetValue(L"RuntimePath", path_runtime);
		}
		if (compiler.command.Length() || compiler.argument_define.Length() || compiler.argument_include.Length() ||
			compiler.argument_output.Length() || compiler.arguments.Length()) {
			node->CreateNode(L"Compiler");
			SafePointer<RegistryNode> cnode = node->OpenNode(L"Compiler");
			if (compiler.command.Length()) {
				cnode->CreateValue(L"Path", RegistryValueType::String);
				cnode->SetValue(L"Path", compiler.command);
			}
			if (compiler.argument_define.Length()) {
				cnode->CreateValue(L"DefineArgument", RegistryValueType::String);
				cnode->SetValue(L"DefineArgument", compiler.argument_define);
			}
			if (compiler.argument_include.Length()) {
				cnode->CreateValue(L"IncludeArgument", RegistryValueType::String);
				cnode->SetValue(L"IncludeArgument", compiler.argument_include);
			}
			if (compiler.argument_output.Length()) {
				cnode->CreateValue(L"OutputArgument", RegistryValueType::String);
				cnode->SetValue(L"OutputArgument", compiler.argument_output);
			}
			if (compiler.arguments.Length()) {
				cnode->CreateNode(L"Arguments");
				SafePointer<RegistryNode> anode = cnode->OpenNode(L"Arguments");
				for (auto & a : compiler.arguments) {
					auto index = AllocateIndex();
					anode->CreateValue(index, RegistryValueType::String);
					anode->SetValue(index, a);
				}
			}
		}
		if (linker.command.Length() || linker.argument_output.Length() || linker.arguments.Length()) {
			node->CreateNode(L"Linker");
			SafePointer<RegistryNode> lnode = node->OpenNode(L"Linker");
			if (linker.command.Length()) {
				lnode->CreateValue(L"Path", RegistryValueType::String);
				lnode->SetValue(L"Path", linker.command);
			}
			if (linker.argument_output.Length()) {
				lnode->CreateValue(L"OutputArgument", RegistryValueType::String);
				lnode->SetValue(L"OutputArgument", linker.argument_output);
			}
			if (linker.arguments.Length()) {
				lnode->CreateNode(L"Arguments");
				SafePointer<RegistryNode> anode = lnode->OpenNode(L"Arguments");
				for (auto & a : linker.arguments) {
					auto index = AllocateIndex();
					anode->CreateValue(index, RegistryValueType::String);
					anode->SetValue(index, a);
				}
			}
		}
		if (resource.command.Length() || resource.redefine_link.Length() || resource.redefine_output.Length() ||
			resource.icon_codec.Length() || resource.icon_extension.Length() || resource.icon_sizes.Length() ||
			resource.compiler.command.Length() || resource.compiler.argument_output.Length() || resource.compiler.arguments.Length() ||
			resource.is_library || resource.is_windows || resource.is_macosx) {
			node->CreateNode(L"Resource");
			SafePointer<RegistryNode> rnode = node->OpenNode(L"Resource");
			if (resource.command.Length()) {
				rnode->CreateValue(L"Path", RegistryValueType::String);
				rnode->SetValue(L"Path", resource.command);
			}
			if (resource.redefine_link.Length()) {
				rnode->CreateValue(L"SetLink", RegistryValueType::String);
				rnode->SetValue(L"SetLink", resource.redefine_link);
			}
			if (resource.redefine_output.Length()) {
				rnode->CreateValue(L"SetOutput", RegistryValueType::String);
				rnode->SetValue(L"SetOutput", resource.redefine_output);
			}
			if (resource.icon_codec.Length()) {
				rnode->CreateValue(L"IconCodec", RegistryValueType::String);
				rnode->SetValue(L"IconCodec", resource.icon_codec);
			}
			if (resource.icon_extension.Length()) {
				rnode->CreateValue(L"IconExtension", RegistryValueType::String);
				rnode->SetValue(L"IconExtension", resource.icon_extension);
			}
			if (resource.is_windows) {
				rnode->CreateValue(L"Windows", RegistryValueType::Boolean);
				rnode->SetValue(L"Windows", true);
			}
			if (resource.is_macosx) {
				rnode->CreateValue(L"MacOSX", RegistryValueType::Boolean);
				rnode->SetValue(L"MacOSX", true);
			}
			if (resource.is_library) {
				rnode->CreateValue(L"Library", RegistryValueType::Boolean);
				rnode->SetValue(L"Library", true);
			}
			if (resource.compiler.command.Length() || resource.compiler.argument_output.Length() || resource.compiler.arguments.Length()) {
				rnode->CreateNode(L"Compiler");
				SafePointer<RegistryNode> cnode = rnode->OpenNode(L"Compiler");
				if (resource.compiler.command.Length()) {
					cnode->CreateValue(L"Path", RegistryValueType::String);
					cnode->SetValue(L"Path", resource.compiler.command);
				}
				if (resource.compiler.argument_output.Length()) {
					cnode->CreateValue(L"OutputArgument", RegistryValueType::String);
					cnode->SetValue(L"OutputArgument", resource.compiler.argument_output);
				}
				if (resource.compiler.arguments.Length()) {
					cnode->CreateNode(L"Arguments");
					SafePointer<RegistryNode> anode = cnode->OpenNode(L"Arguments");
					for (auto & a : resource.compiler.arguments) {
						auto index = AllocateIndex();
						anode->CreateValue(index, RegistryValueType::String);
						anode->SetValue(index, a);
					}
				}
			}
			if (resource.icon_sizes.Length()) {
				rnode->CreateNode(L"IconSizes");
				SafePointer<RegistryNode> snode = rnode->OpenNode(L"IconSizes");
				widechar index[2];
				index[1] = 0;
				int lcnt = 0;
				for (auto & s : resource.icon_sizes) {
					index[0] = L'A' + lcnt;
					snode->CreateValue(index, RegistryValueType::Integer);
					snode->SetValue(index, s);
					lcnt++;
				}
			}
		}
		if (defines.Length()) {
			node->CreateNode(L"Defines");
			SafePointer<RegistryNode> dnode = node->OpenNode(L"Defines");
			for (auto & d : defines) {
				dnode->CreateValue(d, RegistryValueType::Boolean);
				dnode->SetValue(d, true);
			}
		}
	}
};
class Toolset
{
public:
	string arch;
	string compiler;
	string linker;
	Array<string> lib = Array<string>(0x10);
};
class BuildVersion
{
public:
	Array<string> command_line = Array<string>(0x10);
	string os, arch, conf;
	bool successful;
};

struct {
	bool verbose = false;
	bool clean = false;
	bool configure_mode = false;
	bool preserve_mode = false;
	bool self_config_text = false;
	bool build_config_text = false;
	string runtime_path;
	string object_path;
	string root_path;
	string current_os;
	string msvc_root;
	string windows_sdk_root;
	string windows_sdk_version;
	string default_compiler_path;
	SafePointer<Registry> self_config;
	SafePointer<Registry> build_config;
	SafeArray<TargetSetConfig> atoms = SafeArray<TargetSetConfig>(0x20);
	SafeArray<Toolset> tools = SafeArray<Toolset>(0x10);
	SafeArray<BuildVersion> cache_versions = SafeArray<BuildVersion>(0x20);
	Array<string> common_include = Array<string>(0x10);
	string common_resource_compiler;
} state;

handle stdout_clone, stderr_clone, stdin_clone;

#ifdef ENGINE_WINDOWS
string ExecuteAndGetOutput(const string & exe, int * exit_code = 0)
{
	handle pipe_in, pipe_out;
	IO::CreatePipe(&pipe_in, &pipe_out);
	IO::SetStandardOutput(pipe_in);
	IO::SetStandardError(pipe_in);
	SafePointer<Process> process = CreateProcess(exe);
	IO::CloseFile(pipe_in);
	IO::SetStandardOutput(stdout_clone);
	IO::SetStandardError(stderr_clone);
	if (!process) { IO::CloseFile(pipe_out); if (exit_code) *exit_code = -1; return L""; }
	process->Wait();
	if (exit_code) *exit_code = process->GetExitCode();
	FileStream stream(pipe_out, true);
	TextReader reader(&stream, Encoding::ANSI);
	return reader.ReadAll();
}
bool SearchForWindowsToolsets(Console & console)
{
	console.Write(L"Searching for the toolset...");
	try {
		SafePointer<WindowsSpecific::RegistryKey> sys_root = WindowsSpecific::OpenRootRegistryKey(WindowsSpecific::RegistryRootKey::LocalMachine);
		if (!sys_root) {
			console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
			console << TextColor(Console::ColorRed) << L"Registry root access failure." << TextColorDefault() << LineFeed();
			return false;
		}
		SafePointer<WindowsSpecific::RegistryKey> kits_root = sys_root->OpenKey(L"Software\\Microsoft\\Windows Kits\\Installed Roots", WindowsSpecific::RegistryKeyAccess::ReadOnly);
		if (!kits_root) {
			console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
			console << TextColor(Console::ColorRed) << FormatString(L"Failed to open \"%0\".", L"HKLM\\Software\\Microsoft\\Windows Kits\\Installed Roots") << TextColorDefault() << LineFeed();
			return false;
		}
		state.windows_sdk_root = kits_root->GetValueString(L"KitsRoot10");
		SafePointer< Array<string> > versions = kits_root->EnumerateSubkeys();
		SortArray(*versions);
		if (!versions->Length()) {
			console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
			console << TextColor(Console::ColorRed) << L"No Windows 10 SDK versions found in the registry." << TextColorDefault() << LineFeed();
			return false;
		}
		state.windows_sdk_version = versions->LastElement();
	} catch (...) {
		console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
		console << TextColor(Console::ColorRed) << L"General registry access failure." << TextColorDefault() << LineFeed();
		return false;
	}
	string drive = WindowsSpecific::GetShellFolderPath(WindowsSpecific::ShellFolder::WindowsRoot).Fragment(0, 3);
	SafePointer< Array<string> > program_files = IO::Search::GetDirectories(drive + L"Program Files*");
	Array<string> compiler_candidates(0x10);
	for (auto & pf : *program_files) {
		SafePointer< Array<string> > visual_studio = IO::Search::GetDirectories(drive + pf + L"\\*Visual Studio*");
		for (auto & vs : *visual_studio) {
			SafePointer< Array<string> > compiler = IO::Search::GetFiles(drive + pf + L"\\" + vs + L"\\cl.exe", true);
			for (auto & cl : *compiler) compiler_candidates.Append(drive + pf + L"\\" + vs + L"\\" + cl);
		}
	}
	if (!compiler_candidates.Length()) {
		console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
		console << TextColor(Console::ColorRed) << L"Failed to locate the Visual C++ toolset." << TextColorDefault() << LineFeed();
		return false;
	}
	auto base_compiler = compiler_candidates.FirstElement().LowerCase();
	int pos = base_compiler.FindLast(L"\\bin\\");
	state.msvc_root = compiler_candidates.FirstElement().Fragment(0, pos);
	console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
	if (state.verbose) {
		console.SetTextColor(Console::ColorCyan);
		console.WriteLine(FormatString(L"The Windows 10 SDK root is \"%0\".", state.windows_sdk_root));
		console.WriteLine(FormatString(L"The Windows 10 SDK version is \"%0\".", state.windows_sdk_version));
		console.WriteLine(FormatString(L"The Visual C++ toolset root is \"%0\".", state.msvc_root));
		console.SetTextColor(Console::ColorDefault);
	}
	console.Write(L"Setting up the toolset...");
	SafePointer< Array<string> > compilers = IO::Search::GetFiles(state.msvc_root + L"\\cl.exe", true);
	for (auto & cl : *compilers) {
		auto output = ExecuteAndGetOutput(state.msvc_root + L"\\" + cl);
		auto local_bin = IO::Path::GetDirectory(state.msvc_root + L"\\" + cl);
		if (output.Length() && IO::FileExists(local_bin + L"\\link.exe")) {
			Toolset toolset;
			if (output.FindFirst(L"x86") != -1) toolset.arch = L"X86";
			else if (output.FindFirst(L"x64") != -1) toolset.arch = L"X64";
			else if (output.FindFirst(L"ARM64") != -1) toolset.arch = L"ARM64";
			else if (output.FindFirst(L"ARM") != -1) toolset.arch = L"ARM";
			else continue;
			bool duplicate = false;
			for (auto & ts : state.tools) if (ts.arch == toolset.arch) { duplicate = true; break; }
			if (duplicate) continue;
			toolset.compiler = local_bin + L"\\cl.exe";
			toolset.linker = local_bin + L"\\link.exe";
			auto win_sdk_lib = state.windows_sdk_root + L"Lib\\" + state.windows_sdk_version + L"\\";
			auto msvc_lib = state.msvc_root + L"\\lib\\";
			SafePointer< Array<string> > dirs = IO::Search::GetDirectories(win_sdk_lib + L"*");
			for (auto & d : *dirs) {
				SafePointer< Array<string> > archs = IO::Search::GetDirectories(win_sdk_lib + d + L"\\*");
				for (auto & a : *archs) if (string::CompareIgnoreCase(a, toolset.arch) == 0) {
					toolset.lib.Append(win_sdk_lib + d + L"\\" + a);
					break;
				}
			}
			SafePointer< Array<string> > archs = IO::Search::GetDirectories(msvc_lib + L"*");
			for (auto & a : *archs) if (string::CompareIgnoreCase(a, toolset.arch) == 0) {
				toolset.lib.Append(msvc_lib + a);
				break;
			}
			state.tools.Append(toolset);
		}
	}
	auto win_sdk_include = state.windows_sdk_root + L"Include\\" + state.windows_sdk_version + L"\\";
	auto msvc_include = state.msvc_root + L"\\include";
	SafePointer< Array<string> > dirs = IO::Search::GetDirectories(win_sdk_include + L"*");
	for (auto & d : *dirs) { state.common_include.Append(win_sdk_include + d); }
	state.common_include.Append(msvc_include);
	auto rc_base = state.windows_sdk_root + L"bin\\" + state.windows_sdk_version + L"\\";
	SafePointer< Array<string> > rc_list = IO::Search::GetFiles(rc_base + L"rc.exe", true);
	for (auto & rc : *rc_list) {
		int code;
		auto output = ExecuteAndGetOutput(rc_base + rc, &code);
		if (code == 0) { state.common_resource_compiler = rc_base + rc; break; }
	}
	console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
	if (!state.tools.Length()) {
		console << TextColor(Console::ColorRed) << L"No valid toolsets found." << TextColorDefault() << LineFeed();
		return false;
	}
	if (state.verbose) {
		console.SetTextColor(Console::ColorCyan);
		console.WriteLine(FormatString(L"The resource compiler is \"%0\"", state.common_resource_compiler));
		console.WriteLine(L"Include paths:");
		for (auto & i : state.common_include) console.WriteLine(L"  " + i);
		console.WriteLine(L"Architecture toolsets:");
		for (auto & ts : state.tools) {
			console.WriteLine(L"  Architecture: " + ts.arch);
			console.WriteLine(FormatString(L"    The compiler is \"%0\"", ts.compiler));
			console.WriteLine(FormatString(L"    The linker is \"%0\"", ts.linker));
			console.WriteLine(L"    Library paths:");
			for (auto & l : ts.lib) console.WriteLine(L"      " + l);
		}
		console.SetTextColor(Console::ColorDefault);
	}
	return true;
}
#endif

Toolset * FindToolset(const string & arch) { for (auto & ts : state.tools) if (ts.arch == arch) return &ts; return 0; }
void RegisterTarget(const string & target, const string & description, const string & cls)
{
	SafePointer<RegistryNode> targets = state.build_config->OpenNode(L"Targets");
	SafePointer<RegistryNode> tag = targets->OpenNode(target);
	if (!tag) {
		targets->CreateNode(target);
		tag = targets->OpenNode(target);
	}
	try { tag->CreateValue(L"Name", RegistryValueType::String); } catch (...) {}
	try { tag->CreateValue(L"Class", RegistryValueType::String); } catch (...) {}
	tag->SetValue(L"Name", description);
	tag->SetValue(L"Class", cls);
}
void SetDefaultTarget(const string & target)
{
	SafePointer<RegistryNode> targets = state.build_config->OpenNode(L"Targets");
	SafePointer<RegistryNode> tag = targets->OpenNode(target);
	auto cls = tag->GetValueString(L"Class");
	for (auto & v : targets->GetSubnodes()) if (string::CompareIgnoreCase(targets->GetValueString(v + L"/Class"), cls) == 0) {
		try { targets->SetValue(v + L"/Default", false); } catch (...) {}
	}
	try { tag->CreateValue(L"Default", RegistryValueType::Boolean); } catch (...) {}
	tag->SetValue(L"Default", true);
}
void RegisterTargets(Console & console)
{
	console.Write(L"Registering build targets...");
	auto sys_arch = GetSystemPlatform();
	#ifdef ENGINE_WINDOWS
	for (auto & ts : state.tools) {
		string name;
		if (ts.arch == L"X86") name = L"Intel x86";
		else if (ts.arch == L"X64") name = L"Intel x86-64";
		else name = ts.arch;
		RegisterTarget(ts.arch, name, L"arch");
		BuildVersion version_release, version_debug;
		version_release.arch = version_debug.arch = ts.arch;
		version_release.os = version_debug.os = state.current_os;
		version_release.successful = version_debug.successful = false;
		version_release.conf = L"Release";
		version_debug.conf = L"Debug";
		state.cache_versions.Append(version_release);
		state.cache_versions.Append(version_debug);
	}
	Toolset * ts;
	if (sys_arch == Platform::X86) {
		if (ts = FindToolset(L"X86")) { SetDefaultTarget(ts->arch); state.default_compiler_path = ts->compiler; }
	} else if (sys_arch == Platform::X64) {
		if (ts = FindToolset(L"X64")) { SetDefaultTarget(ts->arch); state.default_compiler_path = ts->compiler; }
		else if (ts = FindToolset(L"X86")) { SetDefaultTarget(ts->arch); state.default_compiler_path = ts->compiler; }
	} else if (sys_arch == Platform::ARM) {
		if (ts = FindToolset(L"ARM")) { SetDefaultTarget(ts->arch); state.default_compiler_path = ts->compiler; }
		else if (ts = FindToolset(L"X86")) { SetDefaultTarget(ts->arch); state.default_compiler_path = ts->compiler; }
	} else if (sys_arch == Platform::ARM64) {
		if (ts = FindToolset(L"ARM64")) { SetDefaultTarget(ts->arch); state.default_compiler_path = ts->compiler; }
		else if (ts = FindToolset(L"ARM")) { SetDefaultTarget(ts->arch); state.default_compiler_path = ts->compiler; }
		else if (ts = FindToolset(L"X86")) { SetDefaultTarget(ts->arch); state.default_compiler_path = ts->compiler; }
	}
	RegisterTarget(L"Windows", L"Windows", L"os");
	SetDefaultTarget(L"Windows");
	#endif
	#ifdef ENGINE_MACOSX
	RegisterTarget(L"X64", L"Intel x86-64", L"arch");
	RegisterTarget(L"ARM64", L"ARM64", L"arch");
	if (sys_arch == Platform::X64) SetDefaultTarget(L"X64");
	else if (sys_arch == Platform::ARM64) SetDefaultTarget(L"ARM64");
	RegisterTarget(L"MacOSX", L"Mac OS", L"os");
	SetDefaultTarget(L"MacOSX");
	BuildVersion version_x64_release, version_arm64_release, version_x64_debug, version_arm64_debug;
	version_x64_release.arch = version_x64_debug.arch = L"X64";
	version_arm64_release.arch = version_arm64_debug.arch = L"ARM64";
	version_x64_release.os = version_x64_debug.os = version_arm64_release.os = version_arm64_debug.os = state.current_os;
	version_x64_release.successful = version_x64_debug.successful = version_arm64_release.successful = version_arm64_debug.successful = false;
	version_x64_release.conf = version_arm64_release.conf = L"Release";
	version_x64_debug.conf = version_arm64_debug.conf = L"Debug";
	state.cache_versions.Append(version_x64_release);
	state.cache_versions.Append(version_arm64_release);
	state.cache_versions.Append(version_x64_debug);
	state.cache_versions.Append(version_arm64_debug);
	#endif
	RegisterTarget(L"Console", L"Console", L"subsys");
	RegisterTarget(L"GUI", L"Graphical", L"subsys");
	RegisterTarget(L"Library", L"Library", L"subsys");
	RegisterTarget(L"Silent", L"No user interface", L"subsys");
	SetDefaultTarget(L"Console");
	RegisterTarget(L"Release", L"Release", L"conf");
	RegisterTarget(L"Debug", L"Debug", L"conf");
	SetDefaultTarget(L"Release");
	console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
}
string ExpandPath(const string & path, const string & relative_to)
{
	if (path[0] == L'/' || (path[0] && path[1] == L':')) return IO::ExpandPath(path);
	else return IO::ExpandPath(relative_to + L"/" + path);
}
bool ParseCommandLine(Console & console)
{
	SafePointer< Array<string> > args = GetCommandLine();
	int i = 1;
	while (i < args->Length()) {
		auto & cmd = args->ElementAt(i);
		if (cmd[0] == L':' || cmd[0] == L'-') {
			i++;
			for (int j = 1; j < cmd.Length(); j++) {
				auto arg = cmd[j];
				if (arg == L'c') {
					state.configure_mode = true;
				} else if (arg == L'l') {
					state.clean = true;
				} else if (arg == L'o') {
					if (i < args->Length()) {
						state.object_path = args->ElementAt(i);
						i++;
					} else {
						console << TextColor(Console::ColorYellow) << L"Invalid command line: argument expected." << TextColorDefault() << LineFeed();
						return false;
					}
				} else if (arg == L'p') {
					state.preserve_mode = true;
				} else if (arg == L'r') {
					if (i < args->Length()) {
						state.runtime_path = args->ElementAt(i);
						i++;
					} else {
						console << TextColor(Console::ColorYellow) << L"Invalid command line: argument expected." << TextColorDefault() << LineFeed();
						return false;
					}
				} else if (arg == L'v') {
					state.verbose = true;
				} else {
					console << TextColor(Console::ColorYellow) << FormatString(L"Command line argument \"%0\" is invalid.", string(arg, 1)) << TextColorDefault() << LineFeed();
					return false;
				}
			}
		} else {
			console << TextColor(Console::ColorYellow) << FormatString(L"Unknown usage argument \"%0\".", args->ElementAt(i)) << TextColorDefault() << LineFeed();
			return false;
		}
	}
	return true;
}
bool LoadConfigurations(Console & console)
{
	string root = IO::Path::GetDirectory(IO::GetExecutablePath());
	int scs = 0, bcs = 0;
	console.Write(L"Loading common configuration files...");
	try {
		try {
			FileStream stream(root + L"/ertaconf.ecs", AccessRead, OpenExisting);
			state.self_config = LoadRegistry(&stream);
			if (!state.self_config) throw Exception();
			scs = 1;
		} catch (...) {
			FileStream stream(root + L"/ertaconf.ini", AccessRead, OpenExisting);
			state.self_config = CompileTextRegistry(&stream);
			if (!state.self_config) throw Exception();
			scs = 2;
		}
	} catch (...) { state.self_config = CreateRegistry(); }
	try {
		try {
			FileStream stream(root + L"/ertbuild.ecs", AccessRead, OpenExisting);
			state.build_config = LoadRegistry(&stream);
			if (!state.build_config) throw Exception();
			bcs = 1;
		} catch (...) {
			FileStream stream(root + L"/ertbuild.ini", AccessRead, OpenExisting);
			state.build_config = CompileTextRegistry(&stream);
			if (!state.build_config) throw Exception();
			bcs = 2;
		}
	} catch (...) { state.build_config = CreateRegistry(); }
	console.WriteLine(L"Done.");
	console.SetTextColor(Console::ColorYellow);
	console.WriteLine(FormatString(L"Automatic  configuration: %0", scs ? FormatString(L"PRESENT, %0", scs == 1 ? L"BINARY" : L"TEXT") : string(L"MISSING")));
	console.WriteLine(FormatString(L"Build tool configuration: %0", bcs ? FormatString(L"PRESENT, %0", bcs == 1 ? L"BINARY" : L"TEXT") : string(L"MISSING")));
	console.SetTextColor(Console::ColorDefault);
	state.self_config_text = scs != 1;
	state.build_config_text = bcs != 1;
	if (!state.runtime_path.Length()) state.runtime_path = state.self_config->GetValueString(L"Runtime");
	if (!state.object_path.Length()) state.object_path = state.self_config->GetValueString(L"Object");
	if (!state.runtime_path.Length()) {
		console << TextColor(Console::ColorRed) << L"The Runtime path is unknown, run with :r argument." << TextColorDefault() << LineFeed();
		return false;
	}
	if (!state.object_path.Length()) {
		console << TextColor(Console::ColorRed) << L"The object cache path is unknown, run with :o argument." << TextColorDefault() << LineFeed();
		return false;
	}
	if (state.verbose) {
		console.SetTextColor(Console::ColorCyan);
		console.WriteLine(FormatString(L"The Runtime path is \"%0\".", ExpandPath(state.runtime_path, root)));
		console.WriteLine(FormatString(L"The object cache path is \"%0\".", ExpandPath(state.object_path, root)));
		console.SetTextColor(Console::ColorDefault);
	}
	state.root_path = root;
	#ifdef ENGINE_WINDOWS
	state.current_os = L"Windows";
	#endif
	#ifdef ENGINE_MACOSX
	state.current_os = L"MacOSX";
	#endif
	if (state.preserve_mode) {
		Array<string> nodes_remove(0x10);
		for (auto & nn : state.build_config->GetSubnodes()) {
			if (string::CompareIgnoreCase(nn, L"Targets") == 0) continue;
			auto parts = nn.Split(L'-');
			bool remove = false;
			for (auto & p : parts) if (string::CompareIgnoreCase(p, state.current_os) == 0) { remove = true; break; }
			if (remove) nodes_remove.Append(nn);
		}
		for (auto & nn : nodes_remove) state.build_config->RemoveNode(nn);
	} else state.build_config = CreateRegistry();
	SafePointer<RegistryNode> targets_node = state.build_config->OpenNode(L"Targets");
	if (!targets_node) {
		state.build_config->CreateNode(L"Targets");
	}
	return true;
}
bool StoreConfigurations(Console & console)
{
	console.Write(L"Overwriting common configuration files...");
	try {
		if (state.self_config_text) {
			FileStream stream(state.root_path + L"/ertaconf.ini", AccessReadWrite, CreateAlways);
			RegistryToText(state.self_config, &stream, Encoding::UTF8);
		} else {
			FileStream stream(state.root_path + L"/ertaconf.ecs", AccessReadWrite, CreateAlways);
			state.self_config->Save(&stream);
		}
		if (state.build_config_text) {
			FileStream stream(state.root_path + L"/ertbuild.ini", AccessReadWrite, CreateAlways);
			RegistryToText(state.build_config, &stream, Encoding::UTF8);
		} else {
			FileStream stream(state.root_path + L"/ertbuild.ecs", AccessReadWrite, CreateAlways);
			state.build_config->Save(&stream);
		}
	} catch (...) {
		console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
		return false;
	}
	console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
	return true;
}
void BuildRuntimeCache(Console & console)
{
	console.WriteLine(L"Building the Runtime cache for the new targets.");
	for (auto & bv : state.cache_versions) {
		if (state.clean) bv.command_line << L":CNabco";
		else bv.command_line << L":Nabco";
		bv.command_line << bv.arch;
		bv.command_line << bv.conf;
		bv.command_line << bv.os;
	}
	int step = 1;
	for (auto & bv : state.cache_versions) {
		console << L"Runtime cache build step " << TextColor(Console::ColorGreen) << string(step) << TextColorDefault() << L" of " <<
			TextColor(Console::ColorMagenta) << string(state.cache_versions.Length()) << TextColorDefault() << LineFeed();
		SafePointer<Process> builder = CreateCommandProcess(L"ertbuild", &bv.command_line);
		if (builder) {
			builder->Wait();
			if (!builder->GetExitCode()) bv.successful = true;
		} else {
			console << TextColor(Console::ColorRed) << L"Error running the builder tool." << TextColorDefault() << LineFeed();
		}
		step++;
	}
	console.WriteLine(L"Runtime cache build report:");
	for (auto & bv : state.cache_versions) {
		auto descr = bv.arch + L", " + bv.conf;
		descr += string(L' ', 15 - descr.Length());
		console << TextColor(Console::ColorYellow) << descr << L": " << TextColorDefault();
		if (bv.successful) {
			console << TextColor(Console::ColorGreen) << L"Built successfully." << TextColorDefault();
		} else {
			console << TextColor(Console::ColorRed) << L"Built failed." << TextColorDefault();
		}
		console << LineFeed();
	}
	console.WriteLine(L"Automatic configuration finished.");
}

TargetSetConfig & CreateAtom(const string & arch, const string & os, const string & conf, const string & subs)
{
	state.atoms.Append(TargetSetConfig(arch, os, conf, subs));
	return state.atoms.LastElement();
}
void GenerateAtomsWindows(Console & console)
{
	auto & windows = CreateAtom(L"", state.current_os, L"", L"");
	windows.path_runtime = state.runtime_path;
	windows.source_filter = L"*.c;*.cpp;*.cxx";
	windows.bootstrapper = L"bootstrapper.cpp";
	windows.extension_object = L"obj";
	windows.compiler.argument_define = L"/D";
	windows.compiler.argument_include = L"/I";
	windows.compiler.argument_output = L"/Fo$";
	windows.compiler.arguments << L"/c";
	windows.compiler.arguments << L"/GS";
	windows.compiler.arguments << L"/W3";
	windows.compiler.arguments << L"/Gm-";
	windows.compiler.arguments << L"/WX-";
	windows.compiler.arguments << L"/Gd";
	windows.compiler.arguments << L"/Oy-";
	windows.compiler.arguments << L"/Zc:wchar_t";
	windows.compiler.arguments << L"/Zc:forScope";
	windows.compiler.arguments << L"/Zc:inline";
	windows.compiler.arguments << L"/fp:precise";
	windows.compiler.arguments << L"/EHsc";
	windows.compiler.arguments << L"/DWIN32";
	windows.compiler.arguments << L"/D_UNICODE";
	windows.compiler.arguments << L"/DUNICODE";
	for (auto & i : state.common_include) windows.compiler.arguments << L"/I" + i;
	windows.linker.argument_output = L"/OUT:$";
	windows.linker.arguments << L"/LTCG:INCREMENTAL";
	windows.linker.arguments << L"/NXCOMPAT";
	windows.linker.arguments << L"/DYNAMICBASE";
	windows.linker.arguments << L"kernel32.lib";
	windows.linker.arguments << L"user32.lib";
	windows.linker.arguments << L"gdi32.lib";
	windows.linker.arguments << L"winspool.lib";
	windows.linker.arguments << L"comdlg32.lib";
	windows.linker.arguments << L"advapi32.lib";
	windows.linker.arguments << L"shell32.lib";
	windows.linker.arguments << L"ole32.lib";
	windows.linker.arguments << L"oleaut32.lib";
	windows.linker.arguments << L"uuid.lib";
	windows.linker.arguments << L"odbc32.lib";
	windows.linker.arguments << L"odbccp32.lib";
	windows.resource.command = L"ertres.exe";
	windows.resource.redefine_link = L"$object$/$output$.res";
	windows.resource.is_windows = true;
	windows.resource.icon_codec = L"ICO";
	windows.resource.icon_extension = L"ico";
	windows.resource.icon_sizes << 16;
	windows.resource.icon_sizes << 24;
	windows.resource.icon_sizes << 32;
	windows.resource.icon_sizes << 48;
	windows.resource.icon_sizes << 64;
	windows.resource.icon_sizes << 256;
	windows.resource.compiler.command = state.common_resource_compiler;
	windows.resource.compiler.argument_output = L"/fo$";
	windows.resource.compiler.arguments << L"/r";
	for (auto & i : state.common_include) windows.resource.compiler.arguments << L"/I" + i;
	windows.defines << L"ENGINE_WINDOWS";
	for (auto & ts : state.tools) {
		auto & arch = CreateAtom(ts.arch, state.current_os, L"", L"");
		arch.compiler.command = ts.compiler;
		arch.linker.command = ts.linker;
		arch.linker.arguments << L"/MACHINE:" + ts.arch;
		for (auto & lib : ts.lib) arch.linker.arguments << L"/LIBPATH:" + lib;
		if (ts.arch == L"X64" || ts.arch == L"ARM64") arch.defines << L"ENGINE_X64";
		if (ts.arch == L"ARM" || ts.arch == L"ARM64") arch.defines << L"ENGINE_ARM";
	}
	auto & windows_release = CreateAtom(L"", state.current_os, L"Release", L"");
	auto & windows_debug = CreateAtom(L"", state.current_os, L"Debug", L"");
	windows_release.compiler.arguments << L"/GL";
	windows_release.compiler.arguments << L"/Gy";
	windows_release.compiler.arguments << L"/O2";
	windows_release.compiler.arguments << L"/Oi";
	windows_release.compiler.arguments << L"/MT";
	windows_release.compiler.arguments << L"/errorReport:none";
	windows_release.compiler.arguments << L"/DNDEBUG";
	windows_release.linker.arguments << L"/OPT:REF";
	windows_release.linker.arguments << L"/OPT:ICF";
	windows_release.linker.arguments << L"/ERRORREPORT:NONE";
	windows_debug.compiler.arguments << L"/Z7";
	windows_debug.compiler.arguments << L"/RTC1";
	windows_debug.compiler.arguments << L"/Od";
	windows_debug.compiler.arguments << L"/FC";
	windows_debug.compiler.arguments << L"/MDd";
	windows_debug.compiler.arguments << L"/errorReport:prompt";
	windows_debug.compiler.arguments << L"/D_DEBUG";
	windows_debug.compiler.arguments << L"/diagnostics:column";
	windows_debug.linker.arguments << L"/DEBUG";
	windows_debug.linker.arguments << L"/ERRORREPORT:PROMPT";
	windows_debug.defines << L"ENGINE_DEBUG";
	auto & windows_console = CreateAtom(L"", state.current_os, L"", L"Console");
	auto & windows_gui = CreateAtom(L"", state.current_os, L"", L"GUI");
	auto & windows_library = CreateAtom(L"", state.current_os, L"", L"Library");
	auto & windows_silent = CreateAtom(L"", state.current_os, L"", L"Silent");
	windows_console.extension_executable = L"exe";
	windows_gui.extension_executable = L"exe";
	windows_library.extension_executable = L"dll";
	windows_silent.extension_executable = L"exe";
	windows_console.defines << L"ENGINE_SUBSYSTEM_CONSOLE";
	windows_gui.defines << L"ENGINE_SUBSYSTEM_GUI";
	windows_library.defines << L"ENGINE_SUBSYSTEM_LIBRARY";
	windows_silent.defines << L"ENGINE_SUBSYSTEM_SILENT";
	windows_library.linker.arguments << L"/DLL";
	windows_library.resource.is_library = true;
	for (auto & ts : state.tools) {
		string ver;
		if (ts.arch == L"X86") ver = L",5.01";
		else if (ts.arch == L"X64") ver = L",6.00";
		auto & arch_console = CreateAtom(ts.arch, state.current_os, L"", L"Console");
		auto & arch_gui = CreateAtom(ts.arch, state.current_os, L"", L"GUI");
		auto & arch_library = CreateAtom(ts.arch, state.current_os, L"", L"Library");
		auto & arch_silent = CreateAtom(ts.arch, state.current_os, L"", L"Silent");
		arch_console.linker.arguments << L"/SUBSYSTEM:CONSOLE" + ver;
		arch_gui.linker.arguments << L"/SUBSYSTEM:WINDOWS" + ver;
		arch_library.linker.arguments << L"/SUBSYSTEM:WINDOWS" + ver;
		arch_silent.linker.arguments << L"/SUBSYSTEM:WINDOWS" + ver;
	}
	for (auto & ts : state.tools) {
		auto & arch_release = CreateAtom(ts.arch, state.current_os, L"Release", L"");
		auto & arch_debug = CreateAtom(ts.arch, state.current_os, L"Debug", L"");
		arch_release.path_object = state.object_path + L"/" + state.current_os.LowerCase() + L"_" + ts.arch.LowerCase() + L"_release";
		arch_debug.path_object = state.object_path + L"/" + state.current_os.LowerCase() + L"_" + ts.arch.LowerCase() + L"_debug";
		if (ts.arch == L"X86") {
			arch_release.linker.arguments << L"/SAFESEH";
			arch_debug.compiler.arguments << L"/analyze-";
		}
	}
}
void GenerateAtomsMacOSX(Console & console)
{
	auto & mac = CreateAtom(L"", state.current_os, L"", L"");
	mac.path_runtime = state.runtime_path;
	mac.source_filter = L"*.c;*.cpp;*.cxx;*.m;*.mm";
	mac.bootstrapper = L"bootstrapper.cpp";
	mac.extension_object = L"o";
	mac.compiler.command = L"clang++";
	mac.compiler.argument_define = L"-D";
	mac.compiler.argument_include = L"-I";
	mac.compiler.argument_output = L"-o";
	mac.compiler.arguments << L"-c";
	mac.compiler.arguments << L"-std=c++17";
	mac.compiler.arguments << L"-fmodules";
	mac.compiler.arguments << L"-fcxx-modules";
	mac.linker.command = L"clang++";
	mac.linker.argument_output = L"-o";
	mac.defines << L"ENGINE_UNIX";
	mac.defines << L"ENGINE_MACOSX";
	auto & mac_x64 = CreateAtom(L"X64", state.current_os, L"", L"");
	mac_x64.compiler.arguments << L"--target=x86_64-apple-darwin20.1.0";
	mac_x64.linker.arguments << L"--target=x86_64-apple-darwin20.1.0";
	mac_x64.defines << L"ENGINE_X64";
	auto & mac_arm64 = CreateAtom(L"ARM64", state.current_os, L"", L"");
	mac_arm64.compiler.arguments << L"--target=arm64-apple-macos11";
	mac_arm64.linker.arguments << L"--target=arm64-apple-macos11";
	mac_arm64.defines << L"ENGINE_X64";
	mac_arm64.defines << L"ENGINE_ARM";
	auto & mac_release = CreateAtom(L"", state.current_os, L"Release", L"");
	mac_release.compiler.arguments << L"-O3";
	mac_release.compiler.arguments << L"-fvisibility=hidden";
	mac_release.linker.arguments << L"-O3";
	mac_release.linker.arguments << L"-fvisibility=hidden";
	auto & mac_debug = CreateAtom(L"", state.current_os, L"Debug", L"");
	mac_debug.compiler.arguments << L"-O0";
	mac_debug.compiler.arguments << L"-g";
	mac_debug.linker.arguments << L"-O0";
	mac_debug.linker.arguments << L"-g";
	mac_debug.defines << L"ENGINE_DEBUG";
	auto & mac_console = CreateAtom(L"", state.current_os, L"", L"Console");
	mac_console.defines << L"ENGINE_SUBSYSTEM_CONSOLE";
	auto & mac_gui = CreateAtom(L"", state.current_os, L"", L"GUI");
	mac_gui.defines << L"ENGINE_SUBSYSTEM_GUI";
	mac_gui.extension_executable = L"app";
	mac_gui.resource.command = L"ertres";
	mac_gui.resource.redefine_output = L"$target$/$output$.app/Contents/MacOS/$internal$";
	mac_gui.resource.is_macosx = true;
	mac_gui.resource.icon_codec = L"ICNS";
	mac_gui.resource.icon_extension = L"icns";
	mac_gui.resource.icon_sizes << 16;
	mac_gui.resource.icon_sizes << 32;
	mac_gui.resource.icon_sizes << 64;
	mac_gui.resource.icon_sizes << 128;
	mac_gui.resource.icon_sizes << 256;
	mac_gui.resource.icon_sizes << 512;
	mac_gui.resource.icon_sizes << 1024;
	auto & mac_library = CreateAtom(L"", state.current_os, L"", L"Library");
	mac_library.defines << L"ENGINE_SUBSYSTEM_LIBRARY";
	mac_library.extension_executable = L"dylib";
	mac_library.linker.arguments << L"-dynamiclib";
	auto & mac_silent = CreateAtom(L"", state.current_os, L"", L"Silent");
	mac_silent.defines << L"ENGINE_SUBSYSTEM_SILENT";
	auto & mac_x64_release = CreateAtom(L"X64", state.current_os, L"Release", L"");
	mac_x64_release.path_object = state.object_path + L"/" + state.current_os.LowerCase() + L"_x64_release";
	auto & mac_arm64_release = CreateAtom(L"ARM64", state.current_os, L"Release", L"");
	mac_arm64_release.path_object = state.object_path + L"/" + state.current_os.LowerCase() + L"_arm64_release";
	auto & mac_x64_debug = CreateAtom(L"X64", state.current_os, L"Debug", L"");
	mac_x64_debug.path_object = state.object_path + L"/" + state.current_os.LowerCase() + L"_x64_debug";
	auto & mac_arm64_debug = CreateAtom(L"ARM64", state.current_os, L"Debug", L"");
	mac_arm64_debug.path_object = state.object_path + L"/" + state.current_os.LowerCase() + L"_arm64_debug";
}
void GenerateAtoms(Console & console)
{
	console.Write(L"Creating configuration partitions...");
	#ifdef ENGINE_WINDOWS
	GenerateAtomsWindows(console);
	#endif
	#ifdef ENGINE_MACOSX
	GenerateAtomsMacOSX(console);
	#endif
	console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
}
void PutConfigurationChanges(Console & console)
{
	console.Write(L"Implementing changes to the configuration...");
	for (auto & a : state.atoms) a.Serialize(state.build_config);
	try { state.self_config->CreateValue(L"Runtime", RegistryValueType::String); } catch (...) {}
	try { state.self_config->CreateValue(L"Object", RegistryValueType::String); } catch (...) {}
	try { state.self_config->CreateNode(L"AvailableArchitectures"); } catch (...) {}
	#ifdef ENGINE_WINDOWS
	try { state.self_config->CreateValue(L"SDK", RegistryValueType::String); } catch (...) {}
	try { state.self_config->CreateValue(L"Compiler", RegistryValueType::String); } catch (...) {}
	#endif
	state.self_config->SetValue(L"Runtime", state.runtime_path);
	state.self_config->SetValue(L"Object", state.object_path);
	#ifdef ENGINE_WINDOWS
	state.self_config->SetValue(L"SDK", state.windows_sdk_version);
	state.self_config->SetValue(L"Compiler", state.default_compiler_path);
	#endif
	SafePointer<RegistryNode> aa = state.self_config->OpenNode(L"AvailableArchitectures");
	#ifdef ENGINE_WINDOWS
	for (auto & ts : state.tools) {
		try { aa->CreateValue(ts.arch, RegistryValueType::Boolean); } catch (...) {}
		aa->SetValue(ts.arch, true);
	}
	#endif
	#ifdef ENGINE_MACOSX
	try { aa->CreateValue(L"X64", RegistryValueType::Boolean); } catch (...) {}
	aa->SetValue(L"X64", true);
	try { aa->CreateValue(L"ARM64", RegistryValueType::Boolean); } catch (...) {}
	aa->SetValue(L"ARM64", true);
	#endif
	console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
}

int Main(void)
{
	stdout_clone = IO::CloneHandle(IO::GetStandardOutput());
	stderr_clone = IO::CloneHandle(IO::GetStandardError());
	stdin_clone = IO::CloneHandle(IO::GetStandardInput());
	Console console(stdout_clone, stdin_clone);
	try {
		if (!ParseCommandLine(console)) return 1;
		if (state.configure_mode) {
			if (!LoadConfigurations(console)) return 1;
			#ifdef ENGINE_WINDOWS
			if (!SearchForWindowsToolsets(console)) return 1;
			#endif
			RegisterTargets(console);
			GenerateAtoms(console);
			PutConfigurationChanges(console);
			if (!StoreConfigurations(console)) return 1;
			BuildRuntimeCache(console);
		} else {
			console << ENGINE_VI_APPNAME << LineFeed();
			console << L"Copyright " << string(ENGINE_VI_COPYRIGHT).Replace(L'\xA9', L"(C)") << LineFeed();
			console << L"Version " << ENGINE_VI_APPVERSION << L", build " << ENGINE_VI_BUILD << LineFeed() << LineFeed();
			console << L"Command line syntax:" << LineFeed();
			console << L"  " << ENGINE_VI_APPSYSNAME << L" :cloprv" << LineFeed();
			console << L"The list of valid command line arguments:" << LineFeed();
			console << L"  :c - configure the toolset, the necessary argument," << LineFeed();
			console << L"  :l - use clean Runtime rebuild," << LineFeed();
			console << L"  :o - set the object cache directory (directory as the next argument)," << LineFeed();
			console << L"  :p - preserve custom targets in the current configuration," << LineFeed();
			console << L"  :r - set the Runtime source directory (directory as the next argument)," << LineFeed();
			console << L"  :v - turn on verbose mode." << LineFeed();
			#ifdef ENGINE_WINDOWS
			console << L"The presence of both Windows 10 SDK and Visual C++ Build Tools is necessary." << LineFeed();
			#endif
			#ifdef ENGINE_MACOSX
			console << L"The presence of Xcode is necessary." << LineFeed();
			#endif
			console << LineFeed();
		}
	} catch (Exception & e) {
		console << TextColor(Console::ColorRed) << FormatString(L"Configurator failed: %0.", e.ToString()) << TextColorDefault() << LineFeed();
		return 1;
	} catch (...) {
		console << TextColor(Console::ColorRed) << L"Configurator failed: Unknown exception." << TextColorDefault() << LineFeed();
		return 1;
	}
	return 0;
}