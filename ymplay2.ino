#include <SD/SD.h>
#include <LiquidCrystal/LiquidCrystal.h>
#include <EEPROM/EEPROM.h>

int psgbc1 = A0;
int psgbdir = A1;

#define SHIFT_DATA 2
#define SHIFT_CLOCK 3
#define SHIFT_LATCH 4

#define LCD_D4 5
#define LCD_D5 6
#define LCD_D6 7
#define LCD_D7 8
#define LCD_RS A2
#define LCD_E 9

#define CS 10
#define MOSI 11
#define MISO 12
#define SCK 13
#define BUTTON_NEXT A3
#define BUTTON_PREV A4

#define DIR_NEXT 0
#define DIR_PREV 1

byte last_written_regs[16];

#define CLR(x,y) (x&=(~(1<<y)))
#define SET(x,y) (x|=(1<<y))

static inline void myshiftOut(byte value) {
  for(int i=0;i<8;i++) {
    if(value&(1<<(7-i))) {
      SET(PORTD,SHIFT_DATA);
    } else {
      CLR(PORTD,SHIFT_DATA);
    }
    SET(PORTD,SHIFT_CLOCK);
    CLR(PORTD,SHIFT_CLOCK);
  }
}

static inline void psg_inactive() {
  digitalWrite(psgbc1, LOW);
  digitalWrite(psgbdir, LOW);
  delayMicroseconds(3);
}

static inline void psg_write_byte(byte value) {
  digitalWrite(SHIFT_LATCH, LOW);
  //  shiftOut(SHIFT_DATA, SHIFT_CLOCK, MSBFIRST, value);
  myshiftOut(value);
  digitalWrite(SHIFT_LATCH, HIGH);
  //  PORTD=((value<<2)&~3)|(PIND&3);
  /*  PORTB=(PINB&~3)|((value>>6)&3);*/
/*
  digitalWrite(2, (value>>0)&1);
  digitalWrite(3, (value>>1)&1);
  digitalWrite(4, (value>>2)&1);
  digitalWrite(5, (value>>3)&1);
  digitalWrite(6, (value>>4)&1);
  digitalWrite(7, (value>>5)&1);
*/
//  digitalWrite(8, (value>>6)&1);
//  digitalWrite(9, (value>>7)&1);
  
}

static inline void psg_write_reg(byte reg) {
  psg_write_byte(reg);
  digitalWrite(psgbc1, HIGH);
  digitalWrite(psgbdir, HIGH);
  delayMicroseconds(3);
}

static inline void psg_write_data(byte value) {
  psg_write_byte(value);
  digitalWrite(psgbc1, LOW);
  digitalWrite(psgbdir, HIGH);
  delayMicroseconds(3);
}

void write_reg(byte reg, byte value) {
  if(last_written_regs[reg] == value && reg != 13) {
    return;
  }
  psg_inactive();
  psg_write_reg(reg);
  psg_inactive();
  psg_write_data(value);
  psg_inactive();
  last_written_regs[reg] = value;
}

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

File ymFile;
File dir;

char title[21];
char author[21];
char convertor[21];
byte buffer[128];
char current_file[12];
volatile byte sdpos = 0;
volatile byte psgpos = 0;
int frames = 0;
int frames_loaded = 0;
int loop_frame = 0;
int data_start = 0;

int button_next_state = LOW;
int button_next_last_press = 0;
int button_prev_state = LOW;
int button_prev_last_press = 0;

void clear_all() {
  sdpos = 0;
  psgpos = 0;
  for(int i=0;i<sizeof(buffer);i++) {
    buffer[i] = 0;
  }
  for(int i=0;i<16;i++) {
    last_written_regs[i] = 0xff;
    write_reg(i, 0);
  }
}


static inline uint8_t read8() {
  if(ymFile.available()) {
    return ymFile.read();
  }
  return 0;
}

static inline uint16_t read16() {
  return ((uint16_t)read8()<<8)|(uint16_t)read8();
}

static inline uint32_t read32() {
  return ((uint32_t)read16()<<16)|(uint32_t)read16();
}

static inline void readstr(char *ptr, int maxsize) {
  int pos = 0;
  byte tmp = 0xff;
  while(tmp != 0x00) {
    tmp = ymFile.read();
    if(pos < maxsize) {
      ptr[pos] = tmp;
      pos++;
    }
  }
  if(pos == maxsize) {
    ptr[pos] = '\0';
  }
}

