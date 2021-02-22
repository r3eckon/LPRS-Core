#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <cairo.h>

#define DEVICECOUNT 100

#define TYPE_PRESSURE 0
#define TYPE_WATLEVEL 1
#define TYPE_BARGRAPH 2

#define DATA_HISTORY_COUNT 5

FILE *fp;//device file pointer
FILE *dfp;//data file pointer

char buffer[240];

char * devdesc[DEVICECOUNT];

int lcount=0;

int active_devices =0;

const char s[2] = ",";



struct Device{
	
	int id;
	char * name;
	int type;
} devices[DEVICECOUNT];


char * devicedata[DEVICECOUNT];

GtkWidget * devwidgets[DEVICECOUNT];
GtkWidget *window, *grid;


double graphdata[DEVICECOUNT][DATA_HISTORY_COUNT];
double graphlastid[DEVICECOUNT];

int gridsize;

pthread_t looper;

struct WUPData{

	int id;
	char label[256];
}*lupdata, wup[DEVICECOUNT];

struct DRUPData{

	int id;
	double xval;
	double yval;
	
}drup[DEVICECOUNT], *ldrup;

static gboolean updateWidget(gpointer userdata)
{
	lupdata = (struct WUPData*) userdata;
	gtk_label_set_markup(GTK_LABEL(devwidgets[lupdata->id]), g_markup_printf_escaped("<span font_desc=\"Sans 20\">%s</span>", lupdata->label));
	
	
	return G_SOURCE_REMOVE;
}

static gboolean updateDrawArea(gpointer userdata)
{
	
	for(int i=0; i < lcount; i++)
		gtk_widget_queue_draw(devwidgets[i]);
	
	return FALSE;
}

void initDeviceData()
{
	
	for(int i=0; i < DEVICECOUNT; i++)
	{
		devicedata[i] = malloc(256);
		graphlastid[i] =0;
		
		for(int j=0; j < DATA_HISTORY_COUNT; j++)
		{
			graphdata[i][j] = 0.0;
			
		}

	}
	
}

const int SHRRDERR_NONE = 0;
const int SHRRDERR_OPENFAILED = 1;
const int SHRRDERR_WRITELOCKED = 2;
const int SHRRDERR_LOCKFAILED = 3;
const int SHRRDERR_UNLOCKFAILED = 4;

char drbuff[256];
int lastReadSize;

int readDataFile(char * datafile)
{
	char c;
	
	struct flock lock;
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start =0;
	lock.l_len =0;
	lock.l_pid = getpid();
	
	int fd;
	
	if((fd = open(datafile, O_RDONLY)) < 0)
		return SHRRDERR_OPENFAILED;
		
	fcntl(fd, F_GETLK, &lock);
	if(lock.l_type != F_UNLCK)
		return SHRRDERR_WRITELOCKED;
		
	lock.l_type = F_RDLCK;
	if(fcntl(fd, F_SETLK, &lock) < 0)
		return SHRRDERR_LOCKFAILED;
		
	lastReadSize=0;
	int cdev = 0;//current device
	
	while (read(fd, &c, 1) > 0)
	{
		drbuff[lastReadSize] = c;
		lastReadSize++;
		
		if(c == '\n')//When end of line is reached this signals end of device data line
		{
			strcpy(devicedata[cdev], drbuff);//Copy buffer content to device data storage array
			//printf("Read Device %d Data :%s", cdev, drbuff);
			cdev++;//Increment current device index
			lastReadSize=0;//Reset read size for next device
			memset(drbuff, 0, 256);//Reset buffer for next device
			
			
		}
		
	}
	
	lock.l_type = F_UNLCK;
	if (fcntl(fd, F_SETLK, &lock) < 0)
		return SHRRDERR_UNLOCKFAILED;
	
	close(fd);
	
	return SHRRDERR_NONE;
		
}

