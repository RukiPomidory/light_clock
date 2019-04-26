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

// Предыдущее показание кнопки энкодера
bool prevSW;

// Номер текущего окна настроек
byte window;
// 0 - окно не выбрано
// 1 - настройка времени
// 2 - настройка будильника


// Изменяется ли сейчас время
bool timeChanging = false;

// Какой сектор времени изменяется
byte sector;
// 0 - день
// 1 - месяц
// 2 - год
// 3 - минуты
// 4 - часы

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
        // Сигнал dt низкий, крутимся по часовой стрелке
        if (!dt)
        {
            counter++;
            clickClock();
        }
        else    // А тут сигнал высокий, значит против часовой
        {
            counter--;
            clickReverse();
        }
        D("counter: ");
        D_LN(counter);
    }
    
    prevClk = clk;
}


// Клик по часовой стрелке
void clickClock()
{
    if (settings)
    {
        switch(window)
        {
            case 0:
                displaySettings();
                break;
            
            case 1:
                displaySetTime();
                break;
            
            case 2:
                displaySetAlarm();
                break;
        }
    }
} // void clickClock()


// Клик против часовой стрелки
void clickReverse()
{
    clickClock();
} // void clickReverse()


// Обработка выбранной опции
void selectOption()
{
    if (!settings)
    {
        counter = 0;
        displaySettings();
        settings = true;
        return;
    }
    else if (0 == window)
    {
        switch(counter)
        {
            case 0:
                D_LN("wanna set time");
                displaySetTime();
                window = 1;
                return;
                
            case 1:
                D_LN("wanna set alarm");
                displaySetAlarm();
                window = 2;
                return;
                
            case 2:
                D_LN("wanna go home");
                window = 0;
                displayMain();
                settings = false;
                return;
        }
    }
    else
    {
        switch(window)
        {
            case 1:
                if (5 == counter)
                {
                    window = 0;
                    displaySettings();
                }
                else
                {
                    timeChanging = true;
                    timeSector = (byte) counter;
                }
                break;
                
            default:
                D(window);
                D(" ");
                D_LN("UNDER CONSTRUCTION");
        }
    }
    
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

    display.setCursor(18, 14);
    display.print("ASlab");

    display.setTextSize(2);
    display.setCursor(68, 0);
    display.print("23:41");

    unsigned long start = millis();
    display.display();

    unsigned long lapsed = millis() - start;
    D_LN(lapsed);
} // void displayAll()

void displaySettings()
{
    int selectOffset = 16;

    counter = (3 + counter) % 3;
    D("s");
    D_LN(counter);
    
    display.clearDisplay();
    
    display.setTextSize(1);
    display.setTextColor(WHITE);

    display.setCursor(selectOffset, counter * 10);
    display.print(">");
    display.setCursor(122 - selectOffset, counter * 10);
    display.print("<");
    
    display.setCursor(32, 0);
    display.print("set time");
    
    display.setCursor(32, 10);
    display.print("set alarm");

    display.setCursor(32, 20);
    display.print("exit");
    
    display.display();
} // void displaySettings()


void displaySetTime()
{
    counter = (6 + counter) % 6;
    D("t ");

    display.clearDisplay();
    
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);

    int data[5] = {26, 04, 19, 23, 41};

    for (int i = 0; i < sizeof(data) / sizeof(data[0]); i++)
    {
        String value;
        if (data[i] < 10)
        {
            value = "0" + String(data[i]);
        }
        else
        {
            value = String(data[i]);
        }

        if (i == counter)
        {
            display.setTextColor(BLACK, WHITE);
        }
        else
        {
            display.setTextColor(WHITE);
        }
        
        display.print(value);
        
        if (2 == i || 4 == i)
        {
            display.println();
        }
        else
        {
            display.setTextColor(WHITE);
            if (i < 2)
            {
                display.print('.');
            }
            else
            {
                display.print(':');
            }
        }
    }

    if (5 == counter)
    {
        display.setTextColor(BLACK, WHITE);
    }
    else
    {
        display.setTextColor(WHITE);
    }

    display.setTextSize(1);
    display.setCursor(80, 18);
    display.print(" exit ");
    
    display.display();
    
} // void displaySetTime()


void displaySetAlarm()
{
    
} // void displaySetAlarm()

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
    prevSW = digitalRead(ENCODER_SW);
    window = 0;
    
    displayMain();
} // void setup()

void loop()
{
    // Обновление значений, зависящих от вращения энкодера
    checkEncoder();

    // Обработка нажатия кнопки энкодера
    bool encoderSW = digitalRead(ENCODER_SW);
    if (!encoderSW && prevSW)
    {
        D_LN("click");
        // Обработка выбранной опции
        selectOption();
    }

    prevSW = encoderSW;
    delay(1);
}


