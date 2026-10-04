#include "pch.h"

#include "MainWindow.h"

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
std::unique_ptr<MainWindow> gMainWindow;
bool gSnapshotMode = false;
constexpr int kMinWidth = 1040;
constexpr int kMinHeight = 700;

LRESULT CALLBACK minimumSizeProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    if (message == WM_GETMINMAXINFO) {
        double scale = GetDpiForWindow(hwnd) / 96.0;
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = static_cast<LONG>(kMinWidth * scale);
        info->ptMinTrackSize.y = static_cast<LONG>(kMinHeight * scale);
        if (gSnapshotMode) {
            // Screenshots: allow a window larger than the CI runner's small screen.
            info->ptMaxTrackSize.x = 4000;
            info->ptMaxTrackSize.y = 4000;
        }
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

/// Renders a XAML element to a PNG file, over a solid background (screenshots).
wf::IAsyncAction captureElement(mux::UIElement element, fs::path file, winrt::Windows::UI::Color background) {
    muxm::Imaging::RenderTargetBitmap bitmap;
    co_await bitmap.RenderAsync(element);
    auto buffer = co_await bitmap.GetPixelsAsync();
    uint32_t width = static_cast<uint32_t>(bitmap.PixelWidth());
    uint32_t height = static_cast<uint32_t>(bitmap.PixelHeight());
    std::vector<uint8_t> pixels(buffer.Length());
    winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer).ReadBytes(pixels);
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        double alpha = pixels[i + 3] / 255.0;
        pixels[i] = static_cast<uint8_t>(pixels[i] + background.B * (1 - alpha));
        pixels[i + 1] = static_cast<uint8_t>(pixels[i + 1] + background.G * (1 - alpha));
        pixels[i + 2] = static_cast<uint8_t>(pixels[i + 2] + background.R * (1 - alpha));
        pixels[i + 3] = 255;
    }
    fs::create_directories(file.parent_path());
    auto folder = co_await winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(file.parent_path().wstring());
    auto output = co_await folder.CreateFileAsync(file.filename().wstring(), winrt::Windows::Storage::CreationCollisionOption::ReplaceExisting);
    auto stream = co_await output.OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite);
    auto encoder = co_await winrt::Windows::Graphics::Imaging::BitmapEncoder::CreateAsync(
        winrt::Windows::Graphics::Imaging::BitmapEncoder::PngEncoderId(), stream);
    encoder.SetPixelData(winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8, winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Ignore,
                         width, height, 96, 96, pixels);
    co_await encoder.FlushAsync();
    stream.Close();
}
}  // namespace

std::string demoVideoPrompt();

void MainWindow::appWindowResize() {
    HWND hwnd = AppModel::shared().hwnd;
    double scale = snapshotFolder_ ? 1.0 : GetDpiForWindow(hwnd) / 96.0;
    window_.AppWindow().Resize({static_cast<int32_t>(1440 * scale), static_cast<int32_t>(900 * scale)});
}

void MainWindow::Launch(std::optional<fs::path> snapshotFolder) { gMainWindow.reset(new MainWindow(snapshotFolder)); }

