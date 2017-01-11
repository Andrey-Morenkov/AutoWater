#pragma once

extern bool IsDev;


//-------------------------------------------------------------SENSOR--------------------------------------------------------------
class Sensor
{
  private:
  
      signed char pin;         // пин подключения
      int         val;         // значение сенсора 
	  int         crit_val;    // критическое значение
      
  public:
      
      Sensor( signed char _pin = -1 , int _crit_wal = -1)
      {
          pin = _pin;
          val = -1;
		  crit_val = _crit_wal;
      }

      Sensor (const Sensor& _sensor )
      {
          pin = _sensor.getPin();
          val = _sensor.getVal();
		  crit_val = _sensor.getCritVal();
      }   

	  void setCritVal(int _cv)
	  {
		  crit_val = _cv;
	  }

	  int getCritVal() const
	  {
		  return crit_val;
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
