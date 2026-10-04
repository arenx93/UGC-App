#include "pch.h"
#include "Pages.h"
#include "WinUtil.h"
#include "framecraft/util.h"

using namespace winrt;
namespace mux = winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxm = winrt::Microsoft::UI::Xaml::Media;
using winrt::Windows::Foundation::IInspectable;

namespace fcapp {
namespace {

class ChatPage : public Page {
public:
    ChatPage() {
        root_ = muxc::Grid();
        root_.Padding(ui::margin(28, 22, 28, 22));
        root_.RowSpacing(12);
        for (auto height : {ui::autoLength(), ui::autoLength(), ui::star()}) {
            muxc::RowDefinition row;
            row.Height(height);
            root_.RowDefinitions().Append(row);
        }
        auto titles = ui::vstack(2);
        titles.Children().Append(ui::gradientTitle("ChatGPT", 30));
        titles.Children().Append(ui::secondary("Tus conversaciones se guardan automáticamente en este equipo."));
        root_.Children().Append(titles);

        statusHost_ = ui::card(ui::vstack(), 12);
        muxc::Grid::SetRow(statusHost_, 1);
        root_.Children().Append(statusHost_);

        auto body = ui::columns({ui::pixels(250), ui::star()}, 18);
        muxc::Grid::SetRow(body, 2);
        root_.Children().Append(body);

        auto historySurface = ui::vstack(10);
        historySurface.Padding(ui::uniform(12));
        auto historyHeader = ui::columns({ui::star(), ui::autoLength()}, 8);
        auto historyTitle = ui::text("Conversaciones", ui::Text::bodyStrong);
        historyTitle.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::place(historyHeader, historyTitle, 0);
        newButton_ = ui::button("Nuevo", L"\uE710", [] { AppModel::shared().newChat(); });
        newButton_.Padding(ui::margin(10, 6, 10, 6));
        ui::place(historyHeader, newButton_, 1);
        historySurface.Children().Append(historyHeader);
        muxc::ScrollViewer historyScroll;
        historyScroll.VerticalScrollBarVisibility(muxc::ScrollBarVisibility::Auto);
        historyPanel_ = ui::vstack(4);
        historyScroll.Content(historyPanel_);
        historySurface.Children().Append(historyScroll);
        muxc::Border historyBackground;
        historyBackground.Background(ui::resource(L"LayerFillColorDefaultBrush"));
        historyBackground.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(12));
        historyBackground.Child(historySurface);
        ui::place(body, historyBackground, 0);

        conversation_ = muxc::Grid();
        conversation_.RowSpacing(10);
        for (auto height : {ui::autoLength(), ui::star(), ui::autoLength()}) {
            muxc::RowDefinition row;
            row.Height(height);
            conversation_.RowDefinitions().Append(row);
        }
        currentTitle_ = ui::text("Nuevo chat", ui::Text::title);
        conversation_.Children().Append(currentTitle_);
        messagesScroll_ = muxc::ScrollViewer();
        messagesScroll_.VerticalScrollBarVisibility(muxc::ScrollBarVisibility::Auto);
        messagesScroll_.HorizontalScrollBarVisibility(muxc::ScrollBarVisibility::Disabled);
        messagesPanel_ = ui::vstack(16);
        messagesPanel_.Padding(ui::margin(2, 6, 8, 10));
        messagesScroll_.Content(messagesPanel_);
        muxc::Grid::SetRow(messagesScroll_, 1);
        conversation_.Children().Append(messagesScroll_);

        auto composerContent = ui::vstack(7);
        auto composeRow = ui::columns({ui::star(), ui::autoLength()}, 10);
        editor_ = ui::editor("Escribí un mensaje…", 62, [this](std::string const& text) {
            draft_ = text;
            updateComposer();
        });
        editor_.MaxHeight(170);
        editor_.KeyDown([this](IInspectable const&, mux::Input::KeyRoutedEventArgs const& args) {
            if (args.Key() == winrt::Windows::System::VirtualKey::Enter && (GetKeyState(VK_CONTROL) & 0x8000)) {
                args.Handled(true);
                send();
            }
        });
        ui::accessible(editor_, "Mensaje para ChatGPT");
        ui::place(composeRow, editor_, 0);
        sendButton_ = ui::primaryButton("Enviar", L"\uE724", [this] { send(); });
        sendButton_.VerticalAlignment(mux::VerticalAlignment::Bottom);
        ui::place(composeRow, sendButton_, 1);
        composerContent.Children().Append(composeRow);
        hint_ = ui::secondary("Ctrl+Enter para enviar", ui::Text::caption);
        composerContent.Children().Append(hint_);
        composer_ = ui::card(composerContent, 12);
        muxc::Grid::SetRow(composer_, 2);
        conversation_.Children().Append(composer_);
        ui::place(body, conversation_, 1);
    }

