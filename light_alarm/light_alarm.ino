#include <iarduino_RTC.h>
#include <EEPROM.h>
#include <FastLED.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Закомментить строку ниже, чтобы выключить дебаг
#define DEBUG

#ifdef  DEBUG
#   define  D_INIT() { Serial.begin(115200); while(!Serial); }   
#   define  D(x)    { Serial.print(x); }
#   define  D_LN(x) { Serial.println(x); } 
#else
#   define  D_INIT()
#   define  D(x)
#   define  D_LN(x)
#endif  // #ifdef  DEBUG

// Состояние системы. Сохраняем его в долговременную память EEPROM
struct State
{
    // Время будильника, переведнное в минуты
    int alarmMin;
    
    // В какие дни включать
    byte days;
    
    // Установлен ли будильник
    bool enabled;
};


#define LED_COUNT 60    // Количество светодиодов в ленте
#define LED_PIN 13      // Пин, к которому подключена лента

#define FIRST_OFF 2     // Первая кнопка выключения
#define SECOND_OFF 3    // Вторая кнопка выключения

#define OLED_RESET 4    // Без понятия, что это вообще

#define ENCODER_DT  A1   // 
#define ENCODER_CLK A0   // Пины энкодера
#define ENCODER_SW  A2   //


// Часы реального времени
iarduino_RTC rtc(RTC_DS1302, 8, 9, 10);

// Светодиодная лента
CRGB led[LED_COUNT];

// OLED Дисплей 128x64
Adafruit_SSD1306 display(OLED_RESET);

// Координаты мигающих точек
const int blinkX = 92;
const int blinkY = 0;


// Надо ли мигать двоеточием
bool isBlinking = true;

// Итерация мигания
bool blinkIteration = true;

// Находится ли юзверь в настройках
bool settings = false;

// Значение для сравнения (энкодер)
bool prevClk;

// Счетчик кликов
int counter;

// Текущее время в минутах
int currentTime;

// Установка одного цвета на всю ленту
void setColor(int red, int green, int blue)
{
    for (int i = 0; i < LED_COUNT; i++)
    {
        led[i] = CRGB(red, green, blue);
    }
    FastLED.show();
}

// Проверка работоспособности ленты
void check()
{
    setColor(255, 0, 0);
    delay(300);

    setColor(0, 255, 0);
    delay(300);
    
    setColor(0, 0, 255);
    delay(300);

    setColor(0, 0, 0);
}

void checkEncoder()
{
    bool dt = digitalRead(ENCODER_DT);
    bool clk = digitalRead(ENCODER_CLK);
    
    // (обработка данных при спаде напряжения на clk)
    // Если уровень сигнала clk низкий
    // и в прошлый раз он был высокий
    if (!clk && prevClk)
    {
        // Сигнал dt высокий, крутимся по часовой стрелке
        if (dt)
        {
            counter++;
            clickClock();
        }
        else    // А тут сигнал низкий, значит против часовой
        {
            counter--;
            clickReverse();
        }
        D_LN(counter);
    }
    
    prevClk = clk;
}


// Клик по часовой стрелке
void clickClock()
{
    selectOption();
} // void clickClock()


// Клик против часовой стрелки
void clickReverse()
{
    selectOption();
} // void clickReverse()


// Обработка выбранной опции
void selectOption()
{
    D("SELECT [");
    D(counter);
    D_LN("]");
    
    display.clearDisplay();
    
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(2*counter, counter);
    display.print("26.04.19");
    display.display();
    
} // void selectOption()


void displayMain()
{
    display.clearDisplay();
    
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("26.04.19");

    display.setCursor(83, 18);
    display.print("06:10");

    display.setTextSize(2);
    display.setCursor(68, 0);
    display.print("23:41");

    unsigned long start = millis();
    display.display();

    unsigned long lapsed = millis() - start;
    D_LN(lapsed);
} // void displayAll()

void blink()
{
    display.setTextSize(2);
    display.setCursor(blinkX, blinkY);
    
    if (blinkIteration)
    {
        display.setTextColor(BLACK);
    }
    else
    {
        display.setTextColor(WHITE);
    }
    display.print(":");
    display.display();
    
    blinkIteration = !blinkIteration;
} // void blink()

void setup() 
{
    D_INIT();

    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    
    rtc.begin();
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(led, LED_COUNT);
    //EEPROM.get(State, 0);

    //check();
    displayMain();
} // void setup()

void loop()
{
    // Обновление значений, зависящих от вращения энкодера
    checkEncoder();

    // Обработка нажатия кнопки энкодера
    bool encoderBtn = digitalRead(ENCODER_SW);
    if (!encoderBtn)
    {
        // Обработка выбранной опции
        selectOption();
    }

}


