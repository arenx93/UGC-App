#include "pch.h"

#include "Pages.h"
#include "WinUtil.h"
#include "framecraft/util.h"

using namespace winrt;
namespace mux = winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxm = winrt::Microsoft::UI::Xaml::Media;
namespace shapes = winrt::Microsoft::UI::Xaml::Shapes;
namespace wf = winrt::Windows::Foundation;
namespace fs = std::filesystem;
using winrt::Windows::Foundation::IInspectable;

namespace fcapp {

namespace {

const std::vector<std::string> kLanguages = {"Español", "Español rioplatense", "Inglés", "Portugués", "Sin diálogo"};

std::wstring skillGlyph(std::string const& id) {
    if (id == fc::skills::kJsonProfileID) return L"";
    if (id == fc::skills::kUgcID) return L"";
    if (id == fc::skills::kArthasID) return L"";
    if (id.empty()) return L"";
    return L"";
}

std::wstring lintGlyph(fc::LintItem::State state) {
    switch (state) {
    case fc::LintItem::State::ok: return L"";
    case fc::LintItem::State::warning: return L"";
    case fc::LintItem::State::problem: return L"";
    case fc::LintItem::State::info: return L"";
    }
    return L"";
}

muxm::Brush lintBrush(fc::LintItem::State state) {
    switch (state) {
    case fc::LintItem::State::ok: return ui::resource(L"SystemFillColorSuccessBrush");
    case fc::LintItem::State::warning: return ui::resource(L"SystemFillColorCautionBrush");
    case fc::LintItem::State::problem: return ui::resource(L"SystemFillColorCriticalBrush");
    case fc::LintItem::State::info: return ui::resource(L"TextFillColorSecondaryBrush");
    }
    return ui::resource(L"TextFillColorSecondaryBrush");
}

/// Field title with an optional help tooltip.
mux::UIElement fieldTitle(std::string const& title, std::string const& help = "") {
    auto row = ui::hstack(6);
    row.Children().Append(ui::text(title, ui::Text::bodyStrong));
    if (!help.empty()) {
        auto info = ui::icon(L"", 12);
        info.Foreground(ui::resource(L"TextFillColorSecondaryBrush"));
        info.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::tooltip(info, help);
        row.Children().Append(info);
    }
    return row;
}

/// Aspect ratio button drawn with its real proportions.
muxc::Button aspectButton(std::string const& option, bool selected, std::function<void()> onClick) {
    muxc::Button b;
    b.Width(76);
    b.Height(76);
    b.Padding(ui::uniform(4));
    b.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(10));
    b.Background(selected ? ui::softGradient() : ui::resource(L"CardBackgroundFillColorDefaultBrush"));
    b.BorderBrush(selected ? ui::pinkBrush() : ui::resource(L"CardStrokeColorDefaultBrush"));
    b.BorderThickness(ui::uniform(selected ? 1.5 : 1));
    auto stack = ui::vstack(6);
    stack.HorizontalAlignment(mux::HorizontalAlignment::Center);
    double ratio = 0;
    auto colon = option.find(':');
    if (colon != std::string::npos) {
        try {
            ratio = std::stod(option.substr(0, colon)) / std::stod(option.substr(colon + 1));
        } catch (...) {
        }
    }
    muxc::Grid frame;
    frame.Width(34);
    frame.Height(34);
    if (ratio > 0) {
        shapes::Rectangle shape;
        shape.Width(ratio >= 1 ? 30 : std::max(6.0, 30 * ratio));
        shape.Height(ratio >= 1 ? std::max(6.0, 30 / ratio) : 30);
        shape.RadiusX(3);
        shape.RadiusY(3);
        shape.StrokeThickness(2);
        shape.Stroke(selected ? ui::brandGradient() : ui::resource(L"TextFillColorSecondaryBrush"));
        shape.HorizontalAlignment(mux::HorizontalAlignment::Center);
        shape.VerticalAlignment(mux::VerticalAlignment::Center);
        frame.Children().Append(shape);
    } else {
        auto symbol = ui::icon(L"", 18);
        symbol.Foreground(selected ? ui::pinkBrush() : ui::resource(L"TextFillColorSecondaryBrush"));
        frame.Children().Append(symbol);
    }
    stack.Children().Append(frame);
    std::string title = option == "auto" ? "Auto" : option == "adaptive" ? "Adaptativo" : option;
    auto label = ui::text(title, ui::Text::caption);
    label.HorizontalAlignment(mux::HorizontalAlignment::Center);
    stack.Children().Append(label);
    b.Content(stack);
    std::string hint = option == "9:16" ? "Vertical: Reels, TikTok, Shorts, historias"
                       : option == "16:9" ? "Horizontal: YouTube, pantallas"
                       : option == "1:1" ? "Cuadrado: feed"
                       : option == "4:5" ? "Vertical de feed de Instagram"
                       : (option == "auto" || option == "adaptive") ? "El modelo elige el formato"
                                                                      : "Formato " + option;
    ui::tooltip(b, hint);
    ui::accessible(b, "Formato " + title + (selected ? " (elegido)" : ""));
    b.Click([onClick](auto&&, auto&&) { onClick(); });
    return b;
}

mux::UIElement wrap(std::vector<mux::UIElement> const& items, double itemWidth, double itemHeight) {
    muxc::VariableSizedWrapGrid grid;
    grid.Orientation(muxc::Orientation::Horizontal);
    grid.ItemWidth(itemWidth);
    grid.ItemHeight(itemHeight);
    for (auto const& item : items) {
        item.as<mux::FrameworkElement>().Margin(ui::margin(0, 0, 8, 8));
        grid.Children().Append(item);
    }
    return grid;
}

// MARK: - Assistant

class AssistantPane {
public:
    AssistantPane() {
        scroll_ = muxc::ScrollViewer();
        scroll_.Padding(ui::margin(18, 16, 18, 24));
        auto panel = ui::vstack(16);
        scroll_.Content(panel);

        auto header = ui::vstack(4);
        auto titleRow = ui::hstack(8);
        titleRow.Children().Append(ui::text("Asistente de prompts", ui::Text::subtitle));
        titleRow.Children().Append(ui::pill("IA"));
        header.Children().Append(titleRow);
        header.Children().Append(ui::secondary("Contale tu idea con tus palabras y la convierte en un prompt listo, siguiendo la skill que elijas.", ui::Text::body));
        panel.Children().Append(header);

        skills_ = ui::vstack(8);
        panel.Children().Append(skills_);

        auto ideaSection = ui::vstack(8);
        ideaSection.Children().Append(fieldTitle("¿Qué tenés en mente?"));
        idea_ = ui::editor("Una chica muestra su sérum nuevo en el baño y cuenta, medio dormida, por qué le cambió la piel…", 110,
                           [this](std::string const& text) {
                               if (updating_) return;
                               AppModel::shared().idea = text;
                               refreshButton();
                           });
        ideaSection.Children().Append(idea_);
        examples_ = ui::hstack(6);
        muxc::ScrollViewer exampleScroll;
        exampleScroll.HorizontalScrollBarVisibility(muxc::ScrollBarVisibility::Hidden);
        exampleScroll.HorizontalScrollMode(muxc::ScrollMode::Enabled);
        exampleScroll.VerticalScrollMode(muxc::ScrollMode::Disabled);
        exampleScroll.Content(examples_);
        ideaSection.Children().Append(exampleScroll);
        referenceInfo_ = ui::vstack(2);
        ideaSection.Children().Append(referenceInfo_);
        panel.Children().Append(ideaSection);

        videoOptions_ = ui::vstack(6);
        panel.Children().Append(videoOptions_);

        engine_ = ui::vstack(8);
        panel.Children().Append(engine_);

        build_ = ui::primaryButton("Crear prompt", L"", [] { AppModel::shared().buildPrompt(false); });
        build_.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
        ui::tooltip(build_, "Crear prompt (Ctrl+Shift+Enter)");
        panel.Children().Append(build_);

        error_ = ui::text("", ui::Text::body);
        error_.Foreground(ui::resource(L"SystemFillColorCautionBrush"));
        panel.Children().Append(error_);

        live_ = ui::vstack(6);
        panel.Children().Append(live_);

        result_ = ui::vstack(10);
        draft_ = ui::editor("", 220, [this](std::string const& text) {
            if (!updating_) AppModel::shared().draft = text;
        });
        feedback_ = ui::field("Ej.: más corto, que hable más rápido, cambiá la locación a una cocina…", [this](std::string const& text) {
            if (!updating_) AppModel::shared().feedback = text;
        });
        feedback_.KeyDown([](IInspectable const&, mux::Input::KeyRoutedEventArgs const& args) {
            if (args.Key() == winrt::Windows::System::VirtualKey::Enter) {
                args.Handled(true);
                AppModel::shared().buildPrompt(true);
            }
        });
        panel.Children().Append(result_);
    }