    mux::UIElement root() override { return root_; }
    void activated() override {
        auto& model = AppModel::shared();
        if (model.codexStatus.state == codex::State::unknown) model.refreshCodexStatus();
    }
    void refresh(Change change) override {
        if (change != Change::all && change != Change::chat && change != Change::codex) return;
        refreshStatus();
        refreshHistory();
        refreshMessages();
        updateComposer();
    }

private:
    void send() {
        auto text = fc::trim(draft_);
        auto& model = AppModel::shared();
        if (text.empty() || model.isChatting || model.codexStatus.state != codex::State::loggedIn) return;
        draft_.clear();
        editor_.Text(L"");
        model.sendChat(std::move(text));
    }

    void updateComposer() {
        auto& model = AppModel::shared();
        bool connected = model.codexStatus.state == codex::State::loggedIn;
        editor_.IsEnabled(connected && !model.isChatting);
        sendButton_.IsEnabled(connected && !model.isChatting && !fc::trim(draft_).empty());
        newButton_.IsEnabled(!model.isChatting);
        hint_.Text(model.isChatting ? L"ChatGPT está respondiendo…" : !connected ? L"Iniciá sesión para habilitar el chat"
                                                                            : L"Ctrl+Enter para enviar");
    }

    void refreshHistory() {
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> items;
        for (auto const& chat : model.chats) {
            bool selected = chat.id == model.selectedChatID;
            muxc::Button button;
            button.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
            button.HorizontalContentAlignment(mux::HorizontalAlignment::Stretch);
            button.Padding(ui::margin(10, 8, 10, 8));
            button.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(8));
            button.BorderThickness(ui::uniform(0));
            button.Background(selected ? ui::resource(L"AccentFillColorTertiaryBrush") : ui::resource(L"SubtleFillColorTransparentBrush"));
            auto labels = ui::vstack(1);
            auto title = ui::text(chat.title, ui::Text::bodyStrong);
            title.TextWrapping(mux::TextWrapping::NoWrap);
            title.TextTrimming(mux::TextTrimming::CharacterEllipsis);
            labels.Children().Append(title);
            labels.Children().Append(ui::secondary(chat.messages.empty() ? "Sin mensajes" : relativeTime(chat.updated), ui::Text::caption));
            button.Content(labels);
            ui::accessible(button, chat.title + (selected ? ", conversación actual" : ""));
            button.Click([id = chat.id](auto&&, auto&&) { AppModel::shared().selectChat(id); });
            if (model.chats.size() > 1 || !chat.messages.empty()) {
                muxc::MenuFlyout menu;
                muxc::MenuFlyoutItem remove;
                remove.Text(L"Eliminar conversación");
                remove.Click([id = chat.id](IInspectable const& sender, auto&&) {
                    ui::confirm(sender.as<mux::UIElement>().XamlRoot(), "¿Eliminar esta conversación?",
                                "El chat y todos sus mensajes se borrarán de este equipo.", "Eliminar",
                                [id] { AppModel::shared().deleteChat(id); });
                });
                menu.Items().Append(remove);
                button.ContextFlyout(menu);
                ui::tooltip(button, chat.title + " · clic derecho para eliminar");
            }
            items.push_back(button);
        }
        ui::setChildren(historyPanel_, items);
    }

    void refreshStatus() {
        auto& model = AppModel::shared();
        auto row = ui::columns({ui::autoLength(), ui::star(), ui::autoLength()}, 10);
        ui::place(row, ui::glyphBadge(L"\uE8BD", 32, model.codexStatus.state == codex::State::loggedIn), 0);
        auto labels = ui::vstack(1);
        std::string title, subtitle;
        switch (model.codexStatus.state) {
        case codex::State::unknown:
        case codex::State::checking: title = "Comprobando Codex…"; subtitle = "Verificando tu sesión de ChatGPT."; break;
        case codex::State::notInstalled: title = "Codex no está disponible"; subtitle = "Falta el componente incluido con Framecraft."; break;
        case codex::State::loggedOut: title = "Conectá tu cuenta de ChatGPT"; subtitle = "No necesitás una clave de API."; break;
        case codex::State::loggingIn: title = "Completá el acceso en el navegador"; subtitle = "Después volvé a Framecraft."; break;
        case codex::State::loggedIn: title = "ChatGPT conectado"; subtitle = model.codexStatus.detail; break;
        }
        labels.Children().Append(ui::text(title, ui::Text::bodyStrong));
        if (!subtitle.empty()) labels.Children().Append(ui::secondary(subtitle));
        ui::place(row, labels, 1);
        if (model.codexStatus.state == codex::State::loggedOut)
            ui::place(row, ui::primaryButton("Iniciar sesión", L"\uE77B", [] { AppModel::shared().codexLogin(); }), 2);
        else if (model.codexStatus.state == codex::State::notInstalled)
            ui::place(row, ui::button("Comprobar", L"\uE72C", [] { AppModel::shared().refreshCodexStatus(); }), 2);
        else if (model.codexStatus.state == codex::State::checking || model.codexStatus.state == codex::State::loggingIn) {
            muxc::ProgressRing ring;
            ring.IsActive(true);
            ring.Width(20);
            ring.Height(20);
            ui::place(row, ring, 2);
        }
        statusHost_.Child(row);
    }

