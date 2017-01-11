#include <LinkedList.h>

#include <EEPROM.h>

// Проблемы:
// - Sleep и BT
// - Низкая скорость работы (нужны delay)

//WaterSensor(0 - сухо, 1024 - мокро)
//HygroMeter      (1024 - сухо, 0 - мокро)

//-------- Initialization --------

#include <Streaming.h>
#include <TimerOne.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <stdio.h>


#include "Sensor.h"
#include "Plant.h"
#include "EEPROMcell.h"

//  Hardcoded Пины 
int                 interrupt_pin = 2;        // Порт прерываний 
byte        pump = 5;           // Помпа
byte        relay_Sensors = 10;             // Сенсоры
byte        waterSensor = A0;       // Сенсор уровня воды
                          //

LinkedList <byte> *devices;              // Массив пинов цифровых устройств (Помпа, реле сенсоров + клапаны)
LinkedList <Sensor> *sensors;            // Массив сенсоров (Гигрометры + WaterSensor)
LinkedList <Plant>  *flowers;            // Массив цветков

int                flowers_count = 255;      // Сколько подключенных цветков (255 == 0)

const int           pumpTime = 8;          // Время работы Pump       (в с)

unsigned long       cycle_time_def = 32;       // Цикл проверки датчиков ( в сек) // 86400 = 1 сутки

int                 timer_cycle = 8;         // Цикл таймера в сек
int         sch_size = 0;        // Текущий размер расписания

bool IsDev = false;
bool IsSensorsOn = false;

char number = -1;

char  command_fragment;

String command = "";

bool  next_step = false;
volatile bool  wakeup_timer = false;
volatile bool  wakeup_ex = false;
volatile long  cycle = cycle_time_def;   // Тут будем декрементить

#define ON  LOW
#define OFF HIGH                          

typedef void(*schedule_type)(long);                      // новый тип - указатель на функцию
struct schedule_cell
{
  schedule_type func;
  long          param;
  //String        id;
};
schedule_cell *schedule;                           // расписание - массив указателей на структуру
unsigned char step = 0; // шаг

void Timer1_action();
void DoDevCommand(String _command);

//--------------------------------
struct EEPROMcell
{
public:

  byte  id;    // id цветка
  char  cname[11];  // имя цветка (10 символов макс)
  byte  valve_pin; // пин клапана
  byte  hygro_pin; // пин гигрометра
  int   crit_wet;  // критическая влажность

  EEPROMcell()
  {
  }

  EEPROMcell(const Plant& _plant)
  {
    id = _plant.getId();
    String tmpname = _plant.getName();
    for (int i = 0; i < 11; i++)
    {
      cname[i] = tmpname[i];
    }
    valve_pin = _plant.getValve();
    hygro_pin = _plant.getHygrometer().getPin();
    crit_wet = _plant.getCritWet();
  }

  EEPROMcell(byte _id, char _name[], byte _valve_pin, byte _hygro_pin, int _crit_wet)
  {
    id = _id;
    for (int i = 0; i < 11; i++)
    {
      cname[i] = _name[i];
    }
    valve_pin = _valve_pin;
    hygro_pin = _hygro_pin;
    crit_wet = _crit_wet;
  }
};

Plant PlantFromEEPROMcell(struct EEPROMcell _cell)
{
  Plant tmpPlant;
  tmpPlant.setId(_cell.id);
  tmpPlant.setName(_cell.cname);
  tmpPlant.setValve(_cell.valve_pin);
  tmpPlant.setHygrometer(_cell.hygro_pin);
  tmpPlant.setCritWet(_cell.crit_wet);
  pinMode(tmpPlant.getValve(), OUTPUT);
  return tmpPlant;
}

