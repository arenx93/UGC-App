#pragma once
// App state and actions. Everything the interface does goes through here (UI thread only).
// Port of macos/Sources/Framecraft/AppModel.swift.

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Services.h"
#include "framecraft/library.h"
#include "framecraft/linter.h"
#include "framecraft/presets.h"
#include "framecraft/prompts.h"
#include "framecraft/skills.h"
#include "framecraft/stories.h"
#include "framecraft/templates.h"

namespace fcapp {

enum class Section { create, chat, stories, library, references, skills, guide, settings };
std::string sectionTitle(Section section);
std::wstring sectionGlyph(Section section);
inline const Section kSidebarSections[] = {Section::create, Section::chat, Section::stories, Section::library,
                                           Section::references, Section::skills, Section::guide};

enum class LibraryFilter { all, images, videos, active, favorites, failed };
std::string filterTitle(LibraryFilter filter);

/// What changed, so each screen refreshes only what it shows.
enum class Change { jobs, references, skills, stories, account, form, assistant, chat, storyRun, banner, navigation, codex, all };

struct ChatMessage {
    enum class Role { user, assistant };
    Role role = Role::user;
    std::string text;
};

struct ChatConversation {
    std::string id;
    std::string title;
    std::vector<ChatMessage> messages;
    int64_t created = 0;
    int64_t updated = 0;
};

struct Banner {
    enum class Style { success, info, error };
    std::string text;
    Style style = Style::info;
    int64_t id = 0;
};

/// Progress of "Generar todas las escenas".
struct StoryRun {
    std::string storyID;
    int current = 0;
    int total = 0;
    std::string text;
};

class AppModel {
public:
    AppModel(winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher, bool demo);

    static AppModel& shared();
    static void create(winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher, bool demo);

    // MARK: Observation
    using Listener = std::function<void(Change)>;
    void subscribe(Listener listener);
    void notify(Change change);

    // MARK: Paths
    std::filesystem::path indexFile;
    std::filesystem::path mediaRoot;
    std::filesystem::path generationsDir() const { return mediaRoot / L"Generaciones"; }
    std::filesystem::path referencesDir() const { return mediaRoot / L"Referencias"; }
    std::filesystem::path outputPath(std::string const& fileName) const;
    std::filesystem::path referencePath(fc::ReferenceFile const& reference) const;
    std::vector<std::filesystem::path> outputPaths(fc::Job const& job) const;

    // MARK: Navigation
    Section section = Section::create;
    bool showAssistant = false;
    std::optional<Banner> banner;
    void go(Section target);
    void show(std::string const& text, Banner::Style style = Banner::Style::info);
    /// Set by the window: opens dialogs (job detail, onboarding, palette, prompt preview).
    std::function<void(std::string const& jobID)> openJobDetail;
    std::function<void()> openOnboarding;
    std::function<void()> openPalette;
    std::function<void()> openPromptPreview;
    std::function<void(std::filesystem::path const&)> openMedia;
    bool onboardingNeeded() const { return !demo_ && !preferences.onboardingDone; }
    void finishOnboarding();

    // MARK: Library
    std::vector<fc::Job> jobs;
    std::vector<fc::ReferenceFile> references;
    std::vector<fc::Skill> customSkills;
    std::vector<fc::Skill> builtinSkills;
    std::vector<fc::Story> stories;
    fc::Preferences preferences;
    std::optional<fc::SkillResources> resources;
    LibraryFilter filter = LibraryFilter::all;
    std::string search;

    std::vector<fc::Skill> allSkills() const;
    std::vector<fc::Skill> skillsFor(fc::MediaKind kind) const;
    fc::Skill const* skill(std::string const& id) const;
    std::vector<fc::Job> activeJobs() const;
    std::vector<fc::Job> filteredJobs() const;
    int completedCount() const;
    fc::Job* job(std::string const& id);
    fc::ReferenceFile const* reference(std::string const& id) const;
    std::vector<fc::ReferenceFile> referencesOf(fc::ReferenceKind kind) const;
    void persist();

    // MARK: Account
    bool hasKieKey = false;
    bool hasOpenAIKey = false;
    std::optional<double> credits;
    bool checkingCredits = false;
    winrt::fire_and_forget saveKieKey(std::string raw, std::function<void(std::optional<std::string> error)> done);
    void removeKieKey();
    std::optional<std::string> saveOpenAIKey(std::string const& raw);
    void removeOpenAIKey();
    winrt::fire_and_forget refreshCredits();

    // MARK: Create form
    fc::MediaKind mode() const { return preferences.mode; }
    void setMode(fc::MediaKind mode);
    std::string prompt;
    std::vector<std::string> selectedImages, selectedVideos, selectedAudios;
    bool isGenerating = false;
    std::string generationStep;
    bool isImporting = false;
    void setImageModel(std::string const& id);
    void setImageResolution(std::string const& resolution);
    std::vector<std::string> aspectOptions() const;
    std::string finalPrompt() const;
    std::optional<fc::LintReport> lintReport() const;
    std::optional<std::string> generateBlocker() const;
    std::string settingsSummary() const;
    void insertTag(std::string const& tag);
    void applyTemplate(fc::CreativeTemplate const& creative);

