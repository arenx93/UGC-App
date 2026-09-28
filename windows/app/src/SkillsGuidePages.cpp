#include "pch.h"

#include "Pages.h"
#include "WinUtil.h"
#include "framecraft/util.h"

using namespace winrt;
namespace mux = winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxm = winrt::Microsoft::UI::Xaml::Media;
namespace wf = winrt::Windows::Foundation;
namespace fs = std::filesystem;
using winrt::Windows::Foundation::IInspectable;

namespace fcapp {

namespace {

/// Inline markdown (**bold**, *italic*, `code`) into TextBlock runs.
muxc::TextBlock inlineText(std::string const& source, double size = 14, bool bold = false) {
    muxc::TextBlock block;
    block.TextWrapping(mux::TextWrapping::Wrap);
    block.IsTextSelectionEnabled(true);
    block.FontSize(size);
    if (bold) block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
    namespace docs = winrt::Microsoft::UI::Xaml::Documents;
    std::string buffer;
    bool strong = false, italic = false, code = false;
    auto flush = [&] {
        if (buffer.empty()) return;
        docs::Run run;
        run.Text(hs(buffer));
        if (strong || bold) run.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        if (italic) run.FontStyle(winrt::Windows::UI::Text::FontStyle::Italic);
        if (code) run.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
        block.Inlines().Append(run);
        buffer.clear();
    };
    for (size_t i = 0; i < source.size(); ++i) {
        if (source.compare(i, 2, "**") == 0 && !code) {
            flush();
            strong = !strong;
            ++i;
        } else if (source[i] == '`') {
            flush();
            code = !code;
        } else if (source[i] == '*' && !code && i + 1 < source.size() && source[i + 1] != ' ') {
            flush();
            italic = !italic;
        } else if (source[i] == '[' && !code) {
            // [text](url) -> text
            size_t close = source.find("](", i);
            size_t end = close == std::string::npos ? std::string::npos : source.find(')', close);
            if (close != std::string::npos && end != std::string::npos) {
                buffer += source.substr(i + 1, close - i - 1);
                i = end;
            } else {
                buffer += source[i];
            }
        } else {
            buffer += source[i];
        }
    }
    flush();
    return block;
}

}  // namespace

/// Block-level markdown: headings, lists, quotes, code, tables and rules.
mux::UIElement markdownView(std::string const& markdown) {
    auto panel = ui::vstack(10);
    std::vector<std::string> paragraph, quote;
    std::optional<std::vector<std::string>> code;
    std::vector<std::vector<std::string>> table;
    auto flush = [&] {
        if (!paragraph.empty()) {
            panel.Children().Append(inlineText(fc::join(paragraph, " ")));
            paragraph.clear();
        }
        if (!quote.empty()) {
            muxc::Border border;
            border.BorderBrush(ui::pinkBrush());
            border.BorderThickness(ui::margin(3, 0, 0, 0));
            border.Padding(ui::margin(12, 2, 0, 2));
            auto text = inlineText(fc::join(quote, " "));
            text.FontStyle(winrt::Windows::UI::Text::FontStyle::Italic);
            border.Child(text);
            panel.Children().Append(border);
            quote.clear();
        }
        if (!table.empty()) {
            muxc::Grid grid;
            grid.ColumnSpacing(14);
            grid.RowSpacing(6);
            size_t columns = 0;
            for (auto const& row : table) columns = std::max(columns, row.size());
            for (size_t c = 0; c < columns; ++c) grid.ColumnDefinitions().Append(muxc::ColumnDefinition());
            for (size_t r = 0; r < table.size(); ++r) {
                muxc::RowDefinition def;
                def.Height(ui::autoLength());
                grid.RowDefinitions().Append(def);
                for (size_t c = 0; c < table[r].size(); ++c) {
                    auto cell = inlineText(table[r][c], 13, r == 0);
                    ui::place(grid, cell, static_cast<int>(c), static_cast<int>(r));
                }
            }
            auto frame = ui::card(grid, 12);
            panel.Children().Append(frame);
            table.clear();
        }
    };
    for (auto const& raw : fc::splitLines(markdown)) {
        std::string line = fc::trim(raw);
        if (fc::startsWith(line, "```")) {
            if (code) {
                auto text = ui::text(fc::join(*code, "\n"), ui::Text::caption);
                text.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
                text.IsTextSelectionEnabled(true);
                muxc::Border border;
                border.Background(ui::resource(L"ControlAltFillColorSecondaryBrush"));
                border.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(8));
                border.Padding(ui::uniform(12));
                border.Child(text);
                panel.Children().Append(border);
                code.reset();
            } else {
                flush();
                code = std::vector<std::string>{};
            }
            continue;
        }
        if (code) {
            code->push_back(raw);
            continue;
        }
        if (line.empty()) {
            flush();
            continue;
        }
        if (fc::startsWith(line, "|")) {
            if (line.find_first_not_of("|-: ") == std::string::npos) continue;  // separator row
            std::vector<std::string> cells;
            std::string cell;
            for (size_t i = 1; i < line.size(); ++i) {
                if (line[i] == '|') {
                    cells.push_back(fc::trim(cell));
                    cell.clear();
                } else {
                    cell += line[i];
                }
            }
            if (!fc::trim(cell).empty()) cells.push_back(fc::trim(cell));
            if (!paragraph.empty()) flush();
            table.push_back(cells);
            continue;
        }
        if (!table.empty()) flush();
        if (line == "---" || line == "***") {
            flush();
            muxc::Border rule;
            rule.Height(1);
            rule.Background(ui::resource(L"CardStrokeColorDefaultBrush"));
            rule.Margin(ui::margin(0, 6, 0, 6));
            panel.Children().Append(rule);
            continue;
        }
        size_t hashes = 0;
        while (hashes < line.size() && line[hashes] == '#') ++hashes;
        if (hashes > 0 && hashes <= 6 && hashes < line.size() && line[hashes] == ' ') {
            flush();
            double size = hashes == 1 ? 26 : hashes == 2 ? 20 : 16;
            auto heading = inlineText(line.substr(hashes + 1), size, true);
            heading.Margin(ui::margin(0, hashes <= 2 ? 10 : 4, 0, 0));
            panel.Children().Append(heading);
            continue;
        }
        if (fc::startsWith(line, "> ")) {
            if (!paragraph.empty()) flush();
            quote.push_back(line.substr(2));
            continue;
        }
        if (fc::startsWith(line, "- ") || fc::startsWith(line, "* ") || fc::startsWith(line, "• ")) {
            flush();
            auto row = ui::columns({ui::autoLength(), ui::star()}, 8);
            muxc::Border dot;
            dot.Width(6);
            dot.Height(6);
            dot.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(3));
            dot.Background(ui::pinkBrush());
            dot.VerticalAlignment(mux::VerticalAlignment::Top);
            dot.Margin(ui::margin(6, 8, 0, 0));
            ui::place(row, dot, 0);
            ui::place(row, inlineText(line.substr(line.find(' ') + 1)), 1);
            panel.Children().Append(row);
            continue;
        }
        size_t digits = 0;
        while (digits < line.size() && line[digits] >= '0' && line[digits] <= '9') ++digits;
        if (digits > 0 && digits + 1 < line.size() && (line[digits] == '.' || line[digits] == ')') && line[digits + 1] == ' ') {
            flush();
            auto row = ui::columns({ui::autoLength(), ui::star()}, 8);
            auto number = ui::text(line.substr(0, digits + 1), ui::Text::bodyStrong);
            number.Foreground(ui::pinkBrush());
            ui::place(row, number, 0);
            ui::place(row, inlineText(line.substr(digits + 2)), 1);
            panel.Children().Append(row);
            continue;
        }
        paragraph.push_back(line);
    }
    flush();
    return panel;
}

