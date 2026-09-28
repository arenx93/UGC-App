#include "pch.h"

#include "AppModel.h"

#include <winrt/Microsoft.Windows.AppNotifications.h>
#include <winrt/Microsoft.Windows.AppNotifications.Builder.h>

#include "Http.h"
#include "Media.h"
#include "WinUtil.h"
#include "framecraft/util.h"

using namespace winrt;
namespace wf = winrt::Windows::Foundation;
namespace fs = std::filesystem;
using fc::json;

namespace fcapp {

namespace {
std::unique_ptr<AppModel> gModel;

bool contains(std::vector<std::string> const& list, std::string const& value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

/// A reference ready to upload from a background thread.
struct UploadItem {
    fs::path path;
    std::string mime;
    std::string name;
    std::string fileName;
};

std::vector<std::string> uploadAll(KieService const& client, std::vector<UploadItem> const& items) {
    std::vector<std::string> urls;
    for (auto const& item : items) urls.push_back(client.upload(item.path, item.mime, item.name, item.fileName));
    return urls;
}
}  // namespace

std::string sectionTitle(Section section) {
    switch (section) {
    case Section::create: return "Crear";
    case Section::stories: return "Historias";
    case Section::library: return "Biblioteca";
    case Section::references: return "Referencias";
    case Section::skills: return "Skills";
    case Section::guide: return "Guía UGC";
    case Section::settings: return "Ajustes";
    }
    return "";
}

std::wstring sectionGlyph(Section section) {
    switch (section) {
    case Section::create: return L"";      // Edit (magic pencil)
    case Section::stories: return L"";     // Movies
    case Section::library: return L"";     // Photo library
    case Section::references: return L"";  // Attach
    case Section::skills: return L"";      // Library/brain
    case Section::guide: return L"";       // Book
    case Section::settings: return L"";
    }
    return L"";
}

std::string filterTitle(LibraryFilter filter) {
    switch (filter) {
    case LibraryFilter::all: return "Todo";
    case LibraryFilter::images: return "Imágenes";
    case LibraryFilter::videos: return "Videos";
    case LibraryFilter::active: return "En curso";
    case LibraryFilter::favorites: return "Favoritos";
    case LibraryFilter::failed: return "Con error";
    }
    return "";
}

AppModel& AppModel::shared() { return *gModel; }

void AppModel::create(winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher, bool demo) {
    gModel = std::make_unique<AppModel>(dispatcher, demo);
}

AppModel::AppModel(winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher, bool demo) : dispatcher_(dispatcher), demo_(demo) {
    if (demo) {
        auto root = fs::temp_directory_path() / widen("framecraft-demo-" + fc::newUUID());
        indexFile = root / L"library.json";
        mediaRoot = root / L"media";
    } else {
        indexFile = knownFolder(FOLDERID_LocalAppData) / L"Framecraft Studio" / L"library.json";
        mediaRoot = knownFolder(FOLDERID_Pictures) / L"Framecraft";
    }
    std::error_code error;
    fs::create_directories(generationsDir(), error);
    fs::create_directories(referencesDir(), error);

    resources = fc::SkillResources::locate(executableDirectory());
    builtinSkills = fc::skills::builtins(resources ? &*resources : nullptr);

    if (demo) loadDemoData();
    fc::LibraryIndex index = fc::library::load(indexFile);
    jobs = index.jobs;
    std::sort(jobs.begin(), jobs.end(), [](auto const& a, auto const& b) { return a.created > b.created; });
    references = index.references;
    std::sort(references.begin(), references.end(), [](auto const& a, auto const& b) { return a.created > b.created; });
    customSkills = index.skills;
    stories = index.stories;
    std::sort(stories.begin(), stories.end(), [](auto const& a, auto const& b) { return a.updated > b.updated; });
    preferences = index.preferences;

    // Sanitize preferences saved by older versions.
    if (!fc::presets::imageModel(preferences.imageModel)) preferences.imageModel = "gpt-image-2";
    auto const& resolutions = fc::presets::imageResolutions();
    if (std::find(resolutions.begin(), resolutions.end(), preferences.imageResolution) == resolutions.end()) preferences.imageResolution = "1K";
    preferences.quantity = std::clamp(preferences.quantity, 1, 4);
    if (!fc::presets::isValidCamera(preferences.camera)) preferences.camera = fc::presets::kNone;
    if (!fc::presets::isValidFilm(preferences.film)) preferences.film = fc::presets::kNone;
    auto const& videoResolutions = fc::presets::videoResolutions();
    if (std::find(videoResolutions.begin(), videoResolutions.end(), preferences.videoResolution) == videoResolutions.end()) preferences.videoResolution = "720p";
    auto const& aspects = fc::presets::videoAspects();
    if (std::find(aspects.begin(), aspects.end(), preferences.videoAspect) == aspects.end()) preferences.videoAspect = "9:16";
    preferences.videoDuration = fc::presets::clampDuration(preferences.videoDuration);
    preferences.promptModel = fc::prompts::promptModel(preferences.promptModel).id;
    skillID = preferences.mode == fc::MediaKind::video ? fc::skills::kUgcID : fc::skills::kGeneralID;

    hasKieKey = key(credentials::kKie).has_value();
    hasOpenAIKey = key(credentials::kOpenAI).has_value();
    fixAspect();
    markInterruptedSubmissions();
    if (!stories.empty()) selectedStoryID = stories.front().id;

    if (!demo_) {
        try {
            winrt::Microsoft::Windows::AppNotifications::AppNotificationManager::Default().Register();
        } catch (...) {
        }
        pollTimer_ = dispatcher_.CreateTimer();
        pollTimer_.Interval(std::chrono::seconds(5));
        pollTimer_.Tick([this](auto&&, auto&&) {
            if (!polling_) pollOnce();
        });
        pollTimer_.Start();
        refreshCredits();
    }
}

// MARK: - Observation

void AppModel::subscribe(Listener listener) { listeners_.push_back(std::move(listener)); }

void AppModel::notify(Change change) {
    auto copy = listeners_;
    for (auto const& listener : copy) listener(change);
}

// MARK: - Paths

fs::path AppModel::outputPath(std::string const& fileName) const { return generationsDir() / widen(fileName); }
fs::path AppModel::referencePath(fc::ReferenceFile const& reference) const { return referencesDir() / widen(reference.fileName); }

std::vector<fs::path> AppModel::outputPaths(fc::Job const& job) const {
    std::vector<fs::path> paths;
    for (auto const& output : job.outputs) paths.push_back(outputPath(output));
    return paths;
}

// MARK: - Navigation and messages

void AppModel::go(Section target) {
    section = target;
    notify(Change::navigation);
}

void AppModel::show(std::string const& text, Banner::Style style) {
    static int64_t counter = 0;
    banner = Banner{text, style, ++counter};
    notify(Change::banner);
}

void AppModel::finishOnboarding() {
    preferences.onboardingDone = true;
    persist();
}

// MARK: - Library

std::vector<fc::Skill> AppModel::allSkills() const {
    std::vector<fc::Skill> list = builtinSkills;
    list.insert(list.end(), customSkills.begin(), customSkills.end());
    return list;
}

std::vector<fc::Skill> AppModel::skillsFor(fc::MediaKind kind) const {
    std::vector<fc::Skill> list;
    for (auto const& s : allSkills())
        if (fc::supports(s.media, kind)) list.push_back(s);
    return list;
}

fc::Skill const* AppModel::skill(std::string const& id) const {
    for (auto const& s : builtinSkills)
        if (s.id == id) return &s;
    for (auto const& s : customSkills)
        if (s.id == id) return &s;
    return nullptr;
}

std::vector<fc::Job> AppModel::activeJobs() const {
    std::vector<fc::Job> list;
    for (auto const& j : jobs)
        if (fc::isActive(j.status)) list.push_back(j);
    return list;
}

std::vector<fc::Job> AppModel::filteredJobs() const {
    std::string query = fc::lower(fc::trim(search));
    std::vector<fc::Job> list;
    for (auto const& j : jobs) {
        bool passes = true;
        switch (filter) {
        case LibraryFilter::all: break;
        case LibraryFilter::images: passes = j.kind == fc::MediaKind::image; break;
        case LibraryFilter::videos: passes = j.kind == fc::MediaKind::video; break;
        case LibraryFilter::active: passes = fc::isActive(j.status); break;
        case LibraryFilter::favorites: passes = j.favorite; break;
        case LibraryFilter::failed: passes = j.status == fc::JobStatus::fail || j.status == fc::JobStatus::unknown; break;
        }
        if (passes && !query.empty()) {
            passes = fc::contains(fc::lower(j.prompt), query) || fc::contains(fc::lower(fc::presets::modelName(j.model)), query);
        }
        if (passes) list.push_back(j);
    }
    return list;
}

int AppModel::completedCount() const {
    int count = 0;
    for (auto const& j : jobs)
        if (j.status == fc::JobStatus::success) count += static_cast<int>(j.outputs.size());
    return count;
}

fc::Job* AppModel::job(std::string const& id) {
    for (auto& j : jobs)
        if (j.id == id) return &j;
    return nullptr;
}

fc::ReferenceFile const* AppModel::reference(std::string const& id) const {
    for (auto const& r : references)
        if (r.id == id) return &r;
    return nullptr;
}

std::vector<fc::ReferenceFile> AppModel::referencesOf(fc::ReferenceKind kind) const {
    std::vector<fc::ReferenceFile> list;
    for (auto const& r : references)
        if (r.kind == kind) list.push_back(r);
    return list;
}

void AppModel::persist() {
    fc::LibraryIndex index;
    index.jobs = jobs;
    index.references = references;
    index.skills = customSkills;
    index.stories = stories;
    index.preferences = preferences;
    fc::library::save(indexFile, index);
    updateTaskbar();
}

// MARK: - Keys

std::optional<std::string> AppModel::key(wchar_t const* account) const {
    if (demo_) {
        auto it = demoKeys_.find(account);
        return it == demoKeys_.end() ? std::nullopt : std::optional<std::string>(it->second);
    }
    return credentials::read(account);
}

bool AppModel::writeKey(wchar_t const* account, std::string const& value) {
    if (demo_) {
        demoKeys_[account] = value;
        return true;
    }
    return credentials::write(account, value);
}

void AppModel::deleteKey(wchar_t const* account) {
    if (demo_) {
        demoKeys_.erase(account);
        return;
    }
    credentials::remove(account);
}

winrt::fire_and_forget AppModel::saveKieKey(std::string raw, std::function<void(std::optional<std::string>)> done) {
    std::string value = fc::trim(raw);
    if (value.size() < 10 || value.size() > 512) {
        done(std::string("Esa no parece una clave de KIE válida."));
        co_return;
    }
    std::optional<std::string> failure;
    std::optional<double> balance;
    co_await winrt::resume_background();
    try {
        balance = KieService(value).credits();
    } catch (fc::Error const& error) {
        failure = error.what();
    }
    co_await wil::resume_foreground(dispatcher_);
    if (!failure && !writeKey(credentials::kKie, value)) failure = "No se pudo guardar la clave en el Administrador de credenciales de Windows.";
    if (!failure) {
        hasKieKey = true;
        credits = balance;
        show("KIE conectado. Ya podés generar imágenes, videos y prompts.", Banner::Style::success);
        notify(Change::account);
    }
    done(failure);
}

void AppModel::removeKieKey() {
    deleteKey(credentials::kKie);
    hasKieKey = false;
    credits.reset();
    show("Se quitó la clave de KIE.");
    notify(Change::account);
}

std::optional<std::string> AppModel::saveOpenAIKey(std::string const& raw) {
    std::string value = fc::trim(raw);
    if (!fc::startsWith(value, "sk-") || value.size() > 1024) return std::string("Las claves de OpenAI empiezan con \"sk-\".");
    if (!writeKey(credentials::kOpenAI, value)) return std::string("No se pudo guardar la clave.");
    hasOpenAIKey = true;
    show("Clave de OpenAI guardada. Se verifica al crear un prompt.", Banner::Style::success);
    notify(Change::account);
    return std::nullopt;
}

void AppModel::removeOpenAIKey() {
    deleteKey(credentials::kOpenAI);
    hasOpenAIKey = false;
    if (preferences.provider == fc::PromptProvider::openai) preferences.provider = fc::PromptProvider::kie;
    notify(Change::account);
}

winrt::fire_and_forget AppModel::refreshCredits() {
    auto value = key(credentials::kKie);
    if (!value || demo_) co_return;
    checkingCredits = true;
    notify(Change::account);
    std::optional<double> balance;
    co_await winrt::resume_background();
    try {
        balance = KieService(*value).credits();
    } catch (...) {
    }
    co_await wil::resume_foreground(dispatcher_);
    checkingCredits = false;
    if (balance) credits = balance;
    notify(Change::account);
}

// MARK: - Create form

void AppModel::setMode(fc::MediaKind mode) {
    if (preferences.mode == mode) return;
    preferences.mode = mode;
    modeChanged();
    notify(Change::form);
    notify(Change::assistant);
}

void AppModel::modeChanged() {
    fc::Skill const* current = skill(skillID);
    if ((current && !fc::supports(current->media, preferences.mode)) ||
        (preferences.mode == fc::MediaKind::video && skillID == fc::skills::kGeneralID)) {
        skillID = preferences.mode == fc::MediaKind::video ? fc::skills::kUgcID : fc::skills::kGeneralID;
    }
    if (preferences.mode == fc::MediaKind::image && selectedImages.size() > 4) {
        selectedImages.resize(4);
        show("En imágenes se usan hasta 4 referencias: quedaron las primeras 4.");
    }
    draft.clear();
    notes.reset();
    sources.clear();
}

void AppModel::setImageModel(std::string const& id) {
    preferences.imageModel = id;
    fixAspect();
    notify(Change::form);
}

void AppModel::setImageResolution(std::string const& resolution) {
    preferences.imageResolution = resolution;
    fixAspect();
    notify(Change::form);
}

void AppModel::fixAspect() {
    auto options = fc::presets::ratios(preferences.imageModel, preferences.imageResolution);
    if (std::find(options.begin(), options.end(), preferences.imageAspect) == options.end())
        preferences.imageAspect = options.empty() ? "1:1" : options.front();
}

std::vector<std::string> AppModel::aspectOptions() const {
    return mode() == fc::MediaKind::image ? fc::presets::ratios(preferences.imageModel, preferences.imageResolution)
                                          : fc::presets::videoAspects();
}

std::string AppModel::finalPrompt() const {
    return mode() == fc::MediaKind::image
               ? fc::presets::composePrompt(prompt, preferences.camera, preferences.film, preferences.imageAspect)
               : fc::trim(prompt);
}

std::optional<fc::LintReport> AppModel::lintReport() const {
    if (mode() != fc::MediaKind::video || fc::trim(prompt).empty()) return std::nullopt;
    return fc::linter::lintVideo(prompt, preferences.videoDuration,
                                 {static_cast<int>(selectedImages.size()), static_cast<int>(selectedVideos.size()),
                                  static_cast<int>(selectedAudios.size())},
                                 skillID != fc::skills::kArthasID);
}

std::optional<std::string> AppModel::generateBlocker() const {
    if (isGenerating) return std::string("Ya se está enviando una generación.");
    if (fc::trim(prompt).empty()) return std::string("Escribí un prompt (o pedile uno al asistente).");
    size_t length = fc::characterCount(prompt);
    if (mode() == fc::MediaKind::image) {
        int limit = fc::presets::maxPromptLength(preferences.imageModel);
        if (length > static_cast<size_t>(limit)) return "El prompt supera los " + std::to_string(limit) + " caracteres de este modelo.";
    } else if (length > static_cast<size_t>(fc::presets::kMaxVideoPromptLength)) {
        return "El prompt de video supera los " + std::to_string(fc::presets::kMaxVideoPromptLength) + " caracteres.";
    }
    return std::nullopt;
}

std::string AppModel::settingsSummary() const {
    auto const& p = preferences;
    if (mode() == fc::MediaKind::image) {
        auto const* model = fc::presets::imageModel(p.imageModel);
        return (model ? model->name : p.imageModel) + " · " + p.imageResolution + " · " + p.imageAspect + " · " +
               (p.quantity == 1 ? "1 imagen" : std::to_string(p.quantity) + " imágenes");
    }
    return fc::presets::kVideoModelName + " · " + p.videoResolution + " · " + p.videoAspect + " · " + std::to_string(p.videoDuration) +
           " s" + (p.videoAudio ? " · con audio" : "");
}

void AppModel::insertTag(std::string const& tag) {
    bool needsSpace = !prompt.empty() && prompt.back() != ' ' && prompt.back() != '\n';
    prompt += (needsSpace ? " " : "") + tag + " ";
    notify(Change::form);
}

void AppModel::applyTemplate(fc::CreativeTemplate const& creative) {
    setMode(creative.media);
    if (creative.media == fc::MediaKind::video) {
        if (creative.duration) preferences.videoDuration = fc::presets::clampDuration(*creative.duration);
        auto const& aspects = fc::presets::videoAspects();
        if (std::find(aspects.begin(), aspects.end(), creative.aspect) != aspects.end()) preferences.videoAspect = creative.aspect;
        skillID = fc::skills::kUgcID;
    } else {
        auto options = aspectOptions();
        if (std::find(options.begin(), options.end(), creative.aspect) != options.end()) preferences.imageAspect = creative.aspect;
        skillID = fc::skills::kGeneralID;
    }
    idea = creative.idea;
    showAssistant = true;
    go(Section::create);
    notify(Change::form);
    notify(Change::assistant);
    show(creative.placeholders().empty() ? "Plantilla “" + creative.title + "” lista en el asistente."
                                         : "Plantilla “" + creative.title + "”: cambiá lo que está entre [corchetes] y tocá Crear prompt.",
         Banner::Style::success);
}

// MARK: - References

std::vector<std::string>& AppModel::selection(fc::ReferenceKind kind) {
    switch (kind) {
    case fc::ReferenceKind::image: return selectedImages;
    case fc::ReferenceKind::video: return selectedVideos;
    case fc::ReferenceKind::audio: return selectedAudios;
    }
    return selectedImages;
}

std::vector<std::string> const& AppModel::selection(fc::ReferenceKind kind) const {
    return const_cast<AppModel*>(this)->selection(kind);
}

std::vector<fc::ReferenceFile> AppModel::selectedReferences(fc::ReferenceKind kind) const {
    std::vector<fc::ReferenceFile> list;
    for (auto const& id : selection(kind))
        if (auto const* r = reference(id)) list.push_back(*r);
    return list;
}

int AppModel::selectionLimit(fc::ReferenceKind kind) const {
    if (kind == fc::ReferenceKind::image) return mode() == fc::MediaKind::image ? 4 : 30;
    return 10;
}

double AppModel::selectedSeconds(fc::ReferenceKind kind) const {
    double total = 0;
    for (auto const& r : selectedReferences(kind)) total += r.durationSeconds();
    return total;
}

bool AppModel::isSelected(fc::ReferenceFile const& r) const { return contains(selection(r.kind), r.id); }

std::optional<std::string> AppModel::tag(fc::ReferenceFile const& r) const {
    auto const& list = selection(r.kind);
    auto it = std::find(list.begin(), list.end(), r.id);
    if (it == list.end()) return std::nullopt;
    return std::string(fc::tagPrefix(r.kind)) + std::to_string(it - list.begin() + 1);
}

void AppModel::toggleSelection(fc::ReferenceFile const& r) {
    auto& list = selection(r.kind);
    auto it = std::find(list.begin(), list.end(), r.id);
    if (it != list.end()) {
        list.erase(it);
    } else {
        int limit = selectionLimit(r.kind);
        if (static_cast<int>(list.size()) >= limit) {
            show("Podés usar hasta " + std::to_string(limit) + " " + fc::pluralNoun(r.kind) + " de referencia.", Banner::Style::error);
            return;
        }
        if (r.kind != fc::ReferenceKind::image && selectedSeconds(r.kind) + r.durationSeconds() > 30.05) {
            show(std::string("Los ") + fc::pluralNoun(r.kind) + " seleccionados pueden sumar hasta 30 segundos.", Banner::Style::error);
            return;
        }
        list.push_back(r.id);
    }
    notify(Change::form);
}

void AppModel::moveSelection(fc::ReferenceKind kind, std::string const& id, int offset) {
    auto& list = selection(kind);
    auto it = std::find(list.begin(), list.end(), id);
    if (it == list.end()) return;
    long index = it - list.begin();
    long target = index + offset;
    if (target < 0 || target >= static_cast<long>(list.size())) return;
    std::swap(list[index], list[target]);
    notify(Change::form);
}

winrt::fire_and_forget AppModel::importFiles(std::vector<fs::path> files, bool select) {
    if (files.empty()) co_return;
    isImporting = true;
    notify(Change::references);
    fs::path destination = referencesDir();
    std::vector<fc::ReferenceFile> imported;
    std::vector<std::string> errors;
    co_await winrt::resume_background();
    for (auto const& file : files) {
        std::string displayName = narrow(file.filename().wstring());
        try {
            auto match = fc::media::classifyExtension(narrow(file.extension().wstring()));
            if (!match) throw fc::Error("Formato no soportado. Usá imágenes PNG/JPG/WebP, videos MP4/MOV o audio MP3/WAV/M4A.", true);
            std::error_code error;
            auto bytes = static_cast<int64_t>(fs::file_size(file, error));
            if (error) throw fc::Error("No se pudo leer el archivo.", true);
            if (bytes > fc::maxBytes(match->kind))
                throw fc::Error("Supera los " + std::to_string(fc::maxBytes(match->kind) / 1024 / 1024) + " MB permitidos.", true);
            if (!fc::media::matchesSignature(readFileBytes(file, 16), match->mime, match->kind))
                throw fc::Error(std::string("El archivo no coincide con su formato (") + fc::formatsDescription(match->kind) + ").", true);
            std::optional<int> durationMs;
            if (match->kind != fc::ReferenceKind::image) {
                auto seconds = media::duration(file, match->kind == fc::ReferenceKind::video);
                if (!seconds) throw fc::Error("No pude leer la duración. Convertilo a MP4, MP3 o M4A y probá de nuevo.", true);
                if (*seconds > 30.05)
                    throw fc::Error("Dura " + std::to_string(static_cast<int>(*seconds)) + " s: cada audio o video de referencia puede durar hasta 30 s.", true);
                durationMs = static_cast<int>(std::lround(*seconds * 1000));
            }
            fc::ReferenceFile reference;
            reference.id = fc::newUUID();
            reference.name = fc::prefixCharacters(displayName, 150);
            reference.kind = match->kind;
            reference.mime = match->mime;
            reference.fileName = fc::lower(reference.id) + "." + fc::media::fileExtension(match->mime);
            reference.durationMs = durationMs;
            reference.bytes = bytes;
            reference.created = fc::nowMs();
            fs::create_directories(destination, error);
            fs::copy_file(file, destination / widen(reference.fileName), fs::copy_options::overwrite_existing, error);
            if (error) throw fc::Error("No se pudo copiar el archivo a tu biblioteca.", true);
            imported.push_back(reference);
        } catch (fc::Error const& error) {
            errors.push_back(displayName + ": " + error.what());
        } catch (...) {
            errors.push_back(displayName + ": no se pudo importar.");
        }
    }
    co_await wil::resume_foreground(dispatcher_);
    isImporting = false;
    for (auto const& reference : imported) {
        references.insert(references.begin(), reference);
        if (!select) continue;
        auto& list = selection(reference.kind);
        int limit = selectionLimit(reference.kind);
        bool fits = reference.kind == fc::ReferenceKind::image || selectedSeconds(reference.kind) + reference.durationSeconds() <= 30;
        if (static_cast<int>(list.size()) < limit && fits) list.push_back(reference.id);
    }
    persist();
    notify(Change::references);
    notify(Change::form);
    if (!errors.empty()) {
        show(errors.size() == 1 ? errors[0] : errors[0] + " (y " + std::to_string(errors.size() - 1) + " más)", Banner::Style::error);
    } else if (!imported.empty()) {
        show(imported.size() == 1 ? "Referencia agregada." : std::to_string(imported.size()) + " referencias agregadas.", Banner::Style::success);
    }
}

winrt::fire_and_forget AppModel::pickAndImport() {
    winrt::Windows::Storage::Pickers::FileOpenPicker picker;
    picker.as<::IInitializeWithWindow>()->Initialize(hwnd);
    for (auto ext : {L".png", L".jpg", L".jpeg", L".webp", L".mp4", L".m4v", L".mov", L".mkv", L".mp3", L".wav", L".aac", L".m4a", L".ogg"})
        picker.FileTypeFilter().Append(ext);
    picker.ViewMode(winrt::Windows::Storage::Pickers::PickerViewMode::Thumbnail);
    std::vector<fs::path> files;
    try {
        auto picked = co_await picker.PickMultipleFilesAsync();
        for (auto const& file : picked) files.push_back(fs::path(std::wstring(file.Path())));
    } catch (...) {
    }
    if (!files.empty()) importFiles(files);
}

void AppModel::deleteReferences(std::vector<std::string> const& ids) {
    std::error_code error;
    for (auto const& r : references)
        if (contains(ids, r.id)) fs::remove(referencePath(r), error);
    std::erase_if(references, [&](auto const& r) { return contains(ids, r.id); });
    for (auto* list : {&selectedImages, &selectedVideos, &selectedAudios})
        std::erase_if(*list, [&](auto const& id) { return contains(ids, id); });
    persist();
    notify(Change::references);
    notify(Change::form);
    show(ids.size() == 1 ? "Referencia eliminada." : std::to_string(ids.size()) + " referencias eliminadas.");
}

// MARK: - Generation

void AppModel::update(std::string const& id, std::function<void(fc::Job&)> const& change) {
    if (auto* j = job(id)) {
        change(*j);
        j->updated = fc::nowMs();
    }
}

winrt::fire_and_forget AppModel::generate() {
    if (auto reason = generateBlocker()) {
        show(*reason, Banner::Style::error);
        co_return;
    }
    auto apiKey = key(credentials::kKie);
    if (!apiKey) {
        if (openOnboarding) openOnboarding();
        co_return;
    }
    // Snapshot of the form (the user may keep editing while it uploads).
    bool image = mode() == fc::MediaKind::image;
    std::string text = fc::trim(prompt);
    std::string finalText = finalPrompt();
    fc::Preferences p = preferences;
    auto imageIDs = image ? std::vector<std::string>(selectedImages.begin(), selectedImages.begin() + std::min<size_t>(4, selectedImages.size()))
                          : selectedImages;
    auto videoIDs = image ? std::vector<std::string>{} : selectedVideos;
    auto audioIDs = image ? std::vector<std::string>{} : selectedAudios;
    auto link = pendingSceneLink_;
    pendingSceneLink_.reset();

    if (image && fc::startsWith(text, "{")) {
        json parsed = json::parse(text, nullptr, false);
        if (parsed.is_discarded() || !parsed.is_object()) {
            show("Tu prompt JSON tiene un error de sintaxis. Corregilo antes de generar.", Banner::Style::error);
            co_return;
        }
    }
    if (image && fc::characterCount(finalText) > static_cast<size_t>(fc::presets::maxComposedLength(p.imageModel))) {
        show("El prompt final (con presets) es demasiado largo para este modelo. Acortalo.", Banner::Style::error);
        co_return;
    }
    if (!image && (selectedSeconds(fc::ReferenceKind::video) > 30.05 || selectedSeconds(fc::ReferenceKind::audio) > 30.05)) {
        show("Los videos y audios de referencia pueden sumar hasta 30 s cada uno.", Banner::Style::error);
        co_return;
    }
    int adding = image ? p.quantity : 1;
    int recent = 0;
    for (auto const& j : jobs)
        if (fc::isActive(j.status) && j.created > fc::nowMs() - 3600 * 1000) ++recent;
    if (recent + adding > 12) {
        show("Esperá a que terminen algunas generaciones en curso antes de lanzar más.", Banner::Style::error);
        co_return;
    }
    auto items = [&](std::vector<std::string> const& ids) {
        std::vector<UploadItem> list;
        for (auto const& id : ids) {
            if (auto const* r = reference(id))
                list.push_back({referencePath(*r), r->mime, r->name, fc::lower(r->id) + "." + fc::media::fileExtension(r->mime)});
        }
        return list;
    };
    auto imageItems = items(imageIDs), videoItems = items(videoIDs), audioItems = items(audioIDs);
    if (imageItems.size() + videoItems.size() + audioItems.size() != imageIDs.size() + videoIDs.size() + audioIDs.size()) {
        show("Una referencia seleccionada ya no existe. Volvé a elegirla.", Banner::Style::error);
        co_return;
    }

    isGenerating = true;
    generationStep = imageItems.empty() && videoItems.empty() && audioItems.empty() ? "Enviando…" : "Subiendo referencias…";
    notify(Change::form);
    std::string batch = fc::newUUID();
    fc::JobSettings settings;
    std::vector<std::string> newIDs;
    if (image) {
        settings.resolution = p.imageResolution;
        settings.aspect = p.imageAspect;
        settings.camera = p.camera;
        settings.film = p.film;
        settings.imageReferences = imageIDs;
    } else {
        settings.resolution = p.videoResolution;
        settings.aspect = p.videoAspect;
        settings.duration = p.videoDuration;
        settings.generateAudio = p.videoAudio;
        settings.imageReferences = imageIDs;
        settings.videoReferences = videoIDs;
        settings.audioReferences = audioIDs;
    }

    KieService client(*apiKey);
    std::vector<std::string> imageURLs, videoURLs, audioURLs;
    std::optional<std::string> uploadError;
    co_await winrt::resume_background();
    try {
        imageURLs = uploadAll(client, imageItems);
        videoURLs = uploadAll(client, videoItems);
        audioURLs = uploadAll(client, audioItems);
    } catch (fc::Error const& error) {
        uploadError = error.what();
    } catch (...) {
        uploadError = "No se pudieron subir las referencias.";
    }
    co_await wil::resume_foreground(dispatcher_);
    if (uploadError) {
        isGenerating = false;
        generationStep.clear();
        notify(Change::form);
        show(*uploadError, Banner::Style::error);
        co_return;
    }
    generationStep = "Enviando a KIE…";
    int count = image ? p.quantity : 1;
    for (int i = 0; i < count; ++i) {
        fc::Job j;
        j.id = fc::newUUID();
        j.batchID = batch;
        j.kind = image ? fc::MediaKind::image : fc::MediaKind::video;
        j.model = image ? p.imageModel : fc::presets::kVideoModelID;
        j.prompt = text;
        j.finalPrompt = finalText;
        j.settings = settings;
        j.created = j.updated = fc::nowMs();
        jobs.insert(jobs.begin(), j);
        newIDs.push_back(j.id);
    }
    if (link && !image) {
        updateScene(link->first, link->second, [&](fc::StoryScene& scene) { scene.jobIDs.push_back(newIDs.front()); });
        notify(Change::stories);
    }
    persist();
    notify(Change::jobs);
    notify(Change::form);

    std::string model;
    json input;
    if (image) {
        auto task = fc::inputs::image(p.imageModel, finalText, p.imageAspect, p.imageResolution, imageURLs);
        model = task.model;
        input = task.input;
    } else {
        model = fc::presets::kVideoModelID;
        input = fc::inputs::video(text, p.videoResolution, p.videoAspect, p.videoDuration, p.videoAudio, imageURLs, videoURLs, audioURLs);
    }
    struct Outcome {
        std::string id;
        std::optional<std::string> taskId;
        std::string error;
        bool definite = false;
    };
    std::vector<Outcome> outcomes;
    co_await winrt::resume_background();
    for (auto const& id : newIDs) {
        Outcome outcome{id};
        try {
            outcome.taskId = client.createTask(model, input);
        } catch (fc::Error const& error) {
            outcome.error = error.what();
            outcome.definite = error.definite();
        } catch (...) {
            outcome.error = "No se pudo enviar.";
        }
        outcomes.push_back(outcome);
    }
    co_await wil::resume_foreground(dispatcher_);
    for (auto const& outcome : outcomes) {
        update(outcome.id, [&](fc::Job& j) {
            if (outcome.taskId) {
                j.taskId = outcome.taskId;
                j.status = fc::JobStatus::queued;
                j.progress = 1;
            } else {
                j.status = outcome.definite ? fc::JobStatus::fail : fc::JobStatus::unknown;
                j.error = outcome.definite ? outcome.error : fc::inputs::kUncertainSubmission;
            }
        });
    }
    isGenerating = false;
    generationStep.clear();
    persist();
    notify(Change::jobs);
    notify(Change::form);
    bool anySent = std::any_of(outcomes.begin(), outcomes.end(), [](auto const& o) { return o.taskId.has_value(); });
    if (!anySent && !outcomes.empty()) {
        show(outcomes.front().error, Banner::Style::error);
    } else if (image) {
        show(count == 1 ? "Imagen en camino. Te aviso cuando esté lista." : std::to_string(count) + " imágenes en camino.", Banner::Style::success);
    } else {
        show("Video en camino. Suele tardar unos minutos: podés seguir trabajando.", Banner::Style::success);
    }
    startPolling();
}

void AppModel::startPolling() {
    if (!polling_ && !demo_) pollOnce();
    updateTaskbar();
}

wf::IAsyncAction AppModel::pollOnce() {
    struct Pending {
        std::string id;
        std::string taskId;
        fc::MediaKind kind;
    };
    std::vector<Pending> pending;
    for (auto const& j : jobs) {
        if (j.taskId && (j.status == fc::JobStatus::queued || j.status == fc::JobStatus::generating || j.status == fc::JobStatus::saving))
            pending.push_back({j.id, *j.taskId, j.kind});
        if (pending.size() >= 12) break;
    }
    auto apiKey = key(credentials::kKie);
    if (pending.empty() || !apiKey || polling_) co_return;
    polling_ = true;
    KieService client(*apiKey);
    fs::path generations = generationsDir();

    struct Result {
        std::string id;
        std::optional<fc::KieTaskRecord> record;
        std::vector<std::string> outputs;
        std::optional<std::string> error;
    };
    std::vector<Result> results;
    co_await winrt::resume_background();
    for (auto const& item : pending) {
        Result result{item.id};
        try {
            result.record = client.recordInfo(item.taskId);
            if (result.record->state == "success") {
                if (result.record->resultURLs.empty()) throw fc::Error("KIE no devolvió archivos para esta generación.", true);
                int index = 0;
                for (auto const& url : result.record->resultURLs) {
                    if (!fc::startsWith(url, "https://")) throw fc::Error("KIE devolvió una URL no soportada.", true);
                    bool video = item.kind == fc::MediaKind::video;
                    std::string base = "framecraft-" + fileTimestamp() + "-" + fc::lower(item.id.substr(0, 4)) + "-" + std::to_string(++index);
                    fs::path temporary = generations / widen(base + ".download");
                    HttpResponse response = httpDownload(url, temporary, video ? 180 : 60, video ? 250LL * 1024 * 1024 : 30LL * 1024 * 1024);
                    if (response.status < 200 || response.status >= 300) {
                        std::error_code error;
                        fs::remove(temporary, error);
                        throw fc::Error("Todavía no se pudo guardar el archivo. Se reintenta solo.", false);
                    }
                    std::string extension;
                    if (video) {
                        extension = fc::contains(response.contentType, "quicktime") ? "mov" : "mp4";
                    } else {
                        std::string head = readFileBytes(temporary, 16);
                        if (fc::media::matchesSignature(head, "image/png", fc::ReferenceKind::image)) extension = "png";
                        else if (fc::media::matchesSignature(head, "image/webp", fc::ReferenceKind::image)) extension = "webp";
                        else if (fc::media::matchesSignature(head, "image/jpeg", fc::ReferenceKind::image)) extension = "jpg";
                        else throw fc::Error("KIE devolvió un formato de imagen no soportado.", true);
                    }
                    std::string name = base + "." + extension;
                    std::error_code error;
                    fs::rename(temporary, generations / widen(name), error);
                    if (error) throw fc::Error("No se pudo guardar el archivo generado.", false);
                    result.outputs.push_back(name);
                }
            }
        } catch (fc::Error const& error) {
            result.error = error.what();
        } catch (HttpError const& error) {
            result.error = std::string(error.what()) + " Se reintenta solo.";
        } catch (...) {
            result.error = "No se pudo consultar a KIE. Se reintenta solo.";
        }
        results.push_back(result);
    }
    co_await wil::resume_foreground(dispatcher_);
    polling_ = false;
    bool gotCredits = false;
    for (auto const& result : results) {
        if (!result.record) {
            update(result.id, [&](fc::Job& j) { j.error = result.error; });
            continue;
        }
        auto const& record = *result.record;
        if (record.state == "fail") {
            update(result.id, [&](fc::Job& j) {
                j.status = fc::JobStatus::fail;
                j.progress = record.progress;
                j.error = record.failMessage.value_or("KIE no pudo completar la generación.");
            });
            notifyFinished(result.id);
        } else if (record.state == "success") {
            if (result.outputs.empty()) {
                update(result.id, [&](fc::Job& j) {
                    j.status = fc::JobStatus::saving;
                    j.progress = 99;
                    j.error = result.error;
                });
                continue;
            }
            update(result.id, [&](fc::Job& j) {
                j.outputs = result.outputs;
                j.status = fc::JobStatus::success;
                j.progress = 100;
                j.error.reset();
            });
            notifyFinished(result.id);
            gotCredits = true;
        } else {
            update(result.id, [&](fc::Job& j) {
                j.status = fc::JobStatus::generating;
                j.progress = record.progress;
                j.error.reset();
            });
        }
    }
    markInterruptedSubmissions();
    persist();
    notify(Change::jobs);
    if (gotCredits) refreshCredits();
}

void AppModel::markInterruptedSubmissions() {
    int64_t cutoff = fc::nowMs() - 120 * 1000;
    for (auto& j : jobs) {
        if (j.status == fc::JobStatus::submitting && j.created < cutoff) {
            j.status = fc::JobStatus::unknown;
            j.error = "El envío se interrumpió. " + fc::inputs::kUncertainSubmission;
        }
    }
}

void AppModel::notifyFinished(std::string const& id) {
    if (!preferences.notifyWhenDone || demo_) return;
    auto const* j = job(id);
    if (!j) return;
    if (hwnd && GetForegroundWindow() == hwnd) return;
    try {
        using namespace winrt::Microsoft::Windows::AppNotifications;
        using namespace winrt::Microsoft::Windows::AppNotifications::Builder;
        std::string title = j->status == fc::JobStatus::success ? (j->kind == fc::MediaKind::video ? "Tu video está listo" : "Tu imagen está lista")
                                                                : "La generación falló";
        AppNotificationBuilder builder;
        builder.AddText(hs(title));
        builder.AddText(hs(fc::prefixCharacters(j->prompt, 120)));
        AppNotificationManager::Default().Show(builder.BuildNotification());
    } catch (...) {
        if (hwnd) FlashWindow(hwnd, TRUE);
    }
}

void AppModel::updateTaskbar() {
    if (!hwnd || demo_) return;
    static com_ptr<ITaskbarList3> taskbar = [] {
        com_ptr<ITaskbarList3> list;
        if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(list.put())))) list->HrInit();
        return list;
    }();
    if (!taskbar) return;
    auto active = activeJobs();
    if (active.empty()) {
        taskbar->SetProgressState(hwnd, TBPF_NOPROGRESS);
        return;
    }
    int total = 0;
    for (auto const& j : active) total += std::max(j.progress, 2);
    taskbar->SetProgressState(hwnd, TBPF_NORMAL);
    taskbar->SetProgressValue(hwnd, static_cast<ULONGLONG>(total / static_cast<int>(active.size())), 100);
}

