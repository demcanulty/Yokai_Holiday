// Copyright 2023 Dan McAnulty
// 
// Author: Dan McAnulty(d.e.mcanulty@gmail.com)
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.


#include "plugin.hpp"
#include "Taps.hpp"



struct YH_Red_Trimpot : app::SvgKnob {
	widget::SvgWidget* bg;

	YH_Red_Trimpot() {
		minAngle = -0.75 * M_PI;
		maxAngle = 0.75 * M_PI;

		bg = new widget::SvgWidget;
		fb->addChildBelow(bg, tw);

		setSvg(Svg::load(asset::plugin(pluginInstance,"res/YHComponentLibrary/new_Trimpot.svg")));
		bg->setSvg(Svg::load(asset::plugin(pluginInstance,"res/YHComponentLibrary/new_Trimpot_bg.svg")));
	}
};






struct Taps : Module {
    enum ParamId {
        BUFFER_LENGTH_PARAM,
        BUFFER_FEEDBACK_PARAM,
        REVERSE_LOCATION_PARAM,
        FORWARD_LOCATION_PARAM,
        BUFFER_LENGTH_MOD_PARAM,
        BUFFER_FEEDBACK_MOD_PARAM,
        REVERSE_LOCATION_MOD_PARAM,
        FORWARD_LOCATION_MOD_PARAM,

        PARAMS_LEN
    };
    enum InputId {
        IN_INPUT,
        T_CLOCK_INPUT,
        LEN_INPUT,
        FEEDB_INPUT,
        REV_LOC_INPUT,
        FORW_LOC_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        REV_OUT_OUTPUT,
        FORW_OUT_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        BUFF_LED_LIGHT,
        REV_LED_LIGHT,
        CLOCK_LED_LIGHT,
        LIGHTS_LEN
    };

    enum PTR_STATE{
        PTR_STATE_FADE_FINISHED,
        PTR_STATE_FADE_IN,
        PTR_STATE_FADE_OUT
    };
    


    //***  TAPS  ****
    #define USER_BUFFER_LENGTH_DEFAULT  5.f 
    #define BUFFER_TIME_LENGTH          10
    #define TRAIL_BUFFER_SIZE  300000
    static const int  BUFFER_SIZE  = 96000 * BUFFER_TIME_LENGTH;
   
    float delay_buffer[BUFFER_SIZE];
    float trail_buffer[TRAIL_BUFFER_SIZE];

    float lastWet = 0.f;


    int inptr;
    int rev_outptr;
    int forw_outptr = BUFFER_SIZE / 4;
    int trailptr= 48000;
    uint32_t target_buffer_length = 48000 * 5;
    uint32_t knob_set_buffer_length;
    float target_forw_delay_distance = 2;
    float target_rev_delay_distance = 2;
    float buffer_length_param, buffer_feedback_param, reverse_location_param, forward_location_param;
    uint32_t ptr_trail_distance;

    uint32_t fade_count;
    

    enum PTR_STATE ptr_state;
    int offset_pointer;
    const int fade_buffer_lookahead = 1000;
    
    float fade_amount;

    float calculated_fade_count = 7000.f;
    float last_sample_rate;

    bool clock_input_state, last_clock_state;
    float clock_sample_counter;
    int blink_sample_counter;
    uint32_t startup_delay_counter;
    bool go_ahead_and_do_stuff;
    #define MIN_LENGTH_SETTING  (calculated_fade_count + (float)fade_buffer_lookahead + 100.f)

    #define CLOCK_INPUT_ARRAY_SIZE   5
    uint32_t clock_input_array[CLOCK_INPUT_ARRAY_SIZE];
    uint8_t  clk_array_index;
    float clock_led_brightness;

    float led_dim_rate = 0.5f * 480000.f;

    float buffer_length_mod;
    float buffer_feedback_mod;
    float reverse_location_mod;
    float forward_location_mod;

    float rev_led_brightness;
    float buff_led_brightness;

    float old_feedback;
    int buffer_length_divider;
    float clock_count_delta = 2;
    int target_blink_length;
    

