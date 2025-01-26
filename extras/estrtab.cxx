#include <EngineRuntime.h>

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Storage;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;

enum class Format { NOP, Binary, Text };

StringTable * compile_string_table(Stream * stream)
{
	SafePointer<StringTable> table = new StringTable;
	try {
		stream->Seek(0, Begin);
		TextReader reader(stream);
		Syntax::Spelling spelling;
		spelling.CommentEndOfLineWord = L"//";
		spelling.CommentBlockOpeningWord = L"/*";
		spelling.CommentBlockClosingWord = L"*/";
		SafePointer< Array<Syntax::Token> > notation = Syntax::ParseText(reader.ReadAll(), spelling);
		int pos = 0;
		do {
			if (notation->ElementAt(pos).Class == Syntax::TokenClass::EndOfStream) break;
			if (notation->ElementAt(pos).Class != Syntax::TokenClass::Constant ||
				notation->ElementAt(pos).ValueClass != Syntax::TokenConstantClass::Numeric ||
				notation->ElementAt(pos).NumericClass() != Syntax::NumericTokenClass::Integer) throw Exception();
			int ID = int(notation->ElementAt(pos).AsInteger());
			pos++;
			DynamicString content;
			while (notation->ElementAt(pos).Class == Syntax::TokenClass::Constant &&
				notation->ElementAt(pos).ValueClass == Syntax::TokenConstantClass::String) {
				content += notation->ElementAt(pos).Content;
				pos++;
			}
			table->AddString(content, ID);
		} while (true);
	}
	catch (...) { return 0; }
	table->Retain();
	return table;
}
void string_table_to_text(StringTable * table, ITextWriter & output)
{
	SafePointer< Array<int> > index = table->GetIndex();
	int mid = 0;
	for (int i = 0; i < index->Length(); i++) if (index->ElementAt(i) > mid) mid = index->ElementAt(i);
	int padding_barrier = string(mid).Length() + 1;
	for (int i = 0; i < index->Length(); i++) {
		int ID = index->ElementAt(i);
		const string & Text = table->GetString(ID);
		int padding = padding_barrier - string(ID).Length();
		output << ID;
		for (int j = 0; j < padding; j++) output << L" ";
		output << L"\"" << Syntax::FormatStringToken(Text) << L"\"" << IO::LineFeedSequence;
	}
}

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
							console << TextColor(ConsoleColor::Red) << L"Unknown string table format name." << TextColorDefault() << LineFeed();
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
			console << L"  " << ENGINE_VI_APPSYSNAME << L" <table> :NSfov" << LineFeed();
			console << L"Where" << LineFeed();
			console << L"  table - source binary or text string table file," << LineFeed();
			console << L"  :N    - use no logo mode - don't print application logo," << LineFeed();
			console << L"  :S    - use silent mode - supress any non-explicit output," << LineFeed();
			console << L"  :f    - format used to encode the string table," << LineFeed();
			console << L"            one of the following values must be passed as the next argument:" << LineFeed();
			console << L"            bin   - binary string table," << LineFeed();
			console << L"            ascii - ASCII text notation," << LineFeed();
			console << L"            utf8  - UTF8 text notation," << LineFeed();
			console << L"            utf16 - UTF16-LE text notation," << LineFeed();
			console << L"            utf32 - UTF32-LE text notation," << LineFeed();
			console << L"  :o    - overrides the name of the output file, must be used with :f," << LineFeed();
			console << L"  :v    - prints the text notation into the standard output." << LineFeed();
			console << LineFeed();
		}
	} else {
		SafePointer<StringTable> table;
		try {
			state.input = IO::ExpandPath(state.input);
			FileStream file(state.input, AccessRead, OpenExisting);
			try { table = new StringTable(&file); }
			catch (...) { table.SetReference(0); }
			if (!table) table.SetReference(compile_string_table(&file));
			if (!table) {
				if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Invalid input file format." << TextColorDefault() << LineFeed();
				return 1;
			}
		} catch (...) {
			if (!state.silent) console << TextColor(ConsoleColor::Red) << L"Error accessing the input file." << TextColorDefault() << LineFeed();
			return 1;
		}
		try {
			if (state.print) string_table_to_text(table, console);
			if (state.format != Format::NOP) {
				if (!state.output.Length()) {
					auto suffix = state.format == Format::Binary ? L".ecst" : L".txt";
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
					if (!state.silent) console << L"Writing text string table...";
					FileStream file(state.output, AccessReadWrite, CreateAlways);
					TextWriter writer(&file, state.encoding);
					writer.WriteEncodingSignature();
					string_table_to_text(table, writer);
					if (!state.silent) console << TextColor(ConsoleColor::Green) << L"Succeed" << TextColorDefault() << LineFeed();
				} else if (state.format == Format::Binary) {
					if (!state.silent) console << L"Writing binary string table...";
					FileStream file(state.output, AccessReadWrite, CreateAlways);
					table->Save(&file);
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