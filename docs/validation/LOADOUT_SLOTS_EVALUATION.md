# Evaluación: favoritos de combinación (10)

Estado: **evaluación, sin implementación**. Origen: `prompt_registered_chars.md`.

## 1. Veredicto

La feature es viable y no necesita cambios de protocolo. Todo lo que viaja por la
red ya es un `ConfirmedCharaConditions` completo (personaje, traje, color, ultra,
edition, personal action, win quote, handicap), así que los favoritos son
**memoria local**: el servidor y el rival no tienen que saber que existen.

El trabajo real está en tres sitios:

1. Guardar hasta diez combinaciones completas en las preferencias y normalizarlas
   al cargar.
2. Una página de cards que aplique una combinación entera de una vez.
3. Respetar el mismo candado de edición que ya usan las filas de selección
   (`canEditSelection`, `SelectionBlocker`).

El punto delicado es el paso 2: el selector actual restaura la memoria por
personaje justo después de dibujarse, y eso **pisaría** el traje y el ultra del
favorito si se aplica un personaje distinto. Hay que aplicar el favorito también
sobre esa memoria, no solo sobre la selección viva. Está detallado en el riesgo
R1.

Coste aproximado: **2–3 días** de una persona que ya conozca el repo. La mayor
parte es UI y tests; el modelo de datos es pequeño.

## 2. Lo que ya existe y sirve de base

El flujo actual ya tiene casi todas las piezas:

| Pieza | Dónde | Uso para los favoritos |
| --- | --- | --- |
| `selection::Pick` | `src/common/FighterCatalog.hxx:41` | Estructura de una combinación (fighter, costume, color, ultra, edition, personalAction, winQuote, handicap) |
| `CharaPick` | `src/sf4e/sf4e__OverlayPrefs.hxx:26` | Forma nativa persistida; es lo que se guarda en disco |
| `Data.lobby`, `Data.fighters[44]` | `src/sf4e/sf4e__OverlayPrefs.hxx:46-49` | Selección viva y memoria por personaje; los favoritos van al lado |
| `FromNative` / `ToNative` | `src/common/FighterCatalog.hxx:109-125` | Conversión entre `Pick` y `CharaPick`/nativo |
| `Valid` / `Available` / `Normalize` | `src/common/FighterCatalog.hxx:105-107` | Validez de red, disponibilidad local (DLC/unlocks) y ajuste |
| `FighterSelector` | `src/ui/FighterSelector.hxx:23` | Páginas, cards, `SelectionArt`, navegación y retorno embebido |
| `SelectionArt::Appearance` | `src/ui/SelectionArt.hxx:37` | **La foto de la ropita** exacta (personaje + traje + color) |
| `CostumeLabel` / `UltraLabel` | `src/ui/FighterSelector.hxx:15-16` | Nombres de traje y ultra ya localizados |
| `SelectionRows` | `src/ui/RoomPanel.cxx:93` | Filas de la selección propia en la mesa; ahí entra el botón |
| `DrawNetworkCharaConfig` | `src/sf4e/sf4e__Overlay.cxx:227` | Único punto que lee y escribe la selección viva |
| `ApplicationShell` pantalla `"selection"` | `src/ui/ApplicationShell.cxx:515`, `:809` | Cómo se abre y se dibuja un selector embebido |
| `OverlayPrefs::Clamp` / `FromJson` / `ToJson` | `src/sf4e/sf4e__OverlayPrefs.cxx:120-198` | Dónde se normaliza y se serializa |

No hay que inventar un modelo de selección: un favorito es un `CharaPick` en una
lista corta.

## 3. Modelo de datos

En `OverlayPrefs::Data` (`src/sf4e/sf4e__OverlayPrefs.hxx:42`):

```cpp
// sf4e__OverlayPrefs.hxx: añadir <vector> a los includes
static constexpr int FavouriteLimit = 10;
std::vector<CharaPick> favourites;
```

