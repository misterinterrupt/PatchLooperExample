# Description
Loops incoming audio at a user defined interval.  
Use the simple controls to record loops, play and pause them, and punch-in record over them.
Loops can be very long, or very short.
  
# Controls
Press button two to record. First recording sets loop length. Automatically starts looping if 5 minute limit is hit.  
After first loop sound on sound recording enabled. Press button two to toggle SOS recording. Hold button two to clear loop.  
The red light indicates record enable. The green light indicates play enable.  
Press button one to pause/play loop buffer.  
Knob one mixes live input and loop output. Left is only live thru, right is only loop output.

| Control | Description | Comment |
| --- | --- | --- |
| Encoder Press | Play/Record | pressing the encoder once will begin recording into the buffer |
| Encoder Long Press | Clear/Stop | pressing the encoder for a second or more will clear the buffer and stop the playback |
| Ctrl1 | Dry/Wet | Control the balance of the wet and dry to the output |
| Audio Ins | Audio to be captured by the instrument | In 1 mono signal can be recorded or played through dry |
| Audio Outs | Audio mix | Output 1 plays the mono mix of the looper and the input |

# Code Snippet

```cpp
void NextSamples(float &output, float* in, size_t i)  
{  
    if (rec)  
    {  
        WriteBuffer(in, i);  
    }  

    output = buf[pos];

    ......

    if(play)
    {
        pos++;
        pos %= mod;
    }

    if (!rec)
    {
    output = output * drywet + in[i] * (1 -drywet);
    }
}  

```
