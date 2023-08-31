/* 시스템 입력 키
스프레이 즉시 분사 : g
와이퍼 즉시 작동 : a
와이퍼 계속 작동 : b
시스템 일시 정지 : e
시스템 재가동 : t
루틴 주기 변경 : u입력 후 10초 안에 새로운 값 입력 (단위 1sec=1000)
메모리 해제(시스템 전원 off시 누름 추천) : d */

#include <Servo.h>
#include <SoftwareSerial.h> 

class sensor_list {                    //센서부
  public:
    Servo* servo1;                     //스프레이 서보모터 7번 핀
    Servo* servo2;                     //와이퍼 서보모터 8번 핀
    Servo* servo3;                     //와이퍼 서보모터 6번 핀
    const int photoresistor = A0;         //조도센서 A0번 핀
    bool photo_routine = true;            //조도센서 on/off용 변수
    const int pin_trig = 13;            //초음파센서 trig 13번 핀
    const int pin_echo = 12;            //초음파센서 echo 12번 핀
    int ledpin1 = 2;                  //루틴 비작동 시 빨간 LED 2번 핀
    int ledpin2 = 4;                  //루틴 작동 시 초록 LED 4번 핀
};

class operation {                     //명령부
  public:
    float distance = 0;                  //초음파 거리조절용 변수
    int pos = 0;                     //서보모터 각도조절용 변수
    unsigned long previousMillis = 0;      //루틴 제어용 변수
    long interval = 15000;               //기본 루틴 : 15초에 한 번씩 동작
    long newInterval =0;               //루틴 변경용 변수
    bool wiperA = false;               //와이퍼 A모드
    bool wiperB = false;               //와이퍼 B모드
    bool routine = false;               //전체 루틴 동작 제어용 변수
    bool routine_exit = false;            //전체 루틴 동작 제어용 변수
};

SoftwareSerial btSerial(10, 11); // RX, TX 각각 10, 11번 핀

sensor_list* inits();                  //센서부 초기화용
sensor_list* s;                        
operation* operation_init();            //명령부 초기화용
operation* o;

void photo_option(sensor_list* s); 
void pho_sensor(sensor_list* s, operation* o);
void ult_sensor(operation* o);
void checkSerialInput(sensor_list* s, operation* o);
void spray(sensor_list* s, operation* o);
void spray_action(sensor_list* s);
void wiper(sensor_list* s, operation* o);
void wiper_action(sensor_list* s);

sensor_list* inits() {
    sensor_list* s = new sensor_list;
    s->servo1 = new Servo;
    s->servo2 = new Servo;
    s->servo3 = new Servo;
    return s;
}

operation* operation_init() {
    operation* o = new operation;
    return o;
}

//조도센서 on/off 결정 여부확인
void photo_option(sensor_list* s) {  
    if (btSerial.available()) {
        char in = btSerial.read();
        if (in == 'x') {        //x입력시 비활성화
            s->photo_routine = false;
            Serial.println("photo register is deactivate.");
            return;
        }
        else if (in == 'o') {   //o입력시 활성화
            s->photo_routine = true;
            Serial.println("photo register is activate.");
            return;
        }
    }
}

//조도센서 정의
void pho_sensor(sensor_list* s, operation* o) {  
    int sensorValue = analogRead(s->photoresistor); 
    Serial.print("sensor value : ");
    Serial.println(sensorValue);
    if (sensorValue > 200) {
        o->routine = false;
        return;
    }
    else {
        o->routine = true;
        return;
    }
}