int read_header() {
  Serial.print("Checking file: ");
  Serial.println(ymFile.name());
  // Skip first bits
  if(read32() != 0x594d3521) { /* YM5! */
    Serial.println("Not YM5!");
    return 0;
  }
  if(read32() != 0x4c654f6e) { /* LeOn */
    Serial.println("Not LeOn");
    return 0;
  }
  if(read32() != 0x41724421) { /* ArD! */
    Serial.println("Not ArD!");
    return 0;
  }

  ymFile.seek(12);
  frames = read32();
  ymFile.seek(28);
  loop_frame = read32();
  ymFile.seek(34);
  readstr(title, sizeof(title)-1);
  readstr(author, sizeof(author)-1);
  readstr(convertor, sizeof(convertor)-1);
  data_start = ymFile.position();
  Serial.print("Title: ");
  Serial.println(title);
  Serial.print("Author: ");
  Serial.println(author);
  Serial.print("Convertor: ");
  Serial.println(convertor);
  Serial.print("Length: ");
  Serial.print(frames/50);
  Serial.println(" seconds");
  Serial.print("Loop at: ");
  Serial.println(loop_frame);
  return 1;
}

int eeprom_read() {
  if(EEPROM.read(0) != 'Y') return 0;
  if(EEPROM.read(1) != 'M') return 0;
  if(EEPROM.read(2) != '5') return 0;
  if(EEPROM.read(3) != '!') return 0;
  for(int i=0;i<sizeof(current_file);i++) {
    current_file[i] = EEPROM.read(i+4);
  }
  return 1;
}

void eeprom_write() {
  EEPROM.write(0, 'Y');
  EEPROM.write(1, 'M');
  EEPROM.write(2, '5');
  EEPROM.write(3, '!');
  for(int i=0;i<sizeof(current_file);i++) {
    EEPROM.write(i+4, current_file[i]);
  }
}

void print_frames_in_time(int frames) {
  uint16_t sec;
  byte min;
  sec = frames/50;
  min = sec/60;
  sec = sec-min*60;
  if(min < 10) lcd.print("0");
  lcd.print(min);
  lcd.print(":");
  if(sec < 10) lcd.print("0");
  lcd.print(sec);
}

void print_header() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(title);
  lcd.setCursor(0,1);
  lcd.print(author);
  lcd.setCursor(0,2);
  lcd.print(convertor);
}

int read_until_ym_or_eod(int direction) {
  int fetch_next = 0;
  char last_scanned[12];

  frames_loaded = 0;
  last_scanned[0] = '\0';
  dir.rewindDirectory();
  Serial.println("Searching for next file...");

  /* Since the first file is always going to be the root directory
   * (or nothing, but then we've failed already), we'll start looping
   * by opening the next file.
   */

  while(true) {
    ymFile = dir.openNextFile();
    if(!ymFile) {
      Serial.println("End of files, exiting...");
      /* No more files */
      return 0;
    }

    if(ymFile.isDirectory()) {
      ymFile.close();
      continue;
    }

    Serial.print("Testing: ");
    Serial.println(ymFile.name());

    if(direction == DIR_NEXT) {
      if(current_file[0] == '\0') {
	fetch_next = 1;
      }

      if(fetch_next && read_header()) {
	Serial.print("Found file: ");
	Serial.println(ymFile.name());
	strcpy(current_file, ymFile.name());
	eeprom_write();
	print_header();
	return 1;
      } else {
	if(!strcmp(current_file, ymFile.name())) {
	  fetch_next = 1;
	}
      }
    } else if(direction == DIR_PREV) {
      if(current_file[0] == '\0') {
	fetch_next = 1;
      }

      if(!ymFile.isDirectory() && read_header()) {
	/* Valid YM */
	if(fetch_next) {
	  Serial.print("Found file: ");
	  Serial.println(ymFile.name());
	  strcpy(current_file, ymFile.name());
	  eeprom_write();
	  print_header();
	  return 1;
	} else {
	  if(!strcmp(current_file, ymFile.name())) {
	    if(last_scanned[0] != '\0') {
	      ymFile.close();
	      ymFile = SD.open(last_scanned);
	      read_header();
	      strcpy(current_file, ymFile.name());
	      eeprom_write();
	      print_header();
	      return 1;
	    } else {
	      current_file[0] = '\0';
	      ymFile.close();
	      return 0;
	    }
	  }
	}
	strcpy(last_scanned, ymFile.name());
      }
    }
    
    ymFile.close();
    /*

    if(last_scanned[0] != '\0' && read_header()) {
      strcpy(last_scanned, ymFile.name());
    }
    if(current_file[0] == '\0') {
      Serial.println("We want the first file...");
      reached_current = 1;
    }
    if(!reached_current) {
      if(!strcmp(ymFile.name(), current_file)) {
	// We reached current file, skipping that and going for next
	Serial.println("Reached current file, skipping that...");
	reached_current = 1;
      }
      ymFile.close();
      continue;
    }
    if(!ymFile.isDirectory() && read_header()) {
      // Found one, let's play it
      Serial.print("Found file: ");
      Serial.println(ymFile.name());
      strcpy(current_file, ymFile.name());
      return 1;
    }
    ymFile.close();
*/
  }
}

