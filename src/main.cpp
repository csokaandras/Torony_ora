#include <Arduino.h>
#include "EEPROM.h"
#include "RTC.h"

#define HALL_0 A0
#define HALL_1 A1
#define HALL_2 A2
#define HALL_3 A3
#define HALL_MIN A4
#define HALL_HOUR A5

int motor_rotate = PIN_D8;
int motor_back = PIN_D7;

int incomingByte = 0; // for incoming serial data
String typed = "";
String prevTyped = "";
String errorMsg = "";
bool ErrWrite = false;
bool manual_rotate = false;

// --------------------------------- IMPORTANT ---------------------------------
bool SET_TIME_START = true;
// --------------------------------- IMPORTANT ---------------------------------

const byte powerLossPin = PIN_D2;

RTCTime currentTime;
const int eeAdress = 1;

int manual_rotate_count = 0;
int diffinmin = 0;
int prevdiff = 0;
int currentmin = 0;
int currentsec = 0; // only for monitoring
int rotationCounter = 0;

bool schangeShow = true;
bool allOkay = true;
bool monitorTime = false;
bool monitorSensor = false;

struct DateTime
{
  int year;
  int month;
  int day;
  int dayofweek;
  int hour;
  int min;
  int sec;
};

struct sensor
{
  int name; // thats the value what we use to calculate hour
  int LL;   // Lower Low limit
  int LH;   // Lower High limit
  int HL;   // Higher High limit
  int HH;   // Higher Low limit
  int value;
  int state;
};

RTCTime newSaveDateTime;
DateTime savedDateTime;
DateTime showedDateTime;

sensor s0 = {1, 10, 200, 700, 1000, 0};
sensor s1 = {2, 10, 200, 700, 1000, 0};
sensor s2 = {4, 10, 200, 700, 1000, 0};
sensor s3 = {8, 10, 200, 700, 1000, 0};
sensor sMin = {0, 0, 200, 700, 1000, 0};
sensor sHour = {0, 0, 200, 700, 1000, 0};

void checkSensor(sensor *s)
{
  if ((s->value >= s->LL && s->value < s->LH) || (s->value >= s->HL && s->value < s->HH))
  {
    s->state = 1;
  }
  else if (s->value < s->HL && s->value >= s->LH)
  {
    s->state = 0;
  }
  else
  {
    s->state = -1;
  }
}

int calculateHour(sensor *s0, sensor *s1, sensor *s2, sensor *s3)
{
  int hour = 0;

  checkSensor(s0);
  checkSensor(s1);
  checkSensor(s2);
  checkSensor(s3);

  if (s0->state == 1)
  {
    hour += 1;
  }
  if (s1->state == 1)
  {
    hour += 2;
  }
  if (s2->state == 1)
  {
    hour += 4;
  }
  if (s3->state == 1)
  {
    hour += 8;
  }
  // Serial.println(hour);
  return hour;
}

void logSensor(String text, sensor *s)
{
  Serial.print(text + " ");
  Serial.print(s->value);
  Serial.print(" - ");
  Serial.print(s->state);
  Serial.println();
}

Month convertMonth(int m)
{
  switch (int(m))
  {
  case 1:
    return Month::JANUARY;
  case 2:
    return Month::FEBRUARY;
  case 3:
    return Month::MARCH;
  case 4:
    return Month::APRIL;
  case 5:
    return Month::MAY;
  case 6:
    return Month::JUNE;
  case 7:
    return Month::JULY;
  case 8:
    return Month::AUGUST;
  case 9:
    return Month::SEPTEMBER;
  case 10:
    return Month::OCTOBER;
  case 11:
    return Month::NOVEMBER;
  case 12:
    return Month::DECEMBER;
  }
}

DayOfWeek convertDOW(int dow)
{
  switch (int(dow))
  {
  case 1:
    return DayOfWeek::MONDAY;
  case 2:
    return DayOfWeek::TUESDAY;
  case 3:
    return DayOfWeek::WEDNESDAY;
  case 4:
    return DayOfWeek::THURSDAY;
  case 5:
    return DayOfWeek::FRIDAY;
  case 6:
    return DayOfWeek::SATURDAY;
  case 0:
    return DayOfWeek::SUNDAY;
  }
}

