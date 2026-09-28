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

const std::vector<std::string> kStoryLanguages = {"Español", "Español rioplatense", "Inglés", "Portugués", "Sin diálogo"};

int indexOf(std::vector<std::string> const& list, std::string const& value) {
    auto it = std::find(list.begin(), list.end(), value);
    return it == list.end() ? 0 : static_cast<int>(it - list.begin());
}

mux::UIElement labeledField(std::string const& title, mux::UIElement const& control) {
    auto stack = ui::vstack(6);
    stack.Children().Append(ui::text(title, ui::Text::bodyStrong));
    stack.Children().Append(control);
    return stack;
}

class StoriesPage : public Page {
public:
    StoriesPage() {
        root_ = ui::columns({ui::pixels(270), ui::star()}, 0);
        auto left = muxc::Grid();
        muxc::RowDefinition top;
        top.Height(ui::autoLength());
        left.RowDefinitions().Append(top);
        left.RowDefinitions().Append(muxc::RowDefinition());
        left.BorderBrush(ui::resource(L"CardStrokeColorDefaultBrush"));
        left.BorderThickness(ui::margin(0, 0, 1, 0));
        auto header = ui::vstack(10);
        header.Padding(ui::margin(20, 24, 16, 12));
        header.Children().Append(ui::gradientTitle("Historias", 28));
        auto add = ui::primaryButton("Nueva historia", L"", [] { AppModel::shared().newStory(); });
        add.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
        header.Children().Append(add);
        ui::place(left, header, 0, 0);
        list_ = muxc::ListView();
        list_.SelectionChanged([this](auto&&, auto&&) {
            if (updating_) return;
            if (auto item = list_.SelectedItem().try_as<muxc::ListViewItem>()) {
                auto& m = AppModel::shared();
                m.selectedStoryID = str(unbox_value<hstring>(item.Tag()));
                rebuildDetail();
            }
        });
        ui::place(left, list_, 0, 1);
        ui::place(root_, left, 0);

        detailScroll_ = muxc::ScrollViewer();
        detail_ = ui::vstack(18);
        detail_.Padding(ui::margin(32, 24, 32, 40));
        detail_.MaxWidth(1000);
        detail_.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
        detailScroll_.Content(detail_);
        ui::place(root_, detailScroll_, 1);
    }

    mux::UIElement root() override { return root_; }

    void refresh(Change change) override {
        auto& model = AppModel::shared();
        if (change == Change::storyRun) {
            refreshProgress();
            refreshProduction();
            return;
        }
        if (change != Change::all && change != Change::stories && change != Change::jobs && change != Change::references) return;
        updating_ = true;
        list_.Items().Clear();
        if (model.selectedStoryID.empty() || !model.story(model.selectedStoryID))
            model.selectedStoryID = model.stories.empty() ? "" : model.stories.front().id;
        for (auto const& story : model.stories) {
            muxc::ListViewItem item;
            auto labels = ui::vstack(1);
            auto title = ui::text(story.title, ui::Text::bodyStrong);
            title.TextWrapping(mux::TextWrapping::NoWrap);
            title.TextTrimming(mux::TextTrimming::CharacterEllipsis);
            labels.Children().Append(title);
            labels.Children().Append(ui::secondary(story.scenes.empty() ? "Sin escenas todavía"
                                                                        : std::to_string(story.scenes.size()) + " escenas · " + std::to_string(story.totalDuration()) + " s"));
            item.Content(labels);
            item.Padding(ui::margin(14, 8, 14, 8));
            item.Tag(box_value(hs(story.id)));
            muxc::MenuFlyout menu;
            muxc::MenuFlyoutItem remove;
            remove.Text(L"Eliminar historia");
            remove.Icon(ui::icon(L""));
            remove.Click([id = story.id](IInspectable const& sender, auto&&) {
                ui::confirm(sender.as<mux::UIElement>().XamlRoot(), "¿Eliminar esta historia?", "Se borran el brief y los prompts. Los videos siguen en la Biblioteca.",
                            "Eliminar", [id] { AppModel::shared().deleteStory(id); });
            });
            menu.Items().Append(remove);
            item.ContextFlyout(menu);
            list_.Items().Append(item);
            if (story.id == model.selectedStoryID) list_.SelectedItem(item);
        }
        updating_ = false;
        // Rebuilding the whole detail loses what the user is typing: only do it when the story itself changed.
        if (change == Change::jobs) {
            refreshProduction();
            refreshSceneStatus();
        } else {
            rebuildDetail();
        }
    }

private:
    fc::Story* story() { return AppModel::shared().story(AppModel::shared().selectedStoryID); }

