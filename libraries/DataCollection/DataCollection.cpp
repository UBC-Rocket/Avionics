#include "DataCollection.h"

#define ACCEL_FS 16
#define GYRO_FS 2000
#define AVG_SIZE 5
#define ALT_AVG_SIZE 8

int DataCollection::begin(MPU *mpu[], int MPUlen, MPL *mpl[], int MPLlen) {
  bufPosition = 0;

  MPULength = MPUlen;
  MPLLength = MPLlen;

  for(int i = 0; i < MPULength; i++) {
    mpus[i] = mpu[i];
    mpuError[i] = 0;

    mpus[i]->initAccel(ACCEL_FS);
    mpus[i]->initGyro(GYRO_FS);
    mpus[i]->initMag();
  }

  for(int i = 0; i < MPLLength; i++) {
    mpls[i] = mpl[i];
    mplError[i] = 0;
  }

  pinMode(10, OUTPUT); //neccesary for some reason?

  if(!SD.begin(4)) Serial.println("SD failed to load");
  else Serial.println("SD initialized");

  File dataFile = SD.open("/rdata.txt", FILE_WRITE);
  if(!dataFile) debug("Failed to open SD File for initialization");
  else debug("Opened SD file for initialization: " + (String)dataFile.name());

  return 0;
}

int DataCollection::filterData() {
  return 0;
}

int DataCollection::popGyro(float gyro[]) {
  if(bufPosition < 1) return -1;
  readBuffer3(gyroReadings, gyro, bufPosition - 1);
  return 0;
}

/**
 * Get the most recent accelerometer reading
 * \param accel A 3 element array to be filled with the x y and z components of accelerometer reading
 * \return Returns 0 on success or -1 if buffer is empty
 */

int DataCollection::popAccel(float accel[]) {
  if(bufPosition < 1) return -1;
  readBuffer3(accelReadings, accel, bufPosition - 1);
  return 0;
}

/*
 * Update the array containing the previous accel  
 */
int DataCollection::updatePrevZAccel(){
  float curr_accel[3]; //
  for(int i = AVG_SIZE - 1; i > 0; i--){
	  prev_z_accel[i] = prev_z_accel[i-1];
  }
  popAccel(curr_accel);
  prev_z_accel[0] = curr_accel[2];
  return 0;
}

/*
 * Average the previous Z acceleration readings 
 * \param avgZAccel a float to be filled with the average of the previous accel readings (including the most recent one)
 * \param prevAvgZAccel a float to be filled with the average of the previous accel readings
 */
int DataCollection::avgPrevZAccel(float &avgZAccel, float &prevAvgZAccel) {
  prevAvgZAccel = 0;
  avgZAccel = 0;
  for(int i = 0; i < AVG_SIZE; i++){
	prevAvgZAccel += prev_z_accel[i];
  }
  prevAvgZAccel /= (AVG_SIZE );

  updatePrevZAccel();
  
  for(int i = 0; i < AVG_SIZE; i++){
	  avgZAccel += prev_z_accel[i];
  }
  avgZAccel /= (AVG_SIZE );
  return 0;
}

int DataCollection::getTotalAccel(float total_accel){
  float accel[3];
  popAccel(accel);
  
  total_accel = accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2];
  total_accel = sqrt(total_accel);
  return 0;
}

int DataCollection::updatePrevTotalAccel(){
  float curr_total_accel;
  
  for(int i = AVG_SIZE - 1; i > 0; i--){
	  prev_z_accel[i] = prev_z_accel[i-1];
  }
  getTotalAccel(curr_total_accel);
  prev_total_accel[0] = curr_total_accel;
  return 0;
}

int DataCollection::avgPrevTotalAccel(float &avgTotalAccel, float &prevAvgTotalAccel) {
  avgTotalAccel = 0;
  prevAvgTotalAccel = 0;
  for(int i = 0; i < AVG_SIZE; i++){
	prevAvgTotalAccel += prev_total_accel[i];
  }
  prevAvgTotalAccel /= AVG_SIZE;

  updatePrevTotalAccel();
  
  for(int i = 0; i < AVG_SIZE; i++){
	  avgTotalAccel += prev_total_accel[i];
  }
  avgTotalAccel /= AVG_SIZE;
  return 0;
}

/**
 * Get most recent magnetometer reading
 * \param mag A 3 element array to be filled with the x y and z components of magnetometer reading
 * \return Returns 0 on success or -1 if buffer is empty
 */

int DataCollection::popMag(float mag[]) {
  if(bufPosition < 1) return -1;
  readBuffer3(magReadings, mag, bufPosition - 1);
  return 0;
}

/**
 * Get most recent altitude reading.
 * \param alt a float to be filled with last altitude
 * \return Returns most recent altitude reading or -1 if the buffer is empty
 */

int DataCollection::popAlt(float &alt) {
  if(bufPosition < 1) return -1;
  alt = altReadings[bufPosition - 1];
  return 0;
}

/*
 * Update the array containing the previous altitudes 
 */
int DataCollection::updatePrevAlts(){
  for(int i = ALT_AVG_SIZE - 1; i > 0; i--){
	  prev_alts[i] = prev_alts[i-1];
  }
  popAlt(prev_alts[0]);
  return 0;
}

/*
 * Average the previous altitude readings 
 * \param avgAlt a float to be filled with the average of the previous alt readings (including the most recent one)
 * \param prevAvgAlt a float to be filled with the average of the previous alt readings
 */
int DataCollection::avgPrevAlts(float &avgAlt, float &prevAvgAlt) {
  avgAlt = 0;
  prevAvgAlt = 0;
  for(int i = 0; i < ALT_AVG_SIZE; i++){
	prevAvgAlt += prev_alts[i];
  }
  prevAvgAlt /= ALT_AVG_SIZE;

  updatePrevAlts();
  
  for(int i = 0; i < ALT_AVG_SIZE; i++){
	  avgAlt += prev_alts[i];
  }
  avgAlt /= ALT_AVG_SIZE;
  return 0;
}

