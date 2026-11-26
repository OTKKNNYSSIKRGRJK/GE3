module;

#include<Windows.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.Utils.String;

import <string>;

//////	//////	//////	//////	//////	//////

namespace Lumina::Utils {
	export class String {
	public:
		template<typename CharType, uint32_t Size>
		struct Literal {
		public:
			constexpr CharType const* operator()() const noexcept { return Data_; }

		public:
			consteval Literal(CharType const (&str_)[Size]) noexcept {
				for (uint32_t i{ 0U }; i < Size; ++i) { Data_[i] = str_[i]; }
				Data_[Size - 1U] = '\0';
			}

		public:
			CharType Data_[Size]{};
		};

	public:
		static auto Convert(std::string_view str_) -> std::wstring;
		static auto Convert(std::wstring_view wStr_) -> std::string;
	};
}

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

module : private;

namespace Lumina::Utils {
	auto String::Convert(std::string_view str_) -> std::wstring {
		std::wstring result{};

		if (str_.empty()) { return result; }

		auto sizeInCharacters{
			// https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
			::MultiByteToWideChar(
				// Code page
				CP_UTF8,
				// Flags indicating the conversion type;
				// must be set to either 0 or MB_ERR_INVALID_CHARS for UTF-8
				// lest the function fail with ERROR_INVALID_FLAGS
				0,
				// Pointer to the string to convert
				str_.data(),
				// Size, in bytes, of the string to convert
				static_cast<int>(str_.size()),
				// Pointer to the buffer that receives the converted string;
				// useless since the next parameter is set to 0
				nullptr,
				// Size, in characters, of the buffer to receive the converted string;
				// the function returns the required buffer size in characters (including terminating null character) when set to 0
				0
			)
		};
		if (sizeInCharacters == 0) { return result; }

		result.resize(sizeInCharacters, 0);
		::MultiByteToWideChar(
			CP_UTF8,
			0,
			str_.data(),
			static_cast<int>(str_.size()),
			result.data(),
			sizeInCharacters
		);

		return result;
	}

	auto String::Convert(std::wstring_view wStr_) -> std::string {
		std::string result{};

		if (wStr_.empty()) { return result; }

		auto sizeInBytes{
			// https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
			::WideCharToMultiByte(
				// Code page
				CP_UTF8,
				// Flags indicating the conversion type;
				// must be set to either 0 or MB_ERR_INVALID_CHARS for UTF-8
				// lest the function fail with ERROR_INVALID_FLAGS
				0,
				// Pointer to the string to convert
				wStr_.data(),
				// Size, in characters, of the string to convert
				static_cast<int>(wStr_.size()),
				// Pointer to the buffer that receives the converted string;
				// useless since the next parameter is set to 0
				nullptr,
				// Size, in bytes, of the buffer to receive the converted string;
				// the function returns the required buffer size in bytes (including terminating null character) when set to 0
				0,
				// Pointer to the character to use if a character cannot be represented in the specified code page;
				// must be set to NULL or nullptr for UTF-8 lest the function fail with ERROR_INVALID_PARAMETER
				nullptr,
				// Pointer to a flag that indicates if the function has used a default character in the conversion;
				// must be set to NULL or nullptr for UTF-8 lest the function fail with ERROR_INVALID_PARAMETER
				nullptr
			)
		};
		if (sizeInBytes == 0) { return result; }

		result.resize(sizeInBytes, 0);
		::WideCharToMultiByte(
			CP_UTF8,
			0,
			wStr_.data(),
			static_cast<int>(wStr_.size()),
			result.data(),
			sizeInBytes,
			nullptr,
			nullptr
		);

		return result;
	}
}