    mux::UIElement root() { return scroll_; }

    void refresh(Change change) {
        auto& model = AppModel::shared();
        updating_ = true;
        if (change == Change::all || change == Change::assistant || change == Change::form || change == Change::skills) {
            refreshSkills();
            if (str(idea_.Text()) != model.idea) idea_.Text(hs(model.idea));
            refreshExamples();
            refreshReferenceInfo();
            refreshVideoOptions();
            refreshEngine();
            refreshButton();
            refreshResult();
        }
        if (change == Change::codex) refreshEngine();
        if (change == Change::account) refreshEngine();
        updating_ = false;
    }

private:
    void refreshSkills() {
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> items;
        items.push_back(fieldTitle("Skill (método)", "Una skill es un método escrito que el asistente aplica siempre igual."));
        items.push_back(ui::choiceCard("General",
                                       model.mode() == fc::MediaKind::image ? "Prompt claro en lenguaje natural." : "Prompt de video claro, sin método específico.",
                                       skillGlyph(""), model.skillID == fc::skills::kGeneralID, [] {
                                           auto& m = AppModel::shared();
                                           m.skillID = fc::skills::kGeneralID;
                                           m.notify(Change::assistant);
                                       }));
        for (auto const& skill : model.skillsFor(model.mode())) {
            std::string badge = skill.id == fc::skills::kUgcID ? "Recomendada" : skill.isBuiltin ? "" : "Tuya";
            items.push_back(ui::choiceCard(skill.name, skill.summary, skillGlyph(skill.id), model.skillID == skill.id,
                                           [id = skill.id] {
                                               auto& m = AppModel::shared();
                                               m.skillID = id;
                                               m.notify(Change::assistant);
                                           },
                                           badge));
        }
        if (model.isUGCSkill()) {
            auto walter = ui::toggle("", model.includeWalterExample, [](bool on) { AppModel::shared().includeWalterExample = on; });
            walter.OnContent(box_value(L"Usar el pack de Walter como ejemplo"));
            walter.OffContent(box_value(L"Usar el pack de Walter como ejemplo"));
            ui::tooltip(walter, "8 escenas reales que funcionaron. Más precisión en tono y estructura, pero consume más tokens.");
            items.push_back(walter);
        }
        if (model.skillID == fc::skills::kArthasID) {
            items.push_back(ui::secondary("Pensada para Gemini Omni / Veo 3.1: copiá el prompt a esa herramienta."));
        }
        items.push_back(ui::link("Ver, importar o crear skills", [] { AppModel::shared().go(Section::skills); }));
        ui::setChildren(skills_, items);
    }

    void refreshExamples() {
        auto& model = AppModel::shared();
        std::vector<std::string> list = model.mode() == fc::MediaKind::image
            ? std::vector<std::string>{"Selfie tomando café en un día de lluvia", "Sérum sobre mármol con luz de ventana", "POV abriendo un paquete en el sillón", "Unboxing de zapatillas en la cama"}
            : std::vector<std::string>{"Chica recomienda su sérum frente al espejo", "Mozo de 82 años cuenta su historia en un diner", "Pareja prueba una hamburguesa nueva en el auto", "Review honesta de auriculares caminando por la calle"};
        std::vector<mux::UIElement> chips;
        for (auto const& example : list) {
            auto chip = ui::button(example, L"", [this, example] {
                AppModel::shared().idea = example;
                idea_.Text(hs(example));
            });
            chip.FontSize(12);
            chip.Padding(ui::margin(10, 4, 10, 5));
            ui::tooltip(chip, "Usar esta idea de ejemplo");
            chips.push_back(chip);
        }
        ui::setChildren(examples_, chips);
    }

    void refreshReferenceInfo() {
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> items;
        auto tags = model.referenceTags();
        if (model.mode() == fc::MediaKind::video && !tags.empty()) {
            items.push_back(ui::text("Referencias que va a usar:", ui::Text::caption));
            for (auto const& tag : tags) {
                auto line = ui::secondary(tag);
                line.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
                items.push_back(line);
            }
        } else if (model.mode() == fc::MediaKind::image && !model.selectedImages.empty()) {
            items.push_back(ui::secondary("Va a analizar tus " + std::to_string(std::min<size_t>(model.selectedImages.size(), 4)) + " imagen(es) de referencia."));
        }
        ui::setChildren(referenceInfo_, items);
    }

    void refreshVideoOptions() {
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> items;
        if (model.mode() == fc::MediaKind::video) {
            items.push_back(fieldTitle("Idioma del diálogo", "El prompt se escribe en el idioma en que se habla en el video (regla del kit)."));
            int selected = 0;
            for (size_t i = 0; i < kLanguages.size(); ++i)
                if (kLanguages[i] == model.preferences.dialogueLanguage) selected = static_cast<int>(i);
            items.push_back(ui::combo(kLanguages, selected, [](int index) {
                AppModel::shared().preferences.dialogueLanguage = kLanguages[index];
                AppModel::shared().persist();
            }));
            items.push_back(ui::secondary(std::to_string(model.preferences.videoDuration) + " s de clip → ~" +
                                          std::to_string(fc::dialogue::targetWords(model.preferences.videoDuration)) + " palabras de diálogo"));
        }
        ui::setChildren(videoOptions_, items);
    }

