/***************************************************************************
                 wmSMPmon III - Window Maker system monitor
VERSION :           4.0
DATE    :           2022-02-21
ORIGINAL AUTHORS :  redseb <redseb@goupilfr.org> and
                    PhiR <phir@gcu-squad.org>
CONTRIBUTORS :      Alain Schröder <alain@parkautomat.net>
CURRENT MAINTAINER: Thomas Ribbrock <emgaron@gmx.net>
****************************************************************************
    This file is placed under the conditions of the GNU Library
    General Public License, version 2, or any later version.
    See file COPYING for information on distribution conditions.
***************************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include	<string.h>
#include	<X11/Xlib.h>
#include	<X11/xpm.h>
#include	<X11/extensions/shape.h>
#include	<signal.h>
#include	"wmgeneral.h"
#include	"wmSMPmon_master.xpm"
#include	"wmSMPmon_mask.xbm"
#include	"standards.h"
#ifdef HAVE_NVIDIA
#include	"nvml.h"
#endif

#ifndef VERSION
#define		VERSION		"4.0"
#endif

/*###### Dividers for redraw-loops ######################################*/
#define		DIV1		6
#define		DIV2		10

/*###### Messages #######################################################*/
#define		MSG_NO_SWAP	"No swap mode : Swap is not monitored.\n"

/*###### Funcition definitions ##########################################*/
void usage(int cpus, const char *str);

static void sig_handler(int);


#ifdef HAVE_NVIDIA
static	nvmlDevice_t nvml_device;

unsigned int Get_GPU() {
	nvmlUtilization_t utilization; 
	nvmlReturn_t res = nvmlDeviceGetUtilizationRates(nvml_device,  &utilization );
	if (res){
		printf("Unable to acquire GPU Utilization info: %s\n",nvmlErrorString(res));
		return (0);
	}
	return (utilization.gpu);
}

unsigned int Get_VRAM() {
	nvmlMemory_t memory;
	nvmlReturn_t res = nvmlDeviceGetMemoryInfo(nvml_device, &memory);
	if (res){
		printf("Unable to acquire GPU RAM info: %s\n",nvmlErrorString(res));
		return (0);
	}
	return (memory.used * 100 / memory.total);
}
#endif


static void sig_handler(int s){
	printf("Caught signal %d, terminating gracefully\n",s);
#ifdef HAVE_NVIDIA
	nvmlShutdown();
#endif
	if (fd_stat) {
		fclose(fd_stat);
		fd_stat = NULL;
	}
	exit(0); 
}

