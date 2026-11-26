module;

#include<Windows.h>

#include<DbgHelp.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

module Lumina.Utils.Debug : Exception;

//////	//////	//////	//////	//////	//////

namespace Lumina::Utils::Debug {
	namespace {
		int ExportDump(EXCEPTION_POINTERS* exception_) {
			/*
			CreateDirectory(L"./Dumps", nullptr);
			SYSTEMTIME time{};
			GetLocalTime(&time);
			wchar_t filePath[MAX_PATH]{ 0 };
			StringCchPrintfW(
				filePath, MAX_PATH,
				L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
				time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute
			);
			*/
			std::filesystem::create_directory("Dumps");
			std::wstring filePath{
				std::format(L"./Dumps/{:%Y-%m-%d-%H-%M-%S}.dmp", Timestamp<std::chrono::seconds>())
			};

			HANDLE dumpFileHandle{
				::CreateFile(
					filePath.data(),
					// Access mode
					GENERIC_READ | GENERIC_WRITE,
					// Share mode
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					// Security attributes
					0U,
					// Creation disposition
					CREATE_ALWAYS,
					// Miscellaneous flags and attributes
					0U,
					// Template file handle
					nullptr
				)
			};
			MINIDUMP_EXCEPTION_INFORMATION miniDumpInfo{
				.ThreadId{ ::GetCurrentThreadId() },
				.ExceptionPointers{ exception_ },
				.ClientPointers{ true }
			};
			::MiniDumpWriteDump(
				::GetCurrentProcess(),
				::GetCurrentProcessId(),
				dumpFileHandle,
				MINIDUMP_TYPE::MiniDumpNormal,
				&miniDumpInfo,
				nullptr,
				nullptr
			);

			return EXCEPTION_EXECUTE_HANDLER;
		}
	}

	LONG Exception::OnCrash(EXCEPTION_POINTERS* exception_) {
		return ExportDump(exception_);
	}
}