    void rebuildDetail() {
        auto& model = AppModel::shared();
        detail_.Children().Clear();
        sceneChips_.clear();
        sceneCards_.clear();
        auto* s = story();
        if (!s) {
            buildEmpty();
            return;
        }
        std::string id = s->id;
        buildHeader(*s);
        briefHost_ = ui::vstack(0);
        if (s->scenes.empty()) {
            briefHost_.Children().Append(briefCard(*s));
        } else {
            muxc::Expander expander;
            expander.Header(box_value(L"Brief, ajustes y referencias"));
            expander.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
            expander.HorizontalContentAlignment(mux::HorizontalAlignment::Stretch);
            expander.Content(briefCard(*s));
            briefHost_.Children().Append(expander);
        }
        detail_.Children().Append(briefHost_);
        progress_ = ui::vstack(8);
        detail_.Children().Append(progress_);
        refreshProgress();
        if (!s->scenes.empty()) {
            production_ = ui::vstack(0);
            detail_.Children().Append(production_);
            refreshProduction();
            buildOverview(*s);
            for (auto const& scene : s->scenes) detail_.Children().Append(sceneCard(*s, scene));
            buildConversation(*s);
        } else {
            production_ = nullptr;
        }
        (void)model;
        (void)id;
    }

    void buildEmpty() {
        auto panel = ui::vstack(18);
        panel.Children().Append(ui::gradientTitle("Contá una historia", 32));
        panel.Children().Append(ui::secondary(
            "Pegá el brief (personajes, lugar, qué pasa, tono, duración) y el asistente la divide en escenas, cada una con su prompt listo, de qué trata y en qué orden cargar las referencias. Después la ajustás escena por escena, generás todo en cadena y armás el video final.",
            ui::Text::body));
        auto blank = ui::primaryButton("Historia en blanco", L"", [] { AppModel::shared().newStory(); });
        panel.Children().Append(blank);
        panel.Children().Append(ui::text("O empezá con un formato:", ui::Text::bodyStrong));
        auto grid = ui::columns({ui::star(), ui::star()}, 12);
        grid.RowSpacing(12);
        int index = 0;
        for (auto const& t : fc::templates::stories()) {
            if (index % 2 == 0) grid.RowDefinitions().Append(muxc::RowDefinition());
            auto card = ui::choiceCard(t.title, t.subtitle + " · " + (t.sceneCount ? std::to_string(*t.sceneCount) + " escenas" : std::string("escenas automáticas")),
                                       hs(t.glyph).c_str(), false, [t] { AppModel::shared().newStory(&t); });
            ui::tooltip(card, t.brief);
            ui::place(grid, card, index % 2, index / 2);
            ++index;
        }
        panel.Children().Append(grid);
        detail_.Children().Append(panel);
    }

    void buildHeader(fc::Story const& s) {
        auto header = ui::columns({ui::star(), ui::autoLength()}, 12);
        auto titles = ui::vstack(4);
        muxc::TextBox title;
        title.Text(hs(s.title));
        title.FontSize(26);
        title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
        title.BorderThickness(ui::uniform(0));
        title.Background(muxm::SolidColorBrush(ui::rgb(0, 0, 0, 0)));
        title.Padding(ui::uniform(0));
        ui::accessible(title, "Título de la historia");
        title.TextChanged([id = s.id](IInspectable const& sender, auto&&) {
            AppModel::shared().updateStory(id, [&](fc::Story& story) { story.title = str(sender.as<muxc::TextBox>().Text()); });
        });
        title.LostFocus([](auto&&, auto&&) {
            AppModel::shared().persist();
            AppModel::shared().notify(Change::stories);
        });
        titles.Children().Append(title);
        if (!s.scenes.empty())
            titles.Children().Append(ui::secondary(std::to_string(s.scenes.size()) + " escenas · " + std::to_string(s.totalDuration()) + " s · " +
                                                   fc::presets::kVideoModelName + " · " + s.aspect,
                                                   ui::Text::body));
        ui::place(header, titles, 0);
        if (!s.scenes.empty()) {
            auto actions = ui::hstack(8);
            actions.VerticalAlignment(mux::VerticalAlignment::Top);
            auto exportButton = ui::button("Exportar pack", L"", [id = s.id] { AppModel::shared().exportStory(id); });
            ui::tooltip(exportButton, "Guardar todos los prompts en un .txt, como el pack de Walter");
            actions.Children().Append(exportButton);
            actions.Children().Append(ui::button("Copiar todo", L"", [id = s.id] {
                auto& m = AppModel::shared();
                if (auto* story = m.story(id)) m.copy(fc::stories::exportText(*story, m.displayLines(*story)));
            }));
            ui::place(header, actions, 1);
        }
        detail_.Children().Append(header);
    }

