#include <string>
#include <cstring>
#include "daisysp.h"
#include "daisy_patch.h"

#define SAMPLE_RATE 48000
#define VOICE_LENGTH_SECONDS 1
#define MAX_RECORD_SIZE (SAMPLE_RATE * 60 * 5) // 5 minutes of floats at 48 khz
#define VOICE_SIZE (SAMPLE_RATE / VOICE_LENGTH_SECONDS) // 
#define INVERT_SCREEN true
#define ENCODER_LONG_PRESS_TIME 666
#define INPUT_GAIN_TRIM 1.10
#define LOOP_GAIN_TRIM 1.25

enum Params {
    MIX_XFADE = 0,
    INPUT_AMP = 1,
    LOOP_AMP = 2,
    RND_AMT = 3,
    RECORD_TOGGLE = 4,
    PLAY_TOGGLE = 5,
    CLEAR_BTN = 6,
};

std::string controlLabelStrs[]  = {"mix", "inp", "lop", "rnd", "rec", "ply", "clr"};
std::string modeStrs[]          = {"X", ">"};

using namespace daisysp;
using namespace daisy;

static DaisyPatch patch;

// taken from the private vars in daisy_patch.h
uint32_t screen_update_last_, screen_update_period_;

float DSY_SDRAM_BSS buf[MAX_RECORD_SIZE];

int   selectedParamIdx          = 0; // the currently selected parameter
int   numParams                 = 7; // number of parameters
bool  editingSelectedParam      = false; // selecting a control with a numeric value and clicking the encoder allows turning the encoder to change the value
bool  first                     = true;  // first loop (sets length)
bool  rec                       = false; // true = currently recording
bool  syncRecordPlay            = false; // start playback when recording starts
bool  unsyncRecordPosition      = false;
bool  play                      = false; // true = currently playing
bool  skipPlayPress             = false; // prevent rising edge from changing play state after encoder long press
bool  bufferReset               = false; // reset/clear loop mode toggle
bool  allowBufferReset          = true;  // variable used to stop repeated resets while holding down encoder
bool  savedPlayState            = false;
bool  positive                  = INVERT_SCREEN ? false : true; // draws text, lines, shapes, pixels on the background
bool  negative                  = INVERT_SCREEN ? true : false; // fills the background, if used for text, prints text in an inverted box (i.e. if text is selected)
int   playPosition              = 0;
int   recordPosition            = 0;
int   loopLength                = MAX_RECORD_SIZE;
int   recordingLength           = 0;
// when recording, the input signal goes to the loop, but not the output
// when not recording, the input goes to the output, but not the loop
float xFadeMix                  = 0.5; // output mix of loop and input signal
float inputLevel                = 0;   // input amplitude level of the input 
float loopLevel                 = 0;   // input amplitude level of the loop

void ResetBuffer();
void Controls();

void NextSamples(float &output, float* in, size_t i);
void DisplayControls();
bool selected(int idx);
bool recordToggled();
bool playToggled();
bool resettingBuffer();
void floatToPercent(char* buff, float val);

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
    bufferReset = true; // this is only set for the clear button's icon 
    play = false;
    playPosition = 0;
    recordPosition = 0;
    recordingLength = 0;
    first = true;
    unsyncRecordPosition = false;
    // set buffer to all 0's
    for(int i = 0; i < MAX_RECORD_SIZE; i++)
    {
        buf[i] = 0;
    }
    loopLength = MAX_RECORD_SIZE;
}