//--------------------------------------------------------------SETUP--------------------------------------------------------------
void setup()
{
  //eraseEEPROM();
  Serial.begin(115200);                    // Скорость (бит/c) , для отладки
  Serial.setTimeout(80);
  pinMode(pump, OUTPUT);                   // Устанавливаем пин Pump
  pinMode(relay_Sensors, OUTPUT);          // Устанавливаем пин реле датчиков

  Serial << "Setup begin" << endl;

  devices = new LinkedList<byte>();    // Цифровые устройства
  sensors = new LinkedList<Sensor>();    // Сенсоры
  flowers = new LinkedList<Plant>();     // Цветы 

  devices->add(pump);
  devices->add(relay_Sensors);
  sensors->add(waterSensor);

  Serial << "EEPROM begin" << endl;
  //printEEPROM(40);
  loadEEPROMdata();
  Serial << "EEPROM end" << endl;

  pinMode(interrupt_pin, INPUT);                  // порт прерывания как выход
  set_sleep_mode(SLEEP_MODE_IDLE);                // Определяем режим сна

  cycle = cycle_time_def;
  Timer1.initialize(timer_cycle * 1000000);       // timer_cycle сек
  Timer1.attachInterrupt(Timer1_action);          // прерывание на таймере должно быть всегда

  IsDev = false;
  schedule = nullptr;
  next_step = true;
  step = 0;

  Serial << "Setup end" << endl;

  Serial << "-------------" << endl;
  Serial << "flowers: " << flowers->size() << endl;
  Serial << "sensors: " << sensors->size() << endl;
  Serial << "devices: " << devices->size() << endl;
  Serial << "-------------" << endl;

  delay(6000);
  // flowers - массив считанных цветков
  // devices - массив клапанов + помпа + реле ( чтобы все отключить)
  // sensors - массив water_sensor + гигрометр 
}
//-------------------------------------------------------------/SETUP--------------------------------------------------------------
void eraseLocalData()
{
  flowers_count = 0;
  if (flowers->size() > 0)
  {
    flowers->clear();
  }
  if (devices->size() > 0)
  {
    devices->clear();
  }
  if (sensors->size() > 0)
  {
    sensors->clear();
  }
  devices->add(pump);
  devices->add(relay_Sensors);
  sensors->add(waterSensor);
}

void printEEPROM(int lengt)
{
  for (int i = 0; i < lengt; i++)
  {
    Serial << EEPROM.read(i) << ".";
  }
  Serial << endl;
}

void loadEEPROMdata()
{
  flowers_count = EEPROM.read(0);        // Узнаем кол-во сохраненных цветков
  if (flowers_count == 255)
  {
    flowers_count = 0;
  }
  int local_flowers = flowers_count;
  int iter = 1;
  for (int i = 1; ((i < 1023) && (local_flowers > 0)); i += sizeof(struct EEPROMcell))
  {
    if (EEPROM.read(i) != 255)
    {
      EEPROMcell memoryCell;
      EEPROM.get(i, memoryCell);

      Serial.print("cell.id : ");
      Serial.println(memoryCell.id);
      Serial.print("cell.name : ");
      Serial.println(memoryCell.cname);
      Serial.print("cell.v : ");
      Serial.println(memoryCell.valve_pin);
      Serial.print("cell.h : ");
      Serial.println(memoryCell.hygro_pin);
      Serial.print("cell.w : ");
      Serial.println(memoryCell.crit_wet);

      Plant newPlant;
      newPlant = PlantFromEEPROMcell(memoryCell);
      newPlant.init();
      flowers->add(newPlant);
      devices->add(newPlant.getValve());
      sensors->add(newPlant.getHygrometer());
      local_flowers--;
    }
  }
}

void eraseEEPROM()
{
  for (int i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.update(i, 255);
  }
}

void addNewFlower(Plant _newFlower)
{
  // обновим запись в памяти о количестве цветков
  flowers_count = EEPROM.read(0);
  if (flowers_count == 255)
  {
    flowers_count = 1;
  }
  else
  {
    flowers_count++;
  }
  EEPROM.write(0, flowers_count);

  // запишем цветок в первую свободную ячейку
  for (int i = 1; i < EEPROM.length(); i += sizeof(EEPROMcell))
  {
    if (EEPROM[i] == 255)
    {
      EEPROMcell newCell = _newFlower;
      EEPROM.put(i, newCell);
      break;
    }
  }

  // добавим цветок
  _newFlower.init();
  flowers->add(_newFlower);
  devices->add(_newFlower.getValve());
  sensors->add(_newFlower.getHygrometer());
}

