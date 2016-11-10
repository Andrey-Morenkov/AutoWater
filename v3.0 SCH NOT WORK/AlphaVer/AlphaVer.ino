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
int                 interrupt_pin    = 2;            // Порт прерываний 
const unsigned char valves[]     = { 3, 4 };   // Порты клапанов
const unsigned char pump             = 5;            // Помпа
const unsigned char relay_Sensors    = 10;           // Сенсоры
//

// аналоговые
const unsigned char waterSensor      = A0;           // Сенсор уровня воды
const unsigned char hygrometers[]  = { A1, A2 };   // Порты гигрометров
//

const int           alert_WS         = 470;          // Критическое значение WaterSensor     (0 - сухо, 1024 - мокро)
const int           alert_HM         = 700;          // Критическое значение HygroMeter      (1024 - сухо, 0 - мокро)
const int           pumpTime         = 5;      // Время работы Pump       (в сек)
const int           maxFlowers       = 2;            // Количество контроллируемых цветков    (сделать динамически через sizeof) !!!!!!!!!!!!!!!!!!!!!!!!
const int           maxSensors       = maxFlowers+1; // Количество сенсоров = кол-во цветков + сенсор воды
const int           maxDevices       = maxFlowers+2; // Количество устройств = кол-во клапанов + помпа + реле сенсоров

long        cycle_time_def   = 40;             // Цикл проверки датчиков по умолчанию ( в сек) // 86400 = 1 сутки
long        cycle_time_curr  = cycle_time_def; // текущий цикл

int                 timer_cycle      = 8;              // Цикл таймера в сек

volatile long   cycle      = cycle_time_def;   // Тут будем декрементить
volatile bool   wakeup_timer   = false;
volatile bool   wakeup_bt    = false;
volatile int    timer_count    = 0;

bool reset_sch_flag = false;  // сбросить расписание при выходе из сна?

bool isDev = false;

char number = -1;
unsigned char step = 0;

char  command_fragment;

String command = "";

typedef void(*schedule_type)(long);                              // новый тип - указатель на функцию
struct schedule_cell
{
  schedule_type func ;
  long          param;
};
schedule_cell *schedule;                           // расписание - массив указателей на структуру


#define ON  LOW
#define OFF HIGH
#define WS  maxSensors-1
#define PMP maxDevices-2
#define RS  maxDevices-1

Sensor* sensor;                               // Массив сенсоров
Plant*  flower;                               // Массив цветков

//--------------------------------

void Timer1_action();
void ScheduleBuilder();
void CheckSensors(int o = 0);
void DevicesOFF(int o = 0);
void ResetSchedule(long o = 0);
void Sleep(long sec);
void EnterSleep();

//--------------------------------------------------------------SETUP--------------------------------------------------------------
void setup()
{
   Serial.println("Setup beginning");
     Serial.begin(9600);                      // Скорость (бит/c) , для отладки
     Serial.setTimeout(80);
     pinMode(pump,OUTPUT);                    // Устанавливаем пин Pump
     pinMode(relay_Sensors, OUTPUT);          // Устанавливаем пин датчиков
        
     device = new unsigned char[maxDevices];  // массив для цифровых девайсов
   for (int i = 0; i < maxFlowers; i++)
     device[i] = valves[i];         // заносим клапаны в массив
     device[PMP] = pump;
     device[RS]  = relay_Sensors;

     DevicesOFF(0);

     sensor = new Sensor[maxSensors];
   for (int i = 0; i < maxFlowers; i++)
     sensor[i].setPin(hygrometers[i]);      // установка гигрометров
     sensor[WS].setPin(waterSensor);                // Сенсор воды
     

     flower = new Plant [maxFlowers];
   for (int i = 0; i < maxFlowers; i++)
   {
     // заносим параметры и инициализируем
     flower[i].setParams(valves[i], sensor[i]);
     flower[i].init();
   }
     
     pinMode(interrupt_pin, INPUT);                  // порт прерывания как выход
  
     set_sleep_mode(SLEEP_MODE_IDLE);                // Определяем режим сна

     Timer1.initialize(timer_cycle * 1000000);       // x сек
     Timer1.attachInterrupt(Timer1_action);          // прерывание на таймере должно быть всегда

     isDev = true; // пока работаем в дев моде всегда
   wakeup_timer = true;

   schedule = nullptr;
   step = 0;
     Serial.println("Initialisation complete.");
}
//-------------------------------------------------------------/SETUP--------------------------------------------------------------
void ValveON(long N)
{
  digitalWrite(valves[(int)N], ON);
}
void PumpON(long N)
{
  digitalWrite(pump, ON);
}
void ValveOFF(long N)
{
  digitalWrite(valves[(int)N], OFF);
}
void PumpOFF(long N)
{
  digitalWrite(pump, OFF);
}