//초음파센서 정의
void ult_sensor(operation* o) {  
    digitalWrite(s->pin_trig, LOW);
    delayMicroseconds(2);
    digitalWrite(s->pin_trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(s->pin_trig, LOW);

    long duration = pulseIn(s->pin_echo, HIGH);
    o-> distance = duration * 17 / 1000;  // cm 단위 거리 변환
    Serial.print("distance : ");
    Serial.println(o->distance);
    delay(500);
    if (o->distance <= 20) {  //거리가 20cm이내면 루틴 정지
        o->routine = false;
        return;
    }
    else if (o->distance > 20) {  //거리가 20cm이상시 루틴 실행
        o->routine = true;
        return;
    }
}

//루틴 주기 변경용 변수
bool time_request(operation *o) {
    unsigned long startTime = millis(); // 현재 시간 저장

    while (true) { 
        digitalWrite(s->ledpin1, HIGH);
        digitalWrite(s->ledpin2, LOW);
        if (Serial.available() > 0 || btSerial.available() > 0) { // Serial 또는 Bluetooth로부터의 입력 확인
            if(Serial.available()) {
                o->newInterval = Serial.parseInt();
            } else if(btSerial.available()) {
                o->newInterval = btSerial.parseInt(); // Bluetooth로부터의 입력 처리
            }
            
            if (o->newInterval > 0) {
                o->interval = o->newInterval;
                Serial.print("Interval updated to ");
                Serial.println(o->interval);
                return true; // 값을 받았으므로 루틴 재시작
            }
        }

        // 10초 동안 입력을 기다리고, 입력이 없어도 루틴 재시작
        if (millis() - startTime >= 10000) {
            Serial.println("No input received for 10 seconds. Cancelling.");
            return false;
        }
    }
}


//블루투스 동작 입력 확인
void checkBluetoothInput(sensor_list* s, operation* o) { 
    if (btSerial.available()) {
        char input = btSerial.read();
        switch (input) {
            case 'g':  //g입력 시 스프레이 서보모터 즉시 동작
                spray_action(s);
                delay(100);
                break;
            case 'a':  //a입력 시 와이퍼 스프레이 한 번 왕복
                o->wiperA = true;
                break;
            case 'b':  //b입력 시 와이퍼 스프레이 계속 왕복
                o->wiperB = true;
                break;
            case 's':  //s입력 시 와이퍼 b모드 중지
                Serial.println("b stop"); 
                o->wiperB = false;
                break;
            case 'e':  //e입력 시 시스템 정지 및 5초 내로 새로운 interval 입력시 루틴 주기 변경
                Serial.println("routine stop");
                o->routine = false;
                o->routine_exit = true;
                break;
            case 't':  //e입력 시 전체 루틴 시작 
                Serial.println("restart");
                o->routine = true;
                o->routine_exit = false;
                break;
            case 'u':
                Serial.println("routine stop.");
                Serial.println("you can change time of routine.");
                o->routine = false;
                o->routine_exit = true;

                if (time_request(o) == true) { 
                  Serial.println("System restart."); 
                  o->routine = true;
                  o->routine_exit = false;
                  break;
                }
                else {
                  Serial.println("cancel. System restart.");
                  o->routine = true;
                  o->routine_exit = false;
                  break;
                }
                break;
            case 'd':  //메모리 leak방지 
                Serial.println("Warning! do not Enter the a,b");
                delete s->servo1;
                delete s->servo2;
                delete s->servo3; 
                delete s;
                delete o;
                break;
            default:
                break;
        }
    }
} 


//시리얼 동작 입력 확인
void checkSerialInput(sensor_list* s, operation* o) { 
    if (Serial.available()) {
        char input = Serial.read();
        switch (input) {
            case 'g':  //g입력 시 스프레이 서보모터 즉시 동작
                spray_action(s);
                delay(100);
                break;
            case 'a':  //a입력 시 와이퍼 스프레이 한 번 왕복
                o->wiperA = true;
                break;
            case 'b':  //b입력 시 와이퍼 스프레이 계속 왕복
                o->wiperB = true;
                break;
            case 's':  //s입력 시 와이퍼 b모드 중지
                Serial.println("b stop"); 
                o->wiperB = false;
                break;
            case 'e':  //e입력 시 시스템 정지 및 5초 내로 새로운 interval 입력시 루틴 주기 변경
                Serial.println("routine stop");
                o->routine = false;
                o->routine_exit = true;
                break;
            case 't':  //e입력 시 전체 루틴 시작 
                Serial.println("restart");
                o->routine = true;
                o->routine_exit = false;
                break;
            case 'u':
                Serial.println("routine stop.");
                Serial.println("you can change time of routine.");
                o->routine = false;
                o->routine_exit = true;

                if (time_request(o) == true) { 
                  Serial.println("System restart."); 
                  o->routine = true;
                  o->routine_exit = false;
                  break;
                }
                else {
                  Serial.println("cancel. System restart.");
                  o->routine = true;
                  o->routine_exit = false;
                  break;
                }
                break;
            case 'd':  //메모리 leak방지 
                Serial.println("Warning! do not Enter the a,b");
                delete s->servo1;
                delete s->servo2;
                delete s->servo3; 
                delete s;
                delete o;
                break;
            default:
                break;
        }
    }
}

//스프레이 서보모터 정의
void spray(sensor_list* s, operation* o) {
    unsigned long currentMillis = millis();
    s->servo1->write(0);
    if ((currentMillis - o->previousMillis) >= o->interval) {
        spray_action(s);
        o->previousMillis = currentMillis;
    }
}

//스프레이 서보모터 동작 정의
void spray_action(sensor_list* s) {
    Serial.println("spray action.");
    for (int pos = 0; pos <= 90; ++pos) {
        s->servo1->write(pos);
        delay(15);
    }
    delay(1200);
    for (int pos = 90; pos >= 0; --pos) {
        s->servo1->write(pos);
        delay(15);
    }
}

//와이퍼 서보모터 정의
void wiper(sensor_list* s, operation* o) {
    s->servo2->write(0);
    s->servo3->write(0);
    if (o->wiperA) {
        wiper_action(s);
        o->wiperA = false;
    }
    else if (o->wiperB) {
        wiper_action(s);
    }
}

//와이퍼 서보모터 동작 정의
void wiper_action(sensor_list* s) {
    Serial.println("wiper action.");
    for (int pos = 0; pos < 180; ++pos) {
        s->servo2->write(pos);
        s->servo3->write(pos);
        delay(7);
    }
    for (int pos = 180; pos > 0; --pos) {
        s->servo2->write(pos);
        s->servo3->write(pos);
        delay(7);
    }
    delay(1500);
}


void setup() {
    s = inits();
    o = operation_init();

    pinMode(s->pin_trig, OUTPUT); //초음파 trig핀 13번
    pinMode(s->pin_echo, INPUT);  //초음파 echo핀 12번
    s->servo1->attach(7);      //스프레이 서보모터 7번핀
    s->servo2->attach(8);      //와이퍼 서보모터 8번 핀
    s->servo3->attach(6);      //와이퍼 서보모터 6번 핀
    pinMode(s->photoresistor, INPUT); //조도센서 A0번 핀
    pinMode(s->ledpin1, OUTPUT); //빨간 LED 2번 핀
    pinMode(s->ledpin2, OUTPUT); //초록 LED 4번 핀

    btSerial.begin(9600); 
    Serial.begin(9600);
    Serial.println("**********************");
}

void loop() {
    photo_option(s); 
    //조도센서 활성화 시
    if (s->photo_routine) { 
        while (s->photo_routine) {
            pho_sensor(s, o); //조도센서모듈 호출  
            bool temp = o->routine; //조도센서로 routine 임시 저장
            ult_sensor(o);    //초음파모듈 호출
            checkSerialInput(s, o); //시리얼모니터 입력값에 따른 처리
            checkBluetoothInput(s, o); //블루투스 입력값에 따른 처리 
            wiper(s, o);      //와이퍼 모듈 호출
          
            //초음파센서가 조도센서의 routine을 변경하지 않게끔 조정
            if (temp == false) {
                o->routine = false;
            }
            if (o->routine) {  //routine이 true일 때 지정된 동작 수행
              digitalWrite(s->ledpin1, LOW);
              digitalWrite(s->ledpin2, HIGH);
              spray(s, o);
            }
            else if (!(o->routine)) { //routine이 false일 때 동작
                digitalWrite(s->ledpin1, HIGH);
                digitalWrite(s->ledpin2, LOW);
                while (o->routine_exit) {
                    s->servo1->write(0);
                    checkSerialInput(s, o);
                    checkBluetoothInput(s, o); 
                    wiper(s, o);
                    digitalWrite(s->ledpin1, HIGH);
                    digitalWrite(s->ledpin2, LOW);
                    if (!(o->routine_exit)) {
                        break;
                    }
                }
            }
            photo_option(s); //조도센서 모듈 활성화 여부
        }
    }
   //조도센서 비활성화 시
    else {
        while (!(s->photo_routine)) {
            ult_sensor(o);
            checkSerialInput(s, o);
            checkBluetoothInput(s, o); 
            wiper(s, o);
            if (o->routine) {
                spray(s, o);
                digitalWrite(s->ledpin1, LOW);
               digitalWrite(s->ledpin2, HIGH);
            }
            else if (!(o->routine)) {
              digitalWrite(s->ledpin1, HIGH);
              digitalWrite(s->ledpin2, LOW);
                while (o->routine_exit) {
                    s->servo1->write(0);
                    checkSerialInput(s, o);
                    checkBluetoothInput(s, o); 
                    wiper(s, o);
                    digitalWrite(s->ledpin1, HIGH);
                    digitalWrite(s->ledpin2, LOW);
                    if (!(o->routine_exit)) {
                        break;
                    }
                }
            }
            photo_option(s);
        }
    }
}