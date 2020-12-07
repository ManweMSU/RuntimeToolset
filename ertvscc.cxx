#include "ertcom.h"

using namespace Engine::Reflection;

#include "ertvscc.h"

enum class InterfaceMode { Console, Library, Other };

struct {
	bool create_workspace = false;
	bool common_environment = false;
	SafePointer<Registry> registry;
} config_state;

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
				} else if (arg == L'a') {
					if (i < args->Length()) {
						int error = SelectTarget(args->ElementAt(i), BuildTargetClass::Architecture, console);
						if (error) return error;
						i++;
					} else {
						console << TextColor(Console::ColorYellow) << L"Invalid command line: argument expected." << TextColorDefault() << LineFeed();
						return ERTBT_INVALID_COMMAND_LINE;
					}
				} else if (arg == L'c') {
					if (i < args->Length()) {
						int error = SelectTarget(args->ElementAt(i), BuildTargetClass::Configuration, console);
						if (error) return error;
						i++;
					} else {
						console << TextColor(Console::ColorYellow) << L"Invalid command line: argument expected." << TextColorDefault() << LineFeed();
						return ERTBT_INVALID_COMMAND_LINE;
					}
				} else if (arg == L'd') {
					int error = SelectTarget(L"debug", BuildTargetClass::Configuration, console);
					if (error) return error;
				} else if (arg == L'e') {
					config_state.common_environment = true;
				} else if (arg == L'i') {
					if (i < args->Length()) {
						state.extra_include << args->ElementAt(i);
						i++;
					} else {
						console << TextColor(Console::ColorYellow) << L"Invalid command line: argument expected." << TextColorDefault() << LineFeed();
						return ERTBT_INVALID_COMMAND_LINE;
					}
				} else if (arg == L'r') {
					int error = SelectTarget(L"release", BuildTargetClass::Configuration, console);
					if (error) return error;
				} else if (arg == L'w') {
					config_state.create_workspace = true;
				} else {
					console << TextColor(Console::ColorYellow) << FormatString(L"Command line argument \"%0\" is invalid.", string(arg, 1)) << TextColorDefault() << LineFeed();
					return ERTBT_INVALID_COMMAND_LINE;
				}
			}
		} else {
			if (state.project_file_path.Length()) {
				console << TextColor(Console::ColorYellow) << L"Duplicate input file argument on command line." << TextColorDefault() << LineFeed();
				return ERTBT_DUPLICATE_INPUT_FILE;
			}
			state.project_file_path = IO::ExpandPath(cmd);
			i++;
		}
	}
	return ERTBT_SUCCESS;
}
bool LoadGeneratorConfigurations(Console & console)
{
	auto root = IO::Path::GetDirectory(IO::GetExecutablePath());
	try {
		try {
			FileStream stream(root + L"/ertaconf.ecs", AccessRead, OpenExisting);
			config_state.registry = LoadRegistry(&stream);
			if (!config_state.registry) throw Exception();
		} catch (...) {
			FileStream stream(root + L"/ertaconf.ini", AccessRead, OpenExisting);
			config_state.registry = CompileTextRegistry(&stream);
			if (!config_state.registry) throw Exception();
		}
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed to load the target set." << TextColorDefault() << LineFeed();
		return false;
	}
	return true;
}
InterfaceMode GetCurrentInterface(void)
{
	if (string::CompareIgnoreCase(state.subsys.Name, L"Console") == 0) return InterfaceMode::Console;
	else if (string::CompareIgnoreCase(state.subsys.Name, L"Library") == 0) return InterfaceMode::Library;
	else return InterfaceMode::Other;
}
string ExecuteAndGetOutput(const string & exe, const Array<string> & args, int * exit_code = 0)
{
	handle pipe_in, pipe_out;
	IO::CreatePipe(&pipe_in, &pipe_out);
	IO::SetStandardOutput(pipe_in);
	IO::SetStandardError(pipe_in);
	SafePointer<Process> process = CreateCommandProcess(exe, &args);
	IO::CloseFile(pipe_in);
	IO::SetStandardOutput(state.stdout_clone);
	IO::SetStandardError(state.stderr_clone);
	if (!process) { IO::CloseFile(pipe_out); if (exit_code) *exit_code = -1; return L""; }
	process->Wait();
	if (exit_code) *exit_code = process->GetExitCode();
	FileStream stream(pipe_out, true);
	TextReader reader(&stream, Encoding::UTF8);
	return reader.ReadLine();
}

