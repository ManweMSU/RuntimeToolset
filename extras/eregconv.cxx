#include <EngineRuntime.h>
#include <Storage/TextRegistryGrammar.h>

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Storage;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;

enum class Format { NOP, Binary, Text };

struct {
	bool silent = false;
	bool nologo = false;
	bool print = false;
	Format format = Format::NOP;
	Encoding encoding = Encoding::UTF8;
	string input;
	string output;
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
				} else if (o == L'f') {
					if (state.format != Format::NOP) {
						console << TextColor(ConsoleColor::Red) << L"Format redefinition." << TextColorDefault() << LineFeed();
						return false;
					}
					if (i < args->Length() - 1) {
						i++;
						if (string::CompareIgnoreCase(args->ElementAt(i), L"bin") == 0) {
							state.format = Format::Binary;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"ascii") == 0) {
							state.format = Format::Text;
							state.encoding = Encoding::ANSI;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"ansi") == 0) {
							state.format = Format::Text;
							state.encoding = Encoding::ANSI;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"utf8") == 0) {
							state.format = Format::Text;
							state.encoding = Encoding::UTF8;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"utf16") == 0) {
							state.format = Format::Text;
							state.encoding = Encoding::UTF16;
						} else if (string::CompareIgnoreCase(args->ElementAt(i), L"utf32") == 0) {
							state.format = Format::Text;
							state.encoding = Encoding::UTF32;
						} else {
							console << TextColor(ConsoleColor::Red) << L"Unknown registry format name." << TextColorDefault() << LineFeed();
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
				} else if (o == L'v') {
					state.print = true;
				} else {
					console << TextColor(ConsoleColor::Red) << L"Invalid command line option." << TextColorDefault() << LineFeed();
					return false;
				}
			}
		} else {
			if (state.input.Length()) {
				console << TextColor(ConsoleColor::Red) << L"Input redefinition." << TextColorDefault() << LineFeed();
				return false;
			}
			state.input = args->ElementAt(i);
		}
	}
	return true;
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
			console << L"  " << ENGINE_VI_APPSYSNAME << L" <registry> :NSfov" << LineFeed();
			console << L"Where" << LineFeed();
			console << L"  registry - source binary or text registry file," << LineFeed();
			console << L"  :N       - use no logo mode - don't print application logo," << LineFeed();
			console << L"  :S       - use silent mode - supress any non-explicit output," << LineFeed();
			console << L"  :f       - format used to encode the registry," << LineFeed();
			console << L"               one of the following values must be passed as the next argument:" << LineFeed();
			console << L"               bin   - binary registry," << LineFeed();
			console << L"               ascii - ASCII text notation," << LineFeed();
			console << L"               utf8  - UTF8 text notation," << LineFeed();
			console << L"               utf16 - UTF16-LE text notation," << LineFeed();
			console << L"               utf32 - UTF32-LE text notation," << LineFeed();
			console << L"  :o       - overrides the name of the output file, must be used with :f," << LineFeed();
			console << L"  :v       - prints the text notation into the standard output." << LineFeed();
			console << LineFeed();
		}
	} else {
		SafePointer<Registry> reg;
		SafePointer< Array<Syntax::Token> > tokens;
		string inner;
		try {
			state.input = IO::ExpandPath(state.input);
			FileStream file(state.input, AccessRead, OpenExisting);
			reg = LoadRegistry(&file);
			if (!reg) {
				file.Seek(0, Begin);
				reg = CompileTextRegistry(&file);
				if (!reg) {
					file.Seek(0, Begin);
					TextReader reader(&file);
					inner = reader.ReadAll();
					Syntax::Spelling spelling;
					Syntax::Grammar grammar;
					CreateTextRegistryGrammar(grammar);
					CreateTextRegistrySpelling(spelling);
					tokens = Syntax::ParseText(inner, spelling);
					SafePointer< Syntax::SyntaxTree > tree = new Syntax::SyntaxTree(*tokens, grammar);
					throw InvalidFormatException();
				}
			}
		} catch (Syntax::ParserSpellingException & e) {
			if (!state.silent) {
				console << TextColor(ConsoleColor::Red) << L"Syntax error: " << e.Comments << TextColorDefault() << LineFeed();
				int line = 1;
				int lbegin = 0;
				for (int i = 0; i < e.Position; i++) if (inner[i] == L'\n') { line++; lbegin = i + 1; }
				int lend = e.Position;
				while (lend < inner.Length() && inner[lend] != L'\n') lend++;
				if (lend < inner.Length()) lend--;
				string errstr = inner.Fragment(lbegin, lend - lbegin).Replace(L'\t', L' ');
				console << TextColor(ConsoleColor::Red) << errstr << TextColorDefault() << LineFeed();
				int posrel = e.Position - lbegin;
				console << TextColor(ConsoleColor::DarkRed);
				for (int i = 0; i < posrel; i++) console << L" ";
				console << L"^" << TextColorDefault() << LineFeed();
				console << TextColor(ConsoleColor::Red) << L"At line #" << line << TextColorDefault() << LineFeed();
			}
			return 1;
		} catch (Syntax::ParserSyntaxException & e) {
			if (!state.silent) {
				console << TextColor(ConsoleColor::Red) << L"Syntax error: " << e.Comments << TextColorDefault() << LineFeed();
				int token_begin = tokens->ElementAt(e.Position).SourcePosition;
				int token_end = inner.Length();
				if (e.Position < tokens->Length() - 1) token_end = tokens->ElementAt(e.Position + 1).SourcePosition;
				int line = 1;
				int lbegin = 0;
				for (int i = 0; i < token_begin; i++) if (inner[i] == L'\n') { line++; lbegin = i + 1; }
				int lend = token_begin;
				while (lend < inner.Length() && inner[lend] != L'\n') lend++;
				if (lend < inner.Length()) lend--;
				string errstr = inner.Fragment(lbegin, lend - lbegin).Replace(L'\t', L' ');
				console << TextColor(ConsoleColor::Red) << errstr << TextColorDefault() << LineFeed();
				int posrel = token_begin - lbegin;
				console << TextColor(ConsoleColor::DarkRed);
				for (int i = 0; i < posrel; i++) console << L" ";
				console << L"^";
				for (int i = 0; i < token_end - token_begin - 1; i++) console << L"~";
				console << TextColorDefault() << LineFeed();
				console << TextColor(ConsoleColor::Red) << L"At line #" << line << TextColorDefault() << LineFeed();
			}
			return 1;
		} catch (InvalidFormatException & e) {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Invalid input file format." << TextColorDefault() << LineFeed();
			return 1;
		} catch (IO::FileAccessException & e) {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Error accessing the input file." << TextColorDefault() << LineFeed();
			return 1;
		} catch (...) {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Internal application error." << TextColorDefault() << LineFeed();
			return 1;
		}
		try {
			if (state.print) RegistryToText(reg, &console);
			if (state.format != Format::NOP) {
				if (!state.output.Length()) {
					auto suffix = state.format == Format::Binary ? L".ecs" : L".ini";
					auto serial = 0;
					state.output = IO::ExpandPath(IO::Path::GetDirectory(state.input) + L"/" + IO::Path::GetFileNameWithoutExtension(state.input));
					while (true) {
						string out;
						if (serial) out = state.output + L"." + string(uint(serial), DecimalBase, 3) + suffix;
						else out = state.output + suffix;
						try { IO::GetFileType(out); serial++; } catch (...) { state.output = out; break; }
					}
				}
				if (state.format == Format::Text) {
					if (!state.silent) console << L"Writing text registry...";
					FileStream file(state.output, AccessReadWrite, CreateAlways);
					TextWriter writer(&file, state.encoding);
					writer.WriteEncodingSignature();
					RegistryToText(reg, &writer);
					if (!state.silent) console << TextColor(ConsoleColor::Green) << L"Succeed" << TextColorDefault() << LineFeed();
				} else if (state.format == Format::Binary) {
					if (!state.silent) console << L"Writing binary registry...";
					FileStream file(state.output, AccessReadWrite, CreateAlways);
					reg->Save(&file);
					if (!state.silent) console << TextColor(ConsoleColor::Green) << L"Succeed" << TextColorDefault() << LineFeed();
				}
			}
		} catch (IO::FileAccessException & e) {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Error accessing the output file." << TextColorDefault() << LineFeed();
			return 1;
		} catch (...) {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Internal application error." << TextColorDefault() << LineFeed();
			return 1;
		}
	}
	return 0;
}