char reg_order[16] = {0, 1, 8, 2, 3, 9, 4, 5, 10, 7, 6, 11, 12, 13, 14, 15};

ISR(TIMER1_COMPA_vect)
{
  byte value;
  digitalWrite(A5, HIGH);
  for(int i=0;i<14;i++) {
    value = buffer[(psgpos+reg_order[i])&0x7f];
    if(reg_order[i] == 13 && value == 0xff) continue;
    write_reg(reg_order[i], value);
  }
  psgpos+=16;
  psgpos &= 0x7f;
  digitalWrite(A5, LOW);
}

void printDirectory(File, int);

void setup() {
  for(int i=2;i<=9;i++) {
    pinMode(i, OUTPUT);
  }
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLOCK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  pinMode(psgbc1, OUTPUT);
  pinMode(psgbdir, OUTPUT);
  pinMode(CS, OUTPUT);
  pinMode(BUTTON_NEXT, INPUT);
  pinMode(BUTTON_PREV, INPUT);
  pinMode(A5,OUTPUT);
  digitalWrite(A5,LOW);

  psg_inactive();
  write_reg(7, 255);

  digitalWrite(CS, HIGH);

  Serial.begin(9600);

  pinMode(13, OUTPUT);

  /* Set timer to 50Hz */
  cli();
  TCCR1A = 0;
  TCCR1B = 0;
  OCR1A = 312;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS10);
  TCCR1B |= (1 << CS12);
  TIMSK1 |= (1 << OCIE1A);
  sei();

  
  if (!SD.begin(CS)) {
    Serial.println("initialization failed!");
    return;
  }

  dir = SD.open("/");
  current_file[0] = '\0';
  lcd.begin(20,4);
  
  if(!digitalRead(BUTTON_PREV) && eeprom_read()) {
    Serial.print("Read from EEPROM: ");
    Serial.println(current_file);
    ymFile = SD.open(current_file);
    if(!ymFile) {
      Serial.println("Could not open file.");
      current_file[0] = '\0';
    }
    read_header();
    print_header();
  } 
  lcd.setCursor(0,3);
  lcd.print("                    ");
  if(current_file[0] == '\0') {
    read_until_ym_or_eod(DIR_NEXT);
  }
}

void read_frame() {
  for(int i=0;i<16;i++) {
    if(ymFile.available()) {
      buffer[sdpos&0x7f] = ymFile.read();
      sdpos++;
      sdpos &= 0x7f;
    }
  }
  frames_loaded++;
  if(frames_loaded >= frames) {
    Serial.print("Reached max frames: ");
    Serial.print(frames_loaded);
    Serial.print(" / ");
    Serial.println(frames);
    /* Look for next file */
    ymFile.close();
    clear_all();
    if(!read_until_ym_or_eod(DIR_NEXT)) {
      current_file[0] = '\0';
      if(read_until_ym_or_eod(DIR_NEXT)) {
	Serial.print("New file open (2): ");
	Serial.println(ymFile.name());
	Serial.println(current_file);
      };
    } else {
      Serial.print("New file open: ");
      Serial.println(ymFile.name());
      Serial.println(current_file);
    }
    //    frames_loaded = loop_frame;
    //    ymFile.seek(data_start+16*loop_frame);
  }

}


void loop() {
  while(((sdpos+128-psgpos)&0x7f) < 64) {
    read_frame();
    if(frames_loaded%25 == 0) {
      lcd.setCursor(0,3);
      print_frames_in_time(frames_loaded);
      lcd.print(" / ");
      print_frames_in_time(frames);
    }
  }

  if((!button_next_state && digitalRead(BUTTON_NEXT)) &&
     (button_next_last_press - millis()) > 100) {
    Serial.println("Button NEXT pressed...");
    button_next_state = HIGH;
    clear_all();
    ymFile.close();
    if(!read_until_ym_or_eod(DIR_NEXT)) {
      current_file[0] = '\0';
      read_until_ym_or_eod(DIR_NEXT);
    }
  } else if(button_next_state && !digitalRead(BUTTON_NEXT)) {
    Serial.println("Button NEXT released...");
    button_next_state = LOW;
    button_next_last_press = millis();
  }
  if((!button_prev_state && digitalRead(BUTTON_PREV)) &&
     (button_prev_last_press - millis()) > 100) {
    Serial.println("Button PREV pressed...");
    button_prev_state = HIGH;
    clear_all();
    ymFile.close();
    if(!read_until_ym_or_eod(DIR_PREV)) {
      current_file[0] = '\0';
      read_until_ym_or_eod(DIR_PREV);
    }
  } else if(button_prev_state && !digitalRead(BUTTON_PREV)) {
    Serial.println("Button PREV released...");
    button_prev_state = LOW;
    button_prev_last_press = millis();
  }
}
