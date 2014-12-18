/*
 * =====================================================================================
 *
 *       Filename:  riser_generator.c
 *
 *    Description:  This is an application that allows the user to move the object
 *                  to control pitch and a lowpass filter.
 *        Version:  1.0
 *        Created:  12/14/2014 15:18:46
 *       Revision:  3.0
 *       Compiler:  gcc
 *
 *         Author:  Marlon Fischer, mff273@nyu.edu
 *   Organization:  NYU
 *
 * =====================================================================================
 */

//-----------------------------------------------------------------------------
// #INCLUDES
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <portaudio.h>
#include <stdbool.h>
#include <SOIL/SOIL.h>
#include "Biquad.h"

// OpenGL
#ifdef __MACOSX_CORE__
#include <GLUT/glut.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

// Platform-dependent sleep routines.
#if defined( __WINDOWS_ASIO__ ) || defined( __WINDOWS_DS__ )
#include <windows.h>
#define SLEEP( milliseconds ) Sleep( (DWORD) milliseconds ) 
#else // Unix variants
#include <unistd.h>
#define SLEEP( milliseconds ) usleep( (unsigned long) (milliseconds * 1000.0) )
#endif


//-----------------------------------------------------------------------------
// #DEFINES
//-----------------------------------------------------------------------------
#define INIT_FREQUENCY          220 //defines inital frequency
#define INIT_VOLUME             1 //defines initial amplitude
#define SINE                    0
#define TRI                     1
#define SAW                     2
#define SQUARE                  3
#define FORMAT                  paFloat32
#define BUFFER_SIZE             1024
#define SAMPLE                  float
#define SAMPLE_RATE             44100
#define MONO                    1
#define STEREO                  2
#define cmp_abs(x)              ( sqrt( (x).re * (x).re + (x).im * (x).im ) )
#define INIT_WIDTH              800 //defines initial window width
#define INIT_HEIGHT             600 //defines inital window height
#define PI                      3.14159265358979323846 //defines PI 3.14159265358979323846
#define ROTATION_INCR           .75f //defines how fast the rotation happens
#define RISE_INCR               .1f //Rise speed
#define MIN_VOLUME              -160
#define WATERFALL_SIZE          20
#define X_MIN                   -6.12
#define Y_MIN                   -3.64
#define X_MAX                   6.12
#define Y_MAX                   3.64

//-----------------------------------------------------------------------------
// Name: GLOBAL VARIABLES
//-----------------------------------------------------------------------------

//structure for portaudio callback function
typedef struct {
    float frequency;
    int amplitude;
    int wavetype;

    float lowpass_freq;
    float highpass_freq;

    float wave_buff[BUFFER_SIZE];
} paData;

//struct for positions
typedef struct {
    double x;
    double y;
} Pos;

//struct for circle
typedef struct {
    Pos center;
    Pos coord;
} Texture;

//initialize global data
paData data; 

//initializes filters
biquad* bq_low;
biquad* bq_high;

//initialize waterfall matrix
float g_waterfall[WATERFALL_SIZE][BUFFER_SIZE];

typedef double  MY_TYPE;
typedef char BYTE;   // 8-bit unsigned entity.

// width and height of the window
GLsizei g_width = INIT_WIDTH;
GLsizei g_height = INIT_HEIGHT;
GLsizei g_last_width = INIT_WIDTH;
GLsizei g_last_height = INIT_HEIGHT;

// global audio vars
GLint g_buffer_size = BUFFER_SIZE;

//buffer for wave graphics
SAMPLE g_buffer[BUFFER_SIZE * 2];

//window buffer
SAMPLE g_window[BUFFER_SIZE]; 
unsigned int g_channels = MONO;

// Threads Management
GLboolean g_ready = false;

// fill mode
GLenum g_fillmode = GL_FILL;

// light 0 position
GLfloat g_light0_pos[4] = { 2.0f, 1.2f, 4.0f, 1.0f };

// light 1 parameters
GLfloat g_light1_ambient[] = { .2f, .2f, .2f, 1.0f };
GLfloat g_light1_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat g_light1_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat g_light1_pos[4] = { -2.0f, 0.0f, -4.0f, 1.0f };

// fullscreen
GLboolean g_fullscreen = false;

// modelview stuff
GLfloat g_linewidth = 2.0f;

// Port Audio struct
PaStream *g_stream;

// circle
Texture g_circle;