    // MARK: References
    std::vector<std::string>& selection(fc::ReferenceKind kind);
    std::vector<std::string> const& selection(fc::ReferenceKind kind) const;
    std::vector<fc::ReferenceFile> selectedReferences(fc::ReferenceKind kind) const;
    int selectionLimit(fc::ReferenceKind kind) const;
    double selectedSeconds(fc::ReferenceKind kind) const;
    bool isSelected(fc::ReferenceFile const& reference) const;
    std::optional<std::string> tag(fc::ReferenceFile const& reference) const;
    void toggleSelection(fc::ReferenceFile const& reference);
    void moveSelection(fc::ReferenceKind kind, std::string const& id, int offset);
    winrt::fire_and_forget importFiles(std::vector<std::filesystem::path> files, bool select = true);
    void deleteReferences(std::vector<std::string> const& ids);
    winrt::fire_and_forget pickAndImport();

    // MARK: Generation
    winrt::fire_and_forget generate();
    void startPolling();

    // MARK: Library actions
    void deleteJob(std::string const& id);
    void toggleFavorite(std::string const& id);
    void reuse(std::string const& id);
    winrt::fire_and_forget continueFromLastFrame(std::string id);
    winrt::fire_and_forget useAsReference(std::string id);
    void copy(std::string const& text);

    // MARK: Assistant
    std::string idea;
    std::string skillID;
    bool includeWalterExample = false;
    std::string draft;
    std::optional<std::string> notes;
    std::vector<fc::PromptExample> sources;
    std::vector<std::string> draftHistory;
    std::string feedback;
    bool isBuildingPrompt = false;
    std::optional<std::string> streamingText;
    std::optional<std::string> assistantError;
    codex::Status codexStatus;
    std::optional<std::string> connectionTest;
    bool isTestingConnection = false;
    std::string engineName() const;
    std::vector<std::string> referenceTags() const;
    bool isUGCSkill() const { return skillID == fc::skills::kUgcID; }
    winrt::fire_and_forget buildPrompt(bool refine = false);
    winrt::fire_and_forget testPromptModel();
    winrt::fire_and_forget refreshCodexStatus();
    winrt::fire_and_forget codexLogin();
    winrt::fire_and_forget codexLogout();
    std::optional<std::string> draftProfilePrompt() const;
    void useDraft();
    void useDraftText();
    void restoreDraft(size_t index);

    // MARK: ChatGPT
    std::vector<ChatConversation> chats;
    std::string selectedChatID;
    bool isChatting = false;
    std::optional<std::string> chatError;
    ChatConversation* selectedChat();
    ChatConversation const* selectedChat() const;
    void newChat();
    void selectChat(std::string const& id);
    void deleteChat(std::string const& id);
    winrt::fire_and_forget sendChat(std::string message);
    void clearChat();

    // MARK: Skills
    std::optional<std::string> importSkill(std::filesystem::path const& file);
    void saveSkill(fc::Skill const& skill);
    fc::Skill duplicateSkill(fc::Skill const& skill);
    void deleteSkill(std::string const& id);
    void useSkill(fc::Skill const& skill);

    // MARK: Stories
    std::string selectedStoryID;
    std::optional<std::string> storyStatus;
    std::optional<std::string> storyStreaming;
    std::optional<std::string> storyError;
    std::optional<StoryRun> storyRun;
    std::optional<std::string> assemblingStoryID;
    fc::Story* story(std::string const& id);
    void newStory(fc::StoryTemplate const* from = nullptr);
    void deleteStory(std::string const& id);
    void updateStory(std::string const& id, std::function<void(fc::Story&)> const& change);
    void updateScene(std::string const& storyID, std::string const& sceneID, std::function<void(fc::StoryScene&)> const& change);
    std::vector<std::string> referenceLines(fc::Story const& story) const;
    std::vector<std::string> displayLines(fc::Story const& story) const;
    winrt::fire_and_forget generateStory(std::string id, std::optional<std::string> feedback);
    winrt::fire_and_forget refineScene(std::string storyID, std::string sceneID, std::string feedback);
    /// Loads a scene into Create with its references in order. With `generate`, sends it right away.
    winrt::Windows::Foundation::IAsyncOperation<bool> openScene(std::string storyID, std::string sceneID, bool generate);
    winrt::fire_and_forget generateAllScenes(std::string id);
    void cancelStoryRun();
    winrt::fire_and_forget assembleStory(std::string id);
    winrt::fire_and_forget exportStory(std::string id);
    fc::Job const* latestJob(fc::StoryScene const& scene) const;
    fc::Job const* finishedClip(fc::StoryScene const& scene) const;

    // MARK: System
    HWND hwnd = nullptr;
    bool demo() const { return demo_; }
    void updateTaskbar();

private:
    winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher_{nullptr};
    winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer pollTimer_{nullptr};
    bool demo_ = false;
    bool polling_ = false;
    bool storyRunCancelled_ = false;
    std::optional<std::pair<std::string, std::string>> pendingSceneLink_;
    std::vector<Listener> listeners_;
    std::map<std::wstring, std::string> demoKeys_;
    std::filesystem::path chatsFile_;

    std::optional<std::string> key(wchar_t const* account) const;
    bool writeKey(wchar_t const* account, std::string const& value);
    void deleteKey(wchar_t const* account);

    void fixAspect();
    void modeChanged();
    void update(std::string const& id, std::function<void(fc::Job&)> const& change);
    winrt::Windows::Foundation::IAsyncAction pollOnce();
    void markInterruptedSubmissions();
    void notifyFinished(std::string const& id);
    std::optional<fc::json> skillPayload(std::string const& id) const;
    winrt::Windows::Foundation::IAsyncOperation<bool> lastFrameReference(std::string jobID, std::string name,
                                                                          std::shared_ptr<std::string> referenceID);
    void loadDemoData();
    void loadChats();
    void persistChats() const;
};

}  // namespace fcapp