MainWindow::MainWindow(std::optional<fs::path> snapshotFolder) : snapshotFolder_(std::move(snapshotFolder)) {
    gSnapshotMode = snapshotFolder_.has_value();
    ui::applyTheme();
    window_ = mux::Window();
    window_.Title(L"Framecraft");
    AppModel::create(window_.DispatcherQueue(), snapshotFolder_.has_value());
    auto& model = AppModel::shared();

    HWND hwnd = nullptr;
    window_.as<::IWindowNative>()->get_WindowHandle(&hwnd);
    model.hwnd = hwnd;
    SetWindowSubclass(hwnd, minimumSizeProc, 1, 0);

    window_.SystemBackdrop(muxm::MicaBackdrop());
    window_.ExtendsContentIntoTitleBar(true);
    auto appWindow = window_.AppWindow();
    appWindow.TitleBar().PreferredHeightOption(winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Tall);
    auto iconPath = executableDirectory() / L"Assets" / L"Framecraft.ico";
    if (fs::exists(iconPath)) appWindow.SetIcon(iconPath.wstring());

    root_ = muxc::Grid();
    muxc::RowDefinition titleRow;
    titleRow.Height(ui::pixels(48));
    root_.RowDefinitions().Append(titleRow);
    root_.RowDefinitions().Append(muxc::RowDefinition());

    buildTitleBar();
    buildNavigation();
    buildBanner();
    buildAccelerators();

    window_.Content(root_);
    window_.SetTitleBar(titleBar_);

    appWindowResize();

    model.openJobDetail = [this](std::string const& id) {
        if (root_.XamlRoot()) showJobDetail(root_.XamlRoot(), id);
    };
    model.openOnboarding = [this] {
        if (root_.XamlRoot()) showOnboarding(root_.XamlRoot());
    };
    model.openPalette = [this] { search_.Focus(mux::FocusState::Keyboard); };
    model.openPromptPreview = [this] {
        if (root_.XamlRoot()) showPromptPreview(root_.XamlRoot());
    };
    model.openMedia = [this](fs::path const& file) {
        if (root_.XamlRoot()) showMediaViewer(root_.XamlRoot(), file, true);
    };
    model.subscribe([this](Change change) { refresh(change); });

    select(Section::create);
    refreshAccount();
    window_.Activate();

    root_.Loaded([this](auto&&, auto&&) {
        auto& m = AppModel::shared();
        if (auto settings = nav_.SettingsItem().try_as<muxc::NavigationViewItem>()) settings.Content(box_value(L"Ajustes"));
        if (snapshotFolder_) {
            runSnapshots();
        } else if (m.onboardingNeeded()) {
            showOnboarding(root_.XamlRoot());
        }
        m.updateTaskbar();
    });
    window_.Closed([](auto&&, auto&&) { AppModel::shared().persist(); });
}

// MARK: - Title bar

void MainWindow::buildTitleBar() {
    titleBar_ = ui::columns({ui::autoLength(), ui::star(), ui::autoLength(), ui::pixels(150)}, 12);
    titleBar_.Padding(ui::margin(16, 0, 0, 0));
    titleBar_.Background(muxm::SolidColorBrush(ui::rgb(0, 0, 0, 0)));

    auto brand = ui::hstack(10);
    brand.VerticalAlignment(mux::VerticalAlignment::Center);
    muxc::Image logo;
    logo.Width(20);
    logo.Height(20);
    auto logoPath = executableDirectory() / L"Assets" / L"AppIcon.png";
    logo.Source(muxm::Imaging::BitmapImage(fileUri(logoPath)));
    brand.Children().Append(logo);
    auto title = ui::text("Framecraft", ui::Text::caption);
    title.VerticalAlignment(mux::VerticalAlignment::Center);
    brand.Children().Append(title);
    auto subtitle = ui::secondary("Estudio UGC");
    subtitle.VerticalAlignment(mux::VerticalAlignment::Center);
    brand.Children().Append(subtitle);
    ui::place(titleBar_, brand, 0);

    auto chip = ui::hstack(6);
    chip.VerticalAlignment(mux::VerticalAlignment::Center);
    auto bolt = ui::icon(L"", 12);
    bolt.Foreground(ui::pinkBrush());
    chip.Children().Append(bolt);
    credits_ = ui::secondary("");
    chip.Children().Append(credits_);
    ui::place(titleBar_, chip, 2);

    muxc::Grid::SetRow(titleBar_, 0);
    root_.Children().Append(titleBar_);
}

// MARK: - Navigation

