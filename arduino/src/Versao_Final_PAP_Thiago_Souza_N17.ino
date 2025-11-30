/*
  PINOUT:

   RC522 MODULE    UNO     MEGA
   SDA(SS)         D10     D53
   SCK             D13     D52
   MOSI            D11     D51
   MISO            D12     D50
   PQ              Não conectado
   GND             GND     GND
   RST             D9      D9
   3.3V            3.3V    3.3V
*/

#include <Wire.h> 
#include <RTClib.h> 
#include <Servo.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

#define RFID_SS  10
#define RFID_RST 9

RTC_DS1307 rtc; 
Servo dispensador;
Servo tampa;

MFRC522 rfid( RFID_SS, RFID_RST );

char daysOfTheWeek[7][12] = {"Domingo", "Segunda", "Terça", "Quarta", "Quinta", "Sexta", "Sábado"};

int hora = 0;
int minuto = 0;
int segundo = 0;

int thisChar;
int numero = 0;
int quantTampa = 0;

//função de detecção contínua do cartão=============================================================================

bool coleiraDetetada = false;  // Variável para indicar se o coleira foi detectada
unsigned long coleiraDetetadaTempo = 0;  // Variável para guardar o tempo em que a coleira foi detectada
int tampaPosicao = 0; // Variável para guardar a posição da tampa (0 = fechada, 90 = aberta)

//==================================================================================================================

int numHorarios = 0;
int horaprog[10];
int minutoprog[10];
int segundoprog[10] = {0}; 

unsigned long temporizador = 0;
const unsigned long intervalo = 2000; // intervalo de 2 segundos

SoftwareSerial bluetooth(2, 3); // RX, TX

//Salvar e guardar horários na EEPROM========================================================================================================
void saveHorarios() {
  int address = 0; // endereço na EEPROM onde os horários serão salvos
  EEPROM.write(address, numHorarios); // salva a quantidade de horários na primeira posição da EEPROM
  address++;
  for (int i = 0; i < numHorarios; i++) { // salva cada horário na EEPROM
    EEPROM.write(address, horaprog[i]);
    address++;
    EEPROM.write(address, minutoprog[i]);
    address++;
    EEPROM.write(address, segundoprog[i]);
    address++;
  }
}

void loadHorarios() {
  int address = 0; // endereço na EEPROM onde os horários estão salvos
  numHorarios = EEPROM.read(address); // lê a quantidade de horários na primeira posição da EEPROM
  address++;
  for (int i = 0; i < numHorarios; i++) { // lê cada horário da EEPROM
    horaprog[i] = EEPROM.read(address);
    address++;
    minutoprog[i] = EEPROM.read(address);
    address++;
    segundoprog[i] = EEPROM.read(address);
    address++;
    Serial.print(horaprog[i]);
    Serial.print(":");
    Serial.print(minutoprog[i]);
    Serial.print(" | ");
  }
}
//=========================================================================================================================================

void setup() {
  Serial.begin(9600);
  bluetooth.begin(9600);

  rtc.begin();
  SPI.begin();
  rfid.begin();

  tampa.attach(5);
  dispensador.attach(6);

  pinMode(5, OUTPUT);
  //pinMode(6, OUTPUT);
  //pinMode(8, OUTPUT);

  dispensador.write(0);
  tampa.write(0);

  if (!rtc.begin()) {
    Serial.println("DS1307 não encontrado"); 
    while (1);
  }
  if (!rtc.isrunning()) { 
    Serial.println("DS1307 rodando!"); 
  }
  
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); 

  loadHorarios(); // carrega os horários da EEPROM
}

