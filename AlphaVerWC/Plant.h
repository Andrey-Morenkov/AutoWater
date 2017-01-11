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
  
	  byte   id;		 // id цветка
	  int    wetness;    // влажность
	  String cname;      // имя цветка
	  byte   hygro_pin;  // пин гигрометра
	  byte   valve_pin;  // пин клапана
	  int    crit_wet;   // критическая влажность
      
  public:

	  Plant(int _id, int _wetness, String _name, int hp, int _vp, int _cw)
	  {
		  id = _id;
		  wetness = _wetness;
		  cname = _name;
		  hygro_pin = hp;
		  valve_pin = _vp;
		  crit_wet = _cw;
	  }

      Plant (byte _valve = -1, int _hygrometer = -1, int _crit_wet = -1)
      {
		  Serial << "Empty constructor with v " << _valve << " h "<< _hygrometer << " c "<< _crit_wet << endl;
		  id = -1;
		  wetness = -2;
		  cname = "Unnamed";
          valve_pin = _valve;
          hygro_pin = _hygrometer;
		  crit_wet = _crit_wet;
      }
      
      Plant (const Plant& _flower)
      {
		  Serial << "Copy constructor" << endl;
		  id = _flower.getId();
		  cname = _flower.getName();
          valve_pin  = _flower.getValve();
		  hygro_pin = _flower.getHygroPin();
          wetness = _flower.getWetness();
		  crit_wet   = _flower.getCritWet();
		  pinMode(valve_pin, OUTPUT);
      }

	  void init()
	  {
		  pinMode(valve_pin, OUTPUT);
	  }

	  int getWetness() const
	  {
		  return wetness;
	  }

	  void setHygroPin(int pin)
	  {
		  hygro_pin = pin;
	  }

	  int getHygroPin() const
	  {
		  return hygro_pin;
	  }

	  void setId(int _id)
	  {
		  id = _id;
	  }

      void setWetness(int _val)
      {
          //char buf[64];
          //snprintf(buf, sizeof(buf), "Plant: 0x%x, hygrometer: 0x%x", this, &hygrometer);
          //Serial.println(buf);
          wetness = _val;
      }
      
      void setValve(signed char _valve)
      {
          valve_pin = _valve;
      }

	  void setName(String _name)
	  {
		  cname = _name;
	  }

	  void setCritWet(int _critWet)
	  {
		  crit_wet = _critWet;
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
		  return cname;
	  }
	  int debugWet()
	  {
		  return wetness;
	  }

      int ask()
      {
          //char buf[64];
          //snprintf(buf, sizeof(buf), "Plant: 0x%x, hygrometer: 0x%x", this, &hygrometer);
          //Serial.println(buf);
		  this->wetness = analogRead(hygro_pin);
		  Serial.print("ID : ");
		  Serial.print(id);
		  Serial.print(" ");
		  Serial.print("Wetness : ");
		  Serial.println(this->wetness);
		  return wetness;
      }   
};
//-------------------------------------------------------------/PLANT--------------------------------------------------------------
