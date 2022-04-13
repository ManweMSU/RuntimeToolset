#include "ertcom.h"

BuilderState state;

SafePointer<Registry> tool_config;
SafePointer<Registry> local_config;
SafePointer<Registry> project_initial_config;

string EscapeString(const string & input)
{
	int ucslen = input.GetEncodedLength(Encoding::UTF32);
	Array<uint32> ucs(0x100);
	ucs.SetLength(ucslen);
	input.Encode(ucs, Encoding::UTF32, false);
	DynamicString result;
	for (auto & c : ucs) {
		if (c < 0x20 || c > 0x7F || c == L'\\' || c == L'\"') {
			if (c >= 0x10000) {
				result += L"\\U" + string(c, L"0123456789ABCDEF", 8);
			} else if (c > 0x7F) {
				result += L"\\u" + string(c, L"0123456789ABCDEF", 4);
			} else if (c == L'\\') {
				result += L"\\\\";
			} else if (c == L'\"') {
				result += L"\\\"";
			} else if (c == L'\n') {
				result += L"\\n";
			} else if (c == L'\r') {
				result += L"\\r";
			} else if (c == L'\t') {
				result += L"\\t";
			} else {
				result += L"\\" + string(c, L"01234567", 3);
			}
		} else {
			result += widechar(c);
		}
	}
	return result.ToString();
}
string EscapeStringRc(const string & input)
{
	DynamicString result;
	for (int i = 0; i < input.Length(); i++) {
		auto c = input[i];
		if (c < 0x20 || c == L'\\' || c == L'\"') {
			if (c == L'\\') {
				result += L"\\\\";
			} else if (c == L'\"') {
				result += L"\\\"";
			} else if (c == L'\n') {
				result += L"\\n";
			} else if (c == L'\r') {
				result += L"\\r";
			} else if (c == L'\t') {
				result += L"\\t";
			} else {
				result += L"\\" + string(uint(c), L"01234567", 3);
			}
		} else {
			result += c;
		}
	}
	return result.ToString();
}
string EscapeStringXml(const string & input)
{
	DynamicString result;
	for (int i = 0; i < input.Length(); i++) {
		auto c = input[i];
		if (c == L'\"') {
			result += L"&quot;";
		} else if (c == L'\'') {
			result += L"&apos;";
		} else if (c == L'<') {
			result += L"&lt;";
		} else if (c == L'>') {
			result += L"&gt;";
		} else if (c == L'&') {
			result += L"&amp;";
		} else result += c;
	}
	return result.ToString();
}
string ExpandPath(const string & path, const string & relative_to)
{
	if (path[0] == L'/' || (path[0] && path[1] == L':')) return IO::ExpandPath(path);
	else return IO::ExpandPath(relative_to + L"/" + path);
}
void ClearDirectory(const string & path)
{
	try {
		SafePointer< Array<string> > files = IO::Search::GetFiles(path + L"/*");
		SafePointer< Array<string> > dirs = IO::Search::GetDirectories(path + L"/*");
		for (auto & f : *files) IO::RemoveFile(path + L"/" + f);
		for (auto & d : *dirs) { auto fd = path + L"/" + d; ClearDirectory(fd); IO::RemoveDirectory(fd); }
	} catch (...) {}
}
void AppendArgumentLine(Array<string> & cc, const string & arg_word, const string & arg_val)
{
	if (arg_word.FindFirst(L'$') != -1) {
		cc << arg_word.Replace(L'$', arg_val);
	} else {
		cc << arg_word;
		cc << arg_val;
	}
}
void PrintError(handle from, handle to)
{
	FileStream stream_from(from);
	stream_from.Seek(0, Begin);
	TextReader reader(&stream_from);
	Console local_console(state.stderr_clone);
	local_console.Write(reader.ReadAll());
}
bool CopyFile(const string & from, const string & to)
{
	try {
		FileStream source(from, AccessRead, OpenExisting);
		FileStream dest(to, AccessWrite, CreateAlways);
		source.CopyTo(&dest);
	} catch (...) { return false; }
	return true;
}