// MARK: - Library actions

void AppModel::deleteJob(std::string const& id) {
    auto* j = job(id);
    if (!j) return;
    if (!fc::isFinished(j->status)) {
        show("Esperá a que termine esta generación para borrarla.", Banner::Style::error);
        return;
    }
    std::error_code error;
    for (auto const& path : outputPaths(*j)) fs::remove(path, error);
    std::erase_if(jobs, [&](auto const& other) { return other.id == id; });
    persist();
    notify(Change::jobs);
    show("Generación eliminada. No se reintegran créditos de KIE.");
}

void AppModel::toggleFavorite(std::string const& id) {
    update(id, [](fc::Job& j) { j.favorite = !j.favorite; });
    persist();
    notify(Change::jobs);
}

void AppModel::reuse(std::string const& id) {
    auto const* found = job(id);
    if (!found) return;
    fc::Job j = *found;
    auto existing = [&](std::vector<std::string> const& ids) {
        std::vector<std::string> list;
        for (auto const& reference : ids)
            if (this->reference(reference)) list.push_back(reference);
        return list;
    };
    prompt = j.prompt;
    if (j.kind == fc::MediaKind::video) {
        setMode(fc::MediaKind::video);
        preferences.videoResolution = j.settings.resolution;
        preferences.videoAspect = j.settings.aspect;
        preferences.videoDuration = j.settings.duration.value_or(preferences.videoDuration);
        preferences.videoAudio = j.settings.generateAudio.value_or(true);
        selectedImages = existing(j.settings.imageReferences);
        selectedVideos = existing(j.settings.videoReferences);
        selectedAudios = existing(j.settings.audioReferences);
    } else {
        setMode(fc::MediaKind::image);
        preferences.imageModel = j.model;
        preferences.imageResolution = j.settings.resolution;
        preferences.imageAspect = j.settings.aspect;
        preferences.camera = j.settings.camera.value_or(fc::presets::kNone);
        preferences.film = j.settings.film.value_or(fc::presets::kNone);
        selectedImages = existing(j.settings.imageReferences);
    }
    go(Section::create);
    notify(Change::form);
    show("Ajustes cargados. Revisalos y generá de nuevo.", Banner::Style::success);
}