    void refreshEngine() {
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> items;
        items.push_back(ui::text("Motor: " + model.engineName(), ui::Text::bodyStrong));
        int provider = static_cast<int>(model.preferences.provider);
        items.push_back(ui::segmented({"KIE", "ChatGPT", "OpenAI"}, provider, [](int index) {
            auto& m = AppModel::shared();
            m.preferences.provider = static_cast<fc::PromptProvider>(index);
            m.persist();
            if (m.preferences.provider == fc::PromptProvider::codex && m.codexStatus.state == codex::State::unknown) m.refreshCodexStatus();
            m.notify(Change::assistant);
        }));
        items.push_back(ui::secondary(fc::providerSummary(model.preferences.provider)));
        switch (model.preferences.provider) {
        case fc::PromptProvider::kie: {
            std::vector<std::string> names;
            int selected = 0;
            auto const& models = fc::prompts::promptModels();
            for (size_t i = 0; i < models.size(); ++i) {
                names.push_back(models[i].name + " · " + models[i].note);
                if (models[i].id == model.preferences.promptModel) selected = static_cast<int>(i);
            }
            auto picker = ui::combo(names, selected, [](int index) {
                auto& m = AppModel::shared();
                m.preferences.promptModel = fc::prompts::promptModels()[index].id;
                m.persist();
            });
            picker.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
            items.push_back(picker);
            auto test = ui::hstack(10);
            auto testButton = ui::button(model.isTestingConnection ? "Probando…" : "Probar conexión", L"",
                                         [] { AppModel::shared().testPromptModel(); });
            testButton.IsEnabled(!model.isTestingConnection);
            test.Children().Append(testButton);
            items.push_back(test);
            if (model.connectionTest) {
                auto result = ui::text(*model.connectionTest, ui::Text::caption);
                result.Foreground(fc::startsWith(*model.connectionTest, "✓") ? ui::resource(L"SystemFillColorSuccessBrush")
                                                                                : ui::resource(L"SystemFillColorCautionBrush"));
                items.push_back(result);
            }
            break;
        }
        case fc::PromptProvider::codex: {
            auto const& status = model.codexStatus;
            switch (status.state) {
            case codex::State::unknown:
            case codex::State::checking: {
                auto row = ui::hstack(8);
                muxc::ProgressRing ring;
                ring.Width(16);
                ring.Height(16);
                row.Children().Append(ring);
                row.Children().Append(ui::secondary("Comprobando tu sesión de ChatGPT…"));
                items.push_back(row);
                if (status.state == codex::State::unknown) model.refreshCodexStatus();
                break;
            }
            case codex::State::notInstalled: {
                auto warning = ui::text("No se encontró Codex en esta copia de la app", ui::Text::bodyStrong);
                warning.Foreground(ui::resource(L"SystemFillColorCautionBrush"));
                items.push_back(warning);
                items.push_back(ui::secondary("Descargá la versión más reciente de Framecraft (ya lo incluye) o instalalo desde la Terminal:"));
                for (auto const& command : {codex::kWingetCommand, codex::kNpmCommand}) {
                    auto row = ui::columns({ui::star(), ui::autoLength()});
                    auto text = ui::text(command, ui::Text::caption);
                    text.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
                    text.IsTextSelectionEnabled(true);
                    ui::place(row, text, 0);
                    ui::place(row, ui::subtleButton("Copiar", L"", [command] { AppModel::shared().copy(command); }), 1);
                    items.push_back(row);
                }
                items.push_back(ui::button("Comprobar de nuevo", L"", [] { AppModel::shared().refreshCodexStatus(); }));
                break;
            }
            case codex::State::loggedOut: {
                items.push_back(ui::secondary("Usá tu propia cuenta de ChatGPT (Plus, Pro, Team…). No gasta créditos de KIE."));
                auto login = ui::primaryButton("Iniciar sesión con ChatGPT", L"", [] { AppModel::shared().codexLogin(); });
                login.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
                items.push_back(login);
                break;
            }
            case codex::State::loggingIn: {
                auto row = ui::hstack(8);
                muxc::ProgressRing ring;
                ring.Width(16);
                ring.Height(16);
                row.Children().Append(ring);
                row.Children().Append(ui::secondary("Terminá de iniciar sesión en el navegador que se abrió…"));
                items.push_back(row);
                break;
            }
            case codex::State::loggedIn: {
                auto ok = ui::text("✓ " + status.detail, ui::Text::caption);
                ok.Foreground(ui::resource(L"SystemFillColorSuccessBrush"));
                items.push_back(ok);
                auto row = ui::columns({ui::star(), ui::autoLength()});
                ui::place(row, ui::secondary("Usa tu plan de ChatGPT. Puede tardar un poco más."), 0);
                ui::place(row, ui::button("Cerrar sesión", L"", [] { AppModel::shared().codexLogout(); }), 1);
                items.push_back(row);
                break;
            }
            }
            break;
        }
        case fc::PromptProvider::openai:
            if (!model.hasOpenAIKey) items.push_back(ui::link("Agregar clave de OpenAI en Ajustes", [] { AppModel::shared().go(Section::settings); }));
            items.push_back(ui::secondary("GPT-4.1 mini con tu clave de API de OpenAI."));
            break;
        }
        ui::setChildren(engine_, items);
    }

    void refreshButton() {
        auto& model = AppModel::shared();
        if (model.isBuildingPrompt) {
            auto row = ui::hstack(8);
            muxc::ProgressRing ring;
            ring.Width(16);
            ring.Height(16);
            ring.Foreground(muxm::SolidColorBrush(ui::rgb(255, 255, 255)));
            row.Children().Append(ring);
            row.Children().Append(ui::text("Escribiendo tu prompt…"));
            build_.Content(row);
        } else {
            ui::setLabel(build_, L"", model.draft.empty() ? "Crear prompt" : "Crear otro");
        }
        build_.IsEnabled(!model.isBuildingPrompt && !fc::trim(model.idea).empty());
        error_.Text(hs(model.assistantError.value_or("")));
        ui::show(error_, model.assistantError.has_value());

        std::vector<mux::UIElement> live;
        if (model.isBuildingPrompt && model.streamingText) {
            auto label = ui::text(model.streamingText->empty() ? "Pensando…" : "Escribiendo…", ui::Text::caption);
            live.push_back(label);
            if (!model.streamingText->empty()) {
                std::string tail = *model.streamingText;
                if (tail.size() > 1500) tail = "…" + tail.substr(tail.size() - 1500);
                auto text = ui::secondary(tail);
                text.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
                live.push_back(ui::card(text, 10));
            }
        }
        ui::setChildren(live_, live);
    }

    void refreshResult() {
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> items;
        if (!model.draft.empty() && !model.isBuildingPrompt) {
            auto header = ui::columns({ui::star(), ui::autoLength(), ui::autoLength()}, 8);
            ui::place(header, ui::text("Tu prompt", ui::Text::subtitle), 0);
            ui::place(header, ui::secondary(std::to_string(fc::characterCount(model.draft)) + " caracteres"), 1);
            if (model.draftHistory.size() > 1) {
                muxc::DropDownButton versions;
                versions.Content(ui::labeled(L"", "Versiones"));
                muxc::MenuFlyout flyout;
                for (size_t i = model.draftHistory.size(); i-- > 0;) {
                    muxc::MenuFlyoutItem item;
                    item.Text(hs("Versión " + std::to_string(i + 1) + ": " + fc::prefixCharacters(model.draftHistory[i], 50) + "…"));
                    item.Click([i](auto&&, auto&&) { AppModel::shared().restoreDraft(i); });
                    flyout.Items().Append(item);
                }
                versions.Flyout(flyout);
                ui::tooltip(versions, "Volver a una versión anterior del prompt");
                ui::place(header, versions, 2);
            }
            items.push_back(header);
            if (auto main = model.draftProfilePrompt()) {
                auto box = ui::vstack(6);
                box.Children().Append(ui::text("Prompt principal", ui::Text::caption));
                auto text = ui::text(*main);
                text.IsTextSelectionEnabled(true);
                box.Children().Append(text);
                auto use = ui::primaryButton("Usar texto simple", L"", [] { AppModel::shared().useDraftText(); });
                use.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
                box.Children().Append(use);
                muxc::Border frame;
                frame.Background(ui::softGradient());
                frame.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(10));
                frame.Padding(ui::uniform(12));
                frame.Child(box);
                items.push_back(frame);
                muxc::Expander full;
                full.Header(box_value(L"Perfil JSON completo (avanzado)"));
                full.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
                full.HorizontalContentAlignment(mux::HorizontalAlignment::Stretch);
                draft_.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
                detach(draft_);
                full.Content(draft_);
                items.push_back(full);
            } else {
                draft_.FontFamily(fc::startsWith(model.draft, "{") ? muxm::FontFamily(L"Cascadia Mono, Consolas") : muxm::FontFamily(L"Segoe UI Variable Text"));
                detach(draft_);
                items.push_back(draft_);
            }
            if (str(draft_.Text()) != model.draft) draft_.Text(hs(model.draft));
            if (model.notes) {
                auto box = ui::vstack(4);
                box.Children().Append(ui::labeled(L"", "Notas del asistente"));
                box.Children().Append(ui::text(*model.notes, ui::Text::caption));
                muxc::Border frame;
                frame.Background(ui::softGradient());
                frame.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(10));
                frame.Padding(ui::uniform(12));
                frame.Child(box);
                items.push_back(frame);
            }
            if (!model.sources.empty()) {
                auto list = ui::vstack(2);
                for (auto const& example : model.sources) list.Children().Append(ui::secondary("• " + example.title + " — " + example.author));
                list.Children().Append(ui::secondary("Ejemplos de YouMind OpenLab · CC BY 4.0"));
                muxc::Expander sources;
                sources.Header(box_value(L"Ejemplos que lo inspiraron"));
                sources.Content(list);
                items.push_back(sources);
            }
            auto actions = ui::columns({ui::star(), ui::autoLength()}, 8);
            auto use = ui::primaryButton(model.draftProfilePrompt() ? "Usar perfil JSON completo" : "Usar este prompt", L"",
                                         [] { AppModel::shared().useDraft(); });
            use.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
            ui::place(actions, use, 0);
            auto copy = ui::button("", L"", [] { AppModel::shared().copy(AppModel::shared().draft); });
            ui::tooltip(copy, "Copiar el prompt");
            ui::accessible(copy, "Copiar el prompt");
            ui::place(actions, copy, 1);
            items.push_back(actions);
            items.push_back(fieldTitle("¿Querés ajustar algo?"));
            auto refine = ui::columns({ui::star(), ui::autoLength()}, 8);
            detach(feedback_);
            if (str(feedback_.Text()) != model.feedback) feedback_.Text(hs(model.feedback));
            ui::place(refine, feedback_, 0);
            auto adjust = ui::button("Ajustar", L"", [] { AppModel::shared().buildPrompt(true); });
            adjust.IsEnabled(!model.isBuildingPrompt);
            ui::place(refine, adjust, 1);
            items.push_back(refine);
        }
        ui::setChildren(result_, items);
    }

    /// Removes a persistent control from its current parent before re-adding it.
    static void detach(mux::FrameworkElement const& element) {
        auto parent = element.Parent();
        if (!parent) return;
        if (auto panel = parent.try_as<muxc::Panel>()) {
            uint32_t index = 0;
            if (panel.Children().IndexOf(element, index)) panel.Children().RemoveAt(index);
        } else if (auto expander = parent.try_as<muxc::Expander>()) {
            expander.Content(nullptr);
        } else if (auto border = parent.try_as<muxc::Border>()) {
            border.Child(nullptr);
        } else if (auto content = parent.try_as<muxc::ContentControl>()) {
            content.Content(nullptr);
        }
    }

    muxc::ScrollViewer scroll_{nullptr};
    muxc::StackPanel skills_{nullptr}, examples_{nullptr}, referenceInfo_{nullptr}, videoOptions_{nullptr}, engine_{nullptr}, live_{nullptr},
        result_{nullptr};
    muxc::TextBox idea_{nullptr}, draft_{nullptr}, feedback_{nullptr};
    muxc::Button build_{nullptr};
    muxc::TextBlock error_{nullptr};
    bool updating_ = false;
};

