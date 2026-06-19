module;

#pragma warning(push)
#pragma warning(disable : 4127)
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include<httplib.h>
#pragma warning(pop)

export module HTTPTest;

import <string>;

import nlohmann.json;

namespace Lumina {
	export auto CommunicationTest() -> void {
		// * LLM Client
		httplib::Client client_LLM{ "https://generativelanguage.googleapis.com" };
		// * API Key
		std::string const apiKey{ "" };
		// * Model Path
		std::string const modelPath{ "/v1beta/models/gemini-2.5-flash:generateContent?key=" + apiKey };

		// * Request Body
		nlohmann::json requestBody{};
		requestBody["contents"] = nlohmann::json::object();
		requestBody["contents"]["parts"] = nlohmann::json::object();
		requestBody["contents"]["parts"]["text"] = "I'd like to know the answer of 1+1. This is an API test.";
		
		// * Response from POST Request
		httplib::Result response{
			client_LLM.Post(
				modelPath.data(),
				requestBody.dump(),
				"application/json"
			)
		};

		/*if (response && response->status == 200) {
			auto j = nlohmann::json::parse(response->body);
			std::cout << "AIÇÃï‘ìö: " << j["candidates"][0]["content"]["parts"][0]["text"] << std::endl;
		}
		else {
			std::cout << "ÉGÉâÅ[î≠ê∂: " << (res ? std::to_string(res->status) : "ê⁄ë±é∏îs") << std::endl;
		}*/
	}
}