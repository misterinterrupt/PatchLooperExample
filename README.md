# Description
A looper with controls that are well suited for creative modular use. A functional and fun WIP. 
  
# Controls
Turn the encoder to select `rec`, `ply`, or `clr`.
Hold/long press the encoder while `rec` is selected to start recording and playing at the same time. 
On startup or after performing the `clr` action, the first recording will set the loop's length. 
Looping will start automatically if the 5 minute limit is hit before stopping recording.
After the first loop is recorded, sound on sound recording is enabled, live recording levels are set by adjusting `In1` and `lop`.
Selecting `ply` and pressing the encoder will stop and start loop playback, regardless of the record state.
Hold/long press `ply` to set the playback position to the loop start.
Select `clr` and press the encoder to clear the loop. This resets the loop length, record position, and playback position.
The record icon indicates that the loop is recording when its circle is filled. 
The carrot/square indicate the play/stopped state.  
Knob one mixes live input and loop output. Left is only live thru, right is only loop output.
CTRL2 sets the level of In 1 and displays on the `in1` fader.
CTRL3 sets the level of loop playback and displays on the `lop` fader. 

*~The record position increments when recording is enabled, and stays synced to the play position when playback is also enabled.
After pressing the encoder when `clr` is selected, if `rec`'s sync start is not used to start loop playback when recording starts, the input mix will record into the buffer even while the loop is not playing. This can be used to create concréte-style collaging or put another way: sequential additive punch-in recording. When loop playback starts, the record position is synced to the play position.


| Control | Action | Description | Comment |
| --- | --- | --- | --- |
| global | Encoder Turn | select parameter | turning the encoder will select a parameter to control |
| global | Encoder press | parameter action | pressing the encoder will perform a parameter's action if it has one |
<!-- | global | Encoder press | parameter action | pressing the encoder with a numeric value will enter numeric/fine edit mode for that control | -->
| rec | Encoder Press | Record start | press the encoder once to begin recording into the buffer |
| rec | Encoder Long Press | sync start | hold/long-press the encoder to start playback when recording begins (upon encoder release) |
| ply | Encoder Press | Playback start | press the encoder once to begin recording the live input/loop mix |
| ply | Encoder Long Press | Reset play position | hold/long-press the encoder to set the playback position to the loop start |
| clr | Encoder Press | Record start | press the encoder once to begin recording into the buffer |
| mix | Ctrl1 | X-fade input/loop mix | Control the balance of the input and the loop playback at the output (not recorded into buffer) |
| in1 | Ctrl2 | In 1 amplitude | Control the gain of In 1 (level recorded into buffer) |
| lop | Ctrl3 | Loop amplitude | Control the gain of the loop playback (level recorded into buffer) |
| Audio Ins | In 1 | Audio to be captured by the instrument | In 1 mono signal |
| Audio Outs | Out 1 | Audio mix | Output 1 plays the mono mix of the input loop playback |

# Code Snippet

```cpp
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
    if(rec && play) {
        recordPosition = playPosition;
    }
    if(rec && !play) {
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

```