int readDataFileWithHistory(char * datafile)
{
	char c;
	char * tok;
	
	struct flock lock;
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start =0;
	lock.l_len =0;
	lock.l_pid = getpid();
	
	int fd;
	
	if((fd = open(datafile, O_RDONLY)) < 0)
		return SHRRDERR_OPENFAILED;
		
	fcntl(fd, F_GETLK, &lock);
	if(lock.l_type != F_UNLCK)
		return SHRRDERR_WRITELOCKED;
		
	lock.l_type = F_RDLCK;
	if(fcntl(fd, F_SETLK, &lock) < 0)
		return SHRRDERR_LOCKFAILED;
		
	lastReadSize=0;
	int cdev = 0;//current device
	
	while (read(fd, &c, 1) > 0)
	{
		drbuff[lastReadSize] = c;
		lastReadSize++;
		
		if(c == '\n')//When end of line is reached this signals end of device data line
		{
			strcpy(devicedata[cdev], drbuff);//Copy buffer content to device data storage array
			
			tok = strtok(devicedata[cdev], ",");
			graphdata[cdev][0] = atoi(tok);
			
			for(int i=1; i < DATA_HISTORY_COUNT; i++)
			{
				tok = strtok(NULL, ",");
				graphdata[cdev][i] = atoi(tok);
			}
			
			//printf("Read Device %d Data :%s", cdev, drbuff);
			cdev++;//Increment current device index
			lastReadSize=0;//Reset read size for next device
			memset(drbuff, 0, 256);//Reset buffer for next device
			
			
		}
		
	}
	
	lock.l_type = F_UNLCK;
	if (fcntl(fd, F_SETLK, &lock) < 0)
		return SHRRDERR_UNLOCKFAILED;
	
	close(fd);
	
	return SHRRDERR_NONE;
		
}

double sintime = 0;

void * threadedLoop()
{

	lupdata = malloc(sizeof(struct WUPData));
	char buf[256];
	int cerr;
	while(TRUE)
	{
		
		sintime+=(6.2831/60.0);
		
		if((cerr = readDataFileWithHistory("datahistory.txt")) != 0)
		{
			printf("Error Reading Data File : %d\n", cerr);
			sleep(10);
			continue;
		}
		
		
		for(int i=0; i < active_devices; i++)
		{
			wup[i].id = i;
			sprintf(buf, "%s",devicedata[i]);
			strcpy(wup[i].label, buf);
			memset(buf, 0, 256);
			
			drup[i].yval=sin(sintime);
			drup[i].xval=cos(sintime);
		}
		
		
		g_main_context_invoke(NULL, updateDrawArea, NULL);
		
		sleep(1);
	}
	
}


int currentValue;

gboolean gauge_draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	double lx,ly;
	double markersize = 10.0;
	int markercount = 8;
	double normval;
	
	ldrup = (struct DRUPData*)data;
	
	currentValue = atoi(devicedata[ldrup->id]);
	
	guint width, height;
	
	width = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);
	
	cairo_set_source_rgb(cr, 0.7,0.7,0.7);
	
	ly = sin(0);
	lx = cos(0);
	
	cairo_move_to(cr, (double)(width/2), (double)(height/2));
	cairo_arc(cr, (double)(width/2), (double)(height/2), (double)(width/2), 0, 3.1416/4.0);

	
	cairo_arc(cr, (double)(width/2), (double)(height/2), (double)(width/2), (3.0*3.1416)/4.0,6.2831 );
	cairo_fill(cr);
	
	
	cairo_set_source_rgb(cr, 0.0,0.0,0.0);
	cairo_set_line_width(cr, 3.0);
	
	for(int i=0; i < markercount; i++)
	{
		
		if(i==2)
			continue;
		
		ly = sin((6.2831 / markercount)*i);
		lx = cos((6.2831 / markercount)*i);
		
		cairo_move_to(cr, (double)(width/2) + ((width/2 - 2) * lx), (double)(height/2) + ((height/2 - 2) * ly));
		cairo_line_to(cr, (double)(width/2) + ((width/2 - markersize) * lx), (double)(height/2) + ((height/2 - markersize) * ly));
		
	}
	
	cairo_stroke(cr);
	
	normval = 1.0 - (currentValue/4096.0);
	
	double grange = (3*3.1416)/2.0;
	double goffset = (3.0*3.1416)/4.0;
	double gval = (normval * grange) + goffset;
	
	double valx = cos(gval);
	double valy = sin(gval);
	
	cairo_set_line_width(cr, 4.0);
	
	cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
	
	cairo_move_to(cr, (double)(width/2), (double)(height/2));
	//cairo_line_to(cr, (double)(width/2) + ((width/2) * ldrup->xval), (double)(height/2) + ((height/2) * ldrup->yval));
	cairo_line_to(cr, (double)(width/2) + ((width/2) * valx), (double)(height/2) + ((height/2) * valy));
	
	cairo_stroke(cr);
	
	return FALSE;
}