int ConfigurationInitialize(Console & console)
{
	auto config_file_pref = IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/ertbuild.";
	try {
		FileStream config_file_stream(config_file_pref + L"ecs", AccessRead, OpenExisting);
		tool_config = LoadRegistry(&config_file_stream);
		if (!tool_config) throw Exception();
	} catch (...) {
		try {
			FileStream config_file_stream(config_file_pref + L"ini", AccessRead, OpenExisting);
			tool_config = CompileTextRegistry(&config_file_stream);
			if (!tool_config) throw Exception();
		} catch (...) {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Build tool configuration is unavailable or invalid." << LineFeed() <<
				L"Check ertbuild.ecs or ertbuild.ini." << LineFeed() << L"You may try \"ertaconf\" to repair." << TextColorDefault() << LineFeed();
			return ERTBT_INVALID_CONFIGURATION;
		}
	}
	SafePointer<RegistryNode> general = tool_config->OpenNode(L"Targets");
	if (!general) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"There is no \"Targets\" node in the configuration." << LineFeed() <<
			L"Check ertbuild.ecs or ertbuild.ini." << LineFeed() << L"You may try \"ertaconf\" to repair." << TextColorDefault() << LineFeed();
		return ERTBT_INVALID_CONFIGURATION;
	}
	for (auto & target : general->GetSubnodes()) {
		SafePointer<RegistryNode> node = general->OpenNode(target);
		if (!node) return ERTBT_INVALID_CONFIGURATION;
		auto cls = node->GetValueString(L"Class");
		BuildTarget bt;
		bt.Name = target;
		bt.HumanReadableName = node->GetValueString(L"Name");
		bt.Default = node->GetValueBoolean(L"Default");
		if (string::CompareIgnoreCase(cls, L"arch") == 0) {
			state.vol_arch << bt;
		} else if (string::CompareIgnoreCase(cls, L"os") == 0) {
			state.vol_os << bt;
		} else if (string::CompareIgnoreCase(cls, L"conf") == 0) {
			state.vol_conf << bt;
		} else if (string::CompareIgnoreCase(cls, L"subsys") == 0) {
			state.vol_subsys << bt;
		} else {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << FormatString(L"Unknown target class \"%0\" in the configuration.", cls) << LineFeed() <<
				L"Check ertbuild.ecs or ertbuild.ini." << TextColorDefault() << LineFeed();
			return ERTBT_INVALID_CONFIGURATION;
		}
	}
	if (!state.vol_arch.Length()) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"There is no processor architectures in the configuration." << LineFeed() <<
			L"Check ertbuild.ecs or ertbuild.ini." << TextColorDefault() << LineFeed();
		return ERTBT_INVALID_CONFIGURATION;
	}
	if (!state.vol_os.Length()) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"There is no operating systems in the configuration." << LineFeed() <<
			L"Check ertbuild.ecs or ertbuild.ini." << TextColorDefault() << LineFeed();
		return ERTBT_INVALID_CONFIGURATION;
	}
	if (!state.vol_conf.Length()) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"There is no output configurations in the configuration." << LineFeed() <<
			L"Check ertbuild.ecs or ertbuild.ini." << TextColorDefault() << LineFeed();
		return ERTBT_INVALID_CONFIGURATION;
	}
	if (!state.vol_subsys.Length()) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"There is no subsystems in the configuration." << LineFeed() <<
			L"Check ertbuild.ecs or ertbuild.ini." << TextColorDefault() << LineFeed();
		return ERTBT_INVALID_CONFIGURATION;
	}
	state.arch = state.vol_arch.FirstElement();
	for (auto & e : state.vol_arch) if (e.Default) { state.arch = e; break; }
	state.os = state.vol_os.FirstElement();
	for (auto & e : state.vol_os) if (e.Default) { state.os = e; break; }
	state.conf = state.vol_conf.FirstElement();
	for (auto & e : state.vol_conf) if (e.Default) { state.conf = e; break; }
	state.subsys = state.vol_subsys.FirstElement();
	for (auto & e : state.vol_subsys) if (e.Default) { state.subsys = e; break; }
	return ERTBT_SUCCESS;
}
int SelectTarget(const string & name, BuildTargetClass cls, Console & console)
{
	string cls_name;
	Array<BuildTarget> * search_vol = 0;
	BuildTarget * set = 0;
	if (cls == BuildTargetClass::Architecture) {
		cls_name = L"processor architecture";
		search_vol = &state.vol_arch;
		set = &state.arch;
	} else if (cls == BuildTargetClass::OperatingSystem) {
		cls_name = L"operating system";
		search_vol = &state.vol_os;
		set = &state.os;
	} else if (cls == BuildTargetClass::Configuration) {
		cls_name = L"output configuration";
		search_vol = &state.vol_conf;
		set = &state.conf;
	} else if (cls == BuildTargetClass::Subsystem) {
		cls_name = L"subsystem";
		search_vol = &state.vol_subsys;
		set = &state.subsys;
	}
	for (auto & e : *search_vol) if (string::CompareIgnoreCase(e.Name, name) == 0) { *set = e; return ERTBT_SUCCESS; }
	if (!state.silent) console << TextColor(ConsoleColor::Red) << FormatString(L"No %0 with name \"%1\" was found.", cls_name, name) << TextColorDefault() << LineFeed();
	return ERTBT_UNKNOWN_TARGET;
}
BuildTarget * FindTarget(const string & name, BuildTargetClass cls)
{
	Array<BuildTarget> * search_vol = 0;
	if (cls == BuildTargetClass::Architecture) search_vol = &state.vol_arch;
	else if (cls == BuildTargetClass::OperatingSystem) search_vol = &state.vol_os;
	else if (cls == BuildTargetClass::Configuration) search_vol = &state.vol_conf;
	else if (cls == BuildTargetClass::Subsystem) search_vol = &state.vol_subsys;
	for (auto & e : *search_vol) if (string::CompareIgnoreCase(e.Name, name) == 0) { return &e; }
	return 0;
}
int MakeLocalConfiguration(Console & console)
{
	ObjectArray<RegistryNode> merge(0x10);
	for (auto & nn : tool_config->GetSubnodes()) {
		if (string::CompareIgnoreCase(nn, L"Targets") == 0) continue;
		auto parts = nn.Split(L'-');
		bool accept = true;
		for (auto & p : parts) {
			if (string::CompareIgnoreCase(p, state.arch.Name) && string::CompareIgnoreCase(p, state.os.Name) &&
				string::CompareIgnoreCase(p, state.subsys.Name) && string::CompareIgnoreCase(p, state.conf.Name)) { accept = false; break; }
		}
		if (accept) {
			SafePointer<RegistryNode> node = tool_config->OpenNode(nn);
			if (node) merge.Append(node);
		}
	}
	if (merge.Length()) {
		SafePointer<RegistryNode> node = CreateMergedNode(merge);
		local_config = CreateRegistryFromNode(node);
	} else local_config = CreateRegistry();
	auto local = IO::Path::GetDirectory(IO::GetExecutablePath());
	auto rt_path = local_config->GetValueString(L"RuntimePath");
	auto obj_path = local_config->GetValueString(L"ObjectPath");
	if (!rt_path.Length() || !obj_path.Length()) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"The selected targets are not supported by current configuration." << TextColorDefault() << LineFeed();
		return ERTBT_UNSUPPORTED_TRIPLE;
	}
	state.runtime_source_path = ExpandPath(rt_path, local);
	state.runtime_bootstrapper_path = ExpandPath(local_config->GetValueString(L"Bootstrapper"), local);
	state.runtime_object_path = ExpandPath(obj_path, local);
	state.runtime_modules_path = ExpandPath(local_config->GetValueString(L"Modules"), local);
	state.runtime_resources_path = ExpandPath(local_config->GetValueString(L"Resources"), local);
	return ERTBT_SUCCESS;
}
int LoadProject(Console & console)
{
	auto project_extension = IO::Path::GetExtension(state.project_file_path).LowerCase();
	if (project_extension == L"c" || project_extension == L"cpp" || project_extension == L"cxx") {
		project_initial_config = CreateRegistry();
		project_initial_config->CreateNode(L"CompileList");
		project_initial_config->CreateValue(L"CompileList/A", RegistryValueType::String);
		project_initial_config->SetValue(L"CompileList/A", state.project_file_path);
		project_initial_config->CreateValue(L"NoHiDPI", RegistryValueType::Boolean);
		project_initial_config->SetValue(L"NoHiDPI", true);
		state.project_time = 0;
	} else {
		try {
			FileStream project_stream(state.project_file_path, AccessRead, OpenExisting);
			state.project_time = IO::DateTime::GetFileAlterTime(project_stream.Handle());
			project_initial_config = CompileTextRegistry(&project_stream);
			if (!project_initial_config) throw Exception();
		} catch (...) {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Failed to access the project file." << TextColorDefault() << LineFeed();
			return ERTBT_PROJECT_FILE_ACCESS;
		}
	}
	SafePointer<Registry> project = project_initial_config->Clone();
	auto ss = project_initial_config->GetValueString(L"Subsystem");
	if (ss.Length()) {
		auto error = SelectTarget(ss, BuildTargetClass::Subsystem, console);
		if (error) return error;
	}
	ObjectArray<RegistryNode> merge_nodes(0x10);
	Array<string> merge_nodes_names(0x10);
	for (auto & nn : project_initial_config->GetSubnodes()) {
		if (nn[0] != L'-') continue;
		merge_nodes_names.Append(nn);
		auto parts = nn.Fragment(1, -1).Split(L'-');
		bool accept = true;
		for (auto & p : parts) {
			if (string::CompareIgnoreCase(p, state.arch.Name) && string::CompareIgnoreCase(p, state.os.Name) &&
				string::CompareIgnoreCase(p, state.subsys.Name) && string::CompareIgnoreCase(p, state.conf.Name)) { accept = false; break; }
		}
		if (accept) {
			SafePointer<RegistryNode> node = project_initial_config->OpenNode(nn);
			if (node) merge_nodes.Append(node);
		}
	}
	for (auto & nn : merge_nodes_names) project->RemoveNode(nn);
	merge_nodes.Append(project);
	SafePointer<RegistryNode> rep_node = CreateMergedNode(merge_nodes);
	state.project = CreateRegistryFromNode(rep_node);
	state.project_root_path = IO::Path::GetDirectory(state.project_file_path);
	if (state.project->GetValueString(L"ForcedArch").Length()) {
		auto arch = state.project->GetValueString(L"ForcedArch");
		if (string::CompareIgnoreCase(arch, state.arch.Name)) {
			auto error = SelectTarget(arch, BuildTargetClass::Architecture, console);
			if (error) return error;
			if (!state.silent) console << TextColor(ConsoleColor::Yellow) << FormatString(L"Project forced to switch the architecture to %0.", state.arch.HumanReadableName) << TextColorDefault() << LineFeed();
			return LoadProject(console);
		}
	}
	return ERTBT_SUCCESS;
}
string GeneralCheckForForcedArchitecture(const string & arch, const string & os, const string & conf, const string & subs)
{
	SafePointer<Registry> project = project_initial_config->Clone();
	ObjectArray<RegistryNode> merge_nodes(0x10);
	Array<string> merge_nodes_names(0x10);
	for (auto & nn : project_initial_config->GetSubnodes()) {
		if (nn[0] != L'-') continue;
		merge_nodes_names.Append(nn);
		auto parts = nn.Fragment(1, -1).Split(L'-');
		bool accept = true;
		for (auto & p : parts) {
			if (string::CompareIgnoreCase(p, arch) && string::CompareIgnoreCase(p, os) &&
				string::CompareIgnoreCase(p, subs) && string::CompareIgnoreCase(p, conf)) { accept = false; break; }
		}
		if (accept) {
			SafePointer<RegistryNode> node = project_initial_config->OpenNode(nn);
			if (node) merge_nodes.Append(node);
		}
	}
	for (auto & nn : merge_nodes_names) project->RemoveNode(nn);
	merge_nodes.Append(project);
	SafePointer<RegistryNode> rep_node = CreateMergedNode(merge_nodes);
	return rep_node->GetValueString(L"ForcedArch");
}
void ProjectPostConfig(void)
{
	SafePointer<RegistryNode> include = state.project->OpenNode(L"Include");
	if (include) for (auto & v : include->GetValues()) state.extra_include << ExpandPath(include->GetValueString(v), state.project_root_path);
	SafePointer<RegistryNode> link_with = state.project->OpenNode(L"LinkWith");
	if (link_with) for (auto & v : include->GetValues()) {
		auto name = link_with->GetValueString(v);
		auto path = ExpandPath(name, state.runtime_modules_path);
		state.extra_include << path;
		state.link_with_roots << path;
	}
	auto output_location = state.project->GetValueString(L"OutputLocation");
	if (!output_location.Length()) output_location = FormatString(L"_build/%0_%1_%2", state.os.Name.LowerCase(), state.arch.Name.LowerCase(), state.conf.Name.LowerCase());
	state.project_output_root = ExpandPath(output_location, state.project_root_path);
	state.project_object_path = IO::ExpandPath(state.project_output_root + L"/_obj");
	state.output_executable = IO::ExpandPath(state.project_output_root + L"/" + state.project_output_name);
	auto executable_extension = local_config->GetValueString(L"ExecutableExtension");
	if (executable_extension.Length()) state.output_executable += L"." + executable_extension;
}
bool IsValidIdentifier(const string & value)
{
	for (int i = 0; i < value.Length(); i++) {
		auto c = value[i];
		if ((c < L'0' || c > L'9') && (c < L'A' || c > L'Z') && (c < L'a' || c > L'z') && c != L'.' && c != L'-' && c != L'_') return false;
	}
	return true;
}
int LoadVersionInformation(Console & console)
{
	state.project_output_name = state.project->GetValueString(L"OutputName");
	if (!state.project_output_name.Length()) state.project_output_name = IO::Path::GetFileNameWithoutExtension(state.project_file_path);
	state.version_information.CreateVersionDefines = state.project->GetValueBoolean(L"UseVersionDefines");
	state.version_information.ApplicationName = state.project->GetValueString(L"VersionInformation/ApplicationName");
	state.version_information.InternalName = state.project->GetValueString(L"VersionInformation/InternalName");
	state.version_information.CompanyName = state.project->GetValueString(L"VersionInformation/CompanyName");
	state.version_information.Copyright = state.project->GetValueString(L"VersionInformation/Copyright");
	state.version_information.ApplicationIdentifier = state.project->GetValueString(L"VersionInformation/ApplicationIdentifier");
	state.version_information.CompanyIdentifier = state.project->GetValueString(L"VersionInformation/CompanyIdentifier");
	state.version_information.ApplicationDescription = state.project->GetValueString(L"VersionInformation/Description");
	state.version_information.AccessRequirements.CameraUsageReason = state.project->GetValueString(L"AccessRequirements/Camera");
	state.version_information.AccessRequirements.MicrophoneUsageReason = state.project->GetValueString(L"AccessRequirements/Microphone");
	if (!IsValidIdentifier(state.version_information.InternalName) || !IsValidIdentifier(state.version_information.ApplicationIdentifier) || !IsValidIdentifier(state.version_information.CompanyIdentifier)) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Invalid identifier in the version information." << TextColorDefault() << LineFeed();
		return ERTBT_INVALID_IDENTIFIER;
	}
	try {
		auto verind = state.project->GetValueString(L"VersionInformation/Version").Split(L'.');
		if (verind.Length() > 4) throw Exception();
		if (verind.Length() > 0 && verind[0].Length()) state.version_information.VersionMajor = verind[0].ToUInt32(); else state.version_information.VersionMajor = 0;
		if (verind.Length() > 1) state.version_information.VersionMinor = verind[1].ToUInt32(); else state.version_information.VersionMinor = 0;
		if (verind.Length() > 2) state.version_information.Subversion = verind[2].ToUInt32(); else state.version_information.Subversion = 0;
		if (verind.Length() > 3) state.version_information.Build = verind[3].ToUInt32(); else state.version_information.Build = 0;
	} catch (...) {
		if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Invalid version notation in the version information." << TextColorDefault() << LineFeed();
		return ERTBT_INVALID_VERSION;
	}
	if (!state.version_information.InternalName.Length()) state.version_information.InternalName = state.project_output_name;
	if (!state.version_information.ApplicationIdentifier.Length()) state.version_information.ApplicationIdentifier = IO::Path::GetFileNameWithoutExtension(state.project_file_path);
	if (!state.version_information.CompanyIdentifier.Length()) state.version_information.CompanyIdentifier = L"unknown";
	return ERTBT_SUCCESS;
}