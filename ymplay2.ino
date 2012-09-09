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

byte buffer[128];
volatile byte sdpos = 0;
volatile byte psgpos = 0;


ISR(TIMER1_COMPA_vect)
{
  for(int i=0;i<16;i++) {
    write_reg(i, buffer[(psgpos+i)&0x7f]);
  }
  psgpos+=16;
  psgpos &= 0x7f;
}

File ymFile;

void setup() {
  for(int i=2;i<=9;i++) {
    pinMode(i, OUTPUT);
  }
  pinMode(psgbc1, OUTPUT);
  pinMode(psgbdir, OUTPUT);
  pinMode(CS, OUTPUT);
  /*  pinMode(MOSI, INPUT);
  pinMode(MISO, OUTPUT);
  pinMode(SCK, OUTPUT);*/

  psg_inactive();
  write_reg(7, 255);

  digitalWrite(CS, HIGH);

  Serial.begin(9600);

  pinMode(13, OUTPUT);
  cli();
  TCCR1A = 0;        // set entire TCCR1A register to 0
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

  ymFile = SD.open("u_copier.ym");
  if(!ymFile) {
    Serial.println("Unable to open file");
    return;
  }
  for(int i=0;i<106;i++) {
    if(ymFile.available()) {
      ymFile.read();
    }
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
}


void loop() {
  while(((sdpos+128-psgpos)&0x7f) < 64) {
    read_frame();
  }

//  digitalWrite(13, HIGH);
//  PORTB=(1<<5);
//  write_reg(7, 0b00111110);
//  PORTB=0;
//  digitalWrite(13, LOW);
//  write_reg(8, 15);
//  write_reg(0, count);
//  write_reg(1, 0);
//  count += 1;
//  delay(10);
//  if(count == 0) {
//    Serial.println("Looping...");
//  }
  
}
