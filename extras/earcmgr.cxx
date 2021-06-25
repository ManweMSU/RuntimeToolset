﻿#include <EngineRuntime.h>

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::Storage;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;

enum class TimeClass { Unset = 0, Explicit = 1, Automatic = 2, InheritSource = 3, ArchiveTime = 4 };
enum class ListFormat { Text, Binary, Legacy };
enum class ArchiveFormat { Short = 0, Long = 1 };
class ArchiveEntityTime
{
public:
	Time Value = 0;
	TimeClass Class = TimeClass::Unset;
};
class ArchiveAttribute
{
public:
	string Key;
	string Value;
};
class ArchiveEntity
{
public:
	bool AddMetadata = true;

	string SourceFile;
	Array<uint8> SourceData;
	string Name;
	string Type;
	uint32 ID = 0;
	uint32 Custom = 0;

	ArchiveEntityTime CreationTime;
	ArchiveEntityTime AccessTime;
	ArchiveEntityTime AlterTime;

	Array<ArchiveAttribute> Attributes;

	MethodChain CompressionChain = 0;
	uint32 CompressionBlock = 0x100000;
	CompressionQuality Quality = CompressionQuality::Sequential;

	ArchiveEntity(void) : SourceData(0x100), Attributes(4) {}
};
class ArchiveList
{
public:
	string ListName;
	bool ExcludeMetadata;
	ListFormat Format;
	ArchiveFormat ArcFormat;
	Array<ArchiveEntity> Files;

	ArchiveList(void) : ExcludeMetadata(true), Format(ListFormat::Text), ArcFormat(ArchiveFormat::Long), Files(0x10) {}

