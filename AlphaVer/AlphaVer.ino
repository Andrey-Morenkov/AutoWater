//-------- Initialization --------

#include <TimerOne.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>

const unsigned char Valve_0          = 3;            // Клапан #1
const unsigned char Valve_1          = 4;            // Клапан #2
const unsigned char Pump             = 5;            // Помпа
const unsigned char Relay_Sensors    = 10;           // Сенсоры

const unsigned char WaterSensor      = A0;           // Сенсор уровня воды
const unsigned char HygroMeter_0     = A1;           // Гигрометр #1
const unsigned char HygroMeter_1     = A2;           // Гигрометр #2

const int Alert_WS                   = 470;          // Критическое значение WaterSensor     (0 - сухо, 1024 - мокро)
const int Alert_HM                   = 700;          // Критическое значение HygroMeter      (1024 - сухо, 0 - мокро)
const int MaxTime_P                  = 5000;         // Максимальное время работы Pump       (в мс)
const int MaxFlowers                 = 2;            // Количество контроллируемых цветков
const int MaxSensors                 = MaxFlowers+1; // Количество сенсоров = кол-во цветков + сенсор воды

const unsigned int Cycle             = 10000;        // Цикл работы (в мс)
unsigned long      MaxTime           = 4294967295;   // Максимальное время в ардуино (мс)
unsigned long      CurrTime          = 0;            // Текущее время работы
unsigned long      StartProcess      = 0;            // Время начала работы процесса

char number = -1;
int pin = 2;
String command = "";
char command_fragment;

volatile bool  wakeup_timer = false;
volatile bool  wakeup_bt = false;
volatile int  timer_count = 0;

#define ON LOW
#define OFF HIGH
#define WS MaxSensors-1

//--------------------------------

void LedOn()
{
  digitalWrite(13, HIGH);
}

void LedOff()
{
  digitalWrite(13, LOW);
}

//-------------------------------------------------------------SENSOR--------------------------------------------------------------
class Sensor
{
  private:
  
      signed char pin;         // пин сенсора
      int         val;         // показания сенсора  
      
  public:
      
      Sensor( signed char _pin = -1)
      {
          pin = _pin;
          val = -1;
      }

      Sensor (const Sensor& _sensor )
      {
          pin = _sensor.getPin();
          val = _sensor.getVal();         //опасно ??
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

      int ask()
      {
          val = analogRead(pin);
          Serial.print("   Sensor.ask() Value: ");
          Serial.println(val);
          return val;
      }
};  
//------------------------------------------------------------/SENSOR--------------------------------------------------------------
Sensor* sensor;                               // Массив сенсоров
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
      
      Plant (Plant& _flower)
      {
          valve =      _flower.getValve();
          hygrometer = _flower.getHygrometer().getPin();
      }   
      
      void setValve(signed char _valve)
      {
          valve = _valve;
      }

      void setHygrometer(Sensor _hygrometer)
      {
          hygrometer = _hygrometer;
      }
      
      void setParams (signed char _valve, Sensor _hygrometer)
      {
          valve = _valve;
          hygrometer = _hygrometer;
      }

      signed char getValve()
      {
          return valve;
      }

      Sensor getHygrometer()
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
          if ( sensor[WS].getVal() <= Alert_WS )                            // Если воды меньше нормы то опрос долива воды
          {
              while (sensor[WS].getVal() <= Alert_WS)                       
              {
                  Serial.println("NO WATER !");
                  LedOn();
                  delay(2000);
                  Serial.println("ASKING FOR WATER...");
                  digitalWrite(Relay_Sensors, ON);
                  delay(500);
                  sensor[WS].ask();
                  digitalWrite(Relay_Sensors, OFF);
              }
              LedOff();
          }
          Serial.println("OK WATER, watering... ");    
          digitalWrite(valve,ON);
          Serial.println("VALVE ON");
          digitalWrite(Pump,ON);
          Serial.println("PUMP ON");
          delay(MaxTime_P);
          digitalWrite(Pump,OFF);
           Serial.println("PUMP OFF");
          delay(200);
          digitalWrite(valve,OFF);
          Serial.println("VALVE OFF");        
      }
                 
};
//-------------------------------------------------------------/PLANT--------------------------------------------------------------

Plant*  flower;                               // Массив цветков

