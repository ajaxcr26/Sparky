#include "Engine.h"
#include <atomic>
#include <cmath>
#include <juce_audio_utils/juce_audio_utils.h>
using namespace juce;
class SparkEditor;
struct Sound : SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
struct Voice : SynthesiserVoice {
    double phase = 0, step = 0;
    float level = 0;
    ADSR env;
    std::atomic<int> *patch;
    explicit Voice(std::atomic<int> *p) : patch(p) {}
    bool canPlaySound(SynthesiserSound *s) override { return dynamic_cast<Sound *>(s) != nullptr; }
    void startNote(int n, float v, SynthesiserSound *, int) override {
        phase = 0;
        step = MidiMessage::getMidiNoteInHertz(n) / getSampleRate();
        level = v * .14f;
        env.setSampleRate(getSampleRate());
        ADSR::Parameters p;
        p.attack = .006f;
        p.decay = .22f;
        p.sustain = patch->load() == 2 ? .65f : .18f;
        p.release = .25f;
        env.setParameters(p);
        env.noteOn();
    }
    void stopNote(float, bool tail) override {
        if (tail)
            env.noteOff();
        else {
            env.reset();
            clearCurrentNote();
        }
    }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(AudioBuffer<float> &b, int start, int count) override {
        int p = patch->load();
        for (int i = 0; i < count; ++i) {
            float x = (float)std::sin(MathConstants<double>::twoPi * phase);
            if (p == 1)
                x = .7f * x + .3f * (float)std::sin(MathConstants<double>::twoPi * phase * 2.01);
            if (p == 2)
                x = .8f * x + .2f * (float)std::sin(MathConstants<double>::twoPi * phase * 2);
            if (p == 3)
                x = std::tanh(2 * x) * .8f;
            x *= env.getNextSample() * level;
            phase += step;
            phase -= std::floor(phase);
            for (int c = 0; c < b.getNumChannels(); ++c)
                b.addSample(c, start + i, x);
            if (!env.isActive()) {
                clearCurrentNote();
                break;
            }
        }
    }
};
class Spark : public AudioProcessor {
  public:
    std::array<spark::Candidate, 3> candidates;
    std::vector<spark::Chord> harmony;
    int selected = 0;
    double tempo = 150;
    std::atomic<int> patch{0};
    std::atomic<bool> playing{false}, panic{false};
    std::atomic<float> meter{0};
    std::atomic<double> position{0};
    CriticalSection stateLock;
    Synthesiser synth;
    double tick = 0;
    bool wasPlaying = false;
    MidiBuffer events;
    std::vector<std::string> undoStack, redoStack;
    String status = "Add your chords. Create the missing piece.";
    Spark()
        : AudioProcessor(BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true)) {
        for (int i = 0; i < 16; ++i)
            synth.addVoice(new Voice(&patch));
        synth.addSound(new Sound());
    }
    const String getName() const override { return "SPARK"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return .5; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const String &) override {}
    void prepareToPlay(double sr, int) override {
        synth.setCurrentPlaybackSampleRate(sr);
        events.ensureSize(65536);
        tick = 0;
    }
    void releaseResources() override { synth.allNotesOff(0, false); }
    bool isBusesLayoutSupported(const BusesLayout &l) const override {
        return l.getMainOutputChannelSet() == AudioChannelSet::stereo();
    }
    void processBlock(AudioBuffer<float> &b, MidiBuffer &midi) override {
        ScopedNoDenormals dn;
        b.clear();
        events.clear();
        events.addEvents(midi, 0, b.getNumSamples(), 0);
        midi.clear();
        const ScopedTryLock lock(stateLock);
        if (!lock.isLocked()) {
            synth.allNotesOff(0, false);
            for (int channel = 1; channel <= 16; ++channel)
                midi.addEvent(MidiMessage::allNotesOff(channel), 0);
            return;
        }
        bool run = playing.load();
        if (panic.exchange(false) || run != wasPlaying) {
            synth.allNotesOff(0, false);
            for (int channel = 1; channel <= 16; ++channel)
                events.addEvent(MidiMessage::allNotesOff(channel), 0);
            tick = 0;
        }
        wasPlaying = run;
        if (run && !candidates[selected].notes.empty()) {
            double delta = spark::ppq * tempo / (60 * std::max(1.0, getSampleRate()));
            int end = candidates[selected].spec.end;
            int offset = 0;
            while (offset < b.getNumSamples()) {
                if (tick >= end) {
                    tick -= end;
                    for (int channel = 1; channel <= 16; ++channel)
                        events.addEvent(MidiMessage::allNotesOff(channel), offset);
                    // A fractional tick carried across the loop still triggers notes anchored at
                    // zero.
                    if (tick > 0)
                        for (const auto &note : candidates[selected].notes)
                            if (note.start == 0)
                                events.addEvent(MidiMessage::noteOn(note.channel, note.pitch,
                                                                    (uint8)note.velocity),
                                                offset);
                }
                int length = std::min(b.getNumSamples() - offset,
                                      std::max(1, (int)std::ceil((end - tick) / delta)));
                double until = tick + length * delta;
                for (const auto &note : candidates[selected].notes) {
                    auto schedule = [&](int when, const MidiMessage &message) {
                        if (when >= tick && when < until) {
                            int sample = offset + std::clamp((int)std::floor((when - tick) / delta),
                                                             0, length - 1);
                            events.addEvent(message, sample);
                        }
                    };
                    schedule(note.start,
                             MidiMessage::noteOn(note.channel, note.pitch, (uint8)note.velocity));
                    schedule(note.start + note.duration,
                             MidiMessage::noteOff(note.channel, note.pitch));
                }
                tick = until;
                offset += length;
            }
            position = tick;
        }
        synth.renderNextBlock(b, events, 0, b.getNumSamples());
        midi.addEvents(events, 0, b.getNumSamples(), 0);
        meter = b.getMagnitude(0, b.getNumSamples());
    }
    std::string snapshot() {
        return spark::save(candidates, harmony, selected, tempo, patch.load());
    }
    void apply(const std::string &s) {
        int p = 0;
        spark::restore(s, candidates, harmony, selected, tempo, p);
        patch = p;
        panic = true;
    }
    void remember() {
        undoStack.push_back(snapshot());
        if (undoStack.size() > 64)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
    }
    void undo(bool redo = false) {
        ScopedLock l(stateLock);
        auto &from = redo ? redoStack : undoStack;
        auto &to = redo ? undoStack : redoStack;
        if (from.empty())
            return;
        to.push_back(snapshot());
        auto state = from.back();
        from.pop_back();
        apply(state);
    }
    void getStateInformation(MemoryBlock &b) override {
        ScopedLock l(stateLock);
        auto s = snapshot();
        b.replaceAll(s.data(), s.size());
    }
    void setStateInformation(const void *d, int n) override {
        ScopedLock l(stateLock);
        try {
            apply(std::string((const char *)d, (size_t)n));
        } catch (const std::exception &e) {
            status = e.what();
        }
    }
    bool hasEditor() const override { return true; }
    AudioProcessorEditor *createEditor() override;
};
class Roll : public Component {
    Spark &p;
    int dragged = -1;
    bool resize = false;
    Point<float> origin;
    spark::Note original;

