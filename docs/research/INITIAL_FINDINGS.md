# Initial MIDI research findings — incomplete tutorial study

Requested priority: prettifun, waera, jackwya, slayr, with Rio Leyva and other producers as supporting references. **No producer-specific tutorials have been reviewed yet.** The environment currently denies YouTube and Google access. This report covers verified source-code observations, a reproducible generator audit, and accessible theory readings. It is not the requested finished hour-long tutorial study, and it makes no claims about the named producers' techniques.

## What is wrong with the current generator

The generator at `src/Engine.cpp` creates a target onset count, distributes those onsets approximately evenly, rounds them to a fixed grid, then selects pitches by cycling an index through the current chord's available triad tones. Seeded randomness influences some pitch choices, velocity, and one of two durations. There is no explicit motif object, phrase structure, rhythmic development, local tension/resolution plan, or ending model. The candidate labels overstate the underlying musical structure: an index cycle is not a deliberately composed, recurring motif.

The audible complaint is consistent with those design limits. Technical tests establish that output is valid, in range, and recallable; they do not establish that a melody is good.

### Measured baseline

Audited the actual C++ engine at commit `239383e` using Am–F–C–G repeated over eight bars, MIDI 72–84, default complexity 1, all five densities, all three candidates, and seeds 1–64. That is 960 generated candidates. Raw results are in `generator-audit.csv`. These findings apply to this test setting, not every possible musical context.

- Every density/candidate combination produced **one unique onset pattern across all 64 seeds**. Timing does not use the seed at all.
- Across all tested output, note durations took only two values at this complexity: 480 and 960 ticks (eighth and quarter notes).
- Sparse candidate A had 28 onsets in eight bars. Across its 64 seeds, 27.72% of adjacent pitch movements were repetitions or steps of at most two semitones, while 26.79% exceeded seven semitones.
- Sparse C had just 3.70% repetitions/steps and 25.93% movements exceeding seven semitones. Its cycling pitch rule is not a coherent alternate phrase contour.
- At default complexity, Very busy A/C reached only 64 onsets across eight bars because the eighth-note grid collapses duplicate onsets. That is 32 per four bars, below the brief's initial Very busy Lead target of 40–64.
- Only triad pitch classes are available. There is no way to distinguish a structural chord tone from a passing tone, neighbor, anticipation, or other intentional tension.
- No rule considers the preceding pitch when making the next choice. No rule distinguishes an opening, a phrase continuation, a turnaround, or a resolved ending.

A leap-heavy line is not inherently wrong; deliberate repetition, arpeggiation, or octave motion may be desirable. The problem is that the engine cannot choose these behaviors intentionally or preserve their musical identity. The percentages above are diagnostic, not automatic quality scores.

## Useful lessons from sources actually read

See `SOURCES.md` for exact source URLs and evidence levels. These are general composition references, not evidence about current producer styles.

### Identity can survive a change of notes

Open Music Theory's sentence discussion describes a basic idea through rhythm and contour, allowing varied repetition. Its discussion of pop/rock modules also distinguishes similar beginnings with different endings, and statement–restatement/response–departure–conclusion organization. The useful engineering abstraction is a motif whose identity survives chord-aware transposition and small edits.

Do not force Classical sentences, authentic cadences, or eight-bar templates onto every requested producer style. The textbook itself treats these structures in specific repertories. For SPARK, a short repeated cell with controlled changes may be a more appropriate default than a full Classical phrase. Tutorial listening is needed to choose style-specific defaults.

### Rhythm must be designed before it is humanized

The syncopation reading distinguishes offbeat shifts at different metrical levels and discusses 3+3+2 groupings separately from true triplets. This matters because adding timing jitter to evenly spaced notes does not create an intentional rhythm. A generator should first specify accents, rests, held notes and recurring subdivisions; optional microtiming comes afterward.

3+3+2 and triplets are possible devices, not universal prescriptions. Nothing reviewed so far supports attributing either device to one of the requested producers.

### Tension needs a purpose

The embellishing-tones reading differentiates passing notes, neighbors, anticipations and suspensions by their relationship to stable notes and harmonic changes. A future engine should encode those relationships. Merely allowing random scale tones would expand the pitch pool without adding melodic intention.

At the same time, Classical preparation/resolution rules should not become a hard test that rejects every desirable pop or experimental melody. Harmonic adherence must be controllable and judged in context.

### Global key is not completely determined by chord labels

The source's discussion of Am–F–C–G explicitly describes its potential A-minor/C-major ambiguity. SPARK's demo uses those exact chords. A melody's anchors, register, emphasis and ending can affect perceived tonal focus. A future key control should express a compositional preference while respecting borrowed/local chords; the program should not pretend one chord-name sequence uniquely identifies the intended sound.

## Proposed engine changes to investigate after tutorials

1. **Phrase plan:** Select a short-cell structure, planned rests, an opening anchor and a return/turnaround. Keep eight-bar plans optional. Store the plan with the candidate.
2. **Rhythmic motif:** Represent exact onsets, durations, accents and rests relative to the motif. Repeat or transform that object rather than spacing a requested count evenly across the phrase. Seeded alternatives should change a rhythmic plan meaningfully.
3. **Pitch path:** Select chord-aware structural targets, then connect them with repetition, steps, controlled leaps and optional passing/neighbor figures. Consider the previous pitch and the next target. Preserve imported voicings separately from harmonic labels.
4. **Controlled variants:** A establishes a motif, B changes the space around recognizable material, C changes a documented phrase attribute or ending. Their differences should be explainable and measurable, not unrelated random seeds.
5. **Density by musical role:** Adjust the number of gestures and their internal activity, with protected rest regions. Keep density independent of complexity and variation. Do not let grid deduplication silently turn a requested density into a different setting.
6. **Intentional articulation:** Velocity, duration and gaps should support phrase accents, response and breath. Random velocity alone is not groove.
7. **Context-aware ending regeneration:** Derive local rhythm/pitch targets from the notes before the selection, while enforcing locks and preserving unrelated notes. An independently generated fragment does not automatically connect to the existing phrase.

These are research hypotheses and design proposals, **not implemented improvements or promises of a particular producer's sound**.

## Listening study and acceptance

The requested tutorial work still needs direct access and representative creator links. Record whether a source is a first-hand producer demonstration, an authorized breakdown, or a third-party type-beat interpretation. Track inspected timestamps and what is actually visible/audible. Separate MIDI composition from timbre, effects and arrangement.

For comparisons, use the same chord context, tempo, neutral sound and output level. Compare current output against proposed methods over multiple seeds. Also audition in a representative patch, because articulation and timbre affect perceived musical usefulness. Ask whether the result has a recognizable idea, useful space, a convincing return/ending, and is worth keeping. Do not optimize a numerical metric in place of producer judgment.

No application code was changed during this research pass. No claim is made that the requested tutorial study or one-to-two-hour budget has been completed.
