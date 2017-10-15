# SEQ-Euclid

A 4 channel Euclidian sequencer for [VCV Rack](vcvrack.com) based on the SEQ3 built in sequencer.

This is based on the paper [The Euclidean algorithm generates traditional musical rhythms](http://cgm.cs.mcgill.ca/~godfried/rhythm-and-mathematics.html) from Proceedings of BRIDGES: Mathematical Connections in Art, Music, and Science by Godfried T. Toussaint and the SEQ-3 built-in from vcvrack.

If you are interested in trying this yourself I would recommend looking at the Bjorklund paper The Theory of Rep-Rate Pattern Generation in the SNS Timing System [SNS-NOTE-CNTRL-99](https://www.google.ca/url?sa=t&rct=j&q=&esrc=s&source=web&cd=1&cad=rja&uact=8&ved=0ahUKEwjnms7w0vPWAhWlx4MKHafnCJQQFggpMAA&url=https%3A%2F%2Fpdfs.semanticscholar.org%2Fc652%2Fd0a32895afc5d50b6527447824c31a553659.pdf&usg=AOvVaw1CzsXZMPaPY938Z1PG5zBC) for implementation information.


## Building

This has been built against vcvrack 0.4.0, but I've got a non-standard build environment and won't package this up differently until I can work up the energy to get MinGW going so you will need to integrate this yourself.

In the init function for whatever plugin you integrate this with you will need to include a line like:

createModel<SEQEuclidWidget>(plugin, "SEQE", "SEQ-Euclid");

In Fundimental.cpp for the fundimental plugins, or Core.cpp for the core system etc.

You will also need to add a block like:

struct SEQEuclidWidget : ModuleWidget {
	SEQEuclidWidget();
};

Into the plugin header file, eg: Fundimental.hpp / core.hpp / wherever as above and add this header to SEQEuclid.cpp.

You may also wish to adjust the BG_IMAGE_FILE and FONT_FILE macros in SEQEuclid.cpp.

If you're here you're probably building this yourself, starting this up with no settings.json file will result in the probability controls defaulting to 0 and no signals happening. You will want to turn these to the far right.


## Usage 
The basic idea is that you select a bank to use, enter a pattern fill amount, a pattern length, a probability amount and wire the output to something that needs gates or triggers.

The algorithm created by Bjorklun that's being used to create these patterns will take pattern length and evenly place fill amount of beats in it.

For example 5 and 7 will result in a pattern of 1011011 while 5 and 12 results with 100101001010.

If fill is greater than length the sequencer will output nothing but beats at the given BPM eg 1111111....

You can make things more interesting by using the probabilty and jog controls. 
The probably knob at far right will allow all beats to pass, at 12 noon 50% of beats and far left 0 beats.
The jog control will allow you to step that bank's pattern forward by one step to allow you to offset patterns.


### BPM
Top left is a BMP indicator and control knob. Use to set speed.

### Clock In
You can wire up an external clock source to the input under the BPM next to the clock icon

### Reset
Reset either by trigger signal or button push will reset all internal counters to 0. This has the effect of starting all banks off at the beginning of their sequences.
This is useful if you want to be sure your patterns are lined up as you expect.

### Gate Length
This will allow you to change the length of the gates sent by the sequencer. At full right close beats eg patterns like 11101 the gate signals will bleed into each other. Dial back for individual beats. I found this handy for driving the modal synth.

### Sequencer Banks

#### Fill Display
Shows current fill amount

#### Fill Control
Allows changing the fill amount from 0 to 256

#### Length Display
Shows current fill amount

#### Length Control
Allows changing the length amount from 0 to 256

#### Probability Control
Allows changing the % chance that a beat will be sent out. Far left 0% far right 100%.

#### Gate Out
Sends gate signals out

#### Trigger Out
Sends trigger signals out

### Summed Outputs

#### Gate Sum Out
If there is any gate active in banks 1 through 4 a gate will be sent.


#### Trigger Sum Out
If there is any trigger active in banks 1 through 4 a gate will be sent.

### Blinky Light
This will blink for the duration of each gate signal.

