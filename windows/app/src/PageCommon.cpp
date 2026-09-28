#include "pch.h"

#include "Media.h"
#include "Pages.h"
#include "WinUtil.h"
#include "framecraft/util.h"

using namespace winrt;
namespace mux = winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxm = winrt::Microsoft::UI::Xaml::Media;
namespace wf = winrt::Windows::Foundation;
namespace fs = std::filesystem;

namespace fcapp {

std::string jobStatusText(fc::Job const& job) {
    switch (job.status) {
    case fc::JobStatus::success: return "lista";
    case fc::JobStatus::fail: return "falló";
    case fc::JobStatus::unknown: return "hay que revisar el envío";
    case fc::JobStatus::submitting: return "enviando";
    case fc::JobStatus::queued: return "en cola";
    case fc::JobStatus::saving: return "guardando";
    case fc::JobStatus::generating: return "generando " + std::to_string(job.progress) + "%";
    }
    return "";
}

std::wstring kindGlyph(fc::MediaKind kind) { return kind == fc::MediaKind::video ? L"" : L""; }

std::wstring referenceGlyph(fc::ReferenceKind kind) {
    switch (kind) {
    case fc::ReferenceKind::image: return L"";
    case fc::ReferenceKind::video: return L"";
    case fc::ReferenceKind::audio: return L"";
    }
    return L"";
}

// MARK: - Thumbnails

namespace {
std::map<std::wstring, muxm::ImageSource>& thumbnailCache() {
    static std::map<std::wstring, muxm::ImageSource> cache;
    return cache;
}

winrt::fire_and_forget loadThumbnail(winrt::weak_ref<muxc::Image> weakImage, fs::path file, bool video, int pixels) {
    std::wstring key = file.wstring() + L"#" + std::to_wstring(pixels);
    auto& cache = thumbnailCache();
    muxm::ImageSource source{nullptr};
    if (auto it = cache.find(key); it != cache.end()) {
        source = it->second;
    } else {
        source = co_await media::thumbnail(file, video, pixels);
        if (source) cache[key] = source;
    }
    if (auto image = weakImage.get()) {
        if (source) {
            image.Source(source);
            image.Opacity(1);
        }
    }
}
}  // namespace

muxc::Grid thumbnail(fs::path const& file, bool video, int pixels, double width, double height) {
    muxc::Grid grid;
    if (width > 0) grid.Width(width);
    if (height > 0) grid.Height(height);
    grid.Background(ui::resource(L"ControlAltFillColorTertiaryBrush"));
    auto placeholder = ui::icon(video ? L"" : L"", 22);
    placeholder.Foreground(ui::resource(L"TextFillColorTertiaryBrush"));
    placeholder.HorizontalAlignment(mux::HorizontalAlignment::Center);
    placeholder.VerticalAlignment(mux::VerticalAlignment::Center);
    grid.Children().Append(placeholder);
    muxc::Image image;
    image.Stretch(muxm::Stretch::UniformToFill);
    image.HorizontalAlignment(mux::HorizontalAlignment::Center);
    image.VerticalAlignment(mux::VerticalAlignment::Center);
    image.Opacity(0);
    grid.Children().Append(image);
    loadThumbnail(winrt::make_weak(image), file, video, pixels);
    return grid;
}

namespace {
winrt::fire_and_forget attachVideo(winrt::weak_ref<muxc::MediaPlayerElement> weakPlayer, fs::path file, bool autoplay) {
    try {
        auto storage = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(file.wstring());
        if (auto player = weakPlayer.get()) {
            player.Source(winrt::Windows::Media::Core::MediaSource::CreateFromStorageFile(storage));
            if (auto media = player.MediaPlayer()) {
                media.IsLoopingEnabled(true);
                if (autoplay) media.Play();
            }
        }
    } catch (...) {
    }
}
}  // namespace

mux::UIElement mediaView(fs::path const& file, bool video, bool autoplay) {
    if (!video) {
        muxc::Image image;
        image.Stretch(muxm::Stretch::Uniform);
        muxm::Imaging::BitmapImage bitmap;
        bitmap.UriSource(fileUri(file));
        image.Source(bitmap);
        return image;
    }
    muxc::MediaPlayerElement player;
    player.AreTransportControlsEnabled(true);
    player.AutoPlay(autoplay);
    player.Stretch(muxm::Stretch::Uniform);
    attachVideo(winrt::make_weak(player), file, autoplay);
    return player;
}

// MARK: - Dialogs

namespace {
void sizeDialog(muxc::ContentDialog const& dialog, double width, double height) {
    dialog.Resources().Insert(box_value(L"ContentDialogMaxWidth"), box_value(width));
    dialog.Resources().Insert(box_value(L"ContentDialogMaxHeight"), box_value(height));
}

mux::UIElement row(std::string const& label, std::string const& value) {
    auto grid = ui::columns({ui::pixels(110), ui::star()}, 8);
    ui::place(grid, ui::secondary(label, ui::Text::body), 0);
    ui::place(grid, ui::text(value), 1);
    return grid;
}
}  // namespace

winrt::fire_and_forget showMediaViewer(mux::XamlRoot root, fs::path file, bool video) {
    muxc::Grid frame;
    frame.Width(900);
    frame.Height(600);
    frame.Background(muxm::SolidColorBrush(ui::rgb(0, 0, 0)));
    frame.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(8));
    frame.Children().Append(mediaView(file, video, true));
    auto dialog = ui::dialog(root, narrow(file.filename().wstring()), frame, "Mostrar en la carpeta", "Cerrar");
    sizeDialog(dialog, 1000, 800);
    auto result = co_await dialog.ShowAsync();
    if (result == muxc::ContentDialogResult::Primary) revealInExplorer({file});
}

