/*
Copyright (c) 2019 Arseniy Molodtsov
This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License.

Version 0.6
(27.04.2019)
*/

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
class State
{
    public:
        // Время будильника, переведнное в минуты
        int alarmMin;
        
        // В какие дни включать
        byte days;
        
        // Заведен ли будильник
        bool enabled;
};


#define LED_COUNT 60    // Количество светодиодов в ленте
#define LED_PIN 4      // Пин, к которому подключена лента

#define FIRST_OFF 2     // Первая кнопка выключения
#define SECOND_OFF 3    // Вторая кнопка выключения

#define OLED_RESET 4    // Без понятия, что это вообще

#define ENCODER_DT  A1   // 
#define ENCODER_CLK A0   // Пины энкодера
#define ENCODER_SW  A2   //


// Часы реального времени
iarduino_RTC rtc(RTC_DS1302, 7, 8, 9);

// Светодиодная лента
CRGB led[LED_COUNT];

// OLED Дисплей 128x64
Adafruit_SSD1306 display(OLED_RESET);

// Состояние системы
State state;

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
byte timeSector;
// 0 - день
// 1 - месяц
// 2 - год
// 3 - минуты
// 4 - часы


// Изменяется ли время будильника
bool alarmChanging = false;

// Какой сектор будильника меняется
byte alarmSector;
// 0 - часы
// 1 - минуты
// 2 - вкл/выкл

// Последняя установленная яркость ленты
int prevBrightness = 0;

// Счетчик кликов
int counter;

// Контроль времени опроса rtc модуля
unsigned long startGlobal;

/*
 *  Массив с данными о дате и времени
 */
int dataTime[5];
// 0 - день
// 1 - месяц
// 2 - год
// 3 - часы
// 4 - минуты

