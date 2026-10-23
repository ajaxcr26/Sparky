Use the following as the build brief. It defines the behavior, interface, generation rules, and validation criteria so the developer has fewer gaps to fill with guesses.

A polished first version will depend on a focused scope and repeated testing inside a DAW. **Prioritize a complete, reliable workflow over adding every generation mode immediately.**

---

# SPARK — Detailed product and implementation specification

## 1. Product definition

Build **SPARK**, a generative music plugin that creates individual musical elements around an existing beat.

The user provides musical context, chooses the type of element they need, generates candidates, refines a chosen result, and exports it into their DAW.

SPARK must support elements ranging from prominent lead melodies to brief decorative sounds.

Its defining qualities should be:

- **Musically aware:** Generated parts follow the supplied harmony and timing.
- **Restrained:** Sparse requests produce sparse results.
- **Editable:** Users can modify notes, sound design, timing, and effects.
- **Predictable:** Locked material stays unchanged.
- **Fast to audition:** Comparing ideas requires minimal clicking.
- **Reliable:** Playback, export, and session recall behave consistently.

The primary target workflow is FL Studio on Windows using VST3. Design the architecture so macOS and AU support can be added without rewriting the musical engine.

## 2. Define the first release precisely

The first release must include:

1. Chord MIDI import and manual chord entry.
2. Manual key, tempo, and time-signature controls.
3. Lead, Response, Accent, and Impact generation.
4. Three candidates per generation.
5. An internal synth and layered impact engine.
6. Piano-roll editing.
7. Region-based regeneration.
8. Independent musical and sound locks.
9. MIDI and WAV export.
10. Undo, history, and full session recall.

Treat audio chord detection, live listening, advanced atmosphere generation, and continuous evolving performances as later features.

**Do not display unfinished modes as working features.** A smaller finished plugin should feel intentional.

## 3. Core user journey

The initial screen should make the next action obvious.

Display:

> **Add your chords. Create the missing piece.**

Provide three entry actions:

- **Import MIDI**
- **Enter chords**
- **Try demo**

The demo should load a bundled progression and a finished example. It must work without an account or external service.

After context is loaded:

1. Show the progression on a bar-aligned timeline.
2. Ask the user to choose a role.
3. Offer a useful default sound.
4. Allow an optional text instruction.
5. Generate three candidates.
6. Let the user audition them immediately.
7. Allow editing and export without navigating away.

A user should be able to reach their first playable result without opening an advanced settings panel.

---

## 4. Interface layout

Use a resizable interface with a reference size around **1180 × 760 logical pixels**, and a usable minimum around **960 × 640**.

Organize it into four persistent regions.

### Top bar

Include:

- Project or preset name.
- Undo and redo.
- Host synchronization status.
- Tempo.
- Time signature.
- Key and scale.
- Settings.
- A clear indication if generation requires an internet connection.

When synchronized, label the tempo **Host: 150 BPM**. When manually set, label it **Manual: 150 BPM**.

Avoid displaying a tempo control that appears editable while silently ignoring changes.

### Context strip

Show:

- Bar numbers.
- Chord blocks.
- Selected generation region.
- Loop boundaries.
- A playhead.
- Import and edit controls.

Clicking a chord opens a compact editor for root, quality, extensions, inversion, and duration.

Allow zooming and horizontal scrolling without changing the generation region accidentally.

### Creation panel

Place the following controls on the left:

- Role selector.
- Sound family.
- Prompt field.
- Core musical controls.
- Generate button.
- Expandable advanced options.

The panel should remain stable when candidates change.

### Main editor and candidate area

Use the center and right area for:

- Piano roll or impact-layer editor.
- Candidate tabs.
- Playback controls.
- Version comparison.
- Export handles.

Display three candidates as **A / B / C**, with descriptive subtitles such as:

- Repeating motif.
- More space.
- Alternate ending.

Descriptions should reflect actual differences.

### Visual style

Use a clean, dark production-tool appearance:

- Charcoal background.
- Distinct panel surfaces.
- High-contrast text.
- One primary accent color.
- Separate colors for context notes and generated notes.

Do not rely on color alone to indicate locks, selection, or errors.

Use restrained animation. Reserve movement for the playhead, meters, and brief state changes.

---

## 5. Musical data model

Establish one canonical internal representation for musical timing.