wf::IAsyncOperation<bool> AppModel::lastFrameReference(std::string jobID, std::string name, std::shared_ptr<std::string> referenceID) {
    auto const* j = job(jobID);
    if (!j || j->outputs.empty()) co_return false;
    fs::path video = outputPath(j->outputs.front());
    fc::ReferenceFile reference;
    reference.id = fc::newUUID();
    reference.name = name.empty() ? "Último fotograma · " + fc::prefixCharacters(j->prompt, 40) : name;
    reference.kind = fc::ReferenceKind::image;
    reference.mime = "image/png";
    reference.fileName = fc::lower(reference.id) + ".png";
    reference.created = fc::nowMs();
    fs::path png = referencesDir() / widen(reference.fileName);
    std::optional<std::string> failure;
    co_await winrt::resume_background();
    try {
        media::saveLastFrame(video, png);
    } catch (fc::Error const& error) {
        failure = error.what();
    } catch (winrt::hresult_error const& error) {
        failure = narrow(error.message());
    } catch (...) {
        failure = "Windows no pudo leer el video.";
    }
    std::error_code error;
    if (!failure) reference.bytes = static_cast<int64_t>(fs::file_size(png, error));
    co_await wil::resume_foreground(dispatcher_);
    if (failure) {
        show("No se pudo extraer el último fotograma: " + *failure, Banner::Style::error);
        co_return false;
    }
    references.insert(references.begin(), reference);
    *referenceID = reference.id;
    notify(Change::references);
    co_return true;
}

