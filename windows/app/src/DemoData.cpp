#include "pch.h"

#include "AppModel.h"
#include "WinUtil.h"
#include "framecraft/util.h"

namespace fs = std::filesystem;

namespace fcapp {

namespace {

struct Rgb {
    double r, g, b;
};

/// A 24-bit BMP with a diagonal gradient and a soft circle (demo thumbnails).
std::string demoBitmap(int width, int height, Rgb from, Rgb to) {
    int rowSize = (width * 3 + 3) & ~3;
    int dataSize = rowSize * height;
    std::string bytes(54 + dataSize, '\0');
    auto put32 = [&](size_t at, uint32_t value) {
        for (int i = 0; i < 4; ++i) bytes[at + i] = static_cast<char>((value >> (8 * i)) & 0xFF);
    };
    auto put16 = [&](size_t at, uint16_t value) {
        bytes[at] = static_cast<char>(value & 0xFF);
        bytes[at + 1] = static_cast<char>(value >> 8);
    };
    bytes[0] = 'B';
    bytes[1] = 'M';
    put32(2, static_cast<uint32_t>(bytes.size()));
    put32(10, 54);
    put32(14, 40);
    put32(18, static_cast<uint32_t>(width));
    put32(22, static_cast<uint32_t>(height));
    put16(26, 1);
    put16(28, 24);
    put32(34, static_cast<uint32_t>(dataSize));
    double cx = width * 0.5, cy = height * 0.58, radius = width * 0.26;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double t = (static_cast<double>(x) / width * 0.5 + static_cast<double>(height - y) / height * 0.5);
            double r = from.r + (to.r - from.r) * t, g = from.g + (to.g - from.g) * t, b = from.b + (to.b - from.b) * t;
            double dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy < radius * radius) {
                r = r + (255 - r) * 0.28;
                g = g + (255 - g) * 0.28;
                b = b + (255 - b) * 0.28;
            }
            size_t at = 54 + static_cast<size_t>(y) * rowSize + static_cast<size_t>(x) * 3;
            bytes[at] = static_cast<char>(b);
            bytes[at + 1] = static_cast<char>(g);
            bytes[at + 2] = static_cast<char>(r);
        }
    }
    return bytes;
}

const std::string kVideoPrompt =
    "FORMAT: Vertical 9:16, 15 seconds, one continuous take, no cuts. Rear 1x phone camera, wide lens ~26mm, held in one hand at chest height, constant breathing micro-shake, reframes half a second late. Deep depth of field: the bathroom stays fully readable.\n"
    "VOICE DIRECTION: text in parentheses is acting direction only and is NEVER spoken aloud.\n"
    "0–5 s: P1 lifts @Image1 next to her face in front of the mirror. P1 (half asleep, voice still low): \"Okay, I've been using this every morning for a month.\"\n"
    "5–10 s: She tilts the jar, the autoexposure hunts when she passes the window. P1 (faster, a little laugh): \"And my skin finally stopped fighting me, look.\"\n"
    "10–15 s: She leans closer to the phone, the frame drops slightly as her arm gets tired. P1 (quieter): \"Not sponsored. I just really like it.\"\n"
    "AUDIO: diegetic phone microphone, small bathroom echo, tap dripping. Match @Audio1 for timbre, age, accent and pitch only. Do not reproduce any words from @Audio1 and do not copy its emotion.\n"
    "RESTRICTIONS: no music, no on-screen text, no gimbal, no bokeh, no beauty filter.";

}  // namespace

std::string demoVideoPrompt() { return kVideoPrompt; }

