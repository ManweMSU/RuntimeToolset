#include "ertcom.h"

int CompileSource(const string & source, const string & object, const string & log, Console & console)
{
	if (!state.clean) {
		try {
			FileStream src(source, AccessRead, OpenExisting);
			FileStream out(object, AccessRead, OpenExisting);
			auto src_time = IO::DateTime::GetFileAlterTime(src.Handle());
			auto out_time = IO::DateTime::GetFileAlterTime(out.Handle());
			if (out_time > src_time && out_time > state.project_time) return ERTBT_SUCCESS;
			if (out_time < src_time && !state.silent) {
				console << TextColor(Console::ColorCyan) << IO::Path::GetFileName(source) << TextColorDefault() << L" renewed (" <<
					TextColor(Console::ColorGreen) << src_time.ToLocal().ToString() << TextColorDefault() << L" against " <<
					TextColor(Console::ColorRed) << out_time.ToLocal().ToString() << TextColorDefault() << L")." << LineFeed();
			}
		} catch (...) {}
	}
	auto da = local_config->GetValueString(L"Compiler/DefineArgument");
	auto ia = local_config->GetValueString(L"Compiler/IncludeArgument");
	auto oa = local_config->GetValueString(L"Compiler/OutputArgument");
	auto cc = local_config->GetValueString(L"Compiler/Path");
	if (!cc.Length()) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"No compiler set for current configuration." << TextColorDefault() << LineFeed();
		return ERTBT_INVALID_COMPILER_SET;
	}
	if (!state.silent) console << L"Compiling " << TextColor(Console::ColorCyan) << IO::Path::GetFileName(source) << TextColorDefault() << L"...";
	Array<string> cc_args(0x80);
	cc_args << source;
	AppendArgumentLine(cc_args, ia, state.runtime_source_path);
	for (auto & i : state.extra_include) AppendArgumentLine(cc_args, ia, i);
	AppendArgumentLine(cc_args, oa, object);
	SafePointer<RegistryNode> ld = local_config->OpenNode(L"Defines");
	if (ld) for (auto & v : ld->GetValues()) AppendArgumentLine(cc_args, da, v + L"=1");
	if (state.version_information.CreateVersionDefines) {
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_APPNAME=L\"" + EscapeString(state.version_information.ApplicationName) + L"\"");
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_COMPANY=L\"" + EscapeString(state.version_information.CompanyName) + L"\"");
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_COPYRIGHT=L\"" + EscapeString(state.version_information.Copyright) + L"\"");
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_APPSYSNAME=L\"" + EscapeString(state.version_information.InternalName) + L"\"");
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_APPIDENT=L\"" + EscapeString(state.version_information.ApplicationIdentifier) + L"\"");
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_COMPANYIDENT=L\"" + EscapeString(state.version_information.CompanyIdentifier) + L"\"");
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_DESCRIPTION=L\"" + EscapeString(state.version_information.ApplicationDescription) + L"\"");
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_VERSIONMAJOR=" + string(state.version_information.VersionMajor));
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_VERSIONMINOR=" + string(state.version_information.VersionMinor));
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_SUBVERSION=" + string(state.version_information.Subversion));
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_BUILD=" + string(state.version_information.Build));
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_APPSHORTVERSION=L\"" + string(state.version_information.VersionMajor) + L"." + string(state.version_information.VersionMinor) + L"\"");
		AppendArgumentLine(cc_args, da, L"ENGINE_VI_APPVERSION=L\"" + string(state.version_information.VersionMajor) + L"." + string(state.version_information.VersionMinor) + L"." +
			string(state.version_information.Subversion) + L"." + string(state.version_information.Build) + L"\"");
	}
	SafePointer<RegistryNode> la = local_config->OpenNode(L"Compiler/Arguments");
	if (la) for (auto & v : la->GetValues()) cc_args << la->GetValueString(v);
	handle log_file = IO::CreateFile(log, IO::AccessReadWrite, IO::CreateAlways);
	IO::SetStandardOutput(log_file);
	IO::SetStandardError(log_file);
	IO::CloseFile(log_file);
	SafePointer<Process> compiler = CreateCommandProcess(cc, &cc_args);
	if (!compiler) {
		if (!state.silent) {
			console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
			console << TextColor(Console::ColorRed) << FormatString(L"Failed to launch the compiler (%0).", cc) << TextColorDefault() << LineFeed();
			console << TextColor(Console::ColorRed) << L"You may try \"ertaconf\" to repair." << TextColorDefault() << LineFeed();
		}
		return ERTBT_INVALID_COMPILER_SET;
	}
	compiler->Wait();
	if (compiler->GetExitCode()) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
		if (state.shelllog) {
			IO::SetStandardOutput(state.stdout_clone);
			IO::SetStandardError(state.stderr_clone);
			Shell::OpenFile(log);
		} else PrintError(IO::GetStandardError(), state.stderr_clone);
		return ERTBT_COMPILATION_FAILED;
	}
	IO::SetStandardOutput(state.stdout_clone);
	IO::SetStandardError(state.stderr_clone);
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
	return ERTBT_SUCCESS;
}
int LinkExecutable(const Array<string> & obj_list, const string & output, const string & output_fake, const string & log, Console & console)
{
	auto oa = local_config->GetValueString(L"Linker/OutputArgument");
	auto link = local_config->GetValueString(L"Linker/Path");
	if (!link.Length()) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"No linker set for current configuration." << TextColorDefault() << LineFeed();
		return ERTBT_INVALID_LINKER_SET;
	}
	if (!state.silent) console << L"Linking " << TextColor(Console::ColorCyan) << IO::Path::GetFileName(output_fake) << TextColorDefault() << L"...";
	Array<string> link_args(0x80);
	link_args << obj_list;
	AppendArgumentLine(link_args, oa, output);
	SafePointer<RegistryNode> la = local_config->OpenNode(L"Linker/Arguments");
	if (la) for (auto & v : la->GetValues()) link_args << la->GetValueString(v);
	handle log_file = IO::CreateFile(log, IO::AccessReadWrite, IO::CreateAlways);
	IO::SetStandardOutput(log_file);
	IO::SetStandardError(log_file);
	IO::CloseFile(log_file);
	SafePointer<Process> linker = CreateCommandProcess(link, &link_args);
	if (!linker) {
		if (!state.silent) {
			console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
			console << TextColor(Console::ColorRed) << FormatString(L"Failed to launch the linker (%0).", link) << TextColorDefault() << LineFeed();
			console << TextColor(Console::ColorRed) << L"You may try \"ertaconf\" to repair." << TextColorDefault() << LineFeed();
		}
		return ERTBT_INVALID_LINKER_SET;
	}
	linker->Wait();
	if (linker->GetExitCode()) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
		if (state.shelllog) {
			IO::SetStandardOutput(state.stdout_clone);
			IO::SetStandardError(state.stderr_clone);
			Shell::OpenFile(log);
		} else PrintError(IO::GetStandardError(), state.stderr_clone);
		return ERTBT_LINKING_FAILED;
	}
	IO::SetStandardOutput(state.stdout_clone);
	IO::SetStandardError(state.stderr_clone);
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
	return ERTBT_SUCCESS;
}
int CopyAttachments(Console & console)
{
	SafePointer<RegistryNode> node = state.project->OpenNode(L"Attachments");
	if (node) {
		auto dest_path = IO::Path::GetDirectory(state.output_executable);
		for (auto & ann : node->GetSubnodes()) {
			SafePointer<RegistryNode> sub = node->OpenNode(ann);
			if (!sub) continue;
			auto source = sub->GetValueString(L"From");
			auto dest = sub->GetValueString(L"To");
			source = ExpandPath(source, state.project_root_path);
			dest = ExpandPath(dest, dest_path);
			if (!state.silent) console << L"Copying attachment " << TextColor(Console::ColorCyan) << IO::Path::GetFileName(dest) << TextColorDefault() << L"...";
			IO::CreateDirectoryTree(IO::Path::GetDirectory(dest));
			if (!CopyFile(source, dest)) {
				if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
				return ERTBT_ATTACHMENT_FAILED;
			}
			if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
		}
	}
	return ERTBT_SUCCESS;
}
Array<string> * DecomposeCommand(const string & command)
{
	SafePointer< Array<string> > parts = new Array<string>(0x10);
	int sp = 0;
	while (sp < command.Length()) {
		while (sp < command.Length() && (command[sp] == L' ' || command[sp] == L'\t')) sp++;
		if (sp < command.Length()) {
			widechar bc = 0;
			if (command[sp] == L'\'' || command[sp] == L'\"') bc = command[sp];
			auto ep = sp;
			if (bc == 0) {
				while (ep < command.Length() && command[ep] != L' ' && command[ep] != L'\t') ep++;
				parts->Append(command.Fragment(sp, ep - sp));
				sp = ep;
			} else {
				ep++;
				while (ep < command.Length()) {
					if (command[ep] == bc) {
						if (command[ep + 1] == bc) {
							ep += 2;
						} else {
							ep++;
							widechar from[3] = { bc, bc, 0 };
							widechar to[2] = { bc, 0 };
							parts->Append(command.Fragment(sp + 1, ep - sp - 2).Replace(from, to));
							sp = ep;
							break;
						}
					} else ep++;
				}
			}
		}
	}
	parts->Retain();
	return parts;
}
int InvokeExternalTools(Console & console)
{
	SafePointer<RegistryNode> base = state.project->OpenNode(L"Invoke");
	if (base) {
		IO::SetCurrentDirectory(state.project_root_path);
		for (auto & cn : base->GetValues()) {
			auto command = base->GetValueString(cn);
			SafePointer< Array<string> > args = DecomposeCommand(command);
			if (!args->Length()) {
				if (!state.silent) console << TextColor(Console::ColorRed) << L"The invokation command is empty." << TextColorDefault() << LineFeed();
				return ERTBT_INVALID_INVOKATION;
			}
			auto server = args->FirstElement();
			args->RemoveFirst();
			IO::SetStandardOutput(state.stdout_clone);
			IO::SetStandardError(state.stderr_clone);
			SafePointer<Process> process = CreateCommandProcess(server, args);
			if (!process) {
				if (!state.silent) console << TextColor(Console::ColorRed) << FormatString(L"Failed to launch the server \"%0\".", server) << TextColorDefault() << LineFeed();
				return ERTBT_INVALID_INVOKATION;
			}
			process->Wait();
			if (process->GetExitCode()) return ERTBT_INVOKATION_FAILED;
		}
	}
	return ERTBT_SUCCESS;
}
void SkipResourceToolAlert(Console & console)
{
	SafePointer<RegistryNode> res = state.project->OpenNode(L"Resources");
	if (!state.silent && res) console << TextColor(Console::ColorYellow) << L"Assembly Resources are not available with the current configuration!" << TextColorDefault() << LineFeed();
}
string ProcessResourceString(const string & value)
{
	return value.Replace(L"$object$", state.project_object_path).Replace(L"$output$", state.project_output_name).Replace(L"$internal$",
		state.version_information.InternalName).Replace(L"$target$", state.project_output_root);
}
int InvokeResourceTool(Console & console, Array<string> & link_list)
{
	SafePointer<RegistryNode> res_node = local_config->OpenNode(L"Resource");
	if (res_node) {
		auto rt = res_node->GetValueString(L"Path");
		auto link_with = res_node->GetValueString(L"SetLink");
		auto set_output = res_node->GetValueString(L"SetOutput");
		if (!rt.Length()) {
			SkipResourceToolAlert(console);
			return ERTBT_SUCCESS;
		}
		Array<string> rt_args(0x10);
		rt_args << state.project_file_path;
		rt_args << L"-N";
		if (state.clean) rt_args << L"-C";
		if (state.shelllog) rt_args << L"-E";
		if (state.silent) rt_args << L"-S";
		rt_args << L"-a";
		rt_args << state.arch.Name;
		rt_args << L"-c";
		rt_args << state.conf.Name;
		rt_args << L"-o";
		rt_args << state.os.Name;
		IO::SetStandardOutput(state.stdout_clone);
		IO::SetStandardError(state.stderr_clone);
		SafePointer<Process> restool = CreateCommandProcess(rt, &rt_args);
		if (!restool) {
			if (!state.silent) console << TextColor(Console::ColorRed) << FormatString(L"Failed to launch the resource generator (%0).", rt) << TextColorDefault() << LineFeed();
			return ERTBT_INVALID_RESOURCE_SET;
		}
		restool->Wait();
		if (restool->GetExitCode()) {
			if (!state.silent) console << TextColor(Console::ColorRed) << L"Resource generator failed." << TextColorDefault() << LineFeed();
			return restool->GetExitCode();
		}
		if (link_with.Length()) link_list << ProcessResourceString(link_with);
		if (set_output.Length()) state.output_executable = IO::ExpandPath(ProcessResourceString(set_output));
	} else SkipResourceToolAlert(console);
	return ERTBT_SUCCESS;
}

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
				if (arg == L'C') {
					state.clean = true;
				} else if (arg == L'E') {
					state.shelllog = true;
				} else if (arg == L'N') {
					state.nologo = true;
				} else if (arg == L'O') {
					state.pathout = true;
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
				} else if (arg == L'b') {
					state.build_cache = true;
				} else if (arg == L'd') {
					int error = SelectTarget(L"debug", BuildTargetClass::Configuration, console);
					if (error) return error;
				} else if (arg == L'o') {
					if (i < args->Length()) {
						int error = SelectTarget(args->ElementAt(i), BuildTargetClass::OperatingSystem, console);
						if (error) return error;
						i++;
					} else {
						console << TextColor(Console::ColorYellow) << L"Invalid command line: argument expected." << TextColorDefault() << LineFeed();
						return ERTBT_INVALID_COMMAND_LINE;
					}
				} else if (arg == L'r') {
					int error = SelectTarget(L"release", BuildTargetClass::Configuration, console);
					if (error) return error;
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
void PrintSessionInformation(Console & console)
{
	auto obj_name = state.build_cache ? L"Engine Runtime" : IO::Path::GetFileNameWithoutExtension(state.project_file_path);
	console << L"Building " << TextColor(Console::ColorRed) << obj_name << TextColorDefault() <<
		L" for " << TextColor(Console::ColorMagenta) << state.arch.HumanReadableName << TextColorDefault() <<
		L" " << TextColor(Console::ColorBlue) << state.os.HumanReadableName << TextColorDefault() <<
		L" with the " << TextColor(Console::ColorCyan) << state.conf.HumanReadableName << TextColorDefault() << L" configuration." << LineFeed();
}
int BuildRuntime(Console & console)
{
	auto start = GetTimerValue();
	if (!state.silent) PrintSessionInformation(console);
	state.project_time = 0;
	state.version_information.CreateVersionDefines = false;
	SafePointer< Array<string> > files = IO::Search::GetFiles(state.runtime_source_path + L"/" + local_config->GetValueString(L"CompileFilter"), true);
	Array<string> compile_list(0x100);
	for (auto & f : *files) compile_list << IO::ExpandPath(state.runtime_source_path + L"/" + f);
	IO::CreateDirectoryTree(state.runtime_object_path);
	if (state.clean) {
		SafePointer< Array<string> > prev_files = IO::Search::GetFiles(state.runtime_object_path + L"/*." + local_config->GetValueString(L"ObjectExtension") + L";*.log");
		for (auto & f : *prev_files) IO::RemoveFile(state.runtime_object_path + L"/" + f);
	}
	for (auto & f : compile_list) {
		auto fo = IO::ExpandPath(state.runtime_object_path + L"/" + IO::Path::GetFileNameWithoutExtension(f));
		auto fl = fo + L".log"; fo += L"." + local_config->GetValueString(L"ObjectExtension");
		auto error = CompileSource(f, fo, fl, console);
		if (error) return error;
	}
	auto end = GetTimerValue();
	if (!state.silent) console << TextColor(Console::ColorGreen) << FormatString(L"Runtime build have completed successfully, %0 ms spent.", end - start) << TextColorDefault() << LineFeed();
	return ERTBT_SUCCESS;
}
int BuildProject(Console & console)
{
	auto start = GetTimerValue();
	if (!state.silent) PrintSessionInformation(console);
	Array<string> object_files(0x100);
	Array<string> source_files(0x100);
	SafePointer< Array<string> > object_files_search = IO::Search::GetFiles(state.runtime_object_path + L"/*." + local_config->GetValueString(L"ObjectExtension"));
	if (!object_files_search->Length()) {
		if (!state.silent) console << TextColor(Console::ColorYellow) << L"No object files in Runtime cache! Recompile Runtime! (ertbuild :b)." << TextColorDefault() << LineFeed();
	}
	for (auto & f : *object_files_search) object_files << IO::ExpandPath(state.runtime_object_path + L"/" + f);
	source_files << state.runtime_bootstrapper_path;
	if (state.project->GetValueBoolean(L"CompileAll")) {
		SafePointer< Array<string> > source_files_search = IO::Search::GetFiles(state.project_root_path + L"/" + local_config->GetValueString(L"CompileFilter"), true);
		for (auto & f : *source_files_search) if (IO::Path::GetFileName(f)[0] != L'.') source_files << IO::ExpandPath(state.project_root_path + L"/" + f);
	} else {
		SafePointer<RegistryNode> list = state.project->OpenNode(L"CompileList");
		if (list) for (auto & v : list->GetValues()) source_files << ExpandPath(list->GetValueString(v), state.project_root_path);
	}
	if (!state.pathout) {
		IO::CreateDirectoryTree(state.project_output_root);
		if (state.clean) ClearDirectory(state.project_output_root);
		IO::CreateDirectoryTree(state.project_object_path);
		for (auto & f : source_files) {
			auto fo = IO::ExpandPath(state.project_object_path + L"/" + IO::Path::GetFileNameWithoutExtension(f));
			auto fl = fo + L".log"; fo += L"." + local_config->GetValueString(L"ObjectExtension");
			auto error = CompileSource(f, fo, fl, console);
			if (error) return error;
			object_files << fo;
		}
	}
	if (!state.pathout) {
		auto wd = IO::GetCurrentDirectory();
		auto error = InvokeExternalTools(console);
		IO::SetCurrentDirectory(wd);
		if (error) return error;
	}
	auto error = InvokeResourceTool(console, object_files);
	if (error) return error;
	if (state.pathout) console.WriteLine(state.output_executable);
	if (!state.pathout) {
		string internal_output = IO::ExpandPath(state.project_object_path + L"/" + state.project_output_name + L".tmp");
		string link_log = IO::ExpandPath(state.project_object_path + L"/" + state.project_output_name + L".linker.log");
		error = CopyAttachments(console);
		if (error) return error;
		error = LinkExecutable(object_files, internal_output, state.output_executable, link_log, console);
		if (error) return error;
		if (!CopyFile(internal_output, state.output_executable)) {
			if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed to substitute the executable." << TextColorDefault() << LineFeed();
			return ERTBT_OVERWRITE_FAILED;
		}
	}
	auto end = GetTimerValue();
	if (!state.silent) console << TextColor(Console::ColorGreen) << FormatString(L"Project build have completed successfully, %0 ms spent.", end - start) << TextColorDefault() << LineFeed();
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
		if (!state.nologo && !state.silent) {
			console << ENGINE_VI_APPNAME << LineFeed();
			console << L"Copyright " << string(ENGINE_VI_COPYRIGHT).Replace(L'\xA9', L"(C)") << LineFeed();
			console << L"Version " << ENGINE_VI_APPVERSION << L", build " << ENGINE_VI_BUILD << LineFeed() << LineFeed();
		}
		if (state.project_file_path.Length() && state.build_cache) {
			if (!state.silent) console << TextColor(Console::ColorRed) << L"Invalid command line: both project file and build cache option specified." << TextColorDefault() << LineFeed();
			return ERTBT_DUPLICATE_INPUT_FILE;
		}
		if (state.project_file_path.Length()) {
			error = LoadProject(console);
			if (error) return error;
			MakeLocalConfiguration(console);
			error = LoadVersionInformation(console);
			if (error) return error;
			ProjectPostConfig();
			return BuildProject(console);
		} else if (state.build_cache) {
			MakeLocalConfiguration(console);
			return BuildRuntime(console);
		} else if (!state.silent) {
			console << L"Command line syntax:" << LineFeed();
			console << L"  " << ENGINE_VI_APPSYSNAME << L" <project.ini> :CENOSabdor" << LineFeed();
			console << L"Where project.ini is the project configuration file." << LineFeed();
			console << L"You can optionally use the next build options:" << LineFeed();
			console << L"  :C - clean build, rebuild any cached files," << LineFeed();
			console << L"  :E - use shell error mode - open error logs in an external editor," << LineFeed();
			console << L"  :N - use no logo mode - don't print application logo," << LineFeed();
			console << L"  :O - use output path only mode - evaluate the output executable's path and print it," << LineFeed();
			console << L"  :S - use silent mode - supress any output, except output path," << LineFeed();
			console << L"  :a - specify processor architecture (as the next argument)," << LineFeed();
			console << L"  :b - build the Runtime cache," << LineFeed();
			console << L"  :d - use debug mode configuration," << LineFeed();
			console << L"  :o - specify target operating system (as the next argument)," << LineFeed();
			console << L"  :r - use release mode configuration." << LineFeed();
			console << LineFeed();
		}
	} catch (Exception & e) {
		if (!state.silent) console << TextColor(Console::ColorRed) << FormatString(L"Build tool failed: %0.", e.ToString()) << TextColorDefault() << LineFeed();
		return ERTBT_COMMON_EXCEPTION;
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Build tool failed: Unknown exception." << TextColorDefault() << LineFeed();
		return ERTBT_UNCOMMON_EXCEPTION;
	}
	return ERTBT_SUCCESS;
}