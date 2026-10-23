# Validation record

Execution environment: Debian 13 x86_64, GCC 14.2.0, CMake 4.4.4, JUCE 8.0.6. Five logical CPUs available, ~33 GiB RAM. Native builds use three jobs. No Windows host or physical audio device is available here.

## Automated checks

- Engine suite: 480 assertions passed. Deterministic generation, chord tones, range/region constraints, sparse candidate differentiation, preservation of seven-bar opening during last-bar regeneration, all musical locks together and individually, canonical state round-trip, rejection of unsupported/corrupt state and unsupported chord syntax.
- Plugin integration runner: exercises generated audio at 44.1/48/96 kHz with 64/512/2048-sample blocks; verifies finite unclipped and non-silent output, all-notes-off silence, native processor state round-trip, and MIDI export re-import with matching onsets and note ends. Passed: 1,800 audio blocks across all nine rate/buffer combinations; MIDI timing and state recall passed.

These tests do not establish musical quality, DAW compatibility, real audio-device output, or release acceptance. Windows CI is provided but has not been executed in this workspace.

## DAW matrix — unrun

FL Studio version: none tested. Windows loading/removal, DAW save/reopen, host transport/tempo changes and jumps, looping, offline rendering, multiple instances, and drag-and-drop must be validated on Windows. Host synchronization and drag-and-drop are not implemented in this prototype.

## Acceptance status

A: Lead core covered by technical tests; import/UI workflow and producer assessment need expanded coverage.
B: Last-bar preservation and lock/undo foundations implemented; engine boundary tests passed.
C: Musical event locks and patch selection implemented; no instrument macros/effects yet.
D/E: Response and Impact unimplemented.
F: Processor state round-trip tested by integration runner; DAW project reopen unrun.

## Final native results

Both CTest targets passed (engine and plugin integration). Headless GUI actions passed: chord entry, generation, undo/redo. Actual 24-bit stereo 48 kHz WAV rendering passed read-back checks for format, duration and non-silent samples. Native UI snapshots were inspected at 1180×760 and 960×640; text encoding and minimum-size button overlap were corrected. The Linux VST3 linked and JUCE generated module metadata by loading it. The standalone remained running during a five-second startup smoke check, terminated deliberately by timeout; ALSA reported no MIDI sequencer device, so real device audition was not established. Fontconfig reported no writable cache; native text rendering worked.

AddressSanitizer/UndefinedBehaviorSanitizer passed the 480-assertion engine suite before the final region-duration guard tightening; the final source was rebuilt and both ordinary CTest suites passed afterward. No sanitizer coverage is claimed for JUCE/native GUI code.

Build output and full local logs are under `build/`. `scripts/build-cloud.sh` was executed successfully in the retained workspace-local SDK environment. Source build instructions for a normal Windows/Linux machine are in README.md. Windows CI and all DAW tests remain unrun.
