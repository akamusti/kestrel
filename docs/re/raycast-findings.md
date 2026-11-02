# Raycast behaviour → Kestrel (reverse-engineering notes)

This file separates **observation / inference / design decision**. Kestrel is GPL-2.0-or-later, so
nothing here is code, artwork or data taken from Raycast; only *behaviour and interface contracts*
were carried over.

## Sources

| # | Source | Kind | Used for |
|---|---|---|---|
| S1 | [`typecraft-dev/oma-cast`](https://github.com/typecraft-dev/oma-cast), `SPEC.md` + `reference/01..05` | Open source (MIT helper), static RE of Raycast **2.8.2** | Ranking algorithm, window model, shortcuts, extension manifest |
| S2 | Raycast public docs (`developers.raycast.com`) | Public | Extension API contract, manifest fields |
| S3 | Local KRunner **6.31** (`kf6-krunner`, Plasma 6.7.5) | Running-system artifact | Inventory of 28 runners, `org.kde.krunner1` D-Bus contract |
| S4 | `<oma-cast>/plugin/core/Ranker.js`, `RootSearch.js` | Open-source port | Working reference implementation of the algorithm |

The Raycast application itself was not downloaded or inspected (user decision). S1's RE was carried
out on Raycast 2.8.2 `backend.mjs` / `index.mjs` and is documented with minified symbol names, so it
can be re-verified.

## 1. Ranking (root search)

**Observation [S1 §4]:** Raycast ranks in three stages: candidate production → match classification
→ a 15-step comparator. What matters is not "highest score" but the **class hierarchy of matches**.

- Text normalisation: lowercase + camelCase boundaries + ASCII transliteration.
  Separator set: `\t \n space - . / ( ) [ ]`.
- Fuzzy matching via dynamic programming: per matched character
  `4` (position 0, first row) / `3` (word start) / `2` (elsewhere) / `1` (separator↔separator);
  each non-adjacent jump costs `-1`.
- Threshold `rootSearchSensitivity`: low = any match, medium = `score ≥ 1.5·(n−skipped−2)+4`,
  high = `score > 2·(n−skipped)`. **Default: medium** (backend default; the UI shows "high").
- Comparator order (abridged): exact alias → exact search term → exact title → exact subtitle →
  prefix alias/term → overbounds (1–3 extra chars) → title score → **frecency** → type priority
  (application 5 > command 4 > quicklink 3 > preference/file 1 > contact 0) → collator.
- **Frecency math** [S1 §4.6]: 10-day half-life, +100 points per launch, represented by a single
  `anchor` timestamp:
  ```
  λ = ln2/10
  score(T) = max(1, exp(λ·(anchor−T)/86400))
  visit(t): anchor' = t + 86400·ln(score(t)+100)/λ
  ```
- **Learned search terms**: queries that opened an item stay associated with it for 17 days
  (exact / prefix / overbounds classes).

**Decision:** `src/ranker.cpp` ported this model to C++: scoring, threshold, frecency and search-term
tiers are direct; the 15-step comparator is reduced to 9 steps inside `SearchModel` **while keeping
the order**. Measured: a typical query over 300 applications stays under 2 ms.

**Not adopted:** Raycast's `announcements`, `agents`, `calendar`, `flight`, `weather` sections and
everything AI-related.

## 2. Window model

**Observation [S1 §5.1]:** 750×475 logical px, 66 px header, 42 px footer, radius 8 (26 on
macOS 26), translucent body (`40%` alpha) over compositor blur.

- **Compact mode**: only the 66 px search bar is visible until the user types, then the window grows.
- On show: pick the monitor (`mouse-screen` default), horizontally centred, ~1/4 down from the top,
  focus the search field.
- **Buffer keystrokes typed between the hotkey and focus, then replay them** — mandatory on Wayland
  where focus is slow. Keep the window warm (hidden, not destroyed).
- **Hide on focus loss**; after `popToRootTimeout` (default **90 s**) pop to root.
- **Esc**: default "navigate back or close"; alternative "close and pop to root".
- **Single instance**: a second launch forwards its argv (`toggle`, `run <id>`, deep link) and exits.

**Decision:** fixed 760×560 window; the panel fills it, so the silhouette never jumps while typing
(animating the window size on Wayland is janky). Results beyond the visible rows scroll inside the
list. Translucency and a hairline border are in; KWin blur is **not yet** (needs a KWin effect/wayland
protocol; on the roadmap). Single instance uses `QLocalServer`; KGlobalAccel handles only the global
shortcut over D-Bus, keeping the process handoff local and small. The slide-in uses only `transform`
and `opacity` (composite-only) over 110–140 ms, per the animation rule in `AGENTS.md`.

## 3. Shortcuts

**Observation [S1 §10.3], Linux = Raycast's Windows column:**

| Function | Key |
|---|---|
| Primary / secondary / tertiary | ↵ / Ctrl↵ / Ctrl⇧↵ |
| Action panel | Ctrl K |
| Copy | Ctrl⇧C |
| Favorite / reorder | Ctrl⇧F / Ctrl Alt ↑↓ |
| Jump to item 1–9 | Ctrl 1–9 |
| Back / forward / pop to root | Esc or Ctrl[ / Ctrl] / ⇧Esc |
| Next/previous section | Ctrl↓ / Ctrl↑ |
| Filter dropdown | Ctrl P |
| Settings | Ctrl , |
| Navigation bindings | Emacs (Ctrl B/F/P/N, default) or Vim (Ctrl H/L/K/J) |

**Decision:** the first cut implements ↵, ↑↓, Ctrl 1–9, Ctrl⇧C, Esc, ⇧Esc and Ctrl R (reload);
the Ctrl K action panel, Ctrl P source filter and web-search fallback landed with roadmap §9.5.

## 4. Extension API (manifest)

**Observation [S1 §9.7 / ref 03 §4]:** the Raycast manifest is `package.json`:
`{name, title, description, icon, author, commands[], preferences[], keywords[], categories[]}`.
Command: `{name, title, subtitle?, description, icon?, keywords?[], mode: "view"|"no-view"|"menu-bar",
preferences?[], arguments?[] (max 3), interval?}`.
Argument: `{type: "text"|"password"|"dropdown", name, placeholder, required?, data?}`.
Preference types: `textfield, password, checkbox, dropdown, appPicker, file, directory`.
Command id: `c:n:<extensionId>::-::<commandName>`.

**Decision:** the Kestrel manifest will follow these **field names** (for ecosystem tooling), but the
runtime differs: no Node worker — one `QProcess` per extension plus line-delimited JSON-RPC.
`mode: "no-view"` commands run directly (enough for calculation/command extensions); `"view"`
commands will arrive later with a QML-based view contract. Raycast's React reconciler / render-tree
protocol is **not** reimplemented — it only matters for running Raycast extensions verbatim, which is
out of scope.

**Not adopted:** Store endpoints, a copy of `@raycast/api`, the icon enum, theme files.

## 5. KRunner (local artifact, REA + D-Bus)

**Observation [S3]:** 28 runners are registered (`krunner --list`): applications, calculator, unit
converter, date/time, colours, dictionary, spell check, windows, sessions, bookmarks, recent files,
shell commands, web search keywords, system settings, power, and more. Some are in-process plugins
(`.so`), others are **D-Bus runners**.

**D-Bus runner contract [S3, `gdbus introspect`]:** `org.kde.krunner1`:
```
Match(query: s) → a(sssida{sv})
      // (id, text, iconName, category relevance, relevance, properties)
Actions(matchId: s) → a(sss)      // (id, text, icon)
Run(matchId: s, actionId: s)
```
Runner registration lives in `.desktop` files: `X-Plasma-DBusRunner-Service`,
`X-Plasma-DBusRunner-Path`, `X-Plasma-Runner-Syntaxes`, `X-Plasma-Request-Actions-Once`.
`org.kde.krunner /App` only exposes `display/query/toggleDisplay` — it returns **no results**, so the
way to use KRunner as a result source is the runners' own D-Bus interfaces, not
`org.kde.krunner.App`.

**Decision — hybrid:**

1. **Fast path (Kestrel-owned):** applications. The `.desktop` index lives in memory and search never
   crosses D-Bus; results are personalised with frecency. The launcher's "instant" feel depends on it.
2. **Long tail (borrowed from KRunner):** window switching, calculator, unit conversion, colours,
   dictionary, sessions, recent files → asynchronous queries over `org.kde.krunner1`, appended as
   they arrive. We do not rewrite 20+ runners.
3. **Ranking:** KRunner's `category+relevance` pair enters Kestrel as a **section priority**;
   frecency and the match-class hierarchy stay Kestrel's (the Raycast model).

Implemented in `src/krunnerclient.cpp`: runner metadata is discovered off the UI thread, `Match`
calls are debounced and asynchronous with a bounded timeout, and results are appended after native
applications. Activating a runner result invokes its `Run` method. Superseded query replies are
discarded; Kestrel's application fast path remains local and immediate.

## 6. Open questions

- Steps 5–8 of Raycast's 15-step comparator (contact special case, `hasExactTitle` propagation) have
  no exact counterpart in our narrowed model. If real-world ordering diverges, go back to S1 §4.5 and
  port it verbatim.
- The `frecency_date` epoch (Unix vs 2001) is not settled in S1; Kestrel uses Unix seconds.
- KWin blur still needs a KWin effect/Wayland protocol. KGlobalAccel registration uses QtDBus
  directly, so the core does not require KF6 development packages.
- REA was installed here **without an analysis engine** (no Ghidra/IDA/JADX): the MCP server is
  connected and lists 229 tools, but ELF decompilation returns `provider_unavailable`. The KRunner
  `.so` metadata was therefore read with `objcopy` + a CBOR decoder; deep decompilation would need
  Ghidra to be installed.