int CreateWorkspace(Console & console)
{
	if (!state.silent) console.Write(L"Creating the workspace...");
	try {
		FileStream stream(state.project_root_path + L"/" + IO::Path::GetFileNameWithoutExtension(state.project_file_path) + L".code-workspace", AccessReadWrite, CreateAlways);
		TextWriter writer(&stream, Encoding::UTF8);
		writer.WriteLine(L"{");
		writer.WriteLine(L"\t\"folders\": [");
		writer.WriteLine(L"\t\t{");
		writer.WriteLine(L"\t\t\t\"path\": \".\"");
		writer.WriteLine(L"\t\t}");
		writer.WriteLine(L"\t],");
		writer.WriteLine(L"\t\"settings\": {}");
		writer.Write(L"}");
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
		return ERTBT_UNCOMMON_EXCEPTION;
	}
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
	return ERTBT_SUCCESS;
}
int CreateEnvironment(Console & console)
{
	if (!state.silent) console.Write(L"Creating the environment configuration...");
	try {
		string part_name;
		#ifdef ENGINE_WINDOWS
		part_name = L"Win32";
		#endif
		#ifdef ENGINE_MACOSX
		part_name = L"Mac";
		#endif
		FileStream stream(state.project_root_path + L"/.vscode/c_cpp_properties.json", AccessReadWrite, OpenAlways);
		Configurations configs;
		JsonSerializer serializer(&stream);
		serializer.DeserializeObject(configs);
		Configuration * config = 0;
		for (auto & c : configs.configurations.InnerArray) if (c.name == part_name) { config = &c; break; }
		if (!config) {
			configs.version = 4;
			configs.configurations.AppendNew();
			config = &configs.configurations.InnerArray.LastElement();
			config->name = part_name;
		}
		state.extra_include << state.runtime_source_path;
		config->includePath.Clear();
		config->browse.path.Clear();
		config->includePath << L"${workspaceFolder}/**";
		for (auto & inc : state.extra_include) {
			auto norm = inc.Replace(L'\\', L'/');
			config->includePath << norm;
			config->browse.path << norm + L"/*";
		}
		config->defines.Clear();
		config->defines << L"UNICODE=1";
		config->defines << L"_UNICODE=1";
		SafePointer<RegistryNode> ld = local_config->OpenNode(L"Defines");
		if (ld) for (auto & v : ld->GetValues()) config->defines << v + L"=1";
		#ifdef ENGINE_WINDOWS
		auto cc = local_config->GetValueString(L"Compiler/Path");
		auto sdk = config_state.registry->GetValueString(L"SDK");
		config->macFrameworkPath.Clear();
		config->compilerPath = cc;
		config->windowsSdkVersion = sdk;
		config->intelliSenseMode = L"msvc-x64";
		#endif
		#ifdef ENGINE_MACOSX
		config->macFrameworkPath.Clear();
		config->macFrameworkPath << L"/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks";
		config->compilerPath = L"/usr/bin/clang";
		config->intelliSenseMode = L"clang-x64";
		#endif
		config->cStandard = L"c11";
		config->cppStandard = L"c++17";
		if (state.version_information.CreateVersionDefines) {
			config->defines << L"ENGINE_VI_APPNAME=L\"" + EscapeString(state.version_information.ApplicationName) + L"\"";
			config->defines << L"ENGINE_VI_COMPANY=L\"" + EscapeString(state.version_information.CompanyName) + L"\"";
			config->defines << L"ENGINE_VI_COPYRIGHT=L\"" + EscapeString(state.version_information.Copyright) + L"\"";
			config->defines << L"ENGINE_VI_APPSYSNAME=L\"" + EscapeString(state.version_information.InternalName) + L"\"";
			config->defines << L"ENGINE_VI_APPIDENT=L\"" + EscapeString(state.version_information.ApplicationIdentifier) + L"\"";
			config->defines << L"ENGINE_VI_COMPANYIDENT=L\"" + EscapeString(state.version_information.CompanyIdentifier) + L"\"";
			config->defines << L"ENGINE_VI_DESCRIPTION=L\"" + EscapeString(state.version_information.ApplicationDescription) + L"\"";
			config->defines << L"ENGINE_VI_APPVERSION=L\"" +
				string(state.version_information.VersionMajor) + L"." +
				string(state.version_information.VersionMinor) + L"." +
				string(state.version_information.Subversion) + L"." +
				string(state.version_information.Build) + L"\"";
			config->defines << L"ENGINE_VI_APPSHORTVERSION=L\"" +
				string(state.version_information.VersionMajor) + L"." +
				string(state.version_information.VersionMinor) + L"\"";
			config->defines << L"ENGINE_VI_VERSIONMAJOR=" + string(state.version_information.VersionMajor);
			config->defines << L"ENGINE_VI_VERSIONMINOR=" + string(state.version_information.VersionMinor);
			config->defines << L"ENGINE_VI_SUBVERSION=" + string(state.version_information.Subversion);
			config->defines << L"ENGINE_VI_BUILD=" + string(state.version_information.Build);
		}
		stream.Seek(0, Begin);
		stream.SetLength(0);
		serializer.SerializeObject(configs);
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
		return ERTBT_UNCOMMON_EXCEPTION;
	}
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
	return ERTBT_SUCCESS;
}
void CreateBuildTask(BuildTasks & tasks, const string & arch, const string & os, const string & conf)
{
	auto arch_info = FindTarget(arch, BuildTargetClass::Architecture);
	auto os_info = FindTarget(os, BuildTargetClass::OperatingSystem);
	auto conf_info = FindTarget(conf, BuildTargetClass::Configuration);
	if (!arch_info || !os_info || !conf_info) return;
	auto forced_arch = GeneralCheckForForcedArchitecture(arch_info->Name, os_info->Name, conf_info->Name, state.subsys.Name);
	if (forced_arch.Length() && string::CompareIgnoreCase(forced_arch, arch_info->Name)) return;
	auto name = FormatString(L"Build (%0, %1, %2)", os_info->HumanReadableName, arch_info->HumanReadableName, conf_info->HumanReadableName);
	BuildTask * task = 0;
	for (auto & t : tasks.tasks.InnerArray) if (t.label == name) { task = &t; break; }
	if (!task) {
		tasks.version = L"2.0.0";
		tasks.tasks.AppendNew();
		task = &tasks.tasks.InnerArray.LastElement();
		task->label = name;
	}
	task->command = L"ertbuild";
	task->args.Clear();
	task->args << L"${workspaceFolder}/" + IO::Path::GetFileName(state.project_file_path);
	task->args << L":Naco";
	task->args << arch_info->Name.LowerCase();
	task->args << conf_info->Name.LowerCase();
	task->args << os_info->Name.LowerCase();
	task->type = L"shell";
	task->group.kind = L"build";
	task->group.isDefault = true;
}
int CreateBuildTasks(Console & console)
{
	if (!state.silent) console.Write(L"Creating the build tasks...");
	try {
		FileStream stream(state.project_root_path + L"/.vscode/tasks.json", AccessReadWrite, OpenAlways);
		BuildTasks tasks;
		JsonSerializer serializer(&stream);
		serializer.DeserializeObject(tasks);
		SafePointer<RegistryNode> archs = config_state.registry->OpenNode(L"AvailableArchitectures");
		if (archs) for (auto & v : archs->GetValues()) if (archs->GetValueBoolean(v)) {
			CreateBuildTask(tasks, v, state.os.Name, L"Release");
			CreateBuildTask(tasks, v, state.os.Name, L"Debug");
		}
		stream.Seek(0, Begin);
		stream.SetLength(0);
		serializer.SerializeObject(tasks);
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
		return ERTBT_UNCOMMON_EXCEPTION;
	}
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
	return ERTBT_SUCCESS;
}
int CreateLaunchTask(Console & console)
{
	auto mode = GetCurrentInterface();
	if (mode == InterfaceMode::Library) return ERTBT_SUCCESS;
	if (!state.silent) console.Write(L"Creating the launch task...");
	try {
		auto name = FormatString(L"Launch (%0)", state.os.HumanReadableName);
		FileStream stream(state.project_root_path + L"/.vscode/launch.json", AccessReadWrite, OpenAlways);
		LaunchList list;
		JsonSerializer serializer(&stream);
		serializer.DeserializeObject(list);
		LaunchTask * task = 0;
		for (auto & t : list.configurations.InnerArray) if (t.name == name) { task = &t; break; }
		if (!task) {
			list.version = L"0.2.0";
			list.configurations.AppendNew();
			task = &list.configurations.InnerArray.LastElement();
			task->name = name;
		}
		int exit_code;
		Array<string> args(0x10);
		args << state.project_file_path;
		args << L":OSaco";
		args << state.arch.Name;
		args << state.conf.Name;
		args << state.os.Name;
		auto app_path = ExecuteAndGetOutput(L"ertbuild", args, &exit_code);
		if (exit_code) {
			if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
			if (exit_code > 0) return exit_code;
			else return ERTBT_UNCOMMON_EXCEPTION;
		}
		task->request = L"launch";
		task->program = app_path;
		task->cwd = IO::Path::GetDirectory(app_path);
		task->stopAtEntry = false;
		task->externalConsole = (mode == InterfaceMode::Console) ? true : false;
		#ifdef ENGINE_WINDOWS
		task->type = L"cppvsdbg";
		task->MIMode = L"";
		#endif
		#ifdef ENGINE_MACOSX
		task->type = L"cppdbg";
		task->MIMode = L"lldb";
		#endif
		stream.Seek(0, Begin);
		stream.SetLength(0);
		serializer.SerializeObject(list);
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed." << TextColorDefault() << LineFeed();
		return ERTBT_UNCOMMON_EXCEPTION;
	}
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed." << TextColorDefault() << LineFeed();
	return ERTBT_SUCCESS;
}

