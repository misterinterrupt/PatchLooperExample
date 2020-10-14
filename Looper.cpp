#include <string>
#include <cstring>
#include "daisysp.h"
#include "daisy_patch.h"

#define SAMPLE_RATE 48000
#define VOICE_LENGTH_SECONDS 1
#define MAX_RECORD_SIZE (SAMPLE_RATE * 60 * 5) // 5 minutes of floats at 48 khz
#define VOICE_SIZE (SAMPLE_RATE / VOICE_LENGTH_SECONDS) // 

enum Params {
    DRY_WET_XFADE = 0,
    INPUT_AMP = 1,
    SPD_VAR = 2,
    RND_AMT = 3,
    RECORD_TOGGLE = 4,
    PLAY_TOGGLE = 5,
    CLEAR_BTN = 6,
};

using namespace daisysp;
using namespace daisy;

static DaisyPatch patch;

int selectedParamIdx = 0;
int numParams = 7;
bool selectedParamEntered = false;
bool first = true;  //first loop (sets length)
bool rec   = false; //currently recording
bool play  = false; //currently playing
bool invert = true;
bool noFill = invert ? false : true;
bool fill = invert ? true : false;

std::string controlLabelStrs[] = {"d/w", "amp", "spd", "rnd", "rec", "ply", "clr"};
std::string modeStrs[] = {"X", ">"};

int   pos = 0;
float DSY_SDRAM_BSS buf[MAX_RECORD_SIZE];
int                 mod = MAX_RECORD_SIZE;
int                 len = 0;
float drywet = 0;
float inLevel = 0;
float compensation = 0;
bool res = false;

// taken from the private vars in daisy_patch.h
uint32_t screen_update_last_, screen_update_period_;

void ResetBuffer();
void Controls();

void NextSamples(float &output, float* in, size_t i);
void DisplayControls();
void floatToPercent(char* buff, float val);
bool selected(int idx);

static void AudioCallback(float **in, float **out, size_t size)
{
    float output = 0;

    Controls();

    for(size_t i = 0; i < size; i += 2)
    {
        NextSamples(output, in[0], i);   
	
        // left and right outs ?
        out[0][i] = out[0][i+1] = output;
    }

}

int main(void)
{
    // Set Screen update vars - see declaration, above
    screen_update_period_ = 17; // roughly 60Hz
    screen_update_last_   = dsy_system_getnow();

    patch.Init();
    ResetBuffer();

    // start callback
    patch.StartAdc();
    patch.StartAudio(AudioCallback);

    while(1)
    {
        DisplayControls();
    }
}

//Resets the buffer
void ResetBuffer()
{

    res   = false;
    rec   = false;
    first = true;
    pos   = 0;
    len   = 0;
    for(int i = 0; i < mod; i++)
    {
        buf[i] = 0;
    }
    mod   = MAX_RECORD_SIZE;
}

void UpdateButtons()
{
    // encoder held
    if(patch.encoder.TimeHeldMs() >= 1000)
    {

    }
    // encoder pressed - plays recording
    if(patch.encoder.FallingEdge())
    {
        if(selected(Params::RECORD_TOGGLE)) {
            if(first && rec)
            {
                first = false;
                mod   = len;
                len   = 0;
            }
            rec = !rec;
        }
        if(selected(Params::PLAY_TOGGLE)) {
            play = !play;
        }
        if(selected(Params::CLEAR_BTN)) {
            res = true;
        }
    }
    if(patch.encoder.RisingEdge()){
        if(selected(Params::CLEAR_BTN)) {
            ResetBuffer();
        }
    }
    //encoder changes the selected param
    int newParam = (selectedParamIdx + patch.encoder.Increment()) % numParams;
    selectedParamIdx = newParam >= 0 ? newParam : numParams - 1;
}

// Deals with analog controls 
void Controls()
{
    patch.UpdateAnalogControls();
    patch.DebounceControls();

    drywet = patch.controls[patch.CTRL_1].Process();
    inLevel = patch.controls[patch.CTRL_2].Process();
    compensation = patch.controls[patch.CTRL_4].Process();

    UpdateButtons();
}

void WriteBuffer(float* in, size_t i)
{
    buf[pos] = buf[pos] * 0.75 + (in[i]) * 0.75;
    if(first)
    {
        len++;
    }
}

void NextSamples(float &output, float* in, size_t i)
{
    if (rec)
    {
        WriteBuffer(in, i);
    }
    
    output = buf[pos];
    
    //automatic looptime
    if(len >= MAX_RECORD_SIZE)
    {
        first = false;
        mod   = MAX_RECORD_SIZE;
        len   = 0;
    }
    
    if(play)
    {
        pos++;
        pos %= mod;
    }

    if (!rec)
    {
        output = ((output * drywet) + ((in[i] * inLevel) * (1 -drywet)));
    }
}