  public:
    explicit Roll(Spark &x) : p(x) { setMouseCursor(MouseCursor::CrosshairCursor); }
    Rectangle<float> rect(const spark::Note &n) const {
        int end = p.candidates[p.selected].spec.end;
        return {52.f + (getWidth() - 64.f) * n.start / end,
                (96 - n.pitch) * (getHeight() - 24.f) / 49.f + 24.f,
                std::max(4.f, (getWidth() - 64.f) * n.duration / end),
                (getHeight() - 24.f) / 49.f - 1};
    }
    void paint(Graphics &g) override {
        ScopedLock state(p.stateLock);
        g.fillAll(Colour(0xff151a21));
        int end = p.candidates[p.selected].spec.end;
        float row = (getHeight() - 24.f) / 49.f;
        for (int pitch = 48; pitch <= 96; ++pitch) {
            float y = (96 - pitch) * row + 24;
            bool black = (pitch % 12 == 1 || pitch % 12 == 3 || pitch % 12 == 6 ||
                          pitch % 12 == 8 || pitch % 12 == 10);
            g.setColour(Colour(black ? 0xff11161d : 0xff1b212a));
            g.fillRect(0.f, y, (float)getWidth(), row);
            if (pitch % 12 == 0) {
                g.setColour(Colour(0xff7f8a9b));
                g.setFont(10);
                g.drawText(MidiMessage::getMidiNoteName(pitch, true, true, 4), 2, (int)y, 46,
                           (int)row, Justification::centredLeft);
            }
        }
        for (int t = 0; t <= end; t += 960) {
            float x = 52 + (getWidth() - 64.f) * t / end;
            g.setColour(Colour(t % 3840 ? 0xff252d38 : 0xff3b4755));
            g.drawVerticalLine((int)x, 24, (float)getHeight());
            if (t % 3840 == 0) {
                g.setColour(Colour(0xff9ca8b8));
                g.drawText(String(t / 3840 + 1), (int)x + 5, 0, 20, 22, Justification::centredLeft);
            }
        }
        for (const auto &chord : p.harmony) {
            const char *roots[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
            int x = (int)(52 + (getWidth() - 64.f) * chord.start / end);
            int width = (int)((getWidth() - 64.f) * chord.duration / end);
            g.setColour(Colour(0xffb5f36c));
            g.setFont(12);
            g.drawText(String(roots[chord.root]) + (chord.minor ? "m" : ""), x + 25, 0,
                       std::max(1, width - 28), 22, Justification::centredLeft);
        }
        for (auto &ch : p.harmony)
            for (int pitch : ch.voicing) {
                spark::Note n;
                n.start = ch.start;
                n.duration = ch.duration;
                n.pitch = pitch;
                g.setColour(Colour(0xff394655));
                g.fillRoundedRectangle(rect(n), 2);
            }
        for (auto &n : p.candidates[p.selected].notes) {
            auto r = rect(n);
            g.setColour(Colour(0xffb5f36c).withAlpha(.45f + n.velocity / 240.f));
            g.fillRoundedRectangle(r, 3);
        }
        if (p.playing) {
            g.setColour(Colours::white.withAlpha(.8f));
            int x = (int)(52 + (getWidth() - 64.f) * p.position.load() / end);
            g.drawVerticalLine(x, 24, (float)getHeight());
        }
        if (p.candidates[p.selected].notes.empty()) {
            g.setColour(Colour(0xffa0acbc));
            g.setFont(20);
            g.drawText("Your next idea starts here", getLocalBounds(), Justification::centred);
        }
    }
    void mouseDown(const MouseEvent &e) override {
        ScopedLock l(p.stateLock);
        auto &ns = p.candidates[p.selected].notes;
        for (size_t i = 0; i < ns.size(); ++i)
            if (rect(ns[i]).contains(e.position)) {
                if (e.mods.isRightButtonDown() && (p.candidates[p.selected].spec.locks.pitch ||
                                                   p.candidates[p.selected].spec.locks.timing ||
                                                   p.candidates[p.selected].spec.locks.duration ||
                                                   p.candidates[p.selected].spec.locks.velocity)) {
                    p.status = "Unlock note properties before deleting notes.";
                    return;
                }
                p.remember();
                if (e.mods.isRightButtonDown()) {
                    ns.erase(ns.begin() + (int)i);
                    p.panic = true;
                    repaint();
                    return;
                }
                dragged = (int)i;
                original = ns[i];
                origin = e.position;
                resize = e.position.x > rect(ns[i]).getRight() - 8;
                return;
            }
    }
    void mouseDrag(const MouseEvent &e) override {
        ScopedLock l(p.stateLock);
        auto &ns = p.candidates[p.selected].notes;
        if (dragged < 0 || dragged >= (int)ns.size())
            return;
        auto &n = ns[(size_t)dragged];
        auto s = p.candidates[p.selected].spec;
        int dt =
            (int)std::round((e.position.x - origin.x) * s.end / (getWidth() - 64.f) / 240) * 240;
        auto &locks = s.locks;
        if (resize) {
            if (!locks.duration)
                n.duration =
                    std::clamp(original.duration + dt, 240, std::max(240, s.end - n.start));
        } else {
            if (!locks.timing)
                n.start = std::clamp(original.start + dt, 0, s.end - n.duration);
            if (!locks.pitch)
                n.pitch = std::clamp(original.pitch - (int)std::round((e.position.y - origin.y) *
                                                                      49 / (getHeight() - 24.f)),
                                     0, 127);
        }
        p.panic = true;
        repaint();
    }
    void mouseUp(const MouseEvent &) override { dragged = -1; }
    void mouseDoubleClick(const MouseEvent &e) override {
        ScopedLock l(p.stateLock);
        auto &c = p.candidates[p.selected];
        for (auto &note : c.notes)
            if (rect(note).contains(e.position))
                return;
        if (e.position.x < 52 || e.position.y < 24)
            return;
        if (c.spec.locks.pitch || c.spec.locks.timing || c.spec.locks.duration ||
            c.spec.locks.velocity) {
            p.status = "Unlock note properties before adding or deleting notes.";
            return;
        }
        p.remember();
        int t =
            std::clamp((int)((e.position.x - 52) * c.spec.end / (getWidth() - 64.f) / 240) * 240, 0,
                       c.spec.end - 240);
        int pitch = std::clamp(96 - (int)((e.position.y - 24) * 49 / (getHeight() - 24.f)), 0, 127);
        int id = 1;
        for (auto &n : c.notes)
            id = std::max(id, n.id + 1);
        c.notes.push_back({id, t, 240, pitch, 95, 1});
        p.panic = true;
        repaint();
    }
};
class SparkEditor : public AudioProcessorEditor, private Timer {
    Spark &p;
    Roll roll;
    LookAndFeel_V4 theme;
    Label brand, subtitle, status, regionLabel;
    TextEditor chords;
    ComboBox patch, density, candidate;
    Slider bpm, first, last;
    TextButton demo{"Try demo"}, enter{"Enter chords"}, import{"Import MIDI"},
        generate{"Generate 3 ideas"}, regen{"Regenerate region"}, play{"Play"},
        stop{"Stop / All notes off"}, undo{"Undo"}, redo{"Redo"}, midi{"Export MIDI"},
        wav{"Export WAV"}, save{"Save session"}, load{"Open session"};
    ToggleButton pitchLock{"Pitch"}, timeLock{"Timing"}, durationLock{"Duration"},
        velocityLock{"Velocity"}, soundLock{"Sound"};
    std::unique_ptr<FileChooser> chooser;
    void file(const String &title, const String &wildcard, bool writing,
              std::function<void(File)> action) {
        chooser = std::make_unique<FileChooser>(
            title, File::getSpecialLocation(File::userDocumentsDirectory), wildcard);
        chooser->launchAsync(
            writing ? FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles |
                          FileBrowserComponent::warnAboutOverwriting
                    : FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
            [this, action](const FileChooser &c) {
                auto f = c.getResult();
                if (f != File())
                    try {
                        action(f);
                    } catch (const std::exception &e) {
                        p.status = e.what();
                    }
            });
    }
    void update() {
        ScopedLock state(p.stateLock);
        density.setSelectedId(p.candidates[p.selected].spec.density + 1, dontSendNotification);
        patch.setSelectedId(p.patch + 1, dontSendNotification);
        bpm.setValue(p.tempo, dontSendNotification);
        candidate.setSelectedId(p.selected + 1, dontSendNotification);
        auto &l = p.candidates[p.selected].spec.locks;
        pitchLock.setToggleState(l.pitch, dontSendNotification);
        timeLock.setToggleState(l.timing, dontSendNotification);
        durationLock.setToggleState(l.duration, dontSendNotification);
        velocityLock.setToggleState(l.velocity, dontSendNotification);
        soundLock.setToggleState(l.sound, dontSendNotification);
        roll.repaint();
    }

