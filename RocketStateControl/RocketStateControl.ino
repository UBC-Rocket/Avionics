/*
 * UBC ROCKET
 */
#include <Rocket.h>
#include <MPU.h>
#include <MPL.h>
#include <i2c_t3.h>
#include <TimerOne.h>
#include <DataCollection.h>

/*
 * State Definitions
 */
#define STANDBY           0
#define POWERED_ASCENT    1
#define COASTING          2
#define TEST_APOGEE       3
#define DEPLOY_DROGUE     4
#define DEPLOY_PAYLOAD    5
#define INITIAL_DESCENT   6
#define DEPLOY_MAIN       7
#define FINAL_DESCENT     8
#define LANDED            9

#define NUM_CHECKS        4     //each condition has to pass 5 times 

#define BURNOUT_TIME      4500
#define DROGUE_SIG_TIME   3000
#define PAYLOAD_SIG_TIME  3000
#define MAIN_SIG_TIME     5000  // TIRTH SECONDS
#define PRED_APOGEE_TIME  28000 //whatever time we think apogee will be at

//ignition Circuit Definitions
const int DROGUE_IGNITION_CIRCUIT = 7;  //pin to drogue chute ignition circuit
const int PAYLOAD_IGNITION_CIRCUIT = 8; //pin to nose cone ignition circuit
const int MAIN_IGNITION_CIRCUIT = 9;

//count variables for decisions
int launch_count, burnout_count, coasting_count, test_apogee_count, temp_apogee_count, test_apogee_failed, detect_main_alt_count, landed_count;

//time variables
unsigned long curr_time, launch_time, deploy_drogue_time, deploy_payload_time, deploy_main_time;

//temp for printing and TESTING
float curr_alt, prev_alt, avg_alt, prev_avg_alt, curr_accel[3], prev_accel[3], total_accel, prev_total_accel, avg_z_accel, prev_avg_z_accel;

Rocket rocket(STANDBY, STANDBY);
DataCollection dataCollector;
MPU *mpu1;
MPL *mpl1;

void setup() {

  Serial.begin(9600);
  //while(!Serial) delay(1);

  //Initialize pins for ignition circuits as outputs
  pinMode(DROGUE_IGNITION_CIRCUIT, OUTPUT);
  pinMode(PAYLOAD_IGNITION_CIRCUIT, OUTPUT);
  pinMode(MAIN_IGNITION_CIRCUIT, OUTPUT);
  
  mpl1 = new MPL();
  Serial.print("mpl1 created: ");
  Serial.println((int)(&mpl1), HEX);
  
  mpu1 = new MPU();
  Serial.println("MPU1 init: ");
  Serial.println(mpu1->begin(0, 0x68));

  Serial.println("MPL1 init: ");
  Serial.println(mpl1->begin(0, 0x60));

  MPU *mpus[2] = {mpu1}; // add new sensors here 
  MPL *mpls[1] = {mpl1};

  Serial.println("Data Collection init");
  dataCollector.begin(mpus, 1, mpls, 1); 

  //do we wanna turn on an LED to confirm that we're all initialized?
  
}

/*
 * main loop function
 * uses a switch statement to switch between states
 * update next state after the switch statement
 */