mux::UIElement rulesCard() {
    auto panel = ui::vstack(12);
    panel.Children().Append(ui::text("Las cinco reglas del método", ui::Text::subtitle));
    struct Rule {
        std::wstring glyph;
        std::string title, detail;
    };
    std::vector<Rule> rules = {
        {L"", "Causas físicas, no adjetivos", "\"El operador reencuadra medio segundo tarde\" en vez de \"realista\"."},
        {L"", "Acción por bloques de tiempo", "0–6 s, 6–12 s… Sin eso el modelo adivina el timeline."},
        {L"", "Diálogo con densidad medida", "~2,47 palabras por segundo: 30 s ≈ 74 palabras. Los silencios se hacen en la edición."},
        {L"", "Emoción como mecánica física", "Entre paréntesis y aclarando que no se pronuncia: dónde respira, dónde baja la voz."},
        {L"", "La continuidad se copia y pega", "La biblia de personajes y locación va idéntica en cada escena."},
    };
    for (auto const& rule : rules) {
        auto row = ui::columns({ui::autoLength(), ui::star()}, 12);
        ui::place(row, ui::glyphBadge(rule.glyph, 32), 0);
        auto labels = ui::vstack(1);
        labels.Children().Append(ui::text(rule.title, ui::Text::bodyStrong));
        labels.Children().Append(ui::secondary(rule.detail));
        ui::place(row, labels, 1);
        panel.Children().Append(row);
    }
    return ui::card(panel, 20);
}

