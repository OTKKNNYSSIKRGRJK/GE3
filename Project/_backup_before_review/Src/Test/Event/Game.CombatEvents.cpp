module;

// 昔のイベントシステム。#include するのはこの翻訳単位だけ（Game.CombatEvents.ixx の説明を参照）
#include "EventSystem.h"

module Game.CombatEvents;

namespace {
	// EventSystem.h のマクロで、EventInterface の派生と型の番号（TypeHandle）を作る
	INNO_EVENT(HitEvent) {
		INNO_REGEISTER_EVENTTYPE(HitEvent);
	public:
		Game::CombatEvents::Hit Payload{};
	};

	INNO_EVENT(WallTouchEvent) {
		INNO_REGEISTER_EVENTTYPE(WallTouchEvent);
	public:
		Game::CombatEvents::WallTouch Payload{};
	};

	auto Manager() -> Innominatus::EventManager& {
		return *Innominatus::EventManager::Instance();
	}
}

namespace Game::CombatEvents {
	void Publish(Hit const& e_) {
		HitEvent e{};
		e.Payload = e_;
		Manager().Queue_Event(e);
	}

	void Publish(WallTouch const& e_) {
		WallTouchEvent e{};
		e.Payload = e_;
		Manager().Queue_Event(e);
	}

	void Subscribe(void* context_, HitHandler handler_) {
		if (handler_ == nullptr) { return; }
		Manager().Add_EventListener<HitEvent>(
			Innominatus::EventHandler<HitEvent>{
				[context_, handler_](HitEvent const& e_) { handler_(context_, e_.Payload); }
			}
		);
	}

	void Subscribe(void* context_, WallTouchHandler handler_) {
		if (handler_ == nullptr) { return; }
		Manager().Add_EventListener<WallTouchEvent>(
			Innominatus::EventHandler<WallTouchEvent>{
				[context_, handler_](WallTouchEvent const& e_) { handler_(context_, e_.Payload); }
			}
		);
	}

	void Dispatch() {
		Manager().Dispatch_Event();
	}
}