	string ResolveFilePath(const string & path)
	{
		return IO::ExpandPath(path.Replace(L"<ListPath>", IO::Path::GetDirectory(ListName)));
	}
	static bool Load(ArchiveList & list, const string & file_path)
	{
		try {
			list.Files.Clear();
			list.ListName = IO::ExpandPath(file_path);
			FileStream File(list.ListName, AccessRead, OpenExisting);
			list.Format = ListFormat::Binary;
			SafePointer<Registry> ListReg = LoadRegistry(&File);
			if (!ListReg) {
				File.Seek(0, Begin);
				list.Format = ListFormat::Text;
				TextReader Reader(&File);
				string Title = Reader.ReadLine();
				if (Title == L"ECSAL") ListReg = CompileTextRegistry(&Reader);
			}
			if (ListReg) {
				list.ExcludeMetadata = ListReg->GetValueBoolean(L"ExcludeMetadata");
				list.ArcFormat = static_cast<ArchiveFormat>(ListReg->GetValueInteger(L"ArchiveFormat"));
				auto & files = ListReg->GetSubnodes();
				for (int i = 0; i < files.Length(); i++) {
					SafePointer<RegistryNode> node = ListReg->OpenNode(files[i]);
					ArchiveEntity entity;
					entity.AddMetadata = node->GetValueBoolean(L"AddMetadata");
					if (node->GetValueType(L"Source") == RegistryValueType::Binary) {
						entity.SourceData.SetLength(node->GetValueBinarySize(L"Source"));
						node->GetValueBinary(L"Source", entity.SourceData.GetBuffer());
					} else entity.SourceFile = node->GetValueString(L"Source");
					entity.Name = node->GetValueString(L"Name");
					entity.Type = node->GetValueString(L"Type");
					entity.ID = node->GetValueInteger(L"ID");
					entity.Custom = node->GetValueInteger(L"Custom");
					entity.CreationTime.Value = node->GetValueTime(L"CreationTimeValue");
					entity.CreationTime.Class = static_cast<TimeClass>(node->GetValueInteger(L"CreationTimeClass"));
					entity.AccessTime.Value = node->GetValueTime(L"AccessTimeValue");
					entity.AccessTime.Class = static_cast<TimeClass>(node->GetValueInteger(L"AccessTimeClass"));
					entity.AlterTime.Value = node->GetValueTime(L"AlterTimeValue");
					entity.AlterTime.Class = static_cast<TimeClass>(node->GetValueInteger(L"AlterTimeClass"));
					auto & attrs = node->GetSubnodes();
					for (int i = 0; i < attrs.Length(); i++) {
						SafePointer<RegistryNode> attr_node = node->OpenNode(attrs[i]);
						ArchiveAttribute attr;
						attr.Key = attr_node->GetValueString(L"Key");
						attr.Value = attr_node->GetValueString(L"Value");
						entity.Attributes << attr;
					}
					entity.CompressionChain = node->GetValueInteger(L"CompressionChain");
					if (node->GetValueType(L"CompressionBlock") == RegistryValueType::Integer)
						entity.CompressionBlock = node->GetValueInteger(L"CompressionBlock");
					if (node->GetValueType(L"CompressionQuality") == RegistryValueType::Integer)
						entity.Quality = static_cast<CompressionQuality>(node->GetValueInteger(L"CompressionQuality"));
					list.Files << entity;
				}
			} else {
				File.Seek(0, Begin);
				TextReader Reader(&File);
				string Signature = Reader.ReadLine();
				if (Signature != L"ECSAP") throw Exception();
				list.Format = ListFormat::Legacy;
				list.ArcFormat = ArchiveFormat::Short;
				list.ExcludeMetadata = true;
				int Count = Reader.ReadLine().ToUInt32();
				for (int i = 0; i < Count; i++) {
					ArchiveEntity entity;
					entity.SourceFile = Reader.ReadLine();
					if ((entity.SourceFile.Length() < 2 || entity.SourceFile[1] != L':') && (entity.SourceFile[0] != L'/'))
						entity.SourceFile = L"<ListPath>/" + entity.SourceFile;
					entity.Name = Reader.ReadLine();
					entity.Type = Reader.ReadLine();
					entity.ID = Reader.ReadLine().ToUInt32();
					entity.Custom = Reader.ReadLine().ToUInt32();
					list.Files << entity;
				}
			}
		} catch (...) { return false; }
		return true;
	}
	bool Save(void)
	{
		try {
			FileStream File(ListName, AccessReadWrite, CreateAlways);
			if (Format == ListFormat::Legacy) {
				TextWriter Writer(&File, Encoding::UTF16);
				Writer.WriteEncodingSignature();
				Writer << L"ECSAP\r\n";
				Writer << Files.Length() << L"\r\n";
				for (int i = 0; i < Files.Length(); i++) {
					if (Files[i].SourceFile.FindFirst(L"<ListPath>/") == 0 || Files[i].SourceFile.FindFirst(L"<ListPath>\\") == 0) {
						Writer << Files[i].SourceFile.Fragment(11, -1) << L"\r\n";
					} else {
						Writer << ResolveFilePath(Files[i].SourceFile) << L"\r\n";
					}
					Writer << Files[i].Name << L"\r\n";
					Writer << Files[i].Type << L"\r\n";
					Writer << Files[i].ID << L"\r\n";
					Writer << Files[i].Custom << L"\r\n";
				}
			} else {
				SafePointer<Registry> db = CreateRegistry();
				if (ExcludeMetadata) {
					db->CreateValue(L"ExcludeMetadata", RegistryValueType::Boolean);
					db->SetValue(L"ExcludeMetadata", ExcludeMetadata);
				}
				if (ArcFormat != ArchiveFormat::Short) {
					db->CreateValue(L"ArchiveFormat", RegistryValueType::Integer);
					db->SetValue(L"ArchiveFormat", static_cast<int>(ArcFormat));
				}
				for (int i = 0; i < Files.Length(); i++) {
					string n1 = string(uint32(i + 1), L"0123456789", 9);
					db->CreateNode(n1);
					SafePointer<RegistryNode> node = db->OpenNode(n1);
					if (Files[i].AddMetadata) {
						node->CreateValue(L"AddMetadata", RegistryValueType::Boolean);
						node->SetValue(L"AddMetadata", Files[i].AddMetadata);
					}
					if (Files[i].SourceFile.Length()) {
						node->CreateValue(L"Source", RegistryValueType::String);
						node->SetValue(L"Source", Files[i].SourceFile);
					} else {
						node->CreateValue(L"Source", RegistryValueType::Binary);
						node->SetValue(L"Source", Files[i].SourceData.GetBuffer(), Files[i].SourceData.Length());
					}
					if (Files[i].Name.Length()) {
						node->CreateValue(L"Name", RegistryValueType::String);
						node->SetValue(L"Name", Files[i].Name);
					}
					if (Files[i].Type.Length()) {
						node->CreateValue(L"Type", RegistryValueType::String);
						node->SetValue(L"Type", Files[i].Type);
					}
					if (Files[i].ID) {
						node->CreateValue(L"ID", RegistryValueType::Integer);
						node->SetValue(L"ID", int(Files[i].ID));
					}
					if (Files[i].Custom) {
						node->CreateValue(L"Custom", RegistryValueType::Integer);
						node->SetValue(L"Custom", int(Files[i].Custom));
					}
					if (Files[i].CreationTime.Class != TimeClass::Unset) {
						if (Files[i].CreationTime.Value) {
							node->CreateValue(L"CreationTimeValue", RegistryValueType::Time);
							node->SetValue(L"CreationTimeValue", Files[i].CreationTime.Value);
						}
						node->CreateValue(L"CreationTimeClass", RegistryValueType::Integer);
						node->SetValue(L"CreationTimeClass", static_cast<int>(Files[i].CreationTime.Class));
					}
					if (Files[i].AccessTime.Class != TimeClass::Unset) {
						if (Files[i].AccessTime.Value) {
							node->CreateValue(L"AccessTimeValue", RegistryValueType::Time);
							node->SetValue(L"AccessTimeValue", Files[i].AccessTime.Value);
						}
						node->CreateValue(L"AccessTimeClass", RegistryValueType::Integer);
						node->SetValue(L"AccessTimeClass", static_cast<int>(Files[i].AccessTime.Class));
					}
					if (Files[i].AlterTime.Class != TimeClass::Unset) {
						if (Files[i].AlterTime.Value) {
							node->CreateValue(L"AlterTimeValue", RegistryValueType::Time);
							node->SetValue(L"AlterTimeValue", Files[i].AlterTime.Value);
						}
						node->CreateValue(L"AlterTimeClass", RegistryValueType::Integer);
						node->SetValue(L"AlterTimeClass", static_cast<int>(Files[i].AlterTime.Class));
					}
					for (int j = 0; j < Files[i].Attributes.Length(); j++) {
						string n2 = string(uint32(j + 1), L"0123456789", 9);
						node->CreateNode(n2);
						SafePointer<RegistryNode> attr = node->OpenNode(n2);
						attr->CreateValue(L"Key", RegistryValueType::String);
						attr->SetValue(L"Key", Files[i].Attributes[j].Key);
						attr->CreateValue(L"Value", RegistryValueType::String);
						attr->SetValue(L"Value", Files[i].Attributes[j].Value);
					}
					if (Files[i].CompressionChain.Code) {
						node->CreateValue(L"CompressionChain", RegistryValueType::Integer);
						node->SetValue(L"CompressionChain", int(Files[i].CompressionChain.Code));
						node->CreateValue(L"CompressionBlock", RegistryValueType::Integer);
						node->SetValue(L"CompressionBlock", int(Files[i].CompressionBlock));
						node->CreateValue(L"CompressionQuality", RegistryValueType::Integer);
						node->SetValue(L"CompressionQuality", static_cast<int>(Files[i].Quality));
					}
				}
				if (Format == ListFormat::Text) {
					TextWriter Writer(&File, Encoding::UTF8);
					Writer.WriteEncodingSignature();
					Writer.WriteLine(L"ECSAL");
					RegistryToText(db, &Writer);
				} else {
					db->Save(&File);
				}
			}
		}
		catch (...) { return false; }
		return true;
	}
	bool Archive(Console & console)
	{
		Time started = Time::GetCurrentTime();
		console << L"Archivation started at " << TextColor(ConsoleColor::Cyan) << started.ToLocal().ToString() << TextColorDefault() << L"." << LineFeed();
		console << L"Mode: " << TextColor(ConsoleColor::Magenta);
		if (ArcFormat == ArchiveFormat::Short) console << L"format: 32-bit, ";
		else if (ArcFormat == ArchiveFormat::Long) console << L"format: 64-bit, ";
		if (ExcludeMetadata) console << L"no metadata";
		else console << L"with metadata";
		console << TextColorDefault() << L"." << LineFeed();
		console << L"Initializing subsystem...";
		SafePointer<ThreadPool> pool;
		SafePointer<FileStream> arc_stream;
		SafePointer<NewArchive> arc;
		string file_name;
		try {
			pool = new ThreadPool;
		}
		catch (...) { console << TextColor(ConsoleColor::Red) << L"Failed to set up the thread pool." << TextColorDefault() << LineFeed(); return false; }
		try {
			file_name = IO::ExpandPath(IO::Path::GetDirectory(ListName) + L"/" + IO::Path::GetFileNameWithoutExtension(ListName) + L".ecsa");
			arc_stream = new FileStream(file_name, AccessReadWrite, CreateAlways);
		}
		catch (...) { console << TextColor(ConsoleColor::Red) << L"Failed to create a file." << TextColorDefault() << LineFeed(); return false; }
		try {
			uint flags = 0;
			if (!ExcludeMetadata) flags |= NewArchiveFlags::CreateMetadata;
			if (ArcFormat == ArchiveFormat::Short) flags |= NewArchiveFlags::UseFormat32;
			arc = CreateArchive(arc_stream, Files.Length(), flags);
		}
		catch (...) { console << TextColor(ConsoleColor::Red) << L"Failed to create a new archive." << TextColorDefault() << LineFeed(); return false; }
		console << TextColor(ConsoleColor::Green) << L"Succeed" << TextColorDefault() << LineFeed();
		console << L"Writing an archive at \"" << TextColor(ConsoleColor::Cyan) << file_name << TextColorDefault() << L"\"." << LineFeed();
		console << TextColor(ConsoleColor::Yellow) << L"0%" << TextColorDefault() << LineFeed();
		for (int i = 0; i < Files.Length(); i++) {
			console << L"Packing file with name \"" << TextColor(ConsoleColor::Cyan) << Files[i].Name << TextColorDefault() <<
				L"\" and ID " << TextColor(ConsoleColor::Cyan) << Files[i].Type << TextColorDefault() <<
				L" " << TextColor(ConsoleColor::Cyan) << Files[i].ID << TextColorDefault() << L"...";
			try {
				SafePointer<Stream> source;
				FileStream * file_stream;
				if (Files[i].SourceFile.Length()) {
					source = new FileStream(ResolveFilePath(Files[i].SourceFile), AccessRead, OpenExisting);
					file_stream = static_cast<FileStream *>(source.Inner());
				} else {
					source = new MemoryStream(Files[i].SourceData.GetBuffer(), Files[i].SourceData.Length());
					file_stream = 0;
				}
				arc->SetFileName(i + 1, Files[i].Name);
				arc->SetFileType(i + 1, Files[i].Type);
				arc->SetFileID(i + 1, Files[i].ID);
				arc->SetFileCustom(i + 1, Files[i].Custom);
				if (Files[i].AddMetadata && !ExcludeMetadata) {
					Time created, accessed, altered;
					if (Files[i].CreationTime.Class != TimeClass::Unset) {
						if (Files[i].CreationTime.Class == TimeClass::ArchiveTime) {
							created = started;
						} else if (Files[i].CreationTime.Class == TimeClass::InheritSource) {
							if (file_stream) created = IO::DateTime::GetFileCreationTime(file_stream->Handle());
							else created = started;
						} else {
							created = Files[i].CreationTime.Value;
						}
						arc->SetFileCreationTime(i + 1, created);
					}
					if (Files[i].AccessTime.Class != TimeClass::Unset) {
						if (Files[i].AccessTime.Class == TimeClass::ArchiveTime) {
							accessed = started;
						} else if (Files[i].AccessTime.Class == TimeClass::InheritSource) {
							if (file_stream) accessed = IO::DateTime::GetFileAccessTime(file_stream->Handle());
							else accessed = started;
						} else {
							accessed = Files[i].AccessTime.Value;
						}
						arc->SetFileAccessTime(i + 1, accessed);
					}
					if (Files[i].AlterTime.Class != TimeClass::Unset) {
						if (Files[i].AlterTime.Class == TimeClass::ArchiveTime) {
							altered = started;
						} else if (Files[i].AlterTime.Class == TimeClass::InheritSource) {
							if (file_stream) altered = IO::DateTime::GetFileAlterTime(file_stream->Handle());
							else altered = started;
						} else {
							altered = Files[i].AlterTime.Value;
						}
						arc->SetFileAlterTime(i + 1, altered);
					}
					for (int j = 0; j < Files[i].Attributes.Length(); j++) {
						arc->SetFileAttribute(i + 1, Files[i].Attributes[j].Key, Files[i].Attributes[j].Value);
					}
				}
				if (Files[i].CompressionChain.Length()) {
					arc->SetFileData(i + 1, source, Files[i].CompressionChain, Files[i].Quality, pool, Files[i].CompressionBlock);
				} else {
					arc->SetFileData(i + 1, source);
				}
			}
			catch (...) { console << TextColor(ConsoleColor::Red) << L"Failed." << TextColorDefault() << LineFeed(); return false; }
			console << TextColor(ConsoleColor::Green) << L"Succeed" << TextColorDefault() << LineFeed();
			console << TextColor(ConsoleColor::Yellow) << ((i + 1) * 100 / Files.Length()) << L"%" << TextColorDefault() << LineFeed();
		}
		arc->Finalize();
		console << TextColor(ConsoleColor::Green) << L"Finished with " << (Time::GetCurrentTime() - started).ToShortString() << L"." << TextColorDefault() << LineFeed();
		return true;
	}
};
string string_lexem(const string & str) { return L"\"" + Syntax::FormatStringToken(str) + L"\""; }
string time_lexem(ArchiveEntityTime & time)
{
	if (time.Class == TimeClass::Unset) return L"time unset";
	else if (time.Class == TimeClass::Explicit) return time.Value.ToLocal().ToString();
	else if (time.Class == TimeClass::Automatic) return L"auto " + time.Value.ToLocal().ToString();
	else if (time.Class == TimeClass::InheritSource) return L"source time";
	else return L"archivation time";
}
string comm_lexem(CompressionMethod method)
{
	if (method == CompressionMethod::Huffman) return L"Huffman";
	else if (method == CompressionMethod::LempelZivWelch) return L"Lempel-Ziv-Welch";
	else if (method == CompressionMethod::RunLengthEncoding8bit) return L"RLE8";
	else if (method == CompressionMethod::RunLengthEncoding16bit) return L"RLE16";
	else if (method == CompressionMethod::RunLengthEncoding32bit) return L"RLE32";
	else if (method == CompressionMethod::RunLengthEncoding64bit) return L"RLE64";
	else if (method == CompressionMethod::RunLengthEncoding128bit) return L"RLE128";
	else return L"unknown";
}
string qual_lexem(CompressionQuality qual)
{
	if (qual == CompressionQuality::Force) return L"force";
	else if (qual == CompressionQuality::Optional) return L"optional";
	else if (qual == CompressionQuality::Sequential) return L"sequential";
	else if (qual == CompressionQuality::Variative) return L"variative";
	else if (qual == CompressionQuality::ExtraVariative) return L"extra variative";
	else return L"unknown";
}
string com_lexem(ArchiveEntity & ent)
{
	if (!ent.CompressionChain.Length()) return L"no compression";
	DynamicString result;
	for (int i = 0; i < ent.CompressionChain.Length(); i++) {
		if (i) result += L" -> ";
		result += comm_lexem(ent.CompressionChain[i]);
	}
	result += L" block 0x" + string(ent.CompressionBlock, L"0123456789ABCDEF") + L" quality " + qual_lexem(ent.Quality);
	return result;
}

