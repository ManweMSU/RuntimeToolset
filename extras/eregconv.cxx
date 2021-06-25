#include <EngineRuntime.h>
#include <Storage/TextRegistryGrammar.h>

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Storage;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;

enum class Operation { View, ToText, ToBinary };

int Main(void)
{
	Console console;

	SafePointer< Array<string> > args = GetCommandLine();

	console << ENGINE_VI_APPNAME << LineFeed();
	console << L"Copyright " << string(ENGINE_VI_COPYRIGHT).Replace(L'\xA9', L"(C)") << LineFeed();
	console << L"Version " << ENGINE_VI_APPVERSION << L", build " << ENGINE_VI_BUILD << LineFeed() << LineFeed();
	
	if (args->Length() < 2) {
		console << L"Command line syntax:" << LineFeed();
		console << L"  " << ENGINE_VI_APPSYSNAME << L" <registry> [:text <text file> [:enc <encoding>] | :binary <binary file>]" << LineFeed();
		console << L"Where" << LineFeed();
		console << L"  registry    - source binary or text registry file," << LineFeed();
		console << L"  :text       - produce text registry notation," << LineFeed();
		console << L"  :binary     - produce binary registry file," << LineFeed();
		console << L"  text file   - output text registry file (.txt, .ini, .eini)," << LineFeed();
		console << L"  binary file - output binary registry file (.ecs, .ecsr)," << LineFeed();
		console << L"  :enc        - specify text file encoding," << LineFeed();
		console << L"  encoding    - the required encoding, one of the following:" << LineFeed();
		console << L"    ascii     - basic 1-byte text encoding," << LineFeed();
		console << L"    utf8      - Unicode 1-byte surrogate notation, the default one," << LineFeed();
		console << L"    utf16     - Unicode 2-byte surrogate notation," << LineFeed();
		console << L"    utf32     - pure Unicode 4-byte notation." << LineFeed();
		console << L"Started only with the source registry name, writes it on the standard output." << LineFeed();
		console << LineFeed();
	} else {
		string source = args->ElementAt(1);
		string output;
		Encoding encoding = Encoding::UTF8;
		Operation operation = Operation::View;
		for (int i = 2; i < args->Length(); i++) {
			if (string::CompareIgnoreCase(args->ElementAt(i), L":text") == 0 && i < args->Length() - 1) {
				output = args->ElementAt(i + 1);
				operation = Operation::ToText;
				i++;
			} else if (string::CompareIgnoreCase(args->ElementAt(i), L":binary") == 0 && i < args->Length() - 1) {
				output = args->ElementAt(i + 1);
				operation = Operation::ToBinary;
				i++;
			} else if (string::CompareIgnoreCase(args->ElementAt(i), L":enc") == 0 && i < args->Length() - 1) {
				if (string::CompareIgnoreCase(args->ElementAt(i + 1), L"ascii") == 0) {
					encoding = Encoding::ANSI;
				} else if (string::CompareIgnoreCase(args->ElementAt(i + 1), L"utf8") == 0) {
					encoding = Encoding::UTF8;
				} else if (string::CompareIgnoreCase(args->ElementAt(i + 1), L"utf16") == 0) {
					encoding = Encoding::UTF16;
				} else if (string::CompareIgnoreCase(args->ElementAt(i + 1), L"utf32") == 0) {
					encoding = Encoding::UTF32;
				} else {
					console << TextColor(ConsoleColor::Red) << L"Unknown text encoding \"" + args->ElementAt(i + 1) + L"\"." << TextColorDefault() << LineFeed();
					return 1;
				}
				i++;
			} else {
				console << TextColor(ConsoleColor::Red) << L"Invalid command line syntax." << TextColorDefault() << LineFeed();
				return 1;
			}
		}
		SafePointer<Registry> reg;
		SafePointer< Array<Syntax::Token> > tokens;
		string inner;
		try {
			FileStream file(source, AccessRead, OpenExisting);
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
		}
		catch (Syntax::ParserSpellingException & e) {
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
			return 1;
		}
		catch (Syntax::ParserSyntaxException & e) {
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
			return 1;
		}
		catch (InvalidFormatException & e) {
			console << TextColor(ConsoleColor::Red) << L"Invalid input file format." << TextColorDefault() << LineFeed();
			return 1;
		}
		catch (IO::FileAccessException & e) {
			console << TextColor(ConsoleColor::Red) << L"Error accessing the input file." << TextColorDefault() << LineFeed();
			return 1;
		}
		catch (...) {
			console << TextColor(ConsoleColor::Red) << L"Internal application error." << TextColorDefault() << LineFeed();
			return 1;
		}
		try {
			if (operation == Operation::View) {
				RegistryToText(reg, &console);
			} else if (operation == Operation::ToText) {
				console << L"Writing text registry...";
				FileStream file(output, AccessReadWrite, CreateAlways);
				TextWriter writer(&file, encoding);
				writer.WriteEncodingSignature();
				RegistryToText(reg, &writer);
				console << TextColor(ConsoleColor::Green) << L"Succeed" << TextColorDefault() << LineFeed();
			} else if (operation == Operation::ToBinary) {
				console << L"Writing binary registry...";
				FileStream file(output, AccessReadWrite, CreateAlways);
				reg->Save(&file);
				console << TextColor(ConsoleColor::Green) << L"Succeed" << TextColorDefault() << LineFeed();
			}
		}
		catch (IO::FileAccessException & e) {
			console << TextColor(ConsoleColor::Red) << L"Error accessing the output file." << TextColorDefault() << LineFeed();
			return 1;
		}
		catch (...) {
			console << TextColor(ConsoleColor::Red) << L"Internal application error." << TextColorDefault() << LineFeed();
			return 1;
		}
	}
	return 0;
}