void MainWindow::buildNavigation() {
    auto& model = AppModel::shared();
    nav_ = muxc::NavigationView();
    nav_.PaneDisplayMode(muxc::NavigationViewPaneDisplayMode::Left);
    nav_.IsBackButtonVisible(muxc::NavigationViewBackButtonVisible::Collapsed);
    nav_.IsSettingsVisible(true);
    nav_.OpenPaneLength(250);
    nav_.IsTitleBarAutoPaddingEnabled(false);

    search_ = muxc::AutoSuggestBox();
    search_.PlaceholderText(L"Buscar o hacer…  (Ctrl+K)");
    search_.QueryIcon(muxc::SymbolIcon(muxc::Symbol::Find));
    ui::accessible(search_, "Buscar acciones, plantillas, historias o creaciones");
    search_.TextChanged([this](muxc::AutoSuggestBox const& sender, muxc::AutoSuggestBoxTextChangedEventArgs const& args) {
        if (args.Reason() == muxc::AutoSuggestionBoxTextChangeReason::UserInput) refreshSuggestions(str(sender.Text()));
    });
    search_.GotFocus([this](auto&&, auto&&) {
        refreshSuggestions(str(search_.Text()));
        search_.IsSuggestionListOpen(true);
    });
    search_.QuerySubmitted([this](muxc::AutoSuggestBox const& sender, muxc::AutoSuggestBoxQuerySubmittedEventArgs const& args) {
        std::string chosen = args.ChosenSuggestion() ? str(unbox_value<hstring>(args.ChosenSuggestion())) : "";
        if (chosen.empty() && !suggestionIDs_.empty()) chosen = suggestionIDs_.front().first;
        for (auto const& [label, id] : suggestionIDs_) {
            if (label == chosen) {
                runSuggestion(id);
                break;
            }
        }
        sender.Text(L"");
    });
    nav_.AutoSuggestBox(search_);

    for (Section section : kSidebarSections) {
        muxc::NavigationViewItem item;
        item.Content(box_value(hs(sectionTitle(section))));
        item.Icon(ui::icon(sectionGlyph(section)));
        item.Tag(box_value(static_cast<int32_t>(section)));
        items_[section] = item;
        if (section == Section::guide) {
            muxc::NavigationViewItemSeparator separator;
            nav_.MenuItems().Append(separator);
            muxc::NavigationViewItemHeader header;
            header.Content(box_value(L"Aprender"));
            nav_.MenuItems().Append(header);
        }
        nav_.MenuItems().Append(item);
    }
    activeItem_ = muxc::NavigationViewItem();
    activeItem_.SelectsOnInvoked(false);
    activeItem_.Icon(ui::icon(L""));
    activeItem_.Tag(box_value(-1));
    nav_.FooterMenuItems().Append(activeItem_);

    nav_.SelectionChanged([this](muxc::NavigationView const&, muxc::NavigationViewSelectionChangedEventArgs const& args) {
        if (selecting_) return;
        if (args.IsSettingsSelected()) {
            AppModel::shared().go(Section::settings);
            return;
        }
        if (auto item = args.SelectedItem().try_as<muxc::NavigationViewItem>()) {
            int32_t tag = unbox_value_or<int32_t>(item.Tag(), -1);
            if (tag >= 0) AppModel::shared().go(static_cast<Section>(tag));
        }
    });
    nav_.ItemInvoked([](muxc::NavigationView const&, muxc::NavigationViewItemInvokedEventArgs const& args) {
        if (auto item = args.InvokedItemContainer().try_as<muxc::NavigationViewItem>()) {
            if (unbox_value_or<int32_t>(item.Tag(), 0) == -1) {
                auto& m = AppModel::shared();
                m.filter = LibraryFilter::active;
                m.go(Section::library);
            }
        }
    });

    content_ = muxc::Grid();
    nav_.Content(content_);
    muxc::Grid::SetRow(nav_, 1);
    root_.Children().Append(nav_);
    (void)model;
}

void MainWindow::select(Section section) {
    auto& model = AppModel::shared();
    auto it = pages_.find(section);
    if (it == pages_.end()) {
        std::unique_ptr<Page> page;
        switch (section) {
        case Section::create: page = makeCreatePage(); break;
        case Section::chat: page = makeChatPage(); break;
        case Section::stories: page = makeStoriesPage(); break;
        case Section::library: page = makeLibraryPage(); break;
        case Section::references: page = makeReferencesPage(); break;
        case Section::skills: page = makeSkillsPage(); break;
        case Section::guide: page = makeGuidePage(); break;
        case Section::settings: page = makeSettingsPage(); break;
        }
        it = pages_.emplace(section, std::move(page)).first;
    }
    content_.Children().Clear();
    content_.Children().Append(it->second->root());
    it->second->refresh(Change::all);
    it->second->activated();

    selecting_ = true;
    if (section == Section::settings) nav_.SelectedItem(nav_.SettingsItem());
    else if (auto item = items_.find(section); item != items_.end()) nav_.SelectedItem(item->second);
    selecting_ = false;
    (void)model;
}

