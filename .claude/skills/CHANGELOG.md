# Skills CHANGELOG — v1 → v2 → v3 → v4

## v4 (2026-06-06, post-v4.1.5 refresh)

**Anlass:** Nach Release v4.1.5 wurde der Agent `hardware-emulation-author`
hinzugefügt. Dieser produziert Tier-2.5-Controller-Emulatoren unter
`tests/emulators/<controller>/` und synthetische Flux-Generatoren mit
injizierbaren Defekt-Klassen unter `tests/flux_gen/<controller>/`. Zwei
Skills hatten thematischen Überschneidungs-Bereich und brauchten klare
Abgrenzung + Cross-Ref.

Außerdem zwei echte Skill-Lücken geschlossen, die im Audit identifiziert
wurden: forensische Audit-Trail-/Report-Implementation (`SKELETON_PLANNED`,
keine Skill-Abdeckung) und Protection-DB-Kuration (in v4.1.5 9 Shadow-
Bugs entdeckt, kein Skill schützte davor).

### Neuer Skill: `uft-audit-trail`

**Warum:** `include/uft/uft_audit_trail.h` (22 Funktionen) und
`include/uft/uft_forensic_report.h` (13 Funktionen) sind `UFT_SKELETON_
PLANNED` — 0 Implementierungen, viele Callsites. Das ist die **forensische
USP** des Projekts. Bisher kein Skill, der den Wiring-Workflow + die
Invarianten kapselt.

**Was er enforcet:**
- 6 forensische Invarianten F1–F6 (monoton, append-only, hash-chain,
  no fabrication, dual-clock, severity-truthful)
- 37 Event-Typen in 11 Bändern (Session/File/Format/Track/Sector/
  Hardware/Recovery/Error/Checksum) — Erweiterungsregeln pro Band
- 6 Export-Formate × 14 Report-Sektionen (`UFT_REPORT_SEC_*`)
- Hash-Chain-Pattern + Tampering-Detection-Test
- Locale-Determinismus + Atomic-fsync per Eintrag

**Cross-Refs:** `forensic-integrity` Agent (Review), `uft-crc-engine`
(Hash-Primitive), `uft-recovery-integrity` (Caller-Disziplin).

### Neuer Skill: `uft-protection-db`

**Warum:** v4.1.5 entdeckte 9 Shadow-Duplikate in
`c64_protection_db.c` — `c64_lookup_title()` benutzt bidirektionales
`strstr` + First-Match-Wins, das macht jeden frühen Eintrag zu einer
Falle für spätere kanonische Einträge. Whack-a-mole bei Test-Failures
(5 Folge-Fixes bis grün). `uft-protection-scheme` ist die Algorithmus-
Seite; Daten-Kuration fehlte komplett.

**Was er enforcet:**
- Lookup-Contract dokumentiert (bidirektional, lowercase, FIFO)
- 5 De-Dup-Invarianten D1–D5 (canonical-uniqueness, no-shadow,
  no-prefix-swallow, replace-stub, alphabetical)
- Workflow: Search-before-Add, Self-Lookup-Probe, alphabetical insert
- Common Pitfalls: Stub-Behalten, Year-Fabrication, Article-Variants
- Open follow-up: ~60 latente Shadow-Bugs in c64 DB (Option A Curate
  vs. Option B Lookup-Refactor)

**Cross-Refs:** `uft-protection-scheme` (Algorithmus), `uft-ml-training-
data` (DB liefert Labels), `stub-eliminator` (alte `PROT_UNKNOWN, 0, "",
""` Stubs = D4-Verletzungen).

### README + CHANGELOG

- Skill-Count 20 → 22 (UFT)
- Index-Tabelle erweitert um beide neuen Zeilen unter "Code-Korrektheit"
- Faustregel-Tabelle erweitert um drei Einträge (Audit, Protection-Algo,
  Protection-Data)
- Verifikations-`wc -l` Erwartung 20 → 22

### Was bewusst NICHT in dieser Runde gemacht wurde

- `uft-pll-decoder`, `uft-loss-preflight`, `uft-libusb-mock` — als
  P-MITTEL/NIEDRIG identifiziert; warten bis die nächste Iteration sie
  auslöst (kein Skill ohne wiederkehrenden Pain-Point).