    Taps() 
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(BUFFER_LENGTH_PARAM,    0.f, 10.f, USER_BUFFER_LENGTH_DEFAULT, "Length", "s");
        configParam(BUFFER_FEEDBACK_PARAM,   0.f,  1.f, 0.4f, "Feedback", "");
        configParam(REVERSE_LOCATION_PARAM, 0.f,  1.f, 0.5f, "Location", "");
        configParam(FORWARD_LOCATION_PARAM, 0.f,  1.f, 0.5f,  "Location", "");

        configParam(BUFFER_LENGTH_MOD_PARAM,    -1.f, 1.f, 0.f, "Length Modulation", "");
        configParam(BUFFER_FEEDBACK_MOD_PARAM,   -1.f, 1.f, 0.f, "Feedback Modulation", "");
        configParam(REVERSE_LOCATION_MOD_PARAM, -1.f, 1.f, 0.f,  "Location Modulation", "");
        configParam(FORWARD_LOCATION_MOD_PARAM, -1.f, 1.f, 0.f,  "Location Modulation", "");

        
        configInput(IN_INPUT, "Audio");
        configInput(T_CLOCK_INPUT, "Clock");
        configInput(LEN_INPUT, "Length");
        configInput(FEEDB_INPUT, "Feedback");
        configInput(REV_LOC_INPUT, "Reverse Distance");
        configInput(FORW_LOC_INPUT, "Forward Distance");

