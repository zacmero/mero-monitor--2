# Mero Thought Terminal (Phase 2)

Architecture and design specifications for the dedicated Thought Terminal running natively on Windows CE.

---

## 1. Display & Aesthetic Paradigm

The Foston FS-460BT display is a 480×272 16-bit RGB565 resistive panel. To maximize legibility and visual punch within hardware limitations, the UI follows a strict aesthetic:

- **Deep Black Background**: `#000000` (zero light bleed on black pixels).
- **High-Contrast Monospace Typography**: Clean 14-16px fonts for reflections, memory streams, and searchlight state.
- **Minimalist Color Palette**:
  - `Neon Green / Phosphor (#00FF80)`: Primary system messages and header status.
  - `Cyan (#00C8FF)`: Memory IDs, timestamps, and metadata tags.
  - `Soft Amber (#FFB400)`: Searchlight state, active queries, warnings.
  - `Ghost White / Muted Gray (#DCDCDC / #888888)`: Reflection text, body content.
- **Deliberate Animation Restraint**: Low CPU load, subtle character pacing, occasional glitch/fade effects.

---

## 2. Ingested Artifact Types

The Thought Terminal displays authored internal artifacts from Suzy / Mero Protocol:

1. **Chain of Memories Reflections**: Deep contemplative thoughts and system introspection.
2. **Focus / Searchlight State**: Active goal, current hypothesis, attention vectors.
3. **Persisted Thoughts**: Long-term retained ideas and insights.
4. **Memory Discoveries**: Newly linked or indexed concepts.
5. **Internal Event Summaries**: Chronological milestone digests.

---

## 3. Communication Channel

- **Primary**: Local file ingestion (`\SDMMC\MERO\stream.dat` or `\SDMMC\MERO\thoughts\`).
- **Secondary (Real-time)**: TCP stream over ActiveSync PPP bridge via `/dev/ttyUSB0`.