namespace {

// MARK: - Skills

class SkillsPage : public Page {
public:
    SkillsPage() {
        root_ = ui::columns({ui::pixels(300), ui::star()}, 0);
        auto left = muxc::Grid();
        muxc::RowDefinition top;
        top.Height(ui::autoLength());
        left.RowDefinitions().Append(top);
        left.RowDefinitions().Append(muxc::RowDefinition());
        left.BorderBrush(ui::resource(L"CardStrokeColorDefaultBrush"));
        left.BorderThickness(ui::margin(0, 0, 1, 0));
        auto actions = ui::vstack(10);
        actions.Padding(ui::margin(20, 24, 16, 12));
        actions.Children().Append(ui::gradientTitle("Skills", 28));
        actions.Children().Append(ui::secondary("Métodos que el asistente aplica siempre igual.", ui::Text::body));
        auto buttons = ui::hstack(8);
        buttons.Children().Append(ui::button("Importar", L"", [this] { importSkill(); }));
        buttons.Children().Append(ui::button("Nueva", L"", [this] {
            fc::Skill skill{fc::newUUID(), "Nueva skill", "", "", fc::SkillMedia::any, false, fc::nowMs()};
            edit(skill, true);
        }));
        actions.Children().Append(buttons);
        ui::place(left, actions, 0, 0);
        list_ = muxc::ListView();
        list_.SelectionMode(muxc::ListViewSelectionMode::Single);
        list_.SelectionChanged([this](auto&&, auto&&) {
            if (updating_) return;
            auto item = list_.SelectedItem().try_as<muxc::ListViewItem>();
            if (item) {
                selected_ = str(unbox_value<hstring>(item.Tag()));
                refreshDetail();
            }
        });
        ui::place(left, list_, 0, 1);
        ui::place(root_, left, 0);
        detailScroll_ = muxc::ScrollViewer();
        detail_ = ui::vstack(16);
        detail_.Padding(ui::margin(32, 24, 32, 32));
        detail_.MaxWidth(900);
        detail_.HorizontalAlignment(mux::HorizontalAlignment::Left);
        detailScroll_.Content(detail_);
        ui::place(root_, detailScroll_, 1);
    }

    mux::UIElement root() override { return root_; }