        configOutput(REV_OUT_OUTPUT, "Reverse");
        configOutput(FORW_OUT_OUTPUT, "Forward");
        //configOutput(MIX_OUT_OUTPUT, "Mix");
        //configOutput(CLOCK_OUTPUT, "Clock Out");

        
    }    

    void onReset() override {

    }


    void onSampleRateChange() override {

        
    }

    ~Taps() {
        
    }
    
    //**************************************************
    //***  MOST FREQUENT ELEMENT HELPER UTILITY  *******
    //**************************************************
    //from https://www.geeksforgeeks.org/frequent-element-array/ 
    int mostFrequent(uint32_t * arr, uint32_t arr_size)
    {
        uint32_t maxcount = 0;
        uint32_t element_having_max_freq=0;
        for (uint32_t i = 0; i < arr_size; i++) 
        {
            uint32_t count = 0;
            for (uint32_t j = 0; j < arr_size; j++) 
            {
                if (arr[i] == arr[j])
                {
                    count++;
                }
            }
  
            if (count > maxcount) 
            {
                maxcount = count;
                element_having_max_freq = arr[i];
            }
        }
        return element_having_max_freq;
    }    

    //***********************************
    //***  handle_clock_input()  *******
    //***********************************

    //*** Handles sync on rising edge 
    //*** Sync LED

    void handle_clock_input()
    {
        if (inputs[T_CLOCK_INPUT].isConnected()) 
        {

            //buffer_length_param
            clock_sample_counter += clock_count_delta;
            blink_sample_counter += 2;
            //***  GET CLOCK STATE  ***
            
            if(inputs[T_CLOCK_INPUT].getVoltage() >= 1.7f)
            {
                clock_input_state = true;
            }else
            {
                clock_input_state = false;
            }
            
            //***  CATCH RISING EDGE  ***
            if(clock_input_state != last_clock_state)
            {
                last_clock_state = clock_input_state;

                if (clock_input_state == true)
                {
                    if(clock_sample_counter < MIN_LENGTH_SETTING)
                    {
                        clock_sample_counter = MIN_LENGTH_SETTING + 1;
                    }
                    
                    if(clock_sample_counter > MIN_LENGTH_SETTING)
                    {
                        if(clock_sample_counter > BUFFER_SIZE)
                        {
                            clock_sample_counter = BUFFER_SIZE;
                        }


                        clock_input_array[clk_array_index] = (static_cast <uint32_t>(clock_sample_counter)) & ~0x3;
                        clk_array_index++;
                        if(clk_array_index >= CLOCK_INPUT_ARRAY_SIZE)
                        {
                            clk_array_index = 0;
                        }
                      
                        clock_sample_counter =  mostFrequent(clock_input_array, CLOCK_INPUT_ARRAY_SIZE);
                        if((clock_sample_counter > MIN_LENGTH_SETTING))
                        {
                            target_buffer_length = static_cast <uint32_t>(clock_sample_counter);
                            target_blink_length = blink_sample_counter;
                            //DEBUG("target length in samples = %d", target_buffer_length);
                            clock_led_brightness = 1.f;
                            
                        }

                        
                    }
                    clock_sample_counter = 0;
                    blink_sample_counter = 0;

                    
                    
                }
                
            }
            clock_led_brightness = (target_blink_length - blink_sample_counter) / (float) (target_blink_length);

            if(clock_led_brightness < 0.f)
            {
                clock_led_brightness = 0.f;
            }
            if(clock_led_brightness > 1.f)
            {
                clock_led_brightness = 1.f;
            }

            lights[CLOCK_LED_LIGHT].setBrightness(clock_led_brightness);
        }else
        {
            lights[CLOCK_LED_LIGHT].setBrightness(0.f);
        }
    }


    //*************************
    //***  check_params()  ****
    //*************************

    //Handles all parameter inputs  
    void check_params(float sampleRate)
    {

    
        buffer_length_param        = params[BUFFER_LENGTH_PARAM].getValue();
        buffer_feedback_param      = params[BUFFER_FEEDBACK_PARAM].getValue(); 
        reverse_location_param     = params[REVERSE_LOCATION_PARAM].getValue(); 
        forward_location_param     = params[FORWARD_LOCATION_PARAM].getValue();

        buffer_length_mod          = params[BUFFER_LENGTH_MOD_PARAM].getValue();
        buffer_feedback_mod        = params[BUFFER_FEEDBACK_MOD_PARAM].getValue();
        reverse_location_mod       = params[REVERSE_LOCATION_MOD_PARAM].getValue();
        forward_location_mod       = params[FORWARD_LOCATION_MOD_PARAM].getValue();

        handle_clock_input();
        
        //*************************
        //****  FEEDBACK  *********
        //*************************

        float feedback_mod_addition = (0.1 * inputs[FEEDB_INPUT].getVoltage())  * buffer_feedback_mod;  //SCALE BY MOD
        
        buffer_feedback_param = buffer_feedback_param + feedback_mod_addition;

        if(buffer_feedback_param > 1)
        {
            buffer_feedback_param = 1;
        }else if (buffer_feedback_param < 0)
        {
            buffer_feedback_param = 0;
        }

        if(old_feedback != buffer_feedback_param)
        {
           old_feedback = buffer_feedback_param;
            //DEBUG("%f", buffer_feedback_param);

        }

        //*************************
        //****  BUFFER LENGTH  ****
        //*************************

        float len_mod_addition = (inputs[LEN_INPUT].getVoltage()  * buffer_length_mod);  //SCALE BY MOD
        
        buffer_length_param = buffer_length_param + len_mod_addition;

        if(buffer_length_param > 10)
        {
            buffer_length_param = 10;
        }else if (buffer_length_param < 0.5)
        {
            buffer_length_param = 0.5;
        }


        uint32_t new_buffer_length = (int) sampleRate * buffer_length_param;
        
        
        if( new_buffer_length != knob_set_buffer_length)
        {
            knob_set_buffer_length = new_buffer_length;

            buffer_length_divider = static_cast <int> (buffer_length_param);
            
            //***  1x CLOCK INPUT  ***
            switch(buffer_length_divider)
            {
                case 0:
                    clock_count_delta = 2.f / 6.f;
                break;
                case 1:
                    clock_count_delta = 2.f / 5.f;
                break;
                case 2:
                    clock_count_delta = 2.f / 4.f;
                break;
                case 3:
                    clock_count_delta = 2.f / 3.f;
                break;
                case 4:
                    clock_count_delta = 2.f / 2.f;
                break;
                case 5:
                    clock_count_delta = 2.f;
                break;
                case 6:
                    clock_count_delta = 2.f * 2.f;
                break;
                case 7:
                    clock_count_delta = 2.f * 3.f;
                break;
                case 8:
                    clock_count_delta = 2.f * 4.f;
                break;
                case 9:
                    clock_count_delta = 2.f * 5.f;
                break;
                case 10:
                    clock_count_delta = 2.f * 6.f;
                break;
            }
           
            //DEBUG("divider %d, %f", buffer_length_divider, clock_count_delta);
            if(!inputs[T_CLOCK_INPUT].isConnected())
            {
                
                target_buffer_length = new_buffer_length;
            }
        }

        //*************************
        //****  ON SAMPLERATE  ****
        //*************************

        if(!isNear(last_sample_rate, sampleRate))
        {
            last_sample_rate = sampleRate;
            calculated_fade_count = sampleRate * 0.2;
            led_dim_rate          = sampleRate * 0.5;

        }
        

        //******************************
        //****  FORWARD DISTANCE  ******
        //******************************
        float forw_mod_addition = (0.1 * inputs[FORW_LOC_INPUT].getVoltage())  * forward_location_mod;  //SCALE BY MOD
        
        forward_location_param = forward_location_param + forw_mod_addition;

        if(forward_location_param > 1)
        {
            forward_location_param = 1;
        }else if (forward_location_param < 0)
        {
            forward_location_param = 0;
        }


        if(!isNear(target_forw_delay_distance, forward_location_param))
        {
            target_forw_delay_distance = forward_location_param;

            int distance = (target_buffer_length - 100) * target_forw_delay_distance;

            int target_index = inptr - distance;

            if(target_index < 0)
            {
                target_index = target_buffer_length + target_index;
            }
            forw_outptr = target_index;
        }
        
        //******************************
        //****  REVERSE DISTANCE  ******
        //******************************
        
        float rev_mod_addition = (0.1 * inputs[REV_LOC_INPUT].getVoltage())  * reverse_location_mod;  //SCALE BY MOD
        
        reverse_location_param = reverse_location_param + rev_mod_addition;

        if(reverse_location_param > 1)
        {
            reverse_location_param = 1;
        }else if (reverse_location_param < 0)
        {
            reverse_location_param = 0;
        }
        
        if(!isNear(target_rev_delay_distance, reverse_location_param))
        {
            target_rev_delay_distance = reverse_location_param;

            int distance = (target_buffer_length - 100) * target_rev_delay_distance;

            int target_index = inptr - distance;

            if(target_index < 0)
            {
                target_index = target_buffer_length + target_index;
            }

            rev_outptr = target_index;

        }



    }
    //**********************************************
    //**********************************************
    //**********************************************
    
    //**********************************************
    //**********************************************
    //**********************************************

    void process(const ProcessArgs& args) override 
    {
        
        float rev_wet = 0.f;
        float forw_wet;

        //*** SOFT START  ***
        if(go_ahead_and_do_stuff == false)
        {
            startup_delay_counter++;
            if(startup_delay_counter > args.sampleRate * 0.5f)
            {
                go_ahead_and_do_stuff = true;
            }
            return;
        }

        //*** CHECK PARAM INPUTS  ***

        check_params(args.sampleRate);

        //***  HANDLE INPUT AUDIO ***
        float in = inputs[IN_INPUT].getVoltageSum();
        float dry = in + lastWet * buffer_feedback_param;
        
        int trail_look_ahead = (inptr + fade_buffer_lookahead) % target_buffer_length;

        trail_buffer[trailptr] = delay_buffer[trail_look_ahead];
        delay_buffer[inptr] = dry;
        forw_wet = delay_buffer[forw_outptr];
        

        //******************************
        //***  FADE BETWEEN BUFFERS  ***
        //******************************
        
        if(ptr_state == PTR_STATE_FADE_IN)
        {
            //***  GET POINTER INTO FADE BUFFER
            offset_pointer = trailptr - ptr_trail_distance - 2;  //This little minus 2 was hand-tuned, this is a real click danger-zone (because previously it had to be 5 which was super weird)
            
            if(offset_pointer < 0)  // loop-around the buffer end 
            {
                offset_pointer = TRAIL_BUFFER_SIZE + offset_pointer;
            }
            float trail_sample     = trail_buffer[offset_pointer];
            float non_trail_sample = delay_buffer[rev_outptr];
            
            //***  ACTUAL FADE  ***
            
            //***  START FADE IN WHEN REV READ PTR JUST PASSES THE WRITE PTR
            if(ptr_trail_distance > (uint32_t)fade_buffer_lookahead + 10)
            {
                
                //***  START AT 1.0 OF FADE BUFFER AND FADE FULLY INTO MAIN BUFFER AT 0.0
                fade_amount  = (calculated_fade_count - fade_count) / calculated_fade_count;
                
                //***  AT 0.0, FADE BETWEEN THE BUFFERS IS FINISHED
                if((fade_amount < 0.f))
                {
                    rev_wet = non_trail_sample;
                    ptr_state = PTR_STATE_FADE_FINISHED;
                    
                }else
                {
                    rev_wet = crossfade( non_trail_sample, trail_sample, fade_amount);
                }

                //FADE COUNT INCREASES BY 2 BECAUSE OF OPPOSITE POINTER MOVEMENT
                fade_count+=2;
            }
            //***  BEFORE FADE STARTS USE 100% FADE BUFFERED SAMPLES  ***
            else
            {
                rev_wet = trail_sample;
                fade_count = 0;
                fade_amount = 0.f;
            }


            ptr_trail_distance += 2;

        //*************************
        //***  WHEN NOT FADING  ***
        //*************************

        }else
        {
            ptr_trail_distance = 0;
            rev_wet  = delay_buffer[rev_outptr];
            
        }
        





        //****************************************
        //***  ADVANCE BUFFER WRITE POINTERS
        //****************************************


        trailptr++;
        inptr++;
        forw_outptr++;  // and standard delay pointer

        //*** LOOP TRAIL POINTER
        if (trailptr >= (int)TRAIL_BUFFER_SIZE)
        {
            trailptr = 0;
        }

        //***  LOOP IN POINTER 
        if(inptr >= (int)target_buffer_length)
        {
            inptr = 0;
            
            buff_led_brightness = led_dim_rate;
        }
        

        //***  LOOP STANDARD DELAY POINTER  
        if (forw_outptr >= (int)target_buffer_length)
        {
            forw_outptr = 0;
        }

        //****************************************
        //***  CHECK FOR REV_OUTPTR CROSSING INPTR
        //****************************************

        
        int distance;

        //  how to find the distance between them? 

        //     INPTR  is always heading towards BUFFER_END  
        // and OUTPTR is always heading towards BUFFER_START

        // SO:
        //*** CASE 1:   They're heading towards each other across the boundary 
        if(inptr > rev_outptr)
        {
            int top_distance = target_buffer_length - inptr;        //top distance
            int bot_distance  = rev_outptr;                            //bottom distance
            distance = top_distance + bot_distance;                    //total distance is the sum 
            
        }
        //*** CASE 2:   They're heading right towards each other
        else
        {
            distance = rev_outptr - inptr;                            //total distance is the difference
        }

        //***  IF WE'RE NOT ALREADY FADING  
        if(ptr_state == PTR_STATE_FADE_FINISHED)  
        {
            //***  AND THE REVERSE POINTER IS ABOUT TO HIT THE WRITE POINTER
            if(distance < fade_buffer_lookahead)
            {
                //***  THEN SWITCH BUFFERS  
                ptr_state = PTR_STATE_FADE_IN;
                ptr_trail_distance = 0;

                //AND INDICATE (SO WE HAVE A CLUE FOR ANY RELATED SONIC ISSUES)
                rev_led_brightness = led_dim_rate;
                
            }
        }



     
        


        //****************************************
        //***  ADVANCE REVERSE POINTER  
        //****************************************

        rev_outptr--;

        
        
        if((rev_outptr < 0) ||  (rev_outptr >= (int)target_buffer_length))
        {
            rev_outptr = (int)target_buffer_length - 1;
        }
        
        
        //****************************************
        //***  SET OUTPUTS  
        //****************************************
         outputs[REV_OUT_OUTPUT].setVoltage(rev_wet);
         outputs[FORW_OUT_OUTPUT].setVoltage(forw_wet);
         lastWet = rev_wet;

        buff_led_brightness--;
        if(buff_led_brightness < 0)
        {
            buff_led_brightness = 0;
        }
        float buff_led = buff_led_brightness / led_dim_rate;
        lights[BUFF_LED_LIGHT].setBrightness(buff_led);

        rev_led_brightness--;
        if(buff_led_brightness < 0)
        {
            buff_led_brightness = 0;
        }
        float rev_led = rev_led_brightness / led_dim_rate;

        lights[REV_LED_LIGHT].setBrightness(rev_led);

    }
};