winrt::fire_and_forget AppModel::continueFromLastFrame(std::string id) {
    auto referenceID = std::make_shared<std::string>();
    if (!co_await lastFrameReference(id, "", referenceID)) co_return;
    setMode(fc::MediaKind::video);
    if (static_cast<int>(selectedImages.size()) < selectionLimit(fc::ReferenceKind::image)) selectedImages.push_back(*referenceID);
    persist();
    go(Section::create);
    notify(Change::form);
    std::string tagText = "@Image" + std::to_string(selectedImages.size());
    show("Último fotograma agregado como " + tagText + ". Pedile al prompt que " + tagText + " sea el primer fotograma.", Banner::Style::success);
}

winrt::fire_and_forget AppModel::useAsReference(std::string id) {
    auto const* j = job(id);
    if (!j || j->outputs.empty()) co_return;
    importFiles({outputPath(j->outputs.front())});
    go(Section::create);
}

void AppModel::copy(std::string const& text) {
    copyToClipboard(text);
    show("Copiado al portapapeles.", Banner::Style::success);
}

// MARK: - Assistant

std::string AppModel::engineName() const {
    switch (preferences.provider) {
    case fc::PromptProvider::kie: return fc::prompts::promptModel(preferences.promptModel).name;
    case fc::PromptProvider::codex: return "ChatGPT (Codex)";
    case fc::PromptProvider::openai: return "OpenAI";
    }
    return "";
}