Un vector y no un array de diez: con favoritos no hay huecos, la lista
simplemente es más corta. Eso quita el array de bits, el estado "card vacía" y el
caso "lista llena" como algo distinto de "tope alcanzado".

Decisiones del modelo:

- **Global, no por personaje.** El mazo del prompt (Juri, El Fuerte, Sagat,
  Balrog) es global. La memoria por personaje ya existe aparte en
  `Data.fighters[]` y no se toca.
- **Se guarda el `CharaPick` completo**, aunque la card solo muestre personaje,
  ultra y personal action. Guardar solo el personaje deja la combinación en
  "la última usada" (`Data.fighters[]`), que no es lo que pide el prompt:
  quiere combinaciones fijas. Guardar el pick entero reutiliza `Normalize` y
  `Available` tal cual.
- **Duplicados permitidos, idénticos no.** Dos Juri con ultras distintos son
  válidas y la card las distingue por el ultra. Añadir un pick idéntico a uno
  existente no crea otra card.
- **El escenario queda fuera.** El prompt no lo pide y el escenario es autoridad
  de P1 (`RoomPanel.cxx:316-318`). Un favorito no debe cambiar el stage del
  rival.
- `Data::operator==` (`:64`) debe incluir `favourites`; si no, el guardado por
  diferencias de `sf4e__Overlay.cxx:803` nunca ve el cambio.

## 4. Persistencia

- `ToJson` (`:181`): `"favourites"` como array de objetos, `CharaToJson` por
  entrada, sin huecos.
- `FromJson` (`:136`): tolerante a la ausencia de la clave (perfiles viejos),
  `CharaFromJson` por entrada y recorte al tope.
- `Clamp` (`:120`): recorre `favourites` con `ClampChara(favourite, true)`
  —igual que `fighters[]` en `:127`— para conservar la edition guardada y
  normalizarla contra el catálogo, no contra la regla de la sala actual. Después,
  recortar a `FavouriteLimit`. `Clamp` ya llama a `DropRemovedCustom` al
  principio, así que basta con extender ambos.
- `DropRemovedCustom` (`:76`): debe recorrer también `favourites`; si no, un
  favorito apunta a un traje o color custom desinstalado y la card sale rota.
- No hace falta API nueva de guardado: el bucle de `sf4e__Overlay.cxx:798-803`
  ya copia `s_prefs`, compara y encola. Añadir `favourites` a `Data` basta.

Compatibilidad: el archivo de ajustes es local y `FromJson` ya ignora claves
desconocidas, así que un perfil nuevo en una build vieja no rompe nada.

## 5. UI

### 5.1 Entrada y por qué favoritos

Sí, es la misma feature con otra entrada, y **"favoritos" es mejor nombre que
"slots numerados"**. La diferencia real no es el nombre, es qué guarda cada
entrada:

| Diseño | Qué guarda | Resultado |
| --- | --- | --- |
| Favorito de personaje | Solo el `charaID` | Barato, pero la combinación es "la última usada" vía `Data.fighters[]`; no queda fijada |
| Favorito de combinación | El `CharaPick` completo | Es lo que pide el prompt: ultra, ropita y personal action fijos |