/*###### MAIN PROGRAM ###################################################*/
int main(int argc, char **argv)
{
	XEvent		Event;

	unsigned short offset = 0,
			c1 = DIV1,
			c2 = DIV2,
			etat = 1,
			lecture = 1,
			no_swap = 0,
			draw_mem = 0,
			draw_graph = 1,
			NumCPUs,      /* number of CPUs */
			i = 0,        /* counter */
			mem = 0, /* current memory/swap scaled to 0-100 */
			prec_mem = 0, /* memory from previous round */
			mem2 = 0, /* current memory incl caches*/
			prec_mem2 = 0, /* memory_cache from previous round */
			prec_swap = 0, /* swap from previous round */
			load_width = 3; /* width of load bar: 3 for SMP, 8 for UP */
	unsigned int	t0[WIDTH_T], /* history for CPU 0 -> Graph */
			t1[WIDTH_T], /* history for CPU 1 -> Graph */
			tm[WIDTH_T], /* history for CPU 0+1 -> Graph */
			tram[WIDTH_T], /* history for total RAM -> Graph */
			tcache[WIDTH_T], /* history for cache_mark -> Graph */
			tswap[WIDTH_T], /* history for swap/VRAM -> Graph */
#ifdef HAVE_NVIDIA
			tgpu[WIDTH_T], /* history for GPU -> Graph */
			use_gpu = 0,
			prec_gpu = 0, /* GPU utilization from previous round */
#endif
			delay = 250000,
			delta = 0,
			load = 0;

	unsigned long	load0t = 0, load1t = 0, loadst = 0;

	unsigned int	*CPU_Load; /* CPU load per CPU array */
	unsigned int	t_idx = 0; /* Index to load history tables */
	
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = sig_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);
	sigaction(SIGTERM, &sigIntHandler, NULL);
	sigaction(SIGQUIT, &sigIntHandler, NULL);

	/********** Initialisation **********/
	NumCPUs = NumCpus_DoInit();
	CPU_Load = calloc((NumCPUs),sizeof(int));

	if(NumCPUs == 1) {
		load_width = 8;
	} else if ( NumCPUs == 2) {
			load_width = 3;
	} else if ( NumCPUs == 4 || NumCPUs == 3) {
			load_width = 2;
	} else {
			load_width = 1;
	}


	/* process command line args */
	i = 1; /* skip program name (i=0) */
	while (argc > i) {
		if (!strncmp(argv[i], "-r", 2)) {
			i++;
			if (i == argc) {
				/* parameter missing! */
				usage(NumCPUs,
				    "no refresh rate given when using -r!");
			} else {
				delay = atol(argv[i]) ;
			}
			i++;
			continue;
		}
		if (!strncmp(argv[i], "-h", 2)) {
			usage(NumCPUs, NULL);
		}
		if (!strncmp(argv[i], "-no-swap", 8)) {
			puts(MSG_NO_SWAP);
			no_swap = 1;
			i++;
			continue;
		}
#ifdef HAVE_NVIDIA
		if (!strncmp(argv[i], "-nvidia", 7)) {
			puts(MSG_NO_SWAP);
			puts("Attempting to monitor GPU instead of swap\n");
			use_gpu = 1;
			i++;
			continue;
		}
#endif
		if (!strncmp(argv[i], "-g", 2) && NumCPUs > 1) {
			/* we only support this on SMP systems */
			i++;
			if (i == argc) {
				/* parameter missing! */
				usage(NumCPUs,
				    "no graph style given when using -g!");
			} else {
			    etat = atoi(argv[i]);
			}

			if (1 > etat || etat > 3)
				usage(NumCPUs, "Unknown graph style");
			i++;
			continue;
		}
		if (!strncmp(argv[i], "-draw-mem", 9)) {
			draw_mem = 1;
			i++;
			continue;
		}

		/* if we get here, we found an illegal option */
		usage(NumCPUs, "Illegal option!");
	}
	/* open initial window */
	if (NumCPUs != 2) {
		/* we only have a single CPU - change the mask accordingly
		 * NOTE: The for loop was derived from the differences between
		 * wmSMPmon_mask.xbm and wmSMPmon_mask-single.xbm.
		 * wmSMPmon_mask-single.xbm as such is NOT used in this
		 * program!
		 */
		for (i = 33; i <= 289; i = i+8) {
			wmSMPmon_mask_bits[i] = 0xDF;
		}
	}

	openXwindow(argc, argv, wmSMPmon_master_xpm, wmSMPmon_mask_bits,
	    wmSMPmon_mask_width, wmSMPmon_mask_height);
	
	if(NumCPUs == 2) {
		/* we have two CPUs -> draw separator between CPU load bars */
		copyXPMArea(12, 4, 2, HEIGHT + 2, 7, 4);
	}

	delay = delay / 2 ;

	for (i = 0; i < WIDTH_T; i ++) {
		t0[i] = 0;
		t1[i] = 0;
		tm[i] = 0;
		tram[i] = 0;
		tswap[i] = 0;
		tcache[i] = 0;
#ifdef HAVE_NVIDIA
		tgpu[i] = 0;
#endif
	}

#ifdef HAVE_NVIDIA
	if (use_gpu) {
		// Init NVML here and if it is Ok,disable swap monitoring
		nvmlReturn_t res = nvmlInit_v2();
		if (res){
			printf("Unable to init NVML: %s\n",nvmlErrorString(res));
			use_gpu = 0;
		}
		else {
			nvmlReturn_t res = nvmlDeviceGetHandleByIndex_v2 (0, &nvml_device);
 			if (res){
				printf("Unable to find GPU device: %s.\nDisabling GPU data collection.",nvmlErrorString(res));
				use_gpu = 0;
			}
			else {
				no_swap = 1;
			}
		}
	}
