//trying ESP32 Wrover Module with defaults and Huge App, works!
//can leave bottom plugged in as long as you hold the volume knob towards the headphone jack while uploading

//settings:
// board=esp32 Dev
// regular defaults except:
// partition scheme Huge APP
// also you have to close and reopen arduino every time you want to recompile,yes or changing the board works too
//Jim Updates:
//default volume = calm 7 instead of blaring 21
//sort files
//when song over, play next one
//
//Can't write to sd card? xremember last song played (saves index in a text file whenever music is paused)
//TODO: fast forward?
//TODO: remember last song played?
const bool isDebug = false;//true;//
#include "Arduino.h"

//Audio
#include "Audio.h"
#include "SPI.h"

//SD Card
#include "SD.h"
#include "FS.h"

//LCD
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//SD Card
#define SD_CS 22
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18

//Digital I/O used  //Makerfabs Audio V2.0
#define I2S_DOUT 27
#define I2S_BCLK 26
#define I2S_LRC 25

//SSD1306 aka LCD
#define MAKEPYTHON_ESP32_SDA 4
#define MAKEPYTHON_ESP32_SCL 5
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Button
const int Pin_vol_up = 39;
const int Pin_vol_down = 36;
const int Pin_mute = 35;

const int Pin_previous = 15;
const int Pin_pause = 33;
const int Pin_next = 2;


const int DEFAULTVOL=7;//21
const int MAXSONGS=20;

void logoshow();
void open_new_song(String filename);
void sort_music_list(int lastFile,String fileList[MAXSONGS]);
int get_music_list(fs::FS &fs, const char *dirname, uint8_t levels, String wavlist[MAXSONGS], int & file_index);
void print_song_time();
void print_song_info();
void lcd_text(String text);
void display_music();
void open_new_song(String filename);
//const String lastSongFileName = "lastSongFile.txt";//can't save?

Audio audio;

struct Music_info
{
    String name;
    uint32_t length;
    uint32_t runtime;
    int volume;
    int status;
    int mute_volume;
} music_info = {"", 0, 0, 0, 0, 0};

String file_list[MAXSONGS];
int file_num = 0;
int file_index = 0; //first song is 0th song
bool isPaused=false;
uint run_time = 0;
uint button_time = 0;

void setup() {
    //IO mode init
    pinMode(Pin_vol_up, INPUT_PULLUP);
    pinMode(Pin_vol_down, INPUT_PULLUP);
    pinMode(Pin_mute, INPUT_PULLUP);
    pinMode(Pin_previous, INPUT_PULLUP);
    pinMode(Pin_pause, INPUT_PULLUP);
    pinMode(Pin_next, INPUT_PULLUP);

    //Serial
    if (isDebug)
      Serial.begin(115200);

    //LCD
    Wire.begin(MAKEPYTHON_ESP32_SDA, MAKEPYTHON_ESP32_SCL);
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
      if (isDebug)
        Serial.println(F("SSD1306 allocation failed"));
      for (;;)
          ; // Don't proceed, loop forever
    }
    display.clearDisplay();
    logoshow();

    //SD(SPI)
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    SPI.setFrequency(1000000);
    if (!SD.begin(SD_CS, SPI)) {
        if (isDebug) Serial.println("Card Mount Failed");
        lcd_text("SD ERR");
        while (1)
            ;
    }
    else {
        lcd_text("SD OK");
    }

    //Read SD
    file_num = get_music_list(SD, "/", 0, file_list, file_index);
    for (int i = 0; i < file_num; i++) //debugging
    {
        Serial.println(file_list[i]);
    }
    
    sort_music_list(file_num,file_list);
    if (isDebug){
      Serial.print("Music file count:");
      Serial.println(file_num);
      Serial.println("Sorted music:");
      for (int i = 0; i < file_num; i++) {
        Serial.println(file_list[i]);
      }
    }

    //Audio(I2S)
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(DEFAULTVOL); // 0...21

    //audio.connecttoFS(SD, "/MoonlightBay.mp3"); //ChildhoodMemory.mp3  //MoonRiver.mp3 //320k_test.mp3
    //file_list[0] = "MoonlightBay.mp3";
    open_new_song(file_list[file_index]);
    print_song_time();
    print_song_info();
    run_time = millis();
    
}

void loop()
{
  
    audio.loop();
    if (millis() - run_time > 1000) {
        run_time = millis();
        print_song_time();
        display_music();
        if (music_info.length<=music_info.runtime) { //if song over play next
          audio.stopSong();
          if (file_index < file_num - 1)
              file_index++;
          else
              file_index = 0;
          open_new_song(file_list[file_index]);
          print_song_info();
        }
    }

    if (millis() - button_time > 300) {
        //Button logic
        if (digitalRead(Pin_next) == 0) {
            if (isDebug) Serial.println("Pin_next");
            if (isPaused){
                uint32_t filePos=audio.getFilePos();
                if (isDebug){
                  Serial.print(filePos);
                  Serial.println(" position of the file at the current time");
                }
            }
            if (file_index < file_num - 1)
                file_index++;
            else
                file_index = 0;
            open_new_song(file_list[file_index]);
            print_song_time();
            button_time = millis();
        }
        if (digitalRead(Pin_previous) == 0) {
            //if (isDebug) Serial.println("Pin_previous");
            if (file_index > 0)
                file_index--;
            else
                file_index = file_num - 1;
            open_new_song(file_list[file_index]);
            print_song_time();
            button_time = millis();
        }
        if (digitalRead(Pin_vol_up) == 0)
        {
            //if (isDebug) Serial.println("Pin_vol_up");
            if (music_info.volume < 21)
                music_info.volume++;
            audio.setVolume(music_info.volume);
            button_time = millis();
        }
        if (digitalRead(Pin_vol_down) == 0)
        {
            //if (isDebug) Serial.println("Pin_vol_down");
            if (music_info.volume > 0)
                music_info.volume--;
            audio.setVolume(music_info.volume);
            button_time = millis();
        }
        if (digitalRead(Pin_mute) == 0)
        {
            //if (isDebug) Serial.println("Pin_mute");
            if (music_info.volume != 0)
            {
                music_info.mute_volume = music_info.volume;
                music_info.volume = 0;
            }
            else
            {
                music_info.volume = music_info.mute_volume;
            }
            audio.setVolume(music_info.volume);
            button_time = millis();
        }
        if (digitalRead(Pin_pause) == 0)
        {
            //if (isDebug) Serial.println("Pin_pause");
            audio.pauseResume();
            button_time = millis();
            isPaused=!isPaused;
        }
    }
   

    
    
}