    mux::UIElement briefCard(fc::Story const& s) {
        auto& model = AppModel::shared();
        std::string id = s.id;
        auto panel = ui::vstack(14);
        auto brief = ui::editor(
            "Ej.: Walter, mozo afroamericano de 82 años en un diner americano. Un cliente lo filma con el celular mientras le cuenta que su hija falleció y que trabaja para pagar deudas. El cliente le deja $100 de propina, afuera lo rechaza, se abrazan… 8 escenas, POV del que filma, diálogo en inglés.",
            180, [id](std::string const& text) { AppModel::shared().updateStory(id, [&](fc::Story& story) { story.brief = text; }); });
        brief.Text(hs(s.brief));
        brief.Header(box_value(L"Brief de la historia"));
        panel.Children().Append(brief);

        auto row1 = ui::columns({ui::autoLength(), ui::autoLength(), ui::star()}, 24);
        std::vector<int> durations = {10, 15, 20, 25, 30};
        int durationIndex = static_cast<int>(std::find(durations.begin(), durations.end(), s.sceneDuration) - durations.begin());
        ui::place(row1, labeledField("Duración por escena", ui::segmented({"10 s", "15 s", "20 s", "25 s", "30 s"}, durationIndex, [id, durations](int i) {
                      AppModel::shared().updateStory(id, [&](fc::Story& story) { story.sceneDuration = durations[i]; });
                  })),
                  0);
        std::vector<std::string> counts = {"Automático"};
        for (int n = 2; n <= 12; ++n) counts.push_back(std::to_string(n));
        ui::place(row1, labeledField("Escenas", ui::combo(counts, s.sceneCount ? *s.sceneCount - 1 : 0, [id](int i) {
                      AppModel::shared().updateStory(id, [&](fc::Story& story) {
                          story.sceneCount = i == 0 ? std::nullopt : std::optional<int>(i + 1);
                      });
                  })),
                  1);
        panel.Children().Append(row1);

        auto row2 = ui::columns({ui::autoLength(), ui::autoLength(), ui::autoLength(), ui::star()}, 24);
        ui::place(row2, labeledField("Idioma del diálogo", ui::combo(kStoryLanguages, indexOf(kStoryLanguages, s.dialogueLanguage), [id](int i) {
                      AppModel::shared().updateStory(id, [&](fc::Story& story) { story.dialogueLanguage = kStoryLanguages[i]; });
                  })),
                  0);
        auto const& aspects = fc::presets::videoAspects();
        ui::place(row2, labeledField("Formato", ui::combo(aspects, indexOf(aspects, s.aspect), [id](int i) {
                      AppModel::shared().updateStory(id, [&](fc::Story& story) { story.aspect = fc::presets::videoAspects()[i]; });
                  })),
                  1);
        auto const& resolutions = fc::presets::videoResolutions();
        ui::place(row2, labeledField("Calidad", ui::combo(resolutions, indexOf(resolutions, s.resolution), [id](int i) {
                      AppModel::shared().updateStory(id, [&](fc::Story& story) { story.resolution = fc::presets::videoResolutions()[i]; });
                  })),
                  2);
        std::vector<std::string> skillNames = {"General"};
        std::vector<std::string> skillIDs = {fc::skills::kGeneralID};
        for (auto const& skill : model.skillsFor(fc::MediaKind::video)) {
            skillNames.push_back(skill.name);
            skillIDs.push_back(skill.id);
        }
        auto skillPicker = ui::combo(skillNames, indexOf(skillIDs, s.skillID), [id, skillIDs](int i) {
            AppModel::shared().updateStory(id, [&](fc::Story& story) { story.skillID = skillIDs[i]; });
        });
        skillPicker.MaxWidth(280);
        ui::place(row2, labeledField("Skill", skillPicker), 3);
        panel.Children().Append(row2);
        panel.Children().Append(ui::toggle("Sonido y voz", s.generateAudio, [id](bool on) {
            AppModel::shared().updateStory(id, [&](fc::Story& story) { story.generateAudio = on; });
        }));

        panel.Children().Append(slotsEditor(s));

        auto footer = ui::columns({ui::star(), ui::autoLength()}, 12);
        auto engine = ui::secondary("Motor: " + model.engineName() + " · se cambia en el asistente de Crear");
        engine.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::place(footer, engine, 0);
        auto create = ui::primaryButton(s.scenes.empty() ? "Crear historia" : "Rehacer desde el brief", L"", [id] {
            AppModel::shared().persist();
            AppModel::shared().generateStory(id, std::nullopt);
        });
        create.IsEnabled(!model.storyStatus.has_value());
        ui::place(footer, create, 1);
        panel.Children().Append(footer);
        return ui::card(panel, 20);
    }

