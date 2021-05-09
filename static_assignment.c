#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mpi.h"


//Single Task Assignment

#define MAX_COUNT 256

#define disp_width 400
#define disp_height 400

#define real_max 2.00
#define real_min -2.00
#define imag_max 2.00
#define imag_min -2.00

#define MASTER_ID 0

// BITMAP
#pragma pack(push, 1)
typedef struct _BITMAPFILEHEADER {
	short int bfType;
	int bfsize;
	short int bfReseved1, bfReserved2;
	int bfOffBits;
} BITMAPFILEHEADER;

typedef struct _BITMAPINFOHEADER {
	int biSize;
	int biWidth;
	int biHeight;
	short int biPlanes;
	short int biBitCount;
	int biCompression;
	int biSizeImage;
	int biXpelsPermeter;
	int biYpelsPermeter;
	int biClrUsed;
	int biClrImportant;
} BITMAPINFOHEADER;

typedef struct _RGBTRIPLE {
	unsigned char rgbtBlue;
	unsigned char rgbtGreen;
	unsigned char rgbtRed;
} RGBTRIPLE;

#pragma pack(pop)

struct complex {
	float real;
	float imag;
};

int cal_pixel(complex c) {
	int count = 0;
	float lengthsq, temp;
	
	complex z;
	z.real = 0.0f; z.imag = 0.0f;

	do {
		temp = (float)(z.real * z.real) - (float)(z.imag * z.imag) + (float)c.real;
		z.imag = 2.0 * (float)(z.real * z.imag) + (float)c.imag;
		z.real = temp;
		lengthsq = z.real * z.real + z.imag * z.imag;
		count++;
	} while ((lengthsq < 4.0) && (count < MAX_COUNT));

	return count;
}

int main(int argc, char* argv[]) {

	// BITMAP
	FILE* fpBmp;
	BITMAPFILEHEADER fileHeader = { 0 };
	BITMAPINFOHEADER infoHeader = { 0 };

	int bitcount = 24;
	int width_in_bytes = ((disp_width * bitcount + 31) / 32) * 4;
	uint32_t imagesize = width_in_bytes * disp_height;

	memcpy(&fileHeader, "BM", 2);
	fileHeader.bfsize = 54 + imagesize;
	fileHeader.bfOffBits = 54;

	infoHeader.biSize = 40;
	infoHeader.biPlanes = 1;
	infoHeader.biWidth = disp_width;
	infoHeader.biHeight = disp_height;
	infoHeader.biBitCount = bitcount;
	infoHeader.biSizeImage = imagesize;
	
	const int K = 1024;
	const int msgsize = 256 * K;
	int number_of_proc, process_id, i;
	//int* X, * Y;
	int tag = 0;
	MPI_Status status;

	//---Initialization of MPI---
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &number_of_proc);
	MPI_Comm_rank(MPI_COMM_WORLD, &process_id);
	//---------------------------
	clock_t start, end;
	//---Shared constants between master and slave---
	int task_width_size = 400;
	if (number_of_proc > 1) {
		task_width_size = 400 / (number_of_proc - 1); // task_height_size = 400 (always constant)
	}

	float scale_real, scale_imag;
	scale_real = (real_max - real_min) / disp_width;
	scale_imag = (imag_max - imag_min) / disp_height;

	start = clock();

	//-----------------------------------------------
	//printf("Process ID: %d , phase zero00 \n", process_id);
	if (number_of_proc > 1) {
		if (process_id == MASTER_ID) { // this process is master process
			int num_row = 0;
			int color = 0;
			int curr_recv = 2;

			//printf("number of process: %d \n", number_of_proc);

			
			// testing...
			for (int dest_id = 1; dest_id < number_of_proc; dest_id++) {
				//printf("Process ID: %d , phase zero \n", process_id);
				MPI_Send(&num_row, 1, MPI_INT, dest_id, tag, MPI_COMM_WORLD);
				num_row += task_width_size;
			}

			int recv_buffer[3];

			unsigned char* buf = (unsigned char*)malloc(imagesize);

			for (i = 0; i < disp_height * disp_width; i++) {
				MPI_Recv(&recv_buffer, 3, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
				buf[recv_buffer[1] * width_in_bytes + recv_buffer[2] * 3 + 0] = recv_buffer[0]; //blue
				buf[recv_buffer[1] * width_in_bytes + recv_buffer[2] * 3 + 1] = recv_buffer[0]; //red
				buf[recv_buffer[1] * width_in_bytes + recv_buffer[2] * 3 + 2] = recv_buffer[0]; //green
			}

			fpBmp = fopen("400x400.bmp", "wb");
			fwrite(&fileHeader, sizeof(fileHeader), 1, fpBmp);
			fwrite(&infoHeader, sizeof(infoHeader), 1, fpBmp);
			fwrite((char*)buf, 1, imagesize, fpBmp);

			fclose(fpBmp);
			free(buf);

			end = clock();
			printf("Time elapsed: %f \n", (float)(end - start) / CLOCKS_PER_SEC);

			printf("Execution Done! \n");
			system("mspaint ./400x400.bmp");

			printf("Execution Done! \n");
		}
		else {
			int row_start;
			complex c;
			int color;
			int buffer[3];

			MPI_Recv(&row_start, 1, MPI_INT, MASTER_ID, tag, MPI_COMM_WORLD, &status);
			printf("row_start: %d \n", row_start);	

			for (int h = 0; h < disp_height; h++) {
				for (int w = row_start; w < (row_start + task_width_size); w++) {
					c.real = real_min + ((float)w * scale_real);
					c.imag = imag_min + ((float)h * scale_imag);
					buffer[0] = cal_pixel(c);
					buffer[1] = h;
					buffer[2] = w;
					MPI_Send(&buffer, 3, MPI_INT, MASTER_ID, tag, MPI_COMM_WORLD);
				}
			}
		}
		
	}
	else { //when there is only one process activated
		//int row_start;
		
		complex c;
		int color;

		unsigned char* buf = (unsigned char*)malloc(imagesize);
		for (int row = disp_height - 1; row >= 0; row--) {
			for (int col = 0; col < disp_width; col++) {
				c.real = (float)real_min + ((float)col * (float)scale_real);
				c.imag = (float)imag_min + ((float)row * (float)scale_imag);
				color = cal_pixel(c);
				buf[row * width_in_bytes + col * 3 + 0] = color; //blue
				buf[row * width_in_bytes + col * 3 + 1] = color; //red
				buf[row * width_in_bytes + col * 3 + 2] = color; //green
			}
		}

		fpBmp = fopen("400x400.bmp", "wb");
		fwrite(&fileHeader, sizeof(fileHeader), 1, fpBmp);
		fwrite(&infoHeader, sizeof(infoHeader), 1, fpBmp);
		fwrite((char*)buf, 1, imagesize, fpBmp);

		fclose(fpBmp);
		free(buf);

		end = clock();
		printf("Time elapsed: %f \n", (float)(end - start) / CLOCKS_PER_SEC);

		printf("Execution Done! \n");
		system("mspaint ./400x400.bmp");

		
	}
	
	MPI_Finalize();
	exit(0);
}
