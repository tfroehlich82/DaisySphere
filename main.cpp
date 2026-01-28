#include "daisy_patch.h"
#include "daisysp.h"
#include <string>
#include <vector>

using namespace daisy;
using namespace daisysp;

/**
 * DAISYSPHERE FRAMEWORK
 * Inspired by Hemisphere (O_C), Droplets and EvansUniverse.
 */

class App {
public:
    virtual ~App() {}
    virtual void Init(float samplerate) = 0;
    virtual void Process(float in_l, float in_r, float& out_l, float& out_r, float ctrl1, float ctrl2, bool gate) = 0;
    virtual void UpdateUI(OledDisplay& display, bool left_side) = 0;
    virtual const char* GetName() = 0;
};

// --- APP: OSCILLATOR ---
class OscApp : public App {
    Oscillator osc;
    float freq;
public:
    void Init(float samplerate) override {
        osc.Init(samplerate);
        osc.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
    }
    void Process(float in_l, float in_r, float& out_l, float& out_r, float ctrl1, float ctrl2, bool gate) override {
        freq = mtof(ctrl1 * 127.0f);
        osc.SetFreq(freq);
        osc.SetAmp(ctrl2);
        out_l = out_r = osc.Process();
    }
    void UpdateUI(OledDisplay& display, bool left_side) override {
        int x = left_side ? 0 : 68;
        display.SetCursor(x, 20);
        display.WriteString("Freq:", Font_6x8, true);
        display.SetCursor(x, 30);
        display.WriteString((std::to_string((int)freq) + "Hz").c_str(), Font_6x8, true);
    }
    const char* GetName() override { return "OSCI"; }
};

// --- APP: BITCRUSHER ---
class BitcrushApp : public App {
    float sr;
public:
    void Init(float samplerate) override { sr = samplerate; }
    void Process(float in_l, float in_r, float& out_l, float& out_r, float ctrl1, float ctrl2, bool gate) override {
        float factor = powf(ctrl1, 3.0f); // Non-linear scaling
        int bits = (int)(ctrl2 * 15.0f) + 1;
        
        // Simple Crush Logic
        float step = powf(2.0f, bits);
        out_l = floorf(in_l * step) / step;
        out_r = floorf(in_r * step) / step;
    }
    void UpdateUI(OledDisplay& display, bool left_side) override {
        int x = left_side ? 0 : 68;
        display.SetCursor(x, 20);
        display.WriteString("CRUSH", Font_6x8, true);
    }
    const char* GetName() override { return "BITCR"; }
};

// --- APP: WAVEFOLDER ---
class FolderApp : public App {
public:
    void Init(float samplerate) override {}
    void Process(float in_l, float in_r, float& out_l, float& out_r, float ctrl1, float ctrl2, bool gate) override {
        float gain = ctrl1 * 10.0f + 1.0f;
        auto fold = [](float in) {
            if (in > 1.0f) in = 2.0f - in;
            if (in < -1.0f) in = -2.0f - in;
            return in;
        };
        out_l = fold(in_l * gain);
        out_r = fold(in_r * gain);
    }
    void UpdateUI(OledDisplay& display, bool left_side) override {
        int x = left_side ? 0 : 68;
        display.SetCursor(x, 20);
        display.WriteString("FOLD", Font_6x8, true);
    }
    const char* GetName() override { return "FOLDER"; }
};

// --- APP: REVERB (CLOUDS STYLE) ---
class ReverbApp : public App {
    ReverbSc verb;
public:
    void Init(float samplerate) override {
        verb.Init(samplerate);
    }
    void Process(float in_l, float in_r, float& out_l, float& out_r, float ctrl1, float ctrl2, bool gate) override {
        verb.SetFeedback(ctrl1);
        verb.SetLpFreq(fmap(ctrl2, 500.0f, 18000.0f));
        verb.Process(in_l, in_r, &out_l, &out_r);
    }
    void UpdateUI(OledDisplay& display, bool left_side) override {
        int x = left_side ? 0 : 68;
        display.SetCursor(x, 20);
        display.WriteString("SPACE", Font_6x8, true);
    }
    const char* GetName() override { return "REVERB"; }
};