#endif


	/* -no-swap option was given */
	if (no_swap) {
#ifndef HAVE_NVIDIA
		// draw "NONE"
	    copyXPMArea(83, 63, 83, 8, 29, 50);
#else
			if (use_gpu) {
				copyXPMArea(75, 71, 18, 8, 6, 50);
			}
			else {
	    	copyXPMArea(83, 63, 83, 8, 29, 50);
			}
#endif
}

	/* MAIN LOOP */
	while (1) {
		if (lecture) {
			CPU_Load = Get_CPU_Load(CPU_Load, NumCPUs);

			load = 0;
			offset = 0;
			for (i = 0; i < NumCPUs; i++) {
				load += CPU_Load[i];
				delta = HEIGHT - CPU_Load[i];
				if (NumCPUs == 2 && i == 1) {
					offset=2;
				}
				copyXPMArea(108, 0, load_width, HEIGHT, 4 + i * load_width + offset, 5);
				copyXPMArea(108, 32 + delta, load_width, CPU_Load[i],
				    4 + i * load_width + offset, 5 + delta);
			}
			load = load / NumCPUs;


			/* we have to set load1t in any case to get the correct
			 * graph below. With only one CPU, 'load' will still be
			 * CPU_Load[0], on a SMP system, it will be CPU_Load[1].
			 */
			load0t += CPU_Load[0];
			if (NumCPUs == 2) {
				load1t += CPU_Load[1];
			}
			loadst += load;

			if (c1 > DIV1) {
				mem = Get_Memory();
				mem2 = Get_Memory2();

				if (mem != prec_mem || mem2 != prec_mem2) {
					/* redraw only if mem changed */
					copyXPMArea(30, 63, 30, 8, 29, 39);
					copyXPMArea(1, 63, (mem2 * 30 / 100), 8, 29, 39);
					copyXPMArea(115, 63, 1, 8, (mem * 30 / 100 + 29), 39);
					prec_mem = mem;
					prec_mem2 = mem2;
				}
				if (!no_swap) {
					mem = Get_Swap();

					if (mem != prec_swap) {
						/* redraw if there was a change */
						if (mem == 999) {
							/* swap is disabled => show "none" */
	    				copyXPMArea(83, 63, 83, 8, 29, 50);
							copyXPMArea(1, 71, 18, 8, 6, 50);
							mem=0;
						} else {
							/* draw swap usage */
							copyXPMArea(30, 63, 30, 8, 29, 50);
							copyXPMArea(0, 63, (mem * 30 / 100), 8, 29, 50);
							if (mem > 50) {
								copyXPMArea(22, 71, 18, 8, 6, 50);
							} else { 
									if (mem > 10) {
									copyXPMArea(43, 71, 18, 8, 6, 50);
									} else {
										if (mem > 0) {
											copyXPMArea(60, 63, 18, 8, 6, 50);
									} else {
											copyXPMArea(1, 71, 18, 8, 6, 50);
									}
								}
							}
						}
						prec_swap = mem;
					}
				}
				c1 = 0;

#ifdef HAVE_NVIDIA
				if (use_gpu) {
					mem = Get_VRAM();
					mem2 = Get_GPU();
					copyXPMArea(75, 71, 18, 8, 6, 50);
					copyXPMArea(30, 63, 30, 8, 29, 50);
					copyXPMArea(0, 63, (mem * 30 / 100), 8, 29, 50);
					copyXPMArea(113, 63, 1, 8, (mem2 * 30 / 100 + 29), 50);
					prec_swap = mem;
					prec_gpu = mem2;
				}
#endif
			}

			if (c2 > DIV2) {
				if (draw_mem) {
					tram[t_idx] = prec_mem * HEIGHT / 100;
					tcache[t_idx] = prec_mem2 * HEIGHT / 100;
#ifdef HAVE_NVIDIA
					tgpu[t_idx] = prec_gpu * HEIGHT / 100;
#endif
					if (!no_swap) tswap[t_idx] = prec_swap * HEIGHT / 100;
					else { 
#ifdef HAVE_NVIDIA
						if (use_gpu) {
							tswap[t_idx] = prec_swap  * HEIGHT / 100;
						} else
#endif
						tswap[t_idx] = 0;
					}
				}
				if ((t0[t_idx] = load0t / c2) > HEIGHT)
					t0[t_idx] = HEIGHT;
				t0[t_idx] /= 2;
				if ((t1[t_idx] = load1t / c2) > HEIGHT)
					t1[t_idx] = HEIGHT;
				t1[t_idx] /= 2;
				if (NumCPUs == 2) {
					if ((tm[t_idx] = (load0t + load1t) / (2 * c2)) > HEIGHT)
						tm[t_idx] = HEIGHT;
				} else {
						if ((tm[t_idx] = loadst / c2) > HEIGHT)
							tm[t_idx] = HEIGHT;
				}
				load0t = 0;
				load1t = 0;
				loadst = 0;
				t_idx = (t_idx + 1) % WIDTH_T;
				draw_graph = 1;
				c2 = 0;
			}

				if (draw_graph) {
				/* draw graph */
				switch (etat) {
				case 1 :
					copyXPMArea(64, 32, WIDTH_T, HEIGHT, 15, 5);
					for (i = 0, load = t_idx; i < WIDTH_T; i++, load++) {
						copyXPMArea(116, 0, 1, tm[load % WIDTH_T], 15 + i, HEIGHT + 5 - tm[load % WIDTH_T]);
						if (draw_mem) {
							if (tram[load % WIDTH_T] != 0) copyXPMArea(68, 73, 1, 1, 15 + i, HEIGHT + 5 - tram[load % WIDTH_T]);
							if (tcache[load % WIDTH_T] != 0) copyXPMArea(68, 72, 1, 1, 15 + i, HEIGHT + 5 - tcache[load % WIDTH_T]);
							if (tswap[load % WIDTH_T] != 0 ) copyXPMArea(68, 71, 1, 1, 15 + i, HEIGHT + 5 - tswap[load % WIDTH_T]);
#ifdef HAVE_NVIDIA
							if (tgpu[load % WIDTH_T] != 0 ) copyXPMArea(68, 74, 1, 1, 15 + i, HEIGHT + 5 - tgpu[load % WIDTH_T]);
#endif
						}
					}
					break;
				case 2 :
					copyXPMArea(64, 0, WIDTH_T, HEIGHT, 15, 5);
					for (i = 0, load = t_idx; i < WIDTH_T; i ++, load++) {
						copyXPMArea(116, 0, 1, t0[load % WIDTH_T], 15 + i, HEIGHT/2 + 5 - t0[load % WIDTH_T]);
						copyXPMArea(116, 0, 1, t1[load % WIDTH_T], 15 + i, HEIGHT/2 + 21 - t1[load % WIDTH_T]);
					}
					break;
				case 3 :
					copyXPMArea(64, 0, WIDTH_T, HEIGHT, 15, 5);
					for (i = 0, load = t_idx; i < WIDTH_T; i ++, load++) {
						copyXPMArea(116, 0, 1, t0[load % WIDTH_T], 15 + i, HEIGHT/2 + 5 - t0[load % WIDTH_T]);
						copyXPMArea(117, HEIGHT/2 - t1[load % WIDTH_T], 1, t1[load % WIDTH_T], 15 + i, HEIGHT/2 + 6);
					}
					break;
				}
				draw_graph = 0;
			}
			c1++;
			c2++;
		}
		lecture = 1 - lecture ;
		RedrawWindow();
		if (NumCPUs == 2 &&
		    XCheckMaskEvent(display, ButtonPressMask, &Event)) {
			/* changing graph style not supported on single CPU systems */
			if (Event.type == ButtonPress) {
				if ((etat++) >= 3)
					etat = 1;
				draw_graph = 1;
			}
		}
		usleep(delay);
	}
}

