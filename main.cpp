// Hello World to sweep a servo through its full range

#include "mbed.h"
#include "Servo.h"
#include "Motor.h"
#include "rtos.h"
#include "ultrasonic.h"

//Bluetooth detection
char bnum=0;
char bhit=0;

//Servo Positions
float featherWandPosition = 0.0;
float foodDispenserPosition = 0.0;
float laserPointerPosition = 0.0;

//Mode
int mode = 0;
InterruptIn pb(p8); //Changes Mode; Mode 0 = Bluetooth; Mode 1 = Automated


// Bluetooth

BusOut myled(LED1,LED2,LED3,LED4);
Serial blue(p28,p27);


Servo featherWand(p21);
Servo foodDispenser(p22);
Motor leftMotor(p24, p16, p15); // pwm, fwd, rev, can break
Motor rightMotor(p25, p17, p18); // pwm, fwd, rev, can break

Servo laserPointer(p23);

//Ticker
Ticker foodTicker;

// Mutex locks
Mutex feather_mutex;
Mutex laser_mutex;
Mutex dispenser_mutex;
Mutex mode_mutex;
Mutex motor_mutex;


//Mode Interrupt
void modeInterrupt (void) {
    mode_mutex.lock();
    mode = !mode;
    mode_mutex.unlock();
}



//Ultrasonic Distance
void dist(int distance){
    if( distance < 70 ){ //Auto turn left when distance is less than 5cm
        motor_mutex.lock();
        leftMotor.speed(0.0);
        rightMotor.speed(0.0);
        leftMotor.speed(-5.0);
        rightMotor.speed(5.0);
        Thread::wait(400);
        leftMotor.speed(0.0);
        rightMotor.speed(0.0);
        motor_mutex.unlock();
    }
}

ultrasonic mu(p6, p7, .1, 1, &dist);

//Automated Laser Movement Thread
bool laserOn = false;
void autoLaser(void const *argument){
    while(1){
        while(laserOn){
            laserPointer = ((double)rand() / (double)2) -1;
            Thread::wait(1000);
        }
    }
}

//Automated Food Dispenser Thread (kinda stupid how I wrote this lol)
bool dispenserOn = false;
void autoDispenser(void const *argument){
    while(1){
        dispenser_mutex.lock();
        if(dispenserOn){
            //for(int p=0; p<1; p++){ //Loop 5 times
                //foodDispenser = 1.0;
                //Thread::wait(200);
                foodDispenser = 0.0;
                Thread::wait(400);

                foodDispenser = 7.0;
                
                Thread::wait(400);

                dispenserOn = false;
            //}
        } else {
            foodDispenser = 7.0;
            Thread::wait(200);
            foodDispenser = 0.0;   
        }
        dispenser_mutex.unlock();
    }
}

void enableDispenser(){
    dispenser_mutex.lock();
    dispenserOn = true;
    dispenser_mutex.unlock();
}
    
    
// Automated Movement Mode Thread
void autoMove(void const *argument){
     //Ultrasonic Start
    foodTicker.attach(&enableDispenser, 300); // Enable food dispenser every 5 minutes;
    mu.startUpdates();//start measuring the distance
    while(1){
        mode_mutex.lock();
        if(mode == 1){
            //Ultrasonic Object Detection
            mu.checkDistance();
            
            //Enable Laser
            laserOn = true;
            
            //Move Forward
            motor_mutex.lock();
            leftMotor.speed(0.5);
            rightMotor.speed(0.5);
            motor_mutex.unlock();
            
            feather_mutex.lock();
            //Move feather wand randomly
            featherWandPosition = ((double)rand() / (double)2) -1;
            featherWand = featherWandPosition;
            feather_mutex.unlock();

        } else {
            laserOn = false;
            
            
            feather_mutex.lock();
            featherWandPosition = 0.0;
            featherWand = featherWandPosition;
            feather_mutex.unlock();           
            
            foodTicker.detach();
            motor_mutex.lock();
            leftMotor.speed(0.0);
            rightMotor.speed(0.0);
            motor_mutex.unlock();
        }
        Thread::wait(200);
    mode_mutex.unlock();

    }
}