Store note and chord positions in musical ticks or an equivalent exact representation. Convert to samples only at playback and rendering boundaries.

Do not repeatedly convert between rounded floating-point seconds and beats.

Each generated note should contain:

```text
id
start
duration
pitch
velocity
channel
articulation
expression
sourceVersion
lockedProperties
```

Each chord event should contain:

```text
id
start
duration
rootPitchClass
quality
extensions
alterations
bassPitchClass
voicedPitches
source
confidence
```

Preserve imported chord voicings separately from symbolic chord labels. A chord label does not capture the entire original performance.

Store the generation specification with every candidate:

```text
role
region
harmonicContext
rhythmicSettings
register
density
complexity
motifSettings
soundSettings
placementRules
locks
seed
engineVersion
```

Saving the seed alone is insufficient. Save the actual generated notes and sound parameters.

## 6. MIDI import behavior

Support both single-track and multitrack MIDI files.

When a file contains several tracks:

- Display track names and note counts.
- Let the user choose which tracks provide harmony or melody context.
- Provide an initial recommendation.
- Preserve the source file.

Handle:

- Pickup notes.
- Silence before the first chord.
- Sustained notes crossing bar lines.
- Overlapping notes.
- Tempo changes.
- Non-4/4 time signatures.
- Files with no tempo metadata.

If imported tempo differs from the host, offer:

- **Use host tempo:** Preserve musical positions.
- **Use file tempo:** Set the plugin’s manual preview tempo.

Do not change the DAW’s tempo without a supported, explicit user action.

For chord recognition from MIDI, group notes using onset proximity and overlap. Do not assume every note beginning on a new tick indicates a new chord.

Show ambiguous labels as editable suggestions.

---

## 7. Generation controls with clear meaning

Each control must have a documented musical effect.

### Density

Control the target number of note onsets or events.

For an initial four-bar Lead preset, use approximate targets:

| Setting | Target onsets |
|---|---:|
| Very sparse | 4–8 |
| Sparse | 8–14 |
| Medium | 14–24 |
| Busy | 24–40 |
| Very busy | 40–64 |

These are starting guidelines, not universal rules. Adjust for role, tempo, and phrase length.

Evaluate density using note onsets, including simultaneous events where relevant. Do not achieve “sparse” merely by lowering velocity.

### Complexity

Control:

- Rhythmic variety.
- Syncopation.
- Interval variety.
- Ornamentation.
- Phrase asymmetry.

A high-complexity, low-density phrase can contain a few unusually placed notes.

### Range

Expose a minimum and maximum pitch.

Display note names and MIDI note numbers because DAWs label octaves differently.

For example:

> C5–C6 · MIDI 72–84

Use one documented octave naming convention throughout the interface.

### Repetition

Control how often the primary motif returns.

At high settings, preserve recognizable rhythm and contour while allowing chord-aware pitch changes.

### Variation

Control how much repeated phrases differ.

Keep variation independent from complexity.

### Energy

Influence velocity, articulation, envelope response, and sound movement within bounded limits.

Do not increase output loudness uncontrollably.

### Presence

Provide three understandable starting levels:

- Background.
- Supporting.
- Foreground.

Use these to establish defaults for activity and sound character. Show resulting parameter changes so the behavior remains explainable.

Keep a separate output gain control.

---

## 8. Lead generation requirements

Build leads through a structured process:

1. Select a phrase structure.
2. Create a short rhythmic motif.
3. Assign pitches using the harmony.
4. Repeat and develop the motif.
5. Add optional pickups or ornaments.
6. Shape the ending.
7. Validate constraints.

For an eight-bar phrase, a useful default structure is:

- Bars 1–2: Introduce motif.
- Bars 3–4: Repeat with a small change.
- Bars 5–6: Develop the idea.
- Bars 7–8: Return and create an ending or loop turnaround.

Do not force this structure when the user requests another one.

### Harmonic behavior

Separate:

- Chord tones.
- Scale tones.
- Passing tones.
- Explicit chromatic notes.

Offer harmonic adherence settings:

- **Strict:** Chord tones dominate structural positions.
- **Balanced:** Scale tones and resolved passing notes are allowed.
- **Loose:** More tension is allowed within the user’s constraints.

Borrowed chords must affect local pitch choices. Do not force all notes into the global scale if that contradicts the supplied progression.

### Melodic behavior

Default to a mix of repeated notes and stepwise movement, with occasional controlled leaps.

