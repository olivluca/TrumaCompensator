#include "compensator.hpp"

#define PULSE_DURATION 40000 //40ms
#define MIN_PULSE_INTERVAL PULSE_DURATION*2
#define ONESECOND 1000000

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

void TCompensator::CalcTestDutyCycle() {
  if (Ftest_pulse_duty_cycle<1) {
    Ftest_pulse_duty_cycle=1;
  }
  if (Ftest_pulse_duty_cycle>100) {
    Ftest_pulse_duty_cycle=100;
  }
  Ftest_pulse_duty_on=PWM_PERIOD * Ftest_pulse_duty_cycle / 100;
  if (Fserial_debug) {
    Serial.printf("Test pulse duty on %d over %d\r\n",Ftest_pulse_duty_on, PWM_PERIOD);
  }
}

void TCompensator::ResetPwm()
{
  FPwmTime=10000; //big value, just to bootstrap the cycle (usually for the first few cycles the duty cycle is 100%)
  FNextPwmTime=FPwmTime;
  FPwmOnTime=FPwmTime;
  FDutyCycle=100;
}

void TCompensator::CheckPwm()
{
  if (FPwmTime==0) {
    ResetPwm();
    if (Fserial_debug) {
      Serial.println("**** FPwmTime 0");
    }
    return;
  }
  unsigned long percent=FPwmOnTime * 100 / FPwmTime;
  if (percent<10) {
    FPwmTime=PWM_PERIOD;
    FPwmOnTime = FPwmTime * 10 / 100;
    FDutyCycle=10;
    if (Fserial_debug) {
      Serial.printf("**** Pwm %d%%, clamped to 10%%\r\n",percent);
    }
    return;
  }
  if (percent>95) {
    ResetPwm();
    if (Fserial_debug) {
      Serial.printf("**** Pwm %d%%, set to 100%%\r\n",percent);
    }
    return;
  }
  FDutyCycle=percent;
  if (Fserial_debug) {
    Serial.printf("pwm %d on %d  %d%%\r\n",FPwmTime, FPwmOnTime, FPwmOnTime * 100 / FPwmTime);
  }
}

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
        FPulseCount=0;
        ResetPwm();
        Finput_pulse_start=now;
        Foutput_pulse_start=now;
        Foutput_pulse_interval=10 * ONESECOND; //unknown, start with ten seconds
        Finput_pulse_interval=2 * ONESECOND; //unknown, start with 2 seconds
        Fidle=false;
      } else {
        if (now-Finput_pulse_start>=MIN_PULSE_INTERVAL) { 
          //real pulse for the pump, not part of the pwm cycle
          //if less than two sub-pulses counted the duty cycle is 100%
          if (FPulseCount<2) {
            ResetPwm();
          }
          FPulseCount=0;
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
        } else {
          //not a real pulse, part of the pwm cycle
          if (FPulseCount==2) { //measure the pwm time on the second pulse
            FNextPwmTime=now-FRisingEdgeTime;
          }
        }
      }
      FRisingEdgeTime=now;
      FPulseCount++;
    } else { //falling edge
      if (FPulseCount==2) {  //change the pwm parameters with the second pulse
        FPwmOnTime=now-FRisingEdgeTime;
        FPwmTime=FNextPwmTime;
        CheckPwm();
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
    if (diff<PULSE_DURATION && diff % FPwmTime <= FPwmOnTime) {
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
    if (diff<PULSE_DURATION && diff % PWM_PERIOD <= Ftest_pulse_duty_on) {
      digitalWrite(Ftestpin, Ftest_pulse_on);
    } else {
      digitalWrite(Ftestpin, !Ftest_pulse_on);
    }
  }
}
