/*
 *  accelerateFFT.cpp
 *  Livetronica Studio
 *
 *  Created by Aaron Leese on 7/15/10.
 *  Copyright 2010 StageCraft Software. All rights reserved.
 *
 */

#include "accelerateFFT.h"

class FFTGraphView : public AudioProcessorEditor,
    public Timer
{
     
    Path m_path;
    
public:
    FFTCalculator* myFFTCalculator;
    
    
    FFTGraphView(FFTCalculator* FFTCalc) : AudioProcessorEditor(FFTCalc) {
      
        myFFTCalculator = FFTCalc;
        startTimer(1000/10);
        
        // basically and overlay - no interaction
        setWantsKeyboardFocus(false);
        setInterceptsMouseClicks(false, false);
        
        setSize(300, 300);
        
    }
    
    ~FFTGraphView() {
        
        
    }
    
    
private:
    
    void paint(Graphics& g)  {
        
        g.fillAll(Colours::lightgrey);
         
        
        // path
        
        g.setGradientFill(ColourGradient(Colours::black, 0, 0, Colours::goldenrod, getWidth(), getHeight(), true));
                           
        g.fillPath (m_path);
        
        g.drawText(myFFTCalculator->getNoteName(), 10, 10, 40, 10, Justification::left, false);
        
        g.setColour(Colours::greenyellow);
        
        //float freqPeak = myFFTCalculator->peakIndex;
        
        //float phaseAtPeak = myFFTCalculator->FFTInterleavedInput[myFFTCalculator->peakIndex].imag;
        
       // int test = getWidth()/2 + phaseAtPeak*100;// getXforF(freqPeak);
       // g.drawVerticalLine(test, 0, getHeight());
        
        
    }
    
    void update () {
        
        m_path.clear();
        
        m_path.startNewSubPath (getWidth(), getHeight());
        
        for (int xi = 0; xi <= getWidth(); ++xi )
        {
            float x = float(xi)/float(getWidth()); // [0..1)
            
            // distort ... 
            // displaying logarithmically ....
            float f = (log10f(10 + 10000*x) - 1)/3; 
            
            float y =  myFFTCalculator->FFTData[int((f)*myFFTCalculator->FFTDataSize)]; // not sure why, data is inverted ... hence the 1-
            
            //if (xi == 0)
            //    m_path.startNewSubPath ( getWidth() - xi, (1 - y)*getHeight());
            //else
               
            m_path.lineTo ( getWidth() - xi, (1 - y)*getHeight());
            
            
        }
        
        m_path.lineTo (0, getHeight());
        m_path.lineTo (0, getHeight());
        
        
        repaint();
    }

    void resized() {
        
        
        
    }
    
    void timerCallback() {
        
        update();
       // repaint();
    }
    
    
    int getXforF(float freq)
    {
        
        float x = (powf(10, ((freq/512)*3 + 1)) - 10)/10000;
        
        // float f = (log10f(10 + 10000*x) - 1)/3; 
        
                 
        //DBG("x " + String(x));
        
        return x*getWidth();
        
    }
};


AudioProcessorEditor* FFTCalculator::createEditor()
{
     
    return new FFTGraphView(this);
    
}



/* :::::::::::::: RANGE OF FFT :::::::::: 
 
 When using the DFT it is necessary to know how to control the range and resolution of the spectrum.
 
 The range is determined by the sampling time and is equal to ws/2. ( Nyquist F )
 The resolution is determined by the length of the sampling interval ~ 2p/NTs = ws/N .

*/ 


