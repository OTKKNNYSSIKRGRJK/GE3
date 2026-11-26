#include"EventSystem.h"

namespace Innominatus {
	void EventManager::Initialize() {}

	void EventManager::Finalize() {
		for (auto it{ Container_EventListeners.begin() }; it != Container_EventListeners.end(); ++it) {
			for (auto& evtListener : (*it)) {
				delete evtListener.release();
			}
		}
		Container_EventListeners.clear();
	}

	void EventManager::Dispatch_Event() {
		while (!Container_Event.empty()) {
			Trigger_Event_with_TID(*(Container_Event.front().second.get()), Container_Event.front().first);
			delete Container_Event.front().second.release();
			Container_Event.pop();
		}
	}
}