gboolean level_draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	double normval;
	
	ldrup = (struct DRUPData*)data;
	
	currentValue = atoi(devicedata[ldrup->id]);
	
	guint width, height;
	
	width = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);

	normval = 1.0-(currentValue/4096.0);
	
	cairo_set_source_rgb(cr, 0.5,0.5,0.5);
	cairo_set_line_width(cr, 20.0);
	
	cairo_move_to(cr, (double)(width/2), (double)(height));
	cairo_line_to(cr, (double)(width/2), (double)(0));

	cairo_stroke(cr);
	
	cairo_set_source_rgb(cr, 0.1,0.1,1.0);
	
	cairo_set_line_width(cr, 20.0);
	
	cairo_move_to(cr, (double)(width/2), (double)(height));
	cairo_line_to(cr, (double)(width/2), (double)(height*normval));
	
	cairo_stroke(cr);

	return FALSE;
}

gboolean bargraph_draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	double normval;
	
	ldrup = (struct DRUPData*)data;
	
	currentValue = atoi(devicedata[ldrup->id]);
	
	guint width, height;
	
	width = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);

	
	
	
	cairo_set_line_width(cr, width / DATA_HISTORY_COUNT);
	
	for(int i=0; i < DATA_HISTORY_COUNT; i++)
	{
		cairo_set_source_rgb(cr, 0.1,0.1,1.0 - i * 0.2);
		
		currentValue = graphdata[ldrup->id][i];
		normval = 1.0-(currentValue/4096.0);
		
		cairo_move_to(cr, width - (i * (width / DATA_HISTORY_COUNT) + ((width / DATA_HISTORY_COUNT)/2)), height);
		cairo_line_to(cr, width - (i * (width / DATA_HISTORY_COUNT) + ((width / DATA_HISTORY_COUNT)/2)), height * normval);
		cairo_stroke(cr);
	}

	return FALSE;
}



GtkWidget * aspectframe[DEVICECOUNT];