void FFTCalculator::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages) 
{
   // DBG("proc block ");
    
    float* bufferFloat = buffer.getSampleData(0);
	int numSamples = buffer.getNumSamples();
    
    //lastBufWritePos = (lastBufWritePos + buffer.getNumSamples())%FFTDataSize;

	// FULL FFT ... longest, maybe ....
	
	// shifting the data .... dumb ... this can be soooooo much faster ...
	//for (int i=0; i < FFTDataSize; i++)
	//	FFTData[i] = bufferFloat[(lastBufWritePos + i)%FFTDataSize];
	
	// multiply the data by the window function ...
	vDSP_vmul(buffer.getSampleData(0), 1, windowingVector, 1, bufferFloat, 1, numSamples); // writes to bufferFloat
    
	// move the data into the PCM buffer .....
	for(int i=0;i<dims;i++){
        
		FFTInterleavedInput[i].real = bufferFloat[i]; //(float)(buffer[i])/(32768);
		FFTInterleavedInput[i].imag = (float)0;
	}
	
	/* Split the complex (interleaved) data into two arrays */
	vDSP_ctoz(FFTInterleavedInput,2, &FFTComplexSplitInput,1,dims); // split and write to FFTComplexSplitInput
	
	/* Perform the FFT */
	vDSP_fft_zip(fft_weights, &FFTComplexSplitInput,1 ,logY, FFT_FORWARD); // perform math on FFTComplexSplitInput
	
	/* Merge the separate data (real and imaginary) parts into a single array */
	vDSP_ztoc(&FFTComplexSplitInput,1, FFTInterleavedInput,2,dims); // copy back to FFTInterleavedInput
	
    // can use the MAGNITUDE ::::::
    vDSP_zvabs( &FFTComplexSplitInput, 1, FFTData, 1, FFTDataSize);
    
    // write the FFT data to a single float buffer for display purposes ...
    for (short i=0; i < FFTDataSize; i++)
    {
     //   FFTData[i] = FFTInterleavedInput[i].real;
        
        // DB :::: Y-SCALE
        FFTData[i] = (log2(FFTData[i] + 1) + 4)/15;
    }
         
    calculatePitch();
    
    addMidiNote(midiMessages);
    
}

