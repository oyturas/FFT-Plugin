/*
 *  accelerateFFT.h
 *  Livetronica Studio
 *
 *  Created by Aaron Leese on 7/15/10.
 *  Copyright 2010 StageCraft Software. All rights reserved.
 *
 */

#include "juceHeader.h"

#include <Accelerate/Accelerate.h>

class FFTCalculator : public AudioPluginInstance
{
	
public:
	
    // Accelerate stuff ...
    int nx;
    int ny;
    int dims;
    int logX;
    int logY;
    
    // pitch detection
    StringArray notes; // = {"C ","C#","D ","D#","E ","F ","F#","G ","G#","A ","A#","B "};
    const float base_a4; //=440.0; // set A4=440Hz
    

    int currentRoot; // root notes, C,C#,etc.
    bool scale[12]; 
    float scaleIntervals[12];
    bool continuous;    
       
    
    unsigned short currentNote;
    unsigned short lastNote;
   
    unsigned int cents; // for pitch bending from input
    unsigned int detune; // for pitch wheel
    double currentNoteStartTime;

    vDSP_Length peakIndex;

    DSPComplex *FFTInterleavedInput;
    DSPSplitComplex FFTComplexSplitInput; // for FFT math purposes
    
    FFTSetup fft_weights;
    float *windowingVector;
     
    float *FFTData;
    
    int lastBufWritePos;
    int FFTDataSize;
    
	FFTCalculator() : base_a4(440.0)
	{ 
		
        DBG("FFT CALc");
        
        FFTDataSize = 1024;
        
        notes.add("C ");
        notes.add("C#");
        notes.add("D ");
        notes.add("D#");
        notes.add("E ");
        notes.add("F ");
        notes.add("F# ");
        notes.add("G ");
        notes.add("G#");
        notes.add("A ");
        notes.add("A#");
        notes.add("B ");
        notes.add("C ");
        
        currentNote = lastNote = 0;
        
        nx = 1;
        ny = FFTDataSize;
        
        /* Total area or volume covered by the dimensions */
        dims = nx*ny;
        
        
        // declare a bit array and set the bits appropriate to the scale selected ...
        // using equal temparment
		//
		//		 C    C#    D    D#   E    F     F#    G    G#   A    A#    B   C
		//		 1  16/15  9/8  6/5  5/4  4/3  45/32  3/2  8/5  5/3  9/5  15/8  2 

        scaleIntervals[0] = 1.0f;
        scaleIntervals[1] = 16.0f/15;
        scaleIntervals[2] = 9.0f/8;
        scaleIntervals[3] = 6.0f/5;
        scaleIntervals[4] = 5.0f/4;
        scaleIntervals[5] = 4.0f/3;
        scaleIntervals[6] = 45.0f/32;
        scaleIntervals[7] = 3.0f/2;
        scaleIntervals[8] = 8.0f/5;
        scaleIntervals[9] = 5.0f/3;
        scaleIntervals[10] = 9.0f/5;
        scaleIntervals[11] = 15.0f/8; 
        
        
        /* Get log (base 2) of each dimension */
        logX = (int)log2((double)nx);
        logY = (int)log2((double)ny);
        
        /* Allocate and initialize some data in interleved/complex format */
        FFTInterleavedInput = (DSPComplex*)malloc(dims*sizeof(DSPComplex));
        
        for(int i=0;i<dims;i++){
            FFTInterleavedInput[i].real = 0; //(float)i;
            FFTInterleavedInput[i].imag = 0; //(float)(dims - i) - 1.0f;
        }
        
        /* Setup and allocate some memory for performing the FFTs */
        FFTComplexSplitInput.realp = (float*)malloc(dims*sizeof(float));
        FFTComplexSplitInput.imagp = (float*)malloc(dims*sizeof(float));
        
        /* Setup the weights (twiddle factors) for performing the FFT */
        float max = logX;
        if (logY > logX) max = logY; // maximum
        fft_weights = vDSP_create_fftsetup(max, kFFTRadix2);
        
        windowingVector = (float*)malloc(FFTDataSize*sizeof(float));
        //vDSP_hamm_window( windowingVector, FFTDataSize, 0);
        //vDSP_blkman_window( windowingVector, FFTDataSize, 0);
        vDSP_hann_window( windowingVector, FFTDataSize, 0);
        
        // and a buffer to store the float equivalents of the audio input ....
       
        //FFTInputBuffer = (float*)malloc(FFTDataSize*sizeof(float));
        
        FFTData  = (float*)malloc(FFTDataSize*sizeof(float));
        lastBufWritePos = 0;
		
        currentNote = lastNote = 0;
        
        for (short i=0; i<12; i++)
            scale[i] = 1;
        
        
	}
	
	~FFTCalculator()
	{
		/* Free up the allocations */
		//free(input.realp);
		//free(input.imagp);
	
		//destroy_fftsetup(fft_weights);
	
	}
	  
	
	// :::::::::::::: AudioProcessor Methods
	
	//==============================================================================
	const String getName() const { return "Livetronica Filter"; }	
	int getNumParameters()	{ return 0; }
	float getParameter (int index)	{ return 0; }
	void setParameter (int index, float newValue) {}
	const String getParameterName (int index) { return String::empty; }
	const String getParameterText (int index)	{ return String::empty; }
	const String getInputChannelName (const int channelIndex) const { return String (channelIndex + 1); }
	const String getOutputChannelName (const int channelIndex) const { return String (channelIndex + 1); }
	bool isInputChannelStereoPair (int index) const { return false; }
	bool isOutputChannelStereoPair (int index) const { return false; }
	bool acceptsMidi() const { return true; }
	bool producesMidi() const{return false; }
	
    //============================================================================== PROCESSOR CALLBACK METHODS
    void prepareToPlay (double sampleRate, int samplesPerBlock) {}
    void releaseResources() {}
	void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);
    
    
    //============================================================================== UI
    bool hasEditor() const {
        return true;
    }
	AudioProcessorEditor* createEditor();    
    
    //============================================================================== PROGRAM
    int getNumPrograms()                                        { return 0; }
    int getCurrentProgram()                                     { return 0; }
    void setCurrentProgram (int index)                          { }
    const String getProgramName (int index)                     { return String::empty; }
    void changeProgramName (int index, const String& newName)   { }
	
    //============================================================================== SAVE/LOAD
    void getStateInformation (MemoryBlock& destData) {}
    void setStateInformation (const void* data, int sizeInBytes) {}
    
    // ====================================================================== PluginInstance Methods
    void fillInPluginDescription (PluginDescription &description) const
 	{
        description.name = "FFT";
        description.descriptiveName = "Livetronica FFT";
        description.pluginFormatName = "VST/AU"; 
        description.category = "Effect";
        description.manufacturerName = "Livetronica Studio";
        description.version = "1";
        description.fileOrIdentifier = "ident";
        
        //Time 	lastFileModTime
        
        description.uid = 1;
        
        description.isInstrument = false;
        
        description.numInputChannels = 2;
        description.numOutputChannels = 2;
        
    }
    
    void* getPlatformSpecificData ()
    {
     
        return 0;
    }
    
    void calculatePitch();
	 
    String getNoteName();
    
    void addMidiNote( MidiBuffer& midiMessages );
    
};

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FFTCalculator();
}