winrt::fire_and_forget showJobDetail(mux::XamlRoot root, std::string jobID) {
    auto& model = AppModel::shared();
    auto* found = model.job(jobID);
    if (!found) co_return;
    fc::Job job = *found;
    auto paths = model.outputPaths(job);
    bool video = job.kind == fc::MediaKind::video;

    auto layout = ui::columns({ui::star(), ui::pixels(320)}, 20);
    muxc::Grid media;
    media.MinHeight(520);
    media.CornerRadius(mux::CornerRadiusHelper::FromUniformRadius(10));
    media.Background(muxm::SolidColorBrush(ui::rgb(0, 0, 0)));
    if (job.status == fc::JobStatus::success && !paths.empty()) {
        media.Children().Append(mediaView(paths.front(), video, true));
    } else {
        auto message = ui::vstack(8);
        message.HorizontalAlignment(mux::HorizontalAlignment::Center);
        message.VerticalAlignment(mux::VerticalAlignment::Center);
        muxc::ProgressRing ring;
        ring.IsActive(fc::isActive(job.status));
        message.Children().Append(ring);
        auto status = ui::text(fc::isActive(job.status) ? "Generando… " + std::to_string(job.progress) + "%" : "Sin archivo", ui::Text::bodyStrong);
        status.Foreground(muxm::SolidColorBrush(ui::rgb(255, 255, 255)));
        message.Children().Append(status);
        media.Children().Append(message);
    }
    ui::place(layout, media, 0);

    auto side = ui::vstack(12);
    side.Children().Append(ui::text(fc::presets::modelName(job.model), ui::Text::subtitle));
    side.Children().Append(ui::secondary(formatDateTime(job.created)));
    auto details = ui::vstack(4);
    details.Children().Append(row("Resolución", job.settings.resolution));
    details.Children().Append(row("Formato", job.settings.aspect));
    if (job.settings.duration) details.Children().Append(row("Duración", std::to_string(*job.settings.duration) + " s"));
    if (job.settings.generateAudio) details.Children().Append(row("Audio", *job.settings.generateAudio ? "Sí" : "No"));
    size_t refs = job.settings.imageReferences.size() + job.settings.videoReferences.size() + job.settings.audioReferences.size();
    if (refs) details.Children().Append(row("Referencias", std::to_string(refs)));
    side.Children().Append(details);
    auto promptHeader = ui::columns({ui::star(), ui::autoLength()});
    ui::place(promptHeader, ui::text("Prompt", ui::Text::bodyStrong), 0);
    ui::place(promptHeader, ui::subtleButton("Copiar", L"", [prompt = job.prompt] { AppModel::shared().copy(prompt); }), 1);
    side.Children().Append(promptHeader);
    muxc::ScrollViewer promptScroll;
    promptScroll.MaxHeight(200);
    auto promptText = ui::text(job.prompt);
    promptText.IsTextSelectionEnabled(true);
    promptScroll.Content(promptText);
    side.Children().Append(promptScroll);
    if (job.error) {
        auto error = ui::text(*job.error, ui::Text::caption);
        error.Foreground(ui::resource(L"SystemFillColorCautionBrush"));
        side.Children().Append(error);
    }

    auto dialog = ui::dialog(root, "Detalle", layout, "", "Cerrar");
    sizeDialog(dialog, 1200, 900);
    auto hide = [weak = winrt::make_weak(dialog)] {
        if (auto d = weak.get()) d.Hide();
    };
    auto actions = ui::vstack(8);
    if (job.model != fc::presets::kStoryCutModelID) {
        auto reuse = ui::primaryButton("Reusar ajustes y prompt", L"", [hide, id = job.id] {
            hide();
            AppModel::shared().reuse(id);
        });
        reuse.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
        actions.Children().Append(reuse);
    }
    auto add = [&](std::string const& label, std::wstring const& glyph, std::function<void()> action) {
        auto b = ui::button(label, glyph, action);
        b.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
        b.HorizontalContentAlignment(mux::HorizontalAlignment::Left);
        actions.Children().Append(b);
    };
    if (job.status == fc::JobStatus::success) {
        if (video) {
            add("Continuar desde el último fotograma", L"", [hide, id = job.id] {
                hide();
                AppModel::shared().continueFromLastFrame(id);
            });
        } else {
            add("Usar como referencia", L"", [hide, id = job.id] {
                hide();
                AppModel::shared().useAsReference(id);
            });
        }
        add("Mostrar en la carpeta", L"", [paths] { revealInExplorer(paths); });
        add("Abrir con la app de Windows", L"", [paths] { openPath(paths.front()); });
        add(job.favorite ? "Quitar de favoritos" : "Marcar como favorito", job.favorite ? L"" : L"",
            [id = job.id] { AppModel::shared().toggleFavorite(id); });
    }
    if (fc::isFinished(job.status)) {
        add("Eliminar", L"", [hide, root, id = job.id] {
            hide();
            ui::confirm(root, "¿Eliminar esta generación?", "Se borran los archivos de tu biblioteca. No se reintegran créditos de KIE.",
                        "Eliminar", [id] { AppModel::shared().deleteJob(id); });
        });
    }
    side.Children().Append(actions);
    muxc::ScrollViewer sideScroll;
    sideScroll.Content(side);
    ui::place(layout, sideScroll, 1);
    co_await dialog.ShowAsync();
}