    void refresh(Change change) override {
        if (change != Change::all && change != Change::skills) return;
        auto& model = AppModel::shared();
        updating_ = true;
        list_.Items().Clear();
        auto skills = model.allSkills();
        if (selected_.empty() || !model.skill(selected_)) selected_ = skills.size() > 1 ? skills[1].id : (skills.empty() ? "" : skills[0].id);
        for (auto const& skill : skills) {
            muxc::ListViewItem item;
            auto row = ui::columns({ui::autoLength(), ui::star()}, 10);
            ui::place(row, ui::glyphBadge(skill.isBuiltin ? L"" : L"", 28, skill.id == selected_), 0);
            auto labels = ui::vstack(0);
            auto name = ui::text(skill.name, ui::Text::bodyStrong);
            name.TextWrapping(mux::TextWrapping::NoWrap);
            name.TextTrimming(mux::TextTrimming::CharacterEllipsis);
            labels.Children().Append(name);
            std::string media = skill.media == fc::SkillMedia::video ? "Video" : skill.media == fc::SkillMedia::image ? "Imagen" : "Imagen y video";
            labels.Children().Append(ui::secondary(std::string(skill.isBuiltin ? "Incluida" : "Tuya") + " · " + media));
            ui::place(row, labels, 1);
            item.Content(row);
            item.Padding(ui::margin(12, 8, 12, 8));
            item.Tag(box_value(hs(skill.id)));
            list_.Items().Append(item);
            if (skill.id == selected_) list_.SelectedItem(item);
        }
        updating_ = false;
        refreshDetail();
    }

private:
    void refreshDetail() {
        auto& model = AppModel::shared();
        auto const* skill = model.skill(selected_);
        std::vector<mux::UIElement> items;
        if (!skill) {
            ui::setChildren(detail_, items);
            return;
        }
        fc::Skill copy = *skill;
        auto header = ui::columns({ui::star(), ui::autoLength()}, 12);
        auto titles = ui::vstack(4);
        titles.Children().Append(ui::text(copy.name, ui::Text::title));
        if (!copy.summary.empty()) titles.Children().Append(ui::secondary(copy.summary, ui::Text::body));
        ui::place(header, titles, 0);
        auto buttons = ui::hstack(8);
        buttons.VerticalAlignment(mux::VerticalAlignment::Top);
        buttons.Children().Append(ui::primaryButton("Usar en el asistente", L"", [copy] { AppModel::shared().useSkill(copy); }));
        if (copy.isBuiltin) {
            buttons.Children().Append(ui::button("Duplicar y editar", L"", [this, copy] {
                auto created = AppModel::shared().duplicateSkill(copy);
                selected_ = created.id;
                edit(created, false);
            }));
        } else {
            buttons.Children().Append(ui::button("Editar", L"", [this, copy] { edit(copy, false); }));
            auto remove = ui::button("Eliminar", L"", {});
            remove.Click([id = copy.id](IInspectable const& sender, auto&&) {
                ui::confirm(sender.as<mux::UIElement>().XamlRoot(), "¿Eliminar esta skill?", "No se puede deshacer.", "Eliminar",
                            [id] { AppModel::shared().deleteSkill(id); });
            });
            buttons.Children().Append(remove);
        }
        ui::place(header, buttons, 1);
        items.push_back(header);
        if (copy.id == fc::skills::kUgcID) items.push_back(rulesCard());
        if (copy.id == fc::skills::kJsonProfileID) {
            items.push_back(ui::card(ui::secondary("Esta skill usa un perfil JSON técnico y 123 prompts de ejemplo de GPT Image 2 (YouMind OpenLab, CC BY 4.0). "
                                                   "El asistente elige los 3 ejemplos que más se parecen a tu idea.",
                                                   ui::Text::body)));
        } else {
            items.push_back(ui::card(markdownView(copy.content), 22));
        }
        ui::setChildren(detail_, items);
    }

    winrt::fire_and_forget importSkill() {
        winrt::Windows::Storage::Pickers::FileOpenPicker picker;
        picker.as<::IInitializeWithWindow>()->Initialize(AppModel::shared().hwnd);
        for (auto ext : {L".md", L".txt", L".markdown"}) picker.FileTypeFilter().Append(ext);
        auto file = co_await picker.PickSingleFileAsync();
        if (!file) co_return;
        if (auto error = AppModel::shared().importSkill(fs::path(std::wstring(file.Path())))) AppModel::shared().show(*error, Banner::Style::error);
    }