// Translation
bool g_translate = false;
Pos g_tex_init_pos = {0.,0.};
Pos g_tex_incr = {0.,0.};

// Rotation
bool g_key_rotate_x = false;
bool g_key_rotate_y = false;
GLfloat g_inc_y = 0.0;
GLfloat g_inc_x = 0.0;

//self riser
bool self_rise = false;

//-----------------------------------------------------------------------------
// function prototypes
//-----------------------------------------------------------------------------
void help();
void idleFunc( );
void displayFunc( );
void reshapeFunc( int width, int height );
void keyboardFunc( unsigned char, int, int );
void specialKey( int key, int x, int y );
void initialize_graphics( );
void initialize_glut(int argc, char *argv[]);
void mouseFunc(int button, int state, int x, int y);
void mouseMotionFunc(int x, int y);
void initialize_audio(PaStream **g_stream);
void stop_portAudio(PaStream **g_stream);
void init_datastruct();
void hanning( float * window, unsigned long length );
void riser ();
void drawWindowedTimeDomain( float , SAMPLE *buffer);
double round(double);

// Function copied from the FFT library
void apply_window( float * data, float * window, unsigned long length )
{
   unsigned long i;
   for( i = 0; i < length; i++ )
       data[i] *= window[i];
}


//-----------------------------------------------------------------------------
// name: help()
// desc: bring up help menu
//-----------------------------------------------------------------------------
void help()
{
    printf( "----------------------------------------------------\n" );
    printf( "RISER GENERATOR\n" );
    printf( "----------------------------------------------------\n" );
    printf( "'h' - print this help message\n" );
    printf( "'f' - toggle fullscreen\n" );
    printf( "click and drag mouse up and down - change pitch frequency\n" );
    printf( "click and drag mouse left and right - change lowpass frequency\n" );
    printf( "'spacebar' - automatically move circle to top right corner\n" );
    printf( "'s' - bring circle back to bottom left corner \n" );
    printf( "'w' - change waveform");
    printf( "'m' - mute audio\n" );
    printf( "'arrow keys' - turn on green waveform movement\n");
    printf( "'q' - quit\n" );
    printf( "----------------------------------------------------\n" );
    printf( "\n" );
}


