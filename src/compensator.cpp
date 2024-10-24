#include "compensator.hpp"

TCompensator::TCompensator(int input, int output, int testpin)
{
    Finput=input;
    Foutput=output;
    Ftestpin=testpin;

    pinMode(BUILTIN_LED, OUTPUT);
    pinMode(Finput, INPUT_PULLUP);
    pinMode(Foutput, OUTPUT);
    pinMode(Ftestpin,OUTPUT);
    digitalWrite(Ftestpin,HIGH);
    Fidle=true;
    Fpulse_duration=200*1000; //before first measurement use 200ms
    Ftest_pulse_start=micros();
    Finput_frequency=0;
    Foutput_frequency=0;
    Fserial_debug=false;
    Foldin=HIGH;
    Fcompensation=0;
}

#define ONESECOND 1000000
void TCompensator::Loop()
{
  unsigned long now=micros();
  int in = digitalRead(Finput);

  if (in!=Foldin) {
    if (in==LOW) { //new pulse
      if (Fidle) {
        if (Fserial_debug) {
          Serial.println("initial pulse");
        }
        Foutput_pulse_start=now;
        Foutput_pulse_interval=10*1000*1000; //unknown, start with ten seconds
        Finput_pulse_interval=2 * 1000 * 1000; //unknown, start with 2 seconds
        Fidle=false;
      } else {
        Finput_pulse_interval=now-Finput_pulse_start;
        //clamp value
        if (Finput_pulse_interval>=5000) {  //200Hz, I don't think the pump ever be that fast
          //frequencies in Hz*1000, maybe it's excessive 
          Finput_frequency = ONESECOND * 1000 / Finput_pulse_interval;
          Foutput_frequency = Finput_frequency - Finput_frequency * Fcompensation / 1000; //compensation is % x 10
          if (Foutput_frequency>0) {
            //convert frequency back to interval
            Foutput_pulse_interval=ONESECOND * 1000/Foutput_frequency;
            if (Fserial_debug) { 
              Serial.printf("in interval %d us, freq %d.%03d Hz, out freq %d.%03d Hz, interval %d us, duration %d us\r\n",
                Finput_pulse_interval,Finput_frequency /1000, Finput_frequency % 1000, Foutput_frequency/1000, Foutput_frequency % 1000, Foutput_pulse_interval, Fpulse_duration);
            }
          } else {
            if (Fserial_debug) {
              Serial.printf("output_freq %d less than 0\r\n",Foutput_frequency);
            }
          }
        } else {
          if (Fserial_debug) {
            Serial.printf("input_pulse_interval %d too short \r\n",Finput_pulse_interval);
          }
        }
      }
      Finput_pulse_start=now;
    } else { //pulse end
      unsigned long new_duration=now-Finput_pulse_start;
      //clamp pulse duration
      if (new_duration<20000) { //20ms
        new_duration=20000;
      }
      if (new_duration>500000) { //500ms
        new_duration=500000;
      }
      Fpulse_duration=new_duration;
    }
    Foldin=in;
  }

  //no more pulses
  if (!Fidle && now - Finput_pulse_start > 2 * Finput_pulse_interval) {
    Fidle=true;
    Finput_frequency=0;
    Foutput_frequency=0;
    if (Fserial_debug) {
      Serial.println("no more pulses");
    }
  }
  if (Fidle) {
    digitalWrite(Foutput,LOW);
    digitalWrite(BUILTIN_LED, LOW);
  } else {
    if (now-Foutput_pulse_start>=Foutput_pulse_interval) {
      Foutput_pulse_start=now;
    }
    if (now-Foutput_pulse_start<Fpulse_duration) {
      digitalWrite(Foutput,HIGH);
      digitalWrite(BUILTIN_LED, HIGH);
    } else {
      digitalWrite(Foutput,LOW);
      digitalWrite(BUILTIN_LED, LOW);
    }
  }

  //generate test pulses
  if (now-Ftest_pulse_start>=Ftest_pulse_interval) {
    Ftest_pulse_start=now;
  }
  if (now-Ftest_pulse_start<5*1000) {
    digitalWrite(Ftestpin, LOW);
  } else {
    digitalWrite(Ftestpin,HIGH);
  }
}