void MainWindow::refresh(Change change) {
    auto& model = AppModel::shared();
    if (change == Change::navigation) {
        select(model.section);
        return;
    }
    if (change == Change::banner) {
        if (!model.banner) {
            banner_.IsOpen(false);
            return;
        }
        banner_.Message(hs(model.banner->text));
        banner_.Severity(model.banner->style == Banner::Style::error    ? muxc::InfoBarSeverity::Warning
                         : model.banner->style == Banner::Style::success ? muxc::InfoBarSeverity::Success
                                                                          : muxc::InfoBarSeverity::Informational);
        banner_.IsOpen(true);
        bannerTimer_.Stop();
        bannerTimer_.Interval(std::chrono::milliseconds(model.banner->style == Banner::Style::error ? 9000 : 4500));
        bannerTimer_.Start();
        return;
    }
    if (change == Change::account || change == Change::jobs || change == Change::all) refreshAccount();
    if (change == Change::references || change == Change::stories || change == Change::jobs || change == Change::all) {
        auto badge = [&](Section section, int value) {
            auto item = items_[section];
            if (value <= 0) {
                item.InfoBadge(nullptr);
                return;
            }
            muxc::InfoBadge info;
            info.Value(value);
            item.InfoBadge(info);
        };
        badge(Section::library, model.completedCount());
        badge(Section::references, static_cast<int>(model.references.size()));
        badge(Section::stories, static_cast<int>(model.stories.size()));
    }
    if (auto page = pages_.find(model.section); page != pages_.end()) page->second->refresh(change);
}

void MainWindow::refreshAccount() {
    auto& model = AppModel::shared();
    if (!model.hasKieKey) credits_.Text(L"Sin clave de KIE");
    else if (model.credits) credits_.Text(hs("Créditos: " + formatNumber(*model.credits)));
    else credits_.Text(L"KIE conectado");
    auto active = model.activeJobs();
    ui::show(activeItem_, !active.empty());
    activeItem_.Content(box_value(hs(std::to_string(active.size()) + (active.size() == 1 ? " generación en curso" : " generaciones en curso"))));
    model.updateTaskbar();
}

// MARK: - Search / command bar

void MainWindow::refreshSuggestions(std::string const& query) {
    auto& model = AppModel::shared();
    struct Entry {
        std::string label;
        std::string id;
        bool searchOnly = false;
    };
    std::vector<Entry> entries = {
        {"Acción · Nueva imagen", "new-image"},
        {"Acción · Nuevo video", "new-video"},
        {"Acción · Nueva historia", "new-story"},
        {"Acción · Crear prompt con el asistente", "assistant"},
        {"Acción · Generar ahora (" + model.settingsSummary() + ")", "generate"},
        {"Acción · Agregar referencias", "import"},
        {"Acción · Abrir la carpeta de mis creaciones", "folder"},
        {"Acción · Actualizar créditos de KIE", "credits"},
    };
    for (Section section : kSidebarSections) entries.push_back({"Ir a · " + sectionTitle(section), "go:" + std::to_string(static_cast<int>(section))});
    entries.push_back({"Ir a · Ajustes", "go:" + std::to_string(static_cast<int>(Section::settings))});
    for (auto const& t : fc::templates::creative())
        entries.push_back({"Plantilla · " + t.title + " (" + (t.media == fc::MediaKind::video ? "video" : "imagen") + ")", "tpl:" + t.id});
    for (auto const& t : fc::templates::stories()) entries.push_back({"Plantilla de historia · " + t.title, "stpl:" + t.id});
    for (auto const& s : model.stories) entries.push_back({"Historia · " + s.title, "story:" + s.id});
    for (auto const& s : model.allSkills()) entries.push_back({"Skill · " + s.name, "skill:" + s.id});
    int jobs = 0;
    for (auto const& j : model.jobs) {
        if (++jobs > 200) break;
        entries.push_back({"Creación · " + fc::prefixCharacters(j.prompt, 70), "job:" + j.id, true});
    }

    std::string normalized = fc::lower(fc::trim(query));
    std::vector<std::string> words;
    for (auto const& word : fc::splitLines(fc::replaceAll(normalized, " ", "\n")))
        if (!word.empty()) words.push_back(word);
    suggestionIDs_.clear();
    auto results = winrt::single_threaded_observable_vector<IInspectable>();
    for (auto const& entry : entries) {
        if (words.empty() && entry.searchOnly) continue;
        std::string haystack = fc::lower(entry.label);
        bool match = std::all_of(words.begin(), words.end(), [&](std::string const& w) { return fc::contains(haystack, w); });
        if (!match) continue;
        suggestionIDs_.push_back({entry.label, entry.id});
        results.Append(box_value(hs(entry.label)));
        if (suggestionIDs_.size() >= 14) break;
    }
    search_.ItemsSource(results);
}

