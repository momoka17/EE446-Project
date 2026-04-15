#include <Arduino_APDS9960.h>
#include <Arduino_BMI270_BMM150.h>
#include <PDM.h>

volatile int samplesRead = 0;
short sampleBuffer[256];
float audioLevel = 0.0;

// Threshold
const int LIGHT_THRESHOLD = 100;       
const int PROX_THRESHOLD  = 40; 
const float MOTION_THRESHOLD = 0.20;
const float SOUND_THRESHOLD = 100;

// IMU values
float prevX = 0.0, prevY = 0.0, prevZ = 0.0;
bool firstIMURead = true;

// PDM
void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

// Sound function, returns average sound level
int readSoundLevel() {
  if (samplesRead) {
    long sum = 0;
    for (int i = 0; i < samplesRead; i++) {
      sum += abs(sampleBuffer[i]);
    }
    int level = sum / samplesRead;
    samplesRead = 0;
    return level;
  }
  return 0;
}

// Light function, returns channel brightness
int readLightLevel() {
  int r, g, b, c;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
    return c;
  }
  return -1;
}

// Proximity function, returns proximity level
int readProximityLevel() {
  if (APDS.proximityAvailable()) {
    return APDS.readProximity();
  }
  return -1;
}

// Motion function, return the acceleration magnitude change 
float readMotionLevel() {
  float x, y, z;

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);

    if (firstIMURead) {
      prevX = x;
      prevY = y;
      prevZ = z;
      firstIMURead = false;
      return 0.0;
    }

    float dx = x - prevX;
    float dy = y - prevY;
    float dz = z - prevZ;

    prevX = x;
    prevY = y;
    prevZ = z;

    float motion = sqrt(dx * dx + dy * dy + dz * dz);
    return motion;
  }

  return 0.0;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1500);

  //IMU
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }

  //APDS9960
  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960");
    while (1);
  }


  //Microphone
  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM microphone");
    while (1);
  }

  Serial.println("Smart Workspace Situation Classifier Started.");
  Serial.println("sound,light,prox,motion,label");

}

void loop() {
  // put your main code here, to run repeatedly:
  int soundLevel = readSoundLevel();
  int lightLevel = readLightLevel();
  int proximityLevel = readProximityLevel();
  float motionLevel = readMotionLevel();

  // Binary decision
  bool noisy = (soundLevel > SOUND_THRESHOLD);
  bool bright = (lightLevel > LIGHT_THRESHOLD);
  bool nearUser = (proximityLevel > PROX_THRESHOLD);
  bool moving = (motionLevel > MOTION_THRESHOLD);

  String label;
  if (!noisy && bright && !moving && !nearUser) {
    label = "QUIET_BRIGHT_STEADY_FAR";
  }
  else if (noisy && bright && !moving && !nearUser) {
    label = "NOISY_BRIGHT_STEADY_FAR";
  }
  else if (!noisy && !bright && !moving && nearUser) {
    label = "QUIET_DARK_STEADY_NEAR";
  }
  else if (noisy && bright && moving && nearUser) {
    label = "NOISY_BRIGHT_MOVING_NEAR";
  }
  else {
    if (nearUser) {
      if (moving || noisy) {
        label = "NOISY_BRIGHT_MOVING_NEAR";
      } else {
        label = "QUIET_DARK_STEADY_NEAR";
      }
    } else {
      if (noisy) {
        label = "NOISY_BRIGHT_STEADY_FAR";
      } else {
        label = "QUIET_BRIGHT_STEADY_FAR";
      }
    }
  }

  Serial.print(soundLevel);
  Serial.print(",");
  Serial.print(lightLevel);
  Serial.print(",");
  Serial.print(proximityLevel);
  Serial.print(",");
  Serial.print(motionLevel, 3);
  Serial.print(",");
  Serial.println(label);

  delay(500);

}