- **Audit-trail / Report Skeleton wird hier NICHT implementiert** — das
  ist Code-Arbeit, dieser Skill kapselt nur die Workflow-Disziplin.
- **Protection-DB ~60 latente Duplikate werden NICHT gefixt** — der
  Skill dokumentiert sie als follow-up; bewusste Trennung Skill-Erstellung
  vs. Daten-Cleanup.

### `uft-flux-fixtures`

- **Related** ergänzt: Cross-Ref zu `hardware-emulation-author`.
  Klargestellt: dieser Skill = **format-level** Fixtures (SCP/HFE/ADF/D64
  Parser-Tests, klein, deterministisch); Agent = **controller-level** Flux
  mit Defekt-Klassen (weak bits, CRC, Copy-Protection, Half-Tracks),
  HAL-Replay-Niveau.
- Neue Pfad-Konvention dokumentiert: `tests/flux_gen/<controller>/` (seit
  v4.1.5, owned vom Agent, nicht vom Skill).

### `uft-hal-backend`

- **Related** ergänzt: Cross-Ref zu `hardware-emulation-author`.
  Workflow-Hinweis: Agent **vor** dem real-HW Bench-Session aufrufen, damit
  Sim das fängt was die Bench-Session nicht fängt (Timeouts, FIFO-Overflow,
  fehlende Index-Pulse).
- Cross-Ref zu `uft-flux-fixtures` als Sibling-Layer ergänzt
  (format-level vs. HAL-replay-level).

### Was bewusst NICHT geändert wurde

- **Keine Versions-Bumps in Skill-Frontmatter** — die Skills sind
  formats-agnostisch, kein Bezug zu UFT-Version.
- **`uft-release` v4.2.0-Placeholder** bleibt — wird beim nächsten
  echten Release-Tag aktualisiert, dient nur als Beispiel.
- **CHANGELOG-Erwähnungen v4.1.3 / commit 70e60fc** bleiben — sind
  historisches Audit-Datum, nicht "aktueller Stand".
- **README-Counts (20 UFT + 1 HTB)** unverändert — keine neuen Skills,
  nur Cross-Refs.
- **Keine neuen Skills** angelegt — `hardware-emulation-author` ist ein
  Agent (Fable 5), kein Skill. Skill-Trigger sind weiterhin der
  Workflow-Einstieg; Agenten sind die Ausführungs-Tier.

---

## v3 (2026-04-25, second iteration)

### Neuer Skill: `uft-cross-platform-build`

**Warum:** Wiederkehrender Schmerz über mehrere Sessions — qmake-Quirks,
MSVC-C89-Default, VLAs, basename collisions, macos-14-ARM64-Tag,
MinGW-IQTA-PATH. Klassisch hochfrequente, mechanisch lösbare Probleme
ohne dedizierten Skill.

**Was er enforcet:**
- 6-Konfigurations-CI-Matrix dokumentiert (Ubuntu C++17, Ubuntu C++20,
  Debian apt-Qt, macOS ARM64, Windows MinGW, Windows MSVC)
- 17 nummerierte FIX-Klassen (FIX-001 bis FIX-017), jede mit
  Symptom + Root-Cause + Patch-Pattern
- Konvention: jede neue Build-Failure-Klasse bekommt eine FIX-NNN-ID
  und einen Match-Eintrag im Klassifier (selbst-erweiternd)

**Liefert:**
- `SKILL.md` (~260 Zeilen) mit Matrix-Tabelle, Fix-Klassen-Tabelle,
  Workflow, Common Pitfalls
- `scripts/classify_build_error.sh` — getestet: nimmt einen Build-Log
  und mappt Errors auf FIX-IDs (im Test fand er 3 Klassen aus einem
  realistischen mixed Log)
- `scripts/check_build_parity.sh` — findet Files in `src/` die im `.pro`
  fehlen (CMake auto-globt, qmake nicht); auf echtem Repo getestet
- `scripts/run_matrix_local.sh` — orchestriert lokale Builds für jede
  Matrix-Konfiguration, skippt was der Host nicht kann (Debian via
  docker container `debian:12`)