// MARK: - Create

class CreatePage : public Page {
public:
    CreatePage() {
        root_ = ui::columns({ui::star(), ui::autoLength()}, 0);

        muxc::Grid left;
        scroll_ = muxc::ScrollViewer();
        auto content = ui::vstack(16);
        content.MaxWidth(960);
        content.Padding(ui::margin(32, 24, 32, 130));
        content.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
        scroll_.Content(content);
        left.Children().Append(scroll_);

        // Header.
        auto header = ui::columns({ui::star(), ui::autoLength()}, 8);
        auto titles = ui::vstack(4);
        titles.Children().Append(ui::gradientTitle("Crear", 32));
        titles.Children().Append(ui::secondary("Describí tu idea. Los detalles opcionales quedan a mano cuando los necesitás.", ui::Text::body));
        ui::place(header, titles, 0);
        assistantToggle_ = ui::button("Asistente", L"", [] {
            auto& m = AppModel::shared();
            m.showAssistant = !m.showAssistant;
            m.notify(Change::assistant);
        });
        assistantToggle_.VerticalAlignment(mux::VerticalAlignment::Top);
        ui::tooltip(assistantToggle_, "Mostrar u ocultar el asistente (Ctrl+Alt+I)");
        ui::place(header, assistantToggle_, 1);
        content.Children().Append(header);

        templates_ = ui::vstack(10);
        content.Children().Append(templates_);
        modes_ = ui::columns({ui::autoLength(), ui::star()}, 14);
        content.Children().Append(modes_);

        // Step 1: prompt.
        auto step1 = ui::vstack(12);
        step1Header_ = ui::vstack(0);
        step1.Children().Append(step1Header_);
        prompt_ = ui::editor("", 170, [this](std::string const& text) {
            if (updating_) return;
            AppModel::shared().prompt = text;
            refreshChecker();
            refreshBar();
            refreshCounter();
            refreshStep1Header();
            refreshProgress();
        });
        step1.Children().Append(prompt_);
        tags_ = ui::hstack(6);
        step1.Children().Append(tags_);
        auto tools = ui::columns({ui::autoLength(), ui::autoLength(), ui::autoLength(), ui::star(), ui::autoLength()}, 8);
        ui::place(tools, ui::subtleButton("Pedir ayuda al asistente", L"", [] {
            auto& m = AppModel::shared();
            m.showAssistant = true;
            m.notify(Change::assistant);
        }), 0);
        ui::place(tools, ui::subtleButton("Vista previa", L"", [] {
            if (AppModel::shared().openPromptPreview) AppModel::shared().openPromptPreview();
        }), 1);
        ui::place(tools, ui::subtleButton("Borrar", L"", [this] {
            AppModel::shared().prompt.clear();
            prompt_.Text(L"");
        }), 2);
        counter_ = ui::secondary("");
        counter_.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::place(tools, counter_, 4);
        step1.Children().Append(tools);
        step1Card_ = ui::card(step1, 20);
        content.Children().Append(step1Card_);

        checker_ = ui::vstack(10);
        checkerCard_ = ui::card(checker_, 18);
        content.Children().Append(checkerCard_);

        references_ = ui::vstack(12);
        references_.Padding(ui::margin(2, 8, 2, 4));
        references_.Visibility(mux::Visibility::Collapsed);
        referencesSummary_ = ui::secondary("Opcional");
        auto referencesHeader = ui::vstack(1);
        referencesHeader.Children().Append(ui::text("Referencias", ui::Text::bodyStrong));
        referencesHeader.Children().Append(referencesSummary_);
        auto referencesHeaderGrid = ui::columns({ui::star(), ui::autoLength()});
        ui::place(referencesHeaderGrid, referencesHeader, 0);
        referencesChevron_ = ui::icon(L"\uE70D", 12);
        referencesChevron_.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::place(referencesHeaderGrid, referencesChevron_, 1);
        referencesToggle_ = ui::subtleButton("", L"", {});
        referencesToggle_.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
        referencesToggle_.HorizontalContentAlignment(mux::HorizontalAlignment::Stretch);
        referencesToggle_.Content(referencesHeaderGrid);
        referencesToggle_.Click([this](auto&&, auto&&) {
            referencesOpen_ = !referencesOpen_;
            references_.Visibility(referencesOpen_ ? mux::Visibility::Visible : mux::Visibility::Collapsed);
            referencesChevron_.Glyph(referencesOpen_ ? L"\uE70E" : L"\uE70D");
        });
        auto referencesSection = ui::vstack(0);
        referencesSection.Children().Append(referencesToggle_);
        referencesSection.Children().Append(references_);
        referencesCard_ = referencesSection;
        content.Children().Append(referencesSection);

        settings_ = ui::vstack(14);
        settings_.Padding(ui::margin(2, 8, 2, 4));
        settings_.Visibility(mux::Visibility::Collapsed);
        settingsSummary_ = ui::secondary("");
        auto settingsHeader = ui::vstack(1);
        settingsHeader.Children().Append(ui::text("Ajustes avanzados", ui::Text::bodyStrong));
        settingsHeader.Children().Append(settingsSummary_);
        auto settingsHeaderGrid = ui::columns({ui::star(), ui::autoLength()});
        ui::place(settingsHeaderGrid, settingsHeader, 0);
        settingsChevron_ = ui::icon(L"\uE70D", 12);
        settingsChevron_.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::place(settingsHeaderGrid, settingsChevron_, 1);
        settingsToggle_ = ui::subtleButton("", L"", {});
        settingsToggle_.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
        settingsToggle_.HorizontalContentAlignment(mux::HorizontalAlignment::Stretch);
        settingsToggle_.Content(settingsHeaderGrid);
        settingsToggle_.Click([this](auto&&, auto&&) {
            settingsOpen_ = !settingsOpen_;
            settings_.Visibility(settingsOpen_ ? mux::Visibility::Visible : mux::Visibility::Collapsed);
            settingsChevron_.Glyph(settingsOpen_ ? L"\uE70E" : L"\uE70D");
        });
        auto settingsSection = ui::vstack(0);
        settingsSection.Children().Append(settingsToggle_);
        settingsSection.Children().Append(settings_);
        settingsCard_ = settingsSection;
        content.Children().Append(settingsSection);

        // Floating generate bar.
        muxc::Border bar;
        bar.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(24));
        bar.Background(ui::resource(L"AcrylicInAppFillColorDefaultBrush"));
        bar.BorderBrush(ui::resource(L"CardStrokeColorDefaultBrush"));
        bar.BorderThickness(ui::uniform(1));
        bar.Padding(ui::margin(22, 10, 10, 10));
        bar.Margin(ui::margin(24, 0, 24, 18));
        bar.MaxWidth(900);
        bar.VerticalAlignment(mux::VerticalAlignment::Bottom);
        muxm::ThemeShadow shadow;
        bar.Shadow(shadow);
        bar.Translation({0, 0, 24});
        auto barGrid = ui::columns({ui::star(), ui::autoLength(), ui::autoLength()}, 12);
        auto summary = ui::vstack(2);
        summary_ = ui::text("", ui::Text::bodyStrong);
        summary_.TextWrapping(mux::TextWrapping::NoWrap);
        summary_.TextTrimming(mux::TextTrimming::CharacterEllipsis);
        hint_ = ui::secondary("");
        hint_.TextWrapping(mux::TextWrapping::NoWrap);
        hint_.TextTrimming(mux::TextTrimming::CharacterEllipsis);
        summary.Children().Append(summary_);
        summary.Children().Append(hint_);
        summary.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::place(barGrid, summary, 0);
        active_ = ui::button("", L"", [] {
            auto& m = AppModel::shared();
            m.filter = LibraryFilter::active;
            m.go(Section::library);
        });
        active_.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::place(barGrid, active_, 1);
        generate_ = ui::primaryButton("Generar", L"", [] { AppModel::shared().generate(); });
        generate_.MinWidth(210);
        generate_.MinHeight(46);
        ui::place(barGrid, generate_, 2);
        bar.Child(barGrid);
        left.Children().Append(bar);