    mux::UIElement slotsEditor(fc::Story const& s) {
        auto& model = AppModel::shared();
        std::string id = s.id;
        auto panel = ui::vstack(10);
        auto header = ui::columns({ui::star(), ui::autoLength()});
        auto titles = ui::vstack(2);
        titles.Children().Append(ui::text("Referencias fijas de la historia", ui::Text::bodyStrong));
        titles.Children().Append(ui::secondary("Se cargan en este orden en todas las escenas. Agregá primero los archivos en Referencias."));
        ui::place(header, titles, 0);
        muxc::DropDownButton add;
        add.Content(ui::labeled(L"", "Agregar"));
        muxc::MenuFlyout flyout;
        for (auto kind : fc::kAllReferenceKinds) {
            muxc::MenuFlyoutSubItem sub;
            sub.Text(kind == fc::ReferenceKind::image ? L"Imágenes" : kind == fc::ReferenceKind::video ? L"Videos" : L"Audios");
            auto files = model.referencesOf(kind);
            for (auto const& r : files) {
                muxc::MenuFlyoutItem item;
                item.Text(hs(r.name));
                item.Click([id, kind, rid = r.id](auto&&, auto&&) {
                    auto& m = AppModel::shared();
                    m.updateStory(id, [&](fc::Story& story) { story.slots.push_back({fc::newUUID(), kind, rid, false, ""}); });
                    m.persist();
                    m.notify(Change::stories);
                });
                sub.Items().Append(item);
            }
            if (files.empty()) {
                muxc::MenuFlyoutItem none;
                none.Text(L"No hay archivos");
                none.IsEnabled(false);
                sub.Items().Append(none);
            }
            flyout.Items().Append(sub);
        }
        flyout.Items().Append(muxc::MenuFlyoutSeparator());
        muxc::MenuFlyoutItem lastFrame;
        lastFrame.Text(L"Último fotograma de la escena anterior (imagen)");
        lastFrame.Icon(ui::icon(L""));
        lastFrame.Click([id](auto&&, auto&&) {
            auto& m = AppModel::shared();
            m.updateStory(id, [&](fc::Story& story) {
                story.slots.push_back({fc::newUUID(), fc::ReferenceKind::image, std::nullopt, true, "Primer fotograma clavado: continúa la escena anterior"});
            });
            m.persist();
            m.notify(Change::stories);
        });
        flyout.Items().Append(lastFrame);
        add.Flyout(flyout);
        ui::place(header, add, 1);
        panel.Children().Append(header);

        if (s.slots.empty()) {
            panel.Children().Append(ui::secondary("Sin referencias. Podés sumar la hoja del personaje, la locación, la voz o el último fotograma de la escena anterior."));
        }
        for (auto const& slot : s.slots) {
            auto row = ui::columns({ui::pixels(84), ui::pixels(260), ui::star(), ui::autoLength()}, 10);
            auto tag = ui::pill(s.tag(slot));
            tag.HorizontalAlignment(mux::HorizontalAlignment::Left);
            ui::place(row, tag, 0);
            mux::UIElement what{nullptr};
            if (slot.isLastFrame) {
                what = ui::labeled(L"", "Último fotograma de la escena anterior");
            } else if (auto const* r = slot.referenceID ? model.reference(*slot.referenceID) : nullptr) {
                auto line = ui::hstack(6);
                if (r->kind == fc::ReferenceKind::image) {
                    auto thumb = thumbnail(model.referencePath(*r), false, 80, 28, 28);
                    thumb.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(5));
                    line.Children().Append(thumb);
                }
                auto name = ui::text(r->name, ui::Text::body);
                name.TextWrapping(mux::TextWrapping::NoWrap);
                name.TextTrimming(mux::TextTrimming::CharacterEllipsis);
                name.VerticalAlignment(mux::VerticalAlignment::Center);
                line.Children().Append(name);
                what = line;
            } else {
                auto missing = ui::labeled(L"", "Archivo no encontrado");
                what = missing;
            }
            what.as<mux::FrameworkElement>().VerticalAlignment(mux::VerticalAlignment::Center);
            ui::place(row, what, 1);
            auto note = ui::field("Para qué sirve (ej.: identidad de Walter, locación, voz)", [id, sid = slot.id](std::string const& text) {
                AppModel::shared().updateStory(id, [&](fc::Story& story) {
                    for (auto& other : story.slots)
                        if (other.id == sid) other.note = text;
                });
            });
            note.Text(hs(slot.note));
            ui::place(row, note, 2);
            auto buttons = ui::hstack(2);
            auto move = [id, sid = slot.id, kind = slot.kind](int offset) {
                auto& m = AppModel::shared();
                m.updateStory(id, [&](fc::Story& story) {
                    std::vector<size_t> same;
                    for (size_t i = 0; i < story.slots.size(); ++i)
                        if (story.slots[i].kind == kind) same.push_back(i);
                    for (size_t k = 0; k < same.size(); ++k) {
                        if (story.slots[same[k]].id != sid) continue;
                        long target = static_cast<long>(k) + offset;
                        if (target >= 0 && target < static_cast<long>(same.size())) std::swap(story.slots[same[k]], story.slots[same[target]]);
                        break;
                    }
                });
                m.persist();
                m.notify(Change::stories);
            };
            auto up = ui::subtleButton("", L"", [move] { move(-1); });
            ui::tooltip(up, "Subir");
            auto down = ui::subtleButton("", L"", [move] { move(1); });
            ui::tooltip(down, "Bajar");
            auto remove = ui::subtleButton("", L"", [id, sid = slot.id] {
                auto& m = AppModel::shared();
                m.updateStory(id, [&](fc::Story& story) { std::erase_if(story.slots, [&](auto const& other) { return other.id == sid; }); });
                m.persist();
                m.notify(Change::stories);
            });
            ui::tooltip(remove, "Quitar");
            buttons.Children().Append(up);
            buttons.Children().Append(down);
            buttons.Children().Append(remove);
            ui::place(row, buttons, 3);
            panel.Children().Append(row);
        }
        return panel;
    }

    void refreshProgress() {
        if (!progress_) return;
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> items;
        if (model.storyStatus) {
            auto box = ui::vstack(8);
            auto row = ui::hstack(8);
            muxc::ProgressRing ring;
            ring.Width(18);
            ring.Height(18);
            row.Children().Append(ring);
            row.Children().Append(ui::text(*model.storyStatus, ui::Text::bodyStrong));
            box.Children().Append(row);
            if (model.storyStreaming && !model.storyStreaming->empty()) {
                std::string tail = *model.storyStreaming;
                if (tail.size() > 1800) tail = "…" + tail.substr(tail.size() - 1800);
                auto text = ui::secondary(tail);
                text.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
                box.Children().Append(text);
            } else {
                box.Children().Append(ui::secondary("Una historia completa puede tardar uno o dos minutos."));
            }
            items.push_back(ui::card(box, 16));
        }
        if (model.storyError) {
            auto error = ui::text(*model.storyError, ui::Text::body);
            error.Foreground(ui::resource(L"SystemFillColorCautionBrush"));
            items.push_back(error);
        }
        ui::setChildren(progress_, items);
    }

    void refreshProduction() {
        if (!production_) return;
        auto& model = AppModel::shared();
        auto* s = story();
        if (!s || s->scenes.empty()) return;
        std::string id = s->id;
        int ready = 0;
        for (auto const& scene : s->scenes)
            if (model.finishedClip(scene)) ++ready;
        bool running = model.storyRun && model.storyRun->storyID == id;
        auto panel = ui::vstack(14);
        auto header = ui::hstack(10);
        header.Children().Append(ui::labeled(L"", "Storyboard"));
        header.Children().Append(ui::secondary(std::to_string(ready) + " de " + std::to_string(s->scenes.size()) + " escenas con video"));
        panel.Children().Append(header);

        auto strip = ui::hstack(8);
        for (auto const& scene : s->scenes) {
            auto const* clip = model.finishedClip(scene);
            auto const* latest = model.latestJob(scene);
            bool current = running && model.storyRun->current == scene.number;
            double width = std::max(100.0, scene.duration * 7.0);
            muxc::Button tile;
            tile.Padding(ui::uniform(0));
            tile.Width(width);
            tile.Background(muxm::SolidColorBrush(ui::rgb(0, 0, 0, 0)));
            tile.BorderThickness(ui::uniform(0));
            tile.HorizontalContentAlignment(mux::HorizontalAlignment::Stretch);
            auto body = ui::vstack(5);
            muxc::Grid media;
            media.Height(96);
            media.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(10));
            media.BorderBrush(current ? ui::brandGradient() : ui::resource(L"CardStrokeColorDefaultBrush"));
            media.BorderThickness(ui::uniform(current ? 2.5 : 1));
            if (clip) {
                media.Children().Append(thumbnail(model.outputPath(clip->outputs.front()), true, 300, 0, 96));
            } else {
                media.Background(ui::softGradient());
                auto number = ui::text(std::to_string(scene.number), ui::Text::title);
                number.Foreground(ui::brandGradient());
                number.HorizontalAlignment(mux::HorizontalAlignment::Center);
                number.VerticalAlignment(mux::VerticalAlignment::Center);
                media.Children().Append(number);
            }
            auto seconds = ui::pill(std::to_string(scene.duration) + " s", false);
            seconds.HorizontalAlignment(mux::HorizontalAlignment::Left);
            seconds.VerticalAlignment(mux::VerticalAlignment::Top);
            seconds.Margin(ui::uniform(5));
            media.Children().Append(seconds);
            if (clip) {
                auto check = ui::icon(L"", 14);
                check.Foreground(ui::resource(L"SystemFillColorSuccessBrush"));
                check.HorizontalAlignment(mux::HorizontalAlignment::Left);
                check.VerticalAlignment(mux::VerticalAlignment::Bottom);
                check.Margin(ui::uniform(6));
                media.Children().Append(check);
            } else if (latest && fc::isActive(latest->status)) {
                auto percent = ui::pill(std::to_string(latest->progress) + "%");
                percent.HorizontalAlignment(mux::HorizontalAlignment::Left);
                percent.VerticalAlignment(mux::VerticalAlignment::Bottom);
                percent.Margin(ui::uniform(5));
                media.Children().Append(percent);
            } else if (latest && (latest->status == fc::JobStatus::fail || latest->status == fc::JobStatus::unknown)) {
                auto warning = ui::icon(L"", 14);
                warning.Foreground(ui::resource(L"SystemFillColorCautionBrush"));
                warning.HorizontalAlignment(mux::HorizontalAlignment::Left);
                warning.VerticalAlignment(mux::VerticalAlignment::Bottom);
                warning.Margin(ui::uniform(6));
                media.Children().Append(warning);
            }
            body.Children().Append(media);
            auto label = ui::text(std::to_string(scene.number) + ". " + scene.title, ui::Text::caption);
            label.TextWrapping(mux::TextWrapping::NoWrap);
            label.TextTrimming(mux::TextTrimming::CharacterEllipsis);
            body.Children().Append(label);
            tile.Content(body);
            ui::tooltip(tile, scene.summary);
            ui::accessible(tile, "Escena " + std::to_string(scene.number) + ": " + scene.title);
            tile.Click([this, sceneID = scene.id](auto&&, auto&&) { scrollToScene(sceneID); });
            strip.Children().Append(tile);
        }
        muxc::ScrollViewer scroller;
        scroller.HorizontalScrollBarVisibility(muxc::ScrollBarVisibility::Auto);
        scroller.HorizontalScrollMode(muxc::ScrollMode::Enabled);
        scroller.VerticalScrollMode(muxc::ScrollMode::Disabled);
        scroller.VerticalScrollBarVisibility(muxc::ScrollBarVisibility::Disabled);
        scroller.Content(strip);
        panel.Children().Append(scroller);
        muxc::ProgressBar bar;
        bar.Maximum(static_cast<double>(s->scenes.size()));
        bar.Value(ready);
        panel.Children().Append(bar);

        auto actions = ui::hstack(10);
        if (running) {
            muxc::ProgressRing ring;
            ring.Width(18);
            ring.Height(18);
            actions.Children().Append(ring);
            auto text = ui::text(model.storyRun->text, ui::Text::body);
            text.VerticalAlignment(mux::VerticalAlignment::Center);
            actions.Children().Append(text);
            actions.Children().Append(ui::button("Detener", L"", [] { AppModel::shared().cancelStoryRun(); }));
        } else {
            auto all = ui::primaryButton(ready == 0 ? "Generar todas las escenas" : "Generar las que faltan", L"",
                                         [id] { AppModel::shared().generateAllScenes(id); });
            all.IsEnabled(ready < static_cast<int>(s->scenes.size()) && model.hasKieKey && !model.storyRun);
            ui::tooltip(all, s->usesLastFrame() ? "Envía las escenas en orden: cada una espera a la anterior para usar su último fotograma."
                                                : "Envía todas las escenas sin video a Seedance.");
            actions.Children().Append(all);
            bool assembling = model.assemblingStoryID && *model.assemblingStoryID == id;
            auto assemble = ui::button(assembling ? "Armando…" : "Armar video final", L"", [id] { AppModel::shared().assembleStory(id); });
            assemble.IsEnabled(ready == static_cast<int>(s->scenes.size()) && !model.assemblingStoryID);
            ui::tooltip(assemble, ready < static_cast<int>(s->scenes.size())
                                      ? "Disponible cuando todas las escenas tengan video."
                                      : "Une todas las escenas en un solo MP4 (" + std::to_string(s->totalDuration()) + " s), en tu PC y sin gastar créditos.");
            actions.Children().Append(assemble);
            if (s->finalCutJobID && model.job(*s->finalCutJobID)) {
                actions.Children().Append(ui::button("Ver video final", L"", [cut = *s->finalCutJobID] {
                    if (AppModel::shared().openJobDetail) AppModel::shared().openJobDetail(cut);
                }));
            }
        }
        panel.Children().Append(actions);
        ui::setChildren(production_, {ui::card(panel, 18)});
    }

    void buildOverview(fc::Story const& s) {
        auto& model = AppModel::shared();
        if (!s.summary.empty()) {
            auto box = ui::vstack(6);
            box.Children().Append(ui::labeled(L"", "De qué trata"));
            box.Children().Append(ui::text(s.summary));
            detail_.Children().Append(ui::card(box, 18));
        }
        auto order = ui::vstack(6);
        order.Children().Append(ui::labeled(L"", "Orden de referencias — nunca cambiarlo"));
        auto lines = model.displayLines(s);
        if (lines.empty()) order.Children().Append(ui::secondary("Esta historia no usa referencias.", ui::Text::body));
        for (auto const& line : lines) {
            auto text = ui::text(line);
            text.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
            order.Children().Append(text);
        }
        if (!s.referenceOrder.empty()) order.Children().Append(ui::secondary(s.referenceOrder, ui::Text::body));
        order.Children().Append(ui::secondary("El número del @ sigue el orden de la lista, no el nombre del archivo."));
        detail_.Children().Append(ui::card(order, 18));
        if (!s.continuity.empty()) {
            muxc::Expander bible;
            bible.Header(box_value(L"Biblia de continuidad (va idéntica en cada escena)"));
            bible.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
            bible.HorizontalContentAlignment(mux::HorizontalAlignment::Stretch);
            auto text = ui::text(s.continuity);
            text.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
            text.IsTextSelectionEnabled(true);
            bible.Content(text);
            detail_.Children().Append(bible);
        }
    }

    mux::UIElement sceneCard(fc::Story const& s, fc::StoryScene const& scene) {
        auto& model = AppModel::shared();
        std::string id = s.id, sceneID = scene.id;
        auto report = fc::linter::lintVideo(scene.prompt, scene.duration,
                                            {static_cast<int>(s.slotsOf(fc::ReferenceKind::image).size()), static_cast<int>(s.slotsOf(fc::ReferenceKind::video).size()),
                                             static_cast<int>(s.slotsOf(fc::ReferenceKind::audio).size())},
                                            s.skillID != fc::skills::kArthasID);
        auto panel = ui::vstack(12);
        auto header = ui::columns({ui::autoLength(), ui::star(), ui::autoLength()}, 10);
        muxc::Border number;
        number.Width(28);
        number.Height(28);
        number.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(14));
        number.Background(ui::brandGradient());
        auto digit = ui::text(std::to_string(scene.number), ui::Text::bodyStrong);
        digit.Foreground(muxm::SolidColorBrush(ui::rgb(255, 255, 255)));
        digit.HorizontalAlignment(mux::HorizontalAlignment::Center);
        digit.VerticalAlignment(mux::VerticalAlignment::Center);
        number.Child(digit);
        ui::place(header, number, 0);
        auto titles = ui::vstack(1);
        titles.Children().Append(ui::text(scene.title, ui::Text::subtitle));
        std::vector<std::string> tags;
        for (auto kind : fc::kAllReferenceKinds)
            for (int i = 0; i < fc::linter::highestTag(fc::tagPrefix(kind), scene.prompt); ++i) tags.push_back(std::string(fc::tagPrefix(kind)) + std::to_string(i + 1));
        auto meta = ui::secondary(std::to_string(scene.duration) + " s · " + (tags.empty() ? "sin referencias" : fc::join(tags, " ")));
        meta.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
        titles.Children().Append(meta);
        ui::place(header, titles, 1);
        auto chip = ui::hstack(0);
        sceneChips_[sceneID] = chip;
        ui::place(header, chip, 2);
        panel.Children().Append(header);
        fillChip(chip, model.latestJob(scene));

        if (!scene.summary.empty()) panel.Children().Append(ui::text(scene.summary));
        if (scene.notes) panel.Children().Append(ui::secondary("💡 " + *scene.notes));
        auto checks = ui::hstack(14);
        auto state = report.problems() > 0 ? 2 : report.warnings() > 0 ? 1 : 0;
        std::string checkText = report.problems() > 0 ? std::to_string(report.problems()) + (report.problems() == 1 ? " problema" : " problemas")
                                : report.warnings() > 0 ? std::to_string(report.warnings()) + (report.warnings() == 1 ? " sugerencia" : " sugerencias")
                                                        : "Revisión OK";
        auto checkLabel = ui::text(checkText, ui::Text::caption);
        checkLabel.Foreground(state == 2 ? ui::resource(L"SystemFillColorCriticalBrush")
                              : state == 1 ? ui::resource(L"SystemFillColorCautionBrush")
                                           : ui::resource(L"SystemFillColorSuccessBrush"));
        std::vector<std::string> issues;
        for (auto const& item : report.items)
            if (item.state == fc::LintItem::State::warning || item.state == fc::LintItem::State::problem) issues.push_back("• " + item.title);
        if (!issues.empty()) ui::tooltip(checkLabel, fc::join(issues, "\n"));
        checks.Children().Append(checkLabel);
        checks.Children().Append(ui::secondary(std::to_string(report.dialogueWords) + "/" + std::to_string(report.targetWords) + " palabras"));
        panel.Children().Append(checks);

        muxc::Expander promptExpander;
        promptExpander.Header(box_value(hs("Prompt (" + formatNumber(static_cast<double>(fc::characterCount(scene.prompt))) + " caracteres)")));
        promptExpander.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
        promptExpander.HorizontalContentAlignment(mux::HorizontalAlignment::Stretch);
        auto editor = ui::editor("", 260, [id, sceneID](std::string const& text) {
            AppModel::shared().updateScene(id, sceneID, [&](fc::StoryScene& target) { target.prompt = text; });
        }, true);
        editor.Text(hs(scene.prompt));
        editor.LostFocus([](auto&&, auto&&) { AppModel::shared().persist(); });
        ui::accessible(editor, "Prompt de la escena " + std::to_string(scene.number));
        promptExpander.Content(editor);
        panel.Children().Append(promptExpander);

        auto actions = ui::hstack(8);
        actions.Children().Append(ui::button("Copiar", L"", [id, sceneID] {
            auto& m = AppModel::shared();
            if (auto* story = m.story(id))
                for (auto const& target : story->scenes)
                    if (target.id == sceneID) m.copy(target.prompt);
        }));
        auto open = ui::button("Llevar a Crear", L"", [id, sceneID] { AppModel::shared().openScene(id, sceneID, false); });
        ui::tooltip(open, "Carga el prompt, la duración y las referencias en orden en la pantalla Crear");
        actions.Children().Append(open);
        auto generate = ui::button("Generar ahora", L"", [id, sceneID] { AppModel::shared().openScene(id, sceneID, true); });
        generate.IsEnabled(model.hasKieKey);
        ui::tooltip(generate, "Envía la escena a Seedance con sus referencias");
        actions.Children().Append(generate);
        if (auto const* clip = model.finishedClip(scene)) {
            actions.Children().Append(ui::button("Ver video", L"", [cid = clip->id] {
                if (AppModel::shared().openJobDetail) AppModel::shared().openJobDetail(cid);
            }));
        }
        panel.Children().Append(actions);

        auto refine = ui::columns({ui::star(), ui::autoLength()}, 8);
        auto note = ui::field("Ajustar esta escena: ej. “que Walter tarde más en responder”, “sacá la moza”…", {});
        auto send = [id, sceneID, weak = winrt::make_weak(note)] {
            auto box = weak.get();
            if (!box) return;
            std::string text = fc::trim(str(box.Text()));
            if (text.empty()) return;
            box.Text(L"");
            AppModel::shared().refineScene(id, sceneID, text);
        };
        note.KeyDown([send](IInspectable const&, mux::Input::KeyRoutedEventArgs const& args) {
            if (args.Key() == winrt::Windows::System::VirtualKey::Enter) {
                args.Handled(true);
                send();
            }
        });
        ui::place(refine, note, 0);
        auto adjust = ui::button("Ajustar", L"", send);
        adjust.IsEnabled(!model.storyStatus.has_value());
        ui::place(refine, adjust, 1);
        panel.Children().Append(refine);

        auto card = ui::card(panel, 20);
        sceneCards_[sceneID] = card;
        return card;
    }

    static void fillChip(muxc::StackPanel const& chip, fc::Job const* job) {
        chip.Children().Clear();
        if (!job) return;
        std::string text;
        muxm::Brush brush{nullptr};
        if (job->status == fc::JobStatus::success) {
            text = "Video listo";
            brush = ui::resource(L"SystemFillColorSuccessBackgroundBrush");
        } else if (job->status == fc::JobStatus::fail || job->status == fc::JobStatus::unknown) {
            text = "Falló";
            brush = ui::resource(L"SystemFillColorCautionBackgroundBrush");
        } else {
            text = "Generando " + std::to_string(job->progress) + "%";
            brush = ui::softGradient();
        }
        muxc::Border border;
        border.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(10));
        border.Padding(ui::margin(10, 3, 10, 4));
        border.Background(brush);
        border.Child(ui::text(text, ui::Text::caption));
        chip.Children().Append(border);
    }

    void refreshSceneStatus() {
        auto& model = AppModel::shared();
        auto* s = story();
        if (!s) return;
        for (auto const& scene : s->scenes) {
            auto it = sceneChips_.find(scene.id);
            if (it != sceneChips_.end()) fillChip(it->second, model.latestJob(scene));
        }
    }

    void scrollToScene(std::string const& sceneID) {
        auto it = sceneCards_.find(sceneID);
        if (it == sceneCards_.end()) return;
        mux::BringIntoViewOptions options;
        options.AnimationDesired(true);
        options.VerticalAlignmentRatio(0.0);
        it->second.StartBringIntoView(options);
    }

    void buildConversation(fc::Story const& s) {
        std::string id = s.id;
        auto panel = ui::vstack(10);
        panel.Children().Append(ui::labeled(L"", "Cambios a toda la historia"));
        size_t start = s.messages.size() > 8 ? s.messages.size() - 8 : 0;
        for (size_t i = start; i < s.messages.size(); ++i) {
            auto const& message = s.messages[i];
            auto row = ui::columns({ui::autoLength(), ui::star()}, 8);
            auto glyph = ui::icon(message.fromUser ? L"" : L"", 14);
            glyph.Foreground(message.fromUser ? ui::resource(L"TextFillColorSecondaryBrush") : ui::pinkBrush());
            glyph.VerticalAlignment(mux::VerticalAlignment::Top);
            glyph.Margin(ui::margin(0, 3, 0, 0));
            ui::place(row, glyph, 0);
            auto texts = ui::vstack(1);
            if (message.sceneNumber) texts.Children().Append(ui::secondary("Escena " + std::to_string(*message.sceneNumber)));
            texts.Children().Append(ui::text(message.text));
            ui::place(row, texts, 1);
            panel.Children().Append(row);
        }
        auto input = ui::columns({ui::star(), ui::autoLength()}, 8);
        auto box = ui::field("Ej.: que el arco termine en la escena 6, más humor en la 2, cambiá el diner por una pizzería…", {});
        ui::place(input, box, 0);
        auto send = ui::primaryButton("Enviar", L"", [id, weak = winrt::make_weak(box)] {
            auto target = weak.get();
            if (!target) return;
            std::string text = fc::trim(str(target.Text()));
            if (text.empty()) return;
            target.Text(L"");
            AppModel::shared().generateStory(id, text);
        });
        send.IsEnabled(!AppModel::shared().storyStatus.has_value());
        ui::place(input, send, 1);
        panel.Children().Append(input);
        panel.Children().Append(ui::secondary("Para cambiar una sola escena usá “Ajustar” dentro de esa escena: es más rápido y no toca las demás."));
        detail_.Children().Append(ui::card(panel, 20));
    }

    muxc::Grid root_{nullptr};
    muxc::ListView list_{nullptr};
    muxc::ScrollViewer detailScroll_{nullptr};
    muxc::StackPanel detail_{nullptr}, briefHost_{nullptr}, progress_{nullptr}, production_{nullptr};
    std::map<std::string, muxc::StackPanel> sceneChips_;
    std::map<std::string, muxc::Border> sceneCards_;
    bool updating_ = false;
};

}  // namespace

std::unique_ptr<Page> makeStoriesPage() { return std::make_unique<StoriesPage>(); }

}  // namespace fcapp
