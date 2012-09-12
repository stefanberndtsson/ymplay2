#include <SD/SD.h>

int psgbc1 = A0;
int psgbdir = A1;

#define CS 10
#define MOSI 11
#define MISO 12
#define SCK 13

static inline void psg_inactive() {
  PORTC=0;
  delayMicroseconds(3);
/*
  digitalWrite(psgbc1, LOW);
  digitalWrite(psgbdir, LOW);
  */
}

static inline void psg_write_byte(byte value) {
  PORTD=((value<<2)&~3)|(PIND&3);
  /*  PORTB=(PINB&~3)|((value>>6)&3);*/
/*
  digitalWrite(2, (value>>0)&1);
  digitalWrite(3, (value>>1)&1);
  digitalWrite(4, (value>>2)&1);
  digitalWrite(5, (value>>3)&1);
  digitalWrite(6, (value>>4)&1);
  digitalWrite(7, (value>>5)&1);
*/
  digitalWrite(8, (value>>6)&1);
  digitalWrite(9, (value>>7)&1);
  
}

static inline void psg_write_reg(byte reg) {
  psg_write_byte(reg);
  PORTC=3;
  delayMicroseconds(3);
/*
  digitalWrite(psgbc1, HIGH);
  digitalWrite(psgbdir, HIGH);
  */
}

static inline void psg_write_data(byte value) {
  psg_write_byte(value);
  PORTC=2;
  delayMicroseconds(3);
/*
  digitalWrite(psgbc1, LOW);
  digitalWrite(psgbdir, HIGH);
  */
}

void write_reg(byte reg, byte value) {
  psg_inactive();
  psg_write_reg(reg);
  psg_inactive();
  psg_write_data(value);
  psg_inactive();
}

void clear_all() {
  for(int i=0;i<16;i++) {
    write_reg(i, 0);
  }
}

File ymFile;

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

int read_until_ym_or_eod() {
  File dir;
  int reached_current = 0;

  dir = SD.open("/");

  /* We should have an open ymFile here, but let's check. */
  if(!dir) {
    return 0;
  }

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
    if(current_file[0] == '\0') {
      reached_current = 1;
    }
    if(!reached_current) {
      if(!strcmp(ymFile.name(), current_file)) {
	/* We reached current file, skipping that and going for next */
	reached_current = 1;
      }
      continue;
    }
    if(!ymFile.isDirectory() && read_header()) {
      /* Found one, let's play it */
      Serial.print("Found file: ");
      Serial.println(ymFile.name());
      strcpy(current_file, ymFile.name());
      return 1;
    }
  }
}


ISR(TIMER1_COMPA_vect)
{
  for(int i=0;i<16;i++) {
    write_reg(i, buffer[(psgpos+i)&0x7f]);
  }
  psgpos+=16;
  psgpos &= 0x7f;
}

void printDirectory(File, int);

void setup() {
  for(int i=2;i<=9;i++) {
    pinMode(i, OUTPUT);
  }
  pinMode(psgbc1, OUTPUT);
  pinMode(psgbdir, OUTPUT);
  pinMode(CS, OUTPUT);

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

  current_file[0] = '\0';

  read_until_ym_or_eod();
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
    /* Look for next file */
    clear_all();
    if(!read_until_ym_or_eod()) {
      current_file[0] = '\0';
      read_until_ym_or_eod();
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
  }
}