void WaterPlant(int _N, int &i)
{
  schedule[i].func  = ValveON;
  schedule[i].param = _N;

  schedule[i + 1].func  = PumpON;
  schedule[i + 1].param = NULL;

  schedule[i + 2].func  = Sleep;
  schedule[i + 2].param = pumpTime;

  schedule[i + 3].func = PumpOFF;
  schedule[i + 3].param = NULL;

  schedule[i + 4].func = ValveOFF;
  schedule[i + 4].param = _N;

  i = i + 4;
}

void ResetSchedule(long o)
{
  Serial << "ResetSchedule" << endl;
  delete[] schedule;
  schedule = nullptr;
}
void PrepareToSleep(long o)
{
  Serial << "PrepareToSleep" << endl;
  DevicesOFF();
}

void ScheduleBuilder()
{
  if (wakeup_timer)
  {
    CheckSensors();
    int sch_size = 3;  // в конце идет prepare,sleep и delete
    for (int i = 0; i < maxFlowers;i++)
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
    Serial << "sch size = "<< sch_size<< endl;
    schedule = new schedule_cell[sch_size];
    int cell = 0;
    for (int N = 0; N < maxFlowers; N++)
    {
      if (flower[N].getStat() > alert_HM)
      {
        WaterPlant(N,cell);
        cell++;
      }
    }
    schedule[sch_size - 3].func  = PrepareToSleep;
    schedule[sch_size - 3].param = NULL;

    schedule[sch_size - 2].func  = Sleep;
    schedule[sch_size - 2].param = cycle_time_def;

    schedule[sch_size - 1].func  = ResetSchedule;
    schedule[sch_size - 1].param = NULL;
    Serial << "Schedule built" << endl;
    delay(20);
  }
  if (wakeup_bt)
  {
    
  }
}

void LedOn()
{
  digitalWrite(13, HIGH);
}

void LedOff()
{
  digitalWrite(13, LOW);
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
  int delta = Multiple(sec, (long)timer_cycle);
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
  cycle = sec;
  Serial << "Go to sleep for " << cycle << " sec" << endl;
  wakeup_timer = false;
  Timer1.initialize(timer_cycle * 1000000);
  Timer1.restart();
  EnterSleep();
}

void DevicesOFF(int o)
{
    for (int i = 0; i < maxDevices; i++)
    {
      digitalWrite(device[i],OFF);
    }
}

void CheckSensors(int o)
{
  digitalWrite(relay_Sensors, ON);
  Serial << "SENSORS ON" << endl;
  
  for (int i = 0; i < maxFlowers; i++)
  {
    Serial << "-- FLOWER #" << i << " :" << endl;
      flower[i].ask();
    Serial << "" << endl;
  }

  Serial.println("-- Asking WaterSensor...");
  sensor[WS].ask();
  Serial.println("");
  digitalWrite(relay_Sensors, OFF);
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
  if (flower[N].getStat() > alert_HM)
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
    isDev = false;
    Serial.println("  DevMode OFF");
  }
  else
  {
    Vald = _command.substring(_command.indexOf('s')+1).toInt();
    Serial.print("Val = ");
    Serial.println(Vald);
     
    if (_command.charAt(1) == 'f')
    {
      Serial.print("Target: flower ");
      Serial.println(_command.charAt(2));
      flower[_command.charAt(2)-'0'].setVal(Vald);
      flower[_command.charAt(2)-'0'].ask();
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

//-------------------------------------------------------------Interrupts
void Timer1_action()
{
  cycle -= timer_cycle;
  if (cycle <= 0)
  {
    wakeup_timer = true;
  if (reset_sch_flag)
  {
    ResetSchedule();
    step = 0;
  }   
    cycle = cycle_time_curr;    // возобновили счетчик таймера
  }
}

void BT_Action()
{
  wakeup_bt = true;
}
//------------------------------------------------------------/Interrupts

void EnterSleep()
{
  Serial << "EnterSleep" << endl;
  wakeup_timer = false;
  sleep_enable();                                  //Разрешаем спящий режим
  //attachInterrupt(INT0, BT_Action, LOW);       //Если на 0-вом прерываниии - ноль, то просыпаемся.

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
  //Serial.println("WakeUp");
   delay(80);                      // Ждем пока все проснется
}

//-------------------------------------------------------------LOOP--------------------------------------------------------------
void loop()
{
  Serial << "WakeUpCycle" << endl;
  if (wakeup_timer)
  {
    Serial << "WakeupTimer" << endl;
    if (schedule == nullptr)
    {
      Serial << "No Schedule" << endl;
      delay(20);
      ScheduleBuilder();
    }

    Serial << "Do step " << step << " :" << endl;
    schedule[step].func(schedule[step].param);
    step++;
  }
  else
  {
    if (wakeup_bt)
    {
      Serial << "WakeupBT" << endl;

    }
    else
    {
      Serial << "NoFlags" << endl;
      EnterSleep();
    }
  }
}
