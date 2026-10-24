#include "../src/Plugin.cpp"
#include <iostream>
#include <stdexcept>
int main(int argc, char **argv) {
    try {
        Spark p;
        p.harmony = spark::parseChords("Am F C G");
        p.candidates = spark::generate(p.harmony, spark::Spec{});
        MemoryBlock state;
        p.getStateInformation(state);
        Spark copy;
        copy.setStateInformation(state.getData(), (int)state.getSize());
        if (copy.snapshot() != p.snapshot())
            throw std::runtime_error("Plugin state mismatch");
        int blocks = 0;
        for (double sr : {44100., 48000., 96000.})
            for (int size : {64, 512, 2048}) {
                p.setRateAndBufferSizeDetails(sr, size);
                p.prepareToPlay(sr, size);
                p.playing = true;
                bool audible = false;
                for (int j = 0; j < 200; ++j) {
                    AudioBuffer<float> b(2, size);
                    MidiBuffer midi;
                    p.processBlock(b, midi);
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < size; ++i) {
                            float v = b.getSample(c, i);
                            if (!std::isfinite(v) || std::abs(v) > 1)
                                throw std::runtime_error("Invalid or clipping audio");
                            audible |= std::abs(v) > 0.00001f;
                        }
                    ++blocks;
                }
                if (!audible)
                    throw std::runtime_error("Silent preview");
                p.playing = false;
                p.panic = true;
                AudioBuffer<float> b(2, size);
                MidiBuffer m;
                p.processBlock(b, m);
                if (b.getMagnitude(0, size) != 0)
                    throw std::runtime_error("Stuck voice after panic");
                p.releaseResources();
            }
        auto data = spark::midi(p.candidates[0].notes, 150);
        MemoryInputStream input(data.data(), data.size(), false);
        MidiFile file;
        if (!file.readFrom(input) || file.getTimeFormat() != 960)
            throw std::runtime_error("MIDI read-back failed");
        int ons = 0;
        auto *track = file.getTrack(0);
        for (int i = 0; i < track->getNumEvents(); ++i) {
            auto *e = track->getEventPointer(i);
            if (e->message.isNoteOn()) {
                if (!e->noteOffObject)
                    throw std::runtime_error("Unpaired MIDI note");
                auto &original = p.candidates[0].notes[(size_t)ons++];
                if (e->message.getTimeStamp() != original.start ||
                    e->noteOffObject->message.getTimeStamp() != original.start + original.duration)
                    throw std::runtime_error("MIDI timing mismatch");
            }
        }
        if (ons != (int)p.candidates[0].notes.size())
            throw std::runtime_error("Missing MIDI notes");
        if (argc == 2) {
            ScopedJuceInitialiser_GUI gui;
            std::unique_ptr<AudioProcessorEditor> editor(p.createEditor());
            for (int i = 0; i < editor->getNumChildComponents(); ++i)
                if (auto *button = dynamic_cast<TextButton *>(editor->getChildComponent(i)))
                    if (button->getButtonText() == "Enter chords")
                        button->onClick();
            for (int i = 0; i < editor->getNumChildComponents(); ++i)
                if (auto *button = dynamic_cast<TextButton *>(editor->getChildComponent(i)))
                    if (button->getButtonText() == "Generate 3 ideas")
                        button->onClick();
            if (p.harmony.size() != 4 || p.candidates[0].notes.empty())
                throw std::runtime_error("Editor entry/generation actions failed");
            for (int i = 0; i < editor->getNumChildComponents(); ++i)
                if (auto *button = dynamic_cast<TextButton *>(editor->getChildComponent(i)))
                    if (button->getButtonText() == "Try demo")
                        button->onClick();
            if (p.harmony.size() != 8)
                throw std::runtime_error("Demo did not load eight bars of harmony");
            for (const auto &candidate : p.candidates)
                if (candidate.notes.empty() || candidate.spec.end != 8 * 3840)
                    throw std::runtime_error(
                        "Demo did not immediately generate three eight-bar candidates");
            auto before = p.candidates[0].notes;
            p.undo();
            p.undo(true);
            if (p.candidates[0].notes != before)
                throw std::runtime_error("Undo/redo failed");
            File wave = File::getSpecialLocation(File::tempDirectory)
                            .getNonexistentChildFile("spark-validation", ".wav");
            dynamic_cast<SparkEditor *>(editor.get())->render(wave);
            WavAudioFormat format;
            std::unique_ptr<AudioFormatReader> reader(
                format.createReaderFor(wave.createInputStream().release(), true));
            if (!reader || reader->bitsPerSample != 24 || reader->sampleRate != 48000 ||
                reader->lengthInSamples <= 48000)
                throw std::runtime_error("WAV format read-back failed");
            AudioBuffer<float> rendered(2, (int)reader->lengthInSamples);
            if (!reader->read(&rendered, 0, rendered.getNumSamples(), 0, true, true) ||
                rendered.getMagnitude(0, rendered.getNumSamples()) <= 0)
                throw std::runtime_error("WAV is silent or unreadable");
            reader.reset();
            wave.deleteFile();
            std::cout << "PASS: editor chord/generation actions, undo/redo, 24-bit WAV "
                         "render/read-back, UI snapshots\n";
            editor->setSize(1180, 760);
            auto image = editor->createComponentSnapshot(editor->getLocalBounds());
            File out(argv[1]);
            auto stream = out.createOutputStream();
            if (stream) {
                stream->setPosition(0);
                stream->truncate();
            }
            PNGImageFormat png;
            if (!stream || !png.writeImageToStream(image, *stream))
                throw std::runtime_error("Cannot save UI snapshot");
            editor->setSize(960, 640);
            auto small = editor->createComponentSnapshot(editor->getLocalBounds());
            auto ss = out.getSiblingFile("spark-minimum.png").createOutputStream();
            if (ss) {
                ss->setPosition(0);
                ss->truncate();
            }
            if (!ss || !png.writeImageToStream(small, *ss))
                throw std::runtime_error("Cannot save minimum UI snapshot");
        }
        std::cout << "PASS: " << blocks
                  << " audio blocks, 3 rates, 3 buffers, panic cleanup, state recall, MIDI timing "
                     "read-back\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