void open_new_song(String filename)
{
    music_info.name = filename.substring(0, filename.indexOf("."));
    audio.connecttoFS(SD, filename.c_str());
    music_info.runtime = audio.getAudioCurrentTime();
    music_info.length = audio.getAudioFileDuration();
    //music_info.length=  audio.getFileSize()  / (audio.getSampleRate() * audio.getChannels());
    music_info.volume = audio.getVolume();
    music_info.status = 1;
    //Serial.println("**********start a new sound************");
}

void display_music()
{
    int line_step = 10;//24
    int line = 0;
    char buff[20];
    ;
    sprintf(buff, "%d:%d", music_info.runtime, music_info.length);

    display.clearDisplay();

    display.setTextSize(1);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text

    display.setCursor(0, line); // Start at top-left corner
    display.println(music_info.name);
    line += line_step;
    line += line_step;//in case song title takes up two lines
    display.setCursor(0, line);
    display.println(buff);
    line += line_step;

    sprintf(buff, "V:%d",music_info.volume);

    display.setCursor(0, line);
    display.println(buff);
    line += line_step;

    display.setCursor(0, line);
    display.println(music_info.status);
    line += line_step;

    display.display();
}

void logoshow()
{
    display.clearDisplay();

    display.setTextSize(1);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);             // Start at top-left corner
    display.println(F("MakePython"));
    display.setCursor(0, 10); // Start at top-left corner
    display.println(F("Music"));
    display.setCursor(0, 20); // Start at top-left corner
    display.println(F("Player V2.2"));
    display.setCursor(0,30);
    display.println(F("Jim Town Edition"));
    display.display();
    delay(2000);
}

void lcd_text(String text)
{
    display.clearDisplay();

    display.setTextSize(2);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);             // Start at top-left corner
    display.println(text);
    display.display();
    delay(500);
}

void print_song_info()
{
  
  if (isDebug){
    Serial.println("***********************************");
    Serial.print("File Size:");
    Serial.println(audio.getFileSize());
    Serial.print("File Pos:");
    Serial.println(audio.getFilePos());
    Serial.print("Sample Rate:");
    Serial.println(audio.getSampleRate());
    Serial.print("Bits Per Sample:");
    Serial.println(audio.getBitsPerSample());
    Serial.print("Channels:");
    Serial.println(audio.getChannels());
    Serial.print("Volume:");
    Serial.println(audio.getVolume());
    Serial.println("***********************************");
  }
  
}

void print_song_time()
{
  if (isDebug){
    Serial.print("Current Time:");
    Serial.println(audio.getAudioCurrentTime());
    Serial.print("Total Time:");
    Serial.println(audio.getAudioFileDuration());
  }
    music_info.runtime = audio.getAudioCurrentTime();
    music_info.length = audio.getAudioFileDuration();
    music_info.volume = audio.getVolume();
}

int get_music_list(fs::FS &fs, const char *dirname, uint8_t levels, String wavlist[MAXSONGS], int & file_index)
{
    if (isDebug) Serial.printf("Listing directory: %s\n", dirname);
    int i = 0;

    File root = fs.open(dirname);
    
    if (!root){
        if (isDebug) Serial.println("Failed to open directory");
        return i;
    }
    if (!root.isDirectory()){
        if (isDebug) Serial.println("Not a directory");
        return i;
    }

    File file = root.openNextFile();
    while (file && i<MAXSONGS){
        if (!file.isDirectory()){
            String temp = file.name();
            if (temp.endsWith(".wav")){
                wavlist[i] = temp;
                i++;
            }
            else if (temp.endsWith(".mp3")){
                wavlist[i] = temp;
                if (isDebug) Serial.println(temp);//testing
                i++;
            } 
        }
        file = root.openNextFile();
    }
    if (isDebug && i>=MAXSONGS){
      Serial.println("i>=MAXSONGS!!");
    }
    return i;
}

void sort_music_list(int lastFile,String fileList[MAXSONGS]){
  //bubble sort
  bool swapped;
  do {
    swapped=false;
    for(int i=1; i<lastFile; i++){
      if (fileList[i-1].compareTo(fileList[i])>0){
        String temp=fileList[i];
        fileList[i]=fileList[i-1];
        fileList[i-1]=temp;
        swapped=true;
      }
    }    
  } while (swapped);

}
