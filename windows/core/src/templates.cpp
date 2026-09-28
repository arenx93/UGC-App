#include "framecraft/templates.h"

#include "framecraft/util.h"

namespace fc {

std::vector<std::string> CreativeTemplate::placeholders() const { return templates::placeholders(idea); }

namespace templates {

std::vector<std::string> placeholders(const std::string& text) {
    std::vector<std::string> found;
    size_t position = 0;
    while ((position = text.find('[', position)) != std::string::npos) {
        size_t close = text.find(']', position + 1);
        if (close == std::string::npos) break;
        std::string inner = text.substr(position + 1, close - position - 1);
        size_t length = characterCount(inner);
        if (length >= 2 && length <= 40) {
            found.push_back(text.substr(position, close - position + 1));
            position = close + 1;
        } else {
            position += 1;
        }
    }
    return found;
}

const std::vector<CreativeTemplate>& creative() {
    static const std::vector<CreativeTemplate> list = {
        {"testimonial", "Testimonio", "Alguien real cuenta por qué lo usa", "\xEE\x9D\xBB", MediaKind::video,
         "Una persona común recomienda [tu producto] hablándole al celular en [su cocina / su baño / su auto]. Cuenta el problema que tenía, cómo lo usa y un resultado concreto. Tono natural, sin sonar a anuncio.",
         15, "9:16"},
        {"unboxing", "Unboxing", "Abrir el paquete con reacciones", "\xEE\x9E\xB8", MediaKind::video,
         "POV con una mano: abro la caja de [tu producto] sobre [la cama / el escritorio]. Muestro el empaque, saco el producto, lo giro para mostrar detalles y reacciono en voz baja.",
         15, "9:16"},
        {"grwm", "Get ready with me", "Rutina frente al espejo", "\xEE\x9D\xAE", MediaKind::video,
         "Get ready with me frente al espejo del baño a la mañana. Mientras me preparo uso [tu producto] y cuento por qué lo elegí, como si hablara con una amiga.",
         20, "9:16"},
        {"before-after", "Antes y después", "El cambio en una sola toma", "\xEE\xA2\xAB", MediaKind::video,
         "Muestro [el problema: pelo frizz / mancha en la ropa / piel seca] de cerca con el celular, uso [tu producto] y en la misma toma continua se ve el resultado. Reacción genuina al final.",
         15, "9:16"},
        {"street", "Entrevista callejera", "Pregunta rápida a un desconocido", "\xEE\x9C\xA0", MediaKind::video,
         "Entrevista callejera en [ciudad]: le pregunto a una persona que pasa qué opina de [tu producto o tema]. Micrófono de solapa visible, ruido de la calle, respuesta espontánea con humor.",
         15, "9:16"},
        {"pov-problem", "POV: el problema", "Situación cotidiana que se resuelve", "\xEE\xA2\x90", MediaKind::video,
         "POV: estoy [en el subte / en la oficina / cocinando] y me pasa [problema cotidiano]. Saco [tu producto], lo uso y se resuelve. Filmado como historia de Instagram, cámara en mano.",
         10, "9:16"},
        {"product-hero", "Foto de producto", "Producto sobre una superficie real", "\xEE\x9E\xB8", MediaKind::image,
         "Foto de [tu producto] sobre [mármol / madera / sábanas blancas] con luz natural de ventana, sombras suaves, gotas o texturas reales, como la sacaría una persona con su celular.",
         std::nullopt, "4:5"},
        {"selfie", "Selfie con producto", "Persona real sosteniéndolo", "\xEE\x9C\xA2", MediaKind::image,
         "Selfie espontánea de [persona: edad, estilo] sosteniendo [tu producto] cerca de la cara en [lugar], luz real, piel con textura, encuadre imperfecto de celular.",
         std::nullopt, "9:16"},
        {"flatlay", "Flat lay", "Vista cenital con objetos", "\xEE\xA2\xA9", MediaKind::image,
         "Vista cenital de [tu producto] rodeado de [objetos de la rutina: café, llaves, agenda] sobre [superficie], luz de mañana, composición natural, no de estudio.",
         std::nullopt, "1:1"},
        {"lifestyle", "Momento lifestyle", "El producto en uso, sin posar", "\xEE\xA0\x85", MediaKind::image,
         "Foto cándida de [persona] usando [tu producto] en [lugar cotidiano], como si un amigo la hubiera sacado sin avisar. Movimiento leve, luz disponible.",
         std::nullopt, "4:5"},
    };
    return list;
}

const std::vector<StoryTemplate>& stories() {
    static const std::vector<StoryTemplate> list = {
        {"emotional", "Historia emotiva", "Encuentro real filmado con el celular", "\xEE\xAD\x91",
         "Un cliente filma con el celular a [personaje: nombre, edad, trabajo] en [lugar]. Empieza como una charla casual, el personaje cuenta algo personal de su vida y el cliente hace un gesto inesperado que lo emociona. Final con abrazo. POV del que filma, diálogo en [idioma], tono real, nada de música.",
         6, 20},
        {"product-journey", "De problema a solución", "Tres actos con tu producto", "\xEE\x9F\x81",
         "Tres actos filmados como UGC: 1) [persona] muestra su frustración con [problema], 2) descubre y prueba [tu producto] mientras lo comenta al celular, 3) días después muestra el resultado y lo recomienda. Mismo lugar y misma ropa en los actos 1 y 2. Diálogo en [idioma].",
         3, 15},
        {"day-in-life", "Un día conmigo", "Vlog de la mañana a la noche", "\xEE\x9C\x86",
         "Vlog de un día de [persona: edad, trabajo] en [ciudad]: despertar, café, trabajo, un momento con [tu producto], cena y cierre en la cama. Cada escena en un lugar distinto, cámara en mano, habla a cámara en [idioma].",
         5, 15},
        {"series", "Serie de personajes", "Mismo elenco, capítulo nuevo", "\xEE\x9C\x96",
         "Capítulo de una serie corta con [personajes fijos y cómo son]. En este capítulo [qué pasa]. Humor, cada escena termina con un pequeño remate. Continuidad estricta de ropa y lugar. Diálogo en [idioma].",
         std::nullopt, 15},
    };
    return list;
}

}  // namespace templates
}  // namespace fc