// Переводит число в строку длиной не менее 2 символов
String to2d(int digit)
{
    if (digit < 10)
    {
        return "0" + String(digit);
    }
    
    return String(digit);
}

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
void ledTest()
{
    setColor(255, 0, 0);
    delay(300);

    setColor(0, 255, 0);
    delay(300);
    
    setColor(0, 0, 255);
    delay(300);

    FastLED.setBrightness(0);
    setColor(255, 255, 255);
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
            counter = 0;
            
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
                if (timeChanging)
                {
                    timeChanging = false;
                    counter = timeSector;
                    rtc.settime(-1, dataTime[4], dataTime[3], dataTime[0], dataTime[1], dataTime[2]);
                    displaySetTime();
                }
                else if (5 == counter)
                {
                    window = 0;
                    displaySettings();
                }
                else
                {
                    timeChanging = true;
                    timeSector = (byte) counter;
                    counter = dataTime[timeSector];
                    displaySetTime();
                }
                break;

            case 2:
                if (alarmChanging)
                {
                    alarmChanging = false;
                    counter = alarmSector;
                    EEPROM.put(0, state);
                    displaySetAlarm();
                }
                else if (3 == counter)
                {
                    window = 0;
                    counter = 2; // 2 - это id кнопки выхода
                    displaySettings();
                }
                else
                {
                    alarmChanging = true;
                    alarmSector = (byte) counter;
                    switch(alarmSector)
                    {
                        // Часы
                        case 0:
                            counter = state.alarmMin / 60;
                            break;

                        // Минуты
                        case 1:
                            counter = state.alarmMin % 60;
                            break;

                        // Вкл/выкл
                        case 2:
                            state.enabled = !state.enabled;
                            alarmChanging = false;
                            break;
                    }
                    displaySetAlarm();
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
    String date = to2d(dataTime[0]) + '.' +
                to2d(dataTime[1]) + '.' +
                to2d(dataTime[2]);
    display.print(date);

    display.setCursor(83, 18);
    int minute = state.alarmMin % 60;
    int hour = state.alarmMin / 60;
    String alarmTime = to2d(hour) + ':' +
                to2d(minute);
    display.print(alarmTime);
    

//    display.setCursor(0, 16);
//    display.setTextColor(BLACK, WHITE);
//    display.print("MTW");
//    display.setTextColor(WHITE);
//    display.print("TFSS");

    display.setTextSize(2);
    display.setCursor(68, 0);
    String time = to2d(dataTime[3]) + ':' +
                to2d(dataTime[4]);
    display.print(time);

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
    display.clearDisplay();
    
    if (timeChanging)
    {
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(110, 4);
        display.print("</>");
        
        int limit = 6;
        
        switch(timeSector)
        {
            // День
            // TODO: Зависимость дня от месяца
            case 0:
                limit = 32;
                break;

            // Месяц
            case 1:
                limit = 13;
                break;

            // Год
            case 2:
                limit = 100;
                break;

            // Часы
            case 3:
                limit = 24;
                break;
                
            // Минуты
            case 4:
                limit = 60;
                break;            
        }

        counter = (limit + counter) % limit;
    }
    else
    {
        counter = (6 + counter) % 6;
        D("t ");

        timeSector = counter;
    }
    
    
    display.setTextSize(2);
    display.setCursor(0, 0);

    for (int i = 0; i < sizeof(dataTime) / sizeof(dataTime[0]); i++)
    {
        if (i == timeSector)
        {
            display.setTextColor(BLACK, WHITE);
            
            if (timeChanging)
            {
                dataTime[i] = counter;
            }
        }
        else
        {
            display.setTextColor(WHITE);
        }

        display.print(to2d(dataTime[i]));
        
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

    if (5 == timeSector)
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
    display.clearDisplay();

    if (alarmChanging)
    {
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(110, 4);
        display.print("</>");

        int limit = 4;

        switch(alarmSector)
        {
            // Часы
            case 0:
                limit = 24;
                break;
            
            // Минуты
            case 1:
                limit = 60;
                break;

            // Вкл/выкл
            case 2:
                limit = 2;
                break;    
        }

        counter = (limit + counter) % limit;
    }
    else
    {
        counter = (4 + counter) % 4;
        D("a ");

        alarmSector = counter;
    }

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.setTextColor(WHITE);
    
    // Вытаскиваем двух зайцев из одной шляпы
    int minute = state.alarmMin % 60;
    int hour = state.alarmMin / 60;

    // Инверсия цвета, если нулевой сектор
    if (0 == alarmSector) 
    {
        display.setTextColor(BLACK, WHITE);
        
        if (alarmChanging)
        {
            hour = counter;
            state.alarmMin = hour * 60 + minute;
        }
    }

    // Вывод на дисплей, с предварительным форматированием под 2 цифры
    display.print(to2d(hour));
    
    display.setTextColor(WHITE);
    display.print(":");

    // Аналогично первый
    if (1 == alarmSector) 
    {
        display.setTextColor(BLACK, WHITE);

        if (alarmChanging)
        {
            minute = counter;
            state.alarmMin = hour * 60 + minute;
        }
    }
    
    display.print(to2d(minute));

    // Второй
    if (2 == alarmSector) display.setTextColor(BLACK, WHITE);
    else    display.setTextColor(WHITE);

    display.setTextSize(1);
    display.setCursor(10, 16);
    if (state.enabled)
    {
        display.print(" enabled ");
    }
    else
    {
        display.print(" disabled ");
    }
    

    // Наконец, третий
    if (3 == alarmSector) display.setTextColor(BLACK, WHITE);
    else    display.setTextColor(WHITE);

    display.setCursor(80, 18);
    display.print(" exit ");
    
    display.display();
    
} // void displaySetAlarm()


void initDataTime()
{
    rtc.gettime();
    dataTime[0] = rtc.day;
    dataTime[1] = rtc.month; 
    dataTime[2] = rtc.year;
    dataTime[3] = rtc.Hours;
    dataTime[4] = rtc.minutes;
    
    
} // void initDataTime()

void setup() 
{
    D_INIT();

    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    
    rtc.begin();
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(led, LED_COUNT);
    EEPROM.get(0, state);

    ledTest();
    prevSW = digitalRead(ENCODER_SW);
    window = 0;
    startGlobal = millis();
    initDataTime();
    state.enabled = true;
    
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

    int currentTimeMin = dataTime[3] * 60 + dataTime[4];

    int delta = ((1440 + state.alarmMin - currentTimeMin) % 1440) / 60.0 * 255;
    
    int brightness = 255 - delta;
    brightness = max(0, brightness);

    if (brightness != prevBrightness)
    {
        FastLED.setBrightness(brightness);
        FastLED.show();
        prevBrightness = brightness;
    }

    if (millis() - startGlobal > 5000)
    {
        initDataTime();
        startGlobal = millis();
    }
    

    prevSW = encoderSW;
    delay(1);
} // void loop()


