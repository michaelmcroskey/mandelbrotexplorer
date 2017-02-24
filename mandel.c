
#include "bitmap.h"

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

typedef struct __myarg_t {
	struct bitmap *bm;
	double xmin;
	double xmax;
	double ymin;
	double ymax;
	int max;
	int h_start;
	int h_end;
} myarg_t;

int iteration_to_color( int i, int max );
int iterations_at_point( double x, double y, int max );
void *compute_image( void *args );

void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates. (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=500)\n");
	printf("-H <pixels> Height of the image in pixels. (default=500)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-n <threads>Number of threads.\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}

int main( int argc, char *argv[] )
{
	char c;

	// These are the default configuration values used
	// if no command line arguments are given.

	const char *outfile = "mandel.bmp";
	double xcenter = 0;
	double ycenter = 0;
	double scale = 4;
	int    image_width = 500;
	int    image_height = 500;
	int    max = 1000;
	int	   threads = 1;

	// For each command line argument given,
	// override the appropriate configuration value.

	while((c = getopt(argc,argv,"x:y:s:W:H:m:o:n:h"))!=-1) {
		switch(c) {
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				scale = atof(optarg);
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
			case 'n':
				threads = atoi(optarg);
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}

	// Display the configuration of the image.
	printf("mandel: x=%lf y=%lf scale=%lf max=%d outfile=%s threads=%d\n",xcenter,ycenter,scale,max,outfile,threads);

	// Create a bitmap of the appropriate size.
	struct bitmap *bm = bitmap_create(image_width,image_height);

	// Fill it with a dark blue, for debugging
	bitmap_reset(bm,MAKE_RGBA(255,255,255,0));
	
	// Create threads -- dynamic memory allocation
	pthread_t* thread = malloc(threads*sizeof(pthread_t));
	myarg_t* args = malloc(threads*sizeof(myarg_t));
//	pthread_t thread[threads];
//	myarg_t args[threads];
	
	// Initialize arguments for each args struct
	for (int i=0; i<threads; i++){
		args[i].bm = bm;
		args[i].xmin = xcenter-scale;
		args[i].xmax = xcenter+scale;
		args[i].ymin = ycenter-scale;
		args[i].ymax = ycenter+scale;
		args[i].max = max;
		args[i].h_start = 0;
		args[i].h_end = image_height;
	}
	
	// Compute intervals (eg: thread 2/5 computes lines 100-200)
	int part = image_height/threads;
	int loop = 0;
	for (int i=0; i<image_height; i += part){
		loop++;
		// If last one, account for int division remainder
		if (loop >= threads){
			args[loop-1].h_start = i;
			args[loop-1].h_end = image_height;
		// Otherwise, compute proper interval
		} else {
			args[loop-1].h_start = i;
			args[loop-1].h_end = i+part;
		}
	}
		
	// Compute the Mandelbrot image
	// Create threads
	for (int i=0; i<threads; i++){
		if (pthread_create(&(thread[i]), NULL, compute_image, (void *) (args+i))) {
			fprintf(stderr, "Error creating thread %d\n", i);
			return 1;
		}
	}
	
	// Join Threads
	for (int i=0; i<threads; i++){
		if (pthread_join(thread[i], NULL)){
			fprintf(stderr, "Error joining thread %d\n", i);
			return 1;
		}
	}
	
	// Save the image in the stated file.
	if(!bitmap_save(bm,outfile)) {
		fprintf(stderr,"mandel: couldn't write to %s: %s\n",outfile,strerror(errno));
		return 1;
	}
	
	// Cleanup
	pthread_exit(NULL);
	free(thread);
	free(args);
	return 0;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void *compute_image( void *args ){
	
	myarg_t *m  = (myarg_t*) args;
	
	int i,j;

	int width = bitmap_width(m->bm);
	int height = bitmap_height(m->bm);

	// For every pixel in the image...

	for(j = m->h_start; j < m->h_end; j++) {

		for(i=0; i<width; i++) {

			// Determine the point in x,y space for that pixel.
			double x = m->xmin + i*(m->xmax - m->xmin)/width;
			double y = m->ymin + j*(m->ymax - m->ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,m->max);

			// Set the pixel in the bitmap.
			bitmap_set(m->bm,i,j,iters);
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

	return iteration_to_color(iter,max);
}

/*
Convert a iteration number to an RGBA color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/

int iteration_to_color( int i, int max )
{
	//int gray = 255*i/max;
	int red = 41*i/max;
	int green = 175*i/max;
	int blue = 242*i/max;
	return MAKE_RGBA(red, green, blue, 0);
}
