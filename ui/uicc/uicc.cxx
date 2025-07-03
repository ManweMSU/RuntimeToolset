#include <EngineRuntime.h>

#include "uiml.h"

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;

struct {
	bool silent = false;
	bool warnings_as_errors = false;
	bool supress_warnings = false;
	bool no_strings = false;
	bool no_colors = false;
	bool style_output = false;
	bool time_estimate = false;
	string input_file;
	string output_file;
	Array<string> preload_list = Array<string>(0x10);
} state;

void PrintError(Console & console, const string & file, int offs, int len, bool warning = false)
{
	try {
		FileStream src(file, AccessRead, OpenExisting);
		TextReader reader(&src);
		string text;
		DynamicString result;
		while (!reader.EofReached()) {
			auto line = reader.ReadLine();
			if (line.Length() > 1 && line[0] == L'#' && line[1] == L'#') result << L'\n';
			else result << line << L'\n';
		}
		text = result.ToString();
		int lb = offs;
		int le = offs;
		int ln = 1;
		for (int i = offs; i >= 0; i--) if (text[i] == L'\n') ln++;
		while (lb && (text[lb - 1] >= 32 || text[lb - 1] == L'\t')) lb--;
		while (text[lb] == L' ' || text[lb] == L'\t') lb++;
		while (le < text.Length() - 1 && (text[le + 1] >= 32 || text[le + 1] == L'\t')) le++;
		string line = text.Fragment(lb, le - lb + 1).Replace(L'\t', L' ');
		string pref = string(warning ? L"Warning on " : L"Error on ") + L"line #" + string(ln) + L": ";
		console << pref;
		console << (warning ? TextColor(ConsoleColor::Yellow) : TextColor(ConsoleColor::Red));
		console << line << TextColorDefault() << LineFeed();
		console << string(L' ', line.Fragment(0, offs - lb).GetEncodedLength(Encoding::UTF32) + pref.Length());
		console << (warning ? TextColor(ConsoleColor::DarkYellow) : TextColor(ConsoleColor::DarkRed));
		console << L"^";
		if (len > 1) console << string(L'~', line.Fragment(offs - lb, len).GetEncodedLength(Encoding::UTF32) - 1);
		console << TextColorDefault() << LineFeed() << LineFeed();
	} catch (...) {}
}

class Verifyier : public UI::IResourceResolver, public UI::Format::IMissingStylesReporter
{
public:
	Console & console;
	bool OK;
	Verifyier(Console & cns) : console(cns), OK(true) {}