//***********************************************
//***  ModuleWidget  ****************************
//***********************************************

#define BOTTOM_OFFSET  3

#define LEFT_JACKS_X   6.765

#define KNOB_X      31.213

#define LEN_Y       18.904
#define FEEDB_Y     39.92
#define REV_Y       60.935
#define FORW_Y      81.951

#define IN_Y        112.12 

#define REV_OUT_X   33.802
#define REV_OUT_Y   99.164 
#define FORW_OUT_X  33.765
#define CLOCK_X     20.265
#define CLOCK_Y     112.12

#define INPUT_Y_OFFSET   3.881 

#define LED_X           37.441
#define BUFF_LED_Y      11.436
#define REV_LED_Y       53.594
#define CLOCK_LED_X     CLOCK_X + 4
#define CLOCK_LED_Y     CLOCK_Y - 5

#define TRIMPOT_X           18.497 - 1
#define TRIMPOT_Y_OFFSET  2
#define KNOB_Y_OFFSET   -3

#define TRIM1_Y  23.668
#define TRIM2_Y  44.608
#define TRIM3_Y  65.547
#define TRIM4_Y  86.487

#define MOD_JACK_1_Y    27.723
#define MOD_JACK_2_Y    48.739
#define MOD_JACK_3_Y    69.755
#define MOD_JACK_4_Y    90.418


