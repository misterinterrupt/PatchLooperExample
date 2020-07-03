#include <string>
#include "daisysp.h"
#include "daisy_patch.h"

#define MAX_SIZE (48000 * 60 * 5) // 5 minutes of floats at 48 khz

using namespace daisysp;
using namespace daisy;

static DaisyPatch patch;

bool first = true;  //first loop (sets length)
bool rec   = false; //currently recording
bool play  = false; //currently playing
std::string controlLabelStrs[] = {"d/w", "amp", "~", "cmp"};
std::string modeStrs[] = {"stopped", "playing", "recording"};

int   pos = 0;
float DSY_SDRAM_BSS buf[MAX_SIZE];
int                 mod = MAX_SIZE;
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
void DisplayTwoThirdsBars(bool invert);

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
        //display the four control levels
        DisplayTwoThirdsBars(false);
    }
}

//Resets the buffer
void ResetBuffer()
{
    play  = false;
    rec   = false;
    first = true;
    pos   = 0;
    len   = 0;
    for(int i = 0; i < mod; i++)
    {
        buf[i] = 0;
    }
    mod   = MAX_SIZE;
}

void UpdateButtons()
{
    // encoder pressed - plays recording
    if(patch.encoder.RisingEdge())
    {
        if(first && rec)
        {
            first = false;
            mod   = len;
            len   = 0;
        }

	res = true;
        play = true;
        rec  = !rec;
    }

    // encoder held
    if(patch.encoder.TimeHeldMs() >= 1000 && res)
    {
        ResetBuffer();
        res = false;
    }
    
    // //button2 pressed and not empty buffer
    // if(patch.button2.RisingEdge() && !(!rec && first))
    // {
    //     play = !play;
	// rec = false;
    // }
}

//Deals with analog controls 
void Controls()
{
    patch.UpdateAnalogControls();
    patch.DebounceControls();

    drywet = patch.controls[patch.CTRL_1].Process();
    inLevel = patch.controls[patch.CTRL_2].Process();
    compensation = patch.controls[patch.CTRL_4].Process();

    UpdateButtons();

    //leds
    // patch.led1.Set(0, play == true, 0);
    // patch.led2.Set(rec == true, 0, 0);
   
    // patch.UpdateLeds();
}

void WriteBuffer(float* in, size_t i)
{
    buf[pos] = (buf[pos] * compensation + (in[i] * inLevel) * compensation);
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
    if(len >= MAX_SIZE)
    {
        first = false;
        mod   = MAX_SIZE;
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
void DisplayTwoThirdsBars(bool invert)
{
    size_t maxHeight = 36;
    bool on, off;
    on  = invert ? false : true;
    off = invert ? true : false;
    if(dsy_system_getnow() - screen_update_last_ > screen_update_period_)
    {
        // Graph Knobs
        size_t barwidth, barspacing;
        size_t curx, cury;
        barwidth   = 8;
        barspacing = 24;
        patch.display.Fill(off);
        // Bars for all four knobs.
        for(size_t i = 0; i < DaisyPatch::CTRL_LAST; i++)
        {
            float  v;
            size_t dest;
            curx = (barspacing * i + 1) + (barwidth * i);
            cury = SSD1309_HEIGHT; // start at the top! (bottom)
            v    = patch.GetCtrlValue(static_cast<DaisyPatch::Ctrl>(i));
            dest = (v * maxHeight);
            for(size_t j = dest; j > 0; j--)
            {
                for(size_t k = 0; k < barwidth; k++)
                {
                    patch.display.DrawPixel(curx + k, cury - j, on);
                }
            }
        }
        // rec / play mode
        char* currModeStr = rec ? &modeStrs[2][0] : (play ? &modeStrs[1][0] : &modeStrs[0][0]);
        patch.display.SetCursor(0,9);
        patch.display.WriteString(currModeStr, Font_6x8, true);

        // knob labels
        patch.display.SetCursor(0, 0);
        patch.display.WriteString(&controlLabelStrs[0][0], Font_6x8, true);
        patch.display.SetCursor(32, 0);
        patch.display.WriteString(&controlLabelStrs[1][0], Font_6x8, true);
        patch.display.SetCursor(64, 0);
        patch.display.WriteString(&controlLabelStrs[2][0], Font_6x8, true);
        patch.display.SetCursor(96, 0);
        patch.display.WriteString(&controlLabelStrs[3][0], Font_6x8, true);

        patch.display.Update();
        screen_update_last_   = dsy_system_getnow();
    }
}
