/**
 * @file mandel.c
 * @author Jacob Purcell (purcellj@msoe.edu)
 * @brief 
 * @version 0.1
 * @date 2025-11-11
 * 
 * 		Based on example code found here:
 * 		https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
 * 
 * 		Converted to use jpg instead of BMP and other minor changes
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "jpegrw.h"

#include <sys/mman.h>
#include <wait.h>
#include <sys/stat.h>
#include <pthread.h>
#define NUM_FRAMES 50
#define MAX_PROC 50
#define MAX_THREAD 20
#define STRING_LENGTH 16


// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void compute_image( imgRawImage *img, double xmin, double xmax,
									double ymin, double ymax, int max, int num_thread);
static void show_help();

typedef struct threadArgs_t {
	int this_thread, num_thread;
	imgRawImage* img; 
	double xmin, xmax, ymin, ymax; 
	int max;
} threadArgs_t;


int main( int argc, char *argv[] )
{
	char c;

	int active_proc = 0;

	// These are the default configuration values used
	// if no command line arguments are given.
	const char *outfile = "mandel.jpg";
	double xcenter = 0;
	double ycenter = 0;
	double xscale = 4;
	double yscale = 0; // calc later
	int    image_width = 1000;
	int    image_height = 1000;
	int    max = 1000;
	int	num_proc = 1;
	int num_thread = 1;



	// For each command line argument given,
	// override the appropriate configuration value.

	while((c = getopt(argc,argv,"x:y:s:W:H:m:o:p:t:h"))!=-1) {
		switch(c) 
		{
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				xscale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'p':
				num_proc = atoi(optarg);
				if(num_proc > MAX_PROC){ 
					//If the user input exceeds maximum processes, use max instead
					num_proc = MAX_PROC;
				}
				break;
			case 't':
				num_thread = atoi(optarg);
				if(num_thread > MAX_THREAD){ 
					//If the user input exceeds maximum processes, use max instead
					num_thread = MAX_THREAD;
				}
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}

	for (int k = 0; k < NUM_FRAMES;k++)
        {
            if (active_proc >= num_proc)
            {
                wait(NULL); //if exceeding the number of active processes, wait
                active_proc--; 
            }
            int pid = fork();
            if (pid == 0) 
            {
				char frame[32];
				sprintf(frame, "mandel%d.jpg", k);
				outfile = frame;

				xscale -= (k*0.08);
				ycenter -= (k*0.02);
                // Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
				yscale = (xscale) / image_width * image_height;

				// Display the configuration of the image.
				printf("mandel: x=%lf y=%lf xscale=%lf yscale=%1f max=%d outfile=%s\n",xcenter,ycenter,xscale,yscale,max,outfile);

				// Create a raw image of the appropriate size.
				imgRawImage* img = initRawImage(image_width,image_height);

				// Fill it with a black
				setImageCOLOR(img,0);

				// Compute the Mandelbrot image
				compute_image(img,xcenter-xscale/2,xcenter+xscale/2,ycenter-yscale/2,ycenter+yscale/2,max,num_thread);

				// Save the image in the stated file.
				storeJpegImageFile(img,outfile);

				// free the mallocs
				freeRawImage(img);
                exit(0);
            }else if (pid > 0)
            {
                active_proc++;
            }
            
        }
        //wait for all of the remaining child processes to finish
        while(active_proc > 0)
        {
            wait(NULL);
            active_proc--;
        }
	return 0;
}

void *thread_func(void *arg){
	threadArgs_t* args = (threadArgs_t*) arg;
	int this_thread = args->this_thread;
	int num_thread = args->num_thread;
	int width = args->img->width;
	int height = args->img->height;
	int xmin = args->xmin;
	int xmax = args->xmax;
	int ymin = args->ymin;
	int ymax = args->ymax;
	int max = args->max;
	for(int j = this_thread*(height/num_thread); j < (this_thread+1)*(height/num_thread); j++) {
		for(int i=0;i<width;i++) {
			// Determine the point in x,y space for that pixel.
			double x = xmin + i*(xmax-xmin)/width;
			double y = ymin + j*(ymax-ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(args->img,i,j,iteration_to_color(iters,max));
		}
	}
	pthread_exit(NULL);
}



/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max, int num_thread)
{
	// For every pixel in the image...
	pthread_t threads[num_thread];
		for(int i = 0; i < num_thread; i++){
			threadArgs_t* threadArgs = malloc(sizeof(struct threadArgs_t));
			threadArgs->this_thread = i;
			threadArgs->num_thread = num_thread;
			threadArgs->img = img;
			threadArgs->xmin = xmin;
			threadArgs->xmax = xmax;
			threadArgs->ymin = ymin;
			threadArgs->ymax = ymax;
			threadArgs->max = max;
			if(pthread_create(&threads[i], NULL, thread_func, &threadArgs) != 0){
				perror("pthread_create");
				exit(1);
			}
		}

	//Join all threads
	for(int i = 0; i < num_thread; i++){
		pthread_join(threads[i], NULL);
	}

}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-p <processes> Set number of processes used (MAX: 50)\n");
	printf("-t <threads> Set number of threads used per process (MAX: 20)");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}
