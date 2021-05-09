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
	int data_tag = 15, result_tag = 30, terminate_tag = 45;
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
	if (number_of_proc > 1) {
		if (process_id == MASTER_ID) { // this process is master process
			int num_row = 0;
			int color = 0;
			
			int count = 0;
			int row = 0;

			//According to the manual, split the chunks into 40 chunks (each with 10)
			for (int dest_id = 1; dest_id < number_of_proc; dest_id++) {
				//printf("Process ID: %d , phase zero \n", process_id);
				MPI_Send(&row, 1, MPI_INT, dest_id, data_tag, MPI_COMM_WORLD);
				count++;
				row++;
			}

			int recv_row[disp_width + 2]; //400: slave, 401: row number
			unsigned char* buf = (unsigned char*)malloc(imagesize);

			do {
				MPI_Recv(&recv_row, disp_width + 2, MPI_INT, MPI_ANY_SOURCE, result_tag, MPI_COMM_WORLD, &status);
				count--;

				if (row < disp_height) {
					MPI_Send(&row, 1, MPI_INT, recv_row[disp_width + 0], data_tag, MPI_COMM_WORLD);
					row++;
					count++;
				}
				else {
					MPI_Send(&row, 1, MPI_INT, recv_row[disp_width + 0], terminate_tag, MPI_COMM_WORLD);
				}

				for (int i = 0; i < disp_width; i++) {
					buf[recv_row[401] * width_in_bytes + i * 3 + 0] = recv_row[i]; //blue
					buf[recv_row[401] * width_in_bytes + i * 3 + 1] = recv_row[i]; //red
					buf[recv_row[401] * width_in_bytes + i * 3 + 2] = recv_row[i]; //green
				}

				//display code
			} while (count > 0);

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
		else { // slave
			int row_start;
			complex c;
			int color;
			int send_row[disp_width + 2];
			//int buffer[3];

			MPI_Recv(&row_start, 1, MPI_INT, MASTER_ID, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

			while (status.MPI_TAG == data_tag) {
				c.imag = imag_min + ((float)row_start * scale_imag);
				for (int w = 0; w < disp_width; w++) {
					c.real = real_min + ((float)w * scale_real);
					send_row[w] = cal_pixel(c);
				}
				send_row[400] = process_id;
				send_row[401] = row_start;
				MPI_Send(&send_row, disp_width + 2, MPI_INT, MASTER_ID, result_tag, MPI_COMM_WORLD);
				MPI_Recv(&row_start, 1, MPI_INT, MASTER_ID, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
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