RTCTime convert2RTC(DateTime date)
{
  return RTCTime(date.day, convertMonth(date.month), date.year, date.hour, date.min, date.sec, convertDOW(date.dayofweek), SaveLight::SAVING_TIME_INACTIVE);
}

DateTime convert2DT(RTCTime time)
{
  RTC.getTime(time);

  DateTime saveDateTime;
  saveDateTime.year = time.getYear();
  saveDateTime.month = Month2int(time.getMonth());
  saveDateTime.day = time.getDayOfMonth();
  saveDateTime.hour = time.getHour();
  saveDateTime.min = time.getMinutes();
  saveDateTime.sec = time.getSeconds();

  return saveDateTime;
}

void saveTime()
{
  DateTime saveDateTime = convert2DT(currentTime);
  EEPROM.put(eeAdress, saveDateTime);
}

void printDate(RTCTime date, String text)
{
  Serial.print(text);

  /* DATE */
  Serial.print(date.getYear());
  Serial.print("/");
  Serial.print(Month2int(date.getMonth()));
  Serial.print("/");
  Serial.print(date.getDayOfMonth());
  Serial.print(" - ");

  /* HOURS:MINUTES:SECONDS */
  Serial.print(date.getHour());
  Serial.print(":");
  Serial.print(date.getMinutes());
  Serial.print(":");
  Serial.println(date.getSeconds());
  return;
}

void calculateMinDiff(int current, int saved)
{

  int diff = current - saved;
  if (diff != 0)
  {
    diffinmin += diff;
  }
}

void calculateDifference(DateTime current, DateTime saved)
{
  diffinmin = 0;
  current.hour = current.hour > 12 ? current.hour - 12 : current.hour;
  saved.hour = saved.hour > 12 ? saved.hour - 12 : saved.hour;
  int diff = current.hour - saved.hour;

  if (diff != 0)
  {
    diffinmin += (diff) * 60;
  }
  calculateMinDiff(current.min, saved.min);
}

void showTimeDatas()
{
  Serial.println();
  printDate(convert2RTC(savedDateTime), "Mentett idő:        ");
  printDate(convert2RTC(showedDateTime), "Mechanika akt. idő: ");
  printDate(currentTime, "A pontos idő:       ");
}

void PrintVT100()
{
  Serial.println();
  Serial.println("          ▞                                                                                                                   ▞                ");
  Serial.println("┏━━━┓  ┏━━━━┓ ━━┳━━ ┏┓  ┏┓ ┏━━━━┓ ┏┓   ┰ ┏━━━━┓ ┏━━━━┓ ━━┳━━ ┏━━━━┓ ┏━━━━┓        ━━┳━━ ┏━━━━┓ ┏━━━━┓ ┏━━━━┓ ┏┓   ┰  ┓   ┏ ┏━━━━┓ ┏━━━━┓ ┏━━━━┓");
  Serial.println("┃   ┃  ┃    ┃   ┃   ┃┗┓┏┛┃ ┃    ┃ ┃┗┓  ┃ ┃    ┃ ┗┓       ┃   ┃    ┃ ┃    ┃          ┃   ┃    ┃ ┃    ┃ ┃    ┃ ┃┗┓  ┃  ┗┓ ┏┛ ┃    ┃ ┃    ┃ ┃    ┃");
  Serial.println("┣━━━┻┓ ┣━━━━┫   ┃   ┃ ┗┛ ┃ ┃    ┃ ┃ ┗┓ ┃ ┃    ┃  ┗━━┓    ┃   ┃    ┃ ┣━━━┳┛          ┃   ┃    ┃ ┣━━━┳┛ ┃    ┃ ┃ ┗┓ ┃   ┗┳┛  ┃    ┃ ┣━━━┳┛ ┣━━━━┫");
  Serial.println("┃    ┃ ┃    ┃   ┃   ┃    ┃ ┃    ┃ ┃  ┗┓┃ ┃    ┃     ┗┓   ┃   ┃    ┃ ┃   ┗┓          ┃   ┃    ┃ ┃   ┗┓ ┃    ┃ ┃  ┗┓┃    ┃   ┃    ┃ ┃   ┗┓ ┃    ┃");
  Serial.println("┗━━━━┛ ┚    ┖   ┸   ┸    ┸ ┗━━━━┛ ┸   ┗┛ ┗━━━━┛ ┗━━━━┛   ┸   ┗━━━━┛ ┸    ┸          ┸   ┗━━━━┛ ┸    ┸ ┗━━━━┛ ┸   ┗┛    ┸   ┗━━━━┛ ┸    ┸ ┚    ┖");
  Serial.println("☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲☴☱☲");
  Serial.println("                                                              Developed By: Csóka András");
  Serial.println("                                                               Designed By: Csóka Antal");
  Serial.println();
  showTimeDatas();
  int hour = calculateHour(&s0, &s1, &s2, &s3);
  Serial.print("A mechanika órája: ");
  Serial.println(hour);
}

