#pragma once

//#include "Sensor.h"

//extern const int maxSensors;
//extern Sensor* sensor;
//extern const int pumpTime;

#define ON  LOW
#define OFF HIGH

void LedOn();
void LedOff();



//--------------------------------------------------------------PLANT--------------------------------------------------------------
class Plant
{
private:

	byte   id;		      // id цветка
	int    wetness;     // влажность
	String cname;       // имя цветка
	byte   hygro_pin;   // пин гигрометра 
	byte   valve_pin;   // пин клапана
	int    crit_wet;    // критическая влажность


public:

	Plant(byte _valve = -1, int _hygrometer = -1, int _crit_wet = -1)
	{
		valve_pin = _valve;
		hygro_pin = _hygrometer;
		crit_wet = _crit_wet;
	}

	Plant(const Plant& _flower)
	{
		id = _flower.getId();
		wetness = _flower.getWetness();
		cname = _flower.getName();
		valve_pin = _flower.getValve();
		hygro_pin = _flower.getHygroPin();
		crit_wet = _flower.getCritWet();
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

	int getWetness() const
	{
		return wetness;
	}

	void setVal(int _val)
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

	void setHygroPin(int _pin)
	{
		hygro_pin = _pin;
	}

	byte getHygroPin() const
	{
		return hygro_pin;
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

	int ask()
	{
		Serial.print("ID : ");
		Serial.print(id);
		Serial.print(" ");
		this->wetness = analogRead(hygro_pin);
		Serial.print("Hygrometer: ");
		Serial.println(this->wetness);
		return this->wetness;
	}
};
//-------------------------------------------------------------/PLANT--------------------------------------------------------------