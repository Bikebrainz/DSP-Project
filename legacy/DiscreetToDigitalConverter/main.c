//written by dean urschel
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

//public variable to keep track of the number of lines in the wave file
int totalLines=0;
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
//functions to create carrier wave for reference
int *createCarrierWave(struct EMWAVE cWave);
float waveFunction(float amplitude,float time,int verticleDisplacement);
int *imageBytes(FILE *waveFile,struct EMWAVE wave);


int main(){
    //setup the carrier wave
    struct EMWAVE carrier;
    carrier.frequency=12500;
    carrier.amplitude=50;
    carrier.waveRepresentation=createCarrierWave(carrier);
    //open the file containing the modulated wave
    FILE *waveFile=fopen("modulatedWave.txt","r+");
    int *fileArray=malloc(sizeof(char)*100000);
    fileArray=imageBytes(waveFile,carrier);
    //convert binary array to char array


    int x;
    int y=0;
    char realFileArray[9]={0};
    int n=1;
    int binaryVal=0;
    int binaryAdder=0;
FILE *Image=fopen("Image.jpg","a+b");
    for(x=0;x<totalLines+1;x++){

if(y==8){
realFileArray[8]='\0';

printf("realFileArray: %s || int representation: %d \n",realFileArray,binaryVal);
char character=binaryVal;
//put char to file
fputc(character,Image);

    y=0;

    binaryVal=0;
    n=1;
    realFileArray[0]='\0';
    if (x==totalLines+1){
        break;
    }

}
realFileArray[y]=(char)fileArray[x]+'0';
switch(n){
case 8:
    binaryAdder=1;
    break;
    case 7:
        binaryAdder=2;
        break;
        case 6:
            binaryAdder=4;
            break;
           case 5:
                binaryAdder=8;
                break;
                case 4:
                    binaryAdder=16;
                    break;
                   case 3:
                        binaryAdder=32;
                        break;
                        case 2:
                            binaryAdder=64;
                            break;
                            case 1:
                                binaryAdder=128;
                                break;
}
int u=x-(n-1);
if(fileArray[u]==1){
        if(n==8){
            binaryVal-=1;



        }

   if(fileArray[x]==1){
    binaryVal+=0;
}else{
binaryVal-=binaryAdder;
}
}else{
if(fileArray[x]==0){
    binaryVal+=0;
}else{
binaryVal+=binaryAdder;
}
}

y++;
n++;
    }



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


int *imageBytes(FILE *waveFile,struct EMWAVE wave){
    int *binaryArray=malloc(100000*sizeof(int));
    //loop through the first few descriptive lines in the EMWAVE file
    int x;
    char *intString=malloc(sizeof(char)*500);
    for(x=0;x<2;x++){
    fgets(intString,500*sizeof(char),waveFile);


    }
     //now we loop through and grab the amplitudes and convert them to binary and return a byte array
     int z=0;

     //create file char array to be returned by function
char *fileArray=malloc(sizeof(char)*100000);

int byteIndex=0;


     while(fgets(intString,500*sizeof(char),waveFile)!=NULL){



        int endOfLine=0;
        int y=0;
        char *number=malloc(sizeof(char)*100);
        while(endOfLine==0){
            if(intString[y+43]=='\n'){
                endOfLine=1;
            }
            *(number+y)=*(intString+y+43);
            y++;
        }
        //compare modulated value to carrier wave value
        int num=atoi(number);
        int cNum=wave.waveRepresentation[z];
        //testing purposes
        //printf("Carrier wave amp: %d || Modulated Wave amp: %d \n",cNum,num);
        //check if byte index is above 7, if it is then we set it back to zero


        if(cNum==0){
            cNum=1;
        }
        if(num==(int)(cNum*.5)){
            //printf("0\n");
            *(binaryArray+z)=0;


        }
        if(num==(int)(cNum*1.5)){
        //printf("1\n");
        *(binaryArray+z)=1;

        }

        z++;



     }
     totalLines=z;
return binaryArray;
}
