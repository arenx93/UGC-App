import Foundation

/// A ready-made starting point: fills the assistant's idea and the right settings in one click.
public struct CreativeTemplate: Identifiable, Hashable, Sendable {
    public let id: String
    public let title: String
    public let subtitle: String
    public let symbol: String
    public let media: MediaKind
    /// Idea text for the assistant. Words in [brackets] are placeholders for the user.
    public let idea: String
    public var duration: Int?
    public var aspect: String

    public init(id: String, title: String, subtitle: String, symbol: String, media: MediaKind, idea: String,
                duration: Int? = nil, aspect: String = "9:16") {
        self.id = id
        self.title = title
        self.subtitle = subtitle
        self.symbol = symbol
        self.media = media
        self.idea = idea
        self.duration = duration
        self.aspect = aspect
    }

    /// Placeholders like "[tu producto]" the user should replace.
    public var placeholders: [String] {
        guard let regex = try? NSRegularExpression(pattern: "\\[[^\\]]{2,40}\\]") else { return [] }
        let range = NSRange(idea.startIndex..., in: idea)
        return regex.matches(in: idea, range: range).compactMap { Range($0.range, in: idea).map { String(idea[$0]) } }
    }
}

/// A ready-made story brief for the Historias section.
public struct StoryTemplate: Identifiable, Hashable, Sendable {
    public let id: String
    public let title: String
    public let subtitle: String
    public let symbol: String
    public let brief: String
    public let sceneCount: Int?
    public let sceneDuration: Int

    public init(id: String, title: String, subtitle: String, symbol: String, brief: String, sceneCount: Int?, sceneDuration: Int) {
        self.id = id
        self.title = title
        self.subtitle = subtitle
        self.symbol = symbol
        self.brief = brief
        self.sceneCount = sceneCount
        self.sceneDuration = sceneDuration
    }
}

public enum Templates {
    public static let creative: [CreativeTemplate] = [
        CreativeTemplate(
            id: "testimonial", title: "Testimonio", subtitle: "Alguien real cuenta por qué lo usa",
            symbol: "person.wave.2.fill", media: .video,
            idea: "Una persona común recomienda [tu producto] hablándole al celular en [su cocina / su baño / su auto]. Cuenta el problema que tenía, cómo lo usa y un resultado concreto. Tono natural, sin sonar a anuncio.",
            duration: 15),
        CreativeTemplate(
            id: "unboxing", title: "Unboxing", subtitle: "Abrir el paquete con reacciones",
            symbol: "shippingbox.fill", media: .video,
            idea: "POV con una mano: abro la caja de [tu producto] sobre [la cama / el escritorio]. Muestro el empaque, saco el producto, lo giro para mostrar detalles y reacciono en voz baja.",
            duration: 15),
        CreativeTemplate(
            id: "grwm", title: "Get ready with me", subtitle: "Rutina frente al espejo",
            symbol: "sparkles", media: .video,
            idea: "Get ready with me frente al espejo del baño a la mañana. Mientras me preparo uso [tu producto] y cuento por qué lo elegí, como si hablara con una amiga.",
            duration: 20),
        CreativeTemplate(
            id: "before-after", title: "Antes y después", subtitle: "El cambio en una sola toma",
            symbol: "arrow.left.arrow.right", media: .video,
            idea: "Muestro [el problema: pelo frizz / mancha en la ropa / piel seca] de cerca con el celular, uso [tu producto] y en la misma toma continua se ve el resultado. Reacción genuina al final.",
            duration: 15),
        CreativeTemplate(
            id: "street", title: "Entrevista callejera", subtitle: "Pregunta rápida a un desconocido",
            symbol: "mic.fill", media: .video,
            idea: "Entrevista callejera en [ciudad]: le pregunto a una persona que pasa qué opina de [tu producto o tema]. Micrófono de solapa visible, ruido de la calle, respuesta espontánea con humor.",
            duration: 15),
        CreativeTemplate(
            id: "pov-problem", title: "POV: el problema", subtitle: "Situación cotidiana que se resuelve",
            symbol: "eye.fill", media: .video,
            idea: "POV: estoy [en el subte / en la oficina / cocinando] y me pasa [problema cotidiano]. Saco [tu producto], lo uso y se resuelve. Filmado como historia de Instagram, cámara en mano.",
            duration: 10),
        CreativeTemplate(
            id: "product-hero", title: "Foto de producto", subtitle: "Producto sobre una superficie real",
            symbol: "cube.fill", media: .image,
            idea: "Foto de [tu producto] sobre [mármol / madera / sábanas blancas] con luz natural de ventana, sombras suaves, gotas o texturas reales, como la sacaría una persona con su celular.",
            aspect: "4:5"),
        CreativeTemplate(
            id: "selfie", title: "Selfie con producto", subtitle: "Persona real sosteniéndolo",
            symbol: "camera.fill", media: .image,
            idea: "Selfie espontánea de [persona: edad, estilo] sosteniendo [tu producto] cerca de la cara en [lugar], luz real, piel con textura, encuadre imperfecto de celular.",
            aspect: "9:16"),
        CreativeTemplate(
            id: "flatlay", title: "Flat lay", subtitle: "Vista cenital con objetos",
            symbol: "square.grid.3x3.fill", media: .image,
            idea: "Vista cenital de [tu producto] rodeado de [objetos de la rutina: café, llaves, agenda] sobre [superficie], luz de mañana, composición natural, no de estudio.",
            aspect: "1:1"),
        CreativeTemplate(
            id: "lifestyle", title: "Momento lifestyle", subtitle: "El producto en uso, sin posar",
            symbol: "figure.walk", media: .image,
            idea: "Foto cándida de [persona] usando [tu producto] en [lugar cotidiano], como si un amigo la hubiera sacado sin avisar. Movimiento leve, luz disponible.",
            aspect: "4:5"),
    ]