void changeTime(DateTime time, bool forward)
{
  if (forward)
  {
    showedDateTime.min++;
    if (showedDateTime.min >= 60)
    {
      showedDateTime.hour++;
      showedDateTime.min = 0;
    }
  }
  else
  {
    showedDateTime.min--;
    if (showedDateTime.min <= 0)
    {
      showedDateTime.hour--;
      showedDateTime.min = 60;
    }
  }
  calculateDifference(convert2DT(currentTime), showedDateTime);
}

void onMin()
{
  if (diffinmin > 0)
  {
    changeTime(showedDateTime, true);
  }
  else if (diffinmin < 0)
  {
    changeTime(showedDateTime, false);
  }
}

void setup()
{
  Serial.begin(9600);

  pinMode(HALL_0, INPUT);
  pinMode(HALL_1, INPUT);
  pinMode(HALL_2, INPUT);
  pinMode(HALL_3, INPUT);
  pinMode(HALL_MIN, INPUT);

  pinMode(motor_rotate, OUTPUT);
  pinMode(motor_back, OUTPUT);

  RTC.begin();
  // A fallback time object, for setting the time if there is no time to retrieve from the RTC.
  RTCTime mytime(1, Month::JANUARY, 2025, 15, 00, 00, DayOfWeek::WEDNESDAY, SaveLight::SAVING_TIME_INACTIVE);
  // Tries to retrieve time

  DateTime newDate;
  EEPROM.get(eeAdress, newDate);

  RTCTime savedTime = convert2RTC(newDate);
  RTC.getTime(savedTime); // lekéri a pontos időt

  pinMode(powerLossPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(powerLossPin), saveTime, RISING);

  // az elmentett idő és az óra szerinti közt kiszámolja a különbséget
  EEPROM.get(eeAdress, savedDateTime);
  calculateDifference(convert2DT(currentTime), savedDateTime);
  showedDateTime = savedDateTime;

  if (!RTC.isRunning())
  {
    // ha az óra eleme lemerült
    if (savedTime.getYear() == 2000 && allOkay)
    {
      allOkay = false;
      errorMsg = "Lemerült az elem és nem tudtam visszanyerni a pontos időt! S T O P";
      ErrWrite = true;
    }
    else
    {
      RTC.setTime(savedTime);
    }
  }

  if (SET_TIME_START)
  {
    RTC.setTime(mytime);
    SET_TIME_START = false;
  }

  digitalWrite(motor_back, LOW);
  digitalWrite(motor_rotate, LOW);
}

