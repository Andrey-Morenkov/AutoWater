#pragma once

extern bool IsDev;


//-------------------------------------------------------------SENSOR--------------------------------------------------------------
class Sensor
{
  private:
  
      signed char pin;         // пин подключения
      int         val;         // значение сенсора 
      
  public:
      
      Sensor( signed char _pin = -1)
      {
          pin = _pin;
          val = -1;
      }

      Sensor (const Sensor& _sensor )
      {
          pin = _sensor.getPin();
          val = _sensor.getVal();        
      }   

      void setPin(signed char _pin)
      {
          pin = _pin;
      }

      signed char getPin () const
      {
          return pin;
      }

      int getVal() const
      {
          return val;
      }

      void setVal(int _val)
      {
        val = _val;
      }

      int ask()
      {
        if (!IsDev)
        {
          val = analogRead(pin);
          Serial.print("   Sensor.ask() Value: ");
          Serial.println(val);
          return val;
        }
        else
        {
          Serial.println("  DEV MODE");
          Serial.print("   Sensor.ask() Value: ");
          Serial.println(val);
          return val;
        }
          
      }
};  
//------------------------------------------------------------/SENSOR--------------------------------------------------------------
