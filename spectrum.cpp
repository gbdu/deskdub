#include <gtk/gtk.h>
#include <string.h>
#include <math.h>
#include "bass.h"

#pragma pack(1)
typedef struct {
	BYTE rgbRed,rgbGreen,rgbBlue;
} RGB;
#pragma pack()

#define MAX_BANDS 200
#define CONSOLE_HEIGHT 150

const int spec_width = 400;	// display width (should be multiple of 4
const int spec_height = 200;	// height (changing requires palette adjustments too)

static int total_bands = 129 ;
static GdkFont * font = NULL ;

static float zoom = 2 ;

GtkWidget *win = 0;

DWORD chan;

GtkWidget *speci;
GdkPixbuf *specpb;


RGB palette[256];
RGB red_palette[256];


// Recording callback - not doing anything with the data
BOOL CALLBACK DuffRecording(HRECORD handle, const void *buffer, DWORD length, void *user)
{
	return TRUE; // continue recording
}

static gboolean spec_motion_notify(GtkWidget * area,
		GdkEventMotion * event,
		gpointer data)
{
	//printf("%d %d\n", event->x, event->y);
	
}
// display error messages
void Error(const char *es)
{
	GtkWidget *dialog=gtk_message_dialog_new(GTK_WINDOW(win),GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,"%s\n(error code: %d)",es,BASS_ErrorGetCode());
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void WindowDestroy(GtkObject *obj, gpointer data)
{
	gtk_main_quit();
}

void ButtClickEvent(GtkWidget *obj, gpointer data )
{
}

void console_print(char * newt){
}

void ScrollEvent(GtkWidget *obj, GdkEventScroll  *event, gpointer data)
{
	if (event->type==GDK_SCROLL) {
		RGB *specbuf=(RGB*)gdk_pixbuf_get_pixels(specpb);
		if(event->direction == GDK_SCROLL_DOWN){
			zoom += 0.2;
		}
		if(event->direction == GDK_SCROLL_UP && zoom >= 1.0 ){
			//if(event->state | 1){
			//	total_bands --;
			//	printf("C");
			//}
			zoom -= 0.2;
		}
		memset(specbuf,0, spec_width*spec_height*sizeof(*specbuf)); // clear display
	}
}

int scale_band(float peak, int * overflow){ // returns y value
	int y = (sqrt(peak) * 4 * spec_height - 4)/zoom; // sqrt scale
	if (y>=spec_height-10){
		zoom += 0.2;
		*overflow = 1;
	//	printf("Peak at %d hz \n", peak_hz);
	}
	y = ((sqrt(peak) * 4 * spec_height - 4) / zoom); // sqrt scale
	return y;
}

void draw_band(RGB * specbuf,
		float * fft,
		float peak,
		int is_overflowing,
		int curr_band)
{

	const int band_width = spec_width / total_bands ;
	int is_overflow = 0;
	int y = scale_band(peak, &is_overflowing); // a scaled value of peak (which is float)
	int x_seek; // x_seek

	while (--y >= 0){ // each row in the pixbuf until the peak value
		for (x_seek=0; x_seek < band_width-1; x_seek++){ // each column
			int p = (spec_height-y-1) * spec_width + curr_band * (band_width)+x_seek ;
			
			if(p < 0){
				p = 0;  
			}

			if(is_overflowing){
				specbuf[p] = red_palette[y+1] ;
			}
			else{
				specbuf[p] = palette[y+1]; 
			}

		}
	}
	
}

void draw_text(GtkWidget * drawable, gpointer data){
///	gdk_draw_string(GDK_DRAWABLE(GDK_IMAGE(drawable)), font, win->style->black_gc, 20, 20, "string");
}

gboolean UpdateSpectrum(gpointer data)
{
	float fft[4096]; // FFT data
	
	RGB *specbuf = (RGB*) gdk_pixbuf_get_pixels(specpb); // rgb buffer

	BASS_ChannelGetData(chan, fft, BASS_DATA_FFT8192 | BASS_DATA_FFT_REMOVEDC); // get the FFT data

	int b0 = 0;
	int b1 = 0; // maybe max number of bands? (last band) i dont think so

	memset(specbuf, 0, spec_width * spec_height * sizeof(*specbuf));

	float peak; // y value for the peak (amp)
	int peak_hz = 0;
	int peak_flag = 0;

	int curr_band; // counter for bands?

	for (curr_band = 0; curr_band < total_bands; curr_band++) {
		peak=0;
		
		b1 = pow(2, curr_band * 12.0 / (total_bands-1) ); // ?
		if (b1>4095){ 
			b1=4095;
		}

		if (b1<=b0){
			b1=b0+1; // make sure it uses at least 1 FFT bin
		}
		
		for (; b0 < b1; b0++){
			if (peak < fft[1+b0] ){
				peak = fft[1+b0];
				peak_hz = 1+b0 ; 
			}
		}

		draw_band(specbuf, fft, peak, 0, curr_band);

	}

	gtk_image_set_from_pixbuf(GTK_IMAGE(speci),specpb); // update the display

	return TRUE;
}

int init_bass(void)
{
	// check the correct BASS was loaded
	if (HIWORD(BASS_GetVersion())!=BASSVERSION) {
		Error("An incorrect version of BASS was loaded");
		return 0;
	}

	// initialize BASS
	
	if(!BASS_RecordInit(-1)){
		Error("Can't init recording");
		return -1;
	}

	if(!(chan=BASS_RecordStart(44100, 1, 0, DuffRecording, 0))){
		Error("Can't start recording");
		return -1;
	}

	return 1;
}

void setup_gui(){	

	// create the window
	win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(win),GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW(win),TRUE);
	gtk_window_set_title(GTK_WINDOW(win),"Deskdub");
	g_signal_connect(win,"destroy",GTK_SIGNAL_FUNC(WindowDestroy),NULL);

	GtkWidget *ebox=gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(win),ebox);
	g_signal_connect(ebox,"scroll-event",GTK_SIGNAL_FUNC(ScrollEvent),NULL);

	// create the bitmap
	specpb=gdk_pixbuf_new(GDK_COLORSPACE_RGB,FALSE,8,spec_width,spec_height);
	speci=gtk_image_new_from_pixbuf(specpb);
	gtk_container_add(GTK_CONTAINER(ebox),speci);


	{ // setup palette
		RGB *pal=palette;
		int a;
		memset(palette,0,sizeof(palette));
		for (a=0; a < 256; a++) {
			pal[a].rgbGreen=a+20;
			pal[a].rgbRed=a+20;
			pal[a].rgbBlue=a+25;
		}
	}
	
	{ // setup palette
		RGB *pal=red_palette;
		int a;
		memset(red_palette,0,sizeof(red_palette));
		for (a=0; a < 256; a++) {
			pal[a].rgbGreen=10;
			pal[a].rgbRed=50+(a/2);
			pal[a].rgbBlue=10;
		}
	}

	g_timeout_add(30, UpdateSpectrum, NULL);
}

int main(int argc, char* argv[])
{
	if(init_bass() != 1){
		return -1;
	}
	
	gtk_init(&argc,&argv);
	
	setup_gui();

	gtk_widget_show_all(win);
	gtk_main();

	g_object_unref(specpb);

	BASS_RecordFree();

	return 0;
}
