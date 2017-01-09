// Проблемы:
// - Sleep и BT
// - Низкая скорость работы (нужны delay)


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
int                 interrupt_pin = 2;				 // Порт прерываний 
const unsigned char valves[] = { 3,4 };
const unsigned char Valve_0 = 3;					 // Клапан #1
const unsigned char Valve_1 = 4;				     // Клапан #2
const unsigned char pump = 5;						 // Помпа
const unsigned char relay_Sensors = 10;              // Сенсоры
													 //

													 // аналоговые
const unsigned char waterSensor = A0;				 // Сенсор уровня воды
const unsigned char hygrometers[] = { A1,A2 };
const unsigned char HygroMeter_0 = A1;				 // Гигрометр #1
const unsigned char HygroMeter_1 = A2;				 // Гигрометр #2
													 //

const int           alert_WS = 470;					 // Критическое значение WaterSensor     (0 - сухо, 1024 - мокро)
const int           alert_HM = 700;					 // Критическое значение HygroMeter      (1024 - сухо, 0 - мокро)
const int           pumpTime = 8;					 // Время работы Pump       (в с)
const int           maxFlowers = 2;					 // Количество контроллируемых цветков    (сделать динамически через sizeof) !!!!!!!!!!!!!!!!!!!!!!!!
const int           maxSensors = maxFlowers + 1;	 // Количество сенсоров = кол-во цветков + сенсор воды
const int           maxDevices = maxFlowers + 2;	 // Количество устройств = кол-во клапанов + помпа + реле сенсоров

unsigned long       cycle_time_def = 40;			 // Цикл проверки датчиков ( в сек) // 86400 = 1 сутки

int                 timer_cycle = 8;				 // Цикл таймера в сек
int					sch_size    = 0;				 // Текущий размер расписания

bool IsDev = false;

char number = -1;

char  command_fragment;

String command = "";

bool  next_step = false;
volatile bool  wakeup_timer = false;
volatile bool  wakeup_ex = false;
volatile long  cycle = cycle_time_def;   // Тут будем декрементить

#define ON  LOW
#define OFF HIGH
#define WS  maxSensors-1
#define PMP maxDevices-2
#define RS  maxDevices-1

Sensor* sensor;                               // Массив сенсоров
Plant*  flower;                               // Массив цветков

typedef void(*schedule_type)(long);                      // новый тип - указатель на функцию
struct schedule_cell
{
	schedule_type func;
	long          param;
	//String        id;
};
schedule_cell *schedule;                           // расписание - массив указателей на структуру
unsigned char step = 0; // шаг


//--------------------------------

void Timer1_action();
void DoDevCommand(String _command);

