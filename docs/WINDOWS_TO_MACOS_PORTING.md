# Cambios de Windows y guía de portabilidad a macOS

Documento de referencia para trasladar a la app nativa de macOS los cambios implementados en Windows para la versión 1.0.3. La intención es conservar el comportamiento y la experiencia, usando componentes nativos de cada plataforma; no copiar literalmente la implementación C++/WinUI.

## Resumen funcional

La versión de Windows incorpora dos cambios principales:

1. Un espacio **ChatGPT** independiente, respaldado por el Codex CLI incluido con Framecraft, con varias conversaciones y persistencia local.
2. Una revisión completa de **Crear** para reducir ruido visual: selector de tipo compacto, plantillas en menú, referencias y ajustes progresivamente desplegables, y asistente oculto por defecto.

También se reforzó la compilación y el empaquetado de Windows para que funcionen con Visual Studio Build Tools, una instalación local de Inno Setup y entornos donde `RUNNER_TEMP` no está definido.

## 1. ChatGPT con historial persistente

### Comportamiento visible

- **ChatGPT** aparece como destino principal en la barra lateral, inmediatamente después de **Crear**.
- La pantalla usa dos columnas:
  - historial de conversaciones a la izquierda;
  - conversación seleccionada y caja de mensaje a la derecha.
- **Nuevo chat** crea y selecciona una conversación vacía. Si la conversación actual ya está vacía, se reutiliza para no acumular elementos sin contenido.
- El título se deriva automáticamente del primer mensaje del usuario, limitado a 46 caracteres y con puntos suspensivos cuando corresponde.
- Cada conversación se puede seleccionar y eliminar. La eliminación requiere confirmación.
- La caja permite enviar con el botón o con `Ctrl+Enter`.
- Mientras Codex responde se deshabilitan las acciones que podrían cambiar la conversación activa y se muestra progreso.
- Se muestran estados separados para sesión iniciada, sesión requerida y errores de respuesta.
- Al cerrar y volver a abrir Framecraft se conservan todas las conversaciones, su orden, título y mensajes.

### Modelo de datos

Windows define `ChatMessage` y `ChatConversation` en `windows/app/src/AppModel.h` y persiste este esquema con versión:

```json
{
  "version": 1,
  "chats": [
    {
      "id": "uuid",
      "title": "Título automático",
      "created": 1760000000000,
      "updated": 1760000000000,
      "messages": [
        { "role": "user", "text": "..." },
        { "role": "assistant", "text": "..." }
      ]
    }
  ]
}
```

En Windows se guarda en `%LOCALAPPDATA%\Framecraft Studio\chats.json`. La escritura es atómica: primero se escribe `chats.json.tmp` y después se reemplaza el archivo final con persistencia inmediata. Un archivo ausente, vacío o inválido no bloquea la app; se crea una conversación nueva.

Para macOS se recomienda:

- ruta: `~/Library/Application Support/Framecraft Studio/chats.json`;
- un `ChatStore` observable, separado del estado de generación;
- `Codable` para mantener exactamente el esquema y `version`;
- escritura atómica mediante `Data.write(options: .atomic)`;
- orden descendente por `updated` al cargar;
- creación automática de una conversación vacía cuando no existe ninguna.

### Integración con Codex

Cada envío usa el Codex CLI ya incluido y la sesión de ChatGPT existente. La llamada se realiza fuera del hilo de interfaz. El prompt interno:

- presenta al asistente como ChatGPT dentro de Framecraft;
- responde en español rioplatense salvo pedido contrario;
- admite ideas, escritura, análisis y programación;
- le indica que no ejecute acciones ni modifique archivos;
- devuelve únicamente la respuesta, sin prefijos de rol.

Se envían como contexto los últimos 16 mensajes para limitar el tamaño del comando. El historial visible conserva hasta 40 mensajes por conversación. Cada mensaje del usuario admite hasta 12.000 caracteres.

En macOS la ejecución debe hacerse con `Process` desde una tarea fuera de `MainActor`, capturando solo la respuesta final. La actualización de la conversación vuelve a `MainActor`. No se debe bloquear la interfaz durante el proceso ni conceder al chat acceso de escritura a los archivos del usuario.

### Equivalencia sugerida en SwiftUI

- Destino lateral: `NavigationSplitView` o el contenedor de navegación existente.
- Historial: `List(selection:)`, con menú contextual para eliminar.
- Conversación: `ScrollViewReader` + `LazyVStack` de burbujas.
- Entrada: `TextEditor` o control existente con comando `⌘Return` para enviar.
- Estado: `ProgressView`, mensaje de sesión y error accesible.
- Confirmación: `confirmationDialog` o `alert` destructivo.
- Persistencia: `@Observable`/`ObservableObject` con una única instancia compartida por la app.

## 2. Rediseño de Crear

El objetivo fue conservar todas las opciones sin mostrarlas al mismo tiempo.

### Cambios aplicados

- El asistente lateral queda **oculto por defecto**.
- Se eliminó una franja de progreso redundante.
- El encabezado se simplificó y ya no duplica la búsqueda global del contenedor principal.
- El selector de **Imagen / Video** pasó de dos tarjetas grandes a un control segmentado compacto.
- Las plantillas dejaron de ocupar una estantería horizontal de tarjetas; ahora se eligen desde **Usar una plantilla**, un menú ordenado primero por el tipo activo.
- **Archivos de referencia** está contraído inicialmente y muestra un resumen con la cantidad seleccionada.
- **Ajustes** está contraído inicialmente y muestra la configuración actual en una línea de resumen.
- Los textos se acortaron y se eliminaron números de paso donde no aportaban orientación.
- Las tarjetas comunes usan un radio de esquina de 14 puntos para una apariencia más suave y consistente.
- La barra de generación permanece visible como acción principal.