void loop()
{
  int hour = 0;

  s0.value = analogRead(HALL_0);
  s1.value = analogRead(HALL_1);
  s2.value = analogRead(HALL_2);
  s3.value = analogRead(HALL_3);
  sMin.value = analogRead(HALL_MIN);
  sHour.value = analogRead(HALL_HOUR);

  if (allOkay)
  {
    RTC.getTime(currentTime);

    hour = calculateHour(&s0, &s1, &s2, &s3);

    if (currentmin != currentTime.getMinutes())
    {
      calculateDifference(convert2DT(currentTime), showedDateTime);
      currentmin = currentTime.getMinutes();
      schangeShow = true;
    }

    if (currentmin == 0 && (hour != currentTime.getHour() || hour != currentTime.getHour() - 12) && allOkay)
    {
      errorMsg = "Nem egyezik a mechanika és az elektronika ideje! S T O P";
      ErrWrite = true;
    }

    int prev_state = sMin.state;
    checkSensor(&sMin);
    if (sMin.state == 0 && prev_state == 1)
    {
      onMin();
    }

    // rotation direction
    if (diffinmin > 0)
    {
      // a 13-ason kiadjuk hogy HIGH
      // várunk picit
      // a 12-esen kiadjuk hogy LOW
      // forgatja előre amíg nem lesz 0 a diffinmin
      digitalWrite(motor_rotate, HIGH);
    }
    else if (diffinmin < 0)
    {
      // a 12-esen kiadjuk hogy HIGH
      // várunk picit
      // a 13-ason kiadjuk hogy HIGH
      // forgatja hátra (meghúzza a relét és ezzel visszafelé forgatja) amíg nem lesz 0 a diffinmin
      digitalWrite(motor_back, HIGH);
      // delay(500);
      digitalWrite(motor_rotate, HIGH);
    }
    else if (diffinmin == 0)
    {
      digitalWrite(motor_rotate, LOW);
      digitalWrite(motor_back, LOW);
    }

    if (diffinmin == 0 && schangeShow)
    {
      showedDateTime = convert2DT(currentTime);
      schangeShow = false;
    }

    if (manual_rotate)
    {
      int prev_state = sMin.state;
      checkSensor(&sMin);
      if (sMin.state == 0 && prev_state == 1)
      {

        if (manual_rotate_count > 0)
        {
          digitalWrite(motor_rotate, HIGH);

          manual_rotate_count--;
        }
        else if (manual_rotate_count < 0)
        {
          digitalWrite(motor_back, HIGH);
          // delay(500);
          digitalWrite(motor_rotate, HIGH);

          manual_rotate_count++;
        }
        else if (manual_rotate_count == 0)
        {
          digitalWrite(motor_rotate, LOW);
          digitalWrite(motor_back, LOW);

          manual_rotate = false;
        }
      }
    }

    // monitoring

    if (currentsec != currentTime.getSeconds() && monitorTime)
    {
      currentsec = currentTime.getSeconds();

      showTimeDatas();

      Serial.print(diffinmin);
      Serial.println();
    }

    if (monitorSensor)
    {
      logSensor("s0", &s0);
      logSensor("s1", &s1);
      logSensor("s2", &s2);
      logSensor("s3", &s3);

      checkSensor(&sMin);
      logSensor("min", &sMin);

      checkSensor(&sHour);
      logSensor("hour", &sHour);

      Serial.print("\nÉrzékelők álltal észlelt idő: ");
      Serial.print(hour);
      Serial.println();
    }

    if (Serial.available() > 0)
    {
      // read the incoming byte:
      incomingByte = Serial.read();
      // Serial.print(incomingByte);
      switch (incomingByte)
      {
      case 10:
        typed = "";
        prevTyped = "";
        Serial.println();
        break;

      case 27:
        // Serial.println("Monitorozás befejezve");
        Serial.println();
        monitorTime = false;
        monitorSensor = false;

        typed = "";
        prevTyped = "";
        break;

      case 13:
        if (prevTyped == "-d")
        {
          DateTime newTime;
          newTime.year = typed.substring(0, 4).toInt();
          newTime.month = typed.substring(5, 7).toInt();
          newTime.day = typed.substring(8, 10).toInt();
          newTime.hour = currentTime.getHour();
          newTime.min = currentTime.getMinutes();
          newTime.sec = currentTime.getSeconds();
          Serial.println();
          printDate(convert2RTC(newTime), "Új dátum: ");
          RTCTime newRtc = convert2RTC(newTime);
          RTC.setTime(newRtc);
        }

        if (prevTyped == "-t")
        {
          DateTime newTime;
          newTime.year = currentTime.getYear();
          newTime.month = convert2DT(currentTime).month;
          newTime.day = currentTime.getDayOfMonth();
          newTime.hour = typed.substring(0, 2).toInt();
          newTime.min = typed.substring(3, 5).toInt();
          newTime.sec = typed.substring(6, 7).toInt();
          Serial.println();
          printDate(convert2RTC(newTime), "Új idő: ");
          RTCTime newRtc = convert2RTC(newTime);
          RTC.setTime(newRtc);
        }
        // 2024/12/01-22:26:00

        if (prevTyped == "-r+")
        {
          if (prevTyped.length() >= 3)
          {
            manual_rotate_count = typed.toInt();
          }
          else
          {
            manual_rotate_count = 1;
          }

          manual_rotate = true;

          Serial.println();
        }

        if (prevTyped == "-r-")
        {
          if (prevTyped.length() >= 3)
          {
            manual_rotate_count = typed.toInt() * -1;
          }
          else
          {
            manual_rotate_count = -1;
          }

          manual_rotate = true;

          Serial.println();
        }
        break;

      default:
        Serial.print(char(incomingByte));
        typed += char(incomingByte);
        break;
      }

      if (typed == "VT100")
      {
        PrintVT100();
        typed = "";
      }

      if (typed == "-ciac")
      {
        Serial.println("\n");
        Serial.println(" ╱|、");
        Serial.println("(˚ˎ。7");
        Serial.println("|、˜〵");
        Serial.println("じしˍ,)ノ");
        Serial.println();
        typed = "";
      }

      if (typed == "-h\n")
      {
        Serial.println("\n");
        Serial.println("VT100   kezdőlap megjelenítése");
        Serial.println("-h      segítség kiírása");
        Serial.println("-m      idő folyamatos megjelenítésének bekapcsolása, megállításához nyomja meg az ESC gombot");
        Serial.println("-t      aktuális idő beállítása");
        Serial.println("-d      aktuális dátum beállítása");
        Serial.println("-s      érzékelők állapotának megjelenítése, megállításához nyomja meg az ESC gombot");
        Serial.println("-r-     motor léptetése előre (Ha nincs érték akkor 1-el)");
        Serial.println("-r+     motor léptetése hátra (Ha nincs érték akkor 1-el)");
        Serial.println();
        Serial.println("FONTOS");
        Serial.println("Ha törölni kell egy sorban akkor nyomjon ENTERT-t és kezdje előlről, nem tud karaktert törölni!");
        Serial.println();
        typed = "";
      }

      if (typed == "-m")
      {
        Serial.println();
        Serial.println("Megállításhoz nyomja meg az ESC gombot");
        Serial.println();
        monitorTime = true;
        typed = "";
      }

      if (typed == "-d")
      {
        Serial.println();
        Serial.println("Aktuális dátum beállítása");
        Serial.println("Elfogadáshoz nyomja meg a TAB gombot");
        Serial.println("Kérem ebben a formátumban adja meg ÉÉÉÉ/HH/NN");
        prevTyped = typed;

        typed = "";
      }

      if (typed == "-t")
      {
        Serial.println();
        Serial.println("Aktuális idő beállítása");
        Serial.println("Elfogadáshoz nyomja meg a TAB gombot");
        Serial.println("Kérem ebben a formátumban adja meg óó:pp:mm");
        prevTyped = typed;

        typed = "";
      }

      if (typed == "-s")
      {
        Serial.println();
        Serial.println("Megállításhoz nyomja meg az ESC gombot");
        Serial.println();
        monitorSensor = true;
        typed = "";
      }

      if (typed == "-r+")
      {
        Serial.println();
        Serial.println("A TAB lenyomásával lehet egyesével léptetni");
        Serial.println();

        prevTyped = typed;
        typed = "";
      }

      if (typed == "-r-")
      {
        Serial.println();
        Serial.println("A TAB lenyomásával lehet egyesével léptetni");
        Serial.println();

        prevTyped = typed;
        typed = "";
      }
    }
  }
  else
  {
    if (ErrWrite == true)
    {
      ErrWrite = false;
      Serial.println(errorMsg);
    }
  }
}