// top left is y:0 x:0
// This will render the display with the controls as vertical bars
void DisplayControls()
{
    size_t maxHeight = SSD1309_HEIGHT / 2;
    
    if(dsy_system_getnow() - screen_update_last_ > screen_update_period_) {
        // Graph Knob values
        size_t barWidth, barSpacing;
        size_t barsWidth, barsBottom;
        barWidth   = 15;
        barSpacing = 5;
        patch.display.Fill(fill);
        // Bars for all four knobs.
        for(size_t i = 0; i < DaisyPatch::CTRL_LAST; i++) {
            float  currentCtrlValue;
            size_t currentBarHeight;
            barsWidth            = (barSpacing * i + 1) + (barWidth * i);
            barsBottom           = SSD1309_HEIGHT - 2; // start at the top! (bottom) (64)
            currentCtrlValue    = patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(i));
            currentBarHeight    = (currentCtrlValue * maxHeight);
            for(size_t j = maxHeight; j > 0; j--) {
                for(size_t k = 0; k < barWidth; k++) {
                    // bottom half
                    if((j > currentBarHeight && j != 1) && k % 3 == 0) {
                        patch.display.DrawPixel(barsWidth + k, barsBottom - j, noFill);
                    }
                    // top half
                    if((j < currentBarHeight || j <= 1) && k % 3 != 0) {
                        patch.display.DrawPixel(barsWidth + k, barsBottom - j, noFill);
                    }
                    // lip
                    if((j < 1) && k % 3 != 0) {
                        patch.display.DrawPixel(barsWidth + k, barsBottom - j, noFill);
                    }
                    // debug pixel
                    // patch.display.DrawPixel(90, maxHeight, noFill);
                    // patch.display.DrawPixel(100, (barsBottom - j), noFill);
                    // patch.display.DrawPixel(110, barsBottom - 1, noFill);
                    // patch.display.DrawPixel(127, currentBarHeight, noFill);
                }
            }
        }

        // knob labels & numeric values
        patch.display.SetCursor(0, 0);
        patch.display.WriteString(&controlLabelStrs[0][0], Font_6x8, selected(Params::DRY_WET_XFADE));
        patch.display.SetCursor(4, 12);
        char val[2];
        floatToPercent(val, patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(Params::DRY_WET_XFADE)));
        patch.display.WriteString(val, Font_6x8, noFill);

        patch.display.SetCursor(20, 0);
        patch.display.WriteString(&controlLabelStrs[1][0], Font_6x8, selected(Params::INPUT_AMP));
        patch.display.SetCursor(24, 12);
        floatToPercent(val, patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(Params::INPUT_AMP)));
        patch.display.WriteString(val, Font_6x8, noFill);

        patch.display.SetCursor(40, 0);
        patch.display.WriteString(&controlLabelStrs[2][0], Font_6x8, selected(Params::SPD_VAR));
        patch.display.SetCursor(44, 12);
        floatToPercent(val, patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(Params::SPD_VAR)));
        patch.display.WriteString(val, Font_6x8, noFill);

        patch.display.SetCursor(60, 0);
        patch.display.WriteString(&controlLabelStrs[3][0], Font_6x8, selected(Params::RND_AMT));
        patch.display.SetCursor(64, 12);
        floatToPercent(val, patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(Params::RND_AMT)));
        patch.display.WriteString(val, Font_6x8, noFill);

        // rec label
        patch.display.SetCursor(80, 0);
        patch.display.WriteString(&controlLabelStrs[4][0], Font_6x8,  selected(Params::RECORD_TOGGLE));
        // rec icon
        patch.display.DrawCircle(89, 16, 5, noFill);
        patch.display.DrawCircle(89, 16, 3, noFill);
        patch.display.DrawCircle(89, 16, 2, rec);
        patch.display.DrawCircle(89, 16, 1, rec);
        patch.display.DrawCircle(89, 16, 0, rec);

        // play/stop label
        patch.display.SetCursor(100, 0);
        patch.display.WriteString(&controlLabelStrs[5][0], Font_6x8,  selected(Params::PLAY_TOGGLE));
        // play / stop icon
        char* playModeStr = &modeStrs[1][0];
        if (play) {
            patch.display.SetCursor(104, 12);
            patch.display.WriteString(playModeStr, Font_7x10, noFill);
        } else {
            patch.display.DrawRect(102, 11, 112, 21, noFill);
            // patch.display.WriteString(stopModeStr, Font_7x10, noFill);
        }

        // clear label
        patch.display.SetCursor(80, 30);
        patch.display.WriteString(&controlLabelStrs[6][0], Font_6x8,  selected(Params::CLEAR_BTN));
        // clear icon
        char* clearModeStr = &modeStrs[0][0];
        if (res) {
            patch.display.SetCursor(85, 42);
            patch.display.WriteString(clearModeStr, Font_7x10, noFill);
        } else {
            patch.display.DrawCircle(89, 45, 5, noFill);
        }

        patch.display.Update();
        screen_update_last_ = dsy_system_getnow();
    }
}

bool selected(int idx)
{
    return (idx == selectedParamIdx) ? fill : noFill;
}

void floatToPercent(char* buff, float val)
{
    int percent = static_cast<int>(100 * val);
    strncpy(buff, std::to_string(percent).c_str(), 2);
}