void FFTCalculator::calculatePitch() {
    
    // get absolute value (mag) of each real/img pair .....
    float peakAmp = 0;
    
    // seems neither of these use imag portion ... so ...
    //vDSP_maxmgvi((float*)FFTInterleavedInput, 1, &peakAmp, &peakIndex, FFTDataSize);
    //vDSP_maxvi((float*)FFTInterleavedInput, 1, &peakAmp, &peakIndex, FFTDataSize);
    
    peakIndex = 0;
    
    for (short i =0; i< FFTDataSize; i++)
    {
        if (FFTData[i] > peakAmp)
        {
            peakIndex = i;
            peakAmp = FFTData[i];
        }
    }
    
    
    // signal processing trick ... "spectral reassignment" the quadrature signal (phase) helps us get this more exact ...
    // this works because the group velocity has been shoifted (phase) 
    // ... indicating the local freq of greatest contribution to the bin
    //float peakAdjusted = (float)peakIndex + (float)FFTInterleavedInput[peakIndex].imag;
    
    //DBG("ind " + String((int)peakIndex) + " p " + String((float)FFTData[peakIndex]) + " " + String(peakAmp));
    //peakIndex = round(peakAdjusted);
    
    float harmMaxIndex = 0;
    float harmMax = 0;
    
    for (int i = peakIndex*2-10; i< peakIndex*2+10; i++)
    {
        if (i >= FFTDataSize) continue;
        
        if (FFTData[i] > harmMax)
        {
            harmMaxIndex = float(i);
            harmMax = FFTData[i];
        }
    }
        
    //DBG("harm pred " + String(harmMaxIndex/2.0f));
    
    
    // average the two indices ... or just use the harm ... hmmmm
    float adjustedPeak = (float(peakIndex) + float(harmMaxIndex/2.0f))/2.0f;
    
    float peakF = float(adjustedPeak)/float(FFTDataSize)*float(22050);
    
    if (peakF < 100) return;
    
    // equal temperment ...  
    float noteExact = 69.0f + 12.0f * log2f(peakF/base_a4);
    
    int noteTest = 69 + 12;
    
    while (peakF >= 880.0f) {
        peakF /= 2; // transpose down an octave
        noteTest += 12;
    }
    
    while (peakF < 440.0f) {
        peakF *= 2; // transpose up an octave
        noteTest -= 12;
    }
    
    
    for (short i = 1; i<12; i++)
    {
        if (peakF > 440.0f*scaleIntervals[i])
        {
             noteTest++;
        }
        else
        {
            if (peakAmp > .3)
            {
                /*
                DBG("match : " + String(peakF) 
                    + " " + String(440.0f*scaleIntervals[i-2])
                    + " " + String(440.0f*scaleIntervals[i-1])
                    + " " + String(440.0f*scaleIntervals[i])
                    );
                 */
                
                float cents = 1200 * log2 (peakF / (440.0f*scaleIntervals[i-1]) );
            
               // DBG("cents : " + String(cents) );
                
                if (cents >= 50)  noteTest++; // round UP
                
            }
            
            break;
        }
    }
    
    
     
     
    
    float amplitude = peakAmp;
    int midiNote = (int)round(noteExact);
    
    
    if (midiNote < 69 || midiNote >= 128) return;
    
   
    if (amplitude < .2)
    {
        // all off ...
        currentNote = 0;
        return;
    }
    
    if (amplitude < .3) return;
    
    
    //////// test
  //  currentNote = (int)round(noteExact);
   // DBG("m1 " + getNoteName()); 
    
  //  currentNote = noteTest;
  //  DBG("m2 " + getNoteName()); 
    
    noteExact = noteTest; // using method 2 .... 
    /////////
    
    
    // Quantize ....
    if (!scale[midiNote%12]) 		
    {
        short i = 0;
        
        while (i<12)
        {	
            if (scale[(midiNote+i)%12]) 
            {
                midiNote = midiNote+i;
                break;
            }
            else if (scale[(midiNote-i)%12]) 
            {
                midiNote = midiNote-i;
                break;
            }
            
            i = i+1;
        }
        
    }
    
    
    // disregard OCTAVE changes, this usually is a signal noise issue
    if (  midiNote != lastNote && 
        (midiNote%12 == lastNote%12 // octave
         || lastNote+7 == midiNote // fifth
         || lastNote-7 == midiNote) // fourth
        && lastNote != -1 && lastNote != 0 )
    {
        DBG("octave jump");
        midiNote = lastNote;
    }
    
    CFTimeInterval currentTime = CFAbsoluteTimeGetCurrent();    
    
    if (false && currentTime < currentNoteStartTime + 0.18f) // max trill speed .....
    {
        midiNote = lastNote;
        return;
    }
    else if (amplitude > .3) // new note
    {
        if (lastNote != midiNote) 
        {
            DBG("new note: " + String(midiNote));
            currentNote = midiNote;
        }
    }
    
    currentNoteStartTime = currentTime;
    
}


String FFTCalculator::getNoteName() {
    
    int midiNote = currentNote;
    
    String noteName("");
    
    // get note name 
    if ( (midiNote > 0) & (midiNote<=119) ) { 
         
        String first = notes[midiNote % 12];
        
        String second = String(midiNote/12);
        
        noteName = first + " " + second;
        //return test;
    }

    
    return noteName;
    
}


void FFTCalculator::addMidiNote(MidiBuffer& midiMessages)
{ 
    
    midiMessages.clear();
    
    // Turn off previous note ....
    if (lastNote != 0 && lastNote != currentNote)
    {
        midiMessages.addEvent(MidiMessage::noteOff(1, lastNote),
                              1); // sameplNumber

        lastNote = 0; // ok, done ... do nothing ....
    }

    if (currentNote != 0 && currentNote != lastNote)
    {
        DBG("adding note ..");
        // Turn on a new note ....
        midiMessages.addEvent( MidiMessage::noteOn(1, currentNote, 1.0f),
                              2); // sameplNumber
        
        lastNote = currentNote;  // ok ... done ....
    }
           
       
    
    
}