  public:
    void render(File f) {
        ScopedLock lock(p.stateLock);
        auto notes = p.candidates[p.selected].notes;
        if (notes.empty())
            throw std::runtime_error("Generate a phrase before rendering.");
        double sr = 48000;
        if (p.candidates[p.selected].spec.end / 960.0 * 60 / p.tempo > 180)
            throw std::runtime_error("Prototype WAV rendering is limited to three minutes. Use a "
                                     "shorter phrase or faster tempo.");
        int frames = (int)((p.candidates[p.selected].spec.end / 960.0) * 60 / p.tempo * sr + sr);
        AudioBuffer<float> b(2, frames);
        b.clear();
        Synthesiser synth;
        for (int i = 0; i < 16; ++i)
            synth.addVoice(new Voice(&p.patch));
        synth.addSound(new Sound());
        synth.setCurrentPlaybackSampleRate(sr);
        MidiBuffer events;
        for (auto &n : notes) {
            events.addEvent(MidiMessage::noteOn(n.channel, n.pitch, (uint8)n.velocity),
                            (int)(n.start / 960.0 * 60 / p.tempo * sr));
            events.addEvent(MidiMessage::noteOff(n.channel, n.pitch),
                            (int)((n.start + n.duration) / 960.0 * 60 / p.tempo * sr));
        }
        synth.renderNextBlock(b, events, 0, frames);
        auto stream = f.createOutputStream();
        if (!stream)
            throw std::runtime_error("Cannot write to destination.");
        stream->setPosition(0);
        stream->truncate();
        WavAudioFormat format;
        std::unique_ptr<AudioFormatWriter> writer(
            format.createWriterFor(stream.get(), sr, 2, 24, {}, 0));
        if (!writer)
            throw std::runtime_error("Cannot create WAV writer.");
        stream.release();
        if (!writer->writeFromAudioSampleBuffer(b, 0, frames))
            throw std::runtime_error("WAV write failed.");
        p.status = "Exported 24-bit stereo WAV / 48 kHz / tail included / normalization off";
    }

