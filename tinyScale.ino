/* ATtiny85 as an I2C Master   Ex2        BroHogan                           1/21/11
 * Modified for Digistump - Digispark LCD Shield by Erik Kettenburg 11/2012
 * SETUP:
 * ATtiny Pin 1 = (RESET) N/U                      ATtiny Pin 2 = (D3) N/U
 * ATtiny Pin 3 = (D4) to LED1                     ATtiny Pin 4 = GND
 * ATtiny Pin 5 = SDA on DS1621  & GPIO            ATtiny Pin 6 = (D1) to LED2
 * ATtiny Pin 7 = SCK on DS1621  & GPIO            ATtiny Pin 8 = VCC (2.7-5.5V)
 * NOTE! - It's very important to use pullups on the SDA & SCL lines!
 * PCA8574A GPIO was used wired per instructions in "info" folder in the LiquidCrystal_I2C lib.
 * This ex assumes A0-A2 are set HIGH for an addeess of 0x3F
 * LiquidCrystal_I2C lib was modified for ATtiny - on Playground with TinyWireM lib.
 * TinyWireM USAGE & CREDITS: - see TinyWireM.h
 */

#include <HX711.h>
#include <TinyLiquidCrystal_I2C.h>
#include <EEPROM.h>

const int CELULA_CARGA_DOUT = 1;
const int CELULA_CARGA_SCK = 4;
HX711 escala;

#define GPIO_ADDR     0x27
TinyLiquidCrystal_I2C lcd(GPIO_ADDR, 16, 2);

const unsigned long CLIQUE_LONGO_MS = 1000;
const unsigned long CLIQUE_CURTO_MS = 60;
const int BOTAO = 3;

volatile unsigned long tempo_botao;
volatile unsigned long inicio;
volatile bool botaoPressionado = false;

ISR(PCINT0_vect) {
  unsigned long agora = millis();
  if(digitalRead(BOTAO) == LOW) {
    inicio = agora;
  } else {
    tempo_botao = agora - inicio;
    botaoPressionado = true;
  }
}

void carrega_config(HX711 &sensor) {
  long zero = 0;
  uint8_t *zeroBytes = (uint8_t*)&zero;
  for (int i = 0; i < sizeof(zero); ++i)
    zeroBytes[i] = EEPROM.read(i);

  float fator = 0;
  uint8_t *fatorBytes = (uint8_t*)&fator;
  for (int i = 0; i < sizeof(fator); ++i)
    fatorBytes[i] = EEPROM.read(4 + i);

  escala.set_offset(zero);
  escala.set_scale(fator);
}

void salva_config(HX711 &sensor) {
  long zero = escala.get_offset();
  uint8_t *zeroBytes = (uint8_t*)&zero;
  for (int i = 0; i < sizeof(zero); ++i)
    EEPROM.update(i, zeroBytes[i]);

  float fator = escala.get_scale();
  uint8_t *fatorBytes = (uint8_t*)&fator;
  for (int i = 0; i < sizeof(float); ++i)
    EEPROM.update(4 + i, fatorBytes[i]);
}

bool modo_calibrar(bool pressionado) {
  static int etapa = 0;
  if (pressionado) {
    switch(etapa) {
    case 0:
      {
        escala.set_scale();
        escala.tare();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Coloque 1kg");
        etapa = 1;
      }
      break;
    case 1:
      {
        etapa = 0;
        long unidades = escala.get_units(10);
        float fator = unidades / 1000.0f;
        escala.set_scale(fator);
        cli();
        salva_config(escala);
        sei();
        return false;
      }
      break;
    }
  }

  return true;
}

void modo_normal(bool pressionado) {
  static unsigned long delta = 0;
  if(pressionado) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tara...");
    escala.tare(10);
  }
  
  delta += millis();
  if(delta >= 1000) {
    float peso = escala.get_units(10);
    delta = 0;
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Peso: ");
    lcd.print(round(peso));
    lcd.print("g");
  }
}

bool modo_config = false;
void setup(){  
  lcd.init();
  lcd.backlight();
  lcd.print("Carregando...");
  
  escala.begin(CELULA_CARGA_DOUT, CELULA_CARGA_SCK);
  carrega_config(escala);

  inicio = millis();
  pinMode(BOTAO, INPUT_PULLUP);

  GIMSK |= _BV(PCIE);
  PCMSK |= _BV(PCINT3);
  sei();
}

void loop(){
  bool houveClique = false;
  if(botaoPressionado) {
    if(tempo_botao >= CLIQUE_LONGO_MS) {
      if(!modo_config) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Remova o peso");
      }
      modo_config = true;
    } else if(tempo_botao >= CLIQUE_CURTO_MS) {
      houveClique = true;
    }
    botaoPressionado = false;
  }

  if(modo_config)
    modo_config = modo_calibrar(houveClique);
  else
    modo_normal(houveClique);
}