    mux::UIElement messageBubble(ChatMessage const& message) {
        bool user = message.role == ChatMessage::Role::user;
        auto wrapper = ui::vstack(5);
        wrapper.HorizontalAlignment(user ? mux::HorizontalAlignment::Right : mux::HorizontalAlignment::Left);
        wrapper.MaxWidth(720);
        auto role = ui::secondary(user ? "Vos" : "ChatGPT", ui::Text::caption);
        role.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        wrapper.Children().Append(role);
        muxc::Border bubble;
        bubble.Padding(ui::margin(15, 11, 15, 12));
        bubble.CornerRadius(mux::CornerRadiusHelper::FromRadii(user ? 14 : 4, 14, user ? 4 : 14, 14));
        bubble.Background(user ? ui::brandGradient() : ui::resource(L"CardBackgroundFillColorDefaultBrush"));
        bubble.BorderBrush(user ? muxm::Brush(muxm::SolidColorBrush(ui::rgb(0, 0, 0, 0))) : ui::resource(L"CardStrokeColorDefaultBrush"));
        bubble.BorderThickness(ui::uniform(user ? 0 : 1));
        auto body = ui::text(message.text);
        body.IsTextSelectionEnabled(true);
        if (user) body.Foreground(muxm::SolidColorBrush(ui::rgb(255, 255, 255)));
        bubble.Child(body);
        wrapper.Children().Append(bubble);
        return wrapper;
    }

    void refreshMessages() {
        auto& model = AppModel::shared();
        auto const* chat = model.selectedChat();
        currentTitle_.Text(hs(chat ? chat->title : "Nuevo chat"));
        std::vector<mux::UIElement> items;
        if (!chat || (chat->messages.empty() && !model.isChatting)) {
            auto empty = ui::vstack(10);
            empty.MinHeight(250);
            empty.HorizontalAlignment(mux::HorizontalAlignment::Center);
            empty.VerticalAlignment(mux::VerticalAlignment::Center);
            auto badge = ui::glyphBadge(L"\uE8BD", 50);
            badge.HorizontalAlignment(mux::HorizontalAlignment::Center);
            empty.Children().Append(badge);
            auto title = ui::text("¿En qué te puedo ayudar?", ui::Text::title);
            title.TextAlignment(mux::TextAlignment::Center);
            empty.Children().Append(title);
            auto subtitle = ui::secondary("Pedí ideas, mejorá un texto, analizá una propuesta o resolvé una duda.", ui::Text::body);
            subtitle.TextAlignment(mux::TextAlignment::Center);
            subtitle.MaxWidth(500);
            empty.Children().Append(subtitle);
            items.push_back(empty);
        } else {
            for (auto const& message : chat->messages) items.push_back(messageBubble(message));
        }
        if (model.isChatting) {
            auto thinking = ui::hstack(10);
            muxc::ProgressRing ring;
            ring.IsActive(true);
            ring.Width(18);
            ring.Height(18);
            thinking.Children().Append(ring);
            thinking.Children().Append(ui::secondary("ChatGPT está pensando…", ui::Text::body));
            auto bubble = ui::card(thinking, 12);
            bubble.HorizontalAlignment(mux::HorizontalAlignment::Left);
            items.push_back(bubble);
        }
        if (model.chatError) {
            muxc::InfoBar error;
            error.IsOpen(true);
            error.IsClosable(false);
            error.Severity(muxc::InfoBarSeverity::Error);
            error.Title(L"No se pudo enviar el mensaje");
            error.Message(hs(*model.chatError));
            items.push_back(error);
        }
        ui::setChildren(messagesPanel_, items);
        root_.DispatcherQueue().TryEnqueue([weak = winrt::make_weak(messagesScroll_)] {
            if (auto scroll = weak.get()) scroll.ChangeView(nullptr, scroll.ScrollableHeight(), nullptr, true);
        });
    }

    muxc::Grid root_{nullptr}, conversation_{nullptr};
    muxc::Border statusHost_{nullptr}, composer_{nullptr};
    muxc::ScrollViewer messagesScroll_{nullptr};
    muxc::StackPanel historyPanel_{nullptr}, messagesPanel_{nullptr};
    muxc::TextBox editor_{nullptr};
    muxc::Button sendButton_{nullptr}, newButton_{nullptr};
    muxc::TextBlock hint_{nullptr}, currentTitle_{nullptr};
    std::string draft_;
};

}  // namespace
std::unique_ptr<Page> makeChatPage() { return std::make_unique<ChatPage>(); }
}  // namespace fcapp
