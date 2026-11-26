# エディタUIモジュール 統合ガイド

## 構成
```
editor/
  IEditorPanel.h        パネル共通インターフェース
  PanelBase.h            タイトル/Openフラグ管理の基底クラス
  EditorLayout.h/.cpp     DockSpace構築＋パネル一括管理
  Panels/
    ConsolePanel.h/.cpp
    HierarchyPanel.h/.cpp
    InspectorPanel.h/.cpp
    ScenePanel.h/.cpp
    AIAssistantPanel.h/.cpp
```

## 1. 初期化（既存のImGuiセットアップの後）

```cpp
#include "editor/EditorLayout.h"
#include "editor/Panels/ConsolePanel.h"
#include "editor/Panels/HierarchyPanel.h"
#include "editor/Panels/InspectorPanel.h"
#include "editor/Panels/ScenePanel.h"
#include "editor/Panels/AIAssistantPanel.h"

// 既存のAI API通信クラスをIAIAssistantでラップしますわ
class MyAIAssistant : public Editor::IAIAssistant
{
public:
    void SendMessageAsync(
        const std::vector<Editor::ChatMessage>& history,
        std::function<void(std::string)> onResponse,
        std::function<void(std::string)> onError) override
    {
        // 既存のAPI呼び出しをワーカースレッド or 非同期タスクで実行し、
        // 完了したらonResponse(responseText)、失敗したらonError(errorText)を呼んでくださいまし。
        // 例）m_existingApiClient.SendAsync(ConvertHistory(history), onResponse, onError);
    }
};

Editor::EditorLayout g_editorLayout;
Editor::ConsolePanel* g_console = nullptr;
Editor::HierarchyPanel* g_hierarchy = nullptr;
Editor::InspectorPanel* g_inspector = nullptr;
Editor::ScenePanel* g_scene = nullptr;
Editor::AIAssistantPanel* g_aiAssistant = nullptr;

void InitEditorUI()
{
    auto console = std::make_unique<Editor::ConsolePanel>();
    g_console = console.get();

    auto hierarchy = std::make_unique<Editor::HierarchyPanel>();
    g_hierarchy = hierarchy.get();

    auto inspector = std::make_unique<Editor::InspectorPanel>();
    g_inspector = inspector.get();

    auto scene = std::make_unique<Editor::ScenePanel>();
    g_scene = scene.get();

    auto aiAssistant = std::make_unique<Editor::AIAssistantPanel>(
        std::make_shared<MyAIAssistant>());
    g_aiAssistant = aiAssistant.get();

    // Hierarchyで選択 → Inspectorに反映、AIへのコンテキストヒントも更新
    g_hierarchy->SetOnSelect([](uint64_t id)
    {
        g_inspector->SetSelectedId(id);
        g_aiAssistant->SetContextHint("Selected entity id=" + std::to_string(id));
    });

    // Inspectorの実描画（既存のコンポーネント型に合わせて実装してくださいまし）
    g_inspector->SetDrawCallback([](uint64_t id)
    {
        // 例）ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
    });

    // AIの提案を実際に適用する処理
    g_aiAssistant->SetOnApplySuggestion([](const Editor::ParameterSuggestion& s)
    {
        // 例）ParamRegistry::Set(s.paramPath, s.suggestedValue);
        g_console->AddLog(Editor::LogLevel::Info, "Applied: " + s.paramPath);
    });

    g_editorLayout.RegisterPanel(std::move(hierarchy));
    g_editorLayout.RegisterPanel(std::move(scene));
    g_editorLayout.RegisterPanel(std::move(inspector));
    g_editorLayout.RegisterPanel(std::move(aiAssistant));
    g_editorLayout.RegisterPanel(std::move(console));
}
```

## 2. 毎フレームの描画

```cpp
void RenderEditorUI()
{
    // Sceneに表示するSRVを毎フレーム（もしくはリサイズ時のみ）更新しますわ
    g_scene->SetSceneTexture(
        reinterpret_cast<ImTextureID>(sceneSrvGpuHandle.ptr),
        static_cast<float>(sceneWidth),
        static_cast<float>(sceneHeight));

    g_editorLayout.OnImGuiRender();

    // Sceneパネルのサイズが変わっていたら、オフスクリーンRTVを再生成してくださいまし
    auto vp = g_scene->GetViewportSize();
    if (vp.width != lastWidth || vp.height != lastHeight)
    {
        ResizeSceneRenderTarget(vp.width, vp.height);
    }
}
```

## 3. メインメニューバーへの組み込み（任意）

```cpp
if (ImGui::BeginMainMenuBar())
{
    if (ImGui::BeginMenu("File")) { /* ... */ ImGui::EndMenu(); }
    g_editorLayout.DrawWindowMenu(); // Window > Hierarchy / Scene / ... のトグル
    ImGui::EndMainMenuBar();
}
```

## 補足事項

- **日本語IME入力**: `AIAssistantPanel`の入力欄などで日本語を直接入力する場合、
  `ImGuiIO::Fonts`に日本語グリフレンジ（`GetGlyphRangesJapanese()`）を含むフォントを
  登録しておく必要がございますわ。Win32バックエンドはIME自体には対応しておりますので、
  フォント設定さえ済めば問題なく入力できましてよ。
- **DockBuilderの文字列一致**: `EditorLayout::BuildInitialLayout`内の
  `DockBuilderDockWindow`に渡す文字列は、各パネルの`GetTitle()`と完全一致させる必要が
  ございます（新しいパネルを追加する際はご注意くださいまし）。
- **AIパラメータ提案の連携**: AIの応答テキストから`ParameterSuggestion`を抽出する
  パース処理は、既存のAPI通信部分（別チャットでご検討中とのこと）に合わせて
  実装していただく想定ですの。応答をJSON形式で返すようプロンプト設計しておくと
  パースが安定いたしますわ。
- **拡張の余地**: Behavior Tree EditorやContent Browser、Profilerなどは
  同じ`IEditorPanel`パターンで追加していただけますわ。ご要望あれば
  いつでもお申し付けくださいまし。