//-----------------------------------------------------------------------------
// Name: paCallback( )
// Desc: callback from portAudio
//-----------------------------------------------------------------------------
static int paCallback( const void *inputBuffer,
        void *outputBuffer, unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags, void *userData ) {
    
    float *out = (float*)outputBuffer; //casting the output buffer to a float
    
    //starts the samples at -1
    static float sine_sample = -1; 
    static float triangle_sample = -1;
    static float saw_sample = -1;
    
    //phase for sinewave
    float sine_phase;
    
    //left previous phase holder starting at 0
    static float prev_phase = 0; 
    
    //false is left side of triangle, true is right
    bool triangle_side = false; 
    
    //phase for squarewave
    static int square_phase = 0;
    
    int period = SAMPLE_RATE/data.frequency;
    int amplitude = data.amplitude; //local variable for amplitude

    int i;
    
    //initiate the highpass and lowpass buffers
    bq_low = bq_new(LOWPASS,data.lowpass_freq,10.0,1.0,SAMPLE_RATE);
    bq_high = bq_new(HIGHPASS,data.highpass_freq*1.2,10.0,1.0,SAMPLE_RATE);


    for (i = 0; i < framesPerBuffer; i++){ 
        //tests for cases of what type of waveform
        switch(data.wavetype){
            
            case SINE:
//*****************************************************************SINE********************************************************
                sine_phase = 2. * PI * data.frequency / SAMPLE_RATE + prev_phase; //gets phase of sinewave
                sine_sample = sin(sine_phase);
        
                data.wave_buff[2 * i] = (sine_sample * amplitude); //wave_buffput left times the amplification
                data.wave_buff[2 * i + 1] = (sine_sample * amplitude); //wave_buffput right times the amplification
                
                //set the output going through the lowpass and highpass filters
                out[2*i] = bq_process(bq_high, (bq_process(bq_low, data.wave_buff[2*i])));        
                out[2*i+1] = bq_process(bq_high, (bq_process(bq_low, data.wave_buff[2*i+1])));
                
                //input the g_buffer with the output buffer
                g_buffer[2*i] = out[2*i];
                g_buffer[2*i+1] = out[2*i+1];


                if(sine_phase > 2. * PI){ //restart the phase of sine wave
                    sine_phase -= 2. * PI; 
                }

                prev_phase = sine_phase; //sets previous phase
                break;
    
            case TRI:
//*****************************************************************TRIANGLE**************************************************
                if(triangle_side == false){//going up on triangle
                    triangle_sample += 2. / (period/2);
            
                    if(triangle_sample > 1.){//put so that the triangle would not clip
                        triangle_sample = 1.;
                    }
                }

                if(triangle_side == true){//going down on triangle
                    triangle_sample -= 2. / (period/2);
            
                    if(triangle_sample < -1.){
                        triangle_sample = -1.;
                    }
                }
        
                data.wave_buff[2 * i] = triangle_sample * amplitude;
                data.wave_buff[2 * i + 1] = triangle_sample * amplitude;
                
                //set the output going through the lowpass and highpass filters
                out[2*i] = bq_process(bq_high, (bq_process(bq_low, data.wave_buff[2*i])));        
                out[2*i+1] = bq_process(bq_high, (bq_process(bq_low, data.wave_buff[2*i+1])));
                
                //input the g_buffer with the output buffer
                g_buffer[2*i] = out[2*i];
                g_buffer[2*i+1] = out[2*i+1];


                //sets the sides of the triangle
                if(triangle_sample >= 1. || triangle_sample <= -1.){ 
                    triangle_side = !triangle_side;
                }
                break;
                        
            case SAW:
//*****************************************************************SAW*******************************************************
                saw_sample += 2. / period;//saw only goes up and then drops off for its period
            
                if(saw_sample > 1){
                    saw_sample -= 2;
                }
        
                data.wave_buff[2 * i] = saw_sample * amplitude;
                data.wave_buff[2 * i + 1] = saw_sample * amplitude;
                
                //set the output going through the lowpass and highpass filters
                out[2*i] = bq_process(bq_high, (bq_process(bq_low, data.wave_buff[2*i])));        
                out[2*i+1] = bq_process(bq_high, (bq_process(bq_low, data.wave_buff[2*i+1])));
                
                //input the g_buffer with the output buffer
                g_buffer[2*i] = out[2*i];
                g_buffer[2*i+1] = out[2*i+1];
    
                break;

            case SQUARE:
//*****************************************************************SQUARE*****************************************************
                // Assign wave_buffput
                if (square_phase < period/2) {
                    data.wave_buff[2*i] = amplitude;  /* left */
                    data.wave_buff[2*i+1] = amplitude;  /* right */    
                }
                else if (square_phase >= period/2) {
                    data.wave_buff[2*i] = -amplitude;  /* left */
                    data.wave_buff[2*i+1] = -amplitude;  /* right */    
                }
                
                //set the output going through the lowpass and highpass filters
                out[2*i] = bq_process(bq_high, (bq_process(bq_low, data.wave_buff[2*i])));        
                out[2*i+1] = bq_process(bq_high, (bq_process(bq_low, data.wave_buff[2*i+1])));
                
                //input the g_buffer with the output buffer
                g_buffer[2*i] = out[2*i];
                g_buffer[2*i+1] = out[2*i+1];

                // Increase phase
                square_phase++;

                // Wrap phase
                if (square_phase >= period) {
                    square_phase = 0;
                }
            } 
        }
     
    // set flag
    g_ready = true;
    return paContinue; //return 

}
//-----------------------------------------------------------------------------
// Name: init_datastruct( )
// Desc: Initializes parameters in the data structure
// //-----------------------------------------------------------------------------
void init_datastruct(void) {

    //set initial parameters of wave for frequency and amplitude
    data.frequency = INIT_FREQUENCY;
    data.amplitude = INIT_VOLUME;
    data.wavetype = SINE;
    
    //set the position of the circle
    g_circle.center.x = X_MIN;
    g_circle.center.y = Y_MIN;
    g_circle.coord.x = 0;
    g_circle.coord.y = 0;

    /* Init lowpass and highpass */
    data.lowpass_freq = 0;
    data.highpass_freq = 0;
}