ArchiveList list;
Array<int> selection(0x10);
Syntax::Spelling command_spelling;
bool exiting = false;

bool Execute(Array<Syntax::Token> & command, IO::Console & console)
{
	if (command.Length() > 0) {
		if (command[0].Class == Syntax::TokenClass::CharCombo && command[0].Content == L"?") {
			if (command.Length() > 1) {
				if (string::CompareIgnoreCase(command[1].Content, L"?") == 0) {
					console << L"Syntax: ? [<command>]" << LineFeed();
					console << L"Writes information about all commands or about \"command\", if present." << LineFeed();
				} else if (string::CompareIgnoreCase(command[1].Content, L"rename") == 0) {
					console << L"Syntax: rename <new name>" << LineFeed();
					console << L"Sets name (and path) to \"new path\" for current archive list." << LineFeed();
				} else if (string::CompareIgnoreCase(command[1].Content, L"list") == 0) {
					console << L"Syntax: list [sel] [<format>]" << LineFeed();
					console << L"For each file in archive list (or current selection, if \"sel\" choosen)" << LineFeed();
					console << L"writes it's data in required format: in \"format\" following combinations" << LineFeed();
					console << L"will be substituted by file's fields:" << LineFeed();
					console << L"  <SN> - source file name, if not embeded," << LineFeed();
					console << L"  <AM> - are metadata saved for this file or not," << LineFeed();
					console << L"  <I>  - internal index of file," << LineFeed();
					console << L"  <N>  - internal file name," << LineFeed();
					console << L"  <T>  - file type," << LineFeed();
					console << L"  <ID> - file identifier," << LineFeed();
					console << L"  <UD> - file user defined value," << LineFeed();
					console << L"  <CT> - file creation time," << LineFeed();
					console << L"  <AT> - file access time," << LineFeed();
					console << L"  <MT> - file alter time," << LineFeed();
					console << L"  <A>  - list of file's attributes," << LineFeed();
					console << L"  <C>  - declared compression method," << LineFeed();
					console << L"without \"format\" specified uses \"<N>, <ID> <T> from <SN>\" format." << LineFeed();
				} else if (string::CompareIgnoreCase(command[1].Content, L"select") == 0) {
					console << L"Syntax: select [num <pattern>] [name <pattern>] [type <pattern>] [id <pattern>]" << LineFeed();
					console << L"Selects files, for which some properties correspond specified patterns." << LineFeed();
				} else if (string::CompareIgnoreCase(command[1].Content, L"append") == 0) {
					console << L"Syntax: append <path pattern> [rec]" << LineFeed();
					console << L"Adds files, correspond to path pattern, with recursive search if required, to list." << LineFeed();
					console << L"Sets selection to file added." << LineFeed();
				} else if (string::CompareIgnoreCase(command[1].Content, L"name") == 0) {
					console << L"Syntax: name <operation>" << LineFeed();
					console << L"Applies the specified operation on the names of the files selected." << LineFeed();
					console << L"Possible operations:" << LineFeed();
					console << L"  prepend <prefix>      - adds a fixed prefix to the names," << LineFeed();
					console << L"  append <suffix>       - adds a fixed suffix to the names," << LineFeed();
					console << L"  replace <text> <with> - replaces all <text> substrings with the <with> string." << LineFeed();
				} else if (string::CompareIgnoreCase(command[1].Content, L"set") == 0) {
					console << L"Syntax: set <property> values..." << LineFeed();
					console << L"Sets a list's global or selected files'es local property." << LineFeed();
					console << L"Properties and values:" << LineFeed();
					console << L"  metadata {include|exclude}         - embed or discard metadata," << LineFeed();
					console << L"  format   {e32|e64}                 - archive format: 32-bit compatible or 64-bit," << LineFeed();
					console << L"  notation {text|binary|legacy}      - list file format," << LineFeed();
					console << L"  extended {yes|no}                  - embed metadata for particular file, ignored if metadata = exclude," << LineFeed();
					console << L"  source   <file> [embed]            - source of file data, set link to file or import data to list," << LineFeed();
					console << L"  name     <name>                    - file name," << LineFeed();
					console << L"  type     <type>                    - file type," << LineFeed();
					console << L"  id       <id>                      - file ID," << LineFeed();
					console << L"  user     <user>                    - file user-specific data," << LineFeed();
					console << L"  time     {c|a|m} <class> [<time>]  - file time value:" << LineFeed();
					console << L"    c - creation time, a - access time, m - alter time," << LineFeed();
					console << L"    class is:" << LineFeed();
					console << L"      unset    - no time is set," << LineFeed();
					console << L"      explicit - time is fixed," << LineFeed();
					console << L"      auto     - time is set and can be changed by list editor," << LineFeed();
					console << L"      inherit  - inherit source's time," << LineFeed();
					console << L"      arc      - set archivation time," << LineFeed();
					console << L"    time is in form <year> <month> <day> <hour> <minute> <second> <millisecond>," << LineFeed();
					console << L"  chain    <method 1> ... <method n> - compression chain, methods are:" << LineFeed();
					console << L"    huffman, lzw, rle8, rle16, rle32, rle64, rle128," << LineFeed();
					console << L"  block    <size>                    - compression block size," << LineFeed();
					console << L"  quality  <quality>                 - compression quality:" << LineFeed();
					console << L"    f (force), o (optional), s (sequential), v (variative), e (extra variative)." << LineFeed();
				} else if (string::CompareIgnoreCase(command[1].Content, L"attr") == 0) {
					console << L"Syntax: attr {set|rem} <attribute> [<value>]" << LineFeed();
					console << L"Sets or removes an attribute on selected files." << LineFeed();
				} else {
					console << L"No additional information on command " << command[1].Content << L"." << LineFeed();
				}
			} else {
				console << L"List of available commands:" << LineFeed();
				console << L"  ?       - show this information," << LineFeed();
				console << L"  append  - adds some files to list," << LineFeed();
				console << L"  arc     - produce an archive," << LineFeed();
				console << L"  attr    - set or remove attribute," << LineFeed();
				console << L"  create  - adds new file record without any properties being set," << LineFeed();
				console << L"  exclude - removes selected files from list," << LineFeed();
				console << L"  exit    - exit, discarding any changes," << LineFeed();
				console << L"  finish  - save the list and then exit," << LineFeed();
				console << L"  general - print general archive list information," << LineFeed();
				console << L"  invert  - inverts selection," << LineFeed();
				console << L"  list    - shows content of archive list," << LineFeed();
				console << L"  name    - some special commands on the names of the files," << LineFeed();
				console << L"  rename  - sets new name for list file," << LineFeed();
				console << L"  save    - save the list, rewriting the old one," << LineFeed();
				console << L"  select  - selects files," << LineFeed();
				console << L"  set     - changes some property," << LineFeed();
				console << L"Type \"? <command>\" to get additional information on command (if present)." << LineFeed();
			}
		} else if (command[0].Class == Syntax::TokenClass::Identifier) {
			if (string::CompareIgnoreCase(command[0].Content, L"exit") == 0) {
				exiting = true;
				return false;
			} else if (string::CompareIgnoreCase(command[0].Content, L"save") == 0) {
				console << L"Saving the archive list...";
				if (list.Save()) console << TextColor(ConsoleColor::Green) << L"Succeed." << TextColorDefault() << LineFeed();
				else { console << TextColor(ConsoleColor::Red) << L"Failed." << TextColorDefault() << LineFeed(); return false; }
			} else if (string::CompareIgnoreCase(command[0].Content, L"finish") == 0) {
				console << L"Saving the archive list...";
				if (list.Save()) {
					console << TextColor(ConsoleColor::Green) << L"Succeed." << TextColorDefault() << LineFeed();
					exiting = true;
				} else console << TextColor(ConsoleColor::Red) << L"Failed." << TextColorDefault() << LineFeed();
				return false;
			} else if (string::CompareIgnoreCase(command[0].Content, L"rename") == 0 && command.Length() >= 2) {
				list.ListName = IO::ExpandPath(command[1].Content);
				console << L"Repositioned to \"" << TextColor(ConsoleColor::Yellow) << list.ListName << TextColorDefault() << L"\"." << LineFeed();
				console << L"Next \"save\" calls will use this name." << LineFeed();
			} else if (string::CompareIgnoreCase(command[0].Content, L"list") == 0) {
				bool sel_only = false;
				string pattern = L"<N>, <ID> <T> from <SN>";
				for (int i = 1; i < command.Length(); i++) {
					if (command[i].Class == Syntax::TokenClass::Identifier && string::CompareIgnoreCase(command[i].Content, L"sel") == 0) sel_only = true;
					else pattern = command[i].Content;
				}
				Array<int> indicies(0x100);
				if (sel_only) {
					indicies = selection;
				} else {
					for (int i = 0; i < list.Files.Length(); i++) indicies << i;
				}
				if (indicies.Length()) {
					for (int i = 0; i < indicies.Length(); i++) {
						auto & file = list.Files[indicies[i]];
						string line = pattern;
						DynamicString attr;
						for (int j = 0; j < file.Attributes.Length(); j++) {
							if (j) attr += L", ";
							attr += string_lexem(file.Attributes[j].Key) + L" = " + string_lexem(file.Attributes[j].Value);
						}
						line = line.Replace(L"<SN>", file.SourceFile.Length() ? string_lexem(file.SourceFile) : L"embeded data");
						line = line.Replace(L"<AM>", file.AddMetadata ? L"yes" : L"no");
						line = line.Replace(L"<I>", string(indicies[i] + 1));
						line = line.Replace(L"<N>", string_lexem(file.Name));
						line = line.Replace(L"<T>", string_lexem(file.Type));
						line = line.Replace(L"<ID>", string(file.ID));
						line = line.Replace(L"<UD>", string(file.Custom));
						line = line.Replace(L"<CT>", time_lexem(file.CreationTime));
						line = line.Replace(L"<AT>", time_lexem(file.AccessTime));
						line = line.Replace(L"<MT>", time_lexem(file.AlterTime));
						line = line.Replace(L"<A>", attr);
						line = line.Replace(L"<C>", com_lexem(file));
						console << line << LineFeed();
					}
				} else console << L"(no elements)" << LineFeed();
			} else if (string::CompareIgnoreCase(command[0].Content, L"select") == 0) {
				Array<int> indicies(0x100);
				for (int i = 0; i < list.Files.Length(); i++) indicies << i;
				for (int k = 1; k < command.Length(); k += 2) {
					if (k + 1 < command.Length()) {
						auto & pattern = command[k + 1].Content;
						if (command[k].Class == Syntax::TokenClass::Identifier && string::CompareIgnoreCase(command[k].Content, L"num") == 0) {
							for (int i = indicies.Length() - 1; i >= 0; i--) {
								if (!Syntax::MatchFilePattern(string(indicies[i] + 1), pattern)) indicies.Remove(i);
							}
						} else if (command[k].Class == Syntax::TokenClass::Identifier && string::CompareIgnoreCase(command[k].Content, L"name") == 0) {
							for (int i = indicies.Length() - 1; i >= 0; i--) {
								if (!Syntax::MatchFilePattern(list.Files[indicies[i]].Name, pattern)) indicies.Remove(i);
							}
						} else if (command[k].Class == Syntax::TokenClass::Identifier && string::CompareIgnoreCase(command[k].Content, L"type") == 0) {
							for (int i = indicies.Length() - 1; i >= 0; i--) {
								if (!Syntax::MatchFilePattern(list.Files[indicies[i]].Type, pattern)) indicies.Remove(i);
							}
						} else if (command[k].Class == Syntax::TokenClass::Identifier && string::CompareIgnoreCase(command[k].Content, L"id") == 0) {
							for (int i = indicies.Length() - 1; i >= 0; i--) {
								if (!Syntax::MatchFilePattern(string(list.Files[indicies[i]].ID), pattern)) indicies.Remove(i);
							}
						}
					}
				}
				selection = indicies;
				console << L"Selected " << TextColor(ConsoleColor::Cyan) << indicies.Length() << TextColorDefault() << L" files." << LineFeed();
			} else if (string::CompareIgnoreCase(command[0].Content, L"invert") == 0) {
				Array<int> indicies(0x100);
				for (int i = 0; i < list.Files.Length(); i++) indicies << i;
				for (int i = selection.Length() - 1; i >= 0; i--) indicies.Remove(selection[i]);
				selection = indicies;
				console << L"Selected " << TextColor(ConsoleColor::Cyan) << indicies.Length() << TextColorDefault() << L" files." << LineFeed();
			} else if (string::CompareIgnoreCase(command[0].Content, L"exclude") == 0) {
				int count = 0;
				for (int i = selection.Length() - 1; i >= 0; i--) {
					list.Files.Remove(selection[i]);
					count++;
				}
				selection.Clear();
				console << L"Excluded " << TextColor(ConsoleColor::Cyan) << count << TextColorDefault() << L" files." << LineFeed();
			} else if (string::CompareIgnoreCase(command[0].Content, L"append") == 0 && command.Length() > 1) {
				selection.Clear();
				bool rec = false;
				string prefix = IO::Path::GetDirectory(list.ListName);
				if (command.Length() > 2 && command[2].Class == Syntax::TokenClass::Identifier && string::CompareIgnoreCase(command[2].Content, L"rec") == 0) rec = true;
				SafePointer< Array<string> > files = IO::Search::GetFiles(command[1].Content, rec);
				int count = 0;
				for (int i = 0; i < files->Length(); i++) {
					string name = files->ElementAt(i);
					if (IO::Path::GetFileName(name)[0] == L'.') continue;
					string base = IO::Path::GetDirectory(command[1].Content);
					string src = IO::ExpandPath((base.Length() ? (base + L"/") : L"") + files->ElementAt(i));
					if (string::CompareIgnoreCase(src.Fragment(0, prefix.Length()), prefix) == 0) src = L"<ListPath>" + src.Fragment(prefix.Length(), -1);
					ArchiveEntity ent;
					ent.SourceFile = src;
					ent.Name = name;
					ent.CreationTime.Class = TimeClass::InheritSource;
					ent.AccessTime.Class = TimeClass::InheritSource;
					ent.AlterTime.Class = TimeClass::InheritSource;
					count++;
					selection << list.Files.Length();
					list.Files << ent;
					console << L"Added \"" << TextColor(ConsoleColor::Cyan) << src << TextColorDefault() << L"\"." << LineFeed();
				}
				console << L"Added " << TextColor(ConsoleColor::Cyan) << count << TextColorDefault() << L" files." << LineFeed();
			} else if (string::CompareIgnoreCase(command[0].Content, L"create") == 0) {
				selection.Clear();
				ArchiveEntity ent;
				selection << list.Files.Length();
				list.Files << ent;
				console << L"New entry created." << LineFeed();
			} else if (string::CompareIgnoreCase(command[0].Content, L"general") == 0) {
				string af, lf;
				if (list.ArcFormat == ArchiveFormat::Short) af = L"32-bit";
				else if (list.ArcFormat == ArchiveFormat::Long) af = L"64-bit";
				if (list.Format == ListFormat::Text) lf = L"text extended";
				else if (list.Format == ListFormat::Binary) lf = L"binary extended";
				else if (list.Format == ListFormat::Legacy) lf = L"text compatible";
				console << L"List at \"" << TextColor(ConsoleColor::Cyan) << list.ListName << TextColorDefault() << L"\"," << LineFeed();
				console << TextColor(ConsoleColor::Cyan) << list.Files.Length() << TextColorDefault() << L" files," << LineFeed();
				console << L"metadata usage: " << TextColor(ConsoleColor::Cyan) << (list.ExcludeMetadata ? L"exclude," : L"include,") << TextColorDefault() << LineFeed();
				console << L"archive format: " << TextColor(ConsoleColor::Cyan) << af << TextColorDefault() << L"," << LineFeed();
				console << L"list format   : " << TextColor(ConsoleColor::Cyan) << lf << TextColorDefault() << L"." << LineFeed();
			} else if (string::CompareIgnoreCase(command[0].Content, L"set") == 0 && command.Length() > 2) {
				if (string::CompareIgnoreCase(command[1].Content, L"metadata") == 0) {
					if (string::CompareIgnoreCase(command[2].Content, L"include") == 0) list.ExcludeMetadata = false;
					else if (string::CompareIgnoreCase(command[2].Content, L"exclude") == 0) list.ExcludeMetadata = true;
				} else if (string::CompareIgnoreCase(command[1].Content, L"format") == 0) {
					if (string::CompareIgnoreCase(command[2].Content, L"e32") == 0) list.ArcFormat = ArchiveFormat::Short;
					else if (string::CompareIgnoreCase(command[2].Content, L"e64") == 0) list.ArcFormat = ArchiveFormat::Long;
				} else if (string::CompareIgnoreCase(command[1].Content, L"notation") == 0) {
					if (string::CompareIgnoreCase(command[2].Content, L"text") == 0) list.Format = ListFormat::Text;
					else if (string::CompareIgnoreCase(command[2].Content, L"binary") == 0) list.Format = ListFormat::Binary;
					else if (string::CompareIgnoreCase(command[2].Content, L"legacy") == 0) list.Format = ListFormat::Legacy;
				} else if (string::CompareIgnoreCase(command[1].Content, L"source") == 0) {
					string source = command[2].Content;
					if (command.Length() > 3 && string::CompareIgnoreCase(command[3].Content, L"embed") == 0) {
						try {
							FileStream file(source, AccessRead, OpenExisting);
							Array<uint8> data(0x100);
							data.SetLength(int(file.Length()));
							file.Read(data.GetBuffer(), data.Length());
							for (int i = 0; i < selection.Length(); i++) {
								list.Files[selection[i]].SourceData = data;
								list.Files[selection[i]].SourceFile = L"";
							}
						} catch (...) { console << TextColor(ConsoleColor::Red) << L"Error setting embeded data." << TextColorDefault() << LineFeed(); return false; }
					} else {
						for (int i = 0; i < selection.Length(); i++) {
							list.Files[selection[i]].SourceData.Clear();
							list.Files[selection[i]].SourceFile = source;
						}
					}
				} else if (string::CompareIgnoreCase(command[1].Content, L"name") == 0) {
					string value = command[2].Content;
					for (int i = 0; i < selection.Length(); i++) list.Files[selection[i]].Name = value;
				} else if (string::CompareIgnoreCase(command[1].Content, L"type") == 0) {
					string value = command[2].Content;
					for (int i = 0; i < selection.Length(); i++) list.Files[selection[i]].Type = value;
				} else if (string::CompareIgnoreCase(command[1].Content, L"id") == 0) {
					uint32 value = uint32(command[2].AsInteger());
					for (int i = 0; i < selection.Length(); i++) list.Files[selection[i]].ID = value;
				} else if (string::CompareIgnoreCase(command[1].Content, L"user") == 0) {
					uint32 value = uint32(command[2].AsInteger());
					for (int i = 0; i < selection.Length(); i++) list.Files[selection[i]].Custom = value;
				} else if (string::CompareIgnoreCase(command[1].Content, L"time") == 0) {
					int tid = 0;
					if (string::CompareIgnoreCase(command[2].Content, L"c") == 0) tid = 1;
					else if (string::CompareIgnoreCase(command[2].Content, L"a") == 0) tid = 2;
					else if (string::CompareIgnoreCase(command[2].Content, L"m") == 0) tid = 3;
					ArchiveEntityTime time;
					time.Class = TimeClass::Unset;
					time.Value = 0;
					if (command.Length() > 3) {
						if (string::CompareIgnoreCase(command[3].Content, L"unset") == 0) time.Class = TimeClass::Unset;
						else if (string::CompareIgnoreCase(command[3].Content, L"explicit") == 0) time.Class = TimeClass::Explicit;
						else if (string::CompareIgnoreCase(command[3].Content, L"auto") == 0) time.Class = TimeClass::Automatic;
						else if (string::CompareIgnoreCase(command[3].Content, L"inherit") == 0) time.Class = TimeClass::InheritSource;
						else if (string::CompareIgnoreCase(command[3].Content, L"arc") == 0) time.Class = TimeClass::ArchiveTime;
						else tid = 0;
						if (command.Length() > 10) {
							time.Value = Time(uint32(command[4].AsInteger()), uint32(command[5].AsInteger()), uint32(command[6].AsInteger()),
								uint32(command[7].AsInteger()), uint32(command[8].AsInteger()), uint32(command[9].AsInteger()), uint32(command[10].AsInteger())).ToUniversal();
						}
					} else tid = 0;
					if (tid) {
						for (int i = 0; i < selection.Length(); i++) {
							if (tid == 1) list.Files[selection[i]].CreationTime = time;
							else if (tid == 2) list.Files[selection[i]].AccessTime = time;
							else if (tid == 3) list.Files[selection[i]].AlterTime = time;
						}
					}
				} else if (string::CompareIgnoreCase(command[1].Content, L"chain") == 0) {
					MethodChain chain = 0;
					int p = 2;
					while (p < command.Length() && chain.Length() < 8) {
						CompressionMethod method = static_cast<CompressionMethod>(0);
						if (string::CompareIgnoreCase(command[p].Content, L"huffman") == 0) method = CompressionMethod::Huffman;
						else if (string::CompareIgnoreCase(command[p].Content, L"lzw") == 0) method = CompressionMethod::LempelZivWelch;
						else if (string::CompareIgnoreCase(command[p].Content, L"rle8") == 0) method = CompressionMethod::RunLengthEncoding8bit;
						else if (string::CompareIgnoreCase(command[p].Content, L"rle16") == 0) method = CompressionMethod::RunLengthEncoding16bit;
						else if (string::CompareIgnoreCase(command[p].Content, L"rle32") == 0) method = CompressionMethod::RunLengthEncoding32bit;
						else if (string::CompareIgnoreCase(command[p].Content, L"rle64") == 0) method = CompressionMethod::RunLengthEncoding64bit;
						else if (string::CompareIgnoreCase(command[p].Content, L"rle128") == 0) method = CompressionMethod::RunLengthEncoding128bit;
						else { console << TextColor(ConsoleColor::Red) << L"Unknown method." << TextColorDefault() << LineFeed(); return false; }
						p++;
						chain = chain.Append(method);
					}
					for (int i = 0; i < selection.Length(); i++) list.Files[selection[i]].CompressionChain = chain;
				} else if (string::CompareIgnoreCase(command[1].Content, L"block") == 0) {
					uint32 value = uint32(command[2].AsInteger());
					for (int i = 0; i < selection.Length(); i++) list.Files[selection[i]].CompressionBlock = value;
				} else if (string::CompareIgnoreCase(command[1].Content, L"quality") == 0) {
					CompressionQuality quality = CompressionQuality::Force;
					if (string::CompareIgnoreCase(command[2].Content, L"f") == 0) quality = CompressionQuality::Force;
					else if (string::CompareIgnoreCase(command[2].Content, L"o") == 0) quality = CompressionQuality::Optional;
					else if (string::CompareIgnoreCase(command[2].Content, L"s") == 0) quality = CompressionQuality::Sequential;
					else if (string::CompareIgnoreCase(command[2].Content, L"v") == 0) quality = CompressionQuality::Variative;
					else if (string::CompareIgnoreCase(command[2].Content, L"e") == 0) quality = CompressionQuality::ExtraVariative;
					for (int i = 0; i < selection.Length(); i++) list.Files[selection[i]].Quality = quality;
				} else if (string::CompareIgnoreCase(command[1].Content, L"extended") == 0) {
					bool value = false;
					if (string::CompareIgnoreCase(command[2].Content, L"yes") == 0) value = true;
					else if (string::CompareIgnoreCase(command[2].Content, L"no") == 0) value = false;
					for (int i = 0; i < selection.Length(); i++) list.Files[selection[i]].AddMetadata = value;
				} else {
					console << TextColor(ConsoleColor::Red) << L"Unknown property." << TextColorDefault() << LineFeed();
					return false;
				}
			} else if (string::CompareIgnoreCase(command[0].Content, L"attr") == 0 && command.Length() > 2) {
				string key = command[2].Content;
				if (string::CompareIgnoreCase(command[1].Content, L"rem") == 0) {
					for (int i = 0; i < selection.Length(); i++) {
						for (int j = list.Files[selection[i]].Attributes.Length() - 1; j >= 0; j--) {
							if (string::CompareIgnoreCase(list.Files[selection[i]].Attributes[j].Key, key) == 0) list.Files[selection[i]].Attributes.Remove(j);
						}
					}
				} else if (string::CompareIgnoreCase(command[1].Content, L"set") == 0 && command.Length() > 3) {
					string value = command[3].Content;
					for (int i = 0; i < selection.Length(); i++) {
						if (list.Files[selection[i]].Attributes.Length()) {
							for (int j = list.Files[selection[i]].Attributes.Length() - 1; j >= 0; j--) {
								if (string::CompareIgnoreCase(list.Files[selection[i]].Attributes[j].Key, key) == 0) {
									list.Files[selection[i]].Attributes[j].Value = value;
									break;
								}
								if (!j) {
									ArchiveAttribute attr;
									attr.Key = key;
									attr.Value = value;
									list.Files[selection[i]].Attributes << attr;
								}
							}
						} else {
							ArchiveAttribute attr;
							attr.Key = key;
							attr.Value = value;
							list.Files[selection[i]].Attributes << attr;
						}
					}
				}
			} else if (string::CompareIgnoreCase(command[0].Content, L"name") == 0 && command.Length() > 2) {
				if (string::CompareIgnoreCase(command[1].Content, L"prepend") == 0 && command.Length() > 2) {
					auto prefix = command[2].Content;
					for (int i = 0; i < selection.Length(); i++) {
						auto & file = list.Files[selection[i]];
						file.Name = prefix + file.Name;
					}
				} else if (string::CompareIgnoreCase(command[1].Content, L"append") == 0 && command.Length() > 2) {
					auto postfix = command[2].Content;
					for (int i = 0; i < selection.Length(); i++) {
						auto & file = list.Files[selection[i]];
						file.Name = file.Name + postfix;
					}
				} else if (string::CompareIgnoreCase(command[1].Content, L"replace") == 0 && command.Length() > 3) {
					auto text = command[2].Content;
					auto with = command[3].Content;
					for (int i = 0; i < selection.Length(); i++) {
						auto & file = list.Files[selection[i]];
						file.Name = file.Name.Replace(text, with);
					}
				} else {
					console << TextColor(ConsoleColor::Red) << L"Unknown operation." << TextColorDefault() << LineFeed();
					return false;
				}
			} else if (string::CompareIgnoreCase(command[0].Content, L"arc") == 0) {
				list.Archive(console);
			} else return false;
		} else return false;
	} else return false;
	return true;
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
		console << L"  " << ENGINE_VI_APPSYSNAME << L" <list> [:create] [:cs <command file>] [:arc]" << LineFeed();
		console << L"Where" << LineFeed();
		console << L"  list         - archive file list to edit, .ecsap or .ecsal," << LineFeed();
		console << L"  :create      - create or erase existing list instead of loading," << LineFeed();
		console << L"  :cs          - execute commands from source different of standard input," << LineFeed();
		console << L"  command file - text file with commands, ANSI encoding if no signature present," << LineFeed();
		console << L"  :arc         - archives the list without command interpretation, then exits." << LineFeed();
		console << L"Use \"?\" command with file opened to get internal command list." << LineFeed();
		console << LineFeed();
	} else {
		bool create = false;
		bool prompt = true;
		bool archive = false;
		bool break_on_error = false;
		string list_file;
		SafePointer<ITextReader> input;
		input.SetRetain(&console);
		for (int i = 1; i < args->Length(); i++) {
			if (string::CompareIgnoreCase(args->ElementAt(i), L":create") == 0) {
				create = true;
			} else if (string::CompareIgnoreCase(args->ElementAt(i), L":arc") == 0) {
				archive = true;
			} else if (string::CompareIgnoreCase(args->ElementAt(i), L":cs") == 0 && i < args->Length() - 1) {
				i++;
				try {
					SafePointer<FileStream> input_stream = new FileStream(args->ElementAt(i), AccessRead, OpenExisting);
					input.SetReference(new TextReader(input_stream));
					prompt = false;
					break_on_error = true;
				}
				catch (...) {
					console << TextColor(ConsoleColor::Red) << L"Failed to attach command source." << TextColorDefault() << LineFeed();
					return 1;
				}
			} else if (args->ElementAt(i)[0] != L':') {
				list_file = args->ElementAt(i);
			} else {
				console << TextColor(ConsoleColor::Red) << L"Invalid command line option." << TextColorDefault() << LineFeed();
				return 1;
			}
		}
		if (!list_file.Length()) {
			console << TextColor(ConsoleColor::Red) << L"No list file name specified." << TextColorDefault() << LineFeed();
			return 1;
		}
		try {
			if (create) {
				list.ListName = IO::ExpandPath(list_file);
				console << TextColor(ConsoleColor::Yellow) << L"Created an empty list." << TextColorDefault() << LineFeed();
			} else {
				if (!ArchiveList::Load(list, list_file)) {
					console << TextColor(ConsoleColor::Red) << L"Failed to open an archive list." << TextColorDefault() << LineFeed();
					return 1;
				}
				console << TextColor(ConsoleColor::Yellow) << L"Loaded a list with " << list.Files.Length() << L" files." << TextColorDefault() << LineFeed();
			}
			if (archive) {
				console << LineFeed();
				if (!list.Archive(console)) return 1;
				return 0;
			}
			command_spelling.IsolatedChars << L'?';
			while (!input->EofReached()) {
				if (prompt) {
					console << LineFeed() << IO::Path::GetFileName(list.ListName).UpperCase() << TextColor(ConsoleColor::Magenta) << L"> " << TextColorDefault();
				}
				string command = input->ReadLine();
				if (!command.Length()) continue;
				SafePointer<Array<Syntax::Token>> decomposed;
				try {
					decomposed = Syntax::ParseText(command, command_spelling);
					decomposed->RemoveLast();
					if (!Execute(*decomposed, console)) {
						if (exiting) break;
						else throw Exception();
					}
				}
				catch (Syntax::ParserSpellingException & e) {
					if (break_on_error) {
						console << TextColor(ConsoleColor::Red) << command + L": Invalid command syntax." << TextColorDefault() << LineFeed();
						break;
					}
					console << TextColor(ConsoleColor::Yellow) << L"Invalid command syntax. Type \"?\"." << TextColorDefault() << LineFeed();
				}
				catch (...) {
					if (break_on_error) {
						console << TextColor(ConsoleColor::Red) << command + L": Failed to execute the command." << TextColorDefault() << LineFeed();
						break;
					}
					console << TextColor(ConsoleColor::Yellow) << L"Failed to execute the command." << TextColorDefault() << LineFeed();
				}
			}
		}
		catch (...) {
			console << TextColor(ConsoleColor::Red) << L"Internal error." << TextColorDefault() << LineFeed();
			return 1;
		}
	}
	return 0;
}