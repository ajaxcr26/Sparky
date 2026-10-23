# Dependencies and assets

- JUCE 8.0.6, commit `51a8a6d7aeae7326956d747737ccf1575e61e209`. AGPLv3/commercial dual licensing; consult the upstream LICENSE.md and module notices. Includes Steinberg VST3 SDK under its upstream license. JUCE source is fetched by CMake, not vendored.
- C++17 standard library, system audio/graphics/font libraries on Linux, native platform SDKs on Windows/macOS. CMake is a build-time dependency.
- No sampled instruments, fonts, artwork, or pretrained models are added. Demo harmony is Am–F–C–G, generated locally. Four synth timbres are oscillator formulas in source.
- No external API or credentials are needed. Build-time network access is only used for the pinned JUCE source.