//-----------------------------------------------------------------------------
// Name: initialize_glut( )
// Desc: Initializes Glut with the global vars
//-----------------------------------------------------------------------------
void initialize_glut(int argc, char *argv[]) {
    // initialize GLUT
    glutInit( &argc, argv );
    // double buffer, use rgb color, enable depth buffer
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    // initialize the window size
    glutInitWindowSize( g_width, g_height );
    // set the window postion
    glutInitWindowPosition( 400, 100 );
    // create the window
    glutCreateWindow( "RISER GENERATOR");
    // full screen
    if( g_fullscreen )
        glutFullScreen();

    // set the idle function - called when idle
    glutIdleFunc( idleFunc );
    // set the display function - called when redrawing
    glutDisplayFunc( displayFunc );
    // set the reshape function - called when client area changes
    glutReshapeFunc( reshapeFunc );
    // set the keyboard function - called on keyboard events
    glutKeyboardFunc( keyboardFunc );
    // set window's to specialKey callback
    glutSpecialFunc( specialKey );
    // set the mouse function for new clicks
    glutMouseFunc( mouseFunc );
    // set the mouse function for motion when a button is pressed
    glutMotionFunc( mouseMotionFunc );
    // do our own initialization
    initialize_graphics( );  
}

//-----------------------------------------------------------------------------
// Name: hanning()
// Desc: used for drawing the waveforms
//-----------------------------------------------------------------------------
void hanning( float * window, unsigned long length )
{
   unsigned long i;
   double pi, phase = 0, delta;

   pi = 4.*atan(1.0);
   delta = 2 * pi / (double) length;

   for( i = 0; i < length; i++ )
   {
       window[i] = (float)(0.5 * (1.0 - cos(phase)));
       phase += delta;
   }
}

//-----------------------------------------------------------------------------
// Name: initialize_audio( RtAudio *dac )
// Desc: Initializes PortAudio with the global vars and the stream
//-----------------------------------------------------------------------------
void initialize_audio(PaStream **g_stream) {
    
    PaStreamParameters outputParameters;
    PaError err;

    /* Initialize PortAudio */
    Pa_Initialize();

    /* Set output stream parameters */
    outputParameters.device = Pa_GetDefaultOutputDevice();
    outputParameters.channelCount = g_channels;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = 
        Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    /* Open audio stream */
    err = Pa_OpenStream( &(*g_stream),
            NULL,
            &outputParameters,
            SAMPLE_RATE, BUFFER_SIZE, paNoFlag, 
            paCallback, NULL );

    if (err != paNoError) {
        printf("PortAudio error: open stream: %s\n", Pa_GetErrorText(err));
    }

    /* Start audio stream */
    err = Pa_StartStream( *g_stream );
    if (err != paNoError) {
        printf(  "PortAudio error: start stream: %s\n", Pa_GetErrorText(err));
    }
}

//-----------------------------------------------------------------------------
// Name: stop_portAudio( )
// Desc: Stops the audio stream
//-----------------------------------------------------------------------------
void stop_portAudio(PaStream **g_stream) {
    PaError err;

    /* Stop audio stream */
    err = Pa_StopStream( *g_stream );
    if (err != paNoError) {
        printf(  "PortAudio error: stop stream: %s\n", Pa_GetErrorText(err));
    }
    /* Close audio stream */
    err = Pa_CloseStream(*g_stream);
    if (err != paNoError) {
        printf("PortAudio error: close stream: %s\n", Pa_GetErrorText(err));
    }
    /* Terminate audio stream */
    err = Pa_Terminate();
    if (err != paNoError) {
        printf("PortAudio error: terminate: %s\n", Pa_GetErrorText(err));
    }

    //terminate filter
    bq_destroy(bq_low);
    bq_destroy(bq_high);

}

//-----------------------------------------------------------------------------
// Name: main
// Desc: ...
//-----------------------------------------------------------------------------
int main( int argc, char *argv[] )
{
    //Initialize datatype
    init_datastruct();

    /* Init waterfall */
    memset(g_waterfall, MIN_VOLUME, WATERFALL_SIZE * g_buffer_size * sizeof(float) );
    
    // Initialize Glut
    initialize_glut(argc, argv);

    // Initialize PortAudio
    initialize_audio(&g_stream);

    //make the transform window
    hanning(g_window, g_buffer_size);

    // Print help
    help();

    // Wait until 'q' is pressed to stop the process
    glutMainLoop();

    // This will never get executed

    return EXIT_SUCCESS;
}

//-----------------------------------------------------------------------------
// Name: idleFunc( )
// Desc: callback from GLUT
//-----------------------------------------------------------------------------
void idleFunc( )
{
    // render the scene
    glutPostRedisplay( );
}

