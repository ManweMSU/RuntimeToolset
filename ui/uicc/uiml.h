#pragma once

#include <UserInterface/InterfaceFormat.h>
#include <Storage/Registry.h>

namespace Engine
{
	namespace UI
	{
		namespace Markup
		{
			enum class ErrorClass { OK, UnknownError, SourceAccess, MainInvalidToken, IncludedInvalidToken, UnexpectedLexem, InvalidLocaleIdentifier, InvalidSystemColor, InvalidEffect, InvalidConstantType, InvalidProperty, ObjectRedifinition, NumericConstantTypeMismatch, UndefinedObject };
			enum class WarningClass { InvalidControlParent, UnknownPlatformName };
			class IWarningReporter : public Object
			{
			public:
				virtual void ReportWarning(WarningClass warning_class, int warning_pos, int warning_length, const string & warning_info) = 0;
			};
			void SetWarningReporterCallback(IWarningReporter * callback);
			IWarningReporter * GetWarningReporterCallback(void);
			void SetSuborderingTable(Storage::RegistryNode * table);
			Storage::RegistryNode * GetSuborderingTable(void);
			Format::InterfaceTemplateImage * CompileInterface(const string & main_uiml, bool as_style, int & error_position, int & error_length, ErrorClass & error, string & error_descr, const string & inc_path = L"");
		}
	}
}