void UpdateButtons()
{
    // encoder held
    if(patch.encoder.TimeHeldMs() >= ENCODER_LONG_PRESS_TIME)
    {
        if(selected(Params::RECORD_TOGGLE)) {
            if(!play && !rec) // length is set when recording has ended for the 'first' loop 
            {
                syncRecordPlay = true;
            }
        }
        if(selected(Params::PLAY_TOGGLE)) {
            if(!skipPlayPress) {
                playPosition = 0;
                unsyncRecordPosition = true;
            }
            skipPlayPress = true;
        }
    }
    // encoder pressed - plays recording
    if(patch.encoder.FallingEdge())
    {
        if(selected(Params::RECORD_TOGGLE)) {
            if(first && rec) // length is set when recording has ended for the 'first' loop 
            {
                first = false;
            }
            if(!rec && !play && syncRecordPlay) {
                play = !play;
                syncRecordPlay = false;
            }
            rec = !rec;
        }
        if(selected(Params::PLAY_TOGGLE)) {
            if(!skipPlayPress) {
                play = !play;
            }
            skipPlayPress = false;
        }
        if(selected(Params::CLEAR_BTN)) {
            if(allowBufferReset) {
                ResetBuffer();
            }
            allowBufferReset = false; // reset won't repeat while the encoder is held down
            play = savedPlayState;
        }
    }
    if(patch.encoder.RisingEdge()) {
        if(selected(Params::CLEAR_BTN)) {
            allowBufferReset = true;
            savedPlayState = play;
            play = false;
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

    xFadeMix = patch.controls[patch.CTRL_1].Process();
    inputLevel = patch.controls[patch.CTRL_2].Process() * INPUT_GAIN_TRIM;
    loopLevel = patch.controls[patch.CTRL_3].Process() * LOOP_GAIN_TRIM;

    UpdateButtons();
}

void WriteBuffer(float* in, size_t i)
{
    buf[recordPosition] = (buf[playPosition] * loopLevel) + (in[i] * inputLevel);
    if(first) {
        recordingLength++;
        loopLength = recordingLength;
    }
}

void NextSamples(float &output, float* in, size_t i)
{
    if (rec) {
        // record the input into the loop
        WriteBuffer(in, i);
    }
    // send the loop to the output
    output = buf[playPosition]; // output is just the loop signal now
    
    // automatically set loop length if we max out the total loop time (defined at top of file)
    if(recordingLength >= MAX_RECORD_SIZE) {
        first = false;
        loopLength = MAX_RECORD_SIZE;
        recordingLength = 0;
    }
    
    if(play) {
        playPosition++;
        playPosition %= loopLength;
    }
    if(rec && play && !unsyncRecordPosition) {
        recordPosition = playPosition;
    }
    if(rec && (!play || unsyncRecordPosition)) {
        // **Strange Feature** additive punch in when recording while playback stopped
        recordPosition++;
        recordPosition %= recordingLength;
    }

    // we don't mix the input into the output when recording. Instead, it is routed to the loop which is monitored at the output
    if(rec) {
        output = output * xFadeMix; // output is the loop output adjusted by the xFade mix
    }
    // when recording stops, the loop and the input are routed thru the output and the levels should be the same
    // TODO:: set loopLevel to 1.0 when recording stops (depends on future implementation of knob value takeover mode)
    if(!rec) {
        output = ((output * loopLevel) * xFadeMix) + ((in[i] * inputLevel) * (1 - xFadeMix));
    }
}

// top left is y:0 x:0
// This will render the display with control values as vertical bars
void DisplayControls()
{
    size_t maxHeight = SSD1309_HEIGHT / 2;
    bool recToggle = recordToggled();
    
    if(dsy_system_getnow() - screen_update_last_ > screen_update_period_) {
        // Graph Knob values
        size_t barWidth, barSpacing;
        size_t barsWidth, barsBottom;
        float  currentCtrlValue;
        size_t currentBarHeight;
        barWidth   = 15;
        barSpacing = 5;
        patch.display.Fill(negative);
        // Bars for all four knobs.
        for(size_t i = 0; i < DaisyPatch::CTRL_LAST; i++) {
            barsWidth            = (barSpacing * i + 1) + (barWidth * i);
            barsBottom           = SSD1309_HEIGHT - 2; // start at the top! (bottom) (64)
            currentCtrlValue     = patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(i));
            currentBarHeight     = (currentCtrlValue * maxHeight);
            for(size_t j = maxHeight; j > 0; j--) {
                for(size_t k = 0; k < barWidth; k++) {
                    // bottom half
                    if((j > currentBarHeight && j != 1) && k % 3 == 0) {
                        patch.display.DrawPixel(barsWidth + k, barsBottom - j, positive);
                    }
                    // top half
                    if((j < currentBarHeight || j <= 1) && k % 3 != 0) {
                        patch.display.DrawPixel(barsWidth + k, barsBottom - j, positive);
                    }
                    // lip
                    if((j < 1) && k % 3 != 0) {
                        patch.display.DrawPixel(barsWidth + k, barsBottom - j, positive);
                    }
                    // debug pixel
                    // patch.display.DrawPixel(90, maxHeight, positive);
                    // patch.display.DrawPixel(100, (barsBottom - j), positive);
                    // patch.display.DrawPixel(110, barsBottom - 1, positive);
                    // patch.display.DrawPixel(127, currentBarHeight, positive);
                }
            }
        }

        // knob labels & numeric values
        patch.display.SetCursor(0, 0);
        patch.display.WriteString(&controlLabelStrs[0][0], Font_6x8, selected(Params::MIX_XFADE));
        patch.display.SetCursor(4, 12);
        char val[2];
        floatToPercent(val, patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(Params::MIX_XFADE)));
        patch.display.WriteString(val, Font_6x8, positive);

        patch.display.SetCursor(20, 0);
        patch.display.WriteString(&controlLabelStrs[1][0], Font_6x8, selected(Params::INPUT_AMP));
        patch.display.SetCursor(24, 12);
        floatToPercent(val, patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(Params::INPUT_AMP)));
        patch.display.WriteString(val, Font_6x8, positive);

        patch.display.SetCursor(40, 0);
        patch.display.WriteString(&controlLabelStrs[2][0], Font_6x8, selected(Params::LOOP_AMP));
        patch.display.SetCursor(44, 12);
        floatToPercent(val, patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(Params::LOOP_AMP)));
        patch.display.WriteString(val, Font_6x8, positive);

        patch.display.SetCursor(60, 0);
        patch.display.WriteString(&controlLabelStrs[3][0], Font_6x8, selected(Params::RND_AMT));
        patch.display.SetCursor(64, 12);
        floatToPercent(val, patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(Params::RND_AMT)));
        patch.display.WriteString(val, Font_6x8, positive);

        // rec label
        patch.display.SetCursor(80, 0);
        patch.display.WriteString(&controlLabelStrs[4][0], Font_6x8,  selected(Params::RECORD_TOGGLE));
        // rec icon
        patch.display.DrawCircle(89, 16, 5, positive);
        patch.display.DrawCircle(89, 16, 3, positive);
        patch.display.DrawCircle(89, 16, 2, recToggle);
        patch.display.DrawCircle(89, 16, 1, recToggle);
        patch.display.DrawCircle(89, 16, 0, recToggle);

        // play/stop label
        patch.display.SetCursor(100, 0);
        patch.display.WriteString(&controlLabelStrs[5][0], Font_6x8,  selected(Params::PLAY_TOGGLE));
        // play / stop icon
        char* playModeStr = &modeStrs[1][0];
        if (play) {
            patch.display.SetCursor(104, 12);
            patch.display.WriteString(playModeStr, Font_7x10, positive);
        } else {
            patch.display.DrawRect(102, 11, 112, 21, positive);
            // patch.display.WriteString(stopModeStr, Font_7x10, positive);
        }

        // clear label
        patch.display.SetCursor(80, 30);
        patch.display.WriteString(&controlLabelStrs[6][0], Font_6x8,  selected(Params::CLEAR_BTN));
        // clear icon
        char* clearModeStr = &modeStrs[0][0];
        if (resettingBuffer()) {
            patch.display.DrawCircle(89, 45, 5, positive);
        } else {
            patch.display.SetCursor(85, 42);
            patch.display.WriteString(clearModeStr, Font_7x10, positive);
        }

        patch.display.Update();
        screen_update_last_ = dsy_system_getnow();
    }
}

bool selected(int idx)
{
    return (idx == selectedParamIdx) ? negative : positive;
}
bool recordToggled()
{
    return rec ? positive : negative;
}
bool playToggled()
{
    return play ? positive : negative;
}
bool resettingBuffer()
{
    return bufferReset? positive : negative;
}
void floatToPercent(char* buff, float val)
{
    int percent = static_cast<int>(100 * val);
    strncpy(buff, std::to_string(percent).c_str(), 2);
}