	virtual Graphics::IBitmap * GetTexture(const string & Name) override
	{
		if (!state.silent && !state.supress_warnings) {
			if (OK) console << LineFeed();
			console << TextColor(ConsoleColor::Blue);
			console << L"Warning: referenced but not defined texture object: \"" << Name << L"\"." << LineFeed();
			console << TextColorDefault();
		}
		OK = false;
		return 0;
	}
	virtual Graphics::IFont * GetFont(const string & Name) override
	{
		if (!state.silent && !state.supress_warnings) {
			if (OK) console << LineFeed();
			console << TextColor(ConsoleColor::Blue);
			console << L"Warning: referenced but not defined font object: \"" << Name << L"\"." << LineFeed();
			console << TextColorDefault();
		}
		OK = false;
		return 0;
	}
	virtual UI::Template::Shape * GetApplication(const string & Name) override
	{
		if (!state.silent && !state.supress_warnings) {
			if (OK) console << LineFeed();
			console << TextColor(ConsoleColor::Blue);
			console << L"Warning: referenced but not defined application object: \"" << Name << L"\"." << LineFeed();
			console << TextColorDefault();
		}
		OK = false;
		return 0;
	}
	virtual UI::Template::ControlTemplate * GetDialog(const string & Name) override
	{
		if (!state.silent && !state.supress_warnings) {
			if (OK) console << LineFeed();
			console << TextColor(ConsoleColor::Blue);
			console << L"Warning: referenced but not defined dialog object: \"" << Name << L"\"." << LineFeed();
			console << TextColorDefault();
		}
		OK = false;
		return 0;
	}
	virtual UI::Template::ControlReflectedBase * CreateCustomTemplate(const string & Class) override { return 0; }
	virtual void ReportStyleIsMissing(const string & Name, const string & Class) override
	{
		if (!state.silent && !state.supress_warnings) {
			if (OK) console << LineFeed();
			console << TextColor(ConsoleColor::Blue);
			console << L"Warning: referenced but not defined style object: \"" << Name << L"\" for control class \"" + Class + "\"." << LineFeed();
			console << TextColorDefault();
		}
		OK = false;
	}
};
class WarningCollector : public UI::Markup::IWarningReporter
{
public:
	Array<UI::Markup::WarningClass> classes = Array<UI::Markup::WarningClass>(0x10);
	Array<int> positions = Array<int>(0x10);
	Array<int> lengths = Array<int>(0x10);
	Array<string> infos = Array<string>(0x10);
	virtual void ReportWarning(UI::Markup::WarningClass warning_class, int warning_pos, int warning_length, const string & warning_info) override
	{ classes << warning_class; positions << warning_pos; lengths << warning_length; infos << warning_info; }
};

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
				if (arg == L'S') {
					state.silent = true;
				} else if (arg == L'W') {
					state.warnings_as_errors = true;
				} else if (arg == L'c') {
					state.no_colors = true;
				} else if (arg == L'l') {
					state.style_output = true;
				} else if (arg == L'o') {
					if (i < args->Length()) {
						if (state.output_file.Length()) {
							console << TextColor(ConsoleColor::Yellow) << FormatString(L"Output name redefinition.", args->ElementAt(i)) << TextColorDefault() << LineFeed();
							return false;
						}
						state.output_file = IO::ExpandPath(args->ElementAt(i));
						i++;
					} else {
						console << TextColor(ConsoleColor::Yellow) << L"Invalid command line: argument expected." << TextColorDefault() << LineFeed();
						return false;
					}
				} else if (arg == L'p') {
					if (i < args->Length()) {
						state.preload_list << IO::ExpandPath(args->ElementAt(i));
						i++;
					} else {
						console << TextColor(ConsoleColor::Yellow) << L"Invalid command line: argument expected." << TextColorDefault() << LineFeed();
						return false;
					}
				} else if (arg == L's') {
					state.no_strings = true;
				} else if (arg == L't') {
					state.time_estimate = true;
				} else if (arg == L'w') {
					state.supress_warnings = true;
				} else {
					console << TextColor(ConsoleColor::Yellow) << FormatString(L"Command line argument \"%0\" is invalid.", string(arg, 1)) << TextColorDefault() << LineFeed();
					return false;
				}
			}
		} else {
			if (state.input_file.Length()) {
				console << TextColor(ConsoleColor::Yellow) << L"Duplicate input file argument on command line." << TextColorDefault() << LineFeed();
				return false;
			}
			state.input_file = IO::ExpandPath(cmd);
			i++;
		}
	}
	return true;
}