  public:
    SparkEditor(Spark &x) : AudioProcessorEditor(x), p(x), roll(x) {
        theme.setColour(ResizableWindow::backgroundColourId, Colour(0xff10141a));
        theme.setColour(TextButton::buttonColourId, Colour(0xff26303c));
        theme.setColour(TextButton::textColourOffId, Colour(0xffe9eef5));
        theme.setColour(ComboBox::backgroundColourId, Colour(0xff202833));
        theme.setColour(Slider::thumbColourId, Colour(0xffb5f36c));
        theme.setColour(ToggleButton::tickColourId, Colour(0xffb5f36c));
        setLookAndFeel(&theme);
        setResizable(true, true);
        setResizeLimits(960, 640, 1800, 1200);
        setSize(1180, 760);
        for (Component *c : std::vector<Component *>{
                 &brand,     &subtitle,  &status,       &regionLabel,  &chords,    &patch,
                 &density,   &candidate, &bpm,          &first,        &last,      &demo,
                 &enter,     &import,    &generate,     &regen,        &play,      &stop,
                 &undo,      &redo,      &midi,         &wav,          &save,      &load,
                 &pitchLock, &timeLock,  &durationLock, &velocityLock, &soundLock, &roll})
            addAndMakeVisible(c);
        brand.setText("SPARK", dontSendNotification);
        brand.setFont(Font(FontOptions(30, Font::bold)));
        brand.setColour(Label::textColourId, Colour(0xffb5f36c));
        subtitle.setText("MUSICAL IDEAS, IN CONTEXT    /    Lead prototype / Offline",
                         dontSendNotification);
        subtitle.setColour(Label::textColourId, Colour(0xff99a6b6));
        status.setColour(Label::textColourId, Colour(0xffc0ccd9));
        chords.setText("Am F C G");
        chords.setTextToShowWhenEmpty("One chord per bar: Am F C G", Colours::grey);
        patch.addItemList({"Neon pluck", "Glass bell", "Soft keys", "Warm lead"}, 1);
        patch.setSelectedId(1);
        density.addItemList({"Very sparse", "Sparse", "Medium", "Busy", "Very busy"}, 1);
        density.setSelectedId(2);
        candidate.addItemList({"A / Repeating motif", "B / More space", "C / Alternate contour"},
                              1);
        candidate.setSelectedId(1);
        bpm.setRange(20, 300, 1);
        bpm.setValue(p.tempo);
        bpm.setTextValueSuffix(" BPM / Manual");
        bpm.setSliderStyle(Slider::IncDecButtons);
        bpm.setTextBoxStyle(Slider::TextBoxLeft, false, 180, 28);
        for (auto *s : {&first, &last}) {
            s->setRange(1, 64, 1);
            s->setSliderStyle(Slider::IncDecButtons);
            s->setTextBoxStyle(Slider::TextBoxLeft, false, 65, 28);
        }
        first.setValue(1);
        last.setValue(4);
        regionLabel.setText("Region / first / last bar", dontSendNotification);
        enter.onClick = [this] {
            try {
                auto h = spark::parseChords(chords.getText().toStdString());
                ScopedLock l(p.stateLock);
                p.remember();
                p.harmony = h;
                p.candidates = {};
                for (auto &c : p.candidates)
                    c.spec.end = h.back().start + h.back().duration;
                last.setValue((double)h.size());
                p.status = "Chords loaded. Choose density and generate a Lead.";
                p.playing = false;
            } catch (const std::exception &e) {
                p.status = e.what();
            }
            roll.repaint();
        };
        demo.onClick = [this] {
            chords.setText("Am F C G Am F C G");
            // Both actions are local and quick; run them in order without a delayed callback.
            enter.onClick();
            generate.onClick();
        };
        generate.setColour(TextButton::buttonColourId, Colour(0xffb5f36c));
        generate.setColour(TextButton::textColourOffId, Colour(0xff14200b));
        generate.onClick = [this] {
            try {
                ScopedLock l(p.stateLock);
                spark::Spec s;
                s.end =
                    p.harmony.empty() ? 15360 : p.harmony.back().start + p.harmony.back().duration;
                s.density = density.getSelectedId() - 1;
                s.seed = (uint32)Random::getSystemRandom().nextInt();
                auto cs = spark::generate(p.harmony, s);
                auto locks = p.candidates[p.selected].spec.locks;
                if (locks.pitch || locks.timing || locks.duration || locks.velocity)
                    throw std::runtime_error("Use region regeneration to preserve locked notes, or "
                                             "unlock before generating new candidates.");
                p.remember();
                p.candidates = cs;
                p.status = "Three chord-aware leads / Range C5-C6 (MIDI 72-84) / 1/16 edit grid";
                p.panic = true;
                roll.repaint();
            } catch (const std::exception &e) {
                p.status = e.what();
            }
        };
        regen.onClick = [this] {
            try {
                ScopedLock l(p.stateLock);
                auto s = p.candidates[p.selected].spec;
                s.begin = ((int)first.getValue() - 1) * 3840;
                s.end = (int)last.getValue() * 3840;
                if (s.end > p.candidates[p.selected].spec.end)
                    throw std::runtime_error("Region extends beyond the phrase.");
                s.seed = (uint32)Random::getSystemRandom().nextInt();
                auto c = spark::regenerate(p.candidates[p.selected], p.harmony, s);
                c.spec.end = p.candidates[p.selected].spec.end;
                c.spec.begin = 0;
                p.remember();
                p.candidates[p.selected] = c;
                p.panic = true;
                p.status = "Region regenerated. Notes beginning outside the selection preserved.";
                roll.repaint();
            } catch (const std::exception &e) {
                p.status = e.what();
            }
        };
        candidate.onChange = [this] {
            ScopedLock l(p.stateLock);
            p.selected = candidate.getSelectedId() - 1;
            p.panic = true;
            update();
        };
        patch.onChange = [this] {
            ScopedLock l(p.stateLock);
            if (p.candidates[p.selected].spec.locks.sound) {
                p.status = "Sound is locked. Unlock it to change patch.";
                update();
                return;
            }
            p.remember();
            p.patch = patch.getSelectedId() - 1;
            p.panic = true;
        };
        bpm.onValueChange = [this] {
            ScopedLock l(p.stateLock);
            p.remember();
            p.tempo = bpm.getValue();
            p.panic = true;
        };
        auto locks = [this] {
            ScopedLock l(p.stateLock);
            p.remember();
            p.candidates[p.selected].spec.locks = {
                pitchLock.getToggleState(), timeLock.getToggleState(),
                durationLock.getToggleState(), velocityLock.getToggleState(),
                soundLock.getToggleState()};
        };
        pitchLock.onClick = locks;
        timeLock.onClick = locks;
        durationLock.onClick = locks;
        velocityLock.onClick = locks;
        soundLock.onClick = locks;
        play.onClick = [this] {
            ScopedLock lock(p.stateLock);
            if (p.candidates[p.selected].notes.empty()) {
                p.status = "Generate a phrase before playing.";
                return;
            }
            p.playing = !p.playing.load();
        };
        stop.onClick = [this] {
            p.playing = false;
            p.panic = true;
        };
        undo.onClick = [this] {
            p.undo();
            update();
        };
        redo.onClick = [this] {
            p.undo(true);
            update();
        };
        midi.onClick = [this] {
            file("Export MIDI", "*.mid", true, [this](File f) {
                ScopedLock l(p.stateLock);
                if (p.candidates[p.selected].notes.empty())
                    throw std::runtime_error("Generate a phrase first.");
                auto bytes = spark::midi(p.candidates[p.selected].notes, p.tempo);
                if (!f.withFileExtension("mid").replaceWithData(bytes.data(), bytes.size()))
                    throw std::runtime_error("Cannot write MIDI destination.");
                p.status =
                    "MIDI exported / 960 PPQ / manual tempo / 4/4 / internal sound not included";
            });
        };
        wav.onClick = [this] {
            file("Export WAV (24-bit / 48 kHz)", "*.wav", true,
                 [this](File f) { render(f.withFileExtension("wav")); });
        };
        save.onClick = [this] {
            file("Save SPARK session", "*.spark", true, [this](File f) {
                ScopedLock l(p.stateLock);
                if (!f.withFileExtension("spark").replaceWithText(p.snapshot()))
                    throw std::runtime_error("Cannot save session.");
                p.status = "Session saved with actual notes, chords, patches and locks.";
            });
        };
        load.onClick = [this] {
            file("Open SPARK session", "*.spark", false, [this](File f) {
                auto str = f.loadFileAsString().toStdString();
                ScopedLock l(p.stateLock);
                auto before = p.snapshot();
                p.apply(str);
                p.undoStack.push_back(before);
                p.redoStack.clear();
                p.status = "Session restored. No regeneration required.";
                update();
            });
        };
        import.onClick = [this] {
            file("Import chord MIDI / all pitched tracks", "*.mid;*.midi", false, [this](File f) {
                auto stream = f.createInputStream();
                MidiFile mf;
                if (!stream || !mf.readFrom(*stream) || mf.getTimeFormat() <= 0)
                    throw std::runtime_error(
                        "Use a valid PPQ MIDI file; SMPTE files are unsupported.");
                std::vector<spark::Chord> h;
                std::vector<spark::Note> ns;
                for (int t = 0; t < mf.getNumTracks(); ++t) {
                    auto *seq = mf.getTrack(t);
                    for (int i = 0; i < seq->getNumEvents(); ++i) {
                        auto *e = seq->getEventPointer(i);
                        if (e->message.isNoteOn() && e->noteOffObject &&
                            e->message.getChannel() != 10) {
                            int start = (int)std::lround(e->message.getTimeStamp() * 960 /
                                                         mf.getTimeFormat());
                            int duration =
                                (int)std::lround((e->noteOffObject->message.getTimeStamp() -
                                                  e->message.getTimeStamp()) *
                                                 960 / mf.getTimeFormat());
                            ns.push_back({(int)ns.size() + 1, start, std::max(1, duration),
                                          e->message.getNoteNumber(), e->message.getVelocity(),
                                          e->message.getChannel()});
                        }
                    }
                }
                if (ns.empty())
                    throw std::runtime_error(
                        "MIDI contains no paired pitched notes; current context kept.");
                std::sort(ns.begin(), ns.end(), [](auto &a, auto &b) { return a.start < b.start; });
                for (auto &n : ns) {
                    if (h.empty() || n.start - h.back().start > 120)
                        h.push_back({n.start, n.duration, n.pitch % 12, false, {n.pitch}});
                    else {
                        h.back().voicing.push_back(n.pitch);
                        h.back().duration =
                            std::max(h.back().duration, n.start + n.duration - h.back().start);
                    }
                }
                if (h.size() > 64)
                    throw std::runtime_error(
                        "Too many chord groups (64 maximum). Import a simpler chord MIDI.");
                for (auto &c : h) {
                    bool found = false;
                    for (int pitch : c.voicing) {
                        int root = pitch % 12;
                        bool fifth = false, minor = false, major = false;
                        for (int q : c.voicing) {
                            int pc = (q - root + 12) % 12;
                            fifth |= pc == 7;
                            minor |= pc == 3;
                            major |= pc == 4;
                        }
                        if (fifth && (minor || major)) {
                            c.root = root;
                            c.minor = minor;
                            found = true;
                            break;
                        }
                    }
                    ignoreUnused(found);
                }
                int end = 0;
                for (auto &n : ns)
                    end = std::max(end, n.start + n.duration);
                end = ((end + 3839) / 3840) * 3840;
                if (end > 245760)
                    throw std::runtime_error("Maximum phrase length is 64 bars.");
                ScopedLock l(p.stateLock);
                p.remember();
                p.harmony = h;
                p.candidates = {};
                for (auto &c : p.candidates)
                    c.spec.end = std::max(3840, end);
                last.setValue(end / 3840);
                p.playing = false;
                p.status = "Imported all pitched tracks; voicings retained. Triad labels are "
                           "suggestions. Manual tempo / 4/4 retained.";
                roll.repaint();
            });
        };
        update();
        startTimerHz(30);
    }
    ~SparkEditor() override {
        stopTimer();
        setLookAndFeel(nullptr);
    }
    void timerCallback() override {
        ScopedLock state(p.stateLock);
        status.setText(p.status + "    |    Peak " +
                           String(Decibels::gainToDecibels(p.meter.load(), -100.f), 1) + " dBFS",
                       dontSendNotification);
        play.setButtonText(p.playing ? "Pause" : "Play / loop");
        roll.repaint();
    }
    void paint(Graphics &g) override {
        g.fillAll(Colour(0xff10141a));
        g.setColour(Colour(0xff1b222c));
        g.fillRoundedRectangle(16, 84, 268, (float)getHeight() - 144, 12);
        g.setColour(Colour(0xff9ba8b8));
        g.setFont(12);
        g.drawText("01  /  CREATION", 32, 100, 220, 22, Justification::centredLeft);
        g.drawText("ROLE   Lead", 32, 140, 220, 24, Justification::centredLeft);
        g.drawText("SOUND", 32, 180, 220, 22, Justification::centredLeft);
        g.drawText("DENSITY", 32, 260, 220, 22, Justification::centredLeft);
        g.drawText("LOCKS / preserved during regeneration", 32, 376, 230, 22,
                   Justification::centredLeft);
        g.drawText("Double-click: add / Drag: move / Right edge: resize / Right-click: delete", 310,
                   getHeight() - 115, getWidth() - 330, 24, Justification::centredLeft);
    }
    void resized() override {
        int w = getWidth(), h = getHeight();
        brand.setBounds(24, 12, 155, 46);
        subtitle.setBounds(180, 16, 530, 36);
        undo.setBounds(w - 240, 22, 92, 30);
        redo.setBounds(w - 138, 22, 92, 30);
        chords.setBounds(310, 92, w - 715, 32);
        enter.setBounds(w - 390, 92, 110, 32);
        import.setBounds(w - 270, 92, 115, 32);
        demo.setBounds(w - 145, 92, 115, 32);
        bpm.setBounds(310, 137, 210, 30);
        candidate.setBounds(540, 137, 260, 30);
        roll.setBounds(310, 186, w - 340, h - 326);
        patch.setBounds(32, 208, 235, 34);
        density.setBounds(32, 286, 235, 34);
        generate.setBounds(32, 334, 235, 38);
        pitchLock.setBounds(32, 406, 110, 28);
        timeLock.setBounds(150, 406, 110, 28);
        durationLock.setBounds(32, 438, 110, 28);
        velocityLock.setBounds(150, 438, 110, 28);
        soundLock.setBounds(32, 470, 110, 28);
        regionLabel.setBounds(32, 500, 230, 24);
        first.setBounds(32, 528, 110, 30);
        last.setBounds(152, 528, 110, 30);
        regen.setBounds(32, 566, 235, 30);
        play.setBounds(310, h - 78, 90, 32);
        stop.setBounds(408, h - 78, 150, 32);
        midi.setBounds(566, h - 78, 90, 32);
        wav.setBounds(664, h - 78, 90, 32);
        save.setBounds(762, h - 78, 86, 32);
        load.setBounds(856, h - 78, 90, 32);
        status.setBounds(24, h - 36, w - 48, 28);
    }
};
AudioProcessorEditor *Spark::createEditor() { return new SparkEditor(*this); }
AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new Spark(); }
