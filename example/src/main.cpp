#include "ofMain.h"
#include "ofApp.h"
#include "ofAppGLFWWindow.h"

//========================================================================
int main( )
{
	ofAppGLFWWindow window;
	//window.setMultiDisplayFullscreen(true);
	window.setNumSamples(8);
	ofSetupOpenGL(&window, 1024, 768, OF_WINDOW);
	ofRunApp( new ofApp());
}
