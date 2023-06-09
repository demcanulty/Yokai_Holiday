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


//***  Note:  This code not cleaned up, for the cleaned up version see Taps.cpp
//***         This was just for testing during the prototype phase, as part of my first vcv module development



#include "plugin.hpp"
#include "Taps_Proto.hpp"



struct Taps_Proto : Module {
    enum ParamId {
        BUFFER_LENGTH_PARAM,
        BUFFER_FEEDBACK_PARAM,
        REVERSE_LOCATION_PARAM,
        FORWARD_LOCATION_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        IN_INPUT,
        T_CLOCK_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        REV_OUT_OUTPUT,
        FORW_OUT_OUTPUT,
        MIX_OUT_OUTPUT,
        //CLOCK_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LOOP_LED_LIGHT,
        LOOP_LED_LIGHT_2,
        LIGHTS_LEN
    };

    enum PTR_STATE{
        PTR_STATE_FADE_FINISHED,
        PTR_STATE_FADE_IN,
        PTR_STATE_FADE_OUT
    };
    


    //***  TAPS ORIGINAL  ****
    #define PASS   set_led = !set_led
    #define USER_BUFFER_LENGTH_DEFAULT  5.f 
    #define BUFFER_TIME_LENGTH          10
    #define TRAIL_BUFFER_SIZE  300000
    static const int  BUFFER_SIZE  = 96000 * BUFFER_TIME_LENGTH;
   
    float delay_buffer[BUFFER_SIZE];
    float trail_buffer[TRAIL_BUFFER_SIZE];

    float lastWet = 0.f;
    bool blue_led = 0;
    bool set_led = 0;
    bool crossed_barrier = 0;


    int inptr = 0;
    int rev_outptr = 0;
    int forw_outptr = BUFFER_SIZE / 4;
    int trailptr= 48000;
    uint32_t target_buffer_length = 48000 * 5;
    uint32_t knob_set_buffer_length;
    float target_forw_delay_distance = 2;
    float target_rev_delay_distance = 2;
    float buffer_length_param, buffer_feedback_param, reverse_location_param, forward_location_param;
    uint32_t ptr_trail_distance = 0;

    uint32_t fade_count = 0;
    


    enum PTR_STATE ptr_state = PTR_STATE_FADE_FINISHED;
    enum PTR_STATE last_ptr_state = PTR_STATE_FADE_FINISHED;
    int offset_pointer = 0;
    const int fade_buffer_lookahead = 1000;
    
    float fade_amount = 0;

    float calculated_fade_count = 7000.f;
    float last_sample_rate = 0;

    bool clock_input_state = 0;
    bool last_clock_state = 0;
    uint32_t clock_sample_counter = 0;
    uint32_t general_counter = 0;
    bool go_ahead_and_do_stuff = 0;
    #define MIN_LENGTH_SETTING  (calculated_fade_count + (float)fade_buffer_lookahead + 100.f)

    #define CLOCK_INPUT_ARRAY_SIZE   5
    uint32_t clock_input_array[CLOCK_INPUT_ARRAY_SIZE];
    uint8_t  clk_array_index = 0;
    uint32_t last_max_count = 0;