### Equivalencia sugerida en SwiftUI

- Tipo de contenido: `Picker` con estilo `.segmented`.
- Plantillas: `Menu` con división entre plantillas del tipo activo y el alternativo.
- Referencias y ajustes: `DisclosureGroup`, inicialmente cerrados, con un resumen visible.
- Asistente: inspector, panel o sheet opcional; nunca abierto automáticamente.
- Acción generar: botón prominente y estable, sin competir con tarjetas secundarias.
- Mantener estados de foco, etiquetas de accesibilidad, tema claro/oscuro y ancho adaptable.

La versión de macOS debe preservar las opciones existentes y su lógica; el cambio es de jerarquía visual, no una eliminación de funciones.

## 3. Navegación y búsqueda

El nuevo valor `chat` se añadió al enum de secciones, títulos, iconos, orden lateral, fábrica de páginas, navegación por teclado y resultados de búsqueda global. En macOS hay que actualizar todas las fuentes de navegación equivalentes, no solamente agregar la vista, para evitar destinos inaccesibles o estados sin restaurar.

## 4. Cambios exclusivos de Windows

Estos ajustes no deben trasladarse literalmente a macOS:

- `windows/app/Framecraft.vcxproj` ahora declara las configuraciones antes de importar props, fija el SDK de Windows, genera fuentes XAML explícitamente cuando se compila solo con Build Tools y copia los XAML/XBF procesados al resultado.
- `windows/app/src/XamlGenerated.cpp` incluye los archivos C++ generados por XAML cuando no está instalado el workload UWP completo.
- `windows/scripts/build.ps1` detecta Inno Setup tanto en `Program Files (x86)` como en la instalación local por usuario.
- `windows/scripts/fetch-codex.ps1` elige de forma segura entre `RUNNER_TEMP`, `TEMP` y el directorio temporal del sistema.
- Se corrigió el tipo usado al mover referencias seleccionadas para evitar conversiones inseguras entre índices.

En macOS solo hay que verificar el equivalente funcional: que Codex se encuentre dentro del bundle, que la compilación CI y local produzcan artefactos reproducibles, y que el empaquetado incluya todos los recursos requeridos.

## 5. Mapa de archivos de Windows

| Archivo | Responsabilidad del cambio |
|---|---|
| `windows/app/src/AppModel.h` | Tipos, estado y API de conversaciones; nueva sección; asistente cerrado por defecto. |
| `windows/app/src/AppModel.cpp` | Carga, guardado, selección, eliminación y envío a Codex. |
| `windows/app/src/ChatPage.cpp` | Interfaz completa de historial y conversación. |
| `windows/app/src/CreatePage.cpp` | Rediseño de Crear y revelado progresivo. |
| `windows/app/src/MainWindow.cpp` | Enrutamiento y escenarios visuales de ChatGPT. |
| `windows/app/src/Pages.h` | Fábrica de la nueva página. |
| `windows/app/src/Ui.cpp` | Radio común de tarjetas. |
| `windows/app/Framecraft.vcxproj` | Compatibilidad de WinUI/XAML con Build Tools. |
| `windows/app/src/XamlGenerated.cpp` | Inclusión de fuentes generadas por XAML. |
| `windows/scripts/build.ps1` | Detección de Inno Setup. |
| `windows/scripts/fetch-codex.ps1` | Directorio temporal robusto para descargar Codex. |

## 6. Validación realizada en Windows

- Compilación completa de Release y pruebas del núcleo: correcta.
- Empaquetado del instalador 1.0.3: correcto.
- Codex incluido: versión 0.159.0, sesión detectada y respuesta real verificada.
- Persistencia de chat: envío, creación de segunda conversación, cierre, reapertura y restauración visual verificados.
- Inicio normal de la app y navegación por Crear/ChatGPT: verificados.
- Diseño claro y oscuro: revisado con capturas externas de la ventana.

## 7. Lista de aceptación para macOS

- [ ] ChatGPT aparece en la navegación principal y en la búsqueda global.
- [ ] Se pueden crear, seleccionar y eliminar varias conversaciones.
- [ ] El primer mensaje genera un título corto.
- [ ] El historial sobrevive al cierre y reapertura de la app.
- [ ] Una escritura interrumpida no destruye el historial anterior.
- [ ] La sesión de ChatGPT se refleja correctamente y los errores son recuperables.
- [ ] La llamada a Codex nunca bloquea `MainActor`.
- [ ] Se respetan los límites de 12.000 caracteres, 16 mensajes de contexto y 40 mensajes guardados.
- [ ] Crear abre con el asistente, referencias y ajustes cerrados.
- [ ] Imagen/Video usa un selector compacto y las plantillas están en un menú.
- [ ] Todas las opciones anteriores siguen disponibles.
- [ ] La interfaz funciona con tema claro/oscuro, VoiceOver y ventana estrecha.
- [ ] El bundle final incluye Codex y pasa las pruebas de firma/notarización existentes.