//-----------------------------------------------------------------------------
// Name: keyboardFunc( )
// Desc: key event
//-----------------------------------------------------------------------------
void keyboardFunc( unsigned char key, int x, int y )
{
    //printf("key: %c\n", key);
    switch( key )
    {
        // Print Help
        case 'h':
            help();
            break;

            // Fullscreen
        case 'f':
            if( !g_fullscreen )
            {
                g_last_width = g_width;
                g_last_height = g_height;
                glutFullScreen();
            }
            else
                glutReshapeWindow( g_last_width, g_last_height );

            g_fullscreen = !g_fullscreen;
            printf("[RISER GENERATOR]: fullscreen: %s\n", g_fullscreen ? "ON" : "OFF" );
            break;

        case 'q':
            // Close Stream before exiting
            stop_portAudio(&g_stream);

            exit( 0 );
            break;

        case 'm':
            //if the output is muted, turn back on
            if(data.amplitude == 0){
                data.amplitude = 1;
            }
            else{
                //mutes output
                data.amplitude = 0;
            }
            break;

        case 'w':
            //change waveform type depending on previous waveform
            if(data.wavetype == 0){
                data.wavetype = 1;
            }
            else if(data.wavetype == 1){
                data.wavetype = 2;
            }
            else if(data.wavetype == 2){
                data.wavetype = 3;
            }            
            else{
                data.wavetype = 0;
            }
            break;

        case 's':
            //set the circle back to the begining coordinates
            g_circle.center.x = X_MIN;
            g_circle.center.y = Y_MIN;
            break;

        case ' ':
            //turn on and off the automated rise
            self_rise = !self_rise;
            if(self_rise){
                printf("SELF RISING ON\n");
            }
            if(!self_rise){
                printf("SELF RISING OFF\n");
            }
            break;
    }
}

//-----------------------------------------------------------------------------
// Name: specialUpKey( int key, int x, int y)
// Desc: Callback to know when a special key is pressed
//-----------------------------------------------------------------------------
void specialKey(int key, int x, int y) { 
    // Check which (arrow) key is pressed
    switch(key) {
        case GLUT_KEY_LEFT : // Arrow key left is pressed
            g_key_rotate_y = !g_key_rotate_y;
            g_inc_y = -ROTATION_INCR;
            break;
        case GLUT_KEY_RIGHT :    // Arrow key right is pressed
            g_key_rotate_y = !g_key_rotate_y;
            g_inc_y = ROTATION_INCR;
            break;
        case GLUT_KEY_UP :        // Arrow key up is pressed
            g_key_rotate_x = !g_key_rotate_y;
            g_inc_x = ROTATION_INCR;
            break;
        case GLUT_KEY_DOWN :    // Arrow key down is pressed
            g_key_rotate_x = !g_key_rotate_y;
            g_inc_x = -ROTATION_INCR;
            break;   
    }
}  

//-----------------------------------------------------------------------------
// Name: mouseFunc(int button, int state, int x, int y)
// Desc: Callback to manage the mouse input when click new button
//-----------------------------------------------------------------------------
void mouseFunc(int button, int state, int x, int y) {
    printf("Mouse: %d, %d, x:%d, y:%d\n", button, state, x, y);
    if (state == 0) {
        // start Translation
        g_translate = true;
        g_tex_init_pos.x = x + g_width/2.f;
        g_tex_init_pos.y = y + g_height/2.f;
    }
    else {
        // Stop Translation
        g_translate = false;
        g_circle.center.x += g_tex_incr.x;
        g_circle.center.y += g_tex_incr.y;
        g_tex_incr.x = 0.;
        g_tex_incr.y = 0.;
    }
}

//-----------------------------------------------------------------------------
// Name: mouseFunc(int button, int state, int x, int y)
// Desc: Callback to manage the mouse motion
//-----------------------------------------------------------------------------
void mouseMotionFunc(int x, int y) {
    printf("Mouse Moving: %d, %d\n", x, y);
    if (g_translate) {
        g_tex_incr.x = (x + g_width/2.0f - g_tex_init_pos.x)/50;
        g_tex_incr.y = (g_tex_init_pos.y - (y + g_height/2.0f))/50;
    }
}