Allow users to set:

- Maximum leap.
- Preferred register.
- Rest amount.
- Note-length range.
- Phrase-ending behavior.
- Looping or resolved ending.

A loop ending should transition sensibly into the opening. A resolved ending should have a different musical purpose.

### Triplet handling

Provide:

- Straight.
- Triplet.
- Mixed.

In Mixed mode, include an adjustable triplet occurrence setting.

“Occasional triplet pickups” should produce localized gestures near phrase boundaries, while preserving the rest of the phrase’s rhythm.

---

## 9. Response and Accent behavior

### Response

Require either melody context or a user-selected response region.

Find gaps using note activity and sustained durations, not just note onsets.

Expose:

- Gap sensitivity.
- Maximum overlap.
- Response length.
- Register relationship.
- Motif relationship.

Motif relationship options:

- Echo.
- Contrast.
- Rhythm match.
- Contour variation.

If no gap satisfies the settings, explain the conflict and offer an adjustment. Do not silently fill the entire section.

### Accent

Generate isolated musical punctuation.

Useful controls:

- Events per phrase.
- Allowed beats.
- Minimum event spacing.
- Single note or chord.
- Fixed or varied pitch.
- Short or sustained articulation.

At low density, accents should be genuinely occasional.

---

## 10. Impact engine

Build impacts from independently editable layers:

1. Transient.
2. Body.
3. Sub.
4. Texture.
5. Tail.

Each layer needs:

- Enable.
- Solo.
- Gain.
- Pan.
- Start offset.
- Envelope.
- Pitch where applicable.
- Filter.
- Regenerate.
- Lock.

Provide a combined waveform and the ability to inspect individual layers.

### Timing

Define the impact anchor using a meaningful event, such as the main transient onset.

Do not use a waveform’s loudest sample as the only anchor, because a later resonance peak may be louder than the attack.

### Low-end handling

Expose:

- Sub on/off.
- Sub pitch.
- Sub decay.
- Low-cut frequency for non-sub layers.
- Low-frequency mono control.

Protect Low End should apply visible parameter changes that users can inspect and reverse.

### Sound quality

Prevent:

- Clicks from discontinuous envelopes.
- Accidental DC offset.
- Uncontrolled feedback.
- Excessive sub tails.
- Clipping during summation.

Distinguish intentional distortion inside the sound design from accidental clipping at the output.

---

## 11. Selective regeneration and locks

Implement locks as enforceable constraints.

| Lock | Required preservation |
|---|---|
| Pitch | Existing note pitches |
| Timing | Note onsets |
| Duration | Note lengths |
| Velocity | Note velocities |
| Sound | Instrument and patch parameters |
| Effects | Effect chain and parameters |
| Placement | Event locations |
| Layer | Entire selected impact layer |

Expose convenient grouped locks such as **Rhythm**, which combines timing and duration.

When regenerating a selected region:

- Modify only eligible events inside that region.
- Preserve unrelated notes exactly.
- Handle notes crossing the boundary explicitly.
- Keep the earlier version recoverable.

For boundary-crossing notes, offer a clear default:

> Preserve notes that begin outside the selection.

If a request conflicts with a lock, show the conflict before generation:

> “Making this phrase shorter requires changing locked durations.”

Allow the user to unlock the relevant property or cancel.

**Never ignore a lock to make a result more interesting.**

---

## 12. Prompt interpretation

Treat text input as a way to configure a structured generation request.

Example:

> “Make a bright, playful eight-bar lead with lots of space and a different ending.”

Translate this into:

```text
Role: Lead
Length: 8 bars
Density: Sparse
Sound character: Bright
Motif repetition: High
Ending variation: Moderate
```

Show a compact interpretation before or alongside generation.

If the prompt is ambiguous, use reasonable defaults and make them visible. Do not interrupt every generation with a question.

If a requested capability is unavailable, say so specifically.

For the first release, structured controls must work independently of the text system. A language-model failure must not prevent manual generation.

---

## 13. Candidate comparison

Generate three candidates from the same brief, with controlled differences.

For example:

- Candidate A preserves the strongest motif repetition.
- Candidate B leaves more space.
- Candidate C explores a different contour.

Record the settings responsible for each difference.

Allow:

- Instant selection.
- Favorite.
- Rename.
- Duplicate.
- Regenerate this candidate.
- Create variations from this candidate.

