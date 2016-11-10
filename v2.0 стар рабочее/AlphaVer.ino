#include "Sensor.h"
#include "Plant.h"
//-------- Initialization --------

#include <Streaming.h>
#include <TimerOne.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <stdio.h>

#include "Sensor.h"
#include "Plant.h"

// цифровые
unsigned char       *device;                         // Массив цифровых устройств
int                 Interrupt_pin = 2;            // Порт прерываний 
const unsigned char Valve_0 = 3;            // Клапан #1
const unsigned char Valve_1 = 4;            // Клапан #2
const unsigned char Pump = 5;            // Помпа
const unsigned char Relay_Sensors = 10;           // Сенсоры
												  //

												  // аналоговые
const unsigned char WaterSensor = A0;           // Сенсор уровня воды
const unsigned char HygroMeter_0 = A1;           // Гигрометр #1
const unsigned char HygroMeter_1 = A2;           // Гигрометр #2
												 //

const int           Alert_WS = 470;          // Критическое значение WaterSensor     (0 - сухо, 1024 - мокро)
const int           Alert_HM = 700;          // Критическое значение HygroMeter      (1024 - сухо, 0 - мокро)
const int           PumpTime = 5000;         // Максимальное время работы Pump       (в мс)
const int           MaxFlowers = 2;            // Количество контроллируемых цветков    (сделать динамически через sizeof) !!!!!!!!!!!!!!!!!!!!!!!!
const int           MaxSensors = MaxFlowers + 1; // Количество сенсоров = кол-во цветков + сенсор воды
const int           MaxDevices = MaxFlowers + 2; // Количество устройств = кол-во клапанов + помпа + реле сенсоров

unsigned long       cycle_time = 40;           // Цикл проверки датчиков ( в сек) // 86400 = 1 сутки

int                 timer_cycle = 8;            // Цикл таймера в сек

bool IsDev = false;

char number = -1;

char  command_fragment;

String command = "";


volatile bool  wakeup_timer = false;
volatile bool  wakeup_bt = false;
volatile int   timer_count = 0;
volatile long  cycle = cycle_time;   // Тут будем декрементить

#define ON  LOW
#define OFF HIGH
#define WS  MaxSensors-1
#define PMP MaxDevices-2
#define RS  MaxDevices-1

Sensor* sensor;                               // Массив сенсоров
Plant*  flower;                               // Массив цветков

											  //--------------------------------

void Timer1_action();

//--------------------------------------------------------------SETUP--------------------------------------------------------------
void setup()
{
	Serial.begin(9600);                      // Скорость (бит/c) , для отладки
	Serial.setTimeout(80);
	pinMode(Pump, OUTPUT);                    // Устанавливаем пин Pump
	pinMode(Relay_Sensors, OUTPUT);          // Устанавливаем пин датчиков

	device = new unsigned char[MaxDevices];  // массив для цифровых девайсов
	device[0] = Valve_0;
	device[1] = Valve_1;
	device[PMP] = Pump;
	device[RS] = Relay_Sensors;

	DevicesOFF();

	sensor = new Sensor[MaxSensors];
	sensor[0].setPin(HygroMeter_0);                // Гигрометр #1
	sensor[1].setPin(HygroMeter_1);                // Гигрометр #2
	sensor[WS].setPin(WaterSensor);                // Сенсор воды


	flower = new Plant[MaxFlowers];
	flower[0].setParams(Valve_0, sensor[0]);        // Цветок #1          
	flower[1].setParams(Valve_1, sensor[1]);        // Цветок #2

	for (int i = 0; i < MaxFlowers; i++)
	{
		// инициализация цветков
		flower[i].init();
	}

	pinMode(Interrupt_pin, INPUT);                  // порт прерывания как выход

	set_sleep_mode(SLEEP_MODE_IDLE);                // Определяем режим сна

	Timer1.initialize(timer_cycle * 1000000);       // x сек
	Timer1.attachInterrupt(Timer1_action);          // прерывание на таймере должно быть всегда

	IsDev = true; // пока работаем в дев моде всегда

	Serial.println("Initialisation complete.");
}
//-------------------------------------------------------------/SETUP--------------------------------------------------------------

void LedOn()
{
	digitalWrite(13, HIGH);
}

void LedOff()
{
	digitalWrite(13, LOW);
}

void DevicesOFF()
{
	for (int i = 0; i < MaxDevices; i++)
	{
		digitalWrite(device[i], OFF);
	}
}