int Main(void)
{
	state.stdout_clone = IO::CloneHandle(IO::GetStandardOutput());
	state.stderr_clone = IO::CloneHandle(IO::GetStandardError());
	Console console(state.stdout_clone);
	try {
		int error = ConfigurationInitialize(console);
		if (error) return error;
		error = ParseCommandLine(console);
		if (error) return error;
		#ifdef ENGINE_WINDOWS
		error = SelectTarget(L"Windows", BuildTargetClass::OperatingSystem, console);
		#endif
		#ifdef ENGINE_MACOSX
		error = SelectTarget(L"MacOSX", BuildTargetClass::OperatingSystem, console);
		#endif
		if (error) return error;
		if (!state.nologo && !state.silent) {
			console << ENGINE_VI_APPNAME << LineFeed();
			console << L"Copyright " << string(ENGINE_VI_COPYRIGHT).Replace(L'\xA9', L"(C)") << LineFeed();
			console << L"Version " << ENGINE_VI_APPVERSION << L", build " << ENGINE_VI_BUILD << LineFeed() << LineFeed();
		}
		if (state.project_file_path.Length()) {
			if (!LoadGeneratorConfigurations(console)) return ERTBT_UNCOMMON_EXCEPTION;
			if (config_state.common_environment) {
				state.project_root_path = state.project_file_path;
				state.project_file_path = IO::ExpandPath(state.project_root_path + L"/workspace.file");
			} else {
				error = LoadProject(console);
				if (error) return error;
			}
			error = MakeLocalConfiguration(console);
			if (error) return error;
			if (!config_state.common_environment) {
				error = LoadVersionInformation(console);
				if (error) return error;
				ProjectPostConfig();
			}
			IO::CreateDirectoryTree(state.project_root_path + "/.vscode");
			if (config_state.create_workspace) {
				error = CreateWorkspace(console);
				if (error) return error;
			}
			error = CreateEnvironment(console);
			if (error) return error;
			if (!config_state.common_environment) {
				error = CreateBuildTasks(console);
				if (error) return error;
				return CreateLaunchTask(console);
			}
		} else if (!state.silent) {
			console << L"Command line syntax:" << LineFeed();
			console << L"  " << ENGINE_VI_APPSYSNAME << L" <project> :NSacdeirw" << LineFeed();
			console << L"Where \"project\" is the project configuration file." << LineFeed();
			console << L"You can optionally use the next options:" << LineFeed();
			console << L"  :N - use no logo mode - don't print application logo," << LineFeed();
			console << L"  :S - use silent mode - supress any output," << LineFeed();
			console << L"  :a - specify processor architecture (as the next argument)," << LineFeed();
			console << L"  :c - specify target configuration (as the next argument)," << LineFeed();
			console << L"  :d - use debug mode configuration," << LineFeed();
			console << L"  :e - create the environment only (then project is a directory)," << LineFeed();
			console << L"  :i - adds custom include path (as the next argument)," << LineFeed();
			console << L"  :r - use release mode configuration," << LineFeed();
			console << L"  :w - create the workspace file," << LineFeed();
			console << L"The target configuration passed is used only for setting the environment" << LineFeed();
			console << L"and the run target. Build tasks are created for all available targets." << LineFeed();
			console << LineFeed();
		}
	} catch (Exception & e) {
		if (!state.silent) console << TextColor(Console::ColorRed) << FormatString(L"Configuration generator failed: %0.", e.ToString()) << TextColorDefault() << LineFeed();
		return ERTBT_COMMON_EXCEPTION;
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Configuration generator failed: Unknown exception." << TextColorDefault() << LineFeed();
		return ERTBT_UNCOMMON_EXCEPTION;
	}
	return ERTBT_SUCCESS;
}