int main(int argc, char **argv)
{
	char * token;
	char * tokbuf;
	
	initDeviceData();
	
	fp = fopen("/home/pi/WANPI/devices.txt", "r");
	
	if(fp == NULL)
	{
		printf("ERROR OPENING DEVICE FILE");
		return 1;
	}
	
	printf("Reading Devices File...");
	
	while(fgets(buffer, 240, fp) != NULL)
	{
		devdesc[lcount] = malloc(strlen(buffer)+1);
		strcpy(devdesc[lcount], buffer);
		lcount+=1;
	}
	
	if(lcount == 0)
	{
		printf("DEVICE FILE EMPTY");
		return 1;
	}
	
	printf("OK\nFound %d devices.\n\nID\tTYPE\tNAME\n--------------------------------\n", lcount);
	
	active_devices = lcount;
	
	for(int i=0; i < lcount; i++)
	{
		tokbuf = devdesc[i];
		
		token = strtok(tokbuf, s);//Reading ID
		
		if(token == NULL){
			break;
		}
		
		devices[i].id = atoi(token);
		
		token = strtok(NULL, s);//Reading Type
		
		if(token == NULL){
			break;
		}
		
		devices[i].type = atoi(token);
		
		token = strtok(NULL, s);//Reading name
		
		if(token == NULL){
			break;
		}
		
		devices[i].name = strtok(token, "\n");
		
		printf("%d\t%s\t%s\n", devices[i].id, (devices[i].type == 0 ? "PRESS" : "WATER"), devices[i].name);
		
		
	}
	
	printf("--------------------------------\nStarting GUI...\n");
	
	gtk_init(&argc, &argv);
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 20);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), FALSE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), FALSE);
    
    gtk_widget_set_margin_start(grid, 20);
    gtk_widget_set_margin_end(grid, 20);
    gtk_widget_set_margin_top(grid, 20);
    gtk_widget_set_margin_bottom(grid, 20);
    
    gridsize = (int)(ceil(sqrt((double)lcount)));
    
    char gtext[100];
    
    for(int i=0; i < lcount; i++)
    {
		/*
		devwidgets[i] = gtk_label_new(devices[i].name);
		
		gtk_grid_attach(GTK_GRID(grid), devwidgets[i], i % gridsize, (int)floor(i/gridsize), 1, 1);
		gtk_label_set_markup(GTK_LABEL(devwidgets[i]), g_markup_printf_escaped("<span font_desc=\"Sans 20\">%s</span>", devices[i].name));
		*/
		
		drup[i].id = i;
		
		sprintf(gtext, devices[i].name, (i+1));
		
		aspectframe[i] = gtk_aspect_frame_new(gtext, 0.5,0.5,1,FALSE);
		gtk_grid_attach(GTK_GRID(grid), aspectframe[i], i % gridsize, (int)floor(i/gridsize),1,1);
		devwidgets[i] = gtk_drawing_area_new();
		gtk_widget_set_size_request(devwidgets[i], 100, 100);
		
		//Draw correct graphic depending on sensor type
		if(devices[i].type == TYPE_PRESSURE)
		{
			g_signal_connect(G_OBJECT(devwidgets[i]),"draw", G_CALLBACK(gauge_draw_callback), &drup[i]);
		}
		else if(devices[i].type == TYPE_WATLEVEL)
		{
			g_signal_connect(G_OBJECT(devwidgets[i]),"draw", G_CALLBACK(level_draw_callback), &drup[i]);
		}
		if(devices[i].type == TYPE_BARGRAPH)
		{
			g_signal_connect(G_OBJECT(devwidgets[i]),"draw", G_CALLBACK(bargraph_draw_callback), &drup[i]);
		}
		
		gtk_container_add(GTK_CONTAINER(aspectframe[i]), devwidgets[i]);
		
		gtk_widget_set_hexpand(devwidgets[i], TRUE);
		gtk_widget_set_vexpand(devwidgets[i], TRUE);
		gtk_widget_set_hexpand(aspectframe[i], TRUE);
		gtk_widget_set_vexpand(aspectframe[i], TRUE);
		
	}
	
	/*
	aspectframe = gtk_aspect_frame_new("Gauge #1", 0.5,0.5,1,FALSE);
	gtk_grid_attach(GTK_GRID(grid), aspectframe, 2,2,1,1);
	
	
	devwidgets[lcount] = gtk_drawing_area_new();
	gtk_widget_set_size_request(devwidgets[lcount], 100, 100);
	g_signal_connect(G_OBJECT(devwidgets[lcount]),"draw", G_CALLBACK(draw_callback), &drup);
	
	gtk_container_add(GTK_CONTAINER(aspectframe), devwidgets[lcount]);
	//gtk_grid_attach (GTK_GRID(grid), devwidgets[lcount], 2,2, 1, 1);
	*/
	
	pthread_create(&looper, NULL, threadedLoop, NULL);
	
	gtk_widget_show_all(window);
	gtk_main();
	
	return 0;
}