- `reference/fix_class_history.md` — narrative Doku jeder FIX-NNN mit
  Discovery-Story und Patch-Pattern

### Cross-Refs ergänzt

- `uft-release` → cross-platform-build (full matrix muss grün sein
  vor Tag-Push)
- `uft-stm32-portability` → cross-platform-build (Desktop-Sibling
  des Embedded-Targets)
- `uft-debug-session` → cross-platform-build (Build-Fehler gehören
  dort hin, nicht in debug-session)
- `uft-format-plugin` → cross-platform-build (Parity-Check vor Push)

### README + Verifikation

- Skill-Count von 18 auf 19
- Faustregel-Eintrag für "Build kaputt auf irgendeiner Platform"
- Build/Test/Performance-Sektion zeigt jetzt
  "Linux (Ubuntu/Debian) / macOS / Windows (MinGW/MSVC)" explizit

---

## v2 (2026-04-25, first iteration) — v1 → v2

Datum: 2026-04-25
Anlass: Review nach Audit von UFT v4.1.3 (commit 70e60fc)

## Neuer Skill

### `uft-recovery-integrity`

**Warum:** Das Audit fand 6 Datenintegritäts-Bugs in `src/recovery/`,
darunter 2 kritische (lineare Interpolation auf Binärdaten,
Single-Bit-CRC-False-Positives). Keiner der existierenden 17 Skills
deckt den Bereich ab — `uft-deepread-module` betrifft die
Pipeline-Architektur, nicht die Standalone-Recovery-Utilities.

**Was er enforcet:**
- 5 Datenintegritäts-Invarianten (I1–I5)
- Confidence-Map und `RECONSTRUCTED`-Flag-Disziplin
- Uniqueness-Check für probabilistische Korrekturen
- `crc_ok` / `quality` müssen propagiert werden (nicht ignoriert)

**Liefert:**
- `SKILL.md` (402 Zeilen) mit den 5 Invarianten, Vor/Nach-Codebeispielen
- `scripts/lint_recovery.sh` — heuristischer Lint, der die 5 Bug-Klassen
  erkennt (getestet: fängt 5/6 Audit-Bugs)
- `templates/recovery_fn.c.tmpl` — Scaffold für neue Recovery-Funktionen
  mit eingebauter INTEGRITY-CONTRACT-Doku
- `reference/audit_2026_04_25.md` — die 6 Audit-Bugs als
  Fallstudien zum Zitieren in PR-Reviews

## Bestehende Skills geändert

### `uft-deepread-module`

- **Common Pitfalls** ergänzt: "Fabricating bytes without marking them"
  — pipeline modules, die Bytes synthetisieren, müssen die gleichen
  Integrity-Regeln befolgen wie standalone-Recovery-Code.
- **Related** ergänzt: Cross-Link zu `uft-recovery-integrity`.

### `uft-crc-engine`

- **Common Pitfalls** ergänzt: "Single-bit correction without
  uniqueness check" — mit Vorher/Nachher-Code und Verweis auf
  Audit-Bug. Ohne diese Regel hätte der Skill den
  CRC-False-Positive-Bug nicht verhindert.
- **Related** ergänzt: Cross-Link zu `uft-recovery-integrity` für
  CRC-als-Korrekturmechanismus (im Gegensatz zu CRC-als-Verifikation).

### `uft-format-plugin`

- **Common Pitfalls** ergänzt: "Domain helpers redefined per-file"
  — verweist auf den Audit-Bug mit 5 Definitionen von
  `uft_c64_speed_zone` und etabliert die Regel: vor neuem Helper
  immer `grep -rn` durch include/ und src/.

## Templates fertiggestellt

### `uft-llm-prompt-engineering/templates/`

Drei Templates waren in `SKILL.md` referenziert, aber fehlten im
v1-Paket. Neu geschrieben:

- `new_plugin.md` — LLM-Briefing für neue Format-Plugins, koppelt an
  `uft-format-plugin`
- `refactor.md` — Cross-cutting-Refactor mit explizitem Scope-Limit;
  Anti-Architektur-Creep
- `bug_repro.md` — 5-Phasen-Debug-Workflow als LLM-Prompt

Damit sind alle 6 in `SKILL.md` angekündigten Templates real verfügbar.