std::vector<std::string> AppModel::referenceTags() const {
    std::vector<std::string> tags;
    if (mode() != fc::MediaKind::video) return tags;
    for (auto kind : fc::kAllReferenceKinds) {
        int index = 0;
        for (auto const& r : selectedReferences(kind)) {
            std::string line = std::string(fc::tagPrefix(kind)) + std::to_string(++index) + " = " + r.name;
            if (r.durationMs) line += " (" + fc::media::formattedDuration(r.durationSeconds()) + ")";
            tags.push_back(line);
        }
    }
    return tags;
}

winrt::fire_and_forget AppModel::buildPrompt(bool refine) {
    std::string ideaText = fc::trim(idea);
    if (ideaText.empty() && !(refine && !draft.empty())) {
        assistantError = "Contame primero tu idea.";
        notify(Change::assistant);
        co_return;
    }
    if (fc::characterCount(ideaText) > 6000) {
        assistantError = "La idea puede tener hasta 6000 caracteres.";
        notify(Change::assistant);
        co_return;
    }
    fc::PromptProvider provider = preferences.provider;
    std::string apiKey;
    if (provider == fc::PromptProvider::kie) {
        auto value = key(credentials::kKie);
        if (!value) {
            assistantError = "Conectá tu clave de KIE en Ajustes para crear prompts.";
            notify(Change::assistant);
            co_return;
        }
        apiKey = *value;
    } else if (provider == fc::PromptProvider::openai) {
        auto value = key(credentials::kOpenAI);
        if (!value) {
            assistantError = "Agregá tu clave de OpenAI en Ajustes o elegí otro motor.";
            notify(Change::assistant);
            co_return;
        }
        apiKey = *value;
    }

    fc::MediaKind media = mode();
    fc::Skill const* current = skill(skillID);
    bool structured = media == fc::MediaKind::image && current && current->id == fc::skills::kJsonProfileID;
    std::optional<json> skillData;
    std::vector<fc::PromptExample> examples;
    if (structured && resources) {
        skillData = fc::skills::jsonProfileContext(ideaText, *resources, &examples);
    } else if (current && current->id != fc::skills::kJsonProfileID) {
        std::string content = current->content;
        if (current->id == fc::skills::kUgcID && includeWalterExample)
            content += "\n\n---\n\n# Ejemplo trabajado completo (pack de Walter, 8 escenas que funcionaron)\n\n" +
                       fc::skills::walterExample(resources ? &*resources : nullptr);
        skillData = json{{"name", current->name}, {"content", content}};
    }
    fc::PromptBrief brief;
    brief.idea = ideaText.empty() ? "(ver previousPrompt)" : ideaText;
    brief.media = media;
    brief.targetModel = media == fc::MediaKind::image ? preferences.imageModel : fc::presets::kVideoModelID;
    brief.aspect = media == fc::MediaKind::image ? preferences.imageAspect : preferences.videoAspect;
    brief.resolution = media == fc::MediaKind::image ? preferences.imageResolution : preferences.videoResolution;
    if (media == fc::MediaKind::image) {
        brief.camera = preferences.camera;
        brief.film = preferences.film;
    } else {
        brief.duration = preferences.videoDuration;
        brief.generateAudio = preferences.videoAudio;
        brief.dialogueLanguage = preferences.dialogueLanguage;
    }
    brief.referenceTags = referenceTags();
    if (refine) {
        brief.previousPrompt = draft;
        brief.feedback = fc::trim(feedback);
    }
    std::string briefText = fc::prompts::briefJSON(brief, skillData);
    std::string instructions = fc::prompts::instructions(media, structured);
    fc::PromptModel model = fc::prompts::promptModel(preferences.promptModel);

    // Images the assistant should look at (up to 4).
    std::vector<fs::path> imagePaths;
    std::vector<std::string> imageMimes, imageNames;
    int64_t analysisBytes = 0;
    for (size_t i = 0; i < selectedImages.size() && i < 4; ++i) {
        if (auto const* r = reference(selectedImages[i])) {
            imagePaths.push_back(referencePath(*r));
            imageMimes.push_back(r->mime);
            imageNames.push_back(r->name);
            analysisBytes += r->bytes;
        }
    }
    if (provider != fc::PromptProvider::codex && analysisBytes > fc::prompts::kMaxAnalysisBytes) {
        assistantError = "Para analizar referencias, elegí imágenes que sumen 8 MB o menos (para generar se admiten más grandes).";
        notify(Change::assistant);
        co_return;
    }

    assistantError.reset();
    isBuildingPrompt = true;
    streamingText = "";
    notify(Change::assistant);

    auto dispatcher = dispatcher_;
    auto live = [this, dispatcher](std::string const& text) {
        dispatcher.TryEnqueue([this, text] {
            if (isBuildingPrompt) {
                streamingText = text;
                notify(Change::assistant);
            }
        });
    };
    std::optional<fc::PromptResult> result;
    std::optional<std::string> failure;
    co_await winrt::resume_background();
    try {
        std::string raw;
        if (provider == fc::PromptProvider::kie) {
            KieService client(apiKey);
            std::vector<std::string> urls;
            for (size_t i = 0; i < imagePaths.size(); ++i)
                urls.push_back(client.upload(imagePaths[i], imageMimes[i], imageNames[i],
                                             fc::newUUID() + "." + fc::media::fileExtension(imageMimes[i])));
            raw = client.runModel(model, instructions, briefText, urls, 8000, live);
            if (fc::trim(raw).empty()) throw fc::Error("KIE no devolvió texto. Tu idea sigue ahí: probá de nuevo.", true);
        } else if (provider == fc::PromptProvider::codex) {
            raw = codex::generate(fc::prompts::codexCLIPrompt(instructions, briefText), imagePaths);
        } else {
            std::vector<std::string> dataURLs;
            for (size_t i = 0; i < imagePaths.size(); ++i) {
                std::string bytes = readFileBytes(imagePaths[i]);
                // Base64 through CryptBinaryToStringA.
                DWORD length = 0;
                CryptBinaryToStringA(reinterpret_cast<BYTE const*>(bytes.data()), static_cast<DWORD>(bytes.size()),
                                     CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &length);
                std::string encoded(length, '\0');
                CryptBinaryToStringA(reinterpret_cast<BYTE const*>(bytes.data()), static_cast<DWORD>(bytes.size()),
                                     CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded.data(), &length);
                encoded.resize(length);
                dataURLs.push_back("data:" + imageMimes[i] + ";base64," + encoded);
            }
            raw = fc::prompts::readOpenAI(openAIResponses(apiKey, fc::prompts::openAIBody(instructions, briefText, dataURLs, media, structured)));
        }
        result = fc::prompts::finalize(raw, media, structured);
    } catch (fc::Error const& error) {
        failure = error.what();
    } catch (winrt::hresult_error const& error) {
        failure = narrow(error.message());
    } catch (std::exception const& error) {
        failure = error.what();
    }
    co_await wil::resume_foreground(dispatcher_);
    isBuildingPrompt = false;
    streamingText.reset();
    if (result && media == fc::MediaKind::image &&
        fc::characterCount(fc::presets::composePrompt(result->prompt, preferences.camera, preferences.film, preferences.imageAspect)) >
            static_cast<size_t>(fc::presets::maxComposedLength(preferences.imageModel))) {
        failure = "El prompt quedó demasiado largo para este modelo. Pedí una descripción más concisa.";
        result.reset();
    }
    if (result) {
        draft = result->prompt;
        notes = result->notes;
        sources = examples;
        draftHistory.push_back(result->prompt);
        if (draftHistory.size() > 20) draftHistory.erase(draftHistory.begin());
        if (refine) feedback.clear();
        persist();
    } else {
        assistantError = failure.value_or("No se pudo crear el prompt.");
    }
    notify(Change::assistant);
}