int main() { 
    //Ultrasonic Start
    foodTicker.attach(&enableDispenser, 300); // Enable food dispenser every 5 minutes;
    mu.startUpdates();//start measuring the distance
    
    foodDispenser = 7.0;
    featherWand = 0.0;
    wait(0.1);
    pb.mode(PullUp);
    wait(.01);
    pb.fall(&modeInterrupt);
    
    //Thread t1(autoMove);
    Thread t2(autoLaser);
    Thread t3(autoDispenser);
  
    while(1){
        if(mode == 0) {
             //Bluetooth Button Hit 
            if (blue.getc()=='!') {
                if (blue.getc()=='B') { //button data packet
                    bnum = blue.getc(); //button number
                    bhit = blue.getc(); //1=hit, 0=release
                    if (blue.getc()==char(~('!' + 'B' + bnum + bhit))) { //checksum OK?
                        myled = bnum - '0'; //current button number will appear on LEDs
                        
                        //Switch Cases
                        switch (bnum) {
                            
                            // ********************  
                            // Toy Buttons
                            // ********************  
                            
                            case '1': //number button 1 = Rotate Feather Left
                                if (bhit=='1') {
                                    feather_mutex.lock();

                                    if ( (featherWandPosition > -1.0) ) {
                                        featherWandPosition = featherWandPosition - 0.2;
                                        featherWand = featherWandPosition;
                                    }
                                    feather_mutex.unlock();
                                }
                                break;
                            case '2': //number button 2 = Rotate Feather Right
                                if (bhit=='1') {
                                    feather_mutex.lock();

                                    if ( (featherWandPosition < 1.0) ) {
                                        featherWandPosition = featherWandPosition + 0.2;
                                        featherWand = featherWandPosition;
                                    }
                                    feather_mutex.unlock();
                                }
                                break;
                            case '3': //number button 3 = Release Food Dispenser
                                if (bhit=='1') {
                                    dispenser_mutex.lock();
                                    dispenserOn = true;
                                    dispenser_mutex.unlock();

                                }
                                break;
                            case '4': //number button 4 = Automated Laser Enable
                                if (bhit=='1') {
                                     mode_mutex.lock();
                                    mode = !mode;
                                    mode_mutex.unlock();
                                } 
                                break;
                                
                                
                            // ********************   
                            //Robot Motor Movement   
                            // ********************  
                                
                            case '5': //button 5 up arrow
                                if (bhit=='1') {
                                    leftMotor.speed(1.0);
                                    rightMotor.speed(1.0);
                                } else {
                                    leftMotor.speed(0.0);
                                    rightMotor.speed(0.0);
                                }
                                break;
                            case '6': //button 6 down arrow
                                if (bhit=='1') {
                                    leftMotor.speed(-1.0);
                                    rightMotor.speed(-1.0);
                                } else {
                                    leftMotor.speed(0.0);
                                    rightMotor.speed(0.0);
                                }
                                break;
                            case '7': //button 7 left arrow
                                if (bhit=='1') {
                                    leftMotor.speed(-1.0);
                                    rightMotor.speed(1.0);
                                } else {
                                    leftMotor.speed(0.0);
                                    rightMotor.speed(0.0);
                                }
                                break;
                            case '8': //button 8 right arrow
                                if (bhit=='1') {
                                    leftMotor.speed(1.0);
                                    rightMotor.speed(-1.0);
                                } else {
                                    leftMotor.speed(0.0);
                                    rightMotor.speed(0.0);
                                }
                                break;
                            default:
                                break;
                        }
                    }
                }
            }   
            
        } else if (mode == 1) {
            if(mode == 1){
            //Ultrasonic Object Detection
            mu.checkDistance();
            
            //Enable Laser
            laserOn = true;
            
            //Move Forward
            motor_mutex.lock();
            leftMotor.speed(0.5);
            rightMotor.speed(0.5);
            motor_mutex.unlock();
            
            feather_mutex.lock();
            //Move feather wand randomly
            featherWandPosition = ((double)rand() / (double)2) -1;
            featherWand = featherWandPosition;
            feather_mutex.unlock();

        } else {
                 laserOn = false;
            
            
                feather_mutex.lock();
                featherWandPosition = 0.0;
                featherWand = featherWandPosition;
                feather_mutex.unlock();           
                
                foodTicker.detach();
                motor_mutex.lock();
                leftMotor.speed(0.0);
                rightMotor.speed(0.0);
                motor_mutex.unlock();     
                
       }
    
    }
    
}       
}