#include <Arduino.h>
#include <Stepper.h>
#include "EEPROM.h"
#include "RTC.h"

#define pwmA 5
#define pwmB 11
#define brakeA 9
#define brakeB 8
#define dirA 12
#define dirB 13
#define setTimePin A2

// --------------------------------- IMPORTANT ---------------------------------
bool SET_TIME_START = true;
// --------------------------------- IMPORTANT ---------------------------------

const byte powerLossPin = 2;
const byte interruptPin = 3;
RTCTime currentTime;
const int eeAdress = 1;

const int stepsPerRevolution = 400;
Stepper myStepper = Stepper(stepsPerRevolution, dirA, dirB);

int diffinmin = 0;
int currentmin = 0;
int currentsec = 0; // only for monitoring
int rotationCounter = 0;
bool schangeShow = true;
bool allOkay = true;

struct DateTime{
  int year;
  int month;
  int day;
  int dayofweek;
  int hour;
  int min;
  int sec;
};
DateTime savedDateTime;
DateTime showedDateTime;


Month convertMonth(int m) {
  switch (int(m)) {
    case 1: return Month::JANUARY;
    case 2: return Month::FEBRUARY;
    case 3: return Month::MARCH;
    case 4: return Month::APRIL;
    case 5: return Month::MAY;
    case 6: return Month::JUNE;
    case 7: return Month::JULY;
    case 8: return Month::AUGUST;
    case 9: return Month::SEPTEMBER;
    case 10: return Month::OCTOBER;
    case 11: return Month::NOVEMBER;
    case 12: return Month::DECEMBER;
  }
}

DayOfWeek convertDOW(int dow) {
  switch (int (dow)) {
    case 1: return DayOfWeek::MONDAY;
    case 2: return DayOfWeek::TUESDAY;
    case 3: return DayOfWeek::WEDNESDAY;
    case 4: return DayOfWeek::THURSDAY;
    case 5: return DayOfWeek::FRIDAY;
    case 6: return DayOfWeek::SATURDAY;
    case 0: return DayOfWeek::SUNDAY;
  }
}

RTCTime convert2RTC(DateTime date) {
  return RTCTime (date.day, convertMonth(date.month), date.year, date.hour, date.min, date.sec, convertDOW(date.dayofweek), SaveLight::SAVING_TIME_INACTIVE);
}

DateTime convert2DT(RTCTime time) {
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

void saveTime() {
  DateTime saveDateTime = convert2DT(currentTime);
  EEPROM.put(eeAdress, saveDateTime);
}

void printDate(RTCTime date, String text) {
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

void calculateMinDiff(int current, int saved) {
  int diff = current - saved;
  if (diff != 0) {
    diffinmin += diff;
  }  
}

void calculateDifference(DateTime current, DateTime saved) {
  diffinmin = 0;
  current.hour = current.hour > 12 ? current.hour-12 : current.hour;
  saved.hour = saved.hour > 12 ? saved.hour-12 : saved.hour;
  int diff = current.hour - saved.hour;

  if (diff != 0) {
    diffinmin += (diff) * 60;
  }
  calculateMinDiff(current.min, saved.min);
}

void stopRotate() {

  if (diffinmin > 0) {
    diffinmin--;
  } else if (diffinmin < 0) {
    diffinmin++;
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(pwmA, OUTPUT);
  pinMode(pwmB, OUTPUT);
  pinMode(brakeA, OUTPUT);
  pinMode(brakeB, OUTPUT);

  digitalWrite(pwmA, HIGH);
  digitalWrite(pwmB, HIGH);
  digitalWrite(brakeA, LOW);
  digitalWrite(brakeB, LOW);

  pinMode(setTimePin, INPUT);

  RTC.begin();
  // A fallback time object, for setting the time if there is no time to retrieve from the RTC.
  RTCTime mytime(13, Month::AUGUST, 2024, 15, 12, 00, DayOfWeek::THURSDAY, SaveLight::SAVING_TIME_INACTIVE);
  // Tries to retrieve time
  
  DateTime newDate;
  EEPROM.get(eeAdress, newDate);

  RTCTime savedTime = convert2RTC(newDate);
  RTC.getTime(savedTime); // lekéri a pontos időt

  pinMode(powerLossPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(powerLossPin), saveTime, RISING);
  
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), stopRotate, RISING);

  EEPROM.get(eeAdress, savedDateTime);
  calculateDifference(convert2DT(currentTime), savedDateTime);
  showedDateTime = savedDateTime;

  myStepper.setSpeed(60);

  if (!RTC.isRunning()) {
    // this means the RTC is waking up "as new"
    if (savedTime.getYear() == 2000) {
      allOkay = false;
    } else {
      RTC.setTime(savedTime);
    }
  }

  if (SET_TIME_START)
  {
    RTC.setTime(mytime);
    SET_TIME_START = false;
  }
  
}

void loop() {
  if (allOkay)
  {
    RTC.getTime(currentTime);
  
    if (currentmin != currentTime.getMinutes()) {
      calculateDifference(convert2DT(currentTime), showedDateTime);
      currentmin = currentTime.getMinutes();
      schangeShow = true;
    }

    // rotation direction
    if (diffinmin > 0) {
      myStepper.step(10);
    } else if (diffinmin < 0) {
      myStepper.step(-10);
    }

    if (diffinmin == 0 && schangeShow)
    {
      showedDateTime = convert2DT(currentTime);
      schangeShow = false;
    }

    // monitoring
    if (currentsec != currentTime.getSeconds())
    {
      currentsec = currentTime.getSeconds();
      printDate(convert2RTC(savedDateTime), "Saved time: ");
      printDate(convert2RTC(showedDateTime), "Showed time: ");
      printDate(currentTime, "Current time: ");

      Serial.print(diffinmin);
      Serial.println();
    }
  }
  
}