struct Taps_Widget : ModuleWidget {
    Taps_Widget(Taps* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Taps.svg")));

        
        //***  PARAMS
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(KNOB_X, LEN_Y   )), module, Taps::BUFFER_LENGTH_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(KNOB_X, FEEDB_Y )), module, Taps::BUFFER_FEEDBACK_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(KNOB_X, REV_Y   )), module, Taps::REVERSE_LOCATION_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(KNOB_X, FORW_Y  )), module, Taps::FORWARD_LOCATION_PARAM));

        //***  TRIMPOTS
        addParam(createParamCentered<YH_Red_Trimpot>(mm2px(Vec(TRIMPOT_X, TRIM1_Y)), module, Taps::BUFFER_LENGTH_MOD_PARAM));
        addParam(createParamCentered<YH_Red_Trimpot>(mm2px(Vec(TRIMPOT_X, TRIM2_Y)), module, Taps::BUFFER_FEEDBACK_MOD_PARAM));
        addParam(createParamCentered<YH_Red_Trimpot>(mm2px(Vec(TRIMPOT_X, TRIM3_Y)), module, Taps::REVERSE_LOCATION_MOD_PARAM));
        addParam(createParamCentered<YH_Red_Trimpot>(mm2px(Vec(TRIMPOT_X, TRIM4_Y)), module, Taps::FORWARD_LOCATION_MOD_PARAM));

        //***  INPUTS
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(LEFT_JACKS_X, IN_Y)), module, Taps::IN_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(LEFT_JACKS_X, MOD_JACK_1_Y)), module, Taps::LEN_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(LEFT_JACKS_X, MOD_JACK_2_Y)), module, Taps::FEEDB_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(LEFT_JACKS_X, MOD_JACK_3_Y)), module, Taps::REV_LOC_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(LEFT_JACKS_X, MOD_JACK_4_Y)), module, Taps::FORW_LOC_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CLOCK_X, CLOCK_Y)), module, Taps::T_CLOCK_INPUT));

        //***  OUTS
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(REV_OUT_X, REV_OUT_Y)), module, Taps::REV_OUT_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(FORW_OUT_X, IN_Y)), module, Taps::FORW_OUT_OUTPUT));

        //***  LEDS 
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(LED_X, BUFF_LED_Y)), module, Taps::BUFF_LED_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(LED_X, REV_LED_Y)), module, Taps::REV_LED_LIGHT));
        addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(CLOCK_LED_X, CLOCK_LED_Y)), module, Taps::CLOCK_LED_LIGHT));
    }
};


Model* modelTaps = createModel<Taps, Taps_Widget>("Taps");