void MainWindow::runSuggestion(std::string const& id) {
    auto& model = AppModel::shared();
    auto after = [&](std::string const& prefix) { return id.substr(prefix.size()); };
    if (id == "new-image") {
        model.setMode(fc::MediaKind::image);
        model.go(Section::create);
    } else if (id == "new-video") {
        model.setMode(fc::MediaKind::video);
        model.go(Section::create);
    } else if (id == "new-story") {
        model.newStory();
    } else if (id == "assistant") {
        model.showAssistant = false;
        model.go(Section::create);
        model.notify(Change::assistant);
    } else if (id == "generate") {
        model.generate();
    } else if (id == "import") {
        model.pickAndImport();
    } else if (id == "folder") {
        openPath(model.mediaRoot);
    } else if (id == "credits") {
        model.refreshCredits();
    } else if (fc::startsWith(id, "go:")) {
        model.go(static_cast<Section>(std::stoi(after("go:"))));
    } else if (fc::startsWith(id, "tpl:")) {
        for (auto const& t : fc::templates::creative())
            if (t.id == after("tpl:")) model.applyTemplate(t);
    } else if (fc::startsWith(id, "stpl:")) {
        for (auto const& t : fc::templates::stories())
            if (t.id == after("stpl:")) model.newStory(&t);
    } else if (fc::startsWith(id, "story:")) {
        model.selectedStoryID = after("story:");
        model.go(Section::stories);
        model.notify(Change::stories);
    } else if (fc::startsWith(id, "skill:")) {
        if (auto const* s = model.skill(after("skill:"))) model.useSkill(*s);
    } else if (fc::startsWith(id, "job:")) {
        if (model.openJobDetail) model.openJobDetail(after("job:"));
    }
}

// MARK: - Notices and shortcuts

void MainWindow::buildBanner() {
    banner_ = muxc::InfoBar();
    banner_.IsClosable(true);
    banner_.MaxWidth(640);
    banner_.HorizontalAlignment(mux::HorizontalAlignment::Center);
    banner_.VerticalAlignment(mux::VerticalAlignment::Top);
    banner_.Margin(ui::margin(20, 8, 20, 0));
    banner_.Background(ui::resource(L"AcrylicInAppFillColorDefaultBrush"));
    muxc::Grid::SetRow(banner_, 1);
    root_.Children().Append(banner_);
    bannerTimer_ = window_.DispatcherQueue().CreateTimer();
    bannerTimer_.Tick([this](auto const& timer, auto&&) {
        timer.Stop();
        banner_.IsOpen(false);
    });
}

