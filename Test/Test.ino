#include <TimerOne.h>

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>

char number = -1;
int pin = 2;
String command = "";
char command_fragment;

volatile bool  wakeup_timer = false;
volatile bool  wakeup_bt = false;
volatile int  timer_count = 0;

void setup()
{
  Serial.begin(9600);                // инициализация порта
  pinMode(pin, INPUT);               // порт прерывания как выход
  
  set_sleep_mode(SLEEP_MODE_IDLE);   //Определяем режим сна

  Timer1.initialize(8000000);        // 8 сек
  Timer1.attachInterrupt(Timer1_action); // прерывание на таймере должно быть всегда
  
  Serial.println("Initialisation complete.");
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
  delay(50); 
  EnterSleep();        //Подготовка ко сну и сон
  // после сна начинаем отсюда
  
  if (wakeup_timer)
  {
    Serial.println("WAKEUP from timer (40 sec)");
    wakeup_timer = false;
    timer_count = 0;
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
    command = "";
  }
}