Use safe transitions when switching during playback. Default to switching at the next bar boundary, with an optional immediate mode.

Release active voices properly. Prevent stuck notes and clicks.

Do not automatically replace the chosen candidate when a background generation finishes.

---

## 14. Playback and audition behavior

Offer three preview modes:

- Generated part only.
- Generated part with imported context.
- Host playback.

Make the active mode obvious.

When the DAW is already playing the source beat, keep imported context playback muted by default to avoid doubling.

Include:

- Play/stop.
- Loop.
- Return to selection start.
- Output meter.
- Output gain.
- All Notes Off.

Separate MIDI-context playback from the generated instrument so each can have its own sound and level.

Use approximate loudness compensation for candidate auditioning. Keep that separate from exported gain settings and explain it in a tooltip.

---

## 15. Piano-roll editing

Provide a functional editor with:

- Add, move, resize, and delete notes.
- Marquee selection.
- Copy, paste, duplicate.
- Quantize.
- Transpose.
- Velocity editing.
- Snap settings.
- Zoom and scroll.
- Selected-region regeneration.

Include straight and triplet grids.

Display context notes as subdued, non-editable ghost notes unless the user explicitly enters context-edit mode.

Make keyboard shortcuts discoverable. Disable global shortcuts while the user types into the prompt or renames a candidate.

Dragging an export handle should never accidentally move notes.

---

## 16. Internal sound engine

Start with a limited set of polished patches.

Include approximately 20–30 useful starting sounds covering:

- Bright plucks.
- Glassy bells.
- Mono leads.
- Soft keys.
- Wide synth leads.
- Short chord stabs.

Give patches meaningful names and searchable tags.

Expose consistent macros:

- Brightness.
- Attack.
- Decay.
- Sustain.
- Release.
- Motion.
- Width.
- Grit.

Hide unsupported macros rather than leaving controls that do nothing.

Support:

- Polyphony limits.
- Mono mode.
- Legato.
- Glide.
- Voice stealing.
- Velocity response.
- Pitch bend.

Smooth parameter changes to avoid zipper noise.

For exported MIDI, clarify that internal glide and effects may not translate to another instrument. Preserve compatible expression data where possible.

---

## 17. Audio engine and performance

Keep allocation, file access, networking, model inference, and heavy analysis off the audio callback.

Use worker threads for generation and rendering. Transfer completed results safely to the playback engine.

Requirements:

- Cancel generation.
- Preserve existing playback while generating.
- Show real progress stages.
- Recover cleanly from failed requests.
- Avoid unbounded memory growth.
- Close the plugin safely during an active job.
- Support multiple instances.

Suggested progress labels:

> Reading context → Building phrase → Creating sound → Preparing previews

Do not display invented percentage precision when the remaining duration is unknown.

Measure CPU, memory, and generation time on a documented reference machine. Set performance budgets after an early working prototype rather than guessing hardware-independent limits.

---

## 18. Export requirements

### MIDI

Export:

- Note positions.
- Durations.
- Velocities.
- Channels.
- Supported pitch-bend and controller data.
- Appropriate tempo and meter metadata.

Offer two alignment choices:

- **Selection starts at bar 1.**
- **Preserve project position.**

Default to selection-relative export for easy dragging into a DAW.

### WAV

Offer:

- 24-bit PCM.
- 32-bit float.
- Host sample rate by default.
- Dry or processed.
- Include tail.
- Optional normalization.

Keep normalization off by default.

Measure peaks before fixed-point export and warn or apply an explicitly selected gain adjustment if necessary.

Provide separate options for:

- Rendered phrase.
- Individual impact layers.
- Combined impact.
- Loop-ready render.

A loop-ready render must handle effect tails deliberately. Offer tail wrapping or a separate tail rather than silently cutting it off.

### Temporary export files

Use unique filenames and a managed temporary directory.

Do not remove a file while the DAW may still be importing it.

Example naming:

```text
SPARK_Lead_Am_150BPM_8bars_A.mid
SPARK_Impact_D_Processed.wav
```

---

## 19. Session recall and history

Save:

- Imported musical context.
- Chord edits.
- Generated note data.
- Selected candidate.
- Instrument state.
- Effect settings.
- Locks.
- Region selections.
- Seeds.
- Engine version.
- Necessary audio assets.