    winrt::fire_and_forget edit(fc::Skill skill, bool isNew) {
        auto panel = ui::vstack(12);
        auto name = ui::field("Nombre", {});
        name.Header(box_value(L"Nombre"));
        name.Text(hs(skill.name));
        auto summary = ui::field("Para qué sirve, en una línea", {});
        summary.Header(box_value(L"Descripción corta"));
        summary.Text(hs(skill.summary));
        int mediaIndex = skill.media == fc::SkillMedia::any ? 0 : skill.media == fc::SkillMedia::image ? 1 : 2;
        auto media = ui::combo({"Imágenes y videos", "Imágenes", "Videos"}, mediaIndex, {});
        media.Header(box_value(L"Sirve para"));
        auto content = ui::editor("Escribí el método: reglas, estructura del prompt, vocabulario, restricciones y checklist.", 260, {}, true);
        content.Header(box_value(L"Instrucciones (markdown)"));
        content.Text(hs(skill.content));
        panel.Children().Append(name);
        panel.Children().Append(summary);
        panel.Children().Append(media);
        panel.Children().Append(content);
        auto dialog = ui::dialog(root_.XamlRoot(), isNew ? "Nueva skill" : "Editar skill", panel, "Guardar", "Cancelar");
        dialog.Resources().Insert(box_value(L"ContentDialogMaxWidth"), box_value(760.0));
        auto result = co_await dialog.ShowAsync();
        if (result != muxc::ContentDialogResult::Primary) co_return;
        skill.name = fc::trim(str(name.Text()));
        if (skill.name.empty()) skill.name = "Skill sin nombre";
        skill.summary = fc::trim(str(summary.Text()));
        int index = media.SelectedIndex();
        skill.media = index == 1 ? fc::SkillMedia::image : index == 2 ? fc::SkillMedia::video : fc::SkillMedia::any;
        skill.content = str(content.Text());
        AppModel::shared().saveSkill(skill);
        selected_ = skill.id;
        refresh(Change::skills);
    }

    muxc::Grid root_{nullptr};
    muxc::ListView list_{nullptr};
    muxc::ScrollViewer detailScroll_{nullptr};
    muxc::StackPanel detail_{nullptr};
    std::string selected_;
    bool updating_ = false;
};

// MARK: - Guide

class GuidePage : public Page {
public:
    GuidePage() {
        scroll_ = muxc::ScrollViewer();
        content_ = ui::vstack(20);
        content_.Padding(ui::margin(32, 24, 32, 32));
        content_.MaxWidth(920);
        content_.HorizontalAlignment(mux::HorizontalAlignment::Left);
        scroll_.Content(content_);
    }

    mux::UIElement root() override { return scroll_; }

    void refresh(Change change) override {
        if (change != Change::all || built_) return;
        built_ = true;
        auto& model = AppModel::shared();
        content_.Children().Append(ui::gradientTitle("Cómo escribir UGC que no parezca un anuncio", 30));
        content_.Children().Append(ui::secondary(
            "El método del kit de prompts de Seedance 2.5, dentro de la app. El asistente lo aplica cuando elegís la skill “UGC de celular”, y la revisión del prompt lo chequea mientras escribís.",
            ui::Text::body));
        auto buttons = ui::hstack(10);
        buttons.Children().Append(ui::primaryButton("Probar la skill UGC", L"", [] {
            auto& m = AppModel::shared();
            if (auto const* skill = m.skill(fc::skills::kUgcID)) m.useSkill(*skill);
        }));
        if (model.resources) {
            auto pdf = model.resources->guidePDF();
            auto walter = model.resources->path("ugc-celular/EJEMPLO-WALTER.txt");
            buttons.Children().Append(ui::button("Documento base (PDF)", L"", [pdf] { openPath(pdf); }));
            buttons.Children().Append(ui::button("Pack de Walter (8 escenas)", L"", [walter] { openPath(walter); }));
        }
        content_.Children().Append(buttons);
        content_.Children().Append(rulesCard());
        if (model.resources) content_.Children().Append(ui::card(markdownView(model.resources->text("guia/COMO-FUNCIONA.md")), 24));
    }

private:
    muxc::ScrollViewer scroll_{nullptr};
    muxc::StackPanel content_{nullptr};
    bool built_ = false;
};

// MARK: - Settings

class SettingsPage : public Page {
public:
    SettingsPage() {
        scroll_ = muxc::ScrollViewer();
        content_ = ui::vstack(16);
        content_.Padding(ui::margin(32, 24, 32, 32));
        content_.MaxWidth(820);
        content_.HorizontalAlignment(mux::HorizontalAlignment::Left);
        scroll_.Content(content_);
    }

    mux::UIElement root() override { return scroll_; }