//--------------------------------------------------------------SETUP--------------------------------------------------------------
void setup()
{
	Serial.begin(9600);                      // Скорость (бит/c) , для отладки
	Serial.setTimeout(80);
	pinMode(pump, OUTPUT);                    // Устанавливаем пин Pump
	pinMode(relay_Sensors, OUTPUT);          // Устанавливаем пин датчиков

	device = new unsigned char[maxDevices];  // массив для цифровых девайсов
	for (int i = 0; i < maxFlowers; i++)
		device[i] = valves[i];
	device[PMP] = pump;
	device[RS] = relay_Sensors;

	DevicesOFF();

	sensor = new Sensor[maxSensors];
	for (int i = 0; i < maxFlowers; i++)
		sensor[i].setPin(hygrometers[i]);
	sensor[WS].setPin(waterSensor);                // Сенсор воды


	flower = new Plant[maxFlowers];
	for (int i = 0; i < maxFlowers; i++)
		flower[i].setParams(valves[i], sensor[i]);

	for (int i = 0; i < maxFlowers; i++)
	{
		// инициализация цветков
		flower[i].init();
	}

	pinMode(interrupt_pin, INPUT);                  // порт прерывания как выход

	set_sleep_mode(SLEEP_MODE_IDLE);                // Определяем режим сна

	cycle = cycle_time_def;
	Timer1.initialize(timer_cycle * 1000000);       // x сек
	Timer1.attachInterrupt(Timer1_action);          // прерывание на таймере должно быть всегда

	IsDev = true; // пока работаем в дев моде всегда
	schedule = nullptr;
	next_step = true;
	step = 0;

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

void ValveON(long N)
{
	Serial << "ValveOn" << endl;
	digitalWrite(valves[(int)N], ON);
	next_step = true;
}
void PumpON(long N)
{
	Serial << "PumpOn" << endl;
	digitalWrite(pump, ON);
	next_step = true;
}
void ValveOFF(long N)
{
	Serial << "ValveOFF" << endl;
	digitalWrite(valves[(int)N], OFF);
	next_step = true;
}
void PumpOFF(long N)
{
	Serial << "PumpOFF" << endl;
	digitalWrite(pump, OFF);
	next_step = true;
}

int Multiple(long first, long second)
{
	return ((first / second)*second - first);
}

void Sleep(long sec = cycle_time_def)
{
	Serial << "Sleep" << endl;
	Serial << "sec = " << sec << endl;
	Serial << "tim_cycle = " << timer_cycle << endl;
	/*	int delta = Multiple(sec, (long)timer_cycle);
	if (delta != 0)
	{
	Serial << "Not optimal timer_cycle = " << timer_cycle << endl;
	Serial << "delta = " << delta << " sec" << endl;

	// тут ищем такое значение цикла, при котором разница
	// между sec и возможностями таймера минимальна
	int optimal_cycle = timer_cycle;
	int k;
	for (int i = 2; i <= 8; i++)
	{
	k = Multiple(sec, (long)i);
	if (k <= delta)
	{
	delta = k;
	optimal_cycle = i;
	}

	}
	timer_cycle = optimal_cycle;
	Serial << "Found optimal timer_cycle = " << timer_cycle << endl;
	Serial << "delta = " << delta << " sec" << endl;
	}
	*/
	cycle = sec;
	wakeup_timer = false;
	//Timer1.setPeriod(timer_cycle * 1000000);
	//Timer1.restart();
}

void WaterPlant(int _N, int &i)
{
	schedule[i].func = ValveON;
	schedule[i].param = _N;
	//schedule[i].id = "v";

	schedule[i + 1].func = PumpON;
	schedule[i + 1].param = NULL;
	//schedule[i + 1].id = "p";

	schedule[i + 2].func = Sleep;
	schedule[i + 2].param = pumpTime;
	//schedule[i + 2].id = "s";

	schedule[i + 3].func = PumpOFF;
	schedule[i + 3].param = NULL;
	//schedule[i + 3].id = "/p";

	schedule[i + 4].func = ValveOFF;
	schedule[i + 4].param = _N;
	//schedule[i + 4].id = "/v";

	i = i + 4;
}

void ResetSchedule(long o = 0)
{
	Serial << "ResetSchedule" << endl;
	delete[] schedule;
	schedule = nullptr;
	next_step = true;
}

void DevicesOFF()
{
	for (int i = 0; i < maxDevices; i++)
	{
		digitalWrite(device[i], OFF);
	}
}

void CheckSensors()
{
	digitalWrite(relay_Sensors, ON);
	Serial.println("SENSORS ON");
	Serial.println("");

	for (int i = 0; i < maxFlowers; i++)
	{
		Serial.print("-- FLOWER #");
		Serial.print(i);
		Serial.println(" :");
		flower[i].ask();
		Serial.println("");
	}

	Serial.println("-- WS");
	sensor[WS].ask();
	Serial.println("");
	digitalWrite(relay_Sensors, OFF);
	Serial.println("SENSORS OFF");

}

// OLD
//void ForceWater(int N)
//{
//	Serial << "    Beginning of watering FLOWER # " << N << " :" << endl;
//	flower[N].water();
//	Serial << "    End of watering FLOWER # " << N << endl;
//}

//void WaterPlant(int N)
//{
//	if (flower[N].getStat() > alert_HM)
//	{
//		ForceWater(N);
//	}
//	else
//	{
//		Serial << "    Don't need to water" << endl;
//	}
//}



void ParceCommand()
{
	command = "";
	while (Serial.available() > 0)
	{
		// пока в буфере есть данные, записываем их в строку
		command_fragment = Serial.read();
		command += command_fragment;
	}
}
void PrepareToLongSleep(long o = 0)
{
	DevicesOFF();
	next_step = true;
}
void CalculateScheduleSize()
{
	sch_size = 3;  // в конце идет sleep, prepare и delete
	CheckSensors();
	for (int i = 0; i < maxFlowers; i++)
	{
		if (flower[i].getStat() > alert_HM)
		{
			sch_size += 5;
			Serial << "The flower " << i << " needs watering" << endl;
			delay(20);
		}
		else
		{
			Serial << "The flower " << i << " does not need watering" << endl;
			delay(20);
		}
	}
}
void ScheduleBuilder(String mode = "default", int plant = -1)
{
	step = 0;

	if (mode == "default")
	{
		Serial << "default mode" << endl;
		CalculateScheduleSize();
		Serial << "sch size = " << sch_size << endl;

		schedule = new schedule_cell[sch_size];
		int cell = 0;
		for (int N = 0; N < maxFlowers; N++)
		{
			if (flower[N].getStat() > alert_HM)
			{
				WaterPlant(N, cell);
				cell++;
			}
		}
		schedule[sch_size - 3].func = PrepareToLongSleep;
		schedule[sch_size - 3].param = NULL;

		schedule[sch_size - 2].func = Sleep;
		schedule[sch_size - 2].param = cycle_time_def;

		schedule[sch_size - 1].func = ResetSchedule;
		schedule[sch_size - 1].param = NULL;
		Serial << "Schedule built" << endl;
	}
	if (mode == "custom")
	{
		Serial << "custom mode" << endl;
		DevicesOFF();
		ResetSchedule();
		Serial << "Deleted OLD schedule" << endl;
		sch_size = 6;
		int cell = 0;

		schedule = new schedule_cell[sch_size];
		WaterPlant(plant, cell);
		schedule[sch_size - 1].func = ResetSchedule;
		schedule[sch_size - 1].param = NULL;
		next_step = true;
	}

	delay(20);
}

void DoCommand(String _command)
{
	if (command.charAt(0) == 'd')
	{
		// dp0s100 = flower 0 set 100
		DoDevCommand(command);
	}
	else
	{
		if (command.charAt(0) == 'w')
		{
			CheckSensors();
			// wp1 = water plant 1
			int plantNumber = command.substring(command.indexOf("p") + 1).toInt();
			if (flower[plantNumber].getStat() < alert_HM)
			{
				// отправить ответ, что поливать не нужно
				Serial << "Don't need watering plant # "<< plantNumber << endl;
			}
			else
			{
				Serial << "Begin to custom watering plant # " << plantNumber << endl;
				ScheduleBuilder("custom", plantNumber);
			}
		}
		if (command.charAt(0) == 'f')
		{
			int plantNumber = command.substring(command.indexOf("p") + 1).toInt();
			Serial << "Begin to custom FORCED watering plant # " << plantNumber << endl;
			ScheduleBuilder("custom", plantNumber);
		}

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

		if (_command.charAt(1) == 'p')
		{
			Serial.print("Target: plant ");
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
		cycle = cycle_time_def;    // возобновили счетчик таймера
	}
}

void EX_Action()
{
	wakeup_ex = true;
}

void EnterSleep()
{
	sleep_enable();                                  //Разрешаем спящий режим
	attachInterrupt(INT0, EX_Action, LOW);  //Если на 0-вом прерываниии - ноль, то просыпаемся.

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
	delay(80);                     // Ждем пока все проснется
}

//-------------------------------------------------------------LOOP--------------------------------------------------------------
void loop()
{
	Serial << "New Loop" << endl;
	if (wakeup_ex)
	{
		wakeup_ex = false;
		Serial << "Wakeup BT" << endl;
		ParceCommand();
		DoCommand(command);	
	}
	if (wakeup_timer || next_step)
	{
		next_step = false;
		wakeup_timer = false;

		Serial << "Wakeup TIMER / NEXT STEP" << endl;

		if (schedule == nullptr)
		{
			Serial << "No schedule" << endl;
			ScheduleBuilder();
		}

		Serial << "Step # " << step << endl;
		schedule[step].func(schedule[step].param);
		step++;												// step указывает не следущую выполненную инструкцию
		Serial << "/ Step # " << step - 1 << endl;
	}
	else
	{
		Serial << "Sleep section" << endl;

		next_step = false;
		wakeup_timer = false;

		Serial << cycle << " sec" << endl;
		Serial << timer_cycle << " timer cycle" << endl;
		delay(100);

		if (Serial.available > 0)
		{
			wakeup_ex = true;
		}
		else
		{
			// если в BT ничего нет то идем спать
			EnterSleep();
			attachInterrupt(INT0, EX_Action, LOW);
		}				

		Serial << "/Sleep section" << endl;
	}
	Serial << "/ Loop" << endl;
}
//------------------------------------------------------------/LOOP--------------------------------------------------------------

