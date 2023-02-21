/*
 * mpu9250 I2C addr = 0x68
 * pca9685 I2c addr = 0x40
 * 
 * download 
 * Adafruit_PWMServoDriver.h ! -> my git-Hub NURA2023/rocket/momentwheel
 * 
 * using filter & control systemm
 * LPF(Low Pass Filer)
 * Bang-Bang control(ON-OFF)
 * PID
 * 
 * 
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h> //scl = A5 / sda = A4

Adafruit_PWMServoDriver pwm=Adafruit_PWMServoDriver();

int16_t ax, ay, az, gx, gy, gz; // we use only 'gz'(yaw angular velocity)

//calibration Value
#define I_max 300                       // Anti-windup max
#define I_min 0                         // Anti-windup min
#define bangbang_control_range 0        // Bang-Bang control range factor
#define set_duty 12                     // setting zero angular velocity[%]
#define range_duty 10                    // control duty range angular velocity[%]

//PID gain
//#define Kp 0.001           // P gain
//#define Ki 0              // I gain
//#define Kd 0.01             // D gain
float Kp, Kd, Ki;

//Control & PID & LPF(Low Pass Filter) Value
float P, I, D;
float r = 0;                                  // Desired angular velocity
float y = 0;                                  // measurement angular velocity
float u = 0;                                  // Control Value-PWM duty [%]
float millisTime_i;                           // Initial Time Measurement[ms]
float millisTime_f;                           // Final time measurement[ms]
float dt = 0;                                 // Tiny time[ms]
float y_past = 0;                             // Past input data

float motor_speed, motor_speed_past, motor_speed_filter;

bool state = true;

void Input_data()
{
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(0x68, 14, true);
  ax = Wire.read() << 8 | Wire.read();
  ay = Wire.read() << 8 | Wire.read();
  az = Wire.read() << 8 | Wire.read();
  Wire.read()<< 8 | Wire.read(); //temp data
  gx = Wire.read() << 8 | Wire.read();
  gy = Wire.read() << 8 | Wire.read();
  gz = Wire.read() << 8 | Wire.read();
  
  gz = (double) gz * 250 / 32768; //change 16bit -> degree/s

  gz = constrain(gz, -50, 50);
  
  
}

float lowpassfilter(float filter, float data, float lowpass_constant)
{
  filter = data * (1 - lowpass_constant) + filter * lowpass_constant;
  return filter;   
}

float computePID(float r, float y, float dt)
{
  float error = r - y; // angular velocity increasing -> PID u decreasing

  if (error > 0) //pidbig factor
  {
    Kp = 0.2;
    Ki = 0;
    Kd = 0;
  }
  else
  {
    Kp = 0.01;
    Ki = 0;
    Kd = 0;
    
  }
  
  P = Kp * error;  
  I = I + Ki * error * dt;
  D = Kd * (y - y_past) / dt;
  
  I = constrain(I , I_min , I_max);
  
  u = P + I + D;
  
  if ( abs(error) <= bangbang_control_range)
  {
    u = 0;
  }
  
 

  u = constrain(map(u + set_duty, 0, 100, 1613, 3226), 1613, map(set_duty + range_duty, 0, 100, 1613, 3226));

  Serial.println(map(u, 1613, 3226, 0, 100));
   
  return u; //[us]  
}

void control_motor()
{
  pwm.setPWM(1,0,u);
}

void BLDC_setting(float duty)
{
  if(duty > 0)
  {
    pwm.setPWM(1,0,map(0, 0, 100, 1613, 3226)); //set BLDC
    delay(3000);
  }

  do
  {
    motor_speed = constrain(map(duty, 0, 100, 1613, 3226), 1613, 3226);
    motor_speed_filter = lowpassfilter(motor_speed_past, motor_speed, 0.9);
    //Serial.println(motor_speed_filter);
    pwm.setPWM(1,0,motor_speed_filter);
    motor_speed_past = motor_speed_filter;
    delay(100);
    }while(abs(motor_speed - motor_speed_filter) > 3);
}

void setup() 
{
  Serial.begin(9600);
  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(380); //->measurement 400hz -> 2.5ms / so pca9685 input unit mean 0.620us
  
  BLDC_setting(set_duty);  
}


void loop() 
{ 
    millisTime_f = millis();                             //measurement time
    dt = (millisTime_f - millisTime_i)*0.001;            //calculate time
    millisTime_i = millis();                             //measurement time
  
    Input_data();                                        //Road raw data
  
    y = lowpassfilter(y_past , gz, 0.99);                 //filtering raw data
  
    r = 0;
    u = computePID(r, y, dt);                            //calculate PID

    
    Serial.print(",");
    Serial.println(y);
    control_motor();                                     //control BLDC using 'u' factor
  
    y_past = y;                                          //up-date


    
}
