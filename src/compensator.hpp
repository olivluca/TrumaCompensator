#pragma once
#include <Arduino.h>

class TCompensator {
    private:
        //input pin
        int Finput;
        //output pin
        int Foutput;
        //test pulses 
        int Ftestpin;

        //micros when the last incoming pulse started
        unsigned long Finput_pulse_start;
        //interval between the two last input pulses
        unsigned long Finput_pulse_interval;
        //micros when the last output pulse was generated
        unsigned long Foutput_pulse_start;
        //compensated interval between two outpur pulses
        unsigned long Foutput_pulse_interval;
        //measured input frequency * 1000
        unsigned long Finput_frequency;
        //calculated outpur frequency * 1000
        unsigned long Foutput_frequency;

        //micros when the last test pulse started
        unsigned long Ftest_pulse_start;
        //interval of the test pulses
        unsigned long Ftest_pulse_interval=100*1000;
        uint8_t Ftest_pulse_on=LOW; //to mimic the inversion by the optocoupler
        //waiting for the first input pulse
        boolean Fidle;

        //stores the last input status (to detect changes)
        int Foldin;

        //percentage x 10 of output frequency reduction
        int Fcompensation; //output frequency = input frequency

        boolean Fserial_debug;
     
    public:
        TCompensator(int input, int output, int testpin);  
        void Loop(); 
        void SetCompensation(float AValue) { Fcompensation = trunc(AValue*10); }
        void SetTestPulseInterval(int Avalue) {
            if (Avalue>0) {
                  Ftest_pulse_interval = Avalue;
                  Ftest_pulse_on = HIGH;
                } else {
                  Ftest_pulse_interval = -Avalue;
                  Ftest_pulse_on = LOW;
                }
        }
        void SetSerialDebug(boolean Avalue) {Fserial_debug = Avalue; } 
        unsigned long InputFrequency() { return Finput_frequency; }
        unsigned long OutputFrequency() { return Foutput_frequency; }
};