void CheckSensors()
{
	digitalWrite(Relay_Sensors, ON);
	Serial.println("SENSORS ON");
	Serial.println("");

	for (int i = 0; i < MaxFlowers; i++)
	{
		Serial.print("-- FLOWER #");
		Serial.print(i);
		Serial.println(" :");
		flower[i].ask();
		Serial.println("");
	}

	Serial.println("-- Asking WaterSensor...");
	sensor[WS].ask();
	Serial.println("");
	digitalWrite(Relay_Sensors, OFF);
	Serial.println("SENSORS OFF");

}

void ForceWater(int N)
{
	Serial << "    Beginning of watering FLOWER # " << N << " :" << endl;
	flower[N].water();
	Serial << "    End of watering FLOWER # " << N << endl;
}

void WaterPlant(int N)
{
	if (flower[N].getStat() > Alert_HM)
	{
		ForceWater(N);
	}
	else
	{
		Serial << "    Don't need to water" << endl;
	}
}



void ParceCommand()
{
	while (Serial.available() > 0)
	{
		// пока в буфере есть данные, записываем их в строку
		command_fragment = Serial.read();
		command += command_fragment;
	}
}

void DoDevCommand(String _command)
{
	int Vald;

	if (_command.charAt(1) == '0')
	{
		IsDev = false;
		Serial.println("  DevMode OFF");
	}
	else
	{
		Vald = _command.substring(_command.indexOf('s') + 1).toInt();
		Serial.print("Val = ");
		Serial.println(Vald);

		if (_command.charAt(1) == 'f')
		{
			Serial.print("Target: flower ");
			Serial.println(_command.charAt(2));
			flower[_command.charAt(2) - '0'].setVal(Vald);
			flower[_command.charAt(2) - '0'].ask();
		}
		if (_command.charAt(1) == 'w')
		{
			Serial.print("Target: WS");
			sensor[WS].setVal(Vald);
			sensor[WS].ask();
		}
		Serial.println("DevMode ON");
		Serial.println("Print d0 to OFF");
	}
}


void Timer1_action()
{
	cycle -= timer_cycle;
	if (cycle <= 0)
	{
		wakeup_timer = true;
		cycle = cycle_time;    // возобновили счетчик таймера
	}
}

void BT_Action()
{
	wakeup_bt = true;
}

void EnterSleep()
{
	sleep_enable();                                  //Разрешаем спящий режим
	attachInterrupt(INT0, BT_Action, LOW);  //Если на 0-вом прерываниии - ноль, то просыпаемся.

											// выключаем ненужные модули
	power_adc_disable();
	power_spi_disable();
	power_timer0_disable();
	power_timer2_disable();
	power_twi_disable();


	sleep_mode();
	// тут сон

	sleep_disable();                // Запрещаем спящий режим

									// Врубаем все
	power_twi_enable();
	power_timer2_enable();
	power_timer0_enable();
	power_spi_enable();
	power_adc_enable();

	detachInterrupt(INT0);          // Отключаем прерывания по BT

	Serial.println("WakeUp");       // Проснулись
	delay(80);                      // Ждем пока все проснется
}

//-------------------------------------------------------------LOOP--------------------------------------------------------------
void loop()
{
	Serial << "-- Cycle Start --" << endl;
	delay(50);

	DevicesOFF();       // на всякий случай принудительно все выключим
	if (!wakeup_timer)
	{
		EnterSleep();
	}
	//----------- СОН -----------
	interrupts();
	Serial << "Cycle = " << cycle << endl;
	if (wakeup_timer)
	{
		Serial << "wakeup from TIMER (" << timer_cycle << " sec)" << endl;
		wakeup_timer = false;

		CheckSensors();
		delay(100);
		for (int i = 0; i < MaxFlowers; i++)
		{
			Serial << "Curr/Alert: " << flower[i].getStat() << "/" << Alert_HM << endl;
			WaterPlant(i);
		}
		DevicesOFF();
		Serial << "-- Cycle End--" << endl;
		Serial << "" << endl;
		Serial << "" << endl;
	}

	if (wakeup_bt)
	{
		Serial << "wakeup from BLUETOOTH" << endl;
		wakeup_bt = false;

		ParceCommand();

		Serial.print("Incoming command: ");
		Serial.println(command);

		if (command.startsWith("d"))
		{
			// developer mode
			IsDev = true;
			DoDevCommand(command);
		}
		else
		{
			if (command.toInt() == 1)
			{
				// для теста чекнуть сенсоры
				CheckSensors();
			}
			if (command.toInt() == 2)
			{
				// полить принудительно
				for (int i = 0; i < MaxFlowers; i++)
				{
					ForceWater(i);
				}
			}
			DevicesOFF();
		}
		command = "";
	}
}
//------------------------------------------------------------/LOOP--------------------------------------------------------------

