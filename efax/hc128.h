#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ROUNDS 2048


unsigned int P[512], Q[512], K[8], IV[8], W[1280];

#define sl3(x, n) (((x) << n) ^ ((x) >> (32-n)))
#define sr3(x, n) (((x) >> n) ^ ((x) << (32-n)))
#define f1(x) (sr3((x), 7)) ^ (sr3((x), 18)) ^ ((x) >> 3)
#define f2(x) (sr3((x), 17)) ^ (sr3((x), 19)) ^ ((x) >> 10)
#define g1(x, y, z) (((sr3((x), 10)) ^ (sr3((z), 23))) + (sr3((y), 8)))
#define g2(x, y, z) (((sl3((x), 10)) ^ (sl3((z), 23))) + (sl3((y), 8)))

unsigned int h1(unsigned int x){
     unsigned int y;
     unsigned char a, c;   		
     a = (unsigned char) (x);        
     c = (unsigned char) ((x) >> 16); 
     y = (Q[a])+(Q[256+c]);
     return y;
}
unsigned int h2(unsigned int x){
     unsigned int y;
     unsigned char a, c;   		
     a = (unsigned char) (x);        
     c = (unsigned char) ((x) >> 16); 
     y = (P[a])+(P[256+c]);
     return y;
}


/* Mod Minus */
int mm(int i, int j){
	int temp;

	if (i >= 512)
		i = i%512;
	if (j >= 512)
		j = j%512;

	if (i >= j)
		temp = i-j;
	else
		temp = 512 + i-j;

	return temp;
}

void ksa(){
	int i;

	for (i = 0; i < 8; i++)
		W[i] = K[i];
	for (i = 0; i < 8; i++)
		W[i+8] = IV[i];
	for (i = 16; i < 1280; i++)
		W[i] = (f2(W[i-2])) + W[i-7] + (f1(W[i-15])) + W[i-16] + i;

	for (i = 0; i < 512; i++){
		P[i] = W[i+256];
		Q[i] = W[i+768];
	}

	for (i = 0; i < 512; i++)
P[i] = (P[i] + g1(P[mm(i,3)], P[mm(i, 10)], P[mm(i, 511)]))^(h1(P[mm(i, 12)]));

	for (i = 0; i < 512; i++)
Q[i] = (Q[i] + g2(Q[mm(i,3)], Q[mm(i, 10)], Q[mm(i, 511)]))^(h2(Q[mm(i, 12)]));
}




void encrypt(char Key[16],int nr,unsigned int s[nr]){

	long int i, j;
	
	//unsigned int s[nr];


	for (i = 0; i < 8; i++){
	K[i]=(unsigned int)Key[i];
	IV[i]=(unsigned int)Key[i+8];		}  
	/*for (i = 0; i < 4; i++){
		K[i+4] = K[i];
		IV[i+4] = IV[i];
	}*/

	ksa();

i = 0;
while(1){

	if (i >= nr) break;
	j = i%512;
	if (i%1024 < 512){
		P[j] = (P[j] + g1(P[mm(j,3)], P[mm(j, 10)], P[mm(j, 511)]));
		s[i] = (h1(P[mm(j, 12)]))^P[j];
		
		
		}
	else{
		Q[j] = (Q[j] + g2(Q[mm(j,3)], Q[mm(j, 10)], Q[mm(j, 511)]));
		s[i] = (h2(Q[mm(j, 12)]))^Q[j];
	
		
		}
	//if (i <32) printf("%x ",s);
	//if ((i%4==3) && (i<32)) printf("\n");
	i++;
}


}