## Cleanup

### Zombie-Verzeichnisse entfernt (12 Stück)

Brace-Expansion-Artefakte aus `mkdir -p` mit gequoteten Pfaden:

```
uft-format-converter/{templates,scripts}/
uft-ml-implement-skeleton/{templates,scripts,reference}/
uft-protection-scheme/{templates,scripts}/
uft-benchmark/{templates,scripts}/
uft-ml-training-data/{templates,scripts}/
uft-llm-prompt-engineering/{templates,reference}/
uft-flux-fixtures/{scripts/generators,templates}/
uft-ml-protection-classifier/{templates,reference}/
uft-format-plugin/{templates,scripts}/
uft-hal-backend/{templates,scripts}/
uft-qt-widget/{templates,scripts}/
uft-crc-engine/{templates,scripts/reference}/
```

Alle waren leer und mit literalen Curly-Braces im Pfad. Komplett
gelöscht.

### `README.md` umgeschrieben

v1: behauptete "4 Skills für die Arbeit an UFT mit LLMs und ML-Modulen"
— veraltet, war nur für die letzten 4 ML/LLM-Skills geschrieben.

v2: vollständiger Index aller 18 Skills, gruppiert nach Domäne
(Code-Korrektheit / Build-Test-Performance / UI-HAL / ML / Meta), mit
Faustregel-Tabelle "welcher Skill für welche Aufgabe", und einem
Verifikationsblock zum Selbstcheck nach der Installation.

## Was bewusst NICHT geändert wurde

- **Authoring-Guide** (`docs/SKILL_AUTHORING_GUIDE.md`) — bereits
  exzellent, Konventionen sind klar und konsistent. Nur in einem Punkt
  Erinnerungswert: zwei der ML-Skills haben 12-13 Zeilen YAML
  description (Limit ist 6-8). Vermutlich gerechtfertigt durch
  Bilingualität, aber bei der nächsten Iteration prüfenswert.
- **Bilingualität** in Trigger-Phrasen — alle Skills haben deutsche +
  englische Phrasen. Nicht in jedem Skill 50/50, aber das ist OK,
  weil Anwendungs-Kontexte unterschiedlich sind (Debug-Session ist
  englisch-lastiger; Format-Plugin ist deutsch-lastiger).
- **Frontmatter-Schema** — `name + description` reicht, keine
  `compatibility:` ergänzt. Falls UFT-Skills mal in andere Surfaces
  exportiert werden (Cowork etc.), kann man das nachziehen.
- **Keine eval-Suiten** angelegt. Skill-Creator empfiehlt
  `evals/evals.json` mit Test-Prompts, um die Trigger-Rate zu messen.
  Lohnenswert ab dem Zeitpunkt, wo du merkst, dass Claude einen Skill
  systematisch nicht zieht — bis dahin ist es Overhead.

## Empfohlene Folge-Schritte

1. **`lint_recovery.sh` als pre-commit-Hook einbauen** für
   `src/recovery/`-Änderungen. Zwei Zeilen in `.git/hooks/pre-commit`.
2. **Die im `audit_2026_04_25.md` dokumentierten 6 Bugs fixen** — der
   Skill erklärt das *Wie*, aber die alten Bugs müssen separat
   behoben werden. Reihenfolge: erst CRC-False-Positive (Bug 2), dann
   Interpolation (Bug 1), dann Voting (Bug 3), Rest hat niedrigere
   Priorität.
3. **Bei nächstem Refactor durch `src/flux/`** den dort gefundenen
   PLL-Phase-Bug (`pll->phase` wird gesetzt aber nie gelesen)
   beheben — fällt nicht in einen vorhandenen Skill, ist aber
   einzeln dokumentiert in `audit_2026_04_25.md`.
4. **Wenn Trigger-Probleme auftreten:** `uft-recovery-integrity` ist
   neu, also kann es sein, dass Claude ihn anfangs nicht zieht.
   Falls ja: erste Eval-Suite anlegen mit 5-6 realistischen Prompts
   ("Prüfe meine sector_recovery.c", "Wie verhindere ich
   CRC-Kollisionen bei Korrekturen?", etc.) und Trigger-Rate messen.