/*###### Usage Message ##################################################*/
void usage(int cpus, const char *str)
{
	fflush(stdout);

	if (str) {
		fprintf(stderr, "\nERROR: %s\n", str);
	}

	fputs("\nwmSMPmon "VERSION" - display system load (", stderr);
	if(cpus == 1) {
		fputs("uniprocessor system)\n\n", stderr);
	} else {
		fputs("multiprocessor system)\n\n", stderr);
	}
	fputs("Options : -h        this help screen.\n"
	    "          -r RATE   refresh rate (in microseconds, default 250000).\n",
	    stderr);

	if(cpus > 1) {
		fputs("          -g STYLE  graph style (try 2 or 3, default is 1).\n",
		    stderr);
	}

	fputs("          -no-swap  don't monitor swap size.\n", stderr);
#ifdef HAVE_NVIDIA
	fputs("          -nvidia  monitor NVidia GPU instead of swap.\n", stderr);
	fputs("          -draw-mem draw memory usage graph (red - swap or Video RAM (if nvidia enabled), yellow - non-cached, blue - allocated, white - GPU usage)\n\n", stderr);
#else
	fputs("          -draw-mem draw memory usage graph (red - swap, yellow - non-cached, blue - allocated)\n\n", stderr);
#endif
	fputs("<redseb@goupilfr.org> http://goupilfr.org\n"
	    "<phir@gcu-squad.org> http://gcu-squad.org\n"
	    "<emgaron@gmx.net> http://www.ribbrock.org\n",
	    stderr);

	exit(0);
}