Reopening a project must reproduce the chosen result even if the generation model has changed.

Use a versioned state format and migration handling.

If an asset is missing, identify it clearly and offer relinking. Do not substitute a different sound silently.

Keep undo history bounded, but preserve explicitly saved versions.

---

## 20. Errors, privacy, and recovery

Every failure should preserve the user’s existing work.

Examples:

| Situation | Required behavior |
|---|---|
| Unsupported file | Explain accepted formats |
| MIDI contains no notes | Keep current context and explain |
| Generation fails | Preserve candidates and allow retry |
| Network unavailable | Keep playback, editing, and export available |
| Conflicting constraints | Identify the specific conflict |
| Export fails | Show the destination problem and retry option |
| Missing asset | Offer relink or an explicit replacement |

If cloud generation is used:

- Explain what is uploaded.
- Obtain opt-in before uploading project audio.
- State retention behavior accurately.
- Avoid sending audio when structured MIDI is sufficient.
- Keep credentials out of project files and logs.

Do not require an account for features that can operate locally.

---

## 21. Testing requirements

Test the musical engine separately from the plugin interface.

### Automated checks

Verify:

- Notes stay inside explicit pitch limits.
- Locked values remain unchanged.
- Generation respects selected boundaries.
- Exports preserve musical timing.
- State round-trips restore the same event data.
- Identical saved results play consistently.
- Cancellation leaves valid state.
- Invalid inputs do not crash the plugin.
- Rendered audio contains no invalid sample values.
- Voice cleanup prevents stuck notes.

### DAW integration checks

Test in FL Studio as the primary host.

Include:

- Loading and removing instances.
- Saving, closing, and reopening projects.
- Host tempo changes.
- Looping.
- Transport jumps.
- Different buffer sizes.
- Offline rendering.
- Multiple plugin instances.
- MIDI and audio drag-and-drop.

Test at 44.1, 48, and 96 kHz, and include common small and large audio buffers.

Document the exact host versions tested.

### Musical evaluation

Build a small evaluation set containing:

- Simple major and minor progressions.
- Borrowed chords.
- Sparse and busy source melodies.
- Slow and fast tempos.
- Short and long phrases.
- Progressions with unusual chord durations.

Have producers evaluate:

- Harmonic fit.
- Memorability.
- Appropriate density.
- Useful variation.
- Space around existing parts.
- Whether the result is worth keeping.

Passing technical checks does not establish musical quality.

---

## 22. Required end-to-end acceptance scenarios

The first release is ready only when these workflows succeed.

### Scenario A: Sparse lead

Import eight bars of chords, select Lead, choose Sparse, and generate.

Expected:

- A recognizable motif.
- Intentional rests.
- Notes inside the selected range.
- Sensible treatment of each chord.
- Three meaningfully different candidates.

### Scenario B: Preserve the opening

Select only bar eight and regenerate its ending.

Expected:

- Bars one through seven remain exactly unchanged.
- The new ending connects to the preceding phrase.
- Undo restores the original.

### Scenario C: Change the sound

Lock pitches, timing, duration, and velocity; choose another sound family.

Expected:

- Identical musical events.
- A different patch.
- No hidden timing changes.

### Scenario D: Add a response

Import a melody with clear gaps and generate a Response.

Expected:

- Events occupy eligible gaps.
- Overlap follows the selected limit.
- The response relates to the source phrase.

### Scenario E: Create an impact

Place an impact on the first beat of bar nine.

Expected:

- The main transient lands on that beat.
- Sub and tail are independently adjustable.
- Layer exports align when recombined.

### Scenario F: Reopen the project

Save the DAW project, close it, and reopen.

Expected:

- Same candidate.
- Same notes.
- Same sound.
- Same effects.
- Same locks.
- No regeneration required.

---

## 23. Delivery requirements

Deliver:

- Buildable source code.
- Reproducible build instructions.
- A working VST3 package.
- A standalone development/test application if practical.
- Bundled demo context and presets.
- Automated test results.
- DAW compatibility results.
- Dependency and asset license information.
- A short user guide.
- A clearly stated list of remaining limitations.

Build and validate one complete path early:

**Import chords → generate lead → audition → edit → export → save → reopen.**

Then add the remaining first-release roles using the same reliable foundation. The quality standard is that every visible control behaves clearly, every generated result remains editable, and the producer can safely experiment without losing a good idea.
