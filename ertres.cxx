#include "ertcom.h"

enum class ResourceMode { Windows, MacOSX };

struct UniqueIcon
{
	string SourcePath;
	string ConvertedPath;
	string Reference;
};
struct ApplicationResource
{
	string SourcePath;
	string Name;
	string Locale;
};
struct ApplicationFileFormat
{
	string Extension;
	string Description;
	UniqueIcon * Icon;
	bool CanCreate;
	bool IsProtocol;
};

struct {
	ResourceMode mode;
	SafeArray<UniqueIcon> icon_database = SafeArray<UniqueIcon>(0x20);
	Array<ApplicationResource> resources = Array<ApplicationResource>(0x20);
	Array<ApplicationFileFormat> file_formats = Array<ApplicationFileFormat>(0x20);
	UniqueIcon * application_icon = 0;
	int file_icon_counter = 0;
	bool property_disable_hidpi = false;
	bool property_disable_dock_icon = false;
	bool property_needs_root_elevation = false;
	string resource_manifest_file;
	string resource_script_file;
	string resource_object_file;
	string resource_object_file_log;
	string resource_file_formats_file;
} res_state;

SafePointer<RegistryNode> configuration;

bool IsValidResourceName(const string & value)
{
	for (int i = 0; i < value.Length(); i++) {
		auto c = value[i];
		if ((c < L'a' || c > L'z') && (c < L'A' || c > L'Z') && (c < L'0' || c > L'9') && c != L'_') return false;
	}
	return true;
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
				} else if (arg == L'o') {
					if (i < args->Length()) {
						int error = SelectTarget(args->ElementAt(i), BuildTargetClass::OperatingSystem, console);
						if (error) return error;
						i++;
					} else {
						console << TextColor(Console::ColorYellow) << L"Invalid command line: argument expected." << TextColorDefault() << LineFeed();
						return ERTBT_INVALID_COMMAND_LINE;
					}
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

int BuildIcon(const string & path, UniqueIcon ** icon, Console & console, bool is_file_icon = false)
{
	for (auto & i : res_state.icon_database) if (i.ConvertedPath == path) {
		if (icon) *icon = &i;
		return ERTBT_SUCCESS;
	}
	auto output = IO::ExpandPath(state.project_object_path + L"/" + IO::Path::GetFileNameWithoutExtension(path) + L"." + configuration->GetValueString(L"IconExtension"));
	UniqueIcon result;
	result.SourcePath = path;
	result.ConvertedPath = output;
	if (res_state.mode == ResourceMode::Windows) {
		result.Reference = string(res_state.icon_database.Length());
	} else if (res_state.mode == ResourceMode::MacOSX) {
		if (is_file_icon) {
			res_state.file_icon_counter++;
			result.Reference = L"FileIcon" + string(res_state.file_icon_counter);
		} else result.Reference = L"AppIcon";
	}
	res_state.icon_database.Append(result);
	if (icon) *icon = &res_state.icon_database.LastElement();
	if (!state.clean) {
		try {
			FileStream src(path, AccessRead, OpenExisting);
			FileStream out(output, AccessRead, OpenExisting);
			auto src_time = IO::DateTime::GetFileAlterTime(src.Handle());
			auto out_time = IO::DateTime::GetFileAlterTime(out.Handle());
			if (out_time > src_time && out_time > state.project_time) return ERTBT_SUCCESS;
		} catch (...) {}
	}
	if (!state.silent) console << L"Converting icon file " << TextColor(Console::ColorCyan) << IO::Path::GetFileName(path) << TextColorDefault() << L"...";
	try {
		FileStream source(path, AccessRead, OpenExisting);
		SafePointer<Codec::Image> image = Codec::DecodeImage(&source);
		Array<int> sizes(0x10);
		SafePointer<RegistryNode> sizes_node = configuration->OpenNode(L"IconSizes");
		if (sizes_node) for (auto & v : sizes_node->GetValues()) sizes << sizes_node->GetValueInteger(v);
		for (int i = image->Frames.Length() - 1; i >= 0; i--) {
			auto frame = image->Frames.ElementAt(i);
			bool accept = false;
			for (auto & s : sizes) if (frame->GetWidth() == s && frame->GetHeight() == s) { accept = true; break; }
			if (!accept) image->Frames.Remove(i);
		}
		if (image->Frames.Length()) {
			FileStream output_stream(output, AccessReadWrite, CreateAlways);
			Codec::EncodeImage(&output_stream, image, configuration->GetValueString(L"IconCodec"));
		} else throw Exception();
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
		return ERTBT_INVALID_FILE_FORMAT;
	}
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
	return ERTBT_SUCCESS;
}
int ExtractResources(Console & console, RegistryNode * node = 0, const string & locale = L"")
{
	SafePointer<RegistryNode> current;
	if (node) current.SetRetain(node); else current = state.project->OpenNode(L"Resources");
	if (!current) return ERTBT_SUCCESS;
	for (auto & v : current->GetValues()) {
		ApplicationResource resource;
		resource.SourcePath = ExpandPath(current->GetValueString(v), state.project_root_path);
		if (!IsValidResourceName(v)) {
			if (!state.silent) console << TextColor(Console::ColorRed) << FormatString(L"Invalid resource name \"%0\".", v) << TextColorDefault() << LineFeed();
			return ERTBT_INVALID_RESOURCE;
		}
		resource.Name = v;
		resource.Locale = locale;
		res_state.resources.Append(resource);
	}
	for (auto & v : current->GetSubnodes()) {
		auto cand = v.LowerCase();
		if (cand.Length() != 2) {
			if (!state.silent) console << TextColor(Console::ColorRed) << FormatString(L"Invalid locale name \"%0\".", v) << TextColorDefault() << LineFeed();
			return ERTBT_INVALID_RESOURCE;
		}
		SafePointer<RegistryNode> sub = current->OpenNode(v);
		auto error = ExtractResources(console, sub, cand);
		if (error) return error;
	}
	return ERTBT_SUCCESS;
}
int ExtractFileFormats(Console & console)
{
	SafePointer<RegistryNode> node = state.project->OpenNode(L"FileFormats");
	if (!node) return ERTBT_SUCCESS;
	for (auto & v : node->GetSubnodes()) {
		SafePointer<RegistryNode> sub = node->OpenNode(v);
		if (sub->GetValueString(L"Extension").Length()) {
			ApplicationFileFormat format;
			format.IsProtocol = false;
			format.Extension = sub->GetValueString(L"Extension");
			format.Description = sub->GetValueString(L"Description");
			format.CanCreate = sub->GetValueBoolean(L"CanCreate");
			format.Icon = 0;
			auto error = BuildIcon(ExpandPath(sub->GetValueString(L"Icon"), state.project_root_path), &format.Icon, console, true);
			if (error) return error;
			res_state.file_formats.Append(format);
		} else if (sub->GetValueString(L"Protocol").Length()) {
			ApplicationFileFormat format;
			format.IsProtocol = true;
			format.Extension = sub->GetValueString(L"Protocol");
			format.Description = sub->GetValueString(L"Description");
			format.CanCreate = false;
			format.Icon = 0;
			res_state.file_formats.Append(format);
		} else {
			if (!state.silent) console << TextColor(Console::ColorRed) << L"Unknown format definition." << TextColorDefault() << LineFeed();
			return ERTBT_INVALID_FORMAT_ALIAS;
		}
	}
	return ERTBT_SUCCESS;
}

void GenerateApplicationManifest(const string & at, Console & console)
{
	if (!state.clean) {
		try {
			FileStream out(at, AccessRead, OpenExisting);
			auto out_time = IO::DateTime::GetFileAlterTime(out.Handle());
			if (out_time > state.project_time) return;
		} catch (...) {}
	}
	if (!state.silent) console << L"Writing manifest file " << TextColor(Console::ColorCyan) << IO::Path::GetFileName(at) << TextColorDefault() << L"...";
	try {
		FileStream man_stream(at, AccessWrite, CreateAlways);
		TextWriter man(&man_stream, Encoding::UTF8);
		man.WriteLine(L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>");
		man.WriteLine(L"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">");
		man.WriteLine(L"<assemblyIdentity version=\"" + string(state.version_information.VersionMajor) + L"." + string(state.version_information.VersionMinor) + L"." +
			string(state.version_information.Subversion) + L"." + string(state.version_information.Build) + L"\" processorArchitecture=\"*\" name=\"" +
			EscapeStringXml(state.version_information.CompanyIdentifier) + L"." + EscapeStringXml(state.version_information.ApplicationIdentifier) +
			L"\" type=\"win32\"/>");
		man.WriteLine(L"<description>" + EscapeStringXml(state.version_information.ApplicationDescription) + L"</description>");
		// Common Controls 6.0 support
		man.WriteLine(L"<dependency><dependentAssembly>");
		man.WriteLine(L"\t<assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\" processorArchitecture=\"*\" "
			L"publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/>");
		man.WriteLine(L"</dependentAssembly></dependency>");
		// Make application DPI-aware, enable very long paths
		man.WriteLine(L"<application xmlns=\"urn:schemas-microsoft-com:asm.v3\">");
		if (!res_state.property_disable_hidpi) {
			man.WriteLine(L"\t<windowsSettings>");
			man.WriteLine(L"\t\t<dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">true</dpiAware>");
			man.WriteLine(L"\t</windowsSettings>");
		}
		man.WriteLine(L"\t<windowsSettings xmlns:ws2=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">");
		man.WriteLine(L"\t\t<ws2:longPathAware>true</ws2:longPathAware>");
		man.WriteLine(L"\t</windowsSettings>");
		man.WriteLine(L"</application>");
		// UAC Execution level
		man.WriteLine(L"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\"><security><requestedPrivileges>");
		if (res_state.property_needs_root_elevation) {
			man.WriteLine(L"\t<requestedExecutionLevel level=\"requireAdministrator\" uiAccess=\"FALSE\"></requestedExecutionLevel>");
		} else {
			man.WriteLine(L"\t<requestedExecutionLevel level=\"asInvoker\" uiAccess=\"FALSE\"></requestedExecutionLevel>");
		}
		man.WriteLine(L"</requestedPrivileges></security></trustInfo>");
		// Finilize
		man.WriteLine(L"</assembly>");
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
		throw;
	}
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
}
void GenerateResourceScript(const string & at, Console & console)
{
	if (!state.clean) {
		try {
			FileStream out(at, AccessRead, OpenExisting);
			auto out_time = IO::DateTime::GetFileAlterTime(out.Handle());
			if (out_time > state.project_time) return;
		} catch (...) {}
	}
	if (!state.silent) console << L"Writing resource script file " << TextColor(Console::ColorCyan) << IO::Path::GetFileName(at) << TextColorDefault() << L"...";
	try {
		FileStream script_stream(at, AccessWrite, CreateAlways);
		TextWriter script(&script_stream, Encoding::UTF16);
		script.WriteEncodingSignature();
		script.WriteLine(L"#include <Windows.h>");
		script.LineFeed();
		script.WriteLine(L"CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST \"" + EscapeString(res_state.resource_manifest_file) + L"\"");
		script.LineFeed();
		if (res_state.application_icon) script.WriteLine(L"1 ICON \"" + EscapeString(res_state.application_icon->ConvertedPath) + L"\"");
		if (res_state.file_formats.Length()) {
			int index = 2;
			for (auto & ff : res_state.file_formats) if (ff.Icon) {
				script.WriteLine(string(index) + L" ICON \"" + EscapeString(ff.Icon->ConvertedPath) + L"\"");
				index++;
			}
		}
		if (res_state.icon_database.Length()) script.LineFeed();
		if (res_state.resources.Length()) {
			for (auto & r : res_state.resources) {
				auto inner_name = r.Locale.Length() ? (r.Name + L"-" + r.Locale) : r.Name;
				script.WriteLine(inner_name + L" RCDATA \"" + EscapeString(r.SourcePath) + L"\"");
			}
			script.LineFeed();
		}
		if (state.version_information.ApplicationName.Length()) {
			auto is_library = configuration->GetValueBoolean(L"Library");
			script.WriteLine(L"1 VERSIONINFO");
			script.WriteLine(L"FILEVERSION " + string(state.version_information.VersionMajor) + L", " + string(state.version_information.VersionMinor) + L", " +
				string(state.version_information.Subversion) + L", " + string(state.version_information.Build));
			script.WriteLine(L"PRODUCTVERSION " + string(state.version_information.VersionMajor) + L", " + string(state.version_information.VersionMinor) + L", " +
				string(state.version_information.Subversion) + L", " + string(state.version_information.Build));
			script.WriteLine(L"FILEFLAGSMASK 0x3fL");
			script.WriteLine(L"FILEFLAGS 0x0L");
			script.WriteLine(L"FILEOS VOS_NT_WINDOWS32");
			if (is_library) {
				script.WriteLine(L"FILETYPE VFT_DLL");
			} else {
				script.WriteLine(L"FILETYPE VFT_APP");
			}
			script.WriteLine(L"FILESUBTYPE VFT2_UNKNOWN");
			script.WriteLine(L"BEGIN");
			script.WriteLine(L"\tBLOCK \"StringFileInfo\"");
			script.WriteLine(L"\tBEGIN");
			script.WriteLine(L"\t\tBLOCK \"040004b0\"");
			script.WriteLine(L"\t\tBEGIN");
			if (state.version_information.CompanyName.Length()) script.WriteLine(L"\t\t\tVALUE \"CompanyName\", \"" + EscapeString(state.version_information.CompanyName) + L"\"");
			script.WriteLine(L"\t\t\tVALUE \"FileDescription\", \"" + EscapeString(state.version_information.ApplicationName) + L"\"");
			script.WriteLine(L"\t\t\tVALUE \"FileVersion\", \"" + string(state.version_information.VersionMajor) + L"." + string(state.version_information.VersionMinor) + L"\"");
			script.WriteLine(L"\t\t\tVALUE \"InternalName\", \"" + EscapeString(state.version_information.InternalName) + L"\"");
			if (state.version_information.Copyright.Length()) script.WriteLine(L"\t\t\tVALUE \"LegalCopyright\", \"" + EscapeString(state.version_information.Copyright) + L"\"");
			if (is_library) {
				script.WriteLine(L"\t\t\tVALUE \"OriginalFilename\", \"" + EscapeString(state.version_information.InternalName) + L".dll\"");
			} else {
				script.WriteLine(L"\t\t\tVALUE \"OriginalFilename\", \"" + EscapeString(state.version_information.InternalName) + L".exe\"");
			}
			script.WriteLine(L"\t\t\tVALUE \"ProductName\", \"" + EscapeString(state.version_information.ApplicationName) + L"\"");
			script.WriteLine(L"\t\t\tVALUE \"ProductVersion\", \"" + string(state.version_information.VersionMajor) + L"." + string(state.version_information.VersionMinor) + L"\"");
			script.WriteLine(L"\t\tEND");
			script.WriteLine(L"\tEND");
			script.WriteLine(L"\tBLOCK \"VarFileInfo\"");
			script.WriteLine(L"\tBEGIN");
			script.WriteLine(L"\t\tVALUE \"Translation\", 0x400, 1200");
			script.WriteLine(L"\tEND");
			script.WriteLine(L"END");
		}
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
		throw;
	}
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
}
void GenerateFileFormatsManifest(const string & at, Console & console)
{
	if (!state.clean) {
		try {
			FileStream out(at, AccessRead, OpenExisting);
			auto out_time = IO::DateTime::GetFileAlterTime(out.Handle());
			if (out_time > state.project_time) return;
		} catch (...) {}
	}
	if (!state.silent) console << L"Writing file formats manifest " << TextColor(Console::ColorCyan) << IO::Path::GetFileName(at) << TextColorDefault() << L"...";
	try {
		SafePointer<Registry> manifest = CreateRegistry();
		for (auto & ff : res_state.file_formats) {
			auto name = ff.Extension.LowerCase();
			if (ff.IsProtocol) name += L":";
			manifest->CreateNode(name);
			SafePointer<RegistryNode> fn = manifest->OpenNode(name);
			fn->CreateValue(L"Description", RegistryValueType::String);
			fn->SetValue(L"Description", ff.Description);
			if (!ff.IsProtocol) {
				fn->CreateValue(L"IconIndex", RegistryValueType::Integer);
				fn->SetValue(L"IconIndex", ff.Icon ? ff.Icon->Reference.ToInt32() : -1);
				fn->CreateValue(L"CanCreate", RegistryValueType::Boolean);
				fn->SetValue(L"CanCreate", ff.CanCreate);
			}
		}
		FileStream manifest_stream(at, AccessReadWrite, CreateAlways);
		RegistryToText(manifest, &manifest_stream, Encoding::UTF8);
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
		throw;
	}
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
}

void GeneratePropertyList(const string & at, Console & console)
{
	if (!state.clean) {
		try {
			FileStream out(at, AccessRead, OpenExisting);
			auto out_time = IO::DateTime::GetFileAlterTime(out.Handle());
			if (out_time > state.project_time) return;
		} catch (...) {}
	}
	if (!state.silent) console << L"Writing bundle information file " << TextColor(Console::ColorCyan) << IO::Path::GetFileName(at) << TextColorDefault() << L"...";
	try {
		FileStream list_stream(at, AccessWrite, CreateAlways);
		TextWriter list(&list_stream, Encoding::UTF8);
		list.WriteLine(L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
		list.WriteLine(L"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
		list.WriteLine(L"<plist version=\"1.0\">");
		list.WriteLine(L"<dict>");
		if (!state.version_information.ApplicationName.Length()) state.version_information.ApplicationName = state.version_information.InternalName;
		list.WriteLine(L"\t<key>CFBundleName</key>");
		list.WriteLine(L"\t<string>" + EscapeStringXml(state.version_information.ApplicationName) + L"</string>");
		list.WriteLine(L"\t<key>CFBundleDisplayName</key>");
		list.WriteLine(L"\t<string>" + EscapeStringXml(state.version_information.ApplicationName) + L"</string>");
		list.WriteLine(L"\t<key>CFBundleIdentifier</key>");
		list.WriteLine(L"\t<string>com." + state.version_information.CompanyIdentifier + L"." + state.version_information.ApplicationIdentifier + L"</string>");
		list.WriteLine(L"\t<key>CFBundleVersion</key>");
		list.WriteLine(L"\t<string>" + string(state.version_information.VersionMajor) + L"." + string(state.version_information.VersionMinor) + L"." +
			string(state.version_information.Subversion) + L"." + string(state.version_information.Build) + L"</string>");
		list.WriteLine(L"\t<key>CFBundleDevelopmentRegion</key>");
		list.WriteLine(L"\t<string>en</string>");
		list.WriteLine(L"\t<key>CFBundlePackageType</key>");
		list.WriteLine(L"\t<string>APPL</string>");
		list.WriteLine(L"\t<key>CFBundleExecutable</key>");
		list.WriteLine(L"\t<string>" + EscapeStringXml(state.version_information.InternalName) + L"</string>");
		list.WriteLine(L"\t<key>CFBundleShortVersionString</key>");
		list.WriteLine(L"\t<string>" + string(state.version_information.VersionMajor) + L"." + string(state.version_information.VersionMinor) + L"</string>");
		if (res_state.application_icon) {
			list.WriteLine(L"\t<key>CFBundleIconFile</key>");
			list.WriteLine(L"\t<string>" + res_state.application_icon->Reference + L"</string>");
		}
		list.WriteLine(L"\t<key>NSPrincipalClass</key>");
		list.WriteLine(L"\t<string>NSApplication</string>");
		list.WriteLine(L"\t<key>CFBundleInfoDictionaryVersion</key>");
		list.WriteLine(L"\t<string>6.0</string>");
		list.WriteLine(L"\t<key>NSHumanReadableCopyright</key>");
		list.WriteLine(L"\t<string>" + EscapeStringXml(state.version_information.Copyright) + L"</string>");
		list.WriteLine(L"\t<key>LSMinimumSystemVersion</key>");
		list.WriteLine(L"\t<string>10.10</string>");
		if (res_state.property_disable_hidpi) {
			list.WriteLine(L"\t<key>NSHighResolutionCapable</key>");
			list.WriteLine(L"\t<false/>");
		} else {
			list.WriteLine(L"\t<key>NSHighResolutionMagnifyAllowed</key>");
			list.WriteLine(L"\t<false/>");
			list.WriteLine(L"\t<key>NSHighResolutionCapable</key>");
			list.WriteLine(L"\t<true/>");
		}
		if (res_state.property_disable_dock_icon) {
			list.WriteLine(L"\t<key>LSUIElement</key>");
			list.WriteLine(L"\t<true/>");
		}
		list.WriteLine(L"\t<key>CFBundleSupportedPlatforms</key>");
		list.WriteLine(L"\t<array>");
		list.WriteLine(L"\t\t<string>MacOSX</string>");
		list.WriteLine(L"\t</array>");
		if (res_state.file_formats.Length()) {
			int formats = 0, protocols = 0;
			for (auto & f : res_state.file_formats) if (f.IsProtocol) protocols++; else formats++;
			if (formats) {
				list.WriteLine(L"\t<key>CFBundleDocumentTypes</key>");
				list.WriteLine(L"\t<array>");
				for (auto & f : res_state.file_formats) if (!f.IsProtocol) {
					list.WriteLine(L"\t\t<dict>");
					list.WriteLine(L"\t\t\t<key>CFBundleTypeExtensions</key>");
					list.WriteLine(L"\t\t\t<array>");
					list.WriteLine(L"\t\t\t\t<string>" + EscapeStringXml(f.Extension) + L"</string>");
					list.WriteLine(L"\t\t\t</array>");
					list.WriteLine(L"\t\t\t<key>CFBundleTypeName</key>");
					list.WriteLine(L"\t\t\t<string>" + EscapeStringXml(f.Description) + L"</string>");
					list.WriteLine(L"\t\t\t<key>CFBundleTypeIconFile</key>");
					list.WriteLine(L"\t\t\t<string>" + EscapeStringXml(f.Icon->Reference) + L".icns</string>");
					list.WriteLine(L"\t\t\t<key>CFBundleTypeRole</key>");
					list.WriteLine(L"\t\t\t<string>" + string(f.CanCreate ? L"Editor" : L"Viewer") + L"</string>");
					list.WriteLine(L"\t\t</dict>");
				}
				list.WriteLine(L"\t</array>");
			}
			if (protocols) {
				list.WriteLine(L"\t<key>CFBundleURLTypes</key>");
				list.WriteLine(L"\t<array>");
				for (auto & f : res_state.file_formats) if (f.IsProtocol) {
					list.WriteLine(L"\t\t<dict>");
					list.WriteLine(L"\t\t\t<key>CFBundleURLSchemes</key>");
					list.WriteLine(L"\t\t\t<array>");
					list.WriteLine(L"\t\t\t\t<string>" + EscapeStringXml(f.Extension) + L"</string>");
					list.WriteLine(L"\t\t\t</array>");
					list.WriteLine(L"\t\t\t<key>CFBundleURLName</key>");
					list.WriteLine(L"\t\t\t<string>" + EscapeStringXml(f.Description) + L"</string>");
					list.WriteLine(L"\t\t\t<key>CFBundleTypeRole</key>");
					list.WriteLine(L"\t\t\t<string>Viewer</string>");
					list.WriteLine(L"\t\t</dict>");
				}
				list.WriteLine(L"\t</array>");
			}
		}
		list.WriteLine(L"</dict>");
		list.WriteLine(L"</plist>");
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
		throw;
	}
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
}
int BuildBundle(Console & console)
{
	auto bundle = IO::ExpandPath(state.project_output_root + L"/" + state.project_output_name + L".app");
	if (!state.silent) console << L"Building Mac OS Application Bundle " << TextColor(Console::ColorCyan) << IO::Path::GetFileName(bundle) << TextColorDefault() << L"...";
	IO::CreateDirectoryTree(bundle);
	ClearDirectory(bundle);
	IO::CreateDirectoryTree(bundle + L"/Contents/MacOS");
	IO::CreateDirectoryTree(bundle + L"/Contents/Resources");
	Array<string> locales(0x10);
	locales << L"en";
	for (auto & r : res_state.resources) {
		if (!r.Locale.Length()) continue;
		bool added = false;
		for (auto & l : locales) if (string::CompareIgnoreCase(l, r.Locale) == 0) { added = true; break; }
		if (!added) locales << r.Locale;
	}
	for (auto & l : locales) IO::CreateDirectoryTree(bundle + L"/Contents/Resources/" + l + L".lproj");
	try {
		if (!CopyFile(res_state.resource_manifest_file, bundle + L"/Contents/Info.plist")) throw Exception();
		for (auto & i : res_state.icon_database) if (!CopyFile(i.ConvertedPath, bundle + L"/Contents/Resources/" + i.Reference + L".icns")) throw Exception();
		for (auto & r : res_state.resources) {
			auto inner_name = r.Locale.Length() ? (r.Name + L"-" + r.Locale) : r.Name;
			if (!CopyFile(r.SourcePath, bundle + L"/Contents/Resources/" + inner_name)) {
				if (!state.silent) {
					console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
					console << TextColor(Console::ColorRed) << FormatString(L"Failed to import resource file \"%0\".", r.SourcePath) << TextColorDefault() << LineFeed();
				}
				return ERTBT_BUNDLE_BUILD_ERROR;
			}
		}
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Failed" << TextColorDefault() << LineFeed();
		return ERTBT_BUNDLE_BUILD_ERROR;
	}
	if (!state.silent) console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
	return ERTBT_SUCCESS;
}

int Main(void)
{
	UI::Windows::InitializeCodecCollection();
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
		if (state.project_file_path.Length()) {
			error = LoadProject(console);
			if (error) return error;
			MakeLocalConfiguration(console);
			error = LoadVersionInformation(console);
			if (error) return error;
			ProjectPostConfig();
			configuration = local_config->OpenNode(L"Resource");
			if (!configuration) {
				if (!state.silent) console << TextColor(Console::ColorRed) << L"Resource tool configuration is invalid." << TextColorDefault() << LineFeed();
				return ERTBT_INVALID_CONFIGURATION;
			}
			if (configuration->GetValueBoolean(L"Windows")) res_state.mode = ResourceMode::Windows;
			else if (configuration->GetValueBoolean(L"MacOSX")) res_state.mode = ResourceMode::MacOSX;
			else {
				if (!state.silent) console << TextColor(Console::ColorRed) << L"Resource tool configuration is invalid." << TextColorDefault() << LineFeed();
				return ERTBT_INVALID_CONFIGURATION;
			}
			res_state.property_disable_hidpi = state.project->GetValueBoolean(L"NoHiDPI");
			res_state.property_disable_dock_icon = state.project->GetValueBoolean(L"NoDockIcon");
			res_state.property_needs_root_elevation = state.project->GetValueBoolean(L"NeedsElevation");
			auto app_icon_path = state.project->GetValueString(L"ApplicationIcon");
			if (app_icon_path.Length()) {
				app_icon_path = ExpandPath(app_icon_path, state.project_root_path);
				error = BuildIcon(app_icon_path, &res_state.application_icon, console);
				if (error) return error;
			}
			error = ExtractResources(console);
			if (error) return error;
			error = ExtractFileFormats(console);
			if (error) return error;
			if (res_state.mode == ResourceMode::Windows) {
				res_state.resource_manifest_file = IO::ExpandPath(state.project_object_path + L"/" + state.project_output_name + L".manifest");
				res_state.resource_script_file = IO::ExpandPath(state.project_object_path + L"/" + state.project_output_name + L".rc");
				res_state.resource_object_file = IO::ExpandPath(state.project_object_path + L"/" + state.project_output_name + L".res");
				res_state.resource_object_file_log = IO::ExpandPath(state.project_object_path + L"/" + state.project_output_name + L".log");
				res_state.resource_file_formats_file = IO::ExpandPath(state.project_object_path + L"/" + state.project_output_name + L".formats.ini");
				GenerateApplicationManifest(res_state.resource_manifest_file, console);
				GenerateResourceScript(res_state.resource_script_file, console);
				if (res_state.file_formats.Length()) GenerateFileFormatsManifest(res_state.resource_file_formats_file, console);

				// TODO: COMPILE RES [WINDOWS ONLY]
//                 string res = args->ElementAt(2) + L"/" + IO::Path::GetFileNameWithoutExtension(args->ElementAt(1)) + L".res";
//                 string res_log = args->ElementAt(2) + L"/" + IO::Path::GetFileNameWithoutExtension(args->ElementAt(1)) + L".log";
//                 if (!compile_resource(rc, res, res_log, console)) return 1;
			} else if (res_state.mode == ResourceMode::MacOSX) {
				res_state.resource_manifest_file = IO::ExpandPath(state.project_object_path + L"/" + state.project_output_name + L".plist");
				GeneratePropertyList(res_state.resource_manifest_file, console);
				return BuildBundle(console);
			}
		} else if (!state.silent) {
			console << L"Command line syntax:" << LineFeed();
			console << L"  " << ENGINE_VI_APPSYSNAME << L" <project.ini> :CENSaco" << LineFeed();
			console << L"Where project.ini is the project configuration file." << LineFeed();
			console << L"You can optionally use the next build options:" << LineFeed();
			console << L"  :C - clean build, rebuild any cached files," << LineFeed();
			console << L"  :E - use shell error mode - open error logs in an external editor," << LineFeed();
			console << L"  :N - use no logo mode - don't print application logo," << LineFeed();
			console << L"  :S - use silent mode - supress any output, except output path," << LineFeed();
			console << L"  :a - specify processor architecture (as the next argument)," << LineFeed();
			console << L"  :c - specify target configuration (as the next argument)," << LineFeed();
			console << L"  :o - specify target operating system (as the next argument)." << LineFeed();
			console << LineFeed();
		}
	} catch (Exception & e) {
		if (!state.silent) console << TextColor(Console::ColorRed) << FormatString(L"Resource tool failed: %0.", e.ToString()) << TextColorDefault() << LineFeed();
		return ERTBT_COMMON_EXCEPTION;
	} catch (...) {
		if (!state.silent) console << TextColor(Console::ColorRed) << L"Resource tool failed: Unknown exception." << TextColorDefault() << LineFeed();
		return ERTBT_UNCOMMON_EXCEPTION;
	}
	return ERTBT_SUCCESS;
}

// bool compile_resource(const string & rc, const string & res, const string & log, ITextWriter & console)
// {
//     try {
//         Time max_time = 0;
//         Time rc_time = 0;
//         string wd = IO::Path::GetDirectory(rc) + L"/";
//         try {
//             FileStream date(wd + manifest, AccessRead, OpenExisting);
//             FileStream rcs(wd + rc, AccessRead, OpenExisting);
//             max_time = IO::DateTime::GetFileAlterTime(date.Handle());
//             for (int i = 0; i < icons.Length(); i++) {
//                 FileStream icon(wd + icons[i], AccessRead, OpenExisting);
//                 Time it = IO::DateTime::GetFileAlterTime(icon.Handle());
//                 if (it > max_time) max_time = it;
//             }
//             for (int i = 0; i < reslist.Length(); i++) {
//                 FileStream res(reslist[i].SourcePath, AccessRead, OpenExisting);
//                 Time rt = IO::DateTime::GetFileAlterTime(res.Handle());
//                 if (rt > max_time) max_time = rt;
//             }
//             if (prj_time > max_time) max_time = prj_time;
//             rc_time = IO::DateTime::GetFileAlterTime(rcs.Handle());
//         }
//         catch (...) {}
//         if (max_time < rc_time && !clean) return true;
//         !----------------------------------
//         Time src_time = 0;
//         Time out_time = 0;
//         try {
//             FileStream src(rc, AccessRead, OpenExisting);
//             FileStream out(res, AccessRead, OpenExisting);
//             src_time = IO::DateTime::GetFileAlterTime(src.Handle());
//             out_time = IO::DateTime::GetFileAlterTime(out.Handle());
//         }
//         catch (...) {}
//         if (src_time < out_time && !clean) return true;
//         console << L"Compiling resource script " << IO::Path::GetFileName(rc) << L"...";
//         Array<string> rc_args(0x80);
//         {
//             string out_arg = sys_cfg->GetValueString(L"Compiler/OutputArgument");
//             if (out_arg.FindFirst(L'$') == -1) {
//                 rc_args << out_arg;
//                 rc_args << res;
//             } else rc_args << out_arg.Replace(L'$', res);
//         }
//         {
//             SafePointer<RegistryNode> args_node = sys_cfg->OpenNode(L"Compiler/Arguments");
//             if (args_node) {
//                 auto & args_vals = args_node->GetValues();
//                 for (int i = 0; i < args_vals.Length(); i++) rc_args << args_node->GetValueString(args_vals[i]);
//             }
//         }
//         rc_args << rc;
//         handle rc_log = IO::CreateFile(log, IO::AccessReadWrite, IO::CreateAlways);
//         IO::SetStandardOutput(rc_log);
//         IO::SetStandardError(rc_log);
//         IO::CloseFile(rc_log);
//         SafePointer<Process> compiler = CreateCommandProcess(sys_cfg->GetValueString(L"Compiler/Path"), &rc_args);
//         if (!compiler) {
//             console << L"Failed" << IO::NewLineChar;
//             console << L"Failed to launch the compiler (" + sys_cfg->GetValueString(L"Compiler/Path") + L")." << IO::NewLineChar;
//             return false;
//         }
//         compiler->Wait();
//         if (compiler->GetExitCode()) {
//             console << L"Failed" << IO::NewLineChar;
//             if (errlog) print_error(IO::GetStandardError());
//             else Shell::OpenFile(log);
//             return false;
//         }
//         console << L"Succeed" << IO::NewLineChar;
//     }
//     catch (...) { return false; }
//     return true;
// }