int Main(void)
{
	Console console;
	if (!ParseCommandLine(console)) return 1;
	if (state.input_file.Length()) {
		Codec::InitializeDefaultCodecs();
		UI::CurrentScaleFactor = 1.0;
		Assembly::CurrentLocale = L"en";
		uint32 time = GetTimerValue();
		#ifdef ENGINE_WINDOWS
		string system = L"Windows";
		#endif
		#ifdef ENGINE_MACOSX
		string system = L"MacOSX";
		#endif
		#ifdef ENGINE_LINUX
		string system = L"Linux";
		#endif
		if (!state.output_file.Length()) {
			if (state.style_output) state.output_file = IO::Path::GetDirectory(state.input_file) +
				string(IO::PathDirectorySeparator) + IO::Path::GetFileNameWithoutExtension(state.input_file) + L".estl";
			else state.output_file = IO::Path::GetDirectory(state.input_file) +
				string(IO::PathDirectorySeparator) + IO::Path::GetFileNameWithoutExtension(state.input_file) + L".eui";
		}
		SafePointer<WarningCollector> collector = new WarningCollector;
		UI::Markup::SetWarningReporterCallback(collector);
		if (!state.silent) {
			console << L"Compiling " << TextColor(ConsoleColor::Cyan) << IO::Path::GetFileName(state.input_file) << TextColorDefault() << L"...";
		}
		UI::Markup::ErrorClass error;
		int error_offset, error_length;
		string error_file;
		string inc;
		try {
			FileStream reg_src(IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/uicc.ini", AccessRead, OpenExisting);
			SafePointer<Storage::Registry> reg = Storage::CompileTextRegistry(&reg_src);
			if (!reg) {
				FileStream reg_binary_src(IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/uicc.ecs", AccessRead, OpenExisting);
				reg = Storage::LoadRegistry(&reg_binary_src);
			}
			if (reg) {
				inc = IO::ExpandPath(IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/" + reg->GetValueString(L"Include"));
				SafePointer<Storage::RegistryNode> subordering_node = reg->OpenNode(L"Subordering");
				if (subordering_node) UI::Markup::SetSuborderingTable(subordering_node);
			}
		} catch (...) {}
		SafePointer<UI::Format::InterfaceTemplateImage> image =
			UI::Markup::CompileInterface(state.input_file, state.style_output, error_offset, error_length, error, error_file, inc);
		if (error != UI::Markup::ErrorClass::OK) {
			if (!state.silent) {
				console << TextColor(ConsoleColor::Red) << L"Failed" << LineFeed() << LineFeed();
				console << TextColorDefault();
				if (error_offset >= 0) PrintError(console, state.input_file, error_offset, error_length);
				console << TextColor(ConsoleColor::Red);
				if (error == UI::Markup::ErrorClass::ObjectRedifinition) console << L"Object redifinition.";
				else if (error == UI::Markup::ErrorClass::SourceAccess) console << L"Failed to open a file: \"" << error_file << L"\".";
				else if (error == UI::Markup::ErrorClass::UndefinedObject) console << L"Undefined object.";
				else if (error == UI::Markup::ErrorClass::UnexpectedLexem) console << L"Another token expected.";
				else if (error == UI::Markup::ErrorClass::NumericConstantTypeMismatch) console << L"Numeric type mismatch.";
				else if (error == UI::Markup::ErrorClass::MainInvalidToken) console << L"Invalid token.";
				else if (error == UI::Markup::ErrorClass::InvalidSystemColor) console << L"Unknown system color identifier.";
				else if (error == UI::Markup::ErrorClass::InvalidProperty) console << L"Unknown property identifier.";
				else if (error == UI::Markup::ErrorClass::InvalidLocaleIdentifier) console << L"Invalid locale identifier.";
				else if (error == UI::Markup::ErrorClass::InvalidEffect) console << L"Unknown effect identifier.";
				else if (error == UI::Markup::ErrorClass::InvalidConstantType) console << L"Invalid constant type.";
				else if (error == UI::Markup::ErrorClass::IncludedInvalidToken) console << L"Invalid token in file \"" << error_file << L"\".";
				else console << L"Unknown error.";
				console << TextColorDefault() << LineFeed() << LineFeed();
			}
			return 1;
		}
		if (!state.silent) console << TextColor(ConsoleColor::Green) << L"Succeed" << TextColorDefault() << LineFeed();
		for (int i = 0; i < collector->classes.Length(); i++) {
			if (!state.silent && !state.supress_warnings) {
				if (collector->positions[i] >= 0) PrintError(console, state.input_file, collector->positions[i], collector->lengths[i], true);
				if (state.warnings_as_errors) console << TextColor(ConsoleColor::Red);
				else console << TextColor(ConsoleColor::Yellow);
				if (collector->classes[i] == UI::Markup::WarningClass::InvalidControlParent) {
					Array<string> cls = collector->infos[i].Split(L',');
					console << L"Control with class \"" << cls[0] << L"\" is not assumed to be a child of \"" << cls[1] << "\".";
				} else if (collector->classes[i] == UI::Markup::WarningClass::UnknownPlatformName) {
					console << L"Unknown platform name: " << collector->infos[i] << L".";
				} else console << L"Unknown warning.";
				console << TextColorDefault() << LineFeed();
			}
			if (state.warnings_as_errors) return 1;
		}
		if (!state.silent) console << L"Verifying the image...";
		try {
			Verifyier ver(console);
			bool ver_undone = false;
			UI::InterfaceTemplate interface;
			UI::InterfaceTemplate preloaded_interface;
			SafePointer<UI::Format::InterfaceTemplateImage> clone = image->Clone();
			clone->Specialize(L"", system, 0.0);
			if (state.preload_list.Length()) {
				for (auto & p : state.preload_list) {
					try {
						FileStream preload_stream(p, AccessRead, OpenExisting);
						SafePointer<UI::Format::InterfaceTemplateImage> preload_image = new UI::Format::InterfaceTemplateImage(&preload_stream, L"", system, 0.0);
						preload_image->Compile(preloaded_interface);
						clone->Compile(interface, preloaded_interface, 0, &ver, &ver);
					} catch (...) {
						ver_undone = true;
						ver.OK = false;
						if (!state.silent && !state.supress_warnings) {
							console << TextColor(ConsoleColor::Blue) << LineFeed();
							console << "Failed to load preloadable asset. Check the command line." << LineFeed() << TextColorDefault();
						}
					}
				}
			} else clone->Compile(interface, 0, &ver, &ver);
			if (ver.OK) {
				if (!state.silent) console << TextColor(ConsoleColor::Green) << L"Succeed" << TextColorDefault() << LineFeed();
			} else {
				if (!ver_undone && !state.silent) {
					if (state.supress_warnings) {
						console << TextColor(ConsoleColor::Yellow) << L"Reference issues found" << TextColorDefault() << LineFeed();
					} else {
						console << TextColor(ConsoleColor::Yellow) << L"All the reference issues may result in errors. Source verification is recommended.";
						console << TextColorDefault() << LineFeed() << LineFeed();
					}
				}
			}
		} catch (...) { if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Failed to compile the image" << TextColorDefault() << LineFeed() << LineFeed(); }
		if (!state.silent) console << L"Encoding " << TextColor(ConsoleColor::Cyan) << IO::Path::GetFileName(state.output_file) << TextColorDefault() << L"...";
		try {
			if (state.no_colors) {
				for (int i = 0; i < image->Assets.Length(); i++) for (int j = 0; j < image->Assets[i].Colors.Length(); j++) image->Assets[i].Colors[j].Name = L"";
			}
			uint32 flags = state.no_strings ? 0 : UI::Format::EncodeFlags::EncodeStringNames;
			FileStream Dest(state.output_file, AccessReadWrite, CreateAlways);
			image->Encode(&Dest, flags);
		}
		catch (...) {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Failed" << TextColorDefault() << LineFeed() << LineFeed();
			return 1;
		}
		if (!state.silent) console << TextColor(ConsoleColor::Green) << L"Succeed" << TextColorDefault() << LineFeed();
		if (!state.silent && state.time_estimate) {
			console << LineFeed() << L"Done in " << TextColor(ConsoleColor::Magenta) << string(GetTimerValue() - time) << TextColorDefault() << L" ms." << LineFeed() << LineFeed();
		}
	} else {
		console << ENGINE_VI_APPNAME << LineFeed();
		console << L"Copyright " << string(ENGINE_VI_COPYRIGHT).Replace(L'\xA9', L"(C)") << LineFeed();
		console << L"Version " << ENGINE_VI_APPVERSION << L", build " << ENGINE_VI_BUILD << LineFeed() << LineFeed();
		console << L"Command line syntax:" << LineFeed();
		console << L"  " << ENGINE_VI_APPSYSNAME << L" <source.uiml> :SWclopsw" << LineFeed();
		console << L"Where source.uiml is the input UIML source file." << LineFeed();
		console << L"You can optionally use the next compile options:" << LineFeed();
		console << L"  :S - use silent mode - supress any text output," << LineFeed();
		console << L"  :W - interpret warnings as errors," << LineFeed();
		console << L"  :c - don't include color constants into the output," << LineFeed();
		console << L"  :l - compile style library instead of template," << LineFeed();
		console << L"  :o - redirect the output to a file specified (as the next argument)," << LineFeed();
		console << L"  :p - specify an interface image to preload (as the next argument)," << LineFeed();
		console << L"  :s - don't include string constants into the output," << LineFeed();
		console << L"  :t - perform time estimations," << LineFeed();
		console << L"  :w - supress any warnings and consistency checks." << LineFeed();
		console << LineFeed();
	}
	return 0;
}