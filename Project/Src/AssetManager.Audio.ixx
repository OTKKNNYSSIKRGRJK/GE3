module;

#include<xaudio2.h>
#include<mmsystem.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "winmm.lib")

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

export module Lumina.AssetManager : Audio;

import <cstdint>;

import <memory>;

import <vector>;

import <string>;
import <format>;

import <fstream>;

import Lumina.Utils.Debug;

namespace Lumina::XAudio2 {
	class AudioManager;

	struct AudioStream {
		friend AudioManager;

		class Handle { int ID; };

		WAVEFORMATEX* WaveFormat{ nullptr };
		std::vector<byte> Buffer{};

	private:
		static inline AudioManager* Manager_{ nullptr };
	};

	struct AudioStreamPlayer {
		friend AudioManager;

		class Handle { int ID; };

		IXAudio2SourceVoice* Source{ nullptr };

		/*AudioStreamPlayer(
			AudioStream const& audio_,
			bool flag_Loop_,
			float volume_
		) {
			
		}*/

	private:
		static inline AudioManager* Manager_{ nullptr };
	};
}

namespace Lumina::XAudio2 {
	class AudioManager {
	public:
		AudioStream::Handle LoadFromFile(std::wstring_view filePath_);

		AudioStreamPlayer::Handle Play(
			AudioStream::Handle hndl_Stream_,
			bool flag_Loop_,
			float volume_
		);

	public:
		void Update();

	public:
		void Initialize();
		void Finalize() noexcept;

	private:
		IXAudio2* XAudio2_{ nullptr };
		IXAudio2MasteringVoice* MasteringVoice_{ nullptr };

		std::vector<AudioStream> Array_Streams_{};
		std::vector<AudioStreamPlayer> Array_StreamPlayers_{};
	};

	void AudioManager::Initialize() {
		// COM library initialization
		::CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		::XAudio2Create(&XAudio2_) ||
		Utils::Debug::ThrowIfFailed{ "<AudioManager::Initialize> Failed to create XAudio2!\n" };

		#if defined(_DEBUG)
		XAUDIO2_DEBUG_CONFIGURATION debugConfig{
			.TraceMask{ XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS },
			.BreakMask{ XAUDIO2_LOG_ERRORS },
		};
		XAudio2_->SetDebugConfiguration(&debugConfig, nullptr);
		#endif

		XAudio2_->CreateMasteringVoice(&MasteringVoice_) ||
		Utils::Debug::ThrowIfFailed{ "<AudioManager::Initialize> Failed to create MasteringVoice!\n" };

		// MediaFoundation Initialization
		::MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET) ||
		Utils::Debug::ThrowIfFailed{ "<AudioManager::Initialize> Failed to initialize MediaFoundation!\n" };

		AudioStream::Manager_ = this;
		AudioStreamPlayer::Manager_ = this;
	}

	void AudioManager::Finalize() noexcept {
		for (auto& streamPlayer : Array_StreamPlayers_) {
			if (streamPlayer.Source != nullptr) {
				streamPlayer.Source->DestroyVoice();
				streamPlayer.Source = nullptr;
			}
		}

		for (auto& stream : Array_Streams_) {
			::CoTaskMemFree(stream.WaveFormat);
		}

		::MFShutdown();

		MasteringVoice_->DestroyVoice();
		XAudio2_->Release();

		::CoUninitialize();
	}

	AudioStream::Handle AudioManager::LoadFromFile(std::wstring_view filePath_) {
		int handle_Stream{ static_cast<int>(Array_Streams_.size()) };
		auto& stream{ Array_Streams_.emplace_back() };

		IMFSourceReader* sourceReader{ nullptr };
		::MFCreateSourceReaderFromURL(filePath_.data(), nullptr, &sourceReader);

		IMFMediaType* mediaType{ nullptr };
		::MFCreateMediaType(&mediaType);
		mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		sourceReader->SetCurrentMediaType(
			static_cast<uint32_t>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
			nullptr,
			mediaType
		);
		mediaType->Release();
		mediaType = nullptr;
		sourceReader->GetCurrentMediaType(
			static_cast<uint32_t>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
			&mediaType
		);

		::MFCreateWaveFormatExFromMFMediaType(mediaType, &(stream.WaveFormat), nullptr);

		while (true) {
			IMFSample* sample{ nullptr };
			DWORD streamFlags{};
			sourceReader->ReadSample(
				static_cast<uint32_t>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
				0U,
				nullptr,
				&streamFlags,
				nullptr,
				&sample
			);
			if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) { break; }

			IMFMediaBuffer* mediaBuffer{ nullptr };
			sample->ConvertToContiguousBuffer(&mediaBuffer);

			byte* buffer{ nullptr };
			DWORD bufferLengthInBytes{ 0 };
			mediaBuffer->Lock(&buffer, nullptr, &bufferLengthInBytes);
			stream.Buffer.resize(stream.Buffer.size() + bufferLengthInBytes);
			std::memcpy(
				stream.Buffer.data() + stream.Buffer.size() - bufferLengthInBytes,
				buffer,
				bufferLengthInBytes
			);
			mediaBuffer->Unlock();

			mediaBuffer->Release();
			sample->Release();
		}

		mediaType->Release();
		sourceReader->Release();

		return *reinterpret_cast<AudioStream::Handle*>(&handle_Stream);
	}

	AudioStreamPlayer::Handle AudioManager::Play(
		AudioStream::Handle hndl_Stream_,
		bool flag_Loop_,
		float volume_
	) {
		int streamID{ *reinterpret_cast<int*>(&hndl_Stream_) };
		auto const& stream{ Array_Streams_.at(streamID) };

		IXAudio2SourceVoice* audioSource{ nullptr };
		XAudio2_->CreateSourceVoice(&audioSource, stream.WaveFormat) ||
		Utils::Debug::ThrowIfFailed{ "<AudioManager::Play> Failed to create IXAudio2SourceVoice!\n" };

		XAUDIO2_BUFFER audioBuffer{
			.Flags{ XAUDIO2_END_OF_STREAM },
			.AudioBytes{ sizeof(byte) * static_cast<uint32_t>(stream.Buffer.size()) },
			.pAudioData{ stream.Buffer.data() },
			.LoopCount{ (flag_Loop_) ? (XAUDIO2_LOOP_INFINITE) : (0U) },
		};
		audioSource->SubmitSourceBuffer(&audioBuffer);
		audioSource->SetVolume(volume_);
		audioSource->Start(0U);

		int handle_StreamPlayer = 0;
		int num_StreamPlayers = static_cast<int>(Array_StreamPlayers_.size());
		for (; handle_StreamPlayer < num_StreamPlayers; ++handle_StreamPlayer) {
			if (Array_StreamPlayers_[handle_StreamPlayer].Source == nullptr) {
				Array_StreamPlayers_[handle_StreamPlayer].Source = audioSource;
				break;
			}
		}
		if (handle_StreamPlayer == num_StreamPlayers) {
			Array_StreamPlayers_.emplace_back(audioSource);
		}
		return *reinterpret_cast<AudioStreamPlayer::Handle*>(&handle_StreamPlayer);
	}

	void AudioManager::Update() {
		static XAUDIO2_VOICE_STATE state{};
		for (size_t i = 0; i < Array_StreamPlayers_.size(); ++i) {
			if (Array_StreamPlayers_[i].Source != nullptr) {
				Array_StreamPlayers_[i].Source->GetState(&state);
				if (state.BuffersQueued == 0) {
					Array_StreamPlayers_[i].Source->DestroyVoice();
					Array_StreamPlayers_[i].Source = nullptr;
				}
			}
		}
	}
}