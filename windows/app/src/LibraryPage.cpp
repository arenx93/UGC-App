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

/// Lets a card be dragged out to Explorer or other apps as a file.
void makeDraggable(mux::UIElement const& element, fs::path file) {
    element.CanDrag(true);
    element.DragStarting([file](mux::UIElement const&, mux::DragStartingEventArgs args) -> winrt::fire_and_forget {
        auto deferral = args.GetDeferral();
        try {
            auto storage = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(file.wstring());
            auto items = winrt::single_threaded_vector<winrt::Windows::Storage::IStorageItem>({storage});
            args.Data().SetStorageItems(items);
            args.Data().RequestedOperation(winrt::Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
        } catch (...) {
        }
        deferral.Complete();
    });
}

muxc::MenuFlyoutItem menuItem(std::string const& label, std::wstring const& glyph, std::function<void()> action) {
    muxc::MenuFlyoutItem item;
    item.Text(hs(label));
    if (!glyph.empty()) item.Icon(ui::icon(glyph));
    item.Click([action](auto&&, auto&&) {
        if (action) action();
    });
    return item;
}

class LibraryPage : public Page {
public:
    LibraryPage() {
        root_ = muxc::Grid();
        muxc::RowDefinition headerRow;
        headerRow.Height(ui::autoLength());
        root_.RowDefinitions().Append(headerRow);
        root_.RowDefinitions().Append(muxc::RowDefinition());

        auto header = ui::vstack(14);
        header.Padding(ui::margin(32, 24, 32, 8));
        auto top = ui::columns({ui::star(), ui::pixels(300), ui::autoLength()}, 12);
        auto titles = ui::vstack(2);
        titles.Children().Append(ui::gradientTitle("Biblioteca", 30));
        subtitle_ = ui::secondary("", ui::Text::body);
        titles.Children().Append(subtitle_);
        ui::place(top, titles, 0);
        muxc::AutoSuggestBox search;
        search.PlaceholderText(L"Buscar por prompt o modelo");
        search.QueryIcon(muxc::SymbolIcon(muxc::Symbol::Find));
        search.VerticalAlignment(mux::VerticalAlignment::Center);
        search.TextChanged([](muxc::AutoSuggestBox const& sender, auto&&) {
            auto& m = AppModel::shared();
            m.search = str(sender.Text());
            m.notify(Change::jobs);
        });
        ui::place(top, search, 1);
        auto folder = ui::button("Abrir carpeta", L"", [] { openPath(AppModel::shared().mediaRoot); });
        folder.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::tooltip(folder, "Abrir Imágenes ▸ Framecraft en el Explorador");
        ui::place(top, folder, 2);
        header.Children().Append(top);
        filters_ = ui::hstack(0);
        header.Children().Append(filters_);
        ui::place(root_, header, 0, 0);

        scroll_ = muxc::ScrollViewer();
        grid_ = muxc::VariableSizedWrapGrid();
        grid_.Orientation(muxc::Orientation::Horizontal);
        grid_.ItemWidth(276);
        grid_.ItemHeight(348);
        grid_.Margin(ui::margin(32, 8, 20, 24));
        body_ = ui::vstack(12);
        body_.Children().Append(grid_);
        scroll_.Content(body_);
        ui::place(root_, scroll_, 0, 1);
    }

    mux::UIElement root() override { return root_; }

    void refresh(Change change) override {
        if (change != Change::all && change != Change::jobs && change != Change::navigation) return;
        auto& model = AppModel::shared();
        subtitle_.Text(hs(std::to_string(model.completedCount()) + " creaciones · se guardan en Imágenes ▸ Framecraft"));
        std::vector<std::string> names;
        for (auto filter : {LibraryFilter::all, LibraryFilter::images, LibraryFilter::videos, LibraryFilter::active, LibraryFilter::favorites, LibraryFilter::failed})
            names.push_back(filterTitle(filter));
        ui::setChildren(filters_, {ui::segmented(names, static_cast<int>(model.filter), [](int index) {
                                       auto& m = AppModel::shared();
                                       m.filter = static_cast<LibraryFilter>(index);
                                       m.notify(Change::jobs);
                                   })});
        grid_.Children().Clear();
        if (body_.Children().Size() > 1) body_.Children().RemoveAt(1);
        if (model.jobs.empty()) {
            body_.Children().Append(emptyState("Tu biblioteca está vacía", "Todo lo que generes aparece acá y se guarda en Imágenes ▸ Framecraft.",
                                               "Crear mi primera pieza", [] { AppModel::shared().go(Section::create); }));
            return;
        }
        auto jobs = model.filteredJobs();
        if (jobs.empty()) {
            body_.Children().Append(emptyState("Nada por acá", "No hay creaciones que coincidan con el filtro o la búsqueda.", "", {}));
            return;
        }
        size_t shown = 0;
        for (auto const& job : jobs) {
            if (++shown > limit_) break;
            grid_.Children().Append(card(job));
        }
        if (jobs.size() > limit_) {
            auto more = ui::button("Mostrar más (" + std::to_string(jobs.size() - limit_) + ")", L"", [this] {
                limit_ += 120;
                refresh(Change::jobs);
            });
            more.HorizontalAlignment(mux::HorizontalAlignment::Center);
            body_.Children().Append(more);
        }
    }

private:
    static mux::UIElement emptyState(std::string const& title, std::string const& message, std::string const& action, std::function<void()> onAction) {
        auto panel = ui::vstack(10);
        panel.HorizontalAlignment(mux::HorizontalAlignment::Center);
        panel.Margin(ui::margin(0, 80, 0, 0));
        auto glyph = ui::glyphBadge(L"", 64);
        glyph.HorizontalAlignment(mux::HorizontalAlignment::Center);
        panel.Children().Append(glyph);
        auto heading = ui::text(title, ui::Text::subtitle);
        heading.HorizontalAlignment(mux::HorizontalAlignment::Center);
        panel.Children().Append(heading);
        auto detail = ui::secondary(message, ui::Text::body);
        detail.HorizontalAlignment(mux::HorizontalAlignment::Center);
        detail.TextAlignment(mux::TextAlignment::Center);
        panel.Children().Append(detail);
        if (!action.empty()) {
            auto b = ui::primaryButton(action, L"", onAction);
            b.HorizontalAlignment(mux::HorizontalAlignment::Center);
            panel.Children().Append(b);
        }
        return panel;
    }

    mux::UIElement card(fc::Job const& job) {
        auto& model = AppModel::shared();
        auto paths = model.outputPaths(job);
        bool video = job.kind == fc::MediaKind::video;
        muxc::Grid frame;
        frame.Margin(ui::margin(0, 0, 14, 14));
        frame.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(14));
        frame.Background(ui::resource(L"CardBackgroundFillColorDefaultBrush"));
        frame.BorderBrush(ui::resource(L"CardStrokeColorDefaultBrush"));
        frame.BorderThickness(ui::uniform(1));
        muxc::RowDefinition mediaRow;
        mediaRow.Height(ui::pixels(230));
        frame.RowDefinitions().Append(mediaRow);
        frame.RowDefinitions().Append(muxc::RowDefinition());

        muxc::Grid media;
        media.Background(muxm::SolidColorBrush(ui::rgb(20, 20, 24)));
        switch (job.status) {
        case fc::JobStatus::success:
            if (!paths.empty() && fs::exists(paths.front())) {
                media.Children().Append(thumbnail(paths.front(), video, 520, 0, 230));
                if (video) {
                    muxc::Border play;
                    play.Width(50);
                    play.Height(50);
                    play.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(25));
                    play.Background(ui::resource(L"AcrylicInAppFillColorDefaultBrush"));
                    auto glyph = ui::icon(L"", 20);
                    play.Child(glyph);
                    play.HorizontalAlignment(mux::HorizontalAlignment::Center);
                    play.VerticalAlignment(mux::VerticalAlignment::Center);
                    media.Children().Append(play);
                }
                if (job.outputs.size() > 1) {
                    auto count = ui::pill(std::to_string(job.outputs.size()), false);
                    count.HorizontalAlignment(mux::HorizontalAlignment::Right);
                    count.VerticalAlignment(mux::VerticalAlignment::Bottom);
                    count.Margin(ui::uniform(8));
                    media.Children().Append(count);
                }
            } else {
                media.Children().Append(placeholder(L"", "Archivo no encontrado", "Puede que se haya movido de la carpeta.", false));
            }
            break;
        case fc::JobStatus::fail:
            media.Children().Append(placeholder(L"", "La generación falló", job.error.value_or(""), true));
            break;
        case fc::JobStatus::unknown:
            media.Children().Append(placeholder(L"", "Revisá el envío", job.error.value_or(""), true));
            break;
        default: {
            media.Background(ui::softGradient());
            auto progress = ui::vstack(10);
            progress.HorizontalAlignment(mux::HorizontalAlignment::Center);
            progress.VerticalAlignment(mux::VerticalAlignment::Center);
            muxc::ProgressRing ring;
            ring.IsIndeterminate(job.progress <= 2);
            ring.Value(job.progress);
            ring.Width(56);
            ring.Height(56);
            progress.Children().Append(ring);
            auto percent = ui::text(std::to_string(job.progress) + "%", ui::Text::bodyStrong);
            percent.HorizontalAlignment(mux::HorizontalAlignment::Center);
            progress.Children().Append(percent);
            std::string title = job.status == fc::JobStatus::submitting ? "Enviando…"
                                : job.status == fc::JobStatus::queued   ? "En cola…"
                                : job.status == fc::JobStatus::saving   ? "Guardando…"
                                : video                                  ? "Creando tu video…"
                                                                         : "Creando tu imagen…";
            auto label = ui::text(title, ui::Text::bodyStrong);
            label.HorizontalAlignment(mux::HorizontalAlignment::Center);
            progress.Children().Append(label);
            auto hint = ui::secondary(job.error.value_or("Podés seguir trabajando mientras tanto."));
            hint.HorizontalAlignment(mux::HorizontalAlignment::Center);
            hint.TextAlignment(mux::TextAlignment::Center);
            hint.MaxWidth(220);
            progress.Children().Append(hint);
            media.Children().Append(progress);
            break;
        }
        }
        auto kind = ui::glyphBadge(kindGlyph(job.kind), 26, false);
        kind.HorizontalAlignment(mux::HorizontalAlignment::Left);
        kind.VerticalAlignment(mux::VerticalAlignment::Top);
        kind.Margin(ui::uniform(8));
        media.Children().Append(kind);
        if (job.favorite) {
            auto heart = ui::icon(L"", 16);
            heart.Foreground(ui::pinkBrush());
            heart.HorizontalAlignment(mux::HorizontalAlignment::Right);
            heart.VerticalAlignment(mux::VerticalAlignment::Top);
            heart.Margin(ui::uniform(12));
            media.Children().Append(heart);
        }
        ui::place(frame, media, 0, 0);

        auto info = ui::vstack(4);
        info.Padding(ui::margin(12, 10, 12, 10));
        auto line = ui::columns({ui::autoLength(), ui::star(), ui::autoLength()}, 6);
        ui::place(line, ui::text(fc::presets::modelName(job.model), ui::Text::caption), 0);
        std::string details = job.settings.resolution + " · " + job.settings.aspect;
        if (job.settings.duration) details += " · " + std::to_string(*job.settings.duration) + " s";
        auto detailText = ui::secondary(details);
        detailText.TextWrapping(mux::TextWrapping::NoWrap);
        detailText.TextTrimming(mux::TextTrimming::CharacterEllipsis);
        ui::place(line, detailText, 1);
        ui::place(line, ui::secondary(relativeTime(job.created)), 2);
        info.Children().Append(line);
        auto prompt = ui::secondary(job.prompt);
        prompt.MaxLines(2);
        prompt.TextTrimming(mux::TextTrimming::CharacterEllipsis);
        info.Children().Append(prompt);
        ui::place(frame, info, 0, 1);

        muxc::Button button;
        button.Padding(ui::uniform(0));
        button.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(14));
        button.BorderThickness(ui::uniform(0));
        button.Background(muxm::SolidColorBrush(ui::rgb(0, 0, 0, 0)));
        button.HorizontalContentAlignment(mux::HorizontalAlignment::Stretch);
        button.VerticalContentAlignment(mux::VerticalAlignment::Stretch);
        button.Content(frame);
        button.Width(262);
        button.Height(334);
        ui::accessible(button, std::string(video ? "Video" : "Imagen") + ", " + jobStatusText(job) + ". " + job.prompt);
        button.Click([id = job.id](auto&&, auto&&) {
            if (AppModel::shared().openJobDetail) AppModel::shared().openJobDetail(id);
        });
        button.ContextFlyout(menu(job));
        if (job.status == fc::JobStatus::success && !paths.empty()) makeDraggable(button, paths.front());
        return button;
    }

    static mux::UIElement placeholder(std::wstring const& glyph, std::string const& title, std::string const& message, bool caution) {
        auto panel = ui::vstack(6);
        panel.HorizontalAlignment(mux::HorizontalAlignment::Center);
        panel.VerticalAlignment(mux::VerticalAlignment::Center);
        panel.Padding(ui::uniform(16));
        auto symbol = ui::icon(glyph, 28);
        symbol.Foreground(caution ? ui::resource(L"SystemFillColorCautionBrush") : ui::resource(L"TextFillColorSecondaryBrush"));
        panel.Children().Append(symbol);
        auto heading = ui::text(title, ui::Text::bodyStrong);
        heading.Foreground(muxm::SolidColorBrush(ui::rgb(255, 255, 255)));
        heading.HorizontalAlignment(mux::HorizontalAlignment::Center);
        panel.Children().Append(heading);
        auto detail = ui::text(message, ui::Text::caption);
        detail.Foreground(muxm::SolidColorBrush(ui::rgb(200, 200, 205)));
        detail.TextAlignment(mux::TextAlignment::Center);
        detail.MaxLines(4);
        panel.Children().Append(detail);
        return panel;
    }

    static muxc::MenuFlyout menu(fc::Job const& job) {
        auto& model = AppModel::shared();
        auto paths = model.outputPaths(job);
        muxc::MenuFlyout flyout;
        std::string id = job.id;
        if (job.status == fc::JobStatus::success) {
            flyout.Items().Append(menuItem("Ver en grande", L"", [id] { AppModel::shared().openJobDetail(id); }));
            flyout.Items().Append(menuItem("Mostrar en la carpeta", L"", [paths] { revealInExplorer(paths); }));
            if (job.kind == fc::MediaKind::video) {
                flyout.Items().Append(menuItem("Continuar desde el último fotograma", L"", [id] { AppModel::shared().continueFromLastFrame(id); }));
            } else {
                flyout.Items().Append(menuItem("Usar como referencia", L"", [id] { AppModel::shared().useAsReference(id); }));
            }
            flyout.Items().Append(muxc::MenuFlyoutSeparator());
        }
        if (job.model != fc::presets::kStoryCutModelID)
            flyout.Items().Append(menuItem("Reusar ajustes y prompt", L"", [id] { AppModel::shared().reuse(id); }));
        flyout.Items().Append(menuItem("Copiar prompt", L"", [prompt = job.prompt] { AppModel::shared().copy(prompt); }));
        if (job.status == fc::JobStatus::success)
            flyout.Items().Append(menuItem(job.favorite ? "Quitar de favoritos" : "Marcar como favorito", job.favorite ? L"" : L"",
                                           [id] { AppModel::shared().toggleFavorite(id); }));
        if (fc::isFinished(job.status)) {
            flyout.Items().Append(muxc::MenuFlyoutSeparator());
            auto remove = menuItem("Eliminar…", L"", {});
            remove.Click([id](IInspectable const& sender, auto&&) {
                auto root = sender.as<mux::UIElement>().XamlRoot();
                ui::confirm(root, "¿Eliminar esta generación?", "Se borran los archivos de tu biblioteca. No se reintegran créditos de KIE.",
                            "Eliminar", [id] { AppModel::shared().deleteJob(id); });
            });
            flyout.Items().Append(remove);
        }
        return flyout;
    }

    muxc::Grid root_{nullptr};
    muxc::ScrollViewer scroll_{nullptr};
    muxc::StackPanel body_{nullptr}, filters_{nullptr};
    muxc::VariableSizedWrapGrid grid_{nullptr};
    muxc::TextBlock subtitle_{nullptr};
    size_t limit_ = 120;
};