void AppModel::loadDemoData() {
    const Rgb palettes[][2] = {
        {{255, 149, 0}, {255, 45, 85}},  {{48, 176, 199}, {175, 82, 222}}, {{255, 45, 85}, {88, 86, 214}},
        {{255, 204, 0}, {255, 59, 48}},  {{0, 199, 190}, {0, 122, 255}},   {{175, 82, 222}, {255, 149, 0}},
    };
    fc::LibraryIndex index;
    index.preferences.onboardingDone = true;
    int64_t now = fc::nowMs();
    std::error_code error;
    fs::create_directories(referencesDir(), error);
    fs::create_directories(generationsDir(), error);

    const char* referenceNames[] = {"frasco-serum.png", "baño-luz-natural.png", "modelo-perfil.png"};
    for (int i = 0; i < 3; ++i) {
        fc::ReferenceFile r;
        r.id = fc::newUUID();
        r.name = referenceNames[i];
        r.kind = fc::ReferenceKind::image;
        r.mime = "image/png";
        r.fileName = fc::lower(r.id) + ".png";
        std::string bytes = demoBitmap(400, 400, palettes[i + 3][0], palettes[i + 3][1]);
        writeFileBytes(referencesDir() / widen(r.fileName), bytes);
        r.bytes = static_cast<int64_t>(bytes.size());
        r.created = now - i * 60000;
        index.references.push_back(r);
    }
    fc::ReferenceFile audio;
    audio.id = fc::newUUID();
    audio.name = "voz-referencia.wav";
    audio.kind = fc::ReferenceKind::audio;
    audio.mime = "audio/wav";
    audio.fileName = fc::lower(audio.id) + ".wav";
    audio.durationMs = 12400;
    audio.bytes = 64;
    audio.created = now - 300000;
    writeFileBytes(referencesDir() / widen(audio.fileName), std::string(64, '\0'));
    index.references.push_back(audio);

    const char* prompts[] = {
        "Selfie espontánea tomando café junto a una ventana con lluvia, luz de tarde, piel real.",
        "Frasco de sérum sobre mármol blanco con luz de ventana y sombras de plantas.",
        "Unboxing de zapatillas sobre la cama, POV con una sola mano visible.",
        "Chica en el sillón mostrando su celular con una sonrisa, luz cálida de lámpara.",
        "Look de oficina frente al espejo del ascensor, cámara de iPhone.",
        "Manos preparando una receta en una cocina chica, vista cenital.",
    };
    auto const& models = fc::presets::imageModels();
    for (int i = 0; i < 6; ++i) {
        bool tall = i % 2 == 0;
        fc::Job j;
        j.id = fc::newUUID();
        j.batchID = fc::newUUID();
        j.kind = fc::MediaKind::image;
        j.model = models[i % models.size()].id;
        j.prompt = j.finalPrompt = prompts[i];
        j.settings.resolution = i % 2 ? "2K" : "1K";
        j.settings.aspect = tall ? "9:16" : "1:1";
        j.settings.camera = "iPhone Camera";
        j.status = fc::JobStatus::success;
        j.progress = 100;
        j.taskId = "demo";
        std::string file = "framecraft-demo-" + std::to_string(i + 1) + ".png";
        writeFileBytes(generationsDir() / widen(file), demoBitmap(tall ? 360 : 480, tall ? 640 : 480, palettes[i][0], palettes[i][1]));
        j.outputs = {file};
        j.created = j.updated = now - 3600000LL * (i + 1);
        j.favorite = i == 1;
        index.jobs.push_back(j);
    }
    fc::Job generating;
    generating.id = fc::newUUID();
    generating.batchID = fc::newUUID();
    generating.kind = fc::MediaKind::video;
    generating.model = fc::presets::kVideoModelID;
    generating.prompt = "Chica recomienda su sérum frente al espejo del baño, 15 s, diálogo en inglés.";
    generating.finalPrompt = kVideoPrompt;
    generating.settings.resolution = "1080p";
    generating.settings.aspect = "9:16";
    generating.settings.duration = 15;
    generating.settings.generateAudio = true;
    generating.status = fc::JobStatus::generating;
    generating.progress = 46;
    generating.taskId = "demo";
    generating.created = generating.updated = now - 120000;
    index.jobs.insert(index.jobs.begin(), generating);
    fc::Job failed;
    failed.id = fc::newUUID();
    failed.batchID = fc::newUUID();
    failed.kind = fc::MediaKind::image;
    failed.model = "nano-banana-pro";
    failed.prompt = "Retrato testimonial sosteniendo una barra de proteína.";
    failed.settings.resolution = "1K";
    failed.settings.aspect = "4:5";
    failed.status = fc::JobStatus::fail;
    failed.progress = 12;
    failed.taskId = "demo";
    failed.error = "Content policy: reformulá el prompt.";
    failed.created = failed.updated = now - 600000;
    index.jobs.insert(index.jobs.begin() + 1, failed);

    std::string bible =
        "FORMAT: Vertical 9:16, one continuous take, no cuts. Rear 1x phone camera held by P2 (the customer). Deep depth of field.\n"
        "P1 WALTER: 82-year-old African American waiter, beige polo, red name tag, black half apron.";
    fc::Story story;
    story.id = fc::newUUID();
    story.title = "Walter, 82 años";
    story.brief = "Un mozo de 82 años en un diner cuenta su historia al cliente que lo filma.";
    story.sceneDuration = 30;
    story.slots = {{fc::newUUID(), fc::ReferenceKind::image, index.references[0].id, false, "Identidad de Walter (tres vistas)"},
                   {fc::newUUID(), fc::ReferenceKind::image, std::nullopt, true, "Primer fotograma clavado"},
                   {fc::newUUID(), fc::ReferenceKind::audio, audio.id, false, "Timbre de voz de Walter"}};
    story.summary = "Un cliente filma a Walter, un mozo de 82 años, mientras le cuenta por qué sigue trabajando. La propina, el rechazo y el abrazo.";
    story.continuity = bible;
    story.referenceOrder = "Cargá @Image1 (Walter) y @Image2 (último fotograma de la escena anterior) en ese orden; @Audio1 solo como timbre.";
    story.scenes = {
        {fc::newUUID(), 1, "Hook: “Eighty-two”", "El cliente le pregunta la edad a Walter mientras sirve café; la respuesta abre la historia.", 30,
         bible + "\n0–10 s: P1 pours coffee. P2 (curious, casual): \"How old are you, if you don't mind me asking?\"\n10–30 s: P1 (proud, a small laugh): \"Eighty-two.\" No music, no text.",
         std::string("Diálogo corto: sumar líneas para llegar a ~74 palabras."), {generating.id}},
        {fc::newUUID(), 2, "La hija", "Walter cuenta, sin dramatismo, que su hija falleció y por eso trabaja.", 30,
         bible + "\n@Image2 IS the first frame of this video.\n0–15 s: P1 (quiet, flat): \"My daughter passed away.\" Match @Audio1 for timbre only. No music, no cuts, no text.",
         std::nullopt, {}},
        {fc::newUUID(), 3, "El peso", "El cliente cambia de tema con humor; Walter se ríe y toma el pedido.", 25,
         bible + "\n@Image2 IS the first frame of this video.\n0–25 s: P2 (lighter): \"So what are you having for breakfast?\" No music, no text.",
         std::nullopt, {}},
    };
    story.messages = {{fc::newUUID(), false, "Historia creada: 3 escenas, 85 s en total.", std::nullopt, now}};
    story.created = story.updated = now;
    index.stories = {story};
    fc::library::save(indexFile, index);
    demoKeys_[credentials::kKie] = "demo-key-0000000000";
    credits = 1250;
}

}  // namespace fcapp