winrt::fire_and_forget AppModel::testPromptModel() {
    auto apiKey = key(credentials::kKie);
    if (!apiKey) {
        connectionTest = "Conectá tu clave de KIE primero.";
        notify(Change::assistant);
        co_return;
    }
    fc::PromptModel model = fc::prompts::promptModel(preferences.promptModel);
    isTestingConnection = true;
    connectionTest.reset();
    notify(Change::assistant);
    auto start = std::chrono::steady_clock::now();
    std::string outcome;
    co_await winrt::resume_background();
    try {
        std::string answer = KieService(*apiKey).runModel(model, "Reply with the single word OK.", "ping", {}, 200, [](std::string const&) {});
        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        char buffer[32];
        snprintf(buffer, sizeof buffer, "%.1f", seconds);
        outcome = "✓ " + model.name + " responde (" + buffer + " s): " + fc::prefixCharacters(answer, 40);
    } catch (fc::Error const& error) {
        outcome = "✗ " + model.name + ": " + error.what();
    } catch (...) {
        outcome = "✗ " + model.name + ": no respondió.";
    }
    co_await wil::resume_foreground(dispatcher_);
    isTestingConnection = false;
    connectionTest = outcome;
    notify(Change::assistant);
}

winrt::fire_and_forget AppModel::refreshCodexStatus() {
    codexStatus = {codex::State::checking, ""};
    notify(Change::codex);
    codex::Status status;
    co_await winrt::resume_background();
    try {
        status = codex::status();
    } catch (...) {
        status = {codex::State::notInstalled, ""};
    }
    co_await wil::resume_foreground(dispatcher_);
    codexStatus = status;
    notify(Change::codex);
}

winrt::fire_and_forget AppModel::codexLogin() {
    codexStatus = {codex::State::loggingIn, ""};
    notify(Change::codex);
    std::optional<std::string> failure;
    co_await winrt::resume_background();
    try {
        codex::login();
    } catch (fc::Error const& error) {
        failure = error.what();
    } catch (...) {
        failure = "No se completó el inicio de sesión.";
    }
    co_await wil::resume_foreground(dispatcher_);
    if (failure) show(*failure, Banner::Style::error);
    else show("Sesión de ChatGPT iniciada en Codex.", Banner::Style::success);
    refreshCodexStatus();
}

winrt::fire_and_forget AppModel::codexLogout() {
    co_await winrt::resume_background();
    codex::logout();
    co_await wil::resume_foreground(dispatcher_);
    refreshCodexStatus();
}

std::optional<std::string> AppModel::draftProfilePrompt() const { return fc::prompts::mainPromptFromProfile(draft); }

void AppModel::useDraft() {
    prompt = draft;
    go(Section::create);
    notify(Change::form);
    show("Prompt listo en el editor. Revisalo y generá.", Banner::Style::success);
}

void AppModel::useDraftText() {
    auto text = draftProfilePrompt();
    if (!text) return useDraft();
    prompt = *text;
    go(Section::create);
    notify(Change::form);
    show("Prompt de texto listo en el editor.", Banner::Style::success);
}

void AppModel::restoreDraft(size_t index) {
    if (index >= draftHistory.size()) return;
    draft = draftHistory[index];
    notify(Change::assistant);
}

// MARK: - Skills

std::optional<std::string> AppModel::importSkill(fs::path const& file) {
    fs::path source = file;
    std::error_code error;
    if (fs::is_directory(file, error)) source = file / L"SKILL.md";
    auto size = fs::file_size(source, error);
    if (error) return std::string("No se pudo leer el archivo.");
    if (size > 200000) return std::string("La skill es demasiado grande (máximo 200 KB).");
    std::string text = readFileBytes(source);
    if (fc::startsWith(text, "\xEF\xBB\xBF")) text.erase(0, 3);
    std::string fallback = source.filename() == L"SKILL.md" ? narrow(file.filename().wstring()) : narrow(source.stem().wstring());
    auto parsed = fc::skills::parse(text, fallback);
    if (parsed.body.empty()) return std::string("El archivo está vacío.");
    fc::Skill created{fc::newUUID(), fc::prefixCharacters(parsed.name, 100), parsed.summary, parsed.body,
                      fc::skills::guessMedia(parsed.name, parsed.summary, parsed.body), false, fc::nowMs()};
    customSkills.push_back(created);
    persist();
    notify(Change::skills);
    show("Skill “" + created.name + "” importada.", Banner::Style::success);
    return std::nullopt;
}

void AppModel::saveSkill(fc::Skill const& value) {
    auto it = std::find_if(customSkills.begin(), customSkills.end(), [&](auto const& s) { return s.id == value.id; });
    if (it != customSkills.end()) *it = value;
    else customSkills.push_back(value);
    persist();
    notify(Change::skills);
}

fc::Skill AppModel::duplicateSkill(fc::Skill const& source) {
    fc::Skill copy{fc::newUUID(), source.name + " (copia)", source.summary, source.content, source.media, false, fc::nowMs()};
    customSkills.push_back(copy);
    persist();
    notify(Change::skills);
    return copy;
}

void AppModel::deleteSkill(std::string const& id) {
    std::erase_if(customSkills, [&](auto const& s) { return s.id == id; });
    if (skillID == id) skillID = mode() == fc::MediaKind::video ? fc::skills::kUgcID : fc::skills::kGeneralID;
    persist();
    notify(Change::skills);
    notify(Change::assistant);
}

void AppModel::useSkill(fc::Skill const& value) {
    if (!fc::supports(value.media, mode())) setMode(value.media == fc::SkillMedia::video ? fc::MediaKind::video : fc::MediaKind::image);
    skillID = value.id;
    showAssistant = true;
    go(Section::create);
    notify(Change::assistant);
}

// MARK: - Stories

namespace {

/// The assistant engine as chosen in Create, captured on the UI thread.
struct EngineConfig {
    fc::PromptProvider provider = fc::PromptProvider::kie;
    std::string apiKey;
    fc::PromptModel model;
    std::vector<fs::path> images;
    std::vector<std::string> mimes;
    std::vector<std::string> names;
};

std::string runEngine(EngineConfig const& engine, std::string const& instructions, std::string const& brief, int maxTokens,
                      std::function<void(std::string const&)> const& onText) {
    switch (engine.provider) {
    case fc::PromptProvider::kie: {
        KieService client(engine.apiKey);
        std::vector<std::string> urls;
        for (size_t i = 0; i < engine.images.size(); ++i)
            urls.push_back(client.upload(engine.images[i], engine.mimes[i], engine.names[i],
                                         fc::newUUID() + "." + fc::media::fileExtension(engine.mimes[i])));
        return client.runModel(engine.model, instructions, brief, urls, maxTokens, onText);
    }
    case fc::PromptProvider::codex:
        return codex::generate(fc::prompts::codexCLIPrompt(instructions, brief), engine.images);
    case fc::PromptProvider::openai:
        return fc::prompts::readOpenAI(openAIResponses(
            engine.apiKey, fc::prompts::openAIBody(instructions, brief, {}, fc::MediaKind::video, false, maxTokens)));
    }
    throw fc::Error("Motor desconocido.", true);
}

}  // namespace

fc::Story* AppModel::story(std::string const& id) {
    for (auto& s : stories)
        if (s.id == id) return &s;
    return nullptr;
}

void AppModel::newStory(fc::StoryTemplate const* from) {
    fc::Story created;
    created.id = fc::newUUID();
    created.created = created.updated = fc::nowMs();
    if (from) {
        created.title = from->title;
        created.brief = from->brief;
        created.sceneCount = from->sceneCount;
        created.sceneDuration = from->sceneDuration;
    }
    stories.insert(stories.begin(), created);
    selectedStoryID = created.id;
    persist();
    go(Section::stories);
    notify(Change::stories);
}

void AppModel::deleteStory(std::string const& id) {
    std::erase_if(stories, [&](auto const& s) { return s.id == id; });
    if (selectedStoryID == id) selectedStoryID = stories.empty() ? "" : stories.front().id;
    persist();
    notify(Change::stories);
}

