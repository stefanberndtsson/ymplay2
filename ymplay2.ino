int psgbc1 = A0;
int psgbdir = A1;

#define CS A0
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
  PORTB=(PINB&~3)|((value>>6)&3);
/*
  digitalWrite(2, (value>>0)&1);
  digitalWrite(3, (value>>1)&1);
  digitalWrite(4, (value>>2)&1);
  digitalWrite(5, (value>>3)&1);
  digitalWrite(6, (value>>4)&1);
  digitalWrite(7, (value>>5)&1);
  digitalWrite(8, (value>>6)&1);
  digitalWrite(9, (value>>7)&1);
  */
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

void setup() {
  for(int i=2;i<=9;i++) {
    pinMode(i, OUTPUT);
  }
  pinMode(psgbc1, OUTPUT);
  pinMode(psgbdir, OUTPUT);
  psg_inactive();
  Serial.begin(9600);
}

byte count = 0;

void loop() {
//  digitalWrite(13, HIGH);
//  PORTB=(1<<5);
  write_reg(7, 0b00111110);
//  PORTB=0;
//  digitalWrite(13, LOW);
  write_reg(8, 15);
  write_reg(0, count);
  write_reg(1, 0);
  count += 1;
  delay(10);
  if(count == 0) {
    Serial.println("Looping...");
  }
}