mux::UIElement onboardingContent(std::function<void()> const& onDone) {
    auto panel = ui::vstack(16);
    auto top = ui::hstack(14);
    muxc::Image logo;
    logo.Width(64);
    logo.Height(64);
    logo.Source(muxm::Imaging::BitmapImage(fileUri(executableDirectory() / L"Assets" / L"AppIcon.png")));
    top.Children().Append(logo);
    auto titles = ui::vstack(2);
    titles.Children().Append(ui::gradientTitle("Bienvenido a Framecraft", 28));
    titles.Children().Append(ui::secondary("Fotos y videos UGC que parecen grabados con un celular real.", ui::Text::body));
    top.Children().Append(titles);
    panel.Children().Append(top);

    auto steps = ui::vstack(10);
    auto step = [&](std::wstring const& glyph, std::string const& title, std::string const& detail) {
        auto rowGrid = ui::columns({ui::autoLength(), ui::star()}, 12);
        ui::place(rowGrid, ui::glyphBadge(glyph, 34), 0);
        auto labels = ui::vstack(2);
        labels.Children().Append(ui::text(title, ui::Text::bodyStrong));
        labels.Children().Append(ui::secondary(detail));
        ui::place(rowGrid, labels, 1);
        steps.Children().Append(rowGrid);
    };
    step(L"", "1. Conectá tu clave de KIE", "Con ella generás imágenes (GPT Image 2, Nano Banana Pro) y videos (Seedance 2.5). Se guarda en el Administrador de credenciales de Windows.");
    step(L"", "2. Contale tu idea al asistente", "Te escribe el prompt con el método UGC del kit de Seedance. Podés usar tu cuenta de ChatGPT en vez de KIE.");
    step(L"", "3. Armá historias completas", "Un brief se convierte en todas las escenas, con el orden de referencias, y al final unís todo en un solo video.");
    panel.Children().Append(steps);

    auto keyBox = ui::field("Pegá tu clave de KIE", {});
    keyBox.Header(box_value(L"Clave de API de KIE"));
    auto status = ui::secondary("");
    auto actions = ui::hstack(10);
    auto save = ui::primaryButton("Conectar y empezar", L"", {});
    save.Click([keyBox, status, onDone](auto&&, auto&&) {
        status.Text(L"Verificando…");
        AppModel::shared().saveKieKey(str(keyBox.Text()), [status, onDone](std::optional<std::string> error) {
            if (error) {
                status.Text(hs(*error));
                return;
            }
            AppModel::shared().finishOnboarding();
            if (onDone) onDone();
        });
    });
    actions.Children().Append(save);
    actions.Children().Append(ui::link("Conseguir mi clave", [] { openURL(fc::kie::kKeyPageURL); }));
    actions.Children().Append(ui::link("Más tarde", [onDone] {
        AppModel::shared().finishOnboarding();
        if (onDone) onDone();
    }));
    panel.Children().Append(keyBox);
    panel.Children().Append(actions);
    panel.Children().Append(status);
    return panel;
}

