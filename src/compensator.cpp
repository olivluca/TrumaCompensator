#include "compensator.hpp"

#define PULSE_DURATION 40000 //40ms
#define MIN_PULSE_INTERVAL PULSE_DURATION*2
TCompensator::TCompensator(int input, int output, int testpin)
{
    Finput=input;
    Foutput=output;
    Ftestpin=testpin;

    pinMode(BUILTIN_LED, OUTPUT);
    pinMode(Finput, INPUT_PULLUP);
    pinMode(Foutput, OUTPUT);
    pinMode(Ftestpin,OUTPUT);
    digitalWrite(Ftestpin,LOW);
    Fidle=true;
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
        Finput_pulse_start=now;
        Foutput_pulse_start=now;
        Foutput_pulse_interval=10*1000*1000; //unknown, start with ten seconds
        Finput_pulse_interval=2 * 1000 * 1000; //unknown, start with 2 seconds
        Fidle=false;
      } else if (now-Finput_pulse_start>=MIN_PULSE_INTERVAL) {
        Finput_pulse_interval=now-Finput_pulse_start;
        Finput_pulse_start=now;
        //frequencies in Hz*1000, maybe it's excessive
        Finput_frequency = ONESECOND * 1000 / Finput_pulse_interval;
        Foutput_frequency = Finput_frequency - Finput_frequency * Fcompensation / 1000; //compensation is % x 10
        if (Foutput_frequency>0) {
          //convert frequency back to interval
          Foutput_pulse_interval=ONESECOND * 1000/Foutput_frequency;
          if (Fserial_debug) {
            Serial.printf("in interval %d us, freq %d.%03d Hz, out freq %d.%03d Hz, interval %d us\r\n",
              Finput_pulse_interval,Finput_frequency /1000, Finput_frequency % 1000, Foutput_frequency/1000, Foutput_frequency % 1000, Foutput_pulse_interval);
          }
        } else {
          if (Fserial_debug) {
            Serial.printf("output_freq %d less than 0\r\n",Foutput_frequency);
          }
        }
      }
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
    unsigned long diff=now-Foutput_pulse_start;
    if (diff<PULSE_DURATION && diff % 3000 < 2400) {
      digitalWrite(Foutput,HIGH);
      digitalWrite(BUILTIN_LED, HIGH);
    } else {
      digitalWrite(Foutput,LOW);
      digitalWrite(BUILTIN_LED, LOW);
    }
  }

  //generate test pulses 
  if (Ftest_pulse_interval<100000) {
    digitalWrite(Ftestpin, !Ftest_pulse_on);
    Ftest_pulse_start=now;
  } else {
    if (now-Ftest_pulse_start>=Ftest_pulse_interval) {
      Ftest_pulse_start=now;
    }
    unsigned long diff=now-Ftest_pulse_start;
    if (diff<40*1000 && diff % 3000 < 6400) {
      digitalWrite(Ftestpin, Ftest_pulse_on);
    } else {
      digitalWrite(Ftestpin, !Ftest_pulse_on);
    }
  }
}