    public static let stories: [StoryTemplate] = [
        StoryTemplate(
            id: "emotional", title: "Historia emotiva", subtitle: "Encuentro real filmado con el celular",
            symbol: "heart.fill",
            brief: "Un cliente filma con el celular a [personaje: nombre, edad, trabajo] en [lugar]. Empieza como una charla casual, el personaje cuenta algo personal de su vida y el cliente hace un gesto inesperado que lo emociona. Final con abrazo. POV del que filma, diálogo en [idioma], tono real, nada de música.",
            sceneCount: 6, sceneDuration: 20),
        StoryTemplate(
            id: "product-journey", title: "De problema a solución", subtitle: "Tres actos con tu producto",
            symbol: "chart.line.uptrend.xyaxis",
            brief: "Tres actos filmados como UGC: 1) [persona] muestra su frustración con [problema], 2) descubre y prueba [tu producto] mientras lo comenta al celular, 3) días después muestra el resultado y lo recomienda. Mismo lugar y misma ropa en los actos 1 y 2. Diálogo en [idioma].",
            sceneCount: 3, sceneDuration: 15),
        StoryTemplate(
            id: "day-in-life", title: "Un día conmigo", subtitle: "Vlog de la mañana a la noche",
            symbol: "sun.max.fill",
            brief: "Vlog de un día de [persona: edad, trabajo] en [ciudad]: despertar, café, trabajo, un momento con [tu producto], cena y cierre en la cama. Cada escena en un lugar distinto, cámara en mano, habla a cámara en [idioma].",
            sceneCount: 5, sceneDuration: 15),
        StoryTemplate(
            id: "series", title: "Serie de personajes", subtitle: "Mismo elenco, capítulo nuevo",
            symbol: "theatermasks.fill",
            brief: "Capítulo de una serie corta con [personajes fijos y cómo son]. En este capítulo [qué pasa]. Humor, cada escena termina con un pequeño remate. Continuidad estricta de ropa y lugar. Diálogo en [idioma].",
            sceneCount: nil, sceneDuration: 15),
    ]
}