void setup()
{
     Serial.begin(9600);                      // Скорость (бит/c) , для отладки
     Serial.setTimeout(80);
     pinMode(Pump,OUTPUT);                    // Устанавливаем пин Pump
     pinMode(Relay_Sensors, OUTPUT);          // Устанавливаем пин датчиков
        
     digitalWrite(Pump, OFF);
     digitalWrite(Valve_0, OFF);
     digitalWrite(Valve_1, OFF);
     digitalWrite(Relay_Sensors, OFF);

     sensor = new Sensor[MaxSensors];
     sensor[0].setPin(HygroMeter_0);                // Гигрометр #1
     sensor[1].setPin(HygroMeter_1);                // Гигрометр #2
     sensor[WS].setPin(WaterSensor);                // Сенсор воды
     

     flower = new Plant [MaxFlowers];
     flower[0].setParams(Valve_0,sensor[0]);        // Цветок #1          
     flower[1].setParams(Valve_1,sensor[1]);        // Цветок #2

     for (int i = 0; i < MaxFlowers; i++)
     {
        flower[i].init();
     }
     
     pinMode(pin, INPUT);               // порт прерывания как выход
  
     set_sleep_mode(SLEEP_MODE_IDLE);   //Определяем режим сна

     Timer1.initialize(8000000);        // 8 сек
     Timer1.attachInterrupt(Timer1_action); // прерывание на таймере должно быть всегда
  
     Serial.println("Initialisation complete.");
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

void Timer1_action()
{
  timer_count++;

  // 40 сек
  if (timer_count == 5)
  {
    wakeup_timer = true;
  }
}

void BT_Action()
{
  wakeup_bt = true;
}

void EnterSleep()
{
  sleep_enable(); //Разрешаем спящий режим
  attachInterrupt(INT0, BT_Action, LOW); //Если на 0-вом прерываниии - ноль, то просыпаемся.

  // выключаем ненужные модули
  power_adc_disable();
  power_spi_disable();
  power_timer0_disable();
  power_timer2_disable();
  power_twi_disable();
  
    
  sleep_mode();
  //---------------------------------
  // СОН
  //---------------------------------
  
  sleep_disable();    // Запрещаем спящий режим
  power_all_enable(); // Врубаем все
  detachInterrupt(0); // Отключаем прерывания по BT

  Serial.println("WakeUp"); //Проснулись
  delay(80); // ждем пока все проснется
}


void loop()
{
  Serial.println("--Beggining of cycle--");
  delay(50); 

  // sensors off
  digitalWrite(Pump, OFF);
  digitalWrite(Valve_0, OFF);
  digitalWrite(Valve_1, OFF);
  digitalWrite(Relay_Sensors, OFF);
     
  EnterSleep();        //Подготовка ко сну и сон
  // после сна начинаем отсюда
  
  if (wakeup_timer)
  {
    Serial.println("WAKEUP from timer (40 sec)");
    wakeup_timer = false;
    timer_count = 0;
    CheckSensors();
    delay(100);
    for (int i = 0; i < MaxFlowers; i++)
    {
        
        Serial.print("Curr / Alert :");
        Serial.print(flower[i].getStat());
        Serial.print(" / ");
        Serial.println(Alert_HM);
        
        if (flower[i].getStat() > Alert_HM)
        {
            Serial.print("Beginning of watering FLOWER #");
            Serial.print(i);
            Serial.println(" :");
            flower[i].water();
            Serial.print("End of watering FLOWER #");
            Serial.print(i);
            Serial.println(" :");
        }
    }
    digitalWrite(Pump, OFF);
    digitalWrite(Valve_0, OFF);
    digitalWrite(Valve_1, OFF);
    digitalWrite(Relay_Sensors, OFF);

    Serial.println("--End of cycle, going to sleep--");
    Serial.println("");
    Serial.println("");
  }
  
  if (wakeup_bt)
  {
    Serial.println("WAKEUP from BT"); 
    wakeup_bt = false;

    while (Serial.available() > 0)
    {
      // пока в буфере есть данные, записываем их в строку
      command_fragment = Serial.read();
      command += command_fragment;      
    }
    Serial.print("Incoming command: ");
    Serial.println(command.toInt());
    if (command.toInt() == 1)
    {
      // для теста чекнуть сенсоры
      CheckSensors();
    }
    if (command.toInt() == 2)
    {
      for (int i = 0; i < MaxFlowers; i++)
    {
        
        Serial.print("Curr / Alert :");
        Serial.print(flower[i].getStat());
        Serial.print(" / ");
        Serial.println(Alert_HM);
        
        if (flower[i].getStat() > Alert_HM)
        {
            Serial.print("Beginning of watering FLOWER #");
            Serial.print(i);
            Serial.println(" :");
            flower[i].water();
            Serial.print("End of watering FLOWER #");
            Serial.print(i);
            Serial.println(" :");
        }
    }
    digitalWrite(Pump, OFF);
    digitalWrite(Valve_0, OFF);
    digitalWrite(Valve_1, OFF);
    digitalWrite(Relay_Sensors, OFF);
    }
    command = "";
  }
}