        // Drag & drop from Explorer anywhere on the page.
        left.AllowDrop(true);
        left.DragOver([](IInspectable const&, mux::DragEventArgs const& args) {
            if (args.DataView().Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems())) {
                args.AcceptedOperation(winrt::Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
                args.DragUIOverride().Caption(L"Soltá para agregar como referencia");
            }
        });
        left.Drop([](IInspectable const&, mux::DragEventArgs args) -> winrt::fire_and_forget {
            if (!args.DataView().Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems())) co_return;
            auto deferral = args.GetDeferral();
            auto items = co_await args.DataView().GetStorageItemsAsync();
            deferral.Complete();
            std::vector<fs::path> files;
            for (auto const& item : items) files.push_back(fs::path(std::wstring(item.Path())));
            AppModel::shared().importFiles(files);
        });

        ui::place(root_, left, 0);
        assistantHost_ = muxc::Border();
        assistantHost_.Width(410);
        assistantHost_.BorderBrush(ui::resource(L"CardStrokeColorDefaultBrush"));
        assistantHost_.BorderThickness(ui::margin(1, 0, 0, 0));
        assistantHost_.Background(ui::resource(L"LayerFillColorDefaultBrush"));
        assistantHost_.Child(assistant_.root());
        ui::place(root_, assistantHost_, 1);
        // Narrow windows: the assistant floats over the content instead of squeezing it.
        root_.SizeChanged([this](IInspectable const&, mux::SizeChangedEventArgs const& args) { layoutAssistant(args.NewSize().Width); });
    }

    mux::UIElement root() override { return root_; }

    void refresh(Change change) override {
        updating_ = true;
        auto& model = AppModel::shared();
        bool all = change == Change::all;
        if (all || change == Change::form || change == Change::assistant) {
            refreshTemplates();
            refreshModes();
            refreshStep1();
            if (str(prompt_.Text()) != model.prompt) prompt_.Text(hs(model.prompt));
            refreshTags();
            refreshCounter();
            refreshChecker();
            refreshSettings();
        }
        if (all || change == Change::form || change == Change::references) {
            refreshReferences();
            refreshProgress();
        }
        if (all || change == Change::form || change == Change::jobs || change == Change::account) refreshBar();
        ui::show(assistantHost_, model.showAssistant);
        assistant_.refresh(change);
        updating_ = false;
    }