void AppModel::updateStory(std::string const& id, std::function<void(fc::Story&)> const& change) {
    if (auto* s = story(id)) {
        change(*s);
        s->updated = fc::nowMs();
    }
}

void AppModel::updateScene(std::string const& storyID, std::string const& sceneID, std::function<void(fc::StoryScene&)> const& change) {
    updateStory(storyID, [&](fc::Story& s) {
        for (auto& scene : s.scenes)
            if (scene.id == sceneID) change(scene);
    });
}

std::vector<std::string> AppModel::referenceLines(fc::Story const& s) const {
    std::map<std::string, std::string> names;
    for (auto const& r : references) names[r.id] = r.name;
    return fc::stories::referenceLines(s, names);
}

std::vector<std::string> AppModel::displayLines(fc::Story const& s) const {
    std::map<std::string, std::string> names;
    for (auto const& r : references) names[r.id] = r.name;
    return fc::stories::displayLines(s, names);
}

std::optional<json> AppModel::skillPayload(std::string const& id) const {
    auto const* found = skill(id);
    if (!found || found->id == fc::skills::kJsonProfileID) return std::nullopt;
    std::string content = found->content;
    if (found->id == fc::skills::kUgcID) {
        // Stories always get the worked example: it is the model of a full pack.
        content += "\n\n---\n\n# Ejemplo trabajado completo (pack de Walter, 8 escenas que funcionaron)\n\n" +
                   fc::skills::walterExample(resources ? &*resources : nullptr);
    }
    return json{{"name", found->name}, {"content", content}};
}

winrt::fire_and_forget AppModel::generateStory(std::string id, std::optional<std::string> feedbackText) {
    auto* current = story(id);
    if (!current || storyStatus) co_return;
    if (fc::trim(current->brief).empty()) {
        storyError = "Escribí el brief de la historia.";
        notify(Change::stories);
        co_return;
    }
    EngineConfig engine{preferences.provider, "", fc::prompts::promptModel(preferences.promptModel)};
    if (engine.provider != fc::PromptProvider::codex) {
        auto value = key(engine.provider == fc::PromptProvider::kie ? credentials::kKie : credentials::kOpenAI);
        if (!value) {
            storyError = engine.provider == fc::PromptProvider::kie ? "Conectá tu clave de KIE en Ajustes." : "Agregá tu clave de OpenAI en Ajustes o elegí otro motor.";
            notify(Change::stories);
            co_return;
        }
        engine.apiKey = *value;
    }
    for (auto const& slot : current->slots) {
        if (slot.kind != fc::ReferenceKind::image || slot.isLastFrame || !slot.referenceID || engine.images.size() >= 4) continue;
        if (auto const* r = reference(*slot.referenceID)) {
            engine.images.push_back(referencePath(*r));
            engine.mimes.push_back(r->mime);
            engine.names.push_back(r->name);
        }
    }
    bool revising = feedbackText.has_value() && !current->scenes.empty();
    storyError.reset();
    storyStatus = revising ? "Reescribiendo la historia…" : "Escribiendo la historia…";
    storyStreaming = "";
    if (feedbackText) current->messages.push_back({fc::newUUID(), true, *feedbackText, std::nullopt, fc::nowMs()});
    std::string brief = fc::stories::storyBrief(*current, referenceLines(*current), skillPayload(current->skillID), revising, feedbackText);
    int sceneDuration = current->sceneDuration;
    notify(Change::stories);

    auto dispatcher = dispatcher_;
    auto live = [this, dispatcher](std::string const& text) {
        dispatcher.TryEnqueue([this, text] {
            if (storyStatus) {
                storyStreaming = text;
                notify(Change::storyRun);
            }
        });
    };
    std::optional<fc::StoryDraft> result;
    std::optional<std::string> failure;
    co_await winrt::resume_background();
    try {
        result = fc::stories::parseStory(runEngine(engine, fc::stories::storyInstructions(), brief, 32000, live), sceneDuration);
    } catch (fc::Error const& error) {
        failure = error.what();
    } catch (winrt::hresult_error const& error) {
        failure = narrow(error.message());
    } catch (std::exception const& error) {
        failure = error.what();
    }
    co_await wil::resume_foreground(dispatcher_);
    storyStatus.reset();
    storyStreaming.reset();
    if (result) {
        updateStory(id, [&](fc::Story& s) {
            auto previous = s.scenes;
            s.title = result->title;
            s.summary = result->summary;
            s.continuity = result->continuity;
            s.referenceOrder = result->referenceOrder;
            s.scenes.clear();
            for (size_t i = 0; i < result->scenes.size(); ++i) {
                auto const& d = result->scenes[i];
                fc::StoryScene scene;
                // Keep links to videos already generated for unchanged scenes.
                bool same = i < previous.size() && previous[i].prompt == d.prompt;
                scene.id = i < previous.size() ? previous[i].id : fc::newUUID();
                scene.number = static_cast<int>(i) + 1;
                scene.title = d.title;
                scene.summary = d.summary;
                scene.duration = d.duration;
                scene.prompt = d.prompt;
                scene.notes = d.notes;
                if (same) scene.jobIDs = previous[i].jobIDs;
                s.scenes.push_back(scene);
            }
            s.messages.push_back({fc::newUUID(), false,
                                  revising ? "Listo: actualicé la historia (" + std::to_string(s.scenes.size()) + " escenas)."
                                           : "Historia creada: " + std::to_string(s.scenes.size()) + " escenas, " +
                                                 std::to_string(s.totalDuration()) + " s en total.",
                                  std::nullopt, fc::nowMs()});
        });
        persist();
    } else {
        storyError = failure.value_or("No se pudo crear la historia.");
        updateStory(id, [&](fc::Story& s) {
            s.messages.push_back({fc::newUUID(), false, "No pude completar el pedido: " + *storyError, std::nullopt, fc::nowMs()});
        });
    }
    notify(Change::stories);
}

winrt::fire_and_forget AppModel::refineScene(std::string storyID, std::string sceneID, std::string feedbackText) {
    auto* current = story(storyID);
    if (!current || storyStatus) co_return;
    std::string note = fc::trim(feedbackText);
    if (note.empty()) co_return;
    auto sceneIt = std::find_if(current->scenes.begin(), current->scenes.end(), [&](auto const& s) { return s.id == sceneID; });
    if (sceneIt == current->scenes.end()) co_return;
    fc::StoryScene scene = *sceneIt;
    EngineConfig engine{preferences.provider, "", fc::prompts::promptModel(preferences.promptModel)};
    if (engine.provider != fc::PromptProvider::codex) {
        auto value = key(engine.provider == fc::PromptProvider::kie ? credentials::kKie : credentials::kOpenAI);
        if (!value) {
            storyError = "Conectá tu clave en Ajustes o elegí otro motor en el asistente.";
            notify(Change::stories);
            co_return;
        }
        engine.apiKey = *value;
    }
    storyError.reset();
    storyStatus = "Ajustando la escena " + std::to_string(scene.number) + "…";
    storyStreaming = "";
    current->messages.push_back({fc::newUUID(), true, note, scene.number, fc::nowMs()});
    std::string brief = fc::stories::sceneBrief(*current, scene, referenceLines(*current), skillPayload(current->skillID), note);
    notify(Change::stories);
    auto dispatcher = dispatcher_;
    auto live = [this, dispatcher](std::string const& text) {
        dispatcher.TryEnqueue([this, text] {
            if (storyStatus) {
                storyStreaming = text;
                notify(Change::storyRun);
            }
        });
    };
    std::optional<fc::StoryDraft::Scene> revised;
    std::optional<std::string> failure;
    co_await winrt::resume_background();
    try {
        revised = fc::stories::parseSingleScene(runEngine(engine, fc::stories::sceneInstructions(), brief, 12000, live), scene.duration);
    } catch (fc::Error const& error) {
        failure = error.what();
    } catch (winrt::hresult_error const& error) {
        failure = narrow(error.message());
    } catch (std::exception const& error) {
        failure = error.what();
    }
    co_await wil::resume_foreground(dispatcher_);
    storyStatus.reset();
    storyStreaming.reset();
    if (revised) {
        updateScene(storyID, sceneID, [&](fc::StoryScene& s) {
            s.title = revised->title;
            s.summary = revised->summary;
            s.duration = revised->duration;
            s.prompt = revised->prompt;
            s.notes = revised->notes;
        });
        updateStory(storyID, [&](fc::Story& s) {
            s.messages.push_back({fc::newUUID(), false, "Escena " + std::to_string(scene.number) + " actualizada.", scene.number, fc::nowMs()});
        });
        persist();
    } else {
        storyError = failure.value_or("No se pudo ajustar la escena.");
    }
    notify(Change::stories);
}

fc::Job const* AppModel::latestJob(fc::StoryScene const& scene) const {
    for (auto it = scene.jobIDs.rbegin(); it != scene.jobIDs.rend(); ++it)
        for (auto const& j : jobs)
            if (j.id == *it) return &j;
    return nullptr;
}

fc::Job const* AppModel::finishedClip(fc::StoryScene const& scene) const {
    for (auto it = scene.jobIDs.rbegin(); it != scene.jobIDs.rend(); ++it)
        for (auto const& j : jobs)
            if (j.id == *it && j.status == fc::JobStatus::success && !j.outputs.empty()) return &j;
    return nullptr;
}

