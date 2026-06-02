//Written By Dean Urschel
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <math.h>
#include <string.h>
#define PI 3.14159265
#define cycleResolution 8
#define radianResolution 45
#define zeroModulation .5
#define oneModulation 1.5
//total length of modulated array global variable
int modulatedLength;
//create carrier wave

struct EMWAVE{
    float frequency;   //in hertz
float amplitude;
/*
This array is a numerical representation of the carrier wave over the time period of 1 second
The wave is a sinusoidal one.
*/

FILE *file;
int *waveRepresentation;
};



int *createCarrierWave(struct EMWAVE cWave);
float waveFunction(float amplitude,float time,int verticleDisplacement);

//CREATE FUNCTIONS TO MODULATE CARRIER WAVE ARRAY ACCORDING TO IMAGE FILE AND GET LENGTH OF FILE AND ANOTHER FUNCTION TO PUT MODULATED DATA INTO FILE
struct EMWAVE mWave(struct EMWAVE cWave,FILE *Image);
int fileLength(FILE *file);
void writeWaveToFile(struct EMWAVE wave,FILE *file,int dataLength);
int main()
{
    //to do: prompt user for wave into to generate

    //quick carrier wave creator test
    struct EMWAVE cWave;
    cWave.amplitude=50;
    cWave.frequency=12500;
    cWave.waveRepresentation=createCarrierWave(cWave);
    //create modulated wave struct
    struct EMWAVE modulatedWave;
    //create image file to modulate wave with
    FILE *Image=fopen("x-files.jpg","rb");
    modulatedWave=mWave(cWave,Image);
    modulatedWave.frequency=cWave.frequency;
    modulatedWave.amplitude=cWave.amplitude;
    //print out modulated wave as a test
    int z;
    float timingVar=1.0/(modulatedWave.frequency*cycleResolution);
    for(z=0;z<modulatedLength;z++){
printf("Modulated EM value at time %f seconds: %d\n",(float)z*timingVar,(int)*(modulatedWave.waveRepresentation+z));
    }
//end of print out test

//now lets write this wave to a file

FILE *waveFile=fopen("modulatedWave.txt","w+");
writeWaveToFile(modulatedWave,waveFile,fileLength(Image));

    return 0;
}

int *createCarrierWave(struct EMWAVE cWave){
cWave.waveRepresentation=malloc(100000*sizeof(int));
    int x;
    float piNum=radianResolution;
    int y=0;
float timingVar=1.0/(cWave.frequency*cycleResolution);
    for(x=0;x<cWave.frequency*cycleResolution;x++){
float val=waveFunction(cWave.amplitude,y*piNum,0);
*(cWave.waveRepresentation+x)=(int)val;
printf("EM value at time %f seconds: %d\n",(float)x*timingVar,(int)val);
if(y==cycleResolution){
    y=0;
}
y++;
    }

return cWave.waveRepresentation;
}

float waveFunction(float amplitude,float time,int verticleDisplacement){
double ret, val;



   val = PI / 180;
   ret = (amplitude)*(sin(time*val))+verticleDisplacement;
   return (float)ret;
}

struct EMWAVE mWave(struct EMWAVE cWave,FILE *Image){
    struct EMWAVE mWave;
int dataLength=cycleResolution*cWave.frequency;
int length=fileLength(Image);
int x;
int z=0;
int binaryValue;
mWave.waveRepresentation=malloc(100000*sizeof(float));
//take one fourth of the length
for(x=0;x<length;x++){
        char imageByte=fgetc(Image);

                //loop through each byte of the image and modulate the array accordingly

  int i;


  for (i = 0; i < 8; i++) {

        if(cWave.waveRepresentation[z]==(int)0){
        //if the value is zero then we modify it to be equal to one so that we can modulate that value
    cWave.waveRepresentation[z]=1;
}
      binaryValue=!!((imageByte << i) & 0x80);

      if(binaryValue==0){
            *(mWave.waveRepresentation+z)=(cWave.waveRepresentation[z])*.5;

      }
      if(binaryValue==1){
          *(mWave.waveRepresentation+z)=(cWave.waveRepresentation[z])*1.5;

      }
      z++;

  }

  }




modulatedLength=z;
return mWave;
}

int fileLength(FILE *file){
fseek(file,0,SEEK_END);
int length=ftell(file);
fseek(file,0,SEEK_SET);
return length;
}



void writeWaveToFile(struct EMWAVE wave,FILE *file,int dataLength){
    float timingVar=1.0/(wave.frequency*cycleResolution);
    float pulseLength=(dataLength*8)/(wave.frequency*cycleResolution);
fprintf(file,"This file contains the discreet representation of a modulated EMwave\n");
fprintf (file,"Frequency: %f Hz || Amplitude: %f nM || Amount of data contained: %d bytes || Length of pulse: %f seconds\n",wave.frequency,wave.amplitude,dataLength,pulseLength);
int x;
for(x=0;x<dataLength*cycleResolution;x++){
 fprintf(file,"Time Stamp: %f seconds : amplitude : %d \n",(float)x*timingVar,*(wave.waveRepresentation+x));

}

}
