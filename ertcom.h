#pragma once

#include <EngineRuntime.h>

using namespace Engine;
using namespace Engine::Streaming;
using namespace Engine::IO::ConsoleControl;
using namespace Engine::Storage;

#define ERTBT_SUCCESS				0
#define ERTBT_PROJECT_FILE_ACCESS	1
#define ERTBT_INVALID_IDENTIFIER	2
#define ERTBT_INVALID_VERSION		3
#define ERTBT_OVERWRITE_FAILED		4
#define ERTBT_ATTACHMENT_FAILED		5
#define ERTBT_INVALID_INVOKATION	6
#define ERTBT_INVOKATION_FAILED		7
#define ERTBT_UNSUPPORTED_TRIPLE	9
#define ERTBT_INVALID_COMPILER_SET	10
#define ERTBT_COMPILATION_FAILED	11
#define ERTBT_INVALID_LINKER_SET	12
#define ERTBT_LINKING_FAILED		13
#define ERTBT_INVALID_RESOURCE_SET	14
#define ERTBT_COMMON_EXCEPTION		20
#define ERTBT_UNCOMMON_EXCEPTION	21
#define ERTBT_INVALID_CONFIGURATION	22
#define ERTBT_DUPLICATE_INPUT_FILE	23
#define ERTBT_INVALID_COMMAND_LINE	24
#define ERTBT_UNKNOWN_TARGET		25
#define ERTBT_INVALID_FILE_FORMAT	30
#define ERTBT_INVALID_RESOURCE		31
#define ERTBT_INVALID_FORMAT_ALIAS	32
#define ERTBT_BUNDLE_BUILD_ERROR	33
#define ERTBT_INVALID_RC_SET		34
#define ERTBT_RC_FAILED				35
#define ERTBT_EXTENSIONS_FAILED		40
#define ERTBT_EXTENSIONS_SYNTAX		41

enum class BuildTargetClass { Architecture, OperatingSystem, Configuration, Subsystem };

struct BuildTarget
{
	string Name;
	string HumanReadableName;
	bool Default;
};
struct VersionInformation
{
	bool CreateVersionDefines;
	string ApplicationName;
	string CompanyName;
	string Copyright;
	string InternalName;
	uint32 VersionMajor;
	uint32 VersionMinor;
	uint32 Subversion;
	uint32 Build;
	string ApplicationIdentifier;
	string CompanyIdentifier;
	string ApplicationDescription;
};
struct BuilderState {
	handle stdout_clone;
	handle stderr_clone;

	bool nologo = false;
	bool silent = false;
	bool shelllog = false;
	bool clean = false;
	bool pathout = false;
	bool build_cache = false;
	bool print_information = false;

	string runtime_source_path;
	string runtime_bootstrapper_path;
	string runtime_object_path;

	BuildTarget arch;
	BuildTarget os;
	BuildTarget subsys;
	BuildTarget conf;

	SafePointer<Registry> project;
	Time project_time;
	string project_file_path;
	string project_root_path;
	string project_output_name;
	string project_output_root;
	string project_object_path;
	string output_executable;

	VersionInformation version_information;

	Array<BuildTarget> vol_arch = Array<BuildTarget>(0x10);
	Array<BuildTarget> vol_os = Array<BuildTarget>(0x10);
	Array<BuildTarget> vol_subsys = Array<BuildTarget>(0x10);
	Array<BuildTarget> vol_conf = Array<BuildTarget>(0x10);

	Array<string> extra_include = Array<string>(0x10);
};

extern BuilderState state;

extern SafePointer<Registry> tool_config;
extern SafePointer<Registry> local_config;

string EscapeString(const string & input);
string EscapeStringRc(const string & input);
string EscapeStringXml(const string & input);
string ExpandPath(const string & path, const string & relative_to);
void ClearDirectory(const string & path);
void AppendArgumentLine(Array<string> & cc, const string & arg_word, const string & arg_val);
void PrintError(handle from, handle to);
bool CopyFile(const string & from, const string & to);

int ConfigurationInitialize(Console & console);
int SelectTarget(const string & name, BuildTargetClass cls, Console & console);
BuildTarget * FindTarget(const string & name, BuildTargetClass cls);
int MakeLocalConfiguration(Console & console);
int LoadProject(Console & console);
string GeneralCheckForForcedArchitecture(const string & arch, const string & os, const string & conf, const string & subs);
void ProjectPostConfig(void);
bool IsValidIdentifier(const string & value);
int LoadVersionInformation(Console & console);