    void refresh(Change change) override {
        if (change != Change::all && change != Change::account && change != Change::codex) return;
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> items;
        items.push_back(ui::gradientTitle("Ajustes", 30));

        // KIE.
        auto kie = ui::vstack(10);
        kie.Children().Append(section(L"", "Clave de KIE", "Con ella generás imágenes, videos y prompts. Se guarda en el Administrador de credenciales de Windows."));
        if (model.hasKieKey) {
            auto row = ui::columns({ui::star(), ui::autoLength(), ui::autoLength()}, 10);
            std::string status = "✓ Conectado" + (model.credits ? " · " + formatNumber(*model.credits) + " créditos" : std::string());
            auto ok = ui::text(status, ui::Text::bodyStrong);
            ok.Foreground(ui::resource(L"SystemFillColorSuccessBrush"));
            ok.VerticalAlignment(mux::VerticalAlignment::Center);
            ui::place(row, ok, 0);
            ui::place(row, ui::button("Actualizar créditos", L"", [] { AppModel::shared().refreshCredits(); }), 1);
            auto remove = ui::button("Quitar clave", L"", {});
            remove.Click([](IInspectable const& sender, auto&&) {
                ui::confirm(sender.as<mux::UIElement>().XamlRoot(), "¿Quitar la clave de KIE?", "Vas a tener que pegarla de nuevo para generar.", "Quitar",
                            [] { AppModel::shared().removeKieKey(); });
            });
            ui::place(row, remove, 2);
            kie.Children().Append(row);
        } else {
            auto row = ui::columns({ui::star(), ui::autoLength()}, 10);
            muxc::PasswordBox box;
            box.PlaceholderText(L"Pegá tu clave de KIE");
            ui::place(row, box, 0);
            auto status = ui::secondary("");
            auto save = ui::primaryButton("Conectar", L"", {});
            save.Click([box, status](auto&&, auto&&) {
                status.Text(L"Verificando…");
                AppModel::shared().saveKieKey(str(box.Password()), [status](std::optional<std::string> error) {
                    status.Text(error ? hs(*error) : hstring());
                });
            });
            ui::place(row, save, 1);
            kie.Children().Append(row);
            kie.Children().Append(status);
            kie.Children().Append(ui::link("Conseguir mi clave en kie.ai", [] { openURL(fc::kie::kKeyPageURL); }));
        }
        items.push_back(ui::card(kie, 20));

        // OpenAI.
        auto openai = ui::vstack(10);
        openai.Children().Append(section(L"", "Clave de OpenAI (opcional)", "Solo si querés usar el motor “OpenAI API” en el asistente."));
        if (model.hasOpenAIKey) {
            auto row = ui::columns({ui::star(), ui::autoLength()}, 10);
            auto ok = ui::text("✓ Guardada", ui::Text::bodyStrong);
            ok.Foreground(ui::resource(L"SystemFillColorSuccessBrush"));
            ui::place(row, ok, 0);
            ui::place(row, ui::button("Quitar", L"", [] { AppModel::shared().removeOpenAIKey(); }), 1);
            openai.Children().Append(row);
        } else {
            auto row = ui::columns({ui::star(), ui::autoLength()}, 10);
            muxc::PasswordBox box;
            box.PlaceholderText(L"sk-…");
            ui::place(row, box, 0);
            auto status = ui::secondary("");
            auto save = ui::button("Guardar", L"", {});
            save.Click([box, status](auto&&, auto&&) {
                auto error = AppModel::shared().saveOpenAIKey(str(box.Password()));
                status.Text(error ? hs(*error) : hstring());
            });
            ui::place(row, save, 1);
            openai.Children().Append(row);
            openai.Children().Append(status);
        }
        items.push_back(ui::card(openai, 20));

        // Codex.
        auto codexBox = ui::vstack(10);
        codexBox.Children().Append(section(L"", "ChatGPT (Codex)", "Usá tu cuenta de ChatGPT como motor del asistente. Codex viene incluido: solo iniciás sesión."));
        std::string state;
        switch (model.codexStatus.state) {
        case codex::State::unknown: state = "Sin comprobar"; break;
        case codex::State::checking: state = "Comprobando…"; break;
        case codex::State::notInstalled: state = "No se encontró Codex en esta copia de la app"; break;
        case codex::State::loggedOut: state = "Sin sesión"; break;
        case codex::State::loggingIn: state = "Iniciando sesión en el navegador…"; break;
        case codex::State::loggedIn: state = "✓ " + model.codexStatus.detail; break;
        }
        codexBox.Children().Append(ui::text(state, ui::Text::bodyStrong));
        auto codexButtons = ui::hstack(8);
        if (model.codexStatus.state == codex::State::loggedIn) {
            codexButtons.Children().Append(ui::button("Cerrar sesión", L"", [] { AppModel::shared().codexLogout(); }));
        } else if (model.codexStatus.state == codex::State::loggedOut) {
            codexButtons.Children().Append(ui::primaryButton("Iniciar sesión con ChatGPT", L"", [] { AppModel::shared().codexLogin(); }));
        }
        codexButtons.Children().Append(ui::button("Comprobar", L"", [] { AppModel::shared().refreshCodexStatus(); }));
        codexBox.Children().Append(codexButtons);
        items.push_back(ui::card(codexBox, 20));

        // Appearance and notices.
        auto general = ui::vstack(12);
        general.Children().Append(section(L"", "Apariencia y avisos", ""));
        int theme = model.preferences.theme == "light" ? 1 : model.preferences.theme == "dark" ? 2 : 0;
        auto themePicker = ui::segmented({"Sistema", "Claro", "Oscuro"}, theme, [](int index) {
            auto& m = AppModel::shared();
            m.preferences.theme = index == 1 ? "light" : index == 2 ? "dark" : "system";
            m.persist();
            m.show("El tema se aplica la próxima vez que abras Framecraft.");
        });
        general.Children().Append(themePicker);
        general.Children().Append(ui::toggle("Avisarme cuando termine una generación", model.preferences.notifyWhenDone, [](bool on) {
            AppModel::shared().preferences.notifyWhenDone = on;
            AppModel::shared().persist();
        }));
        general.Children().Append(ui::secondary("Llega una notificación de Windows si estás en otra app. La barra de tareas muestra el progreso."));
        items.push_back(ui::card(general, 20));

        // Library.
        auto library = ui::vstack(10);
        library.Children().Append(section(L"", "Tus creaciones", narrow(model.mediaRoot.wstring())));
        auto libraryButtons = ui::hstack(8);
        libraryButtons.Children().Append(ui::button("Abrir en el Explorador", L"", [] { openPath(AppModel::shared().mediaRoot); }));
        libraryButtons.Children().Append(ui::button("Ver la bienvenida de nuevo", L"", [] {
            if (AppModel::shared().openOnboarding) AppModel::shared().openOnboarding();
        }));
        library.Children().Append(libraryButtons);
        items.push_back(ui::card(library, 20));

        auto about = ui::vstack(6);
        about.Children().Append(section(L"", "Acerca de Framecraft", "App nativa para Windows escrita en C++ con WinUI 3. Tus claves quedan en tu equipo."));
        about.Children().Append(ui::link("Modelos y precios en kie.ai", [] { openURL("https://kie.ai"); }));
        items.push_back(ui::card(about, 20));
        ui::setChildren(content_, items);
    }

private:
    static mux::UIElement section(std::wstring const& glyph, std::string const& title, std::string const& subtitle) {
        auto row = ui::columns({ui::autoLength(), ui::star()}, 12);
        ui::place(row, ui::glyphBadge(glyph, 34), 0);
        auto labels = ui::vstack(2);
        labels.Children().Append(ui::text(title, ui::Text::subtitle));
        if (!subtitle.empty()) labels.Children().Append(ui::secondary(subtitle));
        ui::place(row, labels, 1);
        return row;
    }

    muxc::ScrollViewer scroll_{nullptr};
    muxc::StackPanel content_{nullptr};
};

}  // namespace

std::unique_ptr<Page> makeSkillsPage() { return std::make_unique<SkillsPage>(); }
std::unique_ptr<Page> makeGuidePage() { return std::make_unique<GuidePage>(); }
std::unique_ptr<Page> makeSettingsPage() { return std::make_unique<SettingsPage>(); }

}  // namespace fcapp