void MainWindow::buildAccelerators() {
    namespace input = winrt::Microsoft::UI::Xaml::Input;
    using VK = winrt::Windows::System::VirtualKey;
    using VM = winrt::Windows::System::VirtualKeyModifiers;
    root_.KeyboardAcceleratorPlacementMode(input::KeyboardAcceleratorPlacementMode::Hidden);
    auto add = [this](VK key, VM modifiers, std::function<void()> action) {
        input::KeyboardAccelerator accelerator;
        accelerator.Key(key);
        accelerator.Modifiers(modifiers);
        accelerator.Invoked([action](auto&&, input::KeyboardAcceleratorInvokedEventArgs const& args) {
            args.Handled(true);
            action();
        });
        root_.KeyboardAccelerators().Append(accelerator);
    };
    add(VK::K, VM::Control, [this] { search_.Focus(mux::FocusState::Keyboard); });
    add(VK::Enter, VM::Control, [] { AppModel::shared().generate(); });
    add(VK::Enter, VM::Control | VM::Shift, [] { AppModel::shared().buildPrompt(false); });
    add(VK::N, VM::Control, [] {
        auto& m = AppModel::shared();
        m.setMode(fc::MediaKind::image);
        m.go(Section::create);
    });
    add(VK::N, VM::Control | VM::Shift, [] {
        auto& m = AppModel::shared();
        m.setMode(fc::MediaKind::video);
        m.go(Section::create);
    });
    add(VK::N, VM::Control | VM::Menu, [] { AppModel::shared().newStory(); });
    add(VK::I, VM::Control | VM::Menu, [] {
        auto& m = AppModel::shared();
        m.showAssistant = !m.showAssistant;
        m.go(Section::create);
        m.notify(Change::assistant);
    });
    int index = 0;
    for (Section section : kSidebarSections) {
        auto key = static_cast<VK>(static_cast<int32_t>(VK::Number1) + index++);
        add(key, VM::Control, [section] { AppModel::shared().go(section); });
    }
}

// MARK: - Screenshots (CI)

