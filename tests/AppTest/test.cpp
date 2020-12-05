#include <EngineRuntime.h>

using namespace Engine;
using namespace Engine::Application;
using namespace Engine::UI;

string NameOfArchitecture(Platform arch)
{
	if (arch == Platform::X86) return L"Intel x86";
	else if (arch == Platform::X64) return L"Intel x86-64";
	else if (arch == Platform::ARM) return L"ARM";
	else if (arch == Platform::ARM64) return L"ARM64";
	else return L"Unknown";
}
#ifdef ENGINE_DEBUG
string config = L"debug";
#else
string config = L"release";
#endif

class Callback : public IApplicationCallback {};

int Main(void)
{
	Callback callback;
	CreateController(&callback);
	string text = FormatString(L"Hello, Engine Runtime!\n\nTest was built for %0, current system is %1.\nOperating system: %2, %3 bytes of RAM.\nWas built using %4 configuration.",
		NameOfArchitecture(ApplicationPlatform), NameOfArchitecture(GetSystemPlatform()), OperatingSystemName, GetInstalledMemory(), config);
	GetController()->SystemMessageBox(0, text, ENGINE_VI_APPNAME, 0, MessageBoxButtonSet::Ok, MessageBoxStyle::Information, 0);
	return 0;
}