// --- APP: EUCLIDEAN SEQUENCER ---
class EuclideanApp : public App {
    int length = 16;
    int fill = 4;
    int pos = 0;
    bool last_gate = false;
    bool pattern[32];

    void Generate() {
        for(int i=0; i<32; i++) pattern[i] = false;
        if (fill == 0) return;
        int acc = 0;
        for(int i=0; i<length; i++) {
            acc += fill;
            if (acc >= length) {
                acc -= length;
                pattern[i] = true;
            }
        }
    }
public:
    void Init(float samplerate) override { Generate(); }
    void Process(float in_l, float in_r, float& out_l, float& out_r, float ctrl1, float ctrl2, bool gate) override {
        int new_len = (int)(ctrl1 * 31.0f) + 1;
        int new_fill = (int)(ctrl2 * (float)new_len);
        if (new_len != length || new_fill != fill) {
            length = new_len;
            fill = new_fill;
            Generate();
        }
        if (gate && !last_gate) pos = (pos + 1) % length;
        last_gate = gate;
        out_l = out_r = pattern[pos] ? 1.0f : 0.0f;
    }
    void UpdateUI(OledDisplay& display, bool left_side) override {
        int x = left_side ? 0 : 68;
        display.SetCursor(x, 20);
        display.WriteString((std::to_string(fill) + "/" + std::to_string(length)).c_str(), Font_6x8, true);
        // Visual Pattern
        for(int i=0; i<length; i++) {
            int px = x + (i % 8) * 7;
            int py = 35 + (i / 8) * 7;
            display.DrawRect(px, py, px+4, py+4, true, pattern[i]);
            if (i == pos) display.DrawRect(px-1, py-1, px+5, py+5, true, false);
        }
    }
    const char* GetName() override { return "EUCLID"; }
};

// --- MAIN CONTROL LOGIC ---

DaisyPatch patch;
std::vector<App*> apps;
int left_app_idx = 0;
int right_app_idx = 1;
bool menu_left = false;
bool menu_right = false;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    patch.ProcessAllControls();
    bool g1 = patch.gate_input[0].State();
    bool g2 = patch.gate_input[1].State();

    for (size_t i = 0; i < size; i++) {
        float l_ol, l_or, r_ol, r_or;
        apps[left_app_idx]->Process(in[0][i], in[1][i], l_ol, l_or, patch.controls[0].Value(), patch.controls[1].Value(), g1);
        apps[right_app_idx]->Process(in[2][i], in[3][i], r_ol, r_or, patch.controls[2].Value(), patch.controls[3].Value(), g2);
        out[0][i] = l_ol; out[1][i] = l_or;
        out[2][i] = r_ol; out[3][i] = r_or;
    }
}

int main(void) {
    patch.Init();
    float sr = patch.AudioSampleRate();

    apps.push_back(new OscApp());
    apps.push_back(new FilterApp()); // Aus vorherigem Code
    apps.push_back(new BitcrushApp());
    apps.push_back(new FolderApp());
    apps.push_back(new ReverbApp());
    apps.push_back(new EuclideanApp());

    for (auto a : apps) a->Init(sr);

    patch.StartAudio(AudioCallback);
    while (1) {
        if (patch.encoder[0].RisingEdge()) menu_left = !menu_left;
        if (patch.encoder[3].RisingEdge()) menu_right = !menu_right;

        if (menu_left) {
            int inc = patch.encoder[0].Increment();
            if (inc != 0) left_app_idx = (left_app_idx + inc + apps.size()) % apps.size();
        }
        if (menu_right) {
            int inc = patch.encoder[3].Increment();
            if (inc != 0) right_app_idx = (right_app_idx + inc + apps.size()) % apps.size();
        }

        patch.display.Fill(false);
        patch.display.DrawLine(64, 0, 64, 64, true);

        // UI Links
        patch.display.SetCursor(0, 0);
        patch.display.WriteString(apps[left_app_idx]->GetName(), Font_7x10, !menu_left);
        apps[left_app_idx]->UpdateUI(patch.display, true);

        // UI Rechts
        patch.display.SetCursor(68, 0);
        patch.display.WriteString(apps[right_app_idx]->GetName(), Font_7x10, !menu_right);
        apps[right_app_idx]->UpdateUI(patch.display, false);

        patch.display.Update();
        System::Delay(10);
    }
}