// MARK: - References

class ReferencesPage : public Page {
public:
    ReferencesPage() {
        scroll_ = muxc::ScrollViewer();
        auto content = ui::vstack(18);
        content.Padding(ui::margin(32, 24, 32, 32));
        scroll_.Content(content);
        auto top = ui::columns({ui::star(), ui::autoLength()}, 12);
        auto titles = ui::vstack(2);
        titles.Children().Append(ui::gradientTitle("Referencias", 30));
        titles.Children().Append(ui::secondary("Fotos, videos (≤30 s) y audios (≤30 s) para tus generaciones. Arrastralos acá o a la pantalla Crear.", ui::Text::body));
        ui::place(top, titles, 0);
        auto add = ui::primaryButton("Agregar archivos", L"", [] { AppModel::shared().pickAndImport(); });
        add.VerticalAlignment(mux::VerticalAlignment::Center);
        ui::place(top, add, 1);
        content.Children().Append(top);
        sections_ = ui::vstack(18);
        content.Children().Append(sections_);

        scroll_.AllowDrop(true);
        scroll_.DragOver([](IInspectable const&, mux::DragEventArgs const& args) {
            if (args.DataView().Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
                args.AcceptedOperation(winrt::Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
        });
        scroll_.Drop([](IInspectable const&, mux::DragEventArgs args) -> winrt::fire_and_forget {
            if (!args.DataView().Contains(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems())) co_return;
            auto deferral = args.GetDeferral();
            auto items = co_await args.DataView().GetStorageItemsAsync();
            deferral.Complete();
            std::vector<fs::path> files;
            for (auto const& item : items) files.push_back(fs::path(std::wstring(item.Path())));
            AppModel::shared().importFiles(files, false);
        });
    }

    mux::UIElement root() override { return scroll_; }

    void refresh(Change change) override {
        if (change != Change::all && change != Change::references && change != Change::form) return;
        auto& model = AppModel::shared();
        std::vector<mux::UIElement> sections;
        if (model.references.empty()) {
            muxc::Border drop;
            drop.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(16));
            drop.BorderBrush(ui::pinkBrush());
            drop.BorderThickness(ui::uniform(1.5));
            drop.Background(ui::softGradient());
            drop.Padding(ui::uniform(40));
            auto panel = ui::vstack(10);
            panel.HorizontalAlignment(mux::HorizontalAlignment::Center);
            auto badge = ui::glyphBadge(L"", 56);
            badge.HorizontalAlignment(mux::HorizontalAlignment::Center);
            panel.Children().Append(badge);
            auto heading = ui::text("Arrastrá tus archivos acá", ui::Text::subtitle);
            heading.HorizontalAlignment(mux::HorizontalAlignment::Center);
            panel.Children().Append(heading);
            auto detail = ui::secondary("PNG, JPG, WebP, MP4, MOV, MKV, MP3, WAV, AAC, M4A u OGG", ui::Text::body);
            detail.HorizontalAlignment(mux::HorizontalAlignment::Center);
            panel.Children().Append(detail);
            drop.Child(panel);
            sections.push_back(drop);
        }
        for (auto kind : fc::kAllReferenceKinds) {
            auto list = model.referencesOf(kind);
            if (list.empty()) continue;
            std::string title = kind == fc::ReferenceKind::image ? "Imágenes" : kind == fc::ReferenceKind::video ? "Videos" : "Audios";
            auto header = ui::hstack(8);
            header.Children().Append(ui::text(title, ui::Text::subtitle));
            header.Children().Append(ui::pill(std::to_string(list.size()), false));
            sections.push_back(header);
            muxc::VariableSizedWrapGrid grid;
            grid.Orientation(muxc::Orientation::Horizontal);
            grid.ItemWidth(186);
            grid.ItemHeight(214);
            for (auto const& r : list) grid.Children().Append(tile(r));
            sections.push_back(grid);
        }
        ui::setChildren(sections_, sections);
    }

private:
    static mux::UIElement tile(fc::ReferenceFile const& r) {
        auto& model = AppModel::shared();
        auto path = model.referencePath(r);
        auto tag = model.tag(r);
        muxc::Grid frame;
        frame.Width(172);
        frame.Height(200);
        frame.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(12));
        frame.Background(ui::resource(L"CardBackgroundFillColorDefaultBrush"));
        frame.BorderBrush(tag ? ui::pinkBrush() : ui::resource(L"CardStrokeColorDefaultBrush"));
        frame.BorderThickness(ui::uniform(tag ? 2 : 1));
        muxc::RowDefinition mediaRow;
        mediaRow.Height(ui::pixels(140));
        frame.RowDefinitions().Append(mediaRow);
        frame.RowDefinitions().Append(muxc::RowDefinition());
        muxc::Grid media;
        if (r.kind == fc::ReferenceKind::audio) {
            media.Background(ui::softGradient());
            auto wave = ui::icon(L"", 36);
            wave.Foreground(ui::pinkBrush());
            media.Children().Append(wave);
        } else {
            media.Children().Append(thumbnail(path, r.kind == fc::ReferenceKind::video, 360, 0, 140));
        }
        if (tag) {
            auto badge = ui::pill(*tag);
            badge.HorizontalAlignment(mux::HorizontalAlignment::Left);
            badge.VerticalAlignment(mux::VerticalAlignment::Top);
            badge.Margin(ui::uniform(8));
            media.Children().Append(badge);
        }
        ui::place(frame, media, 0, 0);
        auto info = ui::vstack(2);
        info.Padding(ui::margin(10, 8, 10, 8));
        auto name = ui::text(r.name, ui::Text::caption);
        name.TextWrapping(mux::TextWrapping::NoWrap);
        name.TextTrimming(mux::TextTrimming::CharacterEllipsis);
        info.Children().Append(name);
        std::string meta = std::string(r.kind == fc::ReferenceKind::image ? "Imagen" : r.kind == fc::ReferenceKind::video ? "Video" : "Audio");
        if (r.durationMs) meta += " · " + fc::media::formattedDuration(r.durationSeconds());
        meta += " · " + formatNumber(r.bytes / 1024.0 / 1024.0, 1) + " MB";
        info.Children().Append(ui::secondary(meta));
        ui::place(frame, info, 0, 1);
        ui::tooltip(frame, r.name);
        ui::accessible(frame, r.name + ", " + meta);

        muxc::MenuFlyout flyout;
        flyout.Items().Append(menuItem(tag ? "Quitar de Crear" : "Usar en Crear", L"", [r] { AppModel::shared().toggleSelection(r); }));
        flyout.Items().Append(menuItem("Abrir", L"", [path] { openPath(path); }));
        flyout.Items().Append(menuItem("Mostrar en la carpeta", L"", [path] { revealInExplorer({path}); }));
        flyout.Items().Append(muxc::MenuFlyoutSeparator());
        auto remove = menuItem("Eliminar…", L"", {});
        remove.Click([id = r.id](IInspectable const& sender, auto&&) {
            auto root = sender.as<mux::UIElement>().XamlRoot();
            ui::confirm(root, "¿Eliminar esta referencia?", "Se borra el archivo de tu biblioteca de referencias.", "Eliminar",
                        [id] { AppModel::shared().deleteReferences({id}); });
        });
        flyout.Items().Append(remove);
        frame.ContextFlyout(flyout);
        frame.DoubleTapped([r](auto&&, auto&&) { AppModel::shared().toggleSelection(r); });
        makeDraggable(frame, path);
        return frame;
    }

    muxc::ScrollViewer scroll_{nullptr};
    muxc::StackPanel sections_{nullptr};
};

}  // namespace

std::unique_ptr<Page> makeLibraryPage() { return std::make_unique<LibraryPage>(); }
std::unique_ptr<Page> makeReferencesPage() { return std::make_unique<ReferencesPage>(); }

}  // namespace fcapp
