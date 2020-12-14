﻿#include <EngineRuntime.h>

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Storage;
using namespace Engine::IO::ConsoleControl;

enum class Operation { View, ToText, ToBinary };

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
		output << L"\"" << Syntax::FormatStringToken(Text) << L"\"" << IO::NewLineChar;
	}
}

int Main(void)
{
	Console console;

	SafePointer< Array<string> > args = GetCommandLine();

	console << ENGINE_VI_APPNAME << LineFeed();
	console << L"Copyright " << string(ENGINE_VI_COPYRIGHT).Replace(L'\xA9', L"(C)") << LineFeed();
	console << L"Version " << ENGINE_VI_APPVERSION << L", build " << ENGINE_VI_BUILD << LineFeed() << LineFeed();
	
	if (args->Length() < 2) {
		console << L"Command line syntax:" << LineFeed();
		console << L"  " << ENGINE_VI_APPSYSNAME << L" <table> [:text <text file> [:enc <encoding>] | :binary <binary file>]" << LineFeed();
		console << L"Where" << LineFeed();
		console << L"  table       - source binary or text string table file," << LineFeed();
		console << L"  :text       - produce text string table notation," << LineFeed();
		console << L"  :binary     - produce binary string table file," << LineFeed();
		console << L"  text file   - output text string table file (.txt)," << LineFeed();
		console << L"  binary file - output binary string table file (.ecs, .ecst)," << LineFeed();
		console << L"  :enc        - specify text file encoding," << LineFeed();
		console << L"  encoding    - the required encoding, one of the following:" << LineFeed();
		console << L"    ascii     - basic 1-byte text encoding," << LineFeed();
		console << L"    utf8      - Unicode 1-byte surrogate notation, the default one," << LineFeed();
		console << L"    utf16     - Unicode 2-byte surrogate notation," << LineFeed();
		console << L"    utf32     - pure Unicode 4-byte notation." << LineFeed();
		console << L"Started only with the source string table name, writes it on the standard output." << LineFeed();
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
					console << TextColor(Console::ColorRed) << L"Unknown text encoding \"" + args->ElementAt(i + 1) + L"\"." << TextColorDefault() << LineFeed();
					return 1;
				}
				i++;
			} else {
				console << TextColor(Console::ColorRed) << L"Invalid command line syntax." << TextColorDefault() << LineFeed();
				return 1;
			}
		}
		SafePointer<StringTable> table;
		try {
			FileStream file(source, AccessRead, OpenExisting);
			try {
				table = new StringTable(&file);
			}
			catch (...) { table.SetReference(0); }
			if (!table) table.SetReference(compile_string_table(&file));
			if (!table) {
				console << TextColor(Console::ColorRed) << L"Invalid input file format." << TextColorDefault() << LineFeed();
				return 1;
			}
		}
		catch (...) {
			console << TextColor(Console::ColorRed) << L"Error accessing the input file." << TextColorDefault() << LineFeed();
			return 1;
		}
		try {
			if (operation == Operation::View) {
				string_table_to_text(table, console);
			} else if (operation == Operation::ToText) {
				console << L"Writing text string table...";
				FileStream file(output, AccessReadWrite, CreateAlways);
				TextWriter writer(&file, encoding);
				writer.WriteEncodingSignature();
				string_table_to_text(table, writer);
				console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
			} else if (operation == Operation::ToBinary) {
				console << L"Writing binary string table...";
				FileStream file(output, AccessReadWrite, CreateAlways);
				table->Save(&file);
				console << TextColor(Console::ColorGreen) << L"Succeed" << TextColorDefault() << LineFeed();
			}
		}
		catch (IO::FileAccessException & e) {
			console << TextColor(Console::ColorRed) << L"Error accessing the output file." << TextColorDefault() << LineFeed();
			return 1;
		}
		catch (...) {
			console << TextColor(Console::ColorRed) << L"Internal application error." << TextColorDefault() << LineFeed();
			return 1;
		}
	}
	return 0;
}