El prompt pide combinaciones ("distintas combinaciones de personaje, ultra,
ropita, personal action"), así que el favorito guarda el pick completo. Con eso,
un favorito es exactamente un slot, pero sin números ni huecos.

Comparación de entradas:

| | Favoritos dentro del character select | Rejilla aparte |
| --- | --- | --- |
| Pantalla nueva | No, es una página más del selector | Sí |
| Atajo nuevo | No hace falta | Sí |
| Sitio en el roster | Compite con 44 cards en 15 columnas | No compite |
| Card | Necesita render de ropita, no retrato | Igual |
| Descubrimiento | Alto: está donde ya eliges | Medio: hay que abrirla |

Recomendación: **favoritos como página del selector**, con el roster como
entrada. Se paga el render de card distinto y se ahorra pantalla y atajo.

- La página `"favourites"` es la **primera card** del roster: "Favoritos (n)". No
  es una fila de pie, porque una entrada `wide` abriría el pie de la rejilla y
  convertiría los 44 personajes en filas (`MenuGridCells`, `MenuNavigation.hxx:107`).
  Como card, el roster pasa de 44 a 45 celdas: 15 × 3 exactas con el ancho por
  defecto.
- La fila `favourites` de la mesa (`SelectionRows`, `RoomPanel.cxx:93`) abre
  directamente esa página, con el mismo `canChange` y `SelectionBlocker`.
- X/F siguen abriendo el roster, como hoy; desde ahí, la primera card lleva a
  favoritos. RB es opcional y salta directo a favoritos.
- En Home, una fila `favourites` junto a `selection` cuando haya alguno.
- Añadir: acción de pie "Añadir a favoritos" sobre la card enfocada del roster,
  y una estrella en la card con ratón. Quitar: la misma estrella en la card de
  favoritos.
- La card del roster marca con estrella las combinaciones que ya son favoritas.

#### Qué botón

Los cuatro botones frontales ya están ocupados: A/Cross confirma, B/Circle
vuelve, X/Square es el atajo de Fighter y Y/Triangle el de Options
(`ControllerNavigation.hxx:44-49`). Con favoritos dentro del selector, **el
camino principal no necesita ningún botón nuevo**: X/F abren el roster, la
primera card abre favoritos y Select aplica.

RB (R1 en PS4) queda como atajo opcional para saltar directo a favoritos:

- Es digital, es el botón convencional de "acción secundaria / pestaña
  siguiente" y está libre en el menú de sala. `ReplayTransport` usa RB/RT/LB/LT,
  pero solo durante la reproducción de una repetición, que es otra pantalla.
- RB es HP por defecto, pero el atajo solo se lee con el menú abierto; en
  partida sigue siendo HP. El mismo criterio que ya se aplica a X e Y.
- Si el jugador ata confirmar o volver a RB, el atajo cede: `ControllerButtons`
  ya excluye `menu.confirm|menu.back` de los atajos
  (`ControllerNavigation.hxx:45`). Por eso la fila no se elimina.

Mapeo del atajo opcional:

| Sitio | Cambio |
| --- | --- |
| `MenuNavigation.hxx:19` | `Favourites = 2048` en `MenuInput::Button` |
| `MenuNavigation.hxx:284` | incluir `Favourites` en la máscara de atajos |
| `ControllerNavigation.hxx:15` | `Favourites = 2048` en `ControllerSample::Button` y en el `static_assert` |
| `ControllerNavigation.hxx:47` | `if(shortcuts&input::xinput::RB) result|=ControllerSample::Favourites;` |
| `RoomPanel.cxx:93-107` | fila `favourites` encima de `selection`, con el mismo `canChange` y `SelectionBlocker` |
| `RoomPanel.cxx:436` | incluir `favourites` en la lista de filas con explicación de rechazo |
| `RoomPanel.cxx:741-757` | `Favourites` abre `"favourites"`; `Fighter` sigue abriendo el roster |
| `ApplicationShell.cxx:515` | `a.id=="favourites"` abre con `selectionOpenOn_="favourites"` |
| `ApplicationShell.cxx:807` | leyenda `{"RB", loc::T("room.legend_favourites")}` |

Dos avisos:

- **PS4 por DirectInput.** La rama DirectInput de `ControllerButtons` no mapea
  ningún atajo hoy (`:50`), así que un DS4 sin DS4Windows no tendría RB. Con
  favoritos dentro del selector eso no bloquea nada: la primera card del roster
  sigue estando.
- **Glifo.** La leyenda de sala ya es XInput-céntrica (hardcodea "X", "Y",
  "View" en `ApplicationShell.cxx:807`), así que "RB" es coherente; en PS4
  debería decir "R1". Se puede tomar el glifo del mando cuando exista y caer a
  "RB".

### 5.2 Pantalla

La página `"favourites"` es una página más de `FighterSelector`
(`Page::Favourites`). Reutiliza `GameMenu`, `SelectionArt`, el retorno embebido y
el `openOn_` (`FighterSelector.cxx:245-253`), y se abre con
`openOn_=="favourites"`.

No hace falta una pantalla propia del shell: el selector ya tiene cards,
navegación, foco, leyendas y el candado de edición. Una pantalla aparte
obligaría a duplicar todo eso y a un callback nuevo junto a `selection`
(`ApplicationShell.hxx:247-249`).

### 5.3 Cards

- `GameMenu::Card` (`GameMenu.hxx:144`) recibe `(entry, min, max)` y dibuja.
- La card "Favoritos (n)", que es la primera del roster, no dibuja ropita:
  dibuja una estrella grande y el contador.
- Imagen: `art->Appearance(favourite.fighter, favourite.costume, favourite.color)`,
  que es exactamente la ropita.
- Texto: nombre del personaje, `UltraLabel(favourite.ultra)` (o
  `ShortUltraLabel`), `selection.personal_action` con el número o `common.none`,
  y la edition corta cuando la sala la permite.
- Estrella: rellena si la combinación que muestra esa card ya es favorita.
  Pulsarla la añade o la quita. Con ratón es un `InvisibleButton` en la esquina,
  como la tira de `RoomCards.cxx:25`; con mando es la acción de pie.
- Columnas: 5 × 2 en 16:9, derivadas del ancho como las galerías
  (`max(2, min(5, ancho / (240 * Scale())))`, `FighterSelector.cxx:310`), con
  `cardHeight` proporcional al render del traje.
- El panel de detalle (`preview`) muestra la ropita a tamaño grande, la
  notación del ultra (`UltraMoveInput`) y el nombre del personal action.
- Un favorito no disponible se dibuja atenuado con el motivo, igual que las
  cards `selection.not_owned` del roster (`FighterSelector.cxx:287`).

### 5.4 Aplicar, añadir y quitar

- **Aplicar** (Select sobre una card): copia el favorito al pick vivo, normaliza
  contra la regla de edition de la sala y la disponibilidad, y si el resultado
  no es `Available` se rechaza con el motivo.
- **Añadir**: acción de pie "Añadir a favoritos" sobre la card enfocada del
  roster, y la estrella con ratón. Añade el pick actual si la card es el
  personaje actual; si no, la combinación que ese personaje usó por última vez
  (`s_prefs.fighters[id]`). El pie dice qué combinación va a guardar (ultra y
  personal action incluidos) y solo se habilita si esa combinación es
  `Available`. Sobre la card "Favoritos (n)" no se ofrece.
- **Quitar**: la estrella en la card de favoritos, o `Delete` sobre ella (`Delete`
  ya significa "salir del asiento" en la mesa, `RoomPanel.cxx:739`; hay que
  desambiguar por pantalla).
- **Editar**: en v1, aplicar el favorito, retocar en el selector normal y
  volver a añadir. Un pick idéntico no duplica la card; uno distinto del mismo
  personaje sí crea otra.

### 5.5 Flujos de uso

Casos de uso de la aplicación. Actor: el jugador local. La página de favoritos se
abre desde la primera card del roster, desde la fila `favourites` de la mesa,
desde la fila `favourites` de Home, o con RB. Aplicar un favorito es atómico: o
entra la combinación completa, o no cambia nada.

Estados de una card:

| Estado | Dibujo | Select |
| --- | --- | --- |
| Normal | Ropita, nombre, ultra, personal action | Aplica el favorito |
| No disponible | Atenuada, con el motivo | Explica y no aplica |
| Edition inactiva | La edition tachada y "regla apagada" | Aplica sin la edition |
| Enfocada | Borde de foco | — |

Reglas comunes:

- La primera card del roster es "Favoritos (n)" y abre la página.
- Cancelar (B/Esc) sale de favoritos sin tocar la selección viva.
- Los favoritos nunca cambian el escenario.
- Con la mesa bloqueada, la página se abre en lectura.
- Una lista vacía dice "Aún no tienes favoritos" y ofrece el roster.

#### UC-1 — Añadir un favorito desde la selección actual

1. El jugador elige personaje, ultra, ropita y personal action en el selector
   normal.
2. Vuelve al roster y enfoca la card de ese personaje (la de su pick vivo).
3. Elige "Añadir a favoritos" en el pie, que anuncia exactamente esa
   combinación.
4. La card muestra la estrella.

Resultado: el favorito queda guardado y disponible en las siguientes sesiones.

Notas: la acción solo se ofrece si el pick actual es `Available`; si no, se
explica por qué. Añadir un pick idéntico no duplica la card.

#### UC-2 — Añadir un favorito desde el roster

1. El jugador abre el roster.
2. Enfoca un personaje y elige "Añadir a favoritos" (o pulsa su estrella con
   ratón).

Resultado: se guarda la combinación que ese personaje usó por última vez
(`s_prefs.fighters[id]`), normalizada contra la disponibilidad.

Notas: es el camino de un solo paso para el mazo. Si la combinación no es la
que quería, la aplica, la retoca y repite UC-1.

#### UC-3 — Editar un favorito

1. El jugador aplica el favorito que quiere cambiar.
2. Retoca lo que haga falta en el selector normal.
3. Vuelve al roster, enfoca la card de ese personaje y elige "Añadir a
   favoritos".

Resultado: se crea una card nueva con la combinación retocada; la anterior sigue
ahí si el pick es distinto.

Notas: en v1 no hay un modo de edición dentro de la card. Si se quiere sustituir
en vez de añadir, se quita la anterior con UC-4.

#### UC-4 — Quitar un favorito

1. El jugador enfoca una card de favoritos.
2. Pulsa la estrella (o `Delete`).

Resultado: la card desaparece y el resto conserva su orden.

Notas: quitar no cambia la selección viva aunque el favorito quitado sea el que
está aplicado, y no pide confirmación porque volver a añadirlo es barato.

#### UC-5 — Usar un favorito en una FT (mando)

1. El jugador está en la mesa, sin Ready enviado.
2. Pulsa RB para saltar directo a favoritos; con X abre el roster y va a su
   primera card, "Favoritos (n)".
3. Enfoca la card de su personaje y pulsa Select.
4. La mesa muestra el nuevo resumen; el jugador pulsa Ready.

Resultado: la partida se juega con la combinación del favorito.

Notas: si el favorito no está disponible o la mesa está bloqueada, el paso 3 no
cambia nada y se explica el motivo.

#### UC-6 — Usar un favorito en una FT (teclado)

1. El jugador está en la mesa, sin Ready enviado.
2. Pulsa `F`.
3. Ve a la primera card, "Favoritos (n)", y pulsa Enter.
4. Enfoca la card y pulsa Enter.

Resultado: idéntico a UC-5.

Notas: `F` es el mismo atajo de Fighter; no hay tecla nueva que aprender.

#### UC-7 — Usar un favorito offline

1. El jugador abre Home → Fighter Select.
2. Abre Favoritos y aplica uno.
3. Juega el modo offline.

Resultado: el pick offline queda con la combinación del favorito.

Notas: offline la edition siempre se permite, así que la card no muestra el
aviso de "regla apagada".

#### UC-8 — Un favorito no está disponible

1. El jugador enfoca un favorito con un traje que este PC no tiene instalado.
2. La card está atenuada y el detalle dice qué falta.
3. Select explica el motivo y no aplica.

Resultado: la selección viva no cambia y Ready sigue disponible con el pick
anterior.

Notas: al cargar, `Clamp` y `DropRemovedCustom` ya intentan arreglar el
favorito; este caso es el que sobrevive a ese arreglo.

#### UC-9 — La sala tiene Edition Select apagada

1. El host apaga Edition Select.
2. Las cards con edition distinta de USF4 muestran la edition tachada.
3. El jugador aplica una de ellas.

Resultado: el favorito se aplica con USF4 y la card lo indica antes de
aplicarse.

Notas: el favorito conserva su edition guardada; si el host vuelve a encender
la regla, la card recupera su significado.

#### UC-10 — La mesa está bloqueada

1. El jugador ya envió Ready, o hay partida en curso.
2. Abre Favoritos.
3. Las cards se ven, pero Select no aplica.

Resultado: la página sirve para consultar el mazo sin poder cambiarlo.

Notas: el motivo es el mismo `SelectionBlocker` que ya usan las filas de la
mesa.

#### UC-11 — El tope de diez

1. Ya hay diez favoritos.
2. El jugador elige "Añadir a favoritos".

Resultado: la acción dice que la lista está llena y no añade.

Notas: no hay un favorito 11; el límite es diez. Para meter otro, se quita uno
con UC-4.

#### UC-12 — Primer arranque, sin favoritos

1. El jugador abre Favoritos por primera vez.
2. La página dice "Aún no tienes favoritos" y ofrece el roster.

Resultado: puede añadir el primero con UC-1 o UC-2, o seguir usando el roster
como siempre.

Notas: la fila "Favoritos (0)" existe desde el primer arranque; no hay que
activar nada.

#### UC-13 — Reordenar el mazo (fuera de la v1)

1. El jugador enfoca una card y elige "Mover".
2. La coloca en otra posición.

Resultado: el orden cambia y se guarda.

Notas: el orden por defecto es el de creación; reordenar puede esperar.

## 6. Semántica de edition select

- La edition es un campo del pick (`Pick::edition`) y viaja en la red, pero solo
  se respeta si la regla de la sala está activa
  (`snapshot.lobbySettings.editionSelect`, `sf4e__Overlay.cxx:231`).
- Con la regla apagada, `NormalizeEdition` fuerza USF4 (14) y el favorito pierde
  su edition al aplicarse. Por eso los favoritos se normalizan con
  `editionSelect=true` al cargar (como `fighters[]`) y con la regla real al
  aplicar.
- La card debe mostrar la edition y marcarla como inactiva cuando la regla está
  apagada, para que el jugador no crea que va a jugar SFIV.
- La regla la controla el host (`lobbyEditionSelect`,
  `sf4e__OverlayPrefs.hxx:57`), así que un favorito puede cambiar de significado
  a mitad de sesión. No es un fallo: es la misma semántica que la selección
  normal.

## 7. Flujo de sala y FT

- Aplicar un favorito pasa por el mismo camino que cualquier edición: el callback
  `selection` de `ApplicationShell::Draw` (`:809-819`) y
  `DrawNetworkCharaConfig`.
- `snapshot.canEditSelection` es el candado. Con Ready enviado o partida en
  curso, la página se abre en lectura y las cards no aplican.
  `SelectionBlocker(v)` (`RoomControls.cxx:253`) da el motivo ya localizado.
- El rematch reutiliza la selección viva, así que un favorito aplicado persiste
  sin trabajo extra.
- `lobbyConditions` es la fuente del `request.character` de Ready
  (`sf4e__Overlay.cxx:445-446`) y de `view.selectionSummary` (`:388`), así que
  aplicar el favorito actualiza la mesa, el resumen y el envío a la vez.
- `lobbyMenuCharaID` debe seguir a `pick.fighter`; el envío sobrescribe
  `charaID` con él (`:446`).
- El servidor revalida con `selection::Valid` (`sf4e__SessionServer__Handlers.cxx:387`),
  pero eso es validez de red, no disponibilidad local. El cliente ya filtra con
  `Available` antes de Ready (`sf4e__Overlay.cxx:307`).

## 8. Localización

- Hay 14 catálogos en `locales/`. `localization_test` exige el **mismo conjunto
  de claves** en todos y los mismos marcadores (`localization_test.cxx:182-190`).
  Cada clave nueva va en los 14.
- Claves nuevas, junto a `selection.personal_action` (`locales/en.po:2276`):
  `selection.favourites`, `selection.favourites.detail`,
  `selection.favourite_add`, `selection.favourite_remove`,
  `selection.favourite_empty`, `selection.favourite_full`,
  `selection.favourite_unavailable`, `selection.favourite_saved`,
  `selection.favourite_edition_off`.
- `MenuScreenName` (`MenuPresentation.hxx:13`) necesita `"favourites"`, y
  `controller_navigation_test.cxx:826` la lista `ScreenNames`.
- El texto de las cards usa etiquetas ya existentes (`CostumeLabel`,
  `UltraLabel`, `common.none`), así que no se duplica vocabulario.

## 9. Tests

Existentes que hay que mantener verdes:

- `selection_value_test`, `shell_selection_journey_test`,
  `shell_selector_journey_test`: flujo y promesas de la selección.
- `ui_render_test`: dibuja todas las galerías; añadir la página de favoritos.
- `localization_test`: paridad de claves.
- `controller_navigation_test`: nombres de pantalla.

Nuevos:

1. Ida y vuelta JSON de diez favoritos, incluido uno con edition y otro con
   `personalAction` 255.
2. `Clamp` normaliza un favorito con edition inválida, conserva la válida y
   recorta la lista a diez.
3. `DropRemovedCustom` limpia un favorito con traje custom desinstalado.
4. Aplicar un favorito de otro personaje conserva su traje y ultra (regresión de
   R1).
5. Aplicar con la regla de edition apagada fuerza USF4 y lo indica.
6. Favorito no disponible: no se aplica y se explica (UC-8).
7. El tope: con diez, añadir no crece; sin duplicar el pick idéntico (UC-11).
8. Con Ready enviado, la página no aplica (UC-10).
9. La card dibuja nombre, ultra y personal action.
10. RB salta a favoritos y, con RB atado a confirmar, sigue habiendo camino por
    la primera card del roster.
11. La primera card del roster abre favoritos con d-pad y Select (sin atajo).
12. La fila `favourites` de `SelectionRows` abre la página.
13. UC-1/UC-2: añadir desde la selección actual y desde el roster guarda el pick
    entero sin cambiar la selección viva.
14. UC-4: quitar un favorito no cambia la selección viva ni reordena el resto.
15. UC-12: la lista vacía dice "Aún no tienes favoritos" y ofrece el roster.

## 10. Riesgos

**R1 — La memoria por personaje pisa el favorito (alto).**
`DrawNetworkCharaConfig` (`sf4e__Overlay.cxx:241-247`) detecta el cambio de
fighter y restaura `s_prefs.fighters[pick.fighter]` justo después de dibujar.
Si se aplica un favorito de otro personaje, el traje y el ultra del favorito se
pierden. Solución: al aplicar, escribir también el favorito en
`s_prefs.fighters[favourite.fighter]` antes de que corra esa restauración, o
marcar el frame de aplicación para saltarla.

**R2 — `operator==` sin los favoritos (medio).** El guardado compara `Data`; si
`favourites` no está en la comparación, añadir o quitar un favorito no persiste
nada.

**R3 — `Clamp`/`DropRemovedCustom` sin los favoritos (medio).** Un favorito con
contenido custom desinstalado o con edition imposible queda inválido y la card
miente.

**R4 — Aplicar sin comprobar disponibilidad (medio).** Un favorito con un traje
que este PC no tiene deja el pick no disponible y bloquea Ready. Hay que
normalizar y comprobar `Available` antes de aceptar.

**R5 — Ruido de guardado (bajo).** Cada alta o baja encola un guardado. Diez
favoritos son baratos, pero conviene no guardar en cada frame.

**R6 — `Delete` ambiguo (bajo).** En la mesa ya significa "salir del asiento"
(`RoomPanel.cxx:739`). En favoritos debe quitar la card; se desambigua por
pantalla.

**R7 — El escenario (bajo).** Si alguien mete el stage en el favorito, choca con
la autoridad de P1. Mantenerlo fuera.

**R8 — El atajo RB puede desaparecer (bajo).** Si el jugador ata confirmar o
volver a RB, `ControllerButtons` lo cede a la función de menú. Por eso la primera
card del roster, la fila de la mesa y X/F son caminos independientes, y un DS4
por DirectInput se queda solo con ellos.

**R9 — Ambiguación con el añadido desde el roster (bajo).** "Añadir a favoritos"
sobre una card del roster tiene que decidir qué combinación guarda: la viva si es
ese personaje, o la última suya. Si el jugador cree que guarda lo que ve en el
detalle y guarda otra cosa, se lleva una sorpresa. Por eso el pie dice qué
combinación va a guardar (ultra y personal action incluidos).

## 11. Plan por fases

1. **Datos y persistencia**: `favourites` y `FavouriteLimit` en `Data`,
   `operator==`, `ToJson`, `FromJson`, `Clamp`, `DropRemovedCustom`. Tests 1–3.
2. **Aplicar y alta/baja**: función que copia, normaliza, comprueba, escribe
   también en `s_prefs.fighters[]` y rechaza duplicados. Tests 4–7, 13–14.
3. **Página de favoritos**: `Page::Favourites` en `FighterSelector`, cards con
   `Appearance`, estrella, card "Favoritos (n)" insertada al principio del roster,
   acción de pie de alta y entrada desde `SelectionRows`. Tests 8–12, 15.
4. **Atajo RB opcional**: `MenuInput::Button`, `ControllerSample::Button`, la
   máscara de atajos y la leyenda de sala.
5. **Localización y nombres de pantalla**: 14 catálogos, `MenuScreenName`,
   `ScreenNames`.
6. **Pasada visual 16:9** y `.\dev.cmd full`.

Archivos que se tocan: `sf4e__OverlayPrefs.hxx/.cxx`, `sf4e__Overlay.cxx`,
`FighterSelector.hxx/.cxx`, `RoomPanel.cxx`, `ApplicationShell.hxx/.cxx`,
`ControllerNavigation.hxx`, `MenuNavigation.hxx`, `MenuPresentation.hxx`,
`locales/*.po`, `src/tests/*`.

## 12. Preguntas abiertas

1. **¿Los favoritos son globales o por personaje?** Asumo globales (es un mazo).
2. **¿Favoritos o slots numerados?** Resuelto: favoritos; un favorito guarda la
   combinación entera y no hay huecos ni números.
3. **¿La página sustituye al atajo de Fighter o convive?** Resuelto: convive. X/F
   abren el roster, la primera card lleva a favoritos y RB es solo un atajo.
4. **¿Qué pasa si un favorito no está disponible?** Resuelto: se rechaza y se
   explica (UC-8).
5. **¿Se permite guardar con edition cuando la sala la tiene apagada?**
   Resuelto: sí, se guarda la edition y se muestra inactiva (UC-9).
6. **¿Los favoritos llevan nombre?** El prompt no lo pide; la card ya muestra el
   personaje, el ultra y el personal action. Recomiendo dejarlo fuera de la v1.
7. **¿Se pueden reordenar?** Fuera de la v1 (UC-13); el orden es el de creación.
8. **¿La página existe también offline?** Resuelto: sí, es la misma pantalla
   (UC-7); offline la edition siempre se permite (`sf4e__Overlay.cxx:231`).

## 13. Compilar y probar

```powershell
.\dev.cmd build   # compila sin tests
.\dev.cmd test    # CTest sobre la build actual
.\dev.cmd full    # suite completa, recibo empaquetable
```

No edites archivos mientras compila: `build-current.ps1` compara una huella del
código y aborta con "Source changed during build".