wf::IAsyncOperation<bool> AppModel::openScene(std::string storyID, std::string sceneID, bool generateNow) {
    auto* current = story(storyID);
    if (!current) co_return false;
    auto sceneIt = std::find_if(current->scenes.begin(), current->scenes.end(), [&](auto const& s) { return s.id == sceneID; });
    if (sceneIt == current->scenes.end()) co_return false;
    size_t sceneIndex = static_cast<size_t>(sceneIt - current->scenes.begin());
    fc::StoryScene scene = *sceneIt;
    fc::Story snapshot = *current;
    std::map<fc::ReferenceKind, std::vector<std::string>> selected;
    for (auto kind : fc::kAllReferenceKinds) {
        int needed = fc::linter::highestTag(fc::tagPrefix(kind), scene.prompt);
        auto slots = snapshot.slotsOf(kind);
        std::vector<std::string> ids;
        for (int i = 0; i < needed && i < static_cast<int>(slots.size()); ++i) {
            auto const& slot = slots[i];
            if (slot.isLastFrame) {
                fc::Job const* previous = sceneIndex > 0 ? finishedClip(snapshot.scenes[sceneIndex - 1]) : nullptr;
                if (!previous) {
                    show(snapshot.tag(slot) + " es el último fotograma de la escena anterior: generá primero la escena " +
                             std::to_string(scene.number - 1) + ".",
                         Banner::Style::error);
                    co_return false;
                }
                auto frameID = std::make_shared<std::string>();
                std::string previousID = previous->id;
                if (!co_await lastFrameReference(previousID, snapshot.title + " · último fotograma escena " + std::to_string(scene.number - 1), frameID))
                    co_return false;
                ids.push_back(*frameID);
            } else if (slot.referenceID && reference(*slot.referenceID)) {
                ids.push_back(*slot.referenceID);
            } else {
                show("Falta el archivo de " + snapshot.tag(slot) + ". Elegilo en las referencias de la historia.", Banner::Style::error);
                co_return false;
            }
        }
        selected[kind] = ids;
    }
    setMode(fc::MediaKind::video);
    prompt = scene.prompt;
    preferences.videoDuration = fc::presets::clampDuration(scene.duration);
    preferences.videoAspect = snapshot.aspect;
    preferences.videoResolution = snapshot.resolution;
    preferences.videoAudio = snapshot.generateAudio;
    skillID = snapshot.skillID;
    selectedImages = selected[fc::ReferenceKind::image];
    selectedVideos = selected[fc::ReferenceKind::video];
    selectedAudios = selected[fc::ReferenceKind::audio];
    pendingSceneLink_ = std::make_pair(storyID, sceneID);
    persist();
    notify(Change::form);
    if (generateNow) {
        generate();
        show("Escena " + std::to_string(scene.number) + " enviada a Seedance.", Banner::Style::success);
    } else {
        go(Section::create);
        show("Escena " + std::to_string(scene.number) + " cargada en Crear con sus referencias en orden.", Banner::Style::success);
    }
    co_return true;
}

winrt::fire_and_forget AppModel::generateAllScenes(std::string id) {
    auto* current = story(id);
    if (storyRun || !current || current->scenes.empty()) co_return;
    if (!hasKieKey) {
        if (openOnboarding) openOnboarding();
        co_return;
    }
    bool chained = current->usesLastFrame();
    std::vector<std::string> sceneIDs;
    for (auto const& scene : current->scenes) sceneIDs.push_back(scene.id);
    int total = static_cast<int>(sceneIDs.size());
    storyRunCancelled_ = false;
    storyRun = StoryRun{id, 0, total, "Preparando…"};
    notify(Change::storyRun);
    int sent = 0;
    auto sceneOf = [this, id](std::string const& sceneID) -> fc::StoryScene const* {
        auto* s = story(id);
        if (!s) return nullptr;
        for (auto const& scene : s->scenes)
            if (scene.id == sceneID) return &scene;
        return nullptr;
    };
    for (int index = 0; index < total; ++index) {
        if (storyRunCancelled_) break;
        auto const* scene = sceneOf(sceneIDs[index]);
        if (!scene) break;
        storyRun = StoryRun{id, index + 1, total, "Escena " + std::to_string(index + 1) + " de " + std::to_string(total)};
        notify(Change::storyRun);
        fc::Job const* existing = latestJob(*scene);
        std::string existingID = existing ? existing->id : "";
        if (!existing || existing->status == fc::JobStatus::fail || existing->status == fc::JobStatus::unknown) {
            storyRun->text = "Enviando la escena " + std::to_string(index + 1) + " de " + std::to_string(total) + "…";
            notify(Change::storyRun);
            if (!co_await openScene(id, sceneIDs[index], true)) break;
            // generate() runs on its own: wait until the job appears (or the submission fails).
            for (int wait = 0; wait < 600 && (isGenerating || !pendingSceneLink_.has_value()); ++wait) {
                auto const* now = sceneOf(sceneIDs[index]);
                fc::Job const* latest = now ? latestJob(*now) : nullptr;
                if (latest && latest->id != existingID) break;
                if (!isGenerating) break;
                co_await winrt::resume_after(std::chrono::milliseconds(500));
                co_await wil::resume_foreground(dispatcher_);
            }
            auto const* now = sceneOf(sceneIDs[index]);
            fc::Job const* latest = now ? latestJob(*now) : nullptr;
            if (!latest || latest->id == existingID) {
                show("No se pudo enviar la escena " + std::to_string(index + 1) + ". Revisala y volvé a tocar “Generar todo”.", Banner::Style::error);
                break;
            }
            ++sent;
        }
        if (chained && index < total - 1) {
            storyRun->text = "Esperando que termine la escena " + std::to_string(index + 1) + " para encadenar la " + std::to_string(index + 2) + "…";
            notify(Change::storyRun);
            for (;;) {
                if (storyRunCancelled_) break;
                auto const* now = sceneOf(sceneIDs[index]);
                fc::Job const* latest = now ? latestJob(*now) : nullptr;
                if (!latest || !fc::isActive(latest->status)) break;
                co_await winrt::resume_after(std::chrono::seconds(4));
                co_await wil::resume_foreground(dispatcher_);
            }
            if (storyRunCancelled_) break;
            auto const* now = sceneOf(sceneIDs[index]);
            if (!now || !finishedClip(*now)) {
                show("La escena " + std::to_string(index + 1) + " falló: ajustala y volvé a tocar “Generar todo” (sigue desde ahí).", Banner::Style::error);
                break;
            }
        }
        if (index == total - 1) {
            show(sent == 0 ? "Todas las escenas ya tenían video."
                           : "Listo: " + std::to_string(sent) + (sent == 1 ? " escena en camino." : " escenas en camino.") + " Cuando terminen, armá el video final.",
                 Banner::Style::success);
        }
    }
    storyRun.reset();
    notify(Change::storyRun);
    notify(Change::stories);
}

void AppModel::cancelStoryRun() {
    storyRunCancelled_ = true;
    storyRun.reset();
    notify(Change::storyRun);
    show("Se detuvo “Generar todo”. Las escenas ya enviadas siguen generándose.");
}

winrt::fire_and_forget AppModel::assembleStory(std::string id) {
    auto* current = story(id);
    if (assemblingStoryID || !current) co_return;
    std::vector<fs::path> clips;
    std::vector<std::string> missing;
    for (auto const& scene : current->scenes) {
        if (auto const* clip = finishedClip(scene)) clips.push_back(outputPath(clip->outputs.front()));
        else missing.push_back(std::to_string(scene.number));
    }
    if (!missing.empty()) {
        show(std::string(missing.size() == 1 ? "Falta el video de la escena " : "Faltan videos de las escenas ") + fc::join(missing, ", ") + ".",
             Banner::Style::error);
        co_return;
    }
    assemblingStoryID = id;
    notify(Change::storyRun);
    fc::Story snapshot = *current;
    std::string fileName = "framecraft-historia-" + fileTimestamp() + "-" + fc::lower(id.substr(0, 4)) + ".mp4";
    fs::path destination = outputPath(fileName);
    std::optional<std::string> failure;
    co_await winrt::resume_background();
    try {
        media::concatenate(clips, destination);
    } catch (fc::Error const& error) {
        failure = error.what();
    } catch (winrt::hresult_error const& error) {
        failure = narrow(error.message());
    } catch (...) {
        failure = "Windows no pudo unir los videos.";
    }
    co_await wil::resume_foreground(dispatcher_);
    assemblingStoryID.reset();
    if (failure) {
        show("No se pudo armar el video final: " + *failure, Banner::Style::error);
        notify(Change::storyRun);
        co_return;
    }
    fc::Job cut;
    cut.id = fc::newUUID();
    cut.batchID = fc::newUUID();
    cut.kind = fc::MediaKind::video;
    cut.model = fc::presets::kStoryCutModelID;
    cut.prompt = "Historia completa: " + snapshot.title;
    cut.finalPrompt = snapshot.summary;
    cut.settings.resolution = snapshot.resolution;
    cut.settings.aspect = snapshot.aspect;
    cut.settings.duration = snapshot.totalDuration();
    cut.settings.generateAudio = snapshot.generateAudio;
    cut.status = fc::JobStatus::success;
    cut.progress = 100;
    cut.outputs = {fileName};
    cut.created = cut.updated = fc::nowMs();
    jobs.insert(jobs.begin(), cut);
    updateStory(id, [&](fc::Story& s) { s.finalCutJobID = cut.id; });
    persist();
    notify(Change::jobs);
    notify(Change::stories);
    notify(Change::storyRun);
    show("Video final listo: " + std::to_string(snapshot.scenes.size()) + " escenas, " + std::to_string(snapshot.totalDuration()) +
             " s. Está en la Biblioteca.",
         Banner::Style::success);
    if (openMedia) openMedia(destination);
}

winrt::fire_and_forget AppModel::exportStory(std::string id) {
    auto* current = story(id);
    if (!current) co_return;
    std::string text = fc::stories::exportText(*current, displayLines(*current));
    winrt::Windows::Storage::Pickers::FileSavePicker picker;
    picker.as<::IInitializeWithWindow>()->Initialize(hwnd);
    auto extensions = winrt::single_threaded_vector<winrt::hstring>({L".txt"});
    picker.FileTypeChoices().Insert(L"Texto", extensions);
    picker.SuggestedFileName(hs(current->title + " - pack de prompts"));
    try {
        auto file = co_await picker.PickSaveFileAsync();
        if (!file) co_return;
        std::string bytes = "\xEF\xBB\xBF" + fc::replaceAll(text, "\n", "\r\n");
        if (writeFileBytes(fs::path(std::wstring(file.Path())), bytes)) show("Pack exportado.", Banner::Style::success);
        else show("No se pudo exportar el pack.", Banner::Style::error);
    } catch (...) {
        show("No se pudo exportar el pack.", Banner::Style::error);
    }
}

}  // namespace fcapp