void removeFlower(int id)
{
  flowers_count = EEPROM.read(0);
  if (flowers_count == 1)
  {
    flowers_count = 255;
  }
  else
  {
    flowers_count--;
  }
  EEPROM.write(0, flowers_count);

  for (int i = 1; i < EEPROM.length(); i += sizeof(EEPROMcell))
  {
    if (EEPROM[i] == id)
    {
      EEPROM.write(i, 255);
    }
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

void SensorsOn()
{
  digitalWrite(relay_Sensors, ON);
  IsSensorsOn = true;
  Serial.println("SENSORS ON");
}

void SensorsOff()
{
  digitalWrite(relay_Sensors, OFF);
  IsSensorsOn = false;
  Serial.println("SENSORS OFF");
}

int devicesFoundPosition(byte obj)
{
  for (int i = 0; i < devices->size(); i++)
  {
    if (devices->get(i) == obj)
    {
      return i;
    }
  }
  return -1;
}

int sensorsFoundPosition(byte pin)
{
  for (int i = 0; i < sensors->size(); i++)
  {
    if (sensors->get(i).getPin() == pin)
    {
      return i;
    }
  }
  return -1;
}

int flowersFoundPosition(int _id)
{
  for (int i = 0; i < flowers->size(); i++)
  {
    if (flowers->get(i).getId() == _id)
    {
      return i;
    }
  }
  return -1;
}

void ValveON(long N)
{
  Serial << "ValveOn" << endl;
  digitalWrite(devices->get(devicesFoundPosition((byte)N)), ON);
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
  digitalWrite(devices->get(devicesFoundPosition((byte)N)), OFF);
  next_step = true;
}
void PumpOFF(long N)
{
  Serial << "PumpOFF" << endl;
  digitalWrite(pump, OFF);
  next_step = true;
}

void Sleep(long sec = cycle_time_def)
{
  next_step = false;
  Serial << "Sleep" << endl;
  Serial << "sec = " << sec << endl;
  Serial << "tim_cycle = " << timer_cycle << endl;

  cycle = sec;
  wakeup_timer = false;
}

void WaterPlant(int _N, int &i)
{
  schedule[i].func = ValveON;
  schedule[i].param = _N;
  //schedule[i].id = "v";

  schedule[i + 1].func = PumpON;
  schedule[i + 1].param = 0;
  //schedule[i + 1].id = "p";

  schedule[i + 2].func = Sleep;
  schedule[i + 2].param = pumpTime;
  //schedule[i + 2].id = "s";

  schedule[i + 3].func = PumpOFF;
  schedule[i + 3].param = 0;
  //schedule[i + 3].id = "/p";

  schedule[i + 4].func = ValveOFF;
  schedule[i + 4].param = _N;
  //schedule[i + 4].id = "/v";

  i = i + 4;
}

void ResetSchedule(long o = 0)
{
  //Serial << "ResetSchedule" << endl;
  delete[] schedule;
  schedule = nullptr;
  next_step = true;
}

void DevicesOFF()
{
  for (int i = 0; i < devices->size(); i++)
  {
    digitalWrite(devices->get(i), OFF);
  }
}

void CheckSensors()
{
  if (flowers->size() > 0)
  {
    SensorsOn();
    delay(20);

    for (int i = 0; i < flowers->size(); i++)
    {
      flowers->get(i).ask();
    }

    Serial.println("-- WS :");
    Serial.println(sensors->get(sensorsFoundPosition(waterSensor)).ask());
    Serial.println("");
    delay(20);
    SensorsOff();
  }
}

void ParceCommand()
{
  command = "";
  while (Serial.available() > 0)
  {
    command_fragment = Serial.read();
    command += command_fragment;
  }
  if (command.length() > 0)
  {
    Serial << "<-- " << command;
  }
  /*
  delay(80);
  command = "";
  while (command_fragment != ';')
  {
  // команда это строка до ;
  if (Serial.available() > 0)
  {
  command_fragment = Serial.read();
  command += command_fragment;
  }
  Serial << command_fragment << endl;
  delay(100);
  }*/
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
  for (int i = 0; i < flowers->size(); i++)
  {
    if (flowers->get(i).getStat() > flowers->get(i).getCritWet())
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

void CalculateCustomSceduleSize(int need_water_count = -1)
{
  // -1 => water all
  sch_size = 1; // в конце delete
  if (need_water_count == -1)
  {
    sch_size += flowers->size() * 5;
  }
  else
  {
    sch_size += need_water_count * 5;
  }
}

void ScheduleBuilder(String mode = "default", int plant = -1)
{
  step = 0;

  if (mode == "default")
  {
    Serial << "default mode" << endl;
    CalculateScheduleSize();
    Serial << "flowers size = " << flowers->size() << endl;
    Serial << "sch size = " << sch_size << endl;

    schedule = new schedule_cell[sch_size];
    int cell = 0;
    for (int N = 0; N < flowers->size(); N++)
    {
      if (flowers->get(N).getStat() > flowers->get(N).getCritWet())
      {
        WaterPlant(N, cell);
        cell++;
      }
    }
    schedule[sch_size - 3].func = PrepareToLongSleep;
    schedule[sch_size - 3].param = 0;

    schedule[sch_size - 2].func = Sleep;
    schedule[sch_size - 2].param = cycle_time_def;

    schedule[sch_size - 1].func = ResetSchedule;
    schedule[sch_size - 1].param = 0;
  }
  if (mode == "water_plant")
  {
    Serial << "water_plant mode" << endl;
    DevicesOFF();
    ResetSchedule();
    Serial << "Deleted OLD schedule" << endl;
    CalculateCustomSceduleSize(1);
    int cell = 0;

    schedule = new schedule_cell[sch_size];
    WaterPlant(plant, cell);
    schedule[sch_size - 1].func = ResetSchedule;
    schedule[sch_size - 1].param = 0;
    next_step = true;
  }
  if (mode == "water_all_if_need")
  {
    CalculateCustomSceduleSize();
    Serial << "sch size = " << sch_size << endl;

    schedule = new schedule_cell[sch_size];
    int cell = 0;
    for (int N = 0; N < flowers->size(); N++)
    {
      if (flowers->get(N).getStat() > flowers->get(N).getCritWet())
      {
        WaterPlant(N, cell);
        cell++;
      }
    }

    schedule[sch_size - 1].func = ResetSchedule;
    schedule[sch_size - 1].param = 0;
    next_step = true;
  }

  if (mode == "water_all_force")
  {
    Serial << "water_all_force mode" << endl;
    DevicesOFF();
    ResetSchedule();
    Serial << "Deleted OLD schedule" << endl;
    CalculateCustomSceduleSize();
    int cell = 0;

    schedule = new schedule_cell[sch_size];
    for (int N = 0; N < flowers->size(); N++)
    {
      WaterPlant(N, cell);
      cell++;
    }
    schedule[sch_size - 1].func = ResetSchedule;
    schedule[sch_size - 1].param = 0;
    next_step = true;
  }

  Serial << "Schedule built" << endl;
  delay(20);
}

void DoCommand(String _command)
{
  switch (_command.charAt(0))
  {

  case 'd':
  {
    eraseEEPROM();
    //DoDevCommand(_command);
    break;
  }

  case 'w':
  {
    // полить что-то
    switch (_command.charAt(1))
    {
    case 'f':
    {
      // полить все принудительно
      ScheduleBuilder("water_all_force");
      break;
    }
    case ';':
    {
      // полить все, если необходимо
      ScheduleBuilder("water_all_if_need");
      break;
    }
    default:
      // полить конкретный цветок
      byte flowerNumber = _command.substring(1, _command.length()).toInt();
      ScheduleBuilder("water_plant", flowerNumber);
      break;
    }
    break;
  }
  case 'c':
  {
    // проверить что-то
    CheckSensors();
    if (_command.charAt(1) == ';')
    {
      // отправка значений всех сенсоров
    }
    else
    {
      // отправить значение конкретного цветка
      int fId = _command.substring(1, command.length()-1).toInt();
      int wetness = flowers->get(flowersFoundPosition(fId)).getHygrometer().getVal();
      String cmd = ".u:" + (String)fId + ":" + "w" + ":" + (String)wetness + ";";
      Serial << cmd << endl;
    }
    break;
  }

  case 'r':
  {
    // удалить что-то

    if (_command.charAt(1) == ';')
    {
      // стереть все данные
      eraseLocalData();    // стираем все массивы
      eraseEEPROM();   // стираем всю память
      ResetSchedule(); // обновим расписание
      Serial << "." << _command << endl;
    }
    else
    {
      // стереть конкретный цветок
      //byte id = _command.substring(1).toInt;

    }
    break;
  }
  case 'u':
  {
    // получить список цветков
    for (int i = 0; i < flowers->size(); i++)
    {
      String cmd = ".f" + (String)flowers->get(i).getId() + ":" + (String)flowers->get(i).getName() + ":" + (String)flowers->get(i).getValve() + ":" + (String)flowers->get(i).getHygrometer().getPin() + ":" + (String)flowers->get(i).getCritWet() + ";";
      Serial << cmd << endl;
    }
    Serial << ".f;" << endl;
    break;
  }

  case 'f' :
  {
    Plant *newFlower = new Plant();    
    String clearCommand = _command.substring(1);

    Serial << "Clear: "<< clearCommand <<endl;
      
    int n = 0;
    int iter = 0;
    int pos = clearCommand.indexOf(':', n);
    
    Serial << "New flower arrived" << endl;
    Serial.flush();
    
    while (pos != -1)
    {
      switch (iter)
      {
      case 0:
      {
        Serial << "Id: "<< clearCommand.substring(n, pos).toInt() << endl;
        Serial.flush();
        newFlower->setId(clearCommand.substring(n, pos).toInt());
        n = pos+1;
        iter++;
        break;
      }
      case 1:
      {
        Serial << "Name: "<< clearCommand.substring(n, pos) << endl;
        Serial.flush();
        newFlower->setName(clearCommand.substring(n, pos));
        n = pos + 1;
        iter++;
        break;
      }
      case 2:
      {
        Serial << "Valve: "<< clearCommand.substring(n, pos).toInt() << endl;
        Serial.flush();
        newFlower->setValve(clearCommand.substring(n, pos).toInt());
        n = pos + 1;
        iter++;
        break;
      }
      case 3:
      {
        Serial << "Hygrometer: "<< clearCommand.substring(n, pos).toInt() << endl;
        Serial.flush();
        newFlower->setHygrometer(clearCommand.substring(n, pos).toInt());
        n = pos + 1;
        iter++;
        break;
      }
      }
      pos = clearCommand.indexOf(':', n);
    }
    pos = clearCommand.indexOf(';', n);
    Serial << "CritWet: "<< clearCommand.substring(n, pos).toInt() << endl;
    Serial.flush();
    newFlower->setCritWet(clearCommand.substring(n, pos).toInt());

    addNewFlower(*newFlower);
    ResetSchedule();
    Serial << "." <<_command; // отправить назад телефону
    Serial << ".f;"<<endl;
    break;
  }

  default:
    // хз что пришло
    break;
  }
}

void DoDevCommand(String _command)
{
  /*
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
  }*/
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
  attachInterrupt(digitalPinToInterrupt(2), EX_Action, LOW);  //Если на 0-вом прерываниии - ноль, то просыпаемся.

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

  detachInterrupt(digitalPinToInterrupt(2));          // Отключаем прерывания по BT

  Serial.println("WakeUp");       // Проснулись
  delay(80);                     // Ждем пока все проснется
}

//-------------------------------------------------------------LOOP--------------------------------------------------------------
void loop()
{
  //Serial << "New Loop" << endl;
  if (wakeup_ex)
  {
    wakeup_ex = false;
    //Serial << "Wakeup EX" << endl;
    ParceCommand();
    if (command[0] == '.') DoCommand(command.substring(1));
  }
  if (wakeup_timer || next_step)
  {
    next_step = false;
    wakeup_timer = false;

    //Serial << "Wakeup TIMER / NEXT STEP" << endl;

    if (schedule == nullptr)
    {
      //Serial << "No schedule" << endl;
      ScheduleBuilder();
    }

    //Serial << "Step # " << step << endl;
    schedule[step].func(schedule[step].param);
    step++;                       // step указывает на следущую выполненную инструкцию
                              //Serial << "/ Step # " << step - 1 << endl;
  }
  else
  {
    //Serial << "Sleep section" << endl;

    next_step = false;
    wakeup_timer = false;

    //Serial << cycle << " sec" << endl;
    //Serial << timer_cycle << " timer cycle" << endl;
    delay(100);

    if (Serial.available() > 0)
    {
      wakeup_ex = true;
    }
    else
    {
      // если в EX ничего нет то идем спать
      EnterSleep();
      attachInterrupt(digitalPinToInterrupt(2), EX_Action, LOW);
    }
    //Serial << "/Sleep section" << endl;
  }
  Serial << "/ Loop" << endl;
}
//------------------------------------------------------------/LOOP--------------------------------------------------------------

