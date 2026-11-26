#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BH1750.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Пины подключенных модулей
#define SOIL_PIN 34      // Влажность почвы (ADC1)
#define WATER_LVL_PIN 25 // Поплавок (LOW = вода есть, HIGH = пусто)
#define SERVO_PIN 19     // Сервопривод SG90
#define PUMP_PIN 18      // Модуль D4184 (помпа)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BH1750 lightMeter;

bool hasLightSensor = false;
int targetAngle = 0;
int soilValue = 0;
float luxValue = 0;
bool isTankEmpty = false;
bool isPumpActive = false;

unsigned long lastServoPulse = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastCycleStep = 0;
unsigned long pumpStartTime = 0;

void sendServoPulse(int angle)
{
  int pulseWidthUs = map(angle, 0, 180, 550, 2400);
  digitalWrite(SERVO_PIN, HIGH);
  delayMicroseconds(pulseWidthUs);
  digitalWrite(SERVO_PIN, LOW);
}

void renderOLED()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Шапка
  display.setCursor(0, 0);
  display.print("AGRO STATION");
  if (isPumpActive)
  {
    display.print(" [PUMP!]");
  }
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // 1. Почва и Свет
  display.setCursor(0, 13);
  display.print("Soil: ");
  display.print(soilValue);
  display.print("  Lux: ");
  if (hasLightSensor)
  {
    display.print((int)luxValue);
  }
  else
  {
    display.print("ERR");
  }

  // 2. Бак с водой
  display.setCursor(0, 25);
  display.print("Tank: ");
  if (isTankEmpty)
  {
    display.println("EMPTY (Pump LOCK)");
  }
  else
  {
    display.println("OK (Ready)");
  }

  display.drawLine(0, 36, 127, 36, SSD1306_WHITE);

  // 3. Форточка
  display.setCursor(0, 41);
  display.print("Vent angle: ");
  display.print(targetAngle);
  display.println(" deg");

  int barWidth = map(targetAngle, 0, 90, 0, 124);
  display.drawRect(0, 53, 128, 9, SSD1306_WHITE);
  display.fillRect(2, 55, barWidth, 5, SSD1306_WHITE);

  display.display();
}

void setup()
{
  Serial.begin(115200);

  pinMode(SOIL_PIN, INPUT);
  pinMode(WATER_LVL_PIN, INPUT_PULLUP);
  pinMode(SERVO_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  digitalWrite(SERVO_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println("Ошибка инициализации OLED!");
    for (;;)
      ;
  }

  // Запуск BH1750 (стандартный адрес 0x23)
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire))
  {
    hasLightSensor = true;
    Serial.println("BH1750 успешно запущен");
  }
  else
  {
    Serial.println("BH1750 не найден, проверьте SDA/SCL");
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 28);
  display.println("SYSTEM STARTING...");
  display.display();
  delay(1000);
}

void loop()
{
  unsigned long now = millis();

  // 1. Импульсы на сервопривод каждые 20 мс
  if (now - lastServoPulse >= 20)
  {
    lastServoPulse = now;
    sendServoPulse(targetAngle);
  }

  // 2. Автоматическое выключение помпы через 1.5 секунды после старта
  if (isPumpActive && (now - pumpStartTime >= 1500))
  {
    isPumpActive = false;
    digitalWrite(PUMP_PIN, LOW);
    Serial.println("Помпа остановлена");
  }

  // 3. Опрос датчиков и обновление дисплея каждые 250 мс
  if (now - lastDisplayUpdate >= 250)
  {
    lastDisplayUpdate = now;
    soilValue = analogRead(SOIL_PIN);
    isTankEmpty = (digitalRead(WATER_LVL_PIN) == HIGH);
    if (hasLightSensor)
    {
      luxValue = lightMeter.readLightLevel();
    }
    renderOLED();
  }

  // 4. Тестовый цикл каждые 6 секунд: смена угла форточки и короткий тест помпы
  if (now - lastCycleStep >= 6000)
  {
    lastCycleStep = now;

    if (targetAngle == 0)
    {
      targetAngle = 45;
    }
    else if (targetAngle == 45)
    {
      targetAngle = 90;
      // Включаем помпу на 1.5 сек, только если в баке есть вода
      if (!isTankEmpty)
      {
        isPumpActive = true;
        pumpStartTime = now;
        digitalWrite(PUMP_PIN, HIGH);
        Serial.println("Тестовый запуск помпы...");
      }
    }
    else
    {
      targetAngle = 0;
    }
  }
}
