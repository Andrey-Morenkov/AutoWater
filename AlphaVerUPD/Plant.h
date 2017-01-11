#pragma once

#include "Sensor.h"

extern const int maxSensors;
extern Sensor* sensor;
extern const int pumpTime;

#define ON  LOW
#define OFF HIGH

void LedOn();
void LedOff();



//--------------------------------------------------------------PLANT--------------------------------------------------------------
class Plant
{
  private:
  
    byte   id;     // id цветка
    String name;       // имя цветка
    byte   valve_pin;  // пин клапана
    Sensor hygrometer; // гигрометр
    int    crit_wet;   // критическая влажность
      
      
  public:  

      Plant (byte _valve = -1, Sensor _hygrometer = -1, int _crit_wet = -1)
      {
          valve_pin = _valve;
          hygrometer = _hygrometer;
      crit_wet = _crit_wet;
      pinMode(valve_pin, OUTPUT);
      }
      
      Plant (const Plant& _flower)
      {
      id = _flower.getId();
      name = _flower.getName();
          valve_pin  = _flower.getValve();
          hygrometer = _flower.getHygrometer().getPin();
      crit_wet   = _flower.getCritWet();
      pinMode(valve_pin, OUTPUT);
      }

    void init()
    {
      pinMode(valve_pin, OUTPUT);
    }

    void setId(int _id)
    {
      id = _id;
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
          valve_pin = _valve;
      }

    void setName(String _name)
    {
      name = _name;
    }

    void setCritWet(int _critWet)
    {
      crit_wet = _critWet;
    }

      void setHygrometer(const Sensor& _hygrometer)
      {
          hygrometer = _hygrometer;
      }

      byte getValve() const
      {
          return valve_pin;
      }

    int getCritWet() const
    {
      return crit_wet;
    }

    byte getId() const
    {
      return id;
    }

    String getName() const
    {
      return name;
    }

      const Sensor& getHygrometer() const
      {
          return hygrometer;
      }

      int getStat()
      {
          return hygrometer.getVal();
      }

    int ask()
      {
          //char buf[64];
          //snprintf(buf, sizeof(buf), "Plant: 0x%x, hygrometer: 0x%x", this, &hygrometer);
          //Serial.println(buf);
          Serial.print("ID : ");
          Serial.print(id);
          Serial.print(" ");
          hygrometer.ask();
      }   
};
//-------------------------------------------------------------/PLANT--------------------------------------------------------------