winrt::fire_and_forget MainWindow::runSnapshots() {
    auto& model = AppModel::shared();
    fs::path folder = *snapshotFolder_;
    auto dispatcher = window_.DispatcherQueue();
    auto configureVideo = [&model] {
        model.setMode(fc::MediaKind::video);
        model.skillID = fc::skills::kUgcID;
        model.preferences.videoDuration = 15;
        model.selectedImages.clear();
        for (auto const& r : model.referencesOf(fc::ReferenceKind::image))
            if (model.selectedImages.size() < 2) model.selectedImages.push_back(r.id);
        model.selectedAudios.clear();
        for (auto const& r : model.referencesOf(fc::ReferenceKind::audio)) model.selectedAudios.push_back(r.id);
        model.prompt = demoVideoPrompt();
        model.idea = "Chica recomienda su sérum frente al espejo del baño";
        model.draft = demoVideoPrompt();
        model.notes = "Cargá @Image1 (el frasco) y @Image2 (el baño) en ese orden. Diálogo: 34 de ~37 palabras para 15 s.";
        model.showAssistant = false;
        model.go(Section::create);
        model.notify(Change::all);
    };
    auto configureImage = [&model] {
        model.setMode(fc::MediaKind::image);
        model.prompt = "Selfie espontánea de una chica de 25 años tomando café junto a una ventana con lluvia, luz fría de tarde, piel real con poros, pelo un poco despeinado, taza de cerámica con vapor.";
        model.preferences.camera = "iPhone Camera";
        model.preferences.film = "Selfie";
        model.selectedImages.clear();
        for (auto const& r : model.referencesOf(fc::ReferenceKind::image))
            if (model.selectedImages.size() < 2) model.selectedImages.push_back(r.id);
        model.idea = "Selfie tomando café en un día de lluvia";
        model.draft = model.prompt;
        model.notes.reset();
        model.showAssistant = true;
        model.go(Section::create);
        model.notify(Change::all);
    };
    struct Shot {
        std::string name;
        bool dark;
        std::function<void()> setup;
    };
    std::vector<Shot> shots = {
        {"01-crear-imagen", false, configureImage},
        {"02-crear-video", false, configureVideo},
        {"03-crear-video-oscuro", true, configureVideo},
        {"03b-chatgpt", false, [&model] {
             int64_t now = fc::nowMs();
             model.chats = {{"demo-chat-1", "Ideas para un video UGC de skincare",
                             {{ChatMessage::Role::user, "Ayudame a pensar tres ideas para un video UGC de skincare."},
                              {ChatMessage::Role::assistant,
                               "Claro. Podés probar un antes y después honesto, una rutina rápida de mañana o una reseña tipo “lo compré por curiosidad”. En los tres casos, arrancá con el resultado y después mostrás cómo llegaste ahí."}},
                             now, now},
                            {"demo-chat-2", "Guion para lanzamiento", {{ChatMessage::Role::user, "Armemos un guion breve para un lanzamiento."}},
                             now - 3600000, now - 3600000}};
             model.selectedChatID = "demo-chat-1";
             model.codexStatus = {codex::State::loggedIn, "Sesión iniciada con ChatGPT"};
             model.go(Section::chat);
         }},
        {"03c-chatgpt-oscuro", true, [&model] {
             int64_t now = fc::nowMs();
             model.chats = {{"demo-chat-1", "Ideas para un video UGC de skincare",
                             {{ChatMessage::Role::user, "Ayudame a pensar tres ideas para un video UGC de skincare."},
                              {ChatMessage::Role::assistant,
                               "Claro. Podés probar un antes y después honesto, una rutina rápida de mañana o una reseña tipo “lo compré por curiosidad”. En los tres casos, arrancá con el resultado y después mostrás cómo llegaste ahí."}},
                             now, now},
                            {"demo-chat-2", "Guion para lanzamiento", {{ChatMessage::Role::user, "Armemos un guion breve para un lanzamiento."}},
                             now - 3600000, now - 3600000}};
             model.selectedChatID = "demo-chat-1";
             model.codexStatus = {codex::State::loggedIn, "Sesión iniciada con ChatGPT"};
             model.go(Section::chat);
         }},
        {"04-biblioteca", false, [&model] { model.filter = LibraryFilter::all; model.go(Section::library); }},
        {"05-biblioteca-oscuro", true, [&model] { model.filter = LibraryFilter::all; model.go(Section::library); }},
        {"05b-referencias-oscuro", true, [&model] { model.go(Section::references); }},
        {"06-referencias", false, [&model] { model.go(Section::references); }},
        {"07-skills", false, [&model] { model.go(Section::skills); }},
        {"08-guia", false, [&model] { model.go(Section::guide); }},
        {"09-historias", false, [&model] {
             if (!model.stories.empty()) model.selectedStoryID = model.stories.front().id;
             model.go(Section::stories);
         }},
        {"10-historias-oscuro", true, [&model] {
             if (!model.stories.empty()) model.selectedStoryID = model.stories.front().id;
             model.go(Section::stories);
         }},
        {"11b-ajustes-oscuro", true, [&model] { model.go(Section::settings); }},
        {"11-ajustes", false, [&model] { model.go(Section::settings); }},
    };
    bool dark = mux::Application::Current().RequestedTheme() == mux::ApplicationTheme::Dark;
    appWindowResize();
    for (auto const& shot : shots) {
        if (shot.dark != dark) continue;
        shot.setup();
        co_await winrt::resume_after(std::chrono::milliseconds(2500));
        co_await wil::resume_foreground(dispatcher);
        try {
            co_await captureElement(root_, folder / widen(shot.name + ".png"), dark ? ui::rgb(32, 32, 32) : ui::rgb(243, 243, 243));
            OutputDebugStringA(("snapshot: " + shot.name + "\n").c_str());
        } catch (...) {
        }
    }
    // Welcome screen, shown over the window like the real dialog.
    {
        muxc::Grid overlay;
        overlay.Background(muxm::SolidColorBrush(dark ? ui::rgb(0, 0, 0, 120) : ui::rgb(0, 0, 0, 60)));
        muxc::Grid::SetRowSpan(overlay, 2);
        auto panel = ui::card(onboardingContent([] {}), 28);
        panel.Width(620);
        panel.HorizontalAlignment(mux::HorizontalAlignment::Center);
        panel.VerticalAlignment(mux::VerticalAlignment::Center);
        panel.Background(ui::resource(L"SolidBackgroundFillColorBaseBrush"));
        overlay.Children().Append(panel);
        root_.Children().Append(overlay);
        co_await winrt::resume_after(std::chrono::milliseconds(1500));
        co_await wil::resume_foreground(dispatcher);
        try {
            co_await captureElement(root_, folder / (dark ? L"13-bienvenida-oscuro.png" : L"12-bienvenida.png"),
                                    dark ? ui::rgb(32, 32, 32) : ui::rgb(243, 243, 243));
        } catch (...) {
        }
        uint32_t index = 0;
        if (root_.Children().IndexOf(overlay, index)) root_.Children().RemoveAt(index);
    }
    mux::Application::Current().Exit();
}

}  // namespace fcapp