void loop(){
  //beadvised This MUST happen or system hangs, check this someone needs to get why
  dataCollector.saveRocketState(rocket.currentState);
  dataCollector.collect();
  curr_time = millis();

  //make sure ignition pins stay low
  if ( (deploy_drogue_time == 0) || (curr_time > deploy_drogue_time + DROGUE_SIG_TIME) )
    digitalWrite(DROGUE_IGNITION_CIRCUIT, LOW);
  if ( (deploy_payload_time == 0) || (curr_time > deploy_drogue_time + PAYLOAD_SIG_TIME) )
    digitalWrite(PAYLOAD_IGNITION_CIRCUIT, LOW);
  if ( (deploy_main_time == 0) || (curr_time > deploy_main_time + MAIN_SIG_TIME) )
    digitalWrite(MAIN_IGNITION_CIRCUIT, LOW);

  dataCollector.popAlt(curr_alt);
  dataCollector.avgPrevAlts(avg_alt, prev_avg_alt);
  dataCollector.popAccel(curr_accel);
  dataCollector.getTotalAccel(total_accel);
  dataCollector.avgPrevZAccel(avg_z_accel, prev_avg_z_accel);

  Serial.print("\nCurrent State: ");
  Serial.println(rocket.currentState);
  Serial.print("Next State: ");
  Serial.println(rocket.nextState);
  Serial.println("Current time: " + (String)curr_time);
  Serial.println("Current Altitude: " + (String)curr_alt);
  Serial.println("Average Altitude: " + (String)avg_alt);
  //TODO: sqrt squared of these values??
  Serial.println("Current Acceleration X: " + (String)curr_accel[0] + " Y: "+(String)curr_accel[1] + " Z: " +(String)curr_accel[2]);
  //Serial.println("Total Acceleration: " + (String)total_accel);
  Serial.println("Average Z Acceleration: " + (String)avg_z_accel);
  Serial.println("Prev Average Z Acceleration: " + (String)prev_avg_z_accel);

  //update the current time one last time
  curr_time = millis();

  //MAKE A STATE CHANGE----------------------------------------
  switch (rocket.currentState){
      
    case STANDBY:
      if (rocket.detect_launch(avg_z_accel, avg_alt)){
        launch_count++;
        if (launch_count > NUM_CHECKS){
          rocket.nextState = POWERED_ASCENT;
          launch_time = millis();
        }
      }    
      else
        launch_count = 0;
      break;
      
    case POWERED_ASCENT:
      if (rocket.detect_burnout(avg_z_accel))
        burnout_count++;
      else
        burnout_count = 0;

      if(burnout_count > NUM_CHECKS || curr_time > (launch_time + BURNOUT_TIME))
          rocket.nextState = COASTING;
      break;
      
    case COASTING:
      if(rocket.coasting(avg_z_accel))
        coasting_count++;
      else
        coasting_count = 0;
        
      if(coasting_count > NUM_CHECKS || curr_time > launch_time + PRED_APOGEE_TIME)//TODO: make this agree with an altitude
        rocket.nextState = TEST_APOGEE;
      break;

    case TEST_APOGEE:
      //if (rocket.test_apogee(curr_alt, prev_alt))
      if (rocket.test_apogee(avg_alt, prev_avg_alt))
        test_apogee_count++;
      else 
        test_apogee_count = 0;

      if (test_apogee_count > NUM_CHECKS)
        rocket.nextState = DEPLOY_DROGUE;
      break;
      
    case DEPLOY_DROGUE:
      digitalWrite(DROGUE_IGNITION_CIRCUIT, HIGH); //send logic 1 to ignition circuit
      deploy_drogue_time = millis();
      rocket.nextState = DEPLOY_PAYLOAD;
      break;
      
    case DEPLOY_PAYLOAD:
      if (curr_time > deploy_drogue_time + 3000){
        digitalWrite(PAYLOAD_IGNITION_CIRCUIT, HIGH); //send logic 1 to ignition circuit
        deploy_payload_time = millis();
        rocket.nextState = INITIAL_DESCENT;
      }
      break;

    case INITIAL_DESCENT:
      //if(rocket.detect_main_alt(curr_alt))
      if(rocket.detect_main_alt(avg_alt))
        detect_main_alt_count++;
      else
        detect_main_alt_count = 0;

      if(detect_main_alt_count > NUM_CHECKS)
        rocket.nextState = DEPLOY_MAIN;
      break;
      
    case DEPLOY_MAIN:
      digitalWrite(MAIN_IGNITION_CIRCUIT, HIGH); 
      rocket.nextState = FINAL_DESCENT;
      break;
      
    case FINAL_DESCENT:
      //if (rocket.final_descent(curr_alt, prev_alt, total_accel, prev_total_accel)) //returns true when we've landed
      if (rocket.final_descent(avg_alt, prev_avg_alt, avg_z_accel, prev_avg_z_accel))
        landed_count++;
      else
        landed_count = 0;
        
      if (landed_count > NUM_CHECKS)
        rocket.nextState = LANDED;
      break;

    case LANDED:
      //do something better when SD card is good to go
      rocket.flight_complete();
      break;
      
  }

  //Get Ready for next loop
  rocket.currentState = rocket.nextState;
  delay(50);

  prev_accel[0] = curr_accel[0];
  prev_accel[1] = curr_accel[1];
  prev_accel[2] = curr_accel[2];
  prev_total_accel = total_accel;

  prev_alt = curr_alt;
    
}