void loop() {
  unsigned long tempoAtual = millis();

  DateTime now = rtc.now(); 
  hora = now.hour();
  minuto = now.minute();
  segundo = now.second();

  if (bluetooth.available() > 0) {
    String horario = bluetooth.readStringUntil('\n'); 
    Serial.print("Horário recebido: ");
    Serial.println(horario);

    if (horario == "apagar tudo"){
      numHorarios = 0; 
      Serial.println("Todos os horários foram apagados.");
      saveHorarios(); // salva os horários atualizados na EEPROM
      loadHorarios();
      delay(500);

    } else if (horario.startsWith("Delete: ")) {
      String horario_deletar = horario.substring(8);
      int doisPontos = horario_deletar.indexOf(':');                                                                            
      if (doisPontos != -1 && horario_deletar.length() == 5) { 
        int hora_deletar = horario_deletar.substring(0, doisPontos).toInt(); 
        int minuto_deletar = horario_deletar.substring(doisPontos + 1).toInt(); 
        bool apagou_horario = false;

        for (int i = 0; i < numHorarios; i++) {
          if (horaprog[i] == hora_deletar && minutoprog[i] == minuto_deletar) {
            for (int j = i; j < numHorarios - 1; j++) {
              horaprog[j] = horaprog[j+1];
              minutoprog[j] = minutoprog[j+1];
              segundoprog[j] = segundoprog[j+1];
            }
            numHorarios--;
            saveHorarios(); // salva os horários atualizados na EEPROM
            apagou_horario = true;
            Serial.println("Horário apagado: " + horario_deletar);
            break;
          }
        }
        if (!apagou_horario) {
          Serial.println("Horário não encontrado: " + horario_deletar);
        }
      } else {
        Serial.println("Horário inválido para deletar: " + horario_deletar);
      }
      delay(500);

    } else {
      int index = horario.indexOf(":"); // obtém o índice do caractere ":"
      int horaProg = horario.substring(0, index).toInt(); // obtém a hora programada
      int minutoProg = horario.substring(index+1).toInt(); // obtém o minuto programado

      if (horaProg < 0 || horaProg > 23 || minutoProg < 0 || minutoProg > 59) { // verifica se os valores são válidos
        Serial.println("Horário inválido");

      } else if (numHorarios >= 10) { // verifica se já existem 10 horários programados
        Serial.println("Número máximo de horários atingido");

      } else {
        horaprog[numHorarios] = horaProg; // adiciona o horário ao vetor de horários programados
        minutoprog[numHorarios] = minutoProg;
        numHorarios++; // aumenta a quantidade de horários programados
        Serial.print("Horário programado: ");
        Serial.print(horaProg);
        Serial.print(":");
        Serial.println(minutoProg);
        saveHorarios(); // salva os horários atualizados na EEPROM
        delay(500);
    }
}
}

for (int i = 0; i < numHorarios; i++) { // verifica se é hora de acionar o dispenser
if (horaprog[i] == hora && minutoprog[i] == minuto && segundoprog[i] == segundo) {
  dispensador.write(75);
  delay(2000);
  dispensador.write(0);
  Serial.print("Dispenser acionado às ");
  Serial.print(hora);
  Serial.print(":");
  Serial.print(minuto);
  Serial.print(":");
  Serial.println(segundo);
  Serial.println();

  for (int i = 0; i < 10; i++) {
    Serial.print(horaprog[i]);
    Serial.print(" ");
  }
  Serial.print(":");

  for (int i = 0; i < 10; i++) {
    Serial.print(minutoprog[i]);
    Serial.print(" ");
  }
  Serial.println();
}
}

  loadHorarios();
  Serial.println();
  Serial.print("Data: "); 
  Serial.print(now.day(), DEC); 
  Serial.print('/'); 
  Serial.print(now.month(), DEC); 
  Serial.print('/');
  Serial.print(now.year(), DEC); 
  Serial.print(" / Dia: ");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(" / Horas: "); //IMPRIME O TEXTO NA SERIAL
  Serial.print(hora, DEC); //IMPRIME NO MONITOR SERIAL A HORA
  Serial.print(':'); //IMPRIME O CARACTERE NO MONITOR SERIAL
  Serial.print(minuto, DEC); //IMPRIME NO MONITOR SERIAL OS MINUTOS
  Serial.print(':'); //IMPRIME O CARACTERE NO MONITOR SERIAL
  Serial.print(now.second(), DEC); //IMPRIME NO MONITOR SERIAL OS SEGUNDOS
  Serial.println(); //QUEBRA DE LINHA NA SERIAL
  Serial.println();
  delay(1000);

  //TAMPA==========================================================================================================
    byte data[MAX_LEN];
    byte uid[5];

    if ( rfid.requestTag( MF1_REQIDL, data ) == MI_OK ) {
      if ( rfid.antiCollision( data ) == MI_OK ) {
        memcpy( uid, data, 5 );
        for ( int i = 0; i < 5; i++ ) {
          thisChar = uid[i];
        }
      
        numero = thisChar;  
        Serial.print (numero);
        Serial.println(); 
        
        if (numero != 103){
          Serial.println("Coleira diferente detetada!");
          delay(1000);
        }

        if (numero == 103){ // Numero ja gravado no Cartao RFID 
            coleiraDetetada = true; // Sinaliza que a coleira foi detectada
            coleiraDetetadaTempo = millis(); // Salva o tempo em que a coleira foi detectada         
            
            Serial.println("Coleira 103 detetada.");
            Serial.println("Abrindo tampa...");
            digitalWrite(6, HIGH); // Ativa sinal para abertura das Cancelas do outro Arduino
            
            if (tampaPosicao == 0) { // Se a tampa estiver fechada
              tampa.write(90); // Abre a tampa
              tampaPosicao = 90; // Atualiza a posição da tampa
            }
        }     
      }
    } else { 
      if (millis() - coleiraDetetadaTempo > 5000 && coleiraDetetada) { // Se já passou mais de um segundo desde que a coleira foi detectada e ela ainda está sendo detectada
          Serial.println("Cartão 103 não detetado.");
          Serial.println("Fechando tampa...");
          digitalWrite(6, LOW); // Desativa sinal para abertura das Cancelas do outro Arduino
          
          if (tampaPosicao == 90) { // Se a tampa estiver aberta
            tampa.write(0); // Fecha a tampa
            tampaPosicao = 0; // Atualiza a posição da tampa
          }
          
          coleiraDetetada = false; // Sinaliza que a coleira não está mais sendo detectada
        
      }
    }

}