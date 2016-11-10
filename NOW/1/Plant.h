#pragma once

#include "Sensor.h"

extern const int maxSensors;
extern const int alert_WS;
extern Sensor* sensor;
extern const int pumpTime;
extern const unsigned char pump;
extern const unsigned char relay_Sensors;

#define ON  LOW
#define OFF HIGH
#define WS  maxSensors-1

void LedOn();
void LedOff();



//--------------------------------------------------------------PLANT--------------------------------------------------------------
class Plant
{
  private:
  
      signed char valve;         // пин клапана
      Sensor      hygrometer;    // гигрометр
      
  public:  

      Plant (signed char _valve = -1, Sensor _hygrometer = -1)
      {
          valve = _valve;
          hygrometer = _hygrometer;
      }
      
      Plant (const Plant& _flower)
      {
          valve =      _flower.getValve();
          hygrometer = _flower.getHygrometer().getPin();
      }

      void setVal(int _val)
      {
          //char buf[64];
          //snprintf(buf, sizeof(buf), "Plant: 0x%x, hygrometer: 0x%x", this, &hygrometer);
          //Serial.println(buf);
          hygrometer.setVal(_val);
      }
      
      void setValve(signed char _valve)
      {
          valve = _valve;
      }

      void setHygrometer(const Sensor& _hygrometer)
      {
          hygrometer = _hygrometer;
      }
      
      void setParams (signed char _valve, const Sensor& _hygrometer)
      {
          valve = _valve;
          hygrometer = _hygrometer;
      }

      signed char getValve() const
      {
          return valve;
      }

      const Sensor& getHygrometer() const
      {
          return hygrometer;
      }

      int getStat()
      {
          return hygrometer.getVal();
      }
    
      void init()
      {
          pinMode(valve,OUTPUT);
      }

      void ask()
      {
          Serial.println("   Plant.ask() Asking hygrometer...");
          hygrometer.ask();
      }

      void water()
      {
          Serial.println("STARTING TO WATER... ");
          // добавить аск WS
          if ( sensor[WS].getVal() <= alert_WS )                            // если у WS меньше чем Alert то воды нет!
          {
			  bool blink = true;
              while (sensor[WS].getVal() <= alert_WS)                       
              {
                  Serial.println("NO WATER !");
				  if (blink)
				  {
					  LedOn();
				  }
				  else
				  {
					  LedOff();
				  }
				  blink = !blink;					  
                  
                  delay(2000);
                  Serial.println("ASKING FOR WATER...");
                  digitalWrite(relay_Sensors, ON);
                  delay(500);
                  sensor[WS].ask();
                  digitalWrite(relay_Sensors, OFF);
              }
              LedOff();
          }
          Serial.println("OK WATER, watering... ");    
          digitalWrite(valve,ON);
          Serial.println("VALVE ON");
          digitalWrite(pump,ON);
          Serial.println("PUMP ON");
          delay(pumpTime);
          digitalWrite(pump,OFF);
           Serial.println("PUMP OFF");
          delay(200);
          digitalWrite(valve,OFF);
          Serial.println("VALVE OFF");        
      }
                 
};
//-------------------------------------------------------------/PLANT--------------------------------------------------------------