private:
    void layoutAssistant(double width) {
        bool narrow = width < 1120;
        if (narrow == narrow_) return;
        narrow_ = narrow;
        muxc::Grid::SetColumn(assistantHost_, narrow ? 0 : 1);
        assistantHost_.HorizontalAlignment(narrow ? mux::HorizontalAlignment::Right : mux::HorizontalAlignment::Stretch);
        assistantHost_.Width(narrow ? std::min(400.0, width - 40) : 410);
        assistantHost_.Background(narrow ? ui::resource(L"AcrylicInAppFillColorDefaultBrush") : ui::resource(L"LayerFillColorDefaultBrush"));
        if (narrow) {
            assistantHost_.Shadow(muxm::ThemeShadow());
            assistantHost_.Translation({0, 0, 32});
        } else {
            assistantHost_.Shadow(nullptr);
            assistantHost_.Translation({0, 0, 0});
        }
    }

    void refreshTemplates() {
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> items;
        auto row = ui::hstack(10);
        auto button = ui::button("Usar una plantilla", L"\uE8F1", {});
        muxc::MenuFlyout menu;
        std::vector<fc::CreativeTemplate> sorted;
        for (auto const& t : fc::templates::creative())
            if (t.media == model.mode()) sorted.push_back(t);
        for (auto const& t : fc::templates::creative())
            if (t.media != model.mode()) sorted.push_back(t);
        bool switchedKind = false;
        for (auto const& t : sorted) {
            if (!switchedKind && t.media != model.mode()) {
                menu.Items().Append(muxc::MenuFlyoutSeparator());
                switchedKind = true;
            }
            muxc::MenuFlyoutItem option;
            option.Text(hs(t.title + " · " + (t.media == fc::MediaKind::video ? "video" : "imagen")));
            option.Click([t](auto&&, auto&&) { AppModel::shared().applyTemplate(t); });
            menu.Items().Append(option);
        }
        button.Flyout(menu);
        row.Children().Append(button);
        auto hint = ui::secondary("Punto de partida opcional");
        hint.VerticalAlignment(mux::VerticalAlignment::Center);
        row.Children().Append(hint);
        items.push_back(row);
        ui::setChildren(templates_, items);
    }

    void refreshModes() {
        auto& model = AppModel::shared();
        modes_.Children().Clear();
        auto label = ui::text("Tipo", ui::Text::bodyStrong);
        label.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::place(modes_, label, 0);
        int selected = model.mode() == fc::MediaKind::image ? 0 : 1;
        auto selector = ui::segmented({"Imagen", "Video"}, selected, [](int index) {
            AppModel::shared().setMode(index == 0 ? fc::MediaKind::image : fc::MediaKind::video);
        });
        ui::place(modes_, selector, 1);
    }

    void refreshStep1() {
        auto& model = AppModel::shared();
        bool video = model.mode() == fc::MediaKind::video;
        refreshStep1Header();
        prompt_.PlaceholderText(video ? L"Ej.: 0–5 s: una chica levanta el frasco frente al espejo del baño y dice \"lo uso hace un mes\"…"
                                      : L"Ej.: selfie espontánea de una chica tomando café junto a una ventana con lluvia, luz de tarde, piel real…");
        prompt_.FontFamily(video ? muxm::FontFamily(L"Cascadia Mono, Consolas") : muxm::FontFamily(L"Segoe UI Variable Text"));
    }

    void refreshTags() {
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> items;
        if (model.mode() == fc::MediaKind::video) {
            std::vector<std::string> tags;
            for (auto kind : fc::kAllReferenceKinds)
                for (size_t i = 0; i < model.selection(kind).size(); ++i) tags.push_back(std::string(fc::tagPrefix(kind)) + std::to_string(i + 1));
            if (!tags.empty()) {
                auto label = ui::secondary("Insertar:");
                label.VerticalAlignment(mux::VerticalAlignment::Center);
                items.push_back(label);
                for (auto const& tag : tags) {
                    auto b = ui::button(tag, L"", [this, tag] {
                        AppModel::shared().insertTag(tag);
                        prompt_.Text(hs(AppModel::shared().prompt));
                        prompt_.Select(prompt_.Text().size(), 0);
                        prompt_.Focus(mux::FocusState::Programmatic);
                    });
                    b.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
                    b.FontSize(12);
                    b.Padding(ui::margin(8, 2, 8, 3));
                    b.Background(ui::softGradient());
                    ui::tooltip(b, "Insertar " + tag + " en el prompt");
                    items.push_back(b);
                }
            }
        }
        ui::setChildren(tags_, items);
    }

    void refreshCounter() {
        auto& model = AppModel::shared();
        int limit = model.mode() == fc::MediaKind::image ? fc::presets::maxPromptLength(model.preferences.imageModel) : fc::presets::kMaxVideoPromptLength;
        counter_.Text(hs(formatNumber(static_cast<double>(fc::characterCount(model.prompt))) + " / " + formatNumber(limit)));
    }

    void refreshChecker() {
        auto& model = AppModel::shared();
        auto report = model.lintReport();
        ui::show(checkerCard_, report.has_value());
        if (!report) return;
        std::vector<mux::UIElement> items;
        auto header = ui::columns({ui::autoLength(), ui::star()}, 12);
        auto state = report->problems() > 0 ? fc::LintItem::State::problem : report->warnings() > 0 ? fc::LintItem::State::warning : fc::LintItem::State::ok;
        auto symbol = ui::icon(lintGlyph(state), 20);
        symbol.Foreground(lintBrush(state));
        ui::place(header, symbol, 0);
        auto titles = ui::vstack(0);
        titles.Children().Append(ui::text("Revisión del prompt", ui::Text::bodyStrong));
        std::string summary = std::to_string(report->passed()) + " en orden";
        if (report->warnings()) summary += " · " + std::to_string(report->warnings()) + (report->warnings() == 1 ? " sugerencia" : " sugerencias");
        if (report->problems()) summary += " · " + std::to_string(report->problems()) + (report->problems() == 1 ? " problema" : " problemas");
        titles.Children().Append(ui::secondary(summary));
        ui::place(header, titles, 1);
        items.push_back(header);

        // Dialogue meter: spoken words vs. what fits in the clip.
        auto meter = ui::vstack(4);
        auto meterHeader = ui::columns({ui::star(), ui::autoLength()});
        ui::place(meterHeader, ui::text("Diálogo", ui::Text::caption), 0);
        ui::place(meterHeader, ui::secondary(std::to_string(report->dialogueWords) + " de ~" + std::to_string(report->targetWords) +
                                             " palabras para " + std::to_string(model.preferences.videoDuration) + " s"),
                  1);
        meter.Children().Append(meterHeader);
        muxc::ProgressBar bar;
        bar.Maximum(std::max(report->targetWords, 1) * 1.4);
        bar.Value(report->dialogueWords);
        bar.Height(6);
        meter.Children().Append(bar);
        items.push_back(meter);

        for (auto const& item : report->items) {
            auto row = ui::columns({ui::autoLength(), ui::star()}, 10);
            auto glyph = ui::icon(lintGlyph(item.state), 14);
            glyph.Foreground(lintBrush(item.state));
            glyph.VerticalAlignment(mux::VerticalAlignment::Top);
            glyph.Margin(ui::margin(0, 2, 0, 0));
            ui::place(row, glyph, 0);
            auto texts = ui::vstack(1);
            texts.Children().Append(ui::text(item.title, ui::Text::bodyStrong));
            texts.Children().Append(ui::secondary(item.detail));
            ui::place(row, texts, 1);
            items.push_back(row);
        }
        ui::setChildren(checker_, items);
    }

    bool ideaReady() const {
        auto const& prompt = AppModel::shared().prompt;
        return prompt.find_first_not_of(" \t\r\n") != std::string::npos;
    }

    size_t selectedReferenceCount() const {
        auto const& m = AppModel::shared();
        return m.selectedImages.size() + m.selectedVideos.size() + m.selectedAudios.size();
    }

    void refreshStep1Header() {
        auto& model = AppModel::shared();
        bool video = model.mode() == fc::MediaKind::video;
        auto labels = ui::vstack(2);
        labels.Children().Append(ui::text("Tu idea", ui::Text::subtitle));
        labels.Children().Append(ui::secondary(video ? "Contá qué pasa, qué se dice y cómo se graba."
                                                     : "Contá qué se ve, dónde y con qué luz."));
        ui::setChildren(step1Header_, {labels});
    }

    enum class StepState { done, current, optional };

    mux::UIElement progressChip(int number, std::string const& title, std::string const& detail, StepState state,
                                mux::UIElement target) {
        auto row = ui::hstack(8);
        muxc::Border badge;
        badge.Width(20);
        badge.Height(20);
        badge.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(10));
        if (state == StepState::done) badge.Background(muxm::SolidColorBrush(ui::successColor()));
        else if (state == StepState::current) badge.Background(ui::brandGradient());
        else badge.Background(ui::resource(L"ControlFillColorSecondaryBrush"));
        muxc::TextBlock mark;
        mark.Text(state == StepState::done ? winrt::hstring(L"\u2713") : to_hstring(number));
        mark.FontSize(11);
        mark.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
        if (state != StepState::optional) mark.Foreground(muxm::SolidColorBrush(ui::rgb(255, 255, 255)));
        mark.HorizontalAlignment(mux::HorizontalAlignment::Center);
        mark.VerticalAlignment(mux::VerticalAlignment::Center);
        badge.Child(mark);
        badge.VerticalAlignment(mux::VerticalAlignment::Center);
        row.Children().Append(badge);
        auto labels = ui::vstack(0);
        labels.Children().Append(ui::text(title, ui::Text::bodyStrong));
        auto sub = ui::secondary(detail);
        if (state == StepState::current) sub.Foreground(muxm::SolidColorBrush(ui::rgb(255, 61, 153)));
        labels.Children().Append(sub);
        row.Children().Append(labels);
        muxc::Button chip;
        chip.Content(row);
        chip.Padding(ui::margin(10, 6, 12, 6));
        chip.Background(state == StepState::current ? ui::resource(L"AccentFillColorTertiaryBrush") : ui::resource(L"SubtleFillColorTransparentBrush"));
        chip.BorderThickness(mux::ThicknessHelper::FromUniformLength(0));
        chip.Click([target](auto&&, auto&&) {
            if (target) target.StartBringIntoView();
        });
        ui::tooltip(chip, "Ir a " + title);
        ui::accessible(chip, "Paso " + std::to_string(number) + ", " + title + ": " + detail);
        return chip;
    }

    void refreshProgress() {
        if (!progress_) return;
        auto& model = AppModel::shared();
        bool video = model.mode() == fc::MediaKind::video;
        size_t refs = selectedReferenceCount();
        bool idea = ideaReady();
        auto arrow = [] {
            auto glyph = ui::secondary("\u203A", ui::Text::subtitle);
            glyph.VerticalAlignment(mux::VerticalAlignment::Center);
            return glyph;
        };
        std::vector<mux::UIElement> items;
        items.push_back(progressChip(1, video ? "Video" : "Imagen", "Tipo", StepState::done, modes_));
        items.push_back(arrow());
        items.push_back(progressChip(2, "Tu idea", idea ? std::to_string(model.prompt.size()) + " caracteres" : "Falta escribirla",
                                     idea ? StepState::done : StepState::current, step1Card_));
        items.push_back(arrow());
        items.push_back(progressChip(3, "Referencias", refs == 0 ? "Opcional" : std::to_string(refs) + (refs == 1 ? " elegida" : " elegidas"),
                                     refs > 0 ? StepState::done : StepState::optional, referencesCard_));
        items.push_back(arrow());
        items.push_back(progressChip(4, "Ajustes", model.settingsSummary(), StepState::done, settingsCard_));
        ui::setChildren(progress_, items);
    }

    void refreshReferences() {
        auto& model = AppModel::shared();
        bool video = model.mode() == fc::MediaKind::video;
        std::vector<mux::UIElement> items;
        auto header = ui::columns({ui::star(), ui::autoLength()});
        auto labels = ui::vstack(2);
        labels.Children().Append(ui::text("Archivos de referencia", ui::Text::bodyStrong));
        labels.Children().Append(ui::secondary(video ? "Fotos, videos o audios. El orden define @Image1, @Image2…"
                                                     : "Hasta 4 fotos para conservar producto, persona o estilo."));
        ui::place(header, labels, 0);
        auto add = ui::button(model.isImporting ? "Importando…" : "Agregar archivos", L"", [] { AppModel::shared().pickAndImport(); });
        add.IsEnabled(!model.isImporting);
        add.VerticalAlignment(mux::VerticalAlignment::Top);
        ui::place(header, add, 1);
        items.push_back(header);

        std::vector<fc::ReferenceKind> kinds = {fc::ReferenceKind::image};
        if (video) kinds = {fc::ReferenceKind::image, fc::ReferenceKind::video, fc::ReferenceKind::audio};
        bool any = false;
        for (auto kind : kinds) {
            auto available = model.referencesOf(kind);
            if (available.empty()) continue;
            any = true;
            std::string title = kind == fc::ReferenceKind::image ? "Imágenes" : kind == fc::ReferenceKind::video ? "Videos" : "Audios";
            std::string counter = std::to_string(model.selection(kind).size()) + " de " + std::to_string(model.selectionLimit(kind));
            if (kind != fc::ReferenceKind::image) counter += " · " + fc::media::formattedDuration(model.selectedSeconds(kind)) + " de 30 s";
            auto row = ui::columns({ui::star(), ui::autoLength()});
            ui::place(row, ui::text(title, ui::Text::bodyStrong), 0);
            ui::place(row, ui::secondary(counter), 1);
            items.push_back(row);
            auto strip = ui::hstack(10);
            // Selected first, in order; then the rest.
            std::vector<fc::ReferenceFile> ordered = model.selectedReferences(kind);
            for (auto const& r : available)
                if (!model.isSelected(r)) ordered.push_back(r);
            for (auto const& r : ordered) strip.Children().Append(referenceTile(r));
            muxc::ScrollViewer scroller;
            scroller.HorizontalScrollBarVisibility(muxc::ScrollBarVisibility::Auto);
            scroller.HorizontalScrollMode(muxc::ScrollMode::Enabled);
            scroller.VerticalScrollMode(muxc::ScrollMode::Disabled);
            scroller.VerticalScrollBarVisibility(muxc::ScrollBarVisibility::Disabled);
            scroller.Padding(ui::margin(0, 0, 0, 10));
            scroller.Content(strip);
            items.push_back(scroller);
        }
        if (!any) {
            muxc::Border drop;
            drop.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(12));
            drop.BorderBrush(ui::pinkBrush());
            drop.BorderThickness(ui::uniform(1));
            drop.Background(ui::softGradient());
            drop.Padding(ui::uniform(18));
            auto hint = ui::hstack(12);
            hint.Children().Append(ui::icon(L"", 22));
            auto labels = ui::vstack(2);
            labels.Children().Append(ui::text("Arrastrá tus archivos acá", ui::Text::bodyStrong));
            labels.Children().Append(ui::secondary(video ? "PNG, JPG, WebP, MP4, MOV, MP3, WAV o M4A" : "PNG, JPG o WebP"));
            hint.Children().Append(labels);
            drop.Child(hint);
            items.push_back(drop);
        }
        size_t count = selectedReferenceCount();
        referencesSummary_.Text(hs(count == 0 ? "Opcional · agregá fotos, videos o audio si los necesitás"
                                              : std::to_string(count) + (count == 1 ? " referencia elegida" : " referencias elegidas")));
        ui::setChildren(references_, items);
    }

    mux::UIElement referenceTile(fc::ReferenceFile const& r) {
        auto& model = AppModel::shared();
        bool selected = model.isSelected(r);
        auto tag = model.tag(r);
        muxc::Button tile;
        tile.Width(112);
        tile.Padding(ui::uniform(0));
        tile.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(12));
        tile.BorderBrush(selected ? ui::pinkBrush() : ui::resource(L"CardStrokeColorDefaultBrush"));
        tile.BorderThickness(ui::uniform(selected ? 2 : 1));
        tile.Background(ui::resource(L"CardBackgroundFillColorDefaultBrush"));
        tile.VerticalContentAlignment(mux::VerticalAlignment::Top);
        auto body = ui::vstack(4);
        muxc::Grid media;
        media.Height(92);
        if (r.kind == fc::ReferenceKind::audio) {
            media.Background(ui::softGradient());
            auto wave = ui::icon(L"", 28);
            wave.Foreground(ui::pinkBrush());
            media.Children().Append(wave);
        } else {
            media.Children().Append(thumbnail(model.referencePath(r), r.kind == fc::ReferenceKind::video, 260, 0, 92));
        }
        if (tag) {
            auto badge = ui::pill(*tag);
            badge.HorizontalAlignment(mux::HorizontalAlignment::Left);
            badge.VerticalAlignment(mux::VerticalAlignment::Top);
            badge.Margin(ui::uniform(6));
            media.Children().Append(badge);
        }
        if (r.durationMs) {
            auto duration = ui::pill(fc::media::formattedDuration(r.durationSeconds()), false);
            duration.HorizontalAlignment(mux::HorizontalAlignment::Right);
            duration.VerticalAlignment(mux::VerticalAlignment::Bottom);
            duration.Margin(ui::uniform(6));
            media.Children().Append(duration);
        }
        media.CornerRadius(mux::CornerRadius{11, 11, 0, 0});
        body.Children().Append(media);
        auto name = ui::text(r.name, ui::Text::caption);
        name.TextWrapping(mux::TextWrapping::NoWrap);
        name.TextTrimming(mux::TextTrimming::CharacterEllipsis);
        name.Margin(ui::margin(8, 0, 8, 6));
        body.Children().Append(name);
        tile.Content(body);
        ui::tooltip(tile, r.name + (selected ? " — tocá para quitarla" : " — tocá para usarla"));
        ui::accessible(tile, r.name + (tag ? ", " + *tag : ", sin usar"));
        tile.Click([r](auto&&, auto&&) { AppModel::shared().toggleSelection(r); });
        if (selected && model.selection(r.kind).size() > 1) {
            muxc::MenuFlyout menu;
            muxc::MenuFlyoutItem left;
            left.Text(L"Mover antes");
            left.Click([r](auto&&, auto&&) { AppModel::shared().moveSelection(r.kind, r.id, -1); });
            muxc::MenuFlyoutItem right;
            right.Text(L"Mover después");
            right.Click([r](auto&&, auto&&) { AppModel::shared().moveSelection(r.kind, r.id, 1); });
            menu.Items().Append(left);
            menu.Items().Append(right);
            tile.ContextFlyout(menu);
        }
        return tile;
    }

    void refreshSettings() {
        auto& model = AppModel::shared();
        auto& p = model.preferences;
        std::vector<mux::UIElement> items;
        settingsSummary_.Text(hs(model.settingsSummary()));
        if (model.mode() == fc::MediaKind::image) {
            items.push_back(fieldTitle("Modelo"));
            auto grid = ui::columns({ui::star(), ui::star()}, 10);
            grid.RowSpacing(10);
            int index = 0;
            for (auto const& m : fc::presets::imageModels()) {
                if (index % 2 == 0) grid.RowDefinitions().Append(muxc::RowDefinition());
                auto card = ui::choiceCard(m.name, m.note, hs(m.glyph).c_str(), p.imageModel == m.id, [id = m.id] { AppModel::shared().setImageModel(id); });
                ui::place(grid, card, index % 2, index / 2);
                ++index;
            }
            items.push_back(grid);
            auto row = ui::columns({ui::autoLength(), ui::autoLength()}, 32);
            auto quality = ui::vstack(6);
            quality.Children().Append(fieldTitle("Calidad", "4K es más nítido pero tarda más y gasta más créditos."));
            auto const& resolutions = fc::presets::imageResolutions();
            int selected = static_cast<int>(std::find(resolutions.begin(), resolutions.end(), p.imageResolution) - resolutions.begin());
            quality.Children().Append(ui::segmented(resolutions, selected, [](int i) { AppModel::shared().setImageResolution(fc::presets::imageResolutions()[i]); }));
            ui::place(row, quality, 0);
            auto count = ui::vstack(6);
            count.Children().Append(fieldTitle("Cantidad"));
            count.Children().Append(ui::segmented({"1", "2", "3", "4"}, p.quantity - 1, [](int i) {
                AppModel::shared().preferences.quantity = i + 1;
                AppModel::shared().notify(Change::form);
            }));
            ui::place(row, count, 1);
            items.push_back(row);
        } else {
            auto row = ui::columns({ui::autoLength(), ui::autoLength()}, 32);
            auto quality = ui::vstack(6);
            quality.Children().Append(fieldTitle("Calidad"));
            auto const& resolutions = fc::presets::videoResolutions();
            int selected = static_cast<int>(std::find(resolutions.begin(), resolutions.end(), p.videoResolution) - resolutions.begin());
            quality.Children().Append(ui::segmented(resolutions, selected, [](int i) {
                AppModel::shared().preferences.videoResolution = fc::presets::videoResolutions()[i];
                AppModel::shared().notify(Change::form);
            }));
            ui::place(row, quality, 0);
            auto audio = ui::toggle("Sonido y voz", p.videoAudio, [](bool on) {
                AppModel::shared().preferences.videoAudio = on;
                AppModel::shared().notify(Change::form);
            });
            ui::place(row, audio, 1);
            items.push_back(row);
            auto duration = ui::vstack(6);
            auto durationTitle = ui::hstack(6);
            durationLabel_ = ui::text("Duración: " + std::to_string(p.videoDuration) + " s", ui::Text::bodyStrong);
            durationTitle.Children().Append(durationLabel_);
            auto info = ui::icon(L"\uE946", 12);
            info.Foreground(ui::resource(L"TextFillColorSecondaryBrush"));
            info.VerticalAlignment(mux::VerticalAlignment::Center);
            ui::tooltip(info, "Seedance 2.5 genera clips de 4 a 30 segundos. ~2,47 palabras de diálogo por segundo.");
            durationTitle.Children().Append(info);
            duration.Children().Append(durationTitle);
            muxc::Slider slider;
            slider.Minimum(fc::presets::kMinVideoDuration);
            slider.Maximum(fc::presets::kMaxVideoDuration);
            slider.StepFrequency(1);
            slider.Value(p.videoDuration);
            slider.HorizontalAlignment(mux::HorizontalAlignment::Left);
            slider.Width(520);
            ui::accessible(slider, "Duración del video en segundos");
            slider.ValueChanged([this](IInspectable const&, muxc::Primitives::RangeBaseValueChangedEventArgs const& args) {
                if (updating_) return;
                auto& m = AppModel::shared();
                int value = static_cast<int>(std::lround(args.NewValue()));
                if (value == m.preferences.videoDuration) return;
                m.preferences.videoDuration = value;
                durationLabel_.Text(hs("Duración: " + std::to_string(value) + " s"));
                refreshBar();
                refreshChecker();
            });
            duration.Children().Append(slider);
            auto shortcuts = ui::hstack(6);
            for (int seconds : fc::presets::videoDurationShortcuts()) {
                auto b = ui::button(std::to_string(seconds) + " s", L"", [seconds] {
                    AppModel::shared().preferences.videoDuration = seconds;
                    AppModel::shared().notify(Change::form);
                });
                b.Padding(ui::margin(10, 3, 10, 4));
                if (seconds == p.videoDuration) b.Background(ui::softGradient());
                shortcuts.Children().Append(b);
            }
            duration.Children().Append(shortcuts);
            items.push_back(duration);
        }
        items.push_back(fieldTitle("Formato"));
        std::vector<mux::UIElement> aspects;
        std::string current = model.mode() == fc::MediaKind::image ? p.imageAspect : p.videoAspect;
        for (auto const& option : model.aspectOptions()) {
            aspects.push_back(aspectButton(option, option == current, [option] {
                auto& m = AppModel::shared();
                if (m.mode() == fc::MediaKind::image) m.preferences.imageAspect = option;
                else m.preferences.videoAspect = option;
                m.notify(Change::form);
            }));
        }
        items.push_back(wrap(aspects, 84, 84));
        if (model.mode() == fc::MediaKind::image) {
            auto presets = [&](std::string const& title, std::vector<fc::StylePreset> const& list, std::string const& selected, bool camera) {
                items.push_back(fieldTitle(title, camera ? "Se agrega al final del prompt para imitar esa cámara." : "Cómo se encuadra la foto."));
                auto grid = ui::columns({ui::star(), ui::star()}, 10);
                grid.RowSpacing(10);
                int index = 0;
                for (auto const& preset : list) {
                    if (index % 2 == 0) grid.RowDefinitions().Append(muxc::RowDefinition());
                    auto card = ui::choiceCard(preset.title, preset.summary, hs(preset.glyph).c_str(), selected == preset.id, [id = preset.id, camera] {
                        auto& m = AppModel::shared();
                        if (camera) m.preferences.camera = id;
                        else m.preferences.film = id;
                        m.notify(Change::form);
                    });
                    ui::place(grid, card, index % 2, index / 2);
                    ++index;
                }
                items.push_back(grid);
            };
            presets("Cámara", fc::presets::cameras(), p.camera, true);
            presets("Encuadre", fc::presets::films(), p.film, false);
        }
        ui::setChildren(settings_, items);
    }

    void refreshBar() {
        auto& model = AppModel::shared();
        summary_.Text(hs(model.settingsSummary()));
        // Say what is missing before generating, or that everything is ready.
        auto missing = model.generateBlocker();
        if (!model.hasKieKey) {
            hint_.Text(L"\u26A0 Conectá tu clave de KIE para generar");
            hint_.Foreground(muxm::SolidColorBrush(ui::rgb(230, 126, 34)));
        } else if (missing && !model.isGenerating) {
            hint_.Text(hs("\u2191 " + *missing));
            hint_.Foreground(muxm::SolidColorBrush(ui::rgb(230, 126, 34)));
        } else {
            hint_.Text(L"\u2713 Todo listo · usa tus créditos de KIE · Ctrl+Enter");
            hint_.Foreground(muxm::SolidColorBrush(ui::successColor()));
        }
        auto active = model.activeJobs();
        ui::show(active_, !active.empty());
        ui::setLabel(active_, L"", std::to_string(active.size()) + " en curso");
        if (model.isGenerating) {
            auto row = ui::hstack(8);
            muxc::ProgressRing ring;
            ring.Width(16);
            ring.Height(16);
            ring.Foreground(muxm::SolidColorBrush(ui::rgb(255, 255, 255)));
            row.Children().Append(ring);
            row.Children().Append(ui::text(model.generationStep.empty() ? "Enviando…" : model.generationStep));
            generate_.Content(row);
        } else {
            std::string label = model.mode() == fc::MediaKind::video ? "Generar video"
                                : model.preferences.quantity == 1 ? "Generar imagen"
                                                                  : "Generar " + std::to_string(model.preferences.quantity) + " imágenes";
            ui::setLabel(generate_, L"", label);
        }
        auto blocker = model.generateBlocker();
        generate_.IsEnabled(!blocker.has_value());
        ui::tooltip(generate_, blocker.value_or("Generar (Ctrl+Enter)"));
    }

    muxc::Grid root_{nullptr};
    muxc::ScrollViewer scroll_{nullptr};
    muxc::StackPanel templates_{nullptr}, step1Header_{nullptr}, tags_{nullptr}, checker_{nullptr}, references_{nullptr}, settings_{nullptr};
    muxc::Grid modes_{nullptr};
    muxc::StackPanel progress_{nullptr};
    mux::UIElement step1Card_{nullptr}, referencesCard_{nullptr}, settingsCard_{nullptr};
    muxc::Border checkerCard_{nullptr}, assistantHost_{nullptr};
    muxc::TextBox prompt_{nullptr};
    muxc::TextBlock counter_{nullptr}, summary_{nullptr}, hint_{nullptr}, durationLabel_{nullptr}, referencesSummary_{nullptr},
        settingsSummary_{nullptr};
    muxc::Button generate_{nullptr}, active_{nullptr}, assistantToggle_{nullptr}, referencesToggle_{nullptr}, settingsToggle_{nullptr};
    muxc::FontIcon referencesChevron_{nullptr}, settingsChevron_{nullptr};
    AssistantPane assistant_;
    bool updating_ = false;
    bool narrow_ = false;
    bool referencesOpen_ = false;
    bool settingsOpen_ = false;
};

}  // namespace

std::unique_ptr<Page> makeCreatePage() { return std::make_unique<CreatePage>(); }

}  // namespace fcapp