winrt::fire_and_forget showOnboarding(mux::XamlRoot root) {
    muxc::ContentDialog dialog;
    dialog.XamlRoot(root);
    if (auto s = ui::style(L"DefaultContentDialogStyle")) dialog.Style(s);
    sizeDialog(dialog, 720, 800);
    auto weak = winrt::make_weak(dialog);
    dialog.Content(onboardingContent([weak] {
        if (auto d = weak.get()) d.Hide();
    }));
    co_await dialog.ShowAsync();
}

winrt::fire_and_forget showPromptPreview(mux::XamlRoot root) {
    auto& model = AppModel::shared();
    std::string text = model.finalPrompt();
    auto panel = ui::vstack(10);
    panel.Children().Append(ui::secondary(model.mode() == fc::MediaKind::image
                                              ? "Este es el texto exacto que recibe el modelo, con tus presets de cámara y encuadre incluidos."
                                              : "Este es el texto exacto que recibe Seedance 2.5.",
                                          ui::Text::body));
    muxc::TextBox box;
    box.Text(hs(text.empty() ? "Todavía no escribiste un prompt." : text));
    box.IsReadOnly(true);
    box.TextWrapping(mux::TextWrapping::Wrap);
    box.AcceptsReturn(true);
    box.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
    box.Height(380);
    panel.Children().Append(box);
    panel.Children().Append(ui::secondary(std::to_string(fc::characterCount(text)) + " caracteres"));
    auto dialog = ui::dialog(root, "Prompt final", panel, "Copiar", "Listo");
    sizeDialog(dialog, 760, 700);
    auto result = co_await dialog.ShowAsync();
    if (result == muxc::ContentDialogResult::Primary) AppModel::shared().copy(text);
}

}  // namespace fcapp