//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void reshapeFunc( int w, int h )
{
    // save the new window size
    g_width = w; g_height = h;
    // map the view port to the client area
    glViewport( 0, 0, w, h );
    // set the matrix mode to project
    glMatrixMode( GL_PROJECTION );
    // load the identity matrix
    glLoadIdentity( );
    // create the viewing frustum
    GLfloat fovy = 45.0f;
    GLfloat zNear = .05f;
    GLfloat zFar = 2500.0f;
    //gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, .05, 50.0 );
    gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, 1.0, 1000.0 );
    // set the matrix mode to modelview
    glMatrixMode( GL_MODELVIEW );
    // load the identity matrix
    glLoadIdentity( );
    // position the view point
    gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
}


//-----------------------------------------------------------------------------
// Name: initialize_graphics( )
// Desc: sets initial OpenGL states and initializes any application data
//-----------------------------------------------------------------------------
void initialize_graphics()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);                 // Black Background
    // set the shading model to 'smooth'
    glShadeModel( GL_SMOOTH );
    // enable depth
    glEnable( GL_DEPTH_TEST );
    // set the front faces of polygons
    glFrontFace( GL_CCW );
    // set fill mode
    glPolygonMode( GL_FRONT_AND_BACK, g_fillmode );
    // enable lighting
    glEnable( GL_LIGHTING );
    // enable lighting for front
    glLightModeli( GL_FRONT_AND_BACK, GL_TRUE );
    // material have diffuse and ambient lighting 
    glColorMaterial( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE );
    // enable color
    glEnable( GL_COLOR_MATERIAL );
    // noirmalize (for scaling)
    glEnable( GL_NORMALIZE );
    // line width
    glLineWidth( g_linewidth );

    // enable light 0
    glEnable( GL_LIGHT0 );

    // setup and enable light 1
    glLightfv( GL_LIGHT1, GL_AMBIENT, g_light1_ambient );
    glLightfv( GL_LIGHT1, GL_DIFFUSE, g_light1_diffuse );
    glLightfv( GL_LIGHT1, GL_SPECULAR, g_light1_specular );
    glEnable( GL_LIGHT1 );
}

//-----------------------------------------------------------------------------
// Name: void rotateView ()
// Desc: Rotates the current view by the angle specified in the globals
//-----------------------------------------------------------------------------
void rotateView () {
    
    static GLfloat angle_x = -50;
    static GLfloat angle_y = 50;
    
    if (g_key_rotate_y) {
        //automatically move the object, speed depending on circle y position
        glRotatef ( angle_y += (g_circle.center.y / 10) , 0.0f, 1.0f, 0.0f );
    }
    else {
        glRotatef (angle_y, 0.0f, 1.0f, 0.0f );
    }

    if (g_key_rotate_x) {
        //automattically move the object, speed depending on the circle x position
        glRotatef ( angle_x += (g_circle.center.x / 10), 1.0f, 0.0f, 0.0f );
    }
    else {
        glRotatef (angle_x, 1.0f, 0.0f, 0.0f );
    }
}

//-----------------------------------------------------------------------------
// Name: void riser ()
// automatic riser
//-----------------------------------------------------------------------------
void riser (){
    if(self_rise){
        //increase x by increase rate
        g_circle.center.x += (RISE_INCR);

        //increase y by increase rate / 2 so that circle moves diagonally
        g_circle.center.y += (RISE_INCR/2);
    }
}
//-----------------------------------------------------------------------------
// Name: void drawWindowedTimeDomain(SAMPLE *buffer)
// Desc: Draws the Windowed Time Domain signal in the top of the screen
//-----------------------------------------------------------------------------
void drawWindowedTimeDomain( float z, SAMPLE *buffer) {
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Initialize initial x
    GLfloat x = -5;

    // Calculate increment x
    GLfloat xinc = fabs((2*x)/g_buffer_size);

    glPushMatrix();
    {
        //rotate
        rotateView();
        
        //set the position of the drawn waveform
        glTranslatef(0, 0, z);

        //color the waveform green with blue tint
        glColor3f(0.0, 1.0, .2);
        
        glBegin(GL_LINE_STRIP);

        // Draw Windowed Time Domain
        for (int i=0; i<g_buffer_size; i++)
        {
            glVertex3f(x, 4*buffer[i], 0.0f);
            x += xinc;
        }

        glEnd();

    }
    glPopMatrix();
}

//-----------------------------------------------------------------------------
// Name: round()
// Desc: rouds up number using math function
//-----------------------------------------------------------------------------
double round(double d)
{
    return floor(d + 0.5);
}