    Taps_Proto() 
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(BUFFER_LENGTH_PARAM,    0.5f, 10.f, USER_BUFFER_LENGTH_DEFAULT, "Length", "s");
        configParam(BUFFER_FEEDBACK_PARAM,   0.f,  1.f, 0.4f, "Feedback", "%");
        configParam(REVERSE_LOCATION_PARAM, 0.1f,  1.f, 0.5f, "Location", "");
        configParam(FORWARD_LOCATION_PARAM, 0.1f,  1.f, 0.5f,  "Location", "");
        configInput(IN_INPUT, "Audio");
        configInput(T_CLOCK_INPUT, "Clock");
        configOutput(REV_OUT_OUTPUT, "Reverse");
        configOutput(FORW_OUT_OUTPUT, "Forward");
        configOutput(MIX_OUT_OUTPUT, "Mix");
        //configOutput(CLOCK_OUTPUT, "Clock Out");

        
    }    

    void onReset() override {

    }


    void onSampleRateChange() override {

        
    }

    ~Taps_Proto() {
        
    }
    


    int mostFrequent(uint32_t * arr, uint32_t arr_size)
    {
        uint32_t maxcount = 0;
        uint32_t element_having_max_freq = 0;
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
        last_max_count = maxcount;
        return element_having_max_freq;
    }    


    void handle_clock_input_buffer_change()
    {
        if (inputs[T_CLOCK_INPUT].isConnected()) 
        {
            clock_sample_counter += 2;
            
            //***  GET CLOCK STATE  ***
            //if(clock_output > 1.7f)
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
                    if(clock_sample_counter > MIN_LENGTH_SETTING)
                    {
                        
                        clock_input_array[clk_array_index] = clock_sample_counter & ~0x3;
                        clk_array_index++;
                        if(clk_array_index >= CLOCK_INPUT_ARRAY_SIZE)
                        {
                            clk_array_index = 0;
                        }
                        /*
                        uint32_t this_sum, i;
                        this_sum = 0;
                        for(i=0; i<CLOCK_INPUT_ARRAY_SIZE; i++)
                        {
                            this_sum += clock_input_array[i];
                        }

                        //
                        clock_sample_counter = this_sum / CLOCK_INPUT_ARRAY_SIZE;
                        

                        clock_sample_counter = (clock_sample_counter + target_buffer_length) / 2;
                        */
                        //DEBUG("clock_sample_counter %d", clock_sample_counter);
                        clock_sample_counter =  mostFrequent(clock_input_array, CLOCK_INPUT_ARRAY_SIZE);
                        if((clock_sample_counter > MIN_LENGTH_SETTING))
                        {
                            target_buffer_length = clock_sample_counter;
                            //DEBUG("target length in samples = %d", target_buffer_length);
                        }

                        
                    }
                    clock_sample_counter = 0;
                }
                
            }
            
        }
    }

    void check_params(float sampleRate)
    {

    
        buffer_length_param     = params[BUFFER_LENGTH_PARAM].getValue();
        buffer_feedback_param     = params[BUFFER_FEEDBACK_PARAM].getValue(); 
        reverse_location_param     = params[REVERSE_LOCATION_PARAM].getValue(); 
        forward_location_param     = params[FORWARD_LOCATION_PARAM].getValue();

        
        uint32_t new_buffer_length = (int) sampleRate * buffer_length_param;
        
        
        handle_clock_input_buffer_change();
        
        if( new_buffer_length != knob_set_buffer_length)
        {
            knob_set_buffer_length = new_buffer_length;

            if(!inputs[T_CLOCK_INPUT].isConnected())
            {
                target_buffer_length = new_buffer_length;
            }
        }
        if(!isNear(last_sample_rate, sampleRate))
        {
            last_sample_rate = sampleRate;
            calculated_fade_count = sampleRate * 0.2;
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
            //forw_outptr = target_index;

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

        if(go_ahead_and_do_stuff == false)
        {
            general_counter++;
            if(general_counter > args.sampleRate * 0.5f)
            {
                go_ahead_and_do_stuff = true;
            }
            return;
        }



        check_params(args.sampleRate);

        float in = inputs[IN_INPUT].getVoltageSum();
        float dry = in + lastWet * buffer_feedback_param;
        
        int trail_look_ahead = (inptr + fade_buffer_lookahead) % target_buffer_length;

        trail_buffer[trailptr] = delay_buffer[trail_look_ahead];
        delay_buffer[inptr] = dry;
        forw_wet = delay_buffer[forw_outptr];
        
        

        //******************************************************************************************
        //******************************************************************************************
        //******************************************************************************************


        //******************************
        //***  FADE BETWEEN BUFFERS  ***
        //******************************
        
        if(ptr_state == PTR_STATE_FADE_IN)
        {

            //DEBUGGING  
            
            if(ptr_trail_distance == 0)
            {
                PASS;
            }
            

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

        //******************************************************************************************
        //******************************************************************************************
        //******************************************************************************************

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
            lights[LOOP_LED_LIGHT].setBrightness(1.f * crossed_barrier);
            crossed_barrier = !crossed_barrier;
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
                blue_led = !blue_led;
                lights[LOOP_LED_LIGHT_2].setBrightness(1.f * crossed_barrier);
            }
        }



        //***  DEBUGGING 
        
        if(last_ptr_state != ptr_state)
        {

            last_ptr_state = ptr_state;
            set_led = 0;
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
         outputs[MIX_OUT_OUTPUT].setVoltage((rev_wet + forw_wet) / 2.f);
         lastWet = rev_wet;


    }
};


struct Taps_ProtoWidget : ModuleWidget {
    Taps_ProtoWidget(Taps_Proto* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Taps_Proto.svg")));

        

        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(12.7, 28.4)), module, Taps_Proto::BUFFER_LENGTH_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(38.1, 28.4)), module, Taps_Proto::BUFFER_FEEDBACK_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(25.43, 60.645)), module, Taps_Proto::REVERSE_LOCATION_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(25.4, 89.762)), module, Taps_Proto::FORWARD_LOCATION_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.16, 111)), module, Taps_Proto::IN_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.16, 90)), module, Taps_Proto::T_CLOCK_INPUT));
        //addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.16, 101)), module, Taps_Proto::CLOCK_OUTPUT));


        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(44.342, 64.435)), module, Taps_Proto::REV_OUT_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(44.342, 89.338)), module, Taps_Proto::FORW_OUT_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(44.342, 111)), module, Taps_Proto::MIX_OUT_OUTPUT));

        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(12.268, 6.7)), module, Taps_Proto::LOOP_LED_LIGHT));
        addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(38.1, 6.7)), module, Taps_Proto::LOOP_LED_LIGHT_2));
    }
};


Model* modelTaps_Proto = createModel<Taps_Proto, Taps_ProtoWidget>("Taps_Proto");
