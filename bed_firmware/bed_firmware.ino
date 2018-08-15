#include <ESP8266WiFi.h>

const char* ssid = "SSID";
const char* password = "PSK";

#define RAISE_LED D5
#define LOWER_LED D6

#define LOWER_BED D2
#define RAISE_BED D1

// For bed controls
#define CLOSED HIGH
#define OPEN LOW

// For led indicators
#define ON LOW
#define OFF HIGH

// log of anolog voltages
word av[50];
word* avP = &av[0];

// is bed up or down?
bool bedUp = false;

// varible keeping track of where the bed is by how long the winch has been run
int bedPos = 0;

// TCP stuff
WiFiServer server(21);

// when bed is lowerd green led indicator
// when bed is raised blue led indicator

void setup() {

  pinMode(LOWER_BED, OUTPUT);
  pinMode(RAISE_BED, OUTPUT);

  pinMode(RAISE_LED, OUTPUT);
  pinMode(LOWER_LED, OUTPUT);

  // peripheral pins
  pinMode(D3, INPUT);
  pinMode(D4, INPUT);
  pinMode(D0, INPUT);
  pinMode(D7, INPUT);

  digitalWrite(LOWER_BED, OPEN);
  digitalWrite(RAISE_BED, OPEN);
  digitalWrite(RAISE_LED, OFF);
  digitalWrite(LOWER_LED, OFF);

  for (int i = 0; i < 3; i++) {
    digitalWrite(RAISE_LED, ON);
    digitalWrite(LOWER_LED, ON);
    delay(500);
    digitalWrite(RAISE_LED, OFF);
    digitalWrite(LOWER_LED, OFF);
    delay(500);
  }

  Serial.begin(9600);

  // put bed down if up
  while (!checkBedDown()) {
    delay(50);
  }

  Serial.println(bedUp);
  if (bedUp)
    auto_bed_down();


  // Connect to wifi
  IPAddress ip(192, 168, 1, 26);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(ip, gateway, subnet);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Failed to connect to wifi");
  }

  Serial.print("Connected to "); Serial.println(ssid);
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());

  server.begin();
}

void raise_bed(int mil_time) {
  if (bedPos + (mil_time / 1000) < 6) {
    Serial.println("going up");
    digitalWrite(RAISE_LED, ON);
    digitalWrite(RAISE_BED, CLOSED);
    delay(mil_time);
    digitalWrite(RAISE_BED, OPEN);
    digitalWrite(RAISE_LED, OFF);
    bedPos += (mil_time / 1000);
  }
}

void lower_bed(int mil_time) {
  if (bedPos > 0) {
    Serial.println("going down");
    digitalWrite(LOWER_LED, ON);
    digitalWrite(LOWER_BED, CLOSED);
    delay(mil_time);
    digitalWrite(LOWER_BED, OPEN);
    digitalWrite(LOWER_LED, OFF);
    bedPos -= (mil_time / 1000);
  }
}

String readFromSock(WiFiClient sClient) {
  String str = "";
  int accumDelay = 0;
  while (true) {
    if (sClient.available() > 0) {
      Serial.println("reading from client");
      for (int i = 0; i < 100; i++) {
        char c = sClient.read();
        if (c == 0) {
          Serial.println(accumDelay);
          break;
        }
        Serial.write(c);
        str += c;
      }
      return str;
    } else {
      delay(50);
      accumDelay += 50;
      if (accumDelay > 10 * 1000) {
        return "NULL";
      }
    }
  }
}

bool checkBedDown() {
  // read from anaolog to see if bed is up or down
  if ((((int) & (*avP) - (int) &av[0]) / 2) > 49) {
    avP = &av[0];
    *avP = (word) analogRead(A0);
    // 50 values from anolog have been read over 500 mili sec -> take average and deterimine if bed is up or down
    int sum = 0;
    for (int i = 0; i < 50; i++) {
      sum += (int) av[i];
    }
    double avg = sum / 50;
    bedUp = (avg > 1020);
    return true;
  } else {
    *avP = (word) analogRead(A0);
    avP += 1;
  }
  return false;
}

// test periphials for ground *NOTE* D0 and D7 stay down after being grounded then set to float -> must be kept high
void test_periph() {
  checkBedDown();
  if (!digitalRead(D3)) {
    raise_bed(1000);
  } else if (!digitalRead(D4)) {
    auto_bed_down();
    delay(1000);
  }
}

bool auto_bed_down() {
  double time_elap = 0;
  digitalWrite(LOWER_LED, ON);
  digitalWrite(LOWER_BED, CLOSED);
  while (bedUp) {
    while (!checkBedDown()) {
      delay(1);
      time_elap += 1.4;
    }
    //Serial.println(time_elap);
    if (time_elap / 1000 > 8) {
      Serial.println("8 sec have elapsed bed is not down -> posible mechanical failure?");
      Serial.println("ERROR auto bed down exiting");
      digitalWrite(LOWER_BED, OPEN);
      digitalWrite(LOWER_LED, OFF);
      return false;
    }
  }
  // delay a little longer so their is some slack in cable
  delay(100);
  digitalWrite(LOWER_BED, OPEN);
  digitalWrite(LOWER_LED, OFF);
  Serial.print("Bed is down! took ");
  Serial.print((double) time_elap / 1000.0);
  Serial.println(" sec");
  bedPos = 0;
  return true;
}

void loop() {
  test_periph();
  WiFiClient sClient = server.available();
  if (sClient) {
    sClient.flush();
    sClient.println("BED SERVER");
    String str = readFromSock(sClient);
    if (str.startsWith("UP")) {
      Serial.println("going up");
      int amt = str[str.length() - 1] - '0';
      if (amt > 5 || amt < 0) {
        Serial.println("invalid amt cannot be larger than 5 sec");
      } else {
        raise_bed(amt * 1000);
      }
    } else if (str.startsWith("DOWN")) {
      Serial.println("going down.");
      int amt = str[str.length() - 1] - '0';
      if (amt > 5 || amt < 0) {
        Serial.println("invalid amt cannot be larger than 5 sec");
      } else {
        lower_bed(amt * 1000);
      }
    } else if (str.equals("ADOWN")) {
      Serial.println("auto down...");
      auto_bed_down();
    } else {
      Serial.println("ERROR no such cmd. -> ");
      Serial.print(str);
    }
    Serial.print("Bed pos -> ");
    Serial.println(bedPos);
  }
  delay(10);
}