//-----------------------------------------------------------------------------
// Name: drawCircle() built from OpenGL website forum
// Desc: draws and moves circle depending on parameters
//-----------------------------------------------------------------------------
void drawCircle()
{
    glPushMatrix();
    {
        //automate the circle with spacebar
        riser ();

        //calculate the position of the circle x position
        g_circle.coord.x = g_circle.center.x + g_tex_incr.x;

        //limit the circles x position so it doesnt go off screen
        if(g_circle.coord.x <= X_MIN){
            g_circle.coord.x = X_MIN;
        }
        if(g_circle.coord.x >= X_MAX){
            g_circle.coord.x = X_MAX;
        }
        
        //putting the circles x location range into the filter frequency ranges
        double filter_slope = 1.0 * (INIT_FREQUENCY*8.0) / (X_MAX*2);
        data.lowpass_freq = round(filter_slope * (g_circle.coord.x+X_MAX));
        data.highpass_freq = round(filter_slope * (g_circle.coord.x+X_MAX)) - INIT_FREQUENCY;
        
        //calculate the poisition of the circle y position
        g_circle.coord.y = g_circle.center.y + g_tex_incr.y;

        //limit the circles y position so that it doesnt go off screen
        if(g_circle.coord.y <= Y_MIN){
            g_circle.coord.y = Y_MIN;
        }
        if(g_circle.coord.y >= Y_MAX){
            g_circle.coord.y = Y_MAX;
        }
        
        //putting the circles y location range into the pitch frequency range
        double pitch_slope = 1.0 * ((INIT_FREQUENCY*8.0) - INIT_FREQUENCY) / (Y_MAX*2);
        data.frequency = INIT_FREQUENCY + round(pitch_slope * (g_circle.coord.y + Y_MAX));
        
        //sets the coordinates for the circle
        glTranslatef(g_circle.coord.x,g_circle.coord.y, 0.0f);

        //scales the circle down so that it has more room to move on the screen
        glScalef(0.5f, 0.5f, 0.5f);
        
        //color the circle red
        glColor3f(1.0, 0.0, 0.0);
        
        //Draw the actual circle
        glBegin(GL_LINE_LOOP);

        for(int i =0; i <= 300; i++){
            double angle = 2 * PI * i / 300;
            double x = cos(angle);
            double y = sin(angle);
            glVertex2d(x,y);
        }

        glEnd();
    }
    glPopMatrix();
}
//-----------------------------------------------------------------------------
// Name: drawSpectrum() copied from lab 11
// Desc: Draws the frequency spectrum cascading
//-----------------------------------------------------------------------------
void drawSpectrum(float *buffer, int length) {
    // Deactivate the texture
    glBindTexture(GL_TEXTURE_2D, 0);

    // Draw Time Domain
    int i, k;
    for (k = WATERFALL_SIZE - 1; k >= 0; k--) {
        // Initialize initial x
        GLfloat x = -(length-length*0.2)/2.0f;

        // Calculate increment x
        GLfloat xinc = fabs((2*x)/(float)g_buffer_size);
        
        glColor4f(0.4, .2, 1.0, 1 - (float)k / (float)WATERFALL_SIZE);

        glBegin(GL_LINE_STRIP);
        for (i = 0; i < g_buffer_size; i++)
        {
            if (g_waterfall[k][i] > MIN_VOLUME) {
                glVertex3f(x, g_waterfall[k][i], -k*100);
            }
            x += xinc;
            
        }
        glEnd();

        if (k == 0) {
            memcpy( g_waterfall[k], buffer, g_buffer_size * sizeof(float) );
        }
        else {
            memcpy( g_waterfall[k], g_waterfall[k - 1], g_buffer_size * sizeof(float) );
        }
    }

}
//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
    // local variables
    SAMPLE buffer[g_buffer_size];

    // wait for data
    while( !g_ready ) usleep( 1000 );

    // Hand off to audio callback thread
    g_ready = false;

    // clear the color and depth buffers
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    

    //sine windowed Time Domain
    drawWindowedTimeDomain(0., g_buffer);
    
    //draw circle
    drawCircle();
    
    // Draw spectrum
    drawSpectrum(g_buffer, 500);

    // apply the transform window
    apply_window( (float*)buffer, g_window, g_buffer_size );

    // flush gl commands
    glFlush( );

    // swap the buffers
    glutSwapBuffers( );
}