/**
 * Collect a timestep of data and write to buffer.
 * Collect expects all sensors to be properly initialized.
 * \return Returns amount of space left in buffer or -1 if buffer in use.
 */

int DataCollection::collect() {
  if(bufPosition >= BUFFER_SIZE) writeData();

  time[bufPosition] = millis();
  int droppedReadings; //number of sensor readings ignored due to communication issues

  //Buffers for one timestep of readings from all sensors.
  float gyros[MPULength][3];
  float accels[MPULength][3];
  float mags[MPULength][3];
  float alts[MPLLength];

  droppedReadings = 0;
  for(int x = 0; x < MPULength; x++) {
    if(mpuError[x]) {
      droppedReadings++;
      continue;
    }

    MPU *mpu = mpus[x];

    float tmp[3];
    mpuError[x] = mpuError[x] ? mpuError[x] : mpu->readGyro(tmp);
    loadBuffer3(tmp, gyros, x-droppedReadings);

    mpuError[x] = mpuError[x] ? mpuError[x] : mpu->readAccel(tmp);
    loadBuffer3(tmp, accels, x-droppedReadings);
    debug(tmp[2]);

    mpuError[x] = mpuError[x] ? mpuError[x] : mpu->readMag(tmp);
    loadBuffer3(tmp, mags, x-droppedReadings);
  }

  float tmp[3];

  //average each set of data from MPU and load in to buffer
  average3(gyros, MPULength - droppedReadings, tmp);
  loadBuffer3(tmp, gyroReadings, bufPosition);

  average3(accels, MPULength - droppedReadings, tmp);
  loadBuffer3(tmp, accelReadings, bufPosition);

  average3(mags, MPULength - droppedReadings, tmp);
  loadBuffer3(tmp, magReadings, bufPosition);

  droppedReadings = 0;
  for(int x = 0; x < MPLLength; x++) {
    if(mplError[x]) {
      droppedReadings++;
      continue;
    }

    MPL *mpl = mpls[x];

    float alt;
    mplError[x] = mplError[x] ? mplError[x] : mpl->readAGL(alt);
    alts[x - droppedReadings] = alt;
  }

  altReadings[bufPosition] = average(alts, MPLLength - droppedReadings);

  return BUFFER_SIZE - bufPosition++;
}


/* saveRocketState: Saves the rockets state to the state buffer to be saved next time write data is called
*/


int DataCollection::saveRocketState(int state) {
	
	states[bufPosition] = state;
	return 0;
}

/* writeData: writes contents of buffer to SD card.
* returns: 0 on success, -1 if the buffer is locked, or SD card error code.
*/

int DataCollection::writeData() {
  float lastGyro[3];
  float lastAccel[3];
  float lastMag[3];
  float lastAlt;
  unsigned long lastTime;

  readBuffer3(gyroReadings, lastGyro, bufPosition - 1);
  readBuffer3(accelReadings, lastAccel, bufPosition - 1);
  readBuffer3(magReadings, lastMag, bufPosition - 1);
  lastAlt = altReadings[bufPosition - 1];
  lastTime = time[bufPosition - 1];

  File dataFile = SD.open("/rdata.txt", FILE_WRITE);
  if(!dataFile) debug("Failed to open SD File for writing");
  else debug("Opened SD file for writing: " + (String)dataFile.name());

  for(int i = 0; i < bufPosition; i++) {
    dataFile.print(time[i]);
    dataFile.print("; ");

    dataFile.print(gyroReadings[i][0]);
    dataFile.print(", ");
    dataFile.print(gyroReadings[i][1]);
    dataFile.print(", ");
    dataFile.print(gyroReadings[i][2]);
    dataFile.print("; ");

    dataFile.print(accelReadings[i][0]);
    dataFile.print(", ");
    dataFile.print(accelReadings[i][1]);
    dataFile.print(", ");
    dataFile.print(accelReadings[i][2]);
    dataFile.print("; ");

    dataFile.print(altReadings[i]);
    dataFile.println("; ");

	dataFile.print(states[i]);
	dataFile.println("; ");
  }

  dataFile.close();

  bufPosition = 0;
  loadBuffer3(lastGyro, gyroReadings, bufPosition);
  loadBuffer3(lastAccel, accelReadings, bufPosition);
  loadBuffer3(lastMag, magReadings, bufPosition);
  altReadings[bufPosition] = lastAlt;
  time[bufPosition] = lastTime;

  bufPosition++;
  return 0;
}

void DataCollection::readBuffer3(float buf[][3], float data[], int pos) {
  data[0] = buf[pos][0];
  data[1] = buf[pos][1];
  data[2] = buf[pos][2];
}

void DataCollection::loadBuffer3(float data[], float buf[][3], int pos) {
  buf[pos][0] = data[0];
  buf[pos][1] = data[1];
  buf[pos][2] = data[2];
}

void DataCollection::average3(float data[][3], int length, float avg[]) {
  avg[0] = 0;
  avg[1] = 0;
  avg[2] = 0;

  for(int x = 0; x < length; x++) {
    avg[0] += data[x][0];
    avg[1] += data[x][1];
    avg[2] += data[x][2];
  }

  avg[0] /= length;
  avg[1] /= length;
  avg[2] /= length;
}

float DataCollection::average(float data[], int length) {
  float avg = 0;;
  for(int x = 0; x < length; x++) {
    avg += data[x];
  }

  return avg/length; //divid by zero, care
}

void DataCollection::debug(String